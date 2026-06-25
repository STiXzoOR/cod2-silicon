#ifndef SDL_blendmode_h_
#define SDL_blendmode_h_

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SDL_BLENDMODE_NONE = 0x00000000,

    SDL_BLENDMODE_BLEND = 0x00000001,

    SDL_BLENDMODE_ADD = 0x00000002,

    SDL_BLENDMODE_MOD = 0x00000004,

    SDL_BLENDMODE_MUL = 0x00000008,

    SDL_BLENDMODE_INVALID = 0x7FFFFFFF

} SDL_BlendMode;

typedef enum
{
    SDL_BLENDOPERATION_ADD              = 0x1,
    SDL_BLENDOPERATION_SUBTRACT         = 0x2,
    SDL_BLENDOPERATION_REV_SUBTRACT     = 0x3,
    SDL_BLENDOPERATION_MINIMUM          = 0x4,
    SDL_BLENDOPERATION_MAXIMUM          = 0x5
} SDL_BlendOperation;

typedef enum
{
    SDL_BLENDFACTOR_ZERO                = 0x1,
    SDL_BLENDFACTOR_ONE                 = 0x2,
    SDL_BLENDFACTOR_SRC_COLOR           = 0x3,
    SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR = 0x4,
    SDL_BLENDFACTOR_SRC_ALPHA           = 0x5,
    SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA = 0x6,
    SDL_BLENDFACTOR_DST_COLOR           = 0x7,
    SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR = 0x8,
    SDL_BLENDFACTOR_DST_ALPHA           = 0x9,
    SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA = 0xA
} SDL_BlendFactor;

extern DECLSPEC SDL_BlendMode SDLCALL SDL_ComposeCustomBlendMode(SDL_BlendFactor srcColorFactor,
                                                                 SDL_BlendFactor dstColorFactor,
                                                                 SDL_BlendOperation colorOperation,
                                                                 SDL_BlendFactor srcAlphaFactor,
                                                                 SDL_BlendFactor dstAlphaFactor,
                                                                 SDL_BlendOperation alphaOperation);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
