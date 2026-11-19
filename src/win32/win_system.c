/* Windows implementation of the native platform interface in platform/macos_system.h. */
#if defined(_WIN32) && defined(COD2_X64)
#include "platform/macos_system.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

uint64_t MacSystem_Nanoseconds(void)
{
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    if (!frequency.QuadPart)
        QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (uint64_t)((double)now.QuadPart * 1e9 / (double)frequency.QuadPart);
}

void MacSystem_ObserveFrame(int engineTime)
{
    (void)engineTime;
}

void MacSystem_WaitUntil(uint64_t deadline)
{
    /* Sleep to about 2 ms short, then spin: Sleep can wake a timer tick late. */
    uint64_t now = MacSystem_Nanoseconds();
    if (deadline > now && deadline - now > 2000000)
        Sleep((DWORD)((deadline - now - 2000000) / 1000000));
    while (MacSystem_Nanoseconds() < deadline)
        ;
}

char *MacSystem_HomePath(void)
{
    /* Windows keeps its files in the install folder: fs_homepath follows fs_basepath. */
    return NULL;
}

uint64_t MacSystem_MemoryBytes(void)
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
}

float MacSystem_CPUFrequencyGHz(void)
{
    HKEY key;
    DWORD mhz = 0, size = sizeof(mhz);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegQueryValueExA(key, "~MHz", NULL, NULL, (LPBYTE)&mhz, &size);
        RegCloseKey(key);
    }
    return (float)mhz / 1000.0f;
}

static DWORD Win_PageProtection(int prot)
{
    if (prot & 2)
        return PAGE_READWRITE;
    return prot & 1 ? PAGE_READONLY : PAGE_NOACCESS;
}

void *Win_MapPages(size_t length, int prot)
{
    void *base = VirtualAlloc(NULL, length, MEM_COMMIT | MEM_RESERVE, Win_PageProtection(prot));
    return base ? base : (void *)-1;
}

int Win_UnmapPages(void *address)
{
    return VirtualFree(address, 0, MEM_RELEASE) ? 0 : -1;
}

int Win_ProtectPages(void *address, size_t length, int prot)
{
    DWORD old;
    return VirtualProtect(address, length, Win_PageProtection(prot), &old) ? 0 : -1;
}

/* Mac power and splash calls in shared code. Windows has no equivalent to make. */
int UpdateSystemActivity(int activity)
{
    (void)activity;
    return 0;
}

void CMacGameEngine_DrawSplashScreen(const char *fileName)
{
    (void)fileName;
}

#if !defined(__x86_64__) && !defined(DEDICATED)
void *__ZTIl = 0;
#endif

#if !defined(DEDICATED)
/* SDL's relative mouse mode already reads raw input on Windows. */
void MacRawMouse_Init(void) {}
void MacRawMouse_Shutdown(void) {}
void MacRawMouse_SetActive(int active) { (void)active; }
int MacRawMouse_Available(void) { return 0; }
int MacRawMouse_Read(int *dx, int *dy) { (void)dx; (void)dy; return 0; }
#endif

void Sys_OpenURL(const char *url, int activate)
{
    (void)activate;
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

uintptr_t MacSystem_ImageOffset(const void *address)
{
    return (uintptr_t)address - (uintptr_t)GetModuleHandleA(NULL);
}

int MacSystem_CaptureStack(void **frames, int count, int skip)
{
    void *stack[62];
    int captured;
    if (count <= 0)
        return 0;
    if (count > 62 - skip - 1)
        count = 62 - skip - 1;
    captured = CaptureStackBackTrace((DWORD)(skip + 1), (DWORD)count, stack, NULL);
    memcpy(frames, stack, (size_t)captured * sizeof(*frames));
    return captured;
}

/* Windows 1.3 reads its CD key as the 21-byte "codkey" value of
 * HKLM\SOFTWARE\Activision\Call of Duty 2: 16 key characters, 4 checksum
 * characters and a NUL. It is a 32-bit program, so on 64-bit Windows it sees
 * the 32-bit registry view, and when the game was installed without admin
 * rights Windows redirected the installer's write to the user's VirtualStore,
 * which a 32-bit program then reads first. A 64-bit build sees neither on its
 * own, so look in the same places. */
int Win_ReadRegistryCDKey(char *key, int size)
{
    static const struct {
        HKEY root;
        const char *path;
        REGSAM view;
    } places[] = {
        { HKEY_CURRENT_USER, "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Activision\\Call of Duty 2", 0 },
        { HKEY_CURRENT_USER, "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Activision\\Call of Duty 2", 0 },
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", KEY_WOW64_32KEY },
        { HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", KEY_WOW64_64KEY },
    };
    size_t i;

    if (size < 21)
        return 0;
    for (i = 0; i < sizeof(places) / sizeof(places[0]); ++i) {
        HKEY handle;
        DWORD type = 0, bytes = 21;
        LONG result;
        if (RegOpenKeyExA(places[i].root, places[i].path, 0, KEY_QUERY_VALUE | places[i].view, &handle) != ERROR_SUCCESS)
            continue;
        result = RegQueryValueExA(handle, "codkey", NULL, &type, (BYTE *)key, &bytes);
        RegCloseKey(handle);
        if (result == ERROR_SUCCESS && bytes == 21)
            return 1;
    }
    return 0;
}
#endif
