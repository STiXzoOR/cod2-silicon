#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static WeaponDef weapon;
WeaponDef *bg_weaponDefs[128] = { &weapon };
int BG_GetViewmodelWeaponIndex(const playerState_t *ps) { (void)ps; return 0; }
void AnglesSubtract(const vec_t *a, const vec_t *b, vec_t *out);
float DiffTrack(float target, float current, float rate, float dt);
float DiffTrackAngle(float target, float current, float rate, float dt, float f, float granularity, float epsilon);
#include "sway_source.h"

int main(void)
{
    weapon.swayMaxAngle = weapon.adsSwayMaxAngle = 100;
    weapon.swayLerpSpeed = weapon.adsSwayLerpSpeed = 100;
    weapon.swayHorizScale = 2; weapon.swayVertScale = 3;
    weapon.adsSwayHorizScale = 4; weapon.adsSwayVertScale = 5;
    weapon.bADSPositionInfo = 1;
    for (int ads = 0; ads <= 2; ++ads) {
        playerState_t ps = {0};
        vec3_t view = {0}, offset = {0}, angles = {0};
        ps.fWeaponPosFrac = ads * 0.5f;
        ps.viewangles[0] = 2; ps.viewangles[1] = 1;
        BG_CalculateWeaponPosition_Sway(&ps, view, offset, angles, 1, 10);
        /* Mac 1.3 0x7b822/0x7b851: pitch uses vertical; yaw horizontal. */
        float dt = 10 * 0.0010000000474974513f;
        float factor = 1 / (dt * 60);
        assert(fabsf(offset[1] - factor * (2 + ads)) < 0.00001f);
        assert(fabsf(offset[2] - 2 * factor * (3 + ads)) < 0.00001f);
        assert(view[0] == 2 && view[1] == 1);
    }
    puts("PASS hip, interpolated ADS and full ADS viewmodel sway axes");
    return 0;
}
