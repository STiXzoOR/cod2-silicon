#include <SDL2/SDL.h>
#include "common_types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void IN_Frame(void);
SDL_Window *sdl_gl_window = (SDL_Window *)(uintptr_t)1;
static clientActive_t client;
static clientActive_t *clientPointer = &client;
void *imp_cl = &clientPointer;
static dvar_t mouse, raw, variables[4];
dvar_t *in_mouse = &mouse, *in_rawmouse = &raw;
static int variableCount, relativeMode, rawMode, rawActive, focused = 1;
static int rawX, rawY, resultX, resultY, pendingMotion;
static uint64_t now, rawEvents;

const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, unsigned short flags)
{
    (void)min; (void)max;
    assert(variableCount < 4);
    dvar_t *var = &variables[variableCount++];
    var->name = name; var->flags = flags;
    var->current.integer = var->latched.integer = value;
    return var;
}
void Dvar_SetInt(const dvar_t *value, int integer)
{
    dvar_t *var = (dvar_t *)value;
    var->current.integer = var->latched.integer = integer;
}
void CL_MouseEvent(int x, int y) { resultX += x; resultY += y; }
void CL_MouseEventAbsolute(int x, int y, int dx, int dy) { (void)x; (void)y; (void)dx; (void)dy; }
void Key_ClearStates(void) {}
void Cbuf_AddText(const char *text) { (void)text; }
void Sys_QueEvent(int time, sysEventType_t type, int value, int value2, int length, void *pointer)
{ (void)time; (void)type; (void)value; (void)value2; (void)length; (void)pointer; }
void MacPlatform_MapWindowPoint(int x, int y, int *rx, int *ry) { *rx = x; *ry = y; }
int MacRawMouse_Available(void) { return 1; }
void MacRawMouse_SetMode(int mode) { rawMode = mode; }
void MacRawMouse_SetActive(int active) { rawActive = active; }
uint64_t MacRawMouse_EventCount(void) { return rawEvents; }
uint64_t MacSystem_Nanoseconds(void) { return now; }
int MacRawMouse_Read(int *dx, int *dy) { *dx = rawX; *dy = rawY; rawX = rawY = 0; return *dx || *dy; }
SDL_bool Test_GetRelativeMouseMode(void) { return relativeMode ? SDL_TRUE : SDL_FALSE; }
int Test_SetRelativeMouseMode(SDL_bool value) { relativeMode = value; return 0; }
void Test_SetWindowGrab(SDL_Window *window, SDL_bool value) { (void)window; (void)value; }
Uint32 Test_GetWindowFlags(SDL_Window *window) { (void)window; return focused ? SDL_WINDOW_INPUT_FOCUS : 0; }
SDL_bool Test_IsTextInputActive(void) { return SDL_TRUE; }
void Test_StartTextInput(void) {}
int Test_PollEvent(SDL_Event *event)
{
    if (!pendingMotion) return 0;
    pendingMotion = 0;
    memset(event, 0, sizeof(*event));
    event->type = SDL_MOUSEMOTION;
    event->motion.xrel = 7; event->motion.yrel = -4;
    return 1;
}
Uint32 SDL_GetTicks(void) { return (Uint32)(now / 1000000); }
void SDL_PumpEvents(void) {}
int SDL_PeepEvents(SDL_Event *events, int count, SDL_eventaction action, Uint32 min, Uint32 max)
{
    assert(count == 1 && action == SDL_GETEVENT);
    (void)min; (void)max;
    return Test_PollEvent(events);
}

int main(void)
{
    client.active = 1;
    mouse.current.enabled = raw.current.enabled = 1;
    IN_Frame();
    assert(variableCount == 4 && rawMode == 1 && rawActive);
    assert(!strcmp(variables[0].name, "m_rinput"));
    assert((variables[0].flags & 0x21) == 0x21);
    assert((variables[1].flags & 0x40) == 0x40);
    pendingMotion = 1; rawX = 2; rawY = -3;
    now = 100000000; rawEvents = 100;
    IN_Frame();
    assert(resultX == 2 && resultY == -3); /* SDL duplicate was suppressed */
    assert(variables[1].current.integer == 100);
    assert(variables[2].current.integer == 100 && variables[3].current.integer == 100);
    variables[0].latched.integer = 0;
    pendingMotion = 1;
    IN_Frame();
    assert(rawMode == 0 && !rawActive && variables[0].current.integer == 0);
    assert(resultX == 9 && resultY == -7); /* mode 0 delivers SDL cursor deltas */
    assert(variables[1].current.integer == 0 && variables[2].current.integer == 0);
    variables[0].latched.integer = 2;
    IN_Frame();
    assert(rawMode == 2 && rawActive);
    focused = 0;
    IN_Frame();
    assert(!rawActive && !relativeMode);
    focused = 1; raw.current.enabled = 0;
    IN_Frame();
    assert(!rawActive); /* WS3 in_rawmouse remains an independent preference */
    puts("cod2x native input: modes, latch, suppression, focus and refresh dvars passed");
    return 0;
}
