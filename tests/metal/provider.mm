#include "shader_provider.h"
#include "shader_archive.h"
#include "arb_to_msl.h"
#include "shader_fixed.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

struct MacShimSidecar {
    MacShimKind kind;
    void *payload = nullptr;
    MacMetalRetireFn release = nullptr;
};
static MacMetalShaderOps installed;
extern "C" int MacMetal_InstallShaders(const MacMetalShaderOps *ops) { installed = *ops; return 1; }
extern "C" MacMetalGPU *MacMetal_GPU(void) { return nullptr; }
extern "C" void *MacMetalGPU_Device(MacMetalGPU *) { return nullptr; }
extern "C" MacShimKind MacMetal_SidecarKind(const MacShimSidecar *side) { return side->kind; }
extern "C" void MacMetal_SetSidecarPayload(MacShimSidecar *side, void *payload, MacMetalRetireFn release)
{
    if (side->release) side->release(side->payload);
    side->payload = payload; side->release = release;
}

int main(int argc, char **argv)
{
    bool crossProcess = argc == 4 && !strcmp(argv[3], "--cross-process");
    if (argc != 3 && !crossProcess) return 2;
    @autoreleasepool {
        assert(installed.abiVersion == MAC_METAL_ABI_VERSION);
        if (!installed.prepare(argv[1], argv[2])) {
            std::cout << "SKIP or failed: " << MacMetalShaders_LastError() << '\n';
            return MacMetalShaders_LastStatus() == 77 ? 77 : 1;
        }
        assert(MacMetalShaders_Count() == 417);
        assert(MacMetalShaders_FixedLibrary(nullptr));
        uint32_t cubeTypes[8] = {1}, volumeTypes[8] = {2}, badTypes[8] = {3};
        assert(!MacMetalShaders_PrepareFixed(badTypes));
        assert(MacMetalShaders_PrepareFixed(cubeTypes) && MacMetalShaders_PrepareFixed(volumeTypes));
        assert(strlen(MacMetalShaders_FixedSourceSHA256(nullptr)) == 64);
        assert(strcmp(MacMetalShaders_FixedSourceSHA256(nullptr), MacMetalShaders_FixedSourceSHA256(cubeTypes)));
        assert(strcmp(MacMetalShaders_FixedSourceSHA256(cubeTypes), MacMetalShaders_FixedSourceSHA256(volumeTypes)));
        uint64_t preparedFactories = MacMetalShaders_FactoryCount();
        assert(MacMetalShaders_PrepareFixed(cubeTypes));
        assert(MacMetalShaders_FactoryCount() == preparedFactories);
        for (const char *entry : {MAC_METAL_FIXED_VERTEX, MAC_METAL_FIXED_POST_VERTEX, MAC_METAL_FIXED_FRAGMENT,
                                 MAC_METAL_FIXED_COLORED, MAC_METAL_FIXED_SOLID, MAC_METAL_FIXED_COPY, MAC_METAL_FIXED_GAMMA}) {
            assert(MacMetalShaders_FixedFunction(nullptr, entry));
            assert(MacMetalShaders_FixedFunction(cubeTypes, entry));
            assert(MacMetalShaders_FixedFunction(volumeTypes, entry));
        }
        unsigned count = 0;
        for (unsigned n = 0; n < MacMetalShaders_Count(); ++n) {
            MacMetalShaderLibrary desc = {};
            assert(MacMetalShaders_Describe(n, &desc));
            assert(desc.abiVersion == 1 && strlen(desc.sourceSHA256) == 64);
            MacShimSidecar side{desc.stage == 0 ? MAC_SHIM_VERTEX_SHADER : MAC_SHIM_PIXEL_SHADER};
            void *payload = installed.load(&side, &desc);
            assert(payload && payload == side.payload);
            id<MTLLibrary> library = (__bridge id<MTLLibrary>)MacMetalShaders_Library(payload);
            id<MTLFunction> function = (__bridge id<MTLFunction>)MacMetalShaders_Function(payload);
            assert(library && function && [function.name isEqualToString:@(desc.entryPoint)]);
            MacMetalARBConstants registers = {};
            float value[4] = {-0.0f, 0.25f, 0.5f, 1};
            assert(MacMetalShaders_WriteLocal(payload, 255, value, 1));
            assert(!MacMetalShaders_WriteLocal(payload, 255, value, 2));
            assert(MacMetalShaders_Snapshot(payload, &registers, &registers));
            assert(!memcmp(registers.local[255].v, value, sizeof(value)));
            MacMetalState shadow = {}; shadow.render[34] = 0xff4080c0;
            assert(MacMetalShaders_SnapshotState(payload, &shadow, &registers));
            if (MacMetalShaders_StateBinding(payload, 0)) {
                assert(registers.state[0].v[0] == 64.f / 255.f);
                assert(registers.state[0].v[1] == 128.f / 255.f);
                assert(registers.state[0].v[2] == 192.f / 255.f);
            }
            MacMetalShaderLibrary wrong = desc;
            wrong.sourceSHA256 = "0000000000000000000000000000000000000000000000000000000000000000";
            assert(!installed.load(&side, &wrong));
            wrong = desc; wrong.reflectionSize -= 1;
            assert(!installed.load(&side, &wrong));
            wrong = desc; wrong.entryPoint = "arb_test";
            assert(!installed.load(&side, &wrong));
            side.release(side.payload);
            ++count;
        }
        uint64_t factories = MacMetalShaders_FactoryCount();
        if (@available(macOS 26.0, *)) {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            MTLCompileOptions *options = [MTLCompileOptions new]; options.fastMathEnabled = NO;
            auto vsSource = cod2::arb::Translate("!!ARBvp1.0\nMOV result.position, vertex.position; MOV result.color, vertex.color; END\n").source;
            auto psSource = cod2::arb::Translate("!!ARBfp1.0\nMOV result.color, fragment.color; END\n").source;
            id<MTLLibrary> vs = [device newLibraryWithSource:@(vsSource.c_str()) options:options error:nil];
            id<MTLLibrary> ps = [device newLibraryWithSource:@(psSource.c_str()) options:options error:nil];
            assert(vs && ps);
            std::string directory = std::string(argv[2]) + "/linked-fixture-" + NSUUID.UUID.UUIDString.UTF8String;
            NSString *fixtureIdentity = [NSString stringWithFormat:@"%s/provider-last-fixture.json", argv[2]];
            if (crossProcess) {
                NSData *data = [NSData dataWithContentsOfFile:fixtureIdentity];
                NSDictionary *previous = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
                assert([previous[@"namespace"] isEqualToString:@(MacMetalShaders_CacheNamespace())]);
                directory = [previous[@"directory"] UTF8String];
            }
            assert(!MacMetalShaderArchive_Open((__bridge void *)device, "/tmp/ws32-disallowed", MacMetalShaders_CacheNamespace()));
            assert(!MacMetalShaderArchive_Open((__bridge void *)device, directory.c_str(), "not-a-namespace"));
            for (int pass = 0; pass < (crossProcess ? 1 : 6); ++pass) {
                if (pass == 4) {
                    NSString *metadata = [NSString stringWithFormat:@"%s/%s/archive-v1.json", directory.c_str(), MacMetalShaders_CacheNamespace()];
                    assert([[@"{}" dataUsingEncoding:NSUTF8StringEncoding] writeToFile:metadata atomically:YES]);
                }
                auto *archive = MacMetalShaderArchive_Open((__bridge void *)device, directory.c_str(), MacMetalShaders_CacheNamespace());
                assert(archive);
                assert(!MacMetalShaderArchive_Open((__bridge void *)device, directory.c_str(), MacMetalShaders_CacheNamespace()));
                for (unsigned n = 0; n < 417; ++n) {
                    MacMetalShaderLibrary desc = {}; assert(MacMetalShaders_Describe(n, &desc));
                    MacShimSidecar side{desc.stage == 0 ? MAC_SHIM_VERTEX_SHADER : MAC_SHIM_PIXEL_SHADER};
                    void *payload = installed.load(&side, &desc); assert(payload);
                    id<MTLLibrary> translated = (__bridge id<MTLLibrary>)MacMetalShaders_Library(payload);
                    MTL4LibraryFunctionDescriptor *vertex = [MTL4LibraryFunctionDescriptor new];
                    vertex.library = desc.stage == 0 ? translated : vs; vertex.name = @"arb_vertex";
                    MTL4LibraryFunctionDescriptor *fragment = [MTL4LibraryFunctionDescriptor new];
                    fragment.library = desc.stage == 1 ? translated : ps; fragment.name = @"arb_fragment";
                    MTL4RenderPipelineDescriptor *pipeline = [MTL4RenderPipelineDescriptor new];
                    pipeline.label = @(desc.name); pipeline.vertexFunctionDescriptor = vertex; pipeline.fragmentFunctionDescriptor = fragment;
                    pipeline.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
                    id<MTLRenderPipelineState> native = (__bridge_transfer id<MTLRenderPipelineState>)MacMetalShaderArchive_Render(archive, (__bridge void *)pipeline);
                    assert(native);
                    side.release(side.payload);
                }
                if (crossProcess || pass >= 2) {
                    id<MTLLibrary> fixed = (__bridge id<MTLLibrary>)MacMetalShaders_FixedLibrary(nullptr);
                    for (unsigned pair = 0; pair < 3; ++pair) {
                        MTL4LibraryFunctionDescriptor *vertex = [MTL4LibraryFunctionDescriptor new];
                        vertex.library = pair == 1 ? vs : fixed; vertex.name = pair == 1 ? @"arb_vertex" : @MAC_METAL_FIXED_VERTEX;
                        MTL4LibraryFunctionDescriptor *fragment = [MTL4LibraryFunctionDescriptor new];
                        fragment.library = pair == 0 ? ps : fixed; fragment.name = pair == 0 ? @"arb_fragment" : @MAC_METAL_FIXED_FRAGMENT;
                        MTL4RenderPipelineDescriptor *pipeline = [MTL4RenderPipelineDescriptor new];
                        pipeline.vertexFunctionDescriptor = vertex; pipeline.fragmentFunctionDescriptor = fragment;
                        pipeline.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
                        id<MTLRenderPipelineState> native = (__bridge_transfer id<MTLRenderPipelineState>)MacMetalShaderArchive_Render(archive, (__bridge void *)pipeline);
                        assert(native);
                    }
                }
                unsigned hits = MacMetalShaderArchive_Hits(archive);
                assert(MacMetalShaderArchive_Finish(archive));
                assert(!MacMetalShaderArchive_Render(archive, (__bridge void *)[MTL4RenderPipelineDescriptor new]));
                if (pass == 1) assert(hits == 417);
                if (pass == 2) assert(hits == 417);
                if (pass == 3 || pass == 5) assert(hits == 420);
                if (pass == 4) assert(hits == 0);
                if (crossProcess) assert(hits == 420);
                std::cout << (crossProcess ? "fresh process" : "pass") << ' ' << pass << ": " << (crossProcess || pass >= 2 ? 420 : 417) << " Metal4 PSOs linked; archive hits=" << hits << '\n';
                MacMetalShaderArchive_Close(archive);
            }
            if (!crossProcess) {
                NSData *identity = [NSJSONSerialization dataWithJSONObject:@{@"directory": @(directory.c_str()), @"namespace": @(MacMetalShaders_CacheNamespace())} options:0 error:nil];
                assert([identity writeToFile:fixtureIdentity options:NSDataWritingAtomic error:nil]);
            }
        }
        MacMetalShaders_SealLoading();
        MacMetalShaderLibrary desc = {};
        assert(MacMetalShaders_Describe(0, &desc));
        MacShimSidecar side{desc.stage == 0 ? MAC_SHIM_VERTEX_SHADER : MAC_SHIM_PIXEL_SHADER};
        assert(!installed.load(&side, &desc));
        assert(!installed.prepare(argv[1], argv[2]));
        assert(!MacMetalShaders_PrepareFixed(nullptr));
        assert(MacMetalShaders_FixedFunction(nullptr, MAC_METAL_FIXED_FRAGMENT));
        assert(MacMetalShaders_FactoryCount() == factories);
        std::cout << count << "/417 runtime libraries loaded; invalid identities rejected; sealed factory count unchanged\n";
    }
}
