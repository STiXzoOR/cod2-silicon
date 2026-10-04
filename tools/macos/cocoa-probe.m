/* Optional observer only: time AppKit methods without changing event order. */
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern int MacFrameProbe_IsRecording(void);
static mach_timebase_info_data_t timebase;
static FILE *trace;
static NSEvent *(*originalNext)(id, SEL, NSEventMask, NSDate *, NSRunLoopMode, BOOL);
static void (*originalSend)(id, SEL, NSEvent *);
static void (*originalUpdate)(id, SEL);
static NSModalResponse (*originalModal)(id, SEL, NSModalSession);

static uint64_t now(void)
{
    return (uint64_t)((__uint128_t)mach_absolute_time() * timebase.numer / timebase.denom);
}

static void record(const char *method, uint64_t started, long eventType)
{
    uint64_t ended = now();
    if (ended - started >= 500000 && MacFrameProbe_IsRecording()) {
        fprintf(trace, "%s,%.6f,%ld,%llu\n", method, (ended - started) / 1e6,
                eventType, (unsigned long long)ended);
        fflush(trace);
    }
}

static NSEvent *probeNext(id self, SEL selector, NSEventMask mask, NSDate *until,
                     NSRunLoopMode mode, BOOL dequeue)
{
    uint64_t started = now();
    NSEvent *event = originalNext(self, selector, mask, until, mode, dequeue);
    record("nextEvent", started, event ? event.type : -1);
    return event;
}

static void probeSend(id self, SEL selector, NSEvent *event)
{
    long type = event.type;
    uint64_t started = now();
    originalSend(self, selector, event);
    record("sendEvent", started, type);
}

static void probeUpdate(id self, SEL selector)
{
    uint64_t started = now();
    originalUpdate(self, selector);
    record("updateWindows", started, -1);
}

static NSModalResponse probeModal(id self, SEL selector, NSModalSession session)
{
    uint64_t started = now();
    NSModalResponse result = originalModal(self, selector, session);
    record("runModalSession", started, -1);
    return result;
}

static IMP replace(Class cls, SEL selector, IMP replacement)
{
    Method method = class_getInstanceMethod(cls, selector);
    if (!method)
        return NULL;
    IMP original = method_getImplementation(method);
    class_replaceMethod(cls, selector, replacement, method_getTypeEncoding(method));
    return original;
}

static void install(void)
{
    if (!NSApp) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{ install(); });
        return;
    }
    Class cls = object_getClass(NSApp);
    originalNext = (void *)replace(cls, @selector(nextEventMatchingMask:untilDate:inMode:dequeue:), (IMP)probeNext);
    originalSend = (void *)replace(cls, @selector(sendEvent:), (IMP)probeSend);
    originalUpdate = (void *)replace(cls, @selector(updateWindows), (IMP)probeUpdate);
    originalModal = (void *)replace(cls, @selector(runModalSession:), (IMP)probeModal);
    fprintf(stderr, "[cocoa-probe] installed on %s\n", class_getName(cls));
}

__attribute__((constructor)) static void initialize(void)
{
    const char *path = getenv("COD2_COCOA_TRACE");
    if (!path)
        return;
    trace = fopen(path, "w");
    if (!trace)
        return;
    fprintf(trace, "method,elapsed_ms,event_type,stamp_ns\n");
    mach_timebase_info(&timebase);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ install(); });
}
