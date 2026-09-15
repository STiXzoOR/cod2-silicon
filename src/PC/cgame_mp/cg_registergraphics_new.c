#include "cod2_feature_config.h"
#include "common_types.h"
#include "imports.h"
#include "headers/PC/cgame_mp/cg_local.h"
#include <string.h>

extern void SCR_UpdateScreen(void);
extern void Com_Printf(const char *fmt, ...);
extern void Com_Error(errorParm_t code, const char *fmt, ...);
extern int FX_InitSystem(int maxEffects);
extern void FX_CreateDefaultEffect(void);
extern EffectTemplate * FX_RegisterEffect(const char *fileName);
extern void CG_LoadingString(const char *str);
extern MaterialHandle CL_RegisterMaterial(const char *name, int imageTrack);
extern MaterialHandle CL_RegisterMaterialNoMip(const char *name, int imageTrack);
extern XModel *CL_RegisterModel(const char *name);
extern GfxBrushModel *CL_RegisterInlineModel(int index);
extern void CL_ModelBounds(GfxBrushModel *model, vec_t *mins, vec_t *maxs);
extern const char *CL_GetConfigString(int index);
extern int CM_NumInlineModels(void);
extern void CG_RegisterItems(void);
extern void CG_RegisterScoreboardGraphics(void);
extern int CG_LoadShellShockDvars(const char *name);
extern void CG_SetShellShockParmsFromDvars(shellshock_parms_t *parms);
extern FxImpactTable *CG_RegisterImpactEffects(const char *mapname);

extern byte *cg_items;
extern byte *cg_weapons;

#define CG_MAX_ITEMS 256
#define CG_MAX_WEAPONS 128

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

    cgs->media.lagometerMaterial = (MaterialHandle)(CL_RegisterMaterial("lagometer", 7));
    cgs->media.connectionMaterial = (MaterialHandle)(CL_RegisterMaterial("headicondisconnected", 7));
    cgs->media.youInKillCamMaterial = (MaterialHandle)(CL_RegisterMaterial("headiconyouinkillcam", 7));
    CL_RegisterMaterial("killiconmelee", 7);
    CL_RegisterMaterial("killiconsuicide", 7);
    CL_RegisterMaterial("killiconfalling", 7);
    CL_RegisterMaterial("killiconcrush", 7);
    CL_RegisterMaterial("killicondied", 7);
    cgs->media.tracerMaterial = (MaterialHandle)(CL_RegisterMaterial("gfx/misc/tracer", 6));
    cgs->media.hintMaterials[2] = (MaterialHandle)(CL_RegisterMaterial("gfx/icons/hint_usable", 7));
    cgs->media.hintMaterials[3] = (MaterialHandle)(CL_RegisterMaterial("hint_health", 7));
    cgs->media.hintMaterials[4] = (MaterialHandle)(CL_RegisterMaterial("hint_friendly", 7));
    cgs->media.stanceMaterials[0] = (MaterialHandle)(CL_RegisterMaterial("stance_stand", 7));
    cgs->media.stanceMaterials[1] = (MaterialHandle)(CL_RegisterMaterial("stance_crouch", 7));
    cgs->media.stanceMaterials[2] = (MaterialHandle)(CL_RegisterMaterial("stance_prone", 7));
    cgs->media.stanceMaterials[3] = (MaterialHandle)(CL_RegisterMaterial("stance_flash", 7));
    cgs->media.objectiveMaterials[0] = (MaterialHandle)(CL_RegisterMaterial("objective", 7));
    cgs->media.friendMaterials[0] = (MaterialHandle)(CL_RegisterMaterial("objective_friendly", 7));
    cgs->media.friendMaterials[1] = (MaterialHandle)(CL_RegisterMaterial("objective_friendly_chat", 7));
    cgs->media.damageMaterial = (MaterialHandle)(CL_RegisterMaterial("hit_direction", 7));
    cgs->media.mantleHint = (MaterialHandle)(CL_RegisterMaterial("hint_mantle", 7));
    cgs->media.checkbox_clear = (MaterialHandle)(CL_RegisterMaterialNoMip("ui/assets/checkbox_clear", 7));
    cgs->media.checkbox_checked = (MaterialHandle)(CL_RegisterMaterialNoMip("ui/assets/checkbox_checked", 7));
    cgs->media.checkbox_fail = (MaterialHandle)(CL_RegisterMaterialNoMip("ui/assets/checkbox_fail", 7));
    cgs->media.compassping_friendlyfiring = (MaterialHandle)(CL_RegisterMaterialNoMip("compassping_friendlyfiring", 7));
    cgs->media.compassping_friendlyyelling = (MaterialHandle)(CL_RegisterMaterialNoMip("compassping_friendlyyelling", 7));
    cgs->media.compassping_enemyfiring = (MaterialHandle)(CL_RegisterMaterialNoMip("compassping_enemyfiring", 7));
    cgs->media.compassping_enemyyelling = (MaterialHandle)(CL_RegisterMaterialNoMip("compassping_enemyyelling", 7));
    cgs->media.compassping_grenade = (MaterialHandle)(CL_RegisterMaterialNoMip("compassping_grenade", 7));
    cgs->media.compassping_explosion = (MaterialHandle)(CL_RegisterMaterialNoMip("compassping_explosion", 7));
    cgs->media.grenadeIcon = (MaterialHandle)(CL_RegisterMaterialNoMip("hud_grenadeicon", 7));
    cgs->media.grenadePointer = (MaterialHandle)(CL_RegisterMaterialNoMip("hud_grenadepointer", 7));
    cgs->media.teamStatusBar = (MaterialHandle)(CL_RegisterMaterial("hudcolorbar", 7));

    CG_LoadingString(" - models");
    cgs->media.voiceChatMaterial = (MaterialHandle)(CL_RegisterMaterial("headiconvoicechat", 7));
    cgs->media.balloonMaterial = (MaterialHandle)(CL_RegisterMaterial("headicontalkballoon", 7));
    CG_RegisterScoreboardGraphics();

    memset((void *)cg_items, 0, CG_MAX_ITEMS * sizeof(itemInfo_t));
    memset((void *)cg_weapons, 0, CG_MAX_WEAPONS * sizeof(weaponInfo_t));

    CG_LoadingString(" - items");
    CG_RegisterItems();

    CG_LoadingString(" - inline models");
    cgs->numInlineModels = CM_NumInlineModels();

    if (cgs->numInlineModels > 1) {
        for (i = 1; i < cgs->numInlineModels; i++) {
            cgs->inlineDrawModel[i] = CL_RegisterInlineModel(i);

            GfxBrushModel *model = cgs->inlineDrawModel[i];
            CL_ModelBounds(model, mins, maxs);

            for (j = 0; j < 3; j++) {
                float mid = (mins[j] + maxs[j]) * 0.5f;
                cgs->inlineModelMidpoints[i][j] = mid;
            }
        }
    }

    CG_LoadingString(" - server models");
    for (i = 1; i < 256; i++) {
        str = CL_GetConfigString(0x14e + i);
        if (str[0] == '\0')
            continue;

        cgs->gameModels[i] = CL_RegisterModel(str);
    }

    for (i = 1; i < 64; i++) {
        str = CL_GetConfigString(0x34e + i);
        if (str[0] == '\0')
            continue;
        cgs->fxs[i] = (EffectTemplate *)(FX_RegisterEffect(str));
    }

    cgs->smokeGrenadeFx = (EffectTemplate *)(FX_RegisterEffect("fx/props/american_smoke_grenade.efx"));

    for (i = 1; i < 16; i++) {
        str = CL_GetConfigString(0x48e + i);
        if (str[0] == '\0')
            break;
        if (!CG_LoadShellShockDvars(str)) {
            Com_Error(ERR_DROP, "couldn't register shell shock '%s' -- see console\n", str);
        }
        CG_SetShellShockParmsFromDvars(&cgs->shellshockParms[i]);
    }

    if (!CG_LoadShellShockDvars("hold_breath")) {
        Com_Error(ERR_DROP, "Couldn't find shock file [hold_breath.shock]\n");
    }
    CG_SetShellShockParmsFromDvars(&cgs->holdBreathParams);

    cgs->media.fx = CG_RegisterImpactEffects(mapname);
    if (!cgs->media.fx) {
        Com_Error(ERR_DROP, "Error reading CSV files in the fx directory to identify impact effects");
    }
    cgs->media.fxNoBloodFleshHit = (EffectTemplate *)(FX_RegisterEffect("fx/impacts/flesh_hit_noblood.efx"));

    CG_LoadingString(" - game media done");
}
