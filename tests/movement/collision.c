#define MOVEMENT_COLLISION
#define main unused_fixture_main
#include "movement.c"
#undef main
#include "slidemove_source.h"
#include "mantle_source.h"

static int traceNumber, testWorld;
static vec3_t overStart, overEnd;

qboolean Jump_IsPlayerAboveMax(playerState_t *ps) { (void)ps; return 0; }
Bool Jump_GetStepHeight(playerState_t *ps, const vec_t *origin, float *height)
{ (void)ps; (void)origin; *height = 18; return 1; }
void Jump_ClampVelocity(playerState_t *ps, const vec_t *origin) { (void)ps; (void)origin; }
int XAnimGetLengthMsec(const struct XAnim_s *anims, unsigned int index) { (void)anims; (void)index; return 100; }
void XAnimGetAbsDelta(const struct XAnim_s *anims, unsigned int index, float *rot, float *delta, float frac)
{ (void)anims; (void)rot; delta[0] = frac * (index < 8 ? 16.0f : 31.0f); delta[1] = 0; delta[2] = frac * (index < 8 ? 57.0f - (float)(index-1)*6.0f : -18.0f); }
const char *va(const char *fmt, ...) { return fmt; }

static void collision_world(trace_t *tr, const vec_t *start, const vec_t *end, const vec_t *maxs)
{
    memset(tr, 0, sizeof(*tr)); tr->fraction = 1; tr->entityNum = 1022; tr->normal[2] = 1;
    ++traceNumber;
    if (testWorld == 1) {
        /* A player walks over a four-unit drop, with no horizontal obstacle. */
        if (end[2] < start[2]) tr->fraction = (start[2] + 4.0f) / (start[2] - end[2]);
    } else if (testWorld == 2) {
        /* Synthetic mantle wall, ledge at 39, and clear over destination. */
        if (traceNumber == 1) { tr->fraction = 0.5f; tr->normal[0] = -1; tr->normal[2] = 0; tr->surfaceFlags = 0x4000000; }
        else if (traceNumber == 3) tr->fraction = (start[2] - 39.0f) / (start[2] - end[2]);
        else if (traceNumber == 5) { memcpy(overStart, start, sizeof(overStart)); memcpy(overEnd, end, sizeof(overEnd)); }
    }
    (void)maxs;
}

static void step_drop(void)
{
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.velocity[0] = 190; pm.xyspeed = 190; testWorld = 1; traceNumber = 0;
    PM_StepSlideMove(&pm, &pml, 0);
    close_float(ps.origin[2], -4); assert(lastEvent == 0x8f); assert(eventCount == 1);
    /* Four-unit drop contributes nine bob units, not the biased event byte. */
    assert(ps.bobCycle == 9);
    close_float(ps.velocity[0], 190.0f * ((1.0f - 4.0f / 18.0f) * 0.8f + 0.2f));
    puts("PASS step drop: ground adhesion, view event, slowdown and bob");
}

static void mantle(void)
{
    dvar_t enabled = {.current.enabled = 1}, disabled = {0}, range = {.current.value = 20}, radius = {.current.value = 0.1f}, angle = {.current.value = 60};
    mantle_enable = &enabled; mantle_debug = &disabled; mantle_check_range = &range; mantle_check_radius = &radius; mantle_check_angle = &angle;
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.maxs[0] = ps.maxs[1] = 15; ps.maxs[2] = 70; ps.mins[0] = ps.mins[1] = -15;
    pm.cmd.buttons = 0x400; testWorld = 2; traceNumber = 0;
    Mantle_Check(&pm, &pml);
    assert(pm.mantleStarted); assert(ps.pm_flags & 4); assert(ps.mantleState.transIndex == 3);
    close_float(overStart[0], overEnd[0]); close_float(overEnd[2], overStart[2] - 18);
    assert(ps.mantleState.flags & 1); assert(pm.mantleDuration == 200);
    for (int i = 0; i < 3; ++i) close_float(pm.mantleEndPos[i], i == 0 ? 47 : (i == 2 ? 21 : 0));
    pml.msec = 200; Mantle_Move(&pm, &ps, &pml); assert(!(ps.pm_flags & 4)); assert(!pm.mantleStarted);
    for (int i = 0; i < 3; ++i) close_float(ps.origin[i], pm.mantleEndPos[i]);
    puts("PASS mantle: wall/ledge, vertical over trace, transition and root motion");
}

int main(int argc, char **argv)
{
    fixtureTrace = collision_world;
    if (argc == 1 || !strcmp(argv[1], "step")) step_drop();
    if (argc == 1 || !strcmp(argv[1], "mantle")) mantle();
    return 0;
}
