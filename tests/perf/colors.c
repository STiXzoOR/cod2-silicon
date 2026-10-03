#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char byte;
typedef unsigned int UINT;
static byte *g_colorArrayScratch;
static UINT g_colorArrayScratchCapacity;
enum { COLOR_BYTES_RGBA, COLOR_BYTES_BGRA, COLOR_BYTES_ARGB };
#include "color_array.h"

int main(void)
{
    /* Two interleaved vertices, with a nonzero color offset and padding. */
    const byte rgba[] = {99, 1, 2, 3, 4, 88, 99, 5, 6, 7, 8, 88};
    const byte expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    const byte bgra[] = {99, 3, 2, 1, 4, 88, 99, 7, 6, 5, 8, 88};
    const byte argb[] = {99, 4, 1, 2, 3, 88, 99, 8, 5, 6, 7, 88};
    const unsigned short indices[] = {1, 0, 1};
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, 1, 2, COLOR_BYTES_RGBA, indices, 3));
    assert(!g_colorArrayScratch && !g_colorArrayScratchCapacity);
    /* The production callers use the original interleaved pointer on NULL. */
    for (UINT i = 0; i < 2; ++i)
        assert(!memcmp(rgba + 6 * i + 1, expected + 4 * i, 4));
    assert(!memcmp(CDirect3DDevice_ConvertColorArray(bgra, 6, 1, 2, COLOR_BYTES_BGRA, indices, 3), expected, 8));
    assert(!memcmp(CDirect3DDevice_ConvertColorArray(argb, 6, 1, 2, COLOR_BYTES_ARGB, indices, 3), expected, 8));
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, -1, 2, COLOR_BYTES_BGRA, indices, 3));
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, 1, 0, COLOR_BYTES_BGRA, indices, 3));
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, 1, 2, COLOR_BYTES_BGRA, indices, 0));

    byte padded[12 * 6] = {0};
    memcpy(padded + 9 * 6, bgra, sizeof(bgra));
    const unsigned short selected[] = {10, 9, 10};
    g_colorArrayScratch = realloc(g_colorArrayScratch, 48);
    g_colorArrayScratchCapacity = 48;
    memset(g_colorArrayScratch, 0xa5, 48);
    const byte *colors = CDirect3DDevice_ConvertColorArray(padded, 6, 1, 12, COLOR_BYTES_BGRA, selected, 3);
    assert(!memcmp(colors + 9 * 4, expected, 8));
    /* Unreferenced prefixes and suffixes are neither read nor rewritten. */
    for (UINT i = 0; i < 9 * 4; ++i) assert(colors[i] == 0xa5);
    for (UINT i = 11 * 4; i < 48; ++i) assert(colors[i] == 0xa5);
    free(g_colorArrayScratch);
}
