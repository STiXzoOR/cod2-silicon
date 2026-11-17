#include "shader_archive.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <CommonCrypto/CommonDigest.h>
#include <filesystem>
#include <string>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

struct MacMetalShaderArchive {
    id device, compiler, serializer, archive, binaryArchive;
    NSMutableArray *descriptors;
    NSString *directory;
    NSDictionary *identity;
    bool dirty = false, sealed = false;
    unsigned int hits = 0;
    int lock = -1;
};

static NSString *Digest(NSData *data)
{
    if (!data || data.length > UINT32_MAX) return nil;
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, static_cast<CC_LONG>(data.length), hash);
    NSMutableString *result = [NSMutableString string];
    for (unsigned char byte : hash) [result appendFormat:@"%02x", byte];
    return result;
}

static MTLRenderPipelineDescriptor *ArchiveDescriptor(MTL4RenderPipelineDescriptor *source) API_AVAILABLE(macos(26.0));
static MTLRenderPipelineDescriptor *ArchiveDescriptor(MTL4RenderPipelineDescriptor *source)
{
    if (![source.vertexFunctionDescriptor isKindOfClass:MTL4LibraryFunctionDescriptor.class] ||
        (source.fragmentFunctionDescriptor && ![source.fragmentFunctionDescriptor isKindOfClass:MTL4LibraryFunctionDescriptor.class]) ||
        source.supportVertexBinaryLinking || source.supportFragmentBinaryLinking || source.colorAttachmentMappingState != 0 ||
        source.vertexStaticLinkingDescriptor.functionDescriptors.count || source.vertexStaticLinkingDescriptor.privateFunctionDescriptors.count ||
        source.vertexStaticLinkingDescriptor.groups.count || source.fragmentStaticLinkingDescriptor.functionDescriptors.count ||
        source.fragmentStaticLinkingDescriptor.privateFunctionDescriptors.count || source.fragmentStaticLinkingDescriptor.groups.count) return nil;
    MTL4LibraryFunctionDescriptor *vertex = (MTL4LibraryFunctionDescriptor *)source.vertexFunctionDescriptor;
    MTL4LibraryFunctionDescriptor *fragment = (MTL4LibraryFunctionDescriptor *)source.fragmentFunctionDescriptor;
    MTLRenderPipelineDescriptor *target = [MTLRenderPipelineDescriptor new];
    if (source.options.shaderReflection != MTL4ShaderReflectionNone) return nil;
    target.shaderValidation = source.options.shaderValidation;
    target.vertexFunction = [vertex.library newFunctionWithName:vertex.name];
    target.fragmentFunction = fragment ? [fragment.library newFunctionWithName:fragment.name] : nil;
    target.vertexDescriptor = source.vertexDescriptor;
    target.rasterSampleCount = source.rasterSampleCount;
    target.alphaToCoverageEnabled = source.alphaToCoverageState == MTL4AlphaToCoverageStateEnabled;
    target.alphaToOneEnabled = source.alphaToOneState == MTL4AlphaToOneStateEnabled;
    target.rasterizationEnabled = source.rasterizationEnabled;
    target.maxVertexAmplificationCount = source.maxVertexAmplificationCount;
    target.inputPrimitiveTopology = source.inputPrimitiveTopology;
    target.supportIndirectCommandBuffers = source.supportIndirectCommandBuffers == MTL4IndirectCommandBufferSupportStateEnabled;
    for (unsigned n = 0; n < 8; ++n) {
        MTL4RenderPipelineColorAttachmentDescriptor *from = source.colorAttachments[n];
        if (from.blendingState == MTL4BlendStateUnspecialized) return nil;
        MTLRenderPipelineColorAttachmentDescriptor *to = target.colorAttachments[n];
        to.pixelFormat = from.pixelFormat; to.blendingEnabled = from.blendingState == MTL4BlendStateEnabled;
        to.sourceRGBBlendFactor = from.sourceRGBBlendFactor; to.destinationRGBBlendFactor = from.destinationRGBBlendFactor;
        to.rgbBlendOperation = from.rgbBlendOperation; to.sourceAlphaBlendFactor = from.sourceAlphaBlendFactor;
        to.destinationAlphaBlendFactor = from.destinationAlphaBlendFactor; to.alphaBlendOperation = from.alphaBlendOperation;
        to.writeMask = from.writeMask;
    }
    return target;
}

MacMetalShaderArchive *MacMetalShaderArchive_Open(void *opaque, const char *path, const char *namespaceHash)
{
    if (@available(macOS 26.0, *)) {
        if (!opaque || !path || !namespaceHash || strlen(namespaceHash) != 64 || strspn(namespaceHash, "0123456789abcdef") != 64) return nullptr;
        std::error_code pathError;
        auto directory = std::filesystem::weakly_canonical(path, pathError);
        if (pathError) return nullptr;
        auto support = std::filesystem::weakly_canonical([NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support"].fileSystemRepresentation, pathError);
        if (pathError) return nullptr;
        auto relative = directory.lexically_relative(support);
        if (relative.empty() || relative == ".") return nullptr;
        for (const auto &part : relative) if (part == "..") return nullptr;
        id<MTLDevice> device = (__bridge id<MTLDevice>)opaque;
        if (![device supportsFamily:MTLGPUFamilyMetal4]) return nullptr;
        auto *cache = new MacMetalShaderArchive;
        cache->device = device;
        cache->directory = [[NSString stringWithUTF8String:directory.c_str()] stringByAppendingPathComponent:@(namespaceHash)];
        [NSFileManager.defaultManager createDirectoryAtPath:cache->directory withIntermediateDirectories:YES attributes:nil error:nil];
        cache->lock = open([cache->directory stringByAppendingPathComponent:@".archive.lock"].fileSystemRepresentation, O_CREAT | O_RDWR, 0600);
        if (cache->lock < 0 || flock(cache->lock, LOCK_EX | LOCK_NB)) { MacMetalShaderArchive_Close(cache); return nullptr; }
        cache->identity = @{@"abi": @1, @"schema": @1, @"namespace": @(namespaceHash), @"registry_id": @(device.registryID),
            @"os_build": NSProcessInfo.processInfo.operatingSystemVersionString};
        NSString *archivePath = [cache->directory stringByAppendingPathComponent:@"render.bin"];
        NSData *metadata = [NSData dataWithContentsOfFile:[cache->directory stringByAppendingPathComponent:@"archive-v1.json"]];
        NSDictionary *record = metadata ? [NSJSONSerialization JSONObjectWithData:metadata options:0 error:nil] : nil;
        NSData *binary = [NSData dataWithContentsOfFile:archivePath];
        if ([record isKindOfClass:NSDictionary.class] && [record[@"identity"] isEqual:cache->identity] &&
            [record[@"sha256"] isEqual:Digest(binary)])
            cache->archive = [device newArchiveWithURL:[NSURL fileURLWithPath:archivePath] error:nil];
        MTL4PipelineDataSetSerializerDescriptor *serializer = [MTL4PipelineDataSetSerializerDescriptor new];
        serializer.configuration = MTL4PipelineDataSetSerializerConfigurationCaptureDescriptors | MTL4PipelineDataSetSerializerConfigurationCaptureBinaries;
        cache->serializer = [device newPipelineDataSetSerializerWithDescriptor:serializer];
        MTL4CompilerDescriptor *descriptor = [MTL4CompilerDescriptor new];
        descriptor.pipelineDataSetSerializer = cache->serializer;
        cache->compiler = [device newCompilerWithDescriptor:descriptor error:nil];
        cache->binaryArchive = [device newBinaryArchiveWithDescriptor:[MTLBinaryArchiveDescriptor new] error:nil];
        cache->descriptors = [NSMutableArray array];
        if (!cache->compiler || !cache->serializer) { MacMetalShaderArchive_Close(cache); return nullptr; }
        return cache;
    }
    return nullptr;
}

void *MacMetalShaderArchive_Render(MacMetalShaderArchive *cache, void *opaque)
{
    if (@available(macOS 26.0, *)) {
        if (!cache || cache->sealed || !opaque) return nullptr;
        MTL4RenderPipelineDescriptor *descriptor = (__bridge MTL4RenderPipelineDescriptor *)opaque;
        id<MTL4Archive> archive = cache->archive;
        id<MTLRenderPipelineState> pipeline = [archive newRenderPipelineStateWithDescriptor:descriptor error:nil];
        bool hit = pipeline != nil;
        if (hit) ++cache->hits;
        else {
            id<MTL4Compiler> compiler = cache->compiler;
            MTL4CompilerTaskOptions *options = [MTL4CompilerTaskOptions new];
            if (archive) options.lookupArchives = @[archive];
            NSError *error = nil;
            pipeline = [compiler newRenderPipelineStateWithDescriptor:descriptor compilerTaskOptions:options error:&error];
            if (!pipeline) { fprintf(stderr, "Metal shader pipeline: %s\n", error ? error.localizedDescription.UTF8String : "Compiler returned no pipeline"); return nullptr; }
            cache->dirty = true;
        }
        [cache->descriptors addObject:@{@"descriptor": [descriptor copy], @"hit": @(hit)}];
        return (__bridge_retained void *)pipeline;
    }
    return nullptr;
}

int MacMetalShaderArchive_Finish(MacMetalShaderArchive *cache)
{
    if (@available(macOS 26.0, *)) {
        if (!cache || cache->sealed) return 0;
        cache->sealed = true;
        if (!cache->dirty) return 1;
        /* A new archive must include warm hits as well as misses. Capture all
         * hit descriptors before atomic replacement; misses are already captured. */
        id<MTL4Compiler> compiler = cache->compiler;
        MTL4CompilerTaskOptions *options = [MTL4CompilerTaskOptions new];
        if (cache->archive) options.lookupArchives = @[cache->archive];
        for (NSDictionary *record in cache->descriptors) {
            if ([record[@"hit"] boolValue] && ![compiler newRenderPipelineStateWithDescriptor:record[@"descriptor"] compilerTaskOptions:options error:nil]) return 0;
        }
        id<MTL4PipelineDataSetSerializer> serializer = cache->serializer;
        NSString *temporary = [cache->directory stringByAppendingPathComponent:[@".render-" stringByAppendingString:NSUUID.UUID.UUIDString]];
        NSError *error = nil;
        bool nativeWriter = [serializer serializeAsArchiveAndFlushToURL:[NSURL fileURLWithPath:temporary] error:&error];
        if (!nativeWriter) {
            /* macOS 27.0.1's Metal4 serializer returns NO without an NSError.
             * A runtime MTLBinaryArchive writer is verified consumable by
             * MTL4Archive for these fully specialized traditional descriptors.
             * This creates no legacy pipelines, device or command queue. */
            id<MTLBinaryArchive> binaryArchive = cache->binaryArchive;
            for (NSDictionary *record in cache->descriptors) {
                MTLRenderPipelineDescriptor *descriptor = ArchiveDescriptor(record[@"descriptor"]);
                if (!descriptor || ![binaryArchive addRenderPipelineFunctionsWithDescriptor:descriptor error:&error]) {
                    fprintf(stderr, "Metal shader archive descriptor cannot be harvested: %s\n", error ? error.localizedDescription.UTF8String : "Unsupported descriptor");
                    return 0;
                }
            }
            if (![binaryArchive serializeToURL:[NSURL fileURLWithPath:temporary] error:&error]) return 0;
        }
        NSData *binary = [NSData dataWithContentsOfFile:temporary];
        NSString *digest = Digest(binary);
        if (!digest) {
            fprintf(stderr, "Metal shader archive serialization produced no readable file at %s\n", temporary.fileSystemRepresentation);
            unlink(temporary.fileSystemRepresentation); return 0;
        }
        NSDictionary *metadata = @{@"identity": cache->identity, @"sha256": digest, @"pipeline_count": @(cache->descriptors.count),
            @"writer": nativeWriter ? @"MTL4PipelineDataSetSerializer" : @"MTLBinaryArchive"};
        NSData *json = [NSJSONSerialization dataWithJSONObject:metadata options:NSJSONWritingSortedKeys error:nil];
        NSString *target = [cache->directory stringByAppendingPathComponent:@"render.bin"];
        if (rename(temporary.fileSystemRepresentation, target.fileSystemRepresentation)) {
            perror("Metal shader archive rename"); unlink(temporary.fileSystemRepresentation); return 0;
        }
        return [json writeToFile:[cache->directory stringByAppendingPathComponent:@"archive-v1.json"] options:NSDataWritingAtomic error:nil] ? 1 : 0;
    }
    return 0;
}
unsigned int MacMetalShaderArchive_Hits(const MacMetalShaderArchive *cache) { return cache ? cache->hits : 0; }
void MacMetalShaderArchive_Close(MacMetalShaderArchive *cache)
{
    if (!cache) return;
    if (cache->lock >= 0) { flock(cache->lock, LOCK_UN); close(cache->lock); }
    delete cache;
}
