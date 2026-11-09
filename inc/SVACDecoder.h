#ifndef SVACDECODER_H
#define SVACDECODER_H

#include "ZxSvacDecLib.h"
#include <functional>
#include <memory>
#include <vector>
#include "mpeg-proto.h"


typedef struct VideoDecodeCtx {
    HANDLE video_decode_handle;
    SVAC_PREFETCH_PARAM svac_param;
    int byte_per_sample;
    int thread_num;
    unsigned char *m_buf_org = NULL;
    unsigned char *m_buf     = NULL;
    // svc增强数据
    unsigned char *m_svc_buf_org = NULL;
    unsigned char *m_svc_buf     = NULL;

    //输出缓存
    unsigned char *m_yuv_buf     = NULL;
    bool decode_open = false; // 解码句柄是否打开
    int remain_day;

} VideoDecodeCtx;

typedef struct Frame {

    bool keyFrame;
    std::vector<uint8_t> data;
    size_t size;
    uint64_t timestamp;
    int width;
    int height;
    
} Frame;

class SVACDecoder {

public:
    enum class FrameType {
        I_frame     = 1,
        B_frame     = 2,
        P_frame     = 3,
        Audio_frame = 4,
    };
    using Ptr = std::shared_ptr<SVACDecoder>;

    SVACDecoder(int threadNum);
    ~SVACDecoder();

    using onDec = std::function<void(const Frame &frame)>;

    bool loadLibrary(std::string libPath);

    bool inputFrame(const Frame &frame);

    void setOnDecode(onDec cb);

private:

    onDec _cb;

    VideoDecodeCtx _vdCtx;


    void *_svacInst;

    
    void unloadLibrary();

    bool openDecodeHandle(const char *input, size_t inputSize, VideoDecodeCtx *vdCtx);

    void decodeFrame(
        VideoDecodeCtx *vdCtx,
        const char *input,
        size_t inputSize,
        int keyFrame,
        char *output,
        size_t &outputSize,
        int &width,
        int &height);
};

#endif