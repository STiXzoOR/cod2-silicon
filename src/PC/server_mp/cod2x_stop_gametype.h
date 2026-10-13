#ifndef COD2X_STOP_GAMETYPE_H
#define COD2X_STOP_GAMETYPE_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
qboolean GScr_Cod2xBeforeMapChange(qboolean shutdown, const char *source);
qboolean GScr_Cod2xConsumeMapChange(const char *source, const char *command, const char *target, const char *marker);
void GScr_Cod2xResetMapChange(void);
qboolean GScr_Cod2xQueueExitLevel(void);
qboolean SV_Cod2xBeginMapChange(qboolean shutdown, const char *source);
void SV_Cod2xEndMapChange(qboolean shutdown);
#endif

#endif
