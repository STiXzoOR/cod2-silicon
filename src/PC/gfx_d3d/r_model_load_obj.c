#include "common_types.h"
#include "imports.h"

extern int XModelGetNumLods(struct XModel *model);
extern int XModelGetSurfaces(struct XModel *model, XSurface **surfaces, int lodIndex, XPartBits *partBits);
extern const char *XModelGetSurfaceName(struct XModel *model, int surfIndex, int lodIndex);
extern void *Model_Alloc(int size);
extern MaterialHandle Material_RegisterHandle(const char *name, int imageTrack, int materialType);
extern char *strlwr(char *);

trXSkin_t *R_LoadXSkins(struct XModel *model)
{
    XSurface *surfaces;
    XPartBits partBits;
    char materialName[64];
    int lodCount;
    int totalNumSurfaces;
    int i, j;

    lodCount = XModelGetNumLods(model);

    totalNumSurfaces = 0;
    for (j = 0; j < lodCount; j++) {
        totalNumSurfaces += XModelGetSurfaces(model, &surfaces, j, &partBits);
    }

    trXSkin_t *skins = (trXSkin_t *)Model_Alloc((totalNumSurfaces + lodCount) * 4);
    MaterialHandle *materialHandles = (MaterialHandle *)((char *)skins + lodCount * 4);

    if (lodCount <= 0)
        return skins;

    for (i = 0; i < lodCount; i++) {
        int numSurfaces = XModelGetSurfaces(model, &surfaces, i, &partBits);

        *(MaterialHandle **)&skins[i] = materialHandles;
        materialHandles += numSurfaces;

        MaterialHandle *handles = *(MaterialHandle **)&skins[i];
        for (j = 0; j < numSurfaces; j++) {
            strcpy(materialName, XModelGetSurfaceName(model, j, i));
            strlwr(materialName);
            handles[j] = Material_RegisterHandle(materialName, 0, 8);
        }
    }

    return skins;
}
