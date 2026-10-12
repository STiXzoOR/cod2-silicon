#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "imports.h"

static dvar_t privacy, home, frameCap;
const dvar_t *fs_homepath = &home;
const dvar_t *com_maxfps = &frameCap;
static clientConnection_t connection;
static clientActive_t active;
static LegacyHacks legacy;
static LegacyHacks *legacyPointer = &legacy;
static int qport, pure, checksum;
static void *bgs;
void *imp_legacyHacks = &legacyPointer;
void *imp_g_qport = &qport;
void *imp_cl_connectedToPureServer = &pure;
void *imp_fs_checksumFeed = &checksum;
void *imp_bgs = &bgs;
clientConnection_t clientConnections[1];
clientStatic_t cls;
static dvar_t disableConsole, nextMapDvar;
const dvar_t *sv_disableClientConsole = &disableConsole;
const dvar_t *nextmap = &nextMapDvar;
int g_waitingForServer;
const clientConnection_t *clc = &connection;
void *cl = &active;
static uint64_t milliseconds;
static int summaryClock(clockid_t clock, struct timespec *time)
{
    if (clock == CLOCK_MONOTONIC) {
        time->tv_sec = milliseconds / 1000;
        time->tv_nsec = (milliseconds % 1000) * 1000000;
    } else {
        time->tv_sec = 1791763200;
        time->tv_nsec = 0;
    }
    return 0;
}
const dvar_t *Dvar_RegisterBool_mac(const char *name, int value, int flags)
{
    assert(!strcmp(name, "cl_matchSummaries") && value == 1);
    privacy.flags = flags;
    return &privacy;
}
const char *CL_GetConfigString(int index)
{
    return index == 0 ? "\\mapname\\mp_toujane\\g_gametype\\sd\\g_competitive\\1\\shortversion\\1.4\\mod\\zPAM \"test\"" : "\\sv_pure\\1";
}
char *Info_ValueForKey(const char *info, const char *key)
{
    static char value[512];
    while (*info) {
        const char *start = ++info, *end = strchr(start, '\\');
        if (!end) break;
        info = end + 1;
        end = strchr(info, '\\');
        if (!end) end = info + strlen(info);
        if (strlen(key) == (size_t)(info - start - 1) && !strncmp(start, key, info - start - 1)) {
            size_t count = end - info;
            assert(count < sizeof(value));
            memcpy(value, info, count); value[count] = 0; return value;
        }
        info = end;
    }
    return "";
}
char *NET_AdrToString(netadr_t address) { (void)address; return "192.0.2.1:28960"; }
void CL_StopRecord_f(void) {}
void FS_FCloseFile(int file) { (void)file; }
void SCR_StopCinematic(void) {}
void MSG_WriteReliableCommandToBuffer(const char *command, char *buffer, int size) { (void)command; (void)buffer; (void)size; }
void CL_WritePacket(void) {}
void CL_ClearState(void) { memset(&active, 0, sizeof(active)); }
void CL_ClearMutedList(void) {}
void SND_DisconnectListener(void) {}
int CL_AllLocalClientsDisconnected(void) { return 1; }
void Dvar_SetBool(const dvar_t *var, int value) { (void)var; (void)value; }
void Con_Close(void) {}
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void Dvar_SetString(const dvar_t *var, const char *value) { (void)var; (void)value; }
void I_strncpyz(char *dest, const char *source, int size) { snprintf(dest, size, "%s", source); }
void UI_CloseAll(void) {}
int NET_StringToAdr(const char *text, netadr_t *address) { (void)text; memset(address, 0, sizeof(*address)); return 1; }
void CL_CheckForResend(void) {}
void SND_FadeAllSounds(float target, float time) { (void)target; (void)time; }
void Cod2x_Disconnect(void) {}
void Cod2x_DemoClientDisconnect(void) {}
int Cod2x_FrameFPS(int requested) { return requested; }

/* The no-op baseline drives the same caller sequence before the feature exists. */
#if __has_include("../../src/PC/qcommon/match_summary.c")
#define clock_gettime summaryClock
#include "../../src/PC/qcommon/match_summary.c"
#undef clock_gettime
#else
static void MatchSummary_Init(void) {}
static void MatchSummary_Frame(int msec, int budget, int cap) { (void)msec; (void)budget; (void)cap; }
static void MatchSummary_Snapshot(int ping, int measured, int intermission) { (void)ping; (void)measured; (void)intermission; }
static void MatchSummary_Packet(int dropped) { (void)dropped; }
static void MatchSummary_Finish(const char *reason) { (void)reason; }
#endif

#include "lifecycle_source.h"

static void begin(void)
{
    connection.state = CA_ACTIVE;
    legacy.cl_running = 1;
    clientConnections[0].state = CA_ACTIVE;
    MatchSummary_Snapshot(30, 1, 0);
}
static void frame(int msec, int budget, int cap)
{
    milliseconds += msec;
    MatchSummary_Frame(msec, budget, cap);
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    home.current.string = argv[2];
    frameCap.current.integer = 333;
    legacy.cl_running = 1;
    privacy.current.enabled = strcmp(argv[1], "privacy") != 0;
    MatchSummary_Init();
    if (!strcmp(argv[1], "privacy")) privacy.current.enabled = 1;
    if (!strcmp(argv[1], "retention")) {
        for (int i = 0; i < 205; ++i) {
            begin(); frame(3, 3, 333); CL_Disconnect();
        }
    } else if (!strcmp(argv[1], "lifecycle")) {
        begin(); frame(4, 4, 250);
        MatchSummary_Snapshot(40, 1, 1);
        MatchSummary_Snapshot(40, 1, 1);
        CL_MapLoading("mp_toujane"); CL_Disconnect();
        begin(); frame(8, 8, 125); CL_MapLoading("mp_toujane"); CL_Disconnect();
        connection.demoplaying = 1;
        begin(); frame(3, 3, 333); CL_Disconnect();
    } else if (!strcmp(argv[1], "overflow")) {
        begin(); frame(3, 3, 333); frame(20000, 3, 333);
        CL_Disconnect();
    } else if (!strcmp(argv[1], "unmeasured")) {
        connection.state = CA_ACTIVE;
        MatchSummary_Snapshot(999, 0, 0);
        frame(5, 1, 0);
        CL_Disconnect();
    } else {
        begin();
        for (int i = 0; i < 98; ++i) frame(3, 3, 333);
        frame(7, 3, 333); frame(2000, 3, 333);
        MatchSummary_Packet(0); MatchSummary_Packet(2); MatchSummary_Packet(0);
        MatchSummary_Snapshot(40, 1, 0);
        MatchSummary_Snapshot(999, 0, 0);
        MatchSummary_Snapshot(50, 1, 1);
        CL_Disconnect();
    }
    return 0;
}
