#ifndef SDL_loadso_h_
#define SDL_loadso_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC void *SDLCALL SDL_LoadObject(const char *sofile);

extern DECLSPEC void *SDLCALL SDL_LoadFunction(void *handle,
                                               const char *name);

extern DECLSPEC void SDLCALL SDL_UnloadObject(void *handle);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
