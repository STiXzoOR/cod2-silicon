/* Retain only the production playback/disconnect hooks from the client unit. */
#include "../../src/PC/client_mp/cl_main_mp.c"
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

static dvar_t test_developer, test_cheats, test_name, test_url, test_home, test_gametype;
clientConnection_t clientConnections[1];
const clientConnection_t *clc = clientConnections;
static int closes, writes, setters;
void Com_Printf(const char *format, ...)
{
    (void)format;
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
    ++setters;
    ((dvar_t *)var)->current.string = value;
    ((dvar_t *)var)->modified = 1;
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
    assert(setters == 4 && test_developer.current.integer == 2 && test_cheats.current.enabled);
    Cod2x_DemoClientDisconnect();
    assert(test_developer.current.integer == 1);
    assert(test_cheats.current.enabled == 0);
    assert(!*test_name.current.string && !test_name.modified);
    assert(!*test_url.current.string && !test_url.modified);
    assert(test_gametype.current.integer == 0);
    {
        int previous = setters;
        test_name.modified = test_url.modified = 1;
        Cod2x_DemoClearAutoDvars();
        assert(previous == setters && !test_name.modified && !test_url.modified);
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
    assert(file && fputs("http://localhost/upload/round", file) >= 0);
    assert(!fclose(file));
    test_home.current.string = directory;
    fs_homepath = &test_home;
    strcpy(fs_gamedir, "main");
    test_name.current.string = "round";
    clientConnections[0].demorecording = 1;
    clientConnections[0].demofile = 17;
    CL_StopRecord_f();
    assert(clientConnections[0].demorecording && !closes && !writes);
    assert(Cod2x_DemoUploadPaused(CA_CONNECTING, 0));
    assert(!Cod2x_DemoUploadPaused(CA_ACTIVE, 0));
    assert(Cod2x_DemoClientQuitRequested());
    assert(!clientConnections[0].demorecording && closes == 1 && writes == 2);
    assert(!*test_name.current.string && cod2x_quitAfterUpload);
    assert(!Cod2x_DemoUploadPaused(CA_CONNECTING, 0));
    assert(Cod2x_DemoUploadPaused(CA_CONNECTING, 1));
    Cod2x_DemoUploadFrame(demos, 10, 0);
    assert(!Cod2x_DemoClientQuitRequested());
    assert(access(marker, F_OK));
    assert(!rmdir(demos) && !rmdir(game) && !rmdir(directory));
    puts("CoD2x production demo playback/restore, stoprecord, and quit deferral: pass");
    return 0;
}
