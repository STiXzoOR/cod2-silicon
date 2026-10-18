#include "common_types.h"
#include "metal_backend.h"
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
#include "metal_targets.h"
__attribute__((weak)) void MacMetalBackend_BeginScene(void) {}
#include "metal_object_index.h"
/* Standalone core/resource fixtures omit the draw and target providers. A
 * weak definition also links on Darwin executables that reject unresolved
 * weak imports; the composed providers supply the strong implementation. */
__attribute__((weak)) int MacMetalDraws_Bootstrap(void) { return 1; }
__attribute__((weak)) int MacMetalDraws_DeviceCreated(void) { return 1; }
__attribute__((weak)) int MacMetalDraws_LoadSource(MacShimSidecar *side, const char *source) { (void)side; (void)source; return 1; }
__attribute__((weak)) int MacMetalDraws_Update(const char *method, const void *const *arguments, void *result, size_t size)
{ (void)method; (void)arguments; (void)result; (void)size; return 0; }
__attribute__((weak)) void MacMetalDraws_ObjectsReleased(void) {}
__attribute__((weak)) const char *MacMetalDraws_CacheNamespace(void) { return NULL; }
__attribute__((weak)) int MacMetalTargets_Install(const MacMetalResourceOps *ops, MacMetalTargetResolver resolver)
{ (void)ops; (void)resolver; return 1; }
#endif
#include "shim_dispatch.h"
#include "Mac/DirectX_9/lp64_buffers.h"
#include "platform/macos_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ARG(type, index) (*(type *)arguments[index])
#define U(index) ARG(unsigned int, index)
#define P(index) ARG(void *, index)
#define INVALID ((int32_t)0x8876086c)
#define NOTAVAILABLE ((int32_t)0x8876086a)
/* r_init passes its retained 14-word presentation block, also consumed by
 * Classic CreateDevice/Reset; it is not the host SDK pointer-sized structure. */
enum { PRESENT_WIDTH_WORD = 0, PRESENT_HEIGHT_WORD = 1, PRESENT_REFRESH_WORD = 12 };
extern void *vtbl_CDirect3D[], *vtbl_CDirect3DDevice[], *vtbl_CDirect3DSurface[];
extern void *vtbl_CDirect3DTexture[], *vtbl_CDirect3DCubeTexture[], *vtbl_CDirect3DVolumeTexture[];
extern void *vtbl_CDirect3DVertexBuffer[], *vtbl_CDirect3DIndexBuffer[], *vtbl_CDirect3DVertexDeclaration[];
extern void *vtbl_CDirect3DVertexShader[], *vtbl_CDirect3DPixelShader_ARB[], *vtbl_CDirect3DSwapChain[];
extern const dvar_t *r_fullscreen;
extern dvar_t *Dvar_RegisterBool(const char *, int, int);
extern void R_CapturePendingScreenshotBeforePresent(void);
extern uint16_t MacDisplay_SetGammaRamp(const unsigned short *ramp);
extern UINT32 MacOpenGLUtils_GetLevelSizeInBytes(UINT32, UINT32, UINT32, const D3DFORMAT *);
extern void CDirect3DVertexBuffer_CDirect3DVertexBuffer(const CDirect3DVertexBuffer *, UINT32, DWORD, D3DPOOL);
extern void CDirect3DIndexBuffer_CDirect3DIndexBuffer(const CDirect3DIndexBuffer *, UINT32, D3DFORMAT, DWORD, D3DPOOL);
extern int CDirect3DVertexDeclaration_CDirect3DVertexDeclaration(const CDirect3DVertexDeclaration *, const D3DVERTEXELEMENT9 *);
extern void CDirect3DSurface_CDirect3DSurface(const void *, int, unsigned int, unsigned int, unsigned int, unsigned int, int, const void *, void *);

struct MacShimSidecar {
    const void *object;
    MacShimKind kind;
    unsigned int refs;
    int owned;
    MacMetalResourceDesc desc;
    void *payload;
    MacMetalRetireFn release;
    uint64_t lastUse;
    void *bytes;
    struct MacShimSidecar *parent;
    unsigned int viewLevel, viewSlice;
    void *surfaces[96];
    size_t offsets[96];
    struct MacShimSidecar *next;
};
static MacShimSidecar *objects;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
static void *objectIndex;
#endif
static MacMetalRetirement *retirement;
static MacMetalState shadow;
static void *deviceObject, *backbufferObject, *swapchainObject;
static MacMetalResourceOps resources;
static MacMetalStateOps states;
static MacMetalShaderOps shaders;
static MacMetalDrawOps draws;

MacShimSidecar *MacMetal_FindSidecar(const void *object)
{
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (!object) return NULL;
    if (objectIndex) return MacShimObjectIndex_Get(objectIndex, object);
#endif
    for (MacShimSidecar *side = objects; side; side = side->next)
        if (side->object == object) return side;
    return NULL;
}
static MacShimSidecar *Attach(const void *object, MacShimKind kind, int owned)
{
    MacShimSidecar *side = MacMetal_FindSidecar(object);
    if (side) return side;
    side = calloc(1, sizeof(*side));
    if (!side) return NULL;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    /* Raw pointer identity, with no retain callbacks. The linked list still
     * owns sidecars and supports teardown; lookup must not scan every texture
     * and surface for each material or draw. Allocation failure keeps the
     * existing list fallback for this whole object lifetime. */
    if (!objects) objectIndex = MacShimObjectIndex_Create();
    if (objectIndex) MacShimObjectIndex_Set(objectIndex, object, side);
#endif
    side->object = object; side->kind = kind; side->refs = 1; side->owned = owned;
    side->next = objects; objects = side;
    MacShim_RecordObject(object, 1);
    if (!retirement) retirement = MacMetalRetirement_Create();
    return side;
}
static void RetirePayload(uint64_t value, MacMetalRetireFn release, void *payload)
{
    MacMetalGPU *gpu = MacMetal_GPU();
    if (!gpu) { release(payload); return; } /* Shutdown already drained work. */
    if (!MacMetalRetirement_Add(retirement, value, release, payload)) {
        if (MacMetalGPU_Drain(gpu)) release(payload);
        else fprintf(stderr, "CoD2 Silicon Metal: keep failed retirement alive until exit\n");
    }
}
static void Detach(MacShimSidecar *side)
{
    if (!side) return;
    MacShimSidecar **link = &objects;
    while (*link && *link != side) link = &(*link)->next;
    if (*link) *link = side->next;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (objectIndex) MacShimObjectIndex_Remove(objectIndex, side->object);
    if (!objects && objectIndex) { MacShimObjectIndex_Destroy(objectIndex); objectIndex = NULL; }
#endif
    if (side->payload && side->release) RetirePayload(side->lastUse, side->release, side->payload);
    MacShim_ForgetObject(side->object);
    free(side->bytes);
    free(side);
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (!objects && !MacMetal_GPU()) MacMetalDraws_ObjectsReleased();
#endif
}
const void *MacMetal_SidecarObject(const MacShimSidecar *side) { return side ? side->object : NULL; }
MacShimSidecar *MacMetal_SurfaceParent(const MacShimSidecar *side, unsigned int *level, unsigned int *slice)
{
    if (level) *level = side ? side->viewLevel : 0;
    if (slice) *slice = side ? side->viewSlice : 0;
    return side ? side->parent : NULL;
}
int MacMetal_BindSurfaceView(MacShimSidecar *texture, MacShimSidecar *surface, unsigned int level, unsigned int slice)
{
    if (!texture || !surface || (texture->kind != MAC_SHIM_TEXTURE_2D && texture->kind != MAC_SHIM_TEXTURE_CUBE && texture->kind != MAC_SHIM_TEXTURE_3D) || surface->kind != MAC_SHIM_SURFACE || surface->parent || surface->refs != 1 ||
        texture->desc.levels > 16 || level >= texture->desc.levels || slice >= (texture->kind == MAC_SHIM_TEXTURE_CUBE ? 6u : 1u)) return 0;
    unsigned int i = slice * texture->desc.levels + level;
    if (texture->surfaces[i]) return 0;
    texture->surfaces[i] = (void *)surface->object;
    surface->parent = texture; surface->viewLevel = level; surface->viewSlice = slice;
    return 1;
}
MacShimKind MacMetal_SidecarKind(const MacShimSidecar *side) { return side->kind; }
const MacMetalResourceDesc *MacMetal_SidecarDesc(const MacShimSidecar *side) { return &side->desc; }
void *MacMetal_SidecarPayload(const MacShimSidecar *side) { return side ? side->payload : NULL; }
void MacMetal_TouchSidecar(MacShimSidecar *side, uint64_t value) { if (side && value > side->lastUse) side->lastUse = value; }
void MacMetal_SetSidecarPayload(MacShimSidecar *side, void *payload, MacMetalRetireFn release)
{
    if (!side) return;
    if (side->payload && side->release) RetirePayload(side->lastUse, side->release, side->payload);
    side->payload = payload; side->release = release;
}
const void *MacMetal_SidecarBytes(const MacShimSidecar *side, size_t *length)
{
    if (length) *length = side ? side->desc.length : 0;
    return side ? side->bytes : NULL;
}
int MacMetal_PrepareProviders(const char *sourceCache, const char *translatedCache,
                              const char *cacheNamespace, const char *datasetPath)
{
    if (shaders.prepare && !shaders.prepare(sourceCache, translatedCache)) return 0;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    const char *drawNamespace = MacMetalDraws_CacheNamespace();
    if (drawNamespace) cacheNamespace = drawNamespace;
#endif
    return !states.prepare || states.prepare(cacheNamespace, datasetPath);
}
void *MacMetal_LoadShader(MacShimSidecar *side, const MacMetalShaderLibrary *library)
{
    if (!side || !library || library->abiVersion != MAC_METAL_ABI_VERSION || !shaders.load) return NULL;
    return shaders.load(side, library);
}
void *MacMetal_ReadyPipeline(const MacMetalPipelineKey *key)
{ return key && states.readyPipeline ? states.readyPipeline(key) : NULL; }
int MacMetal_Upload(MacShimSidecar *side, unsigned int level, unsigned int slice,
                     const void *bytes, size_t rowPitch, size_t slicePitch)
{ return side && resources.upload ? resources.upload(side, level, slice, bytes, rowPitch, slicePitch) : NOTAVAILABLE; }
void MacMetal_TargetResized(unsigned int width, unsigned int height)
{
    MacShimSidecar *side = MacMetal_FindSidecar(backbufferObject);
    if (!side) return;
    side->desc.width = width; side->desc.height = height;
    CDirect3DSurface_CDirect3DSurface(backbufferObject, 0, 0, 0, width, height, side->desc.format, NULL, NULL);
}
void MacMetal_ProducerSubmitted(uint64_t value)
{
    /* Zero is cancellation of the unsubmitted producer slot. Providers release
     * pending CPU references/ring reservations without assigning a last-use. */
    if (resources.submitted) resources.submitted(value);
    if (draws.submitted) draws.submitted(value);
    MacMetalRetirement_Collect(retirement, MacMetalGPU_Completed(MacMetal_GPU()));
}
void MacMetal_CollectRetired(uint64_t value) { MacMetalRetirement_Collect(retirement, value); }
MacMetalState *MacMetal_State(void) { return &shadow; }
#define INSTALL(name, field, type) int name(const type *ops) { if (!ops || ops->abiVersion != MAC_METAL_ABI_VERSION || objects) return 0; field = *ops; return 1; }
INSTALL(MacMetal_InstallResources, resources, MacMetalResourceOps)
INSTALL(MacMetal_InstallStates, states, MacMetalStateOps)
INSTALL(MacMetal_InstallShaders, shaders, MacMetalShaderOps)
INSTALL(MacMetal_InstallDraws, draws, MacMetalDrawOps)
static void *NewObject(MacShimKind kind, void **vtable)
{
    void *object = calloc(1, 4096);
    if (!object) return NULL;
    *(void ***)object = vtable;
    if (!Attach(object, kind, 1)) { free(object); return NULL; }
    return object;
}
static void Result(void *result, size_t size, intptr_t value)
{
    if (!result) return;
    if (size == 8) *(uint64_t *)result = (uint64_t)value;
    else if (size == 4) *(uint32_t *)result = (uint32_t)value;
    else if (size == 2) *(uint16_t *)result = (uint16_t)value;
    else if (size == 1) *(uint8_t *)result = (uint8_t)value;
}
static void Unavailable(const char *method, void *result, size_t size)
{
    static const char *const methods[] = {
#define MAC_METAL_HRESULT_ENTRY(name) name,
#include "metal_hresult.inc"
#undef MAC_METAL_HRESULT_ENTRY
    };
    /* The caller zeroed its typed result. Pointer, float, bool and count
     * getters must not receive the bit pattern of an HRESULT error. */
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); ++i) {
        if (!strcmp(method, methods[i])) { Result(result, size, NOTAVAILABLE); return; }
    }
}
static void *Surface(unsigned int width, unsigned int height, unsigned int format, void *bytes)
{
    void *object = NewObject(MAC_SHIM_SURFACE, vtbl_CDirect3DSurface);
    if (!object) return NULL;
    CDirect3DSurface_CDirect3DSurface(object, 0, 0, 0, width, height, format, bytes, NULL);
    MacShimSidecar *side = MacMetal_FindSidecar(object);
    side->desc = (MacMetalResourceDesc){ width, height, 1, 1, format, 0, 0, 0, 0 };
    return object;
}
static void DestroySide(MacShimSidecar *side);
static void *Texture(MacShimKind kind, unsigned int width, unsigned int height, unsigned int depth,
                      unsigned int levels, unsigned int format, unsigned int usage)
{
    void *object = MacMetal_CreateObject(kind, NULL);
    if (!object || !width || !height || !depth) return object;
    if (!levels) {
        unsigned int n = width > height ? width : height;
        if (depth > n) n = depth;
        do { ++levels; n >>= 1; } while (n);
    }
    if (levels > 16) levels = 16;
    MacShimSidecar *side = MacMetal_FindSidecar(object);
    side->desc = (MacMetalResourceDesc){ width, height, depth, levels, format, usage, 0, 0, 0 };
    unsigned int faces = kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1;
    size_t total = 0;
    for (unsigned int face = 0; face < faces; ++face) {
        unsigned int w = width, h = height, d = depth;
        for (unsigned int level = 0; level < levels; ++level) {
            unsigned int i = face * levels + level;
            side->offsets[i] = total;
            D3DFORMAT f = format;
            size_t size = MacOpenGLUtils_GetLevelSizeInBytes(w, h, d, &f);
            if (!size || total > SIZE_MAX - size) { Detach(side); free(object); return NULL; }
            total += size;
            w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1; d = d > 1 ? d / 2 : 1;
        }
    }
    side->bytes = calloc(1, total); side->desc.length = total;
    if (!side->bytes) { Detach(side); free(object); return NULL; }
    for (unsigned int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < levels; ++level) {
            unsigned int i = face * levels + level;
            unsigned int w = width >> level, h = height >> level;
            void *view = Surface(w ? w : 1, h ? h : 1, format, (char *)side->bytes + side->offsets[i]);
            MacShimSidecar *surface = MacMetal_FindSidecar(view);
            if (!MacMetal_BindSurfaceView(side, surface, level, face)) { DestroySide(side); return NULL; }
        }
    }
    return object;
}

static unsigned int AddRefSide(MacShimSidecar *side)
{
    if (side->parent) ++side->parent->refs;
    return ++side->refs;
}
static unsigned int ReleaseSide(MacShimSidecar *side);
static void DestroySide(MacShimSidecar *side)
{
    void *object = (void *)side->object;
    int owned = side->owned;
    if (side->kind == MAC_SHIM_VERTEX_BUFFER) free(((CDirect3DVertexBufferClean *)object)->data);
    if (side->kind == MAC_SHIM_INDEX_BUFFER) free(((CDirect3DIndexBufferClean *)object)->data);
    if (side->kind == MAC_SHIM_DECLARATION) {
        typedef struct { void **vtable; ULONG refs; UINT32 count; void *elements; } Declaration;
        free(((Declaration *)object)->elements);
    }
    for (unsigned int i = 0; i < 96; ++i) {
        MacShimSidecar *surface = MacMetal_FindSidecar(side->surfaces[i]);
        if (surface) { surface->parent = NULL; DestroySide(surface); }
    }
    if (object == deviceObject) {
        deviceObject = NULL;
        MacDisplay_ReleaseDisplay();
        MacShimSidecar *back = MacMetal_FindSidecar(backbufferObject), *swap = MacMetal_FindSidecar(swapchainObject);
        backbufferObject = swapchainObject = NULL;
        if (back) ReleaseSide(back);
        if (swap) ReleaseSide(swap);
        memset(&shadow, 0, sizeof(shadow));
    }
    Detach(side);
    if (owned) free(object);
}
static unsigned int ReleaseSide(MacShimSidecar *side)
{
    unsigned int refs = --side->refs;
    MacShimSidecar *parent = side->parent;
    if (!refs) DestroySide(side);
    if (parent) ReleaseSide(parent);
    return refs;
}
/* Event query uses exactly the shared renderer/presenter timeline. */
static HRESULT QueryInterface(const void *object, const IID *iid, void **out)
{ (void)iid; *out = (void *)object; AddRefSide(MacMetal_FindSidecar(object)); return 0; }
static ULONG QueryAddRef(const void *object) { return AddRefSide(MacMetal_FindSidecar(object)); }
static ULONG QueryRelease(const void *object) { return ReleaseSide(MacMetal_FindSidecar(object)); }
static HRESULT QueryDevice(const void *object, void **out) { (void)object; *out = deviceObject; return 0; }
static int QueryType(const void *object) { return MacMetal_FindSidecar(object)->desc.format; }
static UINT QuerySize(const void *object) { (void)object; return 4; }
static HRESULT QueryIssue(const void *object, DWORD flags)
{ (void)flags; MacShimSidecar *side = MacMetal_FindSidecar(object); MacMetalFence_Set(0); side->lastUse = MacMetalGPU_Submitted(MacMetal_GPU()); return 0; }
static HRESULT QueryData(const void *object, void *bytes, DWORD size, DWORD flags)
{
    (void)flags;
    MacShimSidecar *side = MacMetal_FindSidecar(object);
    if (MacMetalGPU_Completed(MacMetal_GPU()) < side->lastUse) return 1;
    if (bytes && size >= 4) *(uint32_t *)bytes = 1;
    return 0;
}
static void *queryTable[] = { QueryInterface, QueryAddRef, QueryRelease, QueryDevice, QueryType, QuerySize, QueryIssue, QueryData };

void *MacMetal_CreateObject(MacShimKind kind, const MacMetalResourceDesc *desc)
{
    void **table;
    switch (kind) {
    case MAC_SHIM_DEVICE: table = vtbl_CDirect3DDevice; break;
    case MAC_SHIM_SWAPCHAIN: table = vtbl_CDirect3DSwapChain; break;
    case MAC_SHIM_SURFACE: table = vtbl_CDirect3DSurface; break;
    case MAC_SHIM_TEXTURE_2D: table = vtbl_CDirect3DTexture; break;
    case MAC_SHIM_TEXTURE_CUBE: table = vtbl_CDirect3DCubeTexture; break;
    case MAC_SHIM_TEXTURE_3D: table = vtbl_CDirect3DVolumeTexture; break;
    case MAC_SHIM_VERTEX_BUFFER: table = vtbl_CDirect3DVertexBuffer; break;
    case MAC_SHIM_INDEX_BUFFER: table = vtbl_CDirect3DIndexBuffer; break;
    case MAC_SHIM_DECLARATION: table = vtbl_CDirect3DVertexDeclaration; break;
    case MAC_SHIM_VERTEX_SHADER: table = vtbl_CDirect3DVertexShader; break;
    case MAC_SHIM_PIXEL_SHADER: table = vtbl_CDirect3DPixelShader_ARB; break;
    case MAC_SHIM_QUERY: table = queryTable; break;
    default: return NULL;
    }
    void *object = NewObject(kind, table);
    if (object && desc) MacMetal_FindSidecar(object)->desc = *desc;
    return object;
}

static int Device(const char *method, const void *const *arguments, void *result, size_t size)
{
    const char *op = method + strlen("CDirect3DDevice_");
    if (!strcmp(op, "CreateTexture") || !strcmp(op, "CreateVolumeTexture") || !strcmp(op, "CreateCubeTexture")) {
        int volume = !strcmp(op, "CreateVolumeTexture"), cube = !strcmp(op, "CreateCubeTexture");
        unsigned int width = U(1), height = cube ? width : U(2), depth = volume ? U(3) : 1;
        unsigned int levels = U(cube ? 2 : volume ? 4 : 3), usage = U(cube ? 3 : volume ? 5 : 4), format = U(cube ? 4 : volume ? 6 : 5);
        void *texture = Texture(cube ? MAC_SHIM_TEXTURE_CUBE : volume ? MAC_SHIM_TEXTURE_3D : MAC_SHIM_TEXTURE_2D, width, height, depth, levels, format, usage);
        *(void **)P(cube ? 6 : volume ? 8 : 7) = texture;
        if (!texture) Result(result, size, INVALID);
    } else if (!strcmp(op, "CreateVertexBuffer") || !strcmp(op, "CreateIndexBuffer")) {
        int index = !strcmp(op, "CreateIndexBuffer");
        void *object = MacMetal_CreateObject(index ? MAC_SHIM_INDEX_BUFFER : MAC_SHIM_VERTEX_BUFFER, NULL);
        if (!object) { *(void **)P(5) = NULL; Result(result, size, INVALID); return 1; }
        if (index) CDirect3DIndexBuffer_CDirect3DIndexBuffer(object, U(1), U(3), U(2), U(4));
        else CDirect3DVertexBuffer_CDirect3DVertexBuffer(object, U(1), U(2), U(4));
        MacShimSidecar *side = MacMetal_FindSidecar(object);
        side->desc.length = U(1); side->desc.usage = U(2);
        side->desc.format = index ? U(3) : 0;
        *(void **)P(5) = object;
    } else if (!strcmp(op, "CreateQuery")) {
        if (U(1) != 8) { Result(result, size, NOTAVAILABLE); return 1; }
        if (P(2)) {
            void *object = NewObject(MAC_SHIM_QUERY, queryTable);
            if (!object) { *(void **)P(2) = NULL; Result(result, size, INVALID); return 1; }
            MacMetal_FindSidecar(object)->desc.format = U(1);
            *(void **)P(2) = object;
        }
    } else if (!strcmp(op, "CreateVertexDeclaration")) {
        void *object = NewObject(MAC_SHIM_DECLARATION, vtbl_CDirect3DVertexDeclaration);
        if (!object) { *(void **)P(2) = NULL; Result(result, size, INVALID); return 1; }
        CDirect3DVertexDeclaration_CDirect3DVertexDeclaration(object, P(1));
        *(void **)P(2) = object;
    } else if (!strcmp(op, "CreateVertexShader") || !strcmp(op, "CreatePixelShader") || !strcmp(op, "CreatePixelShaderOpenGL")) {
        int vertex = !strcmp(op, "CreateVertexShader");
        void *object = MacMetal_CreateObject(vertex ? MAC_SHIM_VERTEX_SHADER : MAC_SHIM_PIXEL_SHADER, NULL);
        if (!object) { *(void **)P(!strcmp(op, "CreatePixelShaderOpenGL") ? 3 : 2) = NULL; Result(result, size, INVALID); return 1; }
        MacShimSidecar *side = MacMetal_FindSidecar(object);
        const char *source = P(!strcmp(op, "CreatePixelShaderOpenGL") ? 2 : 1);
        if (source && source[0] == '!') {
            side->bytes = strdup(source);
            side->desc.length = strlen(source) + 1;
        }
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
        if (!MacMetalDraws_LoadSource(side, source)) {
            DestroySide(side);
            *(void **)P(!strcmp(op, "CreatePixelShaderOpenGL") ? 3 : 2) = NULL;
            Result(result, size, NOTAVAILABLE); return 1;
        }
#endif
        *(void **)P(!strcmp(op, "CreatePixelShaderOpenGL") ? 3 : 2) = object;
    } else if (!strncmp(op, "Create", 6) && strstr(op, "Surface")) {
        unsigned int output = !strcmp(op, "CreateOffscreenPlainSurface") ? 5 : 7;
        *(void **)P(output) = Surface(U(1), U(2), U(3), NULL);
    } else if (!strcmp(op, "CreateRenderTarget")) {
        *(void **)P(7) = Surface(U(1), U(2), U(3), NULL);
    } else if (!strcmp(op, "GetBackBuffer") || !strcmp(op, "GetRenderTarget") || !strcmp(op, "GetDepthStencilSurface") || !strcmp(op, "GetSwapChain")) {
        unsigned int output = !strcmp(op, "GetBackBuffer") ? 4 : !strcmp(op, "GetDepthStencilSurface") ? 1 : 2;
        void *object = !strcmp(op, "GetSwapChain") ? swapchainObject : !strcmp(op, "GetDepthStencilSurface") ? shadow.depth : !strcmp(op, "GetRenderTarget") ? shadow.color[U(1)] : backbufferObject;
        *(void **)P(output) = object;
        MacShimSidecar *side = MacMetal_FindSidecar(object); if (side) AddRefSide(side);
    } else if (!strcmp(op, "SetRenderTarget")) {
        if (U(1) < 4) shadow.color[U(1)] = P(2); else Result(result, size, INVALID);
    } else if (!strcmp(op, "SetDepthStencilSurface")) shadow.depth = P(1);
    else if (!strcmp(op, "SetRenderState") || !strcmp(op, "GetRenderState")) {
        if (U(1) >= 256) Result(result, size, INVALID);
        else if (*op == 'S') shadow.render[U(1)] = U(2);
        else *(unsigned int *)P(2) = shadow.render[U(1)];
    } else if (!strcmp(op, "SetSamplerState") || !strcmp(op, "GetSamplerState")) {
        if (U(1) >= 16 || U(2) >= 16) Result(result, size, INVALID);
        else if (*op == 'S') shadow.sampler[U(1)][U(2)] = U(3);
        else *(unsigned int *)P(3) = shadow.sampler[U(1)][U(2)];
    } else if (!strcmp(op, "SetTextureStageState") || !strcmp(op, "GetTextureStageState")) {
        if (U(1) >= 8 || U(2) >= 33) Result(result, size, INVALID);
        else if (*op == 'S') shadow.textureStage[U(1)][U(2)] = U(3);
        else *(unsigned int *)P(3) = shadow.textureStage[U(1)][U(2)];
    } else if (!strcmp(op, "SetTexture") || !strcmp(op, "GetTexture")) {
        if (U(1) >= 16) Result(result, size, INVALID);
        else if (*op == 'S') shadow.textures[U(1)] = P(2);
        else *(void **)P(2) = shadow.textures[U(1)];
    } else if (strstr(op, "ShaderConstantF")) {
        MacMetalARBConstants *bank = strstr(op, "Vertex") ? &shadow.vertex : &shadow.fragment;
        if (*op == 'S') { if (!MacMetal_WriteRegisters(bank->env, 256, U(1), P(2), U(3))) Result(result, size, INVALID); }
        else if (U(1) <= 256 && U(3) <= 256 - U(1)) memcpy(P(2), bank->env + U(1), U(3) * sizeof(bank->env[0]));
        else Result(result, size, INVALID);
    } else if (!strcmp(op, "SetStreamSource")) {
        if (U(1) < 16) { shadow.streams[U(1)] = P(2); shadow.offsets[U(1)] = U(3); shadow.strides[U(1)] = U(4); }
        else Result(result, size, INVALID);
    } else if (!strcmp(op, "GetStreamSource")) {
        if (U(1) < 16) {
            *(void **)P(2) = shadow.streams[U(1)];
            *(unsigned int *)P(3) = shadow.offsets[U(1)]; *(unsigned int *)P(4) = shadow.strides[U(1)];
            MacShimSidecar *stream = MacMetal_FindSidecar(shadow.streams[U(1)]);
            if (stream) AddRefSide(stream);
        }
        else Result(result, size, INVALID);
    } else if (!strcmp(op, "GetViewport")) {
        D3DVIEWPORT9 *viewport = P(1);
        *viewport = (D3DVIEWPORT9){ shadow.viewport[0], shadow.viewport[1],
            shadow.viewport[2], shadow.viewport[3], shadow.minDepth, shadow.maxDepth };
    }
    else if (!strcmp(op, "GetIndices") || !strcmp(op, "GetVertexDeclaration") || !strcmp(op, "GetVertexShader") || !strcmp(op, "GetPixelShader")) {
        void *object = !strcmp(op, "GetIndices") ? shadow.indices : !strcmp(op, "GetVertexDeclaration") ? shadow.declaration : !strcmp(op, "GetVertexShader") ? shadow.vs : shadow.ps;
        *(void **)P(1) = object;
        MacShimSidecar *side = MacMetal_FindSidecar(object); if (side) AddRefSide(side);
    }
    else if (!strcmp(op, "SetIndices")) shadow.indices = P(1);
    else if (!strcmp(op, "SetVertexDeclaration")) shadow.declaration = P(1);
    else if (!strcmp(op, "SetVertexShader")) shadow.vs = P(1);
    else if (!strcmp(op, "SetPixelShader")) shadow.ps = P(1);
    else if (!strcmp(op, "SetViewport")) {
        const D3DVIEWPORT9 *viewport = P(1);
        shadow.viewport[0] = viewport->X; shadow.viewport[1] = viewport->Y;
        shadow.viewport[2] = viewport->Width; shadow.viewport[3] = viewport->Height;
        shadow.minDepth = viewport->MinZ; shadow.maxDepth = viewport->MaxZ;
    }
    else if (!strcmp(op, "SetTransform") || !strcmp(op, "GetTransform")) {
        if (U(1) >= 512) Result(result, size, INVALID);
        else if (*op == 'S') memcpy(shadow.transform[U(1)], P(2), sizeof(shadow.transform[0]));
        else memcpy(P(2), shadow.transform[U(1)], sizeof(shadow.transform[0]));
    } else if (!strcmp(op, "Clear")) MacMetalBackend_Clear(U(3), U(4), ARG(float, 5), U(6));
    else if (!strcmp(op, "Present")) { R_CapturePendingScreenshotBeforePresent(); SDL_GL_SwapWindowDirect(); MacMetalRetirement_Collect(retirement, MacMetalGPU_Completed(MacMetal_GPU())); }
    else if (!strcmp(op, "Reset")) {
        const int *pp = P(1);
        if (pp && pp[PRESENT_WIDTH_WORD] && pp[PRESENT_HEIGHT_WORD])
            Result(result, size, MacDisplay_SetMode(pp[PRESENT_WIDTH_WORD],
                pp[PRESENT_HEIGHT_WORD], 32, pp[PRESENT_REFRESH_WORD]) ? INVALID : 0);
    } else if (!strncmp(op, "Draw", 4)) {
        MacMetalDraw draw = { 0 };
        draw.submission = MacMetalBackend_Submission(); draw.state = &shadow; draw.topology = U(1);
        if (!strcmp(op, "DrawIndexedPrimitive")) {
            draw.indexed = 1; draw.baseVertex = ARG(int, 2); draw.minVertex = U(3); draw.vertexCount = U(4); draw.first = U(5); draw.count = U(6);
            MacShimSidecar *index = MacMetal_FindSidecar(shadow.indices);
            draw.indexFormat = index ? index->desc.format : 0;
        } else if (!strcmp(op, "DrawPrimitive")) { draw.first = U(2); draw.count = U(3); }
        else if (!strcmp(op, "DrawPrimitiveUP")) { draw.userMemory = 1; draw.count = U(2); draw.vertexUP = P(3); draw.vertexStride = U(4); }
        else if (!strcmp(op, "DrawIndexedPrimitiveUP")) {
            draw.indexed = draw.userMemory = 1; draw.minVertex = U(2); draw.vertexCount = U(3); draw.count = U(4);
            draw.indexUP = P(5); draw.indexFormat = U(6); draw.vertexUP = P(7); draw.vertexStride = U(8);
        } else { Result(result, size, NOTAVAILABLE); return 1; }
        /* count is the original primitive count. Bounds/copy policy belongs
         * to the provider; UP pointers expire when this method returns. */
        if (draws.encode) Result(result, size, draws.encode(&draw));
    } else if (!strcmp(op, "GetDeviceCaps")) return 0;
    else if (!strcmp(op, "ValidateDevice")) { if (P(1)) *(unsigned int *)P(1) = 1; }
    else if (!strcmp(op, "GetAvailableTextureMem")) Result(result, size, 1024 * 1024 * 1024);
    else if (!strcmp(op, "GetNumberOfSwapChains")) Result(result, size, 1);
    else if (!strcmp(op, "BeginScene")) MacMetalBackend_BeginScene();
    else if (!strcmp(op, "SetGammaRamp")) MacDisplay_SetGammaRamp(P(3));
    else if (!strcmp(op, "EndScene") || !strcmp(op, "TestCooperativeLevel") || !strcmp(op, "Init")) { }
    else Unavailable(method, result, size);
    return 1;
}

static int MetalInvoke(const char *method, const void *object, const void *const *arguments, void *result, size_t size)
{
    if (result) memset(result, 0, size);
    MacShimSidecar *side = MacMetal_FindSidecar(object);
    if (!strcmp(method, "CFence_CFence")) {
        side = Attach(object, MAC_SHIM_FENCE, 0);
        if (side) { side->desc.length = U(2); side->lastUse = MacMetalGPU_Submitted(MacMetal_GPU()); }
        return 1;
    }
    if (!strcmp(method, "Direct3DCreate9")) {
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
        if (!objects && !MacMetalDraws_Bootstrap()) return 1;
#endif
        static unsigned char interface[32]; *(void ***)interface = vtbl_CDirect3D; Attach(interface, MAC_SHIM_DEVICE, 0); Result(result, size, (intptr_t)interface); return 1;
    }
    if (!strcmp(method, "CDirect3D_CreateDevice")) {
        const int *pp = P(5);
        int width = pp && pp[PRESENT_WIDTH_WORD] ? pp[PRESENT_WIDTH_WORD] : 640;
        int height = pp && pp[PRESENT_HEIGHT_WORD] ? pp[PRESENT_HEIGHT_WORD] : 480;
        const dvar_t *borderless = Dvar_RegisterBool("r_borderless", 0, 0x1001);
        int fullscreen = r_fullscreen && r_fullscreen->current.enabled;
        MacPlatform_ConfigureWindow(width, height, fullscreen ? (borderless->current.enabled ? MAC_BORDERLESS : MAC_FULLSCREEN) : MAC_WINDOWED, pp ? pp[PRESENT_REFRESH_WORD] : 0);
        void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
        if (!context) {
            if (!MacShim_IsMetal()) {
                /* The abstract factory also changes owner on an availability
                 * fallback; subsequent CreateDevice calls must stay on GL. */
                if (side) { *(uint32_t *)((char *)object + sizeof(void *)) = side->refs; Detach(side); }
                MacShim_RecordObject(object, 0);
                return 0;
            }
            Result(result, size, INVALID); return 1;
        }
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
        if (!MacMetalDraws_DeviceCreated()) {
            MacDisplay_ReleaseDisplay(); *(void **)P(6) = NULL;
            Result(result, size, NOTAVAILABLE); return 1;
        }
#endif
        deviceObject = NewObject(MAC_SHIM_DEVICE, vtbl_CDirect3DDevice);
        backbufferObject = Surface(width, height, 22, NULL);
        swapchainObject = NewObject(MAC_SHIM_SWAPCHAIN, vtbl_CDirect3DSwapChain);
        shadow.color[0] = backbufferObject;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
        shadow.viewport[2] = width; shadow.viewport[3] = height; shadow.maxDepth = 1;
#endif
        memset(&shadow.vertex, 0, sizeof(shadow.vertex)); memset(&shadow.fragment, 0, sizeof(shadow.fragment));
        *(void **)P(6) = deviceObject;
        return 1;
    }
    const char *op = strchr(method, '_'); op = op ? op + 1 : method;
    if (side && !strcmp(op, "AddRef")) { Result(result, size, AddRefSide(side)); return 1; }
    if (side && !strcmp(op, "Release")) { Result(result, size, ReleaseSide(side)); return 1; }
    if (!strcmp(op, "QueryInterface")) { *(void **)P(2) = (void *)object; if (side) AddRefSide(side); return 1; }
    if (side && method[0] == 'Z' && strstr(method, "D0Ev")) {
        DestroySide(side);
        return 1;
    }
    if (side && method[0] == 'Z' && (strstr(method, "D1Ev") || strstr(method, "D2Ev"))) return 1;
    if (side && resources.shim && resources.shim(method, side, arguments, result, size)) return 1;
    /* Buffer/declaration/surface CPU contracts are already portable and have
     * no GL calls. Sidecars add GPU lifetime without changing those layouts. */
    int cpu = !strncmp(method, "CDirect3DVertexBuffer_", 22) || !strncmp(method, "CDirect3DIndexBuffer_", 21) || !strncmp(method, "CDirect3DVertexDeclaration_", 27);
    if (!strncmp(method, "CDirect3DSurface_", 17)) {
        if (strstr(method, "UpdateOpenGL") || strstr(method, "CreateOpenGL") || strstr(method, "GetGLBlit")) return 1;
        return 0;
    }
    if (cpu) {
        if (side && !strcmp(op, "Lock") && resources.lock)
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
        {
            int status = resources.lock(side, U(1), U(2), U(4), P(3));
            static unsigned int failures;
            if (status && failures++ < 12) fprintf(stderr, "Metal buffer lock rejected: HRESULT=%x kind=%u offset=%u length=%u flags=%x size=%zu usage=%x submitted=%llu completed=%llu\n", (unsigned int)status, side->kind, U(1), U(2), U(4), side->desc.length, side->desc.usage, (unsigned long long)MacMetalGPU_Submitted(MacMetal_GPU()), (unsigned long long)MacMetalGPU_Completed(MacMetal_GPU()));
            Result(result, size, status);
        }
#else
            Result(result, size, resources.lock(side, U(1), U(2), U(4), P(3)));
#endif
        else if (side && !strcmp(op, "Unlock") && resources.unlock)
            Result(result, size, resources.unlock(side));
        else return 0;
        return 1;
    }
    if (!strncmp(method, "CDirect3DDevice_", 16)) {
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
        if (MacMetalDraws_Update(method, arguments, result, size)) return 1;
#endif
        if (states.update && states.update(&shadow, method, arguments, result, size)) return 1;
        return Device(method, arguments, result, size);
    }
    if (side && !strcmp(op, "GetLevelCount")) Result(result, size, side->desc.levels);
    else if (side && !strcmp(op, "GetType")) Result(result, size, side->kind == MAC_SHIM_TEXTURE_CUBE ? 5 : side->kind == MAC_SHIM_TEXTURE_3D ? 4 : 3);
    else if (side && !strcmp(op, "GetSurfaceLevel")) {
        if (U(1) >= side->desc.levels) Result(result, size, INVALID);
        else { *(void **)P(2) = side->surfaces[U(1)]; AddRefSide(MacMetal_FindSidecar(side->surfaces[U(1)])); }
    } else if (side && !strcmp(op, "GetCubeMapSurface")) {
        if (U(1) >= 6 || U(2) >= side->desc.levels) Result(result, size, INVALID);
        else { void *surface = side->surfaces[U(1) * side->desc.levels + U(2)]; *(void **)P(3) = surface; AddRefSide(MacMetal_FindSidecar(surface)); }
    } else if (side && !strcmp(op, "LockRect")) {
        int cube = side->kind == MAC_SHIM_TEXTURE_CUBE;
        unsigned int level = U(cube ? 2 : 1), face = cube ? U(1) : 0;
        if (level >= side->desc.levels || face >= 6) Result(result, size, INVALID);
        else {
            D3DLOCKED_RECT *lock = P(cube ? 3 : 2); D3DFORMAT f = side->desc.format;
            unsigned int w = side->desc.width >> level; if (!w) w = 1;
            lock->Pitch = MacOpenGLUtils_GetLevelSizeInBytes(w, 1, 1, &f);
            lock->pBits = (char *)side->bytes + side->offsets[face * side->desc.levels + level];
        }
    } else if (side && !strcmp(op, "LockBox")) {
        unsigned int level = U(1);
        if (level >= side->desc.levels) Result(result, size, INVALID);
        else {
            D3DLOCKED_BOX *lock = P(2); D3DFORMAT f = side->desc.format;
            unsigned int w = side->desc.width >> level, h = side->desc.height >> level; if (!w) w = 1; if (!h) h = 1;
            lock->RowPitch = MacOpenGLUtils_GetLevelSizeInBytes(w, 1, 1, &f);
            lock->SlicePitch = MacOpenGLUtils_GetLevelSizeInBytes(w, h, 1, &f);
            lock->pBits = (char *)side->bytes + side->offsets[level];
        }
    } else if (side && !strcmp(op, "GetLevelDesc")) {
        unsigned int level = U(1); D3DSURFACE_DESC *desc = P(2); memset(desc, 0, sizeof(*desc));
        desc->Format = side->desc.format; desc->Width = side->desc.width >> level; desc->Height = side->desc.height >> level;
        if (!desc->Width) desc->Width = 1; if (!desc->Height) desc->Height = 1;
    } else if (!strncmp(method, "CDirect3D_", 10)) return 0;
    else if (!strcmp(op, "GetBackBuffer")) {
        *(void **)P(3) = backbufferObject;
        MacShimSidecar *back = MacMetal_FindSidecar(backbufferObject);
        if (back) AddRefSide(back);
    }
    else if (side && !strcmp(op, "SetConstants")) {
        MacMetalARBConstants *bank = side->kind == MAC_SHIM_VERTEX_SHADER ? &shadow.vertex : &shadow.fragment;
        if (!MacMetal_WriteRegisters(bank->env, 256, U(1), P(2), U(3))) Result(result, size, INVALID);
    } else if (!strncmp(method, "COpenGL", 7) || strstr(method, "GetGLName") || strstr(method, "GetProgramId") || strstr(method, "UpdateOpenGL") || strstr(method, "Unlock") || strstr(method, "PreLoad") || strstr(method, "Dirty")) { }
    else Unavailable(method, result, size);
    return 1;
}
/* GL's table returns to the unchanged typed method body. The Metal table
 * consumes it; no branch ever calls GL implementation from a Metal method. */
static int GLInvoke(const char *method, const void *object, const void *const *args, void *result, size_t size)
{ (void)method; (void)object; (void)args; (void)result; (void)size; return 0; }
__attribute__((constructor)) static void InstallBackend(void)
{
    const MacShimBackendOps gl = { GLInvoke }, metal = { MetalInvoke };
    MacShim_InstallBackend(0, &gl);
    MacShim_InstallBackend(1, &metal);
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    MacMetalTargets_Install(NULL, NULL);
#endif
}
