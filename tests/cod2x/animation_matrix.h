/* Included after the full production animation TU and controller seams.
   Expectations come from Mac 1.3 stock angles/lerp and the pinned CoD2x
   animation.cpp:950-984, 1094-1268 controller/stance contracts. */
#include <stdlib.h>
#include <math.h>

animStringItem_t weaponStrings[128];
static dvar_t swingSpeed = {.current.value = 0.3f};
const dvar_t *bg_swingSpeed = &swingSpeed;
static WeaponDef animationWeapon;
static XAnimTree_s animationTrees[2];
static XAnim_s animationDefinitions[2];
static float lastGoalTime, lastRate;
static int goalCalls, clearCalls, rateCalls;
static unsigned int lastGoalIndex, lastRateIndex, clearedIndex;

void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
WeaponDef *BG_GetWeaponDef(int weapon) { assert(weapon == 0); return &animationWeapon; }
float XAnimGetWeight(const XAnimTree_s *tree, unsigned int index)
{ (void)tree; (void)index; return 1.0f; }
float XAnimGetTime(const XAnimTree_s *tree, unsigned int index)
{ (void)tree; (void)index; return 0.25f; }
Bool XAnimIsPrimitive(XAnim_s *anims, unsigned int index)
{ (void)anims; (void)index; return 1; }
Bool XAnimIsLooped(const XAnim_s *anims, unsigned int index)
{ (void)anims; (void)index; return 1; }
int XAnimGetLengthMsec(const XAnim_s *anims, unsigned int index)
{ (void)anims; (void)index; return 800; }
void XAnimSetTime(XAnimTree *tree, unsigned int index, float time)
{ (void)tree; (void)index; (void)time; }
void XAnimClearGoalWeight(XAnimTree_s *tree, unsigned int index, float time)
{ assert(tree == &animationTrees[bgs->anim_user]); clearedIndex = index; lastGoalTime = time; ++clearCalls; }
void XAnimSetCompleteGoalWeight(XAnimTree_s *tree, unsigned int index, float weight,
                              float time, float rate, unsigned int notify, unsigned int type, int restart)
{ assert(tree == &animationTrees[bgs->anim_user]); (void)weight; (void)rate; (void)notify; (void)type; (void)restart; lastGoalIndex = index; lastGoalTime = time; ++goalCalls; }
int XAnimSetCompleteGoalWeightKnobAll(XAnimTree_s *tree, unsigned int index, unsigned int root,
                                    float weight, float time, float rate, unsigned int notify, int restart)
{ (void)tree; (void)index; (void)root; (void)weight; (void)time; (void)rate; (void)notify; (void)restart; assert(0); return 0; }
void XAnimSetAnimRate(XAnimTree_s *tree, unsigned int index, float rate)
{ assert(tree == &animationTrees[bgs->anim_user]); lastRateIndex = index; lastRate = rate; ++rateCalls; }

static void animation_close(float actual, float expected)
{
    if (fabsf(actual - expected) > 0.0001f) {
        fprintf(stderr, "animation expected %.9g, got %.9g\n", expected, actual);
        assert(0);
    }
}

static clientInfo_t *animation_init(int user)
{
    bgs = user ? &level_bgs : &client_bgs;
    memset(bgs, 0, sizeof(*bgs));
    bgs->anim_user = user;
    bgs->time = 1000; bgs->frametime = 1000; bgs->latestSnapshotTime = 1000;
    bgs->animScriptData.numAnimations = 6;
    bgs->animScriptData.animTree.anims = &animationDefinitions[user];
    globalScriptData = &bgs->animScriptData;
    for (int index = 1; index < 4; ++index) {
        animation_t *anim = &bgs->animScriptData.animations[index];
        anim->initialLerp = 37; anim->moveSpeed = 100;
        anim->movetype = 1LL << (index == 1 ? ANIM_MT_WALK : index == 2 ? ANIM_MT_WALKCR : ANIM_MT_WALKPRONE);
    }
    strcpy(bgs->animScriptData.animations[4].name, "reload_fixture");
    strcpy(bgs->animScriptData.animations[5].name, "fire_fixture");
    clientInfo_t *ci = &bgs->clientinfo[5];
    ci->clientNum = 5; ci->pXAnimTree = &animationTrees[user];
    ci->legs.animation = &bgs->animScriptData.animations[3]; ci->legs.animationNumber = 3;
    ci->playerAngles[1] = 0; ci->lerpMoveDir = 45;
    tagCalls = originCalls = goalCalls = clearCalls = rateCalls = 0;
    return ci;
}

static void check_prone_angles(int version, int user, float yaw, int backward)
{
    Fixture_SetRevision(version);
    clientInfo_t *ci = animation_init(user);
    entityState_t es = {.clientNum = 5, .eType = 1, .eFlags = 8, .legsAnim = 3};
    ci->lerpMoveDir = yaw;
    if (backward) {
        bgs->animScriptData.animations[3].movetype = 1LL << ANIM_MT_WALKPRONEBK;
        if (backward >= 2) {
            es.torsoAnim = backward == 2 ? 4 : 5;
            ci->torso.animationNumber = es.torsoAnim;
            ci->torso.animation = &bgs->animScriptData.animations[es.torsoAnim];
        }
    }
    /* The native BG_PlayerAnimation body contains the original inlined
       BG_PlayerAngles and then runs both actual lerp frame bodies. */
    BG_PlayerAnimation(&object, &es, ci);
    int fixed = 0;
#if defined(COD2_CODX) && COD2_CODX
    fixed = version >= 3;
#endif
    float expectedYaw = fixed && version == 3 ? 0 : yaw;
    animation_close(ci->legs.yawAngle, expectedYaw);
    animation_close(ci->torso.yawAngle, 0);
    animation_close(ci->legs.animSpeedScale, 1);
    assert(ci->legs.animationNumber == 3 && ci->legs.animationTime == 0 && rateCalls == (backward >= 2 ? 2 : 1));
    /* Wait past movement interpolation to observe the complete production
       controller/tag result, including prone flatten and yaw-origin offsets. */
    int bits[4] = {0};
    BG_Player_DoControllers(&object, &es, bits, ci, 1000);
    bgs->time += 1000;
    BG_Player_DoControllers(&object, &es, bits, ci, 1000);
    float radians = expectedYaw * 0.017453292519943295f;
    const vec3_t expectedTags[6] = {
        {fixed ? (backward ? -60 : -40) : 0, 0, 0}, {fixed ? 40 : 0, -expectedYaw * 0.1f, 0},
        {0, -expectedYaw * 0.8f, 0}, {0, 0, 0}, {0, 0, 0},
        {fixed ? (backward == 3 ? 23 : backward == 2 ? 24 : backward ? 10 : 3) : 0, 0, 0}
    };
    const vec3_t expectedOriginAngles = {0, expectedYaw, 0};
    const vec3_t expectedOriginOffset = {(1 - cosf(radians)) * -24,
        sinf(radians) * 12 - (fixed && backward ? 8 : 0), fixed && backward ? -6 : 0};
    assert(tagCalls == 12 && originCalls == 2);
    for (int controller = 0; controller < 6; ++controller)
        for (int axis = 0; axis < 3; ++axis)
            animation_close(ci->angles[controller][axis], expectedTags[controller][axis]);
    for (int controller = 0; controller < 6; ++controller)
        for (int axis = 0; axis < 3; ++axis)
            animation_close(emittedTags[controller][axis], expectedTags[controller][axis]);
    for (int axis = 0; axis < 3; ++axis) {
        animation_close(ci->tag_origin_angles[axis], expectedOriginAngles[axis]);
        animation_close(ci->tag_origin_offset[axis], expectedOriginOffset[axis]);
        animation_close(emittedOriginAngles[axis], expectedOriginAngles[axis]);
        animation_close(emittedOriginOffset[axis], expectedOriginOffset[axis]);
    }
}

static void check_mounted_flags(int version, int user, int flags)
{
    /* Mac controllers 0x182b94 test high byte0x3; angle paths 0x183f4b,
       0x184034 do likewise. Entity bits0x1/0x2 are not mounted bits. */
    Fixture_SetRevision(version);
    clientInfo_t *ci = animation_init(user);
    entityState_t es = {.clientNum = 5, .eType = 1, .eFlags = 8 | flags, .legsAnim = 3};
    ci->playerAngles[0] = 30;
    BG_PlayerAnimation(&object, &es, ci);
    int mounted = (flags & 0x300) != 0;
    float expectedYaw = mounted ? 0 : 45;
#if defined(COD2_CODX) && COD2_CODX
    if (version == 3) expectedYaw = 0;
#endif
    animation_close(ci->legs.yawAngle, expectedYaw);
    /* AngleMod retains stock16-bit angle quantization: floor(18*65536/360). */
    animation_close(ci->torso.pitchAngle, mounted ? 0 : 17.99560546875f);
    int bits[4] = {0};
    BG_Player_DoControllers(&object, &es, bits, ci, 1000);
    bgs->time += 1000;
    BG_Player_DoControllers(&object, &es, bits, ci, 1000);
    if (mounted) {
        for (int controller = 0; controller < 6; ++controller)
            for (int axis = 0; axis < 3; ++axis)
                animation_close(emittedTags[controller][axis], 0);
        for (int axis = 0; axis < 3; ++axis) {
            animation_close(emittedOriginAngles[axis], 0);
            animation_close(emittedOriginOffset[axis], 0);
        }
    } else {
        animation_close(emittedTags[2][0], 8.997802734375f);
        animation_close(emittedTags[3][0], 6.3006591796875f);
    }
}

static void check_blend(int version, int user, int before, int after, int codxTime)
{
    Fixture_SetRevision(version);
    clientInfo_t *ci = animation_init(user);
    ci->legs.animationNumber = before;
    ci->legs.animation = &bgs->animScriptData.animations[before];
    entityState_t es = {.clientNum = 5, .eType = 1, .legsAnim = after};
#if defined(COD2_X64)
    int expected = 400;
#else
    int expected = 37;
#endif
#if defined(COD2_CODX) && COD2_CODX
    if (version >= 3) expected = codxTime;
#else
    (void)codxTime;
#endif
    BG_RunLerpFrameRate(ci, &ci->legs, after, &es);
    assert(ci->legs.animationNumber == after && ci->legs.animationTime == expected);
    assert(ci->legs.animation == &bgs->animScriptData.animations[after]);
    assert(goalCalls == 1 && clearCalls == 1 && rateCalls == 1);
    assert(lastGoalIndex == after && clearedIndex == before && lastRateIndex == after);
    animation_close(lastGoalTime, expected * 0.001f);
    animation_close(lastRate, 1);
    /* Snapshot displacement drives the production movement-animation rate. */
    bgs->latestSnapshotTime += 100; es.pos.trBase[0] = 15;
    BG_RunLerpFrameRate(ci, &ci->legs, after, &es);
    animation_close(ci->legs.animSpeedScale, 1.5f);
    animation_close(lastRate, 1.5f);
    assert(lastRateIndex == after);
    animation_close(ci->legs.oldFramePos[0], 15);
    assert(ci->legs.oldFrameSnapshotTime == 1100);
}

static void animation_matrix(void)
{
    const int versions[] = {0, 3, 4, 6, -1};
    for (int version = 0; version < 5; ++version) {
        for (int user = 0; user < 2; ++user) {
            check_prone_angles(versions[version], user, 45, 0);
            check_prone_angles(versions[version], user, -45, 0);
            check_prone_angles(versions[version], user, -45, 1);
            check_prone_angles(versions[version], user, -45, 2);
            check_prone_angles(versions[version], user, -45, 3);
            check_mounted_flags(versions[version], user, 1);
            check_mounted_flags(versions[version], user, 2);
            check_mounted_flags(versions[version], user, 0x100);
            check_mounted_flags(versions[version], user, 0x200);
            check_blend(versions[version], user, 1, 2, 200);
            check_blend(versions[version], user, 2, 3, 232);
            check_blend(versions[version], user, 3, 2, 400);
            check_blend(versions[version], user, 2, 1, 200);
        }
    }
    puts("PASS production angle/controller/tag/prone-yaw and stance lerp/rate contracts at remote 0/3/4/6 and hosted 0 for both animation users");
}
