/* Exercise Mac missile branch boundaries through the production entry points. */
#define main GrenadeIntegrationMain
#include "grenades.c"
#undef main
#include <math.h>

extern void Rand_Init(int seed);
extern float flrand(float min, float max);
extern const vec_t Vec3Normalize(vec_t *v);

static gentity_t *missile(void)
{
    gentity_t *ent = throw_grenade(0, 0);
    ent->s.pos.trType = TR_LINEAR;
    ent->s.pos.trBase[2] = ent->r.currentOrigin[2] = 100;
    ent->s.pos.trDelta[0] = 100;
    ent->s.pos.trDelta[1] = ent->s.pos.trDelta[2] = 0;
    ent->nextthink = 0;
    traceStartSolid = useWall = 0;
    scriptedTraceCount = scriptedTraceIndex = 0;
    triggerCalls = triggerDamage = 0;
    return ent;
}

static void trace_result(int index, float fraction, float normalZ)
{
    trace_t *tr = &scriptedTraces[index];
    memset(tr, 0, sizeof(*tr));
    tr->fraction = fraction;
    tr->normal[2] = normalZ;
    tr->entityNum = fraction == 1 ? 1023 : 1022;
    tr->surfaceFlags = 0x100000;
    scriptedTraceCount = index + 1;
}

static void smoke_class(void)
{
    gentity_t *ent = missile();
    weapon.offhandClass = OFFHAND_CLASS_SMOKE_GRENADE;
    weapon.projExplosion = 0;
    G_ExplodeMissile(ent);
    assert(ent->s.eType == 4 && ent->nextthink == 50 && !eventCounts[188]);
    ent->s.groundEntityNum = 1022;
    G_ExplodeMissile(ent);
    assert(ent->s.eType == 0 && eventCounts[188] == 1);

    ent = missile();
    weapon.projExplosion = 2;
    G_ExplodeMissile(ent);
    assert(ent->s.eType == 0 && eventCounts[191] == 1);
}

static void tangent_bounce(void)
{
    gentity_t *ent = missile();
    weapon.parallelBounce[1] = .25f;
    trace_result(0, .5f, 0);
    scriptedTraces[0].normal[1] = 1;
    /* Keep damping independent of whether a down trace is requested. */
    trace_result(1, 1, 0);
    frame(ent);
    assert(ent->s.pos.trDelta[0] == 25 && ent->s.pos.trDelta[1] == 0);
    assert(ent->s.pos.trType == TR_LINEAR);
}

static void movement_threshold(void)
{
    const float speeds[] = {0, .01f, .02f, .04f};
    for (unsigned i = 0; i < sizeof(speeds) / sizeof(speeds[0]); ++i) {
        gentity_t *ent = missile();
        ent->s.eFlags = 0x400;
        ent->s.pos.trDelta[0] = speeds[i];
        trace_result(0, 1, 0);
        frame(ent);
        assert(links == (i >= 2));
        assert(ent->r.currentOrigin[0] == (i >= 2 ? speeds[i] * .05f : 0));
    }
    gentity_t *ent = missile();
    ent->s.pos.trDelta[0] = .01f;
    ent->nextthink = 1050;
    frame(ent);
    assert(eventCounts[188] == 1);
}

static void down_trace(void)
{
    const float fractions[] = {1, .5f, .5f};
    const float normals[] = {0, 0, 1};
    for (int i = 0; i < 3; ++i) {
        gentity_t *ent = missile();
        trace_result(0, fractions[i], normals[i]);
        trace_result(1, 1, 0);
        frame(ent);
        assert(scriptedTraceIndex == (i == 1 ? 1 : 2));
    }
}

static void landing_base(void)
{
    gentity_t *ent = missile();
    ent->s.origin2[0] = 17;
    trace_result(0, .5f, 1);
    trace_result(1, .5f, 1);
    frame(ent);
    assert(linkedBase[2] == 100.75f);
    assert(linkedOrigin2[0] == 17);
    assert(ent->r.currentOrigin[2] == 100.75f);
}

static void trigger_damage(void)
{
    gentity_t *ent = missile();
    ent->s.eFlags = 0x400;
    weapon.iExplosionInnerDamage = 250;
    weapon.iExplosionRadius = 100;
    frame(ent);
    assert(triggerCalls == 1 && triggerDamage == 250);
}

static void rocket_deadline(void)
{
    for (int offset = -1; offset <= 1; ++offset) {
        gentity_t *ent = missile();
        ent->s.eFlags = 0x400;
        weapon.weapType = WEAPTYPE_PROJECTILE;
        weapon.weapClass = 2;
        weapon.iProjectileSpeed = 200;
        weapon.destabilizationBaseTime = .2f;
        ent->grenade.time = 50 + offset;
        frame(ent);
        assert(!!(ent->flags & 0x10000) == (offset < 0));
        assert(ent->s.pos.trTime == (offset < 0 ? 1050 : 1000));
    }
}

static void rocket_type(void)
{
    for (int projectile = 0; projectile <= 1; ++projectile) {
        gentity_t *ent = missile();
        ent->s.eFlags = 0x400;
        weapon.weapType = projectile ? WEAPTYPE_PROJECTILE : WEAPTYPE_BULLET;
        weapon.weapClass = projectile ? 0 : 2;
        weapon.iProjectileSpeed = 200;
        ent->grenade.time = 0;
        frame(ent);
        assert(!!(ent->flags & 0x10000) == projectile);
    }
}

static void rocket_angle(void)
{
    gentity_t *ent = missile();
    ent->s.eFlags = 0x400;
    weapon.weapType = WEAPTYPE_PROJECTILE;
    weapon.weapClass = 2;
    weapon.iProjectileSpeed = 200;
    weapon.destabilizationAngleMax = .043f;
    weapon.fAdsAimPitch = 45;
    weapon.destabilizationBaseTime = .2f;
    weapon.destabilizationTimeReductionRatio = .5f;
    ent->grenade.time = 0;
    float scale = tanf((float)((double)weapon.destabilizationAngleMax * 0.017453292519943295));
    vec3_t expected;
    Rand_Init(123);
    for (int i = 0; i < 3; ++i)
        expected[i] = flrand(-1, 1) * scale + (i == 0 ? 1 : 0);
    Vec3Normalize(expected);
    for (int i = 0; i < 3; ++i)
        expected[i] *= weapon.iProjectileSpeed;
    Rand_Init(123);
    frame(ent);
    assert(!memcmp(expected, ent->s.pos.trDelta, sizeof(expected)));
    assert(ent->grenade.time == 200);
    level.time += 201;
    G_RunMissile(ent);
    assert(ent->grenade.time == 100);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "smoke")) smoke_class();
    else if (!strcmp(argv[1], "tangent")) tangent_bounce();
    else if (!strcmp(argv[1], "threshold")) movement_threshold();
    else if (!strcmp(argv[1], "downtrace")) down_trace();
    else if (!strcmp(argv[1], "landing")) landing_base();
    else if (!strcmp(argv[1], "trigger")) trigger_damage();
    else if (!strcmp(argv[1], "deadline")) rocket_deadline();
    else if (!strcmp(argv[1], "type")) rocket_type();
    else if (!strcmp(argv[1], "angle")) rocket_angle();
    else return 2;
    printf("F1 missile %s: pass\n", argv[1]);
    return 0;
}
