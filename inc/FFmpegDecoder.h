#ifndef FFMPEG_DECODER_H
#define FFMPEG_DECODER_H

#include "SVACDecoder.h" // Frame

#include <functional>
#include <memory>

/**
 * 通用视频解码器（H.264 / H.265），基于 FFmpeg libavcodec。
 * 输入 Annex-B 的 ES 帧，输出 YUV420P 的 Frame。
 */
class FFmpegDecoder {
public:
    enum class Codec { H264, H265 };

    using OnDecode = std::function<void(const Frame &frame)>;

    FFmpegDecoder(Codec codec);
    ~FFmpegDecoder();

    FFmpegDecoder(const FFmpegDecoder &)            = delete;
    FFmpegDecoder &operator=(const FFmpegDecoder &) = delete;

    /** 输入一帧 H.264/H.265 ES（Annex-B）数据 */
    bool inputFrame(const Frame &frame);

    /** 解码输出回调：输出 YUV420P 的 Frame */
    void setOnDecode(OnDecode cb);

private:
    bool openDecoder();
    void closeDecoder();

    Codec _codec;
    OnDecode _cb;

    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // FFMPEG_DECODER_H
