#ifndef COD2X_STOP_GAMETYPE_H
#define COD2X_STOP_GAMETYPE_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* CodeCallback_StopGameType, asked as a CoD2x server asks it: when the map,
   map_restart, fast_restart or map_rotate command runs, or the server shuts
   down, at most once per server frame. */
qboolean GScr_Cod2xBeforeMapChange(qboolean shutdown, const char *source);
/* Lets the next blocked change through once (CoD2x's match cancel). */
void GScr_Cod2xAllowOneMapChange(void);
qboolean SV_Cod2xBeforeMapChange(qboolean shutdown, const char *source);
/* Called after each G_RunFrame. */
void SV_Cod2xMapChangeFrameDone(void);
#endif

#endif
