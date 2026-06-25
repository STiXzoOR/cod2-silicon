#include "common_types.h"
#include "imports.h"

int XSurfaceGetNumVerts(const XSurface *surface);
int XSurfaceGetNumTris(const XSurface *surface);
void XSurfaceGetTris(const XSurface *surface, r_index_t *dstIndices, int offset);
XVertexInfo *XSurfaceGetVertexInfoArray(const XSurface *surf);
int XSurfaceGetBoneOffset(const XSurface *surf);
void XSurfaceTransferDx7(const XVertexBuffer *surfVerts, GfxVertexDx7 *verts, int vertCount);
void XSurfaceTransfer(const XVertexBuffer *surfVerts, GfxVertex *verts, int vertCount);
long unsigned int XSurfaceGetVerts(const XSurface *surf, DObjSkelMat *boneMatrix, float *pVert, float *pTexCoord, float *pNormal);

int XSurfaceGetNumVerts(const XSurface *surface)
{
    return surface->vertCount;
}

int XSurfaceGetNumTris(const XSurface *surface)
{
    return surface->triCount;
}

void XSurfaceGetTris(const XSurface *surface, r_index_t *dstIndices, int offset)
{
    short sOffset = (short)offset;

    if (sOffset != 0) {

        unsigned int twoOffsets = ((unsigned int)(unsigned short)sOffset << 16) | (unsigned short)sOffset;
        unsigned int *dst = (unsigned int *)dstIndices;
        unsigned int *src = (unsigned int *)surface->triIndices;
        short numMoves = surface->triCount >> 1;
        int i;

        i = 0;
        do {
            dst[0] = src[0] + twoOffsets;
            dst[1] = src[1] + twoOffsets;
            dst[2] = src[2] + twoOffsets;
            dst += 3;
            src += 3;
            i++;
        } while (i != numMoves);
    } else {
        memcpy(dstIndices, surface->triIndices, surface->triCount * 6);
    }
}

XVertexInfo *XSurfaceGetVertexInfoArray(const XSurface *surf)
{
    return (XVertexInfo *)surf->verts;
}

int XSurfaceGetBoneOffset(const XSurface *surf)
{
    return surf->boneOffset;
}

void XSurfaceTransferDx7(const XVertexBuffer *surfVerts, GfxVertexDx7 *verts, int vertCount)
{
    int vertIndex;
    const char *v = (const char *)surfVerts;

    if (vertCount <= 0)
        return;

    for (vertIndex = 0; vertIndex < vertCount; vertIndex++) {
        const XVertexInfo *vi = (const XVertexInfo *)v;

        verts[vertIndex].xyz[0] = vi->offset[0];
        verts[vertIndex].xyz[1] = vi->offset[1];
        verts[vertIndex].xyz[2] = vi->offset[2];

        verts[vertIndex].normal[0] = vi->normal[0];
        verts[vertIndex].normal[1] = vi->normal[1];
        verts[vertIndex].normal[2] = vi->normal[2];

        verts[vertIndex].color.packed = *(const unsigned int *)vi->color;

        verts[vertIndex].texCoord[0] = vi->texCoordX;
        verts[vertIndex].texCoord[1] = vi->texCoordY;

        v += sizeof(XVertexInfo);
    }
}

void XSurfaceTransfer(const XVertexBuffer *surfVerts, GfxVertex *verts, int vertCount)
{
    int vertIndex;
    const char *v = (const char *)surfVerts;

    if (vertCount <= 0)
        return;

    for (vertIndex = 0; vertIndex < vertCount; vertIndex++) {
        const XVertexInfo *vi = (const XVertexInfo *)v;

        verts[vertIndex].xyzw[0] = vi->offset[0];
        verts[vertIndex].xyzw[1] = vi->offset[1];
        verts[vertIndex].xyzw[2] = vi->offset[2];

        verts[vertIndex].xyzw[3] = 1.0f;

        verts[vertIndex].normal[0] = vi->normal[0];
        verts[vertIndex].normal[1] = vi->normal[1];
        verts[vertIndex].normal[2] = vi->normal[2];

        verts[vertIndex].color.packed = *(const unsigned int *)vi->color;

        verts[vertIndex].texCoord[0] = vi->texCoordX;
        verts[vertIndex].texCoord[1] = vi->texCoordY;

        verts[vertIndex].binormal[0] = vi->binormal[0];
        verts[vertIndex].binormal[1] = vi->binormal[1];
        verts[vertIndex].binormal[2] = vi->binormal[2];

        verts[vertIndex].tangent[0] = vi->tangent[0];
        verts[vertIndex].tangent[1] = vi->tangent[1];
        verts[vertIndex].tangent[2] = vi->tangent[2];

        v += sizeof(XVertexInfo);
    }
}

static inline __attribute__((always_inline)) void XSurfaceRotateNormal(const float *normal, const DObjSkelMat *mat, float *out)
{
    out[0] = normal[0] * mat->axis[0][0] + normal[1] * mat->axis[1][0] + normal[2] * mat->axis[2][0];
    out[1] = normal[0] * mat->axis[0][1] + normal[1] * mat->axis[1][1] + normal[2] * mat->axis[2][1];
    out[2] = normal[0] * mat->axis[0][2] + normal[1] * mat->axis[1][2] + normal[2] * mat->axis[2][2];
}

static inline __attribute__((always_inline)) void XSurfaceTransformPos(const float *pos, const DObjSkelMat *mat, float *out)
{
    out[0] = pos[0] * mat->axis[0][0] + pos[1] * mat->axis[1][0] + pos[2] * mat->axis[2][0] + mat->origin[0];
    out[1] = pos[0] * mat->axis[0][1] + pos[1] * mat->axis[1][1] + pos[2] * mat->axis[2][1] + mat->origin[1];
    out[2] = pos[0] * mat->axis[0][2] + pos[1] * mat->axis[1][2] + pos[2] * mat->axis[2][2] + mat->origin[2];
}

static inline __attribute__((always_inline)) void XSurfaceTransformPosWeighted(const float *pos, const DObjSkelMat *mat, float scale, float *out)
{
    out[0] += (pos[0] * mat->axis[0][0] + pos[1] * mat->axis[1][0] + pos[2] * mat->axis[2][0] + mat->origin[0]) * scale;
    out[1] += (pos[0] * mat->axis[0][1] + pos[1] * mat->axis[1][1] + pos[2] * mat->axis[2][1] + mat->origin[1]) * scale;
    out[2] += (pos[0] * mat->axis[0][2] + pos[1] * mat->axis[1][2] + pos[2] * mat->axis[2][2] + mat->origin[2]) * scale;
}

long unsigned int XSurfaceGetVerts(const XSurface *surf, DObjSkelMat *boneMatrix, float *pVert, float *pTexCoord, float *pNormal)
{
    XVertexInfo *vertInfo;
    int vertCount;
    int i;

    vertInfo = (XVertexInfo *)surf->verts;
    short boneOffset = surf->boneOffset;

    if ((short)(boneOffset + 1) != 0) {

        DObjSkelMat *mat = (DObjSkelMat *)((char *)boneMatrix + boneOffset);

        vertCount = surf->vertCount;
        if (vertCount != 0) {
            for (i = 0; i < vertCount; i++) {
                if (pNormal) {
                    XSurfaceRotateNormal(vertInfo->normal, mat, pNormal);
                    pNormal += 3;
                }

                if (pTexCoord) {
                    pTexCoord[0] = vertInfo->texCoordX;
                    pTexCoord[1] = vertInfo->texCoordY;
                    pTexCoord += 2;
                }

                XSurfaceTransformPos(vertInfo->offset, mat, pVert);
                pVert += 3;
                vertInfo = (XVertexInfo *)((char *)vertInfo + sizeof(XVertexInfo));
            }
        }
    } else {

        vertCount = surf->vertCount;
        if (vertCount != 0) {
            const char *vdata = (const char *)vertInfo;

            pVert += 3;

            for (i = 0; i < vertCount; i++) {
                const XVertexInfo *v = (const XVertexInfo *)vdata;

                if (pNormal) {
                    DObjSkelMat *mat = (DObjSkelMat *)((char *)boneMatrix + v->boneOffset);
                    XSurfaceRotateNormal(v->normal, mat, pNormal);
                    pNormal += 3;
                }

                {
                    DObjSkelMat *mat = (DObjSkelMat *)((char *)boneMatrix + v->boneOffset);
                    XSurfaceTransformPos(v->offset, mat, pVert - 3);
                }

                if (v->numWeights != 0) {
                    float scale;
                    int j;
                    const XBlendInfo *blend;

                    scale = (float)v->boneWeight * 0.00390625f;
                    pVert[-3] *= scale;
                    pVert[-2] *= scale;
                    pVert[-1] *= scale;

                    blend = (const XBlendInfo *)(vdata + sizeof(XVertexInfo));
                    for (j = 0; j < v->numWeights; j++) {
                        DObjSkelMat *mat2 = (DObjSkelMat *)((char *)boneMatrix + blend->boneOffset);
                        float s = (float)blend->boneWeight * 1.52587890625e-05f;
                        XSurfaceTransformPosWeighted(blend->offset, mat2, s, pVert - 3);
                        blend++;
                    }
                    vdata = (const char *)blend;
                } else {
                    vdata += 0x40;
                }

                pVert += 3;
            }
        }
    }
}
