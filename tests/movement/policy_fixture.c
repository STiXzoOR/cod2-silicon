/* Production runtime and typed dvars own revision selection in the matrix.
   Prefix only the synthetic allocator/string/log services to avoid collisions
   with services supplied by the production movement/animation fixture. */
#define Com_Error Matrix_Com_Error
#define Com_Printf Matrix_Com_Printf
#define Com_PrintMessage Matrix_Com_PrintMessage
#define I_stricmp Matrix_I_stricmp
#define stricmp Matrix_stricmp
#define strnicmp Matrix_strnicmp
#define I_strncpyz Matrix_I_strncpyz
#define I_strncat Matrix_I_strncat
#define Com_sprintf Matrix_Com_sprintf
#define va Matrix_va
#include "../fidelity/dvar_services.h"
#include "../../src/PC/qcommon/cod2x.h"
#include <assert.h>

#undef Com_Printf
void __attribute__((weak)) Com_Printf(const char *format, ...) { (void)format; }

/* Movement does not load an animation table. The animation fixture overrides
   these with the real production reset implementations. */
void __attribute__((weak)) Cod2x_ResetAnimation(void) {}
void __attribute__((weak)) Cod2x_ResetClientAnimation(void) {}

void Fixture_SetRevision(int revision)
{
    static int initialized;
    /* -1 selects the current release's real hosted zero lifetime. */
    assert(revision == -1 || revision == 0 || revision == 3 || revision == 4 || revision == 6);
    if (!initialized) {
        Dvar_Init();
#if defined(COD2_CODX) && COD2_CODX
        Cod2x_ServerInit(0);
#endif
        initialized = 1;
    }
#if defined(COD2_CODX) && COD2_CODX
    if (Cod2x_ServerActive())
        Cod2x_ServerShutdown();
    if (revision == -1) {
        Cod2x_ServerSpawn();
        assert(Cod2x_GameVersion() == 0 && Cod2x_ServerGameVersion() == 0);
        assert(Dvar_FindVar("g_cod2x")->current.integer == 0);
        return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%d", revision);
    assert(!Cod2x_ServerActive());
    assert(Cod2x_SetClientDvar("g_cod2x", text));
    assert(Cod2x_GameVersion() == revision);
    assert(Dvar_FindVar("g_cod2x")->current.integer == revision);
#else
    assert(!Dvar_FindVar("g_cod2x"));
#endif
}

void Fixture_VerifyHostZero(void)
{
#if defined(COD2_CODX) && COD2_CODX
    Cod2x_ServerSpawn();
    assert(Cod2x_ServerActive());
    assert(Cod2x_GameVersion() == 0 && Cod2x_ServerGameVersion() == 0);
    assert(Cod2x_SetClientDvar("g_cod2x", "6"));
    assert(Cod2x_GameVersion() == 0);
    assert(Dvar_FindVar("g_cod2x")->domain.integer.max == 0);
    Cod2x_ServerShutdown();
    Fixture_SetRevision(6);
#endif
}
