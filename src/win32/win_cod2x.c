/* What CoD2x 1.4.6.8 adds to the Windows client outside the engine, reimplemented
 * for this executable: the start-up checks of the game folder (CoD2x
 * src/mss32/main.cpp, admin.cpp), its registry values (registry.cpp, hwid.cpp),
 * cod2x:// links (url_protocol.cpp), the freeze watchdog (freeze.cpp) and the
 * crash dialog (exception_dialog.cpp). The crash and freeze reports themselves are
 * in win_cod2x_report.c.
 *
 * CoD2MP_s.exe is a 32-bit program without a manifest, so Windows virtualizes its
 * writes to HKLM into the user's VirtualStore and lets it see the 32-bit view of
 * the registry. A 64-bit program gets neither, so the same keys are opened here
 * explicitly, as the game's CD key is read in win_system.c. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/cod2x_native.h"
#include "PC/qcommon/cod2x.h"
#include "PC/qcommon/cod2x_url.h"
#include "win32/win_cod2x.h"

#define COD2X_APP_VERSION "1.4.6.8"
#define COD2X_GAME_KEY "SOFTWARE\\Activision\\Call of Duty 2"
#define COD2X_STORE_KEY "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Activision\\Call of Duty 2"
#define COD2X_STORE_KEY_WOW64 \
    "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Activision\\Call of Duty 2"

extern const void *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags);
extern unsigned char Dvar_GetBool(const char *name);
extern void Cbuf_AddText(const char *text);
extern int com_errorEntered;

static char exePath[MAX_PATH], exeDirectory[MAX_PATH], commandLine[1024];
static char previousVersion[64];
static int versionChanged, versionWritten;
static char pendingURL[512];

/* ---- Messages ------------------------------------------------------------ */

static void Win_Cod2xMessage(const char *title, const char *format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    MessageBoxA(NULL, text, title, MB_ICONERROR | MB_OK | MB_TOPMOST);
}

/* CoD2x's box for a failure it does not expect, naming where it happened. */
static void Win_Cod2xErrorAt(const char *function, int line, int withLastError, const char *format, ...)
{
    char message[512], system[256], text[1536];
    const char *name = strrchr(exePath, '\\') ? strrchr(exePath, '\\') + 1 : "the game";
    DWORD error = GetLastError();
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (withLastError) {
        size_t length;
        system[0] = '\0';
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), system, sizeof(system), NULL);
        length = strlen(system);
        while (length && (system[length - 1] == '\n' || system[length - 1] == '\r'))
            system[--length] = '\0';
        snprintf(message + strlen(message), sizeof(message) - strlen(message), "\n\n%s (error %lu)", system,
                 (unsigned long)error);
    }
    snprintf(text, sizeof(text),
             "An error occurred in CoD2x module '%s' " COD2X_APP_VERSION ":\n\n%s\n\nFile: %s\nFunction: %s\n"
             "Line: %d\n\nTry reinstalling CoD2x or check for updates. If the issue persists, report the error "
             "to developers.",
             name, message, __FILE__, function, line);
    MessageBoxA(NULL, text, "CoD2x - Error", MB_ICONERROR | MB_OK | MB_TOPMOST);
}
#define WIN_COD2X_ERROR(...) Win_Cod2xErrorAt(__func__, __LINE__, 0, __VA_ARGS__)

void Win_Cod2xFatal(const char *message)
{
    Win_Cod2xErrorAt(__func__, __LINE__, 0, "%s", message);
    exit(1);
}
#define WIN_COD2X_ERROR_LAST(...) Win_Cod2xErrorAt(__func__, __LINE__, 1, __VA_ARGS__)

/* CoD2x src/mss32/hwid.cpp: no machine identity, no game. */
int Sys_Cod2xHwidRetry(unsigned int code, const char *error)
{
    Win_Cod2xMessage("Fatal Error", "Error while generating HWID.\n\nUnable to load WMI properties.\nError: 0x%08X %s",
                     code, error);
    if (MessageBoxA(NULL,
                    "Do you want to retry or exit?\n\nIf the problem persists, please restart your computer and try "
                    "again.",
                    "Retry or Exit", MB_RETRYCANCEL | MB_ICONQUESTION) != IDRETRY)
        return 0;
    Sleep(500);
    return 1;
}

/* ---- The executable and its folder (CoD2x main.cpp) ---------------------- */

static int Win_Cod2xExecutable(const char *arguments)
{
    WCHAR wide[MAX_PATH];
    char converted[MAX_PATH];
    char *slash;
    DWORD wideLength;
    int length, unusual;
    if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        WIN_COD2X_ERROR_LAST("Failed to get the executable path.");
        return 0;
    }
    wideLength = GetModuleFileNameW(NULL, wide, MAX_PATH);
    if (!wideLength) {
        WIN_COD2X_ERROR_LAST("Failed to get the executable path in Unicode.");
        return 0;
    }
    /* CoD2x keeps 30 characters spare for the files it builds from this path. */
    if (wideLength >= MAX_PATH - 30) {
        Win_Cod2xMessage("Path too long",
                         "Call of Duty 2 directory path is too long.\n\nPlease move the game to a directory with a "
                         "shorter path.\n\nCurrent path: \n%s\n\n",
                         exePath);
        return 0;
    }
    /* A character the ANSI code page cannot hold becomes '?'. */
    length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide, -1, converted, MAX_PATH, NULL, NULL);
    unusual = length == 0 || length >= MAX_PATH || memchr(converted, '?', (size_t)length) != NULL;
    if (unusual) {
        if (length == 0)
            converted[0] = '\0';
        Win_Cod2xMessage("Non-standard characters in path",
                         "Call of Duty 2 directory path contains non-standard characters.\n\nPlease move the game "
                         "folder or rename the directory path to a path that contains only standard characters "
                         "(A-Z, a-z, 0-9, and symbols like _).\n\nPath: \n%s\n\n'?' - question mark represents "
                         "the invalid character",
                         converted);
        return 0;
    }
    memcpy(exeDirectory, exePath, sizeof(exeDirectory));
    slash = strrchr(exeDirectory, '\\');
    if (!slash) {
        WIN_COD2X_ERROR_LAST("Failed to determine executable directory.");
        return 0;
    }
    *slash = '\0';
    snprintf(commandLine, sizeof(commandLine), "%s", arguments ? arguments : "");
    /* A link opened from a browser starts the game in some other folder. */
    if (!SetCurrentDirectoryA(exeDirectory)) {
        WIN_COD2X_ERROR_LAST("Failed to set current directory");
        return 0;
    }
    return 1;
}

const char *Win_Cod2xExeName(void)
{
    const char *slash;
    if (!exePath[0])
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
    slash = strrchr(exePath, '\\');
    return slash ? slash + 1 : exePath;
}

const char *Win_Cod2xExePath(void)
{
    if (!exePath[0])
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return exePath;
}

const char *Win_Cod2xExeDirectory(void)
{
    if (!exeDirectory[0]) {
        char *slash;
        memcpy(exeDirectory, Win_Cod2xExePath(), sizeof(exeDirectory));
        slash = strrchr(exeDirectory, '\\');
        if (slash)
            *slash = '\0';
    }
    return exeDirectory;
}

const char *Win_Cod2xCommandLine(void)
{
    return commandLine;
}

/* ---- Administrator rights and write access (CoD2x admin.cpp) ------------- */

int Win_Cod2xElevated(void)
{
    HANDLE token = NULL;
    TOKEN_ELEVATION elevation;
    DWORD size = 0;
    SID_IDENTIFIER_AUTHORITY authority = {SECURITY_NT_AUTHORITY};
    PSID administrators = NULL;
    BOOL member = FALSE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        WIN_COD2X_ERROR_LAST("Failed to open process token.");
        return 0;
    }
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        CloseHandle(token);
        return elevation.TokenIsElevated != 0;
    }
    /* Windows XP, or a compatibility mode: membership of Administrators. */
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0,
                                 0, 0, 0, &administrators)) {
        CheckTokenMembership(NULL, administrators, &member);
        FreeSid(administrators);
    }
    CloseHandle(token);
    return member != FALSE;
}

static int Win_Cod2xUnderVirtualStore(const char *path)
{
    char local[MAX_PATH], store[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local) != S_OK)
        return 0;
    snprintf(store, sizeof(store), "%s\\VirtualStore", local);
    return _strnicmp(path, store, strlen(store)) == 0;
}

static int Win_Cod2xFolderWritable(const char *path)
{
    char test[MAX_PATH], resolved[MAX_PATH] = {0};
    HANDLE file;
    DWORD length;
    snprintf(test, sizeof(test), "%s\\__test__.tmp", path);
    file = CreateFileA(test, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    length = GetFinalPathNameByHandleA(file, resolved, MAX_PATH, 0);
    CloseHandle(file);
    if (!strncmp(resolved, "\\\\?\\", 4))
        memmove(resolved, resolved + 4, strlen(resolved + 4) + 1);
    return !(length > 0 && Win_Cod2xUnderVirtualStore(resolved));
}

static int Win_Cod2xUnderWine(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_build_id") != NULL;
}

static void Win_Cod2xWriteAccess(void)
{
    char text[1024];
    SHELLEXECUTEINFOA run;
    if (Win_Cod2xElevated() || Win_Cod2xFolderWritable(exeDirectory) || Win_Cod2xUnderWine())
        return;
    snprintf(text, sizeof(text),
             "Call of Duty 2 requires write access to the game directory.\n\nPlease do one of the following:\n"
             "- Move the game directory to a location with write access.\n"
             "- Run the game with elevated administrator permissions.\n"
             "- Run the game with compatibility mode set to Windows XP.\n\n"
             "This is needed because the game needs to write files to the game installation directory, which is "
             "not allowed by default on newer Windows versions.\n\n"
             "Its recommended to move the game directory to a location with write access (e.g. C:\\Games\\Call of "
             "Duty 2) and uncheck administrator and compatibility mode for maximum security, if possible.\n\n"
             "Current game directory: \n%s\n",
             exeDirectory);
    MessageBoxA(NULL, text, "Error", MB_ICONERROR);
    if (MessageBoxA(NULL,
                    "Would you like to restart the game with administrator privileges?\n\nWarning:\nFor security "
                    "reasons, this is not recommended.\nThe recommended way is to move the game directory to a "
                    "location with write access (e.g. C:\\Games\\Call of Duty 2) and uncheck administrator and "
                    "compatibility mode for maximum security, if possible.\n\n",
                    "Run as administrator", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        memset(&run, 0, sizeof(run));
        run.cbSize = sizeof(run);
        run.lpVerb = "runas";
        run.lpFile = exePath;
        run.lpParameters = commandLine;
        run.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExA(&run))
            MessageBoxA(NULL, "Failed to restart as administrator.", "Error", MB_ICONERROR);
    }
    ExitProcess(0);
}

/* ---- The game's registry key (CoD2x registry.cpp) ------------------------ */

static const char *Win_Cod2xStoreKey(void)
{
    BOOL wow64 = FALSE;
    if (sizeof(void *) == 8 || (IsWow64Process(GetCurrentProcess(), &wow64) && wow64))
        return COD2X_STORE_KEY_WOW64;
    return COD2X_STORE_KEY;
}

/* The key the 32-bit game writes to: HKLM when it may, else its VirtualStore copy. */
static int Win_Cod2xGameKey(HKEY *key)
{
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, COD2X_GAME_KEY, 0, NULL, REG_OPTION_NON_VOLATILE,
                        KEY_READ | KEY_WRITE | KEY_WOW64_32KEY, NULL, key, NULL) == ERROR_SUCCESS)
        return 1;
    if (Win_Cod2xElevated())
        return 0;
    return RegCreateKeyExA(HKEY_CURRENT_USER, Win_Cod2xStoreKey(), 0, NULL, REG_OPTION_NON_VOLATILE,
                           KEY_READ | KEY_WRITE, NULL, key, NULL) == ERROR_SUCCESS;
}

/* Reads see the VirtualStore copy first, as the 32-bit game does. */
int Win_Cod2xRegRead(const char *name, DWORD *type, void *data, DWORD *size)
{
    HKEY key;
    DWORD capacity = *size;
    LONG result = ERROR_FILE_NOT_FOUND;
    if (!Win_Cod2xElevated() && RegOpenKeyExA(HKEY_CURRENT_USER, Win_Cod2xStoreKey(), 0, KEY_READ, &key) ==
                                    ERROR_SUCCESS) {
        result = RegQueryValueExA(key, name, NULL, type, data, size);
        RegCloseKey(key);
        if (result == ERROR_SUCCESS)
            return 1;
        *size = capacity;
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, COD2X_GAME_KEY, 0, KEY_READ | KEY_WOW64_32KEY, &key) == ERROR_SUCCESS) {
        result = RegQueryValueExA(key, name, NULL, type, data, size);
        RegCloseKey(key);
    }
    return result == ERROR_SUCCESS;
}

int Win_Cod2xRegWrite(const char *name, DWORD type, const void *data, DWORD size)
{
    HKEY key;
    LONG result;
    if (!Win_Cod2xGameKey(&key))
        return 0;
    result = RegSetValueExA(key, name, 0, type, data, size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

int Win_Cod2xRegDelete(const char *name)
{
    HKEY key;
    if (!Win_Cod2xGameKey(&key))
        return 0;
    RegDeleteValueA(key, name);
    RegCloseKey(key);
    return 1;
}

/* ---- What CoD2x's update request says about this machine (CoD2x system.cpp, hwid.cpp) ---- */

/* What CoD2x's system.cpp gathers about Windows. */
void Win_Cod2xSystem(Win_Cod2xSystemText *out)
{
    char *build = out->build, *edition = out->edition, *display = out->display, *country = out->country;
    char *virtualization = out->virtualization, *compatibility = out->compatibility, bios[256] = "";
    DWORD length;
    HKEY key;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    typedef const char *(__cdecl * WineText)(void);
    WineText wineVersion = ntdll ? (WineText)(void *)GetProcAddress(ntdll, "wine_get_version") : NULL;
    WineText wineBuild = ntdll ? (WineText)(void *)GetProcAddress(ntdll, "wine_get_build_id") : NULL;
    memset(out, 0, sizeof(*out));
    snprintf(virtualization, sizeof(out->virtualization), "0");
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS ||
        RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ,
                      &key) == ERROR_SUCCESS) {
        length = sizeof(out->build) - 1;
        RegQueryValueExA(key, "CurrentBuildNumber", NULL, NULL, (LPBYTE)build, &length);
        length = sizeof(out->edition) - 1;
        RegQueryValueExA(key, "EditionID", NULL, NULL, (LPBYTE)edition, &length);
        length = sizeof(out->display) - 1;
        RegQueryValueExA(key, "DisplayVersion", NULL, NULL, (LPBYTE)display, &length);
        RegCloseKey(key);
    }
    if (wineVersion) {
        snprintf(virtualization, sizeof(out->virtualization), "wine-%s-%s", wineVersion() ? wineVersion() : "",
                 wineBuild && wineBuild() ? wineBuild() : "");
    } else {
        length = sizeof(bios);
        if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer",
                         RRF_RT_REG_SZ, NULL, bios, &length) == ERROR_SUCCESS) {
            static const char *const names[][2] = {{"vmware", "VMware"},
                                                   {"virtualbox", "VirtualBox"},
                                                   {"qemu", "QEMU"},
                                                   {"microsoft corporation", "Hyper-V"},
                                                   {"parallels", "Parallels"}};
            size_t i;
            CharLowerA(bios);
            for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
                if (strstr(bios, names[i][0])) {
                    snprintf(virtualization, sizeof(out->virtualization), "VM-%s", names[i][1]);
                    break;
                }
            }
        }
    }
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, sizeof(out->country)))
        snprintf(country, sizeof(out->country), "??");
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
                      0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        length = sizeof(out->compatibility) - 1;
        if (!exePath[0])
            GetModuleFileNameA(NULL, exePath, MAX_PATH);
        RegQueryValueExA(key, exePath, NULL, NULL, (LPBYTE)compatibility, &length);
        RegCloseKey(key);
    }
    out->elevated = Win_Cod2xElevated();
}

/* "build\nedition\ndisplay version\ncountry\nvirtualisation\ncompatibility flags\nadmin|user" */
static void Win_Cod2xVersionInfo(char *out, size_t size)
{
    Win_Cod2xSystemText system;
    Win_Cod2xSystem(&system);
    snprintf(out, size, "%s\n%s\n%s\n%s\n%s\n%s\n%s", system.build, system.edition, system.display, system.country,
             system.virtualization, system.compatibility, system.elevated ? "admin" : "user");
}

void Sys_Cod2xMachineHistory(Cod2xMachineHistory *out)
{
    DWORD type, size, count = 0;
    memset(out, 0, sizeof(*out));
    size = sizeof(out->regid) - 1;
    if (!Win_Cod2xRegRead("REGID", &type, out->regid, &size) || type != REG_SZ)
        out->regid[0] = '\0';
    size = sizeof(out->previous) - 1;
    if (!Win_Cod2xRegRead("HWID_OLD", &type, out->previous, &size) || type != REG_SZ)
        out->previous[0] = '\0';
    size = sizeof(out->changed) - 1;
    if (!Win_Cod2xRegRead("HWID_DIFF", &type, out->changed, &size) || type != REG_SZ)
        out->changed[0] = '\0';
    size = sizeof(count);
    if (Win_Cod2xRegRead("HWID_DIFF_COUNT", &type, &count, &size) && type == REG_DWORD)
        out->changes = (int)count;
    Win_Cod2xVersionInfo(out->system, sizeof(out->system));
}

/* After an answer from the update service: the change has been told. */
void Sys_Cod2xMachineHistoryClear(void)
{
    Win_Cod2xRegDelete("HWID_DIFF");
    Win_Cod2xRegDelete("HWID_OLD");
}

/* Settings written without administrator rights went to the VirtualStore. Once the
   game runs elevated it uses the real key, so CoD2x moves them there. */
static void Win_Cod2xMigrateVirtualStore(void)
{
    static const char *const stores[2] = {COD2X_STORE_KEY, COD2X_STORE_KEY_WOW64};
    HKEY store = NULL, game;
    char name[256];
    BYTE data[1024];
    DWORD index, nameLength, dataLength, type, count = 0, longest = 0;
    int i;
    if (!Win_Cod2xElevated())
        return;
    for (i = 0; i < 2 && !store; ++i)
        if (RegOpenKeyExA(HKEY_CURRENT_USER, stores[i], 0, KEY_READ | KEY_WRITE, &store) != ERROR_SUCCESS)
            store = NULL;
    if (!store)
        return;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, COD2X_GAME_KEY, 0, NULL, 0, KEY_WRITE | KEY_WOW64_32KEY, NULL, &game,
                        NULL) != ERROR_SUCCESS) {
        RegCloseKey(store);
        return;
    }
    for (index = 0;; ++index) {
        nameLength = sizeof(name);
        dataLength = sizeof(data);
        if (RegEnumValueA(store, index, name, &nameLength, NULL, &type, data, &dataLength) != ERROR_SUCCESS)
            break;
        RegSetValueExA(game, name, 0, type, data, dataLength);
    }
    RegCloseKey(game);
    /* Remove the copied values from the last, then the keys. */
    if (RegQueryInfoKeyA(store, NULL, NULL, NULL, NULL, NULL, NULL, &count, &longest, NULL, NULL, NULL) ==
            ERROR_SUCCESS &&
        longest < sizeof(name)) {
        while (count--) {
            nameLength = sizeof(name);
            if (RegEnumValueA(store, count, name, &nameLength, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
                RegDeleteValueA(store, name);
        }
    }
    RegCloseKey(store);
    RegDeleteKeyA(HKEY_CURRENT_USER, stores[0]);
    RegDeleteKeyA(HKEY_CURRENT_USER, stores[1]);
}

/* CoD2x's ordering: numbers first, then a version without a pre-release part
   ("-test.1") above one with it, then the pre-release parts token by token. */
int Win_Cod2xCompareVersions(const char *a, const char *b)
{
    const char *ta, *tb;
    while (1) {
        int endA = *a == '-' || !*a, endB = *b == '-' || !*b;
        unsigned long long na = 0, nb = 0;
        char *next;
        if (endA && endB)
            break;
        if (!endA) {
            na = strtoull(a, &next, 10);
            if (next == a)
                break;
            a = *next == '.' ? next + 1 : next;
        }
        if (!endB) {
            nb = strtoull(b, &next, 10);
            if (next == b)
                break;
            b = *next == '.' ? next + 1 : next;
        }
        if (na != nb)
            return na < nb ? -1 : 1;
    }
    ta = *a == '-' ? a + 1 : NULL;
    tb = *b == '-' ? b + 1 : NULL;
    if (!ta || !tb)
        return !ta && !tb ? 0 : (!ta ? 1 : -1);
    while (1) {
        size_t la = strcspn(ta, "."), lb = strcspn(tb, ".");
        int digitsA = la && strspn(ta, "0123456789") >= la, digitsB = lb && strspn(tb, "0123456789") >= lb;
        int result = 0;
        size_t i;
        if (digitsA && digitsB) {
            unsigned long long va = strtoull(ta, NULL, 10), vb = strtoull(tb, NULL, 10);
            result = va < vb ? -1 : va > vb;
        } else if (digitsA != digitsB) {
            result = digitsA ? -1 : 1;
        } else {
            for (i = 0; i < la && i < lb && !result; ++i) {
                int ca = tolower((unsigned char)ta[i]), cb = tolower((unsigned char)tb[i]);
                if (ca != cb)
                    result = ca < cb ? -1 : 1;
            }
            if (!result && la != lb)
                result = la < lb ? -1 : 1;
        }
        if (result)
            return result;
        ta += la + (ta[la] == '.');
        tb += lb + (tb[lb] == '.');
        if (!*ta || !*tb)
            return !*ta && !*tb ? 0 : (!*ta ? -1 : 1);
    }
}

static void Win_Cod2xRegistryInit(void)
{
    HKEY key;
    char version[64] = {0};
    DWORD size = sizeof(version) - 1, type = 0;
    Win_Cod2xMigrateVirtualStore();
    if (!Win_Cod2xGameKey(&key)) {
        WIN_COD2X_ERROR("Failed to create registry key for Call of Duty 2.");
        exit(1);
    }
    RegCloseKey(key);
    if (Win_Cod2xRegRead("COD2X_VERSION", &type, version, &size)) {
        version[sizeof(version) - 1] = '\0';
        if (strcmp(version, COD2X_APP_VERSION)) {
            versionChanged = 1;
            if (version[0] && Win_Cod2xCompareVersions(version, COD2X_APP_VERSION) > 0) {
                char text[512];
                snprintf(text, sizeof(text),
                         "The CoD2x has been downgraded from a newer version.\n\nCurrent version: %s\n"
                         "Previous version: %s\n\nThis may cause issues with the game. Please ensure you are using "
                         "the correct version.\n\nIf you are having issues with the newer version, please contact "
                         "the developers.",
                         COD2X_APP_VERSION, version);
                MessageBoxA(NULL, text, "Version downgrade", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            }
        }
        snprintf(previousVersion, sizeof(previousVersion), "%s", version);
    } else {
        versionChanged = 1;
    }
}

int Win_Cod2xVersionChanged(const char **previous)
{
    *previous = previousVersion;
    return versionChanged;
}

/* The first frame records this version, after everything that compares against it. */
static void Win_Cod2xRegistryFrame(void)
{
    if (versionWritten)
        return;
    versionWritten = 1;
    if (!versionChanged)
        return;
    versionChanged = 0;
    if (!Win_Cod2xRegWrite("COD2X_VERSION", REG_SZ, COD2X_APP_VERSION, sizeof(COD2X_APP_VERSION))) {
        WIN_COD2X_ERROR("Failed to update COD2X_VERSION in registry.");
        exit(1);
    }
}

/* ---- cod2x:// links (CoD2x url_protocol.cpp) ------------------------------ */

static void Win_Cod2xRegisterLinks(void)
{
    const char *path = "Software\\Classes\\CoD2x";
    const char *description = "URL:Call of Duty 2 Protocol";
    char command[1024], current[MAX_PATH * 2];
    DWORD size = sizeof(current) - 1, type = 0;
    HKEY key, commandKey;
    snprintf(command, sizeof(command), "\"%s\" \"%%1\"", exePath);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Classes\\CoD2x\\shell\\open\\command", 0, KEY_READ,
                      &commandKey) == ERROR_SUCCESS) {
        memset(current, 0, sizeof(current));
        if (RegQueryValueExA(commandKey, NULL, NULL, &type, (BYTE *)current, &size) == ERROR_SUCCESS &&
            !_stricmp(current, command)) {
            RegCloseKey(commandKey);
            return;
        }
        RegCloseKey(commandKey);
    }
    if (RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL) !=
        ERROR_SUCCESS) {
        Win_Cod2xMessage("CoD2x Error", "Failed to register cod2x:// protocol in user registry.\n\n");
        return;
    }
    RegSetValueExA(key, NULL, 0, REG_SZ, (const BYTE *)description, (DWORD)strlen(description) + 1);
    RegSetValueExA(key, "URL Protocol", 0, REG_SZ, (const BYTE *)"", 1);
    if (RegCreateKeyExA(key, "shell\\open\\command", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                        &commandKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(commandKey, NULL, 0, REG_SZ, (const BYTE *)command, (DWORD)strlen(command) + 1);
        RegCloseKey(commandKey);
    }
    RegCloseKey(key);
}

int Cod2xNativeURL_Queue(const char *url)
{
    Cod2xURL parsed;
    char commands[sizeof(pendingURL)];
    if (!Cod2x_ParseURL(url, &parsed) || !Cod2x_URLCommands(&parsed, commands, sizeof(commands)))
        return 0;
    memcpy(pendingURL, commands, strlen(commands) + 1);
    return 1;
}

void Cod2xNativeURL_Install(void)
{
}

void Cod2xNativeURL_SetupPaths(void)
{
}

void Cod2xNativeURL_Frame(void)
{
    if (pendingURL[0]) {
        Cbuf_AddText(pendingURL);
        memset(pendingURL, 0, sizeof(pendingURL));
    }
}

void Cod2xNativeURL_Shutdown(void)
{
    memset(pendingURL, 0, sizeof(pendingURL));
}

/* A link opened from a browser arrives as the whole command line, in quotes. Unlike
   CoD2x, which runs whatever the link holds, only connect and password are taken. */
static char *Win_Cod2xLink(char *arguments)
{
    char url[1024];
    size_t length;
    const char *start = arguments;
    if (!arguments)
        return arguments;
    if (*start == '"')
        ++start;
    if (_strnicmp(start, "cod2x://", 8))
        return arguments;
    snprintf(url, sizeof(url), "%s", start);
    length = strlen(url);
    if (length && url[length - 1] == '"')
        url[length - 1] = '\0';
    if (!Cod2xNativeURL_Queue(url))
        fprintf(stderr, "CoD2x: rejected launch link; only connect and password are accepted.\n");
    return (char *)"";
}

char *Win_Cod2xStartup(char *arguments)
{
    if (!Win_Cod2xExecutable(arguments))
        ExitProcess(EXIT_FAILURE);
    Win_Cod2xWriteAccess();
    Win_Cod2xRegistryInit();
    arguments = Win_Cod2xLink(arguments);
    Win_Cod2xRegisterLinks();
    return arguments;
}

/* NumLock state for the console's numpad keys (CoD2x console.cpp). */
int Win_Cod2xNumLock(void)
{
    return (GetKeyState(VK_NUMLOCK) & 1) != 0;
}

/* ---- Diagnostic files (CoD2x exception.cpp) ------------------------------ */

static volatile LONG crashed;

void Win_Cod2xCrashed(void)
{
    InterlockedExchange(&crashed, 1);
}

int Win_Cod2xHasCrashed(void)
{
    return InterlockedCompareExchange(&crashed, 0, 0) != 0;
}

/* "<game folder>\<exe>.<kind>.YYYYMMDD_HHMMSS<suffix>", as CoD2x names them after CoD2MP_s.exe. */
void Win_Cod2xDiagnosticPath(char *path, size_t size, const char *kind, const SYSTEMTIME *time,
                                    const char *suffix)
{
    char directory[MAX_PATH] = {0};
    GetCurrentDirectoryA(sizeof(directory), directory);
    snprintf(path, size, "%s%s%s.%s.%04d%02d%02d_%02d%02d%02d%s", directory, directory[0] ? "\\" : "",
             Win_Cod2xExeName(), kind, time->wYear, time->wMonth, time->wDay, time->wHour, time->wMinute,
             time->wSecond, suffix);
}

static int Win_Cod2xNameOrder(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* CoD2x exception.cpp: only the newest diagnostic files of each kind are kept. */
static void Win_Cod2xKeepNewest(const char *kind, const char *extension, int keep)
{
    char pattern[MAX_PATH * 2], directory[MAX_PATH] = {0}, path[MAX_PATH * 2];
    char *names[512];
    WIN32_FIND_DATAA data;
    HANDLE find;
    int count = 0, i;
    GetCurrentDirectoryA(sizeof(directory), directory);
    snprintf(pattern, sizeof(pattern), "%s\\%s.%s.*.%s", directory, Win_Cod2xExeName(), kind, extension);
    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && count < 512)
            names[count++] = _strdup(data.cFileName);
    } while (FindNextFileA(find, &data));
    FindClose(find);
    /* The timestamp in the name sorts them oldest first. */
    qsort(names, (size_t)count, sizeof(names[0]), Win_Cod2xNameOrder);
    for (i = 0; i < count; ++i) {
        if (names[i] && i + keep < count) {
            snprintf(path, sizeof(path), "%s\\%s", directory, names[i]);
            DeleteFileA(path);
        }
        free(names[i]);
    }
}

static void Win_Cod2xDiagnosticCleanup(void)
{
    Win_Cod2xKeepNewest("crash", "dmp", 5);
    Win_Cod2xKeepNewest("freeze", "dmp", 5);
    Win_Cod2xKeepNewest("crash", "txt", 20);
    Win_Cod2xKeepNewest("freeze", "txt", 20);
}

/* ---- Module detection (CoD2x module_detection.cpp) ----------------------- */

/* Injected DLLs CoD2x looks for, compared without regard to case. Software known to
   break the game gets one warning; anything from the cheat list ends the game. */
static const struct {
    const char *module, *name, *advice;
} cod2xIncompatible[] = {
    {"RTSSHooks.dll", "RivaTuner Statistics Server (RTSS / MSI Afterburner)",
     "Please turn off RTSS and MSI Afterburner before launching the game."},
    {"RTSSVHooksLoader.dll", "RivaTuner Statistics Server (RTSS / MSI Afterburner)",
     "Please turn off RTSS and MSI Afterburner before launching the game."},
    {"xfire_toucan_46139.dll", "Xfire", "Please turn off Xfire before launching the game."},
};

static const char *const cod2xCheatModules[] = {
    "CoD2.dll", "CoD 2.dll", "CoD2 InfHook.dll", "CoD2Chams Glas.dll", "PhunKeh.dll", "CoD2 AimBot.dll",
    "CoD2Aimbot.dll", "CoD2_Aimbot.dll", "AimBot.dll", "TriggerBot.dll", "AutoShoot.dll", "AutoAim.dll",
    "iaimbot.dll", "CoD2 WallHack.dll", "CoD2WallHack.dll", "CoD2_WallHack.dll", "WallHack.dll", "wh.dll",
    "CoD2 ESP.dll", "CoD2ESP.dll", "esp.dll", "CoD2Chams.dll", "chams.dll", "CoD2 MultiHack.dll",
    "CoD2MultiHack.dll", "CoD2Multi.dll", "CoD2 Trainer.dll", "CoD2Trainer.dll", "CoD2Hack.dll",
    "CoD2 Hack.dll", "CoD2Cheat.dll", "omnihack.dll", "multihack.dll", "hack.dll", "hax.dll", "cheat.dll",
    "trainer.dll", "CoD2 SpeedHack.dll", "SpeedHack.dll", "BunnyHop.dll", "bhop.dll", "SpinBot.dll",
    "SuperJump.dll", "noclip.dll", "NoRecoil.dll", "NoSpread.dll", "nospread_cod2.dll", "RapidFire.dll",
    "CoD2Radar.dll", "CoD2 Radar.dll", "d3dhook.dll", "d3d_hook.dll", "d3d9hack.dll", "GodMode.dll",
    "InfiniteAmmo.dll", "CoD2 Aim.dll", "CoD2bot.dll", "CoD2D3D.dll", "cod2Hook.dll", "COD2HOOK1.3.dll",
    "Cod 2 multi hack.dll", "Cod 2 1.3 multi hack.dll", "cod2vision.dll", "cod2WH.dll", "cod2_espz.dll",
    "COD2_HOOK.dll", "BliksCoD2.dll", "BliksCoD2 2.0.dll", "Daves_CoD2_D3D_Hack.dll", "jCod2.dll",
    "oshicod2v2.dll", "oshicod2v3.dll", "oshicod2v5.dll", "Public cod2bot.dll", "qtcod2.dll",
    "Salt3r-Cod2RadarEnemies.dll", "SchoolHackCoD2.dll", "SyC CoD2BoT.dll", "UH-Cod2.dll", "Vossys_Cod2.dll",
    "500mg 1.0.0.dll", "BigTex.dll", "BlankSS.dll", "Buffalo.dll", "CodeDemonD3Dbase.dll", "Codehook.dll",
    "codhck.dll", "ColorModel.dll", "D3DReleasebyWieter20.dll", "D3D_cod-hacks.3dn.ru.dll", "DBEPut.dll",
    "decafCoffee.dll", "deceptivegames.dll", "ellu.dll", "EvilHookv1.dll", "Evolution.dll", "FisH4.dll",
    "GameHacks.dll", "h3he.dll", "h3heV2.dll", "iBase.dll", "Injection_Utility.dll", "lightHook.dll",
    "lolpenis.dll", "n7chams08.dll", "n7chams09.dll", "ObsidainHook.dll", "ObsidianHook.dll", "PBSS.dll",
    "Robbin237's Hack V4.dll", "selfHOOK.dll", "SiBot.dll", "SimpleNameTags.dll", "sirhookcod.dll",
    "syc cod.dll", "Symphony_hook.dll", "the hack.dll", "Thec0re.dll", "tobias.dll", "Uamb.dll",
    "UC-forum Release.dll", "Ucreleasebywieter20.dll", "utility_public.dll", "u_pbss.dll", "VtableD3DBase.dll",
    "xfire.dll", "Y-Hack v1.dll", "[EncoRe]CvarUnlocker.dll", "[undetected] ESP Boxes.dll",
};

static int moduleWarned;
static DWORD moduleRescanAt;

static void Win_Cod2xScanModules(void)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    MODULEENTRY32 entry;
    size_t i;
    if (snapshot == INVALID_HANDLE_VALUE)
        return;
    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Module32First(snapshot, &entry)) {
        do {
            for (i = 0; i < sizeof(cod2xCheatModules) / sizeof(cod2xCheatModules[0]); ++i) {
                if (!_stricmp(entry.szModule, cod2xCheatModules[i])) {
                    CloseHandle(snapshot);
                    MessageBoxA(NULL, "A cheat module was detected, the game will now exit.",
                                "Cheat software detected", MB_ICONERROR | MB_OK | MB_TOPMOST);
                    ExitProcess(EXIT_FAILURE);
                }
            }
            for (i = 0; i < sizeof(cod2xIncompatible) / sizeof(cod2xIncompatible[0]) && !moduleWarned; ++i) {
                if (!_stricmp(entry.szModule, cod2xIncompatible[i].module)) {
                    char text[1024];
                    moduleWarned = 1;
                    snprintf(text, sizeof(text),
                             "%s is loaded and may cause crashes or other issues with Call of Duty 2.\n\n%s",
                             cod2xIncompatible[i].name, cod2xIncompatible[i].advice);
                    MessageBoxA(NULL, text, "Incompatible software detected", MB_ICONWARNING | MB_OK | MB_TOPMOST);
                }
            }
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

/* Once when the game starts and once more five seconds later, for late injections. */
static void Win_Cod2xModulesInit(void)
{
    moduleRescanAt = GetTickCount() + 5000;
    if (!moduleRescanAt)
        moduleRescanAt = 1;
    Win_Cod2xScanModules();
}

static void Win_Cod2xModulesFrame(void)
{
    if (!moduleRescanAt || (LONG)(GetTickCount() - moduleRescanAt) < 0)
        return;
    moduleRescanAt = 0;
    Win_Cod2xScanModules();
}

/* ---- Freeze watchdog (CoD2x freeze.cpp) ---------------------------------- */

static const void *freezeWatch;
static HANDLE watchThread;
static volatile LONG heartbeat, stopping, enabled, userWait;

static int Win_Cod2xStalled(DWORD limit)
{
    return GetTickCount() - (DWORD)heartbeat > limit;
}

static DWORD WINAPI Win_Cod2xWatch(LPVOID argument)
{
    (void)argument;
    while (!stopping && !crashed) {
        Sleep(1000);
        if (!enabled || userWait || com_errorEntered || !Win_Cod2xStalled(12000))
            continue;
        /* The stacks of the main and render threads while the game is still frozen. */
        Win_Cod2xFreezeReport("Main thread did not finish a frame (freeze watchdog)",
                              GetTickCount() - (DWORD)heartbeat);
        if (MessageBoxA(NULL, "Call of Duty 2 has frozen.\n\nDo you want to create a diagnostic file and exit the "
                              "process?",
                        "Application frozen", MB_YESNO | MB_ICONERROR | MB_TOPMOST) == IDYES) {
            char name[MAX_PATH * 2], text[MAX_PATH * 2 + 128];
            if (Win_Cod2xDump(NULL, 0, 1, "freeze", name, sizeof(name))) {
                snprintf(text, sizeof(text),
                         "Diagnostic file was saved to:\n%s\n\nPlease send this file to the developers.", name);
                MessageBoxA(NULL, text, "Application frozen", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            } else {
                MessageBoxA(NULL, "Failed to create the diagnostic file.", "Application frozen",
                            MB_OK | MB_ICONERROR | MB_TOPMOST);
            }
            ExitProcess(1);
        }
        /* Ignored: wait until the game answers again. */
        while (Win_Cod2xStalled(5000) && !stopping && !crashed)
            Sleep(1000);
    }
    return 0;
}

void Cod2xNative_Heartbeat(void)
{
    InterlockedExchange(&heartbeat, (LONG)GetTickCount());
}

void Cod2xNative_BeginUserWait(void)
{
    InterlockedIncrement(&userWait);
    Cod2xNative_Heartbeat();
}

void Cod2xNative_EndUserWait(void)
{
    Cod2xNative_Heartbeat();
    InterlockedDecrement(&userWait);
}

void Cod2xNative_Init(void)
{
    if (watchThread)
        return;
    Win_Cod2xNameThread(GetCurrentThread(), L"CoD2 MainThread");
    Win_Cod2xDiagnosticCleanup();
    Win_Cod2xModulesInit();
    freezeWatch = Dvar_RegisterBool("com_freezeWatch", 1, 0x1000);
    InterlockedExchange(&enabled, Dvar_GetBool("com_freezeWatch") != 0);
    InterlockedExchange(&stopping, 0);
    Cod2xNative_Heartbeat();
    watchThread = CreateThread(NULL, 0, Win_Cod2xWatch, NULL, 0, NULL);
    if (!watchThread) {
        extern void Com_Error(int, const char *, ...);
        Com_Error(0, "Failed to create watchdog thread.");
    }
}

void Cod2xNative_Frame(void)
{
    Win_Cod2xFrameBegin();
    Cod2xNative_Heartbeat();
    if (freezeWatch)
        InterlockedExchange(&enabled, Dvar_GetBool("com_freezeWatch") != 0);
    Win_Cod2xRegistryFrame();
    Win_Cod2xModulesFrame();
    Cod2xNativeURL_Frame();
}

/* After Com_Frame: the crash report says whether a crash was inside it. */
void Cod2xNative_FrameDone(void)
{
    Win_Cod2xFrameEnd();
}

void Cod2xNative_Shutdown(void)
{
    if (watchThread) {
        InterlockedExchange(&stopping, 1);
        WaitForSingleObject(watchThread, INFINITE);
        CloseHandle(watchThread);
        watchThread = NULL;
    }
    Cod2xNativeURL_Shutdown();
}

/* ---- Crash dialog (CoD2x exception.cpp) ---------------------------------- */

enum {
    CRASH_EDIT = 2001,
    CRASH_RESTART,
    CRASH_EXIT,
    CRASH_DUMP,
    CRASH_DETAILS,
    CRASH_ICON,
    CRASH_HEADING,
    CRASH_SAVED,
    CRASH_SEND
};
#define CRASH_BACKGROUND RGB(30, 30, 30)
#define CRASH_LOG_BACKGROUND RGB(18, 18, 18)
#define CRASH_ICON_SIZE 48
/* Client sizes: the short form has no report box. */
#define CRASH_SHORT_WIDTH 860
#define CRASH_SHORT_HEIGHT 215
#define CRASH_LONG_WIDTH 1200
#define CRASH_LONG_HEIGHT 820
#define CRASH_MIN_WIDTH 760
#define CRASH_MIN_HEIGHT 195
/* Two lines for how the sending of the report goes. */
#define CRASH_SEND_HEIGHT 36

typedef struct {
    const char *text, *savedTo;
    EXCEPTION_POINTERS *exception;
    DWORD thread;
    int restart, details;
    int sizes[2][2]; /* the client size last used without and with the report */
    HFONT logFont, buttonFont, headingFont, linkFont;
    HBRUSH background, logBackground;
    HCURSOR hand;
    WNDPROC editProc;
} CrashDialog;

static void Win_Cod2xCrashLayout(HWND window, int width, int height)
{
    CrashDialog *dialog = (CrashDialog *)GetWindowLongPtrA(window, GWLP_USERDATA);
    const int margin = 14, buttonHeight = 42, gap = 10;
    const int textX = margin + CRASH_ICON_SIZE + 12, textWidth = width - textX - margin;
    const int savedY = margin + 26 + 6, linkY = savedY + 16 + 4, headerEnd = linkY + 16;
    const int sendY = headerEnd + 8;
    int buttonWidth = (width - margin * 2 - gap * 2) / 3, buttonX, buttonY, i;
    HWND edit = GetDlgItem(window, CRASH_EDIT);
    static const int buttons[3] = {CRASH_RESTART, CRASH_DUMP, CRASH_EXIT};
    if (buttonWidth > 220)
        buttonWidth = 220;
    if (buttonWidth < 150)
        buttonWidth = 150;
    buttonX = (width - (buttonWidth * 3 + gap * 2)) / 2;
    buttonY = sendY + CRASH_SEND_HEIGHT + 10;
    if (dialog && dialog->details) {
        int editY = sendY + CRASH_SEND_HEIGHT + gap, editHeight;
        buttonY = height - margin - buttonHeight;
        editHeight = buttonY - gap - editY;
        if (editHeight < 120) {
            editHeight = 120;
            buttonY = editY + editHeight + gap;
        }
        SetWindowPos(edit, NULL, margin, editY, width - margin * 2, editHeight, SWP_NOZORDER);
        ShowWindow(edit, SW_SHOW);
    } else {
        ShowWindow(edit, SW_HIDE);
    }
    SetWindowPos(GetDlgItem(window, CRASH_ICON), NULL, margin, margin + (headerEnd - margin - CRASH_ICON_SIZE) / 2,
                 CRASH_ICON_SIZE, CRASH_ICON_SIZE, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(window, CRASH_HEADING), NULL, textX, margin, textWidth, 26, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(window, CRASH_SAVED), NULL, textX, savedY, textWidth, 16, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(window, CRASH_DETAILS), NULL, textX, linkY, textWidth, 16, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(window, CRASH_SEND), NULL, textX, sendY, textWidth, CRASH_SEND_HEIGHT, SWP_NOZORDER);
    for (i = 0; i < 3; ++i)
        SetWindowPos(GetDlgItem(window, buttons[i]), NULL, buttonX + (buttonWidth + gap) * i, buttonY, buttonWidth,
                     buttonHeight, SWP_NOZORDER);
}

/* Resizes the window to a client size that fits the screen, around the same centre. */
static void Win_Cod2xCrashResize(HWND window, int width, int height)
{
    RECT frame = {0, 0, 0, 0}, old;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN), screenHeight = GetSystemMetrics(SM_CYSCREEN), x, y;
    if (width > screenWidth - 80)
        width = screenWidth - 80;
    if (height > screenHeight - 120)
        height = screenHeight - 120;
    frame.right = width;
    frame.bottom = height;
    AdjustWindowRectEx(&frame, (DWORD)GetWindowLongPtrA(window, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongPtrA(window, GWL_EXSTYLE));
    width = frame.right - frame.left;
    height = frame.bottom - frame.top;
    GetWindowRect(window, &old);
    x = (old.left + old.right) / 2 - width / 2;
    y = (old.top + old.bottom) / 2 - height / 2;
    if (x + width > screenWidth)
        x = screenWidth - width;
    if (y + height > screenHeight)
        y = screenHeight - height;
    SetWindowPos(window, NULL, x < 0 ? 0 : x, y < 0 ? 0 : y, width, height, SWP_NOZORDER);
}

/* CoD2x's send line (exception_updateSendLabel), redrawn every 250 ms. */
static void Win_Cod2xCrashSendLine(HWND window)
{
    char line[512];
    HWND send = GetDlgItem(window, CRASH_SEND);
    if (!send)
        return;
    Win_Cod2xSendStatus(line, sizeof(line));
    SetWindowTextA(send, line);
}

static LRESULT CALLBACK Win_Cod2xCrashEdit(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    CrashDialog *dialog = (CrashDialog *)GetWindowLongPtrA(GetParent(window), GWLP_USERDATA);
    if (message == WM_KEYDOWN && wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SendMessageA(window, EM_SETSEL, 0, -1);
        return 0;
    }
    return dialog && dialog->editProc ? CallWindowProcA(dialog->editProc, window, message, wParam, lParam)
                                      : DefWindowProcA(window, message, wParam, lParam);
}

static HWND Win_Cod2xCrashControl(HWND parent, const char *kind, const char *text, DWORD style, int id, HFONT font)
{
    HWND control = CreateWindowExA(0, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, parent,
                                   (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    if (control && font)
        SendMessageA(control, WM_SETFONT, (WPARAM)font, FALSE);
    return control;
}

static LRESULT CALLBACK Win_Cod2xCrashWindow(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    CrashDialog *dialog = (CrashDialog *)GetWindowLongPtrA(window, GWLP_USERDATA);
    switch (message) {
    case WM_CREATE: {
        char saved[MAX_PATH * 2 + 40];
        HWND icon, edit;
        RECT client;
        dialog = (CrashDialog *)((CREATESTRUCTA *)lParam)->lpCreateParams;
        SetWindowLongPtrA(window, GWLP_USERDATA, (LONG_PTR)dialog);
        dialog->background = CreateSolidBrush(CRASH_BACKGROUND);
        dialog->logBackground = CreateSolidBrush(CRASH_LOG_BACKGROUND);
        MessageBeep(MB_ICONERROR);
        dialog->logFont = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        dialog->buttonFont = CreateFontA(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                         DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        dialog->headingFont = CreateFontA(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        dialog->linkFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE, DEFAULT_CHARSET,
                                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                       DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        dialog->hand = LoadCursor(NULL, IDC_HAND);
        icon = Win_Cod2xCrashControl(window, "STATIC", "", SS_ICON | SS_REALSIZEIMAGE, CRASH_ICON, NULL);
        SendMessageA(icon, STM_SETICON,
                     (WPARAM)LoadImageA(NULL, (LPCSTR)IDI_ERROR, IMAGE_ICON, CRASH_ICON_SIZE, CRASH_ICON_SIZE,
                                        LR_SHARED),
                     0);
        Win_Cod2xCrashControl(window, "STATIC", "Call of Duty 2 has crashed.", SS_LEFT, CRASH_HEADING,
                              dialog->headingFont);
        snprintf(saved, sizeof(saved), "The crash report was saved to: %s",
                 dialog->savedTo && dialog->savedTo[0] ? dialog->savedTo : "(unknown)");
        Win_Cod2xCrashControl(window, "STATIC", saved, SS_LEFT | SS_PATHELLIPSIS, CRASH_SAVED, dialog->buttonFont);
        /* The report is hidden until asked for. */
        Win_Cod2xCrashControl(window, "STATIC", "Show crash details", SS_LEFT | SS_NOTIFY, CRASH_DETAILS,
                              dialog->linkFont);
        /* How the sending goes; empty when the fault is not CoD2x's and nothing is sent. */
        Win_Cod2xCrashControl(window, "STATIC", "", SS_LEFT | SS_EDITCONTROL, CRASH_SEND, dialog->buttonFont);
        Win_Cod2xCrashSendLine(window);
        SetTimer(window, 1, 250, NULL);
        edit = Win_Cod2xCrashControl(window, "EDIT", "", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                     CRASH_EDIT, dialog->logFont);
        dialog->editProc = (WNDPROC)SetWindowLongPtrA(edit, GWLP_WNDPROC, (LONG_PTR)Win_Cod2xCrashEdit);
        SetWindowTextA(edit, dialog->text);
        /* The newest log lines are at the end. */
        SendMessageA(edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessageA(edit, EM_SCROLLCARET, 0, 0);
        Win_Cod2xCrashControl(window, "BUTTON", "Restart Game", BS_OWNERDRAW, CRASH_RESTART, NULL);
        Win_Cod2xCrashControl(window, "BUTTON", "Generate Full Dump", BS_OWNERDRAW, CRASH_DUMP, NULL);
        Win_Cod2xCrashControl(window, "BUTTON", "Exit", BS_OWNERDRAW, CRASH_EXIT, NULL);
        GetClientRect(window, &client);
        Win_Cod2xCrashLayout(window, client.right, client.bottom);
        return 0;
    }
    case WM_ERASEBKGND:
        if (dialog) {
            RECT client;
            GetClientRect(window, &client);
            FillRect((HDC)wParam, &client, dialog->background);
            return 1;
        }
        break;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        if (dialog) {
            HDC dc = (HDC)wParam;
            int id = GetDlgCtrlID((HWND)lParam);
            /* A read-only edit control asks with WM_CTLCOLORSTATIC too. */
            if (id == CRASH_EDIT) {
                SetBkColor(dc, CRASH_LOG_BACKGROUND);
                SetTextColor(dc, RGB(212, 212, 212));
                return (LRESULT)dialog->logBackground;
            }
            SetBkColor(dc, CRASH_BACKGROUND);
            if (id == CRASH_SEND) {
                int state = Win_Cod2xSendState();
                SetTextColor(dc, state == 2   ? RGB(90, 210, 90)
                                 : state == 3 ? RGB(255, 90, 70)
                                 : state == 1 ? RGB(220, 190, 70)
                                              : RGB(160, 140, 140));
                return (LRESULT)dialog->background;
            }
            SetTextColor(dc, id == CRASH_HEADING   ? RGB(255, 255, 255)
                             : id == CRASH_DETAILS ? RGB(120, 180, 255)
                                                   : RGB(170, 170, 170));
            return (LRESULT)dialog->background;
        }
        break;
    case WM_DRAWITEM:
        if (dialog) {
            DRAWITEMSTRUCT *item = (DRAWITEMSTRUCT *)lParam;
            int pressed = (item->itemState & ODS_SELECTED) != 0;
            COLORREF color = item->CtlID == CRASH_RESTART ? (pressed ? RGB(0, 90, 160) : RGB(0, 120, 212))
                             : item->CtlID == CRASH_DUMP  ? (pressed ? RGB(45, 45, 45) : RGB(68, 68, 68))
                                                          : (pressed ? RGB(140, 20, 20) : RGB(180, 40, 40));
            HBRUSH brush = CreateSolidBrush(color);
            char label[64] = {0};
            HFONT previous;
            FillRect(item->hDC, &item->rcItem, brush);
            DeleteObject(brush);
            GetWindowTextA(item->hwndItem, label, sizeof(label));
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, RGB(255, 255, 255));
            previous = (HFONT)SelectObject(item->hDC, dialog->buttonFont);
            DrawTextA(item->hDC, label, -1, &item->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(item->hDC, previous);
            if (item->itemState & ODS_FOCUS) {
                RECT focus = item->rcItem;
                InflateRect(&focus, -3, -3);
                DrawFocusRect(item->hDC, &focus);
            }
            return TRUE;
        }
        break;
    case WM_SIZE:
        if (dialog)
            Win_Cod2xCrashLayout(window, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_GETMINMAXINFO: {
        RECT minimum = {0, 0, CRASH_MIN_WIDTH, CRASH_MIN_HEIGHT};
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtrA(window, GWL_STYLE), FALSE,
                           (DWORD)GetWindowLongPtrA(window, GWL_EXSTYLE));
        ((MINMAXINFO *)lParam)->ptMinTrackSize.x = minimum.right - minimum.left;
        ((MINMAXINFO *)lParam)->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return 0;
    }
    case WM_SETCURSOR:
        if (dialog && LOWORD(lParam) == HTCLIENT && GetDlgCtrlID((HWND)wParam) == CRASH_DETAILS) {
            SetCursor(dialog->hand);
            return TRUE;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == CRASH_DETAILS && HIWORD(wParam) == STN_CLICKED && dialog) {
            RECT client;
            int *size;
            GetClientRect(window, &client);
            /* Each form keeps the size it was last given. */
            dialog->sizes[dialog->details][0] = client.right;
            dialog->sizes[dialog->details][1] = client.bottom;
            dialog->details = !dialog->details;
            SetWindowTextA(GetDlgItem(window, CRASH_DETAILS),
                           dialog->details ? "Hide crash details" : "Show crash details");
            size = dialog->sizes[dialog->details];
            Win_Cod2xCrashResize(window, size[0], size[1]);
            GetClientRect(window, &client);
            Win_Cod2xCrashLayout(window, client.right, client.bottom);
            InvalidateRect(window, NULL, TRUE);
        } else if (LOWORD(wParam) == CRASH_RESTART || LOWORD(wParam) == CRASH_EXIT) {
            if (dialog)
                dialog->restart = LOWORD(wParam) == CRASH_RESTART;
            PostQuitMessage(0);
        } else if (LOWORD(wParam) == CRASH_DUMP && dialog) {
            char name[MAX_PATH * 2], text[MAX_PATH * 2 + 400];
            HCURSOR cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            int saved = Win_Cod2xDump(dialog->exception, dialog->thread, 1, "crash", name, sizeof(name));
            SetCursor(cursor);
            if (saved) {
                snprintf(text, sizeof(text),
                         "Full diagnostic file was saved to:\n%s\n"
                         "\n"
                         "Please send this file to the developers.\n"
                         "You can reach them on the Discord - more info at https://cod2x.me/.\n"
                         "\n"
                         "Without this file we would not have a chance to fix this problem. Thank you!",
                         name);
                MessageBoxA(window, text, "Crash Dump Saved", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            } else {
                MessageBoxA(window, "Failed to create crash dump file.", "Crash Dump Error",
                            MB_OK | MB_ICONERROR | MB_TOPMOST);
            }
        }
        return 0;
    case WM_TIMER:
        if (wParam == 1 && dialog)
            Win_Cod2xCrashSendLine(window);
        return 0;
    case WM_CLOSE:
        if (dialog)
            dialog->restart = 0;
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        if (dialog) {
            DeleteObject(dialog->logFont);
            DeleteObject(dialog->buttonFont);
            DeleteObject(dialog->headingFont);
            DeleteObject(dialog->linkFont);
            DeleteObject(dialog->background);
            DeleteObject(dialog->logBackground);
        }
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

int Win_Cod2xCrashDialog(const char *reportPath, EXCEPTION_POINTERS *exception)
{
    static char raw[200 * 1024], text[400 * 1024];
    CrashDialog dialog;
    WNDCLASSEXA windowClass;
    HWND window;
    MSG message;
    FILE *file = fopen(reportPath, "rb");
    size_t length = 0, i, out = 0;
    int width = GetSystemMetrics(SM_CXSCREEN), height = GetSystemMetrics(SM_CYSCREEN);
    if (file) {
        length = fread(raw, 1, sizeof(raw) - 1, file);
        fclose(file);
    }
    /* The edit control needs CRLF line ends. */
    for (i = 0; i < length && out < sizeof(text) - 3; ++i) {
        if (raw[i] == '\n' && (i == 0 || raw[i - 1] != '\r'))
            text[out++] = '\r';
        text[out++] = raw[i];
    }
    text[out] = '\0';
    memset(&dialog, 0, sizeof(dialog));
    dialog.text = text;
    dialog.savedTo = reportPath;
    dialog.exception = exception;
    dialog.thread = GetCurrentThreadId();
    memset(&windowClass, 0, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = Win_Cod2xCrashWindow;
    windowClass.hInstance = GetModuleHandleA(NULL);
    windowClass.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    windowClass.hIcon = LoadIconA(NULL, (LPCSTR)IDI_ERROR);
    windowClass.lpszClassName = "CoD2x_CrashDialog";
    RegisterClassExA(&windowClass);
    dialog.sizes[0][0] = CRASH_SHORT_WIDTH < width - 80 ? CRASH_SHORT_WIDTH : width - 80;
    dialog.sizes[0][1] = CRASH_SHORT_HEIGHT < height - 120 ? CRASH_SHORT_HEIGHT : height - 120;
    dialog.sizes[1][0] = CRASH_LONG_WIDTH < width - 80 ? CRASH_LONG_WIDTH : width - 80;
    dialog.sizes[1][1] = CRASH_LONG_HEIGHT < height - 120 ? CRASH_LONG_HEIGHT : height - 120;
    {
        RECT frame = {0, 0, 0, 0};
        frame.right = dialog.sizes[0][0];
        frame.bottom = dialog.sizes[0][1];
        AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_TOPMOST);
        width = frame.right - frame.left;
        height = frame.bottom - frame.top;
    }
    window = CreateWindowExA(WS_EX_TOPMOST, "CoD2x_CrashDialog", "Call of Duty 2 - Application Crash",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE, (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
                             (GetSystemMetrics(SM_CYSCREEN) - height) / 2, width, height, NULL, NULL,
                             windowClass.hInstance, &dialog);
    if (!window)
        return 0;
    SetForegroundWindow(window);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    DestroyWindow(window);
    UnregisterClassA("CoD2x_CrashDialog", windowClass.hInstance);
    Win_Cod2xSendWait();
    if (dialog.restart) {
        char line[MAX_PATH * 2 + 1024];
        STARTUPINFOA start;
        PROCESS_INFORMATION process;
        snprintf(line, sizeof(line), "\"%s\" %s", exePath, commandLine);
        memset(&start, 0, sizeof(start));
        start.cb = sizeof(start);
        if (CreateProcessA(NULL, line, NULL, NULL, FALSE, 0, NULL, exeDirectory[0] ? exeDirectory : NULL, &start,
                           &process)) {
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
        }
    }
    return 1;
}
#endif
