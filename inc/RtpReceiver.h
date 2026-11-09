#ifndef RTP_RECEIVER_H
#define RTP_RECEIVER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

/**
 * RTP 接收器：基于 media-server librtp 的 rtp_payload_decode（MP2P / PS 负载）
 * 将 RTP 包重组为 PS 字节流。
 */
class RtpReceiver {
public:
    using PsCallback = std::function<void(const uint8_t *data, size_t len)>;

    RtpReceiver();
    ~RtpReceiver();

    RtpReceiver(const RtpReceiver &)            = delete;
    RtpReceiver &operator=(const RtpReceiver &) = delete;

    void setPsCallback(PsCallback cb);

    /** TCP：输入字节流（2 字节长度域 + RTP 包，可能粘包/半包） */
    bool inputTcp(const uint8_t *data, size_t len);

    /** UDP：输入一个完整 RTP 包 */
    bool inputUdp(const uint8_t *rtp, size_t len);

private:
    static int onPacket(void *param, const void *packet, int bytes, uint32_t timestamp, int flags);

    void *_decoder{nullptr}; // rtp_payload_decode_create 返回的解码器
    std::vector<uint8_t> _tcpBuffer;
    PsCallback _psCallback;
};

#endif // RTP_RECEIVER_H
