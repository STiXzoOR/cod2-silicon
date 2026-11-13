#include "common_types.h"
#include "r_cod2x_xray.h"
#if COD2X_XRAY_RENDER
#include "Mac/Metal/metal_backend.h"
#include <string.h>
extern r_backEndGlobals_t backEnd;
static Cod2xXraySettings xray;
static int drawing;
static GfxDrawSurf playerSurfaces[4096];
static int playerNumbers[4096];
void R_Cod2xXraySettings(const Cod2xXraySettings *settings) { xray = *settings; }
void R_Cod2xXrayBegin(GfxDrawSurf *surfaces, int count, int technique, int order)
{
    drawing = 0;
    if (MacShim_IsMetal()) return;
    Cod2xXray_GLHead(0, 0);
    drawing = technique == 6 && xray.mode >= 2;
    if (!drawing) return;
    /* Keep material order inside each player, and prevent another player's
       world-space surfaces from being merged into its draw call. */
    if (!order) {
        int kept = 0, players = 0;
        for (int i = 0; i < count; ++i) {
            unsigned key = surfaces[i].sort;
            int entity = (key >> ((int)key < 0 ? 19 : 4)) & 4095;
            if (entity < 0x7fe && (backEnd.sceneDef.entities[entity].renderFxFlags & COD2X_XRAY_TAG) && players < 4096) {
                int slot = players++;
                while (slot && playerNumbers[slot-1] > entity) {
                    playerNumbers[slot] = playerNumbers[slot-1]; playerSurfaces[slot] = playerSurfaces[slot-1]; --slot;
                }
                playerNumbers[slot] = entity; playerSurfaces[slot] = surfaces[i];
            } else surfaces[kept++] = surfaces[i];
        }
        memcpy(surfaces + kept, playerSurfaces, players * sizeof(*surfaces));
    }
    Cod2xXray_GLBegin(&xray);
}
void R_Cod2xXrayDraw(unsigned first, unsigned last, unsigned count, const void *indices,
                    const void *renderTarget)
{
    const GfxEntity *entity = backEnd.currentEntity;
    if (!drawing || !entity || !(entity->renderFxFlags & COD2X_XRAY_TAG) || !backEnd.viewParms || !renderTarget) return;
    /* An inset viewport still uses the whole render target for the mask,
       half-size glow textures and screen-centered fade. */
    D3DSURFACE_DESC target;
    void **methods = *(void ***)renderTarget;
    if (((HRESULT (*)(const void *, D3DSURFACE_DESC *))methods[12])(renderTarget, &target) < 0) return;
    Cod2xXray_GLPlayer((unsigned)entity->renderFxFlags, entity->origin,
                      (const float *)&backEnd.viewParms->viewMatrix,
                      (const float *)&backEnd.viewParms->projectionMatrix,
                      target.Width, target.Height, first, last, count, indices);
}
void R_Cod2xXrayEnd(void)
{
    if (drawing) Cod2xXray_GLEnd();
    drawing = 0;
}
int R_Cod2xHeadAvailable(void)
{
    return !MacShim_IsMetal() && Cod2xXray_GLHeadAvailable();
}
void R_Cod2xHeadCommand(int operation, const float *point)
{
    if (MacShim_IsMetal()) return;
    float depth = 0;
    if (operation && backEnd.viewParms) {
        const float *m = (const float *)&backEnd.viewParms->projectionMatrix;
        float z = point[0]*m[2] + point[1]*m[6] + point[2]*m[10] + m[14];
        float w = point[0]*m[3] + point[1]*m[7] + point[2]*m[11] + m[15];
        if (w <= 0) return;
        depth = z / w;
        if (depth < 0) depth = 0; if (depth > 1) depth = 1;
    }
    Cod2xXray_GLHead(operation, depth);
}
void R_Cod2xXrayRelease(void)
{
    drawing = 0;
    if (!MacShim_IsMetal()) Cod2xXray_GLRelease();
}
#endif
