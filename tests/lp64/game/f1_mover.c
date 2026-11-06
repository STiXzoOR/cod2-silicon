#include "f1_mover_source.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

level_locals_t level;
static gentity_t entities[2];
void *imp_g_entities = entities;
static int traceCalls, traceMask, tracePass, failures, solidTraces;
static const char *cursorHint, *hintString;
static char configstrings[32][64];
static int errorCalls, errorCode, errorValue, configWrites;
static const char *errorFormat;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
void Com_Error(int code, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    errorValue = va_arg(args, int);
    va_end(args);
    ++errorCalls; errorCode = code; errorFormat = fmt;
}
void G_TraceCapsule(trace_t *tr, const vec_t *start, const vec_t *mins,
                    const vec_t *maxs, const vec_t *end, int pass, int mask)
{
    ++traceCalls; traceMask = mask; tracePass = pass;
    assert(!memcmp(start, end, sizeof(vec3_t)));
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 1;
    tr->startsolid = traceCalls <= solidTraces;
}
void SV_SetBrushModel(gentity_t *ent) {}
void SV_LinkEntity(gentity_t *ent) {}
qboolean G_SpawnString(const char *key, const char *fallback, const char **out)
{
    const char *value = !strcmp(key, "cursorhint") ? cursorHint : hintString;
    *out = value ? value : fallback;
    return value != NULL;
}
void SV_GetConfigstring(int index, char *buffer, int size)
{
    assert(index >= 0x4fe && index < 0x4fe + 32 && size == 1024);
    strcpy(buffer, configstrings[index - 0x4fe]);
}
void SV_SetConfigstring(const int index, const char *value)
{
    assert(index >= 0x4fe && index < 0x4fe + 32);
    strcpy(configstrings[index - 0x4fe], value);
    ++configWrites;
}

static void push(void)
{
    int contents[] = {0, 4, 0x04000000};
    for (int custom = 0; custom < 2; ++custom)
    for (int c = 0; c < 3; ++c)
    for (int missile = 0; missile < 2; ++missile) {
        memset(entities, 0, sizeof(entities));
        gentity_t *ent = &entities[0], *pusher = &entities[1];
        ent->s.number = 7; pusher->s.number = 8;
        ent->s.eType = missile ? 4 : 0;
        ent->r.ownerNum = 9;
        ent->clipmask = custom ? 0x20001 : 0;
        ent->r.contents = contents[c];
        ent->s.groundEntityNum = 8;
        vec3_t move = {1,2,3}, rotate = {0,0,0};
        pushed_p = pushed;
        traceCalls = traceMask = tracePass = 0;
        CHECK(G_TryPushingEntity(ent, pusher, move, rotate));
        CHECK(!memcmp(ent->r.currentOrigin, move, sizeof(move)));
        CHECK(!memcmp(ent->s.pos.trBase, move, sizeof(move)));
        CHECK(ent->s.groundEntityNum == 8);
        CHECK(pushed_p == pushed + 1);
        int ignoredCorpse = custom && c == 2;
        CHECK(traceCalls == !ignoredCorpse);
        if (!ignoredCorpse) {
            CHECK(traceMask == (custom ? 0x20001 : 0x811));
            CHECK(tracePass == (missile ? 9 : 7));
        }
    }
}
static void hints(void)
{
    const char *hints[] = {"HINT_INHERIT", "HINT_HEALTH"};
    const int expected[] = {-1, 3};
    for (int i = 0; i < 2; ++i) {
        memset(entities, 0, sizeof(entities));
        cursorHint = hints[i];
        trigger_use(&entities[0]);
        CHECK(entities[0].s.dmgFlags == expected[i]);
        CHECK(entities[0].s.scale == 255);
        CHECK(entities[0].handler == 18);
        CHECK(entities[0].trigger.singleUserEntIndex == 1023);
    }
}
/* Mac 0x1ba537-0x1ba608: reuse, first-empty allocation and the fatal overflow text at 0x2e13f8. */
static void hint_strings(void)
{
    static const struct { const char *text; int slot, writes; } steps[] = {
        {"first", 0, 1}, {"second", 1, 2}, {"first", 0, 2},
    };
    cursorHint = NULL;
    memset(configstrings, 0, sizeof(configstrings));
    configWrites = errorCalls = 0;
    for (int i = 0; i < 3; ++i) {
        memset(entities, 0, sizeof(entities));
        hintString = steps[i].text;
        trigger_use(&entities[0]);
        CHECK(entities[0].s.scale == steps[i].slot);
        CHECK(entities[0].s.dmgFlags == 2);
        CHECK(configWrites == steps[i].writes);
    }
    CHECK(!errorCalls);
    for (int i = 0; i < 32; ++i)
        snprintf(configstrings[i], sizeof(configstrings[i]), "full%d", i);
    memset(entities, 0, sizeof(entities));
    hintString = "overflow";
    const int writesBeforeOverflow = configWrites;
    trigger_use(&entities[0]);
    CHECK(configWrites == writesBeforeOverflow);
    CHECK(errorCalls == 1 && errorCode == 1 && errorValue == 32);
    CHECK(!strcmp(errorFormat, "\x15" "Too many different hintstring key values on trigger_use entities. Max allowed is %i"));
    CHECK(entities[0].s.scale == 255);
    hintString = NULL;
}

/* Mac 0x1ba8b4-0x1bac68: Z 0 runs once, then -Z and +Z, each over -X/+X and -Y/+Y; old position last. */
static void jitter(void)
{
    static const struct { int solid; float x, y, z; int pushed, ground; } cases[] = {
        {1, -4, -4, 0, 1, 8}, {2, -4, 4, 0, 1, 8}, {3, 4, -4, 0, 1, 8}, {4, 4, 4, 0, 1, 8},
        {5, -4, -4, -4, 1, 8}, {8, 4, 4, -4, 1, 8}, {9, -4, -4, 4, 1, 8}, {12, 4, 4, 4, 1, 8},
        {13, 0, 0, 0, 0, 1023},
    };
    for (int i = 0; i < 9; ++i) {
        memset(entities, 0, sizeof(entities));
        gentity_t *ent = &entities[0], *pusher = &entities[1];
        ent->s.number = 7; pusher->s.number = 8;
        ent->s.groundEntityNum = 8;
        ent->r.maxs[0] = 16;
        vec3_t move = {1, 2, 3}, rotate = {0, 0, 0};
        pushed_p = pushed;
        traceCalls = 0;
        solidTraces = cases[i].solid;
        CHECK(G_TryPushingEntity(ent, pusher, move, rotate));
        CHECK(traceCalls == cases[i].solid + 1);
        /* Candidates surround the moved position; the fallback keeps the old one. */
        const float moved = cases[i].pushed ? 1 : 0;
        CHECK(ent->r.currentOrigin[0] == cases[i].x + moved);
        CHECK(ent->r.currentOrigin[1] == cases[i].y + 2 * moved);
        CHECK(ent->r.currentOrigin[2] == cases[i].z + 3 * moved);
        CHECK(pushed_p == pushed + cases[i].pushed);
        CHECK(ent->s.groundEntityNum == cases[i].ground);
    }
    memset(entities, 0, sizeof(entities));
    entities[0].r.maxs[0] = 16;
    vec3_t move = {0, 0, 0}, rotate = {0, 0, 0};
    pushed_p = pushed;
    traceCalls = 0;
    solidTraces = 14;
    CHECK(!G_TryPushingEntity(&entities[0], &entities[1], move, rotate));
    CHECK(pushed_p == pushed);
    solidTraces = 0;
}

int main(void)
{
    push(); hints(); hint_strings(); jitter();
    if (failures) return 1;
    puts("movers: collision masks, corpse exclusion, missile owner, cursor hints, hint strings and jitter pass");
    return 0;
}
