#include "common_types.h"
#include "imports.h"

#if defined(COD2_X64)
/* C++ bool flags: one byte in the Mac binary and in the typed native data. */
extern Bool g_InhibitOpenGLErrors;
#else
extern bool g_InhibitOpenGLErrors;
#endif

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
