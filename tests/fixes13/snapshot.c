#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define SV_SVC_SERVERCOMMAND 4
#define SV_SVC_EOF 7
#define SV_SVC_SNAPSHOT 6
#define CLIENT_CMDENTRY_MASK 127
static byte buffer[0x20000];
static int downloads, snapshots, builds, stringWrites;
static serverStatic_t server;
void *imp_svs = &server;
static clientSnapshot_t frame;
static const dvar_t *sv_padPackets_dvar;
static int SV_ClientIndexLocal(client_t *client) { (void)client; return 0; }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void LargeLocal_LargeLocal(void *ll, int size) { (void)ll; assert(size == sizeof(buffer)); }
void *LargeLocal_GetBuf(const LargeLocal *ll) { (void)ll; return buffer; }
void ZN10LargeLocalD1Ev(void *ll) { (void)ll; }
void MSG_Init(msg_t *msg, byte *data, int size)
{
    memset(msg, 0, sizeof(*msg)); msg->data = data; msg->maxsize = size;
}
void MSG_WriteByte(msg_t *msg, int value) { (void)value; ++msg->cursize; }
void MSG_WriteLong(msg_t *msg, int value) { (void)value; msg->cursize += 4; }
void MSG_WriteString(msg_t *msg, const char *s) { ++stringWrites; msg->cursize += strlen(s) + 1; }
static void SV_BuildClientSnapshotLocal(client_t *client) { (void)client; ++builds; }
static clientSnapshot_t *SV_ClientFrameLocal(client_t *client, int sequence) { (void)client; (void)sequence; return &frame; }
static int SV_DvarIntLocal(byte *dvar) { (void)dvar; return 0; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void MSG_WriteDeltaPlayerstate(msg_t *msg, byte *from, byte *to) { (void)msg; (void)from; (void)to; ++snapshots; }
static void SV_WritePacketEntitiesLocal(msg_t *msg, const clientSnapshot_t *from, const clientSnapshot_t *to) { (void)msg; (void)from; (void)to; }
static void SV_WritePacketClientsLocal(msg_t *msg, const clientSnapshot_t *from, const clientSnapshot_t *to) { (void)msg; (void)from; (void)to; }
void SV_UpdateServerCommandsToClient(client_t *client, msg_t *msg) { (void)client; (void)msg; }
void SV_WriteDownloadToClient(client_t *client, msg_t *msg) { (void)client; (void)msg; ++downloads; }
static void SV_DropOverflowedClientLocal(client_t *client) { (void)client; abort(); }
void SV_SendMessageToClient(msg_t *msg, client_t *client) { (void)client; assert(msg->cursize < sizeof(buffer)); }
#include "snapshot_source.h"
int main(void)
{
    static client_t client;
    msg_t msg;
    for (int openDownload = 0; openDownload <= 1; ++openDownload)
        for (int state = 0; state <= 4; ++state) {
            client.state = state; client.download = openDownload; downloads = snapshots = builds = 0;
            SV_SendClientSnapshot(&client);
            assert(downloads == (state != 1));
            assert(snapshots == (state == 1 || state == 4));
            assert(builds == snapshots);
        }
    client.reliableAcknowledge = 0; client.reliableSequence = 1; client.reliableSent = 1;
    strcpy(client.reliableCommandInfo[1].cmd, "a");
    /* Seven command bytes plus the EOF must fit in 128 KiB. */
    MSG_Init(&msg, buffer, sizeof(buffer)); msg.cursize = sizeof(buffer) - 7;
    stringWrites = 0; SV_WriteOverflowRecoveryCommandsLocal(&client, &msg);
    assert(stringWrites == 0);
    msg.cursize = sizeof(buffer) - 8;
    SV_WriteOverflowRecoveryCommandsLocal(&client, &msg);
    assert(stringWrites == 1 && msg.cursize == sizeof(buffer) - 1);
    client.reliableSent = 0; msg.cursize = 4; stringWrites = 0;
    SV_WriteOverflowRecoveryCommandsLocal(&client, &msg);
    assert(stringWrites == 1 && client.reliableSent == 1);
    return 0;
}
