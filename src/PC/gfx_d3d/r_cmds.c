#include "common_types.h"
#include "imports.h"

extern void R_ScreenshotCommand(int type);
extern void Material_UpdatePicmipAll(void);

extern refimport_t ri;
extern void R_ImageList_f(void);
extern void (*R_ReloadMaterialTextures_f)(void);
extern void (*R_LoadSun_f)(void);
extern void (*R_SaveSun_f)(void);
extern void (*R_SmcStats_f)(void);
extern void (*R_SmcFlush_f)(void);

static void R_Cmd_ApplyPicmip(void)
{
    Material_UpdatePicmipAll();
}

static void R_Cmd_Screenshot(void)
{
    R_ScreenshotCommand(1);
}

static void R_Cmd_ScreenshotJpeg(void)
{
    R_ScreenshotCommand(0);
}

void R_RegisterCmds(void)
{
    ri.Cmd_AddCommand("screenshot", R_Cmd_Screenshot);
    ri.Cmd_AddCommand("screenshotJpeg", R_Cmd_ScreenshotJpeg);
    ri.Cmd_AddCommand("imagelist", R_ImageList_f);
    ri.Cmd_AddCommand("r_applyPicmip", R_Cmd_ApplyPicmip);
    ri.Cmd_AddCommand("reloadmaterialtextures", R_ReloadMaterialTextures_f);
    ri.Cmd_AddCommand("r_loadsun", R_LoadSun_f);
    ri.Cmd_AddCommand("r_savesun", R_SaveSun_f);
    ri.Cmd_AddCommand("r_smc_stats", R_SmcStats_f);
    ri.Cmd_AddCommand("r_smc_flush", R_SmcFlush_f);
}

void R_UnregisterCmds(void)
{
    ri.Cmd_RemoveCommand("r_applyPicmip");
    ri.Cmd_RemoveCommand("r_vc_stats");
    ri.Cmd_RemoveCommand("screenshot");
    ri.Cmd_RemoveCommand("screenshotJpeg");
    ri.Cmd_RemoveCommand("imagelist");
    ri.Cmd_RemoveCommand("reloadmaterialtextures");
    ri.Cmd_RemoveCommand("r_loadsun");
    ri.Cmd_RemoveCommand("r_savesun");
    ri.Cmd_RemoveCommand("r_smc_stats");
    ri.Cmd_RemoveCommand("r_smc_flush");
}
