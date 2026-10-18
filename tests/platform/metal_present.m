/* Real IOSurface/CGL/Metal test; all images are synthetic, including gamma. */
#include "../../src/platform/macos_metal_present.m"
#include "../../src/platform/macos_display.c"
#include <assert.h>

static void StopWorker(MacMetalPresenter *p)
{
    atomic_store(&p->stopping, true);
    dispatch_semaphore_signal(p->wake);
    pthread_join(p->thread, NULL);
    p->threadStarted = 0;
    dispatch_group_wait(p->inFlight, DISPATCH_TIME_FOREVER);
}

static void CheckPixels(MacMetalPresenter *p, MacMetalSlot *slot, int gamma)
{
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                       width:p->width height:p->height mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> target = [p->device newTextureWithDescriptor:descriptor];
    id<MTLCommandBuffer> command = [p->queue commandBuffer];
    assert(EncodePresentation(p, slot, target, command));
    [command commit];
    [command waitUntilCompleted];
    assert(command.status == MTLCommandBufferStatusCompleted);
    unsigned char pixel[4];
    for (int y = 0; y < p->height; y += p->height - 1) {
        [target getBytes:pixel bytesPerRow:4 fromRegion:MTLRegionMake2D(0, y, 1, 1) mipmapLevel:0];
        if (gamma) {
            assert(pixel[2] == 255 && pixel[1] == 0 && pixel[0] == 128);
        } else {
            /* Top GL half is blue; Metal's top row must be blue too. */
            assert(pixel[0] == (y == 0 ? 255 : 0));
            assert(pixel[1] == 0 && pixel[2] == (y == 0 ? 0 : 255));
        }
        assert(pixel[3] == 255);
    }
}

#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
static void CompleteMailboxTest(void *context, uint64_t value, int success)
{
    (void)value;
    NativePresentCompletion *producer = context;
    int token = (int)(producer->slot - producer->presenter->slots);
    MacMetal_CompleteNativeFrame(producer->presenter, token, success);
    free(producer);
}
#endif

static unsigned int completionSamples;
static void ObserveNativeCompletion(int kind, double stamp, double target, uint64_t sequence)
{
    (void)target;
    assert(kind == 3 && stamp > 0 && sequence == 42);
    ++completionSamples;
}

static void CheckNativeMailbox(SDL_Window *window)
{
    MacMetalPresenter *p = MacMetal_CreateNative(window, 320, 240, 0);
    assert(p && !p->usesGL);
    StopWorker(p);
    atomic_store(&p->stopping, false);
    atomic_store(&p->admission->pending, 17);
    atomic_store(&p->slots[0].sequence, 18);
    atomic_store(&p->slots[0].state, SLOT_READY);
    uint64_t submitted = MacMetalGPU_Submitted(p->gpu);
    /* Busy presentation never queues a wait or consumes the newest mailbox. */
    PresentDrawable(p, (id<CAMetalDrawable>)[NSObject new]);
    assert(atomic_load(&p->slots[0].state) == SLOT_READY);
    assert(MacMetalGPU_Submitted(p->gpu) == submitted);
    atomic_store(&p->admission->pending, 0);
    atomic_store(&p->slots[0].state, SLOT_FREE);
    atomic_store(&p->slots[0].sequence, 0);
    atomic_store(&p->stopping, true);
    int token;
    id<MTLTexture> source = (__bridge id<MTLTexture>)MacMetal_BeginNativeFrame(p, &token);
    assert(source && token >= 0);
    size_t count = 320 * 240 * 4;
    unsigned char *pixels = malloc(count), *output = malloc(count);
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) {
            int i = (y * 320 + x) * 4;
            pixels[i] = (x + y) & 255;
            pixels[i + 1] = y & 255;
            pixels[i + 2] = x & 255;
            pixels[i + 3] = 255;
        }
    [source replaceRegion:MTLRegionMake2D(0, 0, 320, 240) mipmapLevel:0 withBytes:pixels bytesPerRow:320 * 4];
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
    assert(MacMetal_SealNativeFrame(p, token, NULL, 0, 1));
    assert(atomic_load(&p->slots[token].state) == SLOT_NATIVE_PENDING);
    MacMetalSubmission *rendered = MacMetalGPU_Begin(p->gpu, 0);
    assert(rendered);
    NativePresentCompletion *producer = calloc(1, sizeof(*producer));
    *producer = (NativePresentCompletion){ p, &p->slots[token] };
    assert(MacMetalGPU_Commit(rendered, NULL, CompleteMailboxTest, producer));
    assert(MacMetalGPU_Drain(p->gpu));
#else
    id<MTLCommandBuffer> rendered = [p->queue commandBuffer];
    MacMetal_EndNativeFrame(p, token, (__bridge void *)rendered, NULL, 0, 1);
    assert(atomic_load(&p->slots[token].state) == SLOT_NATIVE_PENDING);
    [rendered commit]; [rendered waitUntilCompleted];
#endif
    dispatch_group_wait(p->inFlight, DISPATCH_TIME_FOREVER);
    assert(atomic_load(&p->slots[token].state) == SLOT_READY);
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                       width:320 height:240 mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> target = [p->device newTextureWithDescriptor:descriptor];
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
    MacMetalSubmission *command = MacMetalGPU_Begin(p->gpu, 1);
    assert(command && EncodeNativePresentation(p, &p->slots[token], target, command));
    assert(MacMetalGPU_Commit(command, NULL, NULL, NULL));
    assert(MacMetalGPU_Drain(p->gpu));
#else
    id<MTLCommandBuffer> command = [p->queue commandBuffer];
    assert(EncodePresentation(p, &p->slots[token], target, command));
    [command commit]; [command waitUntilCompleted];
#endif
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
    p->observe = ObserveNativeCompletion;
    atomic_store(&p->slots[token].sequence, 42);
    atomic_store(&p->admission->pending, 42);
    NativePresentCompletion *receipt = calloc(1, sizeof(*receipt));
    *receipt = (NativePresentCompletion){ p, &p->slots[token] };
    dispatch_group_enter(p->inFlight);
    command = MacMetalGPU_Begin(p->gpu, 1);
    assert(command && MacMetalGPU_Commit(command, NULL, CompletePresentation, receipt));
    assert(MacMetalGPU_Drain(p->gpu));
    assert(completionSamples == 1);
    assert(atomic_load(&p->admission->pending) == 0);
    p->observe = NULL;
#endif
    [target getBytes:output bytesPerRow:320 * 4 fromRegion:MTLRegionMake2D(0, 0, 320, 240) mipmapLevel:0];
    assert(memcmp(pixels, output, count) == 0);
    free(pixels); free(output);
    int pending;
    assert(MacMetal_BeginNativeFrame(p, &pending));
    assert(MacMetal_SealNativeFrame(p, pending, NULL, 0, 1));
    assert(atomic_load(&p->slots[pending].state) == SLOT_NATIVE_PENDING);
    MacMetal_CompleteNativeFrame(p, pending, 0);
    assert(atomic_load(&p->slots[pending].state) == SLOT_FREE);
    MacMetal_CompleteNativeFrame(p, pending, 0); /* Duplicate notification is harmless. */
    assert(MacMetal_BeginNativeFrame(p, &pending));
    MacMetal_EndNativeFrame(p, pending, NULL, NULL, 0, 1);
    assert(atomic_load(&p->slots[pending].state) == SLOT_FREE);
#if defined(__APPLE__) && defined(COD2_X64) && defined(__aarch64__)
    MacMetalGPU *shared = MacMetalGPU_Retain(MacMetal_NativeGPU(p));
    uint64_t previous = MacMetalGPU_Submitted(shared);
    MacMetal_Destroy(p);
    p = MacMetal_CreateNativeWithGPU(window, 640, 480, 0, shared);
    assert(p && MacMetal_NativeGPU(p) == shared && MacMetal_Device(p) == MacMetalGPU_Device(shared));
    StopWorker(p);
    MacMetalGPU_Destroy(shared); /* Presenter owns the same live transport. */
    command = MacMetalGPU_Begin(p->gpu, 0);
    assert(command && MacMetalGPU_Commit(command, NULL, NULL, NULL) > previous);
    assert(MacMetalGPU_Drain(p->gpu));
#endif
    MacMetal_Destroy(p);
}

static void CheckGLGammaParity(MacMetalPresenter *p, MacMetalSlot *slot)
{
    size_t count = p->width * p->height * 4;
    unsigned char *pixels = malloc(count), *glOutput = malloc(count), *metalOutput = malloc(count);
    for (int y = 0; y < p->height; ++y)
        for (int x = 0; x < p->width; ++x) {
            int i = (y * p->width + x) * 4;
            pixels[i] = x & 255; pixels[i + 1] = y & 255;
            pixels[i + 2] = (x + y) & 255; pixels[i + 3] = 255;
        }
    glWindowPos2i(0, 0);
    glDrawPixels(p->width, p->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glFinish();
    unsigned short ramp[768];
    for (int i = 0; i < 256; ++i) {
        ramp[i] = i * i;
        ramp[256 + i] = 65535 - i * 257;
        ramp[512 + i] = (i * 7919) & 65535;
    }
    MacDisplay_SetGammaRamp(ramp);
    SetSlotGamma(slot, ramp, presentationGammaRevision, 0);
    GLuint target, framebuffer;
    glGenTextures(1, &target);
    glBindTexture(GL_TEXTURE_2D, target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, p->width, p->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    assert(glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    assert(GammaPresent(&screenContext->gamma, p->width, p->height, 0, 0, p->width, p->height));
    glPopAttrib();
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glReadPixels(0, 0, p->width, p->height, GL_RGBA, GL_UNSIGNED_BYTE, glOutput);
    MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                       width:p->width height:p->height mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> output = [p->device newTextureWithDescriptor:descriptor];
    id<MTLCommandBuffer> command = [p->queue commandBuffer];
    assert(EncodePresentation(p, slot, output, command));
    [command commit]; [command waitUntilCompleted];
    [output getBytes:metalOutput bytesPerRow:p->width * 4 fromRegion:MTLRegionMake2D(0, 0, p->width, p->height) mipmapLevel:0];
    for (int y = 0; y < p->height; ++y)
        for (int x = 0; x < p->width; ++x) {
            int a = (y * p->width + x) * 4;
            int b = ((p->height - 1 - y) * p->width + x) * 4;
            assert(glOutput[a] == metalOutput[b + 2]);
            assert(glOutput[a + 1] == metalOutput[b + 1]);
            assert(glOutput[a + 2] == metalOutput[b]);
            assert(glOutput[a + 3] == metalOutput[b + 3]);
        }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, screenContext->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0_EXT); glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glDeleteFramebuffersEXT(1, &framebuffer); glDeleteTextures(1, &target);
    free(pixels); free(glOutput); free(metalOutput);
}

static void CheckFullscreenMechanics(void)
{
    CGDisplayModeRef before = CGDisplayCopyDisplayMode(CGMainDisplayID());
    MacPlatform_ConfigurePresentation(1, 0);
    MacPlatform_ConfigureWindow(1920, 1080, MAC_FULLSCREEN, 60);
    void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
    assert(context && screenContext->metal);
    SDL_PumpEvents();
    printf("Metal fullscreen request: flags=0x%x Spaces=%s\n", SDL_GetWindowFlags(sdl_gl_window),
           SDL_GetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES));
    assert(screenContext->metal->layer.drawableSize.width == 1920);
    assert(screenContext->metal->layer.drawableSize.height == 1080);
    glClearColor(0, 1, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4];
    glReadPixels(1919, 1079, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0);
    SDL_MinimizeWindow(sdl_gl_window); SDL_PumpEvents();
    glClear(GL_COLOR_BUFFER_BIT); SDL_GL_SwapWindowDirect();
    SDL_RestoreWindow(sdl_gl_window); SDL_RaiseWindow(sdl_gl_window); SDL_PumpEvents();
    assert(MacDisplay_SetMode(2560, 1440, 32, 60) == 0);
    assert(screenContext->metal->layer.drawableSize.width == 2560);
    assert(screenContext->metal->layer.drawableSize.height == 1440);
    MacPlatform_ConfigureWindow(800, 600, MAC_WINDOWED, 60);
    assert(MacDisplay_SetMode(800, 600, 32, 60) == 0);
    assert(screenContext->metal->layer.drawableSize.width == 800);
    MacDisplay_ReleaseContext(&context);
    assert(!(SDL_GetWindowFlags(sdl_gl_window) & SDL_WINDOW_FULLSCREEN));
    MacDisplay_ReleaseDisplay();
    CGDisplayModeRef after = CGDisplayCopyDisplayMode(CGMainDisplayID());
    assert(CGDisplayModeGetWidth(before) == CGDisplayModeGetWidth(after));
    assert(CGDisplayModeGetHeight(before) == CGDisplayModeGetHeight(after));
    assert(CGDisplayModeGetPixelWidth(before) == CGDisplayModeGetPixelWidth(after));
    assert(CGDisplayModeGetPixelHeight(before) == CGDisplayModeGetPixelHeight(after));
    CGDisplayModeRelease(before); CGDisplayModeRelease(after);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc > 1 && !strcmp(argv[1], "--fullscreen")) {
            CheckFullscreenMechanics();
            puts("Metal fullscreen request, selected backing, minimize/restore, resize and display restoration: passed");
            return 0;
        }
        MacPlatform_ConfigurePresentation(1, 0);
        MacPlatform_ConfigureWindow(320, 240, MAC_WINDOWED, 60);
        void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
        assert(context && screenContext->metal);
        MacMetalPresenter *p = screenContext->metal;
        StopWorker(p);
        assert(p->layer.maximumDrawableCount == 2 && p->layer.allowsNextDrawableTimeout);
        assert(!p->layer.presentsWithTransaction && !p->layer.displaySyncEnabled);
        assert(!p->layer.wantsExtendedDynamicRangeContent && p->layer.colorspace == nil);
        assert(p->layer.drawableSize.width == 320 && p->layer.drawableSize.height == 240);
        MacMetalSlot *slot = &p->slots[p->current];
        glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 120, 320, 120);
        glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        MacMetal_EndFrame(p, NULL, 0, 1);
        glFinish();
        MacMetal_BeginFrame(p);
        assert(atomic_load(&slot->state) == SLOT_READY);
        CheckPixels(p, slot, 0);
        for (int i = 0; i < 256; ++i) {
            slot->ramp[i] = 1;
            slot->ramp[256 + i] = 0;
            slot->ramp[512 + i] = 32768.0f / 65535;
        }
        slot->identity = 0;
        CheckPixels(p, slot, 1);
        CheckGLGammaParity(p, slot);

        /* With every transfer slot busy, the client still renders into its
         * private scratch target. Newest READY is retained under occlusion. */
        for (int i = 0; i < SLOT_COUNT; ++i)
            atomic_store(&p->slots[i].state, SLOT_METAL);
        assert(MacMetal_BeginFrame(p) == 0);
        atomic_store(&p->slots[0].sequence, 100);
        atomic_store(&p->slots[0].state, SLOT_READY);
        assert(MacMetal_BeginFrame(p) == 0);
        atomic_store(&p->slots[1].sequence, 99);
        atomic_store(&p->slots[1].state, SLOT_READY);
        assert(MacMetal_BeginFrame(p) == p->slots[1].glTexture);
        assert(atomic_load(&p->slots[0].state) == SLOT_READY);
        atomic_store(&p->submittedSequence, 100);
        assert(LatestReady(p) == -1);
        for (int i = 0; i < SLOT_COUNT; ++i)
            atomic_store(&p->slots[i].state, SLOT_FREE);
        assert(MacDisplay_SetMode(640, 480, 32, 60) == 0);
        assert(screenContext->metal && screenContext->width == 640 && screenContext->height == 480);
        assert(screenContext->metal->layer.drawableSize.width == 640);
        SDL_HideWindow(sdl_gl_window);
        for (int i = 0; i < 100; ++i) {
            glClear(GL_COLOR_BUFFER_BIT);
            SDL_GL_SwapWindowDirect();
        }
        assert(glGetError() == GL_NO_ERROR);
        MacDisplay_ReleaseContext(&context);
        CheckNativeMailbox(sdl_gl_window);
        MacDisplay_ReleaseDisplay();
        MacPlatform_ConfigurePresentation(1, 1);
        MacPlatform_ConfigureWindow(320, 240, MAC_WINDOWED, 60);
        context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
        assert(context && screenContext->metal);
        assert(screenContext->metal->layer.pixelFormat == MTLPixelFormatRGBA16Float);
        assert(screenContext->metal->layer.wantsExtendedDynamicRangeContent);
        /* Resource-failure fallback after a CGL drawable was cleared. */
        MacMetal_Destroy(screenContext->metal);
        screenContext->metal = NULL;
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, screenContext->framebuffer);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                    GL_RENDERBUFFER_EXT, screenContext->color[0]);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
        assert(glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, screenContext->framebuffer);
        glClearColor(0, 1, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindowDirect();
        assert(glGetError() == GL_NO_ERROR);
        MacDisplay_ReleaseContext(&context);
        MacDisplay_ReleaseDisplay();
        puts("Metal IOSurface pixels, origin, gamma, slot pressure, hidden window, resize and EDR: passed");
    }
    return 0;
}
