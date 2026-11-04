#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static clientActive_t active;
static clientConnection_t connection;
static refexport_t re;
#define CL_LOCAL (&active)
#define CLUI_STATE (&connection)
#define RE (&re)
static dvar_t svRunning;
const dvar_t *com_sv_running = &svRunning;
static int initArgs[3];
static int initCalls;

int Sys_Milliseconds(void) { return 0; }
void Con_Close(void) {}
void Con_ClearNotify(void) {}
void Con_ClearSubtitles(void) {}
void Com_Printf(const char *format, ...) { (void)format; }
void Com_TouchMemory(void) {}
void Com_InitDObj(void) {}
void CL_SetExpectedHunkUsage(const char *mapname) { (void)mapname; }
void CL_UpdateLevelHunkUsage(void) {}
const char *GetBspExtension(void) { return "d3dbsp"; }
char *Info_ValueForKey(const char *s, const char *key) { (void)s; (void)key; return "mp_toujane"; }
void I_strncpyz(char *dest, const char *src, int size) { strncpy(dest, src, size - 1); dest[size - 1] = 0; }
int Com_sprintf(char *dest, int size, const char *fmt, ...) { (void)fmt; strncpy(dest, "maps/mp/mp_toujane.d3dbsp", size); return 0; }
static void EndRegistration(void) {}
void CG_Init(int serverMessageNum, int serverCommandSequence, int clientNum)
{
    initArgs[0] = serverMessageNum;
    initArgs[1] = serverCommandSequence;
    initArgs[2] = clientNum;
    ++initCalls;
}
#include "cgame_init_source.h"

int main(void)
{
    re.EndRegistration = EndRegistration;
    svRunning.current.enabled = 1;
    connection.clientNum = 3;
    connection.serverMessageSequence = 1234;
    connection.lastExecutedServerCommand = 56;
    CL_InitCGame();
    /* Mac CL_InitCGame (0x17b4d2) pushes serverMessageSequence, then
       lastExecutedServerCommand, then clientNum. */
    assert(initCalls == 1);
    assert(initArgs[0] == 1234 && initArgs[1] == 56 && initArgs[2] == 3);
    assert(active.cgameInitCalled && active.cgameInitialized);
    puts("online: CL_InitCGame passes the snapshot sequence, command sequence and client number in Mac order");
    return 0;
}
