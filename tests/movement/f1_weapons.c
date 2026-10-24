/* Mac 1.3 PM_Weapon (0x7cbb2) and BG_TakePlayerWeapon (0x7bec4). */
#define main old_weapon_fixture_main
#include "weapons.c"
#undef main

static WeaponDef loaded;
static int loadDefaults;
static cspField_t weaponDefFields[366];
static void BG_InitWeaponDefStrings(WeaponDef *def) { memset(def, 0, sizeof(*def)); }
static void SetConfigString_core(char **dest, const char *value) { *dest = (char *)value; }
void *Hunk_AllocLowInternal(int size) { assert(size == sizeof(loaded)); return &loaded; }
int FS_FOpenFileByMode(const char *name, fileHandle_t *file, int mode)
{ (void)name; assert(mode == 0); *file = 1; return 10; }
void FS_FCloseFile(fileHandle_t file) { assert(file == 1); }
int FS_Read(void *buf, int count, fileHandle_t file)
{ assert(file == 1); if (count == 10) memcpy(buf, "WEAPONFILE", 10); else assert(count == 0); return count; }
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{ (void)fmt; assert(size >= 2); strcpy(dest, "x"); return 1; }
void Com_Printf(const char *fmt, ...) { (void)fmt; }
qboolean Info_Validate(const char *value) { assert(!*value); return 1; }
char *I_strlwr(char *value) { return value; }
qboolean G_ParseWeaponAccurayGraphs(WeaponDef *def) { assert(def == &loaded); return 1; }
static void SetConfigString2(byte *member, const char *value) { (void)member; (void)value; }
static qboolean BG_ParseWeaponDefSpecificFieldType(byte *def, const char *value, int type)
{ (void)def; (void)value; (void)type; return 1; }
int ParseConfigStringToStruct(byte *raw, cspField_t *fields, int count, const char *buffer, int types,
    qboolean (*parse)(byte *, const char *, int), void (*string)(byte *, const char *))
{
    WeaponDef *def = (WeaponDef *)raw;
    assert(fields == weaponDefFields && count == 366 && !*buffer && types == 16);
    (void)parse; (void)string;
    def->iFireTime = 50; def->iRechamberTime = 80;
    def->iAdsTransInTime = loadDefaults ? 0 : 250;
    def->iAdsTransOutTime = loadDefaults ? -1 : 125;
    def->fMaxDamageRange = 0; def->fMinDamageRange = -1;
    def->destabilizationBaseTime = 9.0f; def->destabilizationTimeReductionRatio = 0.5f;
    return 1;
}
#include "f1_load_source.h"

static void reset(playerState_t *ps, pmove_t *pm, pml_t *pml)
{
    memset(ps, 0, sizeof(*ps));
    memset(pm, 0, sizeof(*pm));
    memset(pml, 0, sizeof(*pml));
    memset(&rifle, 0, sizeof(rifle));
    memset(&empty, 0, sizeof(empty));
    bg_iNumWeapons = 1;
    rifle.iClipIndex = rifle.iAmmoIndex = 1;
    rifle.iFireTime = 20;
    rifle.weapSlot = 1;
    ps->weapon = pm->cmd.weapon = 1;
    ps->weaponslots[1] = 1;
    ps->weapons[0] = 2;
    ps->ammoclip[1] = 10;
    pm->ps = ps;
    pml->msec = 10;
    pml->frametime = 0.01f;
    FixtureMantleEnabled(0);
}

int main(void)
{
    playerState_t ps;
    pmove_t pm;
    pml_t pml;
    reset(&ps, &pm, &pml);
    rifle.bSlotStackable = 1;
    assert(BG_TakePlayerWeapon(&ps, 1));
    assert(ps.weaponslots[1] == 0 && ps.weapons[0] == 0 && ps.weapon == 0);

    reset(&ps, &pm, &pml);
    pm.cmd.buttons = 1;
    PM_Weapon(&pm, &pml);
    assert(ps.ammoclip[1] == 9 && ps.weaponTime == 20);
    PM_Weapon(&pm, &pml);
    assert(ps.ammoclip[1] == 9 && ps.weaponTime == 10);
    PM_Weapon(&pm, &pml);
    assert(ps.ammoclip[1] == 8 && ps.weaponTime == 20 && ps.weaponstate == 3);

    reset(&ps, &pm, &pml);
    rifle.bSemiAuto = 1;
    pm.cmd.buttons = 1;
    PM_Weapon(&pm, &pml); PM_Weapon(&pm, &pml); PM_Weapon(&pm, &pml);
    assert(ps.ammoclip[1] == 9 && ps.weaponTime == 1 && ps.weaponstate == 0);
    PM_Weapon(&pm, &pml);
    assert(ps.ammoclip[1] == 9 && ps.weaponTime == 1);
    pm.cmd.buttons = 0; PM_Weapon(&pm, &pml);
    assert(ps.weaponTime == 0);
    pm.cmd.buttons = 1; PM_Weapon(&pm, &pml);
    assert(ps.ammoclip[1] == 8);

    reset(&ps, &pm, &pml);
    ps.ammoclip[1] = ps.ammo[1] = 0;
    pm.cmd.buttons = 1;
    PM_Weapon(&pm, &pml);
    assert(ps.weaponTime == 520 && ps.events[0] == 0x92);

    reset(&ps, &pm, &pml);
    rifle.iDropTime = 17;
    ps.pm_flags = 0x20;
    PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == 2 && ps.weaponTime == 17);

    reset(&ps, &pm, &pml);
    rifle.iReloadTime = 50;
    rifle.iReloadAmmoAdd = 1;
    rifle.bSegmentedReload = 1;
    ps.weaponstate = 5; ps.weaponTime = 30; ps.ammo[1] = 20;
    pm.cmd.buttons = 1;
    PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == 6 && ps.weaponTime == 20);

    reset(&ps, &pm, &pml);
    rifle.iMeleeDamage = 50; rifle.iMeleeTime = 30; rifle.iMeleeDelay = 10;
    ps.weaponstate = 5; ps.weaponTime = 50; ps.weaponDelay = 30;
    pm.cmd.buttons = 4;
    PM_Weapon(&pm, &pml);
    assert(ps.weaponstate == 10 && ps.weaponTime == 30 && ps.weaponDelay == 10);
    assert(ps.aimSpreadScale == 255.0f);

    reset(&ps, &pm, &pml);
    {
        weaponState_t ws = { .ps = &ps, .xyspeed = 100.0f, .frametime = 1.0f };
        vec3_t angles = {0};
        rifle.vStandRot[0] = 10.0f; rifle.vStandMove[0] = 100.0f;
        rifle.fPosRotRate = 1.0f; ps.speed = 200; ps.fWeaponPosFrac = 0.25f;
        BG_AddWeaponMovementAngles(&ws, angles, &rifle);
        assert(ws.vLastMoveAng[0] == 3.75f && angles[0] == 1.875f);
        ws.vGunOffset[0] = 20.0f; angles[0] = 0.0f;
        BG_AddWeaponGunKick(&ws, angles, &rifle);
        assert(angles[0] == 0.0f);
        int idleTime = 0;
        ws.weapIdleTime = &idleTime; ws.fLastIdleFactor = 1.0f;
        ps.eFlags = 8; ps.fWeaponPosFrac = 0.5f;
        rifle.overlayReticle = 1; rifle.fIdleProneFactor = 2.0f;
        BG_AddWeaponIdleAngles(&ws, angles, &rifle);
        assert(ws.fLastIdleFactor == 1.0f);
    }
    reset(&ps, &pm, &pml);
    {
        static dvar_t hold = { .current.value = 1.0f }, gasp = { .current.value = 1.0f };
        static dvar_t scale = { .current.value = 4.5f };
        player_breath_hold_time = &hold;
        player_breath_gasp_time = &gasp;
        player_breath_gasp_scale = &scale;
        ps.fWeaponPosFrac = 0.12345000356435776f;
        ps.holdBreathTimer = 12;
        PM_UpdateHoldBreath(&pm, &pml);
        assert(ps.holdBreathTimer == 2 && ps.holdBreathScale == BM_I2F(0x3f800e28));
        float offset = 1.0f, speed = 0.0f;
        assert(!BG_CalculateWeaponPosition_GunRecoil_SingleAngle(&offset, &speed, 0.5f, 10.0f, 0.0f, 10.0f, 0.0f, -2.0f));
        assert(offset == 1.0f && speed == -1.0f);
#if !defined(F1_BASELINE)
        ps.bobCycle = 63;
        assert(BG_GetBobCycleSingle(&ps) == BM_I2F(0x40fabc6e));
#endif
    }
    for (loadDefaults = 0; loadDefaults <= 1; loadDefaults++) {
        WeaponDef *def = BG_LoadWeaponDefInternal("mp", "fixture");
        assert(def == &loaded);
        assert(def->fOOPosAnimLength[0] == (loadDefaults ? BM_I2F(0x3b5a740e) : 0.004f));
        assert(def->fOOPosAnimLength[1] == (loadDefaults ? BM_I2F(0x3b03126f) : 0.008f));
        assert(def->fMaxDamageRange == BM_I2F(0x497423f0));
        assert(def->fMinDamageRange == BM_I2F(0x497423f2));
        assert(def->destabilizationBaseTime == 9.0f && def->destabilizationTimeReductionRatio == 0.5f);
    }
    puts("PASS F1 weapon inventory, auto/semi-auto cadence, empty fire, ladder, reload interrupt, melee, weapon angles and loader fields");
    return 0;
}
