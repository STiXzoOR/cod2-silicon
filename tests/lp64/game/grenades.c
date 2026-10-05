/* Run real missile/trajectory/landing functions against synthetic world planes. */
#define G_Spawn FixtureUnused_G_Spawn
#define G_FreeEntity FixtureUnused_G_FreeEntity
#define G_TempEntity FixtureUnused_G_TempEntity
#define G_AddEvent FixtureUnused_G_AddEvent
#include "ws40_g_utils.c"
#undef G_Spawn
#undef G_FreeEntity
#undef G_TempEntity
#undef G_AddEvent
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern gentity_t *fire_grenade(gentity_t *, vec_t *, vec_t *, int, int);
extern void G_RunMissile(gentity_t *);
extern void G_ExplodeMissile(gentity_t *);

level_locals_t level;
scr_const_t scr_const;
gentity_t g_entities[1024];
__asm__(".globl _g_entities_ptr\n.set _g_entities_ptr,_g_entities");
entityHandler_t entityHandlers[20];
byte *pPriorityMap;
static vec3_t zero;
byte *vec3_origin_ptr = (byte *)zero;
static WeaponDef weapon;
static gclient_t client;
static int eventCounts[256], wallHits, floorHits, links;
static int useWall, traceStartSolid;
static float wallX = 20.0f;
static vec3_t explosionOrigin;

WeaponDef *BG_GetWeaponDef(int index) { assert(index == 1); return &weapon; }
gentity_t *G_Spawn(void)
{
    gentity_t *ent = &g_entities[72];
    memset(ent, 0, sizeof(*ent));
    ent->s.number = 72;
    ent->s.groundEntityNum = 1023;
    ent->r.inuse = 1;
    return ent;
}
void Scr_SetString(scr_string_t *to, unsigned int value) { *to = value; }
void Com_Printf(const char *fmt, ...) {}
void Com_Error(int code, const char *fmt, ...) { abort(); }
void SV_LinkEntity(gentity_t *ent) { ++links; }
int SV_PointContents(const vec_t *p, int pass, int mask) { return 0; }
void G_FreeEntity(gentity_t *ent) { ent->r.inuse = 0; }
gentity_t *G_TempEntity(const vec_t *p, int event) { abort(); }
void G_AddEvent(gentity_t *ent, int event, int parm)
{
    assert(event >= 0 && event < 256);
    ++eventCounts[event];
    if (event == 188 || event == 191)
        memcpy(explosionOrigin, ent->r.currentOrigin, sizeof(explosionOrigin));
}
int G_RunThink(gentity_t *ent)
{
    if (ent->nextthink && ent->nextthink <= level.time) {
        ent->nextthink = 0;
        G_ExplodeMissile(ent);
    }
    return 1;
}
void G_LocationalTrace(trace_t *tr, const vec_t *start, const vec_t *end,
                      int skip, int mask, byte *priority)
{
    assert(skip == 0 && (mask & 1));
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 1;
    tr->entityNum = 1023;
    if (traceStartSolid) {
        tr->startsolid = 1;
        tr->entityNum = 1022;
        tr->contents = 1;
        return;
    }
    if (end[2] < start[2] && start[2] >= 0 && end[2] <= 0) {
        tr->fraction = start[2] / (start[2] - end[2]);
        tr->normal[2] = 1;
        tr->entityNum = 1022;
        tr->surfaceFlags = 0x100000; /* asphalt */
        tr->contents = 1;
        ++floorHits;
    }
    if (useWall && end[0] > start[0] && start[0] <= wallX && end[0] >= wallX) {
        float f = (wallX - start[0]) / (end[0] - start[0]);
        if (f < tr->fraction) {
            tr->fraction = f;
            tr->normal[0] = -1;
            tr->normal[2] = 0;
            tr->entityNum = 1022;
            tr->surfaceFlags = 0x600000; /* brick */
            tr->contents = 1;
            ++wallHits;
        }
    }
}
void G_TraceCapsule(trace_t *tr, const vec_t *start, const vec_t *mins,
                    const vec_t *maxs, const vec_t *end, int skip, int mask)
{
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 0;
    tr->normal[2] = 1;
    tr->entityNum = 1022;
    tr->surfaceFlags = 0x100000;
}
qboolean G_RadiusDamage(const vec_t *p, gentity_t *inflictor, gentity_t *attacker,
                        float inner, float outer, float radius, gentity_t *ignore, int mod) { return 0; }
void Server_SwitchToValidFxScheduler(void) { abort(); }
EffectTemplate *FX_RegisterEffect(const char *name) { abort(); }
float FX_GetEffectLength(EffectTemplate *fx) { abort(); }
int LogAccuracyHit(gentity_t *target, gentity_t *attacker) { return 0; }
void G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker,
              vec_t *dir, vec_t *p, int damage, int flags, int mod, int hit, int loc) { abort(); }
void G_CheckHitTriggerDamage(gentity_t *attacker, vec_t *start, vec_t *end, int damage, int mod) {}
void G_GrenadeTouchTriggerDamage(gentity_t *ent, vec_t *old, vec_t *p, int radius, int mod) {}
void SnapVectorTowards(vec_t *v, vec_t *to) { abort(); }

static gentity_t *throw_grenade(int smoke, int cooked)
{
    memset(&weapon, 0, sizeof(weapon));
    for (int i = 0; i < 23; ++i) {
        weapon.parallelBounce[i] = .45f;
        weapon.perpendicularBounce[i] = .45f;
    }
    weapon.projExplosion = smoke ? 2 : 0;
    memset(eventCounts, 0, sizeof(eventCounts));
    floorHits = wallHits = links = 0;
    level.time = 1000;
    level.previousTime = 950;
    entityHandlers[7].methodOfDeath = 3;
    g_entities[0].client = &client;
    client.ps.grenadeTimeLeft = cooked;
    vec3_t start = {0, 0, 12}, delta = {30, 0, -40};
    gentity_t *ent = fire_grenade(&g_entities[0], start, delta, 1, 4000);
    assert(ent->s.eFlags == 0x1000000 && ent->s.pos.trType == TR_GRAVITY);
    assert(ent->s.pos.trTime == 1000 && ent->s.pos.trBase[2] == 12);
    assert(ent->nextthink == 1000 + (cooked ? cooked : 4000));
    assert(client.ps.grenadeTimeLeft == 0);
    return ent;
}
static void frame(gentity_t *ent)
{
    level.previousTime = level.time;
    level.time += 50;
    G_RunMissile(ent);
    assert(ent->r.currentOrigin[2] >= 0);
}
int main(void)
{
    for (int smoke = 0; smoke <= 1; ++smoke) {
        gentity_t *ent = throw_grenade(smoke, 0);
        for (int i = 0; i < 70 && ent->s.pos.trType != TR_STATIONARY; ++i)
            frame(ent);
        assert(floorHits > 0 && links > 0);
        assert(ent->s.pos.trType == TR_STATIONARY); /* bounce damping must reach rest */
        assert(ent->s.groundEntityNum == 1022 && ent->r.currentOrigin[2] == 1.5f);
        assert(eventCounts[187] > 0);
        while (ent->s.eType == 4)
            frame(ent);
        assert(eventCounts[smoke ? 191 : 188] == 1);
        assert(explosionOrigin[2] >= 0 && explosionOrigin[2] <= 2);
    }
    useWall = 1;
    gentity_t *ent = throw_grenade(0, 0);
    ent->s.pos.trBase[2] = ent->r.currentOrigin[2] = 100;
    ent->s.pos.trDelta[0] = 200;
    ent->s.pos.trDelta[2] = 0;
    frame(ent); frame(ent); frame(ent);
    assert(wallHits > 0 && ent->s.pos.trDelta[0] < 0);
    assert(ent->r.currentOrigin[0] < wallX && eventCounts[187] > 0);
    useWall = 0;
    ent = throw_grenade(0, 500);
    for (int i = 0; i < 9; ++i) frame(ent);
    assert(!eventCounts[188]);
    frame(ent);
    assert(eventCounts[188] == 1 && level.time == 1500);
    ent = throw_grenade(0, 0);
    traceStartSolid = 1;
    frame(ent);
    assert(ent->r.currentOrigin[0] <= 0 && ent->r.currentOrigin[0] > -.1f);
    assert(ent->r.currentOrigin[2] >= 12 && ent->r.currentOrigin[2] <= 14);
    assert(ent->s.pos.trDelta[2] > 0); /* effective reverse normal, no pass-through */
    puts("grenades: production spawn, gravity, floor/wall bounce, rest, frag/smoke and cooked fuse pass");
}
