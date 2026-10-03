#include "common_types.h"
#include "imports.h"

extern bool g_InhibitOpenGLErrors;

#if defined(COD2_X64)
void game_dprintf(const char *inFormat, ...)
{
    (void)inFormat;
}
#else
inflate_huft game_dprintf(const char *inFormat);

inflate_huft game_dprintf(const char *inFormat)
{
    inflate_huft result = { 0 };
    return result;
}
#endif
