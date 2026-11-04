#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>

#include "PC/gfx_d3d/rb_state.c"

struct DxState dxState;
r_backEndGlobals_t backEnd;
DxGlobals dx;
int alwaysfails;
static int calls;
static D3DTEXTURESTAGESTATETYPE states[8];
static DWORD values[8];
static void *vtable[128];
static struct { void **vtable; } device = { vtable };

static HRESULT D3DVTCC SetTextureStageState(void *dev, DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)
{
    assert(dev == &device && stage == 0 && calls < 8);
    states[calls] = type;
    values[calls++] = value;
    return 0;
}

int main(void)
{
    vtable[0x10c / 4] = (void *)SetTextureStageState;
    dx.device = (void (*)())&device;

    /* Mac 1.3 .data holds {ALPHAOP 4, ALPHAARG1 5, ALPHAARG2 6, ALPHAARG0 27}
       and {COLOROP 1, COLORARG1 2, COLORARG2 3, COLORARG0 26}. */
    /* Op 21 (D3DTOP_LERP, three arguments) with argument codes 1, 2 and 5. */
    RB_ChangeAlphaStageState(0, 21 | (1 << 10) | (2 << 15) | (5 << 20));
    assert(calls == 4);
    assert(states[0] == 4 && values[0] == 25);
    assert(states[1] == 5 && values[1] == s_textureArgTable[1]);
    assert(states[2] == 6 && values[2] == s_textureArgTable[2]);
    assert(states[3] == 27 && values[3] == s_textureArgTable[5]);

    /* Op 1 (D3DTOP_SELECTARG1) uses only its first argument. */
    calls = 0;
    RB_ChangeColorStageState(0, 1 | (5 << 10));
    assert(calls == 2);
    assert(states[0] == 1 && values[0] == 2);
    assert(states[1] == 2 && values[1] == s_textureArgTable[5]);
    puts("native texture stages: Mac alpha/color stage enums reach SetTextureStageState");
}
