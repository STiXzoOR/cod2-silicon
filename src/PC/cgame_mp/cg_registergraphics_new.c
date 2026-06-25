#include "cod2_feature_config.h"
#include "common_types.h"
#include "imports.h"
#include "headers/PC/cgame_mp/cg_local.h"
#include <string.h>

extern void SCR_UpdateScreen(void);
extern void Com_Printf(const char *fmt, ...);
extern void Com_Error(int level, const char *fmt, ...);
extern int FX_InitSystem(int maxEffects);
extern void FX_CreateDefaultEffect(void);
extern void *FX_RegisterEffect(const char *name);
extern void CG_LoadingString(const char *str);
extern void *CL_RegisterMaterial(const char *name, int flags);
extern void *CL_RegisterMaterialNoMip(const char *name, int flags);
extern void *CL_RegisterModel(const char *name);
extern void *CL_RegisterInlineModel(int index);
extern void CL_ModelBounds(void *model, vec3_t mins, vec3_t maxs);
extern const char *CL_GetConfigString(int index);
extern int CM_NumInlineModels(void);
extern void CG_RegisterItems(void);
extern void CG_RegisterScoreboardGraphics(void);
extern int CG_LoadShellShockDvars(const char *name);
extern void CG_SetShellShockParmsFromDvars(void *parms);
extern int CG_RegisterImpactEffects(const char *mapname);

extern byte *cg_items;
extern byte *cg_weapons;

#if COD2_IS_PATCH_13
#    define CGS_ADDR(off) ((char *)cgs + 0x1c180 + (off))
#else
#    define CGS_ADDR(off) ((char *)cgs + (off))
#endif
#define CGS_INT(off) (*(int *)CGS_ADDR(off))
#define CGS_PTR(off) (*(void **)CGS_ADDR(off))

void __attribute_regparm__(1) CG_RegisterGraphics(const char *mapname)
{
    int i, j;
    const char *str;
    vec3_t mins, maxs;

    SCR_UpdateScreen();
    Com_Printf("^5---------- Fx System Initialization ---------\n");
    FX_InitSystem(1);
    FX_CreateDefaultEffect();
    Com_Printf("^5----- Fx System Initialization Complete -----\n");

    CG_LoadingString(" - textures");

    CGS_PTR(0xba40) = CL_RegisterMaterial("lagometer", 7);
    CGS_PTR(0xba34) = CL_RegisterMaterial("headicondisconnected", 7);
    CGS_PTR(0xba38) = CL_RegisterMaterial("headiconyouinkillcam", 7);
    CL_RegisterMaterial("killiconmelee", 7);
    CL_RegisterMaterial("killiconsuicide", 7);
    CL_RegisterMaterial("killiconfalling", 7);
    CL_RegisterMaterial("killiconcrush", 7);
    CL_RegisterMaterial("killicondied", 7);
    CGS_PTR(0xba3c) = CL_RegisterMaterial("gfx/misc/tracer", 6);
    CGS_PTR(0xba4c) = CL_RegisterMaterial("gfx/icons/hint_usable", 7);
    CGS_PTR(0xba50) = CL_RegisterMaterial("hint_health", 7);
    CGS_PTR(0xba54) = CL_RegisterMaterial("hint_friendly", 7);
    CGS_PTR(0xbc58) = CL_RegisterMaterial("stance_stand", 7);
    CGS_PTR(0xbc5c) = CL_RegisterMaterial("stance_crouch", 7);
    CGS_PTR(0xbc60) = CL_RegisterMaterial("stance_prone", 7);
    CGS_PTR(0xbc64) = CL_RegisterMaterial("stance_flash", 7);
    CGS_PTR(0xbc68) = CL_RegisterMaterial("objective", 7);
    CGS_PTR(0xbc6c) = CL_RegisterMaterial("objective_friendly", 7);
    CGS_PTR(0xbc70) = CL_RegisterMaterial("objective_friendly_chat", 7);
    CGS_PTR(0xbc74) = CL_RegisterMaterial("hit_direction", 7);
    CGS_PTR(0xbc78) = CL_RegisterMaterial("hint_mantle", 7);
    CGS_PTR(0xc1d4) = CL_RegisterMaterialNoMip("ui/assets/checkbox_clear", 7);
    CGS_PTR(0xc1d8) = CL_RegisterMaterialNoMip("ui/assets/checkbox_checked", 7);
    CGS_PTR(0xc1dc) = CL_RegisterMaterialNoMip("ui/assets/checkbox_fail", 7);
    CGS_PTR(0xc1e0) = CL_RegisterMaterialNoMip("compassping_friendlyfiring", 7);
    CGS_PTR(0xc1e4) = CL_RegisterMaterialNoMip("compassping_friendlyyelling", 7);
    CGS_PTR(0xc1e8) = CL_RegisterMaterialNoMip("compassping_enemyfiring", 7);
    CGS_PTR(0xc1ec) = CL_RegisterMaterialNoMip("compassping_enemyyelling", 7);
    CGS_PTR(0xc1f0) = CL_RegisterMaterialNoMip("compassping_grenade", 7);
    CGS_PTR(0xc1f4) = CL_RegisterMaterialNoMip("compassping_explosion", 7);
    CGS_PTR(0xc1f8) = CL_RegisterMaterialNoMip("hud_grenadeicon", 7);
    CGS_PTR(0xc1fc) = CL_RegisterMaterialNoMip("hud_grenadepointer", 7);
    CGS_PTR(0xba28) = CL_RegisterMaterial("hudcolorbar", 7);

    CG_LoadingString(" - models");
    CGS_PTR(0xba2c) = CL_RegisterMaterial("headiconvoicechat", 7);
    CGS_PTR(0xba30) = CL_RegisterMaterial("headicontalkballoon", 7);
    CG_RegisterScoreboardGraphics();

    memset((void *)cg_items, 0, 9216);
    memset((void *)cg_weapons, 0, 55808);

    CG_LoadingString(" - items");
    CG_RegisterItems();

    CG_LoadingString(" - inline models");
    CGS_INT(0x7188) = CM_NumInlineModels();

    if (CGS_INT(0x7188) > 1) {
        int offset = 3;
        for (i = 1; i < CGS_INT(0x7188); i++) {
            CGS_PTR(0x718c + i * 4) = CL_RegisterInlineModel(i);

            void *model = CGS_PTR(0x718c + i * 4);
            CL_ModelBounds(model, mins, maxs);

            for (j = 0; j < 3; j++) {
                float mid = (mins[j] + maxs[j]) * 0.5f;
                *(float *)CGS_ADDR(0x8188 + (offset + j) * 4) = mid;
            }
            offset += 3;
        }
    }

    CG_LoadingString(" - server models");
    for (i = 1; i < 256; i++) {
        str = CL_GetConfigString(0x14e + i);
        if (str[0] == '\0')
            continue;

        CGS_PTR(0x63c0 + i * 4) = CL_RegisterModel(str);
    }

    for (i = 1; i < 64; i++) {
        str = CL_GetConfigString(0x34e + i);
        if (str[0] == '\0')
            continue;
        CGS_PTR(0x67c0 + i * 4) = FX_RegisterEffect(str);
    }

    CGS_PTR(0x68c0) = FX_RegisterEffect("fx/props/american_smoke_grenade.efx");

    for (i = 1; i < 16; i++) {
        str = CL_GetConfigString(0x48e + i);
        if (str[0] == '\0')
            break;
        if (!CG_LoadShellShockDvars(str)) {
            Com_Error(1, "couldn't register shell shock '%s' -- see console\n", str);
        }
        CG_SetShellShockParmsFromDvars((void *)CGS_ADDR(0x6948 + i * 132));
    }

    if (!CG_LoadShellShockDvars("hold_breath")) {
        Com_Error(1, "Couldn't find shock file [hold_breath.shock]\n");
    }
    CG_SetShellShockParmsFromDvars((void *)CGS_ADDR(0x7104));

    CGS_INT(0xc200) = CG_RegisterImpactEffects(mapname);
    if (!CGS_INT(0xc200)) {
        Com_Error(1, "Error reading CSV files in the fx directory to identify impact effects");
    }
    CGS_PTR(0xc204) = FX_RegisterEffect("fx/impacts/flesh_hit_noblood.efx");

    CG_LoadingString(" - game media done");
}
