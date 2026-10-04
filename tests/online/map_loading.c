#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static clientConnection_t connection;
const clientConnection_t *clc = &connection;
clientStatic_t cls;
int clc_x64_lastChallenge;
static int qport;
void *imp_g_qport = &qport;
static char buffer[MAX_MSGLEN], command[64];
static int setups, screens;
void LargeLocal_LargeLocal(const LargeLocal *ll, int size) { (void)ll; assert(size == sizeof(buffer)); }
void *LargeLocal_GetBuf(const LargeLocal *ll) { (void)ll; return buffer; }
void ZN10LargeLocalD1Ev(LargeLocal *ll) { (void)ll; }
void MSG_BeginReading(msg_t *msg) { msg->readcount = 0; }
int MSG_ReadLong(msg_t *msg) { msg->readcount += 4; return -1; }
char *MSG_ReadStringLine(msg_t *msg)
{
    static char lines[4][128]; static unsigned slot;
    char *out = lines[slot++ & 3]; int n = 0;
    while (msg->readcount < msg->cursize) {
        char c = msg->data[msg->readcount++];
        if (!c || c == '\n') break;
        assert(n < 127); out[n++] = c;
    }
    out[n] = 0; return out;
}
char *MSG_ReadBigString(msg_t *msg) { return MSG_ReadStringLine(msg); }
void CL_Netchan_AddOOBProfilePacket(int size) { (void)size; }
void Cmd_TokenizeString(const char *s) { snprintf(command, sizeof(command), "%s", s); }
const char *Cmd_Argv(int n) { assert(!n); return command; }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int I_strnicmp(const char *a, const char *b, int n) { return strncasecmp(a, b, n); }
int I_strncmp(const char *a, const char *b, int n) { return strncmp(a, b, n); }
void I_strncpyz(char *a, const char *b, int n) { snprintf(a, n, "%s", b); }
qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b) { return !memcmp(a.ip, b.ip, 4); }
qboolean NET_CompareAdr(netadr_t a, netadr_t b) { return NET_CompareBaseAdr(a,b) && a.port == b.port; }
const char *NET_AdrToString(netadr_t a) { (void)a; return "fixture"; }
void Netchan_Setup(netsrc_t sock, netchan_t *chan, netadr_t a, int port) { (void)sock; (void)chan; (void)a; (void)port; }
Bool NET_OutOfBandPrint(netsrc_t sock, netadr_t a, const char *s) { (void)sock; (void)a; (void)s; return 1; }
void Com_Printf(const char *s, ...) { (void)s; }
void Com_DPrintf(const char *s, ...) { (void)s; }
void Com_Error(int code, const char *s, ...) { (void)code; (void)s; assert(0); }
void Com_PrintMessage(int channel, const char *s) { (void)channel; (void)s; }
int Com_sprintf(char *out, int n, const char *s, ...) { (void)s; *out = 0; return n; }
char *va(const char *s, ...) { (void)s; return "fixture"; }
void PB_HandleClientOobPacket(netadr_t *a, msg_t *m) { (void)a; (void)m; }
void CL_VoicePacket(msg_t *m) { (void)m; }
void CL_ServerInfoPacket(netadr_t a, msg_t *m, int t) { (void)a; (void)m; (void)t; }
void CL_ServerStatusResponse(netadr_t a, msg_t *m) { (void)a; (void)m; }
void CL_ServersResponsePacket(netadr_t a, msg_t *m) { (void)a; (void)m; }
const char *SEH_LocalizeTextMessage(const char *a, const char *b, int c) { (void)b; (void)c; return a; }
void CL_RequestAuthorization(void) {}
void CL_SetupForNewServerMap(const char *map, const char *game)
{ assert(!strcmp(map,"mp_carentan") && !strcmp(game,"dm")); ++setups; }
void UI_DrawConnectScreen(void) { assert(connection.state == CA_CONNECTED); ++screens; }
#include "map_loading_source.h"
int main(void)
{
    byte packet[] = "xxxxloadingnewmap\nmp_carentan\ndm\n";
    netadr_t server = {0}; server.ip[0] = 127; server.ip[3] = 1;
    connection.serverAddress = server;
    for (int state = CA_CONNECTED; state <= CA_ACTIVE; ++state) {
        connection.state = state;
        msg_t msg = {.data=packet, .cursize=sizeof(packet)};
        assert(CL_ConnectionlessPacket(server, &msg, 0));
        assert(connection.state == CA_CONNECTED);
    }
    int before = setups;
    connection.state = CA_ACTIVE; server.ip[3] = 2;
    msg_t msg = {.data=packet, .cursize=sizeof(packet)};
    assert(CL_ConnectionlessPacket(server, &msg, 0));
    assert(connection.state == CA_ACTIVE && setups == before && screens == before);
    puts("online: map loading resumes connected packet flow, rejects another address");
}
