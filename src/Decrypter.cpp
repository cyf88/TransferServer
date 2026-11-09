#include "Decrypter.h"
#include "NaluSpliter.h"
#include "SMCryptor.h"
#include "logger.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

Decrypter::Decrypter()
{
    _decryptBuffer.reserve(10 * 1024 * 1024); // 预留 10MB 缓冲区
    // 此处初始化 vNal，并分配 svac_nalu_t 的内存，初始化其变量，包括为 buf 分配内存
    _vNal.clear();
    _vNal.reserve(MAX_NALU_COUNT); // 预留容量

    for (int i = 0; i < MAX_NALU_COUNT; ++i) {
        svac_nalu_t nalu;
        memset(&nalu, 0, sizeof(svac_nalu_t));

        nalu.buf_size = 0;
        nalu.buf      = (uint8_t *)malloc(DEFAULT_NALU_BUF_SIZE); // 分配内存
        if (nalu.buf) {
            memset(nalu.buf, 0, DEFAULT_NALU_BUF_SIZE); // 内存清零，防止脏数据
        }

        // 其他成员已被memset归零，视具体需要进一步赋初值
        _vNal.push_back(nalu);
    }
    _hSvac = svac_new();
    if (_hSvac == nullptr) {
        BLOG(error) << "Decrypter::Decrypter: failed to create svac stream";
        return;
    }
    _decryptContext.svac_stream = _hSvac;
}
Decrypter::~Decrypter()
{
    if (_hSvac) {
        svac_free(_hSvac);
        _hSvac = nullptr;
    }
    for (auto &nalu : _vNal) {
        if (nalu.buf) {
            free(nalu.buf);
            nalu.buf = nullptr;
        }
    }
    _vNal.clear();
    _vNal.shrink_to_fit();
    _decryptBuffer.clear();
    _decryptBuffer.shrink_to_fit();
}

void Decrypter::setFrameCallback(FrameCallback callback)
{
    _frameCallback = callback;
}

void Decrypter::setVkekInfo(const VKEKInfo &vkekInfo)
{

    _decryptContext.vkek_info = vkekInfo;
}

void Decrypter::processSes(svac_stream_t *hSvac, DecryptContext &context)
{
    memcpy(context.iv, hSvac->ses->iv, hSvac->ses->iv_length_minus1 + 1);
    context.iv_len = hSvac->ses->iv_length_minus1 + 1;
    memcpy(context.evek, hSvac->ses->evek, hSvac->ses->evek_length_minus1 + 1);
    context.evek_len            = hSvac->ses->evek_length_minus1 + 1;
    context.stream_vkek_version = std::string(reinterpret_cast<const char *>(hSvac->ses->vkek_version), hSvac->ses->vkek_version_length_minus1 + 1);

    if (context.stream_vkek_version != context.vkek_info.vkek_version) {
        //TODO 码流vkek version 和设置的vkek 不一致
        // BLOG(error) << "Decrypter::processSes: stream_vkek_version != vkek_version, stream_vkek_version=" << context.stream_vkek_version
        //             << ", vkek_version=" << context.vkek_info.vkek_version;
    }

    // evek 解密[vkek]-> vek
    //BLOG(debug) << "Decrypter::processSes: vkek_info.vkek_version=" << context.vkek_info.vkek_version;
    SMCryptor::sm4Ecb((unsigned char *)context.vkek_info.vkek, context.evek, context.vek, false);

    // 获取码流中的camera_id 即 stream_id
    _decryptContext.stream_id = std::string(reinterpret_cast<const char *>(_hSvac->ses->camera_id), sizeof(_hSvac->ses->camera_id));
}

void Decrypter::decryptNalu(DecryptContext &context, const svac_nalu_t &nalu)
{

    if (nalu.buf == nullptr || nalu.buf_size == 0) {
        BLOG(error) << "Decrypter::processIdrNalu: invalid nalu";
        return;
    }

    if (context.svac_stream->nal->encryption_idc == 1) {
        //BLOG(debug) << "Decrypter::decryptNalu: encryption_idc == 1";
        // 解密当前NALU数据
        // 获取nal buf的第一字节，并将最后两位设置成0，然后拷贝到 _decryptBuffer
        _decryptBuffer.insert(_decryptBuffer.end(), ANNEXB_START_CODE, ANNEXB_START_CODE + 4);
        uint8_t first_byte = nalu.buf[0] & 0xFC;
        _decryptBuffer.push_back(first_byte);
        std::vector<uint8_t> tmpDecrypted(nalu.buf_size - 1);

        decryptRbsp(context, nalu.buf + 1, nalu.buf_size - 1, tmpDecrypted.data());

        size_t rbsp_size = nalu.buf_size - 1;
        size_t nal_buf_size = rbsp_size + 1000; // 预留足够空间
        std::vector<uint8_t> nal_buf(nal_buf_size);

        size_t nal_out_size = 0;
        int rc = prevention(tmpDecrypted.data(), rbsp_size, nal_buf.data(), &nal_out_size);
        if (rc < 0) {
            BLOG(error) << "Decrypter::decryptNalu: rbsp_to_nal failed!";
            return;
        }

        _decryptBuffer.insert(_decryptBuffer.end(), nal_buf.begin(), nal_buf.begin() + nal_out_size);
      

    } else {
        // 直接将数据输出
        _decryptBuffer.insert(_decryptBuffer.end(), ANNEXB_START_CODE, ANNEXB_START_CODE + 4);
        _decryptBuffer.insert(_decryptBuffer.end(), nalu.buf, nalu.buf + nalu.buf_size);
    }
}

void Decrypter::processNalu(const svac_nalu_t &svacNalu)
{
    if (svacNalu.buf == nullptr || svacNalu.buf_size == 0) {
        BLOG(warning) << "Decrypter::processNalu: invalid nalu";
        return;
    }
    // 把当前的nal数据解析到全局的_hSvac里
    svac_nalu_t *nalu = _hSvac->nal;

    bs_t *b                  = bs_new((uint8_t *)svacNalu.buf, svacNalu.buf_size);
    nalu->forbidden_zero_bit = bs_read_u(b, 1);
    nalu->nal_ref_idc        = bs_read_u(b, 1);
    nalu->nal_unit_type      = bs_read_u(b, 4);
    nalu->encryption_idc     = bs_read_f(b, 1);
    nalu->authentication_idc = bs_read_f(b, 1);
    // bs_free(b)

    // 根据 NALU 类型进行处理
    switch (nalu->nal_unit_type) {
        case NAL_SPS:
            // 如若是sps 直接将数据输出
            {
                const uint8_t *buf_start = reinterpret_cast<const uint8_t *>(svacNalu.buf);
                // 用#define定义全局常量并插入起始码
                _decryptBuffer.insert(_decryptBuffer.end(), ANNEXB_START_CODE, ANNEXB_START_CODE + 4);
                _decryptBuffer.insert(_decryptBuffer.end(), buf_start, buf_start + svacNalu.buf_size);
            }
            break;
        case NAL_PPS:
            // 如若是pps 直接将数据输出
            {
                const uint8_t *buf_start = reinterpret_cast<const uint8_t *>(svacNalu.buf);
                _decryptBuffer.insert(_decryptBuffer.end(), ANNEXB_START_CODE, ANNEXB_START_CODE + 4);
                _decryptBuffer.insert(_decryptBuffer.end(), buf_start, buf_start + svacNalu.buf_size);
            }
            break;
        case NAL_SES:
            // 读取 SES 数据
            read_sec_parameter_set_rbsp(_hSvac, b);
            processSes(_hSvac, _decryptContext);
            break;
        case NAL_AUTH:
            // 读取 AUTH 数据
            // read_authentication_rbsp(_hSvac, b);
            break;
        case NAL_IDR:
            _decryptContext.is_key_frame = true;
            // TODO: 其实svacNalu还时原始数，并没有flag标志
            decryptNalu(_decryptContext, svacNalu);
            break;
        case NAL_NONE_IDR:
            _decryptContext.is_key_frame = false;
            decryptNalu(_decryptContext, svacNalu);
            break;
        default:
            // 其他类型的 NALU
            BLOG(debug) << "Decrypter::processNalu: other NALU type=" << nalu->nal_unit_type << ", size=" << nalu->buf_size;
            break;
    }
    bs_free(b);
}

static int INDEX = 0;
bool Decrypter::input(const uint8_t *encryptFrameData, size_t frameLength)
{
    if (encryptFrameData == nullptr || frameLength == 0) {
        BLOG(error) << "Decrypter::input: invalid parameters";
        return false;
    }

    // 确保缓冲区足够大
    // 先清空_decryptBuffer原有数据
    _decryptBuffer.clear();
    if (_decryptBuffer.capacity() < frameLength * 2) {
        _decryptBuffer.reserve(frameLength * 2);
    }

    size_t vNalSize = 0;
    NaluSpliter::splitNalus((const char *)encryptFrameData, frameLength, _vNal, vNalSize);

    INDEX++;
    for (size_t i = 0; i < vNalSize; i++) {
#ifdef Debug
        // 把每一个nal保存成文件
        {
            // 设置文件名格式为 "nal_{i}.bin"
            std::ostringstream filename;
            filename << "nal_" << INDEX << "_" << std::setw(4) << std::setfill('0') << i << ".bin";
            FILE *fp = fopen(filename.str().c_str(), "wb");
            if (fp) {
                fwrite(_vNal[i].buf, 1, _vNal[i].buf_size, fp);
                fclose(fp);
            } else {
                BLOG(error) << "Failed to save NALU to file: " << filename.str();
            }
        }
#endif
        processNalu(_vNal[i]);
    }

    // 调用回调函数，传递解密后的帧数据及关键帧标志
    if (_frameCallback) {
        _frameCallback(_decryptBuffer.data(), _decryptBuffer.size(), _decryptContext.is_key_frame);
    }

    return true;
}

// 这里的传入的encryptRbspData不含首字节
void Decrypter::decryptRbsp(const DecryptContext &context, const uint8_t *encryptRbspData, size_t rbspLength, uint8_t *decryptRbspData)
{

    if (encryptRbspData == nullptr || rbspLength == 0) {
        BLOG(error) << "encryptRbspData is nullptr or rbspLength is 0";
        return;
    }

    if (decryptRbspData == nullptr) {
        decryptRbspData = (uint8_t *)malloc(rbspLength * 2);
        BLOG(error) << "decryptRbspData is nullptr, malloc size=" << rbspLength * 2;
        return;
    }

    // 最后一个字节是不参与加解密的，
    int decryptRbspLength = 0;

    SMCryptor::sm4Ofb(
        const_cast<unsigned char *>(context.vek), (int)context.vek_len, const_cast<unsigned char *>(context.iv), (int)context.iv_len,
        const_cast<unsigned char *>(encryptRbspData), rbspLength - 1, decryptRbspData, decryptRbspLength, true);

    if (decryptRbspLength != rbspLength - 1) {
        BLOG(error) << "decryptRbspLength != rbspLength-1, decryptRbspLength=" << decryptRbspLength << ", rbspLength=" << rbspLength;
    }

    // 将输入的最后一个字节复制给输出
    decryptRbspData[rbspLength - 1] = encryptRbspData[rbspLength - 1];
}