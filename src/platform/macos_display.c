#include "macos_display.h"
#include "macos_system.h"
#include <SDL2/SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#if defined(__APPLE__) && defined(COD2_X64)
#include <OpenGL/OpenGL.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(COD2_X64)
#include "macos_gamma.h"
#endif

/* The first fields retain OpaqueContextRef's native layout. */
typedef struct {
    SDL_GLContext context;
    SDL_Window *drawable;
    int rendererID;
    unsigned char doubleBuffered;
    GLuint framebuffer, color[3], depth;
    int width, height;
    int depthBits, stencil;
#if defined(COD2_X64)
    MacPresentationGamma gamma;
#endif
} MacContext;

typedef struct { int width, height, depth, refresh; } MacMode;
static MacMode *modes;
static int modeCount;
static int windowMode = MAC_WINDOWED;
static int refreshRate;
static int initialized;
static MacContext *screenContext;
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
static int presentMode = 1;
static GLsync presentFence;
void MacPlatform_SetPresentMode(int mode) { presentMode = mode; }
#endif
#if !defined(COD2_X64)
static unsigned short originalGamma[3][256];
static int gammaSaved;
#endif

int g_dip_is_tri, g_dip_drawflag_zero, g_dip_numelems_zero, g_dip_gl_draw;
int g_fp_enable_count, g_fp_bind_count;
SDL_Window *sdl_gl_window;
int sdl_gl_width = 640;
int sdl_gl_height = 480;
unsigned char sInWindowMode = 1;
size_t sDisplayIndex;
/* Native consumers use SDL modes and GL queries, not i386 CDisplayInfo bytes. */
void *sDisplayList[3];

int MacDisplay_Initialize(void)
{
    if (initialized)
        return 0;
    SDL_SetHint("SDL_MAC_USE_GCMOUSE", "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    SDL_SetHint("SDL_MOUSE_RELATIVE_SYSTEM_SCALE", "0");
#if defined(__APPLE__) && defined(COD2_X64)
    /* Cocoa reads this at video initialization, before mode enumeration.
     * The environment can opt into Spaces for Game Mode experiments. */
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
    if (SDL_GetHintBoolean(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, SDL_FALSE))
        SDL_SetHint("SDL_VIDEO_SYNC_WINDOW_OPERATIONS", "1");
#endif
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
        return -1;
#if defined(__APPLE__) && defined(COD2_X64) && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
    /* SDL's Cocoa app delegate installs its own URL drop handler at video
     * initialization. Restore the validated CoD2x handler afterwards. */
    { extern void Cod2xNativeURL_Install(void); Cod2xNativeURL_Install(); }
#endif
    int count = SDL_GetNumDisplayModes(0);
    modes = calloc((size_t)(count > 0 ? count : 1), sizeof(*modes));
    if (!modes)
        return -1;
    for (int i = 0; i < count; ++i) {
        SDL_DisplayMode m;
        if (SDL_GetDisplayMode(0, i, &m) != 0)
            continue;
        int duplicate = 0;
        for (int j = 0; j < modeCount; ++j)
            duplicate |= modes[j].width == m.w && modes[j].height == m.h && modes[j].refresh == m.refresh_rate;
        if (!duplicate)
            modes[modeCount++] = (MacMode){ m.w, m.h, 32, m.refresh_rate };
    }
    if (!modeCount)
        modes[modeCount++] = (MacMode){ 640, 480, 32, 60 };
    initialized = 1;
    return 0;
}

void MacPlatform_ConfigureWindow(int width, int height, int mode, int refresh)
{
    sdl_gl_width = width;
    sdl_gl_height = height;
    windowMode = mode;
    refreshRate = refresh;
    sInWindowMode = mode == MAC_WINDOWED;
}

static int SetWindowMode(void)
{
#if defined(__APPLE__) && defined(COD2_X64)
    /* A native fullscreen Space keeps the desktop display mode. Selecting an
     * exclusive display mode sends SDL down the non-Spaces Cocoa path. */
    if (windowMode == MAC_FULLSCREEN &&
        SDL_GetHintBoolean(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, SDL_FALSE))
        windowMode = MAC_BORDERLESS;
    /* Reset also runs at map load. Toggling out and immediately back into a
     * Cocoa fullscreen Space can cancel its asynchronous transition. */
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, windowMode == MAC_BORDERLESS ? "0" : "1");
    if (windowMode == MAC_WINDOWED && SDL_SetWindowFullscreen(sdl_gl_window, 0) != 0)
#else
    if (SDL_SetWindowFullscreen(sdl_gl_window, 0) != 0)
#endif
        return -1;
    if (windowMode == MAC_FULLSCREEN) {
        SDL_DisplayMode desired = { 0 }, closest;
        desired.w = sdl_gl_width;
        desired.h = sdl_gl_height;
        desired.refresh_rate = refreshRate;
        int display = SDL_GetWindowDisplayIndex(sdl_gl_window);
#if defined(__APPLE__) && defined(COD2_X64)
        int found = 0;
        for (int i = 0; i < SDL_GetNumDisplayModes(display); ++i) {
            SDL_DisplayMode candidate;
            if (SDL_GetDisplayMode(display, i, &candidate) == 0 &&
                candidate.w == desired.w && candidate.h == desired.h &&
                (!desired.refresh_rate || candidate.refresh_rate == desired.refresh_rate)) {
                closest = candidate;
                found = 1;
                break;
            }
        }
        if (!found)
            SDL_SetError("no exact %dx%d display mode", desired.w, desired.h);
        if (!found || SDL_SetWindowDisplayMode(sdl_gl_window, &closest) != 0 ||
            SDL_SetWindowFullscreen(sdl_gl_window, SDL_WINDOW_FULLSCREEN) != 0)
#else
        if (!SDL_GetClosestDisplayMode(display, &desired, &closest) ||
            SDL_SetWindowDisplayMode(sdl_gl_window, &closest) != 0 ||
            SDL_SetWindowFullscreen(sdl_gl_window, SDL_WINDOW_FULLSCREEN) != 0)
#endif
        {
            fprintf(stderr, "CoD2-native exclusive fullscreen unavailable: %s; using desktop fullscreen\n", SDL_GetError());
#if defined(__APPLE__) && defined(COD2_X64)
            windowMode = MAC_BORDERLESS;
            SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#endif
            if (SDL_SetWindowFullscreen(sdl_gl_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
                return -1;
        }
    } else if (windowMode == MAC_BORDERLESS) {
        if (SDL_SetWindowFullscreen(sdl_gl_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
            return -1;
    } else {
        int logicalW, logicalH, pixelW, pixelH;
        SDL_GetWindowSize(sdl_gl_window, &logicalW, &logicalH);
#if defined(COD2_X64)
        SDL_GetWindowSizeInPixels(sdl_gl_window, &pixelW, &pixelH);
#else
        SDL_GL_GetDrawableSize(sdl_gl_window, &pixelW, &pixelH);
#endif
        double scaleX = logicalW > 0 ? (double)pixelW / logicalW : 1;
        double scaleY = logicalH > 0 ? (double)pixelH / logicalH : 1;
        SDL_SetWindowSize(sdl_gl_window, (int)(sdl_gl_width / scaleX), (int)(sdl_gl_height / scaleY));
        SDL_SetWindowPosition(sdl_gl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    return 0;
}

#if defined(__APPLE__) && defined(COD2_X64)
static int SetSurfaceSize(int width, int height)
{
    CGLContextObj context = CGLGetCurrentContext();
    GLint size[2] = { width, height };
    CGLError error = CGLSetParameter(context, kCGLCPSurfaceBackingSize, size);
    if (error == kCGLNoError)
        error = CGLEnable(context, kCGLCESurfaceBackingSize);
    if (error != kCGLNoError)
        fprintf(stderr, "CoD2-native fixed OpenGL backing unavailable: %s\n", CGLErrorString(error));
    return error == kCGLNoError ? 0 : -1;
}
#endif

static int CreateRenderBuffer(MacContext *ctx, int depthBits, int stencil)
{
    glGenFramebuffersEXT(1, &ctx->framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ctx->framebuffer);
    glGenRenderbuffersEXT(3, ctx->color);
    for (int i = 0; i < 3; ++i) {
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, ctx->color[i]);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, ctx->width, ctx->height);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_RENDERBUFFER_EXT, ctx->color[i]);
    }
    glGenRenderbuffersEXT(1, &ctx->depth);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, ctx->depth);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, stencil ? GL_DEPTH24_STENCIL8_EXT :
                             (depthBits == 16 ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24), ctx->width, ctx->height);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, ctx->depth);
    if (stencil)
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, ctx->depth);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    return glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT ? 0 : -1;
}

void *MacDisplay_CreateScreenContext(int depth, int stencil, int samples,
                                    int quality, int interval, int *hasAux)
{
    (void)samples;
    (void)quality;
    (void)interval;
    if (hasAux)
        *hasAux = 0;
    if (MacDisplay_Initialize() != 0)
        return NULL;
    if (screenContext)
        MacDisplay_ReleaseContext((void **)&screenContext);
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth ? depth : 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencil ? 8 : 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (!sdl_gl_window)
        sdl_gl_window = SDL_CreateWindow("CoD2-native", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         sdl_gl_width, sdl_gl_height,
#if defined(__APPLE__) && defined(COD2_X64)
                                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
                                         (windowMode == MAC_WINDOWED ? SDL_WINDOW_ALLOW_HIGHDPI : 0));
#else
                                         SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
#endif
    if (!sdl_gl_window)
        return NULL;
    MacContext *ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->context = SDL_GL_CreateContext(sdl_gl_window);
    ctx->drawable = sdl_gl_window;
    ctx->doubleBuffered = 1;
    ctx->width = sdl_gl_width;
    ctx->height = sdl_gl_height;
    ctx->depthBits = depth;
    ctx->stencil = stencil;
    if (!ctx->context || SetWindowMode() != 0) {
        MacDisplay_ReleaseContext((void **)&ctx);
        return NULL;
    }
#if defined(__APPLE__) && defined(COD2_X64)
    /* CGL fixes the actual back buffer; WindowServer scales to the view.
     * SDL's drawable query describes view bounds, not this override. */
    if (windowMode != MAC_WINDOWED && SetSurfaceSize(ctx->width, ctx->height) != 0) {
        MacDisplay_ReleaseContext((void **)&ctx);
        return NULL;
    }
#endif
    SDL_GL_SetSwapInterval(0);
    if (!strstr((const char *)glGetString(GL_EXTENSIONS), "GL_EXT_framebuffer_blit") ||
        CreateRenderBuffer(ctx, depth, stencil) != 0) {
        fprintf(stderr, "CoD2-native render framebuffer unavailable\n");
        MacDisplay_ReleaseContext((void **)&ctx);
        return NULL;
    }
    screenContext = ctx;
    if (hasAux)
        *hasAux = 1;
    glViewport(0, 0, ctx->width, ctx->height);
    SDL_RaiseWindow(sdl_gl_window);
    return ctx;
}

/* Preserve the original renderer's back/front, AUX0 and AUX1 selection. */
static GLenum RenderBuffer(unsigned int buffer)
{
    if (!screenContext)
        return buffer;
    if (buffer == GL_BACK || buffer == GL_FRONT || buffer == GL_FRONT_AND_BACK)
        return GL_COLOR_ATTACHMENT0_EXT;
    if (buffer == GL_AUX0 || buffer == GL_AUX1)
        return GL_COLOR_ATTACHMENT1_EXT + buffer - GL_AUX0;
    return buffer;
}
void MacGL_DrawBuffer(unsigned int buffer) { glDrawBuffer(RenderBuffer(buffer)); }
void MacGL_ReadBuffer(unsigned int buffer) { glReadBuffer(RenderBuffer(buffer)); }

static void PresentationRect(int renderW, int renderH, int drawableW, int drawableH,
                             int *x, int *y, int *w, int *h)
{
    double scaleX = (double)drawableW / renderW, scaleY = (double)drawableH / renderH;
    double scale = scaleX < scaleY ? scaleX : scaleY;
    *w = (int)(renderW * scale); *h = (int)(renderH * scale);
    *x = (drawableW - *w) / 2; *y = (drawableH - *h) / 2;
}
void MacPlatform_TransformPoint(int x, int y, int logicalW, int logicalH,
                                int drawableW, int drawableH, int *rx, int *ry)
{
    int renderW, renderH, vx, vy, vw, vh;
    MacPlatform_GetRenderSize(&renderW, &renderH);
    *rx = *ry = 0;
    if (logicalW <= 0 || logicalH <= 0 || drawableW <= 0 || drawableH <= 0)
        return;
    PresentationRect(renderW, renderH, drawableW, drawableH, &vx, &vy, &vw, &vh);
    if (vw <= 0 || vh <= 0)
        return;
    *rx = (int)(((double)x * drawableW / logicalW - vx) * renderW / vw);
    *ry = (int)(((double)y * drawableH / logicalH - vy) * renderH / vh);
    if (*rx < 0) *rx = 0;
    if (*ry < 0) *ry = 0;
    if (*rx >= renderW) *rx = renderW - 1;
    if (*ry >= renderH) *ry = renderH - 1;
}
void MacPlatform_MapWindowPoint(int x, int y, int *rx, int *ry)
{
    int logicalW, logicalH, pixelW, pixelH;
    SDL_GetWindowSize(sdl_gl_window, &logicalW, &logicalH);
#if defined(__APPLE__) && defined(COD2_X64)
    MacPlatform_GetDrawableSize(&pixelW, &pixelH);
#else
    SDL_GL_GetDrawableSize(sdl_gl_window, &pixelW, &pixelH);
#endif
    MacPlatform_TransformPoint(x, y, logicalW, logicalH, pixelW, pixelH, rx, ry);
}

void SDL_GL_SwapWindowDirect(void)
{
    MacContext *ctx = screenContext;
    if (!ctx)
        return;
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
    if (presentFence) {
        GLenum ready = glClientWaitSync(presentFence, 0, 0);
        if (presentMode && ready == GL_TIMEOUT_EXPIRED) {
            /* Keep submitting the current scene, never queue an older image. */
            glFlush();
            return;
        }
        glDeleteSync(presentFence);
        presentFence = NULL;
    }
#endif
    int width, height;
#if defined(__APPLE__) && defined(COD2_X64)
    MacPlatform_GetDrawableSize(&width, &height);
#else
    SDL_GL_GetDrawableSize(sdl_gl_window, &width, &height);
#endif
    if (width <= 0 || height <= 0)
        return;
    int x, y, destW, destH;
    PresentationRect(ctx->width, ctx->height, width, height, &x, &y, &destW, &destH);
    GLint readBuffer, drawBuffer;
    glGetIntegerv(GL_READ_BUFFER, &readBuffer);
    glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
#if defined(COD2_X64)
    GLint readFramebuffer, drawFramebuffer;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &readFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &drawFramebuffer);
    glPushAttrib(presentationGammaIdentity ?
                 GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT | GL_PIXEL_MODE_BIT : GL_ALL_ATTRIB_BITS);
#else
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT | GL_PIXEL_MODE_BIT);
#endif
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
    glDrawBuffer(GL_BACK);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, ctx->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
#if defined(COD2_X64)
    if (!GammaPresent(&ctx->gamma, ctx->width, ctx->height, x, y, destW, destH))
#endif
        glBlitFramebufferEXT(0, 0, ctx->width, ctx->height, x, y, x + destW, y + destH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    SDL_GL_SwapWindow(sdl_gl_window);
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
    if (presentMode) {
        presentFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
    }
#endif
#if defined(COD2_X64)
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, readFramebuffer);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, drawFramebuffer);
#else
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ctx->framebuffer);
#endif
    glPopAttrib();
    glReadBuffer(readBuffer);
    glDrawBuffer(drawBuffer);
}

uint16_t MacDisplay_ReleaseContext(void **reference)
{
    MacContext *ctx = reference ? *reference : NULL;
    if (!ctx)
        return 0;
#if defined(__APPLE__) && defined(COD2_X64)
    if (sdl_gl_window)
        SDL_SetWindowFullscreen(sdl_gl_window, 0);
#endif
    if (ctx->context) {
        SDL_GL_MakeCurrent(sdl_gl_window, ctx->context);
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
        if (presentFence) {
            glDeleteSync(presentFence);
            presentFence = NULL;
        }
#endif
#if defined(COD2_X64)
        GammaRelease(&ctx->gamma);
#endif
        glDeleteFramebuffersEXT(1, &ctx->framebuffer);
        glDeleteRenderbuffersEXT(3, ctx->color);
        glDeleteRenderbuffersEXT(1, &ctx->depth);
        SDL_GL_DeleteContext(ctx->context);
    }
    if (ctx == screenContext)
        screenContext = NULL;
    free(ctx);
    *reference = NULL;
    return 0;
}

void MacDisplay_ReleaseDisplay(void)
{
    MacDisplay_ReleaseContext((void **)&screenContext);
#if defined(COD2_X64)
    presentationGammaIdentity = 1;
#else
    if (gammaSaved && sdl_gl_window)
        SDL_SetWindowGammaRamp(sdl_gl_window, originalGamma[0], originalGamma[1], originalGamma[2]);
    gammaSaved = 0;
#endif
    if (sdl_gl_window)
        SDL_DestroyWindow(sdl_gl_window);
    sdl_gl_window = NULL;
    free(modes);
    modes = NULL;
    modeCount = initialized = 0;
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

int MacPlatform_GetRenderSize(int *width, int *height)
{
    *width = screenContext ? screenContext->width : sdl_gl_width;
    *height = screenContext ? screenContext->height : sdl_gl_height;
    return screenContext != NULL;
}
void MacPlatform_GetDrawableSize(int *width, int *height)
{
#if defined(__APPLE__) && defined(COD2_X64)
    if (screenContext && windowMode != MAC_WINDOWED) {
        GLint size[2];
        if (CGLGetParameter(CGLGetCurrentContext(), kCGLCPSurfaceBackingSize, size) == kCGLNoError) {
            *width = size[0]; *height = size[1];
            return;
        }
    }
#endif
    SDL_GL_GetDrawableSize(sdl_gl_window, width, height);
}
uint16_t MacDisplay_GetCurrentDimensions(int *w, int *h) { MacPlatform_GetRenderSize(w, h); return 0; }
int MacDisplay_GetCurrentDepth(void) { return 32; }
int MacDisplay_GetNumModes(void) { MacDisplay_Initialize(); return modeCount; }
const char **MacPlatform_ModeNames(void)
{
    static const char *names[128];
    static char storage[127][32];
    if (names[0])
        return names;
#if defined(__APPLE__) && defined(COD2_X64)
    const int defaults[][2] = { {640, 480}, {800, 600}, {1024, 768}, {1280, 720},
        {1920, 1080}, {2560, 1440}, {3008, 1692}, {3840, 2160}, {5120, 2880}, {6016, 3384} };
    const int defaultCount = sizeof(defaults) / sizeof(*defaults);
#else
    const int defaults[][2] = { {640, 480}, {800, 600}, {1024, 768}, {1280, 720} };
#endif
    int used = 0;
    MacDisplay_Initialize();
#if defined(__APPLE__) && defined(COD2_X64)
    for (int i = 0; i < defaultCount + modeCount && used < 127; ++i) {
        int w = i < defaultCount ? defaults[i][0] : modes[i - defaultCount].width;
        int h = i < defaultCount ? defaults[i][1] : modes[i - defaultCount].height;
#else
    for (int i = 0; i < 4 + modeCount && used < 127; ++i) {
        int w = i < 4 ? defaults[i][0] : modes[i - 4].width;
        int h = i < 4 ? defaults[i][1] : modes[i - 4].height;
#endif
        char name[32];
        snprintf(name, sizeof(name), "%dx%d", w, h);
        int duplicate = 0;
        for (int j = 0; j < used; ++j)
            duplicate |= strcmp(name, names[j]) == 0;
        if (!duplicate) {
            strcpy(storage[used], name);
            names[used] = storage[used];
            ++used;
        }
    }
    return names;
}
uint16_t MacDisplay_GetNthMode(int i, int *w, int *h, int *d, int *r)
{
    if (MacDisplay_Initialize() != 0 || i < 0 || i >= modeCount)
        return 1;
    *w = modes[i].width; *h = modes[i].height; *d = modes[i].depth; *r = modes[i].refresh;
    return 0;
}
uint16_t MacDisplay_GetCurrentMode(int *w, int *h, int *d, int *r)
{
    MacPlatform_GetRenderSize(w, h); *d = 32; *r = refreshRate;
    return 0;
}
uint16_t MacDisplay_SetMode(int w, int h, int d, int r)
{
    (void)d;
    if (w <= 0 || h <= 0)
        return 1;
    MacPlatform_ConfigureWindow(w, h, windowMode, r);
    if (sdl_gl_window && SetWindowMode() != 0)
        return 1;
#if defined(__APPLE__) && defined(COD2_X64)
    if (screenContext && windowMode != MAC_WINDOWED && SetSurfaceSize(w, h) != 0)
        return 1;
    if (screenContext && windowMode == MAC_WINDOWED)
        CGLDisable(CGLGetCurrentContext(), kCGLCESurfaceBackingSize);
#endif
    if (screenContext && (screenContext->width != w || screenContext->height != h)) {
        MacContext replacement = { 0 };
        replacement.width = w;
        replacement.height = h;
        if (CreateRenderBuffer(&replacement, screenContext->depthBits, screenContext->stencil) != 0) {
            glDeleteFramebuffersEXT(1, &replacement.framebuffer);
            glDeleteRenderbuffersEXT(3, replacement.color);
            glDeleteRenderbuffersEXT(1, &replacement.depth);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, screenContext->framebuffer);
            return 1;
        }
        glDeleteFramebuffersEXT(1, &screenContext->framebuffer);
        glDeleteRenderbuffersEXT(3, screenContext->color);
        glDeleteRenderbuffersEXT(1, &screenContext->depth);
        screenContext->framebuffer = replacement.framebuffer;
        memcpy(screenContext->color, replacement.color, sizeof(screenContext->color));
        screenContext->depth = replacement.depth;
        screenContext->width = w;
        screenContext->height = h;
        glViewport(0, 0, w, h);
    }
    return 0;
}
int MacDisplay_SetupDisplay(int w, int h) { sdl_gl_width = w; sdl_gl_height = h; return MacDisplay_Initialize(); }
int MacDisplay_IsFullscreen(void) { return screenContext && windowMode != MAC_WINDOWED; }
int MacDisplay_InWindowMode(void) { return windowMode == MAC_WINDOWED; }
int MacDisplay_IsWindowMode(void) { return MacDisplay_InWindowMode(); }
void *MacDisplay_GetMainWindow(void) { return sdl_gl_window; }
void *MacDisplay_GetMainPort(void) { return sdl_gl_window; }
void *MacDisplay_GetDeviceHandle(void) { return NULL; }
uint16_t MacDisplay_SwapContext(void *ctx) { if (ctx == screenContext) SDL_GL_SwapWindowDirect(); return 0; }
void MacDisplay_FadeIn(float duration) { (void)duration; }
void MacDisplay_FadeOut(float duration) { (void)duration; }
uint16_t MacDisplay_StopCapture(void) { return 0; }
const char *MacDisplay_GetGLVendor(void) { return (const char *)glGetString(GL_VENDOR); }
const char *MacDisplay_GetGLRenderer(void) { return (const char *)glGetString(GL_RENDERER); }
const char *MacDisplay_GetGLExtensions(void) { return (const char *)glGetString(GL_EXTENSIONS); }
int MacDisplay_IsGLExtensionSupported(const char *name)
{
    const char *extensions = MacDisplay_GetGLExtensions();
    if (!extensions || !name || !*name || strchr(name, ' '))
        return 0;
    size_t n = strlen(name);
    for (const char *p = extensions; (p = strstr(p, name)); p += n)
        if ((p == extensions || p[-1] == ' ') && (!p[n] || p[n] == ' '))
            return 1;
    return 0;
}
int MacDisplay_GetCardType(void) { return 0; }
void MacDisplay_GetVideoMemoryInfo(int *video, int *texture)
{
    /* Reserve 1/8 of unified memory, capped below the renderer's signed limit. */
    uint64_t budget = MacSystem_MemoryBytes() / 8;
    if (budget > 1024ULL * 1024 * 1024)
        budget = 1024ULL * 1024 * 1024;
    *video = (int)(budget >> 20);
    *texture = (int)budget;
}
int MacDisplay_GetMaxTextureUnits(void) { GLint n = 0; glGetIntegerv(GL_MAX_TEXTURE_UNITS, &n); return n; }
int MacDisplay_GetMaxTextureImageUnits(void) { GLint n = 0; glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &n); return n; }
uint32_t MacDisplay_GetPCPixelShaderVersion(void) { return 0xffff0200u; }
int MacDisplay_GetSupportsSeparateBlendFunc(void) { return MacDisplay_IsGLExtensionSupported("GL_EXT_blend_func_separate"); }
int MacDisplay_GetSupportsAnisotropicFiltering(void) { return MacDisplay_IsGLExtensionSupported("GL_EXT_texture_filter_anisotropic"); }
float MacDisplay_GetMaxSupportedAnisotropy(void) { GLfloat n = 1; if (MacDisplay_GetSupportsAnisotropicFiltering()) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &n); return n; }
uint16_t MacDisplay_GetAntiAliasingMultiSampleInfo(int *buffers, int *samples, unsigned char *super, unsigned char *multi, unsigned char *alpha)
{
    *buffers = *samples = 0; *super = *multi = *alpha = 0;
    return 0;
}
uint16_t MacDisplay_SetGammaRamp(const unsigned short *ramp)
{
#if defined(COD2_X64)
    if (!ramp)
        return 1;
    memcpy(presentationGammaRamp, ramp, sizeof(presentationGammaRamp));
    presentationGammaIdentity = 1;
    for (int channel = 0; channel < 3; ++channel)
        for (int i = 0; i < 256; ++i)
            if (ramp[channel * 256 + i] != i * 257)
                presentationGammaIdentity = 0;
    ++presentationGammaRevision;
    return 0;
#else
    if (!sdl_gl_window || windowMode == MAC_WINDOWED)
        return 0;
    if (!gammaSaved)
        gammaSaved = SDL_GetWindowGammaRamp(sdl_gl_window, originalGamma[0], originalGamma[1], originalGamma[2]) == 0;
    return (uint16_t)(SDL_SetWindowGammaRamp(sdl_gl_window, ramp, ramp + 256, ramp + 512) != 0);
#endif
}

/* AGL entry points used by the reconstructed renderer, backed by SDL contexts. */
void *aglChoosePixelFormat(void *devices, int count, const int *attributes) { (void)devices; (void)count; (void)attributes; return malloc(1); }
void aglDestroyPixelFormat(void *pixelFormat) { free(pixelFormat); }
void *aglCreateContext(void *pixelFormat, void *share)
{
    (void)pixelFormat;
    if (share && SDL_GL_MakeCurrent(sdl_gl_window, share) != 0)
        return NULL;
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, share != NULL);
    void *ctx = SDL_GL_CreateContext(sdl_gl_window);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    return ctx;
}
int aglDestroyContext(void *ctx) { if (ctx) SDL_GL_DeleteContext(ctx); return 1; }
int aglSetCurrentContext(void *ctx) { return SDL_GL_MakeCurrent(sdl_gl_window, ctx) == 0; }
int aglSetDrawable(void *ctx, void *drawable) { return SDL_GL_MakeCurrent(drawable ? drawable : sdl_gl_window, ctx) == 0; }
void *aglGetDrawable(void *ctx) { (void)ctx; return sdl_gl_window; }
int aglUpdateContext(void *ctx) { return aglSetCurrentContext(ctx); }
void aglSwapBuffers(void *ctx) { if (screenContext && ctx == screenContext->context) SDL_GL_SwapWindowDirect(); else if (sdl_gl_window) SDL_GL_SwapWindow(sdl_gl_window); }
int aglSetFullScreen(void *ctx, int w, int h, int rate, int device) { (void)ctx; (void)device; MacPlatform_ConfigureWindow(w, h, MAC_FULLSCREEN, rate); return SetWindowMode() == 0; }
int aglSetInteger(void *ctx, unsigned int name, const int *values) { if (!values || name != 222 || !aglSetCurrentContext(ctx)) return 0; return SDL_GL_SetSwapInterval(*values) == 0; }
int aglGetInteger(void *ctx, unsigned int name, int *values) { if (!values || name != 222 || !aglSetCurrentContext(ctx)) return 0; *values = SDL_GL_GetSwapInterval(); return 1; }
int aglGetError(void) { return *SDL_GetError() ? 1 : 0; }
const char *aglErrorString(int error) { return error ? SDL_GetError() : "no error"; }
void glVertexArrayParameteriAPPLE(GLenum name, GLint value) { (void)name; (void)value; }
