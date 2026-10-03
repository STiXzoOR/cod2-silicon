#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>

#include "PC/gfx_d3d/r_rendercmds.c"

GfxCmdArray *s_cmdList;
static dvar_t specularScale;
const dvar_t *r_specularColorScale = &specularScale;

void R_ConvertColorToBytes(const vec_t *color, byte *bytes)
{
    int i;
    for (i = 0; i < 4; ++i)
        bytes[i] = (byte)(color[i] * 255);
}

_Static_assert(offsetof(GfxCmdCall, subCmd) == 8, "STABS i386 subCmd at 4 gains native pointer alignment");
_Static_assert(sizeof(GfxCmdSetLightProperties) == 80, "STABS 76 grows by one pointer");
_Static_assert(sizeof(GfxCmdBeginView) == 64, "STABS 48: sceneDef and viewParms pointers grow");

int main(void)
{
    static GfxCmdArray list;
    GfxLight light = { 0 };
    GfxLightDef definition = { 0 };
    GfxSceneDef sceneDef = { 0 };
    GfxLodParms lod = { 0 };
    GfxViewParms view = { 0 };
    GfxCmdSetLightProperties *lightCmd;
    GfxCmdBeginView *viewCmd;
    GfxCmdCall *call;

    s_cmdList = &list;
    specularScale.current.value = 1;
    R_AllocCmd(4, 0, 0x19);
    light.def = &definition;
    R_AddCmdLightProperties(0, &light);
    lightCmd = (void *)(list.cmds + 8);
    assert(lightCmd->header.byteCount == sizeof(*lightCmd));
    assert(lightCmd->lightDef == &definition);
    R_AddCmdBeginView(3, &sceneDef, &view, &lod);
    viewCmd = (void *)(list.cmds + 8 + sizeof(*lightCmd));
    assert(viewCmd->header.byteCount == sizeof(*viewCmd));
    assert(viewCmd->viewParms == &view && viewCmd->viewCount == 3);
    call = R_AllocDelayedCall(1, NULL);
    assert(call && ((uintptr_t)call & 7) == 0);
    call->subCmd = lightCmd;
    assert(call->subCmd == lightCmd);
    assert(list.cmds[0] == 0x19 && ((GfxCmdHeader *)list.cmds)->byteCount == 8);
    assert(list.usedTotal == 8 + sizeof(*lightCmd) + sizeof(*viewCmd) + sizeof(*call));
    assert(!R_AllocCmd(65529, 0, 3));
    puts("renderer mixed command sizes, traversal and pointer alignment: passed");
    return 0;
}
