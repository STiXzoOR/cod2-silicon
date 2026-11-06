#ifndef COD2_METAL_FRAME_STATS_H
#define COD2_METAL_FRAME_STATS_H
/* Private producer diagnostics; not part of the ABI 1 provider tables. */
#ifdef __cplusplus
extern "C" {
#endif
int MacMetalFrame_Skipped(void);
void MacMetalFrame_Draw(int encoded, int failed);
struct MacMetalGPU;
int MacMetalGPU_Backpressure(struct MacMetalGPU *gpu);
#ifdef __cplusplus
}
#endif
#endif
