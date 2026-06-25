#include "common_types.h"
#include "PC/win32/gfx_dll_v60.h"

#if defined(COD2_GFX_DLL) && defined(COD2_GFX_DLL_RETAIL_V60)
#    include <stdlib.h>

#    include "PC/client_mp/gfx_dll_v60_map.h"

extern int g_gfxV60DllActive;
extern void *g_gfxV60AddRefEntity;
extern refexport_t *CL_LoadRendererDll(int apiVersion, refimport_t *ri_in);
extern refimport_t ri;

extern void Com_Printf(const char *, ...);
extern const dvar_t *Dvar_RegisterBool(const char *, int, int);
extern const dvar_t *Dvar_FindVar(const char *);

extern void gfxv60_ShowWin(void), gfxv60_HideWin(void), gfxv60_PumpMsgs(void);
extern unsigned gfxv60_HiAlloc(unsigned);
extern void gfxv60_HiSetOffset(unsigned);
extern void *gfxv60_HiOffToPtr(unsigned);
extern void *gfxv60_GetThreadData(void);
extern unsigned long gfxv60_CurThreadId(void);
extern int gfxv60_IsWorker(void);
extern void gfxv60_ResumeWorker(int);
extern void *gfxv60_InitWorker(void *);
extern void gfxv60_m114(void), gfxv60_m115(void), gfxv60_m116(void), gfxv60_m117(void);
extern void gfxv60_m118(void), gfxv60_m119(void);
extern int gfxv60_m120(void);
extern void gfxv60_m121(void), gfxv60_m122(void);
extern void *gfxv60_MaterialDup(void *, const char *);

extern int Hunk_HideTempMemory(void);
extern void Hunk_ShowTempMemory(int);
extern void DB_EnumXAssets(int, void (*)(union XAssetHeader, void *), void *, int);
extern void *CM_GetPlaneNum(int planeNum);

static int gfxv60_RayTriStub(void)
{
    return 0;
}

static void gfxV60_RegisterWindowClass(void)
{
    extern void *__stdcall GetModuleHandleA(const char *);
    extern long __stdcall DefWindowProcA(void *, unsigned, unsigned, long);
    extern void *__stdcall LoadCursorA(void *, const char *);
    extern unsigned short __stdcall RegisterClassA(const void *);
    struct {
        unsigned style;
        void *wndproc;
        int ce, we;
        void *hInst, *hIcon, *hCursor, *hbr;
        const char *menu, *cls;
    } wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = 0x0003 ;
    wc.wndproc = (void *)DefWindowProcA;
    wc.hInst = GetModuleHandleA((void *)0);
    wc.hCursor = LoadCursorA((void *)0, (const char *)0x7f00 );
    wc.cls = "CoD2";
    RegisterClassA(&wc);
}

void gfxV60_LoadRetailRenderer(refimport_t *ri_local, refexport_t *out_re)
{
    static unsigned char ri_v60[768];
    const int *src = (const int *)ri_local;
    int *dst = (int *)ri_v60;
    refexport_t *ret;
    int k;

    ri_local->Hunk_HideTempMemory = (void *)Hunk_HideTempMemory;
    ri_local->Hunk_ShowTempMemory = (void *)Hunk_ShowTempMemory;
    ri_local->DB_EnumXAssets = (void *)DB_EnumXAssets;
    ri_local->CM_RayTriangleIntersect = (void *)gfxv60_RayTriStub;
    ri_local->DObjGetPartBits = (void *)gfxv60_RayTriStub;

    memcpy(&ri, ri_local, sizeof(*ri_local));

    gfxV60_RegisterWindowClass();
    Dvar_RegisterBool("r_smp_backend_allowed", 0, 0);

    memset(ri_v60, 0, sizeof(ri_v60));
    for (k = 0; k < 181; k++)
        dst[k] = (kV60ToV59[k] >= 0) ? src[kV60ToV59[k]]
                                     : (int)(intptr_t)gfxV60Thunks[k];

    dst[18] = (int)(intptr_t)gfxv60_HiAlloc;
    dst[19] = (int)(intptr_t)gfxv60_HiSetOffset;
    dst[20] = (int)(intptr_t)gfxv60_HiOffToPtr;
    dst[21] = src[18];
    dst[22] = (int)(intptr_t)gfxv60_ShowWin;
    dst[23] = (int)(intptr_t)gfxv60_HideWin;
    dst[24] = (int)(intptr_t)gfxv60_PumpMsgs;
    dst[25] = (int)(intptr_t)Dvar_FindVar;

    dst[112] = (int)(intptr_t)gfxv60_GetThreadData;
    dst[113] = (int)(intptr_t)gfxv60_CurThreadId;
    dst[114] = (int)(intptr_t)gfxv60_IsWorker;
    dst[115] = (int)(intptr_t)gfxv60_ResumeWorker;
    dst[116] = (int)(intptr_t)gfxv60_InitWorker;
    dst[117] = (int)(intptr_t)gfxv60_m114;
    dst[118] = (int)(intptr_t)gfxv60_m115;
    dst[119] = (int)(intptr_t)gfxv60_m116;
    dst[120] = (int)(intptr_t)gfxv60_m117;
    dst[121] = (int)(intptr_t)gfxv60_m118;
    dst[122] = (int)(intptr_t)gfxv60_m119;
    dst[123] = (int)(intptr_t)gfxv60_m120;
    dst[124] = (int)(intptr_t)gfxv60_m121;
    dst[125] = (int)(intptr_t)gfxv60_m122;

    dst[111] = src[107];

    dst[159] = (int)(intptr_t)CM_GetPlaneNum;

    ret = CL_LoadRendererDll(0x3c, (refimport_t *)ri_v60);

    {
#    include "PC/client_mp/gfx_dll_v60_re_map.h"
        const int *rsrc = (const int *)ret;
        int *rdst = (int *)out_re;
        int n = (int)(sizeof(*out_re) / 4);
        for (k = 0; k < n && k < (int)(sizeof(kReV60) / sizeof(kReV60[0])); k++)
            rdst[k] = (kReV60[k] >= 0) ? rsrc[kReV60[k]] : rsrc[k];

        rdst[79] = (int)(intptr_t)gfxv60_MaterialDup;

        g_gfxV60AddRefEntity = (void *)rsrc[20];
    }

    g_gfxV60DllActive = 1;
}
#endif
