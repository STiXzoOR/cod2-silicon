#include "common_types.h"
#include <assert.h>
#include <stdlib.h>

extern gentity_t *G_Spawn(void);
level_locals_t level;
gentity_t g_entities[1024];
scr_const_t scr_const;
unsigned char scrMemTreeGlob[0x80330] __attribute__((aligned(16)));
static gclient_t clients[64];
static int registeredClientSize;
static int registeredEntitySize;

void Scr_SetString(scr_string_t *to, unsigned int value) { *to = value; }
void Com_Error(int code, const char *fmt, ...) { abort(); }
void Com_Printf(const char *fmt, ...) {}
const char *SL_ConvertToString(unsigned int id) { return "entity"; }
void SV_LocateGameData(gentity_t *entities, int count, int entitySize,
                       playerState_t *players, int clientSize)
{
    assert(entities == g_entities);
    assert(players == &clients[0].ps);
    assert(count == level.num_entities);
    registeredClientSize = clientSize;
    registeredEntitySize = entitySize;
}

int main(void)
{
    level.gentities = g_entities;
    level.clients = clients;
    level.num_entities = 72;
    level.time = 1000;
    scr_const.noclass = 12;
    gentity_t *spawned = G_Spawn();
    assert(spawned == &g_entities[72] && spawned->r.inuse);
    assert(spawned->classname == 12 && spawned->s.number == 72);
    assert(registeredClientSize == sizeof(gclient_t));
    assert(registeredEntitySize == sizeof(gentity_t));
    for (int i = 0; i < 1024; i++)
        assert(COD2_GEntityFromHandle(COD2_GEntityHandle(&g_entities[i])) == &g_entities[i]);
    assert(!COD2_GEntityFromHandle(0) && !COD2_GEntityFromHandle(1025));
    level.firstFreeEnt = &g_entities[80];
    level.lastFreeEnt = &g_entities[81];
    g_entities[80].nextFree = COD2_GEntityHandle(&g_entities[81]);
    assert(G_Spawn() == &g_entities[80]);
    assert(level.firstFreeEnt == &g_entities[81]);
    assert(G_Spawn() == &g_entities[81]);
    assert(!level.firstFreeEnt && !level.lastFreeEnt);
    tagInfo_t *tag = (tagInfo_t *)(scrMemTreeGlob + 64);
    assert(COD2_TagInfoHandle(tag) == 65);
    assert(COD2_TagInfoFromHandle(65) == tag);
    assert(!COD2_TagInfoFromHandle(0));
    assert(!COD2_TagInfoFromHandle(0x80331));
    puts("entities: spawn/client stride, free-list and arena handles pass");
}
