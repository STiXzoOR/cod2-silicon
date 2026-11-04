/* Exercise Mac entity spawn and script field behavior through production g_spawn_mp entry points. */
#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

extern void G_CallSpawn(void);
extern qboolean Scr_SetObjectField(unsigned int classnum, int entnum, int offset);
extern void Scr_GetObjectField(unsigned int classnum, int entnum, int offset);
extern void *imp_g_entities, *imp_bg_numItems;

level_locals_t level;
gentity_t g_entities[8];
scr_const_t scr_const;
gclient_t clients[1];

static int setModels, undefinedPushes, clientGets, clientSets, lastClientOffset;
static const char *lastModel;

int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
qboolean G_SpawnStringInternal(SpawnVar *var, const char *key, const char *def, const char **out)
{
    *out = !strcmp(key, "classname") ? "unknown_entity" : def;
    return 1;
}
gentity_t *G_Spawn(void) { return &g_entities[3]; }
void G_SetOrigin(gentity_t *ent, const vec_t *origin) {}
void G_SetAngle(gentity_t *ent, const vec_t *angle) {}
void G_SetModel(gentity_t *ent, const char *name) { ++setModels; lastModel = name; }
void Scr_AddUndefined(void) { ++undefinedPushes; }
void Scr_GetClientField(gclient_t *client, int offset) { ++clientGets; lastClientOffset = offset; }
void Scr_SetClientField(gclient_t *client, int offset) { ++clientSets; lastClientOffset = offset; }

static gentity_t *spawn_with_model(const char *value)
{
    static char key[] = "model";
    memset(&g_entities[3], 0, sizeof(g_entities[3]));
    level.spawnVar.numSpawnVars = 1;
    level.spawnVar.spawnVars[0][0] = key;
    level.spawnVar.spawnVars[0][1] = (char *)value;
    G_CallSpawn();
    return &g_entities[3];
}

/* Mac 0x1a0bd3-0x1a0c03: "*N" stores the zero-extended 16-bit index at s.index (0x8c), which
   SV_SetBrushModel reads at 0x14e1fe. ent->model at 0x164 stays zero. */
static void brush_model_index(void)
{
    gentity_t *ent = spawn_with_model("*12");
    assert(ent->s.index.brushmodel == 12);
    assert(ent->model == 0);
    assert(setModels == 0);

    ent = spawn_with_model("*70000");
    assert(ent->s.index.brushmodel == 70000 - 65536);
    assert(ent->model == 0);

    ent = spawn_with_model("xmodel/prop");
    assert(setModels == 1 && !strcmp(lastModel, "xmodel/prop"));
    assert(ent->s.index.brushmodel == 0);
}

/* Mac 0x1a1527 and 0x1a15c7: client fields on a clientless entity do nothing. The getter pushes
   no value and the setter returns zero. */
static void client_fields(void)
{
    gentity_t *ent = &g_entities[3];
    memset(ent, 0, sizeof(*ent));
    Scr_GetObjectField(0, 3, 0xC002);
    assert(undefinedPushes == 0 && clientGets == 0);
    assert(Scr_SetObjectField(0, 3, 0xC001) == 0 && clientSets == 0);

    ent->client = clients;
    Scr_GetObjectField(0, 3, 0xC002);
    assert(clientGets == 1 && lastClientOffset == 2 && undefinedPushes == 0);
    assert(Scr_SetObjectField(0, 3, 0xC001) == 1 && clientSets == 1 && lastClientOffset == 1);
}

int main(int argc, char **argv)
{
    static int numItems;
    if (argc != 2)
        return 2;
    imp_g_entities = g_entities;
    imp_bg_numItems = &numItems;
    if (!strcmp(argv[1], "brush")) { brush_model_index(); puts("spawn brush: pass"); return 0; }
    if (!strcmp(argv[1], "clientfields")) { client_fields(); puts("spawn clientfields: pass"); return 0; }
    return 2;
}
