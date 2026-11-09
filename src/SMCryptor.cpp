//
// SMCryptor.cpp - 国密加解密封装类实现
//

#include "SMCryptor.h"
#include "openssl/modes.h"
#include "openssl/sms4.h"
#include <algorithm>
#include <cstring>
#include <openssl/asn1.h>
#include <openssl/gmskf.h>
#include <openssl/sm2.h>

unsigned int SMCryptor::sm4Ecb(
    unsigned char *secretkey,
    unsigned char *in,
    unsigned char *out,
    bool isEncrypt) {

    sms4_key_t key;
    if (isEncrypt) {
        sms4_set_encrypt_key(&key, secretkey);
    } else {
        sms4_set_decrypt_key(&key, secretkey);
    }

    sms4_ecb_encrypt(in, out, &key, isEncrypt);
    return 0;
}

unsigned int SMCryptor::sm4Ofb(
    unsigned char *key,
    int keyLen,
    unsigned char *iv,
    int ivLen,
    unsigned char *in,
    int inSize,
    unsigned char *out,
    int &outSize,
    bool isEncrypt) {
    int tmpLen;
    // int inlen, outlen;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    /* Now we can set key and IV */
    EVP_CipherInit_ex(ctx, EVP_sm4_ofb(), NULL, key, iv, isEncrypt);

    if (!EVP_CipherUpdate(ctx, out, &outSize, in, inSize)) {
        /* Error */
        EVP_CIPHER_CTX_free(ctx);
        return 1;
    }

    if (!EVP_CipherFinal_ex(ctx, out + outSize, &tmpLen)) {

        /* Error */
        EVP_CIPHER_CTX_free(ctx);
        return 1;
    }

    outSize += tmpLen;
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

unsigned int SMCryptor::sm3(unsigned char *plainText, unsigned int plain_len, unsigned char *hash_out) {
    sm3(plainText, plain_len, hash_out);
    return 0;
}
