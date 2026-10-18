#ifndef COD2_METAL_RESOURCE_FORMATS_H
#define COD2_METAL_RESOURCE_FORMATS_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned int blockWidth, blockBytes;
    size_t rowPitch, slicePitch, length;
} MacMetalFormatLayout;
/* D3D bytes are retained for locks. Conversion produces classic linear RGBA,
 * with the same legacy GL upload channel interpretation (including X alpha). */
int MacMetalFormat_Layout(uint32_t format, unsigned int width, unsigned int height,
                          unsigned int depth, MacMetalFormatLayout *layout);
int MacMetalFormat_RGBA(uint32_t format, unsigned int width, unsigned int height,
                        unsigned int depth, const void *source, size_t rowPitch,
                        size_t slicePitch, const unsigned char palette[256][4],
                        unsigned char *rgba);
#endif
