#include "common_types.h"
#include <assert.h>
#include <stdio.h>

#include "PC/gfx_d3d/r_specularityimage.c"

int main(void)
{
    byte pixels[32 * 256];
    Image_BuildSpecularityMap(0.0f, pixels);
    for (int power = 0; power < 32; ++power) {
        assert(pixels[power] == 0);
        assert(pixels[255 * 32 + power] == 255);
    }
    assert(pixels[254 * 32 + 31] == 227);

    /* Mac promotes 0.1f before log; this valid shift exposes byte rounding. */
    Image_BuildSpecularityMap(0.06788204610347748f, pixels);
    assert(pixels[114 * 32] == 1);
    assert(pixels[113 * 32] == 0);
    assert(pixels[115 * 32] == 1);
    assert(pixels[254 * 32 + 31] == 227);
    puts("native specularity: original float constant and byte rounding passed");
    return 0;
}
