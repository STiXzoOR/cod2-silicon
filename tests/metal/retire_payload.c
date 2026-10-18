/* CPU-only characterization of the real backend retirement fallback.
 * Compile with the production Metal source directory on the include path.
 * No Metal implementation, framework, or device is linked or created. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "metal_backend.h"

static unsigned int allocationAttempts, allocationFailures, failNextAllocation;

static void *RetirementNodeMalloc(size_t size)
{
    ++allocationAttempts;
    if (failNextAllocation) {
        --failNextAllocation;
        ++allocationFailures;
        return NULL;
    }
    return malloc(size);
}

/* metal_core.m has one malloc: the retirement node allocation. Keep queue
 * creation and payload allocation real, and inject no backend behavior. */
#define malloc RetirementNodeMalloc
#include "metal_core.m"
#undef malloc

/* Including the source exposes its static RetirePayload and retirement queue
 * to this translation unit without adding an export or hook to production.
 * Suppress only the unrelated startup registration attribute in this fixture;
 * the linker can then discard its dispatch graph and provider dependencies. */
#define constructor unused
#include "metal_backend.m"
#undef constructor

/* This is a CPU model of pending work, not a Metal device or command queue. */
struct MacMetalGPU { int pending; };
static MacMetalGPU fakeGPU;
static MacMetalGPU *availableGPU;
static unsigned int drainCalls;
static int drainResult;

MacMetalGPU *MacMetal_GPU(void) { return availableGPU; }

int MacMetalGPU_Drain(MacMetalGPU *gpu)
{
    assert(gpu == &fakeGPU);
    ++drainCalls;
    if (drainResult) gpu->pending = 0;
    return drainResult;
}

typedef struct { unsigned int releases; } ReleaseCount;
typedef struct {
    uint32_t marker;
    ReleaseCount *count;
} Payload;
enum { PAYLOAD_LIVE = 0x72657469 };

static Payload *NewPayload(ReleaseCount *count)
{
    Payload *payload = malloc(sizeof(*payload));
    assert(payload);
    *payload = (Payload){ PAYLOAD_LIVE, count };
    return payload;
}

static void ReleasePayload(void *context)
{
    Payload *payload = context;
    assert(!fakeGPU.pending);
    assert(payload->marker == PAYLOAD_LIVE);
    assert(payload->count->releases == 0);
    ++payload->count->releases;
    payload->marker = 0;
    free(payload);
}

static void BeginCase(int gpuAvailable, int pending, int drains)
{
    assert(!retirement);
    allocationAttempts = allocationFailures = failNextAllocation = drainCalls = 0;
    fakeGPU.pending = pending;
    availableGPU = gpuAvailable ? &fakeGPU : NULL;
    drainResult = drains;
    retirement = MacMetalRetirement_Create();
    assert(retirement);
}

static void DestroyQueue(void)
{
    MacMetalRetirement_Destroy(retirement);
    retirement = NULL;
}

static void SuccessfulEnqueueWaitsForCompletion(void)
{
    BeginCase(1, 1, 1);
    ReleaseCount count = { 0 };
    Payload *payload = NewPayload(&count);
    RetirePayload(7, ReleasePayload, payload);
    assert(allocationAttempts == 1 && allocationFailures == 0);
    assert(drainCalls == 0 && fakeGPU.pending && count.releases == 0);
    MacMetalRetirement_Collect(retirement, 6);
    assert(count.releases == 0 && payload->marker == PAYLOAD_LIVE);
    fakeGPU.pending = 0;
    MacMetalRetirement_Collect(retirement, 7);
    assert(count.releases == 1);
    MacMetalRetirement_Collect(retirement, UINT64_MAX);
    DestroyQueue();
    assert(count.releases == 1);
    puts("PASS: enqueue waits for value 7, then releases exactly once");
}

static void FailedAllocationSuccessfulDrainReleasesOnce(void)
{
    BeginCase(1, 1, 1);
    ReleaseCount count = { 0 };
    Payload *payload = NewPayload(&count);
    failNextAllocation = 1;
    RetirePayload(7, ReleasePayload, payload);
    assert(allocationAttempts == 1 && allocationFailures == 1);
    assert(drainCalls == 1 && !fakeGPU.pending && count.releases == 1);
    MacMetalRetirement_Collect(retirement, UINT64_MAX);
    DestroyQueue();
    assert(count.releases == 1);
    puts("PASS: node allocation failure plus successful drain releases exactly once");
}

static void FailedAllocationFailedDrainRetainsPendingPayload(void)
{
    BeginCase(1, 1, 0);
    ReleaseCount count = { 0 };
    Payload *payload = NewPayload(&count);
    failNextAllocation = 1;
    RetirePayload(7, ReleasePayload, payload);
    assert(allocationAttempts == 1 && allocationFailures == 1);
    assert(drainCalls == 1 && fakeGPU.pending && count.releases == 0);
    assert(payload->marker == PAYLOAD_LIVE);
    MacMetalRetirement_Collect(retirement, UINT64_MAX);
    DestroyQueue();
    assert(fakeGPU.pending && count.releases == 0);
    assert(payload->marker == PAYLOAD_LIVE);
    puts("PASS: node allocation failure plus failed drain retains pending payload");

    /* Production intentionally keeps this unqueued payload until exit. The
     * fixture alone completes its CPU work and frees it for sanitizer cleanup;
     * this does not assert or add a production retry/recovery mechanism. */
    fakeGPU.pending = 0;
    ReleasePayload(payload);
    assert(count.releases == 1);
}

static void NoGPUAfterDrainedShutdownReleasesImmediately(void)
{
    BeginCase(0, 0, 0);
    ReleaseCount count = { 0 };
    Payload *payload = NewPayload(&count);
    failNextAllocation = 1;
    RetirePayload(7, ReleasePayload, payload);
    assert(allocationAttempts == 0 && allocationFailures == 0);
    assert(drainCalls == 0 && count.releases == 1);
    MacMetalRetirement_Collect(retirement, UINT64_MAX);
    DestroyQueue();
    assert(count.releases == 1);
    puts("PASS: NULL GPU after drained shutdown releases immediately");
}

int main(void)
{
    SuccessfulEnqueueWaitsForCompletion();
    FailedAllocationSuccessfulDrainReleasesOnce();
    FailedAllocationFailedDrainRetainsPendingPayload();
    NoGPUAfterDrainedShutdownReleasesImmediately();
    puts("RetirePayload CPU characterization: 4 cases pass");
    return 0;
}
