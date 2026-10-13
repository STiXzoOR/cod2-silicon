#include "common_types.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

static level_locals_t level;
static char fogConfig[128];
static int fogWrites;
void SV_SetConfigstring(int index, const char *value)
{
    assert(index == 12);
    assert(strlen(value) < sizeof(fogConfig));
    strcpy(fogConfig, value);
    ++fogWrites;
}
#include "level_state_source.h"

int main(void)
{
    for (int previous = 0; previous < 2; ++previous) {
        memset(&level, previous ? 0x55 : 0, sizeof(level));
        G_setfog("0 100 1 0.5 0.5 0.5 0");
        assert(level.fFogOpaqueDist == 100 && level.fFogOpaqueDistSqrd == 10000);
        fogWrites = 0;
        initialize_spawned_level();
        /* Mac G_InitGame 0x1ae0b8/0x1ae0bd; reset precedes level scripts. */
        assert(fogWrites == 1 && strcmp(fogConfig, "0") == 0);
        assert(level.fFogOpaqueDist == FLT_MAX && level.fFogOpaqueDistSqrd == FLT_MAX);
        for (int i = 0; i < 16; ++i) {
            objective_t *obj = &level.objectives[i];
            assert(obj->state == OBJST_EMPTY && obj->entNum == 1023);
            assert(obj->origin[0] == 0 && obj->origin[1] == 0 && obj->origin[2] == 0);
            assert(obj->teamNum == 0 && obj->icon == 0);
        }
        /* A later map script must still be able to override the defaults. */
        G_setfog("0 200 1 0.5 0.5 0.5 0");
        assert(level.fFogOpaqueDist == 200 && level.fFogOpaqueDistSqrd == 40000);
    }
    puts("PASS fresh/reused level fog reset and all 16 objective sentinels");
}
