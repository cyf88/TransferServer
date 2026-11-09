#pragma once

#include "svac.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define MAX_NALU_COUNT 16 // 最大Nalu数量
#define DEFAULT_NALU_BUF_SIZE (3 * 1024 * 1024) // 默认Nalu缓冲区大小，3M

// Annex-B 起始码：00 00 00 01
inline constexpr uint8_t ANNEXB_START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

typedef enum {
    DECRYPT_STATUS_OK    = 0,
    DECRYPT_STATUS_ERROR = 1,
} DecryptStatus;

typedef struct {
    std::string vkek_version;
    unsigned char vkek[16];
} VKEKInfo;

/**
 * 流信息结构体
 * 包含流ID、流VKEK版本、输入VKEK映射表
 *
 */

typedef struct {
    std::string stream_id;
    std::string stream_vkek_version;
    VKEKInfo vkek_info;
    bool encrypt_flag;
    bool authentication_flag;
    bool is_key_frame = false;

    unsigned char iv[16];
    size_t iv_len;

    unsigned char evek[16];
    size_t evek_len;

    unsigned char vek[16];
    size_t vek_len;

    bool authentication_result;
    DecryptStatus decrypt_status;

    svac_stream_t *svac_stream;

} DecryptContext;
/**
 * SVAC 帧解密器类
 * 封装 Decryptor，提供简单的帧数据输入和回调接口
 */
class Decrypter {
public:
    /**
     * 解密后的帧数据回调函数类型
     * @param frameData 解密后的帧数据指针
     * @param frameLength 帧数据长度
     * @param isKeyFrame 是否为关键帧（I帧）
     */
    using FrameCallback = std::function<void(const uint8_t *frameData, size_t frameLength, bool isKeyFrame)>;

    /**
     * 构造函数
     */
    Decrypter();

    /**
     * 析构函数
     */
    ~Decrypter();

    // 禁止拷贝和赋值
    Decrypter(const Decrypter &)            = delete;
    Decrypter &operator=(const Decrypter &) = delete;

    /**
     * 设置解密后的帧数据回调函数
     * @param callback 当解密出完整帧时调用的回调函数
     */
    void setFrameCallback(FrameCallback callback);

    /**
     * 设置密钥信息
     * @param vkekInfo VKEK信息
     */
    void setVkekInfo(const VKEKInfo &vkekInfo);


    /**
     * 输入加密前的帧数据
     * @param encryptFrameData 加密的帧数据指针
     * @param frameLength 帧数据长度
     * @return 成功返回true，失败返回false
     */
    bool input(const uint8_t *encryptFrameData, size_t frameLength);

private:
    /**
     * 处理单个 NALU 单元
     * @param nalu NALU 结构体
     */
    void processNalu(const svac_nalu_t &svacNalu);

    void processSes(svac_stream_t* hSvac, DecryptContext& context);

    void decryptNalu(DecryptContext& context, const svac_nalu_t& nalu);


   

    /**
     * 解密RBSP数据
     */
    void decryptRbsp(const DecryptContext& context, const uint8_t *encryptRbspData, size_t rbspLength, uint8_t *decryptRbspData);

    

    FrameCallback _frameCallback; // 帧数据回调函数

    std::vector<uint8_t> _decryptBuffer; // 解密缓冲区
    size_t _decryptBufferLength;
    std::vector<svac_nalu_t> _vNal; // Nalu缓冲区

    svac_stream_t *_hSvac;
    DecryptContext _decryptContext;


};
