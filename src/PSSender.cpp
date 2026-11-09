#include "PSSender.h"
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "logger.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

PSSender::PSSender(Protocol protocol, std::string dstIp, int dstPort, uint32_t ssrc)
    : _protocol(protocol)
    , _dstIp(std::move(dstIp))
    , _dstPort(dstPort)
    , _ssrc(ssrc)
{
    createSocket();
    createRtpEncoder();
}

PSSender::~PSSender()
{
    destroyRtpEncoder();
    if (_psMuxer) {
        ps_muxer_destroy(_psMuxer);
        _psMuxer = nullptr;
    }
    if (_sendSocket >= 0) {
        close(_sendSocket);
        _sendSocket = -1;
    }
}

void *PSSender::psAlloc(void * /*param*/, size_t bytes)
{
    return malloc(bytes);
}

void PSSender::psFree(void * /*param*/, void *packet)
{
    free(packet);
}

void *PSSender::rtpAlloc(void * /*param*/, int bytes)
{
    return malloc(bytes);
}

void PSSender::rtpFree(void * /*param*/, void *packet)
{
    free(packet);
}

// ps_muxer 输出 PS 包 -> 送入 RTP 负载编码器
int PSSender::onPsPacket(void *param, int /*stream*/, void *packet, size_t bytes)
{
    PSSender *self = static_cast<PSSender *>(param);
    return rtp_payload_encode_input(self->_rtpEncoder, packet, static_cast<int>(bytes),
                                    static_cast<uint32_t>(self->_currentPts));
}

// RTP 负载编码器输出 RTP 包 -> 通过 socket 发送
int PSSender::onRtpPacket(void *param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
    PSSender *self = static_cast<PSSender *>(param);
    return self->sendRtpPacket(static_cast<const uint8_t *>(packet), static_cast<size_t>(bytes)) ? 0 : -1;
}

bool PSSender::createRtpEncoder()
{
    struct rtp_payload_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.alloc  = &PSSender::rtpAlloc;
    handler.free   = &PSSender::rtpFree;
    handler.packet = &PSSender::onRtpPacket;

    _rtpEncoder = rtp_payload_encode_create(RTP_PAYLOAD_MP2P, "MP2P",
                                            static_cast<uint16_t>(_ssrc), _ssrc, &handler, this);
    if (!_rtpEncoder) {
        BLOG(error) << "[PSSender] rtp_payload_encode_create failed";
        return false;
    }
    return true;
}

void PSSender::destroyRtpEncoder()
{
    if (_rtpEncoder) {
        rtp_payload_encode_destroy(_rtpEncoder);
        _rtpEncoder = nullptr;
    }
}

bool PSSender::ensureStream(EPSI_STREAM_TYPE streamType)
{
    if (_psMuxer && _streamType == streamType) {
        return true;
    }
    if (_psMuxer) {
        ps_muxer_destroy(_psMuxer);
        _psMuxer = nullptr;
    }

    struct ps_muxer_func_t func;
    memset(&func, 0, sizeof(func));
    func.alloc = &PSSender::psAlloc;
    func.free  = &PSSender::psFree;
    func.write = &PSSender::onPsPacket;

    _psMuxer = ps_muxer_create(&func, this);
    if (!_psMuxer) {
        BLOG(error) << "[PSSender] ps_muxer_create failed";
        return false;
    }
    _stream = ps_muxer_add_stream(_psMuxer, static_cast<int>(streamType), nullptr, 0);
    if (_stream <= 0) {
        BLOG(error) << "[PSSender] ps_muxer_add_stream failed for stream type " << static_cast<int>(streamType);
        return false;
    }
    _streamType = streamType;
    return true;
}

bool PSSender::sendData(const uint8_t *pData, size_t dataSize, EPSI_STREAM_TYPE streamType,
                        uint64_t pts, uint64_t dts, bool isKeyFrame)
{
    if (!pData || dataSize == 0) {
        return false;
    }
    if (!ensureStream(streamType)) {
        return false;
    }

    int flags = isKeyFrame ? MPEG_FLAG_IDR_FRAME : 0;
    _currentPts = pts;
    int r = ps_muxer_input(_psMuxer, _stream, flags, static_cast<int64_t>(pts), static_cast<int64_t>(dts), pData, dataSize);
    if (r != 0) {
        BLOG(error) << "[PSSender] ps_muxer_input failed, r=" << r;
        return false;
    }
    return true;
}

bool PSSender::createSocket()
{
    _sendSocket = (_protocol == Protocol::TCP) ? socket(AF_INET, SOCK_STREAM, 0) : socket(AF_INET, SOCK_DGRAM, 0);
    if (_sendSocket == -1) {
        BLOG(error) << "[PSSender] create socket failed: " << strerror(errno);
        return false;
    }

    int ul = 1;
    ioctl(_sendSocket, FIONBIO, &ul);

    memset(&_dstAddr, 0, sizeof(_dstAddr));
    _dstAddr.sin_family      = AF_INET;
    _dstAddr.sin_addr.s_addr = inet_addr(_dstIp.c_str());
    _dstAddr.sin_port        = htons(_dstPort);

    if (_protocol == Protocol::TCP) {
        int connRet = connect(_sendSocket, reinterpret_cast<struct sockaddr *>(&_dstAddr), sizeof(_dstAddr));
        if (connRet == 0) {
            BLOG(info) << "[PSSender] TCP connected to " << _dstIp << ":" << _dstPort;
        } else if (errno == EINPROGRESS) {
            BLOG(info) << "[PSSender] TCP connecting to " << _dstIp << ":" << _dstPort << " (in progress)...";
        } else {
            BLOG(error) << "[PSSender] TCP connect failed: " << strerror(errno);
        }
    }
    return true;
}

bool PSSender::sendRtpPacket(const uint8_t *buf, size_t len)
{
    if (_protocol == Protocol::TCP) {
        // RTSP 交织式：2 字节长度域 + RTP 包
        if (len + TCP_RTP_LEN_FIELD > sizeof(_tcpSendBuf)) {
            BLOG(error) << "[PSSender] RTP packet too large: " << len;
            return false;
        }
        _tcpSendBuf[0] = static_cast<uint8_t>((len >> 8) & 0xFF);
        _tcpSendBuf[1] = static_cast<uint8_t>(len & 0xFF);
        memcpy(_tcpSendBuf + TCP_RTP_LEN_FIELD, buf, len);

        const uint8_t *p = _tcpSendBuf;
        size_t remaining = len + TCP_RTP_LEN_FIELD;
        const int maxWaitMs  = 5000;
        const int maxRetries = 50;

        for (int retries = 0; remaining > 0 && retries < maxRetries;) {
            ssize_t nRet = send(_sendSocket, p, remaining, 0);
            if (nRet > 0) {
                p += nRet;
                remaining -= static_cast<size_t>(nRet);
                retries = 0;
                continue;
            }
            if (nRet == 0) {
                BLOG(error) << "[PSSender] TCP send returned 0, connection closed";
                return false;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(_sendSocket, &wfds);
                struct timeval tv = {maxWaitMs / 1000, (maxWaitMs % 1000) * 1000};
                int sel = select(_sendSocket + 1, nullptr, &wfds, nullptr, &tv);
                if (sel <= 0) {
                    BLOG(error) << "[PSSender] TCP send: select wait writable failed or timeout";
                    return false;
                }
                retries++;
                continue;
            }
            BLOG(error) << "[PSSender] TCP send failed: " << strerror(errno);
            return false;
        }
        return remaining == 0;
    }

    ssize_t nRet = sendto(_sendSocket, buf, len, 0, reinterpret_cast<const struct sockaddr *>(&_dstAddr), sizeof(_dstAddr));
    if (nRet <= 0 || static_cast<size_t>(nRet) != len) {
        BLOG(error) << "[PSSender] UDP send failed: len=" << len << " ret=" << nRet;
        return false;
    }
    return true;
}
