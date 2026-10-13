#include "camera_source.h"
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
static DObjAnimMat bones[2];
static WeaponDef weapon_defs[128];
static int view_weapon, camera_bone, available_bones, available_turret, available_player_tag;
static int random_index, shake, perturb, third_person_mode, drop_code;
static char drop_text[128];
static jmp_buf drop_return;

int BG_GetViewmodelWeaponIndex(const playerState_t *ps) { (void)ps; return view_weapon; }
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
    assert(handle == 91 && local_client == 3);
    return available_turret ? &turret_object : NULL;
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
    assert(random_index < 2);
    return random_index++ == 0 ? 1610612736 : 536870912;
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

static void Reset(void)
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

static void Weapon(void)
{
    Reset();
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 12.0f, 23.0f, 94.0f);
    Vector(cg->refdef.viewaxis[0], 0.0f, 1.0f, 0.0f);
    Vector(cg->refdef.viewaxis[1], -1.0f, 0.0f, 0.0f);
    Vector(cg->refdef.viewaxis[2], 0.0f, 0.0f, 1.0f);
    Vector(cg->refdefViewAngles, 0.0f, 90.0f, 0.0f);
    Vector(cg->viewModelOrigin, 10.0f, 20.0f, 90.0f);
    Vector(cg->viewModelAngles, 0.0f, 0.0f, 0.0f);
    Vector(cg->viewModelAxis[0], 1.0f, 0.0f, 0.0f);

    Reset();
    cg->predictedPlayerState.viewangles[1] = 90.0f;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 7.0f, 22.0f, 94.0f);
    Vector(cg->refdef.viewaxis[0], -1.0f, 0.0f, 0.0f);
    Vector(cg->refdef.viewaxis[1], 0.0f, -1.0f, 0.0f);
    Vector(cg->refdefViewAngles, 0.0f, 180.0f, 0.0f);

    Reset();
    bones[1].transWeight = 0.25f;
    CG_CalcViewValues();
    Vector(cg->refdef.viewaxis[0], 0.75f, 0.25f, 0.0f);
    Vector(cg->refdef.viewaxis[1], -0.25f, 0.75f, 0.0f);
    Vector(cg->refdefViewAngles, 0.0f, 18.4349488f, 0.0f);

    Reset();
    camera_bone = 0;
    bones[0] = (DObjAnimMat){ .quat = { 0.0f, 0.0f, 0.0f, 1.0f },
                             .trans = { 7.0f, 8.0f, 9.0f }, .transWeight = 2.0f };
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 17.0f, 28.0f, 99.0f);
    Vector(cg->refdef.viewaxis[0], 1.0f, 0.0f, 0.0f);

    Reset();
    bones[1].quat[0] = 1.0f;
    bones[1].quat[2] = 0.0f;
    CG_CalcViewValues();
    Vector(cg->refdef.viewaxis[0], 1.0f, 0.0f, 0.0f);
    Vector(cg->refdef.viewaxis[1], 0.0f, 0.0f, 1.0f);
    Vector(cg->refdef.viewaxis[2], 0.0f, -1.0f, 0.0f);

    Reset();
    bones[1].quat[1] = 1.0f;
    bones[1].quat[2] = 0.0f;
    CG_CalcViewValues();
    Vector(cg->refdef.viewaxis[0], 0.0f, 0.0f, -1.0f);
    Vector(cg->refdef.viewaxis[1], 0.0f, 1.0f, 0.0f);
    Vector(cg->refdef.viewaxis[2], 1.0f, 0.0f, 0.0f);
}

static void Locked(void)
{
    for (int flags = 0x100; flags <= 0x300; flags += 0x100) {
        for (int mode = 0; mode <= 2; ++mode) {
            Reset();
            cg->predictedPlayerState.eFlags = flags;
            cg->predictedPlayerState.viewlocked = mode;
            CG_CalcViewValues();
            Vector(cg->refdef.vieworg, 100.0f, 200.0f, 300.0f);
            Vector(cg->refdefViewAngles, mode == 2 ? 2.0f : 0.0f,
                   mode == 2 ? -3.0f : 0.0f, 0.0f);
            Vector(cg->viewModelOrigin, 100.0f, 200.0f, 300.0f);
            Vector(cg->viewModelAngles, mode == 2 ? 2.0f : 0.0f,
                   mode == 2 ? -3.0f : 0.0f, 0.0f);
        }
    }
    Reset();
    cg->predictedPlayerState.eFlags = 0x100;
    available_turret = 0;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 10.0f, 20.0f, 30.0f);
}

static void Missing(void)
{
    Reset();
    cg->predictedPlayerState.eFlags = 0x100;
    available_player_tag = 0;
    if (!setjmp(drop_return)) {
        CG_CalcViewValues();
        fprintf(stderr, "missing tag_player did not raise ERR_DROP\n");
        abort();
    }
    assert(drop_code == ERR_DROP);
    assert(!strcmp(drop_text, "\025Turret has no bone: tag_player"));
}

static void Gates(void)
{
    for (int gate = 0; gate < 6; ++gate) {
        Reset();
        switch (gate) {
        case 0: view_weapon = 0; break;
        case 1: cg_weaponsArray[1].viewModelDObj = NULL; break;
        case 2: camera_bone = -1; break;
        case 3: available_bones = 0; break;
        case 4: cg->predictedPlayerState.pm_type = 4; break;
        case 5: cg->predictedPlayerState.pm_type = 5; break;
        }
        CG_CalcViewValues();
        Vector(cg->refdef.vieworg, 10.0f, 20.0f, gate == 5 ? 30.0f : 90.0f);
        Vector(cg->refdef.viewaxis[0], 1.0f, 0.0f, 0.0f);
    }
    Reset();
    cg->cubemapShot = 1;
    cg->cubemapSize = 256;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 10.0f, 20.0f, 90.0f);
    Vector(cg->refdef.viewaxis[0], 0.0f, 0.0f, 1.0f);

    Reset();
    next_snapshot.ps.pm_type = 5;
    camera_bone = -1;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 10.0f, 20.0f, 30.0f);
}

static void Pipeline(void)
{
    Reset();
    shake = perturb = 1;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 10.0f, 22.0f, 96.0f);
    Vector(cg->viewModelOrigin, 10.0f, 20.0f, 92.0f);
    Vector(cg->viewModelAngles, 0.0f, 90.0f, 0.0f);
    Vector(cg->refdefViewAngles, 0.0f, 180.0f, 0.0f);

    Reset();
    cg->predictedPlayerState.eFlags = 0x200;
    cg->predictedPlayerState.viewlocked = 2;
    shake = perturb = 1;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 103.0f, 200.0f, 302.0f);
    Vector(cg->viewModelOrigin, 100.0f, 200.0f, 302.0f);
    Vector(cg->viewModelAngles, 2.0f, 87.0f, 0.0f);
}

static void Normal(void)
{
    Reset();
    camera_bone = -1;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 10.0f, 20.0f, 90.0f);
    Near(cg->refdef.fov_x, 90.0f);
    Near(cg->refdef.fov_y, 73.739792f);
    assert(cg->refdef.width == 640 && cg->refdef.height == 480);
}

static void Orbit(void)
{
    Reset();
    cg->renderingThirdPerson = 1;
    cg->predictedPlayerState.eFlags = 0x100;
    cg->predictedPlayerState.viewlocked = 2;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, 60.0f, 200.0f, 368.0f);
    Vector(cg->viewModelOrigin, 60.0f, 200.0f, 368.0f);
    assert(random_index == 0);

#if defined(COD2_CODX) && COD2_CODX
    Reset();
    cg->renderingThirdPerson = 1;
    third_person_mode = 1;
    cg->predictedPlayerState.viewangles[0] = 30.0f;
    cg->predictedPlayerState.viewangles[1] = 60.0f;
    orbit_angle.current.value = 20.0f;
    CG_CalcViewValues();
    Vector(cg->refdef.vieworg, -19.5976849f, -4.8354063f, 108.3527618f);
    Vector(cg->refdefViewAngles, 18.0f, 40.0f, 0.0f);
#endif
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "weapon")) Weapon();
    else if (!strcmp(argv[1], "locked")) Locked();
    else if (!strcmp(argv[1], "missing")) Missing();
    else if (!strcmp(argv[1], "gates")) Gates();
    else if (!strcmp(argv[1], "pipeline")) Pipeline();
    else if (!strcmp(argv[1], "normal")) Normal();
    else if (!strcmp(argv[1], "orbit")) Orbit();
    else abort();
    printf("PASS camera %s\n", argv[1]);
    return 0;
}
