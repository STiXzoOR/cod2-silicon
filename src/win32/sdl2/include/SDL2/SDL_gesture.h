#ifndef SDL_gesture_h_
#define SDL_gesture_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "SDL_touch.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Sint64 SDL_GestureID;

extern DECLSPEC int SDLCALL SDL_RecordGesture(SDL_TouchID touchId);

extern DECLSPEC int SDLCALL SDL_SaveAllDollarTemplates(SDL_RWops *dst);

extern DECLSPEC int SDLCALL SDL_SaveDollarTemplate(SDL_GestureID gestureId,SDL_RWops *dst);

extern DECLSPEC int SDLCALL SDL_LoadDollarTemplates(SDL_TouchID touchId, SDL_RWops *src);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
