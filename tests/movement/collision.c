#define MOVEMENT_COLLISION
#define MOVEMENT_JUMP
#define main unused_fixture_main
#include "movement.c"
#undef main
#include "slidemove_source.h"
#include "mantle_source.h"
#include "jump_source.h"

static int traceNumber, testWorld;
static vec3_t overStart, overEnd;
static int animationCreates;
static int animationStorage[16];
void *Hunk_AllocXAnimServer(int size) { assert(size <= (int)sizeof(animationStorage)); return animationStorage; }
void *Hunk_AllocAlignInternal(int size, int align) { (void)align; return Hunk_AllocXAnimServer(size); }
void *XAnimCreateAnims(const char *name, int count, void *alloc)
{ assert(!strcmp(name, "PLAYER_MANTLE") && count == 11); ++animationCreates; return ((void *(*)(int))alloc)(16); }
void XAnimBlend(XAnim *anims, unsigned int index, const char *name, unsigned int children, unsigned int count, unsigned int flags)
{ (void)name; (void)flags; assert(anims && !index && children == 1 && count == 10); }
XAnimParts *XAnimPrecache(const char *name, Alloc_t alloc) { assert(name && alloc); return (XAnimParts *)alloc(16); }
void XAnimCreate(struct XAnim_s *anims, unsigned int index, const char *name) { assert(anims && index && name); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; assert(0); }

int XAnimGetLengthMsec(const struct XAnim_s *anims, unsigned int index) { assert(anims); (void)index; return 100; }
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
    } else if (testWorld == 3) {
        tr->fraction = 0.5f; tr->surfaceFlags = (7 << 20) | ladderSurface;
    } else if (testWorld == 4) {
        tr->surfaceFlags = 7 << 20;
        if (end[2] < start[2] && end[2] <= 0) {
            tr->fraction = fmaxf(0, start[2] / (start[2] - end[2]));
        }
    }
    (void)maxs;
}

static void jump(void)
{
    static dvar_t height = {.current.value = 39}, step = {.current.value = 18}, slowdown = {.current.enabled = 1}, push = {.current.value = 128}, spread = {.current.value = 64};
    jump_height = &height; jump_stepSize = &step; jump_slowdownEnable = &slowdown; jump_ladderPushVel = &push; jump_spreadAdd = &spread;
    playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
    ps.gravity = 800; ps.pm_flags = 0x20; ps.vLadderVec[0] = -1; pm.cmd.serverTime = 1000; pm.cmd.buttons = 0x400;
    pml.forward[0] = 0.6f; pml.forward[1] = 0.6f; pml.forward[2] = -sqrtf(0.28f);
    assert(Jump_Check(&pm, &pml)); close_float(ps.velocity[0], -128.0f / sqrtf(2)); close_float(ps.velocity[1], 128.0f / sqrtf(2));
    close_float(ps.velocity[2], sqrtf(39.0f * 1600.0f) * 0.75f);
    for (int t = 0; t <= 1801; ++t) {
        ps.pm_flags = 0x80000; ps.pm_time = t;
        close_float(Jump_ReduceFriction(&ps), t > 1800 ? 1 : (t >= 1700 ? 2.5f : (float)t * 1.5f * 0.0005882352706976235f + 1));
    }
    puts("PASS jump: flat ladder reflection and 1802 slowdown timer cases");
}

static void landing(void)
{
    testWorld = 3;
    for (int height = 1; height <= 320; ++height) for (int noSteps = 0; noSteps < 2; ++noSteps) {
        playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
        ps.gravity = 800; ps.groundEntityNum = 1023; ps.velocity[0] = 190;
        pml.previous_origin[2] = (float)height; pml.previous_velocity[2] = 0;
        ladderSurface = noSteps ? 0x2000 : 0; traceNumber = 0;
        float t = (float)(-sqrt((double)(1600.0f * (float)height)) / -800.0);
        float fallHeight = t * 800.0f; fallHeight = fallHeight * fallHeight / 1600.0f;
        int damage = fallHeight > 128 ? (fallHeight >= 300 ? 100 : (int)((fallHeight - 128) / 172 * 100)) : 0;
        float factor = 1; int event = 0;
        if (damage) {
            int stun = damage * 35 + 500; if (stun > 2000) stun = 2000;
            factor = damage >= 100 ? 0.67f : (stun >= 1500 ? 0.2f : 0.5f + ((float)stun - 500.0f) / 1000.0f * -0.3f);
            event = 116 + (noSteps ? 0 : 7);
        } else if (fallHeight >= 12) { factor = 0.67f; event = 93 + (noSteps ? 0 : 7); }
        else if (fallHeight > 4 && !noSteps) event = 7 + (fallHeight < 8 ? 24 : 1);
        PM_GroundTrace(&pm, &pml);
        close_float(ps.velocity[0], 190 * factor);
        assert(eventCount == (fallHeight > 4)); if (eventCount) assert(lastEvent == event);
        assert(ps.groundEntityNum == 1022 && pml.walking);
    }
    ladderSurface = 0;
    puts("PASS landing: 640 fall heights/surfaces, events and proportional stun");
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
    static dvar_t enabled = {.current.enabled = 1}, disabled = {0}, range = {.current.value = 20}, radius = {.current.value = 0.1f}, angle = {.current.value = 60};
    mantle_enable = &enabled; mantle_debug = &disabled; mantle_check_range = &range; mantle_check_radius = &radius; mantle_check_angle = &angle;
    /* Execute the production server animation setup: a listen client relies on it. */
#undef COD2_DEBUG_ONLY
#define COD2_DEBUG_ONLY(...)
#include "server_anim_init_source.h"
#undef COD2_DEBUG_ONLY
    assert(s_mantleAnims && animationCreates == 1);
    Mantle_CreateAnims(Hunk_AllocXAnimServer);
    assert(animationCreates == 1);
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

static unsigned int randomState = 0x43;
static unsigned int next_random(void) { randomState = randomState * 1664525u + 1013904223u; return randomState; }
#ifdef MOVEMENT_REFERENCE
extern void ws43_reference_single(void *pm);
#endif
static void frames(void)
{
    static dvar_t disabled = {0}; mantle_enable = &disabled;
    testWorld = 4; ladderSurface = 0;
    int divergences = 0;
    for (int i = 0; i < 20000; ++i) {
        playerState_t predicted, server; pmove_t clientMove, serverMove; pml_t pml;
        const int heights[] = {60, 40, 11}; int h = (int)(next_random() % 3);
        init(&predicted, &clientMove, &pml, heights[h]);
        predicted.gravity = 800; predicted.pm_flags = h == 2 ? 1 : (h == 1 ? 2 : 0);
        predicted.pm_type = (int)(next_random() % 7);
        if (next_random() & 1) predicted.pm_flags |= 0x8000;
        predicted.origin[2] = (float)(next_random() % 64);
        predicted.bobCycle = (int)(next_random() & 255);
        for (int a = 0; a < 3; ++a) predicted.velocity[a] = (float)((int)(next_random() % 401) - 200);
        int ms = (int)(next_random() % 16) + 1;
        predicted.commandTime = 1000 - ms;
        clientMove.cmd.serverTime = 1000; clientMove.oldcmd.serverTime = predicted.commandTime;
        clientMove.cmd.forwardmove = (byte)(next_random() % 255 - 127);
        clientMove.cmd.rightmove = (byte)(next_random() % 255 - 127);
        clientMove.cmd.buttons = next_random() & 0x2cc0;
        clientMove.cmd.angles[1] = (int)(next_random() & 65535);
        clientMove.tracemask = 0x2810011;
        server = predicted; serverMove = clientMove; serverMove.ps = &server; serverMove.handler = 1;
        PmoveSingle(&clientMove);
#ifdef MOVEMENT_REFERENCE
        ws43_reference_single(&serverMove);
#else
        PmoveSingle(&serverMove);
#endif
        if (memcmp(&predicted, &server, sizeof(predicted))) {
            if (divergences < 20) {
                fprintf(stderr, "divergence case=%d type=%d flags=%x msec=%d cmd=%d,%d buttons=%x offsets:", i, predicted.pm_type, predicted.pm_flags, ms, clientMove.cmd.forwardmove, clientMove.cmd.rightmove, clientMove.cmd.buttons);
                for (size_t b = 0; b < sizeof(predicted); ++b) if (((byte *)&predicted)[b] != ((byte *)&server)[b]) fprintf(stderr, " %zu(%02x/%02x)", b, ((byte *)&predicted)[b], ((byte *)&server)[b]);
                fputc('\n', stderr);
            }
            ++divergences;
        }
        for (int a = 0; a < 3; ++a) assert(isfinite(predicted.origin[a]) && isfinite(predicted.velocity[a]));
        assert(predicted.commandTime == 1000);
    }
    printf("20000 randomized PmoveSingle states: %d divergences on a synthetic floor\n", divergences);
    assert(!divergences);
}

static void backward_frames(void)
{
    /* A held backward command cannot instantly reverse existing velocity.
       Stock Mac rotates movementDir for the command; CoD2x >=3 uses actual
       travel direction. Run the complete movement/collision dispatch. */
    static dvar_t disabled = {0}; mantle_enable = &disabled;
    testWorld = 4; ladderSurface = 0;
    playerState_t results[2];
    int fixed = 0;
#if defined(COD2_CODX) && COD2_CODX
    fixed = Cod2x_GameVersion() >= 3;
#endif
    for (int handler = 0; handler < 2; ++handler) {
        playerState_t ps; pmove_t pm; pml_t pml; init(&ps, &pm, &pml, 60);
        ps.velocity[0] = 190; ps.gravity = 800; ps.commandTime = 1000;
        pm.cmd.serverTime = 1003; pm.oldcmd.serverTime = 1000;
        pm.cmd.forwardmove = -127; pm.handler = handler; pm.tracemask = 0x2810011;
        PmoveSingle(&pm);
        close_float(ps.origin[0], (190.0f - 190.0f * 5.5f * 0.003f - 133.0f * 9.0f * 0.003f) * 0.003f);
        close_float(ps.origin[1], 0); close_float(ps.origin[2], 0);
        assert(ps.velocity[0] == 183 && ps.velocity[1] == 0 && ps.velocity[2] == 0);
        assert(ps.movementDir == (fixed ? 0 : 90) && ps.commandTime == 1003);
        results[handler] = ps;
    }
    assert(!memcmp(&results[0], &results[1], sizeof(results[0])));
    puts("PASS full PmoveSingle backward-command displacement and independent velocity/yaw expectations");
}

int main(int argc, char **argv)
{
#ifdef GAMEPLAY_MATRIX
    Fixture_SetRevision(argc == 3 ? atoi(argv[2]) : 3);
#endif
    fixtureTrace = collision_world;
    int all = argc == 1 || !strcmp(argv[1], "all");
    if (all || !strcmp(argv[1], "step")) step_drop();
    if (all || !strcmp(argv[1], "mantle")) mantle();
    if (all || !strcmp(argv[1], "jump")) jump();
    if (all || !strcmp(argv[1], "landing")) landing();
    if (all || !strcmp(argv[1], "frames")) frames();
    if (all || !strcmp(argv[1], "backward")) backward_frames();
    return 0;
}
