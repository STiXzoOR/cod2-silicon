#include <assert.h>
#include <stddef.h>
#include <string.h>
static int enabled, calls;
static char *programOption;
static char *TestGetenv(const char *name)
{
    ++calls;
    if (!strcmp(name, "COD2_MAC_SHADER_CACHE"))
        return enabled ? "/licensed/cache" : NULL;
    assert(!strcmp(name, "D3D_PROG"));
    return programOption;
}
#define getenv TestGetenv
#define COD2_APPLE_SDK 1
#include "Mac/DirectX_9/lp64_shader_options.h"
#include "renderer_options_source.h"
int main(int argc, char **argv)
{
    enabled = argc > 1;
    if (enabled && !strcmp(argv[1], "off"))
        programOption = "0";
    int expected = enabled && !programOption;
    for (int i = 0; i < 1000; ++i)
        assert(CDirect3DDevice_UsePrograms() == expected);
    assert(calls == 2);
    return 0;
}
