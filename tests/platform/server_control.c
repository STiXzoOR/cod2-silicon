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

const dvar_t *Dvar_RegisterString(const char *name, const char *value, int flags)
{
    int index = name[9] - '1';
    assert(!strncmp(name, "sv_master", 9) && index >= 0 && index < 5 && flags == 0);
    ++registrations;
    if (!masterDvars[index].current.string)
        masterDvars[index].current.string = value;
    return &masterDvars[index];
}

qboolean NET_StringToAdr(const char *name, netadr_t *address)
{
    ++resolves;
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
    return 1;
}
void SVC_Status(netadr_t address) { assert(address.type == NA_IP); ++statuses; }
void SVC_GameCompleteStatus(netadr_t address) { assert(address.type == NA_IP); ++completions; }
void Com_Printf(const char *text, ...) { (void)text; }
void Com_Quit_f(void) { ++quits; }
uint64_t MacSystem_Nanoseconds(void) { return 0; }

int main(void)
{
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
    assert(registrations == 5 && resolves == 2 && heartbeats == 2 && statuses == 2);
    assert(svs.nextHeartbeatTime == 180100 && svs.nextStatusResponseTime == 600100);
    MacServer_MasterHeartbeat("COD-2");
    assert(heartbeats == 2 && statuses == 2);
    svs.time = 180100;
    MacServer_MasterHeartbeat("COD-2");
    assert(heartbeats == 4 && statuses == 2 && resolves == 2);
    masterDvars[2].current.string = "invalid";
    svs.nextHeartbeatTime = 0;
    MacServer_MasterHeartbeat("COD-2");
    assert(heartbeats == 5 && resolves == 3);
    svs.nextHeartbeatTime = 0;
    MacServer_MasterHeartbeat("flatline");
    assert(heartbeats == 6 && resolves == 3);
    MacServer_MasterGameCompleteStatus();
    assert(completions == 1);
    MacServer_InstallSignals();
    raise(SIGTERM);
    assert(quits == 0);
    MacServer_CheckQuit();
    assert(quits == 1);
    puts("PASS: LAN silence, master list/cache/timers, flatline and deferred SIGTERM quit");
}
