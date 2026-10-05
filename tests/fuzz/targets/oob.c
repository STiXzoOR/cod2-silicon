/* Connectionless (out-of-band) server packet handling.
 *
 * Drives SV_ConnectionlessPacket (sv_main_mp.c) with arbitrary packet bodies,
 * the dispatch a server runs for getstatus, getinfo, getchallenge, connect,
 * rcon, ipAuthorize and voice before a client is authenticated. The real
 * tokeniser, info-string builders, rcon command assembly and voice reader run;
 * the game, filesystem, challenge and direct-connect layers are stubbed at the
 * harness boundary so the fuzzer stays on the parsing surface. Replies go to a
 * sink. svs.clients is a small real array so the status/info client loops run.
 */
#include "common_types.h"
#include "fuzz.h"

#include <stdarg.h>

extern void SV_ConnectionlessPacket(netadr_t from, msg_t *msg);
extern void MSG_Init(msg_t *buf, byte *data, int length);

#define FUZZ_MAXCLIENTS 4

/* Real engine globals the server paths read directly. */
serverStatic_t svs;
server_t sv;
int dvar_modifiedFlags;
qboolean gameInitialized;

FUZZ_DVAR(sv_maxclients);
FUZZ_DVAR(sv_privateClients);
FUZZ_DVAR(sv_hostname);
FUZZ_DVAR(sv_mapname);
FUZZ_DVAR(sv_gametype);
FUZZ_DVAR(sv_minPing);
FUZZ_DVAR(sv_maxPing);
FUZZ_DVAR(sv_pure);
FUZZ_DVAR(sv_voice);
FUZZ_DVAR(sv_allowAnonymous);
FUZZ_DVAR(sv_disableClientConsole);
FUZZ_DVAR(com_dedicated);
FUZZ_DVAR(rcon_password);
FUZZ_DVAR(sv_packet_info);

/* imp_ indirection cells the status/info paths dereference. */
int fs_numServerIwds;
void *imp_fs_numServerIwds = &fs_numServerIwds;
void *imp_svs = &svs;
static LegacyHacks legacyHacks;
void *imp_legacyHacks = &legacyHacks;

FUZZ_DVAR(com_sv_running);
FUZZ_DVAR(com_developer);
FUZZ_DVAR(com_logfile);
FUZZ_DVAR(net_profile);
FUZZ_DVAR(net_showprofile);
FUZZ_DVAR(showpackets);
FUZZ_DVAR(showdrop);
FUZZ_DVAR(packetDebug);
void *imp_com_sv_running = &com_sv_running_value;
int com_errorEntered;
int com_fixedConsolePosition;
loopback_t loopbacks[2];

/* common.c's log/console/error leaves, kept inert. */
int FS_Initialized(void) { return 0; }
int FS_Write(const void *b, int n, int h) { (void)b; (void)n; (void)h; return n; }
void FS_Flush(int h) { (void)h; }
int FS_FOpenTextFileWrite(const char *name) { (void)name; return 0; }
void Sys_Print(const char *msg) { (void)msg; }
void Sys_Error(const char *fmt, ...) { (void)fmt; abort(); }
sysEvent_t Sys_GetEvent(void) { sysEvent_t e; memset(&e, 0, sizeof(e)); return e; }
void Z_FreeInternal(void *p) { free(p); }
Bool Sys_SendPacket(int length, const void *data, netadr_t to) { (void)length; (void)data; (void)to; return 1; }

/* Command execution is reached from rcon; the command table stays empty, so
 * Cmd_ExecuteString tokenises and looks up nothing. These close the lookup. */
int Dvar_Command(void) { return 0; }
qboolean SV_GameCommand(void) { return 0; }
qboolean CL_GameCommand(void) { return 0; }

/* va() (q_shared.c) stores into Sys_GetValue(1)->va_string[2][1024]; the slot
 * must be a real va_info_t. Index 2 (g_com_error) wants a jmp_buf-sized cell. */
void *Sys_GetValue(int index)
{
    static va_info_t va;
    static char other[4096];
    return index == 1 ? (void *)&va : (void *)other;
}

static void Reply(const char *tag)
{
    (void)tag;
}

int Sys_Milliseconds(void) { return (int)svs.time; }

/* LargeLocal: the status buffers. The real object is a 4-byte marker into a
 * LIFO bump allocator, so the harness mirrors that with a stack of heap blocks
 * (ASan-watched) and stores only the slot index in the object. */
static void *large_local_stack[16];
static int large_local_top;
void LargeLocal_LargeLocal(LargeLocal *ll, int size)
{
    if (large_local_top >= 16)
        abort();
    large_local_stack[large_local_top] = malloc(size > 0 ? (size_t)size : 1);
    if (!large_local_stack[large_local_top])
        abort();
    ll->_placeholder = large_local_top++;
}
void *LargeLocal_GetBuf(const LargeLocal *ll)
{
    return large_local_stack[ll->_placeholder];
}
void ZN10LargeLocalD1Ev(LargeLocal *ll)
{
    free(large_local_stack[ll->_placeholder]);
    large_local_stack[ll->_placeholder] = NULL;
    if (ll->_placeholder == large_local_top - 1)
        --large_local_top;
}

/* NET_OutOfBandPrint, NET_AdrToString and NET_CompareBaseAdr are the real
 * net_chan_mp.c functions; replies reach Sys_SendPacket, stubbed above. */

/* Dvar accessors used by the status/info builders. */
Bool Dvar_GetBool(const char *name) { (void)name; return 0; }
int Dvar_GetInt(const char *name) { (void)name; return 0; }
const char *Dvar_GetString(const char *name) { (void)name; return ""; }
const char *Dvar_InfoString(int bit) { (void)bit; return "\\sv_test\\1"; }
const dvar_t *Dvar_FindVar(const char *name) { (void)name; return 0; }

/* Server helpers outside the connectionless parsing surface. */
playerState_t *SV_GameClientNum(int num) { (void)num; static playerState_t ps; return &ps; }
int G_GetClientScore(int n) { (void)n; return 0; }
qboolean FS_iwIwd(char *iwd, char *base) { (void)iwd; (void)base; return 1; }
void PB_HandleServerOobPacket(netadr_t *from, msg_t *msg) { (void)from; (void)msg; }

/* The paths reached only from a real connection; stubbed so the dispatch
 * returns to the fuzzer without entering the game or challenge code. */
void SV_GetChallenge(netadr_t from) { (void)from; Reply("challenge"); }
void SV_DirectConnect(netadr_t from) { (void)from; Reply("connect"); }
void SV_AuthorizeIpPacket(netadr_t from) { (void)from; Reply("auth"); }

/* Group voice broadcast is game-side; the harness only exercises the reader. */
void G_BroadcastVoice(gentity_t *talker, VoicePacket_t *packet) { (void)talker; (void)packet; }

static void SetupDvars(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    FuzzDvarInt(&sv_maxclients_value, "sv_maxclients", FUZZ_MAXCLIENTS);
    FuzzDvarInt(&sv_privateClients_value, "sv_privateClients", 1);
    FuzzDvarString(&sv_hostname_value, "sv_hostname", "fuzz");
    FuzzDvarString(&sv_mapname_value, "sv_mapname", "mp_toujane");
    FuzzDvarString(&sv_gametype_value, "sv_gametype", "sd");
    FuzzDvarInt(&sv_minPing_value, "sv_minPing", 0);
    FuzzDvarInt(&sv_maxPing_value, "sv_maxPing", 0);
    FuzzDvarBool(&sv_pure_value, "sv_pure", 0);
    FuzzDvarBool(&sv_voice_value, "sv_voice", 1);
    FuzzDvarBool(&sv_allowAnonymous_value, "sv_allowAnonymous", 0);
    FuzzDvarBool(&sv_disableClientConsole_value, "sv_disableClientConsole", 0);
    FuzzDvarInt(&com_dedicated_value, "dedicated", 2);
    FuzzDvarString(&rcon_password_value, "rcon_password", "secret");
    FuzzDvarBool(&sv_packet_info_value, "sv_packet_info", 0);
    FuzzDvarBool(&com_sv_running_value, "sv_running", 1);
    FuzzDvarBool(&com_developer_value, "developer", 0);
    FuzzDvarInt(&com_logfile_value, "logfile", 0);
    FuzzDvarInt(&net_profile_value, "net_profile", 0);
    FuzzDvarInt(&net_showprofile_value, "net_showprofile", 0);
    FuzzDvarBool(&showpackets_value, "showpackets", 0);
    FuzzDvarBool(&showdrop_value, "showdrop", 0);
    FuzzDvarBool(&packetDebug_value, "packetDebug", 0);
    svs.clients = calloc(FUZZ_MAXCLIENTS, sizeof(client_t));
    if (!svs.clients)
        abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static byte *buf;
    msg_t msg;
    netadr_t from;
    int i;

    SetupDvars();
    if (!buf)
        buf = malloc(MAX_MSGLEN);
    if (!size || size > MAX_MSGLEN - 4)
        return 0;

    svs.time = 1000;
    /* Reset the client slots each run so state never carries between inputs. */
    memset(svs.clients, 0, FUZZ_MAXCLIENTS * sizeof(client_t));
    for (i = 0; i < FUZZ_MAXCLIENTS; ++i) {
        svs.clients[i].netchan.remoteAddress.type = NA_IP;
    }

    memset(&from, 0, sizeof(from));
    from.type = NA_IP;

    /* SV_ConnectionlessPacket reads a leading -1 long, then the command line. */
    *(int *)buf = -1;
    memcpy(buf + 4, data, size);
    MSG_Init(&msg, buf, MAX_MSGLEN);
    msg.cursize = (int)size + 4;

    SV_ConnectionlessPacket(from, &msg);
    return 0;
}
