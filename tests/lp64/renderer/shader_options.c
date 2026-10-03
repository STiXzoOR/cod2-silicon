#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define COD2_APPLE_SDK 1
#include "Mac/DirectX_9/lp64_shader_options.h"
int main(void)
{
    unsetenv("COD2_MAC_SHADER_CACHE"); unsetenv("D3D_PROG");
    assert(!MacShader_UseCachePrograms());
    setenv("D3D_PROG", "1", 1); assert(!MacShader_UseCachePrograms());
    setenv("COD2_MAC_SHADER_CACHE", "/licensed/cache", 1);
    unsetenv("D3D_PROG"); assert(MacShader_UseCachePrograms());
    setenv("D3D_PROG", "0", 1); assert(!MacShader_UseCachePrograms());
    setenv("D3D_PROG", "1", 1); assert(MacShader_UseCachePrograms());
    setenv("COD2_MAC_SHADER_CACHE", "", 1); assert(!MacShader_UseCachePrograms());
}
