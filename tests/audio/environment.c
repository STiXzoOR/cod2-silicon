#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

snd_local_t g_snd;
static int count = 6, messages, roomCalls;
static char *arguments[] = {"snd_setEnvironmentEffects", "level", "room", "0.8", "0.9", "100"};
int Cmd_Argc(void) { return count; }
char *Cmd_Argv(int index) { return arguments[index]; }
void Com_Printf(const char *format, ...) { (void)format; ++messages; }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void SND_SetRoomtype(int room) { (void)room; ++roomCalls; }
#include "environment_source.h"

static void reset(void)
{
    memset(&g_snd, 0, sizeof(g_snd));
    g_snd.Initialized2d = 1;
    g_snd.effect = &g_snd.envEffects[0];
    g_snd.effect->drylevel = 0.2f;
    g_snd.effect->wetlevel = 0.3f;
    count = 6;
    messages = roomCalls = 0;
    arguments[1] = "level";
    arguments[2] = "room";
    arguments[3] = "0.8";
    arguments[4] = "0.9";
    arguments[5] = "100";
}

static void unchanged(void)
{
    snd_local_t before = g_snd;
    SND_SetEnvironmentEffects_f();
    assert(!memcmp(&before, &g_snd, sizeof(g_snd)));
    assert(messages > 0 && roomCalls == 0);
}

int main(void)
{
    reset();
    SND_SetEnvironmentEffects_f();
    assert(g_snd.effect == &g_snd.envEffects[1]);
    assert(g_snd.effect->active && g_snd.effect->roomtype == 2);
    assert(g_snd.effect->drygoal == 0.8f && g_snd.effect->wetgoal == 0.9f);
    assert(fabsf(g_snd.effect->dryrate - 0.006f) < 0.000001f);
    assert(fabsf(g_snd.effect->wetrate - 0.006f) < 0.000001f);
    assert(roomCalls == 1 && messages == 0);
    reset();
    arguments[1] = "SHELLSHOCK"; arguments[2] = "GENERIC";
    arguments[3] = "0"; arguments[4] = "1"; arguments[5] = "0";
    SND_SetEnvironmentEffects_f();
    assert(g_snd.effect == &g_snd.envEffects[2] && g_snd.effect->roomtype == 0);
    assert(g_snd.effect->drygoal == 0 && g_snd.effect->wetgoal == 1);
    assert(g_snd.effect->dryrate == -0.2f && g_snd.effect->wetrate == 0.7f);
    for (int argc = 0; argc < 8; ++argc) {
        if (argc == 6) continue;
        reset(); count = argc; unchanged();
    }
    const char *invalid[] = {"none", "effect", "invalid"};
    for (int i = 0; i < 3; ++i) { reset(); arguments[1] = (char *)invalid[i]; unchanged(); }
    reset(); arguments[2] = "not-a-room"; unchanged();
    for (int arg = 3; arg <= 4; ++arg) {
        reset(); arguments[arg] = "-0.01"; unchanged();
        reset(); arguments[arg] = "1.01"; unchanged();
    }
    reset(); arguments[5] = "-1"; unchanged();
    reset(); g_snd.Initialized2d = 0;
    SND_SetEnvironmentEffects_f();
    assert(g_snd.effect == &g_snd.envEffects[0] && roomCalls == 0);
    puts("environment command -> real sound effect state: PASS");
    return 0;
}
