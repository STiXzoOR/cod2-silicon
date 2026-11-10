#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_server.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* zPAM versions on a CoD2x server. main/ may hold several zpam<N>.iwd and
   zpam_maps_v<N>.iwd files (CoD2x's updater adds new ones beside the old, and
   several servers can share the folder); each server loads one of each, the
   newest that main/zpam.json does not block. It moves to a newer one on start,
   map change or full map_restart, and an empty server restarts its map after a
   minute to get there. Without zpam.json every iwd loads, as on stock servers.
   The updater that downloads new versions and writes zpam.json is not here. */

extern void Com_Printf(const char *fmt, ...);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *dvarName);
extern int Dvar_GetInt(const char *dvarName);
extern Bool Dvar_GetBool(const char *dvarName);
extern const char *Dvar_GetString(const char *dvarName);
extern void Cbuf_AddText(const char *text);
extern int Sys_Milliseconds(void);
extern int I_stricmp(const char *s1, const char *s2);
extern int I_strnicmp(const char *s1, const char *s2, size_t count);
extern const dvar_t *com_dedicated;
extern const dvar_t *com_sv_running;
extern const dvar_t *sv_maxclients;
extern serverStatic_t svs;

#define ZPAM_STATE_FILE "zpam.json"
#define ZPAM_IDLE_MSEC 60000
#define ZPAM_NAME_SIZE 256
#define ZPAM_MAX_BLOCKED 64

static const char zpamPrefix[] = "zpam";
static const char zpamMapsPrefix[] = "zpam_maps_v";

typedef struct {
    char zpam[ZPAM_NAME_SIZE];
    char blocked[ZPAM_MAX_BLOCKED][ZPAM_NAME_SIZE];
    int blockedCount;
} ZpamState;

static const dvar_t *sv_updateZpam;
/* What this server loads of each kind; empty until the first listing. */
static char zpamLoaded[ZPAM_NAME_SIZE];
static char zpamLoadedMaps[ZPAM_NAME_SIZE];
/* A newer version is waiting for the next map load. */
static int zpamSwitchPending;
static unsigned int zpamIdleSince;

void SV_Cod2xZpamInit(void)
{
    sv_updateZpam = Dvar_RegisterBool("sv_updateZpam", 1, 0x1000);
}

/* The version of "zpam409.iwd" for "zpam", or -1 when the name is not prefix, digits, ".iwd". */
static int ZPAM_Version(const char *name, const char *prefix)
{
    size_t length = strlen(prefix);
    const char *p;
    int version;
    if (I_strnicmp(name, prefix, length))
        return -1;
    p = name + length;
    if (!isdigit((unsigned char)*p))
        return -1;
    version = atoi(p);
    while (isdigit((unsigned char)*p))
        ++p;
    return I_stricmp(p, ".iwd") ? -1 : version;
}

static int ZPAM_IsBlocked(const char *name, const ZpamState *state)
{
    for (int i = 0; state && i < state->blockedCount; ++i) {
        if (!I_stricmp(name, state->blocked[i]))
            return 1;
    }
    return 0;
}

/* The highest version among names that is not blocked; the first one wins a tie. */
static const char *ZPAM_Newest(const char *prefix, char **names, int count, const ZpamState *state)
{
    const char *newest = NULL;
    int newestVersion = -1;
    for (int i = 0; i < count; ++i) {
        int version = ZPAM_Version(names[i], prefix);
        if (version > newestVersion && !ZPAM_IsBlocked(names[i], state)) {
            newest = names[i];
            newestVersion = version;
        }
    }
    return newest;
}

static int ZPAM_Listed(const char *name, char **names, int count)
{
    for (int i = 0; i < count; ++i) {
        if (!I_stricmp(name, names[i]))
            return 1;
    }
    return 0;
}

/* A JSON string at p into out; the text after it, or NULL when it is not one. */
static const char *ZPAM_JsonString(const char *p, char *out, size_t size)
{
    size_t used = 0;
    if (*p != '"')
        return NULL;
    for (++p; *p && *p != '"'; ++p) {
        char c = *p;
        if (c == '\\') {
            ++p;
            switch (*p) {
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'u': {
                unsigned int code = 0;
                for (int i = 1; i <= 4; ++i) {
                    if (!isxdigit((unsigned char)p[i]))
                        return NULL;
                    code = code * 16 + (unsigned int)(isdigit((unsigned char)p[i]) ? p[i] - '0'
                                                                                   : (tolower((unsigned char)p[i]) - 'a' + 10));
                }
                p += 4;
                c = code < 0x80 ? (char)code : '?';
                break;
            }
            case '\0': return NULL;
            default: c = *p; break;
            }
        }
        if (out && used + 1 < size)
            out[used++] = c;
    }
    if (*p != '"')
        return NULL;
    if (out && size)
        out[used] = '\0';
    return p + 1;
}

static const char *ZPAM_JsonSpace(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    return p;
}

/* Past one JSON value of any kind, at the comma or bracket after it; NULL when it is broken. */
static const char *ZPAM_JsonSkip(const char *p)
{
    int depth = 0;
    while (*p) {
        if (*p == '"') {
            p = ZPAM_JsonString(p, NULL, 0);
            if (!p)
                return NULL;
            continue;
        }
        if (*p == '{' || *p == '[') {
            ++depth;
        } else if (*p == '}' || *p == ']') {
            if (!depth)
                return p;
            --depth;
        } else if (*p == ',' && !depth) {
            return p;
        }
        ++p;
    }
    return NULL;
}

/* zpam.json of the folder; it counts only when it names a zPAM version. */
static int ZPAM_ReadState(const char *folder, ZpamState *state)
{
    char path[1024], text[8192], key[64];
    const char *p;
    size_t length;
    FILE *file;

    memset(state, 0, sizeof(*state));
    snprintf(path, sizeof(path), "%s/%s", folder, ZPAM_STATE_FILE);
    file = fopen(path, "rb");
    if (!file)
        return 0;
    length = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[length] = '\0';

    p = ZPAM_JsonSpace(text);
    if (*p++ != '{')
        return 0;
    for (;;) {
        p = ZPAM_JsonSpace(p);
        if (*p == '}' || !*p)
            break;
        p = ZPAM_JsonString(p, key, sizeof(key));
        if (!p)
            break;
        p = ZPAM_JsonSpace(p);
        if (*p++ != ':')
            break;
        p = ZPAM_JsonSpace(p);
        if (!strcmp(key, "zpam") && *p == '"') {
            p = ZPAM_JsonString(p, state->zpam, sizeof(state->zpam));
        } else if (!strcmp(key, "block") && *p == '[') {
            state->blockedCount = 0;
            for (++p;;) {
                p = ZPAM_JsonSpace(p);
                if (*p == ']') {
                    ++p;
                    break;
                }
                if (*p == '"' && state->blockedCount < ZPAM_MAX_BLOCKED)
                    p = ZPAM_JsonString(p, state->blocked[state->blockedCount++], ZPAM_NAME_SIZE);
                else
                    p = ZPAM_JsonSkip(p);
                if (!p)
                    break;
                p = ZPAM_JsonSpace(p);
                if (*p == ',')
                    ++p;
                else if (*p != ']')
                    p = NULL;
                if (!p)
                    break;
            }
        } else {
            p = ZPAM_JsonSkip(p);
        }
        if (!p)
            break;
        p = ZPAM_JsonSpace(p);
        if (*p != ',')
            break;
        ++p;
    }
    return state->zpam[0] != '\0';
}

static int ZPAM_Enabled(void)
{
    if (sv_updateZpam)
        return sv_updateZpam->current.enabled;
    /* Before SV_Init only the command line can have set it. */
    return !Dvar_FindVar("sv_updateZpam") || Dvar_GetInt("sv_updateZpam") != 0;
}

/* Moving to a new version needs players to be able to get it from a web server,
   and the zPAM in main/ to be the one in use (no fs_game). A CoD2x match would
   also hold it; there is no match service here. */
static int ZPAM_CanSwitch(void)
{
    return ZPAM_Enabled() && Dvar_GetBool("sv_wwwDownload") && Dvar_GetString("sv_wwwBaseURL")[0] &&
           !Dvar_GetString("fs_game")[0];
}

static void ZPAM_Select(const char *prefix, char *loaded, int canSwitch, char **names, int count,
                        const ZpamState *state, char *hidden)
{
    const char *newest = ZPAM_Newest(prefix, names, count, state);
    const char *selected = newest;
    if (!newest)
        return;
    if (loaded[0] && !canSwitch && ZPAM_Listed(loaded, names, count))
        selected = loaded;
    if (I_stricmp(selected, newest))
        zpamSwitchPending = 1;
    if (I_stricmp(selected, loaded)) {
        Com_Printf("zPAM: Loading %s\n", selected);
        snprintf(loaded, ZPAM_NAME_SIZE, "%s", selected);
    }
    for (int i = 0; i < count; ++i) {
        if (ZPAM_Version(names[i], prefix) >= 0 && I_stricmp(names[i], loaded))
            hidden[i] = 1;
    }
}

static void ZPAM_Remember(const char *prefix, char *loaded, char **names, int count)
{
    const char *newest = ZPAM_Newest(prefix, names, count, NULL);
    if (newest)
        snprintf(loaded, ZPAM_NAME_SIZE, "%s", newest);
}

int SV_Cod2xZpamFilterIwds(const char *folder, char **files, int count)
{
    char mainFolder[1024];
    char *hidden;
    ZpamState state;
    size_t length = strlen(folder);
    int kept = 0;

    if (!ZPAM_Enabled() || count <= 0)
        return count;
    /* Only main/, by name. */
    if (length < 5 || I_stricmp(folder + length - 4, "main") ||
        (folder[length - 5] != '/' && folder[length - 5] != '\\'))
        return count;

    if (!ZPAM_ReadState(folder, &state)) {
        /* The home main/ of a server whose base main/ holds the file must not
           change the choice. */
        snprintf(mainFolder, sizeof(mainFolder), "%s/main", Dvar_GetString("fs_basepath"));
        if (Dvar_GetString("fs_basepath")[0] && ZPAM_ReadState(mainFolder, &state))
            return count;
        /* Everything loads; the newest is what runs, so a first install of the
           file later does not look like a switch. */
        ZPAM_Remember(zpamPrefix, zpamLoaded, files, count);
        ZPAM_Remember(zpamMapsPrefix, zpamLoadedMaps, files, count);
        return count;
    }

    hidden = calloc((size_t)count, 1);
    if (!hidden)
        return count;
    zpamSwitchPending = 0;
    {
        int canSwitch = ZPAM_CanSwitch();
        ZPAM_Select(zpamPrefix, zpamLoaded, canSwitch, files, count, &state, hidden);
        ZPAM_Select(zpamMapsPrefix, zpamLoadedMaps, canSwitch, files, count, &state, hidden);
    }

    /* The engine owns the strings: the ones not loaded go to the end of the list. */
    for (int i = 0; i < count; ++i) {
        char *swap;
        if (hidden[i]) {
            Com_Printf("zPAM: Ignoring %s\n", files[i]);
            continue;
        }
        swap = files[kept];
        files[kept] = files[i];
        files[i] = swap;
        ++kept;
    }
    free(hidden);
    return kept;
}

static int ZPAM_ServerEmpty(void)
{
    for (int i = 0; i < sv_maxclients->current.integer; ++i) {
        if (svs.clients[i].state != 0) /* free slot */
            return 0;
    }
    return 1;
}

void SV_Cod2xZpamFrame(void)
{
    unsigned int now;
    if (!com_dedicated || !com_dedicated->current.integer)
        return;
    if (!zpamSwitchPending || !com_sv_running || !com_sv_running->current.enabled || !ZPAM_ServerEmpty() ||
        !ZPAM_CanSwitch()) {
        zpamIdleSince = 0;
        return;
    }
    /* Nothing changes the map of an empty server, so it restarts its own. */
    now = (unsigned int)Sys_Milliseconds();
    if (!zpamIdleSince) {
        zpamIdleSince = now;
    } else if (now - zpamIdleSince >= ZPAM_IDLE_MSEC) {
        zpamIdleSince = now;
        Com_Printf("zPAM: Server is empty, restarting the map to load the new version\n");
        Cbuf_AddText("map_restart\n");
    }
}
#endif
