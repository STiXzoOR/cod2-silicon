#ifndef COD2_METAL_SHADER_FIXED_H
#define COD2_METAL_SHADER_FIXED_H
#include "metal_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original fixed-function program; all entry points compile at preparation.
 * Buffer 0 is normalized ARBInput, buffer 1 ABI1 ARBConstants. Textures and
 * samplers keep stage slots 0..7. This helper never creates GPU objects. */
typedef struct { float diffuse[4], specular[4], ambient[4], emissive[4], power; } MacMetalFixedMaterial;
typedef struct {
    uint32_t type;
    float diffuse[4], specular[4], ambient[4], position[3], direction[3];
    float range, falloff, attenuation[3], theta, phi;
} MacMetalFixedLight;
typedef struct {
    uint32_t fvf, enabled[8], textureTypes[8]; /* 0=2D, 1=cube, 2=3D */
    MacMetalFixedMaterial material;
    MacMetalFixedLight lights[8];
    float clipPlanes[6][4];
} MacMetalFixedState;
void MacMetalFixed_Init(MacMetalFixedState *);
/* Call once at device creation, before engine setters; touches fixed state only. */
void MacMetalFixed_InitDefaults(MacMetalState *);
int MacMetalFixed_Update(MacMetalFixedState *, const char *method, const void *const *arguments, void *result, size_t resultSize);
int MacMetalFixed_Pack(const MacMetalFixedState *, const MacMetalState *, MacMetalARBConstants *, char *error, size_t errorSize);
int MacMetalFixed_ExpandFVF(uint32_t fvf, const void *vertex, size_t bytes, MacMetalARBInput *);
const char *MacMetalFixed_Source(void);
/* malloc-owned source; release with free. Types participate in PSO identity. */
char *MacMetalFixed_SourceForTypes(const uint32_t textureTypes[8]);
#define MAC_METAL_FIXED_REVISION "fixed-abi1-r1"
#define MAC_METAL_FIXED_VERTEX "fixed_vertex"
#define MAC_METAL_FIXED_POST_VERTEX "fixed_post_vertex"
#define MAC_METAL_FIXED_FRAGMENT "fixed_fragment"
#define MAC_METAL_FIXED_COLORED "fixed_colored"
#define MAC_METAL_FIXED_SOLID "fixed_solid"
#define MAC_METAL_FIXED_COPY "fixed_copy"
#define MAC_METAL_FIXED_GAMMA "fixed_gamma"
#ifdef __cplusplus
}
#endif
#endif
