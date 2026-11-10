#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_server.h"
#include <stdio.h>
#include <string.h>

extern void Com_Printf(const char *fmt, ...);
extern void Com_DPrintf(const char *fmt, ...);
extern const char *va(const char *fmt, ...);
extern const char *NET_AdrToString(netadr_t a);
extern qboolean NET_StringToAdr(const char *s, netadr_t *a);
extern qboolean NET_CompareAdr(netadr_t a, netadr_t b);
extern qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
extern Bool NET_OutOfBandPrint(netsrc_t sock, netadr_t adr, const char *data);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern void Cmd_AddCommand(const char *cmdName, xcommand_t function);
extern char *SV_Cmd_Argv(int arg);
extern int SV_Cmd_Argc(void);
extern void SVC_Status(netadr_t from);
extern void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *ospath);
extern const dvar_t *fs_homepath;
extern char fs_gamedir[256];
extern const dvar_t *com_dedicated;
extern serverStatic_t svs;

/* Masters as on a CoD2x server: Activision and CoD2x fixed, a third for the admin. */
static const dvar_t *sv_cod2xMasters[COD2X_MASTER_COUNT];
static netadr_t sv_cod2xMasterAddresses[COD2X_MASTER_COUNT];
static const dvar_t *sv_cracked, *sv_showPacketStrings;
/* When to ask the CoD2x master for this server's public address; 0 when not. */
static int sv_cod2xIpTime;

static void SV_Cod2xRequestIp(void);

/* Removes ban.txt from the home folder of the running game. */
static void SV_Cod2xUnbanAll(void)
{
    char path[256];
    FS_BuildOSPath(fs_homepath->current.string, fs_gamedir, "ban.txt", path);
    if (remove(path) != -1)
        Com_Printf("All bans removed\n");
    else
        Com_Printf("Error removing bans\n");
}

void SV_Cod2xServerInit(void)
{
    sv_cod2xMasters[0] = Dvar_RegisterString("sv_master1", "cod2master.activision.com", 0x1040);
    sv_cod2xMasters[1] = Dvar_RegisterString("sv_master2", "master.cod2x.me", 0x1040);
    sv_cod2xMasters[2] = Dvar_RegisterString("sv_master3", "", 0x1000);
    sv_cracked = Dvar_RegisterBool("sv_cracked", 0, 0x1000);
    sv_showPacketStrings = Dvar_RegisterBool("showPacketStrings", 0, 0x1000);
    Cmd_AddCommand("unbanAll", SV_Cod2xUnbanAll);
    Cmd_AddCommand("getIp", SV_Cod2xRequestIp);
}

void SV_Cod2xServerSpawned(void)
{
    sv_cod2xIpTime = svs.time + 4000;
}

int SV_Cod2xCracked(void)
{
    return sv_cracked && sv_cracked->current.enabled;
}

/* Resolved once, and again after the dvar changes. A failed lookup leaves the
   address bad until then. Every master uses port 20710, as on CoD2x servers,
   even when the name carries another port. */
const netadr_t *SV_Cod2xMasterAddress(int index)
{
    dvar_t *master = (dvar_t *)sv_cod2xMasters[index];
    netadr_t *address = &sv_cod2xMasterAddresses[index];
    if (address->type == NA_BOT || master->modified) {
        master->modified = 0;
        Com_Printf("Resolving master server %s\n", master->current.string);
        if (!NET_StringToAdr(master->current.string, address)) {
            Com_Printf("Couldn't resolve address: %s\n", master->current.string);
        } else {
            if (strcmp(master->current.string, ":") && master->current.string[0])
                address->port = (unsigned short)((20710 >> 8) | (20710 << 8));
            Com_Printf("%s resolved to %s\n", master->current.string, NET_AdrToString(*address));
        }
    }
    return address;
}

int SV_Cod2xIsMasterAddress(netadr_t from)
{
    for (int i = 0; i < COD2X_MASTER_COUNT; ++i) {
        if (NET_CompareBaseAdr(from, *SV_Cod2xMasterAddress(i)))
            return 1;
    }
    return 0;
}

/* Asks the CoD2x master (only when a master dvar names it) for the address it
   sees this server at; the answer is getIpResponse. */
static void SV_Cod2xRequestIp(void)
{
    for (int i = 0; i < COD2X_MASTER_COUNT; ++i) {
        const netadr_t *address;
        if (strcmp(sv_cod2xMasters[i]->current.string, "master.cod2x.me"))
            continue;
        address = SV_Cod2xMasterAddress(i);
        if (address->type != NA_BAD)
            NET_OutOfBandPrint(NS_SERVER, *address, "getIp");
    }
}

void SV_Cod2xIpResponse(netadr_t from)
{
    sv_cod2xIpTime = 0;
    if (SV_Cod2xIsMasterAddress(from) && SV_Cmd_Argc() == 2)
        Com_Printf("Server IP: %s\n", SV_Cmd_Argv(1));
}

/* Public servers (dedicated 2) report to every master that is set: a heartbeat
   every 3 minutes and the status every 10. */
void SV_Cod2xMasterHeartbeat(const char *name)
{
    int i;
    if (!com_dedicated || com_dedicated->current.integer != 2)
        return;
    if (svs.time >= svs.nextHeartbeatTime) {
        svs.nextHeartbeatTime = svs.time + 180000;
        for (i = 0; i < COD2X_MASTER_COUNT; ++i) {
            const netadr_t *address;
            if (!sv_cod2xMasters[i]->current.string[0])
                continue;
            address = SV_Cod2xMasterAddress(i);
            if (address->type != NA_BAD) {
                Com_DPrintf("Sending heartbeat to %s\n", sv_cod2xMasters[i]->current.string);
                NET_OutOfBandPrint(NS_SERVER, *address, va("heartbeat %s\n", name));
            }
        }
    }
    if (svs.time >= svs.nextStatusResponseTime) {
        svs.nextStatusResponseTime = svs.time + 600000;
        for (i = 0; i < COD2X_MASTER_COUNT; ++i) {
            const netadr_t *address;
            if (!sv_cod2xMasters[i]->current.string[0])
                continue;
            address = SV_Cod2xMasterAddress(i);
            if (address->type != NA_BAD)
                SVC_Status(*address);
        }
    }
    if (svs.time >= sv_cod2xIpTime && sv_cod2xIpTime > 0) {
        sv_cod2xIpTime = 0;
        SV_Cod2xRequestIp();
    }
}

/* showPacketStrings: connectionless packets in both directions, escaped. */
static const char *SV_Cod2xAddressType(int type)
{
    switch (type) {
    case 0: return "INIT";
    case 1: return "BAD";
    case 2: return "LOOPBACK";
    case 3: return "BROADCAST";
    case 4: return "IP";
    case 5: return "IPX";
    case 6: return "BROADCAST_IPX";
    default: return "UNKNOWN";
    }
}

static void SV_Cod2xEscape(char *out, size_t size, const unsigned char *data, int length)
{
    size_t used = 0;
    for (int i = 0; i < length && used < size - 5; ++i) {
        unsigned char c = data[i];
        if (c == '\n' || c == '\r' || c == '\t') {
            out[used++] = '\\';
            out[used++] = c == '\n' ? 'n' : c == '\r' ? 'r' : 't';
        } else if (c < 32 || c > 126) {
            snprintf(out + used, 5, "\\x%02X", c);
            used += 4;
        } else {
            out[used++] = (char)c;
        }
    }
    out[used] = '\0';
}

static void SV_Cod2xPacketPeer(char *out, size_t size, netadr_t address)
{
    out[0] = '\0';
    if (NET_CompareAdr(address, svs.authorizeAddress)) {
        snprintf(out, size, "%s (authorize)", "cod2master.activision.com");
        return;
    }
    for (int i = 0; i < COD2X_MASTER_COUNT; ++i) {
        if (sv_cod2xMasters[i] && NET_CompareAdr(address, sv_cod2xMasterAddresses[i])) {
            snprintf(out, size, "%s (sv_master%i)", sv_cod2xMasters[i]->current.string, i + 1);
            return;
        }
    }
}

void SV_Cod2xShowPacket(int sent, netadr_t address, const void *text, int length, int size)
{
    char escaped[1024], peer[256];
    if (!sv_showPacketStrings || !sv_showPacketStrings->current.enabled)
        return;
    SV_Cod2xEscape(escaped, sizeof(escaped), text, length);
    SV_Cod2xPacketPeer(peer, sizeof(peer), address);
    if (sent)
        Com_Printf("NET_SendPacket: %s %s %i %s\n  < '%s'\n\n", NET_AdrToString(address),
                   SV_Cod2xAddressType(address.type), size, peer, escaped);
    else
        Com_Printf("SV_ConnectionlessPacket: %s %s %i %s\n  > '%s'\n\n", NET_AdrToString(address),
                   SV_Cod2xAddressType(address.type), size, peer, escaped);
}
#endif
