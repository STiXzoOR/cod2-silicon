#ifndef COD2_METAL_SHIM_DISPATCH_H
#define COD2_METAL_SHIM_DISPATCH_H
/* Coalesced zero hooks let fixtures extract a GL function without linking
 * the native backend. metal_dispatch supplies the single strong definition. */
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
#define MAC_METAL_NATIVE 1
#include <stddef.h>
typedef struct {
    int (*invoke)(const char *, const void *, const void *const *, void *, size_t);
} MacShimBackendOps;
int MacShim_InstallBackend(unsigned int backend, const MacShimBackendOps *ops);
void MacShim_SelectRenderer(int metal);
void MacShim_RecordObject(const void *object, int backend);
void MacShim_ForgetObject(const void *object);
typedef struct {
    int (*invoke)(const char *, const void *, const void *const *, void *, size_t);
    int (*isMetal)(void);
} MacShimBoundaryHooks;
#ifdef MAC_SHIM_IMPLEMENTATION
extern MacShimBoundaryHooks MacShim_Hooks;
#else
__attribute__((weak)) MacShimBoundaryHooks MacShim_Hooks;
#endif
int MacShim_Invoke(const char *, const void *, const void *const *, void *, size_t);
int MacShim_IsMetal(void);
static inline int MacShim_Dispatch(const char *method, const void *object,
                                   const void *const *arguments, void *result, size_t size)
{
    return MacShim_Hooks.invoke && MacShim_Hooks.invoke(method, object, arguments, result, size);
}
static inline int MacShim_Metal(void) { return MacShim_Hooks.isMetal && MacShim_Hooks.isMetal(); }
#else
#define MAC_METAL_NATIVE 0
#endif
#endif
