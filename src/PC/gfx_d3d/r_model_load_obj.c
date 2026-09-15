#include "common_types.h"
#include "imports.h"

extern int XModelGetNumLods(const XModel *model);
extern int XModelGetSurfaces(const XModel *model, XSurface ***surfaces, int lodIndex, int **partBits);
extern const char *XModelGetSurfaceName(const XModel *model, int subMatIndex, int lod);
extern void *Model_Alloc(int size);
extern MaterialHandle Material_RegisterHandle(const char *name, int imageTrack, int materialType);
extern char *strlwr(char *);

trXSkin_t *R_LoadXSkins(struct XModel *model)
{
    XSurface **surfaces;
    int *partBits;
    char materialName[64];
    int lodCount;
    int totalNumSurfaces;
    int i, j;

    lodCount = XModelGetNumLods(model);

    totalNumSurfaces = 0;
    for (j = 0; j < lodCount; j++) {
        totalNumSurfaces += XModelGetSurfaces(model, &surfaces, j, &partBits);
    }

    /* x86 packed lodCount trXSkin_t ptrs + totalNumSurfaces MaterialHandles at 4
     * bytes each; on x64 both are 8-byte pointers, so the *4 sizing under-allocated
     * by half and handles[j]= writes overflowed into adjacent material blocks. */
    trXSkin_t *skins = (trXSkin_t *)Model_Alloc((int)((totalNumSurfaces * sizeof(MaterialHandle)) + (lodCount * sizeof(trXSkin_t))));
    MaterialHandle *materialHandles = (MaterialHandle *)((char *)skins + lodCount * sizeof(trXSkin_t));

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
