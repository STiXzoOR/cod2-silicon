#ifndef SDL_keyboard_h_
#define SDL_keyboard_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_keycode.h"
#include "SDL_video.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Keysym
{
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
    Uint32 unused;
} SDL_Keysym;

extern DECLSPEC SDL_Window * SDLCALL SDL_GetKeyboardFocus(void);

extern DECLSPEC const Uint8 *SDLCALL SDL_GetKeyboardState(int *numkeys);

extern DECLSPEC void SDLCALL SDL_ResetKeyboard(void);

extern DECLSPEC SDL_Keymod SDLCALL SDL_GetModState(void);

extern DECLSPEC void SDLCALL SDL_SetModState(SDL_Keymod modstate);

extern DECLSPEC SDL_Keycode SDLCALL SDL_GetKeyFromScancode(SDL_Scancode scancode);

extern DECLSPEC SDL_Scancode SDLCALL SDL_GetScancodeFromKey(SDL_Keycode key);

extern DECLSPEC const char *SDLCALL SDL_GetScancodeName(SDL_Scancode scancode);

extern DECLSPEC SDL_Scancode SDLCALL SDL_GetScancodeFromName(const char *name);

extern DECLSPEC const char *SDLCALL SDL_GetKeyName(SDL_Keycode key);

extern DECLSPEC SDL_Keycode SDLCALL SDL_GetKeyFromName(const char *name);

extern DECLSPEC void SDLCALL SDL_StartTextInput(void);

extern DECLSPEC SDL_bool SDLCALL SDL_IsTextInputActive(void);

extern DECLSPEC void SDLCALL SDL_StopTextInput(void);

extern DECLSPEC void SDLCALL SDL_ClearComposition(void);

extern DECLSPEC SDL_bool SDLCALL SDL_IsTextInputShown(void);

extern DECLSPEC void SDLCALL SDL_SetTextInputRect(const SDL_Rect *rect);

extern DECLSPEC SDL_bool SDLCALL SDL_HasScreenKeyboardSupport(void);

extern DECLSPEC SDL_bool SDLCALL SDL_IsScreenKeyboardShown(SDL_Window *window);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
