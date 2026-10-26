#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sync_xanim_source.h"

static unsigned int names[16], nameCount, pushedName;
static dvar_t developer;
const dvar_t *com_developer = &developer;

void SL_RemoveRefToString(unsigned int value) { (void)value; }
const char *SL_ConvertToString(unsigned int value) { (void)value; return "synthetic"; }
void Scr_AddConstString(unsigned int value) { pushedName = value; }
void Scr_NotifyNum(int entnum, int classnum, unsigned int value, unsigned int count)
{
    assert(entnum == 6 && classnum == 0 && value == 20 && count == 1);
    assert(nameCount < sizeof(names) / sizeof(names[0]));
    names[nameCount++] = pushedName;
}
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Com_Printf(const char *fmt, ...) { (void)fmt; abort(); }

struct Fixture {
    DObj object;
    XAnimParts parts;
    XAnimNotifyInfo notes[3];
};

static void Init(struct Fixture *f, int nested, int synchronized, float start)
{
    const unsigned int count = nested ? 3 : 2;
    memset(f, 0, sizeof(*f));
    memset(g_xAnimInfo, 0, sizeof(g_xAnimInfo));
    memset(names, 0, sizeof(names));
    nameCount = 0;
    g_end = 99;
    XAnimTree_s *tree = calloc(1, offsetof(XAnimTree_s, infoArray) + count * sizeof(unsigned short));
    assert(tree);
    tree->anims = calloc(1, offsetof(XAnim, entries) + count * sizeof(XAnimEntry));
    assert(tree->anims);
    tree->anims->size = count;
    tree->entnum = 7;
    f->object.tree = tree;
    f->parts.frequency = 1.0f;
    f->parts.bLoop = 1;
    f->parts.notify = f->notes;
    f->parts.notifyCount = 1;
    f->notes[0].name = 11;
    f->notes[0].time = 0.5f;
    for (unsigned int i = 0; i < count; ++i) {
        tree->infoArray[i] = i + 1;
        XAnimInfo *info = &g_xAnimInfo[i + 1];
        info->notifyIndex = -1;
        info->s.time = start;
        info->s.weight = info->s.goalWeight = info->s.rate = 1.0f;
        XAnimEntry *entry = &tree->anims->entries[i];
        if (i + 1 < count) {
            entry->numAnims = 1;
            entry->u.s.children = i + 1;
            entry->u.s.flags = i == 0 && synchronized ? 5 : 0;
        } else {
            entry->u.parts = &f->parts;
            info->notifyName = 20;
        }
    }
}

static void Free(struct Fixture *f)
{
    free(f->object.tree->anims);
    free(f->object.tree);
}

static void Advance(struct Fixture *f, float seconds, int notify)
{
    DObjInitServerTime(&f->object, seconds);
    int slices = 0;
    while (DObjUpdateServerInfo(&f->object, seconds, notify))
        assert(++slices < 16);
}

static void TimeIs(const struct Fixture *f, unsigned int anim, float expected, int cycle)
{
    const XAnimState *s = &g_xAnimInfo[f->object.tree->infoArray[anim]].s;
    assert(fabsf(s->time - expected) < 0.000001f);
    assert(s->timeCount == cycle);
}

static void Crossing(void)
{
    struct Fixture f;
    Init(&f, 0, 1, 0.25f);
    Advance(&f, 0.5f, 1);
    TimeIs(&f, 0, 0.75f, 0);
    TimeIs(&f, 1, 0.75f, 0);
    assert(nameCount == 1 && names[0] == 11);
    Free(&f);
}

static void Wrapping(void)
{
    struct Fixture f;
    Init(&f, 0, 1, 0.75f);
    f.parts.notifyCount = 3;
    f.notes[0].time = 0.125f;
    f.notes[1].time = 0.875f;
    f.notes[1].name = 12;
    f.notes[2].time = 1.0f;
    f.notes[2].name = 13;
    Advance(&f, 0.5f, 1);
    TimeIs(&f, 0, 0.25f, 1);
    TimeIs(&f, 1, 0.25f, 1);
    assert(nameCount == 3 && names[0] == 12 && names[1] == 13 && names[2] == 11);
    Free(&f);
}

static void Ending(void)
{
    struct Fixture f;
    Init(&f, 0, 1, 0.75f);
    f.object.tree->anims->entries[0].u.s.flags = 6;
    f.parts.bLoop = 0;
    f.notes[0].time = 1.0f;
    Advance(&f, 0.5f, 1);
    TimeIs(&f, 0, 1.0f, 0);
    TimeIs(&f, 1, 1.0f, 0);
    /* The last slice starts at time 1 and emits the separate script end event. */
    assert(nameCount == 2 && names[0] == 11 && names[1] == 99);
    Free(&f);
}

static void Controls(void)
{
    for (int gate = 0; gate < 3; ++gate) {
        struct Fixture f;
        Init(&f, 0, 1, 0.25f);
        if (gate == 1)
            f.object.tree->entnum = 0;
        if (gate == 2) {
            g_xAnimInfo[2].s.goalWeight = 0;
            g_xAnimInfo[2].s.goalTime = 1;
        }
        Advance(&f, 0.5f, gate != 0);
        TimeIs(&f, 0, 0.75f, 0);
        TimeIs(&f, 1, 0.75f, 0);
        assert(nameCount == 0);
        Free(&f);
    }
}

static void Unsynchronized(void)
{
    struct Fixture f;
    Init(&f, 0, 0, 0.25f);
    Advance(&f, 0.5f, 1);
    TimeIs(&f, 1, 0.75f, 0);
    assert(nameCount == 1 && names[0] == 11);
    Free(&f);
}

static void Nested(void)
{
    struct Fixture f;
    Init(&f, 1, 1, 0.25f);
    Advance(&f, 0.5f, 1);
    TimeIs(&f, 2, 0.75f, 0);
    assert(nameCount == 1 && names[0] == 11);
    Free(&f);
}

static void Rate(void)
{
    struct Fixture f;
    Init(&f, 0, 1, 0.25f);
    g_xAnimInfo[1].s.rate = 0.5f;
    g_xAnimInfo[2].s.rate = 2.0f;
    f.parts.frequency = 2.0f;
    Advance(&f, 0.25f, 1);
    TimeIs(&f, 0, 0.75f, 0);
    TimeIs(&f, 1, 0.75f, 0);
    assert(nameCount == 1 && names[0] == 11);
    Free(&f);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "crossing")) Crossing();
    else if (!strcmp(argv[1], "wrapping")) Wrapping();
    else if (!strcmp(argv[1], "ending")) Ending();
    else if (!strcmp(argv[1], "controls")) Controls();
    else if (!strcmp(argv[1], "unsynchronized")) Unsynchronized();
    else if (!strcmp(argv[1], "nested")) Nested();
    else if (!strcmp(argv[1], "rate")) Rate();
    else abort();
    printf("sync notification: %s passed\n", argv[1]);
    return 0;
}
