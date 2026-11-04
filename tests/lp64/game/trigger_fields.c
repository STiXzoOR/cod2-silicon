/* Exercise Mac trigger setup through production g_trigger_mp entry points. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

extern void SP_trigger_lookat(gentity_t *self);
extern void SP_trigger_multiple(gentity_t *ent);
extern void SP_trigger_damage(gentity_t *ent);
extern void SP_trigger_radius(gentity_t *ent);
extern void SP_trigger_disk(gentity_t *ent);
extern void *imp_level;

level_locals_t level;
scr_const_t scr_const;
static gentity_t ent;
static float waitValue;
static float radiusValue = 32;

void SV_SetBrushModel(gentity_t *e) {}
void SV_LinkEntity(gentity_t *e) {}
qboolean G_SpawnInt(const char *key, const char *def, int *out) { *out = 0; return 1; }
qboolean G_SpawnFloat(const char *key, const char *def, float *out)
{
    *out = !strcmp(key, "wait") ? waitValue : radiusValue;
    return 1;
}

/* Mac 0x1b7532 stores 0x20000000, which Linux names CONTENTS_TRANSLUCENT. */
static void lookat_contents(void)
{
    SP_trigger_lookat(&ent);
    assert(ent.r.contents == 0x20000000);
    assert(ent.r.svFlags == 1 && (ent.s.eFlags & 1));
}

/* Each spawner tests "0 < wait" with ucomiss and a carry jump, which skips the flag on NaN. */
static void check_wait(void (*spawn)(gentity_t *), int flag)
{
    static const float values[] = {-1, 0, 1};
    static const int set[] = {1, 1, 0};
    for (int i = 0; i < 3; ++i) {
        memset(&ent, 0, sizeof(ent));
        waitValue = values[i];
        spawn(&ent);
        assert(!!(ent.spawnflags & flag) == set[i]);
    }
    memset(&ent, 0, sizeof(ent));
    waitValue = NAN;
    spawn(&ent);
    assert(!(ent.spawnflags & flag));
}

static void wait_flags(void)
{
    check_wait(SP_trigger_multiple, 0x10);
    check_wait(SP_trigger_damage, 0x200);
    check_wait(SP_trigger_radius, 0x10);
    check_wait(SP_trigger_disk, 0x10);
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    imp_level = &level;
    level.spawnVar.spawnVarsValid = 1;
    if (!strcmp(argv[1], "lookat")) { lookat_contents(); puts("trigger lookat: pass"); return 0; }
    if (!strcmp(argv[1], "wait")) { wait_flags(); puts("trigger wait: pass"); return 0; }
    return 2;
}
