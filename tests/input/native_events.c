/* SDL3 has pointer text and floating wheel fields; keep its ABI in a separate TU. */
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static void *library;
static bool (*push)(SDL_Event *);
static Uint64 (*ticks)(void);
static SDL_Window *(*focus)(void);
static SDL_WindowID (*windowID)(SDL_Window *);
static char *texts[256];
static int textCount;

void TestInput_ReleaseText(void)
{
    for (int n = 0; n < textCount; ++n) free(texts[n]);
    textCount = 0;
}

static int initialize(const char *path)
{
    if (!library) {
        /* Must use the SDL3 already loaded by sdl2-compat, never a second SDL. */
        library = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
        if (!library) return 0;
        push = dlsym(library, "SDL_PushEvent");
        ticks = dlsym(library, "SDL_GetTicksNS");
        focus = dlsym(library, "SDL_GetKeyboardFocus");
        windowID = dlsym(library, "SDL_GetWindowID");
    }
    return push && ticks && focus && windowID;
}

int TestInput_NativeText(const char *path, const char *text)
{
    if (!initialize(path)) return -1;
    if (textCount == 256) return -3;
    char *retained = strdup(text);
    if (!retained) return -1;
    SDL_Event event = {0};
    SDL_Window *window = focus();
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.timestamp = ticks();
    event.text.windowID = window ? windowID(window) : 0;
    event.text.text = retained;
    if (!push(&event)) { free(retained); return 0; }
    /* sdl2-compat converts pointer text when peeping, after this function returns. */
    texts[textCount++] = retained;
    return 1;
}

int TestInput_NativeWheel(const char *path, float x, float y, int flipped)
{
    if (!initialize(path)) return -1;
    SDL_Event event = {0};
    SDL_Window *window = focus();
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = ticks();
    event.wheel.windowID = window ? windowID(window) : 0;
    event.wheel.x = x;
    event.wheel.y = y;
    event.wheel.direction = flipped ? SDL_MOUSEWHEEL_FLIPPED : SDL_MOUSEWHEEL_NORMAL;
    return push(&event) ? 1 : 0;
}
