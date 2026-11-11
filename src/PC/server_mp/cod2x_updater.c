#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_server.h"
#include "../qcommon/cod2x.h"
#include "../qcommon/cod2x_http.h"
#include "../qcommon/cod2x_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The update check of a CoD2x server. A dedicated server asks CoD2x's master
   over HTTPS at startup when it runs zPAM, and every 30 to 35 minutes while
   sv_update is on and a map runs, or while it runs zPAM. The answer carries the
   zPAM manifest (cod2x_zpam.c) and whether a newer CoD2x exists. CoD2x then
   replaces its own library; this server is a different program, so it only
   says that an update exists. When the HTTPS check fails at startup or on a
   map, the old UDP request goes out as CoD2x sends it.

   COD2X_TEST_UPDATER_URL and COD2X_TEST_UPDATER_ADDRESS in the environment
   point both requests elsewhere, and COD2X_TEST_UPDATER_PERIOD_MS shortens the
   period, for tests only. Without the first two, COD2X_TEST_SERVICE_URL (the
   stand-in for all of CoD2x's web services) takes both: the HTTPS check goes to
   its /api/updater/check and the UDP request to its host and port. */

extern void Com_Printf(const char *fmt, ...);
extern void Com_DPrintf(const char *fmt, ...);
extern const char *va(const char *fmt, ...);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern int Dvar_GetInt(const char *dvarName);
extern int Sys_Milliseconds(void);
extern const char *NET_AdrToString(netadr_t a);
extern qboolean NET_StringToAdr(const char *s, netadr_t *a);
extern qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
extern Bool NET_OutOfBandPrint(netsrc_t sock, netadr_t adr, const char *data);
extern char *SV_Cmd_Argv(int arg);
extern const dvar_t *com_sv_running;

#define UPDATER_URL "https://master.cod2x.me/api/updater/check"
#define UPDATER_HOST "master.cod2x.me"
#define UPDATER_PORT 20720
#define UPDATER_PERIOD_MSEC (30 * 60 * 1000)
#define UPDATER_JITTER_MSEC (5 * 60 * 1000)
#ifdef _WIN32
#define UPDATER_OS "win-x86"
#define UPDATER_TEXT_CHECKING "Auto-Updater: Checking for updates...\n"
#define UPDATER_TEXT_FAILED "Auto-Updater: Request failed: %s\n"
#define UPDATER_TEXT_UDP "Auto-Updater: Trying UDP...\n"
#define UPDATER_UDP_REQUEST "getUpdateInfo2 \"CoD2x MP\" \"" COD2X_RELEASE "\" \"win-x86\" \"server\"\n"
#else
#define UPDATER_OS "linux-i386"
#define UPDATER_TEXT_CHECKING "Checking for updates...\n"
#define UPDATER_TEXT_FAILED "CoD2x: Failed to check for updates: %s\n"
#define UPDATER_TEXT_UDP "Trying UDP...\n"
#define UPDATER_UDP_REQUEST "getUpdateInfo2 \"CoD2x MP\" \"" COD2X_RELEASE "\" \"linux-i386\" \"server\""
#endif

static const dvar_t *sv_update;
static int updaterRunning;
static netadr_t updaterAddress;
static unsigned int updaterNextCheck;
static int updaterNextCheckSet;

void SV_Cod2xUpdaterInit(void)
{
    sv_update = Dvar_RegisterBool("sv_update", 1, 0x1000);
}

static int Updater_Dedicated(void)
{
    return Dvar_GetInt("dedicated") > 0;
}

/* A newer CoD2x is acted on only with sv_update and a map running. */
static int Updater_UpdateEnabled(void)
{
    return sv_update && sv_update->current.enabled && com_sv_running && com_sv_running->current.enabled;
}

/* zPAM is checked also before a map runs. */
static int Updater_CheckEnabled(void)
{
    return Updater_UpdateEnabled() || SV_Cod2xZpamUpdateCheckNeeded();
}

static void Updater_Report(int available, const char *version)
{
    Com_Printf("-----------------------------------\n");
    if (available)
        Com_Printf("CoD2x: Update available: %s -> %s\n", COD2X_RELEASE, version);
    else
        Com_Printf("CoD2x: No updates available.\n");
    Com_Printf("-----------------------------------\n");
}

/* The old request, a UDP packet to the update port of CoD2x's master. */
static void Updater_SendUdp(void)
{
    char testAddress[256];
    const char *test = Cod2x_TestUpdaterAddress(testAddress, sizeof(testAddress));
    int port = UPDATER_PORT;
    char name[256];
    snprintf(name, sizeof(name), "%s", test && test[0] ? test : UPDATER_HOST);
    if (test && test[0] && strrchr(name, ':')) {
        port = atoi(strrchr(name, ':') + 1);
        *strrchr(name, ':') = '\0';
    }
    Com_DPrintf("Resolving AutoUpdate Server %s...\n", name);
    if (!NET_StringToAdr(name, &updaterAddress)) {
        Com_Printf("\nFailed to resolve AutoUpdate server %s.\n", name);
        return;
    }
    updaterAddress.port = (unsigned short)(((port & 0xff) << 8) | ((port >> 8) & 0xff));
    Com_DPrintf("AutoUpdate resolved to %s\n", NET_AdrToString(updaterAddress));
    Com_Printf(UPDATER_TEXT_CHECKING);
    NET_OutOfBandPrint(NS_SERVER, updaterAddress, UPDATER_UDP_REQUEST);
    Com_Printf("-----------------------------------\n");
}

void SV_Cod2xUpdateResponse(netadr_t from)
{
    if (updaterAddress.type == NA_BAD) {
        Com_DPrintf("Auto-Updater has bad address\n");
        return;
    }
    Com_DPrintf("Auto-Updater response from %s\n", NET_AdrToString(from));
    if (!NET_CompareBaseAdr(updaterAddress, from)) {
        Com_DPrintf("Received update packet from unexpected IP.\n");
        return;
    }
    Updater_Report(atoi(SV_Cmd_Argv(1)), SV_Cmd_Argv(3));
}

/* The manifest; a broken one is dropped without costing the rest of the answer. */
static int Updater_ParseZpam(const char *body, Cod2xZpamManifest *manifest)
{
    char path[64];
    memset(manifest, 0, sizeof(*manifest));
    for (int i = 0; i < COD2X_ZPAM_MAX_FILES; ++i) {
        Cod2xZpamFile *file = &manifest->files[i];
        double size = 0;
        snprintf(path, sizeof(path), "$.zpam.files[%d].name", i);
        if (!Cod2x_JsonString(body, path, file->name, sizeof(file->name)))
            break;
        snprintf(path, sizeof(path), "$.zpam.files[%d].url", i);
        if (!Cod2x_JsonString(body, path, file->url, sizeof(file->url)))
            return 0;
        snprintf(path, sizeof(path), "$.zpam.files[%d].sha256", i);
        if (!Cod2x_JsonString(body, path, file->sha256, sizeof(file->sha256)))
            return 0;
        snprintf(path, sizeof(path), "$.zpam.files[%d].size", i);
        if (!Cod2x_JsonNumber(body, path, &size))
            return 0;
        file->size = (size_t)size;
        manifest->fileCount = i + 1;
    }
    while (manifest->blockedCount < COD2X_ZPAM_MAX_BLOCKED) {
        snprintf(path, sizeof(path), "$.zpam.block[%d]", manifest->blockedCount);
        if (!Cod2x_JsonString(body, path, manifest->blocked[manifest->blockedCount], sizeof(manifest->blocked[0])))
            break;
        ++manifest->blockedCount;
    }
    return 1;
}

typedef struct {
    int periodic;
} UpdaterCheck;

static void Updater_Done(void *context, const Cod2xHttpResponse *response)
{
    UpdaterCheck *check = context;
    int periodic = check->periodic;
    int applyUpdate = Updater_UpdateEnabled();
    char error[256], available[64], url[1024] = "", version[256] = "";
    Cod2xZpamManifest *manifest = NULL;
    int parsed = 0, level = 0;

    free(check);
    updaterRunning = 0;
    snprintf(error, sizeof(error), "%s", response->error[0] ? response->error : "No response");
    if (!response->error[0]) {
        if (response->status < 200 || response->status >= 300) {
            snprintf(error, sizeof(error), "HTTP %d", response->status);
        } else if (Cod2x_JsonString(response->body, "$.update.available", available, sizeof(available)) &&
                   (!strcmp(available, "0") || !strcmp(available, "1") || !strcmp(available, "2"))) {
            level = atoi(available);
            Cod2x_JsonString(response->body, "$.update.url", url, sizeof(url));
            Cod2x_JsonString(response->body, "$.update.version", version, sizeof(version));
            /* An update without a file can't be applied. */
            parsed = !level || url[0];
            if (!parsed)
                snprintf(error, sizeof(error), "Invalid response");
        } else {
            snprintf(error, sizeof(error), "Invalid response");
        }
    }

    if (parsed) {
        int zpamSafe = 1;
        manifest = calloc(1, sizeof(*manifest));
        if (manifest) {
            if (!Updater_ParseZpam(response->body, manifest))
                memset(manifest, 0, sizeof(*manifest));
            /* A CoD2x downgrade must not go ahead before the zPAM versions it can't run are blocked. */
            zpamSafe = SV_Cod2xZpamApplyManifest(manifest);
            free(manifest);
        }
        if (applyUpdate && zpamSafe)
            Updater_Report(level, version);
        return;
    }

    Com_Printf(UPDATER_TEXT_FAILED, error);
    if (applyUpdate && !periodic) {
        Com_Printf(UPDATER_TEXT_UDP);
        Updater_SendUdp();
    }
}

/* Only what decides the answer and the installed zPAM: a server has no identity to send. */
static void Updater_SendHttps(int periodic)
{
    const char *url = getenv("COD2X_TEST_UPDATER_URL");
    char zpam[256], service[512];
    Cod2xText body = {0};
    Cod2xHttpRequest request;
    UpdaterCheck *check;

    if (updaterRunning)
        return;
    Cod2x_TextAppendString(&body, "{\"game\":\"CoD2x MP\",\"version\":\"" COD2X_RELEASE "\",\"os\":\"" UPDATER_OS
                                  "\",\"type\":\"server\"");
    SV_Cod2xZpamInstalled(zpam, sizeof(zpam));
    if (zpam[0]) {
        Cod2x_TextAppendString(&body, ",\"zpam\":\"");
        Cod2x_TextAppendJson(&body, zpam, strlen(zpam));
        Cod2x_TextAppendString(&body, "\"");
    }
    Cod2x_TextAppendString(&body, "}");
    check = calloc(1, sizeof(*check));
    if (body.failed || !check) {
        Cod2x_TextFree(&body);
        free(check);
        return;
    }
    check->periodic = periodic;
    memset(&request, 0, sizeof(request));
    request.method = "POST";
    request.url = url && url[0] ? url : Cod2x_ServiceURL(UPDATER_URL, service, sizeof(service));
    request.headers = "Content-Type: application/json\r\n";
    request.body = body.data;
    request.bodyLength = body.length;
    request.timeoutMs = 5000;
    request.connectTimeoutMs = 5000;
    if (Cod2x_HttpStart(&request, Updater_Done, check)) {
        updaterRunning = 1;
        Com_Printf(UPDATER_TEXT_CHECKING);
    } else {
        free(check);
    }
    Cod2x_TextFree(&body);
}

void SV_Cod2xUpdaterCheck(void)
{
    if (Updater_Dedicated() && Updater_CheckEnabled())
        Updater_SendHttps(0);
}

/* Every 30 minutes and a random part of 5 more, so servers started together
   don't ask together. */
static int Updater_PeriodicDue(void)
{
    unsigned int now = (unsigned int)Sys_Milliseconds();
    int due = updaterNextCheckSet && (int)(now - updaterNextCheck) >= 0;
    if (!updaterNextCheckSet || due) {
        const char *test = getenv("COD2X_TEST_UPDATER_PERIOD_MS");
        unsigned int random = 0;
        Cod2x_Random(&random, sizeof(random));
        updaterNextCheck = now + UPDATER_PERIOD_MSEC + random % UPDATER_JITTER_MSEC;
        if (test && atoi(test) > 0)
            updaterNextCheck = now + (unsigned int)atoi(test);
        updaterNextCheckSet = 1;
    }
    return due;
}

void SV_Cod2xUpdaterFrame(void)
{
    if (!Updater_Dedicated())
        return;
    if (Updater_PeriodicDue() && Updater_CheckEnabled())
        Updater_SendHttps(1);
}
#endif
