#ifndef COD2_METAL_SHADER_SETUP_H
#define COD2_METAL_SHADER_SETUP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Native, local setup. The callback never receives shader text; outputs stay
 * in the caller's external cache. Missing input returns 77 (clear test skip). */
typedef void (*MacMetalShaderResult)(const char *name, int status, const char *detail, void *context);
int MacMetal_PrepareShaderCache(const char *input, const char *output,
                               MacMetalShaderResult callback, void *context);

#ifdef __cplusplus
}
#endif
#endif
