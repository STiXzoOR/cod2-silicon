#include "common_types.h"
#include "Mac/DirectX_9/lp64_buffers.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define D3DVTCC
DxGlobals dx;
static DxGlobals *const dx_g = &dx;
DxState dxState;
int alwaysfails;
volatile int g_lastdraw[6];
static materialCommands_t tess;
static r_backEndGlobals_t backEnd;
static GfxViewParms viewParms;
void *imp_tess = &tess, *imp_backEnd = &backEnd;
static dvar_t renderer;
dvar_t *r_rendererInUse = &renderer;
static CDirect3DVertexBufferClean vertexBuffer;
static CDirect3DIndexBufferClean indexBuffer;
static int draws;

static char *RB_TessBase(void) { return (char *)&tess; }
void RB_EndSurface(void) { abort(); }
void *R_AllocStaticVertexBuffer(IDirect3DVertexBuffer9 **buffer, int size)
{
    vertexBuffer.lengthBytes = size;
    vertexBuffer.data = calloc(1, size);
    assert(vertexBuffer.data);
    *buffer = (IDirect3DVertexBuffer9 *)&vertexBuffer;
    return vertexBuffer.data;
}
void *R_AllocStaticIndexBuffer(IDirect3DIndexBuffer9 **buffer, int size)
{
    indexBuffer.lengthBytes = size;
    indexBuffer.indexSizeBytes = 2;
    indexBuffer.data = calloc(1, size);
    assert(indexBuffer.data);
    *buffer = (IDirect3DIndexBuffer9 *)&indexBuffer;
    return indexBuffer.data;
}
void R_FinishStaticVertexBuffer(IDirect3DVertexBuffer9 *buffer) { assert(buffer == (void *)&vertexBuffer); }
void R_FinishStaticIndexBuffer(IDirect3DIndexBuffer9 *buffer) { assert(buffer == (void *)&indexBuffer); }
void RB_ChangeIndices(IDirect3DIndexBuffer9 *buffer) { dxState.indexBuffer = buffer; }
void RB_ChangeStreamSource(int stream, IDirect3DVertexBuffer9 *buffer, int offset, int stride)
{
    assert(stream == 0);
    dxState.streams[0].vb = buffer;
    dxState.streams[0].offset = offset;
    dxState.streams[0].stride = stride;
}
void RB_DrawIndexedPrim(const GfxDrawPrimArgs *args, int primCount);
void RB_DrawTechnique(MaterialVertexDeclType type, const GfxDrawPrimArgs *args)
{
    assert(type == 2);
    RB_DrawIndexedPrim(args, args->primCount);
}
extern const vec_t Vec3Normalize(vec_t *);
extern int VecNCompareCustomEpsilon(const vec_t *, const vec_t *, float, int);
extern void Vec3RotateTranspose(const vec_t *, vec3_t *, vec_t *);

static HRESULT DrawIndexed(IDirect3DDevice9 *device, D3DPRIMITIVETYPE type, INT baseVertex,
                           UINT minVertex, UINT vertexCount, UINT baseIndex, UINT primCount)
{
    (void)device;
    assert(type == D3DPT_TRIANGLELIST && baseVertex == 0 && minVertex == 0);
    assert(vertexCount == 4096 && baseIndex == 0 && primCount == 2048);
    assert(vertexCount * dxState.streams[0].stride <= vertexBuffer.lengthBytes);
    assert((baseIndex + primCount * 3) * indexBuffer.indexSizeBytes <= indexBuffer.lengthBytes);
    const r_index_t *indices = (const r_index_t *)indexBuffer.data;
    for (UINT i = 0; i < primCount * 3; i += 3) {
        assert(indices[i] < vertexCount && indices[i + 1] < vertexCount && indices[i + 2] < vertexCount);
        assert(indices[i] != indices[i + 1] && indices[i] != indices[i + 2] && indices[i + 1] != indices[i + 2]);
    }
    ++draws;
    return 0;
}
#include "fx_cloud_source.h"

int main(void)
{
    void *vtable[84] = {0};
    void **device = vtable;
    vtable[0x148 / 4] = (void *)DrawIndexed;
    dx.device = (void *)&device;
    backEnd.viewParms = &viewParms;
    renderer.current.integer = 1;
    R_CreateParticleCloudBuffer();
    GfxEntity cloud = {.radius = {4,4}, .origin = {1,2,3}, .endpos = {1,2,3}, .materialRGBA = {128,64,32,255}};
    RB_TessParticleCloud(&cloud);
    assert(draws == 1);
    float *vertices = (float *)vertexBuffer.data;
    assert(vertices[3 * 5 + 3] == 1 && vertices[3 * 5 + 4] == 1);
    renderer.current.integer = 2;
    RB_TessParticleCloud(&cloud);
    assert(draws == 1);
    free(vertexBuffer.data);
    free(indexBuffer.data);
    puts("native FX cloud: real buffers, nondegenerate triangles and bounded indexed dispatch passed");
    return 0;
}
