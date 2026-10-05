#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#define DV(name) static dvar_t name##_value; static const dvar_t *name = &name##_value
DV(com_viewlog); DV(com_dedicated); DV(com_animCheck); DV(com_maxfps);
DV(com_fixedtime); DV(com_timescale); DV(com_sv_running); DV(com_statmon);
DV(cl_maxpackets); DV(cl_showSend);
static int com_fullyInitialized, dvar_modifiedFlags, com_frameTime, com_lastFrameTime;
static int accessed, *com_fileAccessed = &accessed, timeClientFrame;
static float com_codeTimeScale = 1, com_timescaleValue;
static int clockMsec, lastServerMsec, lastClientMsec, sleeps, limited;
static int workMsec, packetCount, lastPacketTime, packetPeriod;
static clientStatic_t cls;
static clientConnection_t connection, *connectionPtr = &connection;
static clientActive_t activeClient, *activePtr = &activeClient;
static void *imp_clc = &connectionPtr, *imp_cl = &activePtr;
int Cod2x_FPSLimited(void) { return limited; }
int Com_HasPlayerProfile(void) { return 0; }
void Com_BuildPlayerProfilePath(char *p, int size, const char *file) { (void)p; (void)size; (void)file; }
void Com_WriteConfigToFile(const char *p) { (void)p; }
void Sys_ShowConsole(int a, int b) { (void)a; (void)b; }
void Dvar_ClearModified(const dvar_t *v) { (void)v; }
void SetAnimCheck(int enabled) { (void)enabled; }
int Com_EventLoop(void) { return ++clockMsec; }
void NET_Sleep(int msec) { assert(msec == 0); ++sleeps; }
void Sys_WaitUntilMilliseconds(unsigned int target) { assert(target > (unsigned int)clockMsec); ++sleeps; }
void MacSystem_ObserveFrame(int engineTime) { assert(engineTime == clockMsec); }
void Cbuf_Execute(void) {}
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void CL_SwitchToLocalClient(int n) { assert(n == 0); }
void SV_Frame(int msec) { lastServerMsec = msec; }
const dvar_t *Dvar_RegisterInt(const char *n, int v, int min, int max, int flags)
{ (void)n; (void)v; (void)min; (void)max; (void)flags; return com_dedicated; }
void CL_Shutdown(void) { assert(0); }
void Sys_NormalExit(void) { assert(0); }
void SV_AddDedicatedCommands(void) { assert(0); }
void CL_RunOncePerClientFrame(int msec) { assert(msec == lastServerMsec); }
void SND_UpdateLoopingSounds(void) {}
void SND_Update(void) {}
void CL_Frame(int msec) { lastClientMsec = msec; clockMsec += workMsec; }
void SCR_UpdateScreenInternal(void) {}
void SCR_RunCinematic(void) {}
void StatMon_Warning(int a, int b, const char *s) { (void)a; (void)b; (void)s; }
int Sys_Milliseconds(void) { return clockMsec; }
usercmd_t CL_CreateCmd(void) { usercmd_t cmd = {0}; cmd.serverTime = cls.realtime; return cmd; }
qboolean Sys_IsLANAddress(netadr_t adr) { (void)adr; return 0; }
void CL_WritePacket(void)
{
    if (packetCount) assert(cls.realtime - lastPacketTime == packetPeriod);
    lastPacketTime = cls.realtime; ++packetCount;
    activeClient.outPackets[connection.netchan.outgoingSequence & 31].p_realtime = cls.realtime;
    ++connection.netchan.outgoingSequence;
}
#include "timing_source.h"
static void simulate(int fps, int expected)
{
    com_maxfps_value.current.integer = fps;
    clockMsec = com_lastFrameTime = com_frameTime = sleeps = packetCount = 0;
    connection.state = CA_ACTIVE; connection.netchan.outgoingSequence = 1;
    memset(&activeClient, 0, sizeof(activeClient));
    connection.serverAddress.type = NA_IP;
    cl_maxpackets_value.current.integer = 125;
    packetPeriod = ((8 + expected - 1) / expected) * expected;
    for (int i = 0; i < 1000; ++i) {
        Com_Frame_Try_Block_Function();
        assert(lastServerMsec == expected && lastClientMsec == expected);
        assert(com_frameTime == (i + 1) * expected && com_timescaleValue == 1);
        cls.realtime = com_frameTime;
        CL_SendCmdInternal();
        assert(activeClient.cmds[activeClient.cmdNumber & 127].serverTime == cls.realtime);
    }
    assert(packetCount == 1000 * expected / packetPeriod);
    printf("%d fps: 1000 x %d ms; packets every %d ms\n", fps, expected, packetPeriod);
}
int main(void)
{
    com_timescale_value.current.value = 1;
    simulate(125, 8); simulate(250, 4); simulate(333, 3);
    limited = 1;
#if defined(COD2_CODX) && COD2_CODX
    simulate(333, 4);
#else
    simulate(333, 3);
#endif
    limited = 0;
    /* A renderer hitch passes its real integer elapsed time through unchanged. */
    workMsec = 12; Com_Frame_Try_Block_Function(); Com_Frame_Try_Block_Function();
    assert(lastClientMsec >= 12 && lastClientMsec == lastServerMsec);
    return 0;
}
