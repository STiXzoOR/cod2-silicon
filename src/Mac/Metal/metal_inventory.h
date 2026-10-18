#ifndef COD2_METAL_INVENTORY_H
#define COD2_METAL_INVENTORY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Producer-thread diagnostics, outside the frozen provider ABI. Counts cover
 * live resource payloads and unique ready pipelines, not cumulative factories. */
void MacMetalResources_Inventory(size_t *textures, size_t *buffers, size_t *bufferBytes);
size_t MacMetalStates_ResidentPipelines(void);
#ifdef __cplusplus
}
#endif
#endif
