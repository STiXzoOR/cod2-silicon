#ifndef CG_COD2X_SPECTATOR_H
#define CG_COD2X_SPECTATOR_H
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#define COD2X_SPECTATOR 1
#include "platform/cod2x_xray_gl.h"
enum { COD2X_XRAY_TAG = 0x01000000, COD2X_XRAY_TEAM_SHIFT = 25, COD2X_XRAY_HEALTH_SHIFT = 27 };
typedef struct { int health, primary, secondary, frag, smoke; } Cod2xPlayerInfo;
void CG_Cod2xSpectatorInit(void);
void CG_Cod2xSpectatorFrame(void);
void CG_Cod2xSpectatorReset(void);
void CG_Cod2xSpectatorSettings(Cod2xXraySettings *settings);
int CG_Cod2xPlayerInfo(int client, Cod2xPlayerInfo *info);
void CG_Cod2xTagPlayer(GfxEntity *entity, const struct DObj_s *obj, const centity_t *cent);
void CG_Cod2xHeadDraw(void);
int CG_Cod2xHeadHidesName(int client);
int CG_Cod2xHeadHidesNation(const centity_t *cent);
float CG_Cod2xHeadIconScale(void);
int CG_Cod2xHeadOffset(void);
void CG_Cod2xHeadEvent(const centity_t *cent, int event);
#endif
#endif
