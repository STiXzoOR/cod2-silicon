/* CoD2x raw mouse input on Windows (CoD2x src/mss32/rinput.cpp), reimplemented.
 * m_rinput 1 reads WM_INPUT on a thread of its own through a hidden window,
 * m_rinput 2 on the main thread through the game window, and 0 leaves the mouse
 * to the system cursor. SDL is kept on cursor warping in relative mode, so it
 * never registers raw input itself and these registrations stay ours. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#include <SDL2/SDL.h>
#include <windows.h>
#include <stdint.h>
#include "platform/macos_rawmouse.h"

extern void *Win_GameWindow(void);

static CRITICAL_SECTION mouseLock;
static int lockReady, active, mode, available;
static long deltaX, deltaY;
static uint64_t eventCount;
static HWND threadWindow, mainTarget;
static HANDLE thread, threadReady;
static const char *threadError;

static void Win_RawMouseInput(LPARAM lParam)
{
    RAWINPUT raw;
    UINT size = sizeof(raw);
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1 ||
        raw.header.dwType != RIM_TYPEMOUSE)
        return;
    EnterCriticalSection(&mouseLock);
    if (active) {
        deltaX += raw.data.mouse.lLastX;
        deltaY += raw.data.mouse.lLastY;
    }
    ++eventCount;
    LeaveCriticalSection(&mouseLock);
}

static LRESULT CALLBACK Win_RawMouseWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INPUT) {
        Win_RawMouseInput(lParam);
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

static int Win_RawMouseRegister(HWND target, DWORD flags)
{
    RAWINPUTDEVICE device;
    device.usUsagePage = 0x01; /* generic desktop */
    device.usUsage = 0x02;     /* mouse */
    device.dwFlags = flags;
    device.hwndTarget = flags & RIDEV_REMOVE ? NULL : target;
    return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
}

static DWORD WINAPI Win_RawMouseThread(LPVOID argument)
{
    WNDCLASSEXA windowClass;
    MSG message;
    (void)argument;
    memset(&windowClass, 0, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = Win_RawMouseWindow;
    windowClass.hInstance = GetModuleHandleA(NULL);
    windowClass.lpszClassName = "CoD2_Rinput";
    if (!RegisterClassExA(&windowClass)) {
        threadError = "Failed to register raw input device.\nFailed to register input class!";
        SetEvent(threadReady);
        return 1;
    }
    threadWindow = CreateWindowExA(0, "CoD2_Rinput", "CoD2_Rinput", 0, 0, 0, 0, 0, NULL, NULL,
                                   windowClass.hInstance, NULL);
    if (!threadWindow) {
        UnregisterClassA("CoD2_Rinput", windowClass.hInstance);
        threadError = "Failed to register raw input device.\nFailed to create input window!";
        SetEvent(threadReady);
        return 1;
    }
    if (!Win_RawMouseRegister(threadWindow, 0)) {
        DestroyWindow(threadWindow);
        threadWindow = NULL;
        UnregisterClassA("CoD2_Rinput", windowClass.hInstance);
        threadError = "Failed to register raw input device.";
        SetEvent(threadReady);
        return 1;
    }
    SetEvent(threadReady);
    while (GetMessageA(&message, NULL, 0, 0) > 0)
        DispatchMessageA(&message);
    Win_RawMouseRegister(NULL, RIDEV_REMOVE);
    DestroyWindow(threadWindow);
    threadWindow = NULL;
    UnregisterClassA("CoD2_Rinput", windowClass.hInstance);
    return 0;
}

/* Mode 2: WM_INPUT for the game window is read before SDL dispatches it. */
static void SDLCALL Win_RawMouseHook(void *userdata, void *window, unsigned int message, Uint64 wParam,
                                     Sint64 lParam)
{
    (void)userdata;
    (void)window;
    (void)wParam;
    if (message == WM_INPUT && mode == 2)
        Win_RawMouseInput((LPARAM)lParam);
}

static void Win_RawMouseStop(void)
{
    if (mode == 1 && thread) {
        PostThreadMessageA(GetThreadId(thread), WM_QUIT, 0, 0);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        thread = NULL;
    } else if (mode == 2) {
        Win_RawMouseRegister(NULL, RIDEV_REMOVE);
        mainTarget = NULL;
    }
    EnterCriticalSection(&mouseLock);
    available = 0;
    deltaX = deltaY = 0;
    LeaveCriticalSection(&mouseLock);
}

void MacRawMouse_Init(void)
{
    if (!lockReady) {
        InitializeCriticalSection(&mouseLock);
        lockReady = 1;
    }
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
    SDL_SetWindowsMessageHook(Win_RawMouseHook, NULL);
}

void MacRawMouse_Shutdown(void)
{
    if (!lockReady)
        return;
    MacRawMouse_SetActive(0);
    Win_RawMouseStop();
    mode = 0;
    SDL_SetWindowsMessageHook(NULL, NULL);
}

const char *MacRawMouse_SetMode(int value)
{
    if (!lockReady)
        MacRawMouse_Init();
    if (value == mode)
        return NULL;
    Win_RawMouseStop();
    mode = 0;
    if (value <= 0)
        return NULL;
    /* RInput injected into the game would fight over the same device. */
    if (FindWindowA("Rinput", NULL))
        return "Couldn't register raw input device.\nAnother instance of raw input is already running\n\n"
               "m_rinput was set to 0.";
    if (value == 1) {
        threadError = NULL;
        threadReady = CreateEventA(NULL, TRUE, FALSE, NULL);
        thread = threadReady ? CreateThread(NULL, 0, Win_RawMouseThread, NULL, 0, NULL) : NULL;
        if (!thread) {
            if (threadReady)
                CloseHandle(threadReady);
            threadReady = NULL;
            return "Failed to register raw input device.";
        }
        WaitForSingleObject(threadReady, INFINITE);
        CloseHandle(threadReady);
        threadReady = NULL;
        if (threadError) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
            thread = NULL;
            return threadError;
        }
    } else {
        mainTarget = (HWND)Win_GameWindow();
        if (!Win_RawMouseRegister(mainTarget, 0)) {
            mainTarget = NULL;
            return "Failed to register raw input device.";
        }
    }
    mode = value;
    EnterCriticalSection(&mouseLock);
    available = 1;
    LeaveCriticalSection(&mouseLock);
    return NULL;
}

void MacRawMouse_SetActive(int value)
{
    if (!lockReady)
        return;
    /* A new game window after a video restart takes over the registration. */
    if (value && mode == 2 && mainTarget != (HWND)Win_GameWindow()) {
        mainTarget = (HWND)Win_GameWindow();
        Win_RawMouseRegister(mainTarget, 0);
    }
    EnterCriticalSection(&mouseLock);
    if (value != active) {
        deltaX = deltaY = 0;
        active = value;
    }
    LeaveCriticalSection(&mouseLock);
}

int MacRawMouse_Available(void)
{
    int value;
    if (!lockReady)
        return 0;
    EnterCriticalSection(&mouseLock);
    value = available;
    LeaveCriticalSection(&mouseLock);
    return value;
}

int MacRawMouse_Read(int *dx, int *dy)
{
    if (!lockReady) {
        *dx = *dy = 0;
        return 0;
    }
    EnterCriticalSection(&mouseLock);
    *dx = (int)deltaX;
    *dy = (int)deltaY;
    deltaX = deltaY = 0;
    LeaveCriticalSection(&mouseLock);
    return *dx != 0 || *dy != 0;
}

uint64_t MacRawMouse_EventCount(void)
{
    uint64_t count;
    if (!lockReady)
        return 0;
    EnterCriticalSection(&mouseLock);
    count = eventCount;
    LeaveCriticalSection(&mouseLock);
    return count;
}
#endif
