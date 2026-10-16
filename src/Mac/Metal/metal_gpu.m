#include "metal_core.h"
#include "metal_encode_pool.h"
#include "metal_frame_stats.h"
#import <Metal/Metal.h>
#include <pthread.h>
#include <dlfcn.h>
#include <mach/mach_time.h>

struct MacMetalSubmission {
    MacMetalGPU *gpu;
    id allocator;
    id command;
    id residency;
    NSMutableArray *retained;
    uint64_t value;
    int encoding;
    unsigned int role;
    NSMutableArray *constantPages, *argumentTables;
    size_t constantCursor, tableCursor;
};
struct MacMetalGPU {
    id<MTLDevice> device;
    id queue;
    id<MTLSharedEvent> event;
    id compiler;
    MacMetalSubmission slots[4];
    pthread_mutex_t mutex;
    uint64_t submitted;
    dispatch_group_t inFlight;
    int failed;
    int producerBackpressure;
    unsigned int refs;
    void (*observe)(int, double, double, uint64_t);
    double secondsPerTick;
    FILE *admissionTrace;
    FILE *gpuTrace;
    uint64_t begunAt[4], committedAt[4];
};

MacMetalGPU *MacMetalGPU_Create(void *device)
{
    if (@available(macOS 26.0, *)) {
        id<MTLDevice> native = (__bridge id<MTLDevice>)device;
        if (!native || ![native supportsFamily:MTLGPUFamilyMetal4])
            return NULL;
        MacMetalGPU *gpu = calloc(1, sizeof(*gpu));
        if (!gpu)
            return NULL;
        pthread_mutex_init(&gpu->mutex, NULL);
        gpu->refs = 1;
        const char *trace = getenv("COD2_METAL_ADMISSION_TRACE");
        if (trace && *trace) {
            gpu->admissionTrace = fopen(trace, "w");
            if (gpu->admissionTrace) fprintf(gpu->admissionTrace, "event,ticks,role,slot,value,completed,age_ticks\n");
        }
        trace = getenv("COD2_METAL_GPU_TRACE");
        if (trace && *trace) {
            gpu->gpuTrace = fopen(trace, "w");
            if (gpu->gpuTrace) fprintf(gpu->gpuTrace, "role,value,commit_cpu,gpu_start,gpu_end,wait_enqueue_ms\n");
        }
        if (getenv("COD2_FRAME_CSV")) {
            gpu->observe = dlsym(RTLD_DEFAULT, "MacFrameProbe_Metal");
        }
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        gpu->secondsPerTick = (double)timebase.numer / timebase.denom / 1e9;
        gpu->device = native;
        gpu->event = [native newSharedEvent];
        /* Metal owns its documented default serial feedback queue. */
        gpu->queue = [native newMTL4CommandQueue];
        gpu->inFlight = dispatch_group_create();
        gpu->compiler = [native newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:nil];
        int ready = gpu->queue && gpu->event && gpu->compiler;
        for (unsigned int i = 0; i < 4; ++i) {
            MacMetalSubmission *slot = &gpu->slots[i];
            slot->gpu = gpu;
            slot->role = i / 2;
            slot->allocator = [native newCommandAllocator];
            slot->command = [native newCommandBuffer];
            slot->retained = [NSMutableArray new];
            slot->constantPages = [NSMutableArray new];
            slot->argumentTables = [NSMutableArray new];
            MTLResidencySetDescriptor *descriptor = [MTLResidencySetDescriptor new];
            descriptor.initialCapacity = 64;
            slot->residency = [native newResidencySetWithDescriptor:descriptor error:nil];
            ready &= slot->allocator && slot->command && slot->residency;
            if (slot->residency)
                [gpu->queue addResidencySet:slot->residency];
        }
        if (!ready) {
            MacMetalGPU_Destroy(gpu);
            return NULL;
        }
        return gpu;
    }
    return NULL;
}
MacMetalGPU *MacMetalGPU_Retain(MacMetalGPU *gpu)
{
    if (!gpu) return NULL;
    pthread_mutex_lock(&gpu->mutex);
    ++gpu->refs;
    pthread_mutex_unlock(&gpu->mutex);
    return gpu;
}
void *MacMetalGPU_Device(MacMetalGPU *gpu) { return gpu ? (__bridge void *)gpu->device : NULL; }
void *MacMetalGPU_Queue(MacMetalGPU *gpu) { return gpu ? (__bridge void *)gpu->queue : NULL; }
void *MacMetalGPU_Event(MacMetalGPU *gpu) { return gpu ? (__bridge void *)gpu->event : NULL; }
void *MacMetalGPU_Compiler(MacMetalGPU *gpu) { return gpu ? (__bridge void *)gpu->compiler : NULL; }
uint64_t MacMetalGPU_Completed(MacMetalGPU *gpu) { return gpu ? gpu->event.signaledValue : 0; }
uint64_t MacMetalGPU_Submitted(MacMetalGPU *gpu)
{
    if (!gpu) return 0;
    pthread_mutex_lock(&gpu->mutex);
    uint64_t value = gpu->submitted;
    pthread_mutex_unlock(&gpu->mutex);
    return value;
}
MacMetalSubmission *MacMetalGPU_Begin(MacMetalGPU *gpu, unsigned int role)
{
    if (@available(macOS 26.0, *)) {
        if (!gpu || role > 1) return NULL;
        pthread_mutex_lock(&gpu->mutex);
        if (!role) gpu->producerBackpressure = 0;
        if (gpu->failed) { pthread_mutex_unlock(&gpu->mutex); return NULL; }
        MacMetalSubmission *result = NULL;
        for (unsigned int i = role * 2; i < role * 2 + 2; ++i) {
            MacMetalSubmission *slot = &gpu->slots[i];
            /* Feedback proves completion, including failed GPU work, before reuse. */
            if (!slot->encoding && !slot->value) {
                slot->encoding = 1;
                slot->constantCursor = slot->tableCursor = 0;
                [slot->allocator reset];
                [slot->residency removeAllAllocations];
                [slot->residency commit];
                [slot->command beginCommandBufferWithAllocator:slot->allocator];
                result = slot;
                if (gpu->admissionTrace) gpu->begunAt[i] = mach_absolute_time();
                break;
            }
        }
        if (!result && gpu->admissionTrace) for (unsigned i = role * 2; i < role * 2 + 2; ++i) {
            uint64_t now = mach_absolute_time();
            fprintf(gpu->admissionTrace, "busy,%llu,%u,%u,%llu,%llu,%llu\n", (unsigned long long)now, role, i, (unsigned long long)gpu->slots[i].value, (unsigned long long)gpu->event.signaledValue, (unsigned long long)(now - (gpu->slots[i].value ? gpu->committedAt[i] : gpu->begunAt[i])));
        }
        if (!role) gpu->producerBackpressure = !result;
        pthread_mutex_unlock(&gpu->mutex);
        return result;
    }
    return NULL;
}
int MacMetalGPU_Backpressure(MacMetalGPU *gpu)
{
    if (!gpu) return 0;
    pthread_mutex_lock(&gpu->mutex);
    int busy = gpu->producerBackpressure;
    pthread_mutex_unlock(&gpu->mutex);
    return busy;
}
void *MacMetalGPU_Command(MacMetalSubmission *submission) { return submission ? (__bridge void *)submission->command : NULL; }
uint64_t MacMetalGPU_CopyConstants(MacMetalSubmission *slot, const void *bytes, size_t length)
{
    if (@available(macOS 26.0, *)) {
        if (!slot || !slot->encoding || !bytes || !length) return 0;
        enum { PageBytes = 2 * 1024 * 1024, PageCount = 4, Alignment = 256 };
        size_t cursor = slot->constantCursor, offset = cursor % PageBytes;
        if (length <= PageBytes && length > PageBytes - offset) { cursor += PageBytes - offset; offset = 0; }
        size_t page = cursor / PageBytes;
        if (length <= PageBytes && page < PageCount) {
            if (page == slot->constantPages.count) {
                id<MTLBuffer> fresh = [slot->gpu->device newBufferWithLength:PageBytes options:MTLResourceStorageModeShared];
                if (!fresh) return 0;
                [slot->constantPages addObject:fresh];
            }
            id<MTLBuffer> buffer = slot->constantPages[page];
            if (!offset) [slot->residency addAllocation:buffer];
            memcpy((char *)buffer.contents + offset, bytes, length);
            slot->constantCursor = cursor + ((length + Alignment - 1) & ~(size_t)(Alignment - 1));
            return buffer.gpuAddress + offset;
        }
        id<MTLBuffer> buffer = [slot->gpu->device newBufferWithBytes:bytes length:length options:MTLResourceStorageModeShared];
        if (buffer) MacMetalGPU_Keep(slot, (__bridge void *)buffer, 1);
        return buffer.gpuAddress;
    }
    return 0;
}
void *MacMetalGPU_ArgumentTable(MacMetalSubmission *slot)
{
    if (@available(macOS 26.0, *)) {
        if (!slot || !slot->encoding) return NULL;
        size_t index = slot->tableCursor++;
        if (index < slot->argumentTables.count) {
            id<MTL4ArgumentTable> table = slot->argumentTables[index];
            /* Unused shader slots must not retain retired resource identities
             * from the previous submission. All buffer slots are overwritten
             * by the draw before binding the table. */
            for (unsigned i = 0; i < MAC_METAL_TEXTURE_COUNT; ++i) {
                [table setTexture:(MTLResourceID){0} atIndex:i];
                [table setSamplerState:(MTLResourceID){0} atIndex:i];
            }
            return (__bridge void *)table;
        }
        MTL4ArgumentTableDescriptor *descriptor = [MTL4ArgumentTableDescriptor new];
        descriptor.maxBufferBindCount = MAC_METAL_BUFFER_BINDINGS;
        descriptor.maxTextureBindCount = MAC_METAL_TEXTURE_COUNT;
        descriptor.maxSamplerStateBindCount = MAC_METAL_TEXTURE_COUNT;
        descriptor.initializeBindings = YES;
        id<MTL4ArgumentTable> table = [slot->gpu->device newArgumentTableWithDescriptor:descriptor error:nil];
        if (!table) return NULL;
        if (index < 512) [slot->argumentTables addObject:table];
        else MacMetalGPU_Keep(slot, (__bridge void *)table, 0);
        return (__bridge void *)table;
    }
    return NULL;
}
void MacMetalGPU_Keep(MacMetalSubmission *slot, void *object, int allocation)
{
    if (@available(macOS 26.0, *)) {
        if (!slot || !object) return;
        id native = (__bridge id)object;
        [slot->retained addObject:native];
        if (allocation)
            [slot->residency addAllocation:native];
    }
}
uint64_t MacMetalGPU_Commit(MacMetalSubmission *slot, void *opaqueDrawable, MacMetalCompletion completion, void *context)
{
    if (@available(macOS 26.0, *)) {
        if (!slot || !slot->encoding) return 0;
        MacMetalGPU *gpu = slot->gpu;
        id<MTLDrawable> drawable = (__bridge id<MTLDrawable>)opaqueDrawable;
        [slot->command endCommandBuffer];
        [slot->residency commit];
        pthread_mutex_lock(&gpu->mutex);
        uint64_t value = ++gpu->submitted;
        double commitTime = gpu->gpuTrace ? mach_absolute_time() * gpu->secondsPerTick : 0;
        __block double waitEnqueueMS = 0;
        if (gpu->admissionTrace) {
            unsigned i = (unsigned)(slot - gpu->slots);
            gpu->committedAt[i] = mach_absolute_time();
            fprintf(gpu->admissionTrace, "commit,%llu,%u,%u,%llu,%llu,%llu\n", (unsigned long long)gpu->committedAt[i], slot->role, i, (unsigned long long)value, (unsigned long long)gpu->event.signaledValue, (unsigned long long)(gpu->committedAt[i] - gpu->begunAt[i]));
        }
        slot->value = value;
        slot->encoding = 0;
        dispatch_group_enter(gpu->inFlight);
        MTL4CommitOptions *options = [MTL4CommitOptions new];
        [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback) {
            int success = feedback.error == nil;
            /* Producer GPU feedback exists even when a locked/occluded layer
             * supplies no drawables. It is distinct from physical display. */
            if (!slot->role && gpu->observe)
                gpu->observe(success ? 5 : 6, mach_absolute_time() * gpu->secondsPerTick, 0, value);
            if (!success) fprintf(stderr, "CoD2 Silicon Metal submission: %s\n", feedback.error.localizedDescription.UTF8String);
            if (completion)
                completion(context, value, success);
            pthread_mutex_lock(&gpu->mutex);
            gpu->failed |= !success;
            if (gpu->gpuTrace) fprintf(gpu->gpuTrace, "%u,%llu,%.9f,%.9f,%.9f,%.6f\n", slot->role, (unsigned long long)value, commitTime, feedback.GPUStartTime, feedback.GPUEndTime, waitEnqueueMS);
            if (gpu->admissionTrace) {
                unsigned i = (unsigned)(slot - gpu->slots); uint64_t now = mach_absolute_time();
                fprintf(gpu->admissionTrace, "feedback,%llu,%u,%u,%llu,%llu,%llu\n", (unsigned long long)now, slot->role, i, (unsigned long long)value, (unsigned long long)gpu->event.signaledValue, (unsigned long long)(now - gpu->committedAt[i]));
            }
            pthread_mutex_unlock(&gpu->mutex);
            /* Keep this slot unavailable until destruction finishes, without
             * blocking Begin/Submitted behind arbitrary Objective-C deallocs.
             * No producer can mutate retained while value is still nonzero. */
            [slot->retained removeAllObjects];
            pthread_mutex_lock(&gpu->mutex);
            slot->value = 0;
            pthread_mutex_unlock(&gpu->mutex);
            dispatch_group_leave(gpu->inFlight);
        }];
        if (drawable) {
            uint64_t start = gpu->gpuTrace ? mach_absolute_time() : 0;
            [gpu->queue waitForDrawable:drawable];
            if (start) waitEnqueueMS = (mach_absolute_time() - start) * gpu->secondsPerTick * 1000;
        }
        id commands[] = { slot->command };
        [gpu->queue commit:commands count:1 options:options];
        [gpu->queue signalEvent:gpu->event value:value];
        if (drawable) {
            [gpu->queue signalDrawable:drawable];
            [drawable present];
        }
        pthread_mutex_unlock(&gpu->mutex);
        return value;
    }
    return 0;
}
void MacMetalGPU_Cancel(MacMetalSubmission *slot)
{
    if (@available(macOS 26.0, *)) {
        if (!slot || !slot->encoding) return;
        [slot->command endCommandBuffer];
        [slot->retained removeAllObjects];
        pthread_mutex_lock(&slot->gpu->mutex);
        slot->encoding = 0;
        pthread_mutex_unlock(&slot->gpu->mutex);
    }
}
int MacMetalGPU_Drain(MacMetalGPU *gpu)
{
    if (!gpu) return 1;
    if (dispatch_group_wait(gpu->inFlight, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC))) return 0;
    pthread_mutex_lock(&gpu->mutex);
    int failed = gpu->failed;
    uint64_t value = gpu->submitted;
    pthread_mutex_unlock(&gpu->mutex);
    return !failed && [gpu->event waitUntilSignaledValue:value timeoutMS:10000];
}
void MacMetalGPU_Destroy(MacMetalGPU *gpu)
{
    if (@available(macOS 26.0, *)) {
        if (!gpu) return;
        pthread_mutex_lock(&gpu->mutex);
        unsigned int refs = --gpu->refs;
        pthread_mutex_unlock(&gpu->mutex);
        if (refs) return;
        /* Destruction cannot free feedback captures before completion. */
        dispatch_group_wait(gpu->inFlight, DISPATCH_TIME_FOREVER);
        for (unsigned int i = 0; i < 4; ++i) {
            MacMetalSubmission *slot = &gpu->slots[i];
            if (slot->encoding) MacMetalGPU_Cancel(slot);
            if (slot->residency) [gpu->queue removeResidencySet:slot->residency];
            slot->residency = nil;
            slot->retained = nil;
            slot->constantPages = nil;
            slot->argumentTables = nil;
            slot->command = nil;
            slot->allocator = nil;
        }
        gpu->compiler = nil;
        gpu->queue = nil;
        gpu->event = nil;
        gpu->inFlight = nil;
        gpu->device = nil;
        if (gpu->gpuTrace) fclose(gpu->gpuTrace);
        if (gpu->admissionTrace) fclose(gpu->admissionTrace);
        pthread_mutex_destroy(&gpu->mutex);
        free(gpu);
    }
}
