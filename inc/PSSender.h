#ifndef PSSENDER_H
#define PSSENDER_H

#include "NetworkReceiver.h"  // Protocol
#include "mpeg-ps.h"          // ps_muxer_t, EPSI_STREAM_TYPE, MPEG_FLAG_*

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstddef>
#include <cstdint>
#include <string>

#define TCP_RTP_LEN_FIELD 2  // TCP 交织 RTP 前的 2 字节长度域
#define TCP_SEND_BUF_SIZE 2048

/**
 * PS 发送器：将 ES 数据封装为 PS（media-server libmpeg 的 ps_muxer），
 * 再通过 RTP（media-server librtp 的 rtp_payload_encode，MP2P 负载）推送。
 */
class PSSender {
public:
    PSSender(Protocol protocol, std::string dstIp, int dstPort, uint32_t ssrc);
    ~PSSender();

    PSSender(const PSSender &)            = delete;
    PSSender &operator=(const PSSender &) = delete;

    /** 发送一帧 ES 数据 */
    bool sendData(const uint8_t *pData, size_t dataSize, EPSI_STREAM_TYPE streamType,
                  uint64_t pts, uint64_t dts, bool isKeyFrame);

private:
    bool createSocket();
    bool sendRtpPacket(const uint8_t *buf, size_t len); // 含 TCP 长度前缀处理

    // ps_muxer 回调
    static void *psAlloc(void *param, size_t bytes);
    static void psFree(void *param, void *packet);
    static int onPsPacket(void *param, int stream, void *packet, size_t bytes);

    // rtp_payload 回调
    static void *rtpAlloc(void *param, int bytes);
    static void rtpFree(void *param, void *packet);
    static int onRtpPacket(void *param, const void *packet, int bytes, uint32_t timestamp, int flags);

    bool ensureStream(EPSI_STREAM_TYPE streamType);
    bool createRtpEncoder();
    void destroyRtpEncoder();

    Protocol _protocol;
    std::string _dstIp;
    int _dstPort;
    int _sendSocket{-1};
    struct sockaddr_in _dstAddr{};
    uint32_t _ssrc{0};

    uint8_t _tcpSendBuf[TCP_SEND_BUF_SIZE];

    struct ps_muxer_t *_psMuxer{nullptr};
    int _stream{-1};
    EPSI_STREAM_TYPE _streamType{PSI_STREAM_RESERVED};

    void *_rtpEncoder{nullptr};
    uint64_t _currentPts{0};
};

#endif // PSSENDER_H
