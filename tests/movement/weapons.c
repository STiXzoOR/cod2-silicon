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
const dvar_t *player_toggleBinoculars = &zero;
int bg_iNumWeapons = 1;
static WeaponDef empty, rifle;
WeaponDef *bg_weaponDefs[128] = { &empty, &rifle };

void PM_AddEvent(playerState_t *ps, int event)
{
    ps->events[ps->eventSequence++ & 3] = event;
}
void PM_SetProneMovementOverride(playerState_t *ps) { (void)ps; }
int BG_AnimScriptEvent(playerState_t *ps, scriptAnimEventTypes_t event, qboolean cont, qboolean force)
{ (void)ps; (void)event; (void)cont; (void)force; return 0; }
void BG_UpdateConditionValue(int client, int condition, int value, qboolean convert)
{ (void)client; (void)condition; (void)value; (void)convert; }
void BG_AddPredictableEventToPlayerstate(int event, int parm, playerState_t *ps)
{ (void)parm; PM_AddEvent(ps, event); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
float DiffTrack(float target, float current, float rate, float dt)
{ (void)current; (void)rate; (void)dt; return target; }
#if defined(COD2_CODX) && COD2_CODX
int Cod2x_GameVersion(void) { return 0; }
#endif

int main(void)
{
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
}
