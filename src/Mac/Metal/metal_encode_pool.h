#ifndef COD2_METAL_ENCODE_POOL_H
#define COD2_METAL_ENCODE_POOL_H
#include "metal_core.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Transport-private optimization, outside ABI 1. Each returned slice/table
 * stays immutable until this submission's GPU feedback. Pools are bounded;
 * excess work uses the original completion-retained allocations. */
uint64_t MacMetalGPU_CopyConstants(MacMetalSubmission *, const void *, size_t);
void *MacMetalGPU_ArgumentTable(MacMetalSubmission *);
#ifdef __cplusplus
}
#endif
#endif
