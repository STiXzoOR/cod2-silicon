#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* A dedicated server registers the controls and applies them to its own
   controllers, as CoD2x servers do; only the debug drawing is client side. */
#include "cod2x_pose.h"
#if !defined(DEDICATED)
#include "cod2x_features.h"
#endif
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterVec3(const char *, float, float, float, float, float, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern const dvar_t *com_sv_running;
extern playerState_t *SV_GameClientNum(int);
extern gentity_t *SV_GentityNum(int);
extern void G_GetPlayerViewOrigin(const gentity_t *, vec_t *);
extern void CL_AddDebugLine(const vec_t *, const vec_t *, const vec_t *, qboolean, int, qboolean);
extern void CL_AddDebugString(const vec_t *, const vec_t *, float, const char *, qboolean);

static const dvar_t *debug, *eyePosition, *offsetEnable, *neutral, *offsets[8];

void Cod2x_PoseInit(void)
{
    /* Controls and controller order: CoD2x shared/animation.cpp:1423. */
    static const char *names[8] = {
        "player_offsetAngleBackLow", "player_offsetAngleBackMid", "player_offsetAngleBackUp",
        "player_offsetAngleNeck", "player_offsetAngleHead", "player_offsetAnglePelvis",
        "player_offsetAngleOrigin", "player_offsetPositionOrigin"
    };
    int i;
    debug = Dvar_RegisterInt("player_debug", 0, 0, 2, 0x1080);
    eyePosition = Dvar_RegisterBool("player_debugEyePosition", 0, 0x1080);
    offsetEnable = Dvar_RegisterBool("player_offsetEnable", 0, 0x1080);
    neutral = Dvar_RegisterBool("player_offsetNeutral", 0, 0x1080);
    for (i = 0; i < 8; ++i)
        offsets[i] = Dvar_RegisterVec3(names[i], 0, 0, 0, -FLT_MAX, FLT_MAX, 0x1080);
}

int Cod2x_PoseNeutral(void)
{
    return neutral && neutral->current.enabled && com_sv_running && com_sv_running->current.enabled;
}

void Cod2x_PoseReset(entityState_t *entity)
{
    playerState_t *state;
    if (!Cod2x_PoseNeutral() || entity->clientNum < 0 || entity->clientNum >= 64)
        return;
    state = SV_GameClientNum(entity->clientNum);
    if (!state)
        return;
    state->torsoAnim = (state->torsoAnim & 0x200) ^ 0x200;
    state->legsAnim = (state->legsAnim & 0x200) ^ 0x200;
    entity->torsoAnim = (entity->torsoAnim & 0x200) ^ 0x200;
    entity->legsAnim = (entity->legsAnim & 0x200) ^ 0x200;
}

void Cod2x_PoseOffsets(vec3_t goals[8])
{
    int i, axis;
    if (!offsetEnable || !offsetEnable->current.enabled)
        return;
    for (i = 0; i < 8; ++i)
        for (axis = 0; axis < 3; ++axis)
            goals[i][axis] += offsets[i]->current.vector[axis];
}

void Cod2x_PoseMovement(int fromServer, int time, unsigned int movement, int duration, int finished)
{
    extern void Com_Printf(const char *, ...);
    if (!fromServer && debug && debug->current.integer > 0) {
        if (finished)
            Com_Printf("%i  Controller movement finished\n", time);
        else
            Com_Printf("%i  Controller movement type: %i   %i\n", time, movement, duration);
    }
}

#if !defined(DEDICATED)
extern void AngleVectors(const vec_t *, vec_t *, vec_t *, vec_t *);
extern const float AngleSubtract(const float, const float);
extern float GetLeanFraction(const float);
extern animStringItem_t animMoveTypesStr[44];
extern bgs_t *bgs;

static void Cod2x_PoseString(vec3_t origin, float step, const vec4_t color, float scale,
                            int fromServer, const char *format, ...)
{
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    origin[2] -= step;
    CL_AddDebugString(origin, color, scale, text, fromServer);
}

static void Cod2x_PoseFlagText(unsigned int flags, char *text, size_t size)
{
    /* Names displayed by the upstream pose debugger, including its unknown bits. */
    static const char *names[32] = {
        "0x00000001", "UNK2", "CROUCH", "PRONE", "0x00000010", "0x00000020",
        "FIRING", "NO_COMMUNICATION", "MG", NULL, "0x00000400", "AIMASSIST",
        "0x00001000", "0x00002000", "MANTLE", "0x00008000", "0x00010000",
        "DEAD", "ADS", "BODY_START", "0x100000", "MENU", "PING", "0x0800000",
        "0x1000000", "0x2000000", "0x4000000", "0x8000000", "0x10000000",
        "0x20000000", "0x40000000", "0x80000000"
    };
    unsigned int bit;
    size_t used = (size_t)snprintf(text, size, "eFlags:");
    for (bit = 0; bit < 32 && used < size; ++bit) {
        unsigned int mask = bit == 8 ? 0x300u : 1u << bit;
        if ((flags & mask) && names[bit])
            used += (size_t)snprintf(text + used, size - used, "%s.", names[bit]);
    }
}
#endif

void Cod2x_PoseDiagnostics(const entityState_t *entity, const clientInfo_t *info,
                           const vec3_t goals[8], int fromServer, int frametime)
{
#if defined(DEDICATED)
    (void)entity, (void)info, (void)goals, (void)fromServer, (void)frametime;
#else
    vec4_t crossColor = {1, .5f, .5f, 1};
    const vec4_t white = {1, 1, 1, 1};
    vec3_t origin, start, end, forward, right, column;
    playerState_t *state = NULL;
    char text[1024];
    int axis, i;
    if (!debug || (!debug->current.integer && !eyePosition->current.enabled))
        return;
    memcpy(origin, entity->pos.trBase, sizeof(origin));
    origin[2] += 60;
    if (com_sv_running && com_sv_running->current.enabled &&
        info->clientNum >= 0 && info->clientNum < 64) {
        const gentity_t *player = SV_GentityNum(info->clientNum);
        state = SV_GameClientNum(entity->clientNum);
        if (player && player->client) {
            G_GetPlayerViewOrigin(player, origin);
            crossColor[1] = crossColor[2] = 1;
        }
    }
    for (axis = 0; axis < 3; ++axis) {
        memcpy(start, origin, sizeof(start));
        memcpy(end, origin, sizeof(end));
        start[axis] -= 3;
        end[axis] += 3;
        CL_AddDebugLine(start, end, crossColor, 1, 0, 0);
    }
    if (debug->current.integer != fromServer + 1)
        return;
    AngleVectors(info->playerAngles, forward, right, NULL);
    for (axis = 0; axis < 3; ++axis)
        column[axis] = origin[axis] + 25 * right[axis] + 50 * forward[axis];
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "%s", fromServer ? "Server" : "Client");
    Cod2x_PoseFlagText((unsigned int)entity->eFlags, text, sizeof(text));
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "%s", text);
    {
        size_t used = (size_t)snprintf(text, sizeof(text), "MOVETYPE 0x%x: ", info->clientConditions[3][0]);
        unsigned long long moves = (unsigned int)info->clientConditions[3][0] |
            ((unsigned long long)(unsigned int)info->clientConditions[3][1] << 32);
        for (i = 0; i < NUM_ANIM_MOVETYPES && used < sizeof(text); ++i) {
            if (moves & (1ULL << i)) {
                const char *name = i == 0 ? "UNUSED" : animMoveTypesStr[i].string;
                while (name && *name && used + 2 < sizeof(text))
                    text[used++] = (char)toupper((unsigned char)*name++);
                text[used++] = '.';
                text[used] = 0;
            }
        }
    }
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "%s", text);
    for (i = 0; i < 2; ++i) {
        const lerpFrame_t *frame = i ? &info->legs : &info->torso;
        int index = (i ? entity->legsAnim : entity->torsoAnim) & ~0x200;
        if (frame->animation)
            Cod2x_PoseString(column, i ? 5 : 10, white, .25f, fromServer, "%i %s (0x%x)",
                            frame->animationNumber, frame->animation->name,
                            bgs->animScriptData.animations[index].flags);
        else
            Cod2x_PoseString(column, i ? 5 : 10, white, .25f, fromServer, "-no torso anim-");
    }
    if (state) {
        Cod2x_PoseString(column, 10, white, .25f, fromServer, "torsoAnim:%i, torsoAnimDuration:%i, torsoTimer:%i",
                        state->torsoAnim, state->torsoAnimDuration, state->torsoTimer);
        Cod2x_PoseString(column, 5, white, .25f, fromServer, "legsAnim:%i, legsAnimDuration:%i, legsTimer:%i",
                        state->legsAnim, state->legsAnimDuration, state->legsTimer);
    }
    for (axis = 0; axis < 3; ++axis)
        column[axis] = origin[axis] - 50 * right[axis] + 50 * forward[axis];
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "ci->movementYaw: %.2f", info->lerpMoveDir);
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "torso yaw: %.2f", info->torso.yawAngle);
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "legs yaw:  %.2f", info->legs.yawAngle);
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "torso-legs dif: %.2f", AngleSubtract(info->legs.yawAngle, info->torso.yawAngle));
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "movementYawAngle: %.2f", info->lerpMoveDir);
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "frameTime: %i", frametime);
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "leanf: %.3f %.3f", entity->leanf, GetLeanFraction(info->lerpLean));
    Cod2x_PoseString(column, 5, white, .25f, fromServer, "fTorsoHeight: %.3f", entity->fTorsoHeight);
    for (axis = 0; axis < 3; ++axis)
        column[axis] = origin[axis] - 80 * right[axis];
    column[2] += 50;
    for (i = 0; i < 8; ++i) {
        static const char *names[8] = {"back_low ", "back_mid ", "back_up   ", "neck       ", "head       ", "pelvis     ", "origin     ", "offset     "};
        const float *current = i < 6 ? info->angles[i] : i == 6 ? info->tag_origin_angles : info->tag_origin_offset;
        float squared = 0;
        vec4_t color = {0, 1, 0, 1};
        for (axis = 0; axis < 3; ++axis) {
            float delta = AngleSubtract(goals[i][axis], current[axis]);
            squared += delta * delta;
        }
        float distance = sqrtf(squared);
        if (distance >= .01f) { color[0] = 1; color[1] = 0; }
        Cod2x_PoseString(column, 4, color, .3f, fromServer, "%s: %03.3f %03.3f %03.3f -> %03.3f %03.3f %03.3f (%01.2f)",
                        names[i], goals[i][0], goals[i][1], goals[i][2], current[0], current[1], current[2], distance);
    }
    Cod2x_PoseString(column, 6, white, .3f, fromServer, "player angles: %.3f %.3f %.3f",
                    info->playerAngles[0], info->playerAngles[1], info->playerAngles[2]);
#endif
}
#endif
