#include "PsDemuxer.h"
#include "mpeg-ps.h"
#include "logger.h"

#include <utility>

PsDemuxer::PsDemuxer()
{
    _demuxer = ps_demuxer_create(&PsDemuxer::onPacket, this);
    if (!_demuxer) {
        BLOG(error) << "[PsDemuxer] ps_demuxer_create failed";
    }
}

PsDemuxer::~PsDemuxer()
{
    if (_demuxer) {
        ps_demuxer_destroy(_demuxer);
        _demuxer = nullptr;
    }
}

void PsDemuxer::setPacketCallback(PacketCallback cb)
{
    _cb = std::move(cb);
}

int PsDemuxer::onPacket(void *param, int /*stream*/, int codecid, int flags, int64_t pts, int64_t dts,
                        const void *data, size_t bytes)
{
    PsDemuxer *self = static_cast<PsDemuxer *>(param);
    if (self->_cb) {
        self->_cb(codecid, flags, pts, dts, static_cast<const uint8_t *>(data), bytes);
    }
    return 0;
}

bool PsDemuxer::input(const uint8_t *data, size_t len)
{
    if (!_demuxer || !data || len == 0) {
        return false;
    }
    int n = ps_demuxer_input(_demuxer, data, len);
    if (n < 0) {
        BLOG(error) << "[PsDemuxer] ps_demuxer_input error: " << n;
        return false;
    }
    return true;
}

void PsDemuxer::reset()
{
    if (_demuxer) {
        ps_demuxer_destroy(_demuxer);
        _demuxer = nullptr;
    }
    _demuxer = ps_demuxer_create(&PsDemuxer::onPacket, this);
}
