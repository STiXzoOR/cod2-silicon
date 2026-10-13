#include "common_types.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static dvar_t antilag, fps, spreadFix;
const dvar_t *g_antilag = &antilag;
const dvar_t *sv_fps = &fps;
const dvar_t *g_shotgun_spread_fix = &spreadFix;

#include "shotgun_weapon_source.h"

static dvar_t registered[128];
static int registeredCount, spreadRegistrations;

static dvar_t *FixtureRegister(const char *name, int flags)
{
    assert(registeredCount < 128);
    dvar_t *value = &registered[registeredCount++];
    value->name = name;
    value->flags = flags;
    return value;
}

const dvar_t *Dvar_RegisterBool_mac(const char *name, int def, int flags)
{
    if (!strcmp(name, "g_shotgun_spread_fix")) {
        ++spreadRegistrations;
        assert(def == 1 && flags == 0x1080);
        spreadFix.name = name;
        spreadFix.current.enabled = def;
        spreadFix.reset.enabled = def;
        spreadFix.flags = flags;
        return &spreadFix;
    }
    dvar_t *value = FixtureRegister(name, flags);
    value->current.enabled = def;
    return value;
}

const dvar_t *Dvar_RegisterString_mac(const char *name, const char *def, int flags)
{
    dvar_t *value = FixtureRegister(name, flags);
    value->current.string = def;
    return value;
}

const dvar_t *Dvar_RegisterInt(const char *name, int def, int min, int max, int flags)
{
    dvar_t *value = FixtureRegister(name, flags);
    assert(def >= min && def <= max);
    value->current.integer = def;
    return value;
}

const dvar_t *Dvar_RegisterFloat(const char *name, float def, float min, float max, int flags)
{
    dvar_t *value = FixtureRegister(name, flags);
    assert(def >= min && def <= max);
    value->current.value = def;
    return value;
}

const dvar_t *Dvar_RegisterColor(const char *name, float r, float g, float b, float a, int flags)
{
    (void)r; (void)g; (void)b; (void)a;
    return FixtureRegister(name, flags);
}

#include "shotgun_registration_source.h"

struct level_locals_t level;
static gentity_t entities[1024];
static gclient_t clients[2];
static unsigned char priority[1], riflePriority[1];
void *imp_g_entities = entities;
void *imp_bulletPriorityMap = priority;
void *imp_riflePriorityMap = riflePriority;

static weaponParms params;
static WeaponDef weapon;
static float tape[64];
static int randomCalls, traceCalls, damageCalls, triggerCalls;
static int linkCalls, unlinkCalls, historyCalls, lagged, glass;
static int tracePassEntity;
static vec3_t tracedEnds[32], tracedStarts[32], damagePoints[32];
static int dealtDamage[32], damageFlags[32], damageMods[32], damageOffsets[32];
static const float historical[3] = {11.0f, 22.0f, 33.0f};

float randomf(void)
{
    assert(randomCalls < 64);
    return tape[randomCalls++];
}

const vec_t Vec3Normalize(vec_t *v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
    return length;
}

void G_LocationalTrace(trace_t *tr, const vec_t *start, const vec_t *end, int passEntityNum,
                      int contentmask, unsigned char *priorityMap)
{
    assert(traceCalls < 32);
    assert(passEntityNum == tracePassEntity);
    assert(contentmask == 0x2802831);
    assert(priorityMap == (weapon.bRifleBullet ? riflePriority : priority));
    if (lagged)
        assert(!memcmp(entities[1].r.currentOrigin, historical, sizeof(historical)));
    memcpy(tracedStarts[traceCalls], start, sizeof(vec3_t));
    memcpy(tracedEnds[traceCalls], end, sizeof(vec3_t));
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 0.25f;
    tr->entityNum = 1;
    tr->partGroup = HITLOC_TORSO_UPR;
    tr->normal[0] = -1.0f;
    if (glass && !traceCalls) {
        tr->contents = 0x10;
        tr->surfaceFlags = 4;
        tr->entityNum = 2;
    }
    ++traceCalls;
}

void G_CheckHitTriggerDamage(gentity_t *attacker, vec_t *start, vec_t *end, int damage, int mod)
{
    assert(attacker == &entities[0]);
    assert(damage == weapon.damage && mod == (weapon.bRifleBullet ? 2 : 1));
    assert(!memcmp(start, tracedStarts[triggerCalls], sizeof(vec3_t)));
    for (int axis = 0; axis < 3; ++axis)
        assert(fabsf(end[axis] - (start[axis] + (tracedEnds[triggerCalls][axis] - start[axis]) * 0.25f)) < 0.001f);
    ++triggerCalls;
}

void G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker,
              const vec_t *dir, const vec_t *point, int damage, int dflags,
              int mod, hitLocation_t hitLoc, int timeOffset)
{
    assert(target == &entities[1] && inflictor == &entities[0] && attacker == &entities[0]);
    assert(!memcmp(dir, params.forward, sizeof(vec3_t)) && hitLoc == HITLOC_TORSO_UPR);
    assert(damageCalls < 32);
    memcpy(damagePoints[damageCalls], point, sizeof(vec3_t));
    dealtDamage[damageCalls] = damage;
    damageFlags[damageCalls] = dflags;
    damageMods[damageCalls] = mod;
    damageOffsets[damageCalls++] = timeOffset;
}

Bool SV_GetClientPositionAtTime(int clientNum, int gametime, vec_t *pos)
{
    assert(clientNum == 1 && gametime == level.time - 100);
    memcpy(pos, historical, sizeof(historical));
    ++historyCalls;
    return 1;
}

void SV_LinkEntity(gentity_t *ent) { assert(ent == &entities[1]); ++linkCalls; }
void SV_UnlinkEntity(gentity_t *ent) { assert(ent == &entities[1]); ++unlinkCalls; }
gentity_t *G_TempEntity(const vec_t *origin, int event) { (void)origin; (void)event; abort(); }
const byte DirToByte(const vec_t *dir) { (void)dir; return 0; }
int Dvar_GetInt(const char *name) { assert(!strcmp(name, "scr_friendlyfire")); return 0; }
qboolean OnSameTeam(gentity_t *a, gentity_t *b) { (void)a; (void)b; return 1; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; abort(); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
WeaponDef *BG_GetWeaponDef(int index) { assert(index == 3); return &weapon; }
void BG_GetSpreadForWeapon(const playerState_t *ps, int index, float *min, float *max)
{
    assert(ps == &clients[0].ps && index == 3);
    *min = 4.0f;
    *max = 20.0f;
}
void G_GetPlayerViewOrigin(const gentity_t *ent, vec_t *origin)
{
    assert(ent == &entities[0]);
    memcpy(origin, params.muzzleTrace, sizeof(vec3_t));
}
void AngleVectors(const vec_t *angles, vec_t *forward, vec_t *right, vec_t *up)
{
    assert(angles[0] == 17.0f && angles[1] == 31.0f && angles[2] == 9.0f);
    memcpy(forward, params.forward, sizeof(vec3_t));
    memcpy(right, params.right, sizeof(vec3_t));
    memcpy(up, params.up, sizeof(vec3_t));
}
gentity_t *fire_rocket(gentity_t *ent, vec_t *start, vec_t *dir)
{ (void)ent; (void)start; (void)dir; abort(); }
gentity_t *fire_grenade(gentity_t *ent, vec_t *start, vec_t *velocity, int type, int fuse)
{ (void)ent; (void)start; (void)velocity; (void)type; (void)fuse; abort(); }

static void Reset(int count, int enabled, float range, int rotatedBasis, int useLag)
{
    memset(entities, 0, sizeof(entities));
    memset(clients, 0, sizeof(clients));
    memset(&weapon, 0, sizeof(weapon));
    memset(&params, 0, sizeof(params));
    memset(&level, 0, sizeof(level));
    randomCalls = traceCalls = damageCalls = triggerCalls = 0;
    linkCalls = unlinkCalls = historyCalls = glass = 0;
    lagged = useLag;
    tracePassEntity = 4;
    level.time = 4000;
    level.maxclients = 2;
    level.clients = clients;
    clients[1].sess.connected = CON_CONNECTED;
    clients[1].sess.sessionState = SESS_STATE_PLAYING;
    for (int i = 0; i < 5; ++i)
        entities[i].s.number = i;
    entities[0].client = &clients[0];
    entities[1].client = &clients[1];
    entities[1].takedamage = 1;
    entities[1].r.currentOrigin[0] = 101.0f;
    entities[1].r.currentOrigin[1] = 102.0f;
    entities[1].r.currentOrigin[2] = 103.0f;
    antilag.current.enabled = useLag;
    g_antilag = &antilag;
    fps.current.integer = 20;
    spreadFix.current.enabled = enabled;
    weapon.weapClass = WEAPCLASS_SPREAD;
    weapon.shotCount = count;
    weapon.fMinDamageRange = range;
    weapon.fMaxDamageRange = range * 0.1f;
    weapon.damage = 83;
    weapon.minDamage = 20;
    params.weapDef = &weapon;
    params.muzzleTrace[0] = 7.0f;
    params.muzzleTrace[1] = -11.0f;
    params.muzzleTrace[2] = 19.0f;
    if (rotatedBasis) {
        static const float basis[3][3] = {
            {1.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f},
            {2.0f / 3.0f, 1.0f / 3.0f, -2.0f / 3.0f},
            {-2.0f / 3.0f, 2.0f / 3.0f, -1.0f / 3.0f},
        };
        memcpy(params.forward, basis[0], sizeof(vec3_t));
        memcpy(params.right, basis[1], sizeof(vec3_t));
        memcpy(params.up, basis[2], sizeof(vec3_t));
    } else {
        params.forward[0] = params.right[1] = params.up[2] = 1.0f;
    }
    for (int i = 0; i < 64; ++i)
        tape[i] = (float)((i * 17 + 3) % 61) / 61.0f;
}

/* Endpoint expectations are geometric facts from the pinned behavioral
   contract, expressed separately from the production pattern construction. */
static const double normalized[2][8][2] = {
    {{0, -0.8}, {0, 0}, {0, 0.4}, {0, -0.4}, {-0.4, 0}, {0.4, 0}, {-0.6, 0.6}, {0.6, 0.6}},
    {{-0.1, 0}, {0.1, 0}, {0, 0.5}, {0, -0.5}, {-0.8, 0.6}, {0.8, 0.6}, {-0.8, -0.6}, {0.8, -0.6}},
};

static void ExpectedEndpoint(int pellet, int patterned, int quarterTurns, int variant,
                             float spread, float range, double *end)
{
    double x, y;
    if (patterned) {
        x = normalized[variant][pellet][0];
        y = normalized[variant][pellet][1];
        for (int i = 0; i < quarterTurns; ++i) {
            double saved = x;
            x = -y;
            y = saved;
        }
    } else {
        double angle = (double)tape[pellet * 2] * 2.0 * 3.14159265358979323846;
        x = (double)tape[pellet * 2 + 1] * cos(angle);
        y = (double)tape[pellet * 2 + 1] * sin(angle);
    }
    double radius = tan((double)spread * 3.14159265358979323846 / 180.0) * range;
    for (int axis = 0; axis < 3; ++axis)
        end[axis] = params.muzzleTrace[axis] + range * (double)params.forward[axis]
                  + x * radius * params.right[axis] + y * radius * params.up[axis];
}

static void VerifyDispatch(int count, int patterned, int quarterTurns, int variant,
                           float spread, float range, int timeOffset)
{
    int expectedCalls = patterned ? 2 : count * 2;
    if (randomCalls != expectedCalls) {
        fprintf(stderr, "pellets=%d pattern=%d: expected %d RNG values, observed %d\n",
                count, patterned, expectedCalls, randomCalls);
        abort();
    }
    assert(traceCalls == count && damageCalls == count && triggerCalls == count);
    for (int i = 0; i < count; ++i) {
        double end[3], distanceSquared = 0;
        ExpectedEndpoint(i, patterned, quarterTurns, variant, spread, range, end);
        for (int axis = 0; axis < 3; ++axis) {
            assert(fabs(tracedEnds[i][axis] - end[axis]) < 0.004);
            assert(tracedStarts[i][axis] == params.muzzleTrace[axis]);
            double delta = (end[axis] - params.muzzleTrace[axis]) * 0.25;
            assert(fabs(damagePoints[i][axis] - (params.muzzleTrace[axis] + delta)) < 0.002);
            distanceSquared += delta * delta;
        }
        double distance = sqrt(distanceSquared);
        double fraction = (distance - weapon.fMaxDamageRange) /
                          (weapon.fMinDamageRange - weapon.fMaxDamageRange);
        int expectedDamage = distance < weapon.fMaxDamageRange ? weapon.damage :
                             distance >= weapon.fMinDamageRange ? weapon.minDamage :
                             (int)(weapon.damage * (1.0 - fraction) + weapon.minDamage * fraction);
        if (dealtDamage[i] != expectedDamage) {
            fprintf(stderr, "pellet=%d spread=%g range=%g distance=%g: damage %d, expected %d\n",
                    i, spread, range, distance, dealtDamage[i], expectedDamage);
            abort();
        }
        assert(damageFlags[i] == (weapon.armorPiercing ? 2 : 0));
        assert(damageMods[i] == 1 && damageOffsets[i] == timeOffset);
    }
    if (lagged) {
        assert(historyCalls == 1 && unlinkCalls == 2 && linkCalls == 2);
        assert(entities[1].r.currentOrigin[0] == 101.0f);
        assert(entities[1].r.currentOrigin[1] == 102.0f);
        assert(entities[1].r.currentOrigin[2] == 103.0f);
    } else {
        assert(!historyCalls && !unlinkCalls && !linkCalls);
    }
    assert(randomf() == tape[expectedCalls] && randomCalls == expectedCalls + 1);
}

int main(void)
{
#ifndef FIXTURE_SKIP_REGISTRATION
    G_RegisterDvars();
#if defined(COD2_CODX) && COD2_CODX
    assert(spreadRegistrations == 1 && g_shotgun_spread_fix == &spreadFix);
    assert(spreadFix.current.enabled && spreadFix.reset.enabled && spreadFix.flags == 0x1080);
#else
    assert(spreadRegistrations == 0);
#endif
#endif
    int cases = 0;
    for (int variant = 0; variant < 2; ++variant) {
        for (int turn = 0; turn < 5; ++turn) {
            for (int basis = 0; basis < 2; ++basis) {
                for (int rangeIndex = 0; rangeIndex < 3; ++rangeIndex) {
                    for (int zero = 0; zero < 2; ++zero) {
                        float range = (float[]){256.0f, 512.0f, 2048.0f}[rangeIndex];
                        float spread = zero ? 0.0f : 12.0f;
                        Reset(8, 1, range, basis, cases & 1);
                        tape[0] = turn * 0.25f;
                        tape[1] = variant ? 0.5f : 0.499f;
                        weapon.armorPiercing = cases & 1;
                        Bullet_Fire((gentity_s (*)[16])&entities[0], spread, &params,
                                    (gentity_s (*)[16])&entities[4], level.time - (lagged ? 100 : 25));
#if defined(COD2_CODX) && COD2_CODX
                        VerifyDispatch(8, 1, turn, variant, spread, range, lagged ? 100 : 25);
#else
                        VerifyDispatch(8, 0, turn, variant, spread, range, lagged ? 100 : 25);
#endif
                        ++cases;
                    }
                }
            }
        }
    }
    const int counts[] = {0, 1, 7, 8, 9};
    for (int enabled = 0; enabled < 2; ++enabled) {
        for (unsigned index = 0; index < sizeof(counts) / sizeof(counts[0]); ++index) {
            if (enabled && counts[index] == 8)
                continue;
            Reset(counts[index], enabled, 1024.0f, 1, 0);
            Bullet_Fire((gentity_s (*)[16])&entities[0], 6.0f, &params,
                        (gentity_s (*)[16])&entities[4], level.time);
            VerifyDispatch(counts[index], 0, 0, 0, 6.0f, 1024.0f, 0);
            ++cases;
        }
    }
    Reset(8, 1, 512.0f, 1, 0);
    weapon.weapClass = WEAPCLASS_RIFLE;
    Bullet_Fire((gentity_s (*)[16])&entities[0], 6.0f, &params,
                (gentity_s (*)[16])&entities[4], level.time);
    VerifyDispatch(1, 0, 0, 0, 6.0f, 8192.0f, 0);
    ++cases;

    for (int enabled = 0; enabled < 2; ++enabled) {
        for (int ads = 0; ads < 2; ++ads) {
            Reset(8, enabled, 1024.0f, 1, 1);
            tracePassEntity = 0;
            tape[0] = 0.75f;
            tape[1] = 0.5f;
            entities[0].s.weapon = 3;
            clients[0].fGunPitch = 17.0f;
            clients[0].fGunYaw = 31.0f;
            clients[0].ps.viewangles[2] = 9.0f;
            clients[0].currentAimSpreadScale = 0.25f;
            clients[0].ps.fWeaponPosFrac = ads ? 1.0f : 0.0f;
            weapon.fAdsSpread = 2.0f;
            FireWeaponAntiLag((gentity_s (*)[16])&entities[0], level.time - 100);
#if defined(COD2_CODX) && COD2_CODX
            VerifyDispatch(8, enabled, 3, 1, ads ? 6.5f : 8.0f, 1024.0f, 100);
#else
            VerifyDispatch(8, 0, 3, 1, ads ? 6.5f : 8.0f, 1024.0f, 100);
#endif
            ++cases;
        }
    }

    Reset(1, 1, 512.0f, 0, 0);
    glass = 1;
    Bullet_Fire((gentity_s (*)[16])&entities[0], 6.0f, &params,
                (gentity_s (*)[16])&entities[4], level.time - 25);
    assert(randomCalls == 2 && traceCalls == 2 && triggerCalls == 2 && damageCalls == 1);
    assert(!memcmp(tracedEnds[0], tracedEnds[1], sizeof(vec3_t)));
    assert(tracedStarts[1][0] > tracedStarts[0][0]);
    assert(damageFlags[0] == 0 && damageMods[0] == 1 && damageOffsets[0] == 25);
    ++cases;
    printf("PASS %d authoritative shotgun/control endpoint and damage cases; RNG, hip/ADS fire dispatch, lag compensation and glass recursion", cases);
#ifndef FIXTURE_SKIP_REGISTRATION
    printf("; normal server dvar registration");
#endif
    puts("");
    return 0;
}
