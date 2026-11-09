//
// SMCryptor.h - 国密加解密封装类
// 封装 SM2、SM3、SM4 等国密算法的静态函数
//

#ifndef SM_CRYPTOR_H
#define SM_CRYPTOR_H

/**
 * 国密加解密工具类
 * 提供 SM2、SM3、SM4 等算法的静态方法封装
 */
class SMCryptor {
public:
    static unsigned int sm4Ecb(unsigned char *secretkey, unsigned char *in, unsigned char *out, bool isEncrypt);

    static unsigned int sm4Ofb(
        unsigned char *key,
        int keyLen,
        unsigned char *iv,
        int ivLen,
        unsigned char *in,
        int inSize,
        unsigned char *out,
        int &outSize,
        bool isEncrypt);

    static unsigned int sm3(unsigned char *plainText, unsigned int plain_len, unsigned char *hash_out);

private:
    SMCryptor()                             = delete;
    ~SMCryptor()                            = delete;
    SMCryptor(const SMCryptor &)            = default;
    SMCryptor &operator=(const SMCryptor &) = default;
};

#endif // SM_CRYPTOR_H
