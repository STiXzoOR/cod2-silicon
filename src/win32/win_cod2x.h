#ifndef WIN_COD2X_H
#define WIN_COD2X_H
/* The CoD2x Windows client pieces in win_cod2x.c. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED) && defined(_WIN32)
#include <windows.h>
/* Start-up checks, registry, cod2x:// link; returns the command line to use. */
char *Win_Cod2xStartup(char *arguments);
int Win_Cod2xElevated(void);
int Win_Cod2xCompareVersions(const char *a, const char *b);
/* The game's registry key as the 32-bit game sees it. */
int Win_Cod2xRegRead(const char *name, DWORD *type, void *data, DWORD *size);
int Win_Cod2xRegWrite(const char *name, DWORD type, const void *data, DWORD size);
int Win_Cod2xRegDelete(const char *name);
int Win_Cod2xVersionChanged(const char **previous);
/* CoD2x's error box for an unexpected failure, then exit(1). */
void Win_Cod2xFatal(const char *message);
/* CoD2x's dialog for a crash report; it may start the game again. */
int Win_Cod2xCrashDialog(const char *reportPath, EXCEPTION_POINTERS *exception);
/* The executable, its folder and its arguments; files named after it. */
const char *Win_Cod2xExeName(void);
const char *Win_Cod2xExePath(void);
const char *Win_Cod2xExeDirectory(void);
const char *Win_Cod2xCommandLine(void);
void Win_Cod2xDiagnosticPath(char *path, size_t size, const char *kind, const SYSTEMTIME *time, const char *suffix);
void Win_Cod2xCrashed(void);
int Win_Cod2xHasCrashed(void);
/* What CoD2x's system.cpp gathers about Windows. */
typedef struct {
    char build[64], edition[64], display[32], country[8], virtualization[128], compatibility[64];
    int elevated;
} Win_Cod2xSystemText;
void Win_Cod2xSystem(Win_Cod2xSystemText *out);
/* win_cod2x_report.c: CoD2x's crash and freeze reports. */
void Win_Cod2xCrashReport(EXCEPTION_POINTERS *exception, const char *reason, int owned, char *path, size_t size);
void Win_Cod2xFreezeReport(const char *reason, DWORD frozenMs);
int Win_Cod2xDump(EXCEPTION_POINTERS *exception, DWORD thread, int full, const char *kind, char *path, size_t size);
int Win_Cod2xSendStatus(char *text, size_t size);
int Win_Cod2xSendState(void);
void Win_Cod2xSendWait(void);
void Win_Cod2xNameThread(void *thread, const wchar_t *name);
void Win_Cod2xFrameBegin(void);
void Win_Cod2xFrameEnd(void);
#endif
#endif
