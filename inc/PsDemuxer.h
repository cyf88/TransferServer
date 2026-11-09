#ifndef PS_DEMUXER_H
#define PS_DEMUXER_H

#include <cstddef>
#include <cstdint>
#include <functional>

struct ps_demuxer_t;

/**
 * PS 流解析器封装（基于 media-server libmpeg 的 ps_demuxer）。
 * 从 PS 流中解复用出 ES 数据包。
 */
class PsDemuxer {
public:
    /**
     * ES 数据包回调
     * @param codecid 流类型，参考 EPSI_STREAM_TYPE（如 PSI_STREAM_VIDEO_SVAC / PSI_STREAM_H264）
     * @param flags   标志位，参考 MPEG_FLAG_*（含 MPEG_FLAG_IDR_FRAME）
     * @param pts     显示时间戳（90kHz）
     * @param dts     解码时间戳（90kHz）
     * @param data    ES 数据
     * @param len     ES 数据长度
     */
    using PacketCallback = std::function<void(int codecid, int flags, int64_t pts, int64_t dts,
                                              const uint8_t *data, size_t len)>;

    PsDemuxer();
    ~PsDemuxer();

    PsDemuxer(const PsDemuxer &)            = delete;
    PsDemuxer &operator=(const PsDemuxer &) = delete;

    void setPacketCallback(PacketCallback cb);

    /** 输入 PS 流数据片段（内部自动缓存不完整数据） */
    bool input(const uint8_t *data, size_t len);

    /** 重建 demuxer，清空内部状态 */
    void reset();

private:
    static int onPacket(void *param, int stream, int codecid, int flags, int64_t pts, int64_t dts,
                        const void *data, size_t bytes);

    struct ps_demuxer_t *_demuxer = nullptr;
    PacketCallback _cb;
};

#endif // PS_DEMUXER_H
