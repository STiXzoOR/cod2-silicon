#ifndef SDL_test_memory_h_
#define SDL_test_memory_h_

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

int SDLTest_TrackAllocations(void);

void SDLTest_LogAllocations(void);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
