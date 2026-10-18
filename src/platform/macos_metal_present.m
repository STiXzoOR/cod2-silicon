#include "macos_metal_present.h"
#include "Mac/Metal/metal_core.h"
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CAMetalDisplayLink.h>
#import <IOSurface/IOSurface.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdatomic.h>
#include <pthread.h>
#include <dlfcn.h>

enum { SLOT_FREE, SLOT_RENDERING, SLOT_GL_PENDING, SLOT_READY, SLOT_METAL, SLOT_NATIVE_PENDING };
enum { SLOT_COUNT = 4 };
typedef struct {
    atomic_int state;
    atomic_uint_fast64_t sequence;
    IOSurfaceRef surface;
    GLuint glTexture;
    GLsync fence;
    id<MTLTexture> texture;
    float ramp[768];
    unsigned int gammaRevision;
    int identity;
} MacMetalSlot;

@interface Cod2MetalView : NSView
@end
@implementation Cod2MetalView
- (NSView *)hitTest:(NSPoint)point { (void)point; return nil; }
@end

@interface Cod2MetalAdmission : NSObject {
@public
    atomic_uint_fast64_t pending;
}
@end
@implementation Cod2MetalAdmission
@end

struct MacMetalPresenter {
    MacMetalSlot slots[SLOT_COUNT];
    int current, width, height, edr, usesGL;
    uint64_t sequence;
    atomic_bool stopping;
    atomic_bool wakePending;
    atomic_uint_fast64_t submittedSequence;
    pthread_t thread;
    int threadStarted;
    int displayLink;
    dispatch_semaphore_t wake;
    dispatch_group_t inFlight;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    MacMetalGPU *gpu;
    id<MTLRenderPipelineState> pipeline;
    CAMetalLayer *layer;
    Cod2MetalView *view;
    Cod2MetalAdmission *admission;
    void (*observe)(int, double, double, uint64_t);
};

/* Original presentation-only shader. No game shaders are translated/bundled.
 * Integer reads preserve classic RGBA8 values and GL's bottom-left origin.
 * The gamma lookup matches macos_gamma.h, before optional EDR encoding. */
static const char presentSource[] =
    "#include <metal_stdlib>\nusing namespace metal;\n"
    "struct Params { uint identity; uint edr; uint glOrigin; };\n"
    "vertex float4 present_vertex(uint i [[vertex_id]]) {\n"
    " float2 p[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };\n"
    " return float4(p[i],0,1); }\n"
    "fragment float4 present_fragment(float4 pos [[position]],\n"
    " texture2d<float> scene [[texture(0)]],\n"
    " constant float *ramp [[buffer(0)]], constant Params &p [[buffer(1)]]) {\n"
    " uint2 xy = uint2(pos.xy); if (p.glOrigin) xy.y = scene.get_height()-1-xy.y;\n"
    " float4 c = scene.read(xy);\n"
    " if (!p.identity) { uint3 i = uint3(c.rgb*255.0f+0.5f);\n"
    " c.rgb = float3(ramp[i.r],ramp[256+i.g],ramp[512+i.b]); }\n"
    " if (p.edr) c.rgb = select(pow((c.rgb+0.055f)/1.055f,float3(2.4f)),\n"
    " c.rgb/12.92f,c.rgb<=0.04045f);\n"
    " return c; }\n";

static int LatestReady(MacMetalPresenter *p)
{
    int newest = -1;
    uint64_t sequence = atomic_load(&p->submittedSequence);
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (atomic_load_explicit(&p->slots[i].state, memory_order_acquire) == SLOT_READY) {
            uint64_t candidate = atomic_load(&p->slots[i].sequence);
            if (candidate > sequence) {
                newest = i;
                sequence = candidate;
            }
        }
    }
    return newest;
}

static int EncodePresentation(MacMetalPresenter *p, MacMetalSlot *slot,
                              id<MTLTexture> target, id<MTLCommandBuffer> command)
{
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
    if (!encoder)
        return 0;
    [encoder setRenderPipelineState:p->pipeline];
    [encoder setFragmentTexture:slot->texture atIndex:0];
    [encoder setFragmentBytes:slot->ramp length:sizeof(slot->ramp) atIndex:0];
    uint32_t params[3] = { slot->identity, p->edr, p->usesGL };
    [encoder setFragmentBytes:params length:sizeof(params) atIndex:1];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    return 1;
}

typedef struct { MacMetalPresenter *presenter; MacMetalSlot *slot; } NativePresentCompletion;
static void CompletePresentation(void *context, uint64_t value, int success)
{
    (void)value;
    NativePresentCompletion *completion = context;
    uint64_t sequence = atomic_load(&completion->slot->sequence);
    atomic_compare_exchange_strong(&completion->presenter->admission->pending, &sequence, 0);
    if (completion->presenter->observe)
        completion->presenter->observe(success ? 3 : 4, CACurrentMediaTime(), 0, atomic_load(&completion->slot->sequence));
    atomic_store_explicit(&completion->slot->state, SLOT_FREE, memory_order_release);
    dispatch_group_leave(completion->presenter->inFlight);
    free(completion);
}

static int EncodeNativePresentation(MacMetalPresenter *p, MacMetalSlot *slot,
                                     id<MTLTexture> target, MacMetalSubmission *submission)
{
    if (@available(macOS 26.0, *)) {
        id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(submission);
        MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.renderTargetWidth = p->width;
        pass.renderTargetHeight = p->height;
        id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        MTL4ArgumentTableDescriptor *descriptor = [MTL4ArgumentTableDescriptor new];
        descriptor.maxBufferBindCount = 2;
        descriptor.maxTextureBindCount = 1;
        id<MTL4ArgumentTable> table = [p->device newArgumentTableWithDescriptor:descriptor error:nil];
        id<MTLBuffer> gamma = [p->device newBufferWithBytes:slot->ramp length:sizeof(slot->ramp) options:MTLResourceStorageModeShared];
        uint32_t params[3] = { slot->identity, p->edr, 0 };
        id<MTLBuffer> parameters = [p->device newBufferWithBytes:params length:sizeof(params) options:MTLResourceStorageModeShared];
        if (!encoder || !table || !gamma || !parameters) {
            [encoder endEncoding]; return 0;
        }
        [table setAddress:gamma.gpuAddress atIndex:0];
        [table setAddress:parameters.gpuAddress atIndex:1];
        [table setTexture:slot->texture.gpuResourceID atIndex:0];
        [encoder setRenderPipelineState:p->pipeline];
        [encoder setArgumentTable:table atStages:MTLRenderStageFragment];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        MacMetalGPU_Keep(submission, (__bridge void *)slot->texture, 1);
        MacMetalGPU_Keep(submission, (__bridge void *)target, 1);
        MacMetalGPU_Keep(submission, (__bridge void *)gamma, 1);
        MacMetalGPU_Keep(submission, (__bridge void *)parameters, 1);
        MacMetalGPU_Keep(submission, (__bridge void *)table, 0);
        MacMetalGPU_Keep(submission, (__bridge void *)p->pipeline, 0);
        return 1;
    }
    return 0;
}

static int PresentNative(MacMetalPresenter *p, MacMetalSlot *slot, id<CAMetalDrawable> drawable)
{
    uint64_t sequence = atomic_load(&slot->sequence), expected = 0;
    if (!atomic_compare_exchange_strong(&p->admission->pending, &expected, sequence)) return 0;
    MacMetalSubmission *submission = MacMetalGPU_Begin(p->gpu, 1);
    if (!submission) { atomic_store(&p->admission->pending, 0); return 0; }
    NativePresentCompletion *completion = calloc(1, sizeof(*completion));
    if (!completion || !EncodeNativePresentation(p, slot, drawable.texture, submission)) {
        MacMetalGPU_Cancel(submission); free(completion);
        atomic_store(&p->admission->pending, 0); return 0;
    }
    *completion = (NativePresentCompletion){ p, slot };
    dispatch_group_enter(p->inFlight);
    void (*observe)(int, double, double, uint64_t) = p->observe;
    /* Admission covers shared-queue GPU work, not delayed physical callbacks.
     * The layer can outlive the presenter: capture only the observer here. */
    [drawable addPresentedHandler:^(id<MTLDrawable> presented) {
        if (observe) observe(1, presented.presentedTime, 0, sequence);
    }];
    if (observe) observe(0, CACurrentMediaTime(), 0, sequence);
    MacMetalGPU_Commit(submission, (__bridge void *)drawable, CompletePresentation, completion);
    return 1;
}

static void PresentDrawable(MacMetalPresenter *p, id<CAMetalDrawable> drawable)
{
    if (!drawable || atomic_load(&p->stopping))
        return;
    if (!p->usesGL && atomic_load(&p->admission->pending)) return;
    /* Acquire the drawable first. Frames completed while acquisition waited
     * supersede old images; no client thread ever waits for this worker. */
    int index = LatestReady(p);
    if (index < 0)
        return;
    MacMetalSlot *slot = &p->slots[index];
    int expected = SLOT_READY;
    if (!atomic_compare_exchange_strong(&slot->state, &expected, SLOT_METAL))
        return;
    uint64_t sequence = atomic_load(&slot->sequence);
    if (!p->usesGL) {
        if (PresentNative(p, slot, drawable))
            atomic_store(&p->submittedSequence, sequence);
        else
            atomic_store(&slot->state, SLOT_READY);
        return;
    }
    id<MTLCommandBuffer> command = [p->queue commandBuffer];
    if (!command || !EncodePresentation(p, slot, drawable.texture, command)) {
        atomic_store(&slot->state, SLOT_FREE);
        return;
    }
    /* Older completed slots remain reclaimable by the producer, but can
     * never be displayed after a newer submission, even if rendering pauses. */
    atomic_store(&p->submittedSequence, sequence);
    dispatch_group_enter(p->inFlight);
    [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
        if (completed.status == MTLCommandBufferStatusError)
            fprintf(stderr, "CoD2 Silicon Metal present: %s\n", completed.error.localizedDescription.UTF8String);
        /* GPU reads have finished before GL can reuse this IOSurface. */
        atomic_store_explicit(&slot->state, SLOT_FREE, memory_order_release);
        dispatch_group_leave(p->inFlight);
    }];
    void (*observe)(int, double, double, uint64_t) = p->observe;
    if (observe) {
        [drawable addPresentedHandler:^(id<MTLDrawable> presented) {
            /* The layer can outlive presenter teardown. Capture only the
             * observer function, never the presenter's freed storage. */
            observe(1, presented.presentedTime, 0, sequence);
        }];
        observe(0, CACurrentMediaTime(), 0, sequence);
    }
    [command presentDrawable:drawable];
    [command commit];
}

API_AVAILABLE(macos(14.0))
@interface Cod2MetalDisplayDelegate : NSObject <CAMetalDisplayLinkDelegate>
@property(nonatomic, assign) MacMetalPresenter *presenter;
@end
@implementation Cod2MetalDisplayDelegate
- (void)metalDisplayLink:(CAMetalDisplayLink *)link needsUpdate:(CAMetalDisplayLinkUpdate *)update
{
    (void)link;
    MacMetalPresenter *p = self.presenter;
    if (p->observe)
        p->observe(2, update.targetTimestamp, update.targetPresentationTimestamp, 0);
    PresentDrawable(p, update.drawable);
}
@end

static void *PresentThread(void *opaque)
{
    MacMetalPresenter *p = opaque;
    pthread_setname_np("CoD2 Metal present");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    @autoreleasepool {
        if (@available(macOS 14.0, *)) {
            Cod2MetalDisplayDelegate *delegate = [Cod2MetalDisplayDelegate new];
            delegate.presenter = p;
            CAMetalDisplayLink *link = [[CAMetalDisplayLink alloc] initWithMetalLayer:p->layer];
            link.delegate = delegate;
            /* A range lets the display link track ProMotion/Adaptive-Sync;
             * this clock never changes the client's independent 3 ms cap. */
            link.preferredFrameRateRange = CAFrameRateRangeMake(0, 0, 0);
            link.preferredFrameLatency = 1;
            [link addToRunLoop:NSRunLoop.currentRunLoop forMode:NSRunLoopCommonModes];
            while (!atomic_load(&p->stopping)) {
                @autoreleasepool {
                    [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
                }
            }
            [link invalidate];
            link.delegate = nil;
        } else {
            while (!atomic_load(&p->stopping)) {
                dispatch_semaphore_wait(p->wake, dispatch_time(DISPATCH_TIME_NOW, 10000000));
                atomic_store(&p->wakePending, false);
                @autoreleasepool {
                    if (LatestReady(p) >= 0 && !atomic_load(&p->stopping))
                        PresentDrawable(p, [p->layer nextDrawable]);
                }
            }
        }
    }
    return NULL;
}

void MacMetal_UpdateGeometry(MacMetalPresenter *p)
{
    if (!p)
        return;
    NSView *parent = p->view.superview;
    NSRect bounds = parent.bounds;
    double scale = fmin(bounds.size.width / p->width, bounds.size.height / p->height);
    NSSize size = NSMakeSize(p->width * scale, p->height * scale);
    NSRect frame = NSMakeRect((bounds.size.width - size.width) / 2,
                             (bounds.size.height - size.height) / 2, size.width, size.height);
    if (!NSEqualRects(p->view.frame, bounds))
        p->view.frame = bounds;
    if (!CGRectEqualToRect(p->layer.frame, NSRectToCGRect(frame)))
        p->layer.frame = NSRectToCGRect(frame);
    CGFloat backing = parent.window.backingScaleFactor;
    if (p->layer.contentsScale != backing)
        p->layer.contentsScale = backing;
    /* Exact render backing, including Retina/Spaces. CoreAnimation scales the
     * layer into the aspect-preserving view; no desktop-sized render target. */
    CGSize drawable = CGSizeMake(p->width, p->height);
    if (!CGSizeEqualToSize(p->layer.drawableSize, drawable))
        p->layer.drawableSize = drawable;
}

static MacMetalPresenter *CreatePresenter(SDL_Window *window, int width, int height, int edr, int usesGL, MacMetalGPU *sharedGPU)
{
    if (!window || width <= 0 || height <= 0)
        return NULL;
    MacMetalPresenter *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;
    p->current = -1;
    p->width = width;
    p->height = height;
    p->edr = edr != 0;
    p->usesGL = usesGL;
    if (!usesGL) {
        p->admission = [Cod2MetalAdmission new];
        atomic_init(&p->admission->pending, 0);
    }
    atomic_init(&p->stopping, false);
    atomic_init(&p->wakePending, false);
    atomic_init(&p->submittedSequence, 0);
    if (@available(macOS 14.0, *))
        p->displayLink = 1;
    p->wake = dispatch_semaphore_create(0);
    p->inFlight = dispatch_group_create();
    for (int i = 0; i < SLOT_COUNT; ++i) {
        atomic_init(&p->slots[i].state, SLOT_FREE);
        atomic_init(&p->slots[i].sequence, 0);
        p->slots[i].gammaRevision = UINT_MAX;
    }
    @autoreleasepool {
        p->device = sharedGPU ? (__bridge id<MTLDevice>)MacMetalGPU_Device(sharedGPU) : MTLCreateSystemDefaultDevice();
        if (usesGL)
            p->queue = [p->device newCommandQueue];
        else
            p->gpu = sharedGPU ? MacMetalGPU_Retain(sharedGPU) : MacMetalGPU_Create((__bridge void *)p->device);
        NSError *error = nil;
        MTLCompileOptions *options = [MTLCompileOptions new];
        options.fastMathEnabled = NO;
        id<MTLLibrary> library = [p->device newLibraryWithSource:@(presentSource) options:options error:&error];
        if ((!p->queue && !p->gpu) || !library) {
            fprintf(stderr, "CoD2 Silicon Metal shader setup: %s\n", error.localizedDescription.UTF8String);
            MacMetal_Destroy(p);
            return NULL;
        }
        MTLRenderPipelineDescriptor *pipeline = [MTLRenderPipelineDescriptor new];
        pipeline.vertexFunction = [library newFunctionWithName:@"present_vertex"];
        pipeline.fragmentFunction = [library newFunctionWithName:@"present_fragment"];
        MTLPixelFormat format = edr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
        pipeline.colorAttachments[0].pixelFormat = format;
        if (usesGL) {
            p->pipeline = [p->device newRenderPipelineStateWithDescriptor:pipeline error:&error];
        } else if (@available(macOS 26.0, *)) {
            MTL4RenderPipelineDescriptor *nativePipeline = [MTL4RenderPipelineDescriptor new];
            MTL4LibraryFunctionDescriptor *vertex = [MTL4LibraryFunctionDescriptor new];
            vertex.library = library; vertex.name = @"present_vertex";
            MTL4LibraryFunctionDescriptor *fragment = [MTL4LibraryFunctionDescriptor new];
            fragment.library = library; fragment.name = @"present_fragment";
            nativePipeline.vertexFunctionDescriptor = vertex;
            nativePipeline.fragmentFunctionDescriptor = fragment;
            nativePipeline.colorAttachments[0].pixelFormat = format;
            id<MTL4Compiler> compiler = (__bridge id<MTL4Compiler>)MacMetalGPU_Compiler(p->gpu);
            p->pipeline = [compiler newRenderPipelineStateWithDescriptor:nativePipeline compilerTaskOptions:nil error:&error];
        }
        if ((!p->queue && !p->gpu) || !p->pipeline) {
            fprintf(stderr, "CoD2 Silicon Metal setup: %s\n", error.localizedDescription.UTF8String);
            MacMetal_Destroy(p);
            return NULL;
        }
        GLint binding = 0;
        if (usesGL)
            glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &binding);
        MTLTextureDescriptor *texture = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                       width:width height:height mipmapped:NO];
        texture.storageMode = MTLStorageModeShared;
        texture.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        for (int i = 0; i < SLOT_COUNT; ++i) {
            MacMetalSlot *slot = &p->slots[i];
            size_t row = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, (size_t)width * 4);
            NSDictionary *properties = @{ (id)kIOSurfaceWidth: @(width), (id)kIOSurfaceHeight: @(height),
                (id)kIOSurfaceBytesPerElement: @4, (id)kIOSurfaceBytesPerRow: @(row),
                (id)kIOSurfaceAllocSize: @(IOSurfaceAlignProperty(kIOSurfaceAllocSize, row * height)),
                (id)kIOSurfacePixelFormat: @((uint32_t)0x42475241) };
            slot->surface = IOSurfaceCreate((__bridge CFDictionaryRef)properties);
            if (slot->surface)
                slot->texture = [p->device newTextureWithDescriptor:texture iosurface:slot->surface plane:0];
            CGLError result = kCGLNoError;
            if (usesGL) {
                glGenTextures(1, &slot->glTexture);
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, slot->glTexture);
                result = slot->surface ? CGLTexImageIOSurface2D(CGLGetCurrentContext(), GL_TEXTURE_RECTANGLE_ARB,
                    GL_RGBA8, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, slot->surface, 0) : kCGLBadValue;
            }
            if (!slot->texture || result != kCGLNoError) {
                if (usesGL)
                    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, binding);
                fprintf(stderr, "CoD2 Silicon IOSurface setup: %s\n", CGLErrorString(result));
                MacMetal_Destroy(p);
                return NULL;
            }
        }
        if (usesGL)
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, binding);
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_COCOA) {
            MacMetal_Destroy(p);
            return NULL;
        }
        NSWindow *nativeWindow = info.info.cocoa.window;
        p->view = [[Cod2MetalView alloc] initWithFrame:nativeWindow.contentView.bounds];
        p->layer = [CAMetalLayer layer];
        p->layer.device = p->device;
        p->layer.pixelFormat = format;
        p->layer.maximumDrawableCount = 2;
        p->layer.allowsNextDrawableTimeout = YES;
        p->layer.presentsWithTransaction = NO;
        /* Native display-link delivery needs layer synchronization for
         * compositor pacing; retain the existing GL interop policy. */
        p->layer.displaySyncEnabled = usesGL ? NO : YES;
        p->layer.framebufferOnly = YES;
        p->layer.wantsExtendedDynamicRangeContent = edr != 0;
        if (edr) {
            CGColorSpaceRef color = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
            p->layer.colorspace = color;
            CGColorSpaceRelease(color);
        } else {
            p->layer.colorspace = nil;
        }
        p->view.wantsLayer = YES;
        p->view.layer.backgroundColor = NSColor.blackColor.CGColor;
        [p->view.layer addSublayer:p->layer];
        [nativeWindow.contentView addSubview:p->view positioned:NSWindowAbove relativeTo:nil];
        MacMetal_UpdateGeometry(p);
        if (getenv("COD2_METAL_GPU_TRACE")) fprintf(stderr,
            "Metal backing: scene=%ix%i surface=%zux%zu pitch=%zu drawable=%.0fx%.0f count=%lu view=%.0fx%.0f scale=%.1f layer=%.0fx%.0f compositor_scale=%.3fx%.3f\n",
            width, height, IOSurfaceGetWidth(p->slots[0].surface), IOSurfaceGetHeight(p->slots[0].surface), IOSurfaceGetBytesPerRow(p->slots[0].surface), p->layer.drawableSize.width, p->layer.drawableSize.height, (unsigned long)p->layer.maximumDrawableCount,
            nativeWindow.contentView.bounds.size.width, nativeWindow.contentView.bounds.size.height, nativeWindow.backingScaleFactor,
            p->layer.frame.size.width, p->layer.frame.size.height,
            p->layer.frame.size.width * nativeWindow.backingScaleFactor / width, p->layer.frame.size.height * nativeWindow.backingScaleFactor / height);
        if (getenv("COD2_FRAME_CSV"))
            p->observe = dlsym(RTLD_DEFAULT, "MacFrameProbe_Metal");
        if (pthread_create(&p->thread, NULL, PresentThread, p) != 0) {
            MacMetal_Destroy(p);
            return NULL;
        }
        p->threadStarted = 1;
        fprintf(stderr, "CoD2 Silicon Metal present: %dx%d BGRA8 IOSurface, %s output, %s, 2 drawables\n",
                width, height, edr ? "EDR" : "classic", NSClassFromString(@"CAMetalDisplayLink") ? "display link" : "worker timeout");
    }
    return p;
}

static int ClaimRenderSlot(MacMetalPresenter *p)
{
    int newest = LatestReady(p);
    p->current = -1;
    for (int i = 0; i < SLOT_COUNT; ++i) {
        int expected = SLOT_FREE;
        if (atomic_compare_exchange_strong(&p->slots[i].state, &expected, SLOT_RENDERING)) {
            p->current = i;
            break;
        }
        /* Keep the newest completed frame when the window is occluded. */
        if (i != newest && expected == SLOT_READY &&
            atomic_compare_exchange_strong(&p->slots[i].state, &expected, SLOT_RENDERING)) {
            p->current = i;
            break;
        }
    }
    return p->current;
}

unsigned int MacMetal_BeginFrame(MacMetalPresenter *p)
{
    for (int i = 0; i < SLOT_COUNT; ++i) {
        MacMetalSlot *slot = &p->slots[i];
        if (atomic_load(&slot->state) == SLOT_GL_PENDING) {
            GLenum result = glClientWaitSync(slot->fence, 0, 0);
            if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
                glDeleteSync(slot->fence);
                slot->fence = NULL;
                atomic_store_explicit(&slot->state, SLOT_READY, memory_order_release);
            } else if (result == GL_WAIT_FAILED) {
                /* Do not hand a texture with unknown completion to Metal. */
                fprintf(stderr, "CoD2 Silicon GL/Metal fence failed; dropping frame\n");
                atomic_store(&slot->state, SLOT_RENDERING);
            }
        }
    }
    int index = ClaimRenderSlot(p);
    return index >= 0 ? p->slots[index].glTexture : 0;
}

static void SetSlotGamma(MacMetalSlot *slot, const unsigned short *ramp, unsigned int revision, int identity)
{
    slot->identity = identity;
    if (!identity && slot->gammaRevision != revision) {
        for (int i = 0; i < 768; ++i)
            slot->ramp[i] = ramp[i] / 65535.0f;
        slot->gammaRevision = revision;
    }
}

void MacMetal_EndFrame(MacMetalPresenter *p, const unsigned short *ramp, unsigned int revision, int identity)
{
    if (p->current >= 0) {
        MacMetalSlot *slot = &p->slots[p->current];
        SetSlotGamma(slot, ramp, revision, identity);
        atomic_store(&slot->sequence, ++p->sequence);
        slot->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (slot->fence)
            atomic_store(&slot->state, SLOT_GL_PENDING);
        /* A failed fence quarantines the slot until shutdown, never exposes
         * incomplete GL writes or reuses storage still being written. */
    }
    /* Apple's OpenGL/Metal interop sample requires an explicit GL flush. A
     * zero-timeout fence then proves GPU completion before publishing READY. */
    glFlush();
    if (!p->displayLink && !atomic_exchange(&p->wakePending, true))
        dispatch_semaphore_signal(p->wake);
}

void MacMetal_Destroy(MacMetalPresenter *p)
{
    if (!p)
        return;
    atomic_store(&p->stopping, true);
    dispatch_semaphore_signal(p->wake);
    if (p->threadStarted)
        pthread_join(p->thread, NULL);
    dispatch_group_wait(p->inFlight, DISPATCH_TIME_FOREVER);
    /* Lifecycle barrier only, never part of the 3 ms loop. */
    if (p->usesGL)
        glFinish();
    [p->view removeFromSuperview];
    /* CGLClearDrawable leaves the default FBO undefined. Reattach Cocoa's
     * existing GL view so a failed presenter rebuild can fall back to GL. */
    if (p->usesGL)
        [NSOpenGLContext.currentContext update];
    p->view = nil;
    p->admission = nil;
    p->layer = nil;
    for (int i = 0; i < SLOT_COUNT; ++i) {
        MacMetalSlot *slot = &p->slots[i];
        if (p->usesGL) {
            if (slot->fence)
                glDeleteSync(slot->fence);
            glDeleteTextures(1, &slot->glTexture);
        }
        slot->texture = nil;
        if (slot->surface)
            CFRelease(slot->surface);
    }
    MacMetalGPU_Destroy(p->gpu);
    p->gpu = NULL;
    p->pipeline = nil;
    p->queue = nil;
    p->device = nil;
    p->wake = nil;
    p->inFlight = nil;
    free(p);
}

MacMetalGPU *MacMetal_NativeGPU(MacMetalPresenter *p) { return p ? p->gpu : NULL; }
void *MacMetal_Device(MacMetalPresenter *p) { return p ? (__bridge void *)p->device : NULL; }
void *MacMetal_Layer(MacMetalPresenter *p) { return p ? (__bridge void *)p->layer : NULL; }

MacMetalPresenter *MacMetal_Create(SDL_Window *window, int w, int h, int edr) { return CreatePresenter(window, w, h, edr, 1, NULL); }
MacMetalPresenter *MacMetal_CreateNative(SDL_Window *window, int w, int h, int edr) { return CreatePresenter(window, w, h, edr, 0, NULL); }
MacMetalPresenter *MacMetal_CreateNativeWithGPU(SDL_Window *window, int w, int h, int edr, MacMetalGPU *gpu)
{ return gpu ? CreatePresenter(window, w, h, edr, 0, gpu) : NULL; }
void *MacMetal_BeginNativeFrame(MacMetalPresenter *p, int *token)
{
    if (!p || p->usesGL || !token)
        return NULL;
    *token = ClaimRenderSlot(p);
    return *token >= 0 ? (__bridge void *)p->slots[*token].texture : NULL;
}

int MacMetal_SealNativeFrame(MacMetalPresenter *p, int token, const unsigned short *ramp, unsigned int revision, int identity)
{
    if (!p || p->usesGL || token < 0 || token >= SLOT_COUNT)
        return 0;
    MacMetalSlot *slot = &p->slots[token];
    if (atomic_load(&slot->state) != SLOT_RENDERING)
        return 0;
    SetSlotGamma(slot, ramp, revision, identity);
    atomic_store(&slot->sequence, ++p->sequence);
    dispatch_group_enter(p->inFlight);
    atomic_store(&slot->state, SLOT_NATIVE_PENDING);
    return 1;
}

void MacMetal_CompleteNativeFrame(MacMetalPresenter *p, int token, int success)
{
    if (!p || p->usesGL || token < 0 || token >= SLOT_COUNT)
        return;
    int expected = SLOT_NATIVE_PENDING;
    if (!atomic_compare_exchange_strong(&p->slots[token].state, &expected, success ? SLOT_READY : SLOT_FREE))
        return;
    if (!p->displayLink && !atomic_exchange(&p->wakePending, true))
        dispatch_semaphore_signal(p->wake);
    dispatch_group_leave(p->inFlight);
}

void MacMetal_EndNativeFrame(MacMetalPresenter *p, int token, void *opaque, const unsigned short *ramp, unsigned int revision, int identity)
{
    if (!MacMetal_SealNativeFrame(p, token, ramp, revision, identity))
        return;
    if (!opaque) {
        MacMetal_CompleteNativeFrame(p, token, 0);
        return;
    }
    id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)opaque;
    [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
        MacMetal_CompleteNativeFrame(p, token, completed.status == MTLCommandBufferStatusCompleted);
    }];
}
#endif
