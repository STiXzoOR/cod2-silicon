#if !defined(_WIN32)

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "common_types.h"
#include "imports.h"
#if defined(__APPLE__) && defined(COD2_X64) && !defined(DEDICATED)
#include "platform/macos_console.h"
#include "platform/macos_text.h"
#include <stdlib.h>
#define NATIVE_SYSTEM_CONSOLE 1
#endif

extern Boolean gConsoleRunning;
static char sConsoleText[512];
static char sReturnedText[512];
static char sStdinText[512];
static int sStdinLength;

extern void Sys_Print(const char *msg);
extern int MessageBoxA(void *hWnd, const char *lpText, const char *lpCaption, unsigned int uType);

#if NATIVE_SYSTEM_CONSOLE
extern const dvar_t *loc_language;
extern dvar_t *com_viewlog;
extern dvar_t *com_dedicated;
extern void Dvar_SetInt(const dvar_t *dvar, int value);
extern void Sys_Error(const char *error, ...);
extern void Cbuf_AddText(const char *text);
static qboolean sQuitOnClose;

static void Sys_ConsoleSubmit(const char *utf8)
{
    char *text = MacText_EncodeUTF8(utf8, loc_language ? loc_language->current.integer : 0);
    if (!text) {
        free(text);
        return;
    }
    /* Windows 0x468630 appends pending commands (including empty lines), then
     * echoes the input and clears the field. Its 512-byte buffer reserves five
     * bytes before each append. Keep that limit without its negative-size edge. */
    size_t used = strlen(sConsoleText);
    size_t length = strlen(text);
    size_t available = used < sizeof(sConsoleText) - 5 ? sizeof(sConsoleText) - 5 - used : 0;
    if (length > available)
        length = available;
    memcpy(sConsoleText + used, text, length);
    used += length;
    if (used < sizeof(sConsoleText) - 1)
        sConsoleText[used++] = '\n';
    sConsoleText[used] = '\0';
    /* GetWindowText's original local input buffer is 1024 bytes. */
    if (strlen(text) > 1023)
        text[1023] = '\0';
    Sys_Print("]");
    Sys_Print(text);
    Sys_Print("\n");
    free(text);
}

static void Sys_ConsoleVisibility(int level)
{
    gConsoleRunning = level == 1;
    /* WM_ACTIVATE keeps viewlog in sync with manual minimize/restore. */
    if (com_dedicated && !com_dedicated->current.integer && com_viewlog &&
        ((level == 2 && com_viewlog->current.integer == 1) ||
         (level == 1 && com_viewlog->current.integer == 2)))
        Dvar_SetInt(com_viewlog, level);
}

static void Sys_ConsoleClose(void)
{
    /* Windows WM_CLOSE hides a client log, but requests normal engine shutdown
     * for the dedicated/error console. Do not terminate inside an AppKit event. */
    if (sQuitOnClose || (com_dedicated && com_dedicated->current.integer))
        Cbuf_AddText("quit\n");
    else {
        MacConsole_Show(0);
        gConsoleRunning = 0;
        if (com_viewlog)
            Dvar_SetInt(com_viewlog, 0);
    }
}
#endif

void Sys_CreateConsole(HINSTANCE hInstance)
{
    sConsoleText[0] = '\0';
    sReturnedText[0] = '\0';
    sStdinLength = 0;
#if NATIVE_SYSTEM_CONSOLE
    sQuitOnClose = 0;
    MacConsole_Create(Sys_ConsoleSubmit, Sys_ConsoleClose, Sys_ConsoleVisibility);
#endif
}

void Sys_DestroyConsole(void)
{
#if NATIVE_SYSTEM_CONSOLE
    MacConsole_Destroy();
#endif
    gConsoleRunning = 0;
}

void Sys_ShowConsole(int visLevel, qboolean quitOnClose)
{
    gConsoleRunning = 0;

#if NATIVE_SYSTEM_CONSOLE
    sQuitOnClose = quitOnClose;
    if (visLevel < 0 || visLevel > 2) {
        Sys_Error("Invalid visLevel %d sent to Sys_ShowConsole\n", visLevel);
        return;
    }
    MacConsole_Show(visLevel);
    return;
#endif

    switch (visLevel) {
    case 0:

        break;
    case 1:

        gConsoleRunning = 1;
        break;
    case 2:

        break;
    }
}

char *Sys_ConsoleInput(void)
{
    fd_set fds;
    struct timeval tv;
    ssize_t ret;
    char c;

#if NATIVE_SYSTEM_CONSOLE
    if (sConsoleText[0]) {
        strcpy(sReturnedText, sConsoleText);
        sConsoleText[0] = '\0';
        return sReturnedText;
    }
#endif

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    while (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        ret = read(STDIN_FILENO, &c, 1);
        if (ret <= 0)
            break;
        if (c == '\n') {
            sStdinText[sStdinLength] = '\0';
            sStdinLength = 0;
            strcpy(sReturnedText, sStdinText);
            return sReturnedText;
        }
        if (sStdinLength < (int)sizeof(sStdinText) - 1)
            sStdinText[sStdinLength++] = c;

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    }

    return NULL;
}

void Conbuf_AppendText(const char *pMsg)
{
    if (!pMsg)
        return;

#if NATIVE_SYSTEM_CONSOLE
    MacConsole_AppendText(pMsg, loc_language ? loc_language->current.integer : 0);
#endif

    fputs(pMsg, stdout);
    fflush(stdout);
}

void Sys_SetErrorText(const char *buf)
{
#if NATIVE_SYSTEM_CONSOLE
    /* Sys_Error already presents the original message in its native modal. */
    MacConsole_SetErrorText();
#endif
    fprintf(stderr, "ERROR: %s\n", buf);
}

#endif // !_WIN32
