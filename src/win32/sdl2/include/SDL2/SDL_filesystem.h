#ifndef SDL_filesystem_h_
#define SDL_filesystem_h_

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC char *SDLCALL SDL_GetBasePath(void);

extern DECLSPEC char *SDLCALL SDL_GetPrefPath(const char *org, const char *app);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
