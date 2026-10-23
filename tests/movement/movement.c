#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmove_source.h"
#include "dvars.h"

static bgs_t fixture_bgs;
bgs_t *bgs = &fixture_bgs;
static WeaponDef weapon;
static int animation, eventCount, lastEvent, slideCount;
static int animationCalls, animationBob, failAnimation, stanceCalls, stanceEvent;
static float traceFraction = 1.0f;
static int mantleChecks, weaponCalls;
static int ladderSurface;
static void (*fixtureTrace)(trace_t *, const vec_t *, const vec_t *, const vec_t *);
WeaponDef *BG_GetWeaponDef(int index) { (void)index; return &weapon; }
int BG_AnimScriptAnimation(playerState_t *ps, aistateEnum_t state, scriptAnimMoveTypes_t move, qboolean cont)
{ (void)state; (void)cont; animation = move; animationBob = ps->bobCycle; ++animationCalls; if (failAnimation) { failAnimation = 0; return -1; } return 1; }
void BG_UpdateConditionValue(int client, int condition, int value, qboolean bits)
{ (void)client; (void)condition; (void)value; (void)bits; }
void BG_AddPredictableEventToPlayerstate(int event, int parm, playerState_t *ps)
{ ps->events[ps->eventSequence & 3] = event; ps->eventParms[ps->eventSequence & 3] = parm; ++ps->eventSequence; ++eventCount; lastEvent = event; }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
#ifndef MOVEMENT_JUMP
Bool Jump_Check(pmove_t *pm, pml_t *pml) { (void)pm; (void)pml; return 0; }
float Jump_ReduceFriction(playerState_t *ps) { (void)ps; return 1.0f; }
void Jump_ApplySlowdown(playerState_t *ps) { (void)ps; }
#endif
#ifndef MOVEMENT_COLLISION
void PM_StepSlideMove(pmove_t *pm, pml_t *pml, qboolean gravity)
{ (void)gravity; ++slideCount; for (int i = 0; i < 3; ++i) pm->ps->origin[i] += pm->ps->velocity[i] * pml->frametime; }
#endif
void CG_TraceCapsule(trace_t *tr, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int skip, int mask)
{ (void)mins; (void)maxs; (void)skip; (void)mask; if (fixtureTrace) { fixtureTrace(tr, start, end, maxs); return; } memset(tr, 0, sizeof(*tr)); tr->fraction = traceFraction; tr->surfaceFlags = ladderSurface; tr->normal[0] = -1; tr->entityNum = 1022; }
void G_TraceCapsule(trace_t *tr, const vec_t *s, const vec_t *mi, const vec_t *ma, const vec_t *e, int skip, int mask)
{ CG_TraceCapsule(tr, s, mi, ma, e, skip, mask); }
int CG_PointContents(const vec_t *p, int skip, int mask) { (void)p; (void)skip; (void)mask; return 0; }
int SV_PointContents(const vec_t *p, int skip, int mask) { return CG_PointContents(p, skip, mask); }
void G_PlayerEvent(int client, int event) { (void)client; (void)event; }
#include "snap_source.h"
void BG_AnimUpdatePlayerStateConditions(pmove_t *pm) { (void)pm; }
int BG_AnimScriptEvent(playerState_t *ps, scriptAnimEventTypes_t event, qboolean cont, qboolean force)
{ (void)ps; (void)cont; (void)force; ++stanceCalls; stanceEvent = event; return 0; }
void PM_AdjustAimSpreadScale(pmove_t *pm, pml_t *pml) { (void)pm; (void)pml; }
void PM_UpdateAimDownSightFlag(pmove_t *pm, pml_t *pml) { (void)pm; (void)pml; }
void PM_UpdateAimDownSightLerp(pmove_t *pm, pml_t *pml) { (void)pm; (void)pml; }
void PM_Weapon(pmove_t *pm, pml_t *pml) { (void)pm; (void)pml; ++weaponCalls; }
void PM_ResetWeaponState(playerState_t *ps) { (void)ps; }
int PM_WeaponAmmoAvailable(playerState_t *ps) { (void)ps; return 1; }
qboolean PM_InteruptWeaponWithProneMove(playerState_t *ps) { (void)ps; return 0; }
void PM_ExitAimDownSight(playerState_t *ps) { ps->pm_flags &= ~0x40; }
#ifndef MOVEMENT_JUMP
void Jump_ClearState(playerState_t *ps) { ps->pm_flags &= ~0x80000; ps->jumpOriginZ = 0; }
void Jump_ActivateSlowdown(playerState_t *ps) { (void)ps; }
#endif
#ifndef MOVEMENT_COLLISION
void Mantle_CapView(playerState_t *ps) { (void)ps; }
void Mantle_ClearHint(playerState_t *ps) { ps->mantleState.flags &= ~8; }
void Mantle_Check(pmove_t *pm, pml_t *pml) { (void)pm; (void)pml; ++mantleChecks; }
void Mantle_Move(pmove_t *pm, playerState_t *ps, pml_t *pml) { (void)pm; (void)ps; (void)pml; }
#endif
int BG_PlayAnim(playerState_t *ps, int anim, animBodyPart_t part, int duration, qboolean timer, qboolean cont, qboolean force)
{ (void)ps; (void)anim; (void)part; (void)duration; (void)timer; (void)cont; (void)force; return 0; }
qboolean BG_CheckProne(int pass, const vec_t *pos, float size, float height, float yaw, float *torsoHeight, float *torsoPitch, float *waistPitch, qboolean already, qboolean ground, vec_t *normal, int handler, proneCheckType_t check, float feet)
{ (void)pass; (void)pos; (void)size; (void)height; (void)yaw; (void)torsoHeight; (void)torsoPitch; (void)waistPitch; (void)already; (void)ground; (void)normal; (void)handler; (void)check; (void)feet; return 1; }
#ifdef GAMEPLAY_MATRIX
void Fixture_SetRevision(int revision);
#elif defined(COD2_CODX) && COD2_CODX
int Cod2x_GameVersion(void) { return 3; }
#endif

static void close_float(float actual, float expected)
{
    if (fabsf(actual - expected) > 0.00003f) {
        fprintf(stderr, "expected %.9g, got %.9g\n", expected, actual);
        assert(0);
    }
}

static void init(playerState_t *ps, pmove_t *pm, pml_t *pml, int stance)
{
    memset(ps, 0, sizeof(*ps)); memset(pm, 0, sizeof(*pm)); memset(pml, 0, sizeof(*pml));
    ps->speed = 190; ps->groundEntityNum = 1022; ps->viewHeightTarget = stance;
    ps->mins[0] = ps->mins[1] = -15; ps->maxs[0] = ps->maxs[1] = 15; ps->maxs[2] = 70;
    ps->viewHeightCurrent = (float)stance; ps->weapon = 1;
    pm->ps = ps; pm->cmd.forwardmove = 127;
    pml->forward[0] = 1; pml->right[1] = -1; pml->groundTrace.normal[2] = 1;
    pml->groundTrace.surfaceFlags = 7 << 20; pml->walking = pml->groundPlane = 1;
    pml->msec = 3; pml->frametime = 0.003f;
    weapon.fMoveSpeedScale = 1.0f; animation = 0; eventCount = 0; slideCount = 0;
    animationCalls = 0; failAnimation = 0;
}

#if defined(COD2_X64)
static void test_acceleration_zero(void)
{
    /* Literal original Mac 1.3 PM_Accelerate results: all signed-zero MAXSS
       ties, crossed with acceleration and frame time +0/1. */
    static const unsigned int cases[][5] = {
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
        {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x80000000u, 0x00000000u, 0x3f800000u, 0x00000000u},
        {0x00000000u, 0x80000000u, 0x3f800000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x80000000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
        {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
        {0x80000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x80000000u},
        {0x80000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x80000000u},
        {0x80000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x80000000u},
        {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u},
        {0x80000000u, 0x80000000u, 0x00000000u, 0x3f800000u, 0x80000000u},
        {0x80000000u, 0x80000000u, 0x3f800000u, 0x00000000u, 0x80000000u},
        {0x80000000u, 0x80000000u, 0x3f800000u, 0x3f800000u, 0x80000000u},
    };
    int failures = 0;
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        playerState_t ps; pmove_t pm; pml_t pml;
        vec3_t wishdir = {1.0f, 0.0f, 0.0f};
        const unsigned int velocity[] = {0xbf800000u, 0x80000000u, 0x80000000u};
        float wishspeed, accel;
        unsigned int actual[3];
        init(&ps, &pm, &pml, 60);
        ps.pm_type = 2;
        memcpy(ps.velocity, velocity, sizeof(velocity));
        memcpy(&wishspeed, &cases[i][0], sizeof(wishspeed));
        memcpy(&fixture_stopspeed.current.value, &cases[i][1], sizeof(float));
        memcpy(&accel, &cases[i][2], sizeof(accel));
        memcpy(&pml.frametime, &cases[i][3], sizeof(pml.frametime));
        PM_Accelerate(&ps, &pml, wishdir, wishspeed, accel);
        memcpy(actual, ps.velocity, sizeof(actual));
        if (actual[0] != 0xbf800000u || actual[1] != cases[i][4] || actual[2] != cases[i][4]) {
            fprintf(stderr, "acceleration zero case %u: expected bf800000 %08x %08x, got %08x %08x %08x\n",
                    i, cases[i][4], cases[i][4], actual[0], actual[1], actual[2]);
            ++failures;
        }
    }
    assert(failures == 0);
    puts("PASS acceleration: 16 original signed-zero MAXSS ties");
}
#endif

static void test_bob(void)
{
    const int heights[] = {60, 40, 11};
    const float stances[] = {1, 0.65f, 0.15f};
    const float coeff[][2] = {{0.335f, 0.305f}, {0.34f, 0.315f}, {0.25f, 0.24f}};
    const int anims[][2] = {{ANIM_MT_RUN, ANIM_MT_WALK}, {ANIM_MT_RUNCR, ANIM_MT_WALKCR}, {ANIM_MT_WALKPRONE, ANIM_MT_WALKPRONE}};
    int cases = 0;
    for (int h = 0; h < 3; ++h) for (int walk = 0; walk < 2; ++walk)
        for (int ms = 1; ms <= 16; ++ms) for (int ratio = 1; ratio <= 4; ++ratio)
            for (int old = 0; old < 256; ++old) {
                playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, heights[h]);
                ps.pm_flags = walk ? 0x100 : 0; ps.bobCycle = old;
                float scale = 190.0f * (walk ? 0.4f : 1.0f) * stances[h];
                ps.velocity[0] = scale * ((float)ratio / 4.0f); pm.xyspeed = ps.velocity[0];
                pml.msec = ms; pml.frametime = (float)ms * 0.001f;
                PM_Footsteps(&pm, &pml);
                int expected = (int)((float)old + (float)ms * (ps.velocity[0] / scale * coeff[h][walk])) & 255;
                if (ps.velocity[0] < 10.0f) expected = old;
                if (ps.bobCycle != expected) {
                    fprintf(stderr, "bob stance=%d walk=%d msec=%d speed=%g old=%d expected=%d got=%d\n", heights[h], walk, ms, pm.xyspeed, old, expected, ps.bobCycle); assert(0);
                }
                assert(animation == (ps.velocity[0] < 10.0f ? (h == 2 ? ANIM_MT_IDLEPRONE : ANIM_MT_IDLECR) : anims[h][walk])); ++cases;
            }
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.bobCycle = 93; ps.velocity[0] = 0.5f; PM_Footsteps(&pm, &pml); assert(ps.bobCycle == 0);
    init(&ps, &pm, &pml, 60); ps.velocity[0] = 190; int total = 0;
    for (int i = 0; i < 333; ++i) { int old = ps.bobCycle; PM_Footsteps(&pm, &pml); total += (ps.bobCycle - old) & 255; }
    assert(total == 333); assert(eventCount == 3);
    printf("PASS bob: %d cases; 3 ms at 190 units/s = %d bob units/s, %d footsteps\n", cases, total, eventCount);
    init(&ps, &pm, &pml, 60); ps.velocity[0] = 190; ps.bobCycle = 63; failAnimation = 1;
    PM_Footsteps(&pm, &pml);
    assert(animationCalls == 2 && animation == ANIM_MT_IDLE && animationBob == 64);
    assert(eventCount == 1);
}

static void test_scale(void)
{
    playerState_t ps; pmove_t pm; pml_t pml;
    for (int height = 11; height <= 60; ++height) {
        init(&ps, &pm, &pml, height); ps.pm_flags = 1;
        close_float(PM_CmdScale(&ps, &pm.cmd), 190.0f / 127.0f);
        ps.pm_flags = 0x100; close_float(PM_CmdScale(&ps, &pm.cmd), 190.0f / 127.0f * 0.4f);
    }
    puts("PASS command scale: ADS-walk flag, independent air stance");
}

static void test_snap(void)
{
    const float values[] = {-2.9f, -2.5f, -2.1f, -1.5f, -0.9f, -0.5f, -0.1f, 0.0f,
                            0.1f, 0.5f, 0.9f, 1.5f, 2.1f, 2.5f, 2.9f};
    const int expected[] = {-2, -2, -2, -1, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2};
    for (int i = 0; i < (int)(sizeof(values) / sizeof(values[0])); ++i) {
        vec3_t velocity = {values[i], -values[i], values[i]};
        Sys_SnapVector(velocity);
        close_float(velocity[0], (float)expected[i]);
        close_float(velocity[1], (float)-expected[i]);
        close_float(velocity[2], (float)expected[i]);
    }
    puts("PASS production velocity snapping truncates toward zero");
}

static void test_walk(void)
{
    const int heights[] = {60, 40, 11};
    const float stance[] = {1.0f, 0.65f, 0.15f}, accel[] = {9, 12, 19};
    const float weapons[] = {0, 0.9f, 1, 1.05f, 1.1f};
    int cases = 0;
    for (int h = 0; h < 3; ++h) for (int w = 0; w < 5; ++w) for (int d = 0; d < 3; ++d)
        for (int flags = 0; flags < 4; ++flags) for (int ms = 1; ms <= 16; ++ms) {
            playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, heights[h]);
            weapon.fMoveSpeedScale = weapons[w]; ps.pm_flags = (flags & 1) ? 0x100 : 0;
            pm.cmd.buttons = (flags & 2) ? 0x800 : 0;
            if (d == 1) pm.cmd.forwardmove = -127;
            if (d == 2) { pm.cmd.forwardmove = 0; pm.cmd.rightmove = 127; }
            pml.msec = ms; pml.frametime = (float)ms * 0.001f;
            float speed = 190.0f * (d == 1 ? 0.7f : (d == 2 ? 0.8f : 1));
            if (flags & 1) speed *= 0.4f;
            speed *= stance[h]; if (weapons[w] > 0) speed *= weapons[w]; if (flags & 2) speed *= 0.4f;
            float expected = fminf(speed, fmaxf(speed, 100.0f) * (accel[h] * pml.frametime));
            PM_WalkMove(&pm, &pml);
            close_float(sqrtf(ps.velocity[0]*ps.velocity[0]+ps.velocity[1]*ps.velocity[1]), expected); ++cases;
            ps.velocity[0] = 190; ps.velocity[1] = 0; PM_Friction(&ps, &pml);
            close_float(ps.velocity[0], 190.0f - 190.0f * 5.5f * pml.frametime);
        }
    printf("PASS walk acceleration/friction: %d stance/weapon/direction/ADS/msec cases\n", cases);
}

static void test_lean(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    pm.cmd.buttons = 0x80; PM_UpdateLean(&ps, 175, &pm.cmd, CG_TraceCapsule); close_float(ps.leanf, 0.25f);
    PM_UpdateLean(&ps, 175, &pm.cmd, CG_TraceCapsule); close_float(ps.leanf, 0.5f);
    pm.cmd.buttons = 0; PM_UpdateLean(&ps, 140, &pm.cmd, CG_TraceCapsule); close_float(ps.leanf, 0.25f);
    PM_UpdateLean(&ps, 140, &pm.cmd, CG_TraceCapsule); close_float(ps.leanf, 0);
    ps.viewHeightTarget = 11; pm.cmd.buttons = 0x40; PM_UpdateLean(&ps, 350, &pm.cmd, CG_TraceCapsule); close_float(ps.leanf, -0.25f);
    traceFraction = 0; PM_UpdateLean(&ps, 3, &pm.cmd, CG_TraceCapsule); close_float(ps.leanf, 0);
    puts("PASS lean timing, stance limit and collision cap");
}

static void test_timers(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.pm_time = 2; ps.pm_flags = 0x80000 | 0x600; ps.weaponTime = 100; ps.legsTimer = 100; ps.torsoTimer = 2;
    PM_DropTimers(&ps, 3); assert(ps.pm_time == 0); assert(!(ps.pm_flags & 0x80600));
    assert(ps.weaponTime == 100); assert(ps.legsTimer == 97); assert(ps.torsoTimer == 0);
    puts("PASS movement timers clear timed flags and preserve weapon-owned timers");
}

static void test_direction(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.origin[0] = 1; ps.origin[1] = -1; pm.cmd.rightmove = 127;
    PM_SetMovementDir(&pm, &pml); assert(ps.movementDir == -45);
    puts("PASS legs follow displacement in degrees");

    /* Mac PM_SetMovementDir 0x6ceb8 turns every negative forward command.
       Pinned CoD2x animation.cpp:1331-1339 turns only actual backward travel.
       Expectations below are named angles, not a hash of native output. */
    static const struct { float x, y; int command, stock, codx; } cases[] = {
        {1, 0, -127, 90, 0}, {1, 1, -127, -90, 45},
        {-1, 1, -127, -45, -45}, {-1, -1, -127, 45, 45},
        {0, 1, -127, -90, 90}, {0, -1, -127, 90, -90},
        {1, -1, 127, -45, -45}, {-1, 0, 127, 90, 90}
    };
    int fixed = 0;
#if defined(COD2_CODX) && COD2_CODX
    fixed = Cod2x_GameVersion() >= 3;
#endif
    for (int handler = 0; handler < 2; ++handler) {
        for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            init(&ps, &pm, &pml, 60); pm.handler = handler;
            pm.cmd.forwardmove = cases[i].command;
            ps.origin[0] = cases[i].x; ps.origin[1] = cases[i].y;
            PM_SetMovementDir(&pm, &pml);
            assert(ps.movementDir == (fixed ? cases[i].codx : cases[i].stock));
        }
        init(&ps, &pm, &pml, 60); pm.handler = handler;
        ps.origin[0] = 0.015f; PM_SetMovementDir(&pm, &pml); assert(ps.movementDir == 0);
        ps.origin[0] = -1; ps.groundEntityNum = 1023;
        PM_SetMovementDir(&pm, &pml); assert(ps.movementDir == 0);
        init(&ps, &pm, &pml, 11); pm.handler = handler; ps.pm_flags = 1;
        ps.proneDirection = 135; PM_SetMovementDir(&pm, &pml); assert(ps.movementDir == 90);
    }
    puts("PASS independent backward displacement, speed/air/prone contracts for both handlers");
}

static void test_stance_events(void)
{
    /* Stock Mac PM_CheckDuck 0x6e08e and CoD2x animation.cpp:1389-1393.
       Controller transitions replace exactly these four script events >=3. */
    const int from[] = {11, 60, 11, 40};
    const int buttons[] = {0x200, 0x200, 0, 0};
    const int events[] = {ANIM_ET_PRONE_TO_CROUCH, ANIM_ET_STAND_TO_CROUCH,
                          ANIM_ET_PRONE_TO_STAND, ANIM_ET_CROUCH_TO_STAND};
    int suppressed = 0;
#if defined(COD2_CODX) && COD2_CODX
    suppressed = Cod2x_GameVersion() >= 3;
#endif
    for (int handler = 0; handler < 2; ++handler) {
        for (int i = 0; i < 4; ++i) {
            playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, from[i]);
            pm.handler = handler; ps.pm_flags = from[i] == 11 ? 1 : from[i] == 40 ? 2 : 0;
            pm.cmd.buttons = buttons[i]; pm.cmd.forwardmove = 0;
            stanceCalls = 0; PM_CheckDuck(&pm, &pml);
            assert(stanceCalls == !suppressed);
            if (!suppressed) assert(stanceEvent == events[i]);
            assert(ps.viewHeightTarget == (buttons[i] || from[i] == 11 ? 40 : 60));
            assert((ps.eFlags & 12) == (ps.viewHeightTarget == 40 ? 4 : 0));
        }
    }
    puts("PASS four stance event contracts and effective stance for both handlers");
}

static void test_view(void)
{
    dvar_t dvar = { .current.value = 8.0f };
    const dvar_t *bg_bobMax = &dvar;
#include "bob_max_source.h"
    close_float(bobMax, 8.0f);
    puts("PASS view bob amplitude reads the float dvar");
}

static void test_ladder(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.pm_flags = 0x20; ps.vLadderVec[0] = -1; pml.walking = 0;
    PM_LadderMove(&pm, &pml); close_float(ps.velocity[2], 0.625f * 0.5f * 190.0f * (9.0f * 0.003f));
    assert(ps.movementDir == 0);
    puts("PASS ladder pitch-scaled acceleration");
}

static void test_prone(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 11);
    ps.pm_flags = 1; ps.proneDirectionPitch = 30; ps.proneTorsoPitch = -20;
    PM_UpdatePronePitch(&pm, &pml); close_float(ps.proneDirectionPitch, 30 - 0.003f * 70); close_float(ps.proneTorsoPitch, -20 + 0.003f * 70);
    puts("PASS prone pitch approaches the ground at 70 degrees/s");
}

static void test_dispatch(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.pm_type = 5; ps.velocity[0] = 100; ps.weaponTime = 50; pm.cmd.serverTime = 3; weaponCalls = 0;
    Pmove(&pm); assert(ps.commandTime == 3); close_float(ps.origin[0], 0); assert(ps.weaponTime == 50); assert(weaponCalls == 0);
    init(&ps, &pm, &pml, 60); ps.pm_flags = 0x8000; pm.cmd.serverTime = 3; ps.velocity[0] = 100; mantleChecks = 0;
    Pmove(&pm); assert(pm.cmd.forwardmove == 0); assert(ps.velocity[0] == 0); assert(mantleChecks == 1);
    init(&ps, &pm, &pml, 60); ps.pm_type = 2; pm.cmd.forwardmove = 0; pm.cmd.buttons = 0x80; pm.cmd.serverTime = 3;
    Pmove(&pm); close_float(ps.velocity[2], 570.0f * (9.0f * 0.003f));
    init(&ps, &pm, &pml, 60); ps.groundEntityNum = 1023; ps.commandTime = 997; pm.cmd.serverTime = 1000;
    ladderSurface = 8; traceFraction = 0.5f; Pmove(&pm); assert(ps.pm_flags & 0x20);
    ladderSurface = 0; traceFraction = 1;
    puts("PASS intermission/frozen frame paths and mantle dispatch");
}

#if defined(COD2_X64)
static void test_view_adjust_zero(void)
{
    playerState_t ps; pmove_t pm; pml_t pml;
    init(&ps, &pm, &pml, 60);
    ps.viewHeightCurrent = 0.0f;
    PM_ViewHeightAdjust(&pm, &pml);
    assert(ps.viewHeightCurrent == 60.0f && ps.viewHeightLerpTime == 0);
    ps.pm_type = 4;
    ps.viewHeightCurrent = 0.0f;
    PM_ViewHeightAdjust(&pm, &pml);
    assert(ps.viewHeightCurrent == 0.0f);
    puts("PASS view height: zero current height initializes immediately");
}

static void test_view_adjust_offset(void)
{
    playerState_t ps; pmove_t pm; pml_t pml;
    init(&ps, &pm, &pml, 40);
    ps.viewHeightCurrent = 56.0f;
    ps.viewHeightLerpTime = 1000;
    ps.viewHeightLerpTarget = 40;
    ps.viewHeightLerpDown = 1;
    pm.cmd.serverTime = 1100;
    ps.velocity[0] = 17.0f; ps.velocity[1] = 23.0f; ps.velocity[2] = 31.0f;
    /* Stock offsets are zero. Exercise the Mac adjustment branch with a
       synthetic table offset; all three saved velocity words are restored. */
    viewLerp_StandCrouch[3].iOffset = viewLerp_StandCrouch[4].iOffset = 2;
    PM_ViewHeightAdjust(&pm, &pml);
    viewLerp_StandCrouch[3].iOffset = viewLerp_StandCrouch[4].iOffset = 0;
    assert(ps.velocity[0] == 17.0f && ps.velocity[1] == 23.0f && ps.velocity[2] == 31.0f);
    assert(ps.viewHeightLerpPosAdj == 2.0f);
    puts("PASS view height: position adjustment preserves the velocity vector");
}

static int routeTraces, routeGround;
static void route_trace(trace_t *tr, const vec_t *start, const vec_t *end, const vec_t *maxs)
{
    (void)start; (void)end; (void)maxs;
    memset(tr, 0, sizeof(*tr));
    ++routeTraces;
    tr->fraction = 1.0f;
    tr->entityNum = 5;
    if (!routeGround || routeTraces > 1) {
        tr->startsolid = 1;
        tr->contents = 0x2000001;
    }
}

static void test_trace_routes(void)
{
    playerState_t ps; pmove_t pm; pml_t pml;
    fixtureTrace = route_trace;
    for (int handler = 0; handler < 2; ++handler) {
        for (int grounded = 0; grounded < 2; ++grounded) {
            init(&ps, &pm, &pml, 60);
            pm.handler = handler;
            pm.tracemask = 0x2000001;
            ps.groundEntityNum = grounded ? 1022 : 1023;
            routeGround = 1; routeTraces = 0;
            PM_GroundTrace(&pm, &pml);
            assert(routeTraces == 2 && pm.numtouch == 0 && pm.tracemask == 0x2000001);
        }
        init(&ps, &pm, &pml, 11);
        pm.handler = handler;
        pm.tracemask = 0x2000001;
        ps.pm_flags = 1;
        pm.cmd.buttons = 0x200;
        routeGround = 0; routeTraces = 0;
        PM_CheckDuck(&pm, &pml);
        assert(routeTraces == 1 && pm.numtouch == 0 && pm.tracemask == 0x2000001);
    }
    fixtureTrace = NULL;
    puts("PASS stance and missed-ground probes use the raw trace callback");
}

static void test_linked_weapon(void)
{
    playerState_t ps; pmove_t pm; pml_t pml;
    for (int type = 1; type <= 7; type += 6) {
        init(&ps, &pm, &pml, 60);
        ps.pm_type = type;
        ps.eFlags = 0x100;
        pm.cmd.serverTime = 3;
        weaponCalls = 0;
        Pmove(&pm);
        assert(weaponCalls == 1);
    }
    puts("PASS linked movement types retain their weapon update with turret flags");
}

#endif
int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
#ifdef GAMEPLAY_MATRIX
    Fixture_SetRevision(argc == 3 ? atoi(argv[2]) : 3);
#endif
#if defined(COD2_X64)
    if (!strcmp(argv[1], "acceleration-zero")) test_acceleration_zero();
    else
#endif
    if (!strcmp(argv[1], "bob")) test_bob();
    else if (!strcmp(argv[1], "scale")) test_scale();
    else if (!strcmp(argv[1], "snap")) test_snap();
    else if (!strcmp(argv[1], "walk")) test_walk();
    else if (!strcmp(argv[1], "lean")) test_lean();
    else if (!strcmp(argv[1], "timers")) test_timers();
    else if (!strcmp(argv[1], "direction")) test_direction();
    else if (!strcmp(argv[1], "stance")) test_stance_events();
    else if (!strcmp(argv[1], "view")) test_view();
#if defined(COD2_X64)
    else if (!strcmp(argv[1], "view-adjust-zero")) test_view_adjust_zero();
    else if (!strcmp(argv[1], "view-adjust-offset")) test_view_adjust_offset();
    else if (!strcmp(argv[1], "trace-routes")) test_trace_routes();
    else if (!strcmp(argv[1], "linked-weapon")) test_linked_weapon();
#endif
    else if (!strcmp(argv[1], "ladder")) test_ladder();
    else if (!strcmp(argv[1], "prone")) test_prone();
    else if (!strcmp(argv[1], "dispatch")) test_dispatch();
    else assert(0);
    return 0;
}
