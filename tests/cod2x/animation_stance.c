#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Full production animation TU plus the production server caller/lookup.
   Synthetic game clients and DObj services require no game data or renderer. */
#include "animation_source.h"

bgs_t level_bgs;
bgs_t *bgs;
static bgs_t client_bgs;
static level_locals_t fixture_level;
void *imp_level = &fixture_level;
void *imp_bgs = &bgs;
server_t sv;
scr_const_t scr_const;
void *(*controller_names[6])();

static gclient_t clients[8];
static struct DObj_s object;
static unsigned short tags[6] = {1, 2, 3, 4, 5, 6};
static int revision, playerLookups, expectedClient, tagCalls, originCalls;
#ifdef GAMEPLAY_MATRIX
void Fixture_SetRevision(int version);
static vec3_t emittedTags[6], emittedOriginAngles, emittedOriginOffset;
#endif

/* Count calls at the server boundary while retaining its actual stride/address
   calculation. A client controller must never enter this boundary. */
#define SV_GameClientNum Fixture_GameClientNum
#include "playerstate_source.h"
#undef SV_GameClientNum

playerState_t *SV_GameClientNum(int num)
{
    assert(bgs == &level_bgs && bgs->anim_user == 1);
    assert(num == expectedClient);
    ++playerLookups;
    return Fixture_GameClientNum(num);
}

struct DObj_s *Com_GetServerDObj(int handle)
{
    assert(bgs == &level_bgs && handle == expectedClient);
    return &object;
}

#include "server_controller_source.h"

float GetLeanFraction(float value) { return value; }
#ifndef GAMEPLAY_MATRIX
int Cod2x_GameVersion(void) { return revision; }
#endif
int Cod2x_PoseNeutral(void) { return 0; }
void Cod2x_PoseOffsets(vec3_t goals[8]) { (void)goals; }
void Cod2x_PoseReset(entityState_t *es) { (void)es; }
void Cod2x_PoseDiagnostics(const entityState_t *es, const clientInfo_t *ci, int fromServer, int frametime)
{
    (void)es; (void)ci; (void)fromServer; (void)frametime;
}

qboolean DObjSetControlTagAngles(const struct DObj_s *obj, int *partBits, unsigned int tag, vec_t *angles)
{
    (void)partBits; (void)angles;
    assert(obj == &object && tag >= 1 && tag <= 6);
#ifdef GAMEPLAY_MATRIX
    memcpy(emittedTags[tag - 1], angles, sizeof(vec3_t));
#endif
    ++tagCalls;
    return 1;
}

qboolean DObjSetLocalTag(const struct DObj_s *obj, int *partBits, unsigned int tag, const vec_t *trans, const vec_t *angles)
{
    (void)partBits; (void)tag; (void)trans; (void)angles;
    assert(obj == &object);
#ifdef GAMEPLAY_MATRIX
    memcpy(emittedOriginAngles, angles, sizeof(vec3_t));
    memcpy(emittedOriginOffset, trans, sizeof(vec3_t));
#endif
    ++originCalls;
    return 1;
}

static void checkThrow(const char *name, int flags, int user, int version, int cancel)
{
    animation_t animation = {0};
    gentity_t entity = {0};
    int partBits[4] = {0};
    clientInfo_t *ci;
    int index;

    memset(&level_bgs, 0, sizeof(level_bgs));
    memset(&client_bgs, 0, sizeof(client_bgs));
    memset(clients, 0, sizeof(clients));
    level_bgs.anim_user = 1;
    client_bgs.anim_user = 0;
    fixture_level.frametime = 50;
    expectedClient = 5;
    entity.s.number = expectedClient;
    entity.s.clientNum = expectedClient;
    entity.s.eType = 1;
    entity.s.eFlags = flags;
    entity.client = &clients[expectedClient];
    sv.gameClients = &clients[0].ps;
    sv.gameClientSize = sizeof(clients[0]);
    for (index = 0; index < 8; ++index) {
        clients[index].ps.clientNum = index;
        clients[index].ps.legsTimer = 701 + index;
        clients[index].ps.torsoTimer = 901 + index;
    }
    revision = version;
#ifdef GAMEPLAY_MATRIX
    Fixture_SetRevision(version);
#endif
    playerLookups = tagCalls = originCalls = 0;
    bgs = user == 1 ? &client_bgs : &level_bgs;
    ci = user == 1 ? &level_bgs.clientinfo[expectedClient] : &client_bgs.clientinfo[expectedClient];
    ci->clientNum = expectedClient;
    if (name) {
        strcpy(animation.name, name);
        ci->legs.animation = &animation;
    }
    /* A torso-only throw must not be mistaken for the legs animation. */
    if (!name) {
        strcpy(animation.name, "pb_stand_grenade_throw");
        ci->torso.animation = &animation;
    }

    if (user == 1) {
        G_PlayerController(&entity, partBits);
        assert(bgs == &level_bgs);
    } else {
        bgs = &client_bgs;
        BG_Player_DoControllers(&object, &entity.s, partBits, ci, fixture_level.frametime);
        assert(bgs == &client_bgs);
    }
    assert(tagCalls == 6 && originCalls == 1);
    for (index = 0; index < 8; ++index) {
        assert(clients[index].ps.legsTimer == (cancel && index == expectedClient ? 0 : 701 + index));
        assert(clients[index].ps.torsoTimer == 901 + index);
    }
    assert(playerLookups == cancel);
}

#ifdef GAMEPLAY_MATRIX
#include "animation_matrix.h"
#endif

int main(void)
{
    static const char *names[] = {
        "pb_stand_grenade_throw", "pb_crouch_grenade_throw", "reload_stand",
        "pb_stand_grenade_throw_extra", "PB_STAND_GRENADE_THROW", NULL
    };
    static const int flags[] = {0, 4, 8, 12};
    static const int versions[] = {0, 3, 4, 6};
    int animation, stance, user, version, i, cases = 0;

    for (i = 0; i < 6; ++i)
        controller_names[i] = (void *(*)())&tags[i];
    for (animation = 0; animation < 6; ++animation) {
        for (stance = 0; stance < 4; ++stance) {
            for (user = 0; user < 2; ++user) {
                for (version = 0; version < 4; ++version) {
                    int cancel = 0;
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
                    if (user == 1 && versions[version] >= 3) {
                        if (animation == 0)
                            cancel = flags[stance] != 0;
                        else if (animation == 1)
                            cancel = stance == 0 || stance == 2;
                    }
#endif
                    checkThrow(names[animation], flags[stance], user, versions[version], cancel);
                    ++cases;
                }
            }
        }
    }
    /* The CoD2x controller exits for mounted flags before stance correction. */
    checkThrow(names[0], 4 | 0x100, 1, 6, 0);
    checkThrow(names[1], 8 | 0x200, 1, 6, 0);
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    cod2xAnimationState[0][3].stance = 2;
    cod2xAnimationState[1][3].stance = 4;
    Cod2x_ResetClientAnimation();
    assert(cod2xAnimationState[0][3].stance == 0);
    assert(cod2xAnimationState[1][3].stance == 4);
    Cod2x_ResetAnimation();
    assert(cod2xAnimationState[1][3].stance == 0);
#endif
    printf("cod2x: %d production grenade stance cases and mounted controls passed\n", cases);
#ifdef GAMEPLAY_MATRIX
    animation_matrix();
#endif
    return 0;
}
