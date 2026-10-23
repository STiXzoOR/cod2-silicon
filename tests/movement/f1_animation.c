#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Keep the real animation bodies and real angle math/parser. Services below
   supply only synthetic animation assets, weapon metadata and DObj outputs. */
#include "animation_source.h"

static bgs_t fixture_bgs;
bgs_t *bgs;
void *imp_bgs = &bgs;
scr_const_t scr_const;
animStringItem_t weaponStrings[128];
void *(*controller_names[6])();
char *globalFilename;
const char str_002157b8[] = "";
static dvar_t swing = {.current.value = 0.3f};
const dvar_t *bg_swingSpeed = &swing;
static WeaponDef weapon;
static XAnim_s definitions;
static XAnimTree_s animationTree;
static struct DObj_s object;
static unsigned short tags[6] = {1, 2, 3, 4, 5, 6};
static int looped[8], revision, goalCount, setTimeCount;
static int freeCount, createCount, clearTreeCount;
static float weight, lastTime, lastRate;
static unsigned int noteTypes[8], goalIndices[8];

void Com_Error(int code, const char *format, ...)
{
    va_list args;
    (void)code;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}
void Com_Printf(const char *format, ...) { (void)format; }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int ___tolower(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; }
void I_strncpyz(char *dst, const char *src, int size)
{ assert(size > 0); snprintf(dst, (size_t)size, "%s", src); }
WeaponDef *BG_GetWeaponDef(int index) { (void)index; return &weapon; }
int BG_GetViewmodelWeaponIndex(const playerState_t *ps) { return ps->weapon; }
float GetLeanFraction(float value) { return value; }
float XAnimGetWeight(const XAnimTree_s *t, unsigned int index)
{ (void)t; (void)index; return weight; }
float XAnimGetTime(const XAnimTree_s *t, unsigned int index)
{ (void)t; (void)index; return 0.25f; }
Bool XAnimIsPrimitive(XAnim_s *anims, unsigned int index)
{ (void)anims; (void)index; return 1; }
Bool XAnimIsLooped(const XAnim_s *anims, unsigned int index)
{ (void)anims; assert(index < 8); return looped[index]; }
int XAnimGetLengthMsec(const XAnim_s *anims, unsigned int index)
{ (void)anims; (void)index; return 800; }
void XAnimSetTime(XAnimTree *t, unsigned int index, float value)
{ (void)t; (void)index; lastTime = value; ++setTimeCount; }
void XAnimClearGoalWeight(XAnimTree_s *t, unsigned int index, float blend)
{ (void)t; (void)index; (void)blend; }
void XAnimSetCompleteGoalWeight(XAnimTree_s *t, unsigned int index, float goal,
                               float blend, float rate, unsigned int name,
                               unsigned int type, int restart)
{
    (void)t; (void)goal; (void)blend; (void)rate; (void)name; (void)restart;
    assert(goalCount < 8);
    goalIndices[goalCount] = index;
    noteTypes[goalCount++] = type;
}
int XAnimSetCompleteGoalWeightKnobAll(XAnimTree_s *t, unsigned int index,
                                    unsigned int root, float goal, float blend,
                                    float rate, unsigned int name, int restart)
{
    (void)t; (void)index; (void)root; (void)goal; (void)blend;
    (void)rate; (void)name; (void)restart;
    return 0;
}
void XAnimSetAnimRate(XAnimTree_s *t, unsigned int index, float rate)
{ (void)t; (void)index; lastRate = rate; }
qboolean DObjSetControlTagAngles(const struct DObj_s *obj, int *bits, unsigned int tag, vec_t *angles)
{ (void)obj; (void)bits; (void)tag; (void)angles; return 1; }
qboolean DObjSetLocalTag(const struct DObj_s *obj, int *bits, unsigned int tag, const vec_t *offset, const vec_t *angles)
{ (void)obj; (void)bits; (void)tag; (void)offset; (void)angles; return 1; }
void Scr_FindAnim(const char *file, const char *name, scr_anim_t *anim, int user)
{ (void)file; (void)name; (void)anim; (void)user; assert(0); }
#if defined(COD2_CODX) && COD2_CODX
int Cod2x_GameVersion(void) { return revision; }
playerState_t *SV_GameClientNum(int client) { (void)client; assert(0); return NULL; }
int Cod2x_PoseNeutral(void) { return 0; }
void Cod2x_PoseOffsets(vec3_t goals[8]) { (void)goals; }
void Cod2x_PoseReset(entityState_t *es) { (void)es; }
void Cod2x_PoseDiagnostics(const entityState_t *es, const clientInfo_t *ci, int server, int frametime)
{ (void)es; (void)ci; (void)server; (void)frametime; }
#endif

void XAnimClearTree(XAnimTree *t) { (void)t; ++clearTreeCount; }
static void free_dobj(int number) { (void)number; ++freeCount; }
static struct XModel *get_model(const char *name)
{ assert(strcmp(name, "synthetic") == 0); return (struct XModel *)&object; }
static void create_dobj(void *models, unsigned short count, void *t, int number, void *info)
{ (void)models; (void)t; (void)number; (void)info; assert(count == 1); ++createCount; }

static snd_alias_list_t *sound_alias(const char *name);

static clientInfo_t *reset(void)
{
    bgs = &fixture_bgs;
    memset(bgs, 0, sizeof(*bgs));
    memset(&weapon, 0, sizeof(weapon));
    bgs->time = bgs->latestSnapshotTime = 1000;
    bgs->frametime = 100;
    bgs->animScriptData.numAnimations = 8;
    bgs->animScriptData.animTree.anims = &definitions;
    bgs->animScriptData.torsoAnim = 6;
    bgs->animScriptData.legsAnim = 7;
    globalScriptData = &bgs->animScriptData;
    globalScriptData->soundAlias = sound_alias;
    g_pLoadAnims = NULL;
    for (int i = 0; i < 8; ++i) {
        animation_t *anim = &globalScriptData->animations[i];
        snprintf(anim->name, sizeof(anim->name), "anim%i", i);
        anim->nameHash = BG_StringHash(anim->name);
        anim->initialLerp = 37;
        anim->moveSpeed = 100;
        looped[i] = 1;
    }
    for (int i = 0; i < 6; ++i)
        controller_names[i] = (void *(*)())&tags[i];
    revision = goalCount = setTimeCount = 0;
#if defined(COD2_CODX) && COD2_CODX
    Cod2x_ResetAnimation();
#endif
    freeCount = createCount = clearTreeCount = 0;
    bgs->GetXModel = get_model;
    bgs->CreateDObj = create_dobj;
    bgs->SafeDObjFree = free_dobj;
    weight = 1.0f;
    lastTime = lastRate = -1;
    clientInfo_t *ci = &bgs->clientinfo[5];
    ci->clientNum = 5;
    ci->pXAnimTree = &animationTree;
    return ci;
}

static void check_play(void)
{
    reset();
    playerState_t ps = {0};
    /* Mac BG_PlayAnim 0x1820c0 stores torso duration without setting wasSet. */
    assert(BG_PlayAnim(&ps, 2, (animBodyPart_t)2, 250, 1, 0, 0) == -1);
    assert(ps.torsoAnim == 514 && ps.torsoTimer == 250 && ps.torsoAnimDuration == 250);
    assert(BG_PlayAnim(&ps, 1, (animBodyPart_t)1, 350, 1, 0, 0) == 350);
}

static void check_condition(void)
{
    reset();
    playerState_t ps = {.clientNum = 5, .fWeaponPosFrac = 0.5f};
    pmove_t pm = {.ps = &ps};
    /* Mac 0x182922 compares ps+0xe8 (view pitch) against zero. */
    ps.viewangles[0] = -10;
    BG_AnimUpdatePlayerStateConditions(&pm);
    assert(bgs->clientinfo[5].clientConditions[4][0] == 0);
    ps.viewangles[0] = 10;
    ps.fWeaponPosFrac = 0;
    BG_AnimUpdatePlayerStateConditions(&pm);
    assert(bgs->clientinfo[5].clientConditions[4][0] == 1);
}

static void check_selection(void)
{
    reset();
    playerState_t ps = {.clientNum = 5};
    animScript_t *script = &globalScriptData->scriptAnims[0][1];
    animScriptItem_t *item = &globalScriptData->scriptItems[0];
    script->numItems = 1; script->items[0] = item; item->numCommands = 3;
    for (int i = 0; i < 3; ++i) {
        item->commands[i].bodyPart[0] = 1;
        item->commands[i].animIndex[0] = i + 1;
        item->commands[i].animDuration[0] = 500;
    }
    srand(19);
    int expectedRand = rand();
    srand(19);
    /* Mac 0x187576 loads clientNum before idiv numCommands; no rand call. */
    assert(BG_AnimScriptAnimation(&ps, (aistateEnum_t)0, (scriptAnimMoveTypes_t)1, 1) == 1);
    assert(ps.legsAnim == 515);
    assert(rand() == expectedRand);
    assert(BG_AnimScriptAnimation(&ps, (aistateEnum_t)0, (scriptAnimMoveTypes_t)1, 1) == 0);
    assert(ps.legsAnim == 515);
}

static void check_lerp(void)
{
    clientInfo_t *ci = reset();
    entityState_t es = {.clientNum = 5};
    ci->legs.animationNumber = 1;
    ci->legs.animation = &globalScriptData->animations[1];
    globalScriptData->animations[1].movetype = 1LL << ANIM_MT_WALK;
    globalScriptData->animations[2].movetype = 1LL << ANIM_MT_WALKCR;
    weight = 0;
    BG_RunLerpFrameRate(ci, &ci->legs, 2, &es);
    /* 0x18357a-0x183598: remaining stance duration maximizes initial lerp.
       0x183ab7 onward: a moving loop inherits the previous loop's phase. */
    assert(ci->legs.animationTime == 400 && ci->stanceTransitionTime == 1400);
    assert(setTimeCount == 1 && lastTime == 0.25f);
    weight = 1;
    bgs->latestSnapshotTime = 1100; es.pos.trBase[0] = 15;
    BG_RunLerpFrameRate(ci, &ci->legs, 2, &es);
    assert(ci->legs.animSpeedScale == 1.5f);
    BG_RunLerpFrameRate(ci, &ci->legs, 2, &es);
    assert(ci->legs.animSpeedScale == 1.5f && lastRate == 1.5f);

    ci = reset();
    ci->legs.animationNumber = 1;
    BG_RunLerpFrameRate(ci, &ci->legs, 1, &es);
    assert(ci->legs.animation == &globalScriptData->animations[1]);
    assert(ci->legs.animationTime == 0);

    ci = reset();
    es.eFlags = 0x80000;
    globalScriptData->animations[4].flags = 0x40;
    globalScriptData->animations[4].noteType = 9;
    looped[4] = 0;
    BG_RunLerpFrameRate(ci, &ci->torso, 4, &es);
    assert(goalCount == 3);
    assert(goalIndices[0] == 4 && noteTypes[0] == 0);
    assert(goalIndices[1] == 6 && noteTypes[1] == 9);
    assert(goalIndices[2] == 7 && noteTypes[2] == 9);
    assert(!signbit(BG_Fabsf(-0.0f)));
}

static void check_angles(void)
{
    clientInfo_t *ci = reset();
    entityState_t es = {.clientNum = 5};
    ci->clientConditions[3][0] = 1 << ANIM_MT_IDLE;
    ci->playerAngles[1] = 20;
    BG_PlayerAnimation(&object, &es, ci);
    /* Idle legs retain their angle inside the Mac's 40-degree tolerance. */
    assert(ci->legs.yawAngle == 0.0f && ci->legs.yawing == 0);

    ci = reset();
    ci->clientConditions[3][0] = 1 << ANIM_MT_IDLE;
    ci->playerAngles[1] = 20;
    es.eFlags = 0x40000;
    BG_PlayerAnimation(&object, &es, ci);
    assert(ci->legs.yawAngle == 0.0f);

    ci = reset();
    ci->playerAngles[1] = 100;
    bgs->frametime = 0;
    es.eFlags = 0x40000 | 0x40;
    BG_PlayerAnimation(&object, &es, ci);
    assert(ci->torso.yawAngle == 54.99755859375f);

    ci = reset();
    ci->playerAngles[1] = 20; ci->lerpMoveDir = 45;
    ci->clientConditions[3][0] = 1 << ANIM_MT_CLIMBUP;
    es.eFlags = 0x100;
    BG_PlayerAnimation(&object, &es, ci);
    assert(ci->torso.yawAngle == 19.9951171875f && ci->legs.yawAngle == 19.9951171875f);
}

static void check_controllers(void)
{
    int bits[4] = {0};
    for (int prone = 0; prone < 2; ++prone) {
        clientInfo_t *ci = reset();
        entityState_t es = {.clientNum = 5, .eFlags = prone ? 8 : 0, .fTorsoPitch = 15, .fWaistPitch = 5};
        BG_Player_DoControllers(&object, &es, bits, ci, 1000);
        /* Mac 0x1832e0 and 0x183390: torso minus waist for back_low;
           the pelvis takes the opposite subtraction. */
        assert(ci->angles[0][0] == 10.0f);
        assert(ci->angles[5][0] == -10.0f);
    }
}

static void check_dobj(void)
{
    clientInfo_t *ci = reset();
    entityState_t es = {.clientNum = 5};
    ci->infoValid = 1;
    strcpy(ci->model, "synthetic");
    BG_UpdatePlayerDObj(NULL, &es, ci, 0);
    assert(freeCount == 0 && createCount == 1);
    BG_UpdatePlayerDObj(&object, &es, ci, 0);
    assert(freeCount == 0 && createCount == 1);
    ci->dobjDirty = 1;
    BG_UpdatePlayerDObj(&object, &es, ci, 0);
    assert(freeCount == 1 && createCount == 2);
    ci->infoValid = 0;
    BG_UpdatePlayerDObj(&object, &es, ci, 0);
    assert(freeCount == 2 && createCount == 2 && clearTreeCount == 1);
}

static void check_parser(void)
{
    reset();
    animScriptItem_t item = {0};
    const char *text = "legs anim1 torso anim2 sound synthetic\nboth anim3\n}";
    /* Body pairs occupy the Mac's first cleared word, rather than soundAlias. */
    item.commands[1].bodyPart[1] = 123;
    Com_BeginParseSession("F1 animation fixture");
    BG_ParseCommands(&text, &item, globalScriptData);
    Com_EndParseSession();
    assert(item.numCommands == 2);
    assert(item.commands[0].bodyPart[0] == 1 && item.commands[0].bodyPart[1] == 2);
    assert(item.commands[0].animIndex[0] == 1 && item.commands[0].animIndex[1] == 2);
    assert(item.commands[1].bodyPart[0] == 3 && item.commands[1].bodyPart[1] == 0);
}

static void check_codx(void)
{
#if defined(COD2_CODX) && COD2_CODX
    clientInfo_t *ci = reset();
    entityState_t es = {.clientNum = 5, .eFlags = 8};
    vec3_t goals[8] = {{0}};
    cod2xAnimationState_t *state = BG_Cod2xState(ci);
    ci->clientConditions[3][0] = 1 << ANIM_MT_WALKPRONE;
    ci->lerpLean = 0.0025f;
    goals[0][0] = 0.3f;
    BG_Cod2xControllerGoals(&es, ci, goals, 0.0f, state);
    /* CoD2x animation.cpp:380-389 applies the 30 and 10 degree
       adjustments separately; their intermediate rounding is observable. */
    assert(goals[0][0] == -39.3000030517578125f);

    ci = reset();
    state = BG_Cod2xState(ci);
    state->stance = 2; state->stanceStart = 1000; state->stanceDuration = 400;
    bgs->time = 1001;
    es.eFlags = 0;
    memset(goals, 0, sizeof(goals));
    BG_Cod2xControllerGoals(&es, ci, goals, 0.0f, state);
    /* CoD2x animation.cpp:508-526 uses remaining-time subtraction and
       double cosine before the float curve is applied to controller goals. */
    assert(goals[7][0] == -0.0008566859178245068f);

    ci = reset();
    state = BG_Cod2xState(ci);
    state->movement = 257; state->movementStart = 1000; state->movementDuration = 250;
    ci->torso.pitchAngle = ci->playerAngles[0] = 50;
    bgs->time = 1001; revision = 3;
    int bits[4] = {0};
    BG_Player_DoControllers(&object, &es, bits, ci, 1);
    assert(ci->angles[0][0] == 0.040000081062316895f);
#endif
}

static snd_alias_list_t *sound_alias(const char *name)
{ assert(strcmp(name, "synthetic") == 0); return NULL; }

int main(int argc, char **argv)
{
    struct { const char *name; void (*run)(void); } cases[] = {
        {"play", check_play}, {"condition", check_condition}, {"selection", check_selection},
        {"lerp", check_lerp}, {"angles", check_angles}, {"controllers", check_controllers},
        {"dobj", check_dobj}, {"parser", check_parser}, {"codx", check_codx}
    };
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (argc > 1 && strcmp(argv[1], cases[i].name) != 0)
            continue;
        /* reset() keeps this callback in the real script data used by parser. */
        cases[i].run();
        printf("PASS F1 animation %s\n", cases[i].name);
    }
    return 0;
}
