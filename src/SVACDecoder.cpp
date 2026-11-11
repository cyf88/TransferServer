#include "SVACDecoder.h"
#include "logger.h"
#include <dlfcn.h>

#include <fstream>
#include <sstream>
#include <iomanip>





#define AUTH_FUN_INIT(fun) PF##fun g_pf##fun = NULL

#define AUTH_FUN_GET(fun)                                                                                                                                      \
    g_pf##fun = (PF##fun)dlsym(_svacInst, #fun);                                                                                                               \
    if (g_pf##fun == NULL) {                                                                                                                                   \
        BLOG(error) << "get function error,name:" << #fun;                                                                                                     \
        return 0;                                                                                                                                              \
    }

typedef int (*PFZXSVACDec_Init)(void);
typedef int (*PFZXSVACDec_Open)(HANDLE &handle, int thread_num, int core_num, int svac_version);
typedef int (*PFZXSVACDec_Close)(HANDLE handle);
typedef int (*PFZXSVACDec_PrefetchParam)(HANDLE handle, unsigned char *pBuf, int uBufLen, SVAC_PREFETCH_PARAM *pPreParam);
typedef int (*PFZXSVACDec_Decode)(HANDLE handle, DEC_INPUT_PARAM *decin, DEC_OUTPUT_PARAM *decout, EXT_INFO_COMMON *pExt_info);
typedef int (*PFZXSVACDec_GetHandle)(HANDLE &handle);
typedef int (*PFZXSVACDec_UnInit)();

AUTH_FUN_INIT(ZXSVACDec_Init);
AUTH_FUN_INIT(ZXSVACDec_Open);
AUTH_FUN_INIT(ZXSVACDec_Close);
AUTH_FUN_INIT(ZXSVACDec_PrefetchParam);
AUTH_FUN_INIT(ZXSVACDec_Decode);
AUTH_FUN_INIT(ZXSVACDec_GetHandle);
AUTH_FUN_INIT(ZXSVACDec_UnInit);


static int GetBasicYSize(int nYSize, int nRadio);
static int ModifyVerValue(int _iVersion, int _iWidth, int _iHeight);
static int ModifyThreadValue(int _iThreadNum, SVAC_PREFETCH_PARAM _stuParam);

SVACDecoder::SVACDecoder(int threadNum) {
	memset(&_vdCtx, 0, sizeof(_vdCtx));
	_vdCtx.video_decode_handle = (HANDLE)-1;
	_vdCtx.thread_num = threadNum;
}

SVACDecoder::~SVACDecoder() {

   //中星微的库不支持初始化，所以注释掉
   // g_pfZXSVACDec_UnInit();
   
	SAFE_DELETE_ARRAY(_vdCtx.m_yuv_buf);
	SAFE_DELETE_ARRAY(_vdCtx.m_buf_org);
	SAFE_DELETE_ARRAY(_vdCtx.m_svc_buf_org);

}

bool SVACDecoder::inputFrame(const Frame &frame) {

    if (frame.keyFrame) {
        int ret = openDecodeHandle(reinterpret_cast<const char *>(frame.data.data()), frame.size, &_vdCtx);
        if (ret == false) {
            BLOG(error) << "openDecodeHandle failed";
            _vdCtx.decode_open = false;
            return false;
        }
        _vdCtx.decode_open = true;
    }

    if (_vdCtx.decode_open) {
        if (this->_cb) {
            size_t outSize = 0;
            Frame outFrame;
            decodeFrame(
                &_vdCtx, reinterpret_cast<const char *>(frame.data.data()), frame.size, frame.keyFrame, reinterpret_cast<char *>(_vdCtx.m_yuv_buf), outSize,
                outFrame.width, outFrame.height);
            if (outSize > 0) {
                // 将解码后的数据复制到 outFrame.data
                outFrame.data.resize(outSize);
                memcpy(outFrame.data.data(), _vdCtx.m_yuv_buf, outSize);
                outFrame.size = outSize;
                _cb(outFrame);
            }
        }
    }

    return true;
}

void SVACDecoder::setOnDecode(onDec cb) {
    this->_cb = std::move(cb);
}

bool SVACDecoder::loadLibrary(std::string libPath) {
    if (libPath.empty()) {
        return false;
    }

    // 构建完整库路径
    
    BLOG(info) << "尝试加载SVAC解码库: " << libPath;

    _svacInst = dlopen(libPath.c_str(), RTLD_LAZY);
    if (_svacInst == nullptr) {
        printf("dlopen %s 失败: %s\n", libPath.c_str(), dlerror());
        return false;
    }

    AUTH_FUN_GET(ZXSVACDec_Init);
   // AUTH_FUN_GET(ZXSVACDec_UnInit);
    AUTH_FUN_GET(ZXSVACDec_Open);
    //AUTH_FUN_GET(ZXSVACDec_GetHandle); // Linux SDK 无此接口
    AUTH_FUN_GET(ZXSVACDec_Close);
    AUTH_FUN_GET(ZXSVACDec_PrefetchParam);
    AUTH_FUN_GET(ZXSVACDec_Decode);

	//中星微的初始化返回值不为0，和文档描述不一致
    int ret = g_pfZXSVACDec_Init();
    if (ret != 0) {
        //BLOG(error) << "SVAC解码库初始化失败, error = " << ret;
        //return false;
    }

    return true;
}

void SVACDecoder::unloadLibrary() {
	dlclose(_svacInst);
	_svacInst = nullptr;
}

bool SVACDecoder::openDecodeHandle(const char *input, size_t inputSize, VideoDecodeCtx *vdCtx) {

    SVAC_PREFETCH_PARAM param;
    memset(&param, 0, sizeof(SVAC_PREFETCH_PARAM));
    // 将输入保存成文件，便于调试
	#ifdef DEBUG
    {
        static int dump_index = 0;
        std::ostringstream oss;
        oss << "svac_open_input_" << std::setw(4) << std::setfill('0') << dump_index++ << ".bin";
        std::ofstream ofs(oss.str(), std::ios::binary);
        if (ofs.is_open()) {
            ofs.write(input, inputSize);
            ofs.close();
        }
    }
	#endif

    int ret = g_pfZXSVACDec_PrefetchParam(NULL, (unsigned char *)input, (int)inputSize, &param);
    if (ret != 0) {
        BLOG(error) << "ZXSVACDec_PrefetchParam failed ret = 2";
        return false;
    }
    if (memcmp(&(vdCtx->svac_param), &param, sizeof(SVAC_PREFETCH_PARAM)) != 0) {
        // I帧参数有变化,需要重新创建上下文
        memcpy(&vdCtx->svac_param, &param, sizeof(SVAC_PREFETCH_PARAM));
        vdCtx->byte_per_sample = vdCtx->svac_param.bit_depth_luma == 8 ? 1 : 2;
        SAFE_DELETE_ARRAY(vdCtx->m_buf_org);
        SAFE_DELETE_ARRAY(vdCtx->m_svc_buf_org);
        SAFE_DELETE_ARRAY(vdCtx->m_yuv_buf);
        int nYSize = vdCtx->svac_param.width * vdCtx->svac_param.height;
        if (vdCtx->svac_param.spatial_svc_flag != 0) {
            int nBasicYSize      = GetBasicYSize(nYSize, vdCtx->svac_param.spatial_svc_ratio);
            vdCtx->m_buf_org     = new unsigned char[vdCtx->byte_per_sample * nBasicYSize * 3 / 2 + 16];
            vdCtx->m_svc_buf_org = new unsigned char[vdCtx->byte_per_sample * nYSize * 3 / 2 + 16];
        } else {
            vdCtx->m_buf_org = new unsigned char[vdCtx->byte_per_sample * nYSize * 3 / 2 + 16];
        }

        if (vdCtx->m_buf_org != NULL && ((size_t)vdCtx->m_buf_org) % 16 != 0) {
            vdCtx->m_buf = vdCtx->m_buf_org + 16 - (((size_t)vdCtx->m_buf_org) % 16);
        } else {
            vdCtx->m_buf = vdCtx->m_buf_org;
        }

        if (vdCtx->m_svc_buf_org != NULL && ((size_t)vdCtx->m_svc_buf_org) % 16 != 0) {
            vdCtx->m_svc_buf = vdCtx->m_svc_buf_org + 16 - (((size_t)vdCtx->m_svc_buf_org) % 16);
        } else {
            vdCtx->m_svc_buf = vdCtx->m_svc_buf_org;
        }
        if (vdCtx->m_buf == NULL) {
            BLOG(error) << "vdCtx->m_buf == NULL";
            return false;
        }
        int ver = vdCtx->svac_param.svac_version;
        if (vdCtx->svac_param.width > 1920 || vdCtx->svac_param.height > 1088) {
            ver = ModifyVerValue(vdCtx->svac_param.svac_version, vdCtx->svac_param.width, vdCtx->svac_param.height);
        }
		
        int iThreadnum = ModifyThreadValue(vdCtx->thread_num, vdCtx->svac_param);

        BLOG(info) << "SVAC_VERSION: " << vdCtx->svac_param.svac_version << " THREAD_NUM: " << vdCtx->thread_num;

        vdCtx->remain_day = g_pfZXSVACDec_Open(vdCtx->video_decode_handle, iThreadnum, -1, ver);
        if (vdCtx->video_decode_handle == ((HANDLE)-1)) {
            BLOG(error) << "ZXSVACDec Open failed! remain_day = " << vdCtx->remain_day;
            return false;
        }
        if (vdCtx->m_yuv_buf == NULL) {
            vdCtx->m_yuv_buf = new unsigned char[vdCtx->svac_param.width * vdCtx->svac_param.height * 5];
        }

        BLOG(info) << "ZXSVACDec_Open handle: " << vdCtx->video_decode_handle << " remain days: " << vdCtx->remain_day
                   << " frame rate :" << vdCtx->svac_param.frame_rate << " width :" << vdCtx->svac_param.width << " height : " << vdCtx->svac_param.height;
    }

    return true;
}

void SVACDecoder::decodeFrame(
    VideoDecodeCtx *vdCtx,
    const char *input,
    size_t inputSize,
    int keyFrame,
    char *output,
    size_t &outputSize,
    int &width,
    int &height) {

    outputSize = 0;

    if (keyFrame) {
        if (!openDecodeHandle(input, inputSize, vdCtx)) {
            BLOG(error) << "openDecodeHandle failed";
            return;
        }
        vdCtx->decode_open = true;
    } else {
        if (vdCtx->decode_open == false) {
            // 解码器还没打开，丢弃输入的帧
            BLOG(warning) << "decode_open == false, drop frame";
            return;
        }
    }
    SVAC_PREFETCH_PARAM m_param = vdCtx->svac_param;
    DEC_INPUT_PARAM dec_in;
    DEC_OUTPUT_PARAM dec_out;
    memset(&dec_in, 0, sizeof(DEC_INPUT_PARAM));
    memset(&dec_out, 0, sizeof(DEC_OUTPUT_PARAM));
    int nYSize  = m_param.width * m_param.height;
    int nUVSize = nYSize / 4;
    if (m_param.spatial_svc_flag) {

        BLOG(info) << "spatial_svc_flag true";
        int nBasicYSize  = GetBasicYSize(nYSize, m_param.spatial_svc_ratio);
        int nBasicUVSize = GetBasicYSize(nUVSize, m_param.spatial_svc_ratio);

        unsigned char *dest_buf = vdCtx->m_buf;
        dec_out.pY              = dest_buf;
        dec_out.pU              = dest_buf + vdCtx->byte_per_sample * nBasicYSize;
        dec_out.pV              = dest_buf + vdCtx->byte_per_sample * (nBasicYSize + nBasicUVSize);

        unsigned char *m_pSvcBuf = vdCtx->m_svc_buf;
        dec_out.pY_SVC           = m_pSvcBuf;
        dec_out.pU_SVC           = m_pSvcBuf + vdCtx->byte_per_sample * nYSize;
        dec_out.pV_SVC           = m_pSvcBuf + vdCtx->byte_per_sample * (nYSize + nUVSize);
    } else {
        unsigned char *dest_buf = vdCtx->m_buf;
        dec_out.pY              = dest_buf;
        dec_out.pU              = dest_buf + vdCtx->byte_per_sample * nYSize;
        dec_out.pV              = dest_buf + vdCtx->byte_per_sample * (nYSize + nUVSize);
    }

    int ret    = 0;
    int tryCnt = 0;

    while (tryCnt < 2) {

        dec_in.pBitstream                = (unsigned char *)input;
        dec_in.nLen                      = inputSize;
        dec_in.chroma_format_idc         = 1;
        dec_in.bSvcdec                   = 1;
        dec_in.bExtdecOnly               = 0;
        dec_in.check_authentication_flag = 0;

        dec_out.nWidth  = m_param.width;
        dec_out.nHeight = m_param.height;

        ret = g_pfZXSVACDec_Decode(vdCtx->video_decode_handle, &dec_in, &dec_out, NULL);
        if (ret > 0) {
            break;
        }
        tryCnt++;
    }

    if (ret >= 0) {
        if ((dec_out.nIsEffect & 0x1) == 0) {
            BLOG(error) << "dec_out.nIsEffect & 0x1 = 0";
            return;
        }

        width          = dec_out.nWidth;
        height         = dec_out.nHeight;
        int pixelCount = dec_out.nWidth * dec_out.nHeight;
        memcpy(output, dec_out.pY, pixelCount);
        memcpy(output + pixelCount, dec_out.pU, pixelCount / 4);
        memcpy(output + pixelCount + pixelCount / 4, dec_out.pV, pixelCount / 4);
        outputSize = pixelCount * 3 / 2;
        return;

    } else {
        // ret < 0（如 -65532 POC 不连续 / -65504 图像解码错）：
        // 对齐 triplayer——只要解码器给出了有效宽高，仍输出该帧，避免丢帧导致卡顿。
        if (dec_out.nWidth > 0 && dec_out.nHeight > 0) {
            BLOG(info) << "ZXSVACDec_Decode failed Error code: " << ret << " width: " << dec_out.nWidth << " height: " << dec_out.nHeight;
            width  = dec_out.nWidth;
            height = dec_out.nHeight;
            int pixelCount = dec_out.nWidth * dec_out.nHeight;
            memcpy(output, dec_out.pY, pixelCount);
            memcpy(output + pixelCount, dec_out.pU, pixelCount / 4);
            memcpy(output + pixelCount + pixelCount / 4, dec_out.pV, pixelCount / 4);
            outputSize = pixelCount * 3 / 2;
        } else {
            outputSize = 0;
            BLOG(error) << "ZXSVACDec_Decode failed Error code: " << ret;
        }
    }
    return;
}

static int GetBasicYSize(int nYSize, int nRadio) {
    int nBasicYSize = nYSize;
    switch (nRadio) {
        case 0: {
            nBasicYSize = nYSize * 9 / 16;
            break;
        }
        case 1: {
            nBasicYSize = nYSize * 4 / 16;
            break;
        }
        case 2: {
            nBasicYSize = nYSize * 1 / 16;
            break;
        }
        case 3: {
            nBasicYSize = nYSize * 1 / 36;
            break;
        }
        case 4: {
            nBasicYSize = nYSize * 1 / 64;
        }
        default: break;
    }

    return nBasicYSize;
}

static int ModifyVerValue(int _iVersion, int _iWidth, int _iHeight) {
    int iVersion = _iVersion;
    iVersion |= (_iWidth << 2);
    iVersion |= (_iHeight << 16);
    return iVersion;
}

static int ModifyThreadValue(int _iThreadNum, SVAC_PREFETCH_PARAM _stuParam) {
    int iThreadNum = _iThreadNum;
    int iSSVC      = _stuParam.spatial_svc_flag == 0 ? 1 : 0;
    iThreadNum |= (iSSVC << 4);
    int iSsvcRatio = 0;
    if (_stuParam.spatial_svc_ratio > 0) {
        iSsvcRatio = _stuParam.spatial_svc_ratio - 1;
    }
    if (_stuParam.spatial_svc_ratio == 0) {
        iSsvcRatio = 4;
    }
    iThreadNum |= (iSsvcRatio << 5);
    int iBit = _stuParam.bit_depth_luma == 10 ? 0 : 1;
    iThreadNum |= (iBit << 8);
    int iConsult = _stuParam.reserved[0] == 0 ? 5 : _stuParam.reserved[0];
    iThreadNum |= (iConsult << 10);
    int iExtended_sb_size = _stuParam.reserved[1] == 0 ? 1 : 0;
    iThreadNum |= (iExtended_sb_size << 13);
    int iSAO = _stuParam.reserved[2] == 0 ? 1 : 0;
    iThreadNum |= (iSAO << 14);
    return iThreadNum;
}
