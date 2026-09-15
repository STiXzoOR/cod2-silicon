#include "common_types.h"
#include "imports.h"

extern int XModelGetNumLods(const XModel *model);
extern int XModelGetSurfaces(const XModel *xmodel, XSurface ***xsurfs, int lodIndex, int **partBits);
extern void *Hunk_AllocAlignInternal(int size, int alignment);

extern r_globals_t rg;
extern r_global_permanent_t rgp;

void R_InitStaticModelDynamicData(int smodelIndex)
{
    XSurface **xsurfs;
    int *partBits;
    int lodIndex;
    int maxSurfs;
    int instanceSize;
    void *buffer;

    GfxStaticModelDynamic *smodelDync = &rg.smodelDyncs[smodelIndex];
    struct XModel *xmodel = rgp.world->smodelInsts[smodelIndex].model;

    int lodCount = XModelGetNumLods(xmodel);
    maxSurfs = 0;
    if (lodCount > 0) {
        for (lodIndex = 0; lodIndex < lodCount; lodIndex++) {
            int surfCount = XModelGetSurfaces(xmodel, &xsurfs, lodIndex, &partBits);
            if (surfCount > maxSurfs)
                maxSurfs = surfCount;
        }
    }

    instanceSize = maxSurfs * (int)sizeof(GfxStaticSurface);
    buffer = Hunk_AllocAlignInternal(instanceSize, 4);
    smodelDync->staticSurfs = (GfxStaticSurface *)buffer;
    memset(buffer, 0, instanceSize);
}
