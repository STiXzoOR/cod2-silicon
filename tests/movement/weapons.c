#include "weapons_source.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static dvar_t zero;
void FixtureMantleEnabled(int enabled);
const dvar_t *player_scopeExitOnDamage = &zero;
const dvar_t *player_adsExitDelay = &zero;
const dvar_t *player_breath_hold_time = &zero;
const dvar_t *player_breath_gasp_time = &zero;
const dvar_t *player_breath_hold_lerp = &zero;
const dvar_t *player_breath_gasp_scale = &zero;
const dvar_t *player_breath_gasp_lerp = &zero;
#if defined(COD2_X64)
const dvar_t *player_breath_fire_delay = &zero;
#endif
const dvar_t *player_toggleBinoculars = &zero;
int bg_iNumWeapons = 1;
static WeaponDef empty, rifle;
WeaponDef *bg_weaponDefs[128] = { &empty, &rifle };

#ifndef GAMEPLAY_MATRIX
void PM_AddEvent(playerState_t *ps, int event)
{
    ps->events[ps->eventSequence++ & 3] = event;
}
#endif
void PM_SetProneMovementOverride(playerState_t *ps) { (void)ps; }
int BG_AnimScriptEvent(playerState_t *ps, scriptAnimEventTypes_t event, qboolean cont, qboolean force)
{ (void)ps; (void)event; (void)cont; (void)force; return 0; }
void BG_UpdateConditionValue(int client, int condition, int value, qboolean convert)
{ (void)client; (void)condition; (void)value; (void)convert; }
#ifndef GAMEPLAY_MATRIX
void BG_AddPredictableEventToPlayerstate(int event, int parm, playerState_t *ps)
{ (void)parm; PM_AddEvent(ps, event); }
#endif
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
float DiffTrack(float target, float current, float rate, float dt)
{ (void)current; (void)rate; (void)dt; return target; }
#ifdef GAMEPLAY_MATRIX
void Fixture_SetRevision(int revision);
void Fixture_VerifyHostZero(void);
static pmoveHandler_t handlers[2];
void *imp_pmoveHandlers = handlers;
static int received[32], receivedParms[32], receivedCount;
void CG_CalcEntityLerpPositions(centity_t *cent) { (void)cent; }
void CG_EntityEvent(centity_t *cent, int event)
{
    assert(receivedCount < 32);
    received[receivedCount] = event;
    receivedParms[receivedCount++] = cent->nextState.eventParm;
}
int PM_GetViewHeightLerpTime(const playerState_t *ps, int current, int target)
{ (void)ps; (void)current; (void)target; assert(0); return 0; }
extern const float AngleNormalize180(const float angle);
#include "weapon_events_source.h"
#elif defined(COD2_CODX) && COD2_CODX
int Cod2x_GameVersion(void) { return 0; }
#endif

#ifdef GAMEPLAY_MATRIX
static void grenade_events(void)
{
    static WeaponDef frag, smoke;
    frag.weapType = smoke.weapType = WEAPTYPE_GRENADE;
    frag.offhandClass = OFFHAND_CLASS_FRAG_GRENADE;
    smoke.offhandClass = OFFHAND_CLASS_SMOKE_GRENADE;
    frag.iClipIndex = frag.iAmmoIndex = 2;
    smoke.iClipIndex = smoke.iAmmoIndex = 3;
    frag.iHoldFireTime = smoke.iHoldFireTime = 10;
    frag.iFireTime = smoke.iFireTime = 20;
    frag.iFuseTime = smoke.iFuseTime = 1000;
    bg_weaponDefs[2] = &frag; bg_weaponDefs[3] = &smoke; bg_iNumWeapons = 3;
    rifle.iFireTime = 10; rifle.quickDropTime = 10;
    int suppressed = 0;
#if defined(COD2_CODX) && COD2_CODX
    suppressed = Cod2x_GameVersion() != 0;
#endif
    for (int handler = 0; handler < 2; ++handler) {
        for (int index = 2; index <= 3; ++index) {
            int button = index == 2 ? PM_WEAPON_BUTTON_FRAG : PM_WEAPON_BUTTON_SMOKE;
            for (int owned = 0; owned < 2; ++owned) {
                playerState_t ps = {0};
                pmove_t pm = {.ps = &ps, .handler = handler};
                pml_t pml = {.msec = 10, .frametime = 0.01f};
                entityState_t entity = {0};
                int other = index == 2 ? 3 : 2;
                ps.weapon = pm.cmd.weapon = 1;
                ps.weapons[0] = 2 | (1 << other) | (owned ? 1 << index : 0);
                ps.ammoclip[other] = 1;
                ps.viewHeightTarget = 60; ps.ammoclip[1] = 10;
                pm.cmd.buttons = button | PM_WEAPON_BUTTON_ATTACK;
                PM_Weapon(&pm, &pml);
                assert(ps.weaponstate == WEAPON_FIRING && ps.ammoclip[1] == 9);
                assert(ps.eventSequence == (suppressed ? 1 : 2));
                assert(ps.events[(ps.eventSequence - 1) & 3] == 0x9e);
                BG_PlayerStateToEntityState(&ps, &entity, 0, handler);
                pm.cmd.buttons = button;
#if defined(COD2_X64)
                /* Mac 0x7cf79 checks offhand before finishing firing: all
                   nine frames attempt the unavailable grenade. */
#else
                /* First frame finishes the firing state; the following eight
                   frames attempt the unavailable offhand while ready. */
#endif
                for (int frame = 0; frame < 9; ++frame) {
                    PM_Weapon(&pm, &pml);
                    BG_PlayerStateToEntityState(&ps, &entity, 0, handler);
                }
                assert(ps.ammoclip[1] == 9);
#if defined(COD2_X64)
                assert(ps.eventSequence == (suppressed ? 1 : 11));
#else
                assert(ps.eventSequence == (suppressed ? 1 : 10));
#endif
                centity_t receiver = {.nextState = entity};
                receivedCount = 0; CG_CheckEvents(&receiver);
                assert(receivedCount == (suppressed ? 1 : 4));
                for (int i = 0; i < receivedCount; ++i) {
                    assert(received[i] == (suppressed ? 0x9e : 0x94));
                    assert(receivedParms[i] == 0);
                }
                CG_CheckEvents(&receiver); assert(receivedCount == (suppressed ? 1 : 4));
            }

            /* The actual offhand init/prepare/hold/fire path consumes the final
               grenade and legitimately publishes EV_EMPTY_OFFHAND once. */
            playerState_t ps = {0};
            pmove_t pm = {.ps = &ps, .handler = handler};
            pml_t pml = {.msec = 10, .frametime = 0.01f};
            ps.weapon = pm.cmd.weapon = 1; ps.weapons[0] = 2 | (1 << index);
            ps.ammoclip[index] = 1; pm.cmd.buttons = button;
            PM_Weapon(&pm, &pml);
            assert(ps.offHandIndex == index && ps.weaponstate == WEAPON_OFFHAND_INIT);
            assert(ps.eventSequence == 2 && ps.events[0] == 0xa7 && ps.eventParms[0] == index && ps.events[1] == 0x95);
            PM_Weapon(&pm, &pml); assert(ps.weaponstate == WEAPON_OFFHAND_PREPARE);
            PM_Weapon(&pm, &pml); assert(ps.weaponstate == WEAPON_OFFHAND_HOLD);
            pm.cmd.buttons = 0; PM_Weapon(&pm, &pml);
            assert(ps.weaponstate == WEAPON_OFFHAND_FIRE && ps.ammoclip[index] == 0);
            assert(ps.eventSequence == 5 && ps.events[3] == 0xa6 && ps.eventParms[3] == index && ps.events[0] == 0x94);
            entityState_t entity = {0};
            BG_PlayerStateToEntityState(&ps, &entity, 0, handler);
            centity_t receiver = {.nextState = entity}; receivedCount = 0;
            CG_CheckEvents(&receiver);
            assert(receivedCount == 4 && received[2] == 0xa6 && receivedParms[2] == index && received[3] == 0x94);
            PM_Weapon(&pm, &pml); PM_Weapon(&pm, &pml);
            assert(ps.weaponstate == WEAPON_OFFHAND_END);

            /* The same real offhand path with a second grenade remaining must
               emit the throw without a spurious empty inventory event. */
            ps = (playerState_t){0}; ps.weapon = pm.cmd.weapon = 1;
            ps.weapons[0] = 2 | (1 << index); ps.ammoclip[index] = 2;
            pm.cmd.buttons = button;
            PM_Weapon(&pm, &pml); PM_Weapon(&pm, &pml); PM_Weapon(&pm, &pml);
            pm.cmd.buttons = 0; PM_Weapon(&pm, &pml);
            assert(ps.weaponstate == WEAPON_OFFHAND_FIRE && ps.ammoclip[index] == 1);
            assert(ps.eventSequence == 4 && ps.events[3] == 0xa6 && ps.eventParms[3] == index);
        }
    }
    puts("PASS unavailable smoke/frag event ring, receiving client, and legitimate final grenade for both handlers");
}
#endif

int main(int argc, char **argv)
{
#ifdef GAMEPLAY_MATRIX
    Fixture_SetRevision(argc == 2 ? atoi(argv[1]) : 0);
#else
    (void)argc; (void)argv;
#endif
    FixtureMantleEnabled(1);
    rifle.iDropTime = 20;
    rifle.iRaiseTime = 30;
    rifle.iFireTime = 100;
    rifle.iClipIndex = 1;
    rifle.iAmmoIndex = 1;
    empty.iRaiseTime = 15;
    playerState_t ps = {0};
    pmove_t pm = { .ps = &ps };
    pml_t pml = { .msec = 10, .frametime = 0.01f };
    ps.weapon = pm.cmd.weapon = 1;
    ps.weapons[0] = 2;
#if defined(COD2_X64)
    rifle.weapSlot = 1;
    ps.weaponslots[1] = 1;
#endif
    ps.ammoclip[1] = 10;
    ps.pm_flags = 4;
    pm.cmd.buttons = 1;
    /* Mac 1.3 0x7d0ce/0x7d456: high mantle requests a change to zero. */
    PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == WEAPON_DROPPING && ps.weaponTime == 20);
    assert(ps.ammoclip[1] == 10);
    PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == WEAPON_DROPPING && ps.weaponTime == 10);
    PM_Weapon(&pm, &pml);
    /* 0x7d842/0x7d877: finishing the drop cannot reselect the held command. */
    assert(ps.weapon == 0 && ps.weaponstate == WEAPON_RAISING && ps.weaponTime == 15);
    assert(ps.events[(ps.eventSequence - 1) & 3] == 0x9a);
    PM_Weapon(&pm, &pml);
    assert(ps.weapon == 0 && ps.ammoclip[1] == 10 && ps.weaponTime == 5);
    PM_Weapon(&pm, &pml);
    assert(ps.weapon == 0 && ps.weaponstate == WEAPON_READY);
    ps.pm_flags &= ~4;
    PM_Weapon(&pm, &pml);
    assert(ps.weapon == 1 && ps.weaponstate == WEAPON_RAISING && ps.weaponTime == 30);
    for (int i = 0; i < 3; ++i) PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == WEAPON_READY && ps.ammoclip[1] == 10);
    PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == WEAPON_FIRING && ps.ammoclip[1] == 9);

    /* Actual mantle table has five weapon-inactive transitions and two low
       transitions that keep the weapon usable (overAnimIndex == 10). */
    for (int enable = 0; enable <= 1; ++enable) {
        FixtureMantleEnabled(enable);
        for (int flag = 0; flag <= 1; ++flag) {
            for (int transition = 0; transition < 7; ++transition) {
                ps = (playerState_t){0};
                ps.weapon = 1;
                ps.weapons[0] = 2;
#if defined(COD2_X64)
                ps.weaponslots[1] = 1;
#endif
                ps.ammoclip[1] = 10;
                ps.pm_flags = flag ? 4 : 0;
                ps.mantleState.transIndex = transition;
                PM_Weapon(&pm, &pml);
                int inactive = enable && flag && transition < 5;
                assert(ps.weaponstate == (inactive ? WEAPON_DROPPING : WEAPON_FIRING));
                assert(ps.ammoclip[1] == (inactive ? 10 : 9));
            }
        }
    }

    FixtureMantleEnabled(1);
    /* Independently read Mac 0x7d077..0x7d0ce exclusions. Reload states and
       rechamber may be interrupted; active firing/melee finish their time.
       Offhand/binocular states never enter this change path. */
    for (int state = 0; state <= 22; ++state) {
        for (int time = 0; time <= 20; time += 20) {
            for (int delay = 0; delay <= 10; delay += 10) {
                ps = (playerState_t){0};
                ps.weapon = 1;
                ps.weapons[0] = 2;
                ps.pm_flags = 4;
                ps.weaponstate = state;
                ps.weaponTime = time;
                ps.weaponDelay = delay;
                PM_CheckWeaponChange(&pm);
                int excluded = state >= 12 || state == 2;
                int held = time && (state < 4 || state > 9) &&
                           (state == 3 || state == 10 || state == 11 || delay);
                if (excluded || held) {
                    assert(ps.weaponstate == state);
                    assert(ps.weaponTime == time && ps.weaponDelay == delay);
                } else {
                    assert(ps.weaponstate == WEAPON_DROPPING);
                    assert(ps.weaponTime == 20 && ps.weaponDelay == 0);
                }
            }
        }
    }
    puts("PASS mantle weapon sequence, 28 enable/flag/transition cases and 92 timing/state cases");
#ifdef GAMEPLAY_MATRIX
    grenade_events();
    Fixture_VerifyHostZero();
#endif
}
