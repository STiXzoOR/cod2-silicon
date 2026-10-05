#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

void Window_SetDynamicFlags(itemDef_t *item, int flags) { item->window.dynamicFlags[0] = flags; }
qboolean Item_EnableShowViaDvar(itemDef_t *item, int flags) { (void)item; (void)flags; return 1; }
#include "ui_text_color_source.h"

int main(void)
{
    menuDef_t menu = { .focusColor = { 1, 0.5f, 0.25f, 1 } };
    itemDef_t item = { .parent = &menu, .window.dynamicFlags = { 6 } };
    displayContextDef_t dc = {0};
    vec4_t color;
    /* A 250 Hz focused-item frame dump must follow Mac 1.3's 75 ms steps. */
    for (int time = 0; time < 2000; time += 4) {
        dc.realTime = time;
        Item_TextColor(&dc, &item, &color);
        assert(item.window.dynamicFlags[0] == 6);
        float red = 1.0f - 0.2f * (sinf((float)(time / 75)) * 0.5f + 0.5f);
        assert(fabsf(color[0] - red) < 0.000001f);
        assert(fabsf(color[1] - red * 0.5f) < 0.000001f);
    }
    puts("online: focused UI color uses the reference pulse cadence at 250 Hz passed");
}
