#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

level_locals_t level;
gentity_t g_entities[4];
static gclient_t clients[4];
static dvar_t allowVote = { .current.enabled = 1 }, oldVoting = { .current.enabled = 1 };
static dvar_t gametype = { .current.string = "dm" };
const dvar_t *g_allowVote = &allowVote, *g_oldVoting = &oldVoting, *g_gametype = &gametype;
static const char *args[] = { "callvote", "map_restart", "", "" };
static char vabuf[1024];

int SV_Cmd_Argc(void) { return 2; }
void SV_Cmd_ArgvBuffer(int index, char *buffer, int size) { snprintf(buffer, size, "%s", args[index]); }
const char *va(const char *format, ...)
{
    va_list ap; va_start(ap, format); vsnprintf(vabuf, sizeof(vabuf), format, ap); va_end(ap);
    return vabuf;
}
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list ap; va_start(ap, format); int n = vsnprintf(dest, size, format, ap); va_end(ap);
    return n;
}
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void SV_GameSendServerCommand(int index, int type, const char *text) {}
void SV_SetConfigstring(int index, const char *value) {}
void Cbuf_ExecuteText(int when, const char *text) { assert(!"no vote is pending"); }
void Scr_VoteCalled(gentity_t *ent, const char *a, const char *b, const char *c) { assert(!"old voting is on"); }
/* Map, gametype and kick votes are not exercised. */
void I_strncpyz(char *dest, const char *src, int destsize) { assert(0); }
char *I_CleanStr(char *string) { assert(0); return string; }
const dvar_t *Dvar_RegisterString_mac(const char *dvarName, const char *value, int flags) { assert(0); return NULL; }
qboolean SV_MapExists(const char *name) { assert(0); return 0; }
qboolean Scr_IsValidGameType(const char *pszGameType) { assert(0); return 0; }
const char *Scr_GetGameTypeNameForScript(const char *pszGameTypeScript) { assert(0); return ""; }
#include "callvote_source.h"

int main(void)
{
    level.clients = clients;
    level.maxclients = 4;
    level.numConnectedClients = 2;
    for (int i = 0; i < 4; ++i) {
        g_entities[i].client = &clients[i];
        clients[i].ps.eFlags = 0x100000;
    }
    clients[0].sess.cs.team = TEAM_ALLIES;
    /* Linux 1.3 g_cmds_mp.cpp and Mac 0x1a5a0c clear EF_VOTED for every slot
       up to level.maxclients, not only the connected-client count. */
    Cmd_CallVote_f(&g_entities[0]);
    assert(level.voteYes == 1 && level.voteNo == 0 && level.voteTime == 30000);
    assert(clients[0].ps.eFlags & 0x100000);
    for (int i = 1; i < 4; ++i)
        assert(!(clients[i].ps.eFlags & 0x100000));
    puts("online: callvote clears the voted flag on every client slot and marks the caller");
    return 0;
}
