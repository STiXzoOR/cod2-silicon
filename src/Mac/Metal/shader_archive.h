#ifndef COD2_METAL_SHADER_ARCHIVE_H
#define COD2_METAL_SHADER_ARCHIVE_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct MacMetalShaderArchive MacMetalShaderArchive;
/* WS31 loading helper. The namespace is MacMetalShaders_CacheNamespace (or a
 * complete ABI-1 PSO namespace hash). Directory must be Application Support.
 * Uses the supplied device; creates no device, queue or command submission. */
MacMetalShaderArchive *MacMetalShaderArchive_Open(void *device, const char *directory, const char *namespaceHash);
/* descriptor is MTL4RenderPipelineDescriptor. Returns an owned ObjC pipeline
 * (+1); a C/ObjC caller consumes it with CFBridgingRelease/bridge_transfer.
 * No factories accepted after Finish. Ready PSO lookup belongs to WS31. */
void *MacMetalShaderArchive_Render(MacMetalShaderArchive *, void *descriptor);
int MacMetalShaderArchive_Finish(MacMetalShaderArchive *);
unsigned int MacMetalShaderArchive_Hits(const MacMetalShaderArchive *);
void MacMetalShaderArchive_Close(MacMetalShaderArchive *);
#ifdef __cplusplus
}
#endif
#endif
