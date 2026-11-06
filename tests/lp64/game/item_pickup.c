/* Observable pickup contracts through the complete production item TU. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Touch_Item(gentity_t *, gentity_t *, qboolean);
extern int Add_Ammo(gentity_t *, int, int, qboolean);
gentity_t g_entities[1024];
gitem_t bg_itemlist[128];
qboolean itemRegistered[256];
level_locals_t level;
scr_const_t scr_const;
scr_data_t g_scr_data;
void *imp_g_entities = g_entities;
void *imp_bg_itemlist = bg_itemlist;
static int itemCount = 128;
void *imp_bg_numItems = &itemCount;
static gclient_t pickupClient;
static WeaponDef weapons[128];
static dvar_t pools, dropMax, dropSpeed, dropUp, dropRand;
const dvar_t *g_weaponAmmoPools = &pools, *g_maxDroppedWeapons = &dropMax;
const dvar_t *g_dropForwardSpeed = &dropSpeed, *g_dropUpSpeedBase = &dropUp;
const dvar_t *g_dropUpSpeedRand = &dropRand;
static int ammoMaximum, clipMaximum, canGrab, needSlot, emptySlot;
static int freed, selects, gives, takes, events, notifies, commandCount, scriptArgs;
static char commands[8][160];

int BG_GetNumWeapons(void) { return 127; }
WeaponDef *BG_GetWeaponDef(int i) { assert(i >= 0 && i < 128); return &weapons[i]; }
int BG_AmmoForWeapon(int w) { return w; }
int BG_ClipForWeapon(int w) { return w; }
qboolean BG_WeaponIsClipOnly(int w) { return weapons[w].bClipOnly; }
int BG_GetAmmoTypeMax(int i) { return ammoMaximum; }
int BG_GetAmmoClipSize(int i) { return clipMaximum; }
int BG_GetMaxPickupableAmmo(const playerState_t *ps, int w) { return 0; }
qboolean G_GivePlayerWeapon(playerState_t *ps, int w)
{ ++gives; ps->weapons[w >> 5] |= 1u << (w & 31); return 1; }
qboolean BG_TakePlayerWeapon(playerState_t *ps, int w)
{ ++takes; ps->weapons[w >> 5] &= ~(1u << (w & 31)); return 1; }
Bool BG_DoesWeaponNeedSlot(int w) { return needSlot; }
int BG_IsPlayerWeaponInSlot(const playerState_t *ps, int w, qboolean any) { return 1; }
int BG_GetEmptySlotForWeapon(const playerState_t *ps, int w) { return emptySlot; }
int BG_GetStackSlotForWeapon(const playerState_t *ps, int w, weapSlot_t slot) { return 0; }
qboolean BG_CanItemBeGrabbed(const entityState_t *s, const playerState_t *ps, qboolean touched) { return canGrab; }
void G_SelectWeaponIndex(int clientNum, int w) { assert(clientNum == 0); ++selects; }
void I_strncpyz(char *d, const char *s, int n) { snprintf(d, n, "%s", s); }
char *I_CleanStr(char *s) { return s; }
int SV_GetGuid(int n) { return 123; }
void G_LogPrintf(const char *f, ...) { }
void Com_Printf(const char *f, ...) { }
char *va(const char *f, ...)
{ static char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof(b), f, a); va_end(a); return b; }
void SV_GameSendServerCommand(int n, svscmd_type type, const char *s)
{ assert(n == 0 && type == SV_CMD_CAN_IGNORE && commandCount < 8); snprintf(commands[commandCount++], 160, "%s", s); }
void Scr_AddEntity(gentity_t *e) { assert(e == &g_entities[0] || e == &g_entities[2]); ++scriptArgs; }
void Scr_AddUndefined(void) { ++scriptArgs; }
void Scr_Notify(gentity_t *e, unsigned short s, unsigned int n)
{ assert(e == &g_entities[1] && s == 42 && n == scriptArgs); ++notifies; scriptArgs = 0; }
void G_AddPredictableEvent(gentity_t *e, int event, int param)
{ assert(e == &g_entities[0] && (event == 0x90 || event == 0x91)); ++events; }
void G_AddEvent(gentity_t *e, int event, int param) { G_AddPredictableEvent(e, event, param); }
void G_FreeEntity(gentity_t *e) { assert(e == &g_entities[1]); ++freed; }
gentity_t *G_Spawn(void) { g_entities[2].tagInfo = 77; g_entities[2].parent = 88; return &g_entities[2]; }
int rand(void) { return 0; }
void G_SetOrigin(gentity_t *e, const vec_t *p) { memcpy(e->r.currentOrigin, p, sizeof(vec3_t)); }
void G_SetAngle(gentity_t *e, const vec_t *p) { memcpy(e->r.currentAngles, p, sizeof(vec3_t)); }
void SV_LinkEntity(gentity_t *e) { }
void G_SetModel(gentity_t *e, const char *s) { }
void G_DObjUpdate(gentity_t *e) { }
void G_SetConstString(scr_string_t *s, const char *v) { *s = 1; }
int Com_sprintf(char *d, int n, const char *f, ...)
{ va_list a; va_start(a, f); int r = vsnprintf(d, n, f, a); va_end(a); return r; }
void I_strncat(char *d, int n, const char *s) { strncat(d, s, n - strlen(d) - 1); }
void SV_SetConfigstring(int i, const char *s) { }
int G_ModelIndex(const char *s) { return 1; }
int G_GetHintStringIndex(int *i, const char *s) { *i = 1; return 1; }
qboolean G_XModelBad(int i) { return 0; }
void G_OverrideModel(int i, const char *s) { }
void Scr_Error(const char *s) { abort(); }
void Com_Error(int code, const char *f, ...) { abort(); }
qboolean G_DObjGetWorldTagMatrix(gentity_t *e, unsigned int tag, vec3_t *matrix) { abort(); }
void G_TraceCapsule(trace_t *t, const vec_t *a, const vec_t *mi, const vec_t *ma, const vec_t *b, int pass, int mask) { abort(); }

static void Reset(itemType_t type, int weapon)
{
    memset(g_entities, 0, sizeof(g_entities)); memset(bg_itemlist, 0, sizeof(bg_itemlist));
    memset(&pickupClient, 0, sizeof(pickupClient)); memset(weapons, 0, sizeof(weapons));
    memset(&level, 0, sizeof(level)); memset(&pools, 0, sizeof(pools));
    for (int i = 0; i < 128; ++i) {
        weapons[i].iSharedAmmoCapIndex = -1;
        weapons[i].szDisplayName = "weapon"; weapons[i].szInternalName = "weapon";
        weapons[i].weapSlot = WEAPSLOT_PRIMARY;
        bg_itemlist[i].classname = "item"; bg_itemlist[i].world_model[0] = "model";
        bg_itemlist[i].giType = IT_WEAPON; bg_itemlist[i].giTag = i;
        itemRegistered[i] = 1;
    }
    g_entities[0].client = &pickupClient; g_entities[0].health = 50;
    pickupClient.ps.stats[2] = 100; strcpy(pickupClient.sess.cs.name, "player");
    g_entities[1].item.index = weapon; g_entities[1].s.index.item = weapon;
    g_entities[1].s.clientNum = 1022; g_entities[1].active = 1;
    bg_itemlist[weapon].giType = type; bg_itemlist[weapon].quantity = 5;
    scr_const.trigger = 42;
    ammoMaximum = 100; clipMaximum = 10; canGrab = 1; needSlot = 0; emptySlot = 1;
    dropMax.current.integer = 1;
    freed = selects = gives = takes = events = notifies = commandCount = scriptArgs = 0;
}
static void Pickup(int touched) { Touch_Item(&g_entities[1], &g_entities[0], touched); }
static void Health(int overheal)
{
    Reset(IT_HEALTH, 2); bg_itemlist[2].giTag = 0;
    bg_itemlist[2].quantity = overheal ? 5 : 25;
    g_entities[0].health = overheal ? 150 : 50;
    Pickup(1);
    assert(g_entities[0].health == (overheal ? 155 : 75));
    assert(pickupClient.ps.stats[0] == g_entities[0].health && freed == 1);
}
static void Offhand(int type)
{
    Reset(type, 2); weapons[2].offhandClass = (OffhandClass)1;
    weapons[2].bSlotStackable = 0; g_entities[1].count = 5;
    g_entities[1].item.count2 = g_entities[1].chain = -1;
    Pickup(1); assert(commandCount > 0);
    assert(!strcmp(commands[commandCount - 1], "I \"4\""));
}
static void Pool(int enabled)
{
    Reset(IT_WEAPON, 2); pools.current.enabled = enabled;
    pickupClient.ps.weapons[0] = 1u << 2; pickupClient.ps.ammo[2] = 95;
    g_entities[1].count = 20; g_entities[1].item.count2 = g_entities[1].chain = -1;
    Pickup(1);
    assert(pickupClient.ps.ammo[2] == 100 && g_entities[1].count == 15);
    assert(freed == !enabled && notifies == !enabled && events == !enabled);
}
static void ClipField(void)
{
    Reset(IT_WEAPON, 2); g_entities[1].count = 3;
    g_entities[1].item.count2 = 7; g_entities[1].chain = 2;
    Pickup(1);
    assert(pickupClient.ps.ammo[2] == 3 && pickupClient.ps.ammoclip[2] == 7);
    assert(g_entities[1].chain == 2 && freed == 1);
}
static void ClipCap(void)
{
    Reset(IT_WEAPON, 2); pickupClient.ps.weapons[0] = 1u << 2;
    g_entities[1].count = 4; g_entities[1].item.count2 = g_entities[1].chain = 20;
    Pickup(1);
    assert(pickupClient.ps.ammo[2] == 14);
    assert(g_entities[1].item.count2 == 10 && g_entities[1].chain == 20);
}
static void NoSlotSelection(void)
{
    Reset(IT_WEAPON, 2); g_entities[1].count = 5;
    g_entities[1].item.count2 = g_entities[1].chain = -1;
    Pickup(0); assert(gives == 1 && selects == 0 && freed == 1);
}
static void RandomClamp(void)
{
    Reset(IT_WEAPON, 2); pickupClient.ps.weapons[0] = 1u << 2;
    weapons[2].iDropAmmoMin = -3; weapons[2].iDropAmmoMax = 2;
    g_entities[1].item.count2 = g_entities[1].chain = -1;
    Pickup(1); assert(pickupClient.ps.ammo[2] == 0 && g_entities[1].count == 0);
}
static void Bit31(int denied)
{
    Reset(IT_WEAPON, 31); pickupClient.ps.weapons[0] = 1u << 31;
    g_entities[1].count = 5; g_entities[1].item.count2 = g_entities[1].chain = -1;
    canGrab = !denied; Pickup(denied ? 0 : 1);
    assert(freed == !denied);
}
static void DropFields(void)
{
    Reset(IT_WEAPON, 2); needSlot = 1; emptySlot = 0;
    pickupClient.ps.weapon = 1; pickupClient.ps.weapons[0] = 1u << 1;
    pickupClient.ps.ammo[1] = 5; pickupClient.ps.ammoclip[1] = 2;
    g_entities[1].count = 3; g_entities[1].item.count2 = g_entities[1].chain = -1;
    g_entities[1].spawnflags = 7; g_entities[1].s.groundEntityNum = 12;
    g_entities[1].tagInfo = 123; g_entities[1].parent = 456;
    Pickup(1);
    assert(g_entities[2].spawnflags == 7 && g_entities[2].s.groundEntityNum == 12);
    assert(g_entities[2].tagInfo == 77 && g_entities[2].parent == 88);
}
static void DropFailure(void)
{
    Reset(IT_WEAPON, 2); needSlot = 1; emptySlot = 0;
    pickupClient.ps.weapon = 1; pickupClient.ps.weapons[0] = 1u << 1;
    pickupClient.ps.weaponslots[1] = 3; weapons[2].weapSlot = WEAPSLOT_PRIMARYB;
    g_entities[1].count = 3; g_entities[1].item.count2 = g_entities[1].chain = -1;
    Pickup(1); assert(commandCount == 0 && freed == 0 && gives == 0);
}
static void AmmoControls(void)
{
    Reset(IT_AMMO, 2); pickupClient.ps.ammo[2] = 5; pickupClient.ps.ammoclip[2] = 4;
    assert(Add_Ammo(&g_entities[0], 2, 3, 1) == 3);
    assert(pickupClient.ps.ammo[2] == 2 && pickupClient.ps.ammoclip[2] == 10);
    assert(Add_Ammo(&g_entities[0], 2, 200, 0) == 98);
    assert(pickupClient.ps.ammo[2] == 100);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "health-quantity")) Health(0);
    else if (!strcmp(argv[1], "health-overheal")) Health(1);
    else if (!strcmp(argv[1], "ammo-offhand")) Offhand(IT_AMMO);
    else if (!strcmp(argv[1], "weapon-offhand")) Offhand(IT_WEAPON);
    else if (!strcmp(argv[1], "pool-on")) Pool(1);
    else if (!strcmp(argv[1], "pool-off")) Pool(0);
    else if (!strcmp(argv[1], "clip-field")) ClipField();
    else if (!strcmp(argv[1], "clip-cap")) ClipCap();
    else if (!strcmp(argv[1], "no-slot-select")) NoSlotSelection();
    else if (!strcmp(argv[1], "random-clamp")) RandomClamp();
    else if (!strcmp(argv[1], "bit31")) Bit31(0);
    else if (!strcmp(argv[1], "denied-bit31")) Bit31(1);
    else if (!strcmp(argv[1], "drop-fields")) DropFields();
    else if (!strcmp(argv[1], "drop-failure")) DropFailure();
    else if (!strcmp(argv[1], "ammo-controls")) AmmoControls();
    else abort();
    printf("item pickup %s: pass\n", argv[1]); return 0;
}
