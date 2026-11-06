#include "Mac/Metal/resource_formats.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    MacMetalFormatLayout layout;
    assert(!MacMetalFormat_Layout(999, 8, 8, 1, &layout));
    assert(!MacMetalFormat_Layout(21, 0, 8, 1, &layout));
    assert(!MacMetalFormat_Layout(21, UINT32_MAX, UINT32_MAX, UINT32_MAX, &layout));
    assert(MacMetalFormat_Layout(0x31545844, 5, 3, 2, &layout));
    assert(layout.rowPitch == 16 && layout.slicePitch == 16 && layout.length == 32);
    unsigned char out[4], input[] = { 29, 73, 151, 211 };
    assert(!MacMetalFormat_RGBA(21, 1, 1, 1, input, 3, 4, NULL, out));
    assert(!MacMetalFormat_RGBA(21, 1, 1, 2, input, 4, SIZE_MAX, NULL, out));
    assert(MacMetalFormat_RGBA(21, 1, 1, 1, input, 4, 4, NULL, out));
    assert(!memcmp(out, (unsigned char[]){151, 73, 29, 211}, 4));
    assert(MacMetalFormat_RGBA(22, 1, 1, 1, input, 4, 4, NULL, out));
    assert(out[3] == 211); /* Preserve measured GL XRGB input alpha. */
    assert(MacMetalFormat_RGBA(28, 1, 1, 1, input, 1, 1, NULL, out));
    assert(!memcmp(out, (unsigned char[]){0, 0, 0, 29}, 4));
    unsigned char value16[] = {0xff, 0xff};
    for (unsigned int format = 23; format <= 30; ++format) {
        assert(MacMetalFormat_Layout(format, 1, 1, 1, &layout));
        assert(MacMetalFormat_RGBA(format, 1, 1, 1, value16, layout.rowPitch, layout.slicePitch, NULL, out));
        for (unsigned int c = 0; c < 4; ++c) assert(out[c] == (format == 28 && c < 3 ? 0 : 255));
    }
    unsigned char packed[] = { 0xa3 };
    assert(MacMetalFormat_RGBA(52, 1, 1, 1, packed, 1, 1, NULL, out));
    assert(!memcmp(out, (unsigned char[]){51, 51, 51, 170}, 4));
    puts("resource formats: bounded pitches, tiny BC mips, BGRA/X alpha, legacy packed/luminance/alpha pass");
}
