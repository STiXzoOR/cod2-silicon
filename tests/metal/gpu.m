#include "Mac/Metal/metal_core.h"
#import <Metal/Metal.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
static atomic_int completions, producerReceipts;
static atomic_int released;
@interface RetainedSubmissionObject : NSObject
@property(nonatomic, assign) MacMetalGPU *gpu;
@end
@implementation RetainedSubmissionObject
- (void)dealloc
{
    /* Destruction must not run under the transport mutex. A resource or a
     * provider helper can inspect its transport when its last retain retires. */
    assert(MacMetalGPU_Submitted(self.gpu) > 0);
    atomic_fetch_add(&released, 1);
}
@end
void MacFrameProbe_Metal(int kind, double stamp, double target, uint64_t sequence)
{
    assert(kind == 5 && stamp > 0 && target == 0 && sequence > 0);
    atomic_fetch_add(&producerReceipts, 1);
}
static void complete(void *context, uint64_t value, int success)
{
    assert(context == &completions && value && success);
    atomic_fetch_add(&completions, 1);
}
int main(void)
{
    @autoreleasepool {
        setenv("COD2_FRAME_CSV", "synthetic-observer-fixture", 1);
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (@available(macOS 26.0, *)) {
            if (!device || ![device supportsFamily:MTLGPUFamilyMetal4]) {
                puts("SKIP: Metal 4 device unavailable"); return 77;
            }
        } else {
            puts("SKIP: Metal 4 requires macOS 26"); return 77;
        }
        assert(!MacMetalGPU_Create(NULL));
        MacMetalGPU *gpu = MacMetalGPU_Create((__bridge void *)device);
        assert(gpu && "Metal 4 transport initialization failed on supported hardware");
        assert(MacMetalGPU_Device(gpu) == (__bridge void *)device);
        MacMetalSubmission *a = MacMetalGPU_Begin(gpu, 0), *b = MacMetalGPU_Begin(gpu, 0);
        assert(a && b && a != b && !MacMetalGPU_Begin(gpu, 0));
        MacMetalGPU_Cancel(b);
        assert(MacMetalGPU_Begin(gpu, 0) == b);
        MacMetalGPU_Cancel(b);
        if (@available(macOS 26.0, *)) {
            MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:19 height:13 mipmapped:NO];
            descriptor.storageMode = MTLStorageModeShared;
            descriptor.usage = MTLTextureUsageRenderTarget;
            id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
            id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(a);
            MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
            pass.colorAttachments[0].texture = texture;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(29.0 / 255, 73.0 / 255, 151.0 / 255, 1);
            pass.renderTargetWidth = 19; pass.renderTargetHeight = 13;
            id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
            assert(encoder); [encoder endEncoding];
            MacMetalGPU_Keep(a, (__bridge void *)texture, 1);
            uint64_t value = MacMetalGPU_Commit(a, NULL, complete, &completions);
            assert(value == 1 && MacMetalGPU_Drain(gpu));
            assert(MacMetalGPU_Completed(gpu) >= 1 && atomic_load(&completions) == 1);
            unsigned char pixels[19 * 13 * 4];
            [texture getBytes:pixels bytesPerRow:19 * 4 fromRegion:MTLRegionMake2D(0, 0, 19, 13) mipmapLevel:0];
            for (unsigned int i = 0; i < 19 * 13; ++i)
                assert(pixels[i * 4] == 151 && pixels[i * 4 + 1] == 73 && pixels[i * 4 + 2] == 29 && pixels[i * 4 + 3] == 255);
            a = MacMetalGPU_Begin(gpu, 0); assert(a);
            assert(MacMetalGPU_Commit(a, NULL, complete, &completions) == 2);
            assert(MacMetalGPU_Drain(gpu) && atomic_load(&completions) == 2);
            assert(atomic_load(&producerReceipts) == 2);
            a = MacMetalGPU_Begin(gpu, 0); assert(a);
            RetainedSubmissionObject *object = [RetainedSubmissionObject new]; object.gpu = gpu;
            MacMetalGPU_Keep(a, (__bridge void *)object, 0); object = nil;
            assert(atomic_load(&released) == 0);
            id<MTLSharedEvent> gate = [device newSharedEvent];
            assert(gate);
            id<MTL4CommandQueue> queue = (__bridge id<MTL4CommandQueue>)MacMetalGPU_Queue(gpu);
            [queue waitForEvent:gate value:1];
            assert(MacMetalGPU_Commit(a, NULL, NULL, NULL) == 3);
            assert(MacMetalGPU_Completed(gpu) == 2 && atomic_load(&released) == 0);
            gate.signaledValue = 1;
            assert(MacMetalGPU_Drain(gpu) && atomic_load(&released) == 1);
            a = MacMetalGPU_Begin(gpu, 0); assert(a);
            object = [RetainedSubmissionObject new]; object.gpu = gpu;
            MacMetalGPU_Keep(a, (__bridge void *)object, 0); object = nil;
            assert(atomic_load(&released) == 1);
            MacMetalGPU_Cancel(a);
            assert(atomic_load(&released) == 2);
        }
        MacMetalGPU_Destroy(gpu);
    }
    puts("Metal GPU: shared ownership, allocator retirement, timeline and exact BGRA clear pass");
}
