#ifndef COD2_METAL_BACKEND_H
#define COD2_METAL_BACKEND_H
#include "metal_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* These APIs are native client only; engine/shim layouts are never extended. */
typedef enum {
    MAC_SHIM_DEVICE, MAC_SHIM_SWAPCHAIN, MAC_SHIM_SURFACE,
    MAC_SHIM_TEXTURE_2D, MAC_SHIM_TEXTURE_CUBE, MAC_SHIM_TEXTURE_3D,
    MAC_SHIM_VERTEX_BUFFER, MAC_SHIM_INDEX_BUFFER, MAC_SHIM_DECLARATION,
    MAC_SHIM_VERTEX_SHADER, MAC_SHIM_PIXEL_SHADER, MAC_SHIM_QUERY, MAC_SHIM_FENCE
} MacShimKind;
typedef struct MacShimSidecar MacShimSidecar;
typedef struct {
    uint32_t width, height, depth, levels, format, usage;
    size_t length, rowPitch, slicePitch;
} MacMetalResourceDesc;
typedef struct {
    uint32_t render[256], sampler[16][16], textureStage[8][33];
    float transform[512][16];
    uint32_t viewport[4];
    float minDepth, maxDepth;
    void *textures[16], *streams[16], *indices, *declaration, *vs, *ps;
    uint32_t offsets[16], strides[16];
    void *color[4], *depth;
    MacMetalARBConstants vertex, fragment;
} MacMetalState;
enum { MAC_METAL_SHADER_VERTEX = 0, MAC_METAL_SHADER_FRAGMENT = 1 };
typedef struct {
    uint32_t abiVersion, stage;
    const char *name, *sourceSHA256, *metadataSHA256, *translatorRevision;
    const char *libraryPath, *entryPoint;
    const void *reflection;
    size_t reflectionSize;
} MacMetalShaderLibrary;
typedef struct {
    const unsigned char *bytes;
    size_t length;
    uint32_t schemaVersion;
} MacMetalPipelineKey;
typedef struct {
    uint32_t topology, first, count, minVertex, vertexCount, indexFormat;
    uint32_t indexed, userMemory, vertexStride;
    int32_t baseVertex;
    const void *vertexUP, *indexUP;
    size_t vertexBytes, indexBytes;
    MacMetalState *state;
    MacMetalSubmission *submission;
} MacMetalDraw;
/* Providers installed during loading, before objects/scene readiness. The
 * shim hook uses typed argument addresses listed in shim_methods.json. Return
 * 1 when handled; HRESULT and out parameters must be set on every path. */
typedef struct {
    uint32_t abiVersion;
    int (*shim)(const char *method, MacShimSidecar *, const void *const *, void *, size_t);
    int (*upload)(MacShimSidecar *, unsigned int level, unsigned int slice,
                  const void *bytes, size_t rowPitch, size_t slicePitch);
    int (*lock)(MacShimSidecar *, size_t offset, size_t length, unsigned int flags, void **bytes);
    int (*unlock)(MacShimSidecar *);
    void (*submitted)(uint64_t value);
} MacMetalResourceOps;
typedef struct {
    uint32_t abiVersion;
    int (*update)(MacMetalState *, const char *method, const void *const *, void *result, size_t resultSize);
    void *(*readyPipeline)(const MacMetalPipelineKey *);
    int (*prepare)(const char *cacheNamespace, const char *datasetPath);
} MacMetalStateOps;
typedef struct {
    uint32_t abiVersion;
    void *(*load)(MacShimSidecar *, const MacMetalShaderLibrary *);
    int (*prepare)(const char *sourceCache, const char *translatedCache);
} MacMetalShaderOps;
typedef struct {
    uint32_t abiVersion;
    int (*encode)(const MacMetalDraw *);
    void (*submitted)(uint64_t value);
} MacMetalDrawOps;
int MacMetal_InstallResources(const MacMetalResourceOps *);
int MacMetal_InstallStates(const MacMetalStateOps *);
int MacMetal_InstallShaders(const MacMetalShaderOps *);
int MacMetal_InstallDraws(const MacMetalDrawOps *);
MacShimSidecar *MacMetal_FindSidecar(const void *object);
MacShimKind MacMetal_SidecarKind(const MacShimSidecar *);
const void *MacMetal_SidecarObject(const MacShimSidecar *);
MacShimSidecar *MacMetal_SurfaceParent(const MacShimSidecar *, unsigned int *level, unsigned int *slice);
int MacMetal_BindSurfaceView(MacShimSidecar *texture, MacShimSidecar *surface, unsigned int level, unsigned int slice);
void *MacMetal_CreateObject(MacShimKind, const MacMetalResourceDesc *);
const MacMetalResourceDesc *MacMetal_SidecarDesc(const MacShimSidecar *);
void *MacMetal_SidecarPayload(const MacShimSidecar *);
const void *MacMetal_SidecarBytes(const MacShimSidecar *, size_t *length);
/* Explicit loading operations; never called implicitly by a draw. */
int MacMetal_PrepareProviders(const char *sourceCache, const char *translatedCache,
                              const char *cacheNamespace, const char *datasetPath);
void *MacMetal_LoadShader(MacShimSidecar *, const MacMetalShaderLibrary *);
void *MacMetal_ReadyPipeline(const MacMetalPipelineKey *);
int MacMetal_Upload(MacShimSidecar *, unsigned int level, unsigned int slice,
                     const void *, size_t rowPitch, size_t slicePitch);
void MacMetal_TargetResized(unsigned int width, unsigned int height);
void MacMetal_ProducerSubmitted(uint64_t value);
void MacMetal_CollectRetired(uint64_t value);
void MacMetal_SetSidecarPayload(MacShimSidecar *, void *, MacMetalRetireFn release);
void MacMetal_TouchSidecar(MacShimSidecar *, uint64_t submission);
MacMetalState *MacMetal_State(void);
MacMetalGPU *MacMetal_GPU(void);
MacMetalSubmission *MacMetalBackend_Submission(void);
void *MacMetalBackend_SceneTexture(void);
/* Latched at renderer registration; support/create failure falls back to GL
 * before object construction. No switching live objects between backends. */
void MacShim_SelectRenderer(int metal);
int MacShim_IsMetal(void);
int MacShim_Invoke(const char *, const void *, const void *const *, void *, size_t);

struct SDL_Window;
void MacMetalBackend_PrepareWindow(void);
int MacMetalBackend_Start(struct SDL_Window *, int width, int height, int edr);
void MacMetalBackend_Stop(void);
int MacMetalBackend_Resize(struct SDL_Window *, int width, int height, int edr);
void MacMetalBackend_Clear(unsigned int flags, unsigned int color, float depth, unsigned int stencil);
void MacMetalBackend_Present(const unsigned short *ramp, unsigned int revision, int identity);
int MacMetalBackend_ReadPixels(int width, int height, int bytesPerPixel, unsigned char *pixels);
/* APPLE fence escape hook uses native IDs with a full-width timeline value. */
void MacMetalFence_Gen(int count, unsigned int *ids);
void MacMetalFence_Set(unsigned int id);
int MacMetalFence_Test(unsigned int id);
void MacMetalFence_Delete(int count, const unsigned int *ids);
#ifdef __cplusplus
}
#endif
#endif
