#include "../fidelity/dvar_services.h"
#include "udp_globals.h"
#include "PC/server_mp/cod2x_host_protocol.h"
#include <limits.h>
#include <stdint.h>

serverStatic_t svs;
server_t sv;
qboolean gameInitialized;
static LegacyHacks legacy;
static LegacyHacks *legacyPointer = &legacy;
void *imp_legacyHacks = &legacyPointer;
static int numIwds;
void *imp_fs_numServerIwds = &numIwds;
static void *bgs;
void *imp_bgs = &bgs;
static client_t clients[2];
static uint32_t now;
static int responses, challenges, connects, auths, voices, rcons, redirects, hardeningCalls;
static int argc, qport;
static char tokens[1024];
static char *argv[16];
static const char *line;
static int lasttime;
static netadr_t replyAddress;
static char reply[32768];
extern void Dvar_SetInt(const dvar_t *, int);
extern void Dvar_SetString(const dvar_t *, const char *);
extern Bool Dvar_GetBool(const char *);
extern int Dvar_GetInt(const char *);
extern const char *Dvar_GetString(const char *);

int Sys_Milliseconds(void) { return (int32_t)now; }
int Com_Milliseconds(void) { return (int32_t)now; }
int COD2_NativeTraceEnabled(void) { return 0; }
int I_strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
void Com_DPrintf(const char *format, ...) { (void)format; }
void Cod2x_ServerInit(int dedicated) { (void)dedicated; }
int Cod2x_ServerGameVersion(void) { return 0; }
void SV_AddOperatorCommands(void) {}
void SV_Cmd_TokenizeString(const char *s)
{
    snprintf(tokens, sizeof(tokens), "%s", s);
    argc = 0;
    char *save, *token = strtok_r(tokens, " \t\r\n", &save);
    while (token && argc < 16) { argv[argc++] = token; token = strtok_r(NULL, " \t\r\n", &save); }
}
char *SV_Cmd_Argv(int n) { return n < argc ? argv[n] : ""; }
int SV_Cmd_Argc(void) { return argc; }
void SV_Cmd_ExecuteString(const char *text) { CHECK(!strcmp(text, "status ")); ++rcons; }
void MSG_BeginReading(msg_t *msg) { msg->readcount = 0; }
int MSG_ReadLong(msg_t *msg) { msg->readcount += 4; return 0; }
int MSG_ReadShort(msg_t *msg) { msg->readcount += 2; return qport; }
int MSG_ReadByte(msg_t *msg) { ++msg->readcount; return 0; }
char *MSG_ReadStringLine(msg_t *msg) { (void)msg; return (char *)line; }
void SV_Netchan_AddOOBProfilePacket(int length) { CHECK(length >= 4); }
void SV_GetChallenge(netadr_t from) { (void)from; ++challenges; }
void SV_DirectConnect(netadr_t from) { (void)from; ++connects; }
void SV_AuthorizeIpPacket(netadr_t from) { (void)from; ++auths; }
void SV_VoicePacket(netadr_t from, msg_t *msg) { (void)from; (void)msg; ++voices; }
void PB_HandleServerOobPacket(netadr_t *from, msg_t *msg) { (void)from; (void)msg; }
void SV_ResetSkeletonCache(void) {}
int G_RunFrame(int time) { CHECK(time == svs.time); return 0; }
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) { return a.type == b.type && !memcmp(a.ip, b.ip, 4); }
int Netchan_Process(netchan_t *n, msg_t *msg) { (void)n; (void)msg; return 0; }
void SV_Netchan_Decode(client_t *cl, byte *data, int size) { (void)cl; (void)data; (void)size; }
void SV_ExecuteClientMessage(client_t *cl, msg_t *msg) { (void)cl; (void)msg; }
void SV_PktEvtDbg(const char *format, int a, int b, int c, int d) { (void)format; (void)a; (void)b; (void)c; (void)d; }
const char *NET_AdrToString(netadr_t from) { (void)from; return "fixture address"; }
Bool NET_OutOfBandPrint(netsrc_t sock, netadr_t from, const char *text)
{
    CHECK(sock == NS_SERVER); ++responses; replyAddress = from;
    snprintf(reply, sizeof(reply), "%s", text); return 1;
}
const char *Dvar_InfoString(int bit) { CHECK(bit == 0x404); return "\\protocol\\118"; }
void Info_SetValueForKey(char *s, const char *key, const char *value)
{
    size_t n = strlen(s); snprintf(s + n, 1024 - n, "\\%s\\%s", key, value);
}
char *Info_ValueForKey(const char *s, const char *key) { (void)s; (void)key; return ""; }
qboolean FS_iwIwd(char *s, char *base) { (void)s; (void)base; return 1; }
playerState_t *SV_GameClientNum(int n) { (void)n; return NULL; }
int G_GetClientScore(int n) { (void)n; return 0; }
void LargeLocal_LargeLocal(const LargeLocal *l, int n) { (void)l; CHECK(n == MAX_MSGLEN); }
void *LargeLocal_GetBuf(const LargeLocal *l) { (void)l; static char b[MAX_MSGLEN]; return b; }
void ZN10LargeLocalD1Ev(LargeLocal *l) { (void)l; }
void SV_FlushRedirect(char *s) { (void)s; }
void Com_BeginRedirect(char *b, int n, void (*flush)(char *)) { (void)b; (void)n; (void)flush; ++redirects; }
void Com_EndRedirect(void) {}
int Com_AddToString(const char *s, char *b, int n, int cap, qboolean quote)
{
    (void)quote; CHECK(n + (int)strlen(s) < cap); strcpy(b + n, s); return n + (int)strlen(s);
}
int NetHardening_SVC_RateLimitAddress(netadr_t from, int time) { (void)from; (void)time; ++hardeningCalls; return 0; }
int NetHardening_SVC_RateLimit(int time) { (void)time; ++hardeningCalls; return 0; }
#include "udp_lan.h"
void SVC_RemoteCommand(netadr_t from, msg_t *msg);
#include "udp_source.h"

static const netadr_t remote = {NA_IP, {8,7,6,5}, 100};
static const netadr_t lan = {NA_IP, {192,168,1,5}, 100};
static netadr_t address(int n)
{
    netadr_t from = {NA_IP, {8,0,0,0}, 100}; from.ip[2] = n >> 8; from.ip[3] = n; return from;
}
static void reset(uint32_t time)
{
    now = time; lasttime = 0; SV_Init(); svs.clients = clients;
    const dvar_t *enabled = Dvar_FindVar("sv_rateLimiter");
    if (enabled) Dvar_SetBool(enabled, 1);
    memset(clients, 0, sizeof(clients)); Dvar_SetInt(sv_maxclients, 2);
    Dvar_SetInt(sv_privateClients, 0);
    Dvar_SetString(rcon_password, "secret");
    svs.time = 1000; responses = challenges = connects = auths = voices = rcons = redirects = hardeningCalls = 0;
}
static void dispatch(netadr_t from, const char *command)
{
    byte data[8] = {255,255,255,255}; msg_t msg = {0}; msg.data = data; msg.cursize = 8;
    line = command; SV_PacketEvent(from, &msg);
}
static void disconnect_reply(netadr_t from)
{
    byte data[8] = {0}; msg_t msg = {0}; msg.data = data; msg.cursize = 8;
    SV_PacketEvent(from, &msg);
}
static void registration(void)
{
    reset(1000);
    const dvar_t *enabled = Dvar_FindVar("sv_rateLimiter");
    const dvar_t *requests = Dvar_FindVar("sv_rateLimiterRequestCount");
    const dvar_t *ips = Dvar_FindVar("sv_rateLimiterUniqueIPCount");
#if defined(COD2_CODX) && COD2_CODX
    CHECK(enabled && enabled->current.enabled && enabled->flags == 0x1000);
    CHECK(requests && requests->flags == 0x40 && requests->domain.integer.min == 0 && requests->domain.integer.max == INT_MAX);
    CHECK(ips && ips->flags == 0x40 && ips->domain.integer.min == 0 && ips->domain.integer.max == INT_MAX);
    if (enabled) { Dvar_SetCommand("sv_rateLimiter", "0"); CHECK(!enabled->current.enabled); }
    if (requests) { Dvar_SetCommand("sv_rateLimiterRequestCount", "55"); CHECK(requests->current.integer == 0); }
#else
    CHECK(!enabled && !requests && !ips);
#endif
}
static void queries(void)
{
    const char *commands[] = {"getstatus", "getinfo", "getchallenge"};
    for (int kind = 0; kind < 3; ++kind) {
        reset(1000);
        for (int i = 0; i < 11; ++i) dispatch(remote, commands[kind]);
        CHECK(responses + challenges == 10);
        CHECK(hardeningCalls == 0);
    }
    reset(1000);
    for (int i = 0; i < 10; ++i) dispatch(remote, commands[i % 3]);
    dispatch(remote, "getchallenge"); CHECK(responses + challenges == 10);
    dispatch(remote, "connect"); dispatch(remote, "ipAuthorize"); dispatch(remote, "v");
    CHECK(connects == 1 && auths == 1 && voices == 1);
    netadr_t port = remote; ++port.port; dispatch(port, "getchallenge"); CHECK(responses + challenges == 10);
    now = 1999; dispatch(remote, "getchallenge"); CHECK(responses + challenges == 10);
    now = 2000; dispatch(remote, "getchallenge"); CHECK(responses + challenges == 11);
    dispatch(remote, "getchallenge"); CHECK(responses + challenges == 11);
    reset(1000);
    for (int i = 0; i < 11; ++i) dispatch(address(i), "getchallenge");
    CHECK(challenges == 10);
    now = 1099; dispatch(address(12), "getinfo"); CHECK(responses == 0);
    now = 1100; dispatch(address(13), "getinfo"); CHECK(responses == 1);
    dispatch(address(14), "getstatus"); CHECK(responses == 1);
    now = 1200; dispatch(address(14), "getstatus"); CHECK(responses == 2);
    CHECK(replyAddress.ip[3] == 14 && strstr(reply, "statusResponse"));
    reset(1000);
    for (int i = 0; i < 10; ++i) dispatch(remote, "getchallenge");
    now = 11001;
    for (int i = 0; i < 10; ++i) dispatch(remote, "getchallenge");
    CHECK(challenges == 20);
    now = 12000; dispatch(remote, "getchallenge"); CHECK(challenges == 21);
    reset(1000);
    for (int i = 0; i < 10; ++i) dispatch(address(i), "getchallenge");
    now = 2001;
    for (int i = 0; i < 10; ++i) dispatch(address(i + 10), "getchallenge");
    CHECK(challenges == 20);
    now = 2100; dispatch(address(21), "getchallenge"); CHECK(challenges == 21);
}
static void lan_and_disabled(void)
{
    reset(1000);
    for (int i = 0; i < 30; ++i) { now += 100; dispatch(lan, "getchallenge"); }
    CHECK(challenges == 30);
    for (int i = 0; i < 11; ++i) dispatch(lan, "getchallenge");
    CHECK(challenges == 39);
    const dvar_t *enabled = Dvar_FindVar("sv_rateLimiter");
    if (enabled) Dvar_SetBool(enabled, 0);
    for (int i = 0; i < 30; ++i) dispatch(remote, "getchallenge");
    CHECK(challenges == 69);
    dispatch(remote, "getinfo"); dispatch(remote, "getstatus");
    CHECK(responses == 2 && hardeningCalls == 0);
    for (int i = 0; i < 5; ++i) disconnect_reply(remote);
    CHECK(responses == 7);
    if (enabled) Dvar_SetBool(enabled, 1);
    dispatch(lan, "getchallenge"); CHECK(challenges == 69);
}
static void rcon_and_disconnect(void)
{
    reset(1000);
    for (int i = 0; i < 10; ++i) dispatch(address(i), "getchallenge");
    dispatch(address(20), "rcon secret status"); CHECK(rcons == 1);
    disconnect_reply(address(21)); CHECK(responses == 1 && !strcmp(reply, "disconnect"));
    reset(1000);
    dispatch(remote, "rcon secret status");
    dispatch(remote, "rcon secret status"); CHECK(rcons == 1); /* Stock 500-ms rcon boundary remains. */
    now = 1500; dispatch(remote, "rcon secret status"); CHECK(rcons == 2);
    now = 2000; dispatch(remote, "rcon wrong status"); CHECK(rcons == 2 && redirects == 3);
    reset(1000);
    for (int i = 0; i < 10; ++i) dispatch(address(i), "rcon secret status");
    now = 1500; dispatch(address(30), "rcon secret status"); CHECK(rcons == 1);
    now = 2000; dispatch(address(31), "rcon secret status"); CHECK(rcons == 2);
    reset(1000);
    for (int i = 0; i < 3; ++i) disconnect_reply(remote);
    CHECK(responses == 2);
    now = 1999; disconnect_reply(remote); CHECK(responses == 2);
    now = 2000; disconnect_reply(remote); CHECK(responses == 3);
    reset(1000);
    for (int i = 0; i < 65; ++i) disconnect_reply(address(i));
    CHECK(responses == 64);
    now = 1999; disconnect_reply(address(66)); CHECK(responses == 64);
    now = 2000; disconnect_reply(address(67)); CHECK(responses == 65);
    reset(1000);
    dispatch(remote, "getchallenge"); dispatch(remote, "rcon secret status"); disconnect_reply(remote);
    CHECK(challenges == 1 && rcons == 1 && responses == 0); /* One shared address table. */
    clients[0].state = 2; clients[0].netchan.remoteAddress = address(88); clients[0].netchan.qport = 7;
    qport = 7; disconnect_reply(address(88)); CHECK(responses == 0); qport = 0;
}
static void stats_and_time(void)
{
    reset(1000);
    dispatch(remote, "getchallenge"); dispatch(remote, "getinfo");
    disconnect_reply(address(1)); dispatch(lan, "getchallenge");
    SV_RunFrame();
    const dvar_t *requests = Dvar_FindVar("sv_rateLimiterRequestCount");
    const dvar_t *ips = Dvar_FindVar("sv_rateLimiterUniqueIPCount");
    CHECK(requests && requests->current.integer == 3); CHECK(ips && ips->current.integer == 2);
    dispatch(address(2), "getstatus"); now = 1999; SV_RunFrame();
    CHECK(requests && requests->current.integer == 3); CHECK(ips && ips->current.integer == 2);
    now = 2000; SV_RunFrame(); CHECK(requests && requests->current.integer == 4); CHECK(ips && ips->current.integer == 3);
    now = 2001; dispatch(remote, "getchallenge"); now = 3000; SV_RunFrame();
    CHECK(requests && requests->current.integer == 2); CHECK(ips && ips->current.integer == 1);
    const dvar_t *enabled = Dvar_FindVar("sv_rateLimiter");
    if (enabled) Dvar_SetBool(enabled, 0);
    now = 10000; SV_RunFrame(); CHECK(requests && requests->current.integer == 2);
    if (enabled) Dvar_SetBool(enabled, 1);
    SV_RunFrame(); CHECK(requests && requests->current.integer == 0); CHECK(ips && ips->current.integer == 0);
    reset(UINT32_C(0x7ffffc18));
    for (int i = 0; i < 10; ++i) dispatch(remote, "getchallenge");
    now += 1000; dispatch(remote, "getchallenge"); CHECK(challenges == 11); /* Signed clock boundary. */
    reset(UINT32_C(0xffffff00));
    for (int i = 0; i < 10; ++i) dispatch(remote, "getchallenge");
    now += 999; dispatch(remote, "getchallenge"); CHECK(challenges == 10);
    ++now; dispatch(remote, "getchallenge"); CHECK(challenges == 11); /* Unsigned rollover. */
    now = 100; dispatch(remote, "getchallenge"); CHECK(challenges == 12); /* Clock moved backwards. */
    reset(0); dispatch(remote, "getchallenge"); now = 1000; SV_RunFrame();
    CHECK(requests && requests->current.integer == 1); CHECK(ips && ips->current.integer == 1);
    const uint32_t boundaries[] = {UINT32_C(0x7ffffc18), UINT32_C(0xfffffc18)};
    for (int i = 0; i < 2; ++i) {
        reset(boundaries[i]); dispatch(remote, "getchallenge"); SV_RunFrame();
        CHECK(requests && requests->current.integer == 1); CHECK(ips && ips->current.integer == 1);
        now += 999; dispatch(address(1), "getchallenge"); SV_RunFrame();
        CHECK(requests && requests->current.integer == 1); CHECK(ips && ips->current.integer == 1);
        ++now; SV_RunFrame();
        CHECK(requests && requests->current.integer == 2); CHECK(ips && ips->current.integer == 2);
        now += 1000; SV_RunFrame();
        CHECK(requests && requests->current.integer == 0); CHECK(ips && ips->current.integer == 0);
    }
    for (int i = 0; i < 2; ++i) {
        reset(boundaries[i]); disconnect_reply(remote); disconnect_reply(remote);
        now += 999; disconnect_reply(remote); CHECK(responses == 2);
        ++now; disconnect_reply(remote); CHECK(responses == 3);
        disconnect_reply(remote); CHECK(responses == 3);
    }
    reset(1000); dispatch(remote, "getchallenge"); SV_RunFrame();
    now = 100; dispatch(remote, "getchallenge"); dispatch(remote, "getchallenge"); SV_RunFrame();
    CHECK(requests && requests->current.integer == 2); CHECK(ips && ips->current.integer == 1);
}
static void capacity(void)
{
    reset(1000);
    /* Address consumption happens first, including globally rejected queries. */
    for (int i = 0; i < 16384; ++i) dispatch(address(i), "getchallenge");
    CHECK(challenges == 10);
    SV_RunFrame();
    const dvar_t *requests = Dvar_FindVar("sv_rateLimiterRequestCount");
    const dvar_t *ips = Dvar_FindVar("sv_rateLimiterUniqueIPCount");
    CHECK(requests && requests->current.integer == 16384); CHECK(ips && ips->current.integer == 16384);
    now = 11000; dispatch(address(16385), "getchallenge"); CHECK(challenges == 10);
    ++now; dispatch(address(16385), "getchallenge"); CHECK(challenges == 11);
    reset(1000);
    for (int i = 0; i < 16384; ++i) dispatch(address(i), "getchallenge");
    now = 3000; disconnect_reply(address(16385)); CHECK(responses == 0);
    ++now; disconnect_reply(address(16385)); CHECK(responses == 1);
    now = 100; dispatch(address(16386), "getchallenge"); CHECK(challenges == 11);
}
int main(void)
{
    Dvar_Init(); com_dedicated = Dvar_RegisterInt("dedicated", 2, 0, 2, 0);
    registration();
#if defined(COD2_CODX) && COD2_CODX
    queries(); lan_and_disabled(); rcon_and_disconnect(); stats_and_time(); capacity();
    CHECK(hardeningCalls == 0);
#else
    reset(1000); for (int i = 0; i < 20; ++i) dispatch(remote, "getchallenge");
    CHECK(challenges == 20);
    for (int i = 0; i < 3; ++i) disconnect_reply(remote); CHECK(responses == 3);
#endif
    Dvar_Shutdown(); FINISH("production UDP operations");
}
