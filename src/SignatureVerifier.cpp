#include "SignatureVerifier.h"
#include "logger.h"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sm2.h>
#include <openssl/sm3.h>
#include <openssl/x509.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>

#include <cstdio>
#include <cstring>

namespace {

// 起始码长度：00 00 00 01 -> 4；00 00 01 -> 3；否则 0
size_t startCodeLen(const uint8_t *p, size_t remaining)
{
    if (remaining >= 4 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01)
        return 4;
    if (remaining >= 3 && p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01)
        return 3;
    return 0;
}

// base64 解码到字节缓冲，含非法字符时返回 false（仿照 server.cpp 的实现）
bool base64Decode(const unsigned char *in, size_t inLen, std::vector<uint8_t> &out)
{
    auto val = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    out.clear();
    uint32_t buf = 0;
    int bits = -8;
    for (size_t i = 0; i < inLen; ++i) {
        if (in[i] == '=') break;
        int v = val(in[i]);
        if (v < 0) return false;
        buf = (buf << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return true;
}

// 软验签使用的用户 ID（与 SDK2 保持一致）
const char kSm2UserId[] = "1234567812345678";

} // namespace

SignatureVerifier::SignatureVerifier()
{
    _hSvac = svac_new();
    if (!_hSvac) {
        BLOG(error) << "[SignatureVerifier] failed to create svac stream";
    }
}

SignatureVerifier::~SignatureVerifier()
{
    if (_ecKey) {
        EC_KEY_free(_ecKey);
        _ecKey = nullptr;
    }
    if (_hSvac) {
        svac_free(_hSvac);
        _hSvac = nullptr;
    }
}

bool SignatureVerifier::loadCertificate(const std::string &certPath)
{
    FILE *fp = fopen(certPath.c_str(), "rb");
    if (!fp) {
        BLOG(error) << "[SignatureVerifier] open cert failed: " << certPath;
        return false;
    }

    X509 *cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    if (!cert) {
        rewind(fp);
        cert = d2i_X509_fp(fp, nullptr);
    }
    fclose(fp);

    if (!cert) {
        BLOG(error) << "[SignatureVerifier] parse cert failed: " << certPath;
        return false;
    }

    EVP_PKEY *pkey = X509_get_pubkey(cert);
    if (!pkey) {
        X509_free(cert);
        BLOG(error) << "[SignatureVerifier] get pubkey failed: " << certPath;
        return false;
    }

    if (_ecKey) {
        EC_KEY_free(_ecKey);
        _ecKey = nullptr;
    }
    _ecKey = EVP_PKEY_get1_EC_KEY(pkey);

    EVP_PKEY_free(pkey);
    X509_free(cert);

    if (!_ecKey) {
        BLOG(error) << "[SignatureVerifier] extract EC/SM2 key failed: " << certPath;
        return false;
    }

    BLOG(info) << "[SignatureVerifier] cert loaded: " << certPath;
    return true;
}

void SignatureVerifier::reset()
{
    _needHashData.clear();
    memset(_hash, 0, sizeof(_hash));
    _hashEmpty = true;
    _successiveHashPicturesMinus1 = 0;
}

void SignatureVerifier::input(const uint8_t *data, size_t len)
{
    if (!data || len == 0 || !_ecKey || !_hSvac)
        return;

    _needHashData.clear();
    bool hasAuth = false;

    // 切分原始 Annex-B 码流为 NAL（保留原始字节，含头部与防竞争字节、不含起始码）
    size_t i = 0;
    while (i < len && startCodeLen(data + i, len - i) == 0)
        ++i;
    while (i < len) {
        size_t sc = startCodeLen(data + i, len - i);
        if (sc == 0)
            break;
        size_t nalStart = i + sc;
        size_t j = nalStart;
        while (j < len && startCodeLen(data + j, len - j) == 0)
            ++j;
        size_t nalLen = j - nalStart;
        if (nalLen > 0)
            processNal(data + nalStart, nalLen, hasAuth);
        i = j;
    }

    if (_needHashData.empty())
        return;

    unsigned char digest[32] = {0};
    sm3(_needHashData.data(), _needHashData.size(), digest);

    // 跨帧链式哈希（successive_hash_pictures_minus1 > 0 时）
    if (_successiveHashPicturesMinus1 > 0) {
        if (_hashEmpty) {
            _hashEmpty = false;
            memcpy(_hash, digest, 32);
        } else {
            memcpy(_hash + 32, digest, 32);
            sm3(_hash, 64, digest);
            memcpy(_hash, digest, 32);
        }
    } else {
        _hashEmpty = false;
        memcpy(_hash, digest, 32);
    }

    if (!hasAuth)
        return;

    int authLen = _hSvac->auth->auth_data_lenth_minus_1 + 1;
    if (authLen <= 0 || authLen > (int)sizeof(_hSvac->auth->authentication_data)) {
        report(false, "invalid auth data length");
        return;
    }

    std::vector<uint8_t> sig;
    if (!base64Decode(_hSvac->auth->authentication_data, static_cast<size_t>(authLen), sig) || sig.empty()) {
        report(false, "base64 decode auth data failed");
        return;
    }

    BLOG(info) << "[SignatureVerifier] sigLen=" << sig.size()
               << " sig0=" << (int)sig[0];

    bool ok = verifySm2(digest, 32, sig.data(), sig.size());
    report(ok, ok ? "verify success" : "verify fail");
}

void SignatureVerifier::processNal(const uint8_t *nal, size_t len, bool &hasAuth)
{
    if (len == 0)
        return;

    uint8_t b0 = nal[0];
    int nalType = (b0 >> 2) & 0x0F;
    int authIdc = b0 & 0x01;

    if (authIdc == 1) {
        _needHashData.insert(_needHashData.end(), nal, nal + len);
    }

    switch (nalType) {
        case NAL_SPS:
        case NAL_SES:
        case NAL_AUTH:
            read_nal_unit(_hSvac, const_cast<uint8_t *>(nal), static_cast<int>(len));
            break;
        default:
            break;
    }

    if (nalType == NAL_SES) {
        _successiveHashPicturesMinus1 = _hSvac->ses->successive_hash_pictures_minus1;
    } else if (nalType == NAL_AUTH) {
        hasAuth = true;
    }
}

bool SignatureVerifier::verifySm2(const unsigned char *msg, size_t msgLen,
                                  const unsigned char *sig, size_t sigLen)
{
    if (!_ecKey)
        return false;

    // 裸 64 字节 (r||s) → ASN.1 DER（GMSSL 的 SM2_verify 只认 DER）
    const unsigned char *derSig = sig;
    int derLen = static_cast<int>(sigLen);
    unsigned char *derBuf = nullptr;
    if (sigLen == 64) {
        BIGNUM *r = BN_bin2bn(sig, 32, nullptr);
        BIGNUM *s = BN_bin2bn(sig + 32, 32, nullptr);
        if (r && s) {
            ECDSA_SIG *tmp = ECDSA_SIG_new();
            if (tmp) {
                if (ECDSA_SIG_set0(tmp, r, s) == 1) {
                    r = nullptr; // 所有权已转移给 tmp
                    s = nullptr;
                    int n = i2d_ECDSA_SIG(tmp, &derBuf);
                    if (n > 0) {
                        derSig = derBuf;
                        derLen = n;
                    }
                }
                ECDSA_SIG_free(tmp);
            }
        }
        BN_free(r);
        BN_free(s);
    }

    const EVP_MD *md = EVP_sm3();
    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        OPENSSL_free(derBuf);
        return false;
    }

    unsigned char buf[EVP_MAX_MD_SIZE];
    size_t siz = sizeof(buf);
    unsigned int ulen = 0;

    // e = SM3(ZA || msg)，ZA = SM2_compute_id_digest(SM3, id, pubkey)
    bool ok = false;
    if (EVP_DigestInit_ex(md_ctx, md, nullptr) &&
        SM2_compute_id_digest(md, kSm2UserId, strlen(kSm2UserId), buf, &siz, _ecKey) &&
        EVP_DigestUpdate(md_ctx, buf, siz) &&
        EVP_DigestUpdate(md_ctx, msg, msgLen) &&
        EVP_DigestFinal_ex(md_ctx, buf, &ulen)) {
        ok = (SM2_verify(NID_undef, buf, static_cast<int>(ulen), derSig, derLen, _ecKey) == 1);
    }

    EVP_MD_CTX_free(md_ctx);
    OPENSSL_free(derBuf);
    return ok;
}

void SignatureVerifier::report(bool ok, const std::string &detail)
{
    if (_cb) {
        _cb(ok, detail);
    }
}
