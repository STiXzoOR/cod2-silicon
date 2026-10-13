#include "common_types.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static level_locals_t level;
static struct bgs_t level_bgs;
static scr_const_t scr_const;
static gentity_t g_entities[1024];
static gclient_t clients[64];
static struct bgs_t *bgs;
static serverStatic_t svs;
static void *imp_svs = &svs;
static void *imp_bgs = &bgs;
static dvar_t dvars[128];
static int dvar_count, host_kind, broadcast_registrations;
static int frame_end_calls, make_dead_at_end;
static svEntity_t visibility;
#define SCR_CONST() (&scr_const)

static dvar_t *new_dvar(void)
{
    assert(dvar_count < 128);
    return &dvars[dvar_count++];
}

const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, int flags)
{
    dvar_t *dvar = new_dvar();
    dvar->current.integer = value;
    if (!strcmp(name, "dedicated"))
        dvar->current.integer = host_kind;
    if (!strcmp(name, "sv_playerBroadcastLimit")) {
        assert(value == 15 && min == 0 && max == 64 && flags == 0x1000);
        ++broadcast_registrations;
    }
    return dvar;
}

const dvar_t *Dvar_RegisterBool_mac(const char *name, int value, int flags)
{
    (void)name; (void)flags;
    dvar_t *dvar = new_dvar();
    dvar->current.integer = value;
    return dvar;
}

const dvar_t *Dvar_RegisterString_mac(const char *name, const char *value, int flags)
{
    (void)name; (void)flags;
    dvar_t *dvar = new_dvar();
    dvar->current.string = value;
    return dvar;
}

const dvar_t *Dvar_RegisterFloat(const char *name, float value, float min, float max, int flags)
{
    (void)name; (void)min; (void)max; (void)flags;
    dvar_t *dvar = new_dvar();
    dvar->current.value = value;
    return dvar;
}

const dvar_t *Dvar_RegisterColor(const char *name, float r, float g, float b, float a, int flags)
{
    (void)name; (void)r; (void)g; (void)b; (void)a; (void)flags;
    return new_dvar();
}

void SV_DObjInitServerTime(gentity_t *ent, float dt) { (void)ent; (void)dt; }
int G_DObjUpdateServerTime(gentity_t *ent, int notify) { (void)ent; (void)notify; return 0; }
void Com_Memcpy(void *dest, const void *source, int count) { memcpy(dest, source, count); }
void Scr_AddEntity(gentity_t *ent) { (void)ent; abort(); }
void Scr_Notify(gentity_t *ent, unsigned short event, unsigned int count) { (void)ent; (void)event; (void)count; abort(); }
void Scr_RunCurrentThreads(void) {}
void Scr_IncTime(void) {}
static void G_RunFrameForEntity(gentity_t *ent) { (void)ent; }
tagInfo_t *COD2_TagInfoFromHandle(int handle) { assert(handle == 0); return NULL; }
void HudElem_UpdateClient(gclient_t *client, int num, int which) { (void)client; (void)num; (void)which; }
void ClientEndFrame(gentity_t *ent)
{
    ++frame_end_calls;
    if (ent == &g_entities[0] && make_dead_at_end) {
        assert(ent->r.broadcastTime == 0);
        ent->health = 0;
    }
}
void CheckTeamStatus(void) {}
void CheckVote(void) {}
void DeathmatchScoreboardMessage(gentity_t *ent) { (void)ent; }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
const char *SL_ConvertToString(unsigned int id) { (void)id; return "fixture"; }
void Dvar_SetBool(const dvar_t *dvar, qboolean value) { (void)dvar; (void)value; }
void SaveRegisteredWeapons(void) {}
void SaveRegisteredItems(void) {}
void SV_DObjDisplayAnim(gentity_t *ent) { (void)ent; }
byte *SV_SvEntityForGentity(byte *ent) { (void)ent; return (byte *)&visibility; }
int BoxDistSqrdExceeds(const vec_t *mins, const vec_t *maxs, const vec_t *org, float dist)
{
    (void)mins; (void)maxs; (void)org; (void)dist;
    return 1;
}

#include "player_broadcast_source.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#define ACTIVE_BROADCAST 1
#else
#define ACTIVE_BROADCAST 0
#endif

static void reset_entities(int count)
{
    memset(g_entities, 0, sizeof(g_entities));
    memset(&level, 0, sizeof(level));
    memset(clients, 0, sizeof(clients));
    level.gentities = g_entities;
    level.clients = clients;
    level.num_entities = 64;
    level.maxclients = 64;
    for (int i = 0; i < count; ++i) {
        gentity_t *ent = &g_entities[i];
        ent->s.number = i;
        ent->s.eType = 1;
        ent->r.inuse = 1;
        ent->r.linked = 1;
        ent->client = &clients[i];
        ent->health = 100;
    }
    frame_end_calls = make_dead_at_end = 0;
    svs.time = 1000;
}

static void set_limit(int value)
{
    if (sv_playerBroadcastLimit)
        ((dvar_t *)sv_playerBroadcastLimit)->current.integer = value;
}

static void frame(void)
{
    (void)G_RunFrame(svs.time);
    CHECK(level.time == svs.time && bgs == NULL);
}

static int visible(int entity, int observer)
{
    static const byte pvs[1] = { 0 };
    const vec3_t org = { 0, 0, 0 };
    return SV_EntityIsVisibleToClientLocal((byte *)&g_entities[entity], org, pvs, 1, observer);
}

int main(void)
{
    for (host_kind = 0; host_kind <= 1; ++host_kind) {
        dvar_count = broadcast_registrations = 0;
        memset(dvars, 0, sizeof(dvars));
        G_RegisterDvars();
        CHECK(broadcast_registrations == ACTIVE_BROADCAST);
        if (ACTIVE_BROADCAST)
            CHECK(sv_playerBroadcastLimit && sv_playerBroadcastLimit->current.integer == 15);

        const int counts[] = { 0, 15, 16, 64 };
        const int limits[] = { 0, 15, 64 };
        for (unsigned int c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c)
            for (unsigned int l = 0; l < sizeof(limits) / sizeof(limits[0]); ++l) {
                int count = counts[c], limit = limits[l];
                reset_entities(count);
                set_limit(limit);
                frame();
                CHECK(frame_end_calls == count);
                for (int i = 0; i < count; ++i)
                    CHECK(g_entities[i].r.broadcastTime ==
                          ((ACTIVE_BROADCAST && limit > 0 && count <= limit) ? 1001 : 0));
            }

        /* Dead players and an ET_PLAYER spectator still occupy the threshold. */
        reset_entities(16);
        set_limit(15);
        g_entities[15].health = 0;
        clients[14].sess.cs.team = (team_t)3;
        frame();
        CHECK(g_entities[0].r.broadcastTime == 0);
        reset_entities(15);
        g_entities[14].health = -5;
        clients[13].sess.cs.team = (team_t)3;
        frame();
        CHECK(g_entities[14].r.broadcastTime == 0);
        CHECK(g_entities[13].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));

        /* Inactive, clientless and non-player entities neither count nor refresh. */
        reset_entities(16);
        g_entities[15].s.eType = 0;
        frame();
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));
        CHECK(g_entities[15].r.broadcastTime == 0);
        reset_entities(16);
        g_entities[15].r.inuse = 0;
        frame();
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));
        reset_entities(15);
        g_entities[1023].r.inuse = 1;
        g_entities[1023].s.eType = 1;
        g_entities[1023].health = 100;
        frame();
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));
        CHECK(g_entities[1023].r.broadcastTime == 0);
        reset_entities(15);
        g_entities[1023] = g_entities[14];
        g_entities[1023].health = 0;
        frame();
        CHECK(g_entities[0].r.broadcastTime == 0);
        reset_entities(1);
        g_entities[1023] = g_entities[0];
        frame();
        CHECK(g_entities[1023].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));
        reset_entities(64);
        set_limit(64);
        g_entities[1023] = g_entities[0];
        g_entities[0].r.broadcastTime = 3000;
        frame();
        CHECK(g_entities[0].r.broadcastTime == 3000);
        CHECK(g_entities[1].r.broadcastTime == 0);

        reset_entities(2);
        set_limit(15);
        g_entities[0].r.broadcastTime = -1;
        g_entities[1].health = 0;
        g_entities[1].r.broadcastTime = 3000;
        frame();
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : -1));
        CHECK(g_entities[1].r.broadcastTime == 3000);

        reset_entities(2);
        make_dead_at_end = 1;
        frame();
        CHECK(g_entities[0].r.broadcastTime == 0);
        CHECK(g_entities[1].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));

        /* Refresh uses server milliseconds even if the simulation input differs. */
        reset_entities(2);
        (void)G_RunFrame(900);
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));
        svs.time = 1025;
        frame();
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1026 : 0));

        /* Real visibility predicate: PVS-hidden, masks, flags, self and linking. */
        reset_entities(2);
        CHECK(visible(0, 1) == 0);
        frame();
        CHECK(visible(0, 1) == ACTIVE_BROADCAST);
        g_entities[0].r.svFlags = 1;
        g_entities[0].r.clientMask[0] = 2;
        CHECK(visible(0, 1) == ACTIVE_BROADCAST);
        CHECK(g_entities[0].r.svFlags == 1 && g_entities[0].r.clientMask[0] == 2);
        CHECK(visible(0, 0) == 0);
        g_entities[0].r.linked = 0;
        CHECK(visible(0, 1) == 0);
        g_entities[0].r.linked = 1;
        svs.time = 1001;
        CHECK(visible(0, 1) == ACTIVE_BROADCAST);
        CHECK(g_entities[0].r.broadcastTime == (ACTIVE_BROADCAST ? 1001 : 0));
        set_limit(0);
        svs.time = 1025;
        frame();
        CHECK(visible(0, 1) == ACTIVE_BROADCAST);
        CHECK(g_entities[0].r.broadcastTime == 0);
        CHECK(visible(0, 1) == 0);

        reset_entities(2);
        set_limit(0);
        g_entities[0].r.broadcastTime = 900;
        frame();
        CHECK(g_entities[0].r.broadcastTime == 900);
        CHECK(visible(0, 1) == 1 && g_entities[0].r.broadcastTime == 0);
        g_entities[0].r.svFlags = 8;
        CHECK(visible(0, 1) == 1);
        g_entities[0].r.svFlags = 0;
        visibility.numClusters = 1;
        visibility.clusternums[0] = 0;
        CHECK(visible(0, 1) == 0);
        visibility.numClusters = 0;
    }
    if (!failures)
        printf("PASS %s broadcast: real registration/frame/visibility, listen and dedicated contexts\n",
               ACTIVE_BROADCAST ? "CoD2x" : "stock");
    return failures != 0;
}
