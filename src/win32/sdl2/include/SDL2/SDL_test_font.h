#ifndef SDL_test_font_h_
#define SDL_test_font_h_

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_CHARACTER_SIZE 8
#define FONT_LINE_HEIGHT    (FONT_CHARACTER_SIZE + 2)

int SDLTest_DrawCharacter(SDL_Renderer *renderer, int x, int y, Uint32 c);

int SDLTest_DrawString(SDL_Renderer *renderer, int x, int y, const char *s);

typedef struct SDLTest_TextWindow
{
    SDL_Rect rect;
    int current;
    int numlines;
    char **lines;
} SDLTest_TextWindow;

SDLTest_TextWindow *SDLTest_TextWindowCreate(int x, int y, int w, int h);

void SDLTest_TextWindowDisplay(SDLTest_TextWindow *textwin, SDL_Renderer *renderer);

void SDLTest_TextWindowAddText(SDLTest_TextWindow *textwin, SDL_PRINTF_FORMAT_STRING const char *fmt, ...) SDL_PRINTF_VARARG_FUNC(2);

void SDLTest_TextWindowAddTextWithLength(SDLTest_TextWindow *textwin, const char *text, size_t len);

void SDLTest_TextWindowClear(SDLTest_TextWindow *textwin);

void SDLTest_TextWindowDestroy(SDLTest_TextWindow *textwin);

void SDLTest_CleanupTextDrawing(void);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
