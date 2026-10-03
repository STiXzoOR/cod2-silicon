#include "macos_jpeg.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    unsigned char rgb[32 * 32 * 3];
    for (int i = 0; i < 32 * 32; ++i) {
        rgb[i * 3] = 200; rgb[i * 3 + 1] = 50; rgb[i * 3 + 2] = 25;
    }
    size_t size;
    unsigned char *jpg = MacJpeg_Encode(rgb, 32, 32, 90, &size);
    assert(jpg && size > 100 && jpg[0] == 0xff && jpg[1] == 0xd8);
    int width = 0, height = 0;
    unsigned char *rgba = MacJpeg_Decode(jpg, size, 4096, &width, &height);
    assert(rgba && width == 32 && height == 32);
    assert(abs(rgba[0] - 200) < 5 && abs(rgba[1] - 50) < 5 && abs(rgba[2] - 25) < 5 && rgba[3] == 255);
    assert(!MacJpeg_Decode(jpg, size, 16, &width, &height));
    assert(!MacJpeg_Decode(rgb, sizeof(rgb), 4096, &width, &height));
    free(rgba); free(jpg);
    puts("native JPEG encoding, RGB round trip and dimension bounds: PASS");
}
