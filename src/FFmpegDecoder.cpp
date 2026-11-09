#include "FFmpegDecoder.h"
#include "logger.h"

#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

struct FFmpegDecoder::Impl {
    AVCodecContext *ctx{nullptr};
    AVPacket *pkt{nullptr};
    AVFrame *frame{nullptr};
};

FFmpegDecoder::FFmpegDecoder(Codec codec)
    : _codec(codec)
    , _impl(std::make_unique<Impl>())
{
    if (!openDecoder()) {
        BLOG(error) << "[FFmpegDecoder] openDecoder failed";
    }
}

FFmpegDecoder::~FFmpegDecoder()
{
    closeDecoder();
}

bool FFmpegDecoder::openDecoder()
{
    AVCodecID id = (_codec == Codec::H265) ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    const AVCodec *codec = avcodec_find_decoder(id);
    if (!codec) {
        BLOG(error) << "[FFmpegDecoder] avcodec_find_decoder failed";
        return false;
    }

    _impl->ctx = avcodec_alloc_context3(codec);
    if (!_impl->ctx) {
        BLOG(error) << "[FFmpegDecoder] avcodec_alloc_context3 failed";
        return false;
    }
    _impl->ctx->thread_count = 0; // auto

    int ret = avcodec_open2(_impl->ctx, codec, nullptr);
    if (ret < 0) {
        BLOG(error) << "[FFmpegDecoder] avcodec_open2 failed, ret=" << ret;
        closeDecoder();
        return false;
    }

    _impl->pkt = av_packet_alloc();
    _impl->frame = av_frame_alloc();
    if (!_impl->pkt || !_impl->frame) {
        BLOG(error) << "[FFmpegDecoder] alloc packet/frame failed";
        closeDecoder();
        return false;
    }
    return true;
}

void FFmpegDecoder::closeDecoder()
{
    if (_impl->pkt) {
        av_packet_free(&_impl->pkt);
    }
    if (_impl->frame) {
        av_frame_free(&_impl->frame);
    }
    if (_impl->ctx) {
        avcodec_free_context(&_impl->ctx);
    }
}

void FFmpegDecoder::setOnDecode(OnDecode cb)
{
    _cb = std::move(cb);
}

bool FFmpegDecoder::inputFrame(const Frame &frame)
{
    if (!_impl->ctx || !_impl->pkt || !_impl->frame || frame.data.empty()) {
        return false;
    }

    int ret = av_new_packet(_impl->pkt, static_cast<int>(frame.size));
    if (ret < 0) {
        BLOG(error) << "[FFmpegDecoder] av_new_packet failed";
        return false;
    }
    memcpy(_impl->pkt->data, frame.data.data(), frame.size);

    ret = avcodec_send_packet(_impl->ctx, _impl->pkt);
    av_packet_unref(_impl->pkt);
    if (ret < 0) {
        BLOG(error) << "[FFmpegDecoder] avcodec_send_packet failed, ret=" << ret;
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(_impl->ctx, _impl->frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            BLOG(error) << "[FFmpegDecoder] avcodec_receive_frame failed, ret=" << ret;
            return false;
        }

        AVFrame *f = _impl->frame;
        if (f->format != AV_PIX_FMT_YUV420P) {
            BLOG(warning) << "[FFmpegDecoder] unexpected pix_fmt=" << f->format << ", expect YUV420P";
            av_frame_unref(f);
            continue;
        }

        const int w = f->width;
        const int h = f->height;
        const int ySize  = w * h;
        const int uvSize = ySize / 4;

        Frame out;
        out.width     = w;
        out.height    = h;
        out.keyFrame  = (f->key_frame != 0);
        out.timestamp = 0;
        out.data.resize(ySize + 2 * uvSize);

        // 逐行拷贝为紧凑 YUV420P（处理 linesize 对齐）
        uint8_t *dst = out.data.data();
        for (int y = 0; y < h; ++y)
            memcpy(dst + y * w, f->data[0] + y * f->linesize[0], w);
        uint8_t *dstU = dst + ySize;
        for (int y = 0; y < h / 2; ++y)
            memcpy(dstU + y * (w / 2), f->data[1] + y * f->linesize[1], w / 2);
        uint8_t *dstV = dst + ySize + uvSize;
        for (int y = 0; y < h / 2; ++y)
            memcpy(dstV + y * (w / 2), f->data[2] + y * f->linesize[2], w / 2);

        out.size = out.data.size();
        if (_cb) {
            _cb(out);
        }
        av_frame_unref(f);
    }
    return true;
}
