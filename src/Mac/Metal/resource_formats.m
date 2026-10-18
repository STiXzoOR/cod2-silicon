#include "resource_formats.h"
#include <string.h>

#define FOURCC(a,b,c,d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
static int Multiply(size_t a, size_t b, size_t *result)
{
    if (a && b > SIZE_MAX / a) return 0;
    *result = a * b;
    return 1;
}
int MacMetalFormat_Layout(uint32_t format, unsigned int width, unsigned int height,
                          unsigned int depth, MacMetalFormatLayout *layout)
{
    if (!layout || !width || !height || !depth) return 0;
    unsigned int block = 1, bytes;
    switch (format) {
    case 20: bytes = 3; break; /* R8G8B8: the GL path reads RGB bytes. */
    case 21: case 22: bytes = 4; break;
    case 23: case 24: case 25: case 26: case 29: case 30:
    case 40: case 51: bytes = 2; break;
    case 27: case 28: case 41: case 50: case 52: bytes = 1; break;
    case FOURCC('D','X','T','1'): bytes = 8; block = 4; break;
    case FOURCC('D','X','T','2'): case FOURCC('D','X','T','3'):
    case FOURCC('D','X','T','4'): case FOURCC('D','X','T','5'):
        bytes = 16; block = 4; break;
    default: return 0;
    }
    size_t columns = ((size_t)width + block - 1) / block;
    size_t rows = ((size_t)height + block - 1) / block;
    *layout = (MacMetalFormatLayout){ block, bytes, 0, 0, 0 };
    return Multiply(columns, bytes, &layout->rowPitch) &&
           Multiply(layout->rowPitch, rows, &layout->slicePitch) &&
           Multiply(layout->slicePitch, depth, &layout->length);
}
static uint16_t Read16(const unsigned char *p) { return p[0] | (uint16_t)p[1] << 8; }
static unsigned char Expand(unsigned int value, unsigned int bits)
{
    /* GL's normalized packed input rounds to the nearest 8-bit value. */
    unsigned int maximum = (1u << bits) - 1;
    return (value * 255 + maximum / 2) / maximum;
}
static void RGB565(uint16_t value, unsigned char *out)
{
    out[0] = Expand(value >> 11, 5);
    out[1] = Expand((value >> 5) & 63, 6);
    out[2] = Expand(value & 31, 5);
    out[3] = 255;
}
static void BCBlock(uint32_t format, const unsigned char *block, unsigned char pixels[16][4])
{
    int bc1 = format == FOURCC('D','X','T','1');
    int bc2 = format == FOURCC('D','X','T','2') || format == FOURCC('D','X','T','3');
    const unsigned char *color = block + (bc1 ? 0 : 8);
    unsigned char colors[4][4];
    uint16_t a = Read16(color), b = Read16(color + 2);
    RGB565(a, colors[0]); RGB565(b, colors[1]);
    for (unsigned int c = 0; c < 3; ++c) {
        if (!bc1 || a > b) {
            colors[2][c] = (2 * colors[0][c] + colors[1][c]) / 3;
            colors[3][c] = (colors[0][c] + 2 * colors[1][c]) / 3;
        } else {
            colors[2][c] = (colors[0][c] + colors[1][c]) / 2;
            colors[3][c] = 0;
        }
    }
    colors[2][3] = 255; colors[3][3] = bc1 && a <= b ? 0 : 255;
    unsigned char alpha[8] = { block[0], block[1] };
    uint64_t alphaIndices = 0;
    if (!bc1 && !bc2) {
        for (unsigned int i = 0; i < 6; ++i) alphaIndices |= (uint64_t)block[2 + i] << (8 * i);
        if (alpha[0] > alpha[1]) {
            for (unsigned int i = 2; i < 8; ++i) alpha[i] = ((8 - i) * alpha[0] + (i - 1) * alpha[1]) / 7;
        } else {
            for (unsigned int i = 2; i < 6; ++i) alpha[i] = ((6 - i) * alpha[0] + (i - 1) * alpha[1]) / 5;
            alpha[6] = 0; alpha[7] = 255;
        }
    }
    for (unsigned int i = 0; i < 16; ++i) {
        memcpy(pixels[i], colors[(color[4 + i / 4] >> (2 * (i % 4))) & 3], 4);
        if (bc2) pixels[i][3] = ((block[i / 2] >> (4 * (i % 2))) & 15) * 17;
        else if (!bc1) pixels[i][3] = alpha[(alphaIndices >> (i * 3)) & 7];
    }
}
int MacMetalFormat_RGBA(uint32_t format, unsigned int width, unsigned int height,
                        unsigned int depth, const void *source, size_t rowPitch,
                        size_t slicePitch, const unsigned char palette[256][4],
                        unsigned char *rgba)
{
    MacMetalFormatLayout layout;
    if (!source || !rgba || !MacMetalFormat_Layout(format, width, height, depth, &layout)) return 0;
    size_t rows = ((size_t)height + layout.blockWidth - 1) / layout.blockWidth;
    if (rowPitch < layout.rowPitch || rowPitch > SIZE_MAX / rows || slicePitch < rowPitch * rows ||
        (depth > 1 && slicePitch > SIZE_MAX / (depth - 1)) ||
        (depth - 1) * slicePitch > SIZE_MAX - ((rows - 1) * rowPitch + layout.rowPitch)) return 0;
    const unsigned char *src = source;
    if (layout.blockWidth == 4) {
        for (unsigned int z = 0; z < depth; ++z) {
            for (unsigned int y = 0; y < height; y += 4) {
                for (unsigned int x = 0; x < width; x += 4) {
                    unsigned char pixels[16][4];
                    BCBlock(format, src + z * slicePitch + (y / 4) * rowPitch + (x / 4) * layout.blockBytes, pixels);
                    for (unsigned int py = 0; py < 4 && y + py < height; ++py)
                        for (unsigned int px = 0; px < 4 && x + px < width; ++px)
                            memcpy(rgba + (((size_t)z * height + y + py) * width + x + px) * 4, pixels[py * 4 + px], 4);
                }
            }
        }
        return 1;
    }
    for (unsigned int z = 0; z < depth; ++z) {
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                const unsigned char *p = src + z * slicePitch + y * rowPitch + x * layout.blockBytes;
                unsigned char *out = rgba + (((size_t)z * height + y) * width + x) * 4;
                unsigned int v = layout.blockBytes == 2 ? Read16(p) : *p;
                out[0] = out[1] = out[2] = 0; out[3] = 255;
                switch (format) {
                case 20: memcpy(out, p, 3); break;
                case 21: case 22: out[0] = p[2]; out[1] = p[1]; out[2] = p[0]; out[3] = p[3]; break;
                case 23: RGB565(v, out); break;
                case 24: case 25:
                    out[0] = Expand((v >> 10) & 31, 5); out[1] = Expand((v >> 5) & 31, 5); out[2] = Expand(v & 31, 5);
                    out[3] = format == 24 || v & 0x8000 ? 255 : 0; break;
                case 26: case 30:
                    out[0] = ((v >> 8) & 15) * 17; out[1] = ((v >> 4) & 15) * 17; out[2] = (v & 15) * 17;
                    out[3] = format == 30 ? 255 : (v >> 12) * 17; break;
                case 27: case 29:
                    out[0] = Expand((v >> 5) & 7, 3); out[1] = Expand((v >> 2) & 7, 3); out[2] = Expand(v & 3, 2);
                    if (format == 29) out[3] = v >> 8; break;
                case 28: out[3] = *p; break;
                case 40: case 41:
                    if (!palette) return 0;
                    memcpy(out, palette[*p], 4);
                    if (format == 40) out[3] = p[1]; break;
                case 50: case 51: out[0] = out[1] = out[2] = *p; if (format == 51) out[3] = p[1]; break;
                case 52: out[0] = out[1] = out[2] = (*p & 15) * 17; out[3] = (*p >> 4) * 17; break;
                default: return 0;
                }
            }
        }
    }
    return 1;
}
