#ifndef G_COD2X_WALLBANG_H
#define G_COD2X_WALLBANG_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* Rifle bullets through walls, as CoD2x's sv_wallbang does it (g_cod2x_wallbang.c). */

/* Bullet_Fire_Extended, which the penetration continues behind a wall. */
typedef void (*G_Cod2xBulletFire)(const gentity_t *source, gentity_s (*attacker)[16], vec_t *start, vec_t *end,
                                  float spread, int recursion, const weaponParms *wp, const gentity_t *weaponEnt,
                                  const int gametime);

/* The damage flag of a bullet that went through a wall (iDFlags & 64 in script). */
#define G_COD2X_DAMAGE_WALLBANG 0x40

void G_Cod2xWallbangInit(void);
/* Fires a new bullet (recursion 0) and sends the shooter the debug lines. */
void G_Cod2xWallbangFire(G_Cod2xBulletFire fire, const gentity_t *source, gentity_s (*attacker)[16], vec_t *start,
                         vec_t *end, float spread, const weaponParms *wp, const gentity_t *weaponEnt, int gametime);
/* Bullet_Fire_Extended hooks. */
void G_Cod2xWallbangStart(const vec_t *start);
int G_Cod2xWallbangWalls(void);
void G_Cod2xWallbangBehind(const vec_t *start, const vec_t *hitPos);
void G_Cod2xWallbangPenetrate(G_Cod2xBulletFire fire, const gentity_t *source, gentity_t *attacker,
                              const vec_t *start, const vec_t *end, const vec_t *hitPos, const trace_t *tr,
                              float spread, int recursion, const weaponParms *wp, const gentity_t *weaponEnt,
                              int gametime);
/* Damage behind a wall: range from where the bullet was fired, lowered by the power left. */
int G_Cod2xWallbangDamage(const weaponParms *wp, const vec_t *hitPos, float spread);
void G_Cod2xWallbangHit(gentity_t *attacker, const gentity_t *target, const trace_t *tr, const vec_t *hitPos,
                        int damage, int healthBefore);
#if !defined(DEDICATED)
/* Draws the damage labels again each server frame until they expire (listen server). */
void G_Cod2xWallbangLabelsFrame(void);
#endif
#endif

#endif
