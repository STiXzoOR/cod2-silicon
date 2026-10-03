#include <assert.h>
#include <stddef.h>
#include <string.h>
static int enabled, calls;
static char *TestGetenv(const char *name)
{
    assert(!strcmp(name, "D3D_PROG"));
    ++calls;
    return enabled ? "1" : NULL;
}
#define getenv TestGetenv
#include "renderer_options_source.h"
int main(int argc, char **argv)
{
    (void)argv;
    enabled = argc > 1;
    for (int i = 0; i < 1000; ++i)
        assert(CDirect3DDevice_UsePrograms() == enabled);
    assert(calls == 1);
    return 0;
}
