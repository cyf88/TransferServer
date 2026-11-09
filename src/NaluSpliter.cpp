

#include "NaluSpliter.h"
#include "svac.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include "logger.h"

static std::string charArrayToHex(const char *charArray, int length) {
    std::stringstream ss;

    for (int i = 0; i < length; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(charArray[i]);
    }

    return ss.str();
}

static const char *memfind(const char *buf, ssize_t len, const char *subbuf, ssize_t sublen) {
    for (auto i = 0; i < len - sublen; ++i) {
        if (memcmp(buf + i, subbuf, sublen) == 0) {
            return buf + i;
        }
    }
    return NULL;
}
static size_t prefixSize(const char *ptr, size_t len) {
    if (len < 4) {
        return 0;
    }

    if (ptr[0] != 0x00 || ptr[1] != 0x00) {
        // 不是0x00 00开头
        return 0;
    }

    if (ptr[2] == 0x00 && ptr[3] == 0x01) {
        // 是0x00 00 00 01
        return 4;
    }

    if (ptr[2] == 0x01) {
        // 是0x00 00 01
        return 3;
    }
    return 0;
}

NaluSpliter::NaluSpliter() {}

std::string NaluSpliter::getNaluTypeString(svac_nal_unit_type_t type) {
    switch (type) {
        case NAL_NONE_IDR: return "NAL_NONE_IDR";
        case NAL_IDR: return "NAL_IDR";
        case NAL_NONE_IDR_SVC: return "NAL_NONE_IDR_SVC";
        case NAL_IDR_SVC: return "NAL_IDR_SVC";
        case NAL_SURVEILLANCE_EXTENSION_UNIT: return "NAL_SURVEILLANCE_EXTENSION_UNIT";
        case NAL_SEI: return "NAL_SEI";
        case NAL_SPS: return "NAL_SPS";
        case NAL_PPS: return "NAL_PPS";
        case NAL_SES: return "NAL_SES";
        case NAL_AUTH: return "NAL_AUTH";
        case NAL_E0STREAM: return "NAL_E0STREAM";
        case NAL_RESERVED1: return "NAL_RESERVED1";
        case NAL_CHAPTER6: return "NAL_CHAPTER6";
        case NAL_RESERVED2: return "NAL_RESERVED2";
        case NAL_SVC_PPS: return "NAL_SVC_PPS";
        default: return "Unknown NaluType";
    }
}

void NaluSpliter::copyNaluToCache(std::vector<svac_nalu_t> &vNal, size_t splitIndex, const char *naluStart, size_t naluLength, size_t prefix) {
    // 确保 vNal 容量足够
    if (vNal.size() <= splitIndex) {
        vNal.resize(splitIndex + 1);
    }

    svac_nalu_t &cacheNalu = vNal[splitIndex];
    memset(&cacheNalu, 0, sizeof(svac_nalu_t));

    if (!cacheNalu.buf) {
        cacheNalu.buf = (uint8_t*)malloc(naluLength);
        cacheNalu.buf_size = naluLength;
    }

    // 放入vector的是脱壳后的数据,将数据拷贝到 vNal 队列中
    int nal_size  = static_cast<int>(naluLength);
   // BLOG(info) << "copyNaluToCache: nal_size=" << nal_size;
    int rbsp_size = static_cast<int>(cacheNalu.buf_size);

    //拷贝脱壳后的数据
    nal_to_rbsp(prefix, reinterpret_cast<const uint8_t *>(naluStart), &nal_size, reinterpret_cast<uint8_t *>(cacheNalu.buf), &rbsp_size);
    cacheNalu.buf_size = static_cast<size_t>(rbsp_size);
   // BLOG(info) << "copyNaluToCache: splitIndex=" << splitIndex << ", naluLength=" << naluLength << ", rbsp_size=" << rbsp_size;
}



int NaluSpliter::splitNalus(const char *ptr, size_t len, std::vector<svac_nalu_t> &vNal, size_t &vNalSize) {
    // std::cout << "ptr: " << charArrayToHex(ptr, 10) << std::endl;
    size_t prefix = prefixSize(ptr, len);
    auto start    = ptr + prefix;
    auto end      = ptr + len;
    size_t next_prefix;
    size_t splitIndex = 0;
    while (true) {
        
        auto next_start = memfind(start, end - start, "\x00\x00\x01", 3);
        if (next_start) {
            // 找到下一帧
            if (*(next_start - 1) == 0x00) {
                // 这个是00 00 00 01开头
                //BLOG(info) << "找到下一帧,这个是00 00 00 01开头";
                next_start -= 1;
                next_prefix = 4;
            } else {
                // 这个是00 00 01开头
                next_prefix = 3;
            }
            // 记得加上本帧prefix长度
            // 按顺序将解析出来的nalu拷贝到vNal对应位置，作为缓存，避免频繁malloc
            copyNaluToCache(vNal, splitIndex, start - prefix, next_start - start + prefix, prefix);
            ++splitIndex;

            // 搜索下一帧末尾的起始位置
            start = next_start + next_prefix;
            // 记录下一帧的prefix长度
            prefix = next_prefix;
            continue;
        }
        // 未找到下一帧,这是最后一帧

        copyNaluToCache(vNal, splitIndex, start - prefix, end - start + prefix, prefix);
        vNalSize = splitIndex + 1;
        
        break;
    }

    return 0;
}
