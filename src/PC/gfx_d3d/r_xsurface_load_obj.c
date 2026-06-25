#include "common_types.h"
#include "imports.h"

static inline __attribute__((always_inline)) unsigned short ConsumeUShort(const byte **pos)
{
    unsigned short val = *(const unsigned short *)(*pos);
    *pos += 2;
    return val;
}

static inline __attribute__((always_inline)) short ConsumeShort(const byte **pos)
{
    short val = *(const short *)(*pos);
    *pos += 2;
    return val;
}

static inline __attribute__((always_inline)) int ConsumeInt(const byte **pos)
{
    int val = *(const int *)(*pos);
    *pos += 4;
    return val;
}

static inline __attribute__((always_inline)) unsigned char ConsumeByte(const byte **pos)
{
    unsigned char val = **pos;
    *pos += 1;
    return val;
}

XSurface *XModelReadSurface(XModel *model, int *partBits, const byte **pos, Alloc_t Alloc)
{
    XSurface *surface;
    int j;
    int i;
    short boneOffsetRaw;
    int numTriIndices;
    int allocSize;
    int size;
    short *triIndicesShort;

    surface = (XSurface *)Alloc(0x18);
    model->memUsage += 0x18;

    surface->tileMode = ConsumeByte(pos);

    surface->vertCount = (short)ConsumeUShort(pos);

    surface->triCount = (short)ConsumeUShort(pos);

    boneOffsetRaw = ConsumeShort(pos);

    if (boneOffsetRaw == -1) {

        short extraBlendCount;
        extraBlendCount = ConsumeShort(pos);

        surface->boneOffset = (short)0xFFFF;

        size = ((int)extraBlendCount + (int)surface->vertCount * 4) * 16;
        surface->verts = (XVertexBuffer *)Alloc(size);
        model->memUsage += size;
    } else {

        surface->boneOffset = (short)(boneOffsetRaw << 6);
        partBits[boneOffsetRaw >> 5] |= (1 << (boneOffsetRaw & 0x1f));

        size = (int)surface->vertCount * 64;
        surface->verts = (XVertexBuffer *)Alloc(size);
        model->memUsage += size;
    }

    {
        byte *vertPtr = (byte *)surface->verts;

        for (j = 0; j < (int)surface->vertCount; j++) {
            XVertexInfo *vi = (XVertexInfo *)vertPtr;

            *(int *)&vi->normal[0] = ConsumeInt(pos);
            *(int *)&vi->normal[1] = ConsumeInt(pos);
            *(int *)&vi->normal[2] = ConsumeInt(pos);

            vi->color[0] = (byte)(**pos);
            vi->color[1] = (byte)(*(*pos + 1));
            vi->color[2] = (byte)(*(*pos + 2));
            vi->color[3] = (byte)(*(*pos + 3));
            *pos += 4;

            *(int *)&vi->texCoordX = ConsumeInt(pos);

            *(int *)&vi->texCoordY = ConsumeInt(pos);

            *(int *)&vi->binormal[0] = ConsumeInt(pos);
            *(int *)&vi->binormal[1] = ConsumeInt(pos);
            *(int *)&vi->binormal[2] = ConsumeInt(pos);

            *(int *)&vi->tangent[0] = ConsumeInt(pos);
            *(int *)&vi->tangent[1] = ConsumeInt(pos);
            *(int *)&vi->tangent[2] = ConsumeInt(pos);

            if (boneOffsetRaw != -1) {

                *(int *)&vi->offset[0] = ConsumeInt(pos);
                *(int *)&vi->offset[1] = ConsumeInt(pos);
                *(int *)&vi->offset[2] = ConsumeInt(pos);

                vertPtr += sizeof(XVertexInfo);
            } else {

                byte numWeights;
                short vertBoneOffset;
                int k;

                numWeights = ConsumeByte(pos);
                vi->numWeights = numWeights;

                vertBoneOffset = ConsumeShort(pos);
                partBits[vertBoneOffset >> 5] |= (1 << (vertBoneOffset & 0x1f));
                vi->boneOffset = (short)(vertBoneOffset << 6);

                *(int *)&vi->offset[0] = ConsumeInt(pos);
                *(int *)&vi->offset[1] = ConsumeInt(pos);
                *(int *)&vi->offset[2] = ConsumeInt(pos);

                if (numWeights == 0) {
                    vertPtr += sizeof(XVertexInfo);
                } else {

                    vi->boneWeight = ConsumeByte(pos);

                    vertPtr += sizeof(XVertexInfo);

                    for (k = 0; k < (int)numWeights; k++) {
                        XBlendInfo *blend = (XBlendInfo *)vertPtr;
                        short blendBoneOffset;

                        blendBoneOffset = ConsumeShort(pos);
                        partBits[blendBoneOffset >> 5] |= (1 << (blendBoneOffset & 0x1f));
                        blend->boneOffset = (short)(blendBoneOffset << 6);

                        *(int *)&blend->offset[0] = ConsumeInt(pos);
                        *(int *)&blend->offset[1] = ConsumeInt(pos);
                        *(int *)&blend->offset[2] = ConsumeInt(pos);

                        blend->boneWeight = ConsumeUShort(pos);

                        vertPtr += sizeof(XBlendInfo);
                    }
                }
            }
        }
    }

    {
        int roundedTriCount = ((int)surface->triCount + 1) & ~1;

        numTriIndices = (int)surface->triCount * 3;
        allocSize = numTriIndices * 2 + 6;
        surface->triIndices = (int (*)[4])Alloc(allocSize);

        triIndicesShort = (short *)surface->triIndices;

        for (i = 0; i < (int)surface->triCount * 3; i++) {
            triIndicesShort[i] = (short)ConsumeUShort(pos);
        }

        if ((int)surface->triCount != roundedTriCount) {
            triIndicesShort[i + 0] = triIndicesShort[i - 1];
            triIndicesShort[i + 1] = triIndicesShort[i + 0];
            triIndicesShort[i + 2] = triIndicesShort[i + 1];
            surface->triCount += 1;
        }
    }

    return surface;
}
