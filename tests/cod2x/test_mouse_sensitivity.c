#include "../fidelity/dvar_services.h"
#include <float.h>
#include <math.h>

static clientActive_t client;
static clientActive_t *clientPointer = &client;
void *imp_cl = &clientPointer;
static LegacyHacks legacy;
LegacyHacks *legacyHacks = &legacy;
struct clientStatic_t cls;
unsigned int frame_msec;
int com_frameTime;
static kbutton_t buttons[28];
static kbutton_t *kb = buttons;
const dvar_t *cl_analog_attack_threshold, *cl_stanceHoldTime, *cl_nodelta;
const dvar_t *cl_sensitivity, *cl_mouseAccel, *cl_showMouseRate, *cl_freelook;
const dvar_t *m_filter, *m_yaw, *m_pitch, *m_side, *m_forward;
const dvar_t *cl_yawspeed, *cl_pitchspeed, *cl_anglespeedkey, *cl_bypassMouseInput;
extern void Dvar_SetFloat(const dvar_t *, float);
extern void Dvar_SetInt(const dvar_t *, int);

void Cmd_AddCommand(const char *name, void (*handler)(void)) { (void)name; (void)handler; }
const dvar_t *Dvar_RegisterBool_mac(const char *name, int value, int flags)
{
    return Dvar_RegisterBool(name, value, flags);
}
Bool PM_IsBinocularsADS(const playerState_t *ps) { (void)ps; return 0; }
void UI_MouseEvent(int dx, int dy) { (void)dx; (void)dy; }

#include "mouse_sensitivity_source.h"

static void angles(const char *label, float yaw, float pitch)
{
    if (fabsf(client.viewangles[1] - yaw) > 0.00001f ||
        fabsf(client.viewangles[0] - pitch) > 0.00001f) {
        fprintf(stderr, "%s: yaw %.9g expected %.9g; pitch %.9g expected %.9g\n",
                label, client.viewangles[1], yaw, client.viewangles[0], pitch);
        ++failures;
    }
}

static void reset(int flags, int dx, int dy)
{
    memset(&client, 0, sizeof(client));
    memset(buttons, 0, sizeof(buttons));
    memset(&legacy, 0, sizeof(legacy));
    memset(&cls, 0, sizeof(cls));
    client.snap.ps.eFlags = flags;
    client.cgameSensitivity = 1.0f;
    frame_msec = 10;
    com_frameTime = 10;
    Dvar_SetFloat(cl_sensitivity, 5.0f);
    Dvar_SetFloat(cl_mouseAccel, 0.0f);
    Dvar_SetBool(m_filter, 0);
    Dvar_SetBool(cl_freelook, 1);
    /* Config/console input goes through the real dvar implementation. */
    Dvar_SetCommand("sensitivity_MG", "1");
    CL_MouseEvent(dx, dy);
}

static void register_values(void)
{
    cl_sensitivity = Dvar_RegisterFloat("sensitivity", 5, 0.01f, 100, 0x1001);
    cl_mouseAccel = Dvar_RegisterFloat("cl_mouseAccel", 0, 0, 100, 0x1001);
    cl_showMouseRate = Dvar_RegisterBool("cl_showmouserate", 0, 0);
    cl_freelook = Dvar_RegisterBool("cl_freelook", 1, 0x1001);
    m_filter = Dvar_RegisterBool("m_filter", 0, 0x1001);
    m_yaw = Dvar_RegisterFloat("m_yaw", 0.022f, -100, 100, 0x1001);
    m_pitch = Dvar_RegisterFloat("m_pitch", 0.022f, -100, 100, 0x1001);
    m_side = Dvar_RegisterFloat("m_side", 0.25f, -100, 100, 0x1001);
    m_forward = Dvar_RegisterFloat("m_forward", 0.25f, -100, 100, 0x1001);
    cl_yawspeed = Dvar_RegisterFloat("cl_yawspeed", 140, -1000, 1000, 0x1001);
    cl_pitchspeed = Dvar_RegisterFloat("cl_pitchspeed", 140, -1000, 1000, 0x1001);
    cl_anglespeedkey = Dvar_RegisterFloat("cl_anglespeedkey", 1.5f, 0, 10, 0);
    cl_bypassMouseInput = Dvar_RegisterBool("cl_bypassMouseInput", 0, 0);
}

int main(void)
{
    Dvar_Init();
    CL_InitInput();
    const dvar_t *mg = Dvar_FindVar("sensitivity_MG");
#if defined(COD2_CODX) && COD2_CODX
    CHECK(mg && mg->type == 1 && mg->flags == 0x1001);
    CHECK(mg && mg->current.value == 1.0f && mg->reset.value == 1.0f);
    CHECK(mg && mg->domain.value.min == 0.01f && mg->domain.value.max == 100.0f);
#else
    CHECK(!mg);
#endif
    register_values();
    usercmd_t cmd = {0};
    reset(0x100, 3, 4);
    CL_MouseMove(&cmd);
    angles("MG default", -0.165f, 0.176f);

    reset(0x200, 3, 4);
    Dvar_SetCommand("sensitivity_MG", "2");
    Dvar_SetFloat(cl_sensitivity, 17);
    client.cgameSensitivity = 0.25f;
    CL_MouseMove(&cmd);
#if defined(COD2_CODX) && COD2_CODX
    angles("MG configured sensitivity ignores regular sensitivity and zoom", -0.33f, 0.352f);
#else
    angles("stock MG ignores CoD2x setting", -0.165f, 0.176f);
#endif

    reset(0x300, 3, 4);
    Dvar_SetFloat(cl_mouseAccel, 2);
    Dvar_SetCommand("sensitivity_MG", "2");
    client.cgameSensitivity = 0.1f;
    CL_MouseMove(&cmd);
#if defined(COD2_CODX) && COD2_CODX
    /* Speed 5 px / 10 ms; additive sensitivity (0.5 * 2) + 2 = 3. */
    angles("MG acceleration", -0.495f, 0.528f);
#else
    angles("stock MG acceleration remains fixed", -0.165f, 0.176f);
#endif

    reset(0, 3, 4);
    Dvar_SetFloat(cl_mouseAccel, 2);
    Dvar_SetCommand("sensitivity_MG", "8");
    client.cgameSensitivity = 0.25f;
    CL_MouseMove(&cmd);
    angles("ordinary acceleration and zoom", -0.099f, 0.132f);

    reset(0x100, 10, -8);
    Dvar_SetCommand("sensitivity_MG", "2");
    Dvar_SetBool(m_filter, 1);
    client.mouseDx[1] = 2;
    client.mouseDy[1] = 4;
    CL_MouseMove(&cmd);
#if defined(COD2_CODX) && COD2_CODX
    angles("MG filtering", -0.66f, -0.176f);
#else
    angles("stock filtering", -0.33f, -0.088f);
#endif
    CHECK(client.mouseIndex == 1 && client.mouseDx[1] == 0 && client.mouseDy[1] == 0);

    reset(0x100, 3, 4);
    frame_msec = 0;
    client.mouseDx[1] = 9;
    client.mouseDy[1] = 11;
    CL_MouseMove(&cmd);
    angles("zero-frame consumes buffer without movement", 0, 0);
    CHECK(client.mouseIndex == 1 && !client.mouseDx[1] && !client.mouseDy[1]);
    reset(0x100, 3, 4);
    client.snap.ps.pm_flags = 0x8000;
    CL_MouseMove(&cmd);
    angles("restricted player state", 0, 0);

    reset(0x100, 30, -40);
    Dvar_SetCommand("sensitivity_MG", "4");
    client.cgameMaxYawSpeed = 10;
    client.cgameMaxPitchSpeed = 20;
    CL_MouseMove(&cmd);
    angles("MG angular speed limits", -0.1f, -0.2f);

    reset(0x100, 3, 4);
    Dvar_SetCommand("sensitivity_MG", "2");
    buttons[8].active = 1;
    cmd.rightmove = 10;
    cmd.forwardmove = 10;
    CL_MouseMove(&cmd);
#if defined(COD2_CODX) && COD2_CODX
    CHECK(cmd.rightmove == 13 && cmd.forwardmove == 6);
#else
    CHECK(cmd.rightmove == 11 && cmd.forwardmove == 8);
#endif
    angles("strafe maps both mouse axes to movement", 0, 0);
    reset(0x100, 1000, -1000);
    buttons[8].active = 1;
    cmd.rightmove = cmd.forwardmove = 0;
    CL_MouseMove(&cmd);
    CHECK(cmd.rightmove == 127 && cmd.forwardmove == 127);
    reset(0x100, -1000, 1000);
    buttons[8].active = 1;
    CL_MouseMove(&cmd);
    CHECK(cmd.rightmove == -128 && cmd.forwardmove == -128);

    reset(0x100, 3, 4);
    ((dvar_t *)cl_mouseAccel)->current.value = NAN;
    ((dvar_t *)cl_sensitivity)->current.value = NAN;
    client.cgameSensitivity = NAN;
#if defined(COD2_CODX) && COD2_CODX
    if (mg)
        ((dvar_t *)mg)->current.value = NAN;
#endif
    CL_MouseMove(&cmd);
    angles("native finite defaults", -0.165f, 0.176f);

    reset(0x100, 3, 4);
    Dvar_SetCommand("sensitivity_MG", "2");
    client.serverTime = 50;
    client.cgameUserCmdValue = 7;
    client.cgameUserHoldableValue = 8;
    cmd = CL_CreateCmd();
#if defined(COD2_CODX) && COD2_CODX
    angles("CL_CreateCmd dispatches MG scaling", -0.33f, 0.352f);
    CHECK(cmd.angles[0] == 64 && cmd.angles[1] == 65476);
#else
    angles("stock CL_CreateCmd", -0.165f, 0.176f);
    CHECK(cmd.angles[0] == 32 && cmd.angles[1] == 65506);
#endif
    CHECK(cmd.serverTime == 50 && cmd.weapon == 7 && cmd.offHandIndex == 8);

#if defined(COD2_CODX) && COD2_CODX
    cod2x_game = Dvar_RegisterInt("g_cod2x", 0, 0, 6, 0x18);
    for (int version = 0; version <= 6; ++version) {
        Dvar_SetInt(cod2x_game, version);
        CHECK(Cod2x_GameVersion() == version);
        reset(0x100, 3, 4);
        Dvar_SetCommand("sensitivity_MG", "2");
        CL_MouseMove(&cmd);
        angles("MG setting applies in every server revision state", -0.33f, 0.352f);
    }
#endif
    Dvar_Shutdown();
    Dvar_Init();
    Dvar_SetCommand("sensitivity_MG", "2.5");
    CL_InitInput();
    mg = Dvar_FindVar("sensitivity_MG");
#if defined(COD2_CODX) && COD2_CODX
    CHECK(mg && mg->type == 1 && mg->current.value == 2.5f && mg->reset.value == 1.0f);
#else
    CHECK(mg && mg->type == 7); /* Stock leaves an external config name untouched. */
#endif
    Dvar_Shutdown();
    FINISH("native MG sensitivity registration, mouse, command and revision contracts");
}
