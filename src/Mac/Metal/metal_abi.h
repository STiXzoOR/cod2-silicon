#ifndef COD2_METAL_ABI_H
#define COD2_METAL_ABI_H
#include <stddef.h>
#include <stdint.h>

#define MAC_METAL_ABI_VERSION 1u
#define MAC_METAL_STREAM_COUNT 16u
#define MAC_METAL_TEXTURE_COUNT 16u
/* Table buffer 0 is declaration fetch input; 1 is the register block. Stream
 * addresses occupy 2..17. Compute validation uses output buffer 2 instead. */
enum { MAC_METAL_INPUT_BUFFER = 0, MAC_METAL_CONSTANT_BUFFER = 1,
       MAC_METAL_STREAM_BUFFER = 2, MAC_METAL_BUFFER_BINDINGS = 18 };
typedef struct __attribute__((aligned(16))) { float v[4]; } MacMetalFloat4;
typedef struct {
    MacMetalFloat4 attrib[16], texcoord[8], color[2], position, fogcoord;
} MacMetalARBInput;
typedef struct {
    MacMetalFloat4 env[256], local[256], state[64];
} MacMetalARBConstants;
typedef struct {
    MacMetalFloat4 position, color[2], texcoord[8], fogcoord, depth;
    uint32_t killed;
    uint32_t padding[3];
} MacMetalARBOutput;
/* Separate VS/PS env namespaces; locals belong to the shader, state to the
 * stage. Snapshots copy the complete block; never bind a mutable shadow. */
int MacMetal_WriteRegisters(MacMetalFloat4 *bank, uint32_t capacity,
                            uint32_t first, const float *data, uint32_t count);
#endif
