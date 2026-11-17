#ifndef COD2_METAL_STATES_H
#define COD2_METAL_STATES_H
#include "metal_backend.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAC_METAL_STATE_KEY_SCHEMA 1u
#define MAC_METAL_KEY_CAPACITY 4096u
#define MAC_METAL_STRICT_MATH 1u
#define MAC_METAL_MAX_ATTRIBUTES 32u

typedef struct {
    unsigned char bytes[MAC_METAL_KEY_CAPACITY];
    size_t length;
    uint32_t schemaVersion;
} MacMetalKeyStorage;
typedef struct {
    uint32_t address[3], minFilter, magFilter, mipFilter, anisotropy;
    uint32_t borderColor, maxMipLevel, srgb, elementIndex, dmapOffset;
    float lodBias;
} MacMetalSamplerState;
/* Pointers are borrowed dynamic bindings, never serialized into a key. */
typedef struct {
    uint32_t render[256], textureStage[8][33];
    MacMetalSamplerState sampler[16];
    float transform[512][16];
    uint32_t viewport[4];
    float minDepth, maxDepth;
    void *textures[16], *streams[16], *indices, *declaration, *vs, *ps;
    uint32_t offsets[16], strides[16];
    void *color[4], *depth;
} MacMetalNormalizedState;
typedef struct {
    unsigned char source[32], metadata[32], interfaceSignature[32];
    char translatorRevision[64];
    uint32_t registerABI, compilerOptions;
    char entryPoint[64];
} MacMetalShaderIdentity;
/* format uses MTLVertexFormat's stable numeric value, resolved by WS33.
 * colorOrder is the fetch/producer contract, not a blanket texture swizzle. */
typedef struct {
    uint32_t attribute, stream, offset, format, colorOrder;
} MacMetalVertexAttribute;
typedef struct {
    MacMetalShaderIdentity vertex, fragment;
    MacMetalVertexAttribute attributes[MAC_METAL_MAX_ATTRIBUTES];
    uint32_t attributeCount, strides[16], hardwareVertexFetch;
    uint32_t colorFormats[4], depthFormat, stencilFormat, samples, topology;
    uint32_t fixedFunctionVariant;
    unsigned char specialization[256];
    uint32_t specializationLength;
} MacMetalPipelineRecipe;

typedef struct MacMetalStates MacMetalStates;
typedef struct {
    uint64_t pipelineHits, pipelineMisses, depthHits, depthMisses, samplerHits, samplerMisses;
    uint64_t uniqueMisses, coalescedMisses, missOverflow, invalidKeys;
    uint64_t pipelineCreates, depthCreates, samplerCreates, prohibitedFactories;
    uint64_t archiveLoads, archiveHits, archiveMisses, archiveInvalid, archiveWrites;
} MacMetalStateStats;

void MacMetalStates_Defaults(MacMetalState *);
int MacMetalStates_Normalize(const MacMetalState *, MacMetalNormalizedState *);
int MacMetalStates_Update(MacMetalState *, const char *, const void *const *, void *, size_t);
int MacMetalStates_PipelineKey(const MacMetalNormalizedState *, const MacMetalPipelineRecipe *, MacMetalKeyStorage *);
int MacMetalStates_DepthKey(const MacMetalNormalizedState *, MacMetalKeyStorage *);
int MacMetalStates_SamplerKey(const MacMetalSamplerState *, MacMetalKeyStorage *);
MacMetalPipelineKey MacMetalStates_KeyView(const MacMetalKeyStorage *);
int MacMetalStates_KeyEqual(const MacMetalKeyStorage *, const MacMetalKeyStorage *);
uint64_t MacMetalStates_KeyHash(const MacMetalPipelineKey *);

/* Single producer thread, sharing ABI 1's device/transport. No queue is made.
 * BeginLoading freezes draw lookup. Publish makes every factory fail until an
 * explicit next BeginLoading. Libraries/entry identities must be verified by
 * WS32. hardwareVertexFetch=0 uses its buffer-based fetch wrapper. */
MacMetalStates *MacMetalStates_Create(MacMetalGPU *);
void MacMetalStates_Destroy(MacMetalStates *);
int MacMetalStates_BeginLoading(MacMetalStates *, const char *cacheNamespace, const char *datasetPath);
int MacMetalStates_Prewarm(MacMetalStates *, const MacMetalNormalizedState *, const MacMetalPipelineRecipe *, void *vertexLibrary, void *fragmentLibrary);
int MacMetalStates_Publish(MacMetalStates *);
void *MacMetalStates_ReadyPipeline(MacMetalStates *, const MacMetalPipelineKey *);
void *MacMetalStates_ReadyDepth(MacMetalStates *, const MacMetalPipelineKey *);
void *MacMetalStates_ReadySampler(MacMetalStates *, const MacMetalPipelineKey *);
MacMetalStateStats MacMetalStates_Stats(const MacMetalStates *);
/* Bounded miss queue contains full key bytes. Loading drains it by key, after
 * callers have retained the matching recipes. Overflow is counted, not waited. */
size_t MacMetalStates_Misses(const MacMetalStates *, MacMetalKeyStorage *, size_t capacity);
/* Install before objects; supplied context outlives the renderer. prepare
 * begins loading; caller prewarms then Publish. Destruction requires uninstall
 * at an object-free lifecycle boundary, via Install(NULL). */
int MacMetalStates_Install(MacMetalStates *);
/* Engine bootstrap: install callbacks before Direct3DCreate9; preparation
 * lazily binds the transport which CreateDevice/presenter subsequently made.
 * Current is borrowed; Install(NULL) destroys a default-managed context.
 * An explicitly supplied context remains caller-owned after uninstall. */
int MacMetalStates_InstallDefault(void);
MacMetalStates *MacMetalStates_Current(void);
#ifdef __cplusplus
}
#endif
#endif
