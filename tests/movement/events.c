#include "common_types.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static cg_t fixture;
static cg_t *cg = &fixture;
static int count, event, parm;
#define CG_OFF_2826C offsetof(cg_t, predictedPlayerEntity)
#define CENT_EVENT_PARM offsetof(centity_t, nextState.eventParm)
#define CG_INT(base, offset) (*(int *)((char *)(base) + (offset)))
void CG_DamageFeedback(int yaw, int pitch, int damage) { (void)yaw; (void)pitch; (void)damage; assert(0); }
void CG_EntityEvent(centity_t *cent, int value) { ++count; event = value; parm = cent->nextState.eventParm; }
#include "transition_source.h"

int main(void)
{
    playerState_t ps = {0}, old = {0};
    old.eventSequence = ps.eventSequence = 10;
    ps.events[2] = 8; old.events[2] = 9;
    CG_TransitionPlayerState(&ps, &old);
    assert(count == 0); /* Mac 1.3 0x1e40cc skips <= oldSequence - 4. */
    ps = old; ps.eventSequence = 11; ps.events[2] = 8; ps.eventParms[2] = 37;
    CG_TransitionPlayerState(&ps, &old);
    assert(count == 1 && event == 8 && parm == 37);
    old = ps; ps.eventSequence = 12; ps.events[3] = 31; ps.eventParms[3] = 38;
    count = 0; CG_TransitionPlayerState(&ps, &old);
    assert(count == 1 && event == 31 && parm == 38);
    puts("PASS local predictable-event history boundary and event parameters");
    return 0;
}
