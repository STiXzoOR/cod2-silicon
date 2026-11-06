/* Complete production reliable-command dispatcher with real argument/state consumers. */
#include "server_commands_source.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>

int cod2_native_trace_enabled;
cg_t cgArray[1];
unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
snd_local_t g_snd;
static LegacyHacks hacks;
LegacyHacks *legacyHacks = &hacks;
void *imp_legacyHacks = &legacyHacks;
static dvar_t zero;
static const dvar_t *zero_ptr = &zero;
const dvar_t *cg_chatHeight = &zero, *cg_chatTime = &zero, *cg_thirdPerson = &zero;
void *imp_cg_showmiss = &zero_ptr, *imp_cg_teamChatsOnly = &zero_ptr;
static displayContextDef_t context;
void *imp_cgDC = &context;
static WeaponDef definitions[4];
static menuDef_t offhand_menu;
static const char *arguments[8];
static int argument_count, diagnostics, room_calls, last_room, menu_calls, lookup_calls, command_calls;

int Cmd_Argc(void) { return argument_count; }
void Cmd_ArgvBuffer(int index, char *dest, int capacity)
{ assert(index >= 0); snprintf(dest, (size_t)capacity, "%s", index < argument_count ? arguments[index] : ""); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void Com_Printf(const char *fmt, ...) { (void)fmt; ++diagnostics; }
void SND_SetRoomtype(int room) { ++room_calls; last_room = room; }
WeaponDef *BG_GetWeaponDef(int n) { assert(n > 0 && n < 128); ++lookup_calls; return n < 4 ? &definitions[n] : NULL; }
int CL_GetLocalClientActiveCount(void) { return 1; }
menuDef_t *Menus_FindByName(displayContextDef_t *dc, const char *name)
{ assert(dc == &context && !strcmp(name, "offhandinfo")); return &offhand_menu; }
void Window_AddDynamicFlags(void *window, int flags)
{ assert(window == &offhand_menu && flags == 4); ++menu_calls; }
int CL_GetServerCommand(int sequence)
{ assert(sequence == 1 || sequence == 2); ++command_calls; return sequence == 2; }
#include "server_commands_helpers.h"

/* Unrelated dispatch destinations fail immediately if a tested command reaches them. */
const char *CL_GetConfigString(int n) { (void)n; abort(); }
char *Info_ValueForKey(const char *a, const char *b) { (void)a; (void)b; abort(); }
void Dvar_SetStringByName(const char *a, const char *b) { (void)a; (void)b; abort(); }
void Dvar_SetFromStringByName(const char *a, const char *b) { (void)a; (void)b; abort(); }
const char *GetBspExtension(void) { abort(); }
int Com_sprintf(char *dest, int size, const char *fmt, ...) { (void)dest; (void)size; (void)fmt; abort(); }
void Com_Error(errorParm_t code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Cbuf_AddText(const char *s) { (void)s; abort(); }
const char *va(const char *fmt, ...) { (void)fmt; abort(); }
void I_strncpyz(char *dest, const char *src, int size) { (void)dest; (void)src; (void)size; abort(); }
qboolean CL_Popup(const char *s) { (void)s; abort(); }
void CL_ClosePopup(const char *s) { (void)s; abort(); }
const char *Com_Parse(const char **s) { (void)s; abort(); }
void CL_SwitchFog(int n, int time, int fade) { (void)n; (void)time; (void)fade; abort(); }
void CL_SetFog(int n, float start, float half, int r, int g, int b, float density)
{ (void)n; (void)start; (void)half; (void)r; (void)g; (void)b; (void)density; abort(); }
void SND_DeactivateChannelVolumes(int n, int fade) { (void)n; (void)fade; abort(); }
void SND_SetChannelVolumes(int n, const float *volumes, int fade) { (void)n; (void)volumes; (void)fade; abort(); }
void SND_DeactivateEnvironmentEffects(int n, int fade) { (void)n; (void)fade; abort(); }
void SND_PlayMusicAlias(const snd_alias_t *alias, snd_alias_system_t system) { (void)alias; (void)system; abort(); }
void SND_StopMusic(int fade) { (void)fade; abort(); }
void SND_FadeAllSounds(float volume, int fade) { (void)volume; (void)fade; abort(); }
int Load_ScriptMenu(const char *name, int flags) { (void)name; (void)flags; abort(); }
MaterialHandle CL_RegisterMaterialNoMip(const char *name, int flags) { (void)name; (void)flags; abort(); }
MaterialHandle CL_RegisterMaterial(const char *name, int flags) { (void)name; (void)flags; abort(); }
struct XModel *CL_RegisterModel(const char *name) { (void)name; abort(); }
struct EffectTemplate *FX_RegisterEffect(const char *name) { (void)name; abort(); }
void CG_InitLocalEntities(void) { abort(); }
void CG_InitMarkPolys(void) { abort(); }
void FX_FreeActive(void) { abort(); }
void SND_StopSounds(snd_stopsounds_arg_t mode) { (void)mode; abort(); }
void CG_StartAmbient(void) { abort(); }
void Dvar_SetBool(void *dvar, int value) { (void)dvar; (void)value; abort(); }
void CL_SetADS(int value) { (void)value; abort(); }
void CL_CloseAllMenus(void) { abort(); }
void CG_ScoresUp_f(void) { abort(); }
void CL_SyncTimes(void) { abort(); }
const char *SEH_LocalizeTextMessage(const char *msg, const char *context_name, int error)
{ (void)msg; (void)context_name; (void)error; abort(); }
void CG_GameMessage(const char *msg) { (void)msg; abort(); }
void CG_BoldGameMessage(const char *msg) { (void)msg; abort(); }
void CG_TranslateHudElemMessage(const char *msg, const char *type, char *out) { (void)msg; (void)type; (void)out; abort(); }
Bool CG_PlaySoundOnFirstClient(void) { abort(); }
snd_alias_t *CL_PickSoundAlias(const char *name) { (void)name; abort(); }
int CG_PlayClientSoundAliasByName(const char *name) { (void)name; abort(); }
void CG_SelectWeaponIndex(int n) { (void)n; abort(); }
void CL_ResetPlayerMuting(int n) { (void)n; abort(); }
void CL_CloseInGameMenu(void) { abort(); }
void CG_SetupWeaponDef(void) { abort(); }
void CG_RegisterItems(void) { abort(); }
void CG_NorthDirectionChanged(void) { abort(); }
qboolean CG_LoadShellShockDvars(const char *name) { (void)name; abort(); }
void CG_SetShellShockParmsFromDvars(shellshock_parms_t *parms) { (void)parms; abort(); }
const char *UI_GetMapDisplayNameFromPartialLoadNameMatch(const char *name, int *length)
{ (void)name; (void)length; abort(); }
#if defined(COD2_CODX) && COD2_CODX
int Cod2x_SetClientDvar(const char *name, const char *value) { (void)name; (void)value; abort(); }
#endif

static void Reset(void)
{
    memset(cgArray, 0, sizeof(cgArray)); memset(cgsArray, 0, sizeof(cgsArray));
    memset(&g_snd, 0, sizeof(g_snd)); memset(definitions, 0, sizeof(definitions));
    diagnostics = room_calls = menu_calls = lookup_calls = command_calls = 0;
    last_room = -1;
    cg->time = 100; cg->equippedOffHand = 3;
    definitions[1].offhandClass = 0; definitions[2].offhandClass = 1; definitions[3].offhandClass = 2;
    g_snd.Initialized2d = 1; g_snd.effect = &g_snd.envEffects[0];
    g_snd.effect->drylevel = 0.2f; g_snd.effect->wetlevel = 0.3f;
    argument_count = 6;
    arguments[0] = "r"; arguments[1] = "1"; arguments[2] = "room";
    arguments[3] = "0.8"; arguments[4] = "0.9"; arguments[5] = "0.125";
    arguments[6] = "extra"; arguments[7] = "extra";
}

static void Near(float actual, float expected)
{ assert(isfinite(actual) && fabsf(actual - expected) < 0.000001f); }

static void CheckReverb(int room, int milliseconds)
{
    assert(g_snd.effect == &g_snd.envEffects[1]);
    assert(g_snd.effect->active && g_snd.effect->roomtype == room);
    assert(last_room == room && room_calls == 1 && diagnostics == 0);
    assert(g_snd.effect->drygoal == 0.8f && g_snd.effect->wetgoal == 0.9f);
    float divisor = milliseconds > 0 ? (float)milliseconds : 1.0f;
    Near(g_snd.effect->dryrate, 0.6f / divisor);
    Near(g_snd.effect->wetrate, 0.6f / divisor);
}

static void ReverbRoom(void)
{
    static const char *names[] = {"room", "GENERIC", "bathroom"};
    static const int rooms[] = {2, 0, 3};
    for (int i = 0; i < 3; ++i) {
        Reset(); arguments[2] = names[i];
        const char *shared = CG_Argv(2);
        assert(shared == CG_Argv(5) && !strcmp(shared, "0.125"));
        CG_ServerCommand(); CheckReverb(rooms[i], 125);
    }
}

static void ReverbFades(void)
{
    static const char *text[] = {"-1", "0", "0.0004", "0.0005", "0.0014", "0.0015", "1.25"};
    static const int milliseconds[] = {0, 0, 0, 1, 1, 2, 1250};
    for (int i = 0; i < 7; ++i) {
        Reset(); arguments[5] = text[i]; CG_ServerCommand(); CheckReverb(2, milliseconds[i]);
    }
}

static void ReverbArity(void)
{
    for (int count = 1; count <= 8; ++count) {
        if (count == 6) continue;
        Reset(); argument_count = count; snd_local_t before = g_snd;
        CG_ServerCommand(); assert(!memcmp(&before, &g_snd, sizeof(g_snd)));
        assert(diagnostics == 1 && room_calls == 0);
    }
}

static void OffhandReject(void)
{
    Reset(); arguments[0] = "C"; arguments[1] = "1"; argument_count = 2;
    CG_ServerCommand();
    assert(cg->equippedOffHand == 3 && cg->offhandFadeTime == 0);
    assert(menu_calls == 0 && lookup_calls == 1 && diagnostics == 0);
}

static void OffhandUnregistered(void)
{
    Reset(); arguments[0] = "C"; arguments[1] = "100"; argument_count = 2;
    CG_ServerCommand();
    assert(cg->equippedOffHand == 3 && cg->offhandFadeTime == 0);
    assert(menu_calls == 0 && lookup_calls == 1 && diagnostics == 0);
}

static void OffhandValid(void)
{
    static const char *text[] = {"0", "2", "3"};
    static const int weapons[] = {0, 2, 3};
    for (int i = 0; i < 3; ++i) {
        Reset(); arguments[0] = "C"; arguments[1] = text[i]; argument_count = 2;
        CG_ServerCommand(); assert(cg->equippedOffHand == weapons[i] && cg->offhandFadeTime == 100);
        assert(menu_calls == 1 && lookup_calls == (weapons[i] != 0) && diagnostics == 0);
    }
}

static void Sequence(void)
{
    Reset(); arguments[0] = "C"; arguments[1] = "2"; argument_count = 2;
    CG_ExecuteNewServerCommands(2);
    assert(cgs->serverCommandSequence == 2 && command_calls == 2);
    assert(cg->equippedOffHand == 2 && menu_calls == 1 && lookup_calls == 1);
    CG_ExecuteNewServerCommands(2);
    assert(command_calls == 2 && menu_calls == 1 && lookup_calls == 1);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "reverb-room")) ReverbRoom();
    else if (!strcmp(argv[1], "reverb-fades")) ReverbFades();
    else if (!strcmp(argv[1], "reverb-arity")) ReverbArity();
    else if (!strcmp(argv[1], "offhand-reject")) OffhandReject();
    else if (!strcmp(argv[1], "offhand-unregistered")) OffhandUnregistered();
    else if (!strcmp(argv[1], "offhand-valid")) OffhandValid();
    else if (!strcmp(argv[1], "sequence")) Sequence();
    else abort();
    printf("server commands %s: PASS\n", argv[1]);
    return 0;
}
