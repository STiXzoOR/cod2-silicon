/* Huffman decoding and the MSG_Read* family (msg_mp.c, huffman.c).
 *
 * Mode byte bit 0 clear: the rest is a compressed message. It is decoded into a
 * MAX_MSG_DECOMPRESS_BYTES buffer exactly as SV_ExecuteClientMessage and
 * CL_ParseServerMessage do, and then also used as plain text for an
 * encode/decode round trip that must reproduce it.
 * Mode byte bit 0 set: the rest is a message read by an op stream taken from
 * the message itself (snapshot-style deltas, usercmds, strings, raw reads).
 * Every structure lives in its own exact-size heap block so ASan sees overruns.
 */
#include "common_types.h"
#include "fuzz.h"

#include <stdarg.h>

extern void MSG_Init(msg_t *buf, byte *data, int length);
extern void MSG_BeginReading(msg_t *msg);
extern int MSG_ReadBits(msg_t *msg, int bits);
extern int MSG_ReadBit(msg_t *msg);
extern int MSG_ReadByte(msg_t *msg);
extern int MSG_ReadShort(msg_t *msg);
extern int MSG_ReadLong(msg_t *msg);
extern void MSG_ReadData(msg_t *msg, void *data, int len);
extern char *MSG_ReadString(msg_t *msg);
extern char *MSG_ReadBigString(msg_t *msg);
extern char *MSG_ReadStringLine(msg_t *msg);
extern int MSG_ReadBitsCompress(byte *from, byte *to, int size);
extern int MSG_WriteBitsCompress(byte *from, byte *to, int size);
extern qboolean MSG_ReadDeltaEntity(msg_t *msg, entityState_t *from, entityState_t *to, int number);
extern qboolean MSG_ReadDeltaClient(msg_t *msg, clientState_t *from, clientState_t *to, int number);
extern qboolean MSG_ReadDeltaArchivedEntity(msg_t *msg, archivedEntity_t *from, archivedEntity_t *to, int number);
extern void MSG_ReadDeltaPlayerstate(msg_t *msg, playerState_t *from, playerState_t *to);
extern void MSG_ReadDeltaUsercmdKey(msg_t *msg, int key, usercmd_t *from, usercmd_t *to);

FUZZ_DVAR(cl_shownet);
FUZZ_KBITMASK;

void Com_Printf(const char *fmt, ...)
{
    char text[4096];
    va_list args;

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
}

void Com_Error(int code, const char *fmt, ...)
{
    FuzzEngineError(code, fmt);
}

static byte *decoded;

static void *Fresh(size_t size)
{
    void *p = malloc(size);
    if (!p)
        abort();
    memset(p, 0, size);
    return p;
}

static void Decode(const uint8_t *data, size_t size)
{
    int length;
    byte *encoded;
    size_t plain = size > 4096 ? 4096 : size;
    int encodedSize;
    msg_t msg;

    length = MSG_ReadBitsCompress((byte *)data, decoded, (int)size);
    if (length < 0 || length > MAX_MSG_DECOMPRESS_BYTES)
        abort();

    /* Reading the decoded text as a message must stay in bounds as well. */
    MSG_Init(&msg, decoded, MAX_MSG_DECOMPRESS_BYTES);
    msg.cursize = length;
    while (!msg.overflowed && MSG_ReadBits(&msg, 3) != 3)
        MSG_ReadString(&msg);

    /* Round trip: every encoded byte must decode back to itself. Codes are at
     * most a few bytes long, so four bytes per input byte is ample room. */
    encoded = Fresh(plain * 4 + 8);
    encodedSize = MSG_WriteBitsCompress((byte *)data, encoded, (int)plain);
    if (encodedSize < 0 || (size_t)encodedSize > plain * 4 + 8)
        abort();
    length = MSG_ReadBitsCompress(encoded, decoded, encodedSize);
    if ((size_t)length < plain || memcmp(decoded, data, plain)) {
        fprintf(stderr, "==fuzz== Huffman round trip changed the message (%zu bytes)\n", plain);
        abort();
    }
    free(encoded);
}

static void Read(const uint8_t *data, size_t size)
{
    entityState_t *entFrom = Fresh(sizeof(*entFrom)), *entTo = Fresh(sizeof(*entTo));
    clientState_t *clFrom = Fresh(sizeof(*clFrom)), *clTo = Fresh(sizeof(*clTo));
    archivedEntity_t *arFrom = Fresh(sizeof(*arFrom)), *arTo = Fresh(sizeof(*arTo));
    playerState_t *psFrom = Fresh(sizeof(*psFrom)), *psTo = Fresh(sizeof(*psTo));
    usercmd_t *cmdFrom = Fresh(sizeof(*cmdFrom)), *cmdTo = Fresh(sizeof(*cmdTo));
    byte *copy = Fresh(size ? size : 1);
    byte scratch[256];
    msg_t msg;
    int steps = 0;

    memcpy(copy, data, size);
    MSG_Init(&msg, copy, (int)size);
    msg.cursize = (int)size;
    MSG_BeginReading(&msg);
    while (!msg.overflowed && msg.readcount < msg.cursize && steps++ < 256) {
        int op = MSG_ReadByte(&msg);
        int n;

        switch (op % 16) {
        case 0:
            MSG_ReadBits(&msg, MSG_ReadByte(&msg) & 31);
            break;
        case 1:
            MSG_ReadBit(&msg);
            MSG_ReadShort(&msg);
            MSG_ReadLong(&msg);
            break;
        case 2:
            n = MSG_ReadByte(&msg);
            MSG_ReadData(&msg, scratch, n < 0 ? 0 : n);
            break;
        case 3:
            if (strlen(MSG_ReadString(&msg)) > 1023)
                abort();
            break;
        case 4:
            if (strlen(MSG_ReadBigString(&msg)) > 8191)
                abort();
            break;
        case 5:
            if (strlen(MSG_ReadStringLine(&msg)) > 1023)
                abort();
            break;
        case 6:
            if (!MSG_ReadDeltaEntity(&msg, entFrom, entTo, MSG_ReadBits(&msg, 10)))
                memcpy(entFrom, entTo, sizeof(*entTo));
            break;
        case 7:
            if (!MSG_ReadDeltaClient(&msg, (op & 16) ? NULL : clFrom, clTo, MSG_ReadBits(&msg, 6)))
                memcpy(clFrom, clTo, sizeof(*clTo));
            break;
        case 8:
            if (!MSG_ReadDeltaArchivedEntity(&msg, arFrom, arTo, MSG_ReadBits(&msg, 10)))
                memcpy(arFrom, arTo, sizeof(*arTo));
            break;
        case 9:
        case 10:
            MSG_ReadDeltaPlayerstate(&msg, (op & 16) ? NULL : psFrom, psTo);
            memcpy(psFrom, psTo, sizeof(*psTo));
            break;
        case 11:
        case 12:
            MSG_ReadDeltaUsercmdKey(&msg, MSG_ReadLong(&msg), cmdFrom, cmdTo);
            memcpy(cmdFrom, cmdTo, sizeof(*cmdTo));
            break;
        default:
            MSG_ReadByte(&msg);
            break;
        }
    }
    free(entFrom), free(entTo), free(clFrom), free(clTo), free(arFrom), free(arTo);
    free(psFrom), free(psTo), free(cmdFrom), free(cmdTo), free(copy);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    msg_t init;

    if (!decoded) {
        decoded = Fresh(MAX_MSG_DECOMPRESS_BYTES);
        MSG_Init(&init, decoded, 1);
    }
    if (!size)
        return 0;
    if (data[0] & 1)
        Read(data + 1, size - 1);
    else
        Decode(data + 1, size - 1);
    return 0;
}
