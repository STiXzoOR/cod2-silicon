#ifndef COD2_CDKEY_HASH_H
#define COD2_CDKEY_HASH_H

#if defined(COD2_X64) && COD2_X64 && !defined(__APPLE__)
/* RFC 1321 MD5 with the same state fields as CommonCrypto's CC_MD5_CTX, which the
 * seeding adjusts. No system library on Linux or Windows exposes those fields. */
#include <stdint.h>
#include <string.h>

#define CC_MD5_DIGEST_LENGTH 16
typedef struct {
    uint32_t A, B, C, D;
    uint64_t length;
    unsigned char block[64];
    unsigned int used;
} CC_MD5_CTX;

static inline void CC_MD5_Init(CC_MD5_CTX *context)
{
    context->A = 0x67452301u;
    context->B = 0xefcdab89u;
    context->C = 0x98badcfeu;
    context->D = 0x10325476u;
    context->length = 0;
    context->used = 0;
}

static inline void Com_CDKeyMD5Block(CC_MD5_CTX *context, const unsigned char *data)
{
    static const unsigned char shifts[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };
    static const uint32_t sines[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
    };
    uint32_t words[16], a = context->A, b = context->B, c = context->C, d = context->D;
    int i;
    memcpy(words, data, sizeof(words)); /* x86 is little-endian, as MD5 expects. */
    for (i = 0; i < 64; ++i) {
        uint32_t mix, next = d;
        int word;
        if (i < 16) {
            mix = (b & c) | (~b & d);
            word = i;
        } else if (i < 32) {
            mix = (d & b) | (~d & c);
            word = (5 * i + 1) % 16;
        } else if (i < 48) {
            mix = b ^ c ^ d;
            word = (3 * i + 5) % 16;
        } else {
            mix = c ^ (b | ~d);
            word = (7 * i) % 16;
        }
        mix += a + sines[i] + words[word];
        d = c;
        c = b;
        b += (mix << shifts[(i / 16) * 4 + i % 4]) | (mix >> (32 - shifts[(i / 16) * 4 + i % 4]));
        a = next;
    }
    context->A += a;
    context->B += b;
    context->C += c;
    context->D += d;
}

static inline void CC_MD5_Update(CC_MD5_CTX *context, const void *data, unsigned int length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    context->length += length;
    while (length) {
        unsigned int chunk = 64 - context->used;
        if (chunk > length)
            chunk = length;
        memcpy(context->block + context->used, bytes, chunk);
        context->used += chunk;
        bytes += chunk;
        length -= chunk;
        if (context->used == 64) {
            Com_CDKeyMD5Block(context, context->block);
            context->used = 0;
        }
    }
}

static inline void CC_MD5_Final(unsigned char *digest, CC_MD5_CTX *context)
{
    static const unsigned char pad[64] = { 0x80 };
    uint64_t bits = context->length * 8;
    uint32_t state[4];
    CC_MD5_Update(context, pad, context->used < 56 ? 56 - context->used : 120 - context->used);
    CC_MD5_Update(context, &bits, 8);
    state[0] = context->A;
    state[1] = context->B;
    state[2] = context->C;
    state[3] = context->D;
    memcpy(digest, state, 16);
}
#else
#include <CommonCrypto/CommonDigest.h>
#endif

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
