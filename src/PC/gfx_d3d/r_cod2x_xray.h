#ifndef R_COD2X_XRAY_H
#define R_COD2X_XRAY_H
#include "PC/cgame_mp/cg_cod2x_spectator.h"
#if COD2X_SPECTATOR && defined(__APPLE__)
#define COD2X_XRAY_RENDER 1
enum { GFX_CMD_COD2X_XRAY = 35, GFX_CMD_COD2X_HEAD = 36 };
typedef struct { GfxCmdHeader header; Cod2xXraySettings settings; } GfxCmdCod2xXray;
typedef struct { GfxCmdHeader header; int operation; float point[3]; } GfxCmdCod2xHead;
void R_AddCmdCod2xXray(void);
int R_AddCmdCod2xHead(int operation, const float *point);
void R_Cod2xXraySettings(const Cod2xXraySettings *settings);
void R_Cod2xXrayBegin(GfxDrawSurf *surfaces, int count, int technique, int order);
void R_Cod2xXrayEnd(void);
void R_Cod2xXrayDraw(unsigned int first, unsigned int last, unsigned int count, const void *indices);
int R_Cod2xHeadAvailable(void);
void R_Cod2xHeadCommand(int operation, const float *point);
void R_Cod2xXrayRelease(void);
#endif
#endif
