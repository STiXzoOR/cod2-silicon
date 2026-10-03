#ifndef COD2X_ANIMATION_H
#define COD2X_ANIMATION_H

/* Behavioral reference: CoD2x src/shared/animation.cpp:293-541, 749-789.
   These source-level controllers use typed fields, never the hook ABI/layout. */
typedef struct {
    const XAnimTree_s *tree;
    int lastTime;
    int stance;
    int stanceStart;
    int stanceDuration;
    unsigned int movement;
    int movementStart;
    int movementDuration;
    vec3_t start[8];
} cod2xAnimationState_t;

static cod2xAnimationState_t cod2xAnimationState[2][64];

void Cod2x_ResetAnimation(void)
{
    memset(cod2xAnimationState, 0, sizeof(cod2xAnimationState));
}

static cod2xAnimationState_t *BG_Cod2xState(const clientInfo_t *ci)
{
    cod2xAnimationState_t *state;
    if ((unsigned int)ci->clientNum >= 64 || (unsigned int)bgs->anim_user >= 2)
        return NULL;
    state = &cod2xAnimationState[bgs->anim_user][ci->clientNum];
    if (state->tree != ci->pXAnimTree || bgs->time < state->lastTime)
        memset(state, 0, sizeof(*state));
    state->tree = ci->pXAnimTree;
    state->lastTime = bgs->time;
    return state;
}

static float BG_Cod2xClamp(float value, float lo, float hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

static float *BG_Cod2xController(clientInfo_t *ci, int index)
{
    if (index < 6)
        return ci->angles[index];
    return index == 6 ? ci->tag_origin_angles : ci->tag_origin_offset;
}

static void BG_Cod2xRotate(float parentYaw, vec3_t child)
{
    float radians = (child[1] - parentYaw) * 0.017453292519943295f;
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
        lf->animationTime = transition == 2 ? 232 : duration;
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
    unsigned int movement = 1U | (forward ? 2U : backward ? 4U : 0U) |
        (left ? 8U : right ? 16U : 0U);
    int duration = (es->eFlags & 8) ? 400 : 250;
    int i;

    if ((forward && yaw > 20.0f) || (backward && yaw < -20.0f)) movement |= 32U;
    else if ((forward && yaw < -20.0f) || (backward && yaw > 20.0f)) movement |= 64U;

    if (!(es->eFlags & 8)) {
        movement |= (es->eFlags & 4) ? 128U : 256U;
        if (lean < 0.0f && (forward || backward) && yaw > 0.0f && yaw < 90.0f) {
            float tilt = -lean * ((es->eFlags & 4) ? 3.8f : 7.2f);
            goals[6][0] += tilt;
            goals[6][2] -= tilt;
            movement |= 512U;
        }
        if ((es->eFlags & 4) && lean < 0.0f && (forward || backward)) {
            goals[0][0] -= 40.0f * lean;
            goals[0][1] -= 30.0f * lean;
            goals[1][0] += 20.0f * lean;
            goals[2][0] += 20.0f * lean;
        }
        BG_Cod2xRotate(goals[6][1], goals[0]);
    } else {
        float scale = 1.0f - (ci->lerpLean < 0.0f ? -ci->lerpLean : ci->lerpLean) / 0.25f;
        float flatten = ((forward || backward) && !(left || right) ? 40.0f : 30.0f) * scale;
        const animation_t *torso = ci->torso.animation;
        int reload = torso && strstr(torso->name, "reload") != NULL;
        movement |= 1024U;
        goals[0][0] -= flatten;
        goals[1][0] += flatten;
        goals[5][0] += 3.0f;
        if (backward && scale != 0.0f) {
            goals[7][1] -= 8.0f;
            goals[7][2] -= 6.0f;
            goals[0][0] -= 20.0f;
            goals[5][0] += 7.0f;
            if (torso) {
                goals[5][0] += reload ? 14.0f : 13.0f;
                movement |= reload ? 2048U : 4096U;
                if (!reload) duration = 100;
            }
        } else if ((left || right) && scale != 0.0f) {
            goals[7][2] -= 1.0f;
            duration = 150;
        } else if (forward && reload) {
            goals[5][0] += 3.0f;
            movement |= 8192U;
        }
    }

    if (moves & ((1U << ANIM_MT_CLIMBUP) | (1U << ANIM_MT_CLIMBDOWN))) {
        int up = (moves & (1U << ANIM_MT_CLIMBUP)) != 0;
        goals[3][0] = (up ? 30.0f : -30.0f) + BG_Cod2xClamp(ci->playerAngles[0], -80.0f, 70.0f);
        goals[3][1] = -12.0f + AngleNormalize180(ci->playerAngles[1] - ci->torso.yawAngle);
        goals[3][2] = -9.0f;
        BG_Cod2xRotate(0.0f, goals[3]);
        if (up) {
            float pitch = sinf(BG_Cod2xClamp(ci->playerAngles[0], 0.0f, 90.0f) * 0.017453292519943295f);
            goals[3][0] += pitch * sinf(BG_Cod2xClamp(yaw, -90.0f, 0.0f) * 0.017453292519943295f) * 45.0f;
            goals[3][1] += pitch * sinf(BG_Cod2xClamp(yaw, 0.0f, 90.0f) * 0.017453292519943295f) * 30.0f;
        }
        movement |= up ? 16384U : 32768U;
    }

    if (state->stanceDuration > 0) {
        float fraction = (float)(bgs->time - state->stanceStart) / (float)state->stanceDuration;
        if (fraction > 0.0f && fraction < 1.0f) {
            if (state->stance == 2) {
                float down = BG_Cod2xClamp(fraction / 0.6f, 0.0f, 1.0f);
                float bounce = BG_Cod2xClamp((fraction - 0.4f) / 0.6f, 0.0f, 1.0f);
                float curve = 0.5f * (1.0f - cosf(6.283185307179586f * bounce));
                goals[7][0] -= 2.5f * (1.0f - cosf(6.283185307179586f * down));
                goals[7][2] += 12.0f * curve;
                goals[6][0] -= 10.0f * curve;
                goals[0][0] += 10.0f * curve;
            } else if (state->stance == 3) {
                goals[7][0] -= 2.5f * (1.0f - cosf(6.283185307179586f * fraction));
            }
        }
    }
    if (state->movement != movement) {
        state->movement = movement;
        state->movementStart = bgs->time;
        state->movementDuration = duration;
        for (i = 0; i < 8; ++i)
            memcpy(state->start[i], BG_Cod2xController(ci, i), sizeof(vec3_t));
    }
}

#endif
