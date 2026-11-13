/* Retain only the production playback/disconnect hooks from the client unit. */
#include "../../src/PC/client_mp/cl_main_mp.c"
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

static dvar_t test_developer, test_cheats, test_name, test_url, test_home, test_gametype, test_timeout;
clientConnection_t clientConnections[1];
const clientConnection_t *clc = clientConnections;
static int closes, writes, setters;
static int records, quits, markers, collisions, accept_record;
static char recorded_command[512], marker_path[512], marker_url[1024], name_value[1024], url_value[1024];
void Com_Printf(const char *format, ...)
{
    (void)format;
}
void Com_Error(int code, const char *format, ...)
{
    (void)code;
    (void)format;
    assert(0);
}
void FS_FCloseFile(fileHandle_t handle)
{
    assert(handle == 17);
    ++closes;
}
int FS_Write(const void *data, int length, int handle)
{
    assert(handle == 17 && length == 4 && *(const int *)data == -1);
    ++writes;
    return length;
}
const dvar_t *fs_homepath;
char fs_gamedir[256];
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *ospath)
{
    snprintf(ospath, 256, "%s/%s/%s", base, game, qpath);
}
const dvar_t *Dvar_FindVar(const char *name)
{
    if (!strcmp(name, "developer"))
        return &test_developer;
    if (!strcmp(name, "sv_cheats"))
        return &test_cheats;
    if (!strcmp(name, "ui_joinGametype"))
        return &test_gametype;
    return NULL;
}
void Dvar_ClearModified(const dvar_t *var)
{
    ((dvar_t *)var)->modified = 0;
}
void Dvar_SetInt(const dvar_t *var, int value)
{
    ++setters;
    ((dvar_t *)var)->current.integer = value;
}
void Dvar_SetBool(const dvar_t *var, int value)
{
    ++setters;
    ((dvar_t *)var)->current.enabled = (unsigned char)value;
}
void Dvar_SetString(const dvar_t *var, const char *value)
{
    char *saved = var == &test_name ? name_value : url_value;
    ++setters;
    if (strcmp(var->current.string, value)) {
        snprintf(saved, 1024, "%s", value);
        ((dvar_t *)var)->current.string = saved;
        ((dvar_t *)var)->modified = 1;
    }
}

void I_strncpyz(char *destination, const char *source, int size)
{
    snprintf(destination, (size_t)size, "%s", source);
}
int Com_sprintf(char *destination, int size, const char *format, ...)
{
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(destination, (size_t)size, format, args);
    va_end(args);
    return length;
}
qboolean FS_FileExists(const char *path)
{
    (void)path;
    if (collisions) {
        --collisions;
        return 1;
    }
    return 0;
}
qboolean FS_WriteFile(const char *path, const void *bytes, int length)
{
    ++markers;
    snprintf(marker_path, sizeof(marker_path), "%s", path);
    assert(length < (int)sizeof(marker_url));
    memcpy(marker_url, bytes, (size_t)length);
    marker_url[length] = 0;
    return 1;
}
/* Observe the immediate engine command boundary; this fixture supplies record
   acceptance, while the production recorder retains its existing argc/state checks. */
void Cmd_ExecuteString(const char *command)
{
    if (!strcmp(command, "quit\n")) {
        ++quits;
        return;
    }
    assert(!strncmp(command, "record ", 7));
    ++records;
    snprintf(recorded_command, sizeof(recorded_command), "%s", command);
    if (accept_record && clientConnections[0].state == CA_ACTIVE) {
        clientConnections[0].demorecording = 1;
        clientConnections[0].demofile = 17;
    }
}

int main(void)
{
    char directory[] = "/tmp/cod2x-demo-quit.XXXXXX", game[256], demos[256], marker[256];
    FILE *file;
    test_gametype.current.integer = 3;
    test_developer.current.integer = 1;
    test_cheats.current.enabled = 0;
    cod2x_demoName = &test_name;
    cod2x_demoURL = &test_url;
    cod2x_demoTimeout = &test_timeout;
    test_timeout.current.integer = 60;
    test_name.current.string = "demo";
    test_url.current.string = "https://example.test/";
    Cod2x_DemoPlayback(1);
    assert(test_developer.current.integer == 2);
    assert(test_cheats.current.enabled == 1);
    assert(setters == 2);
    Cod2x_DemoPlayback(1);
    assert(setters == 2);
    test_developer.current.integer = 0;
    test_cheats.current.enabled = 0;
    Cod2x_DemoPlayback(1);
    /* As in CoD2x, only the start of playback sets them. */
    assert(setters == 2 && test_developer.current.integer == 0 && !test_cheats.current.enabled);
    test_cheats.current.enabled = 1;
    Cod2x_DemoClientDisconnect();
    assert(test_developer.current.integer == 1);
    assert(test_cheats.current.enabled == 1); /* sv_cheats is never put back */
    assert(!*test_name.current.string && test_name.modified);
    assert(!strcmp(test_url.current.string, "https://example.test/"));
    assert(test_gametype.current.integer == 0);
    {
        int previous = setters;
        test_name.modified = test_url.modified = 1;
        Cod2x_DemoClearAutoDvars();
        assert(previous + 1 == setters && !test_name.modified && !test_url.modified);
    }
    test_developer.current.integer = 0;
    test_cheats.current.enabled = 1;
    Cod2x_DemoPlayback(1);
    Cod2x_DemoPlayback(0);
    assert(test_developer.current.integer == 0);
    assert(test_cheats.current.enabled == 1);
    Cod2x_DemoPlayback(0);
    assert(mkdtemp(directory));
    snprintf(game, sizeof(game), "%s/main", directory);
    snprintf(demos, sizeof(demos), "%s/demos", game);
    snprintf(marker, sizeof(marker), "%s/round.dm_1.upload", demos);
    assert(!mkdir(game, 0700) && !mkdir(demos, 0700));
    file = fopen(marker, "wb");
    assert(file && fputs("http://user@localhost/upload/round", file) >= 0);
    assert(!fclose(file));
    test_home.current.string = directory;
    fs_homepath = &test_home;
    strcpy(fs_gamedir, "main");
    test_name.current.string = "round";
    clientConnections[0].demorecording = 1;
    clientConnections[0].demofile = 17;
    CL_StopRecord_f();
    assert(clientConnections[0].demorecording && !closes && !writes);
    assert(Cod2x_DemoClientQuitRequested());
    assert(clientConnections[0].demorecording && !closes && !writes);
    assert(!*test_name.current.string && cod2x_quitAfterUpload && cod2x_checkUploads);
    /* Quit schedules the change; the next frame stops recording, removes the
       marker whose demo is missing, and executes quit immediately. */
    assert(!Cod2x_DemoUploadFrame(demos, 10, 0) && !access(marker, F_OK));
    clientConnections[0].state = CA_ACTIVE;
    cod2x_demoClientState = CA_ACTIVE;
    Cod2x_DemoClientFrame();
    assert(!clientConnections[0].demorecording && closes == 1 && writes == 2);
    assert(quits == 1 && cod2x_quitAfterUpload);
    assert(!Cod2x_DemoClientQuitRequested());
    assert(access(marker, F_OK));
    cod2x_quitAfterUpload = 0;

    /* URL-only modification during playback is not the auto-record trigger. */
    clientConnections[0].demoplaying = 1;
    test_name.modified = 0;
    Dvar_SetString(&test_url, "http://127.0.0.1:9/new/");
    Cod2x_DemoClientFrame();
    assert(test_url.modified && *test_url.current.string && records == 0);
    Dvar_SetString(&test_name, "demo-must-not-record");
    Cod2x_DemoClientFrame();
    assert(!*test_name.current.string && !*test_url.current.string);
    assert(!test_name.modified && !test_url.modified && records == 0);
    clientConnections[0].demoplaying = 0;

    /* A connecting-state request is attempted immediately, not held for active. */
    clientConnections[0].state = CA_CONNECTING;
    Dvar_SetString(&test_name, "round/one");
    Cod2x_DemoClientFrame();
    assert(records == 1 && !strcmp(recorded_command, "record round_one\n"));
    assert(!test_name.modified && !clientConnections[0].demorecording);
    clientConnections[0].state = CA_ACTIVE;
    Cod2x_DemoClientFrame();
    assert(records == 1);

    /* Changing only the URL never stops/restarts the recording. Name changes
       rebuild collision names and create the marker before URL validation. */
    Dvar_SetString(&test_url, "invalid URL ");
    Cod2x_DemoClientFrame();
    assert(records == 1 && !markers);
    accept_record = 1;
    collisions = 1;
    Dvar_SetString(&test_name, "next/round");
    Cod2x_DemoClientFrame();
    assert(records == 2 && !strcmp(recorded_command, "record next/round_1\n"));
    assert(clientConnections[0].demorecording && markers == 1);
    assert(!strcmp(marker_path, "demos/next/round_1.dm_1.upload"));
    assert(!strcmp(marker_url, "invalid URL next/round_1"));
    Dvar_SetString(&test_name, "has spaces");
    accept_record = 0;
    Cod2x_DemoClientFrame();
    assert(records == 3 && !strcmp(recorded_command, "record has spaces\n"));
    assert(!clientConnections[0].demorecording && markers == 1 && closes == 2);
    {
        char long_name[256];
        memset(long_name, 'r', sizeof(long_name) - 1);
        long_name[sizeof(long_name) - 1] = 0;
        Dvar_SetString(&test_name, long_name);
        accept_record = 1;
        Cod2x_DemoClientFrame();
        assert(records == 4 && strlen(recorded_command) == 263);
        assert(markers == 2 && strlen(marker_path) == 273);
    }
    /* Disconnect's immediate hook leaves the URL until the state-change frame. */
    Cod2x_DemoClientDisconnect();
    assert(!*test_name.current.string && test_name.modified && *test_url.current.string);
    clientConnections[0].state = CA_DISCONNECTED;
    Cod2x_DemoClientFrame();
    assert(!*test_url.current.string && test_url.modified && closes == 3);
    printf("frame trace: records=%d markers=%d closes=%d quitCommands=%d\n", records, markers, closes, quits);
    assert(!rmdir(demos) && !rmdir(game) && !rmdir(directory));
    puts("CoD2x production demo playback/restore, stoprecord, and quit deferral: pass");
    return 0;
}
