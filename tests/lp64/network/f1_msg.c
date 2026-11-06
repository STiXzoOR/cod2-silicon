#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

const dvar_t *cl_shownet;
unsigned int kbitmask[33];
void Com_Printf(const char *fmt, ...) {}
/* Character cleanup is tested separately; preserve the production char ABI. */
int I_CleanChar(int ch) { return ch == (signed char)0x92 ? '\'' : ch; }
#include "f1_msg_source.c"

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)
static void check_strings(void)
{
    char *(*readers[])(msg_t *) = {MSG_ReadString, MSG_ReadBigString, MSG_ReadStringLine};
    for (unsigned r = 0; r < 3; ++r) {
        byte wire[] = {'A', 0xff, 'B', 0, 77};
        msg_t msg = {0}; msg.data = wire; msg.cursize = sizeof(wire);
        char *s = readers[r](&msg);
        CHECK(!memcmp(s, wire, 4));
        CHECK(msg.readcount == 4 && !msg.overflowed);
        CHECK(MSG_ReadByte(&msg) == 77);
        byte truncated[] = {'Q'};
        memset(&msg, 0, sizeof(msg)); msg.data = truncated; msg.cursize = 1;
        s = readers[r](&msg);
        CHECK(s[0] == 'Q' && s[1] == 0 && msg.overflowed && msg.readcount == 1);
        byte percent[] = {'%', '\n', 0, 77};
        memset(&msg, 0, sizeof(msg)); msg.data = percent; msg.cursize = sizeof(percent);
        s = readers[r](&msg);
        CHECK(s[0] == (r ? '.' : '%'));
        CHECK(msg.readcount == (r == 2 ? 2 : 3));
    }
}
static void usercmds(void)
{
    int deltas[] = {3, 50, 255, 256};
    for (int parity = 0; parity < 2; ++parity)
    for (int d = 0; d < 4; ++d) for (int field = 0; field < 9; ++field) {
        usercmd_t from = {0}, to, decoded;
        from.serverTime = 1000; from.weapon = 3; from.offHandIndex = 2;
        to = from; to.serverTime += deltas[d];
        if (field == 1) to.weapon++;
        if (field == 2) to.offHandIndex++;
        if (field == 3) to.buttons = 0x80;
        if (field == 4) to.angles[2] = 123;
        if (field == 5) to.angles[0] = 44;
        if (field == 6) to.buttons = 1;
        if (field == 7) to.forwardmove = 127;
        if (field == 8) {
            to.weapon++; to.angles[0] = 44; to.forwardmove = 127;
        }
        byte wire[256] = {0};
        msg_t msg = {0}; msg.data = wire; msg.maxsize = sizeof(wire);
        int key = 0x12345678 | parity;
        MSG_WriteDeltaUsercmdKey(&msg, key, &from, &to);
        CHECK(!msg.overflowed);
        msg.readcount = msg.bit = 0;
        CHECK(MSG_ReadBit(&msg) == (deltas[d] <= 255));
        if (deltas[d] <= 255) CHECK(MSG_ReadByte(&msg) == deltas[d]);
        else CHECK(MSG_ReadLong(&msg) == to.serverTime);
        CHECK((MSG_ReadBit(&msg) ^ (key & 1)) == (field != 0));
        if (field) CHECK((MSG_ReadBit(&msg) ^ (key & 1)) == (field <= 4 || field == 8));
        msg.readcount = msg.bit = 0;
        MSG_ReadDeltaUsercmdKey(&msg, key, &from, &decoded);
        CHECK(!msg.overflowed);
        CHECK(decoded.serverTime == to.serverTime);
        CHECK(decoded.weapon == to.weapon && decoded.offHandIndex == to.offHandIndex);
        CHECK(decoded.buttons == to.buttons);
        CHECK(!memcmp(decoded.angles, to.angles, sizeof(to.angles)));
        CHECK(decoded.forwardmove == to.forwardmove && decoded.rightmove == to.rightmove);
    }
}
int main(void)
{
    for (int i = 1; i <= 32; ++i) kbitmask[i] = i == 32 ? ~0u : (1u << i) - 1;
    check_strings(); usercmds();
    if (failures) { fprintf(stderr, "%d failures\n", failures); return 1; }
    puts("f1_msg: string byte/EOF distinction and usercmd modes pass");
}
