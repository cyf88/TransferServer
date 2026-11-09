#include "RtpReceiver.h"
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "logger.h"

#include <cstring>
#include <utility>

namespace {
const size_t kTcpLengthField = 2; // TCP 交织 RTP 前的 2 字节长度域
}

RtpReceiver::RtpReceiver()
{
    struct rtp_payload_t handler;
    memset(&handler, 0, sizeof(handler));
    handler.alloc  = nullptr; // 解码路径不调用 alloc/free
    handler.free   = nullptr;
    handler.packet = &RtpReceiver::onPacket;

    _decoder = rtp_payload_decode_create(RTP_PAYLOAD_MP2P, "MP2P", &handler, this);
    if (!_decoder) {
        BLOG(error) << "[RtpReceiver] rtp_payload_decode_create(MP2P) failed";
    }
}

RtpReceiver::~RtpReceiver()
{
    if (_decoder) {
        rtp_payload_decode_destroy(_decoder);
        _decoder = nullptr;
    }
}

void RtpReceiver::setPsCallback(PsCallback cb)
{
    _psCallback = std::move(cb);
}

int RtpReceiver::onPacket(void *param, const void *packet, int bytes, uint32_t /*timestamp*/, int /*flags*/)
{
    RtpReceiver *self = static_cast<RtpReceiver *>(param);
    if (self->_psCallback && packet && bytes > 0) {
        self->_psCallback(static_cast<const uint8_t *>(packet), static_cast<size_t>(bytes));
    }
    return 0;
}

bool RtpReceiver::inputUdp(const uint8_t *rtp, size_t len)
{
    if (!_decoder) {
        return false;
    }
    int r = rtp_payload_decode_input(_decoder, rtp, static_cast<int>(len));
    if (r < 0) {
        BLOG(error) << "[RtpReceiver] rtp_payload_decode_input failed, r=" << r;
        return false;
    }
    return true;
}

bool RtpReceiver::inputTcp(const uint8_t *data, size_t len)
{
    _tcpBuffer.insert(_tcpBuffer.end(), data, data + len);

    while (_tcpBuffer.size() >= kTcpLengthField) {
        uint16_t packetLen = static_cast<uint16_t>((_tcpBuffer[0] << 8) | _tcpBuffer[1]);
        if (_tcpBuffer.size() < static_cast<size_t>(packetLen) + kTcpLengthField) {
            break; // 数据不完整，等待更多
        }
        inputUdp(_tcpBuffer.data() + kTcpLengthField, packetLen);
        _tcpBuffer.erase(_tcpBuffer.begin(), _tcpBuffer.begin() + packetLen + kTcpLengthField);
    }
    return true;
}
