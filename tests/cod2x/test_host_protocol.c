#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int revision, failures, argc, connects, banned;
static const char *argv[8];
static char packet[32768];
static netadr_t recipient;
static client_t clients[2];
static gentity_t entities[2];
serverStatic_t svs;
void *imp_svs = &svs;
static dvar_t settings[24];
static const dvar_t *pointers[24];
const dvar_t *sv_maxclients = &settings[0], *sv_hostname = &settings[1];
const dvar_t *sv_mapname = &settings[2], *sv_privateClients = &settings[3];
const dvar_t *sv_gametype = &settings[4], *sv_pure = &settings[5];
const dvar_t *sv_allowAnonymous = &settings[6], *sv_disableClientConsole = &settings[7];
const dvar_t *com_dedicated = &settings[8];
const dvar_t *rcon_password = &settings[9];
const dvar_t *sv_minPing = &settings[12], *sv_maxPing = &settings[13];
const dvar_t *sv_voice = &settings[16];
void *imp_net_lanauthorize = &pointers[10], *imp_sv_reconnectlimit = &pointers[11];
void *imp_sv_minPing = &pointers[12], *imp_sv_maxPing = &pointers[13];
void *imp_sv_privatePassword = &pointers[14], *imp_sv_privateClients = &pointers[3];
void *imp_sv_kickBanTime = &pointers[15], *imp_com_dedicated = &pointers[8];
static int numIwds;
void *imp_fs_numServerIwds = &numIwds;
qboolean gameInitialized;

char *Info_ValueForKey(const char *, const char *);
void Info_SetValueForKey(char *, const char *, const char *);
int Cod2x_ServerGameVersion(void) { return revision; }
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) { return a.type == b.type && !memcmp(a.ip, b.ip, 4); }
qboolean NET_CompareAdr(netadr_t a, netadr_t b) { return NET_CompareBaseAdr(a, b) && a.port == b.port; }
qboolean NET_IsLocalAddress(netadr_t a) { return a.type == NA_LOOPBACK || a.type == NA_BOT; }
qboolean Sys_IsLANAddress(netadr_t a) { return NET_IsLocalAddress(a) || a.ip[0] == 192; }
const char *NET_AdrToString(netadr_t a) { (void)a; return "fixture address"; }
qboolean NET_StringToAdr(const char *s, netadr_t *a) { (void)s; a->type = NA_IP; a->ip[0] = 10; return 1; }
Bool NET_OutOfBandPrint(netsrc_t s, netadr_t a, const char *p) { (void)s; recipient = a; snprintf(packet, sizeof(packet), "%s", p); return 1; }
static netadr_t master = {NA_IP, {10,0,0,2}, 123};
const netadr_t *SV_MasterAddress(void) { return &master; }
const char *va(const char *format, ...);
void Com_Printf(const char *format, ...) { (void)format; }
void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
void *Sys_GetValue(int index) { assert(index == 1); static va_info_t info; return &info; }
char *SV_Cmd_Argv(int n) { return (char *)(n < argc ? argv[n] : ""); }
int SV_Cmd_Argc(void) { return argc; }
int I_stricmp(const char *a, const char *b);
void I_strncpyz(char *a, const char *b, int n);
Bool Dvar_GetBool(const char *name) { (void)name; return 0; }
int Dvar_GetInt(const char *name) { (void)name; return 0; }
const char *Dvar_GetString(const char *name) { (void)name; return ""; }
const dvar_t *Dvar_RegisterString(const char *n, const char *v, int f) { (void)n; (void)v; (void)f; static dvar_t d; d.current.string=""; return &d; }
const char *Dvar_InfoString(int bit) { (void)bit; return "\\protocol\\118\\hostname\\fixture"; }
int SV_IsBannedGuid(int guid) { return banned && guid == -2033168971; }
void Netchan_Setup(netsrc_t s, netchan_t *n, netadr_t a, int q) { (void)s; n->remoteAddress=a; n->qport=q; }
gentity_t *SV_GentityNum(int n) { return &entities[n]; }
unsigned int Scr_AllocArray(void) { return 1; }
char *ClientConnect(int n, int id) { (void)n; (void)id; ++connects; return NULL; }
void SV_FreeClient(client_t *cl) { (void)cl; }
void SV_FreeClientScriptId(client_t *cl) { (void)cl; }
void SV_Heartbeat_f(void) {}
void SV_Cmd_TokenizeString(const char *s) { (void)s; }
qboolean FS_iwIwd(char *s, char *base) { (void)s; (void)base; return 1; }
playerState_t *SV_GameClientNum(int n) { (void)n; return NULL; }
int G_GetClientScore(int n) { (void)n; return 0; }
void LargeLocal_LargeLocal(const LargeLocal *l, int s) { (void)l; (void)s; }
void *LargeLocal_GetBuf(const LargeLocal *l) { (void)l; static char b[MAX_MSGLEN]; return b; }
void ZN10LargeLocalD1Ev(LargeLocal *l) { (void)l; }
int Com_sprintf(char *b, int n, const char *f, ...);
void I_strncat(char *b, int n, const char *s);

#if defined(COD2_CODX) && COD2_CODX && __has_include("PC/server_mp/cod2x_host_protocol.h")
#include "PC/server_mp/cod2x_host_protocol.h"
#endif
#include "host_protocol_source.h"

static void Verify(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

static void reset(void)
{
    memset(&svs,0,sizeof(svs)); memset(clients,0,sizeof(clients)); memset(settings,0,sizeof(settings));
    for (int i=0;i<24;++i) pointers[i]=&settings[i];
    svs.clients=clients; svs.time=1000; settings[0].current.integer=2;
    settings[1].current.string="fixture"; settings[2].current.string="mp_fixture";
    settings[4].current.string="dm"; settings[14].current.string="";
    settings[9].current.string=""; settings[15].current.value=10;
    packet[0]=0; argc=0; connects=0; banned=0;
}

static const netadr_t remote = {NA_IP, {8,7,6,5}, 100};
static const char *digest = "00112233445566778899aabbccddeeff";
static const char *hwid = "0123456789abcdef0123456789abcdef";

static void challenge_forwarding(void)
{
    reset(); revision=0; argc=3; argv[0]="getchallenge"; argv[1]="0"; argv[2]=digest;
    SV_GetChallenge(remote);
#if defined(COD2_CODX) && COD2_CODX
    Verify(strstr(packet,"PB \"00112233445566778899aabbccddeeff\"") != NULL, "real getchallenge forwards digest");
    Verify(svs.authorizeAddress.port == (unsigned short)((20700 >> 8) | (20700 << 8)), "CoD2x host authorization port");
#else
    Verify(strstr(packet,"PB ") == NULL, "stock getchallenge request unchanged");
#endif
#if defined(COD2_CODX) && COD2_CODX && WS69_HAS_CHALLENGE_METADATA
    int challenge = svs.challenges[0].challenge;
    Verify(SV_Cod2xChallengeDigest(remote, challenge) != NULL, "same challenge retains digest");
    netadr_t other = remote;
    ++other.port;
    Verify(SV_Cod2xChallengeDigest(other, challenge) == NULL, "digest cannot cross address/port");
    Verify(SV_Cod2xChallengeDigest(remote, challenge + 1) == NULL, "digest cannot cross challenge");
    svs.challenges[0].challenge = challenge + 1;
    Verify(SV_Cod2xChallengeDigest(remote, challenge) == NULL, "recycled challenge cannot expose old digest");
    svs.challenges[0].challenge = challenge;
    argv[2] = "00112233445566778899aabbccddee\"ff";
    SV_GetChallenge(remote);
    Verify(strstr(packet," PB ") == NULL, "malformed digest is never forwarded");
    Verify(SV_Cod2xChallengeDigest(remote, challenge) == NULL, "malformed retry clears prior digest");
    argv[2] = "00112233445566778899AABBCCDDEEFF";
    SV_GetChallenge(remote);
    Verify(strstr(packet," PB \"00112233445566778899AABBCCDDEEFF\"") != NULL, "uppercase hex digest forwards unchanged");
    argc = 1;
    SV_GetChallenge(remote);
    Verify(strstr(packet," PB ") == NULL, "missing digest creates no supplied identity");
#endif
}

static void prepare_connect(int host, int protocol, int extension, const char *id, netadr_t from)
{
    reset(); revision=host;
    static char info[1024];
    snprintf(info,sizeof(info),"\\protocol\\%d\\protocol_cod2x\\%d\\cl_hwid2\\%s\\challenge\\19\\qport\\20\\name\\fixture\\snaps\\40\\rate\\25000",protocol,extension,id);
    argc=2; argv[0]="connect"; argv[1]=info;
    svs.challenges[5].adr=from; svs.challenges[5].challenge=19; svs.challenges[5].guid=123; svs.challenges[5].pingTime=7;
}

static void connect_case(int host, int protocol, int extension, const char *id, netadr_t from, int expected, int expectedGuid)
{
    prepare_connect(host, protocol, extension, id, from);
    SV_DirectConnect(from);
    Verify(connects == expected, "production DirectConnect admission result");
    if (expected) Verify(clients[0].guid == expectedGuid, "production client GUID identity");
}

static void authorization_replies(void)
{
    static const struct { const char *status, *reason, *message; int retained; } cases[] = {
        {"deny", "BAD_CDKEY", "needcdkey", 0},
        {"deny", "CLIENT_UNKNOWN_TO_AUTH", "needcdkey", 0},
        {"deny", "INVALID_CDKEY", "error\nEXE_ERR_CDKEY_IN_USE", 0},
        {"deny", "BANNED_CDKEY", "error\nEXE_ERR_BAD_CDKEY", 0},
        {"demo", "", "error\nEXE_ERR_NOT_A_DEMO_SERVER", 0},
        {"unexpected", "", "error\nEXE_ERR_BAD_CDKEY", 0},
        {"accept", "KEY_IS_GOOD", "challengeResponse 19", 1}
    };
    for (int host = 0; host <= 6; host += 6) {
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            reset(); revision = host;
            svs.authorizeAddress = master;
            svs.challenges[5].adr = remote;
            svs.challenges[5].challenge = 19;
            svs.challenges[5].guid = -2033168971;
            argc = 5; argv[0] = "ipAuthorize"; argv[1] = "19";
            argv[2] = cases[i].status; argv[3] = cases[i].reason; argv[4] = "123";
            SV_AuthorizeIpPacket(master);
            Verify(!strcmp(packet, cases[i].message), "authorization response preserves acceptance/denial");
            Verify(svs.challenges[5].challenge == (cases[i].retained ? 19 : 0), "denied authorization clears challenge");
            if (!cases[i].retained) {
                char info[256];
                snprintf(info, sizeof(info), "\\protocol\\118\\protocol_cod2x\\6\\cl_hwid2\\%s\\challenge\\19", hwid);
                argc = 2; argv[0] = "connect"; argv[1] = info;
                SV_DirectConnect(remote);
                Verify(!connects && strstr(packet, "EXE_BAD_CHALLENGE"), "authorization denial cannot progress to ClientConnect");
            }
            if (cases[i].retained) {
#if defined(COD2_CODX) && COD2_CODX
                Verify(svs.challenges[5].guid == (host == 6 ? -2033168971 : 123), "late authorization cannot overwrite host HWID");
#else
                Verify(svs.challenges[5].guid == 123, "stock authorized GUID remains original");
#endif
            }
        }
    }
    reset(); revision = 6; svs.authorizeAddress = master;
    svs.challenges[5].adr = remote; svs.challenges[5].challenge = 19;
    argc = 5; argv[0] = "ipAuthorize"; argv[1] = "19"; argv[2] = "accept"; argv[3] = "KEY_IS_GOOD"; argv[4] = "123";
    SV_AuthorizeIpPacket(remote);
    Verify(!packet[0] && svs.challenges[5].guid == 0, "untrusted authorization source cannot accept");
    argv[1] = "20";
    SV_AuthorizeIpPacket(master);
    Verify(!packet[0] && svs.challenges[5].guid == 0, "unmatched authorization challenge cannot accept");
}

static void discovery(void)
{
    for (int host = 0; host <= 6; host += 6) {
        reset(); revision=host; argc=2; argv[0]="getinfo"; argv[1]="nonce";
        const char *expected = "118";
#if defined(COD2_CODX) && COD2_CODX
        if (host == 6) expected = "120";
#endif
        void (*responses[])(netadr_t) = {SVC_Info, SVC_Status, SVC_GameCompleteStatus};
        for (unsigned i = 0; i < sizeof(responses) / sizeof(responses[0]); ++i) {
            responses[i](remote);
            const char *info = strchr(packet, '\n');
            char line[8192] = "";
            if (info) {
                size_t n = strcspn(info + 1, "\n");
                assert(n < sizeof(line));
                memcpy(line, info + 1, n);
                line[n] = '\0';
            }
            Verify(info && !strcmp(Info_ValueForKey(line,"protocol"),expected), "discovery/status protocol follows owned host");
            Verify(info && !strcmp(Info_ValueForKey(line,"challenge"),"nonce"), "discovery challenge echoed");
        }
    }
}

static void admission_bans(void)
{
#if defined(COD2_CODX) && COD2_CODX
    for (int local = 0; local <= 1; ++local) {
        netadr_t from = remote;
        if (local) from.type = NA_LOOPBACK;
        for (int kind = 1; kind <= 2; ++kind) {
            prepare_connect(6,118,6,hwid,from);
            if (kind == 1) banned = 1;
            else { svs.tempBans[0].guid = -2033168971; svs.tempBans[0].banTime = 500; }
            SV_DirectConnect(from);
            Verify(connects == 0 && NET_CompareAdr(recipient,from), "HWID bans reject actual incoming address");
            Verify(strstr(packet,kind == 1 ? "permanently" : "temporarily") != NULL, "ban error class");
            if (!local) Verify(svs.challenges[5].challenge == 0, "banned remote challenge cleared");
        }
    }
    prepare_connect(6,118,6,hwid,remote);
    svs.tempBans[0].guid = -2033168971; svs.tempBans[0].banTime = -20000;
    SV_DirectConnect(remote);
    Verify(connects == 1, "expired HWID temp ban permits connection");
    prepare_connect(6,118,6,hwid,remote);
    svs.challenges[5].challenge = 20;
    SV_DirectConnect(remote);
    Verify(!connects && svs.challenges[5].guid == 123 && strstr(packet,"EXE_BAD_CHALLENGE"), "invalid challenge cannot alter GUID");
#endif
}

int main(void)
{
    challenge_forwarding();
    authorization_replies();
    admission_bans();
    connect_case(0,118,0,"",remote,1,123);
#if defined(COD2_CODX) && COD2_CODX
    connect_case(6,118,0,"",remote,0,0);
    connect_case(6,118,5,hwid,remote,0,0);
    connect_case(6,118,6,"short",remote,0,0);
    connect_case(6,118,6,"0123456789abcdef0123456789abcde",remote,0,0);
    connect_case(6,118,6,"0123456789abcdef0123456789abcdef0",remote,0,0);
    connect_case(6,120,6,hwid,remote,0,0);
    connect_case(6,118,6,hwid,remote,1,-2033168971);
    connect_case(6,118,7,hwid,remote,1,-2033168971);
    connect_case(6,118,6,"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",remote,1,1032264773);
    connect_case(6,118,6,"00000000000000000000000000000000",remote,1,-255302587);
    /* Independent FNV-1a zero vector: 28 'a' bytes, then 7c a6 75 82. */
    connect_case(6,118,6,"aaaaaaaaaaaaaaaaaaaaaaaaaaaa\x7c\xa6\x75\x82",remote,1,1);
    netadr_t local = remote;
    local.type = NA_LOOPBACK;
    connect_case(6,118,6,hwid,local,1,-2033168971);
    local.type = NA_BOT;
    connect_case(6,118,0,"",local,1,0);
#else
    connect_case(6,118,0,"",remote,1,123);
#endif
    discovery();
    printf("host admission/auth/discovery: %d failures\n",failures);
    return failures != 0;
}
