#include "common_types.h"
#include "r_cod2x_film.h"

#if COD2X_FILM
#include <string.h>
#include <stdatomic.h>
#include "Mac/Metal/metal_backend.h"

extern refimport_t ri;
extern void Com_Printf(const char *, ...);

static const dvar_t *enabled, *contrast, *brightness, *gamma;
static const dvar_t *saturation, *skySaturation, *color, *darkTint, *lightTint;
static _Atomic(const char *) failure;
static atomic_int failed;

void R_Cod2xFilm_Init(void)
{
    /* CoD2x main filmtweak.cpp: archived, changeable-reset, identity defaults. */
    const int flags = 0x1001;
    enabled = ri.Dvar_RegisterBool("r_filmTweakEnable", 1, flags);
    contrast = ri.Dvar_RegisterFloat("r_filmTweakContrast", 1, 0, 4, flags);
    brightness = ri.Dvar_RegisterFloat("r_filmTweakBrightness", 0, -1, 1, flags);
    gamma = ri.Dvar_RegisterFloat("r_filmTweakGamma", 1, .5f, 2, flags);
    saturation = ri.Dvar_RegisterFloat("r_filmTweakSaturation", 1, 0, 2, flags);
    skySaturation = ri.Dvar_RegisterFloat("r_filmTweakSkySaturation", 1, 0, 2, flags);
    color = ri.Dvar_RegisterVec3("r_filmTweakColor", 1, 1, 1, 0, 2, flags);
    darkTint = ri.Dvar_RegisterVec3("r_filmTweakDarkTint", 1, 1, 1, 0, 2, flags);
    lightTint = ri.Dvar_RegisterVec3("r_filmTweakLightTint", 1, 1, 1, 0, 2, flags);
}

int R_Cod2xFilm_Settings(Cod2xFilmSettings *settings)
{
    const char *message = atomic_exchange(&failure, NULL);
    if (message) {
        Com_Printf("CoD2x: ^3Film colour controls are unavailable: %s^7\n", message);
    }
    if (failed || !enabled || !enabled->current.enabled)
        return 0;

    settings->contrast = contrast->current.value;
    settings->brightness = brightness->current.value;
    settings->gamma = gamma->current.value;
    settings->saturation = saturation->current.value;
    settings->skySaturation = skySaturation->current.value;
    memcpy(settings->color, color->current.vector, sizeof(settings->color));
    memcpy(settings->darkTint, darkTint->current.vector, sizeof(settings->darkTint));
    memcpy(settings->lightTint, lightTint->current.vector, sizeof(settings->lightTint));

    if (settings->contrast != 1 || settings->brightness != 0 || settings->gamma != 1 ||
        settings->saturation != 1 || settings->skySaturation != 1)
        return 1;
    for (int i = 0; i < 3; ++i)
        if (settings->color[i] != 1 || settings->darkTint[i] != 1 || settings->lightTint[i] != 1)
            return 1;
    return 0;
}

void R_Cod2xFilm_Apply(const Cod2xFilmPass *pass)
{
    if (failed)
        return;
    const char *error = MacShim_IsMetal() ? "the native Metal renderer has no film pass" : Cod2xFilm_GLApply(pass);
    if (error) {
        failure = error;
        failed = 1;
    }
}

void R_Cod2xFilm_Release(void)
{
    if (!MacShim_IsMetal())
        Cod2xFilm_GLRelease();
    failure = NULL;
    failed = 0;
}
#endif
