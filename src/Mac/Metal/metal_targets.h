#ifndef COD2_METAL_TARGETS_H
#define COD2_METAL_TARGETS_H
#include "metal_backend.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Provider-local API; ABI 1 headers and engine layouts are unchanged. Views
 * passed to encoders are borrowed. Allocate returns an owned texture view. */
typedef struct MacMetalTargets MacMetalTargets;
typedef struct {
    void *texture;
    unsigned int level, slice, width, height;
} MacMetalTargetView;
typedef struct { int32_t left, top, right, bottom; } MacMetalTargetRect;
enum { MAC_METAL_TARGET_COLOR, MAC_METAL_TARGET_DEPTH, MAC_METAL_TARGET_STENCIL };
MacMetalTargets *MacMetalTargets_Create(MacMetalGPU *gpu);
void MacMetalTargets_Destroy(MacMetalTargets *targets);
int MacMetalTargets_Allocate(MacMetalTargets *, unsigned int width, unsigned int height,
                             unsigned int d3dFormat, MacMetalTargetView *out);
void MacMetalTargets_ReleaseView(MacMetalTargetView *view);
/* Each operation ends its encoder before returning. Draws must do likewise:
 * Begin always loads/stores contents and inserts a queue-stage hazard barrier. */
void *MacMetalTargets_Begin(MacMetalTargets *, MacMetalSubmission *,
                            const MacMetalTargetView *color, const MacMetalTargetView *depth);
void MacMetalTargets_End(void *encoder);
int MacMetalTargets_Clear(MacMetalTargets *, MacMetalSubmission *,
                          const MacMetalTargetView *color, const MacMetalTargetView *depth,
                          unsigned int count, const MacMetalTargetRect *rects,
                          unsigned int flags, uint32_t argb, float z, unsigned int stencil,
                          unsigned int colorMask, unsigned int stencilMask);
int MacMetalTargets_Stretch(MacMetalTargets *, MacMetalSubmission *,
                            const MacMetalTargetView *source, const MacMetalTargetRect *sourceRect,
                            const MacMetalTargetView *destination, const MacMetalTargetRect *destinationRect,
                            unsigned int filter);
int MacMetalTargets_Gamma(MacMetalTargets *, MacMetalSubmission *,
                          const MacMetalTargetView *source, const MacMetalTargetView *destination,
                          const unsigned short *rgb16Ramp, int identity);
/* Synchronization point for explicit readback only. Caller commits its producer
 * first; works with private targets and splits combined depth/stencil aspects. */
int MacMetalTargets_Read(MacMetalTargets *, const MacMetalTargetView *, unsigned int aspect,
                         void *bytes, size_t rowPitch);

/* Compose with WS30's resource table. Resolver supplies borrowed native texture
 * views for its sidecars; its payload layout is deliberately not assumed. */
typedef int (*MacMetalTargetResolver)(MacShimSidecar *, MacMetalTargetView *);
int MacMetalTargets_Install(const MacMetalResourceOps *base, MacMetalTargetResolver resolver);
int MacMetalTargets_PrepareGPU(MacMetalGPU *gpu);
void MacMetalTargets_StopGPU(void);
void MacMetalTargets_CancelClears(void);
int MacMetalTargets_HasClears(void);
int MacMetalTargets_FlushClears(MacMetalSubmission *submission);
int MacMetalTargets_Resolve(MacShimSidecar *, MacMetalTargetView *);
void *MacMetalTargets_BeginDraw(const MacMetalDraw *draw);
void MacMetalTargets_EndDraw(void *encoder);
#ifdef __cplusplus
}
#endif
#endif
