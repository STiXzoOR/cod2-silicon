#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <string.h>

extern const dvar_t *com_developer;

extern UI_Component_data_t __ZN12UI_Component1gE;
#define UI_Component_g __ZN12UI_Component1gE
extern void *imp_cls;
extern void *imp_re;
extern refexport_t re;
extern void *imp_com_developer;

void UI_Component_Init(void);

void UI_Component_Init(void)
{
    byte *globals;
    int uiStarted;

    globals = (byte *)imp_cls;

    uiStarted = ((clientStatic_t *)globals)->uiStarted;
    if (!uiStarted)
        return;

    memset(&UI_Component_g, 0, sizeof(UI_Component_data_t));

    UI_Component_g.screenWidth = (float)((clientStatic_t *)globals)->vidConfig.width;
    UI_Component_g.screenHeight = (float)((clientStatic_t *)globals)->vidConfig.height;

    UI_Component_g.charWidth = 8.0f;
    UI_Component_g.charHeight = 16.0f;
    UI_Component_g.scrollBarSize = 16.0f;

    {
        int dev = com_developer->current.integer;
        if (dev == 0)
            return;
    }

    {
        refexport_t *r = (refexport_t *)imp_re;
        UI_Component_g.cursor = r->RegisterMaterial("ui/assets/3_cursor3", 0x30, 1);
        UI_Component_g.filledCircle = r->RegisterMaterial("ui/assets/sliderbutt_1", 0x30, 1);
    }
}
