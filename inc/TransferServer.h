#ifndef TRANSFER_SERVER_H
#define TRANSFER_SERVER_H

#include "Decrypter.h"
#include "FFmpegDecoder.h"
#include "FramePool.h"
#include "NetworkReceiver.h"
#include "PSSender.h"
#include "PsDemuxer.h"
#include "RtpReceiver.h"
#include "SignatureVerifier.h"
#include "SVACDecoder.h"
#include "YUVEncoder.h"
#include "mpeg-ps.h"

#include <atomic>
#include <cstdint>
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
    int idleTimeoutSec = 0;   // 收流空闲超时（秒），0 表示不超时

    // ===== SVAC 解码库路径（转码时使用）=====
    std::string decoderLibPath = "./sdk/libZXSvacDec.so";

    // ===== 保存到文件（此时不对外推送）=====
    bool saveToFile = false;                 // 是否保存到文件（开启后不创建发送器）
    std::string recvFilePath;                // 接收流(PS)保存路径
    std::string outFilePath;                 // 解密/转码后裸流(ES)保存路径

    // ===== 流验签（软验签）=====
    bool verify = false;                     // 是否验签
    std::string channelId;                   // 通道 ID
    std::string certId;                      // 验签证书 ID（为空时回退使用 channelId）
    std::string certDir;                     // 证书目录（来自 config.ini）
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

    /** 最近一次 start() 失败原因（成功时为空） */
    const std::string &lastError() const { return _lastError; }

    /** 停止时间（steady_clock 毫秒；0 表示未停止） */
    int64_t stoppedMs() const { return _stoppedMs.load(); }

    /** 当前接收到的输入视频编码类型码（PSI_STREAM_*，尚未收到时为 -1） */
    int inputCodecid() const { return _inputCodecid.load(); }

    /** 原始输入码流是否加密：-1 未知，0 否，1 是（仅 SVAC 有效） */
    int inputEncrypted() const { return _inputEncrypted.load(); }

    /** 原始输入码流是否带签名：-1 未知，0 否，1 是（仅 SVAC 有效） */
    int inputSigned() const { return _inputSigned.load(); }

    /** 验签结果快照 */
    struct VerifySnapshot {
        bool enabled    = false;
        bool hasResult  = false;
        bool lastOk     = false;
        uint64_t success = 0;
        uint64_t fail    = 0;
        std::string detail;
        std::string lastTime;
    };

    /** 获取验签统计（线程安全） */
    VerifySnapshot verifyInfo() const;

    /** 实际输入流分辨率（转码输出分辨率与输入一致；尚未解码出帧时为 0） */
    int width() const { return _width.load(); }
    int height() const { return _height.load(); }

private:
    /** 转码路径发送线程：从 FramePool 取 YUV 帧，编码后通过 PSSender 发送 */
    void sendLoop();

    /** 收到一帧“明文视频帧”后的统一处理入口（codecid 为输入编码类型） */
    void onVideoFrame(int codecid, uint8_t *data, size_t len, bool keyFrame);

    /** 记录原始码流的安全标志（加密 / 签名） */
    void observeSecurity(bool sawNalu, bool encrypted, bool signedFlag);

    /** 记录一次验签结果 */
    void recordVerifyResult(bool ok, const std::string &detail);

    /** 解码器输出 YUV 帧 -> FramePool（转码路径共用） */
    void onDecodedYuv(const Frame &frame);

    /** 空闲超时看门狗线程：收流空闲超过 idleTimeoutSec 则自动停止 */
    void watchdogLoop();

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
    std::unique_ptr<SignatureVerifier> _signatureVerifier; // 软验签器

    std::atomic<bool> _running{false};
    std::string _lastError;
    std::atomic<int64_t> _stoppedMs{0}; // 停止时间（steady_clock 毫秒）
    std::thread _sendThread;
    std::mutex _poolMutex;

    uint64_t _pts = 0; // 90kHz 时间戳
    std::atomic<int> _inputCodecid{-1}; // 接收到的输入视频编码类型（PSI_STREAM_*）
    std::atomic<int> _inputEncrypted{-1}; // 原始码流是否加密：-1 未知 / 0 否 / 1 是
    std::atomic<int> _inputSigned{-1};    // 原始码流是否签名：-1 未知 / 0 否 / 1 是
    std::atomic<int> _width{0};   // 实际输入流分辨率（宽）
    std::atomic<int> _height{0};  // 实际输入流分辨率（高）

    // ===== 验签统计 =====
    mutable std::mutex _verifyMutex;
    VerifySnapshot _verifyStats;

    // ===== 空闲超时 =====
    std::atomic<int64_t> _lastRecvMs{0}; // 最近一次收流时间（毫秒）
    std::thread _watchdogThread;          // 空闲超时看门狗线程

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
