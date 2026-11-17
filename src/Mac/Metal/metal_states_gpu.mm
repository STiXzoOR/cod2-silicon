#include "metal_states.h"
#include "metal_state_cache.hpp"
#include "metal_inventory.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

struct MacMetalStates {
    MacMetalGPU *gpu = nullptr;
    id<MTLDevice> device;
    id<MTL4Compiler> compiler API_AVAILABLE(macos(26.0));
    id<MTL4PipelineDataSetSerializer> serializer API_AVAILABLE(macos(26.0));
    MacMetalByteCache<id<MTLRenderPipelineState>> pipelines;
    MacMetalByteCache<id<MTLRenderPipelineState>> bufferFetchPipelines;
    MacMetalByteCache<id<MTLDepthStencilState>> depths;
    MacMetalByteCache<id<MTLSamplerState>> samplers;
    MacMetalByteCache<id> archives;
    MacMetalByteCache<unsigned> misses;
    NSMutableArray *archiveRecords;
    NSMutableArray *compiledKeys;
    NSString *directory, *cacheIdentity;
    MacMetalStateStats stats = {};
    bool loading = false, ready = false, failed = false;
    int lock = -1;
};
static MacMetalStates *active;
static bool managed;
static constexpr NSUInteger ManifestLimit = 16 * 1024 * 1024;
static constexpr NSUInteger ArchiveRecordLimit = 1024, ArchiveKeyLimit = 65536;

static NSString *Digest(NSData *data)
{
    if (data.length > UINT32_MAX) return nil;
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, hash);
    char text[65]; for (unsigned i = 0; i < 32; ++i) snprintf(text + i * 2, 3, "%02x", hash[i]);
    return [NSString stringWithUTF8String:text];
}
static void Unlock(MacMetalStates *s)
{
    if (s->lock >= 0) { flock(s->lock, LOCK_UN); close(s->lock); s->lock = -1; }
}
static bool Factory(MacMetalStates *s)
{
    if (!s) return false;
    if (!s->loading || s->ready) { ++s->stats.prohibitedFactories; return false; }
    return true;
}
MacMetalStates *MacMetalStates_Create(MacMetalGPU *gpu)
{
    if (!gpu) return nullptr;
    if (@available(macOS 26.0, *)) {
        id<MTLDevice> device = (__bridge id<MTLDevice>)MacMetalGPU_Device(gpu);
        if (!device || ![device supportsFamily:MTLGPUFamilyMetal4]) return nullptr;
        auto *s = new MacMetalStates;
        s->gpu = MacMetalGPU_Retain(gpu); s->device = device;
        return s;
    }
    return nullptr; // macOS 13-25 uses GL, no Metal 3 archive/backend.
}
size_t MacMetalStates_ResidentPipelines(void)
{
    std::set<void *> unique;
    if (active) for (const auto &entry : active->pipelines.all()) unique.insert((__bridge void *)entry.second);
    return unique.size();
}
void MacMetalStates_Destroy(MacMetalStates *s)
{
    if (!s) return;
    if (active == s) { fprintf(stderr, "Metal states: uninstall before destroying provider\n"); return; }
    Unlock(s);
    MacMetalGPU *gpu = s->gpu;
    delete s;
    MacMetalGPU_Destroy(gpu);
}
static NSString *SupportPath(const char *path)
{
    if (!path || !*path) return nil;
    NSString *requested = [NSString stringWithUTF8String:path];
    if (!requested || !requested.isAbsolutePath) return nil;
    NSString *root = [[[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support"] stringByStandardizingPath] stringByResolvingSymlinksInPath];
    NSString *resolved = [[requested stringByStandardizingPath] stringByResolvingSymlinksInPath];
    if (![resolved hasPrefix:[root stringByAppendingString:@"/"]] || [resolved.pathComponents containsObject:@"Contents"]) return nil;
    for (NSString *part in resolved.pathComponents) if ([part.pathExtension.lowercaseString isEqualToString:@"app"] || [part.pathExtension.lowercaseString isEqualToString:@"bundle"]) return nil;
    for (NSString *parent = resolved; parent.length > 1; parent = parent.stringByDeletingLastPathComponent)
        if ([[NSFileManager defaultManager] fileExistsAtPath:[parent stringByAppendingPathComponent:@".git"]]) return nil;
    return resolved;
}
static bool ArchiveFile(NSString *name)
{
    return [name isKindOfClass:NSString.class] && name.length && [name.lastPathComponent isEqualToString:name] && ![name isEqualToString:@".."];
}
static bool LoadArchives(MacMetalStates *s) API_AVAILABLE(macos(26.0));
static bool LoadArchives(MacMetalStates *s)
{
    NSData *data = [NSData dataWithContentsOfFile:[s->directory stringByAppendingPathComponent:@"manifest.json"]];
    if (!data) return true; // Cold loading miss, not a draw-time compile.
    if (data.length > ManifestLimit) return false;
    NSDictionary *manifest = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![manifest isKindOfClass:NSDictionary.class] || ![manifest[@"schema"] isEqual:@(MAC_METAL_STATE_KEY_SCHEMA)] || ![manifest[@"identity"] isEqual:s->cacheIdentity]) return false;
    NSArray *sets = manifest[@"archives"];
    if (![sets isKindOfClass:NSArray.class] || sets.count > ArchiveRecordLimit) return false;
    MacMetalByteCache<id> loaded;
    NSMutableArray *records = [NSMutableArray new];
    for (NSDictionary *set in sets) {
        if (![set isKindOfClass:NSDictionary.class] || !ArchiveFile(set[@"file"]) || (set[@"script"] && !ArchiveFile(set[@"script"]))) return false;
        NSString *archivePath = [s->directory stringByAppendingPathComponent:set[@"file"]];
        NSString *scriptPath = set[@"script"] ? [s->directory stringByAppendingPathComponent:set[@"script"]] : nil;
        // Immutable generation payloads cannot redirect to another file, even
        // inside the namespace: pruning preserves the manifest names exactly.
        if (![[archivePath stringByResolvingSymlinksInPath] isEqual:archivePath] || (scriptPath && ![[scriptPath stringByResolvingSymlinksInPath] isEqual:scriptPath])) return false;
        NSData *archive = [NSData dataWithContentsOfFile:archivePath], *script = scriptPath ? [NSData dataWithContentsOfFile:scriptPath] : nil;
        if (!archive || ![Digest(archive) isEqual:set[@"sha256"]] || (scriptPath && (!script || ![Digest(script) isEqual:set[@"scriptSHA256"]]))) return false;
        NSArray *keys = set[@"keys"];
        if (![keys isKindOfClass:NSArray.class] || keys.count > ArchiveKeyLimit || ![set[@"count"] isEqual:@(keys.count)]) return false;
        id<MTL4Archive> object = [s->device newArchiveWithURL:[NSURL fileURLWithPath:archivePath] error:nil];
        if (!object) return false;
        for (id encoded in keys) {
            if (![encoded isKindOfClass:NSString.class]) return false;
            NSData *keyData = [[NSData alloc] initWithBase64EncodedString:encoded options:0];
            MacMetalPipelineKey key = { (const unsigned char *)keyData.bytes, keyData.length, MAC_METAL_STATE_KEY_SCHEMA };
            if (!MacMetalByteCache<int>::valid(key) || key.bytes[3] != 1 || !loaded.insert(key, object)) return false;
        }
        [records addObject:set];
    }
    s->archives = std::move(loaded); s->archiveRecords = records;
    s->stats.archiveLoads += sets.count;
    return true;
}
int MacMetalStates_BeginLoading(MacMetalStates *s, const char *name, const char *path)
{
    if (!s || !name || !*name || strlen(name) > 4096 || s->loading) return 0;
    NSString *root = SupportPath(path);
    if (!root) { fprintf(stderr, "Metal states: cache must be under the user's Application Support\n"); return 0; }
    NSString *namespaceName = [NSString stringWithUTF8String:name];
    if (!namespaceName) return 0;
    if (@available(macOS 26.0, *)) {
        NSString *identity = [NSString stringWithFormat:@"%@|schema=%u|abi=%u|strict=%u|device=%llu:%@|os=%@", namespaceName, MAC_METAL_STATE_KEY_SCHEMA, MAC_METAL_ABI_VERSION, MAC_METAL_STRICT_MATH, (unsigned long long)s->device.registryID, s->device.name, NSProcessInfo.processInfo.operatingSystemVersionString];
        NSString *directory = [root stringByAppendingPathComponent:Digest([identity dataUsingEncoding:NSUTF8StringEncoding])];
        if (![[NSFileManager defaultManager] createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:@{ NSFilePosixPermissions:@0700 } error:nil]) return 0;
        // Recheck after creation, rejecting a symlink in the namespace directory.
        if (![SupportPath(directory.UTF8String) isEqual:directory]) return 0;
        int lock = open([[directory stringByAppendingPathComponent:@"setup.lock"] fileSystemRepresentation], O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
        if (lock < 0) return 0;
        if (flock(lock, LOCK_EX | LOCK_NB)) { close(lock); return 0; }
        s->lock = lock; s->directory = directory; s->cacheIdentity = identity;
        s->loading = true; s->ready = false; s->failed = false;
        s->pipelines.clear(); s->depths.clear(); s->samplers.clear(); s->archives.clear(); s->misses.clear();
        s->bufferFetchPipelines.clear();
        s->archiveRecords = [NSMutableArray new]; s->compiledKeys = [NSMutableArray new];
        MTL4PipelineDataSetSerializerDescriptor *sd = [MTL4PipelineDataSetSerializerDescriptor new];
        // The combined flags return NO without NSError on this tested macOS 27
        // runtime. Binary capture works, with optional scripts (see WS31 report).
        sd.configuration = MTL4PipelineDataSetSerializerConfigurationCaptureBinaries;
        s->serializer = [s->device newPipelineDataSetSerializerWithDescriptor:sd];
        MTL4CompilerDescriptor *cd = [MTL4CompilerDescriptor new]; cd.pipelineDataSetSerializer = s->serializer;
        s->compiler = [s->device newCompilerWithDescriptor:cd error:nil];
        if (!s->serializer || !s->compiler) { s->loading = false; Unlock(s); return 0; }
        if (!LoadArchives(s)) { ++s->stats.archiveInvalid; s->archives.clear(); [s->archiveRecords removeAllObjects]; }
        return 1;
    }
    return 0;
}
static MTLBlendFactor Blend(uint32_t value)
{
    const MTLBlendFactor factors[] = { MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorOne,
        MTLBlendFactorSourceColor, MTLBlendFactorOneMinusSourceColor, MTLBlendFactorSourceAlpha,
        MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorDestinationAlpha, MTLBlendFactorOneMinusDestinationAlpha,
        MTLBlendFactorDestinationColor, MTLBlendFactorOneMinusDestinationColor, MTLBlendFactorSourceAlphaSaturated,
        MTLBlendFactorSourceAlpha, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorBlendColor,
        MTLBlendFactorOneMinusBlendColor, MTLBlendFactorSource1Color, MTLBlendFactorOneMinusSource1Color };
    return factors[value <= 17 ? value : 0];
}
static void BlendPair(uint32_t source, uint32_t dest, MTLBlendFactor *s, MTLBlendFactor *d)
{
    *s = Blend(source); *d = source == 12 ? MTLBlendFactorOneMinusSourceAlpha : source == 13 ? MTLBlendFactorSourceAlpha : Blend(dest);
}
static MTLColorWriteMask WriteMask(uint32_t mask)
{
    return (MTLColorWriteMask)((mask & 1 ? MTLColorWriteMaskRed : 0) | (mask & 2 ? MTLColorWriteMaskGreen : 0) | (mask & 4 ? MTLColorWriteMaskBlue : 0) | (mask & 8 ? MTLColorWriteMaskAlpha : 0));
}
static id<MTLDepthStencilState> PrepareDepth(MacMetalStates *s, const MacMetalNormalizedState *n)
{
    MacMetalKeyStorage storage;
    if (!MacMetalStates_DepthKey(n, &storage)) return nil;
    auto key = MacMetalStates_KeyView(&storage);
    if (auto cached = s->depths.find(key)) return *cached;
    MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
    d.depthCompareFunction = (MTLCompareFunction)(n->render[7] ? n->render[23] - 1 : 7);
    d.depthWriteEnabled = n->render[7] && n->render[14];
    if (n->render[52]) {
        MTLStencilDescriptor *front = [MTLStencilDescriptor new], *back = [MTLStencilDescriptor new];
        for (unsigned face = 0; face < 2; ++face) {
            unsigned base = face && n->render[185] ? 186 : 53;
            MTLStencilDescriptor *stencil = face ? back : front;
            stencil.stencilCompareFunction = (MTLCompareFunction)(n->render[base + 3] - 1);
            stencil.stencilFailureOperation = (MTLStencilOperation)(n->render[base] - 1);
            stencil.depthFailureOperation = (MTLStencilOperation)(n->render[base + 1] - 1);
            stencil.depthStencilPassOperation = (MTLStencilOperation)(n->render[base + 2] - 1);
            stencil.readMask = n->render[58]; stencil.writeMask = n->render[59];
        }
        d.frontFaceStencil = front; d.backFaceStencil = back;
    }
    ++s->stats.depthCreates;
    id<MTLDepthStencilState> object = [s->device newDepthStencilStateWithDescriptor:d];
    if (object) s->depths.insert(key, object);
    return object;
}
static id<MTLSamplerState> PrepareSampler(MacMetalStates *s, const MacMetalSamplerState *n) API_AVAILABLE(macos(26.0));
static id<MTLSamplerState> PrepareSampler(MacMetalStates *s, const MacMetalSamplerState *n)
{
    MacMetalKeyStorage storage;
    if (!MacMetalStates_SamplerKey(n, &storage) || n->elementIndex || n->dmapOffset) return nil;
    auto key = MacMetalStates_KeyView(&storage);
    if (auto cached = s->samplers.find(key)) return *cached;
    MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
    const MTLSamplerAddressMode modes[] = { MTLSamplerAddressModeRepeat, MTLSamplerAddressModeRepeat, MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeClampToBorderColor, MTLSamplerAddressModeMirrorClampToEdge };
    for (unsigned a : n->address) if (a < 1 || a > 5) return nil;
    d.sAddressMode = modes[n->address[0]]; d.tAddressMode = modes[n->address[1]]; d.rAddressMode = modes[n->address[2]];
    switch (n->borderColor) {
    case 0: d.borderColor = MTLSamplerBorderColorTransparentBlack; break;
    case 0xff000000: d.borderColor = MTLSamplerBorderColorOpaqueBlack; break;
    case 0xffffffff: d.borderColor = MTLSamplerBorderColorOpaqueWhite; break;
    default: fprintf(stderr, "Metal states: arbitrary D3D border color requires shader emulation\n"); return nil;
    }
    // Metal 4 bias is S4.6. Reject nonrepresentable D3D values until WS32
    // supplies an exact emulation path; never silently round a classic state.
    if (n->lodBias < -16 || n->lodBias > 15.984375f || std::round(n->lodBias * 64) != n->lodBias * 64) {
        fprintf(stderr, "Metal states: LOD bias requires shader emulation\n"); return nil;
    }
    d.lodBias = n->lodBias;
    d.minFilter = n->minFilter >= 2 ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    d.magFilter = n->magFilter >= 2 ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    d.mipFilter = n->mipFilter == 2 ? MTLSamplerMipFilterLinear : n->mipFilter == 1 ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterNotMipmapped;
    d.maxAnisotropy = n->anisotropy; d.lodMinClamp = n->maxMipLevel;
    d.supportArgumentBuffers = YES;
    ++s->stats.samplerCreates;
    id<MTLSamplerState> object = [s->device newSamplerStateWithDescriptor:d];
    if (object) s->samplers.insert(key, object);
    return object;
}
int MacMetalStates_Prewarm(MacMetalStates *s, const MacMetalNormalizedState *n, const MacMetalPipelineRecipe *r, void *vertex, void *fragment)
{
    if (!Factory(s)) return 0;
    if (!n || !r || !vertex || !fragment) { s->failed = true; return 0; }
    if (@available(macOS 26.0, *)) {
        MacMetalKeyStorage storage;
        if (!MacMetalStates_PipelineKey(n, r, &storage) || n->render[7] == 2 || ![s->device supportsTextureSampleCount:r->samples]) { s->failed = true; return 0; }
        auto key = MacMetalStates_KeyView(&storage);
        if (!PrepareDepth(s, n)) { s->failed = true; return 0; }
        for (const auto &sampler : n->sampler) if (!PrepareSampler(s, &sampler)) { s->failed = true; return 0; }
        if (s->pipelines.find(key)) return 1;
        MacMetalKeyStorage bufferFetchStorage;
        MacMetalPipelineKey bufferFetchKey = {};
        if (!r->hardwareVertexFetch) {
            /* Logical declarations remain distinct ready-lookup keys. Their
             * layouts live in the immutable shader fetch descriptor, so the
             * actual Metal pipeline descriptor is identical across layouts. */
            MacMetalPipelineRecipe descriptorRecipe = *r;
            descriptorRecipe.attributeCount = 0;
            memset(descriptorRecipe.attributes, 0, sizeof(descriptorRecipe.attributes));
            memset(descriptorRecipe.strides, 0, sizeof(descriptorRecipe.strides));
            if (!MacMetalStates_PipelineKey(n, &descriptorRecipe, &bufferFetchStorage)) { s->failed = true; return 0; }
            bufferFetchKey = MacMetalStates_KeyView(&bufferFetchStorage);
            if (auto pipeline = s->bufferFetchPipelines.find(bufferFetchKey)) {
                s->pipelines.insert(key, *pipeline); return 1;
            }
        }
        id<MTLLibrary> vs = (__bridge id<MTLLibrary>)vertex, ps = (__bridge id<MTLLibrary>)fragment;
        if (vs.device != s->device || ps.device != s->device) { s->failed = true; return 0; }
        MTL4RenderPipelineDescriptor *d = [MTL4RenderPipelineDescriptor new];
        MTL4LibraryFunctionDescriptor *vf = [MTL4LibraryFunctionDescriptor new], *pf = [MTL4LibraryFunctionDescriptor new];
        vf.library = vs; vf.name = [NSString stringWithUTF8String:r->vertex.entryPoint];
        pf.library = ps; pf.name = [NSString stringWithUTF8String:r->fragment.entryPoint];
        d.vertexFunctionDescriptor = vf; d.fragmentFunctionDescriptor = pf;
        d.rasterSampleCount = r->samples; d.inputPrimitiveTopology = (MTLPrimitiveTopologyClass)r->topology;
        // Alpha test/fixed function/specialization are implemented by WS32's
        // named library entries, never silently replaced by generic shaders.
        if (n->render[181] && n->render[181] != 0x434f5441) { s->failed = true; return 0; }
        d.alphaToCoverageState = n->render[181] ? MTL4AlphaToCoverageStateEnabled : MTL4AlphaToCoverageStateDisabled;
        if (r->hardwareVertexFetch) {
            MTLVertexDescriptor *vd = [MTLVertexDescriptor vertexDescriptor];
            for (unsigned i = 0; i < r->attributeCount; ++i) {
                const auto &a = r->attributes[i];
                vd.attributes[a.attribute].format = (MTLVertexFormat)a.format;
                vd.attributes[a.attribute].offset = a.offset; vd.attributes[a.attribute].bufferIndex = MAC_METAL_STREAM_BUFFER + a.stream;
            }
            for (unsigned i = 0; i < 16; ++i) if (r->strides[i]) { vd.layouts[MAC_METAL_STREAM_BUFFER + i].stride = r->strides[i]; vd.layouts[MAC_METAL_STREAM_BUFFER + i].stepFunction = MTLVertexStepFunctionPerVertex; }
            d.vertexDescriptor = vd;
        }
        for (unsigned i = 0; i < 4; ++i) {
            auto *c = d.colorAttachments[i]; c.pixelFormat = (MTLPixelFormat)r->colorFormats[i];
            c.writeMask = WriteMask(n->render[i ? 189 + i : 168]);
            c.blendingState = n->render[27] ? MTL4BlendStateEnabled : MTL4BlendStateDisabled;
            bool separate = n->render[206];
            MTLBlendFactor sourceFactor, destFactor;
            BlendPair(n->render[19], n->render[20], &sourceFactor, &destFactor);
            c.sourceRGBBlendFactor = sourceFactor; c.destinationRGBBlendFactor = destFactor; c.rgbBlendOperation = (MTLBlendOperation)(n->render[171] - 1);
            BlendPair(n->render[separate ? 207 : 19], n->render[separate ? 208 : 20], &sourceFactor, &destFactor);
            c.sourceAlphaBlendFactor = sourceFactor; c.destinationAlphaBlendFactor = destFactor; c.alphaBlendOperation = (MTLBlendOperation)(n->render[separate ? 209 : 171] - 1);
        }
        NSError *error = nil;
        id<MTLRenderPipelineState> pipeline;
        if (auto archive = s->archives.find(key)) {
            pipeline = [*archive newRenderPipelineStateWithDescriptor:d error:&error];
            if (pipeline) ++s->stats.archiveHits; else ++s->stats.archiveMisses;
        }
        if (!pipeline) {
            MTL4CompilerTaskOptions *options = [MTL4CompilerTaskOptions new];
            NSMutableArray *archives = [NSMutableArray new];
            for (const auto &entry : s->archives.all()) if (![archives containsObject:entry.second]) [archives addObject:entry.second];
            options.lookupArchives = archives;
            error = nil;
            pipeline = [s->compiler newRenderPipelineStateWithDescriptor:d compilerTaskOptions:options error:&error];
            if (pipeline) [s->compiledKeys addObject:[[NSData dataWithBytes:key.bytes length:key.length] base64EncodedStringWithOptions:0]];
        }
        if (!pipeline) { fprintf(stderr, "Metal states: pipeline preparation failed: %s\n", error.localizedDescription.UTF8String); s->failed = true; return 0; }
        ++s->stats.pipelineCreates; s->pipelines.insert(key, pipeline);
        if (!r->hardwareVertexFetch) s->bufferFetchPipelines.insert(bufferFetchKey, pipeline);
        return 1;
    }
    return 0;
}
static bool Persist(MacMetalStates *s) API_AVAILABLE(macos(26.0));
static void PruneGenerations(MacMetalStates *s, NSArray *records)
{
    NSMutableSet *live = [NSMutableSet new];
    for (NSDictionary *record in records) {
        [live addObject:record[@"file"]];
        if (record[@"script"]) [live addObject:record[@"script"]];
    }
    for (NSString *file in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:s->directory error:nil]) {
        NSString *prefix = [file hasPrefix:@"archive-"] && [file hasSuffix:@".metallib"] ? @"archive-" : [file hasPrefix:@"pipelines-"] && [file hasSuffix:@".json"] ? @"pipelines-" : nil;
        if (!prefix || [live containsObject:file]) continue;
        NSString *uuid = [file.stringByDeletingPathExtension substringFromIndex:prefix.length];
        if (![[NSUUID alloc] initWithUUIDString:uuid]) continue;
        NSString *path = [s->directory stringByAppendingPathComponent:file];
        NSString *type = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:nil][NSFileType];
        if ([type isEqual:NSFileTypeRegular] || [type isEqual:NSFileTypeSymbolicLink]) [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
    }
}
static bool Persist(MacMetalStates *s)
{
    if (![SupportPath(s->directory.UTF8String) isEqual:s->directory] || s->compiledKeys.count > ArchiveKeyLimit) return false;
    NSMutableArray *records = [s->archiveRecords mutableCopy];
    if (s->compiledKeys.count) {
        NSString *generation = NSUUID.UUID.UUIDString;
        NSString *archiveName = [@"archive-" stringByAppendingFormat:@"%@.metallib", generation];
        NSString *scriptName = [@"pipelines-" stringByAppendingFormat:@"%@.json", generation];
        NSString *archivePath = [s->directory stringByAppendingPathComponent:archiveName];
        NSString *scriptPath = [s->directory stringByAppendingPathComponent:scriptName];
        NSError *error = nil;
        BOOL archiveWritten = [s->serializer serializeAsArchiveAndFlushToURL:[NSURL fileURLWithPath:archivePath] error:&error];
        NSData *script = [s->serializer serializeAsPipelinesScriptWithError:&error];
        BOOL scriptWritten = script && [script writeToFile:scriptPath options:NSDataWritingAtomic error:&error];
        if (!archiveWritten || (script && !scriptWritten)) {
            fprintf(stderr, "Metal states: archive write failed: %s\n", error.localizedDescription.UTF8String); return false;
        }
        NSData *archive = [NSData dataWithContentsOfFile:archivePath];
        if (!archive) return false;
        // A failed old archive entry can be rebuilt; don't publish duplicate keys.
        NSSet *newKeys = [NSSet setWithArray:s->compiledKeys];
        for (NSUInteger i = records.count; i-- > 0;) {
            NSMutableDictionary *old = [records[i] mutableCopy]; NSMutableArray *keys = [old[@"keys"] mutableCopy];
            [keys filterUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id key, NSDictionary *) { return ![newKeys containsObject:key]; }]];
            if (!keys.count) [records removeObjectAtIndex:i];
            else { old[@"keys"] = keys; old[@"count"] = @(keys.count); records[i] = old; }
        }
        NSString *archiveDigest = Digest(archive), *scriptDigest = scriptWritten ? Digest(script) : nil;
        if (!archiveDigest || (scriptWritten && !scriptDigest)) return false;
        NSMutableDictionary *record = [@{ @"file":archiveName, @"sha256":archiveDigest, @"keys":[s->compiledKeys copy], @"count":@(s->compiledKeys.count) } mutableCopy];
        if (scriptWritten) { record[@"script"] = scriptName; record[@"scriptSHA256"] = scriptDigest; }
        [records addObject:record];
    }
    if (records.count > ArchiveRecordLimit) return false;
    NSDictionary *manifest = @{ @"schema":@(MAC_METAL_STATE_KEY_SCHEMA), @"identity":s->cacheIdentity, @"archives":records };
    NSData *json = [NSJSONSerialization dataWithJSONObject:manifest options:NSJSONWritingSortedKeys error:nil];
    if (!json || json.length > ManifestLimit || ![json writeToFile:[s->directory stringByAppendingPathComponent:@"manifest.json"] options:NSDataWritingAtomic error:nil]) return false;
    s->archiveRecords = records; ++s->stats.archiveWrites;
    // The exclusive setup lock protects readers while the new manifest is
    // published first, then only our obsolete UUID generation files are pruned.
    PruneGenerations(s, records);
    return true;
}
int MacMetalStates_Publish(MacMetalStates *s)
{
    if (!Factory(s)) return 0;
    if (@available(macOS 26.0, *)) {
        bool success = !s->failed && Persist(s);
        s->ready = success; s->loading = false; Unlock(s);
        s->compiler = nil; s->serializer = nil;
        return success;
    }
    return 0;
}
static void RecordMiss(MacMetalStates *s, const MacMetalPipelineKey *key)
{
    if (!key || !MacMetalByteCache<int>::valid(*key)) { ++s->stats.invalidKeys; return; }
    if (s->misses.find(*key)) { ++s->stats.coalescedMisses; return; }
    if (s->misses.size() == 1024) { ++s->stats.missOverflow; return; }
    s->misses.insert(*key, 1); ++s->stats.uniqueMisses;
}
template<class T> static void *Ready(MacMetalStates *s, const MacMetalPipelineKey *key, const MacMetalByteCache<T> &cache, unsigned kind, uint64_t *hits, uint64_t *misses)
{
    if (!s->ready) return nullptr; // No lookup or miss recording until published.
    if (key && MacMetalByteCache<int>::valid(*key) && key->bytes[3] == kind) {
        if (const T *object = cache.find(*key)) { ++*hits; return (__bridge void *)*object; }
    }
    ++*misses; RecordMiss(s, key); return nullptr; // Drop/coalesce, never wait/compile.
}
void *MacMetalStates_ReadyPipeline(MacMetalStates *s, const MacMetalPipelineKey *key)
{
    return s ? Ready(s, key, s->pipelines, 1, &s->stats.pipelineHits, &s->stats.pipelineMisses) : nullptr;
}
void *MacMetalStates_ReadyDepth(MacMetalStates *s, const MacMetalPipelineKey *key)
{
    return s ? Ready(s, key, s->depths, 2, &s->stats.depthHits, &s->stats.depthMisses) : nullptr;
}
void *MacMetalStates_ReadySampler(MacMetalStates *s, const MacMetalPipelineKey *key)
{
    return s ? Ready(s, key, s->samplers, 3, &s->stats.samplerHits, &s->stats.samplerMisses) : nullptr;
}
MacMetalStateStats MacMetalStates_Stats(const MacMetalStates *s) { return s ? s->stats : MacMetalStateStats{}; }
size_t MacMetalStates_Misses(const MacMetalStates *s, MacMetalKeyStorage *out, size_t capacity)
{
    if (!s) return 0;
    size_t count = 0;
    for (const auto &entry : s->misses.all()) {
        if (out && count < capacity) {
            std::memcpy(out[count].bytes, entry.first.data(), entry.first.size()); out[count].length = entry.first.size(); out[count].schemaVersion = MAC_METAL_STATE_KEY_SCHEMA;
        }
        ++count;
    }
    return count;
}
static void *ReadyCallback(const MacMetalPipelineKey *key) { return MacMetalStates_ReadyPipeline(active, key); }
static int PrepareCallback(const char *name, const char *path)
{
    if (managed) {
        MacMetalGPU *gpu = MacMetal_GPU();
        if (active && active->gpu != gpu) { MacMetalStates *old = active; active = nullptr; MacMetalStates_Destroy(old); }
        if (!active) active = MacMetalStates_Create(gpu);
    }
    return MacMetalStates_BeginLoading(active, name, path);
}
int MacMetalStates_Install(MacMetalStates *s)
{
    MacMetalStateOps ops = { MAC_METAL_ABI_VERSION, s ? MacMetalStates_Update : nullptr, s ? ReadyCallback : nullptr, s ? PrepareCallback : nullptr };
    if (!MacMetal_InstallStates(&ops)) return 0;
    MacMetalStates *old = managed ? active : nullptr;
    active = s; managed = false;
    if (old && old != s) MacMetalStates_Destroy(old);
    return 1;
}
int MacMetalStates_InstallDefault(void)
{
    MacMetalStateOps ops = { MAC_METAL_ABI_VERSION, MacMetalStates_Update, ReadyCallback, PrepareCallback };
    if (!MacMetal_InstallStates(&ops)) return 0;
    MacMetalStates *old = managed ? active : nullptr; active = nullptr; managed = true;
    if (old) MacMetalStates_Destroy(old);
    return 1;
}
MacMetalStates *MacMetalStates_Current(void) { return active; }
