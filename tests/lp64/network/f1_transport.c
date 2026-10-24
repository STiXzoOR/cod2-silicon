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
int Sys_Milliseconds(void) { return 1; }
void Sys_SendPacket(int n, const void *data, netadr_t adr)
{
    assert(n <= sizeof(wire));
    memcpy(wire, data, n);
    wireLength = n;
    sent++;
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
#include "f1_transport_source.c"
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
    expectError = 1;
    if (!setjmp(error)) {
        Netchan_Transmit(chan, MAX_MSGLEN + 1, data);
        assert(!"oversize payload accepted");
    }
    free(data); free(chan);
    puts("f1_transport: Mac 128 KiB cap and fragment bytes pass");
}
