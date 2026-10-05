/* Test-only menu actions and presentation trace; no OS input synthesis. */
#include <SDL.h>
#include "common_types.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static FILE *trace;
static uiInfo_t **info;
static sharedUiInfo_t *shared;
static const char *(*args)(int);
static void (*runScript)(const char **);
static void (*itemScript)(displayContextDef_t *, itemDef_t *, const char *);
static void (*mouseEvent)(int, int);
static void (*textColor)(displayContextDef_t *, itemDef_t *, float *);
static void (*placement)(float *, float *, float *, float *, int, int);
static void (*screenX)(float *, int);
static void (*screenY)(float *, int);

static itemDef_t *findItem(const char *menuName, const char *itemName)
{
    if (!info || !*info) return NULL;
    displayContextDef_t *dc = &(*info)->uiDC;
    for (int m = 0; m < dc->menuCount; ++m) {
        menuDef_t *menu = dc->Menus[m];
        if (!menu || !menu->window.name || strcmp(menu->window.name, menuName)) continue;
        for (int i = 0; i < menu->itemCount; ++i) {
            itemDef_t *item = menu->items[i];
            if (item && item->window.name && !strcmp(item->window.name, itemName)) return item;
        }
    }
    return NULL;
}

static void join(void)
{
    itemDef_t *item = findItem("main_text", "play");
    if (item && item->action) itemScript(&(*info)->uiDC, item, item->action);
    else fprintf(stderr, "[ui-probe] Join Server action missing\n");
}

static void action(void)
{
    const char *script = args(1);
    runScript(&script);
}

static void hover(void)
{
    itemDef_t *item = findItem("joinserver", "refreshSource");
    if (!item) return;
    /* Text rectangles use baseline Y and alignment; cursor coordinates use
     * full-screen virtual scaling. Transform both through the engine helpers. */
    rectDef_t rect = item->textRect[0];
    rect.y -= rect.h;
    placement(&rect.x, &rect.y, &rect.w, &rect.h, rect.horzAlign, rect.vertAlign);
    float scaleX = 1, scaleY = 1;
    screenX(&scaleX, 4);
    screenY(&scaleY, 4);
    int x = (rect.x + rect.w / 2) / scaleX;
    int y = (rect.y + rect.h / 2) / scaleY;
    displayContextDef_t *dc = &(*info)->uiDC;
    mouseEvent(x - dc->cursorx, y - dc->cursory);
}

static void startTrace(void)
{
    if (trace) fclose(trace);
    trace = fopen(args(1), "w");
    if (trace) fputs("time,flags,cursor_x,cursor_y,r,g,b,a,back_r,back_g,back_b,back_a,focus_r,focus_g,focus_b,focus_a\n", trace);
}

static void state(void)
{
    fprintf(stderr, "[ui-probe] displayed=%d refresh=%d players=%d gametypes=%d\n",
            shared->serverStatus.numDisplayServers, shared->serverStatus.refreshActive,
            shared->serverStatus.numPlayersOnServers, shared->numJoinGameTypes);
}

static int peep(SDL_Event *events, int count, SDL_eventaction eventAction, Uint32 min, Uint32 max)
{
    static int installed;
    if (!installed++) {
        void (*add)(const char *, void (*)(void)) = dlsym(RTLD_DEFAULT, "Cmd_AddCommand");
        args = dlsym(RTLD_DEFAULT, "Cmd_Args");
        runScript = dlsym(RTLD_DEFAULT, "UI_RunMenuScript");
        itemScript = dlsym(RTLD_DEFAULT, "Item_RunScript");
        mouseEvent = dlsym(RTLD_DEFAULT, "UI_MouseEvent");
        textColor = dlsym(RTLD_DEFAULT, "Item_TextColor");
        placement = dlsym(RTLD_DEFAULT, "CalcScreenPlacement");
        screenX = dlsym(RTLD_DEFAULT, "CalcScreenX");
        screenY = dlsym(RTLD_DEFAULT, "CalcScreenY");
        info = dlsym(RTLD_DEFAULT, "uiInfo");
        shared = dlsym(RTLD_DEFAULT, "sharedUiInfo");
        if (add && args && runScript && itemScript && mouseEvent && textColor &&
            placement && screenX && screenY && info && shared) {
            add("qa_join", join);
            add("qa_menu_script", action);
            add("qa_hover", hover);
            add("qa_ui_trace", startTrace);
            add("qa_ui_state", state);
        } else fprintf(stderr, "[ui-probe] missing exports\n");
    }
    return SDL_PeepEvents(events, count, eventAction, min, max);
}

static void swap(SDL_Window *window)
{
    itemDef_t *item = trace ? findItem("joinserver", "refreshSource") : NULL;
    if (item) {
        displayContextDef_t *dc = &(*info)->uiDC;
        float color[4];
        textColor(dc, item, color);
        fprintf(trace, "%d,%u,%d,%d,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g\n",
                dc->realTime, item->window.dynamicFlags[0], dc->cursorx, dc->cursory,
                color[0], color[1], color[2], color[3], item->window.backColor[0],
                item->window.backColor[1], item->window.backColor[2], item->window.backColor[3],
                item->parent->focusColor[0], item->parent->focusColor[1],
                item->parent->focusColor[2], item->parent->focusColor[3]);
        fflush(trace);
    }
    SDL_GL_SwapWindow(window);
}

#define INTERPOSE(new, old) \
    __attribute__((used)) static const struct { const void *a, *b; } ip_##old \
    __attribute__((section("__DATA,__interpose"))) = { (const void *)new, (const void *)old }
INTERPOSE(peep, SDL_PeepEvents);
INTERPOSE(swap, SDL_GL_SwapWindow);
