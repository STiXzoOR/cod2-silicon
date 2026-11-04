#include "ws6_snapshot_source.c"
#include <assert.h>
#include <stdlib.h>

server_t sv;
serverStatic_t svs;
void *imp_sv = &sv;
void *imp_svs = &svs;
static dvar_t maxclients;
static dvar_t fps;
static dvar_t maxRate;
static dvar_t padPackets;
const dvar_t *sv_maxclients = &maxclients;
byte *sv_maxclients_dvar = (byte *)&sv_maxclients;
const dvar_t *sv_fps = &fps;
byte *sv_fps_dvar = (byte *)&sv_fps;
/* sv_maxRate keeps its retail default of 0, distinct from sv_fps. */
const dvar_t *sv_maxRate = &maxRate;
byte *sv_maxRate_dvar = (byte *)&sv_maxRate;
const dvar_t *sv_padPackets = &padPackets;
byte *sv_padPackets_dvar = (byte *)&sv_padPackets;
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
void MSG_WriteByte(msg_t *msg, int value) { msg->data[msg->cursize++] = (byte)value; }
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
void MSG_WriteDeltaEntity(msg_t *msg, byte *from, byte *to, int force) {}
/* Archive decoding is never reached: every lookup below hits a cached frame. */
int MSG_ReadBit(msg_t *msg) { abort(); }
int MSG_ReadBits(msg_t *msg, int bits) { abort(); }
int MSG_ReadLong(msg_t *msg) { abort(); }
qboolean MSG_ReadDeltaArchivedEntity(msg_t *msg, archivedEntity_t *from, archivedEntity_t *to, int number) { abort(); }
qboolean MSG_ReadDeltaClient(msg_t *msg, clientState_t *from, clientState_t *to, int number) { abort(); }
void MSG_ReadDeltaPlayerstate(msg_t *msg, byte *from, byte *to) { abort(); }
int G_GetClientArchiveTime(int clientNum) { return 1000; }
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

    /* Mac 0x195107: every archived snapshot advances the shared buffer
       cursor, so the next one is stored after it rather than over it. */
    int firstStart = partsOfArchive[0].start;
    int firstSize = partsOfArchive[0].size;
    assert(svs.nextArchivedSnapshotBuffer == firstStart + firstSize);
    SV_ArchiveSnapshot();
    assert(svs.nextArchivedSnapshotFrames == 2);
    assert(partsOfArchive[1].start == firstStart + firstSize);
    assert(svs.nextArchivedSnapshotBuffer == partsOfArchive[1].start + partsOfArchive[1].size);

    /* Mac 0x194e2b/0x194e5e: a cached frame is a delta base only when it is
       no older than sv_fps archived frames; sv_maxRate plays no part. */
    svs.nextArchivedSnapshotFrames = 100;
    svs.nextArchivedSnapshotBuffer = 0;
    svs.nextCachedSnapshotFrames = 1;
    svs.nextCachedSnapshotEntities = 0;
    svs.nextCachedSnapshotClients = 0;
    memset(frames, 0, sizeof(frames));
    frames[0].archivedFrame = 100 - 30;
    SV_ArchiveSnapshot();
    assert(buffer[0] == 1);
    frames[0].archivedFrame = 100 - 5;
    svs.nextCachedSnapshotFrames = 1;
    SV_ArchiveSnapshot();
    assert(buffer[0] == 0);
    int base;
    memcpy(&base, buffer + 1, 4);
    assert(base == 100 - 5);

    /* Mac SV_GetArchivedClientInfo (0x195a06) converts the archive time with
       sv_fps; with sv_maxRate's default of 0 the anti-lag lookup found nothing. */
    svs.nextArchivedSnapshotFrames = 100;
    svs.nextCachedSnapshotFrames = 1;
    memset(frames, 0, sizeof(frames));
    frames[0].archivedFrame = 100 - 20;
    memset(partsOfArchive, 0, sizeof(partsOfArchive));
    int archiveTime = 1000;
    int ps[0x26a8 / 16][4];
    byte cs[0x100];
    assert(!SV_GetArchivedClientInfo(0, &archiveTime, ps, cs));
    assert(archiveTime == 1000);
    int found = 0;
    assert(SV_FindArchivedSnapshotForClientLocal(0, &found) == &frames[0]);
    assert(found == 1000);

    /* Mac SV_SendClientSnapshot (0x1960e8, 0x196144): delta from any positive
       deltaMessage while active; snapFlags bit 0 is rateDelayed. */
    svs.clients[0].state = 4;
    svs.clients[0].netchan.outgoingSequence = 12;
    svs.clients[0].deltaMessage = 5;
    svs.clients[0].gamestateMessageNum = 10;
    svs.clients[0].rateDelayed = 1;
    svs.clients[0].pureAuthentic = 0;
    svs.nextSnapshotEntities = 0;
    svs.numSnapshotEntities = 0;
    byte out_buf[256];
    msg_t outMsg;
    MSG_Init(&outMsg, out_buf, sizeof(out_buf));
    SV_WriteSnapshotToClientLocal(&svs.clients[0], &outMsg);
    assert(out_buf[0] == 6 && out_buf[5] == 7);
    assert((out_buf[6] & 1) == 1);
    svs.clients[0].rateDelayed = 0;
    svs.clients[0].pureAuthentic = 1;
    MSG_Init(&outMsg, out_buf, sizeof(out_buf));
    SV_WriteSnapshotToClientLocal(&svs.clients[0], &outMsg);
    assert((out_buf[6] & 1) == 0);
    free(svs.clients);
    puts("snapshots: client indexing/copy, archive wrap, archive history, anti-lag lookup and snapshot header pass");
}
