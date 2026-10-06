/* No window or OS input: characterize injected events at the real input pump. */
#include <SDL.h>
#include "common_types.h"
#include <stdio.h>
#include <string.h>

extern int SDL_PumpInputEvents(void);
static dvar_t mouse, raw;
dvar_t *in_mouse = &mouse, *in_rawmouse = &raw;
SDL_Window *sdl_gl_window;
void *imp_cl;
void MacDisplay_PumpVisibility(void) {}
void MacDisplay_FocusChanged(int focused) { (void)focused; }
int MacDisplay_WindowVisible(void) { return 0; }
int MacDisplay_IsFullscreen(void) { return 0; }
void MacRawMouse_SetActive(int active) { (void)active; }
int MacRawMouse_Available(void) { return 0; }
int MacRawMouse_Read(int *dx, int *dy) { (void)dx; (void)dy; return 0; }
void Key_ClearStates(void) {}
void Cbuf_AddText(const char *text) { (void)text; }
void MacPlatform_MapWindowPoint(int x, int y, int *rx, int *ry) { *rx = x; *ry = y; }
int MacPlatform_GetRenderSize(int *width, int *height) { *width = 640; *height = 480; return 1; }
void CL_MouseEvent(int dx, int dy)
{
    printf("{\"engine\":\"relative\",\"dx\":%d,\"dy\":%d}\n", dx, dy);
}
void CL_MouseEventAbsolute(int x, int y, int dx, int dy)
{
    (void)dx; (void)dy;
    printf("{\"engine\":\"absolute\",\"x\":%d,\"y\":%d}\n", x, y);
}
void Sys_QueEvent(int time, sysEventType_t type, int value, int value2, int length, void *pointer)
{
    (void)time; (void)length; (void)pointer;
    printf("{\"engine\":\"event\",\"type\":%d,\"value\":%d,\"down\":%d}\n", type, value, value2);
}
int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    puts("{\"ready\":true}");
    Uint32 deadline = SDL_GetTicks() + 10000;
    while (SDL_GetTicks() < deadline) {
        if (argc > 1 && !strcmp(argv[1], "--sdl-only")) {
            SDL_Event event;
            while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) > 0) {}
        } else {
            SDL_PumpInputEvents();
        }
        SDL_Delay(1);
    }
    SDL_Quit();
    return 0;
}
