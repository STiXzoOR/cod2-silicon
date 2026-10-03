#ifndef COD2_MACOS_DISPLAY_H
#define COD2_MACOS_DISPLAY_H

#include <stdint.h>
const char **MacPlatform_ModeNames(void);

enum { MAC_WINDOWED, MAC_FULLSCREEN, MAC_BORDERLESS };
void MacPlatform_ConfigureWindow(int width, int height, int mode, int refresh);
void *MacDisplay_CreateScreenContext(int depth, int stencil, int samples,
                                    int quality, int interval, int *hasAux);
uint16_t MacDisplay_ReleaseContext(void **context);
void MacDisplay_ReleaseDisplay(void);
void SDL_GL_SwapWindowDirect(void);
int MacPlatform_GetRenderSize(int *width, int *height);
void MacPlatform_GetDrawableSize(int *width, int *height);
uint16_t MacDisplay_SetMode(int width, int height, int depth, int refresh);
void MacPlatform_TransformPoint(int x, int y, int logicalWidth, int logicalHeight,
                                int drawableWidth, int drawableHeight, int *renderX, int *renderY);
void MacPlatform_MapWindowPoint(int x, int y, int *renderX, int *renderY);
void MacGL_DrawBuffer(unsigned int buffer);
void MacGL_ReadBuffer(unsigned int buffer);

#endif
