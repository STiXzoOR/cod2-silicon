#ifndef COD2_METAL_RESOURCES_H
#define COD2_METAL_RESOURCES_H
#include "metal_backend.h"
#include "resource_formats.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MAC_METAL_STORAGE_DEFAULT, MAC_METAL_STORAGE_SHARED, MAC_METAL_STORAGE_PRIVATE };
typedef struct { void *buffer; size_t offset, length; uint64_t generation; } MacMetalBufferBinding;
/* These provider-local APIs do not extend ABI 1. All calls use the producer
 * thread. Returned GPU pointers are borrowed; binding retains residency and
 * the C identity until the ABI submitted/cancel callback. */
const MacMetalResourceOps *MacMetalResources_Ops(void);
void *MacMetalResources_Create(MacShimKind, const MacMetalResourceDesc *, unsigned int storage);
int MacMetalResources_Buffer(MacShimSidecar *, MacMetalSubmission *, size_t offset,
                             size_t length, MacMetalBufferBinding *binding);
void *MacMetalResources_Texture(MacShimSidecar *, MacMetalSubmission *);
/* Borrowed render attachment; target provider retains it and tracks writes. */
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
void *MacMetalResources_Attachment(MacShimSidecar *);
#endif
void *MacMetalResources_NullTexture(MacShimKind, MacMetalSubmission *);
void *MacMetalResources_NullBuffer(MacMetalSubmission *);
/* Lifecycle-only, after Drain and all C resources are released. */
void MacMetalResources_Shutdown(void);
void MacMetalResources_Counts(size_t *resources, size_t *allocations, size_t *pending, size_t *ranges);
#ifdef __cplusplus
}
#endif
#endif
