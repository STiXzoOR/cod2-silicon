#include "metal_backend.h"
#include "metal_frame_stats.h"
#include "platform/macos_metal_present.h"
#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#include <stdio.h>
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
__attribute__((weak)) int MacMetalTargets_PrepareGPU(MacMetalGPU *gpu) { (void)gpu; return 1; }
__attribute__((weak)) void MacMetalTargets_StopGPU(void) {}
__attribute__((weak)) void MacMetalTargets_CancelClears(void) {}
__attribute__((weak)) int MacMetalTargets_HasClears(void) { return 0; }
__attribute__((weak)) int MacMetalTargets_FlushClears(MacMetalSubmission *slot) { (void)slot; return 1; }
#endif

/* SDL3 ScheduleContextUpdates queries the current NSOpenGLContext even when
 * its window has no GL contexts (release-3.4.16 SDL_cocoawindow.m:478). This
 * public class-method guard stops that dependency escape in native Metal mode.
 * The original IMP is called verbatim for GL windows and renderer restarts. */
static IMP originalCurrentContext;
void MacMetalBackend_PrepareWindow(void)
{
    if (!originalCurrentContext) {
        Method method = class_getClassMethod(NSOpenGLContext.class, @selector(currentContext));
        originalCurrentContext = method_getImplementation(method);
        IMP guard = imp_implementationWithBlock(^id(id cls) {
            if (MacShim_IsMetal()) return nil;
            return ((id (*)(id, SEL))originalCurrentContext)(cls, @selector(currentContext));
        });
        method_setImplementation(method, guard);
    }
}
static MacMetalPresenter *presenter;
static id<MTLTexture> scene;
static MacMetalGPU *gpu;
static MacMetalSubmission *submission;
static uint64_t frames, skippedFrames, partialFrames, lifetimeDrops, skipStreak, maxSkipStreak;
static uint64_t frameDraws, frameFailures;
static int frameCommitted;
static int skippedScene;
static FILE *frameTrace;
static double skippedSince, maxSkipSeconds;
int MacMetalFrame_Skipped(void) { return skippedScene == 1; }
void MacMetalFrame_Draw(int encoded, int failed)
{
    frameDraws += encoded;
    frameFailures += failed;
    lifetimeDrops += failed;
}
static uint64_t fenceValues[256];
static unsigned char pendingFences[256];
static uint64_t CommitProducer(void *context, MacMetalCompletion completion)
{
    uint64_t value = MacMetalGPU_Commit(submission, NULL, completion, context);
    if (value) frameCommitted = 1;
    submission = NULL;
    MacMetal_ProducerSubmitted(value);
    for (unsigned int i = 0; i < 256; ++i) if (pendingFences[i]) {
        fenceValues[i] = value;
        pendingFences[i] = 0;
    }
    return value;
}
static uint32_t clearColor;
static int renderWidth, renderHeight, pendingClear;
MacMetalGPU *MacMetal_GPU(void) { return gpu; }

static int StartWithGPU(struct SDL_Window *window, int width, int height, int edr, MacMetalGPU *shared)
{
    presenter = shared ? MacMetal_CreateNativeWithGPU(window, width, height, edr, shared) : MacMetal_CreateNative(window, width, height, edr);
    if (!presenter) return 0;
    gpu = MacMetal_NativeGPU(presenter);
    id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    scene = [device newTextureWithDescriptor:descriptor];
    renderWidth = width; renderHeight = height;
    pendingClear = 1;
    const char *trace = getenv("COD2_METAL_FRAME_TRACE");
    if (!frameTrace && trace && *trace) {
        frameTrace = fopen(trace, "w");
        if (frameTrace) fprintf(frameTrace, "time,frame,backpressure,encoded_draws,failures\n");
    }
    if (!scene) { MacMetalBackend_Stop(); return 0; }
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (!MacMetalTargets_PrepareGPU(gpu)) { MacMetalBackend_Stop(); return 0; }
#endif
    fprintf(stderr, "CoD2 Silicon renderer: Metal 4, shared device/queue/timeline, clear-only\n");
    return 1;
}
int MacMetalBackend_Start(struct SDL_Window *window, int width, int height, int edr)
{ return StartWithGPU(window, width, height, edr, NULL); }
static void CancelProducer(void)
{
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    MacMetalTargets_CancelClears();
#endif
    skippedScene = 0;
    if (!submission) return;
    MacMetalGPU_Cancel(submission); submission = NULL;
    memset(pendingFences, 0, sizeof(pendingFences));
    MacMetal_ProducerSubmitted(0);
}
void MacMetalBackend_Stop(void)
{
    fprintf(stderr, "Metal frames: total=%llu skipped frames (backpressure)=%llu partial_drops=%llu lifetime_drops=%llu max_skip_streak=%llu max_skip_ms=%.3f\n", (unsigned long long)frames, (unsigned long long)skippedFrames, (unsigned long long)partialFrames, (unsigned long long)lifetimeDrops, (unsigned long long)maxSkipStreak, maxSkipSeconds * 1000);
    if (frameTrace) fclose(frameTrace);
    frameTrace = NULL; skippedSince = maxSkipSeconds = 0;
    frames = skippedFrames = partialFrames = lifetimeDrops = skipStreak = maxSkipStreak = 0;
    CancelProducer();
    /* Presenter invalidates/joins drawable worker, drains all sealed scene
     * and presentation work, then destroys the shared queue/device. */
    MacMetal_Destroy(presenter); presenter = NULL;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    MacMetalTargets_StopGPU();
#endif
    MacMetal_CollectRetired(UINT64_MAX);
    gpu = NULL; scene = nil; pendingClear = skippedScene = 0;
}
int MacMetalBackend_Resize(struct SDL_Window *window, int width, int height, int edr)
{
    MacMetalGPU *shared = MacMetalGPU_Retain(gpu);
    CancelProducer();
    MacMetal_Destroy(presenter); presenter = NULL;
    int drained = MacMetalGPU_Drain(shared);
    scene = nil; gpu = NULL;
    int ready = drained && StartWithGPU(window, width, height, edr, shared);
    MacMetalGPU_Destroy(shared);
    if (ready) MacMetal_TargetResized(width, height);
    return ready;
}
static void EncodeClear(void)
{
    if (!submission || !scene) return;
    if (@available(macOS 26.0, *)) {
        id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(submission);
        MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
        pass.colorAttachments[0].texture = scene;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(((clearColor >> 16) & 255) / 255.0, ((clearColor >> 8) & 255) / 255.0, (clearColor & 255) / 255.0, ((clearColor >> 24) & 255) / 255.0);
        pass.renderTargetWidth = renderWidth; pass.renderTargetHeight = renderHeight;
        id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        [encoder endEncoding];
        MacMetalGPU_Keep(submission, (__bridge void *)scene, 1);
    }
}
MacMetalSubmission *MacMetalBackend_Submission(void)
{
    if (skippedScene) return NULL;
    if (!submission) submission = MacMetalGPU_Begin(gpu, 0);
    if (submission && pendingClear) { EncodeClear(); pendingClear = 0; }
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (submission && !MacMetalTargets_FlushClears(submission)) return NULL;
#endif
    return submission;
}
void MacMetalBackend_BeginScene(void)
{
    /* Admission is latched through Present. Retiring a slot mid-frame cannot
     * turn a skipped BSP prefix into a presented model/HUD suffix. */
    skippedScene = 0;
    frameDraws = frameFailures = 0;
    frameCommitted = 0;
    if (!MacMetalBackend_Submission()) {
        int pressure = MacMetalGPU_Backpressure(gpu);
        CancelProducer(); skippedScene = pressure ? 1 : 2;
        if (!pressure) MacMetalFrame_Draw(0, 1);
    }
}
void *MacMetalBackend_SceneTexture(void) { return (__bridge void *)scene; }
void MacMetalBackend_Clear(unsigned int flags, unsigned int color, float depth, unsigned int stencil)
{
    (void)depth; (void)stencil;
    if (flags & 1) { clearColor = color; pendingClear = 1; }
    MacMetalBackend_Submission();
}
static void CompleteScene(void *context, uint64_t value, int success)
{
    (void)value;
    int *token = context;
    MacMetal_CompleteNativeFrame(presenter, *token, success);
    free(token);
}
void MacMetalBackend_Present(const unsigned short *ramp, unsigned int revision, int identity)
{
    if (!gpu) return;
    ++frames;
    double now = CACurrentMediaTime();
    if (frameTrace) fprintf(frameTrace, "%.9f,%llu,%i,%llu,%llu\n", now, (unsigned long long)frames, skippedScene == 1, (unsigned long long)frameDraws, (unsigned long long)frameFailures);
    if ((skippedScene || frameFailures) && (frameDraws || frameCommitted)) ++partialFrames;
    if (skippedScene) {
        if (skippedScene == 1) {
            ++skippedFrames;
            if (!skipStreak) skippedSince = now;
            if (++skipStreak > maxSkipStreak) maxSkipStreak = skipStreak;
            if (now - skippedSince > maxSkipSeconds) maxSkipSeconds = now - skippedSince;
        }
        CancelProducer();
        return;
    }
    if (skipStreak && now - skippedSince > maxSkipSeconds) maxSkipSeconds = now - skippedSince;
    skipStreak = 0;
    MacMetal_UpdateGeometry(presenter);
    if (!submission) MacMetalBackend_Submission();
    if (!submission) return;
    int token = -1;
    id<MTLTexture> target = (__bridge id<MTLTexture>)MacMetal_BeginNativeFrame(presenter, &token);
    int *context = NULL;
    if (@available(macOS 26.0, *)) {
        if (target) {
            id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(submission);
            id<MTL4ComputeCommandEncoder> copy = [command computeCommandEncoder];
            [copy barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
            [copy copyFromTexture:scene sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(renderWidth, renderHeight, 1) toTexture:target destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
            [copy endEncoding];
            MacMetalGPU_Keep(submission, (__bridge void *)scene, 1);
            MacMetalGPU_Keep(submission, (__bridge void *)target, 1);
            context = malloc(sizeof(*context));
            if (context && MacMetal_SealNativeFrame(presenter, token, ramp, revision, identity)) *context = token;
            else { free(context); context = NULL; MacMetal_EndNativeFrame(presenter, token, NULL, NULL, 0, 1); }
        }
    }
    CommitProducer(context, context ? CompleteScene : NULL);
}
int MacMetalBackend_ReadPixels(int width, int height, int bytesPerPixel, unsigned char *pixels)
{
    if (!gpu || !scene || !pixels || width <= 0 || height <= 0 || width > renderWidth || height > renderHeight || (bytesPerPixel != 3 && bytesPerPixel != 4)) return 0;
    /* Read the actual pre-gamma Metal target, not the drawable or a CPU fill. */
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (pendingClear || MacMetalTargets_HasClears()) MacMetalBackend_Submission();
#else
    if (pendingClear) MacMetalBackend_Submission();
#endif
    if (submission) CommitProducer(NULL, NULL);
    if (!MacMetalGPU_Drain(gpu)) return 0;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (MacMetalTargets_HasClears()) {
        if (!MacMetalBackend_Submission()) return 0;
        CommitProducer(NULL, NULL);
        if (!MacMetalGPU_Drain(gpu)) return 0;
    }
#endif
    size_t pitch = (size_t)width * 4;
    unsigned char *bytes = malloc(pitch * height);
    if (!bytes) return 0;
    [scene getBytes:bytes bytesPerRow:pitch fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
    for (int i = 0; i < width * height; ++i) {
        pixels[i * bytesPerPixel] = bytes[i * 4 + 2];
        pixels[i * bytesPerPixel + 1] = bytes[i * 4 + 1];
        pixels[i * bytesPerPixel + 2] = bytes[i * 4];
        if (bytesPerPixel == 4) pixels[i * 4 + 3] = bytes[i * 4 + 3];
    }
    free(bytes); return 1;
}
static unsigned int nextFence;
void MacMetalFence_Gen(int count, unsigned int *ids)
{
    for (int i = 0; i < count; ++i) { unsigned int id = ++nextFence; if (id >= 256) id = nextFence = 1; ids[i] = id; fenceValues[id] = 0; pendingFences[id] = 0; }
}
void MacMetalFence_Set(unsigned int id)
{
    if (pendingClear) MacMetalBackend_Submission();
    if (id >= 256) return;
    /* End-of-scene fences normally retire with the following Present copy.
     * A test before Present flushes once; neither path waits for execution.
     * Event-query id zero requires its value immediately. */
    pendingFences[id] = submission != NULL;
    fenceValues[id] = MacMetalGPU_Submitted(gpu);
    if (!id && submission) CommitProducer(NULL, NULL);
}
int MacMetalFence_Test(unsigned int id)
{
    if (id >= 256) return 0;
    if (pendingFences[id] && submission) CommitProducer(NULL, NULL);
    return !pendingFences[id] && MacMetalGPU_Completed(gpu) >= fenceValues[id];
}
void MacMetalFence_Delete(int count, const unsigned int *ids)
{
    for (int i = 0; i < count; ++i) if (ids[i] < 256) {
        fenceValues[ids[i]] = 0; pendingFences[ids[i]] = 0;
    }
}
