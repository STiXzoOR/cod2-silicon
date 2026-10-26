/* Mac 1.3 Pmove's spectator branch uses scalar SSE rounding throughout. */
static void test_spectator_vertical(void)
{
    static const struct {
        int speed, flags, buttons, forward, right;
        float scale, lean;
        unsigned int velocity[3];
    } cases[] = {
        {400, 0x100, 0x80, 0, 0, 1.3f, 0, {0, 0, 0x42a66665}},
        {400, 0x100, 0x40, 0, 0, 1.3f, 0, {0, 0, 0xc2a66665}},
        {400, 0, 0x80, 0, 0, 1.3f, 0.25f, {0, 0, 0x42a66665}},
        {400, 0, 0x40, 0, 0, 1.3f, -0.25f, {0, 0, 0xc2a66665}},
        {400, 0x100, 0x80, 127, 64, 1.3f, 0, {0x42949912, 0xc215c49c, 0x42a66665}},
        {400, 0x100, 0x40, 127, 64, 1.3f, 0, {0x42949912, 0xc215c49c, 0xc2a66665}},
        {400, 0, 0x80, 0, 0, 1.3f, 0, {0, 0, 0x43500000}},
        {400, 0, 0x80, 0, 0, 2.0f, 0, {0, 0, 0x43a00000}},
        {400, 0x100, 0xc0, 0, 0, 1.3f, 0, {0, 0, 0}},
        {0, 0x100, 0x80, 0, 0, 1.3f, 0, {0, 0, 0}},
        {190, 0x100, 0x80, 0, 0, 1.3f, 0, {0, 0, 0x42200000}},
    };
    int failures = 0;
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        playerState_t ps; pmove_t pm; pml_t pml;
        init(&ps, &pm, &pml, 60);
        ps.pm_type = 4;
        ps.speed = cases[i].speed;
        ps.pm_flags = cases[i].flags;
        ps.leanf = cases[i].lean;
        pm.cmd.buttons = cases[i].buttons;
        pm.cmd.forwardmove = cases[i].forward;
        pm.cmd.rightmove = cases[i].right;
        pml.walking = pml.groundPlane = 0;
        pml.frametime = 0.05f;
        fixture_player_spectateSpeedScale.current.value = cases[i].scale;
        PM_FlyMove(&pm, &pml);
        unsigned int actual[3];
        memcpy(actual, ps.velocity, sizeof(actual));
        if (memcmp(actual, cases[i].velocity, sizeof(actual))) {
            fprintf(stderr, "spectator vertical case %u: expected %08x %08x %08x, got %08x %08x %08x\n",
                    i, cases[i].velocity[0], cases[i].velocity[1], cases[i].velocity[2],
                    actual[0], actual[1], actual[2]);
            ++failures;
        }
        assert(slideCount == 1);
    }
    assert(failures == 0);
    puts("PASS spectator vertical: 11 exact Mac arithmetic cases");
}

static void test_spectator_horizontal(void)
{
    /* Binary32 calculations at two yaw/command inputs; no Mac runtime claim. */
    const unsigned int yaw[] = {0x42b0b170, 0x430558b8};
    const unsigned int velocity[][3] = {
        {0x4307b3c7, 0xc390e696, 0}, {0x42b6dad8, 0xc399549e, 0},
    };
    for (int i = 0; i < 2; ++i) {
        playerState_t ps; pmove_t pm; pml_t pml;
        init(&ps, &pm, &pml, 60);
        ps.pm_type = 4;
        ps.speed = 400;
        memcpy(&ps.viewangles[1], &yaw[i], sizeof(float));
        AngleVectors(ps.viewangles, pml.forward, pml.right, pml.up);
        pm.cmd.forwardmove = -127;
        pm.cmd.rightmove = i ? -64 : 64;
        pml.walking = pml.groundPlane = 0;
        pml.frametime = 0.05f;
        fixture_player_spectateSpeedScale.current.value = 2.0f;
        PM_FlyMove(&pm, &pml);
        unsigned int actual[3];
        memcpy(actual, ps.velocity, sizeof(actual));
        assert(!memcmp(actual, velocity[i], sizeof(actual)));
        assert(slideCount == 1);
    }
    puts("PASS spectator horizontal: two exact binary32 controls");
}
