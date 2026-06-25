#ifndef SDL_test_log_h_
#define SDL_test_log_h_

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

void SDLTest_Log(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

void SDLTest_LogError(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
