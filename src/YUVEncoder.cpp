#include "YUVEncoder.h"
#include "logger.h"
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

struct YUVEncoder::Impl {
    AVCodecContext *enc_ctx = nullptr;
    AVFrame *frame          = nullptr;
    AVPacket *pkt           = nullptr;
    int64_t pts             = 0;
};

YUVEncoder::YUVEncoder(CodecType codec, int width, int height, int fps, int bitrate)
    : _codecType(codec)
    , _width(width)
    , _height(height)
    , _fps(fps)
    , _bitrate(bitrate)
    , _impl(std::make_unique<Impl>())
{
    if (!openEncoder()) {
        BLOG(error) << "YUVEncoder: openEncoder failed";
    }
}

YUVEncoder::~YUVEncoder()
{
    flush();
    closeEncoder();
}

bool YUVEncoder::openEncoder()
{
    AVCodecID codecId    = (_codecType == CodecType::H265) ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    const AVCodec *codec = avcodec_find_encoder(codecId);
    if (!codec) {
        BLOG(error) << "YUVEncoder: avcodec_find_encoder failed for " << (codecId == AV_CODEC_ID_HEVC ? "HEVC" : "H264");
        return false;
    }

    _impl->enc_ctx = avcodec_alloc_context3(codec);
    if (!_impl->enc_ctx) {
        BLOG(error) << "YUVEncoder: avcodec_alloc_context3 failed";
        return false;
    }

    _impl->enc_ctx->width        = _width;
    _impl->enc_ctx->height       = _height;
    _impl->enc_ctx->time_base    = (AVRational) { 1, _fps };
    _impl->enc_ctx->framerate    = (AVRational) { _fps, 1 };
    _impl->enc_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
    _impl->enc_ctx->bit_rate     = _bitrate;
    _impl->enc_ctx->gop_size     = _fps * 2; // I frame every 2 seconds
    _impl->enc_ctx->max_b_frames = 0; // no B frames for low latency

    if (codecId == AV_CODEC_ID_H264) {
        av_opt_set(_impl->enc_ctx->priv_data, "preset", "medium", 0);
        av_opt_set(_impl->enc_ctx->priv_data, "tune", "zerolatency", 0);
    }

    int ret = avcodec_open2(_impl->enc_ctx, codec, nullptr);
    if (ret < 0) {
        BLOG(error) << "YUVEncoder: avcodec_open2 failed, ret=" << ret;
        avcodec_free_context(&_impl->enc_ctx);
        _impl->enc_ctx = nullptr;
        return false;
    }

    _impl->frame = av_frame_alloc();
    if (!_impl->frame) {
        BLOG(error) << "YUVEncoder: av_frame_alloc failed";
        closeEncoder();
        return false;
    }
    _impl->frame->format = AV_PIX_FMT_YUV420P;
    _impl->frame->width  = _width;
    _impl->frame->height = _height;
    ret                  = av_frame_get_buffer(_impl->frame, 32);
    if (ret < 0) {
        BLOG(error) << "YUVEncoder: av_frame_get_buffer failed, ret=" << ret;
        closeEncoder();
        return false;
    }

    _impl->pkt = av_packet_alloc();
    if (!_impl->pkt) {
        BLOG(error) << "YUVEncoder: av_packet_alloc failed";
        closeEncoder();
        return false;
    }

    BLOG(info) << "YUVEncoder opened: " << _width << "x" << _height << " codec=" << (_codecType == CodecType::H265 ? "H265" : "H264") << " fps=" << _fps
               << " bitrate=" << _bitrate;
    return true;
}

void YUVEncoder::closeEncoder()
{
    if (_impl->pkt) {
        av_packet_free(&_impl->pkt);
        _impl->pkt = nullptr;
    }
    if (_impl->frame) {
        av_frame_free(&_impl->frame);
        _impl->frame = nullptr;
    }
    if (_impl->enc_ctx) {
        avcodec_free_context(&_impl->enc_ctx);
        _impl->enc_ctx = nullptr;
    }
}

void YUVEncoder::setOnEncoded(OnEncoded cb)
{
    _onEncoded = std::move(cb);
}

static bool copyPacketToFrame(const AVPacket *pkt, int width, int height, bool keyFrame, uint64_t timestamp, Frame &out)
{
    if (!pkt || pkt->size <= 0 || !pkt->data) {
        return false;
    }
    out.data.assign(pkt->data, pkt->data + pkt->size);
    out.size      = pkt->size;
    out.width     = width;
    out.height    = height;
    out.keyFrame  = keyFrame;
    out.timestamp = timestamp;
    return true;
}

bool YUVEncoder::inputFrame(const Frame &frame)
{
    if (!_impl->enc_ctx || !_impl->frame || !_impl->pkt) {
        BLOG(error) << "YUVEncoder: encoder not opened";
        return false;
    }

    // 分辨率变化：刷新后重建编码器
    if (frame.width != _width || frame.height != _height) {
        int newW = frame.width > 0 ? frame.width : 1920;
        int newH = frame.height > 0 ? frame.height : 1080;
        flush();
        closeEncoder();
        _width  = newW;
        _height = newH;
        if (!openEncoder()) {
            BLOG(error) << "YUVEncoder: reopen after resolution change failed";
            return false;
        }
        BLOG(info) << "YUVEncoder: resolution changed to " << _width << "x" << _height;
    }

    size_t ySize  = static_cast<size_t>(_width) * _height;
    size_t uvSize = ySize / 4;
    if (frame.size < ySize + uvSize + uvSize) {
        BLOG(error) << "YUVEncoder: frame data too small " << frame.size << " expected " << (ySize + uvSize * 2);
        return false;
    }

    // 确保可写
    int ret = av_frame_make_writable(_impl->frame);
    if (ret < 0) {
        BLOG(error) << "YUVEncoder: av_frame_make_writable failed";
        return false;
    }

    const uint8_t *src = frame.data.data();
    std::memcpy(_impl->frame->data[0], src, ySize);
    std::memcpy(_impl->frame->data[1], src + ySize, uvSize);
    std::memcpy(_impl->frame->data[2], src + ySize + uvSize, uvSize);

    _impl->frame->pts = _impl->pts++;

    ret = avcodec_send_frame(_impl->enc_ctx, _impl->frame);
    if (ret < 0) {
        BLOG(error) << "YUVEncoder: avcodec_send_frame failed, ret=" << ret;
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(_impl->enc_ctx, _impl->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            BLOG(error) << "YUVEncoder: avcodec_receive_packet failed, ret=" << ret;
            return false;
        }

        bool keyFrame = (_impl->pkt->flags & AV_PKT_FLAG_KEY) != 0;
        Frame outFrame;
        if (copyPacketToFrame(_impl->pkt, _width, _height, keyFrame, frame.timestamp, outFrame) && _onEncoded) {
            _onEncoded(outFrame);
        }
        av_packet_unref(_impl->pkt);
    }

    return true;
}

bool YUVEncoder::flush()
{
    if (!_impl->enc_ctx || !_impl->pkt) {
        return true;
    }

    int ret = avcodec_send_frame(_impl->enc_ctx, nullptr);
    if (ret < 0) {
        BLOG(error) << "YUVEncoder: flush send_frame failed, ret=" << ret;
        return false;
    }

    while (true) {
        ret = avcodec_receive_packet(_impl->enc_ctx, _impl->pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret == AVERROR(EAGAIN)) {
            break;
        }
        if (ret < 0) {
            BLOG(error) << "YUVEncoder: flush receive_packet failed, ret=" << ret;
            return false;
        }
        bool keyFrame = (_impl->pkt->flags & AV_PKT_FLAG_KEY) != 0;
        Frame outFrame;
        if (copyPacketToFrame(_impl->pkt, _width, _height, keyFrame, 0, outFrame) && _onEncoded) {
            _onEncoded(outFrame);
        }
        av_packet_unref(_impl->pkt);
    }

    return true;
}
