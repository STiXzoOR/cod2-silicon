#include "common_types.h"
#include <assert.h>
#include <string.h>
static gitem_t items[132];
static int count = 129;
static void *imp_bg_itemlist = items;
static void *imp_bg_numItems = &count;
static int I_stricmp(const char *a, const char *b) { return strcmp(a, b); }
static int G_GetWeaponIndexForName(const char *name) { return !strcmp(name, "enfield_mp") ? 1 : 0; }
#include "find_item.h"

int main(void) {
 assert(G_FindItem("enfield_mp") == &items[1]);
 assert(G_FindItem("missing") == 0);
 items[129].pickup_name = "health"; items[129].classname = "item_health";
 items[130].pickup_name = "ammo"; items[130].classname = "item_ammo";
 count = 131;
 assert(G_FindItem("health") == &items[129]);
 assert(G_FindItem("item_ammo") == &items[130]);
}
