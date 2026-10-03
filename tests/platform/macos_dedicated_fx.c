/* Synthetic fixtures exercise the real dedicated FX parser without game data. */
#include "common_types.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

extern void FX_InitTemplates(void);
extern EffectTemplate *FX_RegisterEffect(const char *fileName);
extern float FxScheduler_GetEffectLength(const FxScheduler *scheduler, EffectTemplate *fx);
extern void FxScheduler_Clean(const FxScheduler *scheduler, unsigned char removeTemplates, EffectTemplate *preserve);
extern void MediaHandles_AddHandle(const MediaHandles *media, TMediaElement item);
extern void MediaHandles_AddEffect(const MediaHandles *media, EffectTemplate *fx);
extern void MediaHandles_Shutdown(const MediaHandles *media);
extern Bool PrimitiveTemplate_ParseMaterials(const PrimitiveTemplate *primitive, GPValue *group);
extern Bool FX_GetBoltingFrame(const PrimitiveTemplate *, const FxBoltInfo *, FxBoltFramePtr *);

static FxBoltFrame frame;
static int acquireCalls;
const FxBoltFramePtr FxBoltFrame_Acquire(const FxBoltInfo *bolt)
{
    FxBoltFramePtr result = { .value = &frame };
    assert(bolt->dobjHandle == 1);
    ++frame.refCount;
    ++acquireCalls;
    return result;
}
void FxBoltFrame_Release(const FxBoltFrame *value)
{
    assert(value == &frame && frame.refCount > 0);
    --frame.refCount;
}
const orientation_t *FxBoltFrame_GetOrientation(const FxBoltFrame *value)
{
    assert(value == &frame);
    return &frame.orientation;
}

const FxFlagEntry fxAttributeFlags[26] = { 0 };
const FxFlagEntry fxSpawnFlags[13] = { 0 };
#ifdef DEDICATED
Bool g_rendererExists = 0;
#else
Bool g_rendererExists = 1;
static byte developerCheck;
byte *fx_developer_check_ptr = &developerCheck;
MaterialHandle Material_RegisterHandle(const char *name, int track, int type)
{
    (void)track; (void)type;
    assert(!strcmp(name, "synthetic_material"));
    return (MaterialHandle)(uintptr_t)0x1234;
}
struct XModel *FX_XModelPrecache(const char *name) { (void)name; abort(); }
#endif

static const char fixture[] =
    "particle\n{\nlife 200\ndelay 50\nflags depthHack useAlpha\nspawnFlags orgOnSphere\nshader synthetic_material\n"
    "velocity 1 2 3\nacceleration 4 5 6\n}\n"
    "light\n{\nlife 300\ndelay 150\n}\n";

typedef struct Allocation {
    void *data;
    struct Allocation *next;
} Allocation;
static Allocation *allocations;
static int fileReads;
static const char *stockRoot;
static FILE *stockFile;

void *Z_MallocInternal(int size)
{
    void *data = calloc(1, size);
    assert(data);
    return data;
}

void Z_FreeInternal(void *data)
{
    free(data);
}

void *__Znam(unsigned int size)
{
    return Z_MallocInternal(size);
}

void __ZdaPv(void *data)
{
    Z_FreeInternal(data);
}

void *Hunk_AllocAlignInternal(int size, int align)
{
    Allocation *allocation = malloc(sizeof(*allocation));
    assert(allocation);
    (void)align;
    allocation->data = Z_MallocInternal(size);
    allocation->next = allocations;
    allocations = allocation;
    return allocation->data;
}

void *Hunk_AllocInternal(int size)
{
    return Hunk_AllocAlignInternal(size, sizeof(void *));
}

void *Hunk_AllocateTempMemoryInternal(int size)
{
    return Z_MallocInternal(size);
}

void Hunk_FreeTempMemory(void *data)
{
    Z_FreeInternal(data);
}

int FS_FOpenFileRead(const char *name, fileHandle_t *file, qboolean unique)
{
    if (stockRoot) {
        char path[2048];
        assert(!stockFile);
        snprintf(path, sizeof(path), "%s/%s", stockRoot, name);
        stockFile = fopen(path, "rb");
        if (!stockFile) { *file = 0; return -1; }
        fseek(stockFile, 0, SEEK_END);
        long size = ftell(stockFile);
        rewind(stockFile);
        *file = 1;
        ++fileReads;
        return (int)size;
    }
    assert(!strcmp(name, "fx/synthetic.efx"));
    (void)unique;
    *file = 1;
    ++fileReads;
    return sizeof(fixture) - 1;
}

int FS_Read(void *data, int length, int file)
{
    if (stockRoot) {
        assert(file == 1 && stockFile);
        return (int)fread(data, 1, length, stockFile);
    }
    assert(file == 1 && length == sizeof(fixture) - 1);
    memcpy(data, fixture, length);
    return length;
}

void FS_FCloseFile(fileHandle_t file)
{
    assert(file == 1);
    if (stockRoot) { fclose(stockFile); stockFile = NULL; }
}

void Com_Error(int code, const char *format, ...)
{
    (void)code;
    (void)format;
    abort();
}

void Com_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void I_strncpyz(char *dest, const char *src, int size)
{
    assert(size > 0);
    snprintf(dest, size, "%s", src);
}

int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int strcmpi(const char *a, const char *b) { return strcasecmp(a, b); }
int strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }

char *strlwr(char *text)
{
    char *cursor;
    for (cursor = text; *cursor; ++cursor)
        *cursor = tolower((unsigned char)*cursor);
    return text;
}

void Com_StripExtension(const char *name, char *dest)
{
    const char *dot = strrchr(name, '.');
    size_t length = dot ? (size_t)(dot - name) : strlen(name);
    memcpy(dest, name, length);
    dest[length] = 0;
}

qboolean Com_ValidXModelName(const char *name)
{
    return !strncmp(name, "xmodel/", 7);
}

struct XModel *XModelPrecache(const char *name, Alloc_t alloc, Alloc_t allocColl)
{
    (void)name;
    (void)alloc;
    (void)allocColl;
    if (stockRoot) {
        static XModel model;
        return &model;
    }
    assert(!"The synthetic effect does not load models");
    return NULL;
}

float flrand(float low, float high)
{
    return (low + high) * 0.5f;
}

int main(int argc, char **argv)
{
    EffectTemplate *effect;
    FxScheduler scheduler = { 0 };
    MediaHandles handles = { 0 };
    GPValue emptyMaterial = { 0 };
    int values[9];
    int i;

    if (argc == 2) {
#ifndef DEDICATED
        assert(!"Stock audit runs with rendering/media registration disabled");
#endif
        char path[2048], name[1024];
        stockRoot = argv[1];
        snprintf(path, sizeof(path), "%s/effects.txt", stockRoot);
        FILE *list = fopen(path, "r");
        assert(list);
        FX_InitTemplates();
        int count = 0, failed = 0;
        while (fgets(name, sizeof(name), list)) {
            name[strcspn(name, "\r\n")] = 0;
            effect = FX_RegisterEffect(name);
            if (!effect) { ++failed; printf("STOCK REJECTED: %s\n", name); }
            ++count;
            /* Stock exceeds the original per-map 256-template cache. */
            FxScheduler_Clean(&scheduler, 1, NULL);
            while (allocations) {
                Allocation *allocation = allocations;
                allocations = allocation->next;
                free(allocation->data);
                free(allocation);
            }
            FX_InitTemplates();
        }
        fclose(list);
        FxScheduler_Clean(&scheduler, 1, NULL);
        printf("STOCK AUDIT: %d effects, %d rejected (%d file reads); media mocked\n", count, failed, fileReads);
        goto free_allocations;
    }

    PrimitiveTemplate bolted = { .mAttributeFlags = 2 };
    FxBoltInfo bolt = { .dobjHandle = 1 };
    FxBoltFramePtr framePtr = { .value = NULL };
    assert((uintptr_t)&frame > UINT32_MAX);
    assert(FX_GetBoltingFrame(&bolted, &bolt, &framePtr));
    assert(framePtr.value == &frame && frame.refCount == 1);
    assert(FX_GetBoltingFrame(&bolted, &bolt, &framePtr));
    assert(framePtr.value == &frame && frame.refCount == 1 && acquireCalls == 2);
    FxBoltFrame_Release(framePtr.value);

    FX_InitTemplates();
    effect = FX_RegisterEffect("fx/synthetic.efx");
    assert(effect && effect->mPrimitiveCount == 2);
    assert(effect->mPrimitives[0]->mAttributeFlags == 0x81);
    assert(effect->mPrimitives[0]->mSpawnFlags == 1);
    assert(FxScheduler_GetEffectLength(&scheduler, effect) == 450.0f);
#ifdef DEDICATED
    assert(effect->mPrimitives[0]->mMediaHandles.mMediaList.size == 0);
#else
    assert(effect->mPrimitives[0]->mMediaHandles.mMediaList.size == 1);
    assert(effect->mPrimitives[0]->mMediaHandles.mMediaList.elements[0].material == (MaterialHandle)(uintptr_t)0x1234);
#endif
    assert(!PrimitiveTemplate_ParseMaterials(effect->mPrimitives[0], &emptyMaterial));
    for (i = 0; i < 24; ++i) {
        const FxChannel *channel = &effect->mPrimitives[0]->mFxChannels[i];
        assert(channel->curve && channel->curve->keyCount == 2);
        assert(isfinite(channel->scaleRange.mMin));
        assert(isfinite(channel->scaleRange.mMax));
    }
    assert(FX_RegisterEffect("fx/synthetic.efx") == effect && fileReads == 1);

    for (i = 0; i < 9; ++i) {
        TMediaElement media;
        media.data = &values[i];
        MediaHandles_AddHandle(&handles, media);
    }
    assert(handles.mMediaList.size == 9);
    for (i = 0; i < 9; ++i)
        assert(handles.mMediaList.elements[i].data == &values[i]);
    MediaHandles_Shutdown(&handles);
    for (i = 0; i < 9; ++i)
        MediaHandles_AddEffect(&handles, effect);
    for (i = 0; i < 9; ++i)
        assert(handles.mMediaList.elements[i].data == effect);
    MediaHandles_Shutdown(&handles);
    FxScheduler_Clean(&scheduler, 1, NULL);

free_allocations:
    while (allocations) {
        Allocation *allocation = allocations;
        allocations = allocation->next;
        free(allocation->data);
        free(allocation);
    }
    puts("PASS: native FX parsing, 450 ms lifetime, cache, channels and media growth");
    return 0;
}
