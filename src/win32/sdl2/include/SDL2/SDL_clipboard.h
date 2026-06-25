#ifndef SDL_clipboard_h_
#define SDL_clipboard_h_

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC int SDLCALL SDL_SetClipboardText(const char *text);

extern DECLSPEC char * SDLCALL SDL_GetClipboardText(void);

extern DECLSPEC SDL_bool SDLCALL SDL_HasClipboardText(void);

extern DECLSPEC int SDLCALL SDL_SetPrimarySelectionText(const char *text);

extern DECLSPEC char * SDLCALL SDL_GetPrimarySelectionText(void);

extern DECLSPEC SDL_bool SDLCALL SDL_HasPrimarySelectionText(void);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
