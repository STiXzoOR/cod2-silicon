/* Optional DYLD observer: no engine code or clock/physics changes. */
#include <SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/OpenGL.h>
#include <CoreGraphics/CoreGraphics.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <dlfcn.h>
#include <mach/mach_time.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define INTERPOSE(replacement, original) \
    __attribute__((used)) static const struct { const void *replace, *target; } \
    interpose_##original __attribute__((section("__DATA,__interpose"))) = \
        { (const void *)&replacement, (const void *)&original }

static struct {
    uint64_t stamp, cpu, swap, poll, blit, clear, fence, upload, program, buffer, wait, overshoot, sleep, pump, peep, sceneClear, presentClear, sleepOvershoot, flush, syncIssue;
    int engine;
    unsigned int polls, uploads, programs, buffers, syncTimeouts;
} frames[262144];
static unsigned int count;
static uint64_t presentStamps[262144];
static unsigned int presentCount;
static uint64_t swapTime, waitTime, waitOvershoot;
static int clientBoundary;
static uint64_t sleepTime, pumpTime, peepTime, sceneClearTime, presentClearTime;
static GLuint currentDrawFramebuffer;
static _Thread_local int capActive;
static uint64_t sleepOvershoot, flushTime, syncIssueTime;
static uint64_t pollTime, blitTime, clearTime, fenceTime, start, duration;
static unsigned int polls;
static uint64_t uploadTime, programTime, bufferTime, previousSwap;
static unsigned int uploads, programs, buffers, syncTimeouts;
static mach_timebase_info_data_t timebase;
static volatile sig_atomic_t recording;
static int finished;
static int *frameTime;
static const char *filename;
static uint64_t geometryCheck;
static int logicalWidth, logicalHeight, drawableWidth, drawableHeight;
static Uint32 windowFlags;
extern void MacProbe_BeginCPU(void);
void MacFrameProbe_Frame(int engineTime);
extern void MacProbe_SaveCPU(void);

static void backingSize(SDL_Window *window, int *width, int *height)
{
    GLint enabled = 0, size[2];
    CGLContextObj ctx = CGLGetCurrentContext();
    if (ctx && CGLIsEnabled(ctx, kCGLCESurfaceBackingSize, &enabled) == kCGLNoError && enabled &&
        CGLGetParameter(ctx, kCGLCPSurfaceBackingSize, size) == kCGLNoError) {
        *width = size[0]; *height = size[1];
    } else {
        SDL_GL_GetDrawableSize(window, width, height);
    }
}

static uint64_t nanos(void)
{
    return (uint64_t)((__uint128_t)mach_absolute_time() * timebase.numer / timebase.denom);
}

static uint64_t cpuNanos(void)
{
    struct timespec value;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value);
    return (uint64_t)value.tv_sec * 1000000000ull + value.tv_nsec;
}

static void begin(int signal)
{
    (void)signal;
    if (!finished)
        recording = 1;
}

__attribute__((constructor)) static void initialize(void)
{
    const char *pidFile = getenv("COD2_FRAME_PID");
    if (pidFile) {
        FILE *stream = fopen(pidFile, "w");
        if (stream) { fprintf(stream, "%d\n", getpid()); fclose(stream); }
    }
    mach_timebase_info(&timebase);
    filename = getenv("COD2_FRAME_CSV");
    const char *seconds = getenv("COD2_FRAME_SECONDS");
    duration = (uint64_t)((seconds ? atof(seconds) : 15.0) * 1e9);
    frameTime = dlsym(RTLD_DEFAULT, "com_frameTime");
    if (filename)
        signal(SIGUSR1, begin);
}

static void save(void)
{
    finished = 1;
    recording = 0;
    MacProbe_SaveCPU();
    char temporary[4096];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", filename) >= sizeof(temporary))
        return;
    FILE *stream = fopen(temporary, "w");
    if (!stream)
        return;
    fprintf(stream, "interval_ms,swap_ms,poll_ms,blit_ms,clear_ms,fence_ms,engine_ms,poll_calls,cpu_ms,upload_ms,program_ms,buffer_ms,upload_calls,program_calls,buffer_calls,cap_wait_ms,wait_overshoot_ms,sync_timeouts,sleep_ms,pump_ms,peep_ms,scene_clear_ms,present_clear_ms,sleep_overshoot_ms,flush_ms,sync_issue_ms\n");
    for (unsigned int i = 1; i < count; ++i) {
        fprintf(stream, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u,%.6f,%.6f,%.6f,%.6f,%u,%u,%u,%.6f,%.6f,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                (frames[i].stamp - frames[i - 1].stamp) / 1e6,
                frames[i].swap / 1e6, frames[i].poll / 1e6,
                frames[i].blit / 1e6, frames[i].clear / 1e6, frames[i].fence / 1e6,
                (uint32_t)frames[i].engine - (uint32_t)frames[i - 1].engine, frames[i].polls,
                (frames[i].cpu - frames[i - 1].cpu) / 1e6,
                frames[i].upload / 1e6, frames[i].program / 1e6, frames[i].buffer / 1e6,
                frames[i].uploads, frames[i].programs, frames[i].buffers,
                frames[i].wait / 1e6, frames[i].overshoot / 1e6, frames[i].syncTimeouts, frames[i].sleep / 1e6,
                frames[i].pump / 1e6, frames[i].peep / 1e6,
                frames[i].sceneClear / 1e6, frames[i].presentClear / 1e6, frames[i].sleepOvershoot / 1e6, frames[i].flush / 1e6, frames[i].syncIssue / 1e6);
    }
    fclose(stream);
    rename(temporary, filename);
    char presented[4096];
    if (snprintf(presented, sizeof(presented), "%s.presented.csv", filename) >= sizeof(presented))
        return;
    stream = fopen(presented, "w");
    if (stream) {
        fprintf(stream, "interval_ms\n");
        for (unsigned int i = 1; i < presentCount; ++i)
            fprintf(stream, "%.6f\n", (presentStamps[i] - presentStamps[i - 1]) / 1e6);
        fclose(stream);
    }
}

static void probeSwap(SDL_Window *window)
{
    if (!recording) {
        SDL_GL_SwapWindow(window);
        return;
    }
    uint64_t stamp = nanos();
    SDL_GL_SwapWindow(window);
    uint64_t end = nanos();
    if (!geometryCheck)
    {
        int logicalW, logicalH, drawableW, drawableH;
        GLint viewport[4];
        SDL_GetWindowSize(window, &logicalW, &logicalH);
        backingSize(window, &drawableW, &drawableH);
        logicalWidth = logicalW; logicalHeight = logicalH;
        drawableWidth = drawableW; drawableHeight = drawableH;
        windowFlags = SDL_GetWindowFlags(window);
        geometryCheck = stamp;
        glGetIntegerv(GL_VIEWPORT, viewport);
        fprintf(stderr, "[frame-probe] viewport=%dx%d window=%dx%d drawable=%dx%d swapInterval=%d windowFlags=0x%x engineClock=%s\n",
                viewport[2], viewport[3], logicalW, logicalH, drawableW, drawableH,
                SDL_GL_GetSwapInterval(), windowFlags, frameTime ? "available" : "missing");
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
        qos_class_t qos = QOS_CLASS_UNSPECIFIED;
        pthread_get_qos_class_np(pthread_self(), &qos, NULL);
        int viewW, viewH;
        SDL_GL_GetDrawableSize(window, &viewW, &viewH);
        fprintf(stderr, "[frame-probe-display] sdlDrawable=%dx%d display=%zux%zu pixels=%zux%zu refresh=%.2f qos=0x%x\n",
                viewW, viewH, CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode),
                CGDisplayModeGetPixelWidth(mode), CGDisplayModeGetPixelHeight(mode),
                CGDisplayModeGetRefreshRate(mode), qos);
        CGDisplayModeRelease(mode);
    }
    if (stamp - geometryCheck >= 1000000000ull) {
        int lw, lh, dw, dh;
        SDL_GetWindowSize(window, &lw, &lh);
        backingSize(window, &dw, &dh);
        Uint32 flags = SDL_GetWindowFlags(window);
        /* Focus changes are recorded too; callers decide comparability. */
        if (lw != logicalWidth || lh != logicalHeight || dw != drawableWidth || dh != drawableHeight || flags != windowFlags) {
            fprintf(stderr, "[frame-probe-change] window=%dx%d drawable=%dx%d windowFlags=0x%x\n", lw, lh, dw, dh, flags);
            logicalWidth = lw; logicalHeight = lh;
            drawableWidth = dw; drawableHeight = dh; windowFlags = flags;
        }
        geometryCheck = stamp;
    }
    swapTime += end - stamp;
    if (presentCount < sizeof(presentStamps) / sizeof(*presentStamps))
        presentStamps[presentCount++] = end;
    if (!clientBoundary) {
        /* Compatibility with the earlier binaries: label swap-boundary data. */
        swapTime = previousSwap;
        previousSwap = end - stamp;
        MacFrameProbe_Frame(frameTime ? *frameTime : 0);
        clientBoundary = 0;
    }
}

void MacFrameProbe_Wait(uint64_t started, uint64_t target, uint64_t ended)
{
    capActive = !ended;
    if (!ended)
        return;
    if (recording) {
        waitTime += ended - started;
        uint64_t late = ended > target ? ended - target : 0;
        if (late > waitOvershoot)
            waitOvershoot = late;
    }
}

void MacFrameProbe_Frame(int engineTime)
{
    clientBoundary = 1;
    if (!recording)
        return;
    uint64_t stamp = nanos(), cpu = cpuNanos();
    if (!start) {
        start = stamp;
        MacProbe_BeginCPU();
        fprintf(stderr, "[frame-probe-clock] boundary=%s\n",
                dlsym(RTLD_DEFAULT, "MacSystem_ObserveFrame") ? "client" : "swap");
    }
    frames[count].stamp = stamp;
    frames[count].cpu = cpu;
    frames[count].swap = swapTime;
    frames[count].flush = flushTime;
    frames[count].syncIssue = syncIssueTime;
    flushTime = syncIssueTime = 0;
    frames[count].sleep = sleepTime;
    frames[count].sleepOvershoot = sleepOvershoot;
    sleepOvershoot = 0;
    frames[count].pump = pumpTime;
    frames[count].peep = peepTime;
    frames[count].sceneClear = sceneClearTime;
    frames[count].presentClear = presentClearTime;
    sleepTime = pumpTime = peepTime = sceneClearTime = presentClearTime = 0;
    frames[count].wait = waitTime;
    frames[count].overshoot = waitOvershoot;
    frames[count].poll = pollTime;
    frames[count].polls = polls;
    frames[count].blit = blitTime;
    frames[count].clear = clearTime;
    frames[count].fence = fenceTime;
    frames[count].engine = engineTime;
    frames[count].upload = uploadTime;
    frames[count].program = programTime;
    frames[count].buffer = bufferTime;
    frames[count].syncTimeouts = syncTimeouts;
    syncTimeouts = 0;
    frames[count].uploads = uploads;
    frames[count].programs = programs;
    frames[count].buffers = buffers;
    pollTime = blitTime = clearTime = fenceTime = swapTime = waitTime = waitOvershoot = 0;
    uploadTime = programTime = bufferTime = 0;
    uploads = programs = buffers = 0;
    polls = 0;
    if (++count == sizeof(frames) / sizeof(frames[0]) || stamp - start >= duration)
        save();
}

static int probePoll(SDL_Event *event)
{
    if (!recording)
        return SDL_PollEvent(event);
    uint64_t start = nanos();
    ++polls;
    int result = SDL_PollEvent(event);
    pollTime += nanos() - start;
    return result;
}

static void probePump(void)
{
    if (!recording) {
        SDL_PumpEvents();
        return;
    }
    uint64_t start = nanos();
    ++polls;
    SDL_PumpEvents();
    uint64_t elapsed = nanos() - start;
    pollTime += elapsed;
    pumpTime += elapsed;
}

static int probePeep(SDL_Event *events, int numevents, SDL_eventaction action, Uint32 minType, Uint32 maxType)
{
    if (!recording)
        return SDL_PeepEvents(events, numevents, action, minType, maxType);
    uint64_t start = nanos();
    int result = SDL_PeepEvents(events, numevents, action, minType, maxType);
    uint64_t elapsed = nanos() - start;
    pollTime += elapsed;
    peepTime += elapsed;
    return result;
}

static void probeBlit(GLint x0, GLint y0, GLint x1, GLint y1,
                      GLint dx0, GLint dy0, GLint dx1, GLint dy1,
                      GLbitfield mask, GLenum filter)
{
    if (!recording) {
        glBlitFramebufferEXT(x0, y0, x1, y1, dx0, dy0, dx1, dy1, mask, filter);
        return;
    }
    uint64_t start = nanos();
    glBlitFramebufferEXT(x0, y0, x1, y1, dx0, dy0, dx1, dy1, mask, filter);
    blitTime += nanos() - start;
}

static GLboolean probeFence(GLuint fence)
{
    if (!recording)
        return glTestFenceAPPLE(fence);
    uint64_t start = nanos();
    GLboolean result = glTestFenceAPPLE(fence);
    fenceTime += nanos() - start;
    return result;
}

static void probeFlush(void)
{
    if (!recording) { glFlush(); return; }
    uint64_t started = nanos();
    glFlush();
    flushTime += nanos() - started;
}

static GLsync probeIssue(GLenum condition, GLbitfield flags)
{
    if (!recording)
        return glFenceSync(condition, flags);
    uint64_t started = nanos();
    GLsync result = glFenceSync(condition, flags);
    syncIssueTime += nanos() - started;
    return result;
}

static GLenum probeSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if (!recording)
        return glClientWaitSync(sync, flags, timeout);
    uint64_t start = nanos();
    GLenum result = glClientWaitSync(sync, flags, timeout);
    if (result == GL_TIMEOUT_EXPIRED)
        ++syncTimeouts;
    fenceTime += nanos() - start;
    return result;
}

static void probeClear(GLbitfield mask)
{
    if (!recording) {
        glClear(mask);
        return;
    }
    uint64_t start = nanos();
    glClear(mask);
    uint64_t elapsed = nanos() - start;
    clearTime += elapsed;
    if (currentDrawFramebuffer)
        sceneClearTime += elapsed;
    else
        presentClearTime += elapsed;
}

static int probeFullscreen(SDL_Window *window, Uint32 flags)
{
    int result = SDL_SetWindowFullscreen(window, flags);
    if (filename)
        fprintf(stderr, "[frame-probe-mode] requested=0x%x actual=0x%x result=%d error=%s\n",
                flags, SDL_GetWindowFlags(window), result, result ? SDL_GetError() : "none");
    return result;
}

static kern_return_t probeSleep(uint64_t deadline)
{
    if (!recording || !capActive)
        return mach_wait_until(deadline);
    uint64_t started = nanos();
    kern_return_t result = mach_wait_until(deadline);
    uint64_t ended = nanos();
    sleepTime += ended - started;
    uint64_t target = (uint64_t)((__uint128_t)deadline * timebase.numer / timebase.denom);
    if (ended > target && ended - target > sleepOvershoot)
        sleepOvershoot = ended - target;
    return result;
}

static void probeFramebuffer(GLenum target, GLuint framebuffer)
{
    if (target == GL_FRAMEBUFFER_EXT || target == GL_DRAW_FRAMEBUFFER_EXT)
        currentDrawFramebuffer = framebuffer;
    glBindFramebufferEXT(target, framebuffer);
}

INTERPOSE(probeSleep, mach_wait_until);
INTERPOSE(probeFramebuffer, glBindFramebufferEXT);
INTERPOSE(probeSwap, SDL_GL_SwapWindow);
INTERPOSE(probePoll, SDL_PollEvent);
INTERPOSE(probePump, SDL_PumpEvents);
INTERPOSE(probePeep, SDL_PeepEvents);
INTERPOSE(probeBlit, glBlitFramebufferEXT);
INTERPOSE(probeClear, glClear);
INTERPOSE(probeFence, glTestFenceAPPLE);
INTERPOSE(probeSync, glClientWaitSync);
INTERPOSE(probeIssue, glFenceSync);
INTERPOSE(probeFlush, glFlush);
INTERPOSE(probeFullscreen, SDL_SetWindowFullscreen);

/* Creation/upload calls are counted only during the warmed capture. */
#define TRACE_GL(name, phase, declaration, arguments) \
    static void probe_##name declaration { \
        if (!recording) { name arguments; return; } \
        uint64_t started = nanos(); \
        name arguments; \
        phase##Time += nanos() - started; ++phase##s; \
    } \
    INTERPOSE(probe_##name, name)

TRACE_GL(glTexImage2D, upload,
    (GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLint b, GLenum format, GLenum type, const GLvoid *p),
    (t, l, f, w, h, b, format, type, p));
TRACE_GL(glTexSubImage2D, upload,
    (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, const GLvoid *p),
    (t, l, x, y, w, h, format, type, p));
TRACE_GL(glCompressedTexImage2DARB, upload,
    (GLenum t, GLint l, GLenum f, GLsizei w, GLsizei h, GLint b, GLsizei n, const GLvoid *p),
    (t, l, f, w, h, b, n, p));
TRACE_GL(glCompressedTexSubImage2D, upload,
    (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLsizei n, const GLvoid *p),
    (t, l, x, y, w, h, f, n, p));
TRACE_GL(glTexImage3D, upload,
    (GLenum t, GLint l, GLint f, GLsizei w, GLsizei h, GLsizei d, GLint b, GLenum format, GLenum type, const GLvoid *p),
    (t, l, f, w, h, d, b, format, type, p));
TRACE_GL(glTexSubImage3D, upload,
    (GLenum t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type, const GLvoid *p),
    (t, l, x, y, z, w, h, d, format, type, p));
TRACE_GL(glCompressedTexImage3DARB, upload,
    (GLenum t, GLint l, GLenum f, GLsizei w, GLsizei h, GLsizei d, GLint b, GLsizei n, const GLvoid *p),
    (t, l, f, w, h, d, b, n, p));
TRACE_GL(glProgramStringARB, program,
    (GLenum t, GLenum f, GLsizei n, const GLvoid *p), (t, f, n, p));
TRACE_GL(glBufferDataARB, buffer,
    (GLenum t, GLsizeiptrARB n, const GLvoid *p, GLenum u), (t, n, p, u));
