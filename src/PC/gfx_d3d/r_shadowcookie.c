#include "common_types.h"
#include "imports.h"

static ShadowCookieGlob shadowCookieGlob;

void R_ResetShadowCookies(void)
{
    shadowCookieGlob.weightCap = 10.0f;
    shadowCookieGlob.lastTime = -1;
}
