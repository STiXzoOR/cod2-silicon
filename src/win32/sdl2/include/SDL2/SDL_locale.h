#ifndef _SDL_locale_h
#define _SDL_locale_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus

extern "C" {

#endif

typedef struct SDL_Locale
{
    const char *language;
    const char *country;
} SDL_Locale;

extern DECLSPEC SDL_Locale * SDLCALL SDL_GetPreferredLocales(void);

#ifdef __cplusplus

}

#endif
#include "close_code.h"

#endif
