#include "common_types.h"
gentity_t testEntity;

/* Match the real server/sound/VM signatures in a separate translation unit. */
gentity_t *SV_AddTestClient(void) { return &testEntity; }
snd_alias_list_t *Com_FindSoundAlias(const char *name)
{
    (void)name;
    return (snd_alias_list_t *)(uintptr_t)0x100000000ull;
}
const char *Scr_GetTypeName(unsigned int index) { (void)index; return "undefined"; }
