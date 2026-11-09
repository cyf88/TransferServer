#pragma once

#include "SVACDecoder.h"
#include <functional>
#include <memory>
#include <string>

/**
 * YUV 编码器：将 YUV 帧编码为 H.264 或 H.265，通过回调输出
 * 使用 FFmpeg libavcodec + libx264/libx265
 */
class YUVEncoder {
public:
    enum class CodecType {
        H264,
        H265,
    };

    YUVEncoder(CodecType codec, int width, int height, int fps = 25, int bitrate = 2000000);
    ~YUVEncoder();

    // 禁止拷贝
    YUVEncoder(const YUVEncoder &)            = delete;
    YUVEncoder &operator=(const YUVEncoder &) = delete;

    /** 编码回调：输出 H.264/H.265 的 Frame（data 为 NAL 数据，keyFrame 表示是否为关键帧） */
    using OnEncoded = std::function<void(const Frame &frame)>;

    /** 输入一帧 YUV（YUV420P，data 布局：Y + U + V） */
    bool inputFrame(const Frame &frame);

    /** 设置编码输出回调 */
    void setOnEncoded(OnEncoded cb);

    /** 刷新编码器内部缓冲，获取剩余数据（结束时调用） */
    bool flush();

    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    CodecType getCodecType() const { return _codecType; }

private:
    bool openEncoder();
    void closeEncoder();

    CodecType _codecType;
    int _width;
    int _height;
    int _fps;
    int _bitrate;

    OnEncoded _onEncoded;

    struct Impl;
    std::unique_ptr<Impl> _impl;
};
