#include "common_types.h"
#include "imports.h"
#include "www_download.h"
#include "cod2_feature_config.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static clientConnection_t clientConnections[1];
static clientConnection_t *clc = clientConnections;
static clientActive_t client;
static clientActive_t *cl = &client;
static clientStatic_t cls;
static LegacyHacks hacks;
static LegacyHacks *legacyHacks = &hacks;
void *imp_clc = &clc, *imp_cl = &cl, *imp_legacyHacks = &legacyHacks;
static byte **clc_ptr = (byte **)&clc;
static byte *uiBytes = (byte *)&hacks;
static byte **download_ui_ptr = &uiBytes;
static int pure, checksum;
static void *bgs;
void *imp_cl_connectedToPureServer = &pure, *imp_fs_checksumFeed = &checksum, *imp_bgs = &bgs;
static dvar_t maxpackets, zero, one;
static const dvar_t *cl_maxpackets = &maxpackets, *cl_showSend = &zero;
static const dvar_t *cl_packetdup = &zero, *cl_nodelta = &zero;
static const dvar_t *com_developer = &zero, *com_sv_running = &one;
static const dvar_t *cl_paused = &zero, *sv_disableClientConsole = &zero;
static refexport_t re;
static int sent, created, canceled, restarted, failed, groups;
static int shortValues[2], shortIndex, longIndex, httpFlags, openFailure, nextRequests;
static int invalidName;
static netadr_t lastLanAddress;
static dlStatus_t downloadStatus;
static char renamed[256];
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #x); ++failed; } } while (0)

void CL_NextDownload(void);
void CL_ParseWWWDownload(msg_t *msg);
void CL_AddReliableCommand(const char *s) { if (!strncmp(s, "nextdl", 6)) ++nextRequests; }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
void I_strncat(char *dest, int size, const char *src) { strncat(dest, src, size - strlen(dest) - 1); }
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); int n = vsnprintf(dest, size, fmt, ap); va_end(ap); return n; }
char *va(const char *fmt, ...)
{ static char b[1024]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap); return b; }
void MSG_WriteReliableCommandToBuffer(const char *text, char *out, int size) { I_strncpyz(out, text, size); }
void MSG_SetDefaultUserCmd(playerState_t *ps, usercmd_t *cmd) { (void)ps; memset(cmd, 0, sizeof(*cmd)); }
void MSG_Init(msg_t *msg, byte *data, int length)
{ memset(msg, 0, sizeof(*msg)); msg->data = data; msg->maxsize = length; memset(data, 0, length); }
void MSG_WriteByte(msg_t *msg, int value) { msg->data[msg->cursize++] = value; }
void MSG_WriteLong(msg_t *msg, int value) { memcpy(msg->data + msg->cursize, &value, 4); msg->cursize += 4; }
void MSG_WriteBits(msg_t *msg, int value, int bits) { (void)bits; MSG_WriteByte(msg, value); }
void MSG_WriteString(msg_t *msg, const char *s) { int n = (int)strlen(s) + 1; memcpy(msg->data + msg->cursize, s, n); msg->cursize += n; }
int MSG_WriteBitsCompress(byte *from, byte *to, int size) { memcpy(to, from, size); return size; }
int Com_HashKey(char *s, int n) { (void)s; (void)n; return 0; }
void MSG_WriteDeltaUsercmdKey(msg_t *msg, int key, usercmd_t *a, usercmd_t *b)
{ (void)key; (void)a; MSG_WriteLong(msg, b->serverTime); }
void CL_Netchan_Transmit(netchan_t *chan, byte *data, int length)
{ (void)data; (void)length; ++sent; ++chan->outgoingSequence; }
void CL_Netchan_TransmitNextFragment(netchan_t *chan) { (void)chan; abort(); }
usercmd_t CL_CreateCmd(void) { ++created; return (usercmd_t){.serverTime = cls.realtime}; }
qboolean Sys_IsLANAddress(netadr_t address)
{ lastLanAddress = address; return address.type == NA_IP && address.ip[0] == 192; }
void LargeLocal_LargeLocal(LargeLocal *ll, int size) { (void)ll; CHECK(size == MAX_MSGLEN); }
void *LargeLocal_GetBuf(LargeLocal *ll) { static byte buffer[MAX_MSGLEN]; (void)ll; return buffer; }
void ZN10LargeLocalD1Ev(LargeLocal *ll) { (void)ll; }
int MSG_ReadShort(msg_t *msg) { (void)msg; assert(shortIndex < 2); return shortValues[shortIndex++]; }
int MSG_ReadLong(msg_t *msg) { (void)msg; return longIndex++ ? httpFlags : 4; }
char *MSG_ReadString(msg_t *msg) { (void)msg; return "https://example.test/main/map.iwd"; }
void MSG_ReadData(msg_t *msg, void *out, int count) { (void)msg; memset(out, 0x42, count); }
int FS_SV_FOpenFileWrite(const char *path) { CHECK(strstr(path, ".tmp") != NULL); return openFailure ? 0 : 9; }
int FS_Write(const void *data, int length, int file) { (void)data; CHECK(file == 9); return length; }
void FS_FCloseFile(int file) { CHECK(file == 9); }
void FS_SV_Rename(const char *from, const char *to) { CHECK(strstr(from, ".tmp") != NULL); snprintf(renamed, sizeof(renamed), "%s", to); }
qboolean NetHardening_ValidDownloadName(const char *name, qboolean allow)
{ (void)name; (void)allow; return !invalidName; }
qboolean NetHardening_ValidUrl(const char *url) { return !strncmp(url, "https://", 8); }
void FS_Restart(int feed) { (void)feed; ++restarted; }
#if COD2_FEATURE_WWW_DOWNLOAD
void DL_CancelDownload(void) { ++canceled; }
#endif
char *Dvar_GetString(const char *name) { (void)name; return "/private"; }
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *out)
{ snprintf(out, 256, "%s/%s/%s", base, game, qpath); }
#if COD2_FEATURE_WWW_DOWNLOAD
int DL_BeginDownload(const char *to, const char *url, int debug)
{ (void)debug; CHECK(strstr(to, ".tmp") != NULL); CHECK(strstr(url, "https://") == url); downloadStatus = DL_STATUS_IN_PROGRESS; return 1; }
void DL_DownloadLoop(void) { }
dlStatus_t DL_GetStatus(void) { return downloadStatus; }
int DL_DLIsMotd(void) { return 0; }
#endif
void Sys_OpenURL(const char *url, int active) { (void)url; (void)active; abort(); }
void Cbuf_ExecuteText(int when, const char *text) { (void)when; CHECK(!strcmp(text, "reconnect\n")); }
int FS_ReadFile(const char *path, void **buf) { (void)path; (void)buf; return -1; }
void FS_FreeFile(void *buf) { (void)buf; }
void Dvar_SetStringByName(const char *name, const char *value) { (void)name; (void)value; }
int rename(const char *from, const char *to) { (void)from; snprintf(renamed, sizeof(renamed), "%s", to); return 0; }
int remove(const char *path) { (void)path; abort(); }
void FS_CopyFile(char *from, char *to) { (void)from; (void)to; abort(); }
const char *CL_GetConfigString(int index) { (void)index; abort(); }
const char *Info_ValueForKey(const char *s, const char *key) { (void)s; (void)key; abort(); }
void UI_SetMap(const char *map, const char *game) { (void)map; (void)game; abort(); }
void SCR_UpdateScreenInternal(void) { abort(); }
void CL_ShutdownCGame(void) { abort(); }
void CL_ShutdownUI(void) { abort(); }
void Com_Restart(void) { abort(); }
void CL_StartHunkUsers(void) { abort(); }
void Dvar_SetInt(const dvar_t *d, int value) { (void)d; (void)value; }
void Dvar_SetBool(const dvar_t *d, unsigned char value) { (void)d; (void)value; }
void CL_InitCGame(void) { abort(); }
const char *FS_ReferencedIwdPureChecksums(void) { abort(); }
void Cod2x_Disconnect(void) { }
void Cod2x_DemoClientDisconnect(void) { }
void MatchSummary_Finish(const char *reason) { (void)reason; }
void CL_StopRecord_f(void) { clc->demorecording = 0; }
void SCR_StopCinematic(void) { }
void CL_ClearState(void) { }
void CL_ClearMutedList(void) { }
void SND_DisconnectListener(void) { }
Bool CL_AllLocalClientsDisconnected(void) { return 1; }
#include "scheduling_source.h"

static void reset(connstate_t state)
{
    memset(clc, 0, sizeof(*clc)); memset(cl, 0, sizeof(*cl)); memset(&cls, 0, sizeof(cls));
    memset(&hacks, 0, sizeof(hacks)); memset(&lastLanAddress, 0, sizeof(lastLanAddress));
    clc->state = state; clc->connectTime = -99999; clc->lastPacketSentTime = 1000;
    clc->netchan.outgoingSequence = 1;
    clc->netchan.remoteAddress.type = clc->serverAddress.type = NA_IP;
    clc->netchan.remoteAddress.ip[0] = clc->serverAddress.ip[0] = 8;
    maxpackets.current.integer = 100; one.current.enabled = 1; client.cgameInitialized = 1;
    hacks.cl_running = 1; cls.realtime = 1000;
    sent = created = canceled = restarted = shortIndex = longIndex = openFailure = nextRequests = invalidName = httpFlags = 0;
    renamed[0] = 0; downloadStatus = DL_STATUS_IDLE;
}
static void begin(void)
{
    strcpy(clc->downloadList, "@main/map.iwd@main/map.iwd");
    CL_NextDownload();
    CHECK(!strcmp(cls.downloadTempName, "main/map.iwd.tmp"));
}
static void udp(int block, int size)
{
    shortValues[0] = block; shortValues[1] = size; shortIndex = longIndex = 0;
    msg_t msg = {0}; CL_ParseDownload(&msg);
}
static void download_timing(void)
{
    reset(CA_CONNECTED); begin();
    cls.realtime = 1049; CL_SendCmdInternal(); CHECK(sent == 0);
    cls.realtime = 1050; CL_SendCmdInternal(); CHECK(sent == 1 && clc->lastPacketSentTime == 1050);
    cls.realtime = 1099; CL_SendCmdInternal(); CHECK(sent == 1);
    cls.realtime = 1100; CL_SendCmdInternal(); CHECK(sent == 2);
    reset(CA_CONNECTED); strcpy(cls.downloadName, "main/map.iwd");
    cls.realtime = 1050; CL_SendCmdInternal(); CHECK(!sent);
    reset(CA_CONNECTED); strcpy(cls.downloadTempName, "main/map.iwd.tmp");
    cls.realtime = 1050; CL_SendCmdInternal(); CHECK(sent == 1);
}
static void connection_timing(void)
{
    for (int state = CA_CONNECTED; state <= CA_LOADING; ++state) {
        reset(state); cls.realtime = 1999; CL_SendCmdInternal(); CHECK(sent == 0);
        cls.realtime = 2000; CL_SendCmdInternal(); CHECK(sent == 1);
        cls.realtime = 2999; CL_SendCmdInternal(); CHECK(sent == 1);
        cls.realtime = 3000; CL_SendCmdInternal(); CHECK(sent == 2);
    }
}
static void local_gate_order(void)
{
    for (int kind = 0; kind <= 1; ++kind) {
        reset(CA_CONNECTED);
        if (!kind) clc->netchan.remoteAddress.type = clc->serverAddress.type = NA_LOOPBACK;
        else clc->netchan.remoteAddress.ip[0] = clc->serverAddress.ip[0] = 192;
        cls.realtime = 1999; CL_SendCmdInternal(); CHECK(!sent);
        cls.realtime = 2000; CL_SendCmdInternal(); CHECK(sent == 1);
        begin(); cls.realtime = 2049; CL_SendCmdInternal(); CHECK(sent == 1);
        cls.realtime = 2050; CL_SendCmdInternal(); CHECK(sent == 2);
    }
}
static void active_controls(void)
{
    reset(CA_ACTIVE); clc->connectTime = 1000; clc->demowaiting = 1;
    client.outPackets[0].p_realtime = 1000;
    cls.realtime = 1009; CL_SendCmdInternal(); CHECK(!sent && created == 1 && client.cmdNumber == 1);
    cls.realtime = 1010; CL_SendCmdInternal(); CHECK(sent == 1 && created == 2);
    CHECK(client.outPackets[1].p_realtime == 1010 && client.outPackets[1].p_cmdNumber == 2);
    CHECK(clc->lastPacketSentTime == 1010);
}
static void ordinary_controls(void)
{
    reset(CA_ACTIVE); client.outPackets[0].p_realtime = 1000;
    cls.realtime = 1009; CL_SendCmdInternal(); CHECK(!sent && created == 1);
    cls.realtime = 1010; CL_SendCmdInternal(); CHECK(sent == 1 && created == 2);
    reset(CA_ACTIVE); clc->demoplaying = 1; cls.realtime = 5000; CL_SendCmdInternal(); CHECK(!sent && created == 1);
    reset(CA_CINEMATIC); cls.realtime = 5000; CL_SendCmdInternal(); CHECK(!sent && !created);
    reset(CA_LOGO); cls.realtime = 5000; CL_SendCmdInternal(); CHECK(!sent && !created);
}
static void channel_address(void)
{
    reset(CA_ACTIVE); client.outPackets[0].p_realtime = 1000;
    clc->netchan.remoteAddress.ip[0] = 192; cls.realtime = 1001;
    CL_SendCmdInternal(); CHECK(sent == 1 && lastLanAddress.ip[0] == 192);
    reset(CA_ACTIVE); client.outPackets[0].p_realtime = 1000;
    clc->netchan.remoteAddress.type = NA_LOOPBACK; cls.realtime = 1001;
    CL_SendCmdInternal(); CHECK(sent == 1);
}
static void udp_completion(void)
{
    reset(CA_CONNECTED); begin(); udp(0, 4);
    CHECK(nextRequests == 1 && clc->downloadBlock == 1 && cls.downloadTempName[0]);
    udp(1, 0);
    CHECK(!cls.downloadName[0] && !cls.downloadTempName[0]);
    CHECK(!clc->downloadName[0] && !clc->downloadTempName[0]);
    CHECK(!strcmp(renamed, "main/map.iwd") && restarted == 1 && canceled == !!COD2_FEATURE_WWW_DOWNLOAD);
    int before = sent; cls.realtime += 50; CL_SendCmdInternal(); CHECK(sent == before);
}
static void udp_failure_cleanup(void)
{
    reset(CA_CONNECTED); begin(); openFailure = 1; udp(0, 4);
    CHECK(!cls.downloadTempName[0] && canceled == !!COD2_FEATURE_WWW_DOWNLOAD);
#if COD2_FEATURE_NET_HARDENING
    reset(CA_CONNECTED); begin(); invalidName = 1; udp(0, 4);
    CHECK(!cls.downloadName[0] && !cls.downloadTempName[0]);
#endif
}
static void www_completion_and_disconnect(void)
{
#if COD2_FEATURE_WWW_DOWNLOAD
    reset(CA_CONNECTED); begin(); msg_t msg = {0};
    CL_ParseWWWDownload(&msg); CHECK(cls.wwwDlInProgress && cls.downloadTempName[0]);
    downloadStatus = DL_STATUS_DONE; CL_WWWDownload();
    CHECK(!cls.downloadTempName[0] && !cls.wwwDlInProgress && restarted == 1 && canceled == !!COD2_FEATURE_WWW_DOWNLOAD);
    CHECK(!strcmp(renamed, "/private/main/map.iwd"));
#endif
    reset(CA_CONNECTED); begin(); CL_Disconnect();
    CHECK(!cls.downloadTempName[0] && !cls.downloadName[0] && canceled == !!COD2_FEATURE_WWW_DOWNLOAD);
    reset(CA_CONNECTED); begin(); cls.wwwDlDisconnected = 1; cls.wwwDlInProgress = 1;
    CL_Disconnect(); CHECK(cls.downloadTempName[0] && cls.wwwDlDisconnected && !canceled);
    reset(CA_CONNECTED); begin(); cls.wwwDlDisconnected = 1;
    int sequence = clc->reliableSequence;
    CL_DownloadsComplete();
    CHECK(clc->reliableSequence == sequence && restarted == 1 && canceled == !!COD2_FEATURE_WWW_DOWNLOAD);
    CHECK(!cls.wwwDlDisconnected && !cls.downloadTempName[0]);
}
int main(void)
{
#define RUN(f) do { int before = failed; f(); ++groups; printf("%s: %s\n", #f, failed == before ? "PASS" : "FAIL"); } while (0)
    RUN(download_timing); RUN(connection_timing); RUN(local_gate_order); RUN(active_controls);
    RUN(ordinary_controls); RUN(channel_address); RUN(udp_completion); RUN(udp_failure_cleanup); RUN(www_completion_and_disconnect);
    printf("%d groups, %d failed assertions\n", groups, failed); return failed != 0;
}
