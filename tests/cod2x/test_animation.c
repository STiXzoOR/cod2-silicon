#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Typed fixtures for pure controller calculations; engine ABI/layout is not
   modeled here. The owning translation unit gets a separate syntax check. */
typedef float vec3_t[3];
typedef struct XAnimTree_s { int unused; } XAnimTree_s;
typedef struct {
    char name[64];
    int initialLerp;
    float moveSpeed;
    int flags;
    int64_t movetype;
} animation_t;
typedef struct { animation_t *animation; int animationTime; float yawAngle; } lerpFrame_t;
typedef struct {
    XAnimTree_s *pXAnimTree;
    int clientNum;
    lerpFrame_t legs, torso;
    vec3_t angles[6], tag_origin_angles, tag_origin_offset, playerAngles;
    float lerpMoveDir, lerpLean;
    int clientConditions[9][2];
} clientInfo_t;
typedef struct { int eFlags, legsAnim; } entityState_t;
typedef struct {
    int time, anim_user;
    struct { animation_t animations[2]; } animScriptData;
} bgs_t;
static bgs_t world;
static bgs_t *bgs = &world;

/* Values from opencod2 src/headers/PC/bgame/bg_types.h:60-101. */
enum {
    ANIM_MT_IDLE = 1, ANIM_MT_IDLECR = 2, ANIM_MT_IDLEPRONE = 3,
    ANIM_MT_WALK = 4, ANIM_MT_WALKBK = 5, ANIM_MT_WALKCR = 6,
    ANIM_MT_WALKCRBK = 7, ANIM_MT_WALKPRONE = 8, ANIM_MT_WALKPRONEBK = 9,
    ANIM_MT_RUN = 10, ANIM_MT_RUNBK = 11, ANIM_MT_RUNCR = 12,
    ANIM_MT_RUNCRBK = 13, ANIM_MT_TURNRIGHTCR = 16, ANIM_MT_TURNLEFTCR = 17,
    ANIM_MT_CLIMBUP = 18, ANIM_MT_CLIMBDOWN = 19,
    ANIM_MT_STUMBLE_CROUCH_FORWARD = 39, ANIM_MT_STUMBLE_CROUCH_BACKWARD = 40
};

static float AngleNormalize180(float angle)
{
    angle = fmodf(angle + 180.0f, 360.0f);
    if (angle < 0.0f)
        angle += 360.0f;
    return angle - 180.0f;
}

#include "../../src/PC/bgame/cod2x_animation.h"

static int closeTo(float a, float b)
{
    return fabsf(a - b) < 0.0001f;
}

int main(void)
{
    clientInfo_t ci = {0};
    entityState_t es = {0};
    animation_t stand = { .movetype = 1LL << ANIM_MT_IDLE, .initialLerp = 80 };
    animation_t crouch = { .movetype = 1LL << ANIM_MT_RUNCR, .initialLerp = 80 };
    animation_t prone = { .movetype = 1LL << ANIM_MT_IDLEPRONE, .initialLerp = 80 };
    animation_t stumble = { .movetype = 1LL << ANIM_MT_STUMBLE_CROUCH_FORWARD };
    XAnimTree_s tree = {0};
    vec3_t goals[8] = {{0}}, rotation = {20.0f, 90.0f, 0.0f};
    cod2xAnimationState_t *state, *server;
    ci.pXAnimTree = &tree;
    world.time = 100;
    assert(BG_Cod2xStance(&stand) == 0 && BG_Cod2xStance(&crouch) == 1);
    assert(BG_Cod2xStance(&prone) == 2 && BG_Cod2xStance(&stumble) == 1);
    BG_Cod2xBlendTime(&ci, &ci.legs, NULL, &crouch);
    assert(ci.legs.animationTime == 0);
    BG_Cod2xBlendTime(&ci, &ci.legs, &stand, &crouch);
    state = BG_Cod2xState(&ci);
    assert(ci.legs.animationTime == 200 && state->stance == 1 && state->stanceDuration == 200);
    BG_Cod2xBlendTime(&ci, &ci.legs, &crouch, &prone);
    assert(ci.legs.animationTime == 232 && state->stance == 2 && state->stanceDuration == 400);
    BG_Cod2xBlendTime(&ci, &ci.legs, &prone, &crouch);
    assert(ci.legs.animationTime == 400 && state->stance == 3);
    BG_Cod2xBlendTime(&ci, &ci.legs, &crouch, &stand);
    assert(ci.legs.animationTime == 200 && state->stance == 4);
    BG_Cod2xBlendTime(&ci, &ci.torso, &stand, &crouch);
    assert(ci.torso.animationTime == 80);
    stand.initialLerp = 0;
    BG_Cod2xBlendTime(&ci, &ci.torso, &stand, &stand);
    assert(ci.torso.animationTime == 170);
    stand.moveSpeed = 10.0f;
    BG_Cod2xBlendTime(&ci, &ci.torso, &stand, &stand);
    assert(ci.torso.animationTime == 120);

    BG_Cod2xRotate(0.0f, rotation);
    assert(closeTo(rotation[0], 0.0f) && closeTo(rotation[2], 20.0f));
    Cod2x_ResetAnimation();
    es.eFlags = 8;
    ci.clientConditions[3][0] = 1 << ANIM_MT_WALKPRONEBK;
    BG_Cod2xControllerGoals(&es, &ci, goals, 0.0f, state);
    assert(closeTo(goals[0][0], -60.0f) && closeTo(goals[1][0], 40.0f));
    assert(closeTo(goals[5][0], 10.0f));
    assert(closeTo(goals[7][1], -8.0f) && closeTo(goals[7][2], -6.0f));
    assert(state->movementDuration == 400);
    ci.torso.animation = &prone;
    strcpy(prone.name, "reload_prone");
    memset(goals, 0, sizeof(goals));
    BG_Cod2xControllerGoals(&es, &ci, goals, 0.0f, state);
    assert(closeTo(goals[5][0], 24.0f) && state->movementDuration == 400);
    strcpy(prone.name, "fire_prone");
    memset(goals, 0, sizeof(goals));
    BG_Cod2xControllerGoals(&es, &ci, goals, 0.0f, state);
    assert(closeTo(goals[5][0], 23.0f) && state->movementDuration == 100);

    Cod2x_ResetAnimation();
    memset(goals, 0, sizeof(goals));
    ci.clientConditions[3][0] = 0;
    es.eFlags = 0;
    state->stance = 2;
    state->stanceStart = 100;
    state->stanceDuration = 400;
    world.time = 380;
    BG_Cod2xControllerGoals(&es, &ci, goals, 0.0f, state);
    assert(closeTo(goals[7][2], 12.0f) && closeTo(goals[6][0], -10.0f));
    assert(closeTo(goals[0][0], 10.0f));
    world.anim_user = 1;
    server = BG_Cod2xState(&ci);
    assert(server != state && server->stance == 0);
    world.anim_user = 0;
    world.time = 50;
    assert(BG_Cod2xState(&ci)->stance == 0); /* time rewind/demo reset */
    puts("cod2x: stance timing, posture, diagonal alignment, state reset tests passed");
    return 0;
}
