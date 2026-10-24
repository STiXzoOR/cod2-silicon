#include "f1_mover_source.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

level_locals_t level;
static gentity_t entities[2];
void *imp_g_entities = entities;
static int traceCalls, traceMask, tracePass, failures;
static const char *cursorHint;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
void Com_Error(int code, const char *fmt, ...) { abort(); }
void G_TraceCapsule(trace_t *tr, const vec_t *start, const vec_t *mins,
                    const vec_t *maxs, const vec_t *end, int pass, int mask)
{
    ++traceCalls; traceMask = mask; tracePass = pass;
    assert(!memcmp(start, end, sizeof(vec3_t)));
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 1;
}
void SV_SetBrushModel(gentity_t *ent) {}
void SV_LinkEntity(gentity_t *ent) {}
qboolean G_SpawnString(const char *key, const char *fallback, const char **out)
{
    *out = !strcmp(key, "cursorhint") ? cursorHint : fallback;
    return !strcmp(key, "cursorhint");
}
void SV_GetConfigstring(int index, char *buffer, int size) { abort(); }
void SV_SetConfigstring(const int index, const char *value) { abort(); }

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
int main(void)
{
    push(); hints();
    if (failures) return 1;
    puts("movers: collision masks, corpse exclusion, missile owner and cursor hints pass");
    return 0;
}
