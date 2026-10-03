#if COD2_APPLE_SDK
#include <stdlib.h>
#include <string.h>
/* A locally extracted licensed cache selects the real Mac shader path by
 * default. D3D_PROG=0 explicitly requests the fixed-function approximation. */
static int MacShader_UseCachePrograms(void)
{
    const char *cache = getenv("COD2_MAC_SHADER_CACHE");
    const char *option = getenv("D3D_PROG");
    return cache && *cache && (!option || strcmp(option, "0"));
}
#endif
