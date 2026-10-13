#include "common_types.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void MacServer_InstallSignals(void);
extern void MacServer_CheckQuit(void);
extern void MacServer_MasterHeartbeat(const char *);
extern void MacServer_MasterGameCompleteStatus(void);
serverStatic_t svs;
level_locals_t level;
static dvar_t dedicated, masterDvars[5];
const dvar_t *com_dedicated = &dedicated;
static int registrations, resolves, heartbeats, statuses, completions, quits;
static int hostRevision;
static unsigned short lastHeartbeatPort;
static char lastResolved[256];
int Cod2x_ServerGameVersion(void) { return hostRevision; }

const dvar_t *Dvar_RegisterString(const char *name, const char *value, int flags)
{
    int index = name[9] - '1';
    assert(!strncmp(name, "sv_master", 9) && index >= 0 && index < 5 && flags == 0);
    if (!index)
        assert(!strcmp(value, "cod2master.activision.com:20710"));
    else if (index == 1) {
#if defined(COD2_CODX) && COD2_CODX
        assert(!strcmp(value, hostRevision >= 6 ? "master.cod2x.me:20710" : ""));
#else
        assert(!*value);
#endif
    } else
        assert(!*value);
    ++registrations;
    if (!masterDvars[index].current.string)
        masterDvars[index].current.string = value;
    return &masterDvars[index];
}

qboolean NET_StringToAdr(const char *name, netadr_t *address)
{
    ++resolves;
    snprintf(lastResolved, sizeof(lastResolved), "%s", name);
    if (!strcmp(name, "invalid"))
        return 0;
    address->type = NA_IP;
    address->ip[0] = 127;
    address->ip[3] = 1;
    address->port = 123;
    return 1;
}

Bool NET_OutOfBandPrint(netsrc_t source, netadr_t address, const char *text)
{
    assert(source == NS_SERVER && address.type == NA_IP);
    assert(!strcmp(text, "heartbeat COD-2\n") || !strcmp(text, "heartbeat flatline\n"));
    ++heartbeats;
    lastHeartbeatPort = address.port;
    return 1;
}
void SVC_Status(netadr_t address) { assert(address.type == NA_IP); ++statuses; }
void SVC_GameCompleteStatus(netadr_t address) { assert(address.type == NA_IP); ++completions; }
void Com_Printf(const char *text, ...) { (void)text; }
void Com_Quit_f(void) { ++quits; }
uint64_t MacSystem_Nanoseconds(void) { return 0; }

int main(int argc, char **argv)
{
    int extra = 0;
    hostRevision = argc > 1 ? atoi(argv[1]) : 0;
#if defined(COD2_CODX) && COD2_CODX
    extra = hostRevision >= 6;
#endif
    unsetenv("COD2_SERVER_TICK_CSV");
    dedicated.current.integer = 1;
    MacServer_MasterHeartbeat("COD-2");
    MacServer_MasterGameCompleteStatus();
    assert(!registrations && !resolves && !heartbeats && !statuses && !completions);
    masterDvars[0].current.string = "127.0.0.1:29240";
    masterDvars[2].current.string = "127.0.0.1:29241";
    dedicated.current.integer = 2;
    svs.time = 100;
    MacServer_MasterHeartbeat("COD-2");
    assert(registrations == 5 && resolves == 2 + extra && heartbeats == 2 + extra && statuses == 2 + extra);
    assert(svs.nextHeartbeatTime == 180100 && svs.nextStatusResponseTime == 600100);
    MacServer_MasterHeartbeat("COD-2");
    assert(heartbeats == 2 + extra && statuses == 2 + extra);
    svs.time = 180100;
    MacServer_MasterHeartbeat("COD-2");
    assert(heartbeats == 4 + 2 * extra && statuses == 2 + extra && resolves == 2 + extra);
    masterDvars[2].current.string = "invalid";
    svs.nextHeartbeatTime = 0;
    MacServer_MasterHeartbeat("COD-2");
    assert(heartbeats == 5 + 3 * extra && resolves == 3 + extra);
    svs.nextHeartbeatTime = 0;
    MacServer_MasterHeartbeat("flatline");
    assert(heartbeats == 6 + 4 * extra && resolves == 3 + extra);
    MacServer_MasterGameCompleteStatus();
    assert(completions == 1 + extra);

    /* A changed name is re-resolved. Bare names get the production default
       port; an explicitly empty master stops all three kinds of traffic. */
    masterDvars[0].current.string = "replacement.example";
    masterDvars[1].current.string = "";
    svs.time = 600100;
    MacServer_MasterHeartbeat("COD-2");
    assert(!strcmp(lastResolved, "replacement.example"));
    assert(resolves == 4 + extra && heartbeats == 7 + 4 * extra);
    assert(statuses == 3 + extra && svs.nextStatusResponseTime == 1200100);
    assert(lastHeartbeatPort == (unsigned short)((20710 >> 8) | (20710 << 8)));
    masterDvars[0].current.string = "";
    svs.nextHeartbeatTime = svs.nextStatusResponseTime = 0;
    MacServer_MasterHeartbeat("COD-2");
    MacServer_MasterGameCompleteStatus();
    assert(resolves == 4 + extra && heartbeats == 7 + 4 * extra);
    assert(statuses == 3 + extra && completions == 1 + extra);
    MacServer_InstallSignals();
    raise(SIGTERM);
    assert(quits == 0);
    MacServer_CheckQuit();
    assert(quits == 1);
    puts("PASS: LAN silence, master list/cache/timers, flatline and deferred SIGTERM quit");
}
