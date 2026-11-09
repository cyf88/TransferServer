#ifndef _NALU_SPLITER_H
#define _NALU_SPLITER_H

#include "svac.h"
#include <string>
#include <vector>

class NaluSpliter {

public:
    NaluSpliter();
    ~NaluSpliter();

    static std::string getNaluTypeString(svac_nal_unit_type_t type);

    static int splitNalus(const char *ptr, size_t len, std::vector<svac_nalu_t> &vNal, size_t &vNalSize);

private:
    
    /**
     * 将 NALU 数据拷贝到缓存中
     * @param vNal NALU 向量
     * @param splitIndex 索引位置   
     * @param naluStart 数据起始位置
     * @param naluLength 数据长度
     * @param prefix prefix 长度
     */
    static void copyNaluToCache(std::vector<svac_nalu_t> &vNal, size_t splitIndex, const char *naluStart, size_t naluLength, size_t prefix);
};

#endif // _NALU_SPLITER_H