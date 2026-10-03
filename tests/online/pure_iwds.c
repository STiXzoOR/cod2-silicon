#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int count, sums[1024], fakeChecksum;
static char *names[1024];
void *imp_fs_numServerIwds = &count;
void *imp_fs_serverIwds = sums;
void *imp_fs_serverIwdNames = names;
void *imp_fs_fakeChkSum = &fakeChecksum;
static char tokens[8192];
static char *argv[1024];
static int argc;
void Cmd_TokenizeString(const char *text)
{
    char *word;
    strcpy(tokens, text);
    argc = 0;
    for (word = strtok(tokens, " "); word; word = strtok(NULL, " "))
        argv[argc++] = word;
}
int Cmd_Argc(void) { return argc; }
char *Cmd_Argv(int n) { return argv[n]; }
char *CopyStringInternal(const char *text) { return strdup(text); }
void Z_FreeInternal(void *p) { free(p); }
void Com_Memcpy(void *dest, const void *src, int size) { memcpy(dest, src, size); }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void SND_StopSounds(snd_stopsounds_arg_t flags) { (void)flags; }
void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
void FS_ShutdownServerIwdNames(void)
{
    for (int i = 0; i < count; ++i) {
        free(names[i]);
        names[i] = NULL;
    }
    count = 0;
}
#include "pure_iwds_source.h"
int main(void)
{
    FS_PureServerSetLoadedIwds("10 -20 30", "main/iw_00 main/iw_01 main/iw_CoD2x_01");
    assert(count == 3 && sums[0] == 10 && sums[1] == -20 && sums[2] == 30);
    assert(names[2] && !strcmp(names[2], "main/iw_CoD2x_01"));
    assert(!strcmp(names[0], "main/iw_00") && !strcmp(names[1], "main/iw_01"));
    FS_PureServerSetLoadedIwds("30 10 -20", "main/iw_CoD2x_01 main/iw_00 main/iw_01");
    FS_PureServerSetLoadedIwds("40", "mod/map");
    assert(count == 1 && sums[0] == 40 && !strcmp(names[0], "mod/map"));
    FS_PureServerSetLoadedIwds("", "");
    assert(count == 0 && !names[0]);
    puts("online: pure IWD pointer copy, replacement and cleanup passed");
    return 0;
}
