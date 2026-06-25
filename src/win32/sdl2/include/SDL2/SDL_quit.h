#ifndef SDL_quit_h_
#define SDL_quit_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"

#define SDL_QuitRequested() \
        (SDL_PumpEvents(), (SDL_PeepEvents(NULL,0,SDL_PEEKEVENT,SDL_QUIT,SDL_QUIT) > 0))

#endif
