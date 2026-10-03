#include "ws6_snapshot_source.c"
#include <assert.h>
#include <stdlib.h>

server_t sv;
serverStatic_t svs;
void *imp_sv = &sv;
void *imp_svs = &svs;
static dvar_t maxclients;
static dvar_t fps;
const dvar_t *sv_maxclients = &maxclients;
byte *sv_maxclients_dvar = (byte *)&sv_maxclients;
const dvar_t *sv_fps = &fps;
const dvar_t *sv_maxRate = &fps;
static clientState_t states[3];
static byte buffer[0x20000];
static byte archive[0x2000000];
static archivedSnapshot_t partsOfArchive[1200];
static cachedSnapshot_t frames[512];
int __mh_execute_header;

clientState_t *G_GetClientState(int index) { return &states[index]; }
int GetFollowPlayerState(int index, byte *ps) { return 0; }
void Com_Error(int code, const char *fmt, ...) { abort(); }
void Com_DPrintf(const char *fmt, ...) {}
void LargeLocal_LargeLocal(byte *ll, int size) {}
void *LargeLocal_GetBuf(const LargeLocal *ll) { return buffer; }
void ZN10LargeLocalD1Ev(byte *ll) {}
void MSG_Init(msg_t *msg, byte *data, int len)
{
    memset(msg, 0, sizeof(*msg));
    msg->data = data;
    msg->maxsize = len;
}
void MSG_WriteBit0(msg_t *msg) { msg->data[msg->cursize++] = 0; }
void MSG_WriteBit1(msg_t *msg) { msg->data[msg->cursize++] = 1; }
void MSG_WriteLong(msg_t *msg, int value)
{
    memcpy(msg->data + msg->cursize, &value, 4);
    msg->cursize += 4;
}
void MSG_WriteBits(msg_t *msg, int value, int bits) { MSG_WriteLong(msg, value); }
void MSG_WriteDeltaClient(msg_t *msg, byte *from, byte *to, int force) {}
void MSG_WriteDeltaPlayerstate(msg_t *msg, byte *from, byte *to) {}
void MSG_WriteDeltaArchivedEntity(msg_t *msg, byte *from, byte *to, int force) {}
byte *SV_GentityNum(int index) { abort(); }
byte *SV_SvEntityForGentity(byte *ent) { abort(); }

int main(void)
{
    svs.clients = calloc(4, sizeof(client_t));
    assert(SV_ClientIndexLocal(&svs.clients[3]) == 3);
    clientState_t out[3] = {0};
    svs.snapshotClients = (char *(*)())out;
    svs.numSnapshotClients = 3;
    maxclients.current.integer = 3;
    for (int i = 0; i < 3; i++) {
        svs.clients[i].state = 2;
        states[i].clientIndex = i;
        states[i].modelindex = i + 5;
    }
    clientSnapshot_t frame = {0};
    SV_CopyCurrentClientsToSnapshotLocal(&frame);
    assert(frame.num_clients == 3);
    assert(out[2].modelindex == 7);

    sv.state = 2;
    svs.archiveEnabled = 1;
    svs.archivedSnapshotFrames = (char *(*)())partsOfArchive;
    svs.archivedSnapshotBuffer = archive;
    svs.cachedSnapshotFrames = frames;
    maxclients.current.integer = 0;
    fps.current.integer = 20;
    /* Exercise the archive wrap branch, which used msg+4 for the native data pointer. */
    svs.nextArchivedSnapshotBuffer = sizeof(archive) - 2;
    SV_ArchiveSnapshot();
    assert(svs.nextArchivedSnapshotFrames == 1);
    assert(partsOfArchive[0].size > 2);
    assert(archive[sizeof(archive) - 2] == buffer[0]);
    assert(archive[0] == buffer[2]);
    free(svs.clients);
    puts("snapshots: client indexing/copy and archive wrap pass");
}
