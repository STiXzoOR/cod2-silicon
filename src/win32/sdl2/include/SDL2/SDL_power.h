#ifndef SDL_power_h_
#define SDL_power_h_

#include "SDL_stdinc.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDL_POWERSTATE_UNKNOWN,
    SDL_POWERSTATE_ON_BATTERY,
    SDL_POWERSTATE_NO_BATTERY,
    SDL_POWERSTATE_CHARGING,
    SDL_POWERSTATE_CHARGED
} SDL_PowerState;

extern DECLSPEC SDL_PowerState SDLCALL SDL_GetPowerInfo(int *seconds, int *percent);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
