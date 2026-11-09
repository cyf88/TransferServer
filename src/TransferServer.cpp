#include "TransferServer.h"
#include "logger.h"

#include <chrono>
#include <cstring>
#include <vector>

namespace {
const int kFramePoolCapacity = 100;
const int kPtsIncrement      = 3600; // 90kHz / 25fps

// 支持的视频编码类型（音频/其他流忽略）
bool isVideoCodec(int codecid)
{
    return codecid == PSI_STREAM_VIDEO_SVAC
        || codecid == PSI_STREAM_H264
        || codecid == PSI_STREAM_H265;
}

// 转码输出 stream_type
EPSI_STREAM_TYPE transcodeStreamType(const TransferConfig &cfg)
{
    return (cfg.transcodeFormat == "h265" || cfg.transcodeFormat == "H265")
        ? PSI_STREAM_H265
        : PSI_STREAM_H264;
}

YUVEncoder::CodecType encoderCodecType(const TransferConfig &cfg)
{
    return (cfg.transcodeFormat == "h265" || cfg.transcodeFormat == "H265")
        ? YUVEncoder::CodecType::H265
        : YUVEncoder::CodecType::H264;
}
} // namespace

TransferServer::TransferServer()
    : _running(false)
{
}

TransferServer::~TransferServer()
{
    stop();
}

bool TransferServer::start(const TransferConfig &cfg)
{
    if (_running) {
        BLOG(warning) << "[TransferServer] already running";
        return true;
    }

    _cfg = cfg;
    _pts = 0;

    // ===== 第一步：尽早启动 NetworkReceiver，使 socket 进入监听状态 =====
    _networkReceiver = std::make_shared<NetworkReceiver>(_cfg.protocol, _cfg.recvIp, _cfg.recvPort);
    if (!_networkReceiver->start()) {
        BLOG(error) << "[TransferServer] NetworkReceiver start failed";
        return false;
    }

    // ===== 第二步：按需初始化解密器 =====
    if (_cfg.decrypt) {
        VKEKInfo vkekInfo;
        vkekInfo.vkek_version = _cfg.vkekVersion;
        memset(vkekInfo.vkek, 0, sizeof(vkekInfo.vkek));
        memcpy(vkekInfo.vkek, _cfg.vkek, sizeof(vkekInfo.vkek));
        _decryptor.setVkekInfo(vkekInfo);
        BLOG(info) << "[TransferServer] decrypt enabled, vkek_version=" << _cfg.vkekVersion;
    }

    // ===== 第三步：创建发送器 =====
    _psSender = std::make_unique<PSSender>(_cfg.outProtocol, _cfg.dstIp, _cfg.dstPort, _cfg.ssrc);

    // ===== 第四步：搭建回调链 =====
    // RTP 输入 -> RtpReceiver
    _networkReceiver->setCallback([this](const uint8_t *data, size_t len) {
        if (_cfg.protocol == Protocol::TCP) {
            _rtpReceiver.inputTcp(data, len);
        } else {
            _rtpReceiver.inputUdp(data, len);
        }
    });

    // RTP 重组出 PS -> PsDemuxer 解复用出 ES
    _rtpReceiver.setPsCallback([this](const uint8_t *psData, size_t psLen) {
        _demuxer.input(psData, psLen);
    });

    // 解复用出 ES 数据包 -> （SVAC 解密） -> onVideoFrame
    _demuxer.setPacketCallback([this](int codecid, int flags, int64_t /*pts*/, int64_t /*dts*/,
                                      const uint8_t *data, size_t len) {
        if (!isVideoCodec(codecid)) {
            return; // 忽略音频及其他非视频流
        }
        bool keyFrame = (flags & MPEG_FLAG_IDR_FRAME) != 0;
        // 只有 SVAC 视频涉及加解密
        if (_cfg.decrypt && codecid == PSI_STREAM_VIDEO_SVAC) {
            _decryptor.input(data, len);
        } else {
            onVideoFrame(codecid, const_cast<uint8_t *>(data), len, keyFrame);
        }
    });

    // 解密后的 SVAC 帧 -> onVideoFrame
    _decryptor.setFrameCallback([this](const uint8_t *frameData, size_t frameLength, bool isKeyFrame) {
        onVideoFrame(PSI_STREAM_VIDEO_SVAC, const_cast<uint8_t *>(frameData), frameLength, isKeyFrame);
    });

    // ===== 第五步：启动发送线程（转码路径），否则直接标记运行 =====
    _running = true;
    if (_cfg.transcode) {
        _sendThread = std::thread(&TransferServer::sendLoop, this);
    }

    BLOG(info) << "[TransferServer] started: recv " << _cfg.recvIp << ":" << _cfg.recvPort
               << " -> send " << _cfg.dstIp << ":" << _cfg.dstPort
               << ", decrypt=" << (_cfg.decrypt ? "on" : "off")
               << ", transcode=" << (_cfg.transcode ? _cfg.transcodeFormat : "off");
    return true;
}

void TransferServer::stop()
{
    bool wasRunning = _running.exchange(false);
    if (_networkReceiver)
        _networkReceiver->stop();
    if (_sendThread.joinable())
        _sendThread.join();
    if (wasRunning)
        BLOG(info) << "[TransferServer] stopped";
}

void TransferServer::onVideoFrame(int codecid, uint8_t *data, size_t len, bool keyFrame)
{
    if (!data || len == 0)
        return;

    if (!_cfg.transcode) {
        // 透传：按原始编码重新封装
        if (!_psSender)
            return;
        _pts += kPtsIncrement;
        _psSender->sendData(data, len, static_cast<EPSI_STREAM_TYPE>(codecid), _pts, _pts, keyFrame);
        return;
    }

    // 转码：解码 -> 编码
    Frame f;
    f.data.assign(data, data + len);
    f.size       = len;
    f.keyFrame   = keyFrame;
    f.width      = 0;
    f.height     = 0;
    f.timestamp  = 0;

    if (codecid == PSI_STREAM_VIDEO_SVAC) {
        if (!_svacDecoder) {
            _svacDecoder = std::make_unique<SVACDecoder>(0);
            if (!_svacDecoder->loadLibrary(_cfg.decoderLibPath)) {
                BLOG(error) << "[TransferServer] load SVAC decoder library failed: " << _cfg.decoderLibPath;
                _svacDecoder.reset();
                return;
            }
            _svacDecoder->setOnDecode([this](const Frame &frame) { onDecodedYuv(frame); });
        }
        _svacDecoder->inputFrame(f);
    } else if (codecid == PSI_STREAM_H264 || codecid == PSI_STREAM_H265) {
        if (!_ffmpegDecoder) {
            FFmpegDecoder::Codec codec = (codecid == PSI_STREAM_H265)
                ? FFmpegDecoder::Codec::H265
                : FFmpegDecoder::Codec::H264;
            _ffmpegDecoder = std::make_unique<FFmpegDecoder>(codec);
            _ffmpegDecoder->setOnDecode([this](const Frame &frame) { onDecodedYuv(frame); });
        }
        _ffmpegDecoder->inputFrame(f);
    }
    // 其他视频编码：暂不支持转码，忽略
}

void TransferServer::onDecodedYuv(const Frame &frame)
{
    YUVEncoder::CodecType codecType = encoderCodecType(_cfg);

    std::lock_guard<std::mutex> lk(_poolMutex);
    if (!_framePool) {
        int w = frame.width > 0 ? frame.width : _cfg.width;
        int h = frame.height > 0 ? frame.height : _cfg.height;
        _framePool  = std::make_unique<FramePool>(kFramePoolCapacity, w, h);
        _yuvEncoder = std::make_unique<YUVEncoder>(codecType, w, h, _cfg.fps, _cfg.bitrate);
    }
    FramePool *pool = _framePool.get();
    if (!pool)
        return;
    Frame *slot = pool->getWriteFrame();
    if (!slot)
        return;
    slot->data      = frame.data;
    slot->size      = frame.data.size();
    slot->width     = frame.width;
    slot->height    = frame.height;
    slot->keyFrame  = frame.keyFrame;
    slot->timestamp = frame.timestamp;
    pool->commitWrite();
}

void TransferServer::sendLoop()
{
    bool encoderCallbackSet = false;
    EPSI_STREAM_TYPE streamType = transcodeStreamType(_cfg);

    while (_running) {
        Frame *frame    = nullptr;
        YUVEncoder *enc = nullptr;
        {
            std::lock_guard<std::mutex> lk(_poolMutex);
            if (_framePool)
                frame = _framePool->getFrame();
            enc = _yuvEncoder.get();

            if (enc && !encoderCallbackSet) {
                enc->setOnEncoded([this, streamType](const Frame &encoded) {
                    if (!_psSender || encoded.data.empty())
                        return;
                    std::vector<uint8_t> buf(encoded.data.size());
                    memcpy(buf.data(), encoded.data.data(), encoded.data.size());
                    _pts += kPtsIncrement;
                    _psSender->sendData(buf.data(), buf.size(), streamType, _pts, _pts, encoded.keyFrame);
                });
                encoderCallbackSet = true;
            }
        }

        if (frame && enc) {
            enc->inputFrame(*frame);
            {
                std::lock_guard<std::mutex> lk(_poolMutex);
                if (_framePool)
                    _framePool->releaseFrame();
            }
        } else if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
