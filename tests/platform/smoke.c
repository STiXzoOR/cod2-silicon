#include "macos_display.h"
#include "macos_rawmouse.h"
#include "macos_audio.h"
#include "macos_system.h"
#include <SDL2/SDL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static void CheckUDP(void)
{
    int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
    assert(socketFD >= 0);
    struct sockaddr_in address = { 0 };
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(socketFD, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t length = sizeof(address);
    assert(getsockname(socketFD, (struct sockaddr *)&address, &length) == 0);
    const char payload[] = "native UDP";
    assert(sendto(socketFD, payload, sizeof(payload), 0, (struct sockaddr *)&address, length) == sizeof(payload));
    char received[32];
    assert(recv(socketFD, received, sizeof(received), 0) == sizeof(payload));
    assert(memcmp(payload, received, sizeof(payload)) == 0);
    close(socketFD);
    printf("UDP loopback: passed\n");
}

int main(int argc, char **argv)
{
    int mode = argc > 1 && strcmp(argv[1], "--borderless") == 0 ? MAC_BORDERLESS :
               (argc > 1 && strcmp(argv[1], "--fullscreen") == 0 ? MAC_FULLSCREEN : MAC_WINDOWED);
    MacPlatform_ConfigureWindow(640, 480, mode, 60);
    void *context = MacDisplay_CreateScreenContext(24, 1, 0, 0, 0, NULL);
    if (!context) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    printf("GL_VERSION=%s\nGL_RENDERER=%s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    SDL_version version;
    SDL_GetVersion(&version);
    printf("SDL2 runtime=%d.%d.%d driver=%s swap_interval=%d\n", version.major, version.minor, version.patch, SDL_GetCurrentVideoDriver(), SDL_GL_GetSwapInterval());
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    const char *required[] = { "GL_ARB_vertex_program", "GL_ARB_fragment_program", "GL_APPLE_fence",
        "GL_APPLE_vertex_array_object", "GL_EXT_texture_compression_s3tc", "GL_EXT_blend_func_separate",
        "GL_EXT_framebuffer_object", "GL_ARB_multitexture", "GL_ARB_texture_env_combine", "GL_EXT_framebuffer_blit" };
    for (size_t i = 0; i < sizeof(required) / sizeof(*required); ++i) {
        int found = strstr(extensions, required[i]) != NULL;
        printf("%s=%s\n", required[i], found ? "yes" : "NO");
        assert(found);
    }
    printf("GL_APPLE_vertex_array_range=%s (authorized no-op)\n", strstr(extensions, "GL_APPLE_vertex_array_range") ? "yes" : "no");
    int renderW, renderH, drawableW, drawableH;
    MacPlatform_GetRenderSize(&renderW, &renderH);
    MacPlatform_GetDrawableSize(&drawableW, &drawableH);
    GLint viewport[4], attachmentW;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_WIDTH_EXT, &attachmentW);
    printf("mode=%d render=%dx%d drawable=%dx%d viewport=%dx%d attachment_width=%d\n", mode, renderW, renderH, drawableW, drawableH, viewport[2], viewport[3], attachmentW);
    assert(renderW == 640 && renderH == 480 && viewport[2] == 640 && attachmentW == 640);
    assert(SDL_GL_GetSwapInterval() == 0);
    glClearColor(0.125f, 0.5f, 0.875f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    unsigned char pixel[4];
    glReadPixels(20, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] >= 31 && pixel[0] <= 33 && pixel[1] >= 127 && pixel[1] <= 129);
    /* The renderer uses AUX0 for blur/glow and AUX1 for dynamic shadows. */
    MacGL_DrawBuffer(GL_AUX0); MacGL_ReadBuffer(GL_AUX0);
    glClearColor(0, 1, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    assert(glGetError() == GL_NO_ERROR);
    MacGL_DrawBuffer(GL_AUX1); MacGL_ReadBuffer(GL_AUX1);
    glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindowDirect();
    glReadPixels(20, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 255 && pixel[1] == 0);
    MacGL_ReadBuffer(GL_AUX0);
    glReadPixels(20, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0 && pixel[1] == 255);
    MacGL_DrawBuffer(GL_BACK); MacGL_ReadBuffer(GL_BACK);
    glReadPixels(20, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] >= 31 && pixel[0] <= 33 && pixel[1] >= 127 && pixel[1] <= 129);
    puts("auxiliary render buffer isolation: passed");
    int rx, ry;
    MacPlatform_TransformPoint(240, 540, 1920, 1080, 3840, 2160, &rx, &ry);
    assert(rx == 0 && ry == 240);
    MacPlatform_TransformPoint(960, 540, 1920, 1080, 3840, 2160, &rx, &ry);
    assert(rx == 320 && ry == 240);
    MacPlatform_TransformPoint(1900, 540, 1920, 1080, 3840, 2160, &rx, &ry);
    assert(rx == 639 && ry == 240);
    puts("Retina letterbox inverse input transform: passed");
    CheckUDP();
    MacRawMouse_Init();
    MacRawMouse_SetActive(1);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    printf("GCMouse devices=%d; capturing raw and SDL deltas for 3 seconds\n", MacRawMouse_Available());
    unsigned long sdlEvents = 0;
    long rawX = 0, rawY = 0, sdlX = 0, sdlY = 0;
    uint64_t end = MacSystem_Nanoseconds() + 3000000000ull;
    while (MacSystem_Nanoseconds() < end) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_MOUSEMOTION) {
                ++sdlEvents; sdlX += event.motion.xrel; sdlY += event.motion.yrel;
            }
        }
        int dx, dy;
        MacRawMouse_Read(&dx, &dy);
        rawX += dx; rawY += dy;
        SDL_GL_SwapWindowDirect();
        assert(glGetError() == GL_NO_ERROR);
        SDL_Delay(4);
    }
    printf("GCMouse events=%llu deltas=%ld,%ld; SDL events=%lu deltas=%ld,%ld\n", (unsigned long long)MacRawMouse_EventCount(), rawX, rawY, sdlEvents, sdlX, sdlY);
    /* Reset changes the FBO size while keeping GL object/context ownership. */
    assert(MacDisplay_SetMode(800, 600, 32, 60) == 0);
    MacPlatform_GetRenderSize(&renderW, &renderH);
    glGetIntegerv(GL_VIEWPORT, viewport);
    assert(renderW == 800 && renderH == 600 && viewport[2] == 800 && viewport[3] == 600);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindowDirect();
    assert(glGetError() == GL_NO_ERROR);
    puts("resolution reset: passed");
    MacRawMouse_Shutdown();
    SDL_SetRelativeMouseMode(SDL_FALSE);
    assert(MacAudio_PlayTone(0.25));
    MacAudioStats stats;
    MacAudio_GetStats(&stats);
    printf("AudioUnit callbacks=%llu frames=%llu audible_frames=%llu peak=%.6f\n", (unsigned long long)stats.callbacks, (unsigned long long)stats.frames, (unsigned long long)stats.audible_frames, stats.peak);
    assert(stats.callbacks && stats.audible_frames && stats.peak > 0);
    MacDisplay_ReleaseContext(&context);
    assert(context == NULL);
    MacDisplay_ReleaseDisplay();
    puts("platform smoke: passed");
    return 0;
}
