#ifndef SHADERGEN_SHA256_H
#define SHADERGEN_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t length;
    unsigned char block[64];
    size_t used;
} Sha256;

void Sha256_Init(Sha256 *s);
void Sha256_Update(Sha256 *s, const void *data, size_t size);
void Sha256_Final(Sha256 *s, unsigned char digest[32]);
/* hex must hold 65 bytes. */
void Sha256_Hex(const void *data, size_t size, char *hex);
void Sha256_ToHex(const unsigned char digest[32], char *hex);

#endif
