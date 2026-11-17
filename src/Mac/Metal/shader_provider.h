#ifndef COD2_METAL_SHADER_PROVIDER_H
#define COD2_METAL_SHADER_PROVIDER_H
#include "metal_backend.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Producer/loading APIs. Describe pointers are borrowed until the next prepare.
 * Load does identity validation and ready lookup only; it never compiles. */
int MacMetalShaders_Prepare(const char *sourceCache, const char *translatedCache);
int MacMetalShaders_SetupAfterExtraction(const char *sourceCache);
void MacMetalShaders_BeginLoading(void);
void MacMetalShaders_SealLoading(void);
unsigned int MacMetalShaders_Count(void);
int MacMetalShaders_Describe(unsigned int index, MacMetalShaderLibrary *library);
int MacMetalShaders_Find(const char *name, MacMetalShaderLibrary *library);
const char *MacMetalShaders_CacheNamespace(void);
const char *MacMetalShaders_LastError(void);
int MacMetalShaders_LastStatus(void);
uint64_t MacMetalShaders_FactoryCount(void);
/* Typed fixed-function variants are prepared by WS31 during loading. A null
 * types array selects eight 2D stages. Ready access never creates functions. */
int MacMetalShaders_PrepareFixed(const uint32_t textureTypes[8]);
void *MacMetalShaders_FixedLibrary(const uint32_t textureTypes[8]);
void *MacMetalShaders_FixedFunction(const uint32_t textureTypes[8], const char *entry);
const char *MacMetalShaders_FixedSourceSHA256(const uint32_t textureTypes[8]);
/* Borrowed Objective-C library/function; retain in prepared PSOs/submissions.
 * payload is the result of ABI 1 MacMetal_LoadShader, never legacy C bytes. */
void *MacMetalShaders_Library(const void *payload);
void *MacMetalShaders_Function(const void *payload);
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
void *MacMetalShaders_AssociateReady(MacShimSidecar *, const MacMetalShaderLibrary *);
int MacMetalShaders_Description(const void *payload, MacMetalShaderLibrary *);
/* Returns -1 for an unused slot, otherwise 0=2D, 1=cube, 2=3D. */
int MacMetalShaders_TextureType(const void *payload, unsigned int slot);
#endif
const char *MacMetalShaders_StateBinding(const void *payload, unsigned int slot);
int MacMetalShaders_WriteLocal(void *payload, unsigned int first, const float *values, unsigned int count);
int MacMetalShaders_Snapshot(const void *payload, const MacMetalARBConstants *stage, MacMetalARBConstants *snapshot);
/* Resolve the source-reachable ARB fog state from the raw D3D shadow. Other
 * state bindings use Snapshot with an explicitly populated stage.state bank. */
int MacMetalShaders_SnapshotState(const void *payload, const MacMetalState *state, MacMetalARBConstants *snapshot);
#ifdef __cplusplus
}
#endif
#endif
