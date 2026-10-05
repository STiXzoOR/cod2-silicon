#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static itemDef_t target;
static const char *property;
static int token, component;
qboolean String_Parse(const char **args, char *out, int size)
{
    (void)args;
    snprintf(out, size, "%s", token++ ? property : "header");
    return 1;
}
qboolean Float_Parse(const char **args, float *out)
{
    (void)args;
    *out = ++component * 0.125f;
    return 1;
}
int Menu_ItemsMatchingGroup(menuDef_t *menu, const char *name) { (void)menu; (void)name; return 1; }
itemDef_t *Menu_GetMatchingItemByNumber(menuDef_t *menu, int index, const char *name)
{ (void)menu; (void)name; assert(index == 0); return &target; }
int I_stricmp(const char *a, const char *b) { return strcmp(a, b); }
void Window_AddDynamicFlags(itemDef_t *item, int flags) { item->window.dynamicFlags[0] |= flags; }
#include "ui_item_color_source.h"

int main(void)
{
    itemDef_t source = {0};
    const char *args = "";
    const char *names[] = { "backcolor", "forecolor", "bordercolor" };
    for (int i = 0; i < 3; ++i) {
        memset(&target, 0, sizeof(target));
        itemDef_t expected = target;
        float *colors[] = { expected.window.backColor, expected.window.foreColor, expected.window.borderColor };
        for (int j = 0; j < 4; ++j) colors[i][j] = (j + 1) * 0.125f;
        if (i == 1) expected.window.dynamicFlags[0] |= 0x10000;
        property = names[i]; token = component = 0;
        Script_SetItemColor(NULL, &source, &args);
        assert(!memcmp(&target, &expected, sizeof(target)));
    }
    puts("online: menu script colors preserve adjacent native fields passed");
}
