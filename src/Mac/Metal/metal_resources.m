#include "metal_resources.h"
#include "metal_inventory.h"
#include "shim_dispatch.h"
#import <Metal/Metal.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define INVALID ((int32_t)0x8876086c)
#define BUSY ((int32_t)0x8876021c)
#define UNAVAILABLE ((int32_t)0x8876086a)
#define OUTOFMEMORY ((int32_t)0x8007000e)
#define READONLY 0x10u
#define DISCARD 0x2000u
#define NOOVERWRITE 0x1000u
#define DYNAMIC 0x200u
#define RING_SIZE 3
#define ARG(type, index) (*(type const *)arguments[index])
#define U(index) ARG(uint32_t, index)
#define P(index) ARG(void *, index)

typedef struct BufferRange {
    size_t offset, length;
    unsigned int slot;
    uint64_t value;
    struct BufferRange *next, *pendingNext;
} BufferRange;
typedef struct {
    uint32_t width, height, depth;
    size_t offset;
    MacMetalFormatLayout layout;
    int locked, dirty, fullDirty;
    uint32_t flags, x, y, z, w, h, d;
} TextureLevel;
typedef struct PendingUse PendingUse;
@interface COD2MetalResource : NSObject {
@public
    MacShimSidecar *side;
    MacShimKind kind;
    MacMetalResourceDesc desc;
    unsigned int storage, current, pool, fvf, lod;
    unsigned char *cpu;
    id<MTLBuffer> buffers[RING_SIZE];
    uint64_t generations[RING_SIZE], nextGeneration;
    BufferRange *ranges;
    PendingUse *use;
    int locked, bufferDirty;
    size_t lockOffset, lockLength;
    unsigned int lockFlags;
    id<MTLTexture> texture;
    void *views[96];
    int nativeBC;
    TextureLevel levels[96];
    __unsafe_unretained COD2MetalResource *next;
}
@end
static __unsafe_unretained COD2MetalResource *live;
static size_t resourceCount, pendingCount;
/* Raw keys/values: the core payload owns resources; this registry never does. */
static CFMutableDictionaryRef registry;
static unsigned char palettes[16][256][4];
static unsigned int currentPalette;
static id<MTLTexture> nullTextures[3];
static id<MTLBuffer> nullBuffer;
static id<MTLDevice> nullDevice;
static NSHashTable *trackedAllocations;
static void Track(id object)
{
    if (!trackedAllocations) trackedAllocations = [NSHashTable weakObjectsHashTable];
    if (object) [trackedAllocations addObject:object];
}

@implementation COD2MetalResource
- (void)dealloc
{
    if (live == self) live = next;
    else for (COD2MetalResource *r = live; r; r = r->next)
        if (r->next == self) { r->next = next; break; }
    /* A retired payload can outlive its C sidecar and address reuse. */
    if (side && registry && CFDictionaryGetValue(registry, side) == (__bridge const void *)self)
        CFDictionaryRemoveValue(registry, side);
    while (ranges) { BufferRange *range = ranges; ranges = range->next; free(range); }
    free(cpu);
    --resourceCount;
}
@end
struct PendingUse {
    MacShimSidecar *side;
    void *resource;
    BufferRange *ranges;
    uint64_t levels[2];
    NSMutableSet *allocations;
    struct PendingUse *next;
};
static PendingUse *pending;

static uint32_t Mip(uint32_t n, unsigned int level) { n >>= level; return n ? n : 1; }
static int IsBuffer(MacShimKind kind) { return kind == MAC_SHIM_VERTEX_BUFFER || kind == MAC_SHIM_INDEX_BUFFER; }
static int IsTexture(MacShimKind kind) { return kind >= MAC_SHIM_TEXTURE_2D && kind <= MAC_SHIM_TEXTURE_3D; }
static COD2MetalResource *Resource(MacShimSidecar *side)
{
    return side && registry ? (__bridge COD2MetalResource *)CFDictionaryGetValue(registry, side) : nil;
}
static void Ref(MacShimSidecar *side, int add)
{
    const void *object = MacMetal_SidecarObject(side);
    const void *args[] = { &object };
    uint32_t result;
    /* Core owns reference transfer, including external surface views. */
    MacShim_Invoke(add ? "CDirect3DTexture_AddRef" : "CDirect3DTexture_Release", object, args, &result, sizeof(result));
}
static void ReleasePayload(void *payload)
{
    /* Consume __bridge_retained ownership immediately. CFBridgingRelease's
     * autoreleased return kept old map payloads alive in the engine's pool. */
    CFRelease(payload);
}
static void CollectRanges(COD2MetalResource *r)
{
    uint64_t completed = MacMetalGPU_Completed(MacMetal_GPU());
    BufferRange **link = &r->ranges;
    while (*link) {
        BufferRange *range = *link;
        if (range->value != UINT64_MAX && range->value <= completed) { *link = range->next; free(range); }
        else link = &range->next;
    }
}
static int Overlap(COD2MetalResource *r, unsigned int slot, size_t offset, size_t length)
{
    CollectRanges(r);
    for (BufferRange *range = r->ranges; range; range = range->next)
        if (range->slot == slot && offset < range->offset + range->length && range->offset < offset + length) return 1;
    return 0;
}
static int Keep(COD2MetalResource *r, MacMetalSubmission *submission, id allocation,
                BufferRange *range, unsigned int level)
{
    if (!submission || !allocation) return 0;
    PendingUse *use = r->use;
    if (!use) {
        use = calloc(1, sizeof(*use));
        if (!use) return 0;
        use->side = r->side; use->resource = (__bridge_retained void *)r;
        use->allocations = [NSMutableSet new];
        use->next = pending; pending = use; r->use = use; ++pendingCount;
        Ref(r->side, 1);
    }
    if (range) { range->pendingNext = use->ranges; use->ranges = range; }
    if (level < 96) use->levels[level / 64] |= UINT64_C(1) << (level % 64);
    if (![use->allocations containsObject:allocation]) {
        [use->allocations addObject:allocation];
        MacMetalGPU_Keep(submission, (__bridge void *)allocation, 1);
    }
    return 1;
}
static void Submitted(uint64_t value)
{
    while (pending) {
        PendingUse *use = pending; pending = use->next; --pendingCount;
        COD2MetalResource *r = (__bridge COD2MetalResource *)use->resource;
        r->use = NULL;
        for (BufferRange *range = use->ranges; range; range = range->pendingNext) range->value = value;
        if (!value)
            for (unsigned int i = 0; i < 96; ++i)
                if (use->levels[i / 64] & (UINT64_C(1) << (i % 64))) r->levels[i].dirty = r->levels[i].fullDirty = 1;
        if (!value && IsBuffer(r->kind) && r->storage == MAC_METAL_STORAGE_PRIVATE) r->bufferDirty = 1;
        if (value) MacMetal_TouchSidecar(use->side, value);
        Ref(use->side, 0);
        ReleasePayload(use->resource);
        use->allocations = nil;
        free(use);
    }
}
static id<MTLBuffer> NewBuffer(COD2MetalResource *r)
{
    id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(MacMetal_GPU());
    MTLResourceOptions options = r->storage == MAC_METAL_STORAGE_PRIVATE ? MTLResourceStorageModePrivate : MTLResourceStorageModeShared;
    id<MTLBuffer> buffer = [device newBufferWithLength:r->desc.length options:options];
    if (buffer) { Track(buffer); buffer.label = @"CoD2 resource buffer"; }
    return buffer;
}
static MTLPixelFormat BCFormat(uint32_t format)
{
    switch (format) {
    case 0x31545844: return MTLPixelFormatBC1_RGBA;
    case 0x32545844: case 0x33545844: return MTLPixelFormatBC2_RGBA;
    case 0x34545844: case 0x35545844: return MTLPixelFormatBC3_RGBA;
    default: return MTLPixelFormatInvalid;
    }
}
static int Shim(const char *, MacShimSidecar *, const void *const *, void *, size_t);
static uint32_t VolumeAddRef(void *object)
{
    uint32_t result = 0; const void *args[] = { &object };
    MacShim_Invoke("CDirect3DSurface_AddRef", object, args, &result, sizeof(result)); return result;
}
static uint32_t VolumeRelease(void *object)
{
    uint32_t result = 0; const void *args[] = { &object };
    MacShim_Invoke("CDirect3DSurface_Release", object, args, &result, sizeof(result)); return result;
}
static int32_t VolumeQuery(void *object, const void *iid, void **output)
{
    (void)iid; if (!output) return INVALID; *output = object; VolumeAddRef(object); return 0;
}
static int32_t VolumeDevice(void *object, void **output)
{
    (void)object; if (output) *output = NULL; return UNAVAILABLE;
}
static int32_t VolumeSetPrivate(void *object, const void *guid, const void *bytes, uint32_t length, uint32_t flags)
{ (void)object; (void)guid; (void)bytes; (void)length; (void)flags; return UNAVAILABLE; }
static int32_t VolumeGetPrivate(void *object, const void *guid, void *bytes, uint32_t *length)
{ (void)object; (void)guid; (void)bytes; (void)length; return UNAVAILABLE; }
static int32_t VolumeFreePrivate(void *object, const void *guid) { (void)object; (void)guid; return UNAVAILABLE; }
static int32_t VolumeContainer(void *object, const void *iid, void **output)
{
    (void)iid;
    MacShimSidecar *parent = MacMetal_SurfaceParent(MacMetal_FindSidecar(object), NULL, NULL);
    if (!parent || !output) return INVALID;
    *output = (void *)MacMetal_SidecarObject(parent); Ref(parent, 1); return 0;
}
static int32_t VolumeDesc(void *object, void *output)
{
    int32_t result = 0; const void *args[] = { &object, &output };
    Shim("CDirect3DVolume_GetDesc", MacMetal_FindSidecar(object), args, &result, sizeof(result)); return result;
}
static int32_t VolumeLock(void *object, void *output, const void *box, uint32_t flags)
{
    int32_t result = 0; const void *args[] = { &object, &output, &box, &flags };
    Shim("CDirect3DVolume_LockBox", MacMetal_FindSidecar(object), args, &result, sizeof(result)); return result;
}
static int32_t VolumeUnlock(void *object)
{
    int32_t result = 0; const void *args[] = { &object };
    Shim("CDirect3DVolume_UnlockBox", MacMetal_FindSidecar(object), args, &result, sizeof(result)); return result;
}
static void VolumeD1(void *object) { (void)object; }
static void VolumeD0(void *object)
{
    const void *args[] = { &object };
    MacShim_Invoke("ZN16CDirect3DSurfaceD0Ev", object, args, NULL, 0);
}
/* ABI 1 has no volume-view kind. A surface identity holds the parent/mip
 * reference transfer; this local vtable supplies IDirect3DVolume9 slots. */
static void *volumeTable[] = { VolumeQuery, VolumeAddRef, VolumeRelease, VolumeDevice,
    VolumeSetPrivate, VolumeGetPrivate, VolumeFreePrivate, VolumeContainer, VolumeDesc, VolumeLock, VolumeUnlock, VolumeD1, VolumeD0 };
void *MacMetalResources_Create(MacShimKind kind, const MacMetalResourceDesc *input, unsigned int storage)
{
    if (!input || (!IsBuffer(kind) && !IsTexture(kind)) || storage > MAC_METAL_STORAGE_PRIVATE) return NULL;
    id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(MacMetal_GPU());
    if (!device) return NULL;
    COD2MetalResource *r = [COD2MetalResource new];
    ++resourceCount; r->next = live; live = r;
    r->kind = kind; r->desc = *input;
    r->storage = storage ? storage : (input->usage & DYNAMIC ? MAC_METAL_STORAGE_SHARED : MAC_METAL_STORAGE_PRIVATE);
    if (IsBuffer(kind)) {
        if (!r->desc.length || r->desc.length > UINT32_MAX || r->desc.length > device.maxBufferLength ||
            (kind == MAC_SHIM_INDEX_BUFFER && r->desc.format != 101 && r->desc.format != 102)) return NULL;
        r->cpu = calloc(1, r->desc.length);
        r->buffers[0] = NewBuffer(r); r->generations[0] = ++r->nextGeneration;
        if (!r->cpu || !r->buffers[0]) return NULL;
        r->bufferDirty = r->storage == MAC_METAL_STORAGE_PRIVATE;
        if (!r->bufferDirty) memset(r->buffers[0].contents, 0, r->desc.length);
    } else {
        if (!input->width || !input->height || !input->depth || input->width > 16384 || input->height > 16384 ||
            (kind == MAC_SHIM_TEXTURE_CUBE && (input->width != input->height || input->depth != 1)) ||
            (kind == MAC_SHIM_TEXTURE_2D && input->depth != 1) ||
            (kind == MAC_SHIM_TEXTURE_3D && (input->width > 2048 || input->height > 2048 || input->depth > 2048))) return NULL;
        unsigned int n = input->width;
        if (input->height > n) n = input->height;
        if (input->depth > n) n = input->depth;
        unsigned int maximum = 0; do { ++maximum; n >>= 1; } while (n);
        if (!r->desc.levels) r->desc.levels = maximum;
        if (r->desc.levels > maximum || r->desc.levels > 16) return NULL;
        size_t total = 0;
        unsigned int faces = kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1;
        for (unsigned int face = 0; face < faces; ++face) {
            for (unsigned int mip = 0; mip < r->desc.levels; ++mip) {
                TextureLevel *level = &r->levels[face * r->desc.levels + mip];
                level->width = Mip(input->width, mip); level->height = Mip(input->height, mip); level->depth = Mip(input->depth, mip);
                if (!MacMetalFormat_Layout(input->format, level->width, level->height, level->depth, &level->layout) ||
                    total > SIZE_MAX - level->layout.length || level->layout.rowPitch > INT_MAX || level->layout.slicePitch > INT_MAX) return NULL;
                level->offset = total; total += level->layout.length;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
                level->dirty = level->fullDirty = !(input->usage & 1);
#else
                level->dirty = level->fullDirty = 1;
#endif
            }
        }
        if (total > device.maxBufferLength) return NULL;
        r->desc.length = total; r->desc.rowPitch = r->levels[0].layout.rowPitch; r->desc.slicePitch = r->levels[0].layout.slicePitch;
        r->cpu = calloc(1, total);
        if (!r->cpu) return NULL;
        MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
        desc.textureType = kind == MAC_SHIM_TEXTURE_CUBE ? MTLTextureTypeCube : kind == MAC_SHIM_TEXTURE_3D ? MTLTextureType3D : MTLTextureType2D;
        r->nativeBC = BCFormat(input->format) != MTLPixelFormatInvalid && device.supportsBCTextureCompression && kind != MAC_SHIM_TEXTURE_3D;
        desc.pixelFormat = r->nativeBC ? BCFormat(input->format) : MTLPixelFormatRGBA8Unorm;
        desc.width = input->width; desc.height = input->height; desc.depth = input->depth;
        desc.mipmapLevelCount = r->desc.levels;
        desc.storageMode = r->storage == MAC_METAL_STORAGE_PRIVATE ? MTLStorageModePrivate : MTLStorageModeShared;
        desc.usage = MTLTextureUsageShaderRead | (input->usage & 1 ? MTLTextureUsageRenderTarget : 0);
        r->texture = [device newTextureWithDescriptor:desc];
        if (!r->texture) return NULL;
        Track(r->texture); r->texture.label = @"CoD2 resource texture";
    }
    void *object = MacMetal_CreateObject(kind, &r->desc);
    if (!object) return NULL;
    r->side = MacMetal_FindSidecar(object);
    if (!registry) registry = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(registry, r->side, (__bridge const void *)r);
    MacMetal_SetSidecarPayload(r->side, (__bridge_retained void *)r, ReleasePayload);
    if (IsTexture(kind)) {
        unsigned int faces = kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1;
        for (unsigned int face = 0; face < faces; ++face) {
            for (unsigned int mip = 0; mip < r->desc.levels; ++mip) {
                TextureLevel *level = &r->levels[face * r->desc.levels + mip];
                MacMetalResourceDesc desc = { level->width, level->height, level->depth, 1, input->format, input->usage,
                    level->layout.length, level->layout.rowPitch, level->layout.slicePitch };
                void *view = MacMetal_CreateObject(MAC_SHIM_SURFACE, &desc);
                if (view && kind == MAC_SHIM_TEXTURE_3D) *(void ***)view = volumeTable;
                MacShimSidecar *surface = MacMetal_FindSidecar(view);
                if (!surface || !MacMetal_BindSurfaceView(r->side, surface, mip, face)) {
                    if (surface) Ref(surface, 0);
                    Ref(r->side, 0); return NULL;
                }
                r->views[face * r->desc.levels + mip] = view;
            }
        }
    }
    return object;
}
static int Rename(COD2MetalResource *r, int discard)
{
    for (unsigned int i = 1; i <= RING_SIZE; ++i) {
        unsigned int slot = (r->current + i) % RING_SIZE;
        if (Overlap(r, slot, 0, r->desc.length)) continue;
        if (!r->buffers[slot]) r->buffers[slot] = NewBuffer(r);
        if (!r->buffers[slot]) return OUTOFMEMORY;
        r->current = slot; r->generations[slot] = ++r->nextGeneration;
        if (discard) memset(r->cpu, 0, r->desc.length);
        if (r->storage != MAC_METAL_STORAGE_PRIVATE) memcpy(r->buffers[slot].contents, r->cpu, r->desc.length);
        else r->bufferDirty = 1;
        return 0;
    }
    return BUSY;
}
static int Lock(MacShimSidecar *side, size_t offset, size_t length, unsigned int flags, void **bytes)
{
    if (bytes) *bytes = NULL;
    COD2MetalResource *r = Resource(side);
    if (!r || !IsBuffer(r->kind) || !bytes || r->locked || offset >= r->desc.length ||
        (flags & ~(READONLY | DISCARD | NOOVERWRITE | 0x800u | 0x4000u | 1u)) ||
        ((flags & DISCARD) && (flags & (READONLY | NOOVERWRITE))) ||
        ((flags & (DISCARD | NOOVERWRITE)) && !(r->desc.usage & DYNAMIC))) return INVALID;
    /* Static-model cache calls pass 0x1001; GL ignores the low modifier bit. */
    if (!length) length = r->desc.length - offset;
    if (length > r->desc.length - offset) return INVALID;
    int result = 0;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (flags & DISCARD) {
        /* The engine also uses DISCARD for append writes when gpuSync is
         * disabled. Unreferenced nonzero ranges can use the current backing:
         * clearing or renaming here would waste the three generations while
         * earlier ranges in this producer slot are still pending. Offset zero
         * starts a new allocation; overlapping ranges still require rename. */
        if (!offset || Overlap(r, r->current, offset, length)) result = Rename(r, 1);
    }
#else
    if (flags & DISCARD) result = Rename(r, 1);
#endif
    else if (!(flags & READONLY) && Overlap(r, r->current, offset, length))
        result = flags & NOOVERWRITE ? BUSY : Rename(r, 0);
    if (result) return result;
    r->locked = 1; r->lockOffset = offset; r->lockLength = length; r->lockFlags = flags;
    *bytes = r->cpu + offset;
    return 0;
}
static int UploadBuffer(COD2MetalResource *r, MacMetalSubmission *submission)
{
    if (r->storage != MAC_METAL_STORAGE_PRIVATE) {
        if (r->bufferDirty) memcpy(r->buffers[r->current].contents, r->cpu, r->desc.length);
        else memcpy((char *)r->buffers[r->current].contents + r->lockOffset, r->cpu + r->lockOffset, r->lockLength);
        r->bufferDirty = 0;
        return 0;
    }
    if (!submission) return BUSY;
    if (@available(macOS 26.0, *)) {
        id<MTLDevice> device = r->buffers[r->current].device;
        size_t offset = r->bufferDirty ? 0 : r->lockOffset, length = r->bufferDirty ? r->desc.length : r->lockLength;
        id<MTLBuffer> staging = [device newBufferWithBytes:r->cpu + offset length:length options:MTLResourceStorageModeShared];
        if (!staging) return OUTOFMEMORY;
        Track(staging);
        if (!Keep(r, submission, r->buffers[r->current], NULL, 96)) return OUTOFMEMORY;
        MacMetalGPU_Keep(submission, (__bridge void *)staging, 1);
        id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(submission);
        id<MTL4ComputeCommandEncoder> encoder = [command computeCommandEncoder];
        if (!encoder) return OUTOFMEMORY;
        [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
        [encoder copyFromBuffer:staging sourceOffset:0 toBuffer:r->buffers[r->current] destinationOffset:offset size:length];
        [encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
        [encoder endEncoding];
        r->bufferDirty = 0;
        return 0;
    }
    return UNAVAILABLE;
}
static int Unlock(MacShimSidecar *side)
{
    COD2MetalResource *r = Resource(side);
    if (!r || !r->locked) return INVALID;
    int result = r->lockFlags & READONLY ? 0 : UploadBuffer(r, MacMetalBackend_Submission());
    if (result == BUSY) {
        /* Engine Unlock callers do not retry a busy producer slot. Preserve
         * the complete CPU shadow and flush it before the next binding. */
        r->bufferDirty = 1;
        result = 0;
    }
    if (!result) r->locked = 0;
    return result;
}
int MacMetalResources_Buffer(MacShimSidecar *side, MacMetalSubmission *submission, size_t offset,
                             size_t length, MacMetalBufferBinding *binding)
{
    if (binding) memset(binding, 0, sizeof(*binding));
    COD2MetalResource *r = Resource(side);
    if (!r || !IsBuffer(r->kind) || r->locked || !submission || !binding || !length ||
        offset > r->desc.length || length > r->desc.length - offset) return INVALID;
    if (r->bufferDirty) { int result = UploadBuffer(r, submission); if (result) return result; }
    CollectRanges(r);
    BufferRange *range = calloc(1, sizeof(*range));
    if (!range) return OUTOFMEMORY;
    *range = (BufferRange){ offset, length, r->current, UINT64_MAX, r->ranges, NULL };
    if (!Keep(r, submission, r->buffers[r->current], range, 96)) { free(range); return OUTOFMEMORY; }
    r->ranges = range;
    *binding = (MacMetalBufferBinding){ (__bridge void *)r->buffers[r->current], offset, length, r->generations[r->current] };
    return 0;
}
static int UploadLevel(COD2MetalResource *r, unsigned int mip, unsigned int face, MacMetalSubmission *submission)
{
    if (mip >= r->desc.levels || face >= (r->kind == MAC_SHIM_TEXTURE_CUBE ? 6u : 1u)) return INVALID;
    unsigned int index = face * r->desc.levels + mip;
    TextureLevel *level = &r->levels[index];
    if (!level->dirty) return 0;
    if (level->locked) return INVALID;
    if (!submission) return BUSY;
    if (@available(macOS 26.0, *)) {
        unsigned int x = level->fullDirty ? 0 : level->x, y = level->fullDirty ? 0 : level->y, z = level->fullDirty ? 0 : level->z;
        unsigned int w = level->fullDirty ? level->width : level->w, h = level->fullDirty ? level->height : level->h, d = level->fullDirty ? level->depth : level->d;
        unsigned int block = level->layout.blockWidth;
        size_t row = r->nativeBC ? ((w + 3) / 4) * level->layout.blockBytes : (size_t)w * 4;
        size_t rows = r->nativeBC ? (h + 3) / 4 : h, image = row * rows;
        const unsigned char *source = r->cpu + level->offset + z * level->layout.slicePitch + (y / block) * level->layout.rowPitch + (x / block) * level->layout.blockBytes;
        id<MTLBuffer> staging = [r->texture.device newBufferWithLength:image * d options:MTLResourceStorageModeShared];
        if (!staging) return OUTOFMEMORY;
        Track(staging);
        if (r->nativeBC) {
            for (unsigned int plane = 0; plane < d; ++plane)
                for (size_t rowIndex = 0; rowIndex < rows; ++rowIndex)
                    memcpy((char *)staging.contents + plane * image + rowIndex * row,
                           source + plane * level->layout.slicePitch + rowIndex * level->layout.rowPitch, row);
        } else if (!MacMetalFormat_RGBA(r->desc.format, w, h, d, source, level->layout.rowPitch, level->layout.slicePitch,
                                      palettes[currentPalette], staging.contents)) return INVALID;
        if (!Keep(r, submission, r->texture, NULL, index)) return OUTOFMEMORY;
        MacMetalGPU_Keep(submission, (__bridge void *)staging, 1);
        id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(submission);
        id<MTL4ComputeCommandEncoder> encoder = [command computeCommandEncoder];
        if (!encoder) return OUTOFMEMORY;
        [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
        [encoder copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:row sourceBytesPerImage:image
                    sourceSize:MTLSizeMake(w, h, d) toTexture:r->texture
              destinationSlice:face destinationLevel:mip destinationOrigin:MTLOriginMake(x, y, z)];
        [encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
        [encoder endEncoding];
        level->dirty = level->fullDirty = 0;
        return 0;
    }
    return UNAVAILABLE;
}
static int Upload(MacShimSidecar *side, unsigned int mip, unsigned int face, const void *bytes, size_t row, size_t image)
{
    COD2MetalResource *r = Resource(side);
    if (!r || !IsTexture(r->kind) || !bytes || mip >= r->desc.levels || face >= (r->kind == MAC_SHIM_TEXTURE_CUBE ? 6u : 1u)) return INVALID;
    TextureLevel *level = &r->levels[face * r->desc.levels + mip];
    size_t rows = ((size_t)level->height + level->layout.blockWidth - 1) / level->layout.blockWidth;
    if (level->locked || row < level->layout.rowPitch || row > SIZE_MAX / rows) return INVALID;
    if (!image && level->depth == 1) image = row * rows;
    if (image < row * rows || (level->depth > 1 && image > SIZE_MAX / (level->depth - 1)) ||
        (level->depth - 1) * image > SIZE_MAX - ((rows - 1) * row + level->layout.rowPitch)) return INVALID;
    for (unsigned int z = 0; z < level->depth; ++z)
        for (size_t y = 0; y < rows; ++y)
            memcpy(r->cpu + level->offset + z * level->layout.slicePitch + y * level->layout.rowPitch,
                   (const char *)bytes + z * image + y * row, level->layout.rowPitch);
    level->dirty = level->fullDirty = 1;
    return UploadLevel(r, mip, face, MacMetalBackend_Submission());
}
void *MacMetalResources_Texture(MacShimSidecar *side, MacMetalSubmission *submission)
{
    unsigned int mip = 0, face = 0;
    MacShimSidecar *parent = MacMetal_SurfaceParent(side, &mip, &face);
    if (parent) side = parent;
    COD2MetalResource *r = Resource(side);
    if (!r || !IsTexture(r->kind) || !submission) return NULL;
    unsigned int faces = r->kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1;
    for (unsigned int f = 0; f < faces; ++f)
        for (unsigned int m = 0; m < r->desc.levels; ++m) {
            if (r->levels[f * r->desc.levels + m].locked || UploadLevel(r, m, f, submission)) return NULL;
        }
    return Keep(r, submission, r->texture, NULL, 96) ? (__bridge void *)r->texture : NULL;
}
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
void *MacMetalResources_Attachment(MacShimSidecar *side)
{
    unsigned int mip = 0, face = 0;
    MacShimSidecar *parent = MacMetal_SurfaceParent(side, &mip, &face);
    COD2MetalResource *r = Resource(parent ? parent : side);
    if (!r || !IsTexture(r->kind) || !(r->desc.usage & 1)) return NULL;
    if (r->levels[face * r->desc.levels + mip].locked) return NULL;
    return (__bridge void *)r->texture;
}
#endif
typedef struct { int32_t left, top, right, bottom; } LockRect;
typedef struct { uint32_t left, top, right, bottom, front, back; } LockBox;
typedef struct { int32_t pitch; void *bytes; } LockedRect;
typedef struct { int32_t rowPitch, slicePitch; void *bytes; } LockedBox;
static int LockTexture(COD2MetalResource *r, unsigned int mip, unsigned int face,
                       void *output, const void *region, unsigned int flags, int volume)
{
    if (!output) return INVALID;
    memset(output, 0, volume ? sizeof(LockedBox) : sizeof(LockedRect));
    if (!r || !IsTexture(r->kind) || mip >= r->desc.levels || face >= (r->kind == MAC_SHIM_TEXTURE_CUBE ? 6u : 1u) ||
        (flags & ~(READONLY | DISCARD | 0x800u | 0x4000u)) || ((flags & READONLY) && (flags & DISCARD))) return INVALID;
    TextureLevel *level = &r->levels[face * r->desc.levels + mip];
    if (level->locked) return INVALID;
    uint32_t x = 0, y = 0, z = 0, right = level->width, bottom = level->height, back = level->depth;
    if (region) {
        if (volume) {
            const LockBox *box = region;
            x = box->left; y = box->top; z = box->front; right = box->right; bottom = box->bottom; back = box->back;
        } else {
            const LockRect *rect = region;
            if (rect->left < 0 || rect->top < 0 || rect->right < 0 || rect->bottom < 0) return INVALID;
            x = rect->left; y = rect->top; right = rect->right; bottom = rect->bottom;
        }
    }
    if (x >= right || y >= bottom || z >= back || right > level->width || bottom > level->height || back > level->depth) return INVALID;
    unsigned int block = level->layout.blockWidth;
    if (x % block || y % block || (right % block && right != level->width) || (bottom % block && bottom != level->height)) return INVALID;
    if ((flags & DISCARD) && (!(r->desc.usage & DYNAMIC) || region)) return INVALID;
    if (flags & DISCARD) memset(r->cpu + level->offset, 0, level->layout.length);
    /* A deferred upload still owns the previous dirty rectangle. Changing
     * lock coordinates, even for a readonly lock, must preserve those edits. */
    if (level->dirty) level->fullDirty = 1;
    level->locked = 1; level->flags = flags;
    level->x = x; level->y = y; level->z = z; level->w = right - x; level->h = bottom - y; level->d = back - z;
    void *bytes = r->cpu + level->offset + z * level->layout.slicePitch + (y / block) * level->layout.rowPitch + (x / block) * level->layout.blockBytes;
    if (volume) *(LockedBox *)output = (LockedBox){ (int32_t)level->layout.rowPitch, (int32_t)level->layout.slicePitch, bytes };
    else *(LockedRect *)output = (LockedRect){ (int32_t)level->layout.rowPitch, bytes };
    return 0;
}
static int UnlockTexture(COD2MetalResource *r, unsigned int mip, unsigned int face)
{
    if (!r || mip >= r->desc.levels || face >= (r->kind == MAC_SHIM_TEXTURE_CUBE ? 6u : 1u)) return INVALID;
    TextureLevel *level = &r->levels[face * r->desc.levels + mip];
    if (!level->locked) return INVALID;
    level->locked = 0;
    if (level->flags & READONLY) return 0;
    level->dirty = 1;
    /* Dirty shadow remains retryable at binding if producer slots are busy. */
    return UploadLevel(r, mip, face, MacMetalBackend_Submission());
}
static void Result(void *result, size_t size, int32_t value)
{
    if (!result) return;
    if (size == 4) memcpy(result, &value, sizeof(value));
    else if (size == 1) *(uint8_t *)result = (uint8_t)value;
    else if (size == 2) *(uint16_t *)result = (uint16_t)value;
}
static void Describe(COD2MetalResource *r, const MacMetalResourceDesc *desc, unsigned int mip, void *output, int volume)
{
    uint32_t words[8] = { desc->format, volume ? 2 : 1, desc->usage, r ? r->pool : 0, 0, 0, 0, 0 };
    if (volume) { words[4] = Mip(desc->width, mip); words[5] = Mip(desc->height, mip); words[6] = Mip(desc->depth, mip); }
    else { words[6] = Mip(desc->width, mip); words[7] = Mip(desc->height, mip); }
    memcpy(output, words, (volume ? 7 : 8) * sizeof(uint32_t));
}
static void DirtyPalettes(void)
{
    for (COD2MetalResource *r = live; r; r = r->next)
        if (r->desc.format == 40 || r->desc.format == 41)
            for (unsigned int i = 0; i < 96; ++i) r->levels[i].dirty = r->levels[i].fullDirty = 1;
}
static int Shim(const char *method, MacShimSidecar *side, const void *const *arguments, void *result, size_t size)
{
    const char *op = strchr(method, '_');
    if (!op) return 0;
    ++op;
    int32_t status = 0;
    if (!strncmp(method, "CDirect3DDevice_", 16)) {
        if (!strcmp(op, "CreateTexture") || !strcmp(op, "CreateCubeTexture") || !strcmp(op, "CreateVolumeTexture")) {
            int cube = !strcmp(op, "CreateCubeTexture"), volume = !strcmp(op, "CreateVolumeTexture");
            MacMetalResourceDesc desc = { U(1), cube ? U(1) : U(2), volume ? U(3) : 1,
                U(cube ? 2 : volume ? 4 : 3), U(cube ? 4 : volume ? 6 : 5), U(cube ? 3 : volume ? 5 : 4), 0, 0, 0 };
            void **output = P(cube ? 6 : volume ? 8 : 7);
            if (!output) status = INVALID;
            else {
                *output = MacMetalResources_Create(cube ? MAC_SHIM_TEXTURE_CUBE : volume ? MAC_SHIM_TEXTURE_3D : MAC_SHIM_TEXTURE_2D, &desc, MAC_METAL_STORAGE_DEFAULT);
                if (!*output) status = INVALID;
                else Resource(MacMetal_FindSidecar(*output))->pool = U(cube ? 5 : volume ? 7 : 6);
            }
        } else if (!strcmp(op, "CreateVertexBuffer") || !strcmp(op, "CreateIndexBuffer")) {
            int index = !strcmp(op, "CreateIndexBuffer");
            MacMetalResourceDesc desc = { 0, 0, 0, 0, index ? U(3) : 0, U(2), U(1), 0, 0 };
            void **output = P(5);
            if (!output) status = INVALID;
            else {
                *output = MacMetalResources_Create(index ? MAC_SHIM_INDEX_BUFFER : MAC_SHIM_VERTEX_BUFFER, &desc, MAC_METAL_STORAGE_DEFAULT);
                if (!*output) status = INVALID;
                else { COD2MetalResource *r = Resource(MacMetal_FindSidecar(*output)); r->pool = U(4); r->fvf = index ? 0 : U(3); }
            }
        } else if (!strcmp(op, "SetPaletteEntries") || !strcmp(op, "GetPaletteEntries")) {
            if (U(1) >= 16 || !P(2)) status = INVALID;
            else if (*op == 'S') { memcpy(palettes[U(1)], P(2), sizeof(palettes[0])); if (U(1) == currentPalette) DirtyPalettes(); }
            else memcpy(P(2), palettes[U(1)], sizeof(palettes[0]));
        } else if (!strcmp(op, "SetCurrentTexturePalette")) {
            if (U(1) >= 16) status = INVALID;
            else if (currentPalette != U(1)) { currentPalette = U(1); DirtyPalettes(); }
        } else if (!strcmp(op, "GetCurrentTexturePalette")) {
            if (!P(1)) status = INVALID; else *(uint32_t *)P(1) = currentPalette;
        } else if (!strcmp(op, "UpdateTexture")) {
            COD2MetalResource *source = Resource(MacMetal_FindSidecar(P(1))), *dest = Resource(MacMetal_FindSidecar(P(2)));
            if (!source || !dest || !IsTexture(source->kind) || source->kind != dest->kind ||
                source->desc.length != dest->desc.length || source->desc.format != dest->desc.format ||
                source->desc.width != dest->desc.width || source->desc.height != dest->desc.height || source->desc.depth != dest->desc.depth ||
                source->desc.levels != dest->desc.levels) status = INVALID;
            else {
                unsigned int count = dest->desc.levels * (dest->kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1);
                for (unsigned int i = 0; i < count; ++i) if (source->levels[i].locked || dest->levels[i].locked) status = INVALID;
                if (!status) {
                    memcpy(dest->cpu, source->cpu, dest->desc.length);
                    for (unsigned int i = 0; i < count; ++i) dest->levels[i].dirty = dest->levels[i].fullDirty = 1;
                    if (!MacMetalResources_Texture(dest->side, MacMetalBackend_Submission())) status = BUSY;
                }
            }
        } else return 0; /* Target/post operations are owned by WS34. */
        Result(result, size, status); return 1;
    }
    unsigned int mip = 0, face = 0;
    MacShimSidecar *parent = MacMetal_SurfaceParent(side, &mip, &face);
    COD2MetalResource *r = Resource(parent ? parent : side);
    if (side && MacMetal_SidecarKind(side) == MAC_SHIM_SURFACE && !strcmp(op, "GetDesc")) {
        if (!P(1)) status = INVALID;
        else Describe(r, MacMetal_SidecarDesc(side), 0, P(1), parent && r && r->kind == MAC_SHIM_TEXTURE_3D);
    } else if (!r) return 0;
    else if (IsBuffer(r->kind)) {
        if (!strcmp(op, "GetDesc")) {
            uint32_t desc[6] = { r->desc.format, r->kind == MAC_SHIM_INDEX_BUFFER ? 7 : 6, r->desc.usage, r->pool, (uint32_t)r->desc.length, r->fvf };
            if (!P(1)) status = INVALID; else memcpy(P(1), desc, (r->kind == MAC_SHIM_INDEX_BUFFER ? 5 : 6) * sizeof(uint32_t));
        } else if (!strcmp(op, "GetType")) status = r->kind == MAC_SHIM_INDEX_BUFFER ? 7 : 6;
        else return 0; /* ABI buffer lock/unlock callbacks are already wired. */
    } else if (!strcmp(op, "GetVolumeLevel") || !strcmp(op, "GetSurfaceLevel") || !strcmp(op, "GetCubeMapSurface")) {
        int cube = !strcmp(op, "GetCubeMapSurface");
        unsigned int level = U(cube ? 2 : 1), slice = cube ? U(1) : 0;
        void **output = P(cube ? 3 : 2);
        if (output) *output = NULL;
        if (!output || level >= r->desc.levels || slice >= (cube ? 6u : 1u) ||
            r->kind != (cube ? MAC_SHIM_TEXTURE_CUBE : !strcmp(op, "GetVolumeLevel") ? MAC_SHIM_TEXTURE_3D : MAC_SHIM_TEXTURE_2D)) status = INVALID;
        else { *output = r->views[slice * r->desc.levels + level]; Ref(MacMetal_FindSidecar(*output), 1); }
    } else if (!strcmp(op, "GetLevelDesc")) {
        if (U(1) >= r->desc.levels || !P(2)) status = INVALID;
        else Describe(r, &r->desc, U(1), P(2), r->kind == MAC_SHIM_TEXTURE_3D);
    } else if (!strcmp(op, "LockRect")) {
        int cube = !parent && r->kind == MAC_SHIM_TEXTURE_CUBE;
        status = LockTexture(r, parent ? mip : U(cube ? 2 : 1), parent ? face : cube ? U(1) : 0,
            P(parent ? 1 : cube ? 3 : 2), P(parent ? 2 : cube ? 4 : 3), U(parent ? 3 : cube ? 5 : 4), 0);
    } else if (!strcmp(op, "UnlockRect")) {
        status = UnlockTexture(r, parent ? mip : U(r->kind == MAC_SHIM_TEXTURE_CUBE ? 2 : 1), parent ? face : r->kind == MAC_SHIM_TEXTURE_CUBE ? U(1) : 0);
    } else if (!strcmp(op, "LockBox")) status = LockTexture(r, parent ? mip : U(1), 0, P(parent ? 1 : 2), P(parent ? 2 : 3), U(parent ? 3 : 4), 1);
    else if (!strcmp(op, "UnlockBox")) status = UnlockTexture(r, parent ? mip : U(1), 0);
    else if (!strcmp(op, "SetLOD")) { status = r->lod; r->lod = U(1) < r->desc.levels ? U(1) : r->desc.levels - 1; }
    else if (!strcmp(op, "GetLOD")) status = r->lod;
    else if (!strcmp(op, "IsDirty")) { Result(result, size, r->levels[face * r->desc.levels + mip].dirty); return 1; }
    else if (!strcmp(op, "PreLoad") || !strcmp(op, "UpdateOpenGLSurfaces")) {
        (void)MacMetalResources_Texture(r->side, MacMetalBackend_Submission()); return 1;
    } else return 0;
    Result(result, size, status);
    return 1;
}
static const MacMetalResourceOps ops = { MAC_METAL_ABI_VERSION, Shim, Upload, Lock, Unlock, Submitted };
const MacMetalResourceOps *MacMetalResources_Ops(void) { return &ops; }
__attribute__((constructor)) static void Install(void)
{
    MacMetal_InstallResources(&ops);
}
static void CheckNullDevice(void)
{
    id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(MacMetal_GPU());
    if (nullDevice != device) { MacMetalResources_Shutdown(); nullDevice = device; }
}
void *MacMetalResources_NullTexture(MacShimKind kind, MacMetalSubmission *submission)
{
    if (!IsTexture(kind) || !submission) return NULL;
    CheckNullDevice();
    unsigned int index = kind - MAC_SHIM_TEXTURE_2D;
    if (!nullTextures[index]) {
        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
        desc.textureType = index == 1 ? MTLTextureTypeCube : index == 2 ? MTLTextureType3D : MTLTextureType2D;
        desc.storageMode = MTLStorageModeShared;
        desc.usage = MTLTextureUsageShaderRead;
        nullTextures[index] = [nullDevice newTextureWithDescriptor:desc];
        if (!nullTextures[index]) return NULL;
        Track(nullTextures[index]);
        const unsigned char black[] = { 0, 0, 0, 255 };
        for (unsigned int face = 0; face < (index == 1 ? 6u : 1u); ++face)
            [nullTextures[index] replaceRegion:MTLRegionMake3D(0, 0, 0, 1, 1, 1) mipmapLevel:0 slice:face withBytes:black bytesPerRow:4 bytesPerImage:4];
    }
    MacMetalGPU_Keep(submission, (__bridge void *)nullTextures[index], 1);
    return (__bridge void *)nullTextures[index];
}
void *MacMetalResources_NullBuffer(MacMetalSubmission *submission)
{
    if (!submission) return NULL;
    CheckNullDevice();
    if (!nullBuffer) {
        nullBuffer = [nullDevice newBufferWithLength:256 options:MTLResourceStorageModeShared];
        if (!nullBuffer) return NULL;
        memset(nullBuffer.contents, 0, 256); Track(nullBuffer);
    }
    MacMetalGPU_Keep(submission, (__bridge void *)nullBuffer, 1);
    return (__bridge void *)nullBuffer;
}
void MacMetalResources_Shutdown(void)
{
    for (unsigned int i = 0; i < 3; ++i) nullTextures[i] = nil;
    nullBuffer = nil;
    nullDevice = nil;
    if (!resourceCount && registry) { CFRelease(registry); registry = NULL; }
}
void MacMetalResources_Counts(size_t *resources, size_t *allocations, size_t *uses, size_t *ranges)
{
    if (resources) *resources = resourceCount;
    if (allocations) *allocations = [trackedAllocations allObjects].count;
    if (uses) *uses = pendingCount;
    if (ranges) {
        *ranges = 0;
        for (COD2MetalResource *r = live; r; r = r->next)
            for (BufferRange *range = r->ranges; range; range = range->next) ++*ranges;
    }
}
void MacMetalResources_Inventory(size_t *textures, size_t *buffers, size_t *bufferBytes)
{
    *textures = *buffers = *bufferBytes = 0;
    for (COD2MetalResource *r = live; r; r = r->next) {
        if (r->texture) ++*textures;
        for (unsigned int i = 0; i < RING_SIZE; ++i) if (r->buffers[i]) {
            ++*buffers;
            *bufferBytes += r->buffers[i].length;
        }
    }
}
