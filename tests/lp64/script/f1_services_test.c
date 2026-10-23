#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../../src/PC/script/scr_memorytree.c"
#include "../../../src/PC/script/scr_stringlist.c"

unsigned char scrStringGlob[65664] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[0x80380] __attribute__((aligned(8)));
scrMemTreePub_t scrMemTreePub;

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Scr_TerminalError(const char *msg) { (void)msg; abort(); }
void Scr_DumpScriptThreads(void) {}
void Scr_DumpScriptVariables(void) {}
void *Z_VirtualAllocInternal(int size) { return calloc(1, (size_t)size); }
void Z_VirtualFreeInternal(void *ptr) { free(ptr); }

static void check_reuse(void)
{
    unsigned int ids[4];
    MT_Init();
    for (int i = 0; i < 4; i++)
        ids[i] = MT_AllocIndex(8, 0);
    assert(ids[0] == 1 && ids[1] == 2 && ids[2] == 3 && ids[3] == 4);
    MT_FreeIndex(ids[0], 8);
    MT_FreeIndex(ids[2], 8);
    /* Mac MT_AddMemoryNode promotes the larger allocation score: 1 before 3. */
    assert(MT_AllocIndex(8, 0) == 1);
    assert(MT_AllocIndex(8, 0) == 3);
}

static void check_force(void)
{
    byte seen[0x10000] = {0};
    byte *bits;
    unsigned int freeCount = 0;
    MT_Init();
    assert(MT_AllocIndex(8, 0) == 1);
    bits = MT_InitForceAlloc();
    MT_ForceAllocIndex(bits, 1, 8);
    MT_FinishForceAlloc(bits);
    /* Force-rebuilding preserves the occupied bucket and covers each free bucket once. */
    for (unsigned int node = 1; node < 0x10000; node++) {
        if (!mt_size[node])
            continue;
        for (unsigned int j = 0; j < (1u << (mt_size[node] - 1)); j++) {
            assert(node + j > 1 && node + j < 0x10000);
            assert(!seen[node + j]);
            seen[node + j] = 1;
            freeCount++;
        }
    }
    assert(freeCount == 0x10000 - 2);
    assert(Scr_GetStringUsage() == 1);
    for (unsigned int expected = 2; expected < 1024; expected++)
        assert(MT_AllocIndex(8, 0) == expected);
}

static void check_restart(void)
{
    unsigned int persistent;
    SL_Init();
    /* These ASCII names all hash to bucket 11290, including their NUL byte. */
    persistent = SL_GetString_("f100002", 4, 0);
    SL_GetString_("f101331", 0, 0);
    SL_Init();
    assert(SL_FindString("f100002") == persistent);
    assert(strcmp(SL_ConvertToString(persistent), "f100002") == 0);
    /* With one survivor the hash ring returns directly to its head (Mac 0x43be1). */
    assert(SG_W0(11290) == (0x8000 | 11290));
    assert(SL_FindString("f101331") == 0);
    SL_GetString_("f102660", 0, 0);
    SL_Init();
    assert(SL_FindString("f100002") == persistent);
    assert(SL_FindString("f102660") == 0);
    SL_ShutdownSystem(4);
    assert(SL_FindString("f100002") == 0);
    assert(Scr_GetStringUsage() == 0);
}

static void check_last_string(void)
{
    unsigned int last;
    SL_Init();
    for (unsigned int i = 1; i < 0xffff; i++)
        assert(MT_AllocIndex(8, 0) == i);
    last = SL_GetString_("", 0, 0);
    assert(last == 0xffff);
    assert(SL_FindString("") == last);
    assert(SL_GetString_("", 0, 0) == last);
}

static void check_last_chain(void)
{
    unsigned int earlier, last;
    SL_Init();
    for (unsigned int i = 1; i < 0xfffe; i++)
        assert(MT_AllocIndex(8, 0) == i);
    /* Aa and BB are equal-length31-hash collisions. */
    earlier = SL_GetString_("Aa", 0, 0);
    last = SL_GetString_("BB", 0, 0);
    assert(earlier == 0xfffe && last == 0xffff);
    assert(SL_FindString("Aa") == earlier);
    assert(SL_GetString_("BB", 0, 0) == last);
    assert(SL_FindString("Aa") == earlier);
    assert(SL_FindString("BB") == last);
}

/* Independent buddy-bin model; priority is derived from the Mac instructions. */
static byte oracle_free[17][0x10000];
static unsigned int random_state = 0x13f1c;

static unsigned int next_random(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static int oracle_size(unsigned int bytes)
{
    int size = 0;
    while ((8u << size) < bytes)
        size++;
    return size;
}

static unsigned int oracle_score(unsigned int node)
{
    unsigned int complement = 0x10000 - node;
    unsigned int population = 0;
    for (unsigned int bits = complement; bits; bits >>= 1)
        population += bits & 1;
    return complement - population + (complement & -complement);
}

static unsigned int oracle_alloc(unsigned int bytes)
{
    int wanted = oracle_size(bytes);
    for (int size = wanted; size <= 16; size++) {
        unsigned int best = 0;
        for (unsigned int node = 1; node < 0x10000; node++) {
            if (oracle_free[size][node] &&
                (!best || oracle_score(node) > oracle_score(best)))
                best = node;
        }
        if (!best)
            continue;
        oracle_free[size][best] = 0;
        while (size > wanted) {
            size--;
            oracle_free[size][best + (1u << size)] = 1;
        }
        return best;
    }
    abort();
}

static void oracle_release(unsigned int node, unsigned int bytes)
{
    int size = oracle_size(bytes);
    while (size < 16 && oracle_free[size][node ^ (1u << size)]) {
        oracle_free[size][node ^ (1u << size)] = 0;
        node &= ~(1u << size);
        size++;
    }
    oracle_free[size][node] = 1;
}

static void check_allocator(void)
{
    unsigned int ids[256] = {0};
    unsigned int sizes[256] = {0};
    MT_Init();
    for (int size = 0; size < 16; size++)
        oracle_free[size][1u << size] = 1;
    for (unsigned int step = 0; step < 4000; step++) {
        unsigned int slot = (next_random() >> 16) & 255;
        if (ids[slot]) {
            MT_FreeIndex(ids[slot], sizes[slot]);
            oracle_release(ids[slot], sizes[slot]);
            ids[slot] = 0;
        } else {
            sizes[slot] = ((next_random() >> 16) & 1023) + 1;
            ids[slot] = MT_AllocIndex(sizes[slot], 0);
            assert(ids[slot] == oracle_alloc(sizes[slot]));
        }
        if (step % 317 == 0) {
            byte *bits = MT_InitForceAlloc();
            for (unsigned int i = 0; i < 256; i++) {
                if (ids[i])
                    MT_ForceAllocIndex(bits, ids[i], sizes[i]);
            }
            MT_FinishForceAlloc(bits);
        }
    }
    for (unsigned int slot = 0; slot < 256; slot++) {
        if (ids[slot])
            MT_FreeIndex(ids[slot], sizes[slot]);
    }
    assert(Scr_GetStringUsage() == 0);
    for (unsigned int expected = 1; expected < 0x10000; expected++)
        assert(MT_AllocIndex(8, 0) == expected);
}

static void check_literal_vector(void)
{
    const float value[3] = {1.25f, -2.5f, 3.0f};
    byte packed[sizeof(value) + 1];
    unsigned int id;
    memcpy(packed + 1, value, sizeof(value));
    SL_Init();
    id = SL_GetStringForVector((const float *)(const void *)(packed + 1));
    assert(strcmp(SL_ConvertToString(id), "(1.25, -2.5, 3)") == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "reuse")) check_reuse();
    else if (!strcmp(argv[1], "force")) check_force();
    else if (!strcmp(argv[1], "restart")) check_restart();
    else if (!strcmp(argv[1], "last-string")) check_last_string();
    else if (!strcmp(argv[1], "last-chain")) check_last_chain();
    else if (!strcmp(argv[1], "allocator")) check_allocator();
    else if (!strcmp(argv[1], "literal-vector")) check_literal_vector();
    else abort();
    puts("F1 script services check passed");
    return 0;
}
