#ifndef SDL_test_images_h_
#define SDL_test_images_h_

#include "SDL.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDLTest_SurfaceImage_s {
  int width;
  int height;
  unsigned int bytes_per_pixel;
  const char *pixel_data;
} SDLTest_SurfaceImage_t;

SDL_Surface *SDLTest_ImageBlit(void);
SDL_Surface *SDLTest_ImageBlitColor(void);
SDL_Surface *SDLTest_ImageBlitAlpha(void);
SDL_Surface *SDLTest_ImageBlitBlendAdd(void);
SDL_Surface *SDLTest_ImageBlitBlend(void);
SDL_Surface *SDLTest_ImageBlitBlendMod(void);
SDL_Surface *SDLTest_ImageBlitBlendNone(void);
SDL_Surface *SDLTest_ImageBlitBlendAll(void);
SDL_Surface *SDLTest_ImageFace(void);
SDL_Surface *SDLTest_ImagePrimitives(void);
SDL_Surface *SDLTest_ImagePrimitivesBlend(void);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
