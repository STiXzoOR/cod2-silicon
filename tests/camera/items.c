#include "common_types.h"
#include "headers/PC/cgame_mp/cg_local.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Both symbols are linked from the generated production definitions. */
extern int bg_numItems;
extern void *imp_bg_numItems;
static gitem_t items[132];
static itemInfo_t visuals[132];
static itemInfo_t *visual_pointer = visuals;
void *imp_bg_itemlist = items;
void *imp_cg_items = &visual_pointer;
static jmp_buf error_target;
static int registered, rendered, errors;
static int expected_index;

void Com_Error(int code, const char *format, ...)
{
    char message[128];
    va_list args;
    assert(code == 1);
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    assert(strstr(message, "Bad item index 131 on entity"));
    errors++;
    longjmp(error_target, 1);
}

void CG_RegisterItemVisuals(int index)
{
    assert(index == expected_index);
    registered++;
}

struct DObj_s *CG_PreProcess_GetDObj(int number, int type, struct XModel *model)
{
    assert(number == 42 && type == 3);
    assert(model == (struct XModel *)1);
    return (struct DObj_s *)2;
}

void AnglesToAxis(const vec_t *angles, vec3_t *axis)
{
    (void)angles;
    memset(axis, 0, sizeof(vec3_t) * 3);
    axis[0][0] = axis[1][1] = axis[2][2] = 1;
}

void CG_AddCEntityToScene(GfxEntity *entity, struct DObj_s *object, centity_t *cent)
{
    assert(object == (struct DObj_s *)2);
    assert(cent->nextState.index.item == expected_index);
    assert(entity->origin[0] == 11 && entity->origin[1] == 12 && entity->origin[2] == 13);
    rendered++;
}

#include "item_source.h"

int main(void)
{
    centity_t cent;
    assert(imp_bg_numItems == &bg_numItems);
    memset(&cent, 0, sizeof(cent));
    cent.nextState.number = 42;
    cent.nextState.eType = 3;
    cent.lerpOrigin[0] = 11;
    cent.lerpOrigin[1] = 12;
    cent.lerpOrigin[2] = 13;
    for (int index = 6; index <= 130; index += 124) {
        expected_index = index;
        cent.nextState.index.item = index;
        visuals[index].registered = 1;
        visuals[index].models[0] = (struct XModel *)1;
        if (!setjmp(error_target))
            CG_Item(&cent);
        else
            assert(!"valid dropped item disconnected");
    }
    assert(rendered == 2 && errors == 0);
    assert(bg_numItems == 131);
    expected_index = 6;
    cent.nextState.index.item = 6;
    visuals[6].registered = 0;
    CG_Item(&cent);
    assert(registered == 1 && rendered == 2);
    cent.nextState.eFlags = 0x20;
    CG_Item(&cent);
    assert(registered == 1 && rendered == 2);
    /* Bounds checking also precedes the hidden-item shortcut in Mac. */
    cent.nextState.index.item = 131;
    if (!setjmp(error_target)) {
        CG_Item(&cent);
        assert(!"terminal item index must disconnect");
    }
    assert(errors == 1 && rendered == 2);
    puts("PASS: generated item count/import, dropped items 6/130, registration, hidden item, bound 131");
    return 0;
}
