#include "metal_states.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <CommonCrypto/CommonDigest.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <array>

static MacMetalStateOps installed;
static MacMetalGPU *transport;
extern "C" MacMetalGPU *MacMetal_GPU(void) { return transport; }
extern "C" int MacMetal_InstallStates(const MacMetalStateOps *ops)
{
    if (!ops || ops->abiVersion != MAC_METAL_ABI_VERSION) return 0;
    installed = *ops; return 1;
}
static void GLReference(const unsigned char *metal)
{
    CGLPixelFormatAttribute attributes[] = { kCGLPFAAccelerated, kCGLPFAColorSize, (CGLPixelFormatAttribute)24, (CGLPixelFormatAttribute)0 };
    CGLPixelFormatObj pixelFormat; GLint count; CGLContextObj context;
    if (CGLChoosePixelFormat(attributes, &pixelFormat, &count) != kCGLNoError || !count) {
        puts("SKIP: accelerated CGL reference unavailable; Metal readback still checked against exact bytes"); return;
    }
    assert(CGLCreateContext(pixelFormat, nullptr, &context) == kCGLNoError);
    CGLDestroyPixelFormat(pixelFormat); assert(CGLSetCurrentContext(context) == kCGLNoError);
    GLuint texture, framebuffer; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 19, 13, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffersEXT(1, &framebuffer); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glViewport(0, 0, 19, 13); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_DITHER);
    glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); glColor4f(.2f,.4f,.8f,1);
    glBegin(GL_TRIANGLES); glVertex2f(-1,-1); glVertex2f(3,-1); glVertex2f(-1,3); glEnd();
    unsigned char reference[19*13*4]; glReadPixels(0,0,19,13,GL_BGRA,GL_UNSIGNED_BYTE,reference);
    assert(glGetError() == GL_NO_ERROR && !memcmp(reference, metal, sizeof(reference)));
    glDeleteFramebuffersEXT(1,&framebuffer); glDeleteTextures(1,&texture);
    assert(CGLSetCurrentContext(nullptr) == kCGLNoError); CGLDestroyContext(context);
    puts("Metal states synthetic GL/Metal parity: MAE=RMSE=p99=0, pixels >16/255=0");
}
static void Identity(MacMetalShaderIdentity *out, const char *source, const char *entry)
{
    CC_SHA256(source, (CC_LONG)strlen(source), out->source);
    out->registerABI = MAC_METAL_ABI_VERSION; out->compilerOptions = MAC_METAL_STRICT_MATH;
    strcpy(out->entryPoint, entry); strcpy(out->translatorRevision, "synthetic-1");
}
static void ObservedProjection(MacMetalGPU *gpu, id<MTLLibrary> library, const MacMetalPipelineRecipe &base, const char *path)
{
    if (!path) { puts("SKIP: private Toujane observed set absent; synthetic cache fixtures still run"); return; }
    NSData *input = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path]];
    if (!input) { puts("SKIP: private Toujane observed set absent; synthetic cache fixtures still run"); return; }
    NSDictionary *manifest = [NSJSONSerialization JSONObjectWithData:input options:0 error:nil];
    assert([manifest[@"schema_version"] isEqual:@1] && [manifest[@"coverage"] isEqual:@"incomplete"]);
    NSArray *records = manifest[@"records"]; assert([records isKindOfClass:NSArray.class] && records.count);
    NSString *root = [NSHomeDirectory() stringByAppendingPathComponent:[@"Library/Application Support/CoD2-native-ws31/observed-projection/" stringByAppendingString:NSUUID.UUID.UUIDString]];
    MacMetalStates *states = MacMetalStates_Create(gpu); assert(states);
    assert(MacMetalStates_BeginLoading(states, [@"synthetic-observation-" stringByAppendingString:manifest[@"provenance_sha256"]].UTF8String, root.UTF8String));
    std::vector<MacMetalKeyStorage> keys, depthKeys;
    std::vector<std::array<MacMetalKeyStorage,16>> samplerKeys;
    for (NSDictionary *record in records) {
        MacMetalState raw; MacMetalStates_Defaults(&raw);
        NSDictionary *render = record[@"states"];
        for (NSString *number in render) { assert(number.intValue >= 0 && number.intValue < 256); raw.render[number.intValue] = [render[number] unsignedIntValue]; }
        NSArray *samplers = record[@"samplers"], *stages = record[@"stages"];
        assert(samplers.count == 16 && stages.count == 8);
        for (unsigned i = 0; i < 16; ++i) { assert([samplers[i] count] == 14); for (unsigned j = 0; j < 14; ++j) raw.sampler[i][j] = [samplers[i][j] unsignedIntValue]; }
        for (unsigned i = 0; i < 8; ++i) { assert([stages[i] count] == 33); for (unsigned j = 0; j < 33; ++j) raw.textureStage[i][j] = [stages[i][j] unsignedIntValue]; }
        MacMetalNormalizedState n; assert(MacMetalStates_Normalize(&raw, &n));
        MacMetalPipelineRecipe recipe = base;
        recipe.topology = [record[@"type"] unsignedIntValue] == 1 ? 1 : [record[@"type"] unsignedIntValue] == 2 ? 2 : 3;
        // This is a synthetic projection with explicit absent-field defaults.
        // Labels tag specialization records; they are NEVER source digests.
        NSData *labels = [NSJSONSerialization dataWithJSONObject:@[record[@"vs_name"],record[@"ps_name"]] options:0 error:nil];
        assert(labels.length <= sizeof(recipe.specialization));
        recipe.specializationLength = (uint32_t)labels.length; memcpy(recipe.specialization,labels.bytes,labels.length);
        assert(MacMetalStates_Prewarm(states,&n,&recipe,(__bridge void *)library,(__bridge void *)library));
        MacMetalKeyStorage key, depthKey; std::array<MacMetalKeyStorage,16> sk;
        assert(MacMetalStates_PipelineKey(&n,&recipe,&key) && MacMetalStates_DepthKey(&n,&depthKey));
        for (unsigned i = 0; i < 16; ++i) assert(MacMetalStates_SamplerKey(&n.sampler[i], &sk[i]));
        keys.push_back(key); depthKeys.push_back(depthKey); samplerKeys.push_back(sk);
    }
    assert(MacMetalStates_Publish(states));
    for (size_t i = 0; i < keys.size(); ++i) {
        auto key = MacMetalStates_KeyView(&keys[i]), depth = MacMetalStates_KeyView(&depthKeys[i]);
        assert(MacMetalStates_ReadyPipeline(states,&key) && MacMetalStates_ReadyDepth(states,&depth));
        for (auto &storage : samplerKeys[i]) { auto sampler = MacMetalStates_KeyView(&storage); assert(MacMetalStates_ReadySampler(states,&sampler)); }
    }
    auto stats = MacMetalStates_Stats(states);
    assert(stats.pipelineMisses == 0 && stats.depthMisses == 0 && stats.samplerMisses == 0);
    printf("Toujane synthetic projection: draws=%llu subsets=%zu PSOs=%llu depth=%llu samplers=%llu pipeline/depth/sampler misses=0; full capture incomplete\n", [manifest[@"draws"] unsignedLongLongValue],keys.size(), (unsigned long long)stats.pipelineCreates,(unsigned long long)stats.depthCreates,(unsigned long long)stats.samplerCreates);
    MacMetalStates_Destroy(states); [[NSFileManager defaultManager] removeItemAtPath:root error:nil];
}
int main(int argc, char **argv)
{
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        MacMetalGPU *gpu = MacMetalGPU_Create((__bridge void *)device);
        if (!gpu) { puts("SKIP: Metal 4 device unavailable (GL is the pre-26 fallback)"); return 77; }
        if (@available(macOS 26.0, *)) {
            const char *source = "#include <metal_stdlib>\nusing namespace metal;\nstruct O { float4 p [[position]]; };\nvertex O vs(uint i [[vertex_id]]) { float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)}; return {float4(p[i],0,1)}; }\nfragment float4 ps(){return float4(0.2,0.4,0.8,1);}\n";
            MTLCompileOptions *options = [MTLCompileOptions new]; options.mathMode = MTLMathModeSafe; options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
            NSError *error = nil;
            id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:source] options:options error:&error];
            assert(library && !error);
            MacMetalPipelineRecipe recipe = {};
            Identity(&recipe.vertex, source, "vs"); Identity(&recipe.fragment, source, "ps");
            recipe.colorFormats[0] = MTLPixelFormatBGRA8Unorm; recipe.samples = 1; recipe.topology = 3;
            MacMetalState raw; MacMetalStates_Defaults(&raw); raw.render[7] = 0;
            MacMetalNormalizedState n; assert(MacMetalStates_Normalize(&raw, &n));
            MacMetalKeyStorage key, depthKey, samplerKey;
            assert(MacMetalStates_PipelineKey(&n, &recipe, &key));
            assert(MacMetalStates_DepthKey(&n, &depthKey)); assert(MacMetalStates_SamplerKey(&n.sampler[0], &samplerKey));
            NSString *root = [NSHomeDirectory() stringByAppendingPathComponent:[@"Library/Application Support/CoD2-native-ws31/fixtures/" stringByAppendingString:NSUUID.UUID.UUIDString]];
            ObservedProjection(gpu, library, recipe, argc > 1 ? argv[1] : nullptr);
            MacMetalStates *states = MacMetalStates_Create(gpu); assert(states);
            assert(!MacMetalStates_BeginLoading(states, "fixture", "/tmp/ws31-forbidden"));
            assert(MacMetalStates_InstallDefault() && !MacMetalStates_Current());
            transport = gpu;
            assert(installed.prepare("fixture", root.UTF8String) && MacMetalStates_Current());
            MacMetalStates_Destroy(states); states = MacMetalStates_Current();
            auto loadingKey = MacMetalStates_KeyView(&key);
            assert(!MacMetalStates_ReadyPipeline(states, &loadingKey)); // Loading has no draw map.
            assert(MacMetalStates_Prewarm(states, &n, &recipe, (__bridge void *)library, (__bridge void *)library));
            assert(MacMetalStates_Prewarm(states, &n, &recipe, (__bridge void *)library, (__bridge void *)library));
            auto s = MacMetalStates_Stats(states); assert(s.pipelineCreates == 1 && s.depthCreates == 1 && s.samplerCreates == 1);
            assert(MacMetalStates_Publish(states));
            auto view = MacMetalStates_KeyView(&key), depth = MacMetalStates_KeyView(&depthKey), sampler = MacMetalStates_KeyView(&samplerKey);
            void *pso = MacMetalStates_ReadyPipeline(states, &view);
            assert(pso && MacMetalStates_ReadyPipeline(states, &view) == pso);
            assert(MacMetalStates_ReadyDepth(states, &depth) && MacMetalStates_ReadySampler(states, &sampler));
            MacMetalKeyStorage miss = key; miss.bytes[8] ^= 1; auto missing = MacMetalStates_KeyView(&miss);
            assert(!MacMetalStates_ReadyPipeline(states, &missing) && !MacMetalStates_ReadyPipeline(states, &missing));
            MacMetalKeyStorage misses[4]; assert(MacMetalStates_Misses(states, misses, 4) == 1 && MacMetalStates_KeyEqual(&miss, misses));
            s = MacMetalStates_Stats(states); assert(s.pipelineHits == 2 && s.pipelineMisses == 2 && s.uniqueMisses == 1 && s.coalescedMisses == 1);
            MacMetalKeyStorage depthMiss = depthKey, samplerMiss = samplerKey;
            depthMiss.bytes[8] = 7; samplerMiss.bytes[8] = 2; // Valid, unprepared compare/address variants.
            auto missingDepth = MacMetalStates_KeyView(&depthMiss), missingSampler = MacMetalStates_KeyView(&samplerMiss);
            assert(!MacMetalStates_ReadyDepth(states, &missingDepth) && !MacMetalStates_ReadyDepth(states, &missingDepth));
            assert(!MacMetalStates_ReadySampler(states, &missingSampler) && !MacMetalStates_ReadySampler(states, &missingSampler));
            s = MacMetalStates_Stats(states);
            assert(s.depthHits == 1 && s.depthMisses == 2 && s.samplerHits == 1 && s.samplerMisses == 2 && s.uniqueMisses == 3 && s.coalescedMisses == 3);
            assert(!MacMetalStates_Prewarm(states, &n, &recipe, (__bridge void *)library, (__bridge void *)library));
            assert(MacMetalStates_Stats(states).prohibitedFactories == 1 && MacMetalStates_Stats(states).pipelineCreates == 1);
            for (unsigned i = 0; i < 1100; ++i) {
                MacMetalKeyStorage absent = key;
                absent.bytes[10] ^= 1; // Remains absent even if the first two bytes match.
                absent.bytes[8] = (unsigned char)i; absent.bytes[9] = (unsigned char)(i >> 8);
                auto absentKey = MacMetalStates_KeyView(&absent); assert(!MacMetalStates_ReadyPipeline(states, &absentKey));
            }
            s = MacMetalStates_Stats(states);
            assert(s.uniqueMisses == 1024 && s.missOverflow > 0 && s.pipelineCreates == 1 && s.depthCreates == 1 && s.samplerCreates == 1);
            assert(MacMetalStates_Install(states)); assert(installed.readyPipeline(&view) == pso);
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:19 height:13 mipmapped:NO];
            td.storageMode = MTLStorageModeShared; td.usage = MTLTextureUsageRenderTarget;
            id<MTLTexture> target = [device newTextureWithDescriptor:td]; assert(target);
            MacMetalSubmission *slot = MacMetalGPU_Begin(gpu, 0); assert(slot);
            id<MTL4CommandBuffer> command = (__bridge id<MTL4CommandBuffer>)MacMetalGPU_Command(slot);
            MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
            pass.colorAttachments[0].texture = target; pass.colorAttachments[0].loadAction = MTLLoadActionClear; pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.renderTargetWidth = 19; pass.renderTargetHeight = 13;
            id<MTL4RenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass]; assert(encoder);
            [encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)pso];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [encoder endEncoding];
            MacMetalGPU_Keep(slot, pso, 0); MacMetalGPU_Keep(slot, (__bridge void *)target, 1);
            assert(MacMetalGPU_Commit(slot, nullptr, nullptr, nullptr) && MacMetalGPU_Drain(gpu));
            unsigned char pixels[19 * 13 * 4]; [target getBytes:pixels bytesPerRow:19 * 4 fromRegion:MTLRegionMake2D(0,0,19,13) mipmapLevel:0];
            for (unsigned i = 0; i < 19 * 13; ++i) assert(pixels[4*i] == 204 && pixels[4*i+1] == 102 && pixels[4*i+2] == 51 && pixels[4*i+3] == 255);
            GLReference(pixels);
            assert(MacMetalStates_Install(nullptr)); MacMetalStates_Destroy(states);
            states = MacMetalStates_Create(gpu); assert(MacMetalStates_BeginLoading(states, "fixture", root.UTF8String));
            assert(MacMetalStates_Stats(states).archiveLoads == 1);
            assert(MacMetalStates_Prewarm(states, &n, &recipe, (__bridge void *)library, (__bridge void *)library));
            assert(MacMetalStates_Stats(states).archiveHits == 1);
            assert(MacMetalStates_Publish(states) && MacMetalStates_ReadyPipeline(states, &view));
            assert(MacMetalStates_Stats(states).pipelineMisses == 0);
            MacMetalStates_Destroy(states);
            NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:root error:nil];
            assert(names.count == 1);
            NSString *namespaceDir = [root stringByAppendingPathComponent:names[0]];
            NSString *manifestPath = [namespaceDir stringByAppendingPathComponent:@"manifest.json"];
            NSString *gitMarker = [namespaceDir stringByAppendingPathComponent:@".git"];
            assert([[NSData data] writeToFile:gitMarker atomically:YES]);
            states = MacMetalStates_Create(gpu);
            assert(!MacMetalStates_BeginLoading(states, "fixture", root.UTF8String));
            MacMetalStates_Destroy(states);
            assert([[NSFileManager defaultManager] removeItemAtPath:gitMarker error:nil]);
            NSMutableDictionary *manifest = [[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:manifestPath] options:NSJSONReadingMutableContainers error:nil] mutableCopy];
            assert(manifest && [manifest[@"archives"] count] == 1);
            NSString *archivePath = [namespaceDir stringByAppendingPathComponent:manifest[@"archives"][0][@"file"]];
            assert([[NSData dataWithBytes:"corrupt" length:7] writeToFile:archivePath atomically:YES]);
            states = MacMetalStates_Create(gpu); assert(MacMetalStates_BeginLoading(states, "fixture", root.UTF8String));
            assert(MacMetalStates_Stats(states).archiveInvalid == 1 && MacMetalStates_Stats(states).archiveLoads == 0);
            assert(MacMetalStates_Prewarm(states, &n, &recipe, (__bridge void *)library, (__bridge void *)library));
            assert(MacMetalStates_Publish(states) && MacMetalStates_ReadyPipeline(states, &view));
            MacMetalStates_Destroy(states);
            unsigned archiveFiles = 0;
            for (NSString *file in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:namespaceDir error:nil]) if ([file hasSuffix:@".metallib"]) ++archiveFiles;
            assert(archiveFiles == 1); // Corrupt/replaced generations are pruned.
            manifest = [[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:manifestPath] options:NSJSONReadingMutableContainers error:nil] mutableCopy];
            archivePath = [namespaceDir stringByAppendingPathComponent:manifest[@"archives"][0][@"file"]];
            NSString *redirect = [namespaceDir stringByAppendingPathComponent:[NSString stringWithFormat:@"archive-%@.metallib", NSUUID.UUID.UUIDString]];
            assert([[NSFileManager defaultManager] moveItemAtPath:archivePath toPath:redirect error:nil]);
            assert(symlink(redirect.lastPathComponent.fileSystemRepresentation, archivePath.fileSystemRepresentation) == 0);
            states = MacMetalStates_Create(gpu); assert(MacMetalStates_BeginLoading(states, "fixture", root.UTF8String));
            assert(MacMetalStates_Stats(states).archiveInvalid == 1 && MacMetalStates_Stats(states).archiveLoads == 0);
            assert(MacMetalStates_Prewarm(states, &n, &recipe, (__bridge void *)library, (__bridge void *)library));
            assert(MacMetalStates_Publish(states)); MacMetalStates_Destroy(states);
            archiveFiles = 0;
            for (NSString *file in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:namespaceDir error:nil]) if ([file hasSuffix:@".metallib"]) ++archiveFiles;
            assert(archiveFiles == 1); // Redirected and orphaned payloads are gone.
            states = MacMetalStates_Create(gpu); assert(MacMetalStates_BeginLoading(states, "new-shader-manifest", root.UTF8String));
            assert(MacMetalStates_Stats(states).archiveLoads == 0 && MacMetalStates_Stats(states).archiveInvalid == 0);
            MacMetalStates *contender = MacMetalStates_Create(gpu);
            assert(!MacMetalStates_BeginLoading(contender, "new-shader-manifest", root.UTF8String));
            MacMetalStates_Destroy(contender); MacMetalStates_Destroy(states);
            states = MacMetalStates_Create(gpu);
            NSString *escape = [root stringByAppendingPathComponent:@"escape"];
            assert(symlink("/tmp", escape.fileSystemRepresentation) == 0);
            assert(!MacMetalStates_BeginLoading(states, "fixture", [escape stringByAppendingPathComponent:@"forbidden"].UTF8String));
            assert(!MacMetalStates_BeginLoading(states, "fixture", [root stringByAppendingPathComponent:@"Test.app/cache"].UTF8String));
            assert(!MacMetalStates_BeginLoading(states, "fixture", [root stringByAppendingPathComponent:@"Test.APP/cache"].UTF8String));
            assert(!MacMetalStates_BeginLoading(states, "fixture", [root stringByAppendingPathComponent:@"Test.BUNDLE/cache"].UTF8String));
            MacMetalStates_Destroy(states);
            NSString *repo = [root stringByAppendingPathComponent:@"private-repo"];
            [[NSFileManager defaultManager] createDirectoryAtPath:repo withIntermediateDirectories:YES attributes:nil error:nil];
            assert([[NSData data] writeToFile:[repo stringByAppendingPathComponent:@".git"] atomically:YES]);
            states = MacMetalStates_Create(gpu); assert(!MacMetalStates_BeginLoading(states,"fixture",repo.UTF8String)); MacMetalStates_Destroy(states);
            [[NSFileManager defaultManager] removeItemAtPath:root error:nil];
            puts("Metal states GPU: ready hits/miss coalescing, factory prohibition, archive reload/hit, prewarm zero misses and exact NPOT pixels pass");
        }
        MacMetalGPU_Destroy(gpu);
    }
}
