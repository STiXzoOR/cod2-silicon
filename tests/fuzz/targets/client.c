/* What a connecting or connected player can send a server (sv_client_mp.c).
 *
 * Mode byte bit 0 clear: a connect request. The rest is the text after
 * "connect ", tokenised by the real SV_Cmd_TokenizeString and given to the
 * real SV_DirectConnect: protocol, challenge, address and private-slot checks,
 * userinfo validation, slot choice and, on CoD2x, its admission rules. Bit 1
 * chooses a loopback sender, which skips the challenge table.
 * Bits 2-3 choose another connectionless request instead: 1, the text after
 * "getchallenge " for the real SV_GetChallenge, with a nearly full challenge
 * table; 2, the text after "ipAuthorize " for the real SV_AuthorizeIpPacket,
 * sent from the key server's address (any other is refused at once), bit 1
 * choosing whether the challenge it names exists.
 *
 * Mode byte bit 0 set: a message from a connected client, given to the real
 * SV_ExecuteClientMessage: reliable client commands (through the real command
 * table: userinfo, disconnect, the download commands), and the two move
 * packets with their delta-compressed user commands. Bits 1-2 choose the
 * client's state, bit 3 whether its server id is current, bit 4 whether the
 * rest is already Huffman-compressed (as on the wire) or plain, in which case
 * the harness compresses it so the fuzzer can change one field at a time.
 *
 * The game module, the file system and everything that sends are stubbed at
 * the harness boundary. ERR_DROP unwinds to the harness: dropping the sender
 * is the server's proper answer to a bad message. Any other engine error
 * reached from this input is a finding.
 */
#include "common_types.h"
#include "fuzz.h"

#include <stdarg.h>

extern void SV_ExecuteClientMessage(client_t *cl, msg_t *msg);
extern void SV_DirectConnect(netadr_t from);
extern void SV_GetChallenge(netadr_t from);
extern void SV_AuthorizeIpPacket(netadr_t from);
extern void SV_Cmd_TokenizeString(const char *text_in);
extern void MSG_Init(msg_t *buf, byte *data, int length);
extern int MSG_WriteBitsCompress(byte *from, byte *to, int size);

#define FUZZ_MAXCLIENTS 4
#define FUZZ_CHALLENGE 12345
/* The longest connectionless line a server reads: MSG_ReadStringLine stops at 1023 characters. */
#define FUZZ_LINE 1023

/* Real engine globals the server paths read directly. */
serverStatic_t svs;
server_t sv;
int sv_serverId_value;
loopback_t loopbacks[2];
FUZZ_KBITMASK;

FUZZ_DVAR(sv_maxclients);
FUZZ_DVAR(sv_privateClients);
FUZZ_DVAR(sv_privatePassword);
FUZZ_DVAR(sv_minPing);
FUZZ_DVAR(sv_maxPing);
FUZZ_DVAR(sv_pure);
FUZZ_DVAR(sv_floodProtect);
FUZZ_DVAR(sv_showCommands);
FUZZ_DVAR(sv_reconnectlimit);
FUZZ_DVAR(sv_kickBanTime);
FUZZ_DVAR(com_dedicated);
FUZZ_DVAR(com_sv_running);
FUZZ_DVAR(cl_shownet);
FUZZ_DVAR(net_profile);
FUZZ_DVAR(net_showprofile);
FUZZ_DVAR(showpackets);

/* imp_ indirection cells: each holds the address of the variable of that name. */
void *imp_svs = &svs;
void *imp_sv_serverId_value = &sv_serverId_value;
void *imp_sv_privateClients = &sv_privateClients;
void *imp_sv_privatePassword = &sv_privatePassword;
void *imp_sv_minPing = &sv_minPing;
void *imp_sv_maxPing = &sv_maxPing;
void *imp_sv_pure = &sv_pure;
void *imp_sv_floodProtect = &sv_floodProtect;
void *imp_sv_showCommands = &sv_showCommands;
void *imp_sv_reconnectlimit = &sv_reconnectlimit;
void *imp_sv_kickBanTime = &sv_kickBanTime;
void *imp_com_dedicated = &com_dedicated;
/* The engine reaches this struct through a cell holding its address. */
static LegacyHacks legacyHacks;
static LegacyHacks *legacyHacksAddress = &legacyHacks;
void *imp_legacyHacks = &legacyHacksAddress;

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int code, const char *fmt, ...) { FuzzEngineError(code, fmt); }

/* Platform. */
int Sys_Milliseconds(void) { return (int)svs.time; }
Bool Sys_SendPacket(int length, const void *data, netadr_t to) { (void)length; (void)data; (void)to; return 1; }
qboolean Sys_IsLANAddress(netadr_t adr) { return adr.type == NA_LOOPBACK || adr.type == NA_BOT; }
void Z_FreeInternal(void *p) { free(p); }
void *Sys_GetValue(int index)
{
    static va_info_t va;
    static char other[4096];
    return index == 1 ? (void *)&va : (void *)other;
}

/* LargeLocal: a 4-byte marker into a LIFO allocator in the engine; here a
 * stack of heap blocks, so the sanitizer watches each one. */
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
void *LargeLocal_GetBuf(const LargeLocal *ll) { return large_local_stack[ll->_placeholder]; }
void ZN10LargeLocalD1Ev(LargeLocal *ll)
{
    free(large_local_stack[ll->_placeholder]);
    large_local_stack[ll->_placeholder] = NULL;
    if (ll->_placeholder == large_local_top - 1)
        --large_local_top;
}
/* An ERR_DROP leaves a function without running its LargeLocal destructors. */
static void LargeLocalReset(void)
{
    while (large_local_top > 0) {
        free(large_local_stack[--large_local_top]);
        large_local_stack[large_local_top] = NULL;
    }
}

/* The game module: every call is accepted and does nothing. */
static gentity_t *entities;
static playerState_t playerState;
char *ClientConnect(int clientNum, int scriptPersId) { (void)clientNum; (void)scriptPersId; return NULL; }
void ClientBegin(int clientNum) { (void)clientNum; }
void ClientCommand(int clientNum) { (void)clientNum; }
void ClientDisconnect(int clientNum) { (void)clientNum; }
void ClientThink(int clientNum) { (void)clientNum; }
void ClientUserinfoChanged(int clientNum) { (void)clientNum; }
void G_SetLastServerTime(int clientNum, int serverTime) { (void)clientNum; (void)serverTime; }
Bool BG_IsWeaponValid(const playerState_t *ps, int weaponIndex) { (void)ps; return weaponIndex >= 0 && weaponIndex < 64; }
gentity_t *SV_GentityNum(int num) { return &entities[num >= 0 && num < FUZZ_MAXCLIENTS ? num : 0]; }
playerState_t *SV_GameClientNum(int num) { (void)num; return &playerState; }
unsigned int Scr_AllocArray(void) { return 1; }
void SV_FreeClientScriptId(client_t *cl) { (void)cl; }
Bool SV_Loaded(void) { return 1; }
const char *SEH_StringEd_GetString(const char *key) { return key; }

/* Everything the server would send or look up elsewhere. */
void SV_SendServerCommand(client_t *cl, svscmd_type type, const char *fmt, ...) { (void)cl; (void)type; (void)fmt; }
void SV_SendMessageToClient(msg_t *msg, client_t *client) { (void)msg; (void)client; }
void SV_SendClientSnapshot(client_t *client) { (void)client; }
void SV_UpdateServerCommandsToClient(client_t *client, msg_t *msg) { (void)client; (void)msg; }
void SV_Heartbeat_f(void) {}
void SV_SetUserinfo(int index, const char *val)
{
    if (index >= 0 && index < FUZZ_MAXCLIENTS)
        snprintf(svs.clients[index].userinfo, sizeof(svs.clients[index].userinfo), "%s", val);
}
void FS_FCloseFile(fileHandle_t f) { (void)f; }
const char *FS_LoadedIwdPureChecksums(void) { return "1 2 3"; }

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* The newest revision, so CoD2x's own admission rules run on every connect. */
int Cod2x_ServerGameVersion(void) { return 7; }
int Cod2x_HostRevision(void) { return 7; }
const char *Cod2x_HostRelease(void) { return "1.4.7.1"; }
void SV_Cod2xShowPacket(int sent, netadr_t address, const void *text, int length, int size)
{
    (void)sent; (void)address; (void)text; (void)length; (void)size;
}
void SV_Cod2xStripTextCodes(const char *text, char *out, size_t size)
{
    if (size)
        snprintf(out, size, "%s", text);
}
#endif

/* The parser's tables name the engine's empty string (literals.S). */
const char str_002157b8[] = "";
/* No ban list on disk. */
int FS_ReadFile(const char *qpath, void **buffer) { (void)qpath; if (buffer) *buffer = NULL; return -1; }
void FS_FreeFile(void *buffer) { (void)buffer; }

/* For the challenge request and the key server's reply. Challenge numbers come from a fixed
   sequence, so an input always meets the same server, and no name is ever looked up. */
static unsigned fuzz_rand_state;
static int FuzzRand(void)
{
    fuzz_rand_state = fuzz_rand_state * 214013u + 2531011u;
    return (int)((fuzz_rand_state >> 16) & 0x7fff);
}
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
int Cod2x_CrtRand(void) { return FuzzRand(); }
int SV_Cod2xCracked(void) { return 0; }
int SV_Cod2xIsMasterAddress(netadr_t from) { (void)from; return 0; }
#else
int Com_CrtRand(void) { return FuzzRand(); }
const netadr_t *SV_MasterAddress(void)
{
    static netadr_t none;
    none.type = NA_BAD;
    return &none;
}
#endif
qboolean Sys_StringToAdr(const char *s, netadr_t *a) { (void)s; (void)a; return 0; }
Bool Dvar_GetBool(const char *name) { (void)name; return 0; }
const dvar_t *Dvar_RegisterString(const char *name, const char *value, int flags)
{
    static dvar_t var;
    (void)flags;
    FuzzDvarString(&var, name, value);
    return &var;
}
FUZZ_DVAR(net_lanauthorize);
void *imp_net_lanauthorize = &net_lanauthorize;

static void Setup(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    FuzzDvarInt(&sv_maxclients_value, "sv_maxclients", FUZZ_MAXCLIENTS);
    FuzzDvarInt(&sv_privateClients_value, "sv_privateClients", 1);
    FuzzDvarString(&sv_privatePassword_value, "sv_privatePassword", "private");
    FuzzDvarInt(&sv_minPing_value, "sv_minPing", 0);
    FuzzDvarInt(&sv_maxPing_value, "sv_maxPing", 0);
    FuzzDvarBool(&sv_pure_value, "sv_pure", 1);
    FuzzDvarBool(&sv_floodProtect_value, "sv_floodProtect", 1);
    FuzzDvarBool(&sv_showCommands_value, "sv_showCommands", 0);
    FuzzDvarInt(&sv_reconnectlimit_value, "sv_reconnectlimit", 3);
    FuzzDvarInt(&sv_kickBanTime_value, "sv_kickBanTime", 300);
    FuzzDvarInt(&com_dedicated_value, "dedicated", 2);
    FuzzDvarBool(&com_sv_running_value, "sv_running", 1);
    FuzzDvarInt(&cl_shownet_value, "cl_shownet", 0);
    FuzzDvarInt(&net_profile_value, "net_profile", 0);
    FuzzDvarInt(&net_showprofile_value, "net_showprofile", 0);
    FuzzDvarBool(&showpackets_value, "showpackets", 0);
    FuzzDvarBool(&net_lanauthorize_value, "net_lanauthorize", 0);
    {
        /* The first MSG_Init builds the Huffman tables the compressor needs. */
        static byte scratch[8];
        msg_t first;
        MSG_Init(&first, scratch, sizeof(scratch));
    }
    /* A loaded map: every configstring is at least the empty string. */
    for (size_t i = 0; i < sizeof(sv.configstrings) / sizeof(sv.configstrings[0]); ++i)
        sv.configstrings[i] = (char *)"";
    sv.configstrings[0] = (char *)"\\sv_hostname\\fuzz\\mapname\\mp_toujane";
    svs.clients = calloc(FUZZ_MAXCLIENTS, sizeof(client_t));
    entities = calloc(FUZZ_MAXCLIENTS, sizeof(gentity_t));
    if (!svs.clients || !entities)
        abort();
}

static void Connect(const uint8_t *data, size_t size, int loopback)
{
    static char line[FUZZ_LINE + 1];
    netadr_t from;
    size_t length = size < FUZZ_LINE - 8 ? size : FUZZ_LINE - 8;

    memset(&from, 0, sizeof(from));
    from.type = loopback ? NA_LOOPBACK : NA_IP;
    from.ip[0] = 10;
    from.ip[3] = 7;
    /* A loopback address carries the local client's number there, never a port. */
    from.port = loopback ? 0 : 0x1234;
    /* The challenge a getchallenge from this address would have been given. */
    svs.challenges[0].adr = from;
    svs.challenges[0].challenge = FUZZ_CHALLENGE;
    svs.challenges[0].time = svs.time - 40;
    svs.challenges[0].pingTime = svs.time - 40;

    memcpy(line, "connect ", 8);
    memcpy(line + 8, data, length);
    line[8 + length] = '\0';
    SV_Cmd_TokenizeString(line);
    SV_DirectConnect(from);
}

/* The key server the fuzzed server believes in. Set before either handler runs, so neither
   ever looks a name up. */
static netadr_t KeyServer(void)
{
    netadr_t adr;
    memset(&adr, 0, sizeof(adr));
    adr.type = NA_IP;
    adr.ip[0] = 10;
    adr.ip[3] = 99;
    adr.port = 0x5cd0;
    svs.authorizeAddress = adr;
    return adr;
}

/* "getchallenge [...]": a slot in the challenge table, and for an address that is not on the
   local network, the request to the key server. */
static void Challenge(const uint8_t *data, size_t size, int loopback)
{
    static char line[FUZZ_LINE + 1];
    netadr_t from;
    size_t length = size < FUZZ_LINE - 13 ? size : FUZZ_LINE - 13;

    KeyServer();
    memset(&from, 0, sizeof(from));
    from.type = loopback ? NA_LOOPBACK : NA_IP;
    from.ip[0] = 10;
    from.ip[3] = 7;
    from.port = loopback ? 0 : 0x1234;
    /* A table that is nearly full: the oldest slot is the one reused. */
    for (int i = 0; i < 1024; ++i) {
        svs.challenges[i].adr.type = NA_IP;
        svs.challenges[i].adr.ip[0] = 11;
        svs.challenges[i].adr.port = (unsigned short)i;
        svs.challenges[i].time = svs.time - 1000 - (i * 7) % 900;
    }
    memcpy(line, "getchallenge ", 13);
    memcpy(line + 13, data, length);
    line[13 + length] = '\0';
    SV_Cmd_TokenizeString(line);
    SV_GetChallenge(from);
}

/* "ipAuthorize <challenge> <status> <reason> <guid>": the key server's verdict on a player.
   Only the key server's address gets past the first test, so the packet comes from it. */
static void Authorize(const uint8_t *data, size_t size, int known)
{
    static char line[FUZZ_LINE + 1];
    netadr_t from = KeyServer();
    size_t length = size < FUZZ_LINE - 12 ? size : FUZZ_LINE - 12;

    svs.challenges[3].adr.type = NA_IP;
    svs.challenges[3].adr.ip[0] = 10;
    svs.challenges[3].adr.ip[3] = 7;
    svs.challenges[3].adr.port = 0x1234;
    svs.challenges[3].challenge = known ? FUZZ_CHALLENGE : 1;
    svs.challenges[3].time = svs.time - 40;
    svs.tempBans[2].guid = 777;
    svs.tempBans[2].banTime = svs.time - 1000;
    memcpy(line, "ipAuthorize ", 12);
    memcpy(line + 12, data, length);
    line[12 + length] = '\0';
    SV_Cmd_TokenizeString(line);
    SV_AuthorizeIpPacket(from);
}

static void Message(const uint8_t *data, size_t size, int mode)
{
    static byte *wire;
    static byte *plain;
    client_t *cl = &svs.clients[1];
    msg_t msg;
    int length;

    if (!wire) {
        wire = malloc(MAX_MSGLEN);
        plain = malloc(MAX_MSGLEN);
        if (!wire || !plain)
            abort();
    }
    cl->state = 2 + ((mode >> 1) & 3) % 3; /* connected, primed or active */
    cl->serverId = (mode & 8) ? sv_serverId_value : sv_serverId_value + 16;
    cl->netchan.remoteAddress.type = NA_IP;
    cl->gentity = (void *)&entities[1];
    cl->messageAcknowledge = 4;
    cl->gamestateMessageNum = 2;
    snprintf(cl->name, sizeof(cl->name), "fuzz");
    snprintf(cl->userinfo, sizeof(cl->userinfo), "\\name\\fuzz\\rate\\25000\\snaps\\20");

    if (mode & 16) {
        length = (int)(size < MAX_MSGLEN ? size : MAX_MSGLEN);
        memcpy(wire, data, (size_t)length);
    } else {
        /* Huffman output can be longer than its input; keep the input small enough to fit. */
        int count = (int)(size < MAX_MSGLEN / 4 ? size : MAX_MSGLEN / 4);
        memcpy(plain, data, (size_t)count);
        length = MSG_WriteBitsCompress(plain, wire, count);
        if (length < 0 || length > MAX_MSGLEN)
            abort();
    }
    MSG_Init(&msg, wire, MAX_MSGLEN);
    msg.cursize = length;
    SV_ExecuteClientMessage(cl, &msg);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int mode;

    Setup();
    if (size < 2)
        return 0;
    mode = data[0];
    ++data;
    --size;

    /* The same server for every input: nothing carries over. */
    svs.time = 100000;
    sv_serverId_value = 0x30;
    memset(svs.clients, 0, FUZZ_MAXCLIENTS * sizeof(client_t));
    memset(svs.challenges, 0, sizeof(svs.challenges));
    memset(entities, 0, FUZZ_MAXCLIENTS * sizeof(gentity_t));
    memset(&legacyHacks, 0, sizeof(legacyHacks));

    fuzz_rand_state = 1;
    fuzz_error_armed = 1;
    fuzz_error_allow_drop = 1;
    if (!setjmp(fuzz_error_jump)) {
        if (mode & 1)
            Message(data, size, mode);
        else if ((mode & 12) == 4)
            Challenge(data, size, mode & 2);
        else if ((mode & 12) == 8)
            Authorize(data, size, mode & 2);
        else
            Connect(data, size, mode & 2);
    }
    fuzz_error_armed = 0;
    LargeLocalReset();
    return 0;
}
