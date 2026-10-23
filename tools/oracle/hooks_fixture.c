/* Synthetic world: exercise the real trace/command hooks without game data. */
#include "common_types.h"
#include <stdlib.h>
#include <string.h>

serverStatic_t svs;
level_locals_t level;
gentity_t g_entities[1024];
static gclient_t clients[2];
static client_t connections[2];

extern void Sys_DiffInputFrame(void);
extern int Sys_DiffBotMove(void);
extern int Sys_DiffClientThink(int clientNum, const usercmd_t *cmd);
extern void Sys_StateHashFrame(void);

void SV_ClientThink(client_t *cl, usercmd_t *cmd)
{
    int i = (int)(cl - svs.clients);
    if (!Sys_DiffClientThink(i, cmd))
        return;
    cl->lastUsercmd = *cmd;
    clients[i].ps.origin[0] += cmd->forwardmove * 0.125f;
    clients[i].ps.velocity[1] = cmd->rightmove;
    clients[i].ps.viewangles[1] = (float)cmd->angles[1];
    clients[i].ps.pm_time = cmd->serverTime - level.startTime;
    clients[i].ps.pm_flags = cmd->buttons;
}

int main(void)
{
    svs.clients = connections;
    level.startTime = getenv("ORACLE_FIXTURE_BASE") ? atoi(getenv("ORACLE_FIXTURE_BASE")) : 5000;
    level.time = level.startTime;
    level.maxclients = 2;
    level.num_entities = 2;
    for (int i = 0; i < 2; ++i) {
        connections[i].state = 4;
        connections[i].bIsTestClient = 1;
        g_entities[i].r.inuse = 1;
        g_entities[i].client = &clients[i];
        clients[i].ps.weapon = 7 + i;
        clients[i].ps.stats[0] = 100;
        g_entities[i].s.number = i;
    }
    for (int frame = 1; frame <= 24; ++frame) {
        svs.time = level.startTime + frame * 50;
        if (!Sys_DiffBotMove())
            return 3;
        if (getenv("ORACLE_FIXTURE_NETWORK")) {
            usercmd_t noise;
            memset(&noise, 0, sizeof noise);
            noise.forwardmove = -1;
            SV_ClientThink(&connections[0], &noise);
        }
        Sys_DiffInputFrame();
        level.framenum = frame;
        level.time = svs.time;
        Sys_StateHashFrame();
    }
    return 1; /* MAXFRAMES should have terminated the run. */
}
