#ifndef COD2_MACOS_METAL_PRESENT_H
#define COD2_MACOS_METAL_PRESENT_H

#include <stdint.h>

typedef struct MacMetalPresenter MacMetalPresenter;
struct SDL_Window;
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
typedef struct MacMetalGPU MacMetalGPU;
MacMetalGPU *MacMetal_NativeGPU(MacMetalPresenter *presenter);
MacMetalPresenter *MacMetal_CreateNativeWithGPU(struct SDL_Window *, int width, int height, int edr, MacMetalGPU *);
#endif

/* Main/render thread only; the owning CGL context must be current. A zero
 * texture means render to the private scratch FBO and skip this submission. */
MacMetalPresenter *MacMetal_Create(struct SDL_Window *window, int width, int height, int edr);
unsigned int MacMetal_BeginFrame(MacMetalPresenter *presenter);
void MacMetal_EndFrame(MacMetalPresenter *presenter, const unsigned short *ramp,
                       unsigned int revision, int identity);
void MacMetal_UpdateGeometry(MacMetalPresenter *presenter);
void MacMetal_Destroy(MacMetalPresenter *presenter);

/* Borrowed Objective-C objects, for a future native renderer using this
 * layer/device and the same worker/display-link lifetime. */
void *MacMetal_Device(MacMetalPresenter *presenter);
void *MacMetal_Layer(MacMetalPresenter *presenter);

/* Native backend mailbox: all pointers are borrowed id<MTL...> objects.
 * Begin returns NULL instead of waiting when storage is busy. End installs a
 * completion handler before the caller commits its rendering command buffer. */
MacMetalPresenter *MacMetal_CreateNative(struct SDL_Window *window, int width, int height, int edr);
void *MacMetal_BeginNativeFrame(MacMetalPresenter *presenter, int *token);
void MacMetal_EndNativeFrame(MacMetalPresenter *presenter, int token, void *commandBuffer,
                             const unsigned short *ramp, unsigned int revision, int identity);
/* Queue-independent completion, including Metal 4 shared-event listeners.
 * Seal on the producer before submission; Complete once GPU writes finish.
 * Every successful Seal requires Complete, including failed/cancelled work. */
int MacMetal_SealNativeFrame(MacMetalPresenter *presenter, int token,
                            const unsigned short *ramp, unsigned int revision, int identity);
void MacMetal_CompleteNativeFrame(MacMetalPresenter *presenter, int token, int success);

#endif
