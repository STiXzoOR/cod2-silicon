#ifndef R_COD2X_FILM_H
#define R_COD2X_FILM_H

#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#define COD2X_FILM 1
#include "platform/cod2x_film_gl.h"

enum { GFX_CMD_COD2X_FILM = 34 };
typedef struct {
    GfxCmdHeader header;
    Cod2xFilmPass pass;
} GfxCmdCod2xFilm;

void R_Cod2xFilm_Init(void);
int R_Cod2xFilm_Settings(Cod2xFilmSettings *settings);
void R_Cod2xFilm_Apply(const Cod2xFilmPass *pass);
void R_Cod2xFilm_Release(void);
void R_AddCmdCod2xFilm(const refdef_t *refdef);
#endif

#endif
