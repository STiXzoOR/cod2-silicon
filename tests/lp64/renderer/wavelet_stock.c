/* Optional licensed-data audit. Inputs/decoded output must stay outside git. */
#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Wavelet_DecompressLevel(byte *, byte *, WaveletDecode *);

int main(int argc, char **argv)
{
    assert(argc == 3);
    FILE *input = fopen(argv[1], "rb");
    assert(input && !fseek(input, 0, SEEK_END));
    long length = ftell(input);
    assert(length >= 28 && !fseek(input, 0, SEEK_SET));
    /* The 16-bit reservoir sits two bytes ahead; its refill reads another
     * dword. The native image loader supplies six bytes for this lookahead. */
    byte *file = calloc(1, length + 6);
    assert(file && fread(file, 1, length, input) == length);
    fclose(input);
    assert(!memcmp(file, "IWi", 3) && file[3] == 5);
    assert(file[4] == 6 || file[4] == 7);
    unsigned short width, height, depth;
    memcpy(&width, file + 6, 2);
    memcpy(&height, file + 8, 2);
    memcpy(&depth, file + 10, 2);
    assert(width && height && depth == 1 && !(file[5] & 4));
    int start = 0;
    if (!(file[5] & 2))
        for (int dimension = 1; dimension < width || dimension < height; dimension *= 2)
            ++start;
    size_t size = (size_t)width * height * 4;
    byte *pixels = calloc(1, size);
    assert(pixels);
    WaveletDecode decode = { .data = file + 28, .width = width, .height = height,
                             .channels = file[4] == 6 ? 4 : 3, .bpp = 4 };
    FILE *output = fopen(argv[2], "wb");
    assert(output);
    byte *previous = NULL;
    int levels = 0;
    for (int level = start; level >= 0; --level) {
        int w = width >> level, h = height >> level;
        if (!w) w = 1;
        if (!h) h = 1;
        size_t mipSize = (size_t)w * h * 4;
        byte *next = pixels + size - mipSize;
        decode.mipLevel = level;
        Wavelet_DecompressLevel(previous, next, &decode);
        assert(decode.data <= file + length + 2);
        if (file[4] == 7)
            for (size_t pixel = 0; pixel < mipSize; pixel += 4)
                assert(next[pixel + 3] == 255);
        assert(fwrite(next, 1, mipSize, output) == mipSize);
        previous = next;
        ++levels;
    }
    assert(!fclose(output));
    printf("%s: format=%u %ux%u mips=%d consumed=%td/%ld\n", argv[1], file[4],
            width, height, levels, decode.data - file, length);
    free(pixels);
    free(file);
    return 0;
}
