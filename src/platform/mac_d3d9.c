/* The Mac client's Direct3D 9 path on Metal (COD2_MAC_D3D9).
 *
 * The renderer calls the device through its COM vtables, exactly as the Windows
 * client does with the system d3d9.dll (src/win32/win_d3d9.c). Here the device
 * comes from libd3d9metal (mtld3d's native host build), a Direct3D 9
 * implementation on Metal. This file is the Mac half of that seam: the window
 * and its CAMetalLayer, application memory, the presentation parameters, shader
 * compilation with vkd3d-shader, constant tables and screenshot readback.
 *
 * It keeps the Win_D3D9_ names so the renderer's COD2_WIN32_D3D9 sites are
 * shared, and it includes no engine headers: every Direct3D layout here is the
 * 64-bit Windows one, which libd3d9metal uses too. */
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <vkd3d_shader.h>

#include "d3d9metal.h"
#include "platform/macos_display.h"

typedef int32_t HRESULT;
#define S_OK 0
#define E_NOTIMPL ((HRESULT)0x80004001)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_FAIL ((HRESULT)0x80004005)
#define E_OUTOFMEMORY ((HRESULT)0x8007000e)
#define D3DERR_INVALIDCALL ((HRESULT)0x8876086c)

extern SDL_Window *sdl_gl_window;
extern int MacDisplay_Initialize(void);
extern void *MacDisplay_CreateD3D9Window(void);
extern void *MacD3D9View_Layer(SDL_Window *window, int renderWidth, int renderHeight);
extern const void *Dvar_RegisterBool(const char *name, unsigned char value, unsigned short flags);
extern unsigned char Dvar_GetBool(const char *name);

/* The window's Metal layer and the back buffer it is fitted to. */
static void *metalLayer;
static uint32_t layerWidth, layerHeight;

/* The host table. The window handle is the SDL window, and a default-size back
 * buffer is the size the layer was last fitted to. */
static void *Host_MetalLayer(void *user, void *hwnd, uint32_t *width, uint32_t *height)
{
    (void)user;
    if (!hwnd || !metalLayer)
        return NULL;
    *width = layerWidth;
    *height = layerHeight;
    return metalLayer;
}

static void *Host_Alloc(void *user, size_t size, size_t align)
{
    void *block = NULL;
    (void)user;
    if (align < sizeof(void *))
        align = sizeof(void *);
    return posix_memalign(&block, align, size) == 0 ? block : NULL;
}

static void Host_Free(void *user, void *ptr, size_t size)
{
    (void)user;
    (void)size;
    free(ptr);
}

void *Win_D3D9_Direct3DCreate9(void)
{
    static int installed;
    if (!installed) {
        static D3D9MetalHost host;
        static char cache[1024];
        const char *home = getenv("HOME");
        const char *dir = getenv("COD2_D3D9_CACHE");
        if (dir)
            snprintf(cache, sizeof cache, "%s", dir);
        else
            snprintf(cache, sizeof cache, "%s/Library/Caches/cod2-d3d9metal", home ? home : "/tmp");
        {
            /* The library keeps its compiled shaders and pipelines here and builds them
             * again before the first frame of the next launch. It does not make the
             * folder, and without one it keeps nothing: every launch would then compile
             * each shader the first time something is drawn with it. */
            char *slash;
            for (slash = strchr(cache + 1, '/'); slash; slash = strchr(slash + 1, '/')) {
                *slash = '\0';
                mkdir(cache, 0755);
                *slash = '/';
            }
            mkdir(cache, 0755);
        }
        {
            /* The library's switches (its mtld3d.conf). It presents in HDR where the
             * display can; the game's picture is not made for that and is shown as it
             * is. Entries are read in order and a later one wins, so what a developer
             * puts in MTLD3D_CONFIG still holds. */
            static char config[2048];
            const char *given = getenv("MTLD3D_CONFIG");
            snprintf(config, sizeof config, "color.hdr.enable=false%s%s", given && *given ? ";" : "",
                     given ? given : "");
            setenv("MTLD3D_CONFIG", config, 1);
        }
        host.size = (uint32_t)sizeof host;
        host.metal_layer = Host_MetalLayer;
        host.alloc_app = Host_Alloc;
        host.free_app = Host_Free;
        host.cache_dir = cache;
        int result = d3d9metal_init(&host);
        if (result != D3D9METAL_OK && result != D3D9METAL_E_ALREADY) {
            fprintf(stderr, "d3d9metal_init failed: %d\n", result);
            return NULL;
        }
        installed = 1;
    }
    return d3d9metal_Direct3DCreate9(32);
}

/* The window the device presents to, in the mode the game asked for. It is the
 * client's ordinary window (SDL's, and with it SDL's input, the monitor choice,
 * vid_xpos and vid_ypos, r_borderless) with a Metal layer and no OpenGL context.
 * The window is moved or resized only when what the game asks for changes, so a
 * vid_restart with the same settings leaves it where it is. */
static void *ConfigureWindow(int width, int height, int fullscreen, int refresh)
{
    static int applied[4] = {0, 0, -1, 0};
    int mode;

    Dvar_RegisterBool("r_borderless", 1, 0x1001);
    mode = !fullscreen ? MAC_WINDOWED : Dvar_GetBool("r_borderless") ? MAC_BORDERLESS : MAC_FULLSCREEN;
    if (!fullscreen)
        refresh = 0;
    if (!sdl_gl_window || applied[0] != width || applied[1] != height || applied[2] != mode || applied[3] != refresh) {
        MacPlatform_ConfigureWindow(width, height, mode, refresh);
        if (!MacDisplay_CreateD3D9Window()) {
            fprintf(stderr, "Couldn't create the game window: %s\n", SDL_GetError());
            return NULL;
        }
        applied[0] = width;
        applied[1] = height;
        applied[2] = mode;
        applied[3] = refresh;
    }
    metalLayer = MacD3D9View_Layer(sdl_gl_window, width, height);
    if (!metalLayer) {
        fprintf(stderr, "Couldn't create the window's Metal layer\n");
        return NULL;
    }
    layerWidth = (uint32_t)width;
    layerHeight = (uint32_t)height;
    return sdl_gl_window;
}

void *Win_D3D9_PrepareWindow(int width, int height, int fullscreen, int refresh)
{
    return ConfigureWindow(width, height, fullscreen, refresh);
}

/* D3DPRESENT_PARAMETERS as 64-bit Windows lays it out: the window handle sits in
 * the middle with pointer alignment. */
extern int RB_SceneTarget_Wanted(void);

typedef struct {
    uint32_t BackBufferWidth, BackBufferHeight, BackBufferFormat, BackBufferCount;
    uint32_t MultiSampleType, MultiSampleQuality, SwapEffect;
    void *hDeviceWindow;
    uint32_t Windowed, EnableAutoDepthStencil, AutoDepthStencilFormat, Flags;
    uint32_t FullScreen_RefreshRateInHz, PresentationInterval;
} PresentParameters;
_Static_assert(sizeof(PresentParameters) == 64, "D3DPRESENT_PARAMETERS is 64 bytes");

/* r_init.c describes the presentation in fourteen 32-bit words (see
 * win_d3d9.c). Window and full screen are a matter for the window (see
 * ConfigureWindow), which libd3d9metal never touches in a host application. The
 * device is always described to it as full screen, because it applies the
 * game's gamma ramp only for a full-screen device, as Windows does, and this
 * client has always applied r_gamma in a window too. */
void Win_D3D9_PresentParameters(const int *words, void *window, void *out)
{
    PresentParameters *parameters = out;

    /* Reset describes a new mode with the same words; the window follows it. */
    ConfigureWindow(words[0], words[1], !words[7], words[12]);
    memset(parameters, 0, sizeof *parameters);
    parameters->BackBufferWidth = (uint32_t)words[0];
    parameters->BackBufferHeight = (uint32_t)words[1];
    parameters->BackBufferFormat = (uint32_t)words[2];
    parameters->BackBufferCount = (uint32_t)words[3];
    /* With a scene target (rb_scenetarget.c) the samples live there, and the back
     * buffer has one per pixel. */
    if (!RB_SceneTarget_Wanted()) {
        parameters->MultiSampleType = (uint32_t)words[4];
        parameters->MultiSampleQuality = (uint32_t)words[5];
    }
    parameters->SwapEffect = (uint32_t)words[6];
    parameters->hDeviceWindow = window;
    parameters->Windowed = 0;
    parameters->EnableAutoDepthStencil = 1;
    parameters->AutoDepthStencilFormat = (uint32_t)words[8];
    parameters->Flags = (uint32_t)words[9];
    parameters->FullScreen_RefreshRateInHz = 0;
    parameters->PresentationInterval = (uint32_t)words[11];
}

/* A reference-counted block with ID3DBlob's and ID3DXConstantTable's first five
 * methods: QueryInterface, AddRef, Release, GetBufferPointer, GetBufferSize. */
typedef struct {
    const void *const *vtable;
    int32_t references;
    uint32_t size;
    uint8_t data[];
} Blob;

static HRESULT Blob_QueryInterface(Blob *self, const void *iid, void **out)
{
    (void)self;
    (void)iid;
    *out = NULL;
    return E_NOINTERFACE;
}
static uint32_t Blob_AddRef(Blob *self)
{
    return (uint32_t)__sync_add_and_fetch(&self->references, 1);
}
static uint32_t Blob_Release(Blob *self)
{
    int32_t references = __sync_sub_and_fetch(&self->references, 1);
    if (!references)
        free(self);
    return (uint32_t)references;
}
static void *Blob_GetBufferPointer(Blob *self)
{
    return self->data;
}
static uint32_t Blob_GetBufferSize(Blob *self)
{
    return self->size;
}
static const void *const blobVtable[] = {
    (void *)Blob_QueryInterface, (void *)Blob_AddRef,        (void *)Blob_Release,
    (void *)Blob_GetBufferPointer, (void *)Blob_GetBufferSize,
};

static Blob *Blob_Make(const void *data, size_t size)
{
    Blob *blob = malloc(sizeof *blob + size + 1);
    if (!blob)
        return NULL;
    blob->vtable = blobVtable;
    blob->references = 1;
    blob->size = (uint32_t)size;
    memcpy(blob->data, data, size);
    blob->data[size] = 0;
    return blob;
}

/* Shaders. The Windows game compiles the HLSL in materials/shaders as each
 * material loads; so does this, with vkd3d-shader, using the game's settings:
 * the macro PC, half-precision globals allowed, and the engine's own include
 * handler (the preloaded shader text, then lib/). */
extern HRESULT IncludeClass_Open(const void *self, int type, const char *name, const void *parent, const void **data,
                                 void *bytes);

static int Shader_OpenInclude(const char *filename, bool local, const char *parent_data, void *context,
                              struct vkd3d_shader_code *out)
{
    const void *data = NULL;
    int bytes = 0;
    (void)local;
    (void)context;
    if (IncludeClass_Open(NULL, 0, filename, parent_data, &data, &bytes) != 0 || !data)
        return VKD3D_ERROR;
    out->code = data;
    out->size = (size_t)bytes;
    return VKD3D_OK;
}

static void Shader_CloseInclude(const struct vkd3d_shader_code *code, void *context)
{
    (void)code;
    (void)context;
}

HRESULT Win_D3D9_CompileShader(const char *source, int length, const char *name, const char *entry, const char *target,
                               void **shader, void **messages)
{
    static const struct vkd3d_shader_macro macros[] = { { "PC", "" } };
    static const struct vkd3d_shader_compile_option options[] = {
        { VKD3D_SHADER_COMPILE_OPTION_BACKWARD_COMPATIBILITY, VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_ALLOW_HALF_GLOBALS },
    };
    struct vkd3d_shader_preprocess_info preprocess = {
        .type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO,
        .macros = macros,
        .macro_count = 1,
        .pfn_open_include = Shader_OpenInclude,
        .pfn_close_include = Shader_CloseInclude,
    };
    struct vkd3d_shader_hlsl_source_info hlsl = {
        .type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO,
        .next = &preprocess,
        .entry_point = entry,
        .profile = target,
    };
    struct vkd3d_shader_compile_info info = {
        .type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO,
        .next = &hlsl,
        .source = { source, (size_t)length },
        .source_type = VKD3D_SHADER_SOURCE_HLSL,
        .target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE,
        .options = options,
        .option_count = 1,
        .log_level = VKD3D_SHADER_LOG_ERROR,
        .source_name = name,
    };
    struct vkd3d_shader_code code = { 0 };
    char *text = NULL;
    int result;

    *shader = NULL;
    if (messages)
        *messages = NULL;
    result = vkd3d_shader_compile(&info, &code, &text);
    if (result == VKD3D_OK) {
        *shader = Blob_Make(code.code, code.size);
        vkd3d_shader_free_shader_code(&code);
        vkd3d_shader_free_messages(text);
        return *shader ? S_OK : E_OUTOFMEMORY;
    }
    if (messages && text)
        *messages = Blob_Make(text, strlen(text));
    vkd3d_shader_free_messages(text);
    return E_FAIL;
}

#define CTAB_FOURCC 0x42415443u /* "CTAB" */

HRESULT Win_D3D9_GetShaderConstantTable(const void *function, void **table)
{
    const uint32_t *token = function;
    const uint32_t *data = NULL;
    uint32_t size = 0;

    *table = NULL;
    if (!token || (*token & 0xfffe0000u) != 0xfffe0000u)
        return D3DERR_INVALIDCALL;
    /* Walk the token stream to the CTAB comment, which compilers put first. */
    for (++token; *token != 0x0000ffffu;) {
        uint32_t opcode = *token & 0xffffu;
        if (opcode == 0xfffeu) {
            uint32_t length = (*token >> 16) & 0x7fffu;
            if (length >= 1 && token[1] == CTAB_FOURCC) {
                data = token + 2;
                size = (length - 1) * 4;
                break;
            }
            token += length + 1;
        } else {
            token += ((*token >> 24) & 0xfu) + 1;
        }
    }
    if (!data || size < 28)
        return (HRESULT)0x88760b59; /* D3DXERR_INVALIDDATA */
    *table = Blob_Make(data, size);
    return *table ? S_OK : E_OUTOFMEMORY;
}

/* Screenshots: the back buffer's top left width x height pixels as rows of red,
 * green, blue and alpha bytes, top row first, before Present. The calls go
 * through the device's and the surfaces' vtables by their Direct3D 9 slots. */
typedef struct {
    uint32_t Format, Type, Usage, Pool, MultiSampleType, MultiSampleQuality, Width, Height;
} SurfaceDesc;
typedef struct {
    int32_t Pitch;
    void *pBits;
} LockedRect;

#define METHOD(object, slot, type) ((type)(*(void ***)(object))[slot])
enum { D3DFMT_A8R8G8B8 = 21, D3DFMT_X8R8G8B8 = 22, D3DPOOL_SYSTEMMEM = 2, D3DLOCK_READONLY = 0x10 };

static void Surface_Release(void *surface)
{
    if (surface)
        METHOD(surface, 2, uint32_t (*)(void *))(surface);
}

int Win_D3D9_ReadBackBuffer(void *device, int width, int height, unsigned char *rgba)
{
    void *backBuffer = NULL, *resolved = NULL, *copy = NULL;
    SurfaceDesc desc;
    LockedRect locked;
    int ok = 0;

    if (METHOD(device, 18, HRESULT (*)(void *, uint32_t, uint32_t, int, void **))(device, 0, 0, 0, &backBuffer) < 0)
        return 0;
    METHOD(backBuffer, 12, HRESULT (*)(void *, SurfaceDesc *))(backBuffer, &desc);
    if ((desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_X8R8G8B8) || (uint32_t)width > desc.Width ||
        (uint32_t)height > desc.Height)
        goto done;
    if (desc.MultiSampleType) {
        if (METHOD(device, 28, HRESULT (*)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int, void **, void *))(
                device, desc.Width, desc.Height, desc.Format, 0, 0, 0, &resolved, NULL) < 0 ||
            METHOD(device, 34, HRESULT (*)(void *, void *, const void *, void *, const void *, uint32_t))(
                device, backBuffer, NULL, resolved, NULL, 0) < 0)
            goto done;
    } else {
        resolved = backBuffer;
        METHOD(resolved, 1, uint32_t (*)(void *))(resolved);
    }
    if (METHOD(device, 36, HRESULT (*)(void *, uint32_t, uint32_t, uint32_t, uint32_t, void **, void *))(
            device, desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy, NULL) < 0 ||
        METHOD(device, 32, HRESULT (*)(void *, void *, void *))(device, resolved, copy) < 0 ||
        METHOD(copy, 13, HRESULT (*)(void *, LockedRect *, const void *, uint32_t))(copy, &locked, NULL, D3DLOCK_READONLY) < 0)
        goto done;
    for (int y = 0; y < height; ++y) {
        const unsigned char *src = (const unsigned char *)locked.pBits + (size_t)y * (size_t)locked.Pitch;
        unsigned char *dst = rgba + (size_t)y * (size_t)width * 4;
        for (int x = 0; x < width; ++x, src += 4, dst += 4) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = desc.Format == D3DFMT_X8R8G8B8 ? 0xff : src[3];
        }
    }
    METHOD(copy, 14, HRESULT (*)(void *))(copy);
    ok = 1;
done:
    Surface_Release(copy);
    Surface_Release(resolved);
    Surface_Release(backBuffer);
    return ok;
}

/* Stand-ins for the OpenGL fences the backend paces itself with (see
 * src/win32/gl_loader.c): a fence is always finished. */
void MacD3D9_GenFences(int count, unsigned int *fences)
{
    for (int i = 0; i < count; ++i)
        fences[i] = 1;
}
void MacD3D9_SetFence(unsigned int fence)
{
    (void)fence;
}
unsigned char MacD3D9_TestFence(unsigned int fence)
{
    (void)fence;
    return 1;
}
void MacD3D9_DeleteFences(int count, const unsigned int *fences)
{
    (void)count;
    (void)fences;
}
