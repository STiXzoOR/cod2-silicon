#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shade_source.h"
#include "water_source.h"
#include "Mac/DirectX_9/lp64_buffers.h"

#define VTABLE(object) (*(void ***)(object))
typedef HRESULT(D3DVTCC *SetTextureStageStateFn)(void *, DWORD, D3DTEXTURESTAGESTATETYPE, DWORD);
typedef HRESULT(D3DVTCC *SetTransformFn)(void *, D3DTRANSFORMSTATETYPE, const D3DMATRIX *);
typedef HRESULT(D3DVTCC *SetTextureFn)(void *, DWORD, IDirect3DBaseTexture9 *);
typedef HRESULT(D3DVTCC *SetSamplerStateFn)(void *, DWORD, DWORD, DWORD);
extern void MatrixIdentity44(float (*)[4]);
#define RB_SetSampler RB_ProductionSetSampler
#include "water_helpers.h"
#undef RB_SetSampler
extern void FFT_Init(int *, complex_t *);

DxGlobals dx;
struct DxState dxState;
materialCommands_t tess;
r_global_permanent_t rgp;
r_globals_t rg;
r_backEndGlobals_t backEnd;
refimport_t ri;
GfxBackEndData *backEndData;
void *imp_tess = &tess, *imp_rgp = &rgp, *imp_backEnd = &backEnd, *imp_dx = &dx;
static byte special, uploading;
void *imp_g_special = &special, *imp_g_WarmOff = &uploading;
static dvar_t renderer, drawWater, zeroDvar;
dvar_t *r_rendererInUse = &renderer;
static const dvar_t *drawWaterPtr = &drawWater;
void *imp_r_drawWater = &drawWaterPtr;
const dvar_t *r_drawPrimFloor = &zeroDvar, *r_drawPrimCap = &zeroDvar;
const dvar_t *r_debugShader = &zeroDvar;
const dvar_t *r_objectiveColorDx7Min = &zeroDvar, *r_objectiveColorDx7Max = &zeroDvar;
const dvar_t *sc_enable = &zeroDvar, *r_lightMap = &zeroDvar;
const dvar_t *r_outdoorAwayBias = &zeroDvar, *r_outdoorDownBias = &zeroDvar;
int alwaysfails;
volatile int g_rigid_step, g_lastdraw[6];
static void *deviceVtable[120];
static struct { void **vtable; } device = { deviceVtable };
static int draws, bindings, stageCalls[2], transformCalls[2], uploadCount;
static const GfxImage *boundImages[16];
static byte boundStates[16];
static int requestedStates[16];
static DWORD samplerStates[16][14];
static int textureCalls;
static IDirect3DBaseTexture9 *deviceTextures[16];
static DWORD coordIndex[2], transformFlags[2];
static D3DMATRIX transforms[2];
static struct { D3DFORMAT format; int mip, size; byte bytes[128]; } uploads[8];
static byte vertexSnapshot[136];
static int vertexSnapshotSize;

void R_Error(int level, const char *format, ...)
{ (void)level; (void)format; assert(0); }
void Com_Error(int level, const char *format, ...)
{ (void)level; (void)format; assert(0); }
void RB_ChangeState_0(int bits) { (void)bits; }
void RB_ChangeState_1(int bits) { (void)bits; }
void RB_UpdateFogColor(FogColorSrcEnum source) { (void)source; }
void RB_SetSamplerConstantDx7(unsigned int color) { (void)color; }
void RB_ChangeColorStageState(int stage, int bits) { (void)stage; (void)bits; }
void RB_ChangeAlphaStageState(int stage, int bits) { (void)stage; (void)bits; }
void RB_SetViewMatrixForWDx7(float w) { (void)w; }
void Com_Memcpy(void *dst, const void *src, int size) { memcpy(dst, src, size); }
void R_FatalLockError(HRESULT result) { (void)result; assert(0); }
Bool RB_GetViewport(GfxViewport *viewport) { (void)viewport; assert(0); return 0; }
int MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(float *m, float w, float h)
{ (void)m; (void)w; (void)h; return 0; }

void RB_SetSampler(int index, int state, GfxImage *image)
{
    assert(index >= 0 && index < 16 && !uploading);
    requestedStates[index] = state;
    RB_ProductionSetSampler(index, state, image);
    boundImages[index] = dxState.samplerImage[index];
    boundStates[index] = dxState.samplerState[index];
    ++bindings;
}

static HRESULT SetTexture(void *object, DWORD index, IDirect3DBaseTexture9 *texture)
{
    assert(object == &device && index < 16 && !uploading);
    const GfxImage *image = (const void *)texture;
    if (image && image->mapType == 3 && drawWater.current.enabled)
        assert(uploadCount > 0);
    deviceTextures[index] = texture;
    ++textureCalls;
    return 0;
}

static HRESULT SetSamplerState(void *object, DWORD index, DWORD state, DWORD value)
{
    assert(object == &device && index < 16 && state < 14 && !uploading);
    samplerStates[index][state] = value;
    return 0;
}

static HRESULT Draw(void *object, D3DPRIMITIVETYPE type, INT base, UINT first,
                    UINT count, UINT index, UINT primitives)
{
    assert(object == &device && type == D3DPT_TRIANGLELIST && primitives == 1);
    (void)base; (void)first; (void)count; (void)index;
    if (vertexSnapshotSize) {
        const CDirect3DVertexBufferClean *buffer = (const void *)dxState.streams[0].vb;
        memcpy(vertexSnapshot, buffer->data + dxState.streams[0].offset, vertexSnapshotSize);
    }
    ++draws;
    return 0;
}

static HRESULT SetStage(void *object, DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value)
{
    assert(object == &device && stage < 2);
    if (type == D3DTSS_TEXCOORDINDEX)
        coordIndex[stage] = value;
    else {
        assert(type == D3DTSS_TEXTURETRANSFORMFLAGS);
        transformFlags[stage] = value;
    }
    ++stageCalls[stage];
    return 0;
}

static HRESULT SetTransform(void *object, D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix)
{
    assert(object == &device && state >= D3DTS_TEXTURE0 && state < D3DTS_TEXTURE0 + 2);
    int stage = state - D3DTS_TEXTURE0;
    transforms[stage] = *matrix;
    ++transformCalls[stage];
    return 0;
}

static HRESULT SetDeclaration(void *object, IDirect3DVertexDeclaration9 *declaration)
{ assert(object == &device && declaration); return 0; }
static HRESULT SetFvf(void *object, DWORD fvf)
{ assert(object == &device && fvf); return 0; }

void Image_UploadData(GfxImage *image, D3DFORMAT format, int face, int mip, const byte *pixels)
{
    assert(uploading && face == 0 && image && uploadCount < 8);
    int width = image->width >> mip, height = image->height >> mip;
    if (!width) width = 1;
    if (!height) height = 1;
    int size = width * height * (format == D3DFMT_L8 ? 1 : 4);
    assert(size <= sizeof(uploads[0].bytes));
    uploads[uploadCount].format = format;
    uploads[uploadCount].mip = mip;
    uploads[uploadCount].size = size;
    memcpy(uploads[uploadCount].bytes, pixels, size);
    ++uploadCount;
}

static complex_t spectrum[16];
static float frequencies[16];
static water_t water;
static GfxImage image;
static GfxViewParms view;

static void Init(void)
{
    backEndData = calloc(1, sizeof(*backEndData));
    assert(backEndData);
    backEndData->frameCount = 1;
    backEnd.viewParms = &view;
    MatrixIdentity44(view.viewMatrix.m);
    MatrixIdentity44(view.projectionMatrix.m);
    FFT_Init(rg.fftBitswap, rg.fftTrigTable);
    for (int i = 0; i < 1024; ++i)
        rg.sinTable[i] = sinf((float)((double)i * 6.283185307179586 / 1024));
    for (int i = 0; i < 16; ++i) {
        spectrum[i].real = (i % 5 - 2) * 0.09f;
        spectrum[i].imag = (i % 3 - 1) * 0.13f;
        frequencies[i] = i ? (i % 7 + 1) * 0.2f : 0.0f;
    }
    spectrum[0].real = 1.0f;
    spectrum[0].imag = 2.0f;
    water = (water_t){ .M = 4, .N = 4, .H0 = spectrum, .wTerm = frequencies, .image = &image };
    image = (GfxImage){ .mapType = 3, .semantic = 5, .width = 4, .height = 4 };
    image.texture.basemap = (void *)&image;
    drawWater.current.enabled = 1;
    renderer.flags = 0xffff;
    renderer.type = renderer.modified = 0xff;
    deviceVtable[0x148 / 4] = Draw;
    deviceVtable[0x10c / 4] = SetStage;
    deviceVtable[0xb0 / 4] = SetTransform;
    deviceVtable[0x15c / 4] = SetDeclaration;
    deviceVtable[0x164 / 4] = SetFvf;
    deviceVtable[0x104 / 4] = SetTexture;
    deviceVtable[0x114 / 4] = SetSamplerState;
    dx.device = (void *)&device;
}

/* Independent direct Fourier sum; no FFT code or production output reused. */
static void ExpectedHeights(float time, float heights[16])
{
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            double real = 0, imag = 0;
            for (int v = 0; v < 4; ++v) {
                for (int u = 0; u < 4; ++u) {
                    int i = v * 4 + u;
                    if (!frequencies[i]) continue;
                    int phase = (int)floorf((float)((double)(frequencies[i] * time) *
                                                   162.97466172610083) + 0.5f) & 1023;
                    float a = spectrum[i].real * rg.sinTable[(phase + 255) & 1023];
                    float b = spectrum[i].imag * rg.sinTable[phase];
                    double angle = 6.283185307179586 * (u * x + v * y) / 4;
                    real += a * cos(angle) - b * sin(angle);
                    imag += a * sin(angle) + b * cos(angle);
                }
            }
            heights[y * 4 + x] = (float)sqrt(real * real + imag * imag);
        }
    }
}

static void CheckPayload(int normal)
{
    float heights[16];
    ExpectedHeights(backEnd.sceneDef.floatTime, heights);
    if (normal) {
        assert(uploadCount == 1 && uploads[0].format == D3DFMT_A8R8G8B8);
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                int i = y * 4 + x;
                /* Mac 1.3 carries the preceding linear index across rows. */
                float a = heights[i] - heights[i ? i - 1 : 3];
                float b = heights[i] - heights[((y + 3) & 3) * 4 + x];
                float scale = 127.5f / sqrtf(a * a + b * b + 256.0f);
                const byte *pixel = uploads[0].bytes + i * 4;
                assert(pixel[0] == 255);
                int expected[] = { (int)floorf(a * scale + 128.0f),
                                   (int)floorf(b * scale + 128.0f),
                                   (int)floorf(16 * scale + 128.0f) };
                for (int c = 0; c < 3; ++c) {
                    if (abs((int)pixel[c + 1] - expected[c]) > 1)
                        fprintf(stderr, "normal pixel %d channel %d: got %u expected %d\n",
                                i, c, pixel[c + 1], expected[c]);
                    assert(abs((int)pixel[c + 1] - expected[c]) <= 1);
                }
            }
        }
    } else {
        byte expected[16];
        assert(uploadCount == 3);
        for (int i = 0; i < 16; ++i)
            expected[i] = (byte)(int)floorf(fminf(heights[i] / 16, 1) * 255 + 0.5f);
        for (int mip = 0, width = 4; mip < 3; ++mip, width >>= 1) {
            assert(uploads[mip].format == D3DFMT_L8 && uploads[mip].mip == mip);
            assert(!memcmp(uploads[mip].bytes, expected, width * width));
            for (int y = 0; y < width / 2; ++y)
                for (int x = 0; x < width / 2; ++x) {
                    int i = 2 * y * width + 2 * x;
                    expected[y * (width / 2) + x] = (expected[i] + expected[i + 1] +
                                                     expected[i + width] + expected[i + width + 1]) / 4;
                }
        }
    }
}

static void DrawTechnique(MaterialTechnique *technique, Material *material, int mode)
{
    MaterialTechniqueSet set = {0};
    GfxDrawPrimArgs args = { .vertexCount = 3, .primCount = 1 };
    set.techniques[0] = technique;
    material->techniqueSet = &set;
    tess.material = material;
    renderer.current.integer = mode;
    RB_DrawSingleTechnique(0, 0, &args, NULL);
    assert(!special);
}

static void CheckDraw(void)
{
    MaterialStateMapRuleSet rules = { .ruleCount = 1,
                                     .rules = {{ .stateBitsClear = { -1, -1 } }} };
    MaterialStateMap map = {0};
    for (int i = 0; i < 11; ++i) map.ruleSet[i] = &rules;
    MaterialWaterDef definition = { .map = &water };
    MaterialTextureDef texture = { .name = "synthetic-water", .semantic = 5,
                                   .samplerState = 0x32, .u.water = &definition };
    Material material = { .textureCount = 1, .textures = &texture };
    MaterialTechnique technique = { .passCount = 1 };
    MaterialVertexDeclaration declaration = {0};
    declaration.decl[0] = (void (*)())&declaration;
    char matchingName[] = "synthetic-water";
    assert(matchingName != texture.name);
    MaterialShaderArgument arguments[2] = {{ .type = 4, .dest = 5, .u.name = matchingName },
                                          { .type = 4, .dest = 7, .u.name = texture.name }};
    technique.passArray.dx9[0].stateMap = &map;
    technique.passArray.dx9[0].vertexDecl = &declaration;
    technique.passArray.dx9[0].pixelArgs = arguments;
    technique.passArray.dx9[0].pixelArgCount = 2;
    backEnd.sceneDef.floatTime = 1.5f;
    DrawTechnique(&technique, &material, 0);
    assert(draws == 1 && bindings == 2 && uploadCount == 3);
    assert(boundImages[5] == &image && boundStates[5] == 6 && boundStates[7] == 6);
    assert(deviceTextures[5] == image.texture.basemap && deviceTextures[7] == image.texture.basemap);
    assert(samplerStates[5][D3DSAMP_MINFILTER] == D3DTEXF_LINEAR);
    assert(samplerStates[5][D3DSAMP_MAGFILTER] == D3DTEXF_LINEAR);
    assert(samplerStates[5][D3DSAMP_MIPFILTER] == D3DTEXF_POINT);
    CheckPayload(0);
    arguments[0].u.name = "missing-material-texture";
    arguments[1].u.name = "missing-material-texture";
    int calls = bindings;
    DrawTechnique(&technique, &material, 0);
    /* Existing DX9 missing-name handling skips a binding and keeps the cache. */
    assert(bindings == calls && uploadCount == 3 && boundImages[5] == &image);
    arguments[0].u.name = matchingName;
    arguments[1].u.name = texture.name;
    uploadCount = 0;
    ++backEndData->frameCount;
    memset(&technique.passArray, 0, sizeof(technique.passArray));
    technique.passArray.dx7[0].stateMap = &map;
    technique.passArray.dx7[0].samplers[0] = arguments[0];
    technique.passArray.dx7[0].samplers[1] = (MaterialShaderArgument){ .type = 3, .u.codeSampler = 0 };
    technique.passArray.dx7[0].genTexCoords[0] = 1;
    technique.passArray.dx7[0].genTexCoords[1] = 2;
    GfxImage black = {0};
    black.texture.basemap = (void *)&black;
    rgp.blackImage = &black;
    backEnd.texScrollAmountDx7 = 0.25f;
    view.depthHackNearClip = 12;
    view.viewMatrix._11 = 2;
    view.viewMatrix._22 = 4;
    view.viewMatrix._33 = 8;
    view.viewMatrix._41 = 9;
    view.viewMatrix._42 = 7;
    view.viewMatrix._43 = 5;
    DrawTechnique(&technique, &material, 2);
    assert(draws == 3 && bindings == 4 && uploadCount == 1);
    assert(boundImages[0] == &image && boundStates[0] == 6);
    assert(boundImages[1] == &black && boundStates[1] == 1);
    assert(transformCalls[0] == 1 && transformCalls[1] == 1 && transforms[1]._32 == -0.25f);
    assert(coordIndex[0] == 0x20000 && transformFlags[0] == D3DTTFF_COUNT3);
    assert(transforms[0]._11 == 0.5f && transforms[0]._22 == 0.25f && transforms[0]._33 == 0.125f);
    assert(!transforms[0]._41 && !transforms[0]._42 && !transforms[0]._43);
    CheckPayload(1);
    backEnd.texScrollAmountDx7 = 0.75f;
    calls = textureCalls;
    DrawTechnique(&technique, &material, 2);
    assert(uploadCount == 1 && transformCalls[0] == 1 && transformCalls[1] == 2);
    assert(textureCalls == calls);
    assert(transforms[1]._32 == -0.75f);
    technique.passArray.dx7[0].genTexCoords[0] = 0;
    technique.passArray.dx7[0].genTexCoords[1] = 0;
    DrawTechnique(&technique, &material, 2);
    assert(coordIndex[0] == 0 && coordIndex[1] == 1);
    assert(transformFlags[0] == D3DTTFF_DISABLE && transformFlags[1] == D3DTTFF_DISABLE);
    texture.semantic = 0;
    texture.u.image = &black;
    uploadCount = 0;
    DrawTechnique(&technique, &material, 2);
    assert(boundImages[0] == &black && boundStates[0] == 0x32 && !uploadCount);
    assert(samplerStates[0][D3DSAMP_ADDRESSU] == D3DTADDRESS_CLAMP);
    assert(samplerStates[0][D3DSAMP_ADDRESSV] == D3DTADDRESS_CLAMP);
    texture.semantic = 5;
    texture.u.water = &definition;
    image.mapType = 2;
    DrawTechnique(&technique, &material, 2);
    assert(boundImages[0] == &image && boundStates[0] == 0x32 && !uploadCount);
    image.mapType = 3;
    ++backEndData->frameCount;
    DrawTechnique(&technique, &material, 2);
    assert(uploadCount == 1 && boundStates[0] == 6);
    assert(samplerStates[0][D3DSAMP_ADDRESSU] == D3DTADDRESS_WRAP);
    assert(samplerStates[0][D3DSAMP_ADDRESSV] == D3DTADDRESS_WRAP);
    image.semantic = 1;
    ++backEndData->frameCount;
    uploadCount = 0;
    DrawTechnique(&technique, &material, 2);
    /* Original dispatch uses material semantic, independently of image semantic. */
    assert(uploadCount == 1 && boundStates[0] == 6);
    technique.passArray.dx7[0].samplers[0].u.name = "missing-material-texture";
    technique.passArray.dx7[0].samplers[1].u.codeSampler = 24;
    uploadCount = 0;
    DrawTechnique(&technique, &material, 2);
    assert(!boundImages[0] && !boundImages[1] && !deviceTextures[0] && !deviceTextures[1]);
    assert(requestedStates[0] == 0 && requestedStates[1] == 0);
    assert(!uploadCount);
}

static void *converterVtable[] = { StdConverterARGB_Convert };
static struct { void **vtable; } converter = { converterVtable };
void *CColorConverter_GetColorConverter(int mode) { assert(mode == 0); return &converter; }
void RB_ChangeStreamSource(int index, IDirect3DVertexBuffer9 *buffer, int offset, int stride)
{
    assert(index == 0);
    dxState.streams[index].vb = buffer;
    dxState.streams[index].offset = offset;
    dxState.streams[index].stride = stride;
}

static DWORD lastLockFlags;
static HRESULT Lock(const CDirect3DVertexBuffer *buffer, UINT offset, UINT size, void **data, DWORD flags)
{
    lastLockFlags = flags;
    return CDirect3DVertexBuffer_Lock(buffer, offset, size, data, flags);
}

static void CheckVertices(void)
{
    const int strides[] = { 20, 24, 32, 36, 64, 68 };
    const int colors[] = { -1, 12, 12, 24, 28, 24 };
    byte storage[2048], prior[2048], input[136], expected[136];
    void *vtable[16] = {0};
    vtable[11] = Lock;
    vtable[12] = CDirect3DVertexBuffer_Unlock;
    CDirect3DVertexBufferClean buffer = { .vtable = vtable, .data = storage,
                                         .lengthBytes = sizeof(storage) };
    r_vb_state_t slot = { .total = sizeof(storage), .buffer = (void (*)())&buffer };
    dx.dynamicVertexBuffer = &slot;
    dx.gpuSync = 1;
    memset(storage, 0xa5, sizeof(storage));
    for (int s = 0; s < 6; ++s) {
        int stride = strides[s], size = 2 * stride, offset = slot.used;
        memcpy(prior, storage, offset);
        for (int i = 0; i < size; ++i) input[i] = (byte)(i * 7 + s);
        memcpy(expected, input, size);
        if (colors[s] >= 0)
            for (int v = 0; v < 2; ++v)
                for (int c = 0; c < 4; ++c)
                    expected[v * stride + colors[s] + c] = input[v * stride + colors[s] + 3 - c];
        RB_SetVertexData(0, input, 2, stride);
        assert(slot.used == offset + size && dxState.streams[0].offset == offset);
        assert(lastLockFlags == (offset ? 0 : 0x2000));
        assert(!memcmp(storage + offset, expected, size));
        assert(!memcmp(storage, prior, offset));
        vertexSnapshotSize = size;
        GfxDrawPrimArgs args = { .vertexCount = 2, .primCount = 1 };
        RB_DrawIndexedPrim(&args, 1);
        assert(!memcmp(vertexSnapshot, expected, size));
        assert(!memcmp(storage + offset, expected, size));
    }
    void *locked;
    byte *originalStorage = buffer.data;
    for (DWORD flags = 0; flags <= 0x2000; flags += 0x1000) {
        assert(CDirect3DVertexBuffer_Lock((const void *)&buffer, 16, 8, &locked, flags) == 0);
        assert(locked == storage + 16 && buffer.data == originalStorage);
    }
    dx.gpuSync = 0;
    RB_SetVertexData(0, input, 1, 20);
    assert(lastLockFlags == 0x2000 && buffer.data == originalStorage);
    /* Characterize the native unsupported-stride fallback; no fidelity claim. */
    int offset = slot.used;
    RB_SetVertexData(0, input, 1, 28);
    assert(!memcmp(storage + offset, input, 28));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Init();
    if (!strcmp(argv[1], "draw")) {
        CheckDraw();
    } else if (!strcmp(argv[1], "vertices")) {
        CheckVertices();
    } else if (!strcmp(argv[1], "coords")) {
        backEnd.texScrollAmountDx7 = 0.375f;
        view.depthHackNearClip = 10;
        RB_ChangeGenTexCoords(1, 2);
        assert(transforms[1]._32 == -0.375f && dxState.genTexCoords[1] == 2);
    } else {
        int normal = !strcmp(argv[1], "mode");
        renderer.current.integer = normal ? 2 : 0;
        backEnd.sceneDef.floatTime = 1.5f;
        RB_UploadWaterTexture(&image, &water);
        CheckPayload(normal);
        byte first[128];
        memcpy(first, uploads[0].bytes, uploads[0].size);
        int count = uploadCount;
        RB_UploadWaterTexture(&image, &water);
        assert(uploadCount == count && !uploading);
        backEnd.sceneDef.floatTime = 2.0f;
        RB_UploadWaterTexture(&image, &water);
        assert(uploadCount == count && !memcmp(first, uploads[0].bytes, uploads[0].size));
        backEnd.sceneDef.floatTime = 1.5f;
        ++backEndData->frameCount;
        uploadCount = 0;
        RB_UploadWaterTexture(&image, &water);
        CheckPayload(normal);
        assert(!memcmp(first, uploads[0].bytes, uploads[0].size));
        ++backEndData->frameCount;
        backEnd.sceneDef.floatTime = 3.25f;
        uploadCount = 0;
        RB_UploadWaterTexture(&image, &water);
        CheckPayload(normal);
        assert(memcmp(first, uploads[0].bytes, uploads[0].size));
        ++backEndData->frameCount;
        drawWater.current.enabled = 0;
        uploadCount = 0;
        RB_UploadWaterTexture(&image, &water);
        assert(!uploadCount && water.writable.frameCount == backEndData->frameCount);
        drawWater.current.enabled = 1;
        RB_UploadWaterTexture(&image, &water);
        assert(!uploadCount);
    }
    free(backEndData);
    printf("production water %s: passed\n", argv[1]);
}
