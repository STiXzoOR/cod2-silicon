#include "active_frame_source.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
static centity_t entities[1024];
centity_t *cg_entities = entities;
scr_const_t scr_const;
weaponInfo_t cg_weaponsArray[128];
static float draw_context[32];
void *imp_cgDC = draw_context;
static dvar_t zero, bob_max, fov, fov_scale, viewsize, range, orbit_angle;
const dvar_t *bg_bobMax = &bob_max;
const dvar_t *cg_dumpAnims = &zero;
const dvar_t *cg_errorDecay = &zero;
const dvar_t *cg_fov = &fov;
const dvar_t *cg_fovMin = &zero;
const dvar_t *cg_fovScale = &fov_scale;
const dvar_t *cg_paused = &zero;
const dvar_t *cg_thirdPerson = &zero;
const dvar_t *cg_thirdPersonAngle = &orbit_angle;
const dvar_t *cg_thirdPersonRange = &range;
const dvar_t *cg_viewsize = &viewsize;
vec3_t mins = { -4.0f, -4.0f, -4.0f };
static snapshot_t next_snapshot;
static DObj_s weapon_object, turret_object;
static char calls[4096];
static int calls_len;
static void Call(int code) { assert(calls_len < (int)sizeof(calls) - 1); calls[calls_len++] = (char)code; calls[calls_len] = 0; }
static DObjAnimMat bones[2];
static WeaponDef weapon_defs[128];
static int view_weapon, camera_bone, available_bones, available_turret, available_player_tag;
static int random_index, shake, perturb, third_person_mode, drop_code;
static char drop_text[128];
static jmp_buf drop_return;

int BG_GetViewmodelWeaponIndex(const playerState_t *ps) { assert(ps == &cg->predictedPlayerState); return view_weapon; }
WeaponDef *BG_GetWeaponDef(int index) { assert(index >= 0 && index < 128); return &weapon_defs[index]; }
int BG_IsAimDownSightWeapon(int index) { (void)index; return 0; }
float CL_GetMenuBlurRadius(void) { return 0.0f; }
void SetScreenScaling(float x, float y, int left, int top, int width, int height)
{ (void)x; (void)y; (void)left; (void)top; (void)width; (void)height; }
float BG_GetBobCycle(const playerState_t *ps) { (void)ps; return 0.0f; }
float BG_GetSpeed(const playerState_t *ps, int time) { (void)ps; (void)time; return 0.0f; }
float BG_GetVerticalBobFactor(const playerState_t *ps, float cycle, float speed, float max_amp)
{ (void)ps; (void)cycle; (void)speed; (void)max_amp; return 0.0f; }
float BG_GetHorizontalBobFactor(const playerState_t *ps, float cycle, float speed, float max_amp)
{ (void)ps; (void)cycle; (void)speed; (void)max_amp; return 0.0f; }
void BG_CalculateViewAngles(viewState_t *state, vec_t *angles)
{ (void)state; angles[0] = angles[1] = angles[2] = 0.0f; }
void AddLeanToPosition(vec_t *position, float yaw, float lean, float roll, float distance)
{ (void)position; (void)yaw; (void)lean; (void)roll; (void)distance; }
int CG_PointContents(const vec_t *point, int skip, int mask)
{ (void)point; (void)skip; (void)mask; return 0; }
unsigned int CG_ShakeCamera(void)
{
    if (shake) {
        cg->refdef.vieworg[2] += 2.0f;
        cg->refdefViewAngles[1] += 90.0f;
    }
    return 0;
}
void CG_PerturbCamera(void) { if (perturb) cg->refdef.vieworg[0] += 3.0f; }
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(errorParm_t code, const char *format, ...)
{
    va_list args;
    drop_code = code;
    va_start(args, format);
    vsnprintf(drop_text, sizeof(drop_text), format, args);
    va_end(args);
    longjmp(drop_return, 1);
}
int DObjGetBoneIndex(const DObj *obj, unsigned int name)
{ assert(obj == &weapon_object && name == scr_const.tag_camera); return camera_bone; }
DObjAnimMat *DObjGetRotTransArray(const struct DObj_s *obj)
{ assert(obj == &weapon_object); return available_bones ? bones : NULL; }
struct DObj_s *Com_GetClientDObj(int handle, int local_client)
{
    assert(local_client == 0);
    Call('a' + handle);
    return handle == 2 ? NULL : &turret_object;
}
qboolean CG_DObjGetWorldTagPos(const centity_t *cent, struct DObj_s *obj, unsigned int name, vec_t *position)
{
    assert(cent == &entities[7] && obj == &turret_object && name == scr_const.tag_player);
    if (!available_player_tag)
        return 0;
    position[0] = 100.0f;
    position[1] = 200.0f;
    position[2] = 300.0f;
    return 1;
}
void CG_TraceCapsule(trace_t *trace, const vec_t *start, const vec_t *minimum,
                     const vec_t *maximum, const vec_t *end, int skip, int mask)
{
    (void)start; (void)minimum; (void)maximum; (void)end; (void)skip; (void)mask;
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1.0f;
}
int rand(void)
{
    assert(random_index < 4);
    ++random_index; return 1073741824;
}
#if defined(COD2_CODX) && COD2_CODX
int Cod2x_ThirdPersonMode(void) { return third_person_mode; }
#endif

static void Near(float actual, float expected)
{
    if (!isfinite(actual) || fabsf(actual - expected) > 0.0001f) {
        fprintf(stderr, "camera value %.9g; expected %.9g\n", (double)actual, (double)expected);
        abort();
    }
}

static void Vector(const float *actual, float x, float y, float z)
{ Near(actual[0], x); Near(actual[1], y); Near(actual[2], z); }

static void ResetCamera(void)
{
    memset(cgArray, 0, sizeof(cgArray));
    memset(cgsArray, 0, sizeof(cgsArray));
    memset(&next_snapshot, 0, sizeof(next_snapshot));
    memset(entities, 0, sizeof(entities));
    memset(cg_weaponsArray, 0, sizeof(cg_weaponsArray));
    memset(bones, 0, sizeof(bones));
    memset(weapon_defs, 0, sizeof(weapon_defs));
    cg->nextSnap = &next_snapshot;
    cg->time = 1000;
    cg->predictedPlayerState.origin[0] = 10.0f;
    cg->predictedPlayerState.origin[1] = 20.0f;
    cg->predictedPlayerState.origin[2] = 30.0f;
    cg->predictedPlayerState.viewHeightCurrent = 60.0f;
    cg->predictedPlayerState.viewlocked_entNum = 7;
    cgs->viewWidth = 640;
    cgs->viewHeight = 480;
    cgs->viewAspect = 4.0f / 3.0f;
    fov.current.value = 90.0f;
    fov_scale.current.value = 1.0f;
    viewsize.current.integer = 100;
    bob_max.current.value = 8.0f;
    range.current.value = 40.0f;
    orbit_angle.current.value = 0.0f;
    scr_const.tag_camera = 51;
    scr_const.tag_player = 52;
    view_weapon = 1;
    camera_bone = 1;
    available_bones = available_turret = available_player_tag = 1;
    random_index = shake = perturb = third_person_mode = drop_code = 0;
    drop_text[0] = '\0';
    cg_weaponsArray[1].viewModelDObj = &weapon_object;
    bones[0].trans[0] = 900.0f;
    bones[1].quat[2] = bones[1].quat[3] = bones[1].transWeight = 1.0f;
    bones[1].trans[0] = 2.0f;
    bones[1].trans[1] = 3.0f;
    bones[1].trans[2] = 4.0f;
    entities[7].nextState.number = 91;
    entities[7].nextState.weapon = 2;
    entities[7].localClientNum = 3;
    weapon_defs[2].vertViewJitter = 4.0f;
    weapon_defs[2].horizViewJitter = 6.0f;
}

static snapshot_t snapshot;
static LegacyHacks legacy;
static LegacyHacks *legacy_ptr = &legacy;
static bgs_t *current_bgs;
void *imp_bgs = &current_bgs;
void *imp_legacyHacks = &legacy_ptr;
WeaponDef *bg_weaponDefs[128];
int bg_iNumWeapons = 127;
static vec3_t aim, lod;
static int snapshots, input_fire, fx_time, effect_count;
static float input_angle;
extern void BG_WeaponFireRecoil(const playerState_t *, vec_t *, vec_t *);

void CG_AddLagometerFrameInfo(void) { Call('L'); }
void CG_ProcessSnapshots(void) { assert(current_bgs == &cg->bgs); ++snapshots; Call('S'); }
void CL_SetLodOrigin(const refdef_t *fd) { memcpy(lod, fd->vieworg, sizeof(lod)); Call('O'); }
void FX_AdjustTime(int time) { fx_time = time; Call('F'); }
void CG_UpdateShellShock(const shellshock_parms_t *p, int start, int duration)
{ assert(p == cg->shellshock.parms && start == cg->shellshock.startTime && duration == cg->shellshock.duration); Call('H'); }
void CG_DObjUpdateInfo(struct DObj_s *obj) { assert(obj == &turret_object); Call('U'); }
void CG_ProcessClientNoteTracks(int number) { Call('A' + number); }
void CG_CalcEntityLerpPositions(centity_t *cent)
{ Call(cent == &cg->predictedPlayerEntity ? 'P' : '0' + (int)(cent - entities)); }
void CG_ProcessEntity(centity_t *cent)
{ Call(cent == &cg->predictedPlayerEntity ? 'Q' : '0' + (int)(cent - entities)); }
void CL_ResetSkeletonCache(int level) { assert(level == 0); Call('R'); }
void CG_AddPacketEntities(void) { Call('E'); }
void R_UpdateEffectsNonBolt(void) { Call('N'); }
void CG_AddLocalEntities(void) { Call('J'); }
void CL_Input(void) { input_angle = cg->kickAngles[0]; Call('I'); }
void CG_PredictPlayerState(void)
{
    Call('V');
    if (input_fire)
        BG_WeaponFireRecoil(&cg->predictedPlayerState, cg->vGunSpeed, cg->kickAVel);
}
void CG_UpdateViewWeaponAnim(playerState_t *ps) { assert(ps == &cg->predictedPlayerState); Call('W'); }
void SND_SetListener(int n, const vec_t *origin, vec3_t *axis)
{ assert(n == cg->nextSnap->ps.clientNum && origin == cg->refdef.vieworg && axis == cg->refdef.viewaxis); Call('T'); }
void CG_AddViewWeapon(playerState_t *ps) { assert(ps == &cg->predictedPlayerState); Call('Y'); }
void CL_FX_AdjustCamera(refdef_t *fd) { assert(fd == &cg->refdef); Call('Z'); }
EffectTemplate *FX_RegisterEffect(const char *name) { assert(name == cg->testFxName); return NULL; }
void FX_PlaySimpleEffect(EffectTemplate *fx, const vec_t *origin)
{ (void)fx; assert(origin == cg->testFxPos); ++effect_count; }
void DObjDisplayAnim(DObj *obj) { assert(obj == &turret_object); Call('D'); }
void R_UpdateEffectsBolt(void) { Call('K'); }
int CL_BeginDelayedDrawing(void) { Call('['); return 37; }
void CG_DrawPlayerSprites(void) { Call('s'); }
void CG_Draw3dHudElems(void) { Call('h'); }
unsigned int CG_Draw2D(void) { Call('d'); return 0; }
void CL_EndDelayedDrawing(int marker) { assert(marker == 37); Call(']'); }
int BG_GetNumWeapons(void) { return 127; }
void CL_SetUserCmdAimValues(vec_t *angles) { memcpy(aim, angles, sizeof(aim)); Call('u'); }
void CL_SetUserCmdValue(int weapon, int offhand, float sensitivity)
{ assert(weapon == cg->weaponSelect && offhand == cg->equippedOffHand); Near(sensitivity, 1.0f); Call('v'); }
void CL_RenderScene(const refdef_t *fd) { assert(fd == &cg->refdef); Call('r'); }
Bool CL_IsRenderingSplitScreen(void) { return 0; }
qboolean CG_DrawShellShockSavedScreenBlend(const shellshock_parms_t *p, int start, int duration)
{ assert(p == cg->shellshock.parms && start == cg->shellshock.startTime && duration == cg->shellshock.duration); Call('b'); return 0; }
void CG_TileClear(void) { Call('t'); }
void CL_IssueDelayedDrawing(int marker) { assert(marker == 37); Call('!'); }
#if COD2_FEATURE_RUMBLE
void CG_UpdateRumble(void) {}
#endif

static void Reset(void)
{
    ResetCamera();
    memset(&snapshot, 0, sizeof(snapshot));
    memset(&legacy, 0, sizeof(legacy));
    memset(calls, 0, sizeof(calls));
    memset(cg_weaponsArray, 0, sizeof(cg_weaponsArray));
    calls_len = snapshots = input_fire = effect_count = 0;
    current_bgs = NULL;
    cg->snap = &snapshot;
    cg->predictedPlayerState.viewlocked_entNum = 1023;
    cg->predictedPlayerState.weapon = 1;
    cg->nextSnap->ps.pm_flags = 0x800000;
    cg->zoomSensitivity = 1.0f;
    cg_dumpAnims = &zero;
    zero.current.integer = -1;
    /* The dvars sharing zero in the camera fixture must be disabled here. */
    static dvar_t disabled;
    cg_thirdPerson = cg_paused = cg_errorDecay = cg_fovMin = &disabled;
    weapon_defs[1].fHipViewKickCenterSpeed = 100.0f;
    weapon_defs[1].fAdsViewKickCenterSpeed = 400.0f;
    for (int i = 0; i < 128; ++i)
        bg_weaponDefs[i] = &weapon_defs[i];
}

static void Frame(int milliseconds)
{
    assert(CG_DrawActiveFrame(cg->time + milliseconds, 0, 0, 0, 1));
    assert(current_bgs == NULL);
}

static void Recoil(void)
{
    Reset();
    cg->kickAVel[0] = -100.0f;
    cg->kickAVel[1] = 40.0f;
    cg->kickAVel[2] = -20.0f;
    cg->offsetAngles[0] = 2.0f;
    Frame(5);
    Vector(cg->kickAngles, -0.5f, 0.2f, -0.1f);
    Vector(cg->kickAVel, -100.0f, 40.0f, -20.0f);
    Vector(aim, 1.5f, 0.2f, -0.1f);
    Near(input_angle, -0.5f);

    Reset();
    cg->kickAVel[0] = 100.0f;
    Frame(12); /* 5 + 5 + 2 ms: 0.5 + 0.4975 + 0.1986. */
    Near(cg->kickAngles[0], 1.1961f);
    Near(cg->kickAVel[0], 99.3f);

    Reset();
    cg->kickAngles[0] = 1.0f;
    cg->predictedPlayerState.fWeaponPosFrac = 0.5f;
    Frame(5);
    Near(cg->kickAngles[0], 0.99985f);
    Near(cg->kickAVel[0], -0.5f);
    Reset();
    cg->kickAngles[0] = 1.0f;
    cg->predictedPlayerState.fWeaponPosFrac = 0.5001f;
    Frame(5);
    Near(cg->kickAngles[0], 0.9994f);
    Near(cg->kickAVel[0], -2.0f);
    Reset();
    view_weapon = 0;
    cg->kickAngles[0] = -1.0f;
    Frame(5);
    Near(cg->kickAngles[0], -0.9964f);
    Near(cg->kickAVel[0], 12.0f);
}

static void Limits(void)
{
    Reset();
    cg->kickAngles[0] = 0.001f;
    cg->kickAVel[0] = -100.0f;
    cg->kickAngles[1] = 9.9f;
    cg->kickAVel[1] = 1000.0f;
    cg->kickAngles[2] = -9.9f;
    cg->kickAVel[2] = -1000.0f;
    Frame(5);
    Vector(cg->kickAngles, 0.0f, 10.0f, -10.0f);
    Vector(cg->kickAVel, 0.0f, 0.0f, 0.0f);
    Reset();
    cg->kickAngles[0] = 9.0f;
    cg->kickAVel[0] = 1000.0f;
    weapon_defs[1].fHipViewKickCenterSpeed = 0.0f;
    Frame(1);
    Near(cg->kickAngles[0], 10.0f); /* Equality does not clear velocity. */
    Near(cg->kickAVel[0], 1000.0f);
    Reset();
    cg->kickAngles[0] = NAN;
    Frame(5);
    Near(cg->kickAngles[0], 0.0f);
    Near(cg->kickAVel[0], 0.0f);
    Reset();
    cg->kickAngles[0] = 2.0f;
    cg->kickAVel[0] = 30.0f;
    Frame(0);
    Near(cg->kickAngles[0], 2.0f);
    Near(cg->kickAVel[0], 30.0f);
    Frame(-10);
    assert(cg->frametime == 0 && cg->oldTime == cg->time);
    Near(cg->kickAngles[0], 2.0f);
    cg->nextSnap->ps.pm_flags = 0x8000; /* Different byte: not the recoil bit. */
    Frame(5);
    Vector(cg->kickAngles, 0, 0, 0);
    Vector(cg->kickAVel, 0, 0, 0);
}

static void Entities(void)
{
    Reset();
    next_snapshot.numEntities = 3;
    next_snapshot.entities[0].number = 3;
    next_snapshot.entities[1].number = 2;
    next_snapshot.entities[2].number = 4;
    entities[3].nextState.eType = 9;
    entities[2].nextState.eType = 1;
    entities[4].nextState.eType = 10;
    Frame(5);
    assert(strcmp(calls, "LSOFHdUD3c2eUERENJIVWTYZaUAPQK[shd]uvrbt!") == 0);
    /* A mounted player has a separate late preparation pass. */
    Reset();
    next_snapshot.ps.pm_flags |= 0x400000;
    next_snapshot.ps.clientNum = 1;
    cg->predictedPlayerState.viewlocked_entNum = 8;
    /* Keep camera outside its mounted branch while testing snapshot flags. */
    Frame(5);
    assert(strstr(calls, "ZbUBPQ8K") != NULL);
}

static void Producer(void)
{
    Reset();
    weapon_defs[1].fHipViewKickPitchMin = weapon_defs[1].fHipViewKickPitchMax = 100.0f;
    weapon_defs[1].fHipViewKickYawMin = weapon_defs[1].fHipViewKickYawMax = 40.0f;
    input_fire = 1;
    Frame(5);
    Vector(cg->kickAVel, -100, 40, -20);
    Vector(aim, 0, 0, 0); /* Prediction fires after the frame's recoil integration. */
    input_fire = 0;
    Frame(5);
    Vector(aim, -0.5f, 0.2f, -0.1f);
}

static void Gates(void)
{
    for (int gate = 0; gate < 5; ++gate) {
        Reset();
        cg->kickAngles[0] = 2.0f;
        if (gate == 0) cg->infoScreenText[0] = 'x';
        if (gate == 1) cg->nextSnap = NULL;
        if (gate == 2) next_snapshot.snapFlags = 2;
        if (gate == 3) legacy.cl_serverloadmap[0] = legacy.cl_serverloadgametype[0] = 'x';
        assert(!CG_DrawActiveFrame(1005, 0, 0, 0, gate != 4));
        assert(current_bgs == NULL && cg->clientFrame == 0);
        assert(snapshots == (gate == 0 ? 0 : 1));
        Near(cg->kickAngles[0], 2.0f);
    }
    Reset();
    legacy.cl_serverloadwaiting = 1;
    legacy.cl_serverloadmap[0] = 'x'; /* Both strings are required for the gate. */
    cg->frameInterpolation = 0.25f;
    snapshot.ps.origin[0] = 4.0f;
    next_snapshot.ps.origin[0] = 12.0f;
    next_snapshot.ps.viewHeightCurrent = 60.5f;
    next_snapshot.ps.deltaTime = 20;
    snapshot.ps.shellshockIndex = 2;
    snapshot.ps.shellshockTime = 100;
    snapshot.ps.shellshockDuration = 200;
    Frame(5);
    Vector(lod, 6.0f, 0.0f, 60.5f);
    assert(fx_time == 985 && legacy.cl_serverloadwaiting == 0);
    assert(cg->shellshock.parms == &cgs->shellshockParms[2]);
    assert(cg->shellshock.startTime == 100 && cg->shellshock.duration == 200);
    cg->weaponSelect = 128;
    cg->predictedPlayerState.weaponslots[1] = 4;
    cg->testFxRespawnTime = 5;
    cg->testFxTime = 1005;
    Frame(5);
    assert(cg->weaponSelect == 0 && effect_count == 0);
    Frame(1);
    assert(effect_count == 1 && cg->testFxTime == 1011);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "recoil")) Recoil();
    else if (!strcmp(argv[1], "limits")) Limits();
    else if (!strcmp(argv[1], "entities")) Entities();
    else if (!strcmp(argv[1], "producer")) Producer();
    else if (!strcmp(argv[1], "gates")) Gates();
    else abort();
    puts(argv[1]);
    return 0;
}
