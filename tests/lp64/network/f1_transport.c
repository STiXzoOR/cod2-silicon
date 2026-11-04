#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>

static dvar_t disabled;
const dvar_t *net_profile = &disabled, *net_showprofile = &disabled;
const dvar_t *net_lanauthorize = &disabled, *showpackets = &disabled;
const dvar_t *showdrop = &disabled, *packetDebug = &disabled;
const dvar_t *com_sv_running = &disabled;
void *imp_legacyHacks;
loopback_t loopbacks[2];
static jmp_buf error;
static int expectError;
static int sent;
static byte wire[1400];
static int wireLength;
void Com_Printf(const char *fmt, ...) {}
void Com_Error(int level, const char *fmt, ...)
{
    assert(expectError);
    longjmp(error, 1);
}
void Com_Memcpy(void *to, const void *from, int n) { memcpy(to, from, n); }
int Com_sprintf(char *dest, int size, const char *fmt, ...) { *dest = 0; return 0; }
int Sys_Milliseconds(void) { return 1; }
Bool Sys_SendPacket(int n, const void *data, netadr_t adr)
{
    assert(n <= sizeof(wire));
    memcpy(wire, data, n);
    wireLength = n;
    sent++;
    return 1;
}
void MSG_Init(msg_t *msg, byte *data, int n)
{
    memset(msg, 0, sizeof(*msg)); msg->data = data; msg->maxsize = n;
}
void MSG_WriteData(msg_t *msg, const void *data, int n)
{
    assert(n >= 0 && msg->cursize + n <= msg->maxsize);
    memcpy(msg->data + msg->cursize, data, n); msg->cursize += n;
}
void MSG_WriteLong(msg_t *msg, int v)
{
    byte b[4] = {(byte)v, (byte)(v >> 8), (byte)(v >> 16), (byte)(v >> 24)};
    MSG_WriteData(msg, b, 4);
}
void MSG_WriteShort(msg_t *msg, int v)
{
    byte b[2] = {(byte)v, (byte)(v >> 8)}; MSG_WriteData(msg, b, 2);
}
void MSG_BeginReading(msg_t *msg) { msg->readcount = 0; msg->bit = 0; }
int MSG_ReadLong(msg_t *msg)
{
    const byte *p = msg->data + msg->readcount;
    if (msg->readcount + 4 > msg->cursize) { msg->readcount = msg->cursize + 1; return -1; }
    msg->readcount += 4;
    return (int)((unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24);
}
int MSG_ReadShort(msg_t *msg)
{
    const byte *p = msg->data + msg->readcount;
    if (msg->readcount + 2 > msg->cursize) { msg->readcount = msg->cursize + 1; return -1; }
    msg->readcount += 2;
    return (short)((unsigned)p[0] | (unsigned)p[1] << 8);
}
#include "f1_transport_source.c"
/* Mac 1.3 Netchan_TransmitNextFragment (0x158d9e) writes the client's qport
   short, then fragmentStart as a long on both sides; Netchan_Process
   (0x1592e2) always reads it as a long. A retail client's fragments must
   therefore reassemble on our server, and ours on a retail server. */
static void client_fragments_reach_server(const byte *data)
{
    netchan_t *client = calloc(1, sizeof(*client));
    netchan_t *server = calloc(1, sizeof(*server));
    byte *packet = malloc(MAX_MSGLEN + 4);
    const int length = 2600;
    client->sock = NS_CLIENT1; client->remoteAddress.type = NA_IP;
    client->outgoingSequence = 9; client->qport = 0x2345;
    server->sock = NS_SERVER; server->remoteAddress.type = NA_IP;
    server->incomingSequence = 8;
    sent = 0;
    assert(Netchan_Transmit(client, length, data));
    int offset = 0, done = 0;
    do {
        int n = length - offset;
        if (n > 1300) n = 1300;
        assert(wireLength == n + 12 && wire[3] == 0x80);
        assert(wire[4] == 0x45 && wire[5] == 0x23);
        assert(wire[6] == (byte)offset && wire[7] == (byte)(offset >> 8));
        assert(wire[8] == 0 && wire[9] == 0);
        assert(wire[10] == (byte)n && wire[11] == (byte)(n >> 8));
        msg_t msg;
        memset(&msg, 0, sizeof(msg));
        memcpy(packet, wire, wireLength);
        msg.data = packet; msg.maxsize = MAX_MSGLEN + 4; msg.cursize = wireLength;
        done = Netchan_Process(server, &msg);
        offset += n;
        if (done) {
            assert(offset == length && msg.cursize == length + 4);
            assert(!memcmp(packet + 4, data, length));
            break;
        }
        assert(client->unsentFragments && Netchan_TransmitNextFragment(client));
    } while (1);
    assert(server->incomingSequence == 9);
    free(packet); free(server); free(client);
}
int main(void)
{
    netchan_t *chan = calloc(1, sizeof(*chan));
    byte *data = malloc(MAX_MSGLEN + 1);
    for (int i = 0; i <= MAX_MSGLEN; i++) data[i] = (byte)i;
    const int lengths[] = {0, 1299, 1300, 2600, 0x4001, MAX_MSGLEN};
    for (unsigned k = 0; k < sizeof(lengths) / sizeof(lengths[0]); k++) {
        memset(chan, 0, sizeof(*chan));
        chan->sock = NS_SERVER; chan->remoteAddress.type = NA_IP;
        chan->outgoingSequence = 7;
        int length = lengths[k];
        sent = 0;
        assert(Netchan_Transmit(chan, length, data));
        if (length < 1300) {
            assert(sent == 1 && wireLength == length + 4);
            assert(wire[0] == 7 && wire[3] == 0);
            assert(!memcmp(wire + 4, data, length));
        } else {
            assert(chan->unsentLength == length);
            assert(!memcmp(chan->unsentBuffer, data, length));
            int offset = 0;
            do {
                int n = length - offset;
                if (n > 1300) n = 1300;
                assert(wireLength == n + 10 && wire[3] == 0x80);
                assert(wire[4] == (byte)offset && wire[5] == (byte)(offset >> 8));
                assert(wire[8] == (byte)n && wire[9] == (byte)(n >> 8));
                assert(!memcmp(wire + 10, data + offset, n));
                offset += n;
                if (!chan->unsentFragments) break;
                assert(Netchan_TransmitNextFragment(chan));
            } while (1);
            assert(offset == length);
        }
        assert(chan->outgoingSequence == 8 && !chan->unsentFragments);
    }
    client_fragments_reach_server(data);
    expectError = 1;
    if (!setjmp(error)) {
        Netchan_Transmit(chan, MAX_MSGLEN + 1, data);
        assert(!"oversize payload accepted");
    }
    free(data); free(chan);
    puts("f1_transport: Mac 128 KiB cap, fragment bytes and client fragments pass");
}
