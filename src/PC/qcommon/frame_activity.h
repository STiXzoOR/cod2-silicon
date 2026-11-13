#ifndef COD2_FRAME_ACTIVITY_H
#define COD2_FRAME_ACTIVITY_H

#if defined(COD2_X64) && !defined(DEDICATED)
#include <stdint.h>
#include <time.h>

enum FrameActivityKind {
    FRAME_PROGRAM_UPLOAD, FRAME_TEXTURE_UPLOAD, FRAME_TEXTURE_CREATE,
    FRAME_TARGET_CREATE, FRAME_RESOLVE, FRAME_SCREEN_COPY, FRAME_PRESENT,
    FRAME_VIDEO_RESTART, FRAME_DRAW, FRAME_ACTIVITY_COUNT
};
typedef struct {
    unsigned count[FRAME_ACTIVITY_COUNT];
    uint64_t usec[FRAME_ACTIVITY_COUNT];
} FrameActivity;

/* Coalesced, initially null in standalone renderer fixtures. Match summaries
 * attach the collector only when the user's local diagnostics are enabled. */
__attribute__((weak)) FrameActivity *cod2_frame_activity;

static inline uint64_t FrameActivity_Now(void)
{
    struct timespec now;
    if (!cod2_frame_activity)
        return 0;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_nsec / 1000;
}

/* CPU wall time in the calls, including driver waits, not GPU timings. */
static inline void FrameActivity_Record(int kind, uint64_t started)
{
    if (!cod2_frame_activity)
        return;
    __atomic_fetch_add(&cod2_frame_activity->count[kind], 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&cod2_frame_activity->usec[kind], FrameActivity_Now() - started, __ATOMIC_RELAXED);
}
#define FRAME_ACTIVITY_BEGIN(name) uint64_t name = FrameActivity_Now()
#define FRAME_ACTIVITY_END(kind, name) FrameActivity_Record(kind, name)
#else
#define FRAME_ACTIVITY_BEGIN(name) ((void)0)
#define FRAME_ACTIVITY_END(kind, name) ((void)0)
#endif
#endif
