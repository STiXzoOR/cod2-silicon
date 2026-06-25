#include "cod2_feature_config.h"

#if COD2_FEATURE_SCRIPT_DEBUGGER

#    define UI_Component ct_UI_Component
#    define UI_LinesComponent ct_UI_LinesComponent
#    define UI_ScrollPane ct_UI_ScrollPane
#    define UI_VerticalDivider ct_UI_VerticalDivider
#    define VariableUnion ct_VariableUnion
#    include "common_types.h"
#    undef UI_Component
#    undef UI_LinesComponent
#    undef UI_ScrollPane
#    undef UI_VerticalDivider
#    undef VariableUnion

#    include "scr_debugger_ui.h"

extern void Com_Printf(const char *fmt, ...);
extern void *imp_re;
extern void *imp_cls;

extern void IN_Frame(void);
extern void R_BeginDebugFrame(void);
extern void R_EndDebugFrame(void);
extern void R_EndFrame(void);
extern int Sys_Milliseconds(void);

extern sysEvent_t Sys_GetEvent(void);

extern void IN_ActivateMouse(int active);
extern void Con_CloseConsole(void);

extern const Scr_ScriptWatch_vtbl Scr_ScriptWatch_vtable;
extern const Scr_ScriptCallStack_vtbl Scr_ScriptCallStack_vtable;

extern Scr_ScriptWatch g_scriptWatch;
extern Scr_ScriptWatch *scr_debuggerWatch;

extern Scr_ScriptWindow **Scr_Debugger_GetScriptWindows(void);
extern int Scr_Debugger_GetScriptWindowCount(void);

extern void Scr_ScriptCallStack_UpdateStack(Scr_ScriptCallStack *self);

extern void Scr_ScriptWatch_UpdateBreakpoints(Scr_ScriptWatch *self, int announce);
extern void Scr_ScriptWatch_Evaluate(Scr_ScriptWatch *self);
extern void Scr_ScriptWatch_UpdateHeight(Scr_ScriptWatch *self);
extern void Scr_ScriptWatch_SortHitBreakpointsTop(Scr_ScriptWatch *self);

static int g_scrDebugPaused;

static Scr_ScriptCallStack g_scriptCallStack;
static int g_composeInited;

#    define DBG_K_F5 0xab
#    define DBG_K_F10 0xb0
#    define DBG_K_F11 0xb1
#    define DBG_K_ESCAPE 0x1b

#    define DBG_PUMP_WATCHDOG_MS 60000

static void Scr_Compose_EnsureInited(void)
{
    if (g_composeInited)
        return;
    g_composeInited = 1;

    g_scriptCallStack.vtbl = &Scr_ScriptCallStack_vtable;
    g_scriptCallStack.vtbl->Init(&g_scriptCallStack);

    if (g_scriptWatch.vtbl == 0) {
        g_scriptWatch.vtbl = &Scr_ScriptWatch_vtable;
        g_scriptWatch.vtbl->Init(&g_scriptWatch);
    }
}

void Scr_Debugger_InitComponents(void)
{
    Scr_Compose_EnsureInited();
}

void Scr_DrawDebugger(void)
{

    const float SW = 640.0f, SH = 480.0f;
    const float watchW = 256.0f;
    float srcW = SW - watchW;
    float watchH = SH * 0.5f;
    float stackH = SH - watchH;

    Scr_ScriptWindow **windows;
    int winCount;

    Scr_Compose_EnsureInited();

    windows = Scr_Debugger_GetScriptWindows();
    winCount = Scr_Debugger_GetScriptWindowCount();
    if (windows && winCount > 0) {
        int idx = scr_debuggerWatch ? scr_debuggerWatch->selectedLine : 0;
        if (idx < 0 || idx >= winCount)
            idx = 0;
        if (windows[idx] && windows[idx]->vtbl && windows[idx]->vtbl->Draw)
            windows[idx]->vtbl->Draw(windows[idx], 0.0f, 0.0f, srcW, SH,
                                     0.0f, windows[idx]->scrollTop);
    }

    if (g_scriptWatch.vtbl && g_scriptWatch.vtbl->Draw)
        g_scriptWatch.vtbl->Draw(&g_scriptWatch, srcW, 0.0f, watchW, watchH,
                                 0.0f, g_scriptWatch.scrollTop);

    if (g_scriptCallStack.vtbl && g_scriptCallStack.vtbl->Draw)
        g_scriptCallStack.vtbl->Draw(&g_scriptCallStack, srcW, watchH,
                                     watchW, stackH,
                                     0.0f, g_scriptCallStack.scrollTop);
}

static void Scr_Debugger_RouteKey(int key, int down)
{

    if (down) {
        if (key == DBG_K_F5 || key == DBG_K_F10 || key == DBG_K_F11 ||
            key == DBG_K_ESCAPE) {
            g_scrDebugPaused = 0;
            return;
        }
    }

    if (g_scriptWatch.hasFocus && g_scriptWatch.vtbl && g_scriptWatch.vtbl->KeyEvent) {
        g_scriptWatch.vtbl->KeyEvent(&g_scriptWatch, 0, key);
        return;
    }
    {
        Scr_ScriptWindow **windows = Scr_Debugger_GetScriptWindows();
        int winCount = Scr_Debugger_GetScriptWindowCount();
        int idx = scr_debuggerWatch ? scr_debuggerWatch->selectedLine : 0;
        if (windows && winCount > 0) {
            if (idx < 0 || idx >= winCount)
                idx = 0;
            if (windows[idx] && windows[idx]->vtbl && windows[idx]->vtbl->KeyEvent) {
                windows[idx]->vtbl->KeyEvent(windows[idx], 0, key);
                return;
            }
        }
    }

    if (g_scriptWatch.vtbl && g_scriptWatch.vtbl->KeyEvent)
        g_scriptWatch.vtbl->KeyEvent(&g_scriptWatch, 0, key);
}

void Debug_Frame(void)
{
    sysEvent_t ev;

    IN_Frame();

    for (;;) {
        ev = Sys_GetEvent();
        if ((int)ev.evType == SE_NONE)
            break;
        if ((int)ev.evType == SE_KEY)
            Scr_Debugger_RouteKey(ev.evValue, ev.evValue2);

        if (g_scrDebugPaused == 0)
            break;
    }

    R_BeginDebugFrame();
    Scr_DrawDebugger();
    R_EndFrame();
    R_EndDebugFrame();
}

void Com_CheckSyncFrame(void)
{
    int watchdogStart;

    if (g_scrDebugPaused == 0)
        return;

    IN_ActivateMouse(1);

    watchdogStart = Sys_Milliseconds();
    do {
        Debug_Frame();
        if (Sys_Milliseconds() - watchdogStart > DBG_PUMP_WATCHDOG_MS) {

            g_scrDebugPaused = 0;
            break;
        }
    } while (g_scrDebugPaused != 0);

    IN_ActivateMouse(1);
}

void Scr_DisplayDebugger(void)
{
    Scr_Compose_EnsureInited();

    Scr_ScriptCallStack_UpdateStack(&g_scriptCallStack);

    Scr_ScriptWatch_UpdateBreakpoints(&g_scriptWatch, 0);
    Scr_ScriptWatch_Evaluate(&g_scriptWatch);

    Scr_ScriptWatch_UpdateHeight(&g_scriptWatch);

    Con_CloseConsole();

    g_scrDebugPaused = 1;
    Com_CheckSyncFrame();

    Scr_ScriptWatch_UpdateBreakpoints(&g_scriptWatch, 0);
    g_scrDebugPaused = 0;
}

__attribute__((weak)) void IN_ActivateMouse(int active)
{
    (void)active;
}
__attribute__((weak)) void Con_CloseConsole(void) {}

#endif
