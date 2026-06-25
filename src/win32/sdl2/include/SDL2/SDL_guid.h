#ifndef SDL_guid_h_
#define SDL_guid_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Uint8 data[16];
} SDL_GUID;

extern DECLSPEC void SDLCALL SDL_GUIDToString(SDL_GUID guid, char *pszGUID, int cbGUID);

extern DECLSPEC SDL_GUID SDLCALL SDL_GUIDFromString(const char *pchGUID);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
