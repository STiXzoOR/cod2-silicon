/* Optional DYLD observer: no engine code or clock/physics changes. */
#include <SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <dlfcn.h>
#include <mach/mach_time.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define INTERPOSE(replacement, original) \
    __attribute__((used)) static const struct { const void *replace, *target; } \
    interpose_##original __attribute__((section("__DATA,__interpose"))) = \
        { (const void *)&replacement, (const void *)&original }

static struct { uint64_t stamp, swap, poll, blit, clear, fence; int engine; unsigned int polls; } frames[65536];
static unsigned int count;
static uint64_t pollTime, blitTime, clearTime, fenceTime, start, duration;
static unsigned int polls;
static mach_timebase_info_data_t timebase;
static volatile sig_atomic_t recording;
static int finished;
static int *frameTime;
static const char *filename;
extern void MacProbe_BeginCPU(void);
extern void MacProbe_SaveCPU(void);

static uint64_t nanos(void)
{
    return (uint64_t)((__uint128_t)mach_absolute_time() * timebase.numer / timebase.denom);
}

static void begin(int signal)
{
    (void)signal;
    if (!finished)
        recording = 1;
}

__attribute__((constructor)) static void initialize(void)
{
    mach_timebase_info(&timebase);
    filename = getenv("COD2_FRAME_CSV");
    const char *seconds = getenv("COD2_FRAME_SECONDS");
    duration = (uint64_t)((seconds ? atof(seconds) : 15.0) * 1e9);
    frameTime = dlsym(RTLD_DEFAULT, "com_frameTime");
    if (filename)
        signal(SIGUSR2, begin);
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
    fprintf(stream, "interval_ms,swap_ms,poll_ms,blit_ms,clear_ms,fence_ms,engine_ms,poll_calls\n");
    for (unsigned int i = 1; i < count; ++i) {
        fprintf(stream, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u\n",
                (frames[i].stamp - frames[i - 1].stamp) / 1e6,
                frames[i].swap / 1e6, frames[i].poll / 1e6,
                frames[i].blit / 1e6, frames[i].clear / 1e6, frames[i].fence / 1e6,
                (uint32_t)frames[i].engine - (uint32_t)frames[i - 1].engine, frames[i].polls);
    }
    fclose(stream);
    rename(temporary, filename);
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
    if (!start)
    {
        start = stamp;
        MacProbe_BeginCPU();
        int logicalW, logicalH, drawableW, drawableH;
        GLint viewport[4];
        SDL_GetWindowSize(window, &logicalW, &logicalH);
        SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
        glGetIntegerv(GL_VIEWPORT, viewport);
        fprintf(stderr, "[frame-probe] viewport=%dx%d window=%dx%d drawable=%dx%d swapInterval=%d windowFlags=0x%x engineClock=%s\n",
                viewport[2], viewport[3], logicalW, logicalH, drawableW, drawableH,
                SDL_GL_GetSwapInterval(), SDL_GetWindowFlags(window), frameTime ? "available" : "missing");
    }
    frames[count].stamp = stamp;
    frames[count].swap = end - stamp;
    frames[count].poll = pollTime;
    frames[count].polls = polls;
    frames[count].blit = blitTime;
    frames[count].clear = clearTime;
    frames[count].fence = fenceTime;
    frames[count].engine = frameTime ? *frameTime : 0;
    pollTime = blitTime = clearTime = fenceTime = 0;
    polls = 0;
    if (++count == 65536 || stamp - start >= duration)
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
    pollTime += nanos() - start;
}

static int probePeep(SDL_Event *events, int numevents, SDL_eventaction action, Uint32 minType, Uint32 maxType)
{
    if (!recording)
        return SDL_PeepEvents(events, numevents, action, minType, maxType);
    uint64_t start = nanos();
    int result = SDL_PeepEvents(events, numevents, action, minType, maxType);
    pollTime += nanos() - start;
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

static void probeClear(GLbitfield mask)
{
    if (!recording) {
        glClear(mask);
        return;
    }
    uint64_t start = nanos();
    glClear(mask);
    clearTime += nanos() - start;
}

INTERPOSE(probeSwap, SDL_GL_SwapWindow);
INTERPOSE(probePoll, SDL_PollEvent);
INTERPOSE(probePump, SDL_PumpEvents);
INTERPOSE(probePeep, SDL_PeepEvents);
INTERPOSE(probeBlit, glBlitFramebufferEXT);
INTERPOSE(probeClear, glClear);
INTERPOSE(probeFence, glTestFenceAPPLE);
