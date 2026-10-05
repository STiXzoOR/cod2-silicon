#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gentity_t g_entities[1];
static gclient_t client;
static dvar_t cheats = { .current.enabled = 1 };
const dvar_t *g_cheats = &cheats;
static const char *args[] = { "setviewpos", "10", "20", "80", "135", "25" };
static int argc, teleports, messages;
int SV_Cmd_Argc(void) { return argc; }
void SV_Cmd_ArgvBuffer(int index, char *buffer, int size) { snprintf(buffer, size, "%s", args[index]); }
const char *va(const char *format, ...) { return format; }
void SV_GameSendServerCommand(int index, int type, const char *text)
{ assert(index == 0 && type == SV_CMD_RELIABLE && text); ++messages; }
void TeleportPlayer(gentity_t *ent, vec_t *origin, vec_t *angles)
{
    assert(ent == g_entities);
    assert(origin[0] == 10 && origin[1] == 20 && origin[2] == 20);
    assert(angles[1] == 135 && angles[2] == 0);
    assert(angles[0] == (argc == 6 ? 25 : 0));
    ++teleports;
}
#include "setviewpos_source.h"
int main(void)
{
    g_entities[0].client = &client;
    client.ps.viewHeightCurrent = 60;
    argc = 5; Cmd_SetViewpos_f(g_entities);
    argc = 6; Cmd_SetViewpos_f(g_entities);
    argc = 4; Cmd_SetViewpos_f(g_entities);
    cheats.current.enabled = 0;
    argc = 6; Cmd_SetViewpos_f(g_entities);
    assert(teleports == 2 && messages == 2);
    puts("online: deterministic native yaw/pitch camera preserves cheats and eye height passed");
}
