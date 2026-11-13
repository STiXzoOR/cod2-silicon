#ifndef COD2X_FILM_GL_H
#define COD2X_FILM_GL_H

/* A scene command owns these values; the rendering thread never reads dvars. */
typedef struct {
    float contrast, brightness, gamma;
    float saturation, skySaturation;
    float color[3], darkTint[3], lightTint[3];
} Cod2xFilmSettings;

typedef struct {
    int x, y, width, height;
    int targetWidth, targetHeight;
    Cod2xFilmSettings settings;
} Cod2xFilmPass;

/* NULL means success; a static diagnostic otherwise. Current context required. */
const char *Cod2xFilm_GLApply(const Cod2xFilmPass *pass);
void Cod2xFilm_GLRelease(void);

#endif
