#ifndef COD2_CDKEY_HASH_H
#define COD2_CDKEY_HASH_H

#include <CommonCrypto/CommonDigest.h>

static inline void Com_CDKeyHashBytes(const char *key, unsigned int length, char hash[33])
{
    static const char digits[] = "0123456789abcdef";
    unsigned char digest[CC_MD5_DIGEST_LENGTH];
    CC_MD5_CTX context;
    unsigned int i;
    /* CoD2 1.3 uses seeded MD5 for its PB authorization field. */
    CC_MD5_Init(&context);
    context.A += 11u * 0xb684a3u;
    context.B += 71u * 0xb684a3u;
    context.C += 37u * 0xb684a3u;
    context.D += 97u * 0xb684a3u;
    CC_MD5_Update(&context, key, length);
    CC_MD5_Final(digest, &context);
    for (i = 0; i < 16; ++i) {
        hash[2 * i] = digits[digest[i] >> 4];
        hash[2 * i + 1] = digits[digest[i] & 15];
    }
    hash[32] = '\0';
}

#endif
