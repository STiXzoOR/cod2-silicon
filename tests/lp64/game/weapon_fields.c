#include "ws6_weapon_symbols.h"
#include "../../../src/PC/bgame/bg_weapons_load_obj.c"
#include <assert.h>

int main(void)
{
    assert(sizeof(weaponDefFields) / sizeof(weaponDefFields[0]) == 366);
    assert(weaponDefFields[0].iOffset == offsetof(WeaponDef, szDisplayName));
    assert(weaponDefFields[3].iOffset == offsetof(WeaponDef, playerAnimType));
    WeaponDef weapon;
    memset(&weapon, 0x5a, sizeof(weapon));
    BG_InitWeaponDefStrings(&weapon);
    assert(weapon.szInternalName && !weapon.szInternalName[0]);
    assert(weapon.szDisplayName && !weapon.szDisplayName[0]);
    assert(weapon.szXAnims[22] && !weapon.szXAnims[22][0]);
    assert(weapon.meleeImpactRumble && !weapon.meleeImpactRumble[0]);
    assert(weapon.playerAnimType == 0x5a5a5a5a);
    puts("weapons: native field offsets and string initialization pass");
}
