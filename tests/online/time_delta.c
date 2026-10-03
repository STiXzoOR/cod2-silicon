#include "common_types.h"
#include <assert.h>
#include <stdio.h>

static clientActive_t active;
static clientConnection_t connection;
static clientStatic_t client;
#define CL_LOCAL (&active)
#define CLUI_STATE (&connection)
#define CLS (&client)
static dvar_t showDelta;
static const dvar_t *cl_showTimeDelta = &showDelta;
void Com_Printf(const char *format, ...) { (void)format; }
static float CL_ComTimescaleValue(void) { return 1.0f; }
#include "time_delta_source.h"

int main(void)
{
    for (int sign = -1; sign <= 1; sign += 2) {
        client.realtime = 1000;
        active.snap.serverTime = sign * 1500000000;
        active.oldSnapServerTime = active.snap.serverTime - 50;
        int newDelta = active.snap.serverTime - 1055;
        active.serverTimeDelta = newDelta - 200;
        CL_AdjustTimeDelta();
        assert(active.serverTimeDelta == newDelta - 100);
    }
    puts("online: time-delta smoothing preserves large positive and negative server epochs");
    return 0;
}
