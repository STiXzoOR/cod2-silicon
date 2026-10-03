#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static clientConnection_t connection;
static clientActive_t active;
#define CLUI_STATE (&connection)
#define CL_LOCAL (&active)
static refexport_t exports;
#define RE (&exports)
static dvar_t showCommands;
static const dvar_t *cl_showServerCommands = &showCommands;
static char bigConfigString[8192], tokens[8192], modified[8192], reason[1024];
static char *argv[3];
static int argc, modifiedIndex, cleared, expectedError;
static jmp_buf errorJump;
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t code, const char *fmt, ...)
{
    assert(expectedError && code == ERR_SERVERDISCONNECT);
    snprintf(reason, sizeof(reason), "%s", fmt);
    longjmp(errorJump, 1);
}
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(dest, size, fmt, args);
    va_end(args);
    return n;
}
void Cmd_TokenizeString2(const char *text, int flags)
{
    (void)flags;
    strcpy(tokens, text);
    argc = 0;
    char *p = tokens;
    while (*p && argc < 3) {
        while (*p == ' ') ++p;
        argv[argc++] = p;
        if (argc == 3) break;
        while (*p && *p != ' ') ++p;
        if (*p) *p++ = 0;
    }
}
void Cmd_TokenizeString(const char *s) { Cmd_TokenizeString2(s, 3); }
char *Cmd_Argv(int n) { return n < argc ? argv[n] : ""; }
int Cmd_Argc(void) { return argc; }
const char *SEH_SafeTranslateString(const char *s) { return s; }
const char *UI_ReplaceConversionString(const char *s, const char *r) { (void)s; return r; }
void CL_ConfigstringModified(void)
{
    modifiedIndex = atoi(Cmd_Argv(1));
    strcpy(modified, Cmd_Argv(2));
}
void Con_ClearNotify(void) { ++cleared; }
void Con_ClearSubtitles(void) { }
static void clearFlares(void) { }
#include "server_commands_source.h"
static qboolean command(const char *text)
{
    int n = ++connection.serverCommandSequence;
    strcpy(connection.serverCommands[n & 127], text);
    return CL_GetServerCommand(n);
}
int main(void)
{
    exports.ClearFlares = clearFlares;
    assert(command("d 13 player identity") && modifiedIndex == 13);
    assert(!strcmp(modified, "player identity"));
    assert(!command("x 100 start "));
    assert(!command("y 100 middle "));
    assert(command("z 100 end") && modifiedIndex == 100);
    assert(!strcmp(modified, "start middle end"));
    memset(active.cmds, 0xff, sizeof(active.cmds));
    assert(command("B") && command("n") && cleared == 2);
    assert(active.cmds[0].buttons == 0);
    expectedError = 1;
    if (!setjmp(errorJump)) { command("w server-reason"); abort(); }
    assert(!strcmp(reason, "server-reason"));
    puts("online: stock reliable command opcodes, multipart configstrings and disconnect passed");
    return 0;
}
