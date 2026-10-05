#include <SDL2/SDL.h>
#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Fixture engine services collect the production SDL input path's events. */
SDL_Window *sdl_gl_window;
static dvar_t mouse, raw;
dvar_t *in_mouse = &mouse, *in_rawmouse = &raw;
static clientActive_t *client;
void *imp_cl = &client;
static struct { int type, value, value2; } events[64];
static int count, absoluteX, absoluteY, relativeX, relativeY, cleared, quit;
static int pumps;
static int fullscreen, focused = 1, visible = 1, displayFocus = -1;
static Uint32 ticks = 1;
void MacTest_PumpEvents(void) { ++pumps; }
Uint32 MacTest_GetTicks(void) { return ticks; }
/* Drive focus states explicitly: desktop focus belongs to the user/test runner. */
Uint32 MacTest_GetWindowFlags(SDL_Window *window)
{
    return (SDL_GetWindowFlags(window) & ~SDL_WINDOW_INPUT_FOCUS) |
        (focused ? SDL_WINDOW_INPUT_FOCUS : 0);
}
int MacDisplay_IsFullscreen(void) { return fullscreen; }
void MacDisplay_FocusChanged(int focus) { displayFocus = focus; }
void MacDisplay_PumpVisibility(void) {}
int MacDisplay_WindowVisible(void) { return visible; }
extern int SDL_PumpInputEvents(void);
extern void IN_Frame(void);
void Sys_QueEvent(int time, sysEventType_t type, int value, int value2, int length, void *pointer)
{
    (void)time; (void)length; (void)pointer;
    assert(count < 64);
    events[count].type = type; events[count].value = value; events[count++].value2 = value2;
}
void CL_MouseEvent(int dx, int dy) { relativeX += dx; relativeY += dy; }
void CL_MouseEventAbsolute(int x, int y, int dx, int dy) { (void)dx; (void)dy; absoluteX = x; absoluteY = y; }
void Key_ClearStates(void) { ++cleared; }
void Cbuf_AddText(const char *text) { assert(!strcmp(text, "quit\n")); ++quit; }
void MacPlatform_MapWindowPoint(int x, int y, int *rx, int *ry) { *rx = x * 2; *ry = y * 2; }

/* SDL2-compat Event2to3 rejects injected TEXTINPUT and passes NULL to SDL3.
 * Feed synthetic events at the poll boundary; all conversion is production code. */
static SDL_Event pending[64];
static int pendingHead, pendingTail;
int MacTest_PollEvent(SDL_Event *event)
{
    if (pendingHead == pendingTail)
        return 0;
    *event = pending[pendingTail++ % 64];
    return 1;
}
int MacTest_PeepEvents(SDL_Event *event, int numevents, SDL_eventaction action, Uint32 minType, Uint32 maxType)
{
    assert(numevents == 1 && action == SDL_GETEVENT);
    assert(minType == SDL_FIRSTEVENT && maxType == SDL_LASTEVENT);
    return MacTest_PollEvent(event);
}
static void push(SDL_Event *event) { assert(pendingHead - pendingTail < 64); pending[pendingHead++ % 64] = *event; }
static void key(int type, SDL_Keycode code)
{
    SDL_Event e = {0}; e.type = type; e.key.keysym.sym = code; push(&e);
}
int main(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    SDL_PumpInputEvents();
    SDL_PumpInputEvents();
    assert(pumps == 1);
    ++ticks;
    SDL_PumpInputEvents();
    assert(pumps == 2);
    sdl_gl_window = SDL_CreateWindow("CoD2 input probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 240, SDL_WINDOW_SHOWN);
    assert(sdl_gl_window);
    SDL_RaiseWindow(sdl_gl_window);
    SDL_Delay(100);
    SDL_PumpEvents();
    SDL_PumpInputEvents();
    count = 0;
    key(SDL_KEYDOWN, SDLK_a); key(SDL_KEYUP, SDLK_a);
    key(SDL_KEYDOWN, SDLK_F1); key(SDL_KEYDOWN, SDLK_KP_ENTER); key(SDL_KEYDOWN, SDLK_BACKSPACE);
    SDL_Event e = {0};
    e.type = SDL_TEXTINPUT; strcpy(e.text.text, "A\xe2\x82\xac"); push(&e);
    e = (SDL_Event){0}; e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_RIGHT; push(&e);
    e.type = SDL_MOUSEBUTTONUP; push(&e);
    e = (SDL_Event){0}; e.type = SDL_MOUSEWHEEL; e.wheel.y = 2; e.wheel.direction = SDL_MOUSEWHEEL_FLIPPED; push(&e);
    e = (SDL_Event){0}; e.type = SDL_MOUSEMOTION; e.motion.x = 10; e.motion.y = 20; push(&e);
    SDL_PumpInputEvents();
    assert(count == 14);
    assert(events[0].value == 'a' && events[0].value2 == 1 && events[1].value2 == 0);
    assert(events[2].value == 0xa7 && events[3].value == 0xbf);
    assert(events[4].value == 0x7f && events[5].type == SE_CHAR && events[5].value == 8);
    assert(events[6].value == 'A' && events[7].value == 0x20ac);
    assert(events[8].value == 0xc9 && events[8].value2 == 1 && events[9].value2 == 0);
    for (int i = 10; i < 14; ++i)
        assert(events[i].value == 0xcd && events[i].value2 == !(i & 1));
    assert(absoluteX == 20 && absoluteY == 40);
    client = calloc(1, sizeof(*client));
    assert(client);
    client->active = 1; mouse.current.enabled = raw.current.enabled = 1;
    IN_Frame();
    relativeX = relativeY = 0;
    e = (SDL_Event){0}; e.type = SDL_MOUSEMOTION; e.motion.xrel = 7; e.motion.yrel = -4; push(&e);
    IN_Frame();
    assert(relativeX == 7 && relativeY == -4);
    client->keyCatchers = 2;
    fullscreen = 1;
    IN_Frame();
    assert(SDL_GetRelativeMouseMode() == SDL_TRUE);
    assert(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE);
    relativeX = relativeY = 0;
    e = (SDL_Event){0}; e.type = SDL_MOUSEMOTION; e.motion.xrel = 3; e.motion.yrel = 2; push(&e);
    IN_Frame();
    assert(relativeX == 3 && relativeY == 2);
    /* Mission Control occludes a still-key window without SDL focus loss. */
    int clearedBeforeOcclusion = cleared;
    visible = 0;
    IN_Frame();
    assert(cleared == clearedBeforeOcclusion + 1);
    assert(SDL_GetRelativeMouseMode() == SDL_FALSE);
    assert(SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE);
    visible = 1;
    IN_Frame();
    assert(SDL_GetRelativeMouseMode() == SDL_TRUE);
    assert(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE);
    focused = 0;
    e = (SDL_Event){0}; e.type = SDL_WINDOWEVENT; e.window.event = SDL_WINDOWEVENT_FOCUS_LOST; push(&e);
    e = (SDL_Event){0}; e.type = SDL_QUIT; push(&e);
    IN_Frame();
    assert(cleared && quit == 1 && displayFocus == 0);
    assert(SDL_GetRelativeMouseMode() == SDL_FALSE);
    assert(SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE);
    focused = 1;
    e = (SDL_Event){0}; e.type = SDL_WINDOWEVENT; e.window.event = SDL_WINDOWEVENT_FOCUS_GAINED; push(&e);
    IN_Frame();
    assert(displayFocus == 1 && SDL_GetRelativeMouseMode() == SDL_TRUE);
    assert(SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    free(client); SDL_DestroyWindow(sdl_gl_window); SDL_Quit();
    puts("input: keys, UTF-8 events, buttons, flipped multi-click wheel, scaled UI, relative fallback, focus and quit passed");
    return 0;
}
