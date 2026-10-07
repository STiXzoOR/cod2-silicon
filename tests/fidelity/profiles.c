#include "common_types.h"
#include "cod2x_policy.h"
#include "check.h"
#include <stdarg.h>
#include <stdlib.h>
static dvar_t profile, chosen, base, home;
const dvar_t *com_playerProfile = &profile, *ui_playerProfileAlreadyChosen = &chosen;
const dvar_t *fs_basepath = &base, *fs_homepath = &home;
char fs_gamedir[256] = "main";
void *imp_fs_gamedir = fs_gamedir;
static char created[256], removed[256], active[64], exec[64];
static int disconnected, resets;
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void I_strncpyz(char *out, const char *s, int n) { snprintf(out, n, "%s", s); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
int Com_sprintf(char *out, int n, const char *fmt, ...)
{
    va_list args; va_start(args, fmt); int result = vsnprintf(out, n, fmt, args); va_end(args); return result;
}
void Com_ExecStartupConfigs(const char *path) { snprintf(exec, sizeof(exec), "%s", path ? path : ""); }
void Com_CheckSetRecommended(void) {}
void Cbuf_ExecuteText(int when, const char *text) { CHECK(when == 0 && !strcmp(text, "disconnect")); ++disconnected; }
void Cbuf_AddText(const char *text) { (void)text; }
void Dvar_ResetDvars(unsigned int flags, DvarSetSource source) { CHECK(flags == 0xffffffff && source == DVAR_SOURCE_EXTERNAL); ++resets; }
Bool Dvar_AnyLatchedValues(void) { return 0; }
void Dvar_SetString(const dvar_t *var, const char *s) { ((dvar_t *)var)->current.string = s; }
void Dvar_SetInt(const dvar_t *var, int value) { ((dvar_t *)var)->current.integer = value; }
const dvar_t *Dvar_RegisterInt(const char *n, int v, int min, int max, unsigned int flags) { (void)n; (void)v; (void)min; (void)max; (void)flags; return &chosen; }
const dvar_t *Dvar_RegisterString(const char *n, const char *v, unsigned int flags) { (void)n; (void)v; (void)flags; return &profile; }
void Dvar_ChangeResetValue(const dvar_t *var, DvarValue value) { ((dvar_t *)var)->reset = value; }
const char **FS_ListFiles(const char *path, const char *extension, int behavior, int *count, int alloc)
{
    static const char *names[] = {"One", "Two", 0}; (void)path; (void)extension; (void)behavior; (void)alloc; *count = 2; return names;
}
void FS_FreeFileList(const char **list, int alloc) { (void)list; (void)alloc; }
int FS_ReadFile(const char *path, void **out) { CHECK(!strcmp(path, "players/active.txt")); *out = strdup("Two\nignored"); return 11; }
void FS_FreeFile(void *out) { free(out); }
const char *Com_Parse(const char **cursor) { static char token[64]; sscanf(*cursor, "%63s", token); return token; }
qboolean FS_WriteFile(const char *path, const void *data, int n) { CHECK(!strcmp(path, "players/active.txt")); memcpy(active, data, n); active[n] = 0; return 1; }
qboolean FS_CreatePath(char *path) { snprintf(created, sizeof(created), "%s", path); return 0; }
qboolean Sys_RemoveDirTree(const char *path) { snprintf(removed, sizeof(removed), "%s", path); return 1; }
void FS_BuildOSPath(const char *, const char *, const char *, char *);
Bool Com_NewPlayerProfile(const char *);
#include "profiles_source.h"
void FS_BuildOSPath(const char *b, const char *g, const char *q, char *out) { FS_BuildOSPath_Internal(b, g, q, out, 0); }
int main(void)
{
    base.current.string = "/licensed"; home.current.string = "/writable"; profile.current.string = "One";
    CHECK(Com_NewPlayerProfile("New"));
    CHECK(!strcmp(created, "/writable/main/players/New/"));
    CHECK(Com_DeletePlayerProfile("Two"));
    CHECK(!strcmp(removed, "/writable/main/players/Two/"));
    Com_ChangePlayerProfile("Two");
    CHECK(!strcmp(active, "Two") && !strcmp(exec, "players/Two/config_mp.cfg") && disconnected == 1 && resets == 1);
    Com_InitPlayerProfiles();
    CHECK(!strcmp(profile.current.string, "Two") && !strcmp(exec, "players/Two/config_mp.cfg"));
    char path[256];
    FS_BuildOSPath("/writable", "pam", "players/Two/config_mp.cfg", path);
#if defined(COD2_CODX) && COD2_CODX
    /* Keep the released shared config mapping until profile migration ships. */
    CHECK(!strcmp(path, "/writable/main/config_mp.cfg"));
    FS_BuildOSPath("/writable", "pam", "config_mp.cfg", path);
    CHECK(!strcmp(path, "/writable/main/config_mp.cfg"));
#else
    CHECK(!strcmp(path, "/writable/pam/players/Two/config_mp.cfg"));
#endif
    FINISH("profiles");
}
