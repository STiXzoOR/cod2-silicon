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
static char selected[64], execOS[256];
static int disconnected, resets;
void FS_BuildOSPath(const char *, const char *, const char *, char *);
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void I_strncpyz(char *out, const char *s, int n) { snprintf(out, n, "%s", s); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
int Com_sprintf(char *out, int n, const char *fmt, ...)
{
    va_list args; va_start(args, fmt); int result = vsnprintf(out, n, fmt, args); va_end(args); return result;
}
void Com_ExecStartupConfigs(const char *path)
{
    snprintf(exec, sizeof(exec), "%s", path ? path : "");
    if (path) FS_BuildOSPath(home.current.string, fs_gamedir, path, execOS);
}
void Com_CheckSetRecommended(void) {}
void Cbuf_ExecuteText(int when, const char *text) { CHECK(when == 0 && !strcmp(text, "disconnect")); ++disconnected; }
void Cbuf_AddText(const char *text) { (void)text; }
void Dvar_ResetDvars(unsigned int flags, DvarSetSource source) { CHECK(flags == 0xffffffff && source == DVAR_SOURCE_EXTERNAL); ++resets; }
Bool Dvar_AnyLatchedValues(void) { return 0; }
void Dvar_SetString(const dvar_t *var, const char *s)
{
    CHECK(var == &profile);
    snprintf(selected, sizeof(selected), "%s", s);
    ((dvar_t *)var)->current.string = selected;
}
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
Bool Com_NewPlayerProfile(const char *);
#include "profiles_source.h"
static void ProfileLifecycle(const char *game, const char *newDir, const char *deletedDir,
    const char *configOS, const char *activeOS)
{
    char path[256];
    snprintf(fs_gamedir, sizeof(fs_gamedir), "%s", game);
    Dvar_SetString(&profile, "One");
    disconnected = resets = 0;
    CHECK(Com_NewPlayerProfile("New"));
    CHECK(!strcmp(created, newDir));
    CHECK(Com_DeletePlayerProfile("Two"));
    CHECK(!strcmp(removed, deletedDir));
    Com_ChangePlayerProfile("Two");
    CHECK(!strcmp(active, "Two") && !strcmp(exec, "players/Two/config_mp.cfg") && disconnected == 1 && resets == 1);
    CHECK(!strcmp(profile.current.string, "Two"));
    CHECK(!strcmp(execOS, configOS));
    FS_BuildOSPath(home.current.string, NULL, "players/active.txt", path);
    CHECK(!strcmp(path, activeOS));
    /* The second token names a different profile and must not be selected. */
    Dvar_SetString(&profile, "One");
    Com_InitPlayerProfiles();
    CHECK(!strcmp(profile.current.string, "Two") && !strcmp(exec, "players/Two/config_mp.cfg"));
    CHECK(!strcmp(execOS, configOS));
}
int main(void)
{
    char path[256];
    base.current.string = "/licensed"; home.current.string = "/writable";
    ProfileLifecycle("main", "/writable/main/players/New/", "/writable/main/players/Two/",
        "/writable/main/players/Two/config_mp.cfg", "/writable/main/players/active.txt");
#if defined(COD2_CODX) && COD2_CODX
    ProfileLifecycle("pam", "/writable/pam/players/New/", "/writable/pam/players/Two/",
        "/writable/main/players/Two/config_mp.cfg", "/writable/pam/players/active.txt");
#define PROFILE_GAME "main"
#define MOVIE_GAME "main"
#else
    ProfileLifecycle("pam", "/writable/pam/players/New/", "/writable/pam/players/Two/",
        "/writable/pam/players/Two/config_mp.cfg", "/writable/pam/players/active.txt");
#define PROFILE_GAME "pam"
#define MOVIE_GAME "movie"
#endif
    /* fs_gamedir is the resolved fs_game; NULL/empty game uses that mod. */
    const struct { const char *game, *qpath, *expected; } cases[] = {
        {"pam", "players/One/config_mp.cfg", "/writable/" PROFILE_GAME "/players/One/config_mp.cfg"},
        {"pam", "players/Two/config_mp.cfg", "/writable/" PROFILE_GAME "/players/Two/config_mp.cfg"},
        {"mods/pam", "players/Two/config_mp.cfg",
#if defined(COD2_CODX) && COD2_CODX
            "/writable/main/players/Two/config_mp.cfg"},
#else
            "/writable/mods/pam/players/Two/config_mp.cfg"},
#endif
        {NULL, "players/Two/config_mp.cfg", "/writable/" PROFILE_GAME "/players/Two/config_mp.cfg"},
        {"", "players/Two/config_mp.cfg", "/writable/" PROFILE_GAME "/players/Two/config_mp.cfg"},
        {"main", "config_mp.cfg", "/writable/main/config_mp.cfg"},
        {"pam", "config_mp.cfg", "/writable/pam/config_mp.cfg"},
        {NULL, "config_mp.cfg", "/writable/pam/config_mp.cfg"},
        {"", "config_mp.cfg", "/writable/pam/config_mp.cfg"},
        {"pam", "players/Two/settings.cfg", "/writable/pam/players/Two/settings.cfg"},
        {"movie", "config_mp.cfg", "/writable/" MOVIE_GAME "/config_mp.cfg"},
        {"movie", "movie.iwd", "/writable/movie/movie.iwd"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        FS_BuildOSPath(home.current.string, cases[i].game, cases[i].qpath, path);
        if (strcmp(path, cases[i].expected))
            fprintf(stderr, "qpath=%s game=%s: expected %s, got %s\n", cases[i].qpath,
                cases[i].game ? cases[i].game : "(null)", cases[i].expected, path);
        CHECK(!strcmp(path, cases[i].expected));
    }
    FINISH("profiles");
}
