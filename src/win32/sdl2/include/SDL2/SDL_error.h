#ifndef SDL_error_h_
#define SDL_error_h_

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC int SDLCALL SDL_SetError(SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(1);

extern DECLSPEC const char *SDLCALL SDL_GetError(void);

extern DECLSPEC char * SDLCALL SDL_GetErrorMsg(char *errstr, int maxlen);

extern DECLSPEC void SDLCALL SDL_ClearError(void);

#define SDL_OutOfMemory()   SDL_Error(SDL_ENOMEM)
#define SDL_Unsupported()   SDL_Error(SDL_UNSUPPORTED)
#define SDL_InvalidParamError(param)    SDL_SetError("Parameter '%s' is invalid", (param))
typedef enum
{
    SDL_ENOMEM,
    SDL_EFREAD,
    SDL_EFWRITE,
    SDL_EFSEEK,
    SDL_UNSUPPORTED,
    SDL_LASTERROR
} SDL_errorcode;

extern DECLSPEC int SDLCALL SDL_Error(SDL_errorcode code);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
