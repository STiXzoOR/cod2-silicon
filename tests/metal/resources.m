#include "Mac/Metal/metal_resources.h"
#include "Mac/Metal/shim_dispatch.h"
#import <Metal/Metal.h>
#import <objc/runtime.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ResourceGL_Start(void), ResourceGL_Stop(void);
extern void ResourceTest_PresentationParameters(void);
extern void ResourceGL_Read(uint32_t, unsigned int, unsigned int, unsigned int, unsigned int,
                             unsigned int, unsigned int, const unsigned char *, size_t, unsigned char *);
extern void ResourceGL_BufferRead(unsigned int, const unsigned char *, unsigned int, unsigned int, unsigned int, unsigned char *);
static unsigned char referencePalette[256][4];
/* The old GL palette entry points are stubs. Expand from an independent
 * PALETTEENTRY oracle, then use GL's working BGRA sampler/readback path. */
static void Reference(uint32_t format, unsigned int kind, unsigned int w, unsigned int h,
                      unsigned int d, unsigned int face, unsigned int z,
                      const unsigned char *data, size_t length, unsigned char *out)
{
    if (format == 0x32545844 || format == 0x34545844) {
        /* DXT2 shares GL's DXT3 storage; the unmapped DXT4 extension is BC3. */
        ResourceGL_Read(format == 0x32545844 ? 0x33545844 : 0x35545844, kind, w, h, d, face, z, data, length, out);
        return;
    }
    if (format == 24 || format == 30) {
        unsigned char *opaque = malloc(length);
        memcpy(opaque, data, length);
        for (size_t i = 1; i < length; i += 2) opaque[i] |= format == 24 ? 0x80 : 0xf0;
        ResourceGL_Read(format == 24 ? 25 : 26, kind, w, h, d, face, z, opaque, length, out);
        free(opaque); return;
    }
    if (format == 27 || format == 29 || format == 52) {
        unsigned int bytes = format == 29 ? 2 : 1;
        unsigned char *bgra = malloc((size_t)w * h * d * 4);
        for (size_t i = 0; i < (size_t)w * h * d; ++i) {
            unsigned int value = data[i * bytes];
            if (format == 52) {
                bgra[i * 4] = bgra[i * 4 + 1] = bgra[i * 4 + 2] = (value & 15) * 17;
                bgra[i * 4 + 3] = (value >> 4) * 17;
            } else {
                bgra[i * 4] = (value & 3) * 85;
                bgra[i * 4 + 1] = (((value >> 2) & 7) * 255 + 3) / 7;
                bgra[i * 4 + 2] = ((value >> 5) * 255 + 3) / 7;
                bgra[i * 4 + 3] = bytes == 2 ? data[i * 2 + 1] : 255;
            }
        }
        ResourceGL_Read(21, kind, w, h, d, face, z, bgra, (size_t)w * h * d * 4, out);
        free(bgra); return;
    }
    if (format != 40 && format != 41) {
        ResourceGL_Read(format, kind, w, h, d, face, z, data, length, out);
        return;
    }
    unsigned int pixelBytes = format == 40 ? 2 : 1;
    unsigned char *bgra = malloc((size_t)w * h * d * 4);
    for (size_t i = 0; i < (size_t)w * h * d; ++i) {
        const unsigned char *color = referencePalette[data[i * pixelBytes]];
        bgra[i * 4] = color[2]; bgra[i * 4 + 1] = color[1]; bgra[i * 4 + 2] = color[0];
        bgra[i * 4 + 3] = pixelBytes == 2 ? data[i * 2 + 1] : color[3];
    }
    ResourceGL_Read(21, kind, w, h, d, face, z, bgra, (size_t)w * h * d * 4, out);
    free(bgra);
}
static MacMetalGPU *gpu;
static MacMetalSubmission *active;
MacMetalGPU *MacMetal_GPU(void) { return gpu; }
MacMetalSubmission *MacMetalBackend_Submission(void)
{
    if (!active) active = MacMetalGPU_Begin(gpu, 0);
    return active;
}
static id<MTLComputePipelineState> pipelines[3];
static void Begin(void) { assert(MacMetalBackend_Submission()); }
static uint64_t Commit(void)
{
    assert(active);
    uint64_t value = MacMetalGPU_Commit(active, NULL, NULL, NULL);
    assert(value); active = NULL; MacMetal_ProducerSubmitted(value);
    return value;
}
static void Finish(void) { Commit(); assert(MacMetalGPU_Drain(gpu)); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu)); }
static void Release(void *object)
{
    uint32_t refs = 0;
    const void *args[] = { &object };
    assert(MacShim_Invoke("CDirect3DTexture_Release", object, args, &refs, sizeof(refs)));
}
static int32_t Call(const char *method, void *object, const void *const *args)
{
    int32_t result = -1;
    assert(MacShim_Invoke(method, object, args, &result, sizeof(result)));
    return result;
}
static void Prepare(id<MTLDevice> device)
{
    const char *source = "#include <metal_stdlib>\nusing namespace metal;\n"
        "constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::nearest, mip_filter::nearest);\n"
        "float3 facecoord(uint f, float2 uv) { float2 q=uv*2.0-1.0; switch(f) {"
        "case 0:return float3(1,-q.y,-q.x); case 1:return float3(-1,-q.y,q.x);"
        "case 2:return float3(q.x,1,q.y); case 3:return float3(q.x,-1,-q.y);"
        "case 4:return float3(q.x,-q.y,1); default:return float3(-q.x,-q.y,-1); }}\n"
        "kernel void sample2d(device uchar4 *out [[buffer(0)]], constant uint *p [[buffer(1)]],"
        "texture2d<float> t [[texture(0)]], uint i [[thread_position_in_grid]]) {"
        "if(i>=p[0]*p[1])return; float2 uv=(float2(i%p[0],i/p[0])+.5)/float2(p[0],p[1]);"
        "out[i]=uchar4(round(clamp(t.sample(s,uv,level(p[3])),0.0,1.0)*255.0)); }\n"
        "kernel void samplecube(device uchar4 *out [[buffer(0)]], constant uint *p [[buffer(1)]],"
        "texturecube<float> t [[texture(0)]], uint i [[thread_position_in_grid]]) {"
        "if(i>=p[0]*p[1])return; float2 uv=(float2(i%p[0],i/p[0])+.5)/float2(p[0],p[1]);"
        "out[i]=uchar4(round(clamp(t.sample(s,facecoord(p[4],uv),level(p[3])),0.0,1.0)*255.0)); }\n"
        "kernel void sample3d(device uchar4 *out [[buffer(0)]], constant uint *p [[buffer(1)]],"
        "texture3d<float> t [[texture(0)]], uint i [[thread_position_in_grid]]) {"
        "if(i>=p[0]*p[1]*p[2])return; float3 uv=(float3(i%p[0],(i/p[0])%p[1],i/(p[0]*p[1]))+.5)/float3(p[0],p[1],p[2]);"
        "out[i]=uchar4(round(clamp(t.sample(s,uv,level(p[3])),0.0,1.0)*255.0)); }\n";
    NSError *error = nil;
    MTLCompileOptions *options = [MTLCompileOptions new]; options.mathMode = MTLMathModeSafe;
    id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:source] options:options error:&error];
    if (!library) fprintf(stderr, "%s\n", error.localizedDescription.UTF8String);
    assert(library);
    NSArray *names = @[ @"sample2d", @"samplecube", @"sample3d" ];
    if (@available(macOS 26.0, *)) {
        id<MTL4Compiler> compiler = (__bridge id<MTL4Compiler>)MacMetalGPU_Compiler(gpu);
        for (unsigned int i = 0; i < 3; ++i) {
            MTL4LibraryFunctionDescriptor *function = [MTL4LibraryFunctionDescriptor new]; function.library = library; function.name = names[i];
            MTL4ComputePipelineDescriptor *desc = [MTL4ComputePipelineDescriptor new]; desc.computeFunctionDescriptor = function;
            pipelines[i] = [compiler newComputePipelineStateWithDescriptor:desc dynamicLinkingDescriptor:nil compilerTaskOptions:nil error:&error];
            if (!pipelines[i]) fprintf(stderr, "%s\n", error.localizedDescription.UTF8String);
            assert(pipelines[i]);
        }
    }
}
static id<MTLBuffer> Sample(id<MTLTexture> texture, unsigned int mip, unsigned int face)
{
    Begin();
    unsigned int w = (unsigned int)texture.width >> mip, h = (unsigned int)texture.height >> mip, d = (unsigned int)texture.depth >> mip;
    if (!w) w = 1; if (!h) h = 1; if (!d) d = 1;
    unsigned int parameters[] = { w, h, d, mip, face };
    id<MTLBuffer> output = [texture.device newBufferWithLength:w * h * d * 4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> params = [texture.device newBufferWithBytes:parameters length:sizeof(parameters) options:MTLResourceStorageModeShared];
    if (@available(macOS 26.0, *)) {
        unsigned int index = texture.textureType == MTLTextureTypeCube ? 1 : texture.textureType == MTLTextureType3D ? 2 : 0;
        MTL4ArgumentTableDescriptor *desc = [MTL4ArgumentTableDescriptor new]; desc.maxBufferBindCount = 2; desc.maxTextureBindCount = 1;
        id<MTL4ArgumentTable> table = [texture.device newArgumentTableWithDescriptor:desc error:nil]; assert(table);
        [table setAddress:output.gpuAddress atIndex:0]; [table setAddress:params.gpuAddress atIndex:1]; [table setTexture:texture.gpuResourceID atIndex:0];
        id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active);
        id<MTL4ComputeCommandEncoder> encoder = [command computeCommandEncoder];
        [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
        [encoder setComputePipelineState:pipelines[index]]; [encoder setArgumentTable:table];
        [encoder dispatchThreadgroups:MTLSizeMake((w * h * d + 31) / 32, 1, 1) threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
        [encoder endEncoding];
        MacMetalGPU_Keep(active, (__bridge void *)texture, 1); MacMetalGPU_Keep(active, (__bridge void *)output, 1);
        MacMetalGPU_Keep(active, (__bridge void *)params, 1); MacMetalGPU_Keep(active, (__bridge void *)table, 0);
        MacMetalGPU_Keep(active, (__bridge void *)pipelines[index], 0);
    }
    Finish(); return output;
}
static void Pattern(uint32_t format, const MacMetalFormatLayout *layout, unsigned int h, unsigned int d, unsigned int seed, unsigned char *data)
{
    for (size_t i = 0; i < layout->length; ++i) data[i] = (unsigned char)(i * 29 + seed * 47 + 13);
    if (layout->blockWidth == 4) {
        for (unsigned int z = 0; z < d; ++z)
            for (unsigned int y = 0; y < (h + 3) / 4; ++y)
                for (size_t x = 0; x < layout->rowPitch / layout->blockBytes; ++x) {
                    unsigned char *block = data + z * layout->slicePitch + y * layout->rowPitch + x * layout->blockBytes;
                    unsigned char *color = block + (format == 0x31545844 ? 0 : 8);
                    /* Includes both BC1 transparent and four-color modes. */
                    color[0] = seed & 1 ? 0 : 0xff; color[1] = seed & 1 ? 0 : 0xff;
                    color[2] = seed & 1 ? 0xff : 0; color[3] = seed & 1 ? 0xff : 0;
                    color[4] = 0xe4; color[5] = 0x1b; color[6] = 0x4e; color[7] = 0xb1;
                }
    }
}
static size_t cases, components;
static double totalError, totalSquared;
static unsigned int histogram[256], maximumError;
static void Compare(uint32_t format, unsigned int kind, unsigned int mip, unsigned int face,
                     const unsigned char *actual, const unsigned char *expected, size_t pixels)
{
    unsigned int errors[256] = {0}, max = 0; double sum = 0, squared = 0; size_t badPixels = 0;
    for (size_t i = 0; i < pixels; ++i) {
        unsigned int pixelMax = 0;
        for (unsigned int c = 0; c < 4; ++c) {
            unsigned int e = abs((int)actual[i * 4 + c] - expected[i * 4 + c]);
            ++errors[e]; ++histogram[e]; sum += e; squared += e * e;
            if (e > max) max = e; if (e > pixelMax) pixelMax = e;
        }
        if (pixelMax > 16) ++badPixels;
    }
    size_t cumulative = 0; unsigned int p99 = 0;
    while (p99 < 255 && (cumulative += errors[p99]) < (size_t)ceil(pixels * 4 * .99)) ++p99;
    if (sum / (pixels * 4) > 1 || sqrt(squared / (pixels * 4)) > 2 || p99 > 4 || badPixels * 1000 >= pixels) {
        fprintf(stderr, "parity FAILED format=%08x kind=%u mip=%u face=%u MAE=%.6f RMSE=%.6f p99=%u max=%u bad=%zu/%zu first=%u,%u,%u,%u / %u,%u,%u,%u\n",
                format, kind, mip, face, sum / (pixels * 4), sqrt(squared / (pixels * 4)), p99, max, badPixels, pixels,
                actual[0], actual[1], actual[2], actual[3], expected[0], expected[1], expected[2], expected[3]); abort();
    }
    ++cases; components += pixels * 4; totalError += sum; totalSquared += squared;
    if (max > maximumError) maximumError = max;
}
static void FormatCase(uint32_t format, MacShimKind kind, unsigned int storage, int npot)
{
    int bc = format >= 0x31545844;
    unsigned int w = kind == MAC_SHIM_TEXTURE_CUBE ? (npot ? 7 : 8) : (bc && !npot) ? 8 : 7;
    unsigned int h = kind == MAC_SHIM_TEXTURE_CUBE ? w : (bc && !npot) ? 8 : 5;
    MacMetalResourceDesc desc = { w, h, kind == MAC_SHIM_TEXTURE_3D ? 3 : 1, 0, format, 0, 0, 0, 0 };
    void *object = MacMetalResources_Create(kind, &desc, storage); assert(object);
    MacShimSidecar *side = MacMetal_FindSidecar(object); desc = *MacMetal_SidecarDesc(side);
    unsigned int faces = kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1;
    for (unsigned int face = 0; face < faces; ++face) {
        for (unsigned int mip = 0; mip < desc.levels; ++mip) {
            @autoreleasepool {
                unsigned int mw = w >> mip, mh = h >> mip, md = desc.depth >> mip;
                if (!mw) mw = 1; if (!mh) mh = 1; if (!md) md = 1;
                MacMetalFormatLayout layout; assert(MacMetalFormat_Layout(format, mw, mh, md, &layout));
                unsigned char *data = malloc(layout.length); Pattern(format, &layout, mh, md, mip + face + 1, data);
                /* Deliberately padded pitches, including each volume slice. */
                size_t row = layout.rowPitch + layout.blockBytes * 3;
                size_t rows = layout.slicePitch / layout.rowPitch, image = row * (rows + 2);
                unsigned char *padded = calloc(md, image);
                for (unsigned int z = 0; z < md; ++z)
                    for (size_t y = 0; y < rows; ++y) memcpy(padded + z * image + y * row, data + z * layout.slicePitch + y * layout.rowPitch, layout.rowPitch);
                Begin(); assert(!MacMetal_Upload(side, mip, face, padded, row, image));
                id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(side, active); assert(texture);
                assert(texture.mipmapLevelCount == desc.levels);
                assert(texture.storageMode == (storage == MAC_METAL_STORAGE_PRIVATE ? MTLStorageModePrivate : MTLStorageModeShared));
                if (bc && kind != MAC_SHIM_TEXTURE_3D) assert(texture.pixelFormat == (format == 0x31545844 ? MTLPixelFormatBC1_RGBA : (format == 0x32545844 || format == 0x33545844) ? MTLPixelFormatBC2_RGBA : MTLPixelFormatBC3_RGBA));
                id<MTLBuffer> output = Sample(texture, mip, face);
                unsigned char *reference = malloc((size_t)mw * mh * md * 4);
                for (unsigned int z = 0; z < md; ++z)
                    Reference(format, bc && kind == MAC_SHIM_TEXTURE_3D ? MAC_SHIM_TEXTURE_2D : kind, mw, mh,
                        bc ? 1 : md, face, bc ? 0 : z, data + (bc ? z * layout.slicePitch : 0), bc ? layout.slicePitch : layout.length,
                        reference + (size_t)z * mw * mh * 4);
                Compare(format, kind, mip, face, output.contents, reference, (size_t)mw * mh * md);
                free(reference); free(padded); free(data);
            }
        }
    }
    Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
}
static void Buffers(void)
{
    const MacMetalResourceOps *ops = MacMetalResources_Ops();
    for (unsigned int private = 0; private < 2; ++private) {
        for (unsigned int index = 0; index < 3; ++index) {
          @autoreleasepool {
            MacMetalResourceDesc desc = { 0, 0, 0, 0, index ? index == 1 ? 101 : 102 : 0, 0x208, 128, 0, 0 };
            void *object = MacMetalResources_Create(index ? MAC_SHIM_INDEX_BUFFER : MAC_SHIM_VERTEX_BUFFER, &desc,
                private ? MAC_METAL_STORAGE_PRIVATE : MAC_METAL_STORAGE_SHARED); assert(object);
            MacShimSidecar *side = MacMetal_FindSidecar(object); void *bytes;
            unsigned char source[128] = {0}, reference[128];
            assert(ops->lock(side, 129, 1, 0, &bytes) && !bytes);
            assert(ops->lock(side, 120, 9, 0, &bytes) && !bytes);
            assert(ops->lock(side, 16, 1, 2, &bytes) && !bytes);
            assert(!ops->lock(side, 16, 0, 0x1001, &bytes)); for (unsigned int i = 0; i < 112; ++i) ((unsigned char *)bytes)[i] = (unsigned char)(i * 29 + 31);
            memcpy(source + 16, bytes, 112);
            assert(ops->lock(side, 0, 1, 0, &bytes));
            assert(!ops->unlock(side));
            Begin(); MacMetalBufferBinding binding; assert(!MacMetalResources_Buffer(side, active, 0, 128, &binding));
            id<MTLBuffer> readback = [(__bridge id<MTLDevice>)MacMetalGPU_Device(gpu) newBufferWithLength:128 options:MTLResourceStorageModeShared];
            if (@available(macOS 26.0, *)) {
                id<MTL4ComputeCommandEncoder> encoder = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
                [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
                [encoder copyFromBuffer:(__bridge id<MTLBuffer>)binding.buffer sourceOffset:binding.offset toBuffer:readback destinationOffset:0 size:128];
                [encoder endEncoding]; MacMetalGPU_Keep(active, (__bridge void *)readback, 1);
            }
            Finish(); for (unsigned int i = 0; i < 128; ++i) assert(((unsigned char *)readback.contents)[i] == (i < 16 ? 0 : (unsigned char)((i - 16) * 29 + 31)));
            ResourceGL_BufferRead(desc.format, source, 128, 0, 128, reference);
            assert(!memcmp(readback.contents, reference, 128));
            ResourceGL_BufferRead(desc.format, source, 128, 16, 112, reference);
            assert(!memcmp((char *)readback.contents + 16, reference, 112));
            assert(!ops->lock(side, 121, 7, 0, &bytes)); for (unsigned int i = 0; i < 7; ++i) ((unsigned char *)bytes)[i] = (unsigned char)(i * 47 + 53); assert(!ops->unlock(side));
            for (unsigned int i = 0; i < 7; ++i) source[121 + i] = (unsigned char)(i * 47 + 53);
            Begin(); assert(!MacMetalResources_Buffer(side, active, 0, 128, &binding));
            if (@available(macOS 26.0, *)) {
                id<MTL4ComputeCommandEncoder> encoder = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
                [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
                [encoder copyFromBuffer:(__bridge id<MTLBuffer>)binding.buffer sourceOffset:0 toBuffer:readback destinationOffset:0 size:128];
                [encoder endEncoding]; MacMetalGPU_Keep(active, (__bridge void *)readback, 1);
            }
            Finish(); for (unsigned int i = 0; i < 128; ++i) assert(((unsigned char *)readback.contents)[i] == (i < 16 ? 0 : i < 121 ? (unsigned char)((i - 16) * 29 + 31) : (unsigned char)((i - 121) * 47 + 53)));
            ResourceGL_BufferRead(desc.format, source, 128, 0, 128, reference);
            assert(!memcmp(readback.contents, reference, 128));
            assert(!ops->lock(side, 0, 128, 0x10, &bytes));
            for (unsigned int i = 0; i < 128; ++i) assert(((unsigned char *)bytes)[i] == (i < 16 ? 0 : i < 121 ? (unsigned char)((i - 16) * 29 + 31) : (unsigned char)((i - 121) * 47 + 53)));
            assert(!ops->unlock(side));
            /* Never lock again: repeated bindings must collect completed ranges
             * and coalesce resource references within each producer submission. */
            for (unsigned int frame = 0; frame < 16; ++frame) {
                Begin();
                for (unsigned int draw = 0; draw < 8; ++draw)
                    assert(!MacMetalResources_Buffer(side, active, 0, 128, &binding));
                size_t uses, ranges;
                MacMetalResources_Counts(NULL, NULL, &uses, &ranges);
                assert(uses == 1 && ranges == 8);
                Finish();
            }
            Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
          }
        }
    }
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    /* The real engine requests DISCARD for disjoint append writes. More than
     * three draws in an unsubmitted batch must preserve every earlier range. */
    if (@available(macOS 26.0, *)) {
      for (unsigned storage = MAC_METAL_STORAGE_SHARED; storage <= MAC_METAL_STORAGE_PRIVATE; ++storage) {
        MacMetalResourceDesc desc = {0,0,0,0,0,0x208,1024,0,0};
        void *object = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, storage); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object);
        MacMetalBufferBinding binding; void *bytes;
        Begin();
        for (unsigned i = 0; i < 8; ++i) {
            assert(!ops->lock(side, i * 128, 128, 0x2000, &bytes));
            memset(bytes, i + 91, 128); assert(!ops->unlock(side));
            assert(!MacMetalResources_Buffer(side, active, i * 128, 128, &binding));
        }
        id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
        id<MTLBuffer> readback = [device newBufferWithLength:1024 options:MTLResourceStorageModeShared];
        id<MTL4ComputeCommandEncoder> copy = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
        [copy barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageAll visibilityOptions:MTL4VisibilityOptionDevice];
        [copy copyFromBuffer:(__bridge id<MTLBuffer>)binding.buffer sourceOffset:0 toBuffer:readback destinationOffset:0 size:1024];
        [copy endEncoding]; MacMetalGPU_Keep(active, (__bridge void *)readback, 1); Finish();
        for (unsigned i = 0; i < 1024; ++i) assert(((unsigned char *)readback.contents)[i] == i / 128 + 91);
        Release(object);
      }
    }
#endif
    /* Block GPU completion explicitly, proving retirement rather than relying
     * on CPU frame age or the speed of this device. */
    if (@available(macOS 26.0, *)) {
      for (unsigned int storage = MAC_METAL_STORAGE_SHARED; storage <= MAC_METAL_STORAGE_PRIVATE; ++storage) {
        @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
        id<MTLSharedEvent> gate = [device newSharedEvent];
        id<MTL4CommandQueue> queue = (__bridge id<MTL4CommandQueue>)MacMetalGPU_Queue(gpu);
        [queue waitForEvent:gate value:1];
        MacMetalResourceDesc desc = { 0, 0, 0, 0, 0, 0x208, 128, 0, 0 };
        void *object = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, storage); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object); void *bytes;
        uint64_t values[3]; void *buffers[3];
        id<MTLBuffer> readback = [device newBufferWithLength:384 options:MTLResourceStorageModeShared];
        Begin();
        for (unsigned int generation = 0; generation < 3; ++generation) {
            assert(!ops->lock(side, 0, 128, 0x2000, &bytes)); memset(bytes, generation + 71, 128); assert(!ops->unlock(side));
            MacMetalBufferBinding binding; assert(!MacMetalResources_Buffer(side, active, 0, 64, &binding));
            values[generation] = binding.generation; buffers[generation] = binding.buffer;
            assert(ops->lock(side, 0, 1, 0x1000, &bytes) == (int32_t)0x8876021c && !bytes);
            assert(!ops->lock(side, 64, 64, 0x1000, &bytes)); memset(bytes, generation + 81, 64); assert(!ops->unlock(side));
            assert(!MacMetalResources_Buffer(side, active, 64, 64, &binding));
            id<MTL4ComputeCommandEncoder> encoder = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
            [encoder copyFromBuffer:(__bridge id<MTLBuffer>)binding.buffer sourceOffset:0 toBuffer:readback destinationOffset:generation * 128 size:128];
            [encoder endEncoding];
        }
        assert(values[0] < values[1] && values[1] < values[2] && buffers[0] != buffers[1] && buffers[1] != buffers[2]);
        assert(ops->lock(side, 0, 128, 0x2000, &bytes) == (int32_t)0x8876021c && !bytes);
        MacMetalGPU_Keep(active, (__bridge void *)readback, 1); uint64_t value = Commit();
        assert(MacMetalGPU_Completed(gpu) < value);
        Begin(); assert(ops->lock(side, 0, 128, 0x2000, &bytes) == (int32_t)0x8876021c);
        MacMetalGPU_Cancel(active); active = NULL; MacMetal_ProducerSubmitted(0);
        gate.signaledValue = 1; assert(MacMetalGPU_Drain(gpu));
        for (unsigned int i = 0; i < 384; ++i) assert(((unsigned char *)readback.contents)[i] == i / 128 + (i % 128 < 64 ? 71 : 81));
        assert(!ops->lock(side, 0, 128, 0x2000, &bytes)); memset(bytes, 99, 128); assert(!ops->unlock(side));
        Begin(); MacMetalBufferBinding binding; assert(!MacMetalResources_Buffer(side, active, 0, 128, &binding));
        /* Final engine reference disappears before commit; pending references
         * keep its sidecar alive, and GPU residency survives COM destruction. */
        Release(object); assert(MacMetal_FindSidecar(object)); Finish(); assert(!MacMetal_FindSidecar(object));
        }
      }
    }
    puts("buffers: bounded offset/zero-length locks, shared/private copies, pending/in-flight ring wrap and NOOVERWRITE pass");
}
static void DeferredBuffer(void)
{
    if (@available(macOS 26.0, *)) {
        @autoreleasepool {
            id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
            id<MTLSharedEvent> gate = [device newSharedEvent];
            [(__bridge id<MTL4CommandQueue>)MacMetalGPU_Queue(gpu) waitForEvent:gate value:1];
            Begin(); Commit(); Begin(); Commit(); assert(!MacMetalBackend_Submission());
            MacMetalResourceDesc desc = {0, 0, 0, 0, 0, 8, 64, 0, 0};
            void *object = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, MAC_METAL_STORAGE_PRIVATE); assert(object);
            MacShimSidecar *side = MacMetal_FindSidecar(object); void *bytes;
            const MacMetalResourceOps *ops = MacMetalResources_Ops();
            assert(!ops->lock(side, 8, 8, 0, &bytes)); memset(bytes, 29, 8); assert(!ops->unlock(side));
            assert(!ops->lock(side, 32, 4, 0, &bytes)); memset(bytes, 73, 4); assert(!ops->unlock(side));
            gate.signaledValue = 1; assert(MacMetalGPU_Drain(gpu));
            Begin(); MacMetalBufferBinding binding; assert(!MacMetalResources_Buffer(side, active, 0, 64, &binding));
            id<MTLBuffer> output = [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
            id<MTL4ComputeCommandEncoder> encoder = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
            [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
            [encoder copyFromBuffer:(__bridge id<MTLBuffer>)binding.buffer sourceOffset:0 toBuffer:output destinationOffset:0 size:64];
            [encoder endEncoding]; MacMetalGPU_Keep(active, (__bridge void *)output, 1); Finish();
            for (unsigned int i = 0; i < 64; ++i)
                assert(((unsigned char *)output.contents)[i] == (i >= 8 && i < 16 ? 29 : i >= 32 && i < 36 ? 73 : 0));
            Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
        }
    }
    puts("private upload: exhausted producer slots defer and merge bounded shadow writes without leaving a lock held");
}
static void DeferredTexture(void)
{
    if (@available(macOS 26.0, *)) {
      for (unsigned int readOnlyBetween = 0; readOnlyBetween < 2; ++readOnlyBetween) {
        @autoreleasepool {
            MacMetalResourceDesc desc = {4, 4, 1, 1, 21, 0, 0, 0, 0};
            void *object = MacMetalResources_Create(MAC_SHIM_TEXTURE_2D, &desc, MAC_METAL_STORAGE_PRIVATE); assert(object);
            MacShimSidecar *side = MacMetal_FindSidecar(object);
            unsigned char initial[64]; memset(initial, 11, sizeof(initial));
            Begin(); assert(!MacMetal_Upload(side, 0, 0, initial, 16, 64)); Finish();
            id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
            id<MTLSharedEvent> gate = [device newSharedEvent];
            [(__bridge id<MTL4CommandQueue>)MacMetalGPU_Queue(gpu) waitForEvent:gate value:1];
            Begin(); Commit(); Begin(); Commit(); assert(!MacMetalBackend_Submission());
            uint32_t mip = 0, flags = 0;
            struct { int32_t pitch; void *bytes; } locked;
            void *output = &locked;
            int32_t rect[4]; const void *region = rect;
            const void *lockArgs[] = { &object, &mip, &output, &region, &flags };
            const void *unlockArgs[] = { &object, &mip };
            for (unsigned int edit = 0; edit < 2; ++edit) {
                rect[0] = rect[1] = edit * 2; rect[2] = rect[3] = edit * 2 + 2;
                assert(!Call("CDirect3DTexture_LockRect", object, lockArgs));
                for (unsigned int y = 0; y < 2; ++y) memset((unsigned char *)locked.bytes + y * locked.pitch, edit ? 73 : 29, 8);
                assert(Call("CDirect3DTexture_UnlockRect", object, unlockArgs) == (int32_t)0x8876021c);
                if (readOnlyBetween) {
                    rect[0] = 2; rect[1] = 0; rect[2] = 4; rect[3] = 2; flags = 0x10;
                    assert(!Call("CDirect3DTexture_LockRect", object, lockArgs));
                    assert(!Call("CDirect3DTexture_UnlockRect", object, unlockArgs)); flags = 0;
                }
            }
            gate.signaledValue = 1; assert(MacMetalGPU_Drain(gpu));
            Begin(); id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(side, active); assert(texture);
            id<MTLBuffer> actual = Sample(texture, 0, 0);
            for (unsigned int y = 0; y < 4; ++y) for (unsigned int x = 0; x < 4; ++x) for (unsigned int c = 0; c < 4; ++c)
                assert(((unsigned char *)actual.contents)[(y * 4 + x) * 4 + c] == (x < 2 && y < 2 ? 29 : x >= 2 && y >= 2 ? 73 : 11));
            Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
        }
      }
    }
    puts("deferred texture: disjoint edits and intervening readonly locks survive producer backpressure");
}


/* Fail one upload encoder without changing production dispatch or allocation. */
static unsigned int failedEncoderCalls;
static id FailEncoder(id object, SEL selector)
{
    (void)object; (void)selector; ++failedEncoderCalls; return nil;
}
static Method EncoderMethod(void)
{
    Begin();
    id command = (__bridge id)MacMetalGPU_Command(active);
    Method method = class_getInstanceMethod(object_getClass(command), @selector(computeCommandEncoder));
    assert(method); failedEncoderCalls = 0; return method;
}
static void EncoderFailure(void)
{
    if (@available(macOS 26.0, *)) {
      @autoreleasepool {
        const MacMetalResourceOps *ops = MacMetalResources_Ops();
        MacMetalResourceDesc desc = {0, 0, 0, 0, 0, 8, 64, 0, 0};
        void *object = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, MAC_METAL_STORAGE_PRIVATE); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object); void *bytes;
        assert(!ops->lock(side, 8, 8, 0, &bytes)); memset(bytes, 73, 8);
        Method method = EncoderMethod();
        IMP original = method_setImplementation(method, (IMP)FailEncoder);
        int status = ops->unlock(side);
        method_setImplementation(method, original);
        assert(failedEncoderCalls == 1 && status == (int32_t)0x8007000e);
        /* Unlock retains its lock on an allocation failure; retry it explicitly. */
        assert(!ops->unlock(side));
        MacMetalBufferBinding binding;
        assert(!MacMetalResources_Buffer(side, active, 0, 64, &binding));
        id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
        id<MTLBuffer> output = [device newBufferWithLength:64 options:MTLResourceStorageModeShared];
        id<MTL4ComputeCommandEncoder> encoder = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
        assert(encoder);
        [encoder barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
        [encoder copyFromBuffer:(__bridge id<MTLBuffer>)binding.buffer sourceOffset:0 toBuffer:output destinationOffset:0 size:64];
        [encoder endEncoding]; MacMetalGPU_Keep(active, (__bridge void *)output, 1); Finish();
        for (unsigned int i = 0; i < 64; ++i)
            assert(((unsigned char *)output.contents)[i] == (i >= 8 && i < 16 ? 73 : 0));
        Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
      }
      @autoreleasepool {
        MacMetalResourceDesc desc = {4, 4, 1, 1, 21, 0, 0, 0, 0};
        void *object = MacMetalResources_Create(MAC_SHIM_TEXTURE_2D, &desc, MAC_METAL_STORAGE_PRIVATE); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object);
        unsigned char bytes[64]; memset(bytes, 91, sizeof(bytes));
        Method method = EncoderMethod();
        IMP original = method_setImplementation(method, (IMP)FailEncoder);
        int status = MacMetal_Upload(side, 0, 0, bytes, 16, 64);
        method_setImplementation(method, original);
        assert(failedEncoderCalls == 1 && status == (int32_t)0x8007000e);
        /* Binding retries the dirty shadow after the failed upload. */
        id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(side, active); assert(texture);
        id<MTLBuffer> output = Sample(texture, 0, 0);
        for (unsigned int i = 0; i < 64; ++i) assert(((unsigned char *)output.contents)[i] == 91);
        Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
      }
    }
    puts("encoder allocation: buffer unlock and texture binding retry preserve uploaded bytes");
}

static void Cancellation(void)
{
    @autoreleasepool {
        MacMetalResourceDesc desc = { 3, 5, 1, 1, 21, 0, 0, 0, 0 };
        unsigned char bytes[3 * 5 * 4]; memset(bytes, 59, sizeof(bytes));
        void *object = MacMetalResources_Create(MAC_SHIM_TEXTURE_2D, &desc, MAC_METAL_STORAGE_PRIVATE); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object);
        Begin(); assert(!MacMetal_Upload(side, 0, 0, bytes, 12, 60));
        MacMetalGPU_Cancel(active); active = NULL; MacMetal_ProducerSubmitted(0);
        Begin(); id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(side, active); assert(texture);
        id<MTLBuffer> output = Sample(texture, 0, 0);
        for (unsigned int i = 0; i < sizeof(bytes); ++i) assert(((unsigned char *)output.contents)[i] == 59);
        Release(object); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
    }
}
static void PartialCase(uint32_t format, MacShimKind kind)
{
    @autoreleasepool {
        MacMetalResourceDesc desc = { 8, 8, kind == MAC_SHIM_TEXTURE_3D ? 3 : 1, 2, format, 0, 0, 0, 0 };
        void *object = MacMetalResources_Create(kind, &desc, MAC_METAL_STORAGE_PRIVATE); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object);
        MacMetalFormatLayout layout; assert(MacMetalFormat_Layout(format, 8, 8, desc.depth, &layout));
        unsigned char *expected = malloc(layout.length); Pattern(format, &layout, 8, desc.depth, 3, expected);
        uint32_t face = kind == MAC_SHIM_TEXTURE_CUBE ? 5 : 0, mip = 0, flags = 0;
        Begin(); assert(!MacMetal_Upload(side, 0, face, expected, layout.rowPitch, layout.slicePitch)); Finish();
        void *view = NULL; void **out = &view;
        const void *get[] = { &object, &mip, &out };
        const void *cubeGet[] = { &object, &face, &mip, &out };
        assert(!Call(kind == MAC_SHIM_TEXTURE_CUBE ? "CDirect3DCubeTexture_GetCubeMapSurface" :
            kind == MAC_SHIM_TEXTURE_3D ? "CDirect3DVolumeTexture_GetVolumeLevel" : "CDirect3DTexture_GetSurfaceLevel", object,
            kind == MAC_SHIM_TEXTURE_CUBE ? cubeGet : get)); assert(view);
        int32_t rect[] = { layout.blockWidth == 4 ? 4 : 2, layout.blockWidth == 4 ? 4 : 1, 8, layout.blockWidth == 4 ? 8 : 4 };
        uint32_t box[] = { (uint32_t)rect[0], (uint32_t)rect[1], (uint32_t)rect[2], (uint32_t)rect[3], 1, 2 };
        struct { int32_t pitch, slicePitch; void *bytes; } locked = {0};
        void *output = &locked; const void *region = kind == MAC_SHIM_TEXTURE_3D ? (void *)box : (void *)rect;
        const void *lockArgs[] = { &view, &output, &region, &flags };
        int32_t result;
        if (kind == MAC_SHIM_TEXTURE_3D) {
            void **table = *(void ***)view;
            uint32_t volumeDesc[7]; assert(!((int32_t (*)(void *, void *))table[8])(view, volumeDesc));
            assert(volumeDesc[0] == format && volumeDesc[1] == 2 && volumeDesc[6] == 3);
            result = ((int32_t (*)(void *, void *, const void *, uint32_t))table[9])(view, output, region, flags);
        } else result = Call("CDirect3DSurface_LockRect", view, lockArgs);
        assert(!result && locked.bytes && (size_t)locked.pitch == layout.rowPitch);
        size_t start = (kind == MAC_SHIM_TEXTURE_3D ? layout.slicePitch : 0) +
            (rect[1] / layout.blockWidth) * layout.rowPitch + (rect[0] / layout.blockWidth) * layout.blockBytes;
        size_t rowBytes = ((rect[2] - rect[0]) / layout.blockWidth) * layout.blockBytes;
        size_t rows = (rect[3] - rect[1]) / layout.blockWidth;
        for (size_t y = 0; y < rows; ++y) {
            memset((unsigned char *)locked.bytes + y * layout.rowPitch, 37 + y, rowBytes);
            memset(expected + start + y * layout.rowPitch, 37 + y, rowBytes);
        }
        const void *unlockArgs[] = { &view };
        if (kind == MAC_SHIM_TEXTURE_3D) assert(!((int32_t (*)(void *))(*(void ***)view)[10])(view));
        else assert(!Call("CDirect3DSurface_UnlockRect", view, unlockArgs));
        Begin(); id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(side, active); assert(texture);
        id<MTLBuffer> actual = Sample(texture, 0, face);
        unsigned char *reference = malloc((size_t)8 * 8 * desc.depth * 4);
        for (unsigned int z = 0; z < desc.depth; ++z)
            Reference(format, layout.blockWidth == 4 && kind == MAC_SHIM_TEXTURE_3D ? MAC_SHIM_TEXTURE_2D : kind,
                8, 8, layout.blockWidth == 4 ? 1 : desc.depth, face, layout.blockWidth == 4 ? 0 : z,
                expected + (layout.blockWidth == 4 ? z * layout.slicePitch : 0), layout.blockWidth == 4 ? layout.slicePitch : layout.length,
                reference + z * 8 * 8 * 4);
        Compare(format, kind, 0, face, actual.contents, reference, 8 * 8 * desc.depth);
        /* The sole external view keeps the parent alive after its engine ref
         * is released; releasing the view destroys both identities. */
        Release(object); assert(MacMetal_FindSidecar(object));
        Release(view); assert(!MacMetal_FindSidecar(view) && !MacMetal_FindSidecar(object));
        MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
        free(reference); free(expected);
    }
}
static void PaletteCase(uint32_t format)
{
    @autoreleasepool {
        unsigned char palette[256][4], indices[32], reference[64], glBytes[64];
        for (unsigned int i = 0; i < 256; ++i) { palette[i][0] = i; palette[i][1] = 255 - i; palette[i][2] = i * 7; palette[i][3] = i * 13; }
        void *device = MacMetal_CreateObject(MAC_SHIM_DEVICE, NULL);
        uint32_t number = 2; const void *entries = palette; const void *set[] = { &device, &number, &entries };
        assert(!Call("CDirect3DDevice_SetPaletteEntries", device, set));
        memcpy(referencePalette, palette, sizeof(referencePalette));
        const void *select[] = { &device, &number }; assert(!Call("CDirect3DDevice_SetCurrentTexturePalette", device, select));
        MacMetalResourceDesc desc = {4, 4, 1, 1, format, 0, 0, 0, 0};
        void *object = MacMetalResources_Create(MAC_SHIM_TEXTURE_2D, &desc, MAC_METAL_STORAGE_SHARED); assert(object);
        MacShimSidecar *side = MacMetal_FindSidecar(object); unsigned int pixelBytes = format == 40 ? 2 : 1;
        for (unsigned int i = 0; i < 16; ++i) { indices[i * pixelBytes] = i * 11; if (pixelBytes == 2) indices[i * 2 + 1] = 37 + i; }
        for (unsigned int change = 0; change < 2; ++change) {
            if (change) { palette[11][0] = 231; palette[11][2] = 19; assert(!Call("CDirect3DDevice_SetPaletteEntries", device, set)); memcpy(referencePalette, palette, sizeof(referencePalette)); }
            for (unsigned int i = 0; i < 16; ++i) {
                memcpy(reference + i * 4, palette[indices[i * pixelBytes]], 4);
                if (pixelBytes == 2) reference[i * 4 + 3] = indices[i * 2 + 1];
                glBytes[i * 4] = reference[i * 4 + 2]; glBytes[i * 4 + 1] = reference[i * 4 + 1]; glBytes[i * 4 + 2] = reference[i * 4]; glBytes[i * 4 + 3] = reference[i * 4 + 3];
            }
            Begin(); if (!change) assert(!MacMetal_Upload(side, 0, 0, indices, 4 * pixelBytes, 16 * pixelBytes));
            id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(side, active); assert(texture);
            id<MTLBuffer> actual = Sample(texture, 0, 0);
            ResourceGL_Read(21, MAC_SHIM_TEXTURE_2D, 4, 4, 1, 0, 0, glBytes, sizeof(glBytes), reference);
            Compare(format, MAC_SHIM_TEXTURE_2D, 0, 0, actual.contents, reference, 16);
        }
        Release(object); Release(device); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
    }
}
static void RejectBufferTextureCopy(void)
{
    void *device = MacMetal_CreateObject(MAC_SHIM_DEVICE, NULL);
    MacMetalResourceDesc desc = {0, 0, 0, 0, 0, 0, 8, 0, 0};
    void *source = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, MAC_METAL_STORAGE_SHARED);
    desc.length = 16;
    void *dest = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, MAC_METAL_STORAGE_SHARED);
    assert(source && dest);
    const void *args[] = { &device, &source, &dest };
    assert(Call("CDirect3DDevice_UpdateTexture", device, args) == (int32_t)0x8876086c);
    const void *reverse[] = { &device, &dest, &source };
    assert(Call("CDirect3DDevice_UpdateTexture", device, reverse) == (int32_t)0x8876086c);
    Release(source); Release(dest); Release(device);
    MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
}
static void TextureCopy(MacShimKind kind, unsigned int storage)
{
    @autoreleasepool {
        MacMetalResourceDesc desc = {7, kind == MAC_SHIM_TEXTURE_CUBE ? 7 : 5,
            kind == MAC_SHIM_TEXTURE_3D ? 3 : 1, 0, 21, 0, 0, 0, 0};
        void *device = MacMetal_CreateObject(MAC_SHIM_DEVICE, NULL);
        void *source = MacMetalResources_Create(kind, &desc, storage);
        void *dest = MacMetalResources_Create(kind, &desc,
            storage == MAC_METAL_STORAGE_SHARED ? MAC_METAL_STORAGE_PRIVATE : MAC_METAL_STORAGE_SHARED);
        assert(source && dest);
        MacShimSidecar *side = MacMetal_FindSidecar(source);
        desc = *MacMetal_SidecarDesc(side);
        unsigned int faces = kind == MAC_SHIM_TEXTURE_CUBE ? 6 : 1;
        Begin();
        for (unsigned int face = 0; face < faces; ++face)
            for (unsigned int mip = 0; mip < desc.levels; ++mip) {
                unsigned int w = desc.width >> mip, h = desc.height >> mip, d = desc.depth >> mip;
                if (!w) w = 1; if (!h) h = 1; if (!d) d = 1;
                MacMetalFormatLayout layout; assert(MacMetalFormat_Layout(21, w, h, d, &layout));
                unsigned char *bytes = malloc(layout.length); Pattern(21, &layout, h, d, mip + face + 73, bytes);
                assert(!MacMetal_Upload(side, mip, face, bytes, layout.rowPitch, layout.slicePitch)); free(bytes);
            }
        Finish();
        MacMetalResourceDesc mismatch = desc; mismatch.width = mismatch.height = 8;
        void *wrong = MacMetalResources_Create(kind, &mismatch, storage); assert(wrong);
        const void *bad[] = { &device, &source, &wrong };
        assert(Call("CDirect3DDevice_UpdateTexture", device, bad) == (int32_t)0x8876086c);
        Release(wrong);
        const void *args[] = { &device, &source, &dest };
        Begin(); assert(!Call("CDirect3DDevice_UpdateTexture", device, args)); Release(source);
        for (unsigned int face = 0; face < faces; ++face)
            for (unsigned int mip = 0; mip < desc.levels; ++mip) {
                unsigned int w = desc.width >> mip, h = desc.height >> mip, d = desc.depth >> mip;
                if (!w) w = 1; if (!h) h = 1; if (!d) d = 1;
                MacMetalFormatLayout layout; assert(MacMetalFormat_Layout(21, w, h, d, &layout));
                unsigned char *bytes = malloc(layout.length), *reference = malloc((size_t)w * h * d * 4);
                Pattern(21, &layout, h, d, mip + face + 73, bytes);
                Begin();
                id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_Texture(MacMetal_FindSidecar(dest), active);
                assert(texture); id<MTLBuffer> actual = Sample(texture, mip, face);
                for (unsigned int z = 0; z < d; ++z)
                    Reference(21, kind, w, h, d, face, z, bytes, layout.length, reference + (size_t)z * w * h * 4);
                Compare(21, kind, mip, face, actual.contents, reference, (size_t)w * h * d);
                free(reference); free(bytes);
            }
        Release(dest); Release(device); MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
    }
}
/* Engine map changes need reclamation while its outer autorelease pool is
 * still alive. Existing per-case pools previously hid deferred transfers. */
static void RepeatedRelease(void)
{
    size_t before, after;
    MacMetalResources_Counts(&before, NULL, NULL, NULL);
    MacMetalResourceDesc desc = {0}; desc.length = 256;
    for (unsigned int i = 0; i < 16; ++i) {
        void *object = MacMetalResources_Create(MAC_SHIM_VERTEX_BUFFER, &desc, MAC_METAL_STORAGE_SHARED);
        assert(object);
        Release(object);
        MacMetal_CollectRetired(MacMetalGPU_Completed(gpu));
        MacMetalResources_Counts(&after, NULL, NULL, NULL);
        assert(after == before);
    }
}
int main(void)
{
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (@available(macOS 26.0, *)) {
            if (!device || ![device supportsFamily:MTLGPUFamilyMetal4]) {
                puts("SKIP: Metal 4 device unavailable"); return 77;
            }
        } else {
            puts("SKIP: Metal 4 requires macOS 26"); return 77;
        }
        gpu = MacMetalGPU_Create((__bridge void *)device);
        assert(gpu && "Metal 4 transport initialization failed on supported hardware");
        MacShim_SelectRenderer(1); ResourceTest_PresentationParameters(); Prepare(device); ResourceGL_Start();
        printf("device=%s BC=%d\n", device.name.UTF8String, device.supportsBCTextureCompression);
        RepeatedRelease();
        RejectBufferTextureCopy();
        PaletteCase(41); PaletteCase(40);
        const uint32_t formats[] = { 20, 21, 22, 23, 25, 26, 28, 50, 51, 0x31545844, 0x33545844, 0x35545844, 40, 41, 24, 27, 29, 30, 52, 0x32545844, 0x34545844 };
        for (unsigned int f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f)
            for (unsigned int kind = MAC_SHIM_TEXTURE_2D; kind <= MAC_SHIM_TEXTURE_3D; ++kind)
                for (unsigned int storage = MAC_METAL_STORAGE_SHARED; storage <= MAC_METAL_STORAGE_PRIVATE; ++storage) {
                    @autoreleasepool { FormatCase(formats[f], kind, storage, 0); }
                }
        for (unsigned int f = 9; f < sizeof(formats) / sizeof(formats[0]); ++f)
            for (unsigned int kind = MAC_SHIM_TEXTURE_2D; kind <= MAC_SHIM_TEXTURE_3D; ++kind)
                if (formats[f] >= 0x31545844)
                    for (unsigned int storage = MAC_METAL_STORAGE_SHARED; storage <= MAC_METAL_STORAGE_PRIVATE; ++storage) {
                        @autoreleasepool { FormatCase(formats[f], kind, storage, 1); }
                    }
        for (unsigned int f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f)
            for (unsigned int kind = MAC_SHIM_TEXTURE_2D; kind <= MAC_SHIM_TEXTURE_3D; ++kind) PartialCase(formats[f], kind);
        Buffers(); DeferredBuffer(); DeferredTexture(); EncoderFailure(); Cancellation();
        for (unsigned int kind = MAC_SHIM_TEXTURE_2D; kind <= MAC_SHIM_TEXTURE_3D; ++kind)
            for (unsigned int storage = MAC_METAL_STORAGE_SHARED; storage <= MAC_METAL_STORAGE_PRIVATE; ++storage)
                TextureCopy(kind, storage);
        Begin();
        for (unsigned int kind = MAC_SHIM_TEXTURE_2D; kind <= MAC_SHIM_TEXTURE_3D; ++kind) {
            id<MTLTexture> texture = (__bridge id<MTLTexture>)MacMetalResources_NullTexture(kind, active); assert(texture);
            for (unsigned int face = 0; face < (kind == MAC_SHIM_TEXTURE_CUBE ? 6u : 1u); ++face) {
                id<MTLBuffer> output = Sample(texture, 0, face); assert(!memcmp(output.contents, (unsigned char[]){0,0,0,255}, 4));
                Begin();
            }
        }
        id<MTLBuffer> nullBuffer = (__bridge id<MTLBuffer>)MacMetalResources_NullBuffer(active); assert(nullBuffer);
        id<MTLBuffer> nullReadback = [device newBufferWithLength:256 options:MTLResourceStorageModeShared];
        if (@available(macOS 26.0, *)) {
            id<MTL4ComputeCommandEncoder> encoder = [(__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(active) computeCommandEncoder];
            [encoder copyFromBuffer:nullBuffer sourceOffset:0 toBuffer:nullReadback destinationOffset:0 size:256];
            [encoder endEncoding]; MacMetalGPU_Keep(active, (__bridge void *)nullReadback, 1);
        }
        Finish();
        for (unsigned int i = 0; i < 256; ++i) assert(!((unsigned char *)nullReadback.contents)[i]);
        ResourceGL_Stop(); MacMetalResources_Shutdown(); MacMetal_CollectRetired(UINT64_MAX);
        for (unsigned int i = 0; i < 3; ++i) pipelines[i] = nil;
        MacMetalGPU_Destroy(gpu); gpu = NULL;
    }
    size_t resources, allocations, uses, ranges; MacMetalResources_Counts(&resources, &allocations, &uses, &ranges);
    size_t cumulative = 0; unsigned int p99 = 0;
    while (p99 < 255 && (cumulative += histogram[p99]) < (size_t)ceil(components * .99)) ++p99;
    printf("GL/Metal parity: %zu subresources, %zu channels, MAE=%.6f/255 RMSE=%.6f/255 p99=%u/255 max=%u/255\n", cases, components,
        totalError / components, sqrt(totalSquared / components), p99, maximumError);
    printf("teardown: resources=%zu allocations=%zu pending=%zu ranges=%zu\n", resources, allocations, uses, ranges);
    assert(!resources && !allocations && !uses && !ranges);
    puts("Metal resources PASS");
}
