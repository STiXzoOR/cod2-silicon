#include "macos_input_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(!MacInput_UseRaw(0, 2, 1, 1, 1, 1));
    assert(MacInput_UseRaw(1, 0, 1, 1, 1, 1));
    assert(!MacInput_UseRaw(1, 1, 1, 1, 1, 1));
    assert(!MacInput_UseRaw(1, 2, 1, 1, 1, 1));
    assert(!MacInput_UseRaw(1, 0, 0, 1, 1, 1));
    assert(!MacInput_UseRaw(1, 0, 1, 0, 1, 1));
    assert(!MacInput_UseRaw(1, 0, 1, 1, 0, 1));
    assert(!MacInput_UseRaw(1, 0, 1, 1, 1, 0));
    float remainder = 0;
    assert(MacInput_WheelSteps(&remainder, .25f) == 0);
    assert(MacInput_WheelSteps(&remainder, .25f) == 0);
    assert(MacInput_WheelSteps(&remainder, .25f) == 0);
    assert(MacInput_WheelSteps(&remainder, .25f) == 1);
    assert(MacInput_WheelSteps(&remainder, -2.5f) == -2);
    assert(MacInput_WheelSteps(&remainder, -.5f) == -1);
    int x, y;
    MacInput_RenderToUI(932, 18, 1280, 720, &x, &y);
    assert(x == 466 && y == 12);
    MacInput_RenderToUI(960, 540, 1920, 1080, &x, &y);
    assert(x == 320 && y == 240);
    MacInput_RenderToUI(400, 300, 800, 600, &x, &y);
    assert(x == 320 && y == 240);
    MacInput_RenderToUI(10, 20, 0, 0, &x, &y);
    assert(x == 0 && y == 0);
    puts("PASS system menus/console, raw gameplay, fractional wheel, absolute UI coordinates");
}
