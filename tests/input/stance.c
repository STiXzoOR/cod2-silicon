#include "common_types.h"
#include "imports.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static clientActive_t client;
static clientActive_t *clientPointer = &client;
void *imp_cl = &clientPointer;
static LegacyHacks legacy;
LegacyHacks *legacyHacks = &legacy;
void *imp_legacyHacks = &legacyHacks;
struct clientStatic_t cls;
unsigned int frame_msec;
int com_frameTime;
void *imp_com_frameTime = &com_frameTime;
void *imp_frame_msec = &frame_msec;
static kbutton_t buttons[28];
static kbutton_t *kb = buttons;
static dvar_t holdTime, bypass, zero, one;
const dvar_t *cl_stanceHoldTime = &holdTime;
const dvar_t *cl_bypassMouseInput = &bypass;
const dvar_t *cl_sensitivity = &one, *cl_mouseAccel = &zero, *cl_showMouseRate = &zero;
const dvar_t *cl_freelook = &zero, *m_filter = &zero, *m_yaw = &zero, *m_pitch = &zero;
const dvar_t *m_side = &zero, *m_forward = &zero, *cl_yawspeed = &zero;
const dvar_t *cl_pitchspeed = &zero, *cl_anglespeedkey = &one;
static char keyArg[32], timeArg[32];
static int failures, groups;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); ++failures; } } while (0)

char *Cmd_Argv(int index)
{
    if (index == 1) return keyArg;
    if (index == 2) return timeArg;
    abort();
}
void Com_Printf(const char *format, ...) { (void)format; }
#include "stance_source.h"

static void reset(int stance)
{
    memset(&client, 0, sizeof(client));
    memset(&legacy, 0, sizeof(legacy));
    memset(buttons, 0, sizeof(buttons));
    memset(&cls, 0, sizeof(cls));
    memset(&bypass, 0, sizeof(bypass));
    legacy.cl_stance = stance;
    holdTime.current.integer = 300;
    one.current.value = 1.0f;
    frame_msec = 10;
    com_frameTime = 1000;
    client.serverTime = 1000;
    client.cgameSensitivity = 1.0f;
    snprintf(keyArg, sizeof(keyArg), "%d", 50);
    snprintf(timeArg, sizeof(timeArg), "%d", com_frameTime);
}

static void event(void (*handler)(void), int key, int time)
{
    snprintf(keyArg, sizeof(keyArg), "%d", key);
    snprintf(timeArg, sizeof(timeArg), "%d", time);
    handler();
}

static unsigned int stanceBits(void)
{
    usercmd_t cmd = CL_CreateCmd();
    return cmd.buttons & 0x2300;
}

static void held_stance(void)
{
    static const unsigned int persistent[] = {0, 0x200, 0x100};
    for (int stance = 0; stance < 3; ++stance) {
        reset(stance);
        event(IN_DownDown, 50, 1000);
        CHECK(legacy.cl_stanceTemp == 1);
        CHECK(stanceBits() == 0x2200);
        event(IN_Prone_Down, 51, 1000);
        CHECK(stanceBits() == 0x2100);
        event(IN_Prone_Up, 51, 1001);
        CHECK(stanceBits() == 0x2200);
        event(IN_DownUp, 50, 1001);
        CHECK(legacy.cl_stanceTemp == 0 && legacy.cl_stance == stance);
        CHECK(stanceBits() == persistent[stance]);
    }
}

static void stance_hold_timer(void)
{
    for (int stance = 0; stance < 3; ++stance) {
        reset(stance);
        IN_Stance_Down();
        CHECK(client.stanceHeld && client.stancePosition == stance && client.stanceTime == 1000);
        com_frameTime = 1299;
        CHECK(stanceBits() == 0x200 && client.stanceHeld);
        com_frameTime = 1300;
        CHECK(stanceBits() == (stance == 2 ? 0 : 0x100));
        CHECK(legacy.cl_stance == (stance == 2 ? 0 : 2) && !client.stanceHeld);
        IN_Stance_Up();
        CHECK(stanceBits() == (stance == 2 ? 0 : 0x100));
    }
    for (int delay = 0; delay <= 1000; delay += 1000) {
        reset(0);
        holdTime.current.integer = delay;
        IN_Stance_Down();
        com_frameTime += delay;
        CHECK(stanceBits() == 0x100 && !client.stanceHeld);
    }
}

static void timer_during_held_override(void)
{
    reset(0);
    IN_Stance_Down();
    event(IN_DownDown, 50, 1100);
    com_frameTime = 1400;
    CHECK(stanceBits() == 0x2200 && client.stanceHeld);
    IN_Stance_Up();
    CHECK(client.stanceHeld); /* Release is ignored while a held override is active. */
    event(IN_DownUp, 50, 1400);
    CHECK(stanceBits() == 0x100 && !client.stanceHeld);
}

static void stance_taps(void)
{
    for (int stance = 0; stance < 3; ++stance) {
        reset(stance);
        IN_Stance_Down();
        com_frameTime += 100;
        IN_Stance_Up();
        CHECK(legacy.cl_stance == (stance == 1 ? 0 : 1));
        CHECK(stanceBits() == (stance == 1 ? 0 : 0x200));
        CHECK(!client.stanceHeld);
    }
}

static void released_stance_press(void)
{
    reset(0);
    event(IN_Prone_Down, 50, 1000);
    event(IN_Prone_Up, 50, 1001);
    CHECK(buttons[25].wasPressed && !buttons[25].active);
    CHECK(stanceBits() == 0 && !buttons[25].wasPressed);
    /* KeyMove reconciles transient/stale command stance bits with current state. */
    for (int stance = 0; stance < 3; ++stance) {
        reset(stance);
        usercmd_t cmd = {0};
        cmd.buttons = 0x2301;
        CL_KeyMove(&cmd);
        CHECK((cmd.buttons & 0x2301) == (unsigned int)(1 | (stance == 1 ? 0x200 : stance == 2 ? 0x100 : 0)));
    }
}

static void raise_without_jumping(void)
{
    for (int type = 0; type <= 1; ++type) {
        for (int stance = 1; stance <= 2; ++stance) {
            reset(stance);
            client.snap.ps.pm_type = type;
            event(IN_UpDown, 50, 1000);
            usercmd_t cmd = CL_CreateCmd();
            CHECK(legacy.cl_stance == stance - 1);
            CHECK(!(cmd.buttons & 0x400));
            CHECK(buttons[12].wasPressed); /* Outside flying modes, leave this edge alone. */
            event(IN_UpUp, 50, 1001);
        }
    }
    reset(0);
    event(IN_UpDown, 50, 1000);
    CHECK(CL_CreateCmd().buttons & 0x400);
    event(IN_UpUp, 50, 1001);
    CHECK(!(CL_CreateCmd().buttons & 0x400));
}

static void flying_jump_range(void)
{
    for (int type = -1; type <= 8; ++type) {
        reset(0);
        client.snap.ps.pm_type = type;
        buttons[12].wasPressed = 1;
        usercmd_t cmd = {0};
        CL_CmdButtons(&cmd);
        int flying = type >= 2 && type <= 4;
        CHECK(!!(cmd.buttons & 0x400) == flying);
        CHECK(!!buttons[12].wasPressed == !flying);
    }
}

static void stance_handlers(void)
{
    reset(0);
    IN_LowerStance(); CHECK(legacy.cl_stance == 1);
    IN_LowerStance(); CHECK(legacy.cl_stance == 2);
    IN_LowerStance(); CHECK(legacy.cl_stance == 2);
    IN_RaiseStance(); CHECK(legacy.cl_stance == 1);
    IN_RaiseStance(); CHECK(legacy.cl_stance == 0);
    IN_ToggleProne(); CHECK(legacy.cl_stance == 2);
    IN_ToggleProne(); CHECK(legacy.cl_stance == 0);
    IN_ToggleCrouch(); CHECK(legacy.cl_stance == 1);
    IN_ToggleCrouch(); CHECK(legacy.cl_stance == 0);
    IN_GoProne(); CHECK(legacy.cl_stance == 2);
    IN_GoCrouch(); CHECK(legacy.cl_stance == 1);
    event(IN_GoStandDown, 50, 1000);
    CHECK(legacy.cl_stance == 0 && !buttons[10].active);
    event(IN_GoStandUp, 50, 1001);
    event(IN_GoStandDown, 50, 1002);
    CHECK(buttons[10].active);
    event(IN_GoStandUp, 50, 1003);
    event(IN_DownDown, 51, 1004);
    IN_GoProne(); IN_LowerStance(); IN_RaiseStance(); IN_ToggleProne(); IN_ToggleCrouch(); IN_Stance_Down();
    CHECK(legacy.cl_stance == 0 && !client.stanceHeld);
}

static void command_controls(void)
{
    reset(0);
    event(IN_ForwardDown, 50, 995);
    event(IN_ForwardDown, 51, 995);
    event(IN_ForwardUp, 50, 998);
    CHECK(buttons[2].active);
    usercmd_t cmd = CL_CreateCmd();
    CHECK(cmd.forwardmove == 63 && !cmd.rightmove);
    event(IN_ForwardUp, 51, 1001);
    CHECK(!buttons[2].active);
    for (int ads = 0; ads <= 1; ++ads) {
        for (int speed = 0; speed <= 1; ++speed) {
            reset(0);
            client.usingAds = ads;
            buttons[9].active = speed;
            CHECK(!!(CL_CreateCmd().buttons & 0x1000) == (speed == !ads));
            client.snap.ps.weaponstate = 19;
            CHECK(CL_CreateCmd().buttons & 0x1000);
        }
    }
    reset(0);
    client.keyCatchers = 8;
    client.cgameInShellshock = 1;
    client.serverTime = 6001;
    client.snap.serverTime = 1000;
    client.cgameUserCmdValue = 7;
    client.cgameUserHoldableValue = 9;
    client.viewangles[1] = 90;
    cmd = CL_CreateCmd();
    CHECK((cmd.buttons & 0x40800) == 0x40800);
    CHECK(cmd.serverTime == 6000 && cmd.weapon == 7 && cmd.offHandIndex == 9 && cmd.angles[1] == 16384);
    bypass.current.enabled = 1;
    CHECK(!(CL_CreateCmd().buttons & 0x40000));
}

int main(void)
{
#define RUN(test) do { int before = failures; test(); ++groups; \
    printf("%s: %s\n", #test, failures == before ? "PASS" : "FAIL"); } while (0)
    RUN(held_stance);
    RUN(stance_hold_timer);
    RUN(timer_during_held_override);
    RUN(stance_taps);
    RUN(released_stance_press);
    RUN(raise_without_jumping);
    RUN(flying_jump_range);
    RUN(stance_handlers);
    RUN(command_controls);
    printf("%d groups, %d failed assertions\n", groups, failures);
    return failures != 0;
}
