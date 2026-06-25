#include "common_types.h"
#include "cod2_feature_config.h"
#include "scr_debugger.h"

#if COD2_FEATURE_SCRIPT_DEBUGGER

#    include <string.h>
#    include <stdlib.h>

#    if COD2_FEATURE_SCRIPT_DEBUGGER_REMOTE
#        include <sys/types.h>
#        include <sys/socket.h>
#        include <netinet/in.h>
#        include <arpa/inet.h>
#        include <unistd.h>
#        include <fcntl.h>
#        include <errno.h>
#    endif

extern void Com_Printf(const char *fmt, ...);
extern void Com_DPrintf(const char *fmt, ...);
extern void I_strncpyz(char *dest, const char *src, int destsize);

extern unsigned char *Scr_GetProgramBuffer(void);

#    define OP_BREAKPOINT 135
#    define OP_ASSIGN_BREAKPOINT 136
#    define OP_BOTH_BREAKPOINT 137
#    define BP_FREE 127

struct scrDebuggerThread_s;

typedef struct scrDebuggerGlob_s {
    char _pad0[59 * 4];
    struct scrDebuggerThread_s *threadList;
    char _pad1[1 * 4];
    unsigned int hitLine;
    char _pad2[(145 - 62) * 4];
    unsigned char *bpInfoTable;
    unsigned char savedOpcode;
    char _pad146[3];
    unsigned char *hitCodePos;
    int stepMode;
    unsigned int hitThreadStartLine;
    char _pad3[2 * 4];
    unsigned char *manualBp;
    unsigned int manualBpLine;
    char _pad4[1 * 4];
    unsigned char *pendingBp;
    char _pad5[1 * 4];
    void *hitValue;
    char _pad6[1 * 4];
    int hitType;
    char _pad7[(169 - 160) * 4];
    unsigned int curThreadStartLine;
    char _pad8[2 * 4];
    int breakpointCount;

    char _pad9[(0x25c - 173) * 4];
    int inUpdate;
    char _pad10[(0x278 - 0x25d) * 4];
    int pendingUpdate;
    char _pad11[(0x288 - 0x279) * 4];
    int field288;
    char _pad12[(0x299 - 0x289) * 4];
    int debuggerInited;
    int debuggerSystemInited;
    char _pad13[(0x2a8 - 0x29b) * 4];
    void *busyHandle;
} scrDebuggerGlob_t;

static scrDebuggerGlob_t scrDebuggerGlob;

static unsigned char *g_dbgManualOverride;
static unsigned char *g_dbgPendingManual;
static int g_dbgServerActive;
static int g_dbgContinue;
static int g_dbgPendingResolve;
static int g_dbgSuppress;
static int g_dbgManualPending;
static int g_dbgStepTimeout;

struct Scr_ScriptWatch;
extern struct Scr_ScriptWatch *scr_debuggerWatch;

extern void Scr_ScriptWatch_Init(struct Scr_ScriptWatch *self);
extern void Scr_ScriptWatch_Shutdown(struct Scr_ScriptWatch *self);
extern void Scr_ScriptWatch_Evaluate(struct Scr_ScriptWatch *self);
extern void Scr_ScriptWatch_UpdateBreakpoints(struct Scr_ScriptWatch *self, int announce);
extern void Scr_ScriptWatch_SortHitBreakpointsTop(struct Scr_ScriptWatch *self);

extern void Scr_DisplayDebugger(void);
extern void Scr_Debugger_InitComponents(void);

static void Scr_ScriptWatch__Init(void)
{
    Scr_Debugger_InitComponents();
}
static void Scr_ScriptWatch__Shutdown(void)
{
    if (scr_debuggerWatch)
        Scr_ScriptWatch_Shutdown(scr_debuggerWatch);
}
static void Scr_ScriptWatch__Evaluate(void)
{
    if (scr_debuggerWatch)
        Scr_ScriptWatch_Evaluate(scr_debuggerWatch);
}
static void Scr_ScriptWatch__UpdateBreakpoints(void *c)
{
    (void)c;
    if (scr_debuggerWatch)
        Scr_ScriptWatch_UpdateBreakpoints(scr_debuggerWatch, 0);
}
static void Scr_ScriptWatch__SortHitBreakpointsTop(void)
{
    if (scr_debuggerWatch)
        Scr_ScriptWatch_SortHitBreakpointsTop(scr_debuggerWatch);
}
static void Scr_ResetTimeout(void) {}

#    if COD2_FEATURE_SCRIPT_DEBUGGER_REMOTE

#        define SCR_DEBUG_PORT 28000

static int g_dbgListenSock = -1;
static int g_dbgClientSock = -1;
static int g_dbgRemoteServer = 0;
static int g_dbgSocketState = 0;

static int Sys_IsRemoteDebugServer(void)
{
    return g_dbgRemoteServer;
}

static int Sys_IsMainThread(void)
{
    return 1;
}

static int Sys_StartRemoteDebugServer(void)
{
    struct sockaddr_in addr;
    int sock;
    int one = 1;
    int flags;

    if (g_dbgListenSock != -1)
        return g_dbgRemoteServer;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return 0;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SCR_DEBUG_PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }
    if (listen(sock, 1) < 0) {
        close(sock);
        return 0;
    }

    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    g_dbgListenSock = sock;
    g_dbgRemoteServer = 1;
    return 1;
}

static void Sys_StopRemoteDebugServer(void)
{
    if (g_dbgClientSock != -1) {
        close(g_dbgClientSock);
        g_dbgClientSock = -1;
    }
    if (g_dbgListenSock != -1) {
        close(g_dbgListenSock);
        g_dbgListenSock = -1;
    }
    g_dbgRemoteServer = 0;
    g_dbgSocketState = 0;
}

static void Sys_PollRemoteAccept(void)
{
    int s;
    int flags;

    if (g_dbgListenSock == -1 || g_dbgClientSock != -1)
        return;

    s = accept(g_dbgListenSock, 0, 0);
    if (s < 0)
        return;

    flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    g_dbgClientSock = s;
}

static int Sys_UpdateDebugSocket(void)
{
    char buf[256];
    int n;
    int code;

    if (g_dbgSocketState != 0)
        return g_dbgSocketState;

    Sys_PollRemoteAccept();
    if (g_dbgClientSock == -1)
        return 0;

    n = (int)recv(g_dbgClientSock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {

            close(g_dbgClientSock);
            g_dbgClientSock = -1;
        }
        return 0;
    }
    buf[n] = '\0';

    code = atoi(buf) & 0xff;
    g_dbgSocketState = code;
    return code;
}

static void Sys_AckDebugSocket(void)
{
    g_dbgSocketState = 0;
}

static void NET_Sleep(int msec)
{

    (void)msec;
}

#    else

static int Sys_IsRemoteDebugServer(void)
{
    return 0;
}
static int Sys_IsMainThread(void)
{
    return 1;
}
static int Sys_StartRemoteDebugServer(void)
{
    return 0;
}
static void Sys_StopRemoteDebugServer(void) {}
static int Sys_UpdateDebugSocket(void)
{
    return 0;
}
static void Sys_AckDebugSocket(void) {}
static void NET_Sleep(int msec)
{
    (void)msec;
}

#    endif

static void Scr_AddTextRemote(void) {}

static unsigned char *Scr_FindBreakpointInfo(const unsigned char *codePos)
{
    unsigned char *base = Scr_GetProgramBuffer();

    int off = (int)(codePos - base);
    return scrDebuggerGlob.bpInfoTable + off;
}

static void Scr_AddBreakpoint(const unsigned char *codePos)
{
    unsigned char *info = Scr_FindBreakpointInfo(codePos);
    info[0] = codePos[0];
    scrDebuggerGlob.breakpointCount++;
}

static void Scr_RemoveBreakpoint(unsigned char *codePos)
{
    unsigned char *info = Scr_FindBreakpointInfo(codePos);
    codePos[0] = info[0];
    info[0] = BP_FREE;
    scrDebuggerGlob.breakpointCount--;
}

static void Scr_AddManualBreakpoint_Internal(unsigned char *codePos)
{
    unsigned char op = codePos[0];

    if (op == OP_BREAKPOINT || op == OP_BOTH_BREAKPOINT) {

        g_dbgManualPending = 1;
        return;
    }
    if (op == OP_ASSIGN_BREAKPOINT) {
        codePos[0] = OP_BOTH_BREAKPOINT;
        return;
    }
    Scr_AddBreakpoint(codePos);
    codePos[0] = OP_BREAKPOINT;
}

static void Scr_RemoveManualBreakpoint(unsigned char *codePos)
{
    unsigned char op;

    if (scrDebuggerGlob.manualBp == codePos && g_dbgManualPending) {
        g_dbgManualPending = 0;
        return;
    }
    op = codePos[0];
    if (op == OP_BREAKPOINT) {
        Scr_RemoveBreakpoint(codePos);
    } else {

        codePos[0] = OP_ASSIGN_BREAKPOINT;
    }
}

static void Scr_AddAssignmentBreakpoint(unsigned char *codePos)
{
    if (codePos[0] != OP_BREAKPOINT) {
        Scr_AddBreakpoint(codePos);
        codePos[0] = OP_ASSIGN_BREAKPOINT;
        return;
    }
    codePos[0] = OP_BOTH_BREAKPOINT;
}

void Scr_RemoveAssignmentBreakpoint(unsigned char *codePos)
{
    unsigned char op = codePos[0];

    if (op == OP_ASSIGN_BREAKPOINT) {
        Scr_RemoveBreakpoint(codePos);
        return;
    }

    codePos[0] = OP_BREAKPOINT;
}

unsigned char Scr_GetBreakpointType(const char **pp)
{
    const char *p = *pp;
    char c = p[0];

    if (c == '#') {
        p++;
        *pp = p;
        if (p[0] == '@') {
            *pp = p + 1;
            return 5;
        }
        if (p[0] != '#') {
            return 3;
        }
        *pp = p + 1;
        return 4;
    }
    if (c == '@') {
        *pp = p + 1;
        return 1;
    }
    if (c == '?') {
        *pp = p + 1;
        return 2;
    }
    return 0;
}

int Scr_HitBreakpoint(void *value, const char *file, unsigned int line, int column)
{
    unsigned char *opPos;
    unsigned char savedOpcode;
    int isManualHit = 0;
    int condStop = 0;
    int wantStop;

    (void)column;

    opPos = (unsigned char *)file - 1;
    savedOpcode = Scr_FindBreakpointInfo(opPos)[0];

    if (scrDebuggerGlob.manualBp == opPos) {
        unsigned int bpLine = scrDebuggerGlob.manualBpLine;
        if (bpLine != 0) {
            if (line == bpLine) {
                isManualHit = 1;
                condStop = 0;
            } else {
                isManualHit = 0;
                condStop = 1;
            }
        } else {
            isManualHit = 1;
            condStop = 0;
        }
    }

    if (g_dbgPendingResolve) {
        g_dbgPendingResolve = 0;
        savedOpcode = 0;
        if (scrDebuggerGlob.pendingBp != 0) {
            Scr_RemoveManualBreakpoint(scrDebuggerGlob.pendingBp);
            scrDebuggerGlob.pendingBp = 0;
            condStop = 1;
        }
    }

    scrDebuggerGlob.hitValue = value;

    wantStop = 0;

    if (wantStop == 0 && isManualHit == 0 && condStop != 0)
        return savedOpcode;

    if (scrDebuggerGlob.manualBp != 0) {
        Scr_RemoveManualBreakpoint(scrDebuggerGlob.manualBp);
        scrDebuggerGlob.manualBp = 0;
    }

    if (g_dbgSuppress)
        return savedOpcode;

    if (!Sys_IsRemoteDebugServer()) {

        Com_Printf("Script breakpoint hit\n");
        return savedOpcode;
    }

    scrDebuggerGlob.hitValue = value;
    scrDebuggerGlob.hitCodePos = opPos;
    scrDebuggerGlob.savedOpcode = savedOpcode;
    scrDebuggerGlob.hitLine = line;

    Scr_ScriptWatch__SortHitBreakpointsTop();
    Scr_DisplayDebugger();

    if (Sys_IsRemoteDebugServer() || g_dbgContinue == 0) {
        Scr_ResetTimeout();
        return savedOpcode;
    }

    g_dbgContinue = 0;
    scrDebuggerGlob.stepMode = -1;
    Scr_ResetTimeout();
    return savedOpcode;
}

int Scr_HitAssignmentBreakpoint(void *value, const char *file, unsigned int line, int column)
{
    unsigned char *opPos;
    unsigned char savedOpcode;

    (void)line;
    (void)column;

    opPos = (unsigned char *)file - 1;
    savedOpcode = Scr_FindBreakpointInfo(opPos)[0];

    scrDebuggerGlob.hitValue = value;

    Scr_ScriptWatch__UpdateBreakpoints(&scrDebuggerGlob);

    return savedOpcode;
}

void Scr_InitDebuggerSystem(void)
{
    Sys_StartRemoteDebugServer();

    if (scrDebuggerGlob.debuggerSystemInited)
        return;

    g_dbgManualOverride = 0;
    g_dbgPendingManual = 0;
    g_dbgServerActive = 0;

    scrDebuggerGlob.field288 = 0;
    scrDebuggerGlob.stepMode = -1;
    scrDebuggerGlob.inUpdate = 0;
    scrDebuggerGlob.pendingUpdate = 0;

    Scr_ScriptWatch__Init();
    g_dbgStepTimeout = 0;

    scrDebuggerGlob.debuggerSystemInited = 1;

    if (Sys_IsRemoteDebugServer()) {
        Scr_RunDebugger();
        return;
    }
    Scr_ScriptWatch__UpdateBreakpoints(&scrDebuggerGlob);
}

void Scr_ShutdownDebuggerSystem(int restart)
{
    (void)restart;

    if (!scrDebuggerGlob.debuggerSystemInited)
        return;

    scrDebuggerGlob.debuggerSystemInited = 0;

    Scr_ScriptWatch__Shutdown();

    if (g_dbgManualOverride) {
        Scr_RemoveManualBreakpoint(g_dbgManualOverride);
        g_dbgManualOverride = 0;
    }
    if (g_dbgPendingManual) {
        Scr_RemoveManualBreakpoint(g_dbgPendingManual);
        g_dbgPendingManual = 0;
    }

    Sys_StopRemoteDebugServer();
}

extern unsigned int Scr_GetProgramLen(void);

static void Scr_InitDebugger(void)
{
    unsigned int len;

    if (scrDebuggerGlob.debuggerInited)
        return;

    len = Scr_GetProgramLen();
    if (len == 0)
        return;

    scrDebuggerGlob.bpInfoTable = (unsigned char *)malloc(len);
    if (scrDebuggerGlob.bpInfoTable == 0)
        return;

    memset(scrDebuggerGlob.bpInfoTable, BP_FREE, len);

    Scr_ScriptWatch__Init();
    scrDebuggerGlob.debuggerInited = 1;
}

static void Scr_ShutdownDebugger(void)
{
    if (!scrDebuggerGlob.debuggerInited)
        return;

    scrDebuggerGlob.debuggerInited = 0;
    if (scrDebuggerGlob.bpInfoTable) {
        free(scrDebuggerGlob.bpInfoTable);
        scrDebuggerGlob.bpInfoTable = 0;
    }
}

void Scr_RunDebugger(void)
{
    scrDebuggerGlob.inUpdate = 0;
    Scr_DisplayDebugger();
}

void Scr_AddDebuggerRefs(void)
{
    scrDebuggerGlob.inUpdate = 0;
    Scr_ScriptWatch__Evaluate();
    Scr_ScriptWatch__UpdateBreakpoints(&scrDebuggerGlob);
}

static void Scr_ProcessDebugMessages(void)
{
    while ((unsigned int)(Sys_UpdateDebugSocket() - 4) <= 41) {
        Scr_AddTextRemote();
        Sys_AckDebugSocket();
    }
    NET_Sleep(1);
}

static int Scr_UpdateDebugSocket(void)
{
    if ((unsigned int)(Sys_UpdateDebugSocket() - 1) > 43)
        return 0;
    do {
        Scr_AddTextRemote();
        Sys_AckDebugSocket();
    } while ((unsigned int)(Sys_UpdateDebugSocket() - 1) <= 43);
    return 1;
}

static void Scr_UpdateRemoteDebugger(void)
{
    if (!Sys_IsMainThread())
        return;
    if (!scrDebuggerGlob.debuggerSystemInited)
        return;
    if (!Sys_IsRemoteDebugServer())
        return;
    if (Sys_UpdateDebugSocket() == 0)
        return;

    Scr_ScriptWatch__UpdateBreakpoints(&scrDebuggerGlob);
    Scr_ScriptWatch__Evaluate();
    Scr_ProcessDebugMessages();
    Scr_ScriptWatch__UpdateBreakpoints(&scrDebuggerGlob);
}

void Scr_UpdateDebugger(void)
{
    if (!scrDebuggerGlob.debuggerSystemInited)
        return;

    if (scrDebuggerGlob.busyHandle != 0)
        return;

    if (g_dbgStepTimeout != 0) {
        if (scrDebuggerGlob.inUpdate != 0) {
            g_dbgStepTimeout = 0;
        }
    }

    Scr_UpdateRemoteDebugger();

    scrDebuggerGlob.inUpdate = 0;

    if (scrDebuggerGlob.pendingUpdate) {
        scrDebuggerGlob.pendingUpdate = 0;
        Scr_ScriptWatch__SortHitBreakpointsTop();
        Scr_RunDebugger();
    }
}

extern unsigned char *Scr_ResolveSourcePos(const char *filename, int line);

void Scr_AddManualBreakpoint(const char *filename, int line)
{
    unsigned char *codePos;

    if (!filename)
        return;

    if (!scrDebuggerGlob.bpInfoTable)
        Scr_InitDebugger();

    codePos = Scr_ResolveSourcePos(filename, line);
    if (codePos) {
        Scr_AddManualBreakpoint_Internal(codePos);
        return;
    }

    scrDebuggerGlob.pendingBp = 0;
    g_dbgPendingResolve = 1;
    Com_DPrintf("Scr_Debugger: deferred breakpoint %s:%d (no source map)\n",
                filename, line);
}

void Scr_Debugger_OnInstruction(const char *codePos)
{
    (void)codePos;

    if (!scrDebuggerGlob.debuggerSystemInited)
        return;

    if (g_dbgPendingResolve || scrDebuggerGlob.pendingUpdate)
        Scr_UpdateDebugger();
}

__attribute__((weak)) unsigned char *Scr_GetProgramBuffer(void)
{
    return 0;
}
__attribute__((weak)) unsigned int Scr_GetProgramLen(void)
{
    return 0;
}
__attribute__((weak)) unsigned char *Scr_ResolveSourcePos(const char *f, int l)
{
    (void)f;
    (void)l;
    return 0;
}

#endif
