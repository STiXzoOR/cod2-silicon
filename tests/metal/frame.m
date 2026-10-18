#include "Mac/Metal/metal_backend.h"
#include "Mac/Metal/shim_dispatch.h"
#import <SDL2/SDL.h>
#import <Metal/Metal.h>
#include <assert.h>
#include <stdio.h>
extern unsigned int MacMetalTest_Cancellations;

/* Include the real presenter/frame implementations so this fixture can wrap
 * the lifecycle boundary without adding an API used only by tests. */
#include "../../src/platform/macos_metal_present.m"
static void PendingWorkDestroy(MacMetalPresenter *p);
#define MacMetal_Destroy PendingWorkDestroy
#include "../../src/Mac/Metal/metal_frame.m"
#undef MacMetal_Destroy

static dispatch_semaphore_t pendingDestroyEntered, pendingDestroyReturned;
static atomic_int pendingCallbacks, pendingReleases;
typedef struct {
    MacMetalPresenter *presenter;
    MacMetalGPU *gpu;
    void *gate;
    uint64_t value;
    int token, observedBlocked;
} PendingWorkCheck;
static PendingWorkCheck *pendingLifecycle;

@interface PendingWorkResource : NSObject
@end
@implementation PendingWorkResource
- (void)dealloc { atomic_fetch_add(&pendingReleases, 1); }
@end

static void PendingWorkDestroy(MacMetalPresenter *p)
{
    if (pendingLifecycle && pendingLifecycle->presenter == p)
        dispatch_semaphore_signal(pendingDestroyEntered);
    MacMetal_Destroy(p);
    if (pendingLifecycle && pendingLifecycle->presenter == p)
        dispatch_semaphore_signal(pendingDestroyReturned);
}

static void StopPendingWorker(MacMetalPresenter *p)
{
    atomic_store(&p->stopping, true);
    dispatch_semaphore_signal(p->wake);
    if (p->threadStarted) pthread_join(p->thread, NULL);
    p->threadStarted = 0;
}

static void *ReleasePendingGate(void *opaque)
{
    @autoreleasepool {
        PendingWorkCheck *check = opaque;
        assert(!dispatch_semaphore_wait(pendingDestroyEntered,
            dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC)));
        /* The operation has reached the real teardown boundary. Completion
         * cannot advance until this thread opens the independent GPU event. */
        assert(dispatch_semaphore_wait(pendingDestroyReturned,
            dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC)) != 0);
        assert(atomic_load(&pendingCallbacks) == 0);
        assert(atomic_load(&pendingReleases) == 0);
        assert(MacMetalGPU_Completed(check->gpu) < check->value);
        id<MTLSharedEvent> gate = (__bridge id<MTLSharedEvent>)check->gate;
        assert(gate.signaledValue == 0);
        check->observedBlocked = 1;
        gate.signaledValue = 1;
    }
    return NULL;
}

static void CompletePendingScene(void *opaque, uint64_t value, int success)
{
    PendingWorkCheck *check = opaque;
    assert(success && value == check->value);
    MacMetal_CompleteNativeFrame(check->presenter, check->token, success);
    atomic_fetch_add(&pendingCallbacks, 1);
}
static void CompletePendingPresentation(void *opaque, uint64_t value, int success)
{
    assert(success && value == pendingLifecycle->value);
    CompletePresentation(opaque, value, success);
    atomic_fetch_add(&pendingCallbacks, 1);
}

static void CheckPendingLifecycle(SDL_Window *window, int resize)
{
    if (@available(macOS 26.0, *)) {
        MacMetalPresenter *old = presenter;
        MacMetalGPU *shared = MacMetal_GPU();
        void *queueIdentity = MacMetalGPU_Queue(shared);
        void *eventIdentity = MacMetalGPU_Event(shared);
        assert(old && shared);
        StopPendingWorker(old);
        assert(MacMetalGPU_Drain(shared));
        uint64_t previous = MacMetalGPU_Submitted(shared);
        int token = -1;
        id<MTLTexture> source = (__bridge id<MTLTexture>)MacMetal_BeginNativeFrame(old, &token);
        assert(source && token >= 0);
        PendingWorkCheck check = { old, shared, NULL, 0, token, 0 };
        id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(shared);
        id<MTLSharedEvent> gate = [device newSharedEvent];
        assert(gate);
        check.gate = (__bridge void *)gate;
        atomic_store(&pendingCallbacks, 0);
        atomic_store(&pendingReleases, 0);
        pendingDestroyEntered = dispatch_semaphore_create(0);
        pendingDestroyReturned = dispatch_semaphore_create(0);
        pendingLifecycle = &check;
        MacMetalSubmission *work = MacMetalGPU_Begin(shared, resize ? 0 : 1);
        assert(work);
        id<MTLTexture> target = source;
        if (resize) {
            id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(work);
            MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
            pass.colorAttachments[0].texture = source;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(.25, .5, .75, 1);
            pass.renderTargetWidth = old->width; pass.renderTargetHeight = old->height;
            id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
            assert(encoder); [encoder endEncoding];
            assert(MacMetal_SealNativeFrame(old, token, NULL, 0, 1));
            assert(atomic_load(&old->slots[token].state) == SLOT_NATIVE_PENDING);
        } else {
            size_t count = (size_t)old->width * old->height * 4;
            unsigned char *bytes = malloc(count); assert(bytes);
            memset(bytes, 127, count);
            [source replaceRegion:MTLRegionMake2D(0, 0, old->width, old->height)
                mipmapLevel:0 withBytes:bytes bytesPerRow:old->width * 4];
            free(bytes);
            assert(MacMetal_SealNativeFrame(old, token, NULL, 0, 1));
            MacMetal_CompleteNativeFrame(old, token, 1);
            MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                width:old->width height:old->height mipmapped:NO];
            descriptor.storageMode = MTLStorageModeShared;
            descriptor.usage = MTLTextureUsageRenderTarget;
            target = [device newTextureWithDescriptor:descriptor]; assert(target);
            assert(EncodeNativePresentation(old, &old->slots[token], target, work));
            atomic_store(&old->slots[token].state, SLOT_METAL);
            atomic_store(&old->admission->pending, atomic_load(&old->slots[token].sequence));
            dispatch_group_enter(old->inFlight);
        }
        MacMetalGPU_Keep(work, (__bridge void *)source, 1);
        MacMetalGPU_Keep(work, (__bridge void *)target, 1);
        PendingWorkResource *resource = [PendingWorkResource new];
        MacMetalGPU_Keep(work, (__bridge void *)resource, 0); resource = nil;
        id<MTL4CommandQueue> queue = (__bridge id<MTL4CommandQueue>)queueIdentity;
        [queue waitForEvent:gate value:1];
        if (resize) {
            /* Completion cannot execute while the independent event is zero;
             * publish its expected value before committing the command. */
            check.value = previous + 1;
            assert(MacMetalGPU_Commit(work, NULL, CompletePendingScene, &check) == check.value);
        } else {
            NativePresentCompletion *completion = calloc(1, sizeof(*completion)); assert(completion);
            *completion = (NativePresentCompletion){ old, &old->slots[token] };
            check.value = previous + 1;
            assert(MacMetalGPU_Commit(work, NULL, CompletePendingPresentation, completion) == check.value);
        }
        assert(MacMetalGPU_Completed(shared) < check.value);
        pthread_t releaseThread;
        assert(!pthread_create(&releaseThread, NULL, ReleasePendingGate, &check));
        if (resize) {
            assert(MacMetalBackend_Resize(window, 31, 17, 0));
            assert(MacMetal_GPU() == shared && MacMetalGPU_Queue(shared) == queueIdentity && MacMetalGPU_Event(shared) == eventIdentity);
            assert(MacMetalGPU_Submitted(shared) >= check.value && MacMetalGPU_Completed(shared) >= check.value);
        } else {
            MacMetalBackend_Stop();
            assert(!MacMetal_GPU() && !presenter && !MacMetalBackend_SceneTexture());
        }
        assert(!pthread_join(releaseThread, NULL));
        assert(check.observedBlocked && atomic_load(&pendingCallbacks) == 1 && atomic_load(&pendingReleases) == 1);
        assert(!dispatch_semaphore_wait(pendingDestroyReturned, DISPATCH_TIME_NOW));
        pendingLifecycle = NULL;
        pendingDestroyEntered = pendingDestroyReturned = nil;
        if (resize) {
            unsigned char pixels[31 * 17 * 4];
            MacMetalBackend_Clear(1, 0xff258744, 1, 0);
            assert(MacMetalBackend_ReadPixels(31, 17, 4, pixels));
            assert(MacMetalGPU_Submitted(shared) > check.value);
            for (unsigned int i = 0; i < 31 * 17; ++i)
                assert(pixels[i * 4] == 37 && pixels[i * 4 + 1] == 135 && pixels[i * 4 + 2] == 68 && pixels[i * 4 + 3] == 255);
        }
        printf("Metal pending %s: submitted %s GPU work blocked teardown until event release; completion/resource retirement exact\n",
            resize ? "resize" : "stop", resize ? "sealed scene" : "presentation");
    }
}

int main(void)
{
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (@available(macOS 26.0, *)) {
            if (!device || ![device supportsFamily:MTLGPUFamilyMetal4]) {
                puts("SKIP: Metal 4 device unavailable"); return 77;
            }
        } else {
            puts("SKIP: Metal 4 requires macOS 26"); return 77;
        }
        MacShim_SelectRenderer(1);
        MacMetalBackend_PrepareWindow();
        assert(SDL_Init(SDL_INIT_VIDEO) == 0);
        SDL_Window *window = SDL_CreateWindow("Metal core fixture", 0, 0, 19, 13, SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN);
        assert(window);
        assert(MacMetalBackend_Start(window, 19, 13, 0));
        MacMetalGPU *gpu = MacMetal_GPU();
        void *queue = MacMetalGPU_Queue(gpu), *event = MacMetalGPU_Event(gpu);
        unsigned char pixels[27 * 15 * 4];
        MacMetalBackend_Clear(1, 0xff1d4997, 1, 0);
        assert(MacMetalBackend_ReadPixels(19, 13, 3, pixels));
        for (int i = 0; i < 19 * 13; ++i)
            assert(pixels[i * 3] == 29 && pixels[i * 3 + 1] == 73 && pixels[i * 3 + 2] == 151);
        uint64_t previous = MacMetalGPU_Submitted(gpu);
        unsigned int cancellations = MacMetalTest_Cancellations;
        assert(MacMetalBackend_Submission());
        assert(previous && MacMetalBackend_Resize(window, 27, 15, 0));
        assert(MacMetalTest_Cancellations == cancellations + 1);
        assert(MacMetal_GPU() == gpu && MacMetalGPU_Queue(gpu) == queue && MacMetalGPU_Event(gpu) == event);
        MacMetalBackend_Clear(1, 0xff258744, 1, 0);
        assert(MacMetalBackend_ReadPixels(27, 15, 4, pixels));
        assert(MacMetalGPU_Submitted(gpu) > previous);
        for (int i = 0; i < 27 * 15; ++i)
            assert(pixels[i * 4] == 37 && pixels[i * 4 + 1] == 135 && pixels[i * 4 + 2] == 68 && pixels[i * 4 + 3] == 255);
        /* Simulate draw output independent of the engine's last clear value.
         * A screenshot/fence can commit it before Present acquires a new slot. */
        if (@available(macOS 26.0, *)) {
            MacMetalSubmission *draw = MacMetalBackend_Submission();
            assert(draw);
            id<MTLTexture> target = (__bridge id<MTLTexture>)MacMetalBackend_SceneTexture();
            MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
            pass.colorAttachments[0].texture = target;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(79 / 255.0, 107 / 255.0, 211 / 255.0, 1);
            pass.renderTargetWidth = 27; pass.renderTargetHeight = 15;
            id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(draw);
            id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
            [encoder endEncoding];
            MacMetalGPU_Keep(draw, (__bridge void *)target, 1);
        }
        assert(MacMetalBackend_ReadPixels(27, 15, 4, pixels));
        MacMetalBackend_Present(NULL, 0, 1);
        assert(MacMetalBackend_ReadPixels(27, 15, 4, pixels));
        for (int i = 0; i < 27 * 15; ++i)
            assert(pixels[i * 4] == 79 && pixels[i * 4 + 1] == 107 && pixels[i * 4 + 2] == 211 && pixels[i * 4 + 3] == 255);
        /* A draw acquired the slot without a color clear. Presentation must
         * make the scene resident for its copy independently of clear. */
        assert(MacMetalBackend_Submission());
        MacMetalBackend_Present(NULL, 0, 1);
        assert(MacMetalBackend_ReadPixels(27, 15, 4, pixels));
        for (int i = 0; i < 27 * 15; ++i)
            assert(pixels[i * 4] == 79 && pixels[i * 4 + 1] == 107 && pixels[i * 4 + 2] == 211 && pixels[i * 4 + 3] == 255);
        /* A tail fence and Present share one producer submission. Testing a
         * fence before Present still submits it without waiting for the GPU. */
        previous = MacMetalGPU_Submitted(gpu);
        MacMetalBackend_Clear(1, 0xff258744, 1, 0);
        unsigned int fence;
        MacMetalFence_Gen(1, &fence);
        MacMetalFence_Set(fence);
        assert(MacMetalGPU_Submitted(gpu) == previous);
        MacMetalBackend_Present(NULL, 0, 1);
        assert(MacMetalGPU_Submitted(gpu) == previous + 1);
        assert(MacMetalGPU_Drain(gpu) && MacMetalFence_Test(fence));
        previous = MacMetalGPU_Submitted(gpu);
        assert(MacMetalBackend_Submission());
        MacMetalFence_Set(fence);
        MacMetalFence_Test(fence);
        assert(MacMetalGPU_Submitted(gpu) == previous + 1);
        assert(MacMetalGPU_Drain(gpu) && MacMetalFence_Test(fence));
        MacMetalFence_Delete(1, &fence);
        /* Slot retirement during a rejected frame must not admit its suffix. */
        MacMetalSubmission *a = MacMetalGPU_Begin(gpu, 0), *b = MacMetalGPU_Begin(gpu, 0);
        assert(a && b);
        MacMetalBackend_BeginScene();
        assert(!MacMetalBackend_Submission());
        MacMetalGPU_Cancel(a); MacMetalGPU_Cancel(b);
        assert(!MacMetalBackend_Submission());
        previous = MacMetalGPU_Submitted(gpu);
        MacMetalBackend_Present(NULL, 0, 1);
        assert(MacMetalGPU_Submitted(gpu) == previous);
        MacMetalBackend_BeginScene();
        assert(MacMetalBackend_Submission());
        MacMetalBackend_Present(NULL, 0, 1);
        assert(MacMetalGPU_Drain(gpu));
        assert(!MacMetalBackend_ReadPixels(28, 15, 3, pixels));
        CheckPendingLifecycle(window, 1);
        cancellations = MacMetalTest_Cancellations;
        assert(MacMetalBackend_Submission());
        MacMetalBackend_Stop();
        assert(MacMetalTest_Cancellations == cancellations + 1);
        assert(MacMetalBackend_Start(window, 31, 17, 0));
        CheckPendingLifecycle(window, 0);
        SDL_DestroyWindow(window); SDL_Quit();
    }
    puts("Metal frame: engine ARGB clear, real RGB/RGBA readback, resize preserves queue/timeline");
}
