#ifndef G_COD2X_PLAYERINFO_H
#define G_COD2X_PLAYERINFO_H

#include "common_types.h"

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX

extern int BG_GetNumWeapons(void);
extern WeaponDef *BG_GetWeaponDef(int weapon);

/* Revision 7 stores spectator information in existing player netfields.
 * surfType is an unsigned health byte. dmgFlags has the 0xC1 format byte,
 * two 7-bit weapon indices and two 4-bit grenade counts; bits 22..23 are zero.
 * These fields also survive archived snapshots and demo recording.
 */
static void G_Cod2xPlayerInfoFrame(gentity_t *entities)
{
    enum { PLAYER_ENTITY_TYPE = 1 };
    for (int clientNum = 0; clientNum < 64; ++clientNum) {
        gentity_t *ent = &entities[clientNum];
        if (!ent->r.inuse || !ent->client)
            continue;

        ent->s.surfType = 0;
        ent->s.dmgFlags = 0;
        if (ent->s.eType != PLAYER_ENTITY_TYPE || ent->health <= 0 ||
            ent->client->sess.sessionState != SESS_STATE_PLAYING)
            continue;

        const playerState_t *ps = &ent->client->ps;
        int grenades[2] = { 0, 0 };
        const int weaponCount = BG_GetNumWeapons();
        for (int weapon = 1; weapon <= weaponCount &&
             weapon < (int)(sizeof(ps->weapons) * 8); ++weapon) {
            if (!((unsigned int)ps->weapons[weapon / 32] & (1u << (weapon % 32))))
                continue;
            const WeaponDef *def = BG_GetWeaponDef(weapon);
            if (!def || def->offhandClass < OFFHAND_CLASS_FRAG_GRENADE ||
                def->offhandClass > OFFHAND_CLASS_SMOKE_GRENADE)
                continue;
            grenades[def->offhandClass - OFFHAND_CLASS_FRAG_GRENADE] +=
                ps->ammo[def->iAmmoIndex] + ps->ammoclip[def->iClipIndex];
        }
        for (int type = 0; type < 2; ++type) {
            if (grenades[type] < 0)
                grenades[type] = 0;
            else if (grenades[type] > 15)
                grenades[type] = 15;
        }

        const unsigned int loadout =
            ((unsigned char)ps->weaponslots[1] & 127u) |
            (((unsigned char)ps->weaponslots[2] & 127u) << 7) |
            ((unsigned int)grenades[0] << 14) |
            ((unsigned int)grenades[1] << 18);
        ent->s.surfType = ent->health > 255 ? 255 : ent->health;
        ent->s.dmgFlags = (int)(0xC1000000u | loadout);
    }
}

#endif
#endif
