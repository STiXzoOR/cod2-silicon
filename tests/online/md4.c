#include "common_types.h"
#include <CommonCrypto/CommonDigest.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

void Com_Memcpy(void *dest, const void *src, int size) { memcpy(dest, src, size); }
void Com_Memset(void *dest, const int value, int size) { memset(dest, value, size); }
#include "md4_source.h"
static unsigned int ExpectedChecksum(const byte *data, int size, int keyed, int key)
{
    CC_MD4_CTX context;
    unsigned int digest[4];
    CC_MD4_Init(&context);
    if (keyed)
        CC_MD4_Update(&context, &key, sizeof(key));
    CC_MD4_Update(&context, data, size);
    CC_MD4_Final((byte *)digest, &context);
    return digest[0] ^ digest[1] ^ digest[2] ^ digest[3];
}
int main(void)
{
    byte data[320];
    int lengths[] = { 0, 1, 3, 55, 56, 64, 129, 320 };
    int keys[] = { 0, -1, 0x12345678 };
    for (int i = 0; i < sizeof(data); ++i)
        data[i] = (byte)(i * 17);
    for (int i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        assert(Com_BlockChecksum(data, lengths[i]) == ExpectedChecksum(data, lengths[i], 0, 0));
        for (int j = 0; j < sizeof(keys) / sizeof(keys[0]); ++j)
            assert(Com_BlockChecksumKey(data, lengths[i], keys[j]) ==
                   ExpectedChecksum(data, lengths[i], 1, keys[j]));
    }
    puts("online: MD4 IWD and keyed pure checksums match CommonCrypto");
    return 0;
}
