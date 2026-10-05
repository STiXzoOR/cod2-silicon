#if defined(__APPLE__) && defined(COD2_X64) && defined(DEDICATED)
#include "common_types.h"
#include "macos_system.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Com_Quit_f(void);
extern void Com_Printf(const char *format, ...);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, int);
extern qboolean NET_StringToAdr(const char *, netadr_t *);
extern Bool NET_OutOfBandPrint(netsrc_t, netadr_t, const char *);
extern void SVC_Status(netadr_t);
extern void SVC_GameCompleteStatus(netadr_t);
extern const dvar_t *com_dedicated;
extern serverStatic_t svs;
extern level_locals_t level;

/* Shared HUD alignment vocabulary normally resides in the client UI unit. */
const char str_002b3f60[] = "fullscreen";

static volatile sig_atomic_t quitRequested;

static void MacServer_RequestQuit(int signalNumber)
{
    quitRequested = signalNumber;
}

void MacServer_InstallSignals(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = MacServer_RequestQuit;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGTERM, &action, NULL) || sigaction(SIGINT, &action, NULL)) {
        perror("CoD2 server signal setup");
        exit(1);
    }
}

void MacServer_CheckQuit(void)
{
    static int initialized, lastFrame = -1;
    static FILE *ticks;
    if (!initialized) {
        const char *path = getenv("COD2_SERVER_TICK_CSV");
        initialized = 1;
        if (path && *path) {
            ticks = fopen(path, "w");
            if (!ticks)
                perror("CoD2 server tick trace");
            else
                fputs("wall_ns,simulation_ms,frame\n", ticks);
        }
    }
    if (ticks && level.framenum > 0 && level.framenum != lastFrame) {
        lastFrame = level.framenum;
        fprintf(ticks, "%llu,%d,%d\n", (unsigned long long)MacSystem_Nanoseconds(), svs.time, lastFrame);
        fflush(ticks);
    }
    if (quitRequested)
        Com_Quit_f();
}

static const dvar_t *masters[5];
static netadr_t masterAddresses[5];
static char masterNames[5][256];

static qboolean MacServer_MasterAddress(int index, netadr_t *address)
{
    const char *name;
    if (!masters[index]) {
        char dvarName[16];
        snprintf(dvarName, sizeof(dvarName), "sv_master%d", index + 1);
        masters[index] = Dvar_RegisterString(dvarName, index ? "" : "cod2master.activision.com:20710", 0);
    }
    name = masters[index]->current.string;
    if (!name[0] || strlen(name) >= sizeof(masterNames[index]))
        return 0;
    if (strcmp(name, masterNames[index])) {
        strcpy(masterNames[index], name);
        memset(&masterAddresses[index], 0, sizeof(masterAddresses[index]));
        if (!NET_StringToAdr(name, &masterAddresses[index])) {
            Com_Printf("Could not resolve sv_master%d\n", index + 1);
            masterAddresses[index].type = NA_BAD;
        } else if (!strchr(name, ':')) {
            masterAddresses[index].port = (unsigned short)((20710 >> 8) | (20710 << 8));
        }
    }
    *address = masterAddresses[index];
    return address->type == NA_IP;
}

void MacServer_MasterHeartbeat(const char *heartbeat)
{
    int sendHeartbeat, sendStatus;
    if (!com_dedicated || com_dedicated->current.integer != 2)
        return;
    sendHeartbeat = svs.time >= svs.nextHeartbeatTime;
    sendStatus = svs.time >= svs.nextStatusResponseTime;
    if (!sendHeartbeat && !sendStatus)
        return;
    if (sendHeartbeat)
        svs.nextHeartbeatTime = svs.time + 180000;
    if (sendStatus)
        svs.nextStatusResponseTime = svs.time + 600000;
    for (int i = 0; i < 5; ++i) {
        netadr_t address;
        if (!MacServer_MasterAddress(i, &address))
            continue;
        if (sendHeartbeat) {
            char message[128];
            snprintf(message, sizeof(message), "heartbeat %s\n", heartbeat);
            NET_OutOfBandPrint(NS_SERVER, address, message);
        }
        if (sendStatus)
            SVC_Status(address);
    }
}

void MacServer_MasterGameCompleteStatus(void)
{
    if (!com_dedicated || com_dedicated->current.integer != 2)
        return;
    for (int i = 0; i < 5; ++i) {
        netadr_t address;
        if (MacServer_MasterAddress(i, &address))
            SVC_GameCompleteStatus(address);
    }
}
#endif
