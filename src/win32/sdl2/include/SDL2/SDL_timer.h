#ifndef SDL_timer_h_
#define SDL_timer_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

extern DECLSPEC Uint32 SDLCALL SDL_GetTicks(void);

extern DECLSPEC Uint64 SDLCALL SDL_GetTicks64(void);

#define SDL_TICKS_PASSED(A, B)  ((Sint32)((B) - (A)) <= 0)

extern DECLSPEC Uint64 SDLCALL SDL_GetPerformanceCounter(void);

extern DECLSPEC Uint64 SDLCALL SDL_GetPerformanceFrequency(void);

extern DECLSPEC void SDLCALL SDL_Delay(Uint32 ms);

typedef Uint32 (SDLCALL * SDL_TimerCallback) (Uint32 interval, void *param);

typedef int SDL_TimerID;

extern DECLSPEC SDL_TimerID SDLCALL SDL_AddTimer(Uint32 interval,
                                                 SDL_TimerCallback callback,
                                                 void *param);

extern DECLSPEC SDL_bool SDLCALL SDL_RemoveTimer(SDL_TimerID id);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
