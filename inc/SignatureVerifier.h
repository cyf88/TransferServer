#ifndef SIGNATURE_VERIFIER_H
#define SIGNATURE_VERIFIER_H

#include "svac.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// EC_KEY 前向声明，避免在头文件中引入完整 OpenSSL 类型
typedef struct ec_key_st EC_KEY;

/**
 * SVAC 码流软验签器（GB/T 35114）
 *
 * 用 GMSSL 的 SM3 + SM2 对码流中的 AUTH 验证单元做签名校验，不依赖加密硬件（软验签）。
 * 算法照搬 SDK2 35114_sdk 的 SSL_VERIFY_SIGNATURE / decryptNalUnitsStream：
 *   1. 切分原始 Annex-B 码流，对 authentication_idc == 1 的 NAL 累积哈希；
 *   2. SES NAL 解析 successive_hash_pictures_minus1；
 *   3. AUTH NAL 解析 base64 编码的签名；
 *   4. digest = SM3(累积数据)，必要时链式哈希，再用证书公钥做 SM2 验签。
 */
class SignatureVerifier {
public:
    using ResultCallback = std::function<void(bool ok, const std::string &detail)>;

    SignatureVerifier();
    ~SignatureVerifier();

    SignatureVerifier(const SignatureVerifier &) = delete;
    SignatureVerifier &operator=(const SignatureVerifier &) = delete;

    /** 从 .cer 证书加载 SM2 公钥（支持 PEM / DER 编码） */
    bool loadCertificate(const std::string &certPath);

    /** 输入原始 SVAC Annex-B 码流，累积哈希并在出现 AUTH 时验签 */
    void input(const uint8_t *data, size_t len);

    void setResultCallback(ResultCallback cb) { _cb = std::move(cb); }

    /** 新流开始时重置哈希链状态 */
    void reset();

private:
    /** 单次 SM2 软验签：msg 为 SM3 摘要（32 字节），sig 为签名（DER/ASN.1） */
    bool verifySm2(const unsigned char *msg, size_t msgLen,
                   const unsigned char *sig, size_t sigLen);

    /** 处理单个 NAL（含头部、不含起始码） */
    void processNal(const uint8_t *nal, size_t len, bool &hasAuth);

    void report(bool ok, const std::string &detail);

private:
    ResultCallback _cb;
    EC_KEY *_ecKey = nullptr;          // 证书提取的 SM2 公钥
    svac_stream_t *_hSvac = nullptr;   // 复用 svac.cpp 的 SES/AUTH 解析

    // 本批次累积的哈希数据（authentication_idc == 1 的原始 NAL 字节）
    std::vector<uint8_t> _needHashData;

    // 跨帧链式哈希状态
    unsigned char _hash[32] = {0};
    bool _hashEmpty = true;

    // SES 参数（来自码流）
    int _successiveHashPicturesMinus1 = 0;
};

#endif // SIGNATURE_VERIFIER_H
