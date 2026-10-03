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
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, 1, 2, COLOR_BYTES_RGBA));
    assert(!g_colorArrayScratch && !g_colorArrayScratchCapacity);
    /* The production callers use the original interleaved pointer on NULL. */
    for (UINT i = 0; i < 2; ++i)
        assert(!memcmp(rgba + 6 * i + 1, expected + 4 * i, 4));
    assert(!memcmp(CDirect3DDevice_ConvertColorArray(bgra, 6, 1, 2, COLOR_BYTES_BGRA), expected, 8));
    assert(!memcmp(CDirect3DDevice_ConvertColorArray(argb, 6, 1, 2, COLOR_BYTES_ARGB), expected, 8));
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, -1, 2, COLOR_BYTES_BGRA));
    assert(!CDirect3DDevice_ConvertColorArray(rgba, 6, 1, 0, COLOR_BYTES_BGRA));
    free(g_colorArrayScratch);
}
