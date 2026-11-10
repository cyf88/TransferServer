#ifndef TRANSFER_SERVER_H
#define TRANSFER_SERVER_H

#include "Decrypter.h"
#include "FFmpegDecoder.h"
#include "FramePool.h"
#include "NetworkReceiver.h"
#include "PSSender.h"
#include "PsDemuxer.h"
#include "RtpReceiver.h"
#include "SVACDecoder.h"
#include "YUVEncoder.h"
#include "mpeg-ps.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

/**
 * 转发配置。由外部 HTTP 接口下发，控制一次转发的完整行为：
 * 是否解密、是否转码、转码格式、解密密钥等。
 */
struct TransferConfig {
    // ===== 输入（PS over RTP）=====
    Protocol protocol = Protocol::TCP;   // 输入传输协议 TCP / UDP
    std::string recvIp;                  // 输入监听 IP
    uint16_t recvPort = 0;               // 输入监听端口

    // ===== 输出（PS over RTP）=====
    std::string dstIp;                   // 输出目标 IP
    int dstPort = 0;                     // 输出目标端口
    Protocol outProtocol = Protocol::TCP; // 输出传输协议
    uint32_t ssrc = 0x12345678;          // RTP SSRC

    // ===== 处理开关 =====
    bool decrypt = false;                // 是否解密
    bool transcode = false;              // 是否转码（SVAC -> H.264/H.265）
    std::string transcodeFormat = "h264"; // 转码格式："h264" | "h265"

    // ===== 解密密钥 =====
    std::string vkekVersion = "1.0";     // 密钥版本
    unsigned char vkek[16] = {0};        // 解密密钥（16 字节）

    // ===== 转码参数 =====
    int fps = 25;
    int bitrate = 2000000;
    int width = 1920;
    int height = 1080;

    // ===== SVAC 解码库路径（转码时使用）=====
    std::string decoderLibPath = "./sdk/libZXSvacDec.so";

    // ===== 保存到文件（此时不对外推送）=====
    bool saveToFile = false;                 // 是否保存到文件（开启后不创建发送器）
    std::string recvFilePath;                // 接收流(PS)保存路径
    std::string outFilePath;                 // 解密/转码后裸流(ES)保存路径
};

/**
 * TransferServer 核心类。
 *
 * 处理流水线：
 *   RTP 输入 -> RtpReceiver -> PsDemuxer（PS 解复用）
 *   -> [Decrypter 解密]（仅 SVAC 视频，可选）
 *   -> [解码 + YUVEncoder 转码]（可选，否则透传原编码）
 *   -> PSSender（重新封装为 PS over RTP 并推送）
 */
class TransferServer {
public:
    TransferServer();
    ~TransferServer();

    /** 启动转发。传入配置；若已在运行则忽略并返回 true。 */
    bool start(const TransferConfig &cfg);
    void stop();
    bool isRunning() const { return _running.load(); }

    /** 获取实际收流端口（由服务分配；仅在 start() 后有效） */
    uint16_t getRecvPort() const { return _networkReceiver ? _networkReceiver->getPort() : 0; }

    /** 获取当前转发配置（只读，start() 后有效） */
    const TransferConfig &config() const { return _cfg; }

    /** 当前接收到的输入视频编码类型码（PSI_STREAM_*，尚未收到时为 -1） */
    int inputCodecid() const { return _inputCodecid.load(); }

private:
    /** 转码路径发送线程：从 FramePool 取 YUV 帧，编码后通过 PSSender 发送 */
    void sendLoop();

    /** 收到一帧“明文视频帧”后的统一处理入口（codecid 为输入编码类型） */
    void onVideoFrame(int codecid, uint8_t *data, size_t len, bool keyFrame);

    /** 解码器输出 YUV 帧 -> FramePool（转码路径共用） */
    void onDecodedYuv(const Frame &frame);

private:
    TransferConfig _cfg;

    RtpReceiver _rtpReceiver;
    PsDemuxer _demuxer;
    Decrypter _decryptor;
    std::unique_ptr<SVACDecoder> _svacDecoder;
    std::unique_ptr<FFmpegDecoder> _ffmpegDecoder;

    std::shared_ptr<NetworkReceiver> _networkReceiver;
    std::unique_ptr<PSSender> _psSender;
    std::unique_ptr<FramePool> _framePool;
    std::unique_ptr<YUVEncoder> _yuvEncoder;

    std::atomic<bool> _running{false};
    std::thread _sendThread;
    std::mutex _poolMutex;

    uint64_t _pts = 0; // 90kHz 时间戳
    std::atomic<int> _inputCodecid{-1}; // 接收到的输入视频编码类型（PSI_STREAM_*）

    // ===== 文件保存（saveToFile 模式）=====
    std::FILE *_recvFile = nullptr; // 接收流(PS)文件句柄
    std::FILE *_outFile = nullptr;  // 解密/转码后裸流(ES)文件句柄
    std::mutex _fileMutex;

    void openSaveFiles();
    void writeRecvStream(const uint8_t *data, size_t len);
    void writeOutStream(const uint8_t *data, size_t len);
    void closeSaveFiles();
};

#endif // TRANSFER_SERVER_H
