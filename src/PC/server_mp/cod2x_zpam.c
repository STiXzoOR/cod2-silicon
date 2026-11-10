#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_server.h"
#include "../qcommon/cod2x_http.h"
#include "../qcommon/cod2x_json.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* zPAM versions on a CoD2x server. main/ may hold several zpam<N>.iwd and
   zpam_maps_v<N>.iwd files (CoD2x's updater adds new ones beside the old, and
   several servers can share the folder); each server loads one of each, the
   newest that main/zpam.json does not block. It moves to a newer one on start,
   map change or full map_restart, and an empty server restarts its map after a
   minute to get there. Without zpam.json every iwd loads, as on stock servers.
   The update check (cod2x_updater.c) hands over CoD2x's manifest: the blocked
   versions are written at once, new versions are downloaded in the background
   and checked against the manifest's size and SHA-256, zpam.json is written,
   and versions nobody needs any more are removed after a week. */

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
#define ZPAM_MAX_BLOCKED COD2X_ZPAM_MAX_BLOCKED
/* Old versions stay this long for other servers sharing the folder. */
#define ZPAM_CLEANUP_SECONDS (7 * 24 * 60 * 60)
#ifdef _WIN32
#define ZPAM_SEPARATOR '\\'
#else
#define ZPAM_SEPARATOR '/'
#endif

static const char zpamPrefix[] = "zpam";
static const char zpamMapsPrefix[] = "zpam_maps_v";

typedef struct {
    char zpam[ZPAM_NAME_SIZE];
    char maps[ZPAM_NAME_SIZE];
    char blocked[ZPAM_MAX_BLOCKED][ZPAM_NAME_SIZE];
    int blockedCount;
    long long updated;
} ZpamState;

/* The background install: 0 idle, 1 running, 2 done (the result below is ready). */
static int zpamWorker;
static char zpamWorkerError[512];
static char zpamWorkerInstalled[512];

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

/* The highest version below below among names that is not blocked; the first one wins a tie. */
static const char *ZPAM_NewestBelow(const char *prefix, char **names, int count, const ZpamState *state, int below)
{
    const char *newest = NULL;
    int newestVersion = -1;
    for (int i = 0; i < count; ++i) {
        int version = ZPAM_Version(names[i], prefix);
        if (version > newestVersion && version < below && !ZPAM_IsBlocked(names[i], state)) {
            newest = names[i];
            newestVersion = version;
        }
    }
    return newest;
}

static const char *ZPAM_Newest(const char *prefix, char **names, int count, const ZpamState *state)
{
    return ZPAM_NewestBelow(prefix, names, count, state, INT_MAX);
}

static int ZPAM_IsIwdName(const char *name)
{
    return ZPAM_Version(name, zpamPrefix) >= 0 || ZPAM_Version(name, zpamMapsPrefix) >= 0;
}

static int ZPAM_Listed(const char *name, char **names, int count)
{
    for (int i = 0; i < count; ++i) {
        if (!I_stricmp(name, names[i]))
            return 1;
    }
    return 0;
}

static void ZPAM_Path(char *out, size_t size, const char *folder, const char *name)
{
    snprintf(out, size, "%s%c%s", folder, ZPAM_SEPARATOR, name);
}

/* zpam.json of the folder; it counts only when it names a zPAM version. */
static int ZPAM_ReadState(const char *folder, ZpamState *state)
{
    char path[1024], text[8192], key[32];
    double updated = 0;
    size_t length;
    FILE *file;

    memset(state, 0, sizeof(*state));
    ZPAM_Path(path, sizeof(path), folder, ZPAM_STATE_FILE);
    file = fopen(path, "rb");
    if (!file)
        return 0;
    length = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[length] = '\0';
    Cod2x_JsonString(text, "$.zpam", state->zpam, sizeof(state->zpam));
    Cod2x_JsonString(text, "$.maps", state->maps, sizeof(state->maps));
    if (Cod2x_JsonNumber(text, "$.updated", &updated))
        state->updated = (long long)updated;
    while (state->blockedCount < ZPAM_MAX_BLOCKED) {
        snprintf(key, sizeof(key), "$.block[%d]", state->blockedCount);
        if (!Cod2x_JsonString(text, key, state->blocked[state->blockedCount], ZPAM_NAME_SIZE))
            break;
        ++state->blockedCount;
    }
    return state->zpam[0] != '\0';
}

/* Written beside and moved over the old one, so other servers read either. */
static int ZPAM_WriteState(const char *folder, const ZpamState *state)
{
    char path[1024], temporary[1100];
    Cod2xText text = {0};
    FILE *file;
    int written;

    Cod2x_TextAppendString(&text, "{\"zpam\":\"");
    Cod2x_TextAppendJson(&text, state->zpam, strlen(state->zpam));
    Cod2x_TextAppendString(&text, "\",\"maps\":\"");
    Cod2x_TextAppendJson(&text, state->maps, strlen(state->maps));
    Cod2x_TextAppendString(&text, "\",\"block\":[");
    for (int i = 0; i < state->blockedCount; ++i) {
        Cod2x_TextAppendString(&text, i ? ",\"" : "\"");
        Cod2x_TextAppendJson(&text, state->blocked[i], strlen(state->blocked[i]));
        Cod2x_TextAppendString(&text, "\"");
    }
    Cod2x_TextPrintf(&text, "],\"updated\":%lld}\n", state->updated);
    if (text.failed) {
        Cod2x_TextFree(&text);
        return 0;
    }
    ZPAM_Path(path, sizeof(path), folder, ZPAM_STATE_FILE);
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    file = fopen(temporary, "wb");
    if (!file) {
        Cod2x_TextFree(&text);
        return 0;
    }
    written = fwrite(text.data, 1, text.length, file) == text.length;
    if (fclose(file) != 0)
        written = 0;
    Cod2x_TextFree(&text);
    /* A cut file must not replace a good one: without it every version loads. */
    if (!written || !Cod2x_ReplaceFile(temporary, path)) {
        remove(temporary);
        return 0;
    }
    return 1;
}

/* The names in the folder; free with ZPAM_FreeList. */
static int ZPAM_List(const char *folder, char ***names)
{
    DIR *dir = opendir(folder);
    struct dirent *entry;
    int count = 0, capacity = 0;
    *names = NULL;
    if (!dir)
        return 0;
    while ((entry = readdir(dir)) != NULL) {
        char *copy;
        if (count == capacity) {
            char **grown = realloc(*names, sizeof(char *) * (size_t)(capacity ? capacity * 2 : 64));
            if (!grown)
                break;
            *names = grown;
            capacity = capacity ? capacity * 2 : 64;
        }
        copy = malloc(strlen(entry->d_name) + 1);
        if (!copy)
            break;
        strcpy(copy, entry->d_name);
        (*names)[count++] = copy;
    }
    closedir(dir);
    return count;
}

static void ZPAM_FreeList(char **names, int count)
{
    for (int i = 0; i < count; ++i)
        free(names[i]);
    free(names);
}

static int ZPAM_Enabled(void)
{
    if (sv_updateZpam)
        return sv_updateZpam->current.enabled;
    /* Before SV_Init only the command line can have set it. */
    return !Dvar_FindVar("sv_updateZpam") || Dvar_GetInt("sv_updateZpam") != 0;
}

/* Players get a new version only from a web server; the game server is too slow. */
static int ZPAM_WwwDownload(void)
{
    return Dvar_GetBool("sv_wwwDownload") && Dvar_GetString("sv_wwwBaseURL")[0];
}

/* A server running its own mod does not use the zPAM in main/. */
static int ZPAM_FsGameSet(void)
{
    return Dvar_GetString("fs_game")[0] != '\0';
}

/* Moving to a new version needs all of these. */
static int ZPAM_CanAutoUpdate(void)
{
    return ZPAM_Enabled() && ZPAM_WwwDownload() && !ZPAM_FsGameSet();
}

/* A match holds the version it started with, also while it downloads and loads. */
static int ZPAM_MatchRunning(void)
{
    return SV_Cod2xMatchRunning();
}

static int ZPAM_CanSwitch(void)
{
    return !ZPAM_MatchRunning() && ZPAM_CanAutoUpdate();
}

static void ZPAM_MainFolder(char *out, size_t size)
{
    const char *base = Dvar_GetString("fs_basepath");
    if (!base[0])
        out[0] = '\0';
    else
        snprintf(out, size, "%s%cmain", base, ZPAM_SEPARATOR);
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
        ZPAM_MainFolder(mainFolder, sizeof(mainFolder));
        if (mainFolder[0] && ZPAM_ReadState(mainFolder, &state))
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
    if (ZPAM_MatchRunning())
        return 0;
    for (int i = 0; i < sv_maxclients->current.integer; ++i) {
        if (svs.clients[i].state != 0) /* free slot */
            return 0;
    }
    return 1;
}

/* ---- updates from CoD2x's manifest ---------------------------------------- */

void SV_Cod2xZpamInstalled(char *out, size_t size)
{
    char folder[1024], **names;
    const char *newest;
    int count;
    if (zpamLoaded[0]) {
        snprintf(out, size, "%s", zpamLoaded);
        return;
    }
    ZPAM_MainFolder(folder, sizeof(folder));
    count = ZPAM_List(folder, &names);
    newest = ZPAM_Newest(zpamPrefix, names, count, NULL);
    snprintf(out, size, "%s", newest ? newest : "");
    ZPAM_FreeList(names, count);
}

int SV_Cod2xZpamUpdateCheckNeeded(void)
{
    char installed[ZPAM_NAME_SIZE];
    if (!ZPAM_Enabled() || ZPAM_FsGameSet())
        return 0;
    SV_Cod2xZpamInstalled(installed, sizeof(installed));
    return installed[0] != '\0';
}

/* Whether this server loads the newest versions not blocked; otherwise it switches when it can. */
static void ZPAM_UpdateSwitchPending(const char *folder)
{
    ZpamState state;
    char **names;
    const char *newest, *newestMaps;
    int count;
    if (!ZPAM_ReadState(folder, &state))
        return;
    count = ZPAM_List(folder, &names);
    newest = ZPAM_Newest(zpamPrefix, names, count, &state);
    newestMaps = ZPAM_Newest(zpamMapsPrefix, names, count, &state);
    /* With every version blocked nothing is hidden and there is nothing to switch to. */
    zpamSwitchPending = (newest && I_stricmp(newest, zpamLoaded)) || (newestMaps && I_stricmp(newestMaps, zpamLoadedMaps));
    ZPAM_FreeList(names, count);
}

static int ZPAM_CompareNames(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* The blocked list goes into zpam.json at once, before a CoD2x downgrade from the
   same answer could matter. Returns 0 when it could not be written. */
static int ZPAM_SaveBlock(const char *folder, const Cod2xZpamManifest *manifest, int *changed)
{
    char sorted[COD2X_ZPAM_MAX_BLOCKED][ZPAM_NAME_SIZE], current[COD2X_ZPAM_MAX_BLOCKED][ZPAM_NAME_SIZE];
    ZpamState state;
    intptr_t lock;
    int same;

    *changed = 0;
    if (!ZPAM_ReadState(folder, &state))
        return 1; /* not managed yet: the first install writes the list */
    memcpy(sorted, manifest->blocked, sizeof(sorted[0]) * (size_t)manifest->blockedCount);
    qsort(sorted, (size_t)manifest->blockedCount, sizeof(sorted[0]), ZPAM_CompareNames);
    memcpy(current, state.blocked, sizeof(current[0]) * (size_t)state.blockedCount);
    qsort(current, (size_t)state.blockedCount, sizeof(current[0]), ZPAM_CompareNames);
    same = manifest->blockedCount == state.blockedCount;
    for (int i = 0; same && i < state.blockedCount; ++i)
        same = !strcmp(sorted[i], current[i]);
    if (same)
        return 1;

    lock = Cod2x_LockFolder(folder, "zpam");
    if (lock == -1)
        return 0; /* an install holds it; tried again with the next check */
    ZPAM_ReadState(folder, &state);
    memcpy(state.blocked, sorted, sizeof(sorted[0]) * (size_t)manifest->blockedCount);
    state.blockedCount = manifest->blockedCount;
    state.updated = (long long)time(NULL);
    *changed = ZPAM_WriteState(folder, &state);
    Cod2x_UnlockFolder(lock);

    if (*changed) {
        Cod2xText list = {0};
        for (int i = 0; i < state.blockedCount; ++i) {
            if (i)
                Cod2x_TextAppendString(&list, ", ");
            Cod2x_TextAppendString(&list, state.blocked[i]);
        }
        Com_Printf("zPAM: Blocked versions: %s\n", list.length ? list.data : "none");
        Cod2x_TextFree(&list);
    }
    return *changed;
}

typedef struct {
    char folder[1024];
    char loaded[ZPAM_NAME_SIZE], loadedMaps[ZPAM_NAME_SIZE];
    Cod2xZpamManifest manifest;
} ZpamJob;

/* Worker: the files main/ lacks, next to their names with ".part". */
static int ZPAM_DownloadMissing(const ZpamJob *job, char *fetched, char *error, size_t size)
{
    for (int i = 0; i < job->manifest.fileCount; ++i) {
        const Cod2xZpamFile *file = &job->manifest.files[i];
        Cod2xHttpProgress progress = {0};
        char path[1100], part[1100], reason[256], hash[65];
        struct stat info;
        ZPAM_Path(path, sizeof(path), job->folder, file->name);
        if (stat(path, &info) == 0 && (size_t)info.st_size == file->size)
            continue;
        snprintf(part, sizeof(part), "%s.part", path);
        if (!Cod2x_HttpDownload(file->url, part, "PK", &progress, reason, sizeof(reason)))
            snprintf(error, size, "%s: %s", file->name, reason);
        else if (progress.downloaded != file->size)
            snprintf(error, size, "%s: received %lu of %lu bytes", file->name, (unsigned long)progress.downloaded,
                     (unsigned long)file->size);
        else if (!Cod2x_Sha256File(part, hash) || strcmp(hash, file->sha256))
            snprintf(error, size, "%s: hash mismatch", file->name);
        if (error[0]) {
            remove(part);
            return 0;
        }
        fetched[i] = 1;
    }
    return 1;
}

/* Worker: into main/, the map pack first since a new zPAM may need its maps. */
static int ZPAM_MoveDownloaded(const ZpamJob *job, const char *fetched, char *installed, size_t installedSize,
                               char *error, size_t size)
{
    const char *prefixes[2] = {zpamMapsPrefix, zpamPrefix};
    for (int p = 0; p < 2; ++p) {
        for (int i = 0; i < job->manifest.fileCount; ++i) {
            const Cod2xZpamFile *file = &job->manifest.files[i];
            char path[1100], part[1100];
            size_t used;
            if (!fetched[i] || ZPAM_Version(file->name, prefixes[p]) < 0)
                continue;
            ZPAM_Path(path, sizeof(path), job->folder, file->name);
            snprintf(part, sizeof(part), "%s.part", path);
            if (!Cod2x_ReplaceFile(part, path)) {
                snprintf(error, size, "Failed to move %s into main", file->name);
                return 0;
            }
            used = strlen(installed);
            snprintf(installed + used, installedSize - used, "%s%s", used ? ", " : "", file->name);
        }
    }
    return 1;
}

/* Worker: zpam.json for the manifest. The file (and its time) stays when nothing
   changed, and the blocked list is taken from the manifest only for a new file:
   a slow install of an older answer must not undo a newer list. */
static int ZPAM_SaveState(const ZpamJob *job, ZpamState *state, char *error, size_t size)
{
    ZpamState current;
    int hasCurrent;
    memset(state, 0, sizeof(*state));
    for (int i = 0; i < job->manifest.fileCount; ++i) {
        if (ZPAM_Version(job->manifest.files[i].name, zpamPrefix) >= 0)
            snprintf(state->zpam, sizeof(state->zpam), "%s", job->manifest.files[i].name);
        if (ZPAM_Version(job->manifest.files[i].name, zpamMapsPrefix) >= 0)
            snprintf(state->maps, sizeof(state->maps), "%s", job->manifest.files[i].name);
    }
    hasCurrent = ZPAM_ReadState(job->folder, &current);
    if (hasCurrent) {
        memcpy(state->blocked, current.blocked, sizeof(current.blocked));
        state->blockedCount = current.blockedCount;
    } else {
        memcpy(state->blocked, job->manifest.blocked, sizeof(state->blocked));
        state->blockedCount = job->manifest.blockedCount;
    }
    if (hasCurrent && !strcmp(current.zpam, state->zpam) && !strcmp(current.maps, state->maps)) {
        state->updated = current.updated;
        return 1;
    }
    state->updated = (long long)time(NULL);
    if (!ZPAM_WriteState(job->folder, state)) {
        snprintf(error, size, "Failed to write " ZPAM_STATE_FILE);
        return 0;
    }
    return 1;
}

/* Worker: removes versions no server sharing the folder should need: all but what
   this server loads and the two newest of each kind. Deleting can fail on
   Windows while another server has the file open; it is tried again next time. */
static void ZPAM_Cleanup(const ZpamJob *job, const ZpamState *state)
{
    const char *keep[6];
    const char *prefixes[2] = {zpamPrefix, zpamMapsPrefix};
    char **names;
    int count, kept = 0;
    if ((long long)time(NULL) - state->updated < ZPAM_CLEANUP_SECONDS)
        return;
    count = ZPAM_List(job->folder, &names);
    keep[kept++] = job->loaded;
    keep[kept++] = job->loadedMaps;
    for (int p = 0; p < 2; ++p) {
        const char *newest = ZPAM_Newest(prefixes[p], names, count, state);
        keep[kept++] = newest ? newest : "";
        newest = ZPAM_NewestBelow(prefixes[p], names, count, state, ZPAM_Version(newest ? newest : "", prefixes[p]));
        keep[kept++] = newest ? newest : "";
    }
    for (int i = 0; i < count; ++i) {
        int needed = 0;
        char path[1100];
        if (!ZPAM_IsIwdName(names[i]))
            continue;
        for (int k = 0; k < kept && !needed; ++k)
            needed = !I_stricmp(names[i], keep[k]);
        if (needed)
            continue;
        ZPAM_Path(path, sizeof(path), job->folder, names[i]);
        remove(path);
    }
    ZPAM_FreeList(names, count);
}

/* Worker: the whole install, all or nothing. New files are only added, so no
   running server sees a change. */
static void ZPAM_Install(void *argument)
{
    ZpamJob *job = argument;
    char fetched[COD2X_ZPAM_MAX_FILES] = {0};
    char error[512] = "", installed[512] = "";
    intptr_t lock = Cod2x_LockFolder(job->folder, "zpam");
    if (lock != -1) {
        ZpamState state;
        int ok = ZPAM_DownloadMissing(job, fetched, error, sizeof(error)) &&
                 ZPAM_MoveDownloaded(job, fetched, installed, sizeof(installed), error, sizeof(error));
        for (int i = 0; i < job->manifest.fileCount; ++i) {
            char path[1100], part[1100];
            if (!fetched[i])
                continue;
            ZPAM_Path(path, sizeof(path), job->folder, job->manifest.files[i].name);
            snprintf(part, sizeof(part), "%s.part", path);
            remove(part);
        }
        if (ok && ZPAM_SaveState(job, &state, error, sizeof(error)))
            ZPAM_Cleanup(job, &state);
        Cod2x_UnlockFolder(lock);
    }
    /* Another server is installing; the next check picks up its result. */
    Cod2x_Lock();
    snprintf(zpamWorkerError, sizeof(zpamWorkerError), "%s", error);
    snprintf(zpamWorkerInstalled, sizeof(zpamWorkerInstalled), "%s", installed);
    zpamWorker = 2;
    Cod2x_Unlock();
    free(job);
}

int SV_Cod2xZpamApplyManifest(const Cod2xZpamManifest *manifest)
{
    static int wwwPrinted;
    char folder[1024];
    int changed;
    ZpamJob *job;

    /* An answer without a manifest (or a broken one) must not clear the blocked list. */
    if (!Dvar_GetInt("dedicated") || !manifest->fileCount)
        return 1;
    /* Only servers already running zPAM are updated. */
    ZPAM_MainFolder(folder, sizeof(folder));
    if (!folder[0] || !SV_Cod2xZpamUpdateCheckNeeded())
        return 1;
    /* Also without web downloads: an earlier automatic update may have left versions. */
    if (!ZPAM_SaveBlock(folder, manifest, &changed)) {
        Com_Printf("zPAM: Failed to save blocked versions to " ZPAM_STATE_FILE ", CoD2x update postponed\n");
        return 0;
    }
    if (changed)
        ZPAM_UpdateSwitchPending(folder);
    /* The names become file names. */
    for (int i = 0; i < manifest->fileCount; ++i) {
        if (!ZPAM_IsIwdName(manifest->files[i].name)) {
            Com_Printf("zPAM: Invalid file name in update info: %s\n", manifest->files[i].name);
            return 1;
        }
    }
    if (!ZPAM_WwwDownload()) {
        if (!wwwPrinted) {
            wwwPrinted = 1;
            Com_Printf("zPAM: Automatic update needs sv_wwwDownload 1 and sv_wwwBaseURL, update skipped\n");
        }
        return 1;
    }
    Cod2x_Lock();
    if (zpamWorker) {
        Cod2x_Unlock();
        return 1;
    }
    zpamWorker = 1;
    Cod2x_Unlock();
    job = calloc(1, sizeof(*job));
    if (job) {
        snprintf(job->folder, sizeof(job->folder), "%s", folder);
        snprintf(job->loaded, sizeof(job->loaded), "%s", zpamLoaded);
        snprintf(job->loadedMaps, sizeof(job->loadedMaps), "%s", zpamLoadedMaps);
        job->manifest = *manifest;
    }
    if (!job || !Cod2x_ThreadStart(ZPAM_Install, job)) {
        free(job);
        Cod2x_Lock();
        zpamWorker = 0;
        Cod2x_Unlock();
    }
    return 1;
}

void SV_Cod2xZpamFrame(void)
{
    unsigned int now;
    char error[512], installed[512];
    int finished;
    if (!com_dedicated || !com_dedicated->current.integer)
        return;
    Cod2x_Lock();
    finished = zpamWorker == 2;
    if (finished) {
        snprintf(error, sizeof(error), "%s", zpamWorkerError);
        snprintf(installed, sizeof(installed), "%s", zpamWorkerInstalled);
        zpamWorker = 0;
    }
    Cod2x_Unlock();
    if (finished) {
        char folder[1024];
        if (error[0]) {
            extern void Cod2x_ReportError(const char *kind, const char *message);
            char message[600];
            Com_Printf("zPAM: Update failed: %s\n", error);
            snprintf(message, sizeof(message), "zPAM update failed: %s", error);
            Cod2x_ReportError("zpam-update", message);
        }
        if (installed[0])
            Com_Printf("zPAM: Installed %s\n", installed);
        /* Also when another server installed it. */
        if (!error[0]) {
            ZPAM_MainFolder(folder, sizeof(folder));
            ZPAM_UpdateSwitchPending(folder);
        }
    }
    if (!zpamSwitchPending || !com_sv_running || !com_sv_running->current.enabled || !ZPAM_ServerEmpty() ||
        !ZPAM_CanAutoUpdate()) {
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
