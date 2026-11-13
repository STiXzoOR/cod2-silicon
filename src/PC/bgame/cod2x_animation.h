#ifndef COD2X_ANIMATION_H
#define COD2X_ANIMATION_H

#include "../qcommon/cod2x_math.h"

/* Behavioral reference: CoD2x src/shared/animation.cpp:293-541, 749-789.
   These source-level controllers use typed fields, never the hook ABI/layout. */
typedef struct {
    int stance;
    int stanceStart;
    int stanceDuration;
    unsigned int movement;
    int movementEnd;
    int movementRunning;
    int movementDuration;
    vec3_t start[8];
} cod2xAnimationState_t;

static cod2xAnimationState_t cod2xAnimationState[2][64];

void Cod2x_ResetAnimation(void)
{
    memset(cod2xAnimationState, 0, sizeof(cod2xAnimationState));
}
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX

void Cod2x_ResetClientAnimation(void)
{
    /* Native cgame owns animation user 0; the authoritative host owns user 1. */
    memset(cod2xAnimationState[0], 0, sizeof(cod2xAnimationState[0]));
}
#endif

static cod2xAnimationState_t *BG_Cod2xState(const clientInfo_t *ci)
{
    cod2xAnimationState_t *state;
    if ((unsigned int)ci->clientNum >= 64 || (unsigned int)bgs->anim_user >= 2)
        return NULL;
    state = &cod2xAnimationState[bgs->anim_user][ci->clientNum];
    return state;
}

static float BG_Cod2xClamp(float value, float lo, float hi)
{
    return Cod2x_Clamp(value, lo, hi);
}

static float *BG_Cod2xController(clientInfo_t *ci, int index)
{
    if (index < 6)
        return ci->angles[index];
    return index == 6 ? ci->tag_origin_angles : ci->tag_origin_offset;
}

static void BG_Cod2xRotate(float parentYaw, vec3_t child)
{
    float radians = Cod2x_DegreesToRadians(child[1] - parentYaw);
    float pitch = child[0];
    float roll = child[2];
    child[0] = pitch * cosf(radians) - roll * sinf(radians);
    child[2] = pitch * sinf(radians) + roll * cosf(radians);
}

/* CoD2x animation.cpp:1094-1149 includes running, turning and stumble crouches. */
static int BG_Cod2xStance(const animation_t *anim)
{
    const unsigned long long crouch = (1ULL << ANIM_MT_IDLECR) |
        (1ULL << ANIM_MT_WALKCR) | (1ULL << ANIM_MT_WALKCRBK) |
        (1ULL << ANIM_MT_RUNCR) | (1ULL << ANIM_MT_RUNCRBK) |
        (1ULL << ANIM_MT_TURNRIGHTCR) | (1ULL << ANIM_MT_TURNLEFTCR) |
        (1ULL << ANIM_MT_STUMBLE_CROUCH_FORWARD) |
        (1ULL << ANIM_MT_STUMBLE_CROUCH_BACKWARD);
    const unsigned long long prone = (1ULL << ANIM_MT_IDLEPRONE) |
        (1ULL << ANIM_MT_WALKPRONE) | (1ULL << ANIM_MT_WALKPRONEBK);
    if (anim && (anim->movetype & crouch))
        return 1;
    return anim && (anim->movetype & prone) ? 2 : 0;
}

/* CoD2x animation.cpp:1214-1268; cod2_player.h:39-40. */
static void BG_Cod2xBlendTime(clientInfo_t *ci, lerpFrame_t *lf,
                              const animation_t *previous, const animation_t *next)
{
    int before = BG_Cod2xStance(previous);
    int after = BG_Cod2xStance(next);
    int transition = 0;
    int minimum = -1;
    cod2xAnimationState_t *state = BG_Cod2xState(ci);
    if (!previous && lf == &ci->legs) {
        lf->animationTime = 0;
        return;
    }
    if (lf == &ci->legs && next) {
        if (before == 0 && after == 1) transition = 1;
        if (before == 1 && after == 2) transition = 2;
        if (before == 2 && after == 1) transition = 3;
        if (before == 1 && after == 0) transition = 4;
    }
    lf->animationTime = next ? next->initialLerp : 200;
    if (transition) {
        int duration = (transition == 1 || transition == 4) ? 200 : 400;
        /* Upstream truncates its binary64 400 * 0.58 result to 231 ms. */
        lf->animationTime = transition == 2 ? 231 : duration;
        if (state) {
            state->stance = transition;
            state->stanceStart = bgs->time;
            state->stanceDuration = duration;
        }
    } else {
        if (!next || lf->animationTime <= 0) {
            minimum = next && next->moveSpeed != 0.0f ? 120 :
                (previous && previous->moveSpeed != 0.0f ? 250 : 170);
        }
        if (lf->animationTime < minimum)
            lf->animationTime = minimum;
    }
}

/* The replacement controller computes decimal products in double before
   storing float goals, matching its C++ source even on older servers. */
static void BG_Cod2xBaseControllerGoals(const entityState_t *es, const clientInfo_t *ci,
                                       vec3_t goals[8], float *leanOut)
{
    float torso[3] = {0.0f, ci->torso.yawAngle, 0.0f};
    float head[3];
    float origin[3] = {0.0f, ci->legs.yawAngle, 0.0f};
    if (!(ci->clientConditions[3][0] & ((1U << ANIM_MT_CLIMBUP) | (1U << ANIM_MT_CLIMBDOWN)))) {
        torso[0] = ci->torso.pitchAngle;
        if (es->eFlags & 8) {
            torso[0] = Cod2x_AngleNormalize180(torso[0]);
            torso[0] = (float)((double)torso[0] * (torso[0] <= 0.0f ? 0.25 : 0.5));
        }
    }
    Cod2x_AnglesSubtract(ci->playerAngles, torso, head);
    Cod2x_AnglesSubtract(torso, origin, torso);
    memset(goals, 0, sizeof(vec3_t) * 8);
    goals[7][2] = es->fTorsoHeight;
    float lean = GetLeanFraction(ci->lerpLean);
    *leanOut = lean;
    torso[2] = (float)((double)lean * 50.0 * 0.925);
    head[2] = torso[2];
    if (lean != 0.0f) {
        double lateral = (es->eFlags & 4) ? (lean <= 0.0f ? 12.5 : 2.5) : (lean <= 0.0f ? 5.0 : 2.5);
        goals[7][1] = (float)(-(double)lean * lateral + (double)goals[7][1]);
    }
    if (!(es->eFlags & 0x20000))
        origin[1] = Cod2x_AngleSubtract(origin[1], ci->playerAngles[1]);
    if (es->eFlags & 8) {
        if (lean != 0.0f)
            head[2] = (float)((double)head[2] * 0.5);
        float radians = (float)((double)torso[1] * 0.0174532925199433);
        float sine = sinf(radians), cosine = cosf(radians);
        origin[0] += es->fTorsoPitch;
        goals[7][0] = (float)((double)goals[7][0] + (1.0 - (double)cosine) * -24.0);
        goals[7][1] = (float)((double)goals[7][1] + (double)sine * -12.0);
        if (lean * sine > 0.0f)
            goals[7][1] = (float)((double)goals[7][1] + -(double)lean * (1.0 - (double)cosine) * 16.0);
        goals[0][1] = (float)((double)torso[2] * -1.2);
        goals[0][2] = (float)((double)torso[2] * 0.3);
        goals[1][1] = (float)((double)torso[1] * 0.1 - (double)torso[2] * 0.2);
        goals[1][2] = (float)((double)torso[2] * 0.2);
        goals[2][0] = torso[0];
        goals[2][1] = (float)((double)torso[1] * 0.8 + (double)torso[2]);
        goals[2][2] = (float)((double)torso[2] * -0.2);
    } else {
        if (lean != 0.0f && (!(es->eFlags & 4) || lean <= 0.0f)) {
            torso[2] = (float)((double)torso[2] * 1.25);
            head[2] = (float)((double)head[2] * 1.25);
        }
        origin[2] = (float)((double)origin[2] + (double)lean * 3.75);
        goals[0][0] = (float)((double)torso[0] * 0.2);
        goals[0][1] = (float)((double)torso[1] * 0.4);
        goals[0][2] = (float)((double)torso[2] * 0.5);
        goals[1][0] = (float)((double)torso[0] * 0.3);
        goals[1][1] = (float)((double)torso[1] * 0.4);
        goals[1][2] = (float)((double)torso[2] * 0.5);
        goals[2][0] = (float)((double)torso[0] * 0.5);
        goals[2][1] = (float)((double)torso[1] * 0.2);
        goals[2][2] = (float)((double)torso[2] * -0.6);
    }
    if (es->fTorsoPitch != 0.0f || es->fWaistPitch != 0.0f) {
        goals[0][0] += Cod2x_AngleSubtract(es->fTorsoPitch, es->fWaistPitch);
        goals[5][0] = Cod2x_AngleSubtract(es->fWaistPitch, es->fTorsoPitch);
    }
    goals[3][0] = (float)((double)head[0] * 0.3);
    goals[3][1] = (float)((double)head[1] * 0.3);
    goals[4][0] = (float)((double)head[0] * 0.7);
    goals[4][1] = (float)((double)head[1] * 0.7);
    goals[4][2] = (float)((double)head[2] * -0.3);
    memcpy(goals[6], origin, sizeof(vec3_t));
}

static void BG_Cod2xControllerGoals(const entityState_t *es, clientInfo_t *ci,
                                     vec3_t goals[8], float lean,
                                     cod2xAnimationState_t *state)
{
    unsigned int moves = (unsigned int)ci->clientConditions[3][0];
    int flags = bgs->animScriptData.animations[es->legsAnim & ~0x200].flags;
    int forward = (moves & ((1U << ANIM_MT_WALK) | (1U << ANIM_MT_RUN) |
        (1U << ANIM_MT_WALKCR) | (1U << ANIM_MT_RUNCR) | (1U << ANIM_MT_WALKPRONE))) != 0;
    int backward = (moves & ((1U << ANIM_MT_WALKBK) | (1U << ANIM_MT_RUNBK) |
        (1U << ANIM_MT_WALKCRBK) | (1U << ANIM_MT_RUNCRBK) | (1U << ANIM_MT_WALKPRONEBK))) != 0;
    int left = (flags & 0x10) != 0;
    int right = (flags & 0x20) != 0;
    float yaw = ci->lerpMoveDir;
    unsigned int movement = 1U | (forward ? 32U : backward ? 64U : 0U) |
        (left ? 8U : right ? 16U : 0U);
    int duration = (es->eFlags & 8) ? 400 : 250;
    int i;

    if ((forward && yaw > 20.0f) || (backward && yaw < -20.0f)) movement |= 16384U;
    else if ((forward && yaw < -20.0f) || (backward && yaw > 20.0f)) movement |= 32768U;

    if (!(es->eFlags & 8)) {
        movement |= (es->eFlags & 4) ? 131072U : 65536U;
        if (lean < 0.0f && (forward || backward) && yaw > 0.0f && yaw < 90.0f) {
            float tilt = -lean * ((es->eFlags & 4) ? 3.8f : 7.2f);
            goals[6][0] += tilt;
            goals[6][2] -= tilt;
            movement |= (es->eFlags & 4) ? 256U : 128U;
        }
        if ((es->eFlags & 4) && lean < 0.0f && (forward || backward)) {
            goals[0][0] -= 40.0f * lean;
            goals[0][1] -= 30.0f * lean;
            goals[1][0] += 20.0f * lean;
            goals[2][0] += 20.0f * lean;
        }
        BG_Cod2xRotate(goals[6][1], goals[0]);
    } else {
#if defined(COD2_X64)
        float scale = 1.0f;
        if (ci->lerpLean < 0.0f)
            scale = (0.25f + ci->lerpLean) / 0.25f;
        else if (ci->lerpLean > 0.0f)
            scale = (0.25f - ci->lerpLean) / 0.25f;
#else
        float scale = 1.0f - (ci->lerpLean < 0.0f ? -ci->lerpLean : ci->lerpLean) / 0.25f;
        float flatten = ((forward || backward) && !(left || right) ? 40.0f : 30.0f) * scale;
#endif
        const animation_t *torso = ci->torso.animation;
        int reload = torso && strstr(torso->name, "reload") != NULL;
        movement |= 262144U;
#if defined(COD2_X64)
        goals[0][0] += -30.0f * scale;
        goals[1][0] += 30.0f * scale;
        if ((forward || backward) && !(left || right)) {
            goals[0][0] += -10.0f * scale;
            goals[1][0] += 10.0f * scale;
        }
#else
        goals[0][0] -= flatten;
        goals[1][0] += flatten;
#endif
        goals[5][0] += 3.0f;
        if (backward && scale != 0.0f) {
            goals[7][1] -= 8.0f;
#if defined(COD2_X64)
            goals[7][2] -= 3.0f;
            goals[7][2] -= 3.0f;
#else
            goals[7][2] -= 6.0f;
#endif
            goals[0][0] -= 20.0f;
            goals[5][0] += 7.0f;
            if (torso) {
                goals[5][0] += reload ? 14.0f : 13.0f;
                movement |= reload ? 2048U : 1024U;
                if (!reload) duration = 100;
            }
        } else if ((left || right) && scale != 0.0f) {
            goals[7][2] -= 1.0f;
            duration = 150;
        } else if (forward && reload) {
            goals[5][0] += 3.0f;
            movement |= 512U;
        }
    }

    if (moves & ((1U << ANIM_MT_CLIMBUP) | (1U << ANIM_MT_CLIMBDOWN))) {
        int up = (moves & (1U << ANIM_MT_CLIMBUP)) != 0;
        goals[3][0] = (up ? 30.0f : -30.0f) + BG_Cod2xClamp(ci->playerAngles[0], -80.0f, 70.0f);
        goals[3][1] = -12.0f + Cod2x_AngleSubtract(ci->playerAngles[1], ci->torso.yawAngle);
        goals[3][2] = -9.0f;
        BG_Cod2xRotate(0.0f, goals[3]);
        if (up) {
            float pitch = sinf(Cod2x_DegreesToRadians(BG_Cod2xClamp(ci->playerAngles[0], 0.0f, 90.0f)));
            goals[3][0] += pitch * sinf(Cod2x_DegreesToRadians(BG_Cod2xClamp(yaw, -90.0f, 0.0f))) * 45.0f;
            goals[3][1] += pitch * sinf(Cod2x_DegreesToRadians(BG_Cod2xClamp(yaw, 0.0f, 90.0f))) * 30.0f;
        }
        movement |= up ? 4096U : 8192U;
    }

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    /* CoD2x animation.cpp:479-491: release the authoritative legs timer when
       the grenade throw animation no longer matches the player's stance. */
    if (bgs->anim_user == 1 && ci->legs.animation) {
        const char *name = ci->legs.animation->name;
        int wrongStance = 0;
        if (strcmp(name, "pb_stand_grenade_throw") == 0)
            wrongStance = (es->eFlags & (4 | 8)) != 0;
        else if (strcmp(name, "pb_crouch_grenade_throw") == 0)
            wrongStance = (es->eFlags & 4) == 0;
        if (wrongStance)
            SV_GameClientNum(es->clientNum)->legsTimer = 0;
    }

#endif
    if (state->stanceDuration > 0) {
#if defined(COD2_X64)
        float fraction = 1.0f - (float)(state->stanceStart + state->stanceDuration - bgs->time) / (float)state->stanceDuration;
#else
        float fraction = (float)(bgs->time - state->stanceStart) / (float)state->stanceDuration;
#endif
        if (fraction > 0.0f && fraction < 1.0f) {
            if (state->stance == 2) {
#if defined(COD2_X64)
                float down = BG_Cod2xClamp((float)((double)fraction / 0.6), 0.0f, 1.0f);
                float bounce = BG_Cod2xClamp((float)(((double)fraction - 0.4) / 0.6), 0.0f, 1.0f);
                float curve = (float)(0.5 * (1.0 - __builtin_cos(6.283185307179586 * (double)bounce)));
                float downCurve = (float)(0.5 * (1.0 - __builtin_cos(6.283185307179586 * (double)down)));
                goals[7][0] += -5.0f * downCurve;
#else
                float down = BG_Cod2xClamp(fraction / 0.6f, 0.0f, 1.0f);
                float bounce = BG_Cod2xClamp((fraction - 0.4f) / 0.6f, 0.0f, 1.0f);
                float curve = 0.5f * (1.0f - cosf(6.283185307179586f * bounce));
                goals[7][0] -= 2.5f * (1.0f - cosf(6.283185307179586f * down));
#endif
                goals[7][2] += 12.0f * curve;
                goals[6][0] -= 10.0f * curve;
                goals[0][0] += 10.0f * curve;
            } else if (state->stance == 3) {
#if defined(COD2_X64)
                float curve = (float)(0.5 * (1.0 - __builtin_cos(6.283185307179586 * (double)fraction)));
                goals[7][0] += -5.0f * curve;
#else
                goals[7][0] -= 2.5f * (1.0f - cosf(6.283185307179586f * fraction));
#endif
            }
        }
    }
    /* The goal calculation updates this interval even during an existing blend. */
    state->movementDuration = duration;
    if (state->movement != movement) {
        state->movement = movement;
        state->movementEnd = bgs->time + duration;
        state->movementRunning = 1;
        for (i = 0; i < 8; ++i)
            memcpy(state->start[i], BG_Cod2xController(ci, i), sizeof(vec3_t));
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
        Cod2x_PoseMovement(bgs->anim_user != 0, bgs->time, movement, duration, 0);
#endif
    }
}

#endif
