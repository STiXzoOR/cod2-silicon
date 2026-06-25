#include "common_types.h"

#    include "PC/gfx_d3d/r_xmodelsurfs_load.h"

extern void Com_Memcpy(void *dest, const void *src, int count);
extern MaterialHandle Material_RegisterHandle(const char *name, int baseImageFlags,
                                              int imageTrack);

void *R_LoadXModelSurfsSurface(void *surfsCtx, void *partBitsArg,
                               int **streamCursor, void *(*alloc)(int size))
{
    unsigned char *ctx = (unsigned char *)surfsCtx;

    unsigned int *partBits = (unsigned int *)partBitsArg;
    int *memUsage = (int *)(ctx + 0x84);
    unsigned char *surf;
    unsigned char *c;
    unsigned char *v;
    int vertCount, triCount, marker;
    int skinned;
    int vertBufSize;
    int vi, idxCount, roundedIdx, ii;
    short *tris;

    surf = (unsigned char *)alloc(0x18);
    c = (unsigned char *)*streamCursor;
    *memUsage += 0x18;

    ((XSurface *)surf)->tileMode = *c++;
    vertCount = *(unsigned short *)c;
    c += 2;
    (*(unsigned short *)&((XSurface *)surf)->vertCount) = (unsigned short)vertCount;
    triCount = *(unsigned short *)c;
    c += 2;
    (*(unsigned short *)&((XSurface *)surf)->triCount) = (unsigned short)triCount;
    marker = (short)*(unsigned short *)c;
    c += 2;

    if (marker == -1) {

        int extraVerts = (short)*(unsigned short *)c;
        c += 2;
        skinned = 1;
        (*(unsigned short *)&((XSurface *)surf)->boneOffset) = 0xffff;
        vertBufSize = (extraVerts + vertCount * 4) << 4;
    } else {

        skinned = 0;
        (*(unsigned short *)&((XSurface *)surf)->boneOffset) = (unsigned short)(marker << 6);
        partBits[marker >> 5] |= (1u << (marker & 0x1f));
        vertBufSize = vertCount << 6;
    }

    v = (unsigned char *)alloc(vertBufSize);
    *memUsage += vertBufSize;
    (*(void **)&((XSurface *)surf)->verts) = v;

    for (vi = 0; vi < vertCount; vi++) {

        *(unsigned int * *)((char *)v + offsetof(GfxVertex, xyzw[0])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, xyzw[1])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, xyzw[2])) = *(unsigned int *)c;
        c += 4;
        v[0x0c] = c[0];
        v[0x0d] = c[1];
        v[0x0e] = c[2];
        v[0x0f] = c[3];
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, color.packed)) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, binormal[1])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, normal[0])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, normal[1])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, normal[2])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, texCoord[0])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, texCoord[1])) = *(unsigned int *)c;
        c += 4;
        *(unsigned int * *)((char *)v + offsetof(GfxVertex, binormal[0])) = *(unsigned int *)c;
        c += 4;

        if (skinned) {
            int numWeights = *c++;
            int b0;
            v[0x3c] = (unsigned char)numWeights;
            b0 = (short)*(unsigned short *)c;
            c += 2;
            partBits[b0 >> 5] |= (1u << (b0 & 0x1f));
            *(unsigned short *)(v + 0x3e) = (unsigned short)(b0 << 6);
            *(unsigned int * *)((char *)v + offsetof(GfxVertex, binormal[2])) = *(unsigned int *)c;
            c += 4;
            *(unsigned int * *)((char *)v + offsetof(GfxVertex, tangent[0])) = *(unsigned int *)c;
            c += 4;
            *(unsigned int * *)((char *)v + offsetof(GfxVertex, tangent[1])) = *(unsigned int *)c;
            c += 4;

            if (numWeights != 0) {
                int w;
                unsigned char *wp = v + 0x40;

                v[0x3d] = *c++;

                for (w = 0; w < numWeights; w++, wp += 0x10) {
                    int bn = (short)*(unsigned short *)c;
                    c += 2;
                    partBits[bn >> 5] |= (1u << (bn & 0x1f));
                    *(unsigned short *)(wp + 0x0c) = (unsigned short)(bn << 6);
                    *(unsigned int *)(wp + 0x00) = *(unsigned int *)c;
                    c += 4;
                    *(unsigned int *)(wp + 0x04) = *(unsigned int *)c;
                    c += 4;
                    *(unsigned int *)(wp + 0x08) = *(unsigned int *)c;
                    c += 4;
                    *(unsigned short *)(wp + 0x0e) = *(unsigned short *)c;
                    c += 2;
                }
            }

            v += 0x40 + numWeights * 0x10;
        } else {
            *(unsigned int * *)((char *)v + offsetof(GfxVertex, binormal[2])) = *(unsigned int *)c;
            c += 4;
            *(unsigned int * *)((char *)v + offsetof(GfxVertex, tangent[0])) = *(unsigned int *)c;
            c += 4;
            *(unsigned int * *)((char *)v + offsetof(GfxVertex, tangent[1])) = *(unsigned int *)c;
            c += 4;
            v += 0x40;
        }
    }

    triCount = (short)(*(unsigned short *)&((XSurface *)surf)->triCount);
    roundedIdx = (triCount + 1) & ~1;
    idxCount = triCount * 3;
    tris = (short *)alloc((idxCount + 3) * 2);
    (*(void **)&((XSurface *)surf)->triIndices) = tris;

    for (ii = 0; ii < idxCount; ii++) {
        tris[ii] = *(short *)c;
        c += 2;
    }
    if (roundedIdx != triCount) {
        tris[ii] = tris[ii - 1];
        tris[ii + 1] = tris[ii - 1];
        tris[ii + 2] = tris[ii - 1];
        (*(unsigned short *)&((XSurface *)surf)->triCount) += 1;
    }

    *streamCursor = (int *)c;
    return surf;
}

typedef void *(*v60_alloc_t)(int size);
typedef int (*v60_numlods_t)(void *model);
typedef int (*v60_surfcount_t)(void *model, void **outSurfs, int lod, void **outNames);
typedef const char *(*v60_surfname_t)(void *model, int surfIdx, int lod);

static int *gRefImportV60Copy[181];
static int **gRefImportV60;

void R_XModelSurfs_SetRefImport(void *rimp_v60)
{
    if (rimp_v60) {
        Com_Memcpy(gRefImportV60Copy, rimp_v60, 181 * (int)sizeof(int *));
        gRefImportV60 = gRefImportV60Copy;
    }
}

void *R_RegisterXModelSurfs(void *xmodel)
{
    v60_alloc_t alloc;
    v60_numlods_t getNumLods;
    v60_surfcount_t getSurfCount;
    v60_surfname_t getSurfName;
    int numLods, totalSurfs;
    int lod, s;
    void **lodArray;
    void **pool;

    if (!gRefImportV60 || !xmodel)
        return xmodel;

    alloc = (v60_alloc_t)gRefImportV60[3];
    getNumLods = (v60_numlods_t)gRefImportV60[97];
    getSurfCount = (v60_surfcount_t)gRefImportV60[94];
    getSurfName = (v60_surfname_t)gRefImportV60[101];

    numLods = getNumLods(xmodel);
    if (numLods <= 0)
        return xmodel;

    totalSurfs = 0;
    for (lod = 0; lod < numLods; lod++) {
        void *surfs = 0, *names = 0;
        totalSurfs += getSurfCount(xmodel, &surfs, lod, &names);
    }

    lodArray = (void **)alloc((totalSurfs + numLods) << 2);
    pool = lodArray + numLods;

    for (lod = 0; lod < numLods; lod++) {
        void *surfs = 0, *names = 0;
        int count = getSurfCount(xmodel, &surfs, lod, &names);

        lodArray[lod] = pool;

        for (s = 0; s < count; s++) {
            const char *name = getSurfName(xmodel, s, lod);
            MaterialHandle h = Material_RegisterHandle(name ? name : "", 0, 8);
            pool[s] = (void *)(intptr_t)h;
        }
        pool += count;
    }

    return lodArray;
}
