/* Netchan fragment reassembly and the server XOR decode
 * (net_chan_mp.c Netchan_Process, sv_net_chan_mp.c SV_Netchan_Decode).
 *
 * The input is a series of packets. Each is framed as a 2-byte little-endian
 * length followed by that many bytes, and fed to Netchan_Process on a single
 * server-side channel, exactly as SV_PacketEvent does after matching a client.
 * The channel's receive buffer is a MAX_MSGLEN LargeLocal, as in Com_EventLoop,
 * so a reassembled message that runs past it is caught. When Process reports a
 * complete message it is also run through SV_Netchan_Decode with a key built
 * from fuzzer-chosen client fields, the way SV_PacketEvent decodes the tail.
 */
#include "common_types.h"
#include "fuzz.h"

#include <stdarg.h>

extern qboolean Netchan_Process(netchan_t *chan, msg_t *msg);
extern void Netchan_Setup(netsrc_t sock, netchan_t *chan, netadr_t adr, int qport);
extern void SV_Netchan_Decode(client_t *client, byte *data, int size);
extern void MSG_Init(msg_t *buf, byte *data, int length);

FUZZ_DVAR(net_profile);
FUZZ_DVAR(net_showprofile);
FUZZ_DVAR(showpackets);
FUZZ_DVAR(showdrop);
FUZZ_DVAR(packetDebug);
FUZZ_DVAR(net_lanauthorize);
FUZZ_DVAR(com_sv_running);

/* Netchan_Process reaches NetProf_PrepProfiling, which consults these only
 * when net_profile is set; it is left disabled so no profile is allocated. */
void *imp_com_sv_running = &com_sv_running_value;
static LegacyHacks legacyHacks;
void *imp_legacyHacks = &legacyHacks;

void Com_Printf(const char *fmt, ...)
{
    (void)fmt;
}

void Com_DPrintf(const char *fmt, ...)
{
    (void)fmt;
}

void Com_Error(int code, const char *fmt, ...)
{
    FuzzEngineError(code, fmt);
}

int Sys_Milliseconds(void)
{
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static byte *recv;
    static netchan_t *chan;
    static client_t *client;
    FuzzReader reader;
    int packets = 0;

    if (!recv) {
        recv = malloc(MAX_MSGLEN);
        chan = malloc(sizeof(*chan));
        client = malloc(sizeof(*client));
        if (!recv || !chan || !client)
            abort();
    }
    if (!size)
        return 0;

    memset(chan, 0, sizeof(*chan));
    chan->sock = (netsrc_t)(data[0] & 1); /* 0 = client side, 1 = server side */
    chan->remoteAddress.type = NA_IP;

    reader.p = data + 1;
    reader.end = data + size;

    while (FuzzLeft(&reader) && packets++ < 64) {
        size_t len = FuzzU16(&reader);
        const uint8_t *bytes;
        msg_t msg;

        len = FuzzBytes(&reader, &bytes, len);
        if (len > MAX_MSGLEN)
            len = MAX_MSGLEN;
        /* The packet shares the receive buffer the reassembler writes back to. */
        memcpy(recv, bytes, len);
        MSG_Init(&msg, recv, MAX_MSGLEN);
        msg.cursize = (int)len;

        if (Netchan_Process(chan, &msg)) {
            memset(client, 0, sizeof(*client));
            client->netchan.remoteAddress.type = NA_IP;
            client->serverId = (int)FuzzU32(&reader);
            client->challenge = (int)FuzzU32(&reader);
            client->messageAcknowledge = (int)FuzzU32(&reader);
            client->reliableAcknowledge = FuzzU8(&reader);
            /* Netchan_Process consumed the sequence header; the rest is payload. */
            if (msg.readcount < msg.cursize)
                SV_Netchan_Decode(client, msg.data + msg.readcount, msg.cursize - msg.readcount);
        }
    }
    return 0;
}
