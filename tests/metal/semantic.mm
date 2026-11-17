/* Standalone local-cache differential test; never linked into the game.
 * API references: MTLDevice.h/MTLLibrary.h/MTLTexture.h in the active SDK;
 * https://developer.apple.com/documentation/metal/mtldevice/makelibrary(source:options:)
 * https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_pixelformats/opengl_pixelformats.html
 */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using V4 = simd_float4;
struct ARBInput { V4 attrib[16], texcoord[8], color[2], position, fogcoord; };
struct ARBConstants { V4 env[256], local[256], state[64]; };
struct ARBOutput { V4 position, color[2], texcoord[8], fogcoord, depth; uint32_t killed; };
static_assert(sizeof(ARBInput) == 448 && sizeof(ARBConstants) == 9216 &&
              sizeof(ARBOutput) == 224, "MSL host layout changed");

static std::string Read(const char *path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error(std::string("cannot read ") + path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static void Require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}
static void GLCheck(const char *operation)
{
    GLenum error = glGetError();
    if (error) {
        std::ostringstream message;
        message << operation << ": GL error 0x" << std::hex << error;
        throw std::runtime_error(message.str());
    }
}
static GLuint GLProgram(GLenum target, const std::string& source)
{
    GLuint program;
    glGenProgramsARB(1, &program);
    glBindProgramARB(target, program);
    glProgramStringARB(target, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)source.size(), source.data());
    GLenum error = glGetError();
    if (error) {
        GLint offset = -1;
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &offset);
        const GLubyte *detail = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
        std::ostringstream message;
        message << "GL reference compile rejected instruction stream at " << offset
                << ": " << (detail ? (const char *)detail : "no diagnostic");
        glDeleteProgramsARB(1, &program);
        throw std::runtime_error(message.str());
    }
    return program;
}
struct Output { std::string alias, target; int mask = 0; size_t offset = 0; };
static std::vector<Output> Outputs(const std::string& source)
{
    std::vector<Output> result;
    const std::regex declarations(R"(OUTPUT\s+(\w+)\s*=\s*(result\.[\w.]+(?:\[\s*\d+\s*\])?)\s*;)");
    for (std::sregex_iterator i(source.begin(), source.end(), declarations), end; i != end; ++i) {
        Output output{(*i)[1], (*i)[2]};
        std::string target = std::regex_replace(output.target, std::regex(R"(\s+)"), "");
        if (target == "result.position") output.offset = offsetof(ARBOutput, position);
        else if (target == "result.color" || target == "result.color.primary") output.offset = offsetof(ARBOutput, color);
        else if (target == "result.color.secondary") output.offset = offsetof(ARBOutput, color) + sizeof(V4);
        else if (target == "result.fogcoord") output.offset = offsetof(ARBOutput, fogcoord);
        else if (target == "result.depth") output.offset = offsetof(ARBOutput, depth);
        else if (target.rfind("result.texcoord[", 0) == 0)
            output.offset = offsetof(ARBOutput, texcoord) + sizeof(V4) * std::stoi(target.substr(16));
        else throw std::runtime_error("unknown output target " + target);
        std::regex writes("(?:^|;)\\s*[A-Z][A-Z0-9_]*\\s+" + output.alias + "(?:\\.([xyzwrgba]+))?\\s*,");
        for (std::sregex_iterator j(source.begin(), source.end(), writes); j != end; ++j) {
            std::string mask = (*j)[1];
            if (mask.empty()) output.mask = 15;
            for (char component : mask) {
                size_t n = std::string("xyzw").find(component);
                if (n == std::string::npos) n = std::string("rgba").find(component);
                output.mask |= 1 << n;
            }
        }
        if (target == "result.fogcoord" || target == "result.depth") output.mask &= 1;
        if (output.mask) result.push_back(output);
    }
    Require(!result.empty(), "no written output components discovered");
    return result;
}
/* Legacy ARBvp has no transform feedback. Redirect OUTPUT aliases to temporaries
 * without changing original instructions, then relay through a float texcoord.
 * Only original write-mask components participate in comparison. */
static std::string VertexRelay(const std::string& source, const Output& output)
{
    std::regex declarations(R"(OUTPUT\s+(\w+)\s*=\s*result\.[\w.]+(?:\[\s*\d+\s*\])?\s*;)");
    std::string rerouted = std::regex_replace(source, declarations, "TEMP $1;");
    size_t end = rerouted.rfind("END");
    Require(end != std::string::npos, "vertex program has no END");
    rerouted.replace(end, 3, "MOV result.texcoord[0], " + output.alias +
                     ";\nMOV result.position, { 0, 0, 0, 1 };\nEND");
    return rerouted;
}
static void Synthetic(unsigned sample, ARBInput& input, ARBConstants& constants)
{
    memset(&input, 0, sizeof(input));
    memset(&constants, 0, sizeof(constants));
    for (int n = 0; n < 256; ++n)
        for (int c = 0; c < 4; ++c) {
            constants.env[n][c] = .11f + .017f * ((n * 7 + c * 3 + sample * 5) % 37);
            constants.local[n][c] = .09f + .013f * ((n * 3 + c * 7 + sample * 11) % 43);
        }
    for (int n = 0; n < 16; ++n)
        for (int c = 0; c < 4; ++c)
            input.attrib[n][c] = .13f + .019f * ((n * 5 + c * 7 + sample * 3) % 31);
    for (int n = 0; n < 8; ++n) input.texcoord[n] = input.attrib[n + 8];
    if (sample >= 4) input.attrib[3][3] = .93f;
    input.color[0] = input.attrib[3];
    input.color[1] = input.attrib[4];
    input.fogcoord = input.attrib[5];
    input.position = V4{.5f, .5f, .5f, 1};
    constants.state[0] = V4{.21f, .37f, .49f, .63f};
}
struct Texture {
    int unit; std::string kind; GLuint gl;
    id<MTLTexture> metal; id<MTLSamplerState> sampler;
};
static std::vector<Texture> Textures(id<MTLDevice> device, NSDictionary *metadata)
{
    std::vector<Texture> result;
    NSDictionary *bindings = metadata[@"textures"];
    for (NSString *key in bindings) {
        Texture texture{};
        texture.unit = key.intValue;
        texture.kind = [bindings[key] UTF8String];
        GLenum target = texture.kind == "CUBE" ? GL_TEXTURE_CUBE_MAP :
                        texture.kind == "3D" ? GL_TEXTURE_3D : GL_TEXTURE_2D;
        int depth = texture.kind == "3D" ? 4 : 1;
        int faces = texture.kind == "CUBE" ? 6 : 1;
        MTLTextureDescriptor *descriptor = [[MTLTextureDescriptor alloc] init];
        descriptor.textureType = texture.kind == "CUBE" ? MTLTextureTypeCube :
                                 texture.kind == "3D" ? MTLTextureType3D : MTLTextureType2D;
        descriptor.pixelFormat = MTLPixelFormatRGBA32Float;
        descriptor.width = descriptor.height = 4;
        descriptor.depth = depth;
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.usage = MTLTextureUsageShaderRead;
        texture.metal = [device newTextureWithDescriptor:descriptor];
        Require(texture.metal != nil, "Metal texture allocation failed");
        MTLSamplerDescriptor *sampler = [[MTLSamplerDescriptor alloc] init];
        sampler.minFilter = sampler.magFilter = MTLSamplerMinMagFilterNearest;
        sampler.mipFilter = MTLSamplerMipFilterNotMipmapped;
        sampler.sAddressMode = sampler.tAddressMode = sampler.rAddressMode =
            texture.kind == "CUBE" ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        texture.sampler = [device newSamplerStateWithDescriptor:sampler];
        glActiveTextureARB(GL_TEXTURE0_ARB + texture.unit);
        glGenTextures(1, &texture.gl);
        glBindTexture(target, texture.gl);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, texture.kind == "CUBE" ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, texture.kind == "CUBE" ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_R, texture.kind == "CUBE" ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        for (int face = 0; face < faces; ++face) {
            std::vector<V4> pixels(16 * depth);
            for (int n = 0; n < 16 * depth; ++n)
                for (int c = 0; c < 4; ++c)
                    pixels[n][c] = c == 3 ? .62f + .012f * ((n + face + texture.unit * 3) % 29) :
                        .12f + .023f * ((n * 3 + c * 5 + face * 7 + texture.unit * 11) % 29);
            if (texture.kind == "3D")
                glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F_ARB, 4, 4, depth, 0, GL_RGBA, GL_FLOAT, pixels.data());
            else
                glTexImage2D(texture.kind == "CUBE" ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : target,
                             0, GL_RGBA32F_ARB, 4, 4, 0, GL_RGBA, GL_FLOAT, pixels.data());
            [texture.metal replaceRegion:MTLRegionMake3D(0, 0, 0, 4, 4, depth)
                             mipmapLevel:0 slice:face withBytes:pixels.data()
                             bytesPerRow:4 * sizeof(V4) bytesPerImage:16 * sizeof(V4)];
        }
        GLCheck("synthetic texture upload");
        result.push_back(texture);
    }
    glActiveTextureARB(GL_TEXTURE0_ARB);
    return result;
}
static void GLConstants(GLenum stage, const ARBConstants& constants)
{
    GLint envCount, localCount;
    glGetProgramivARB(stage, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &envCount);
    glGetProgramivARB(stage, GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB, &localCount);
    for (int n = 0; n < std::min(envCount, 256); ++n)
        glProgramEnvParameter4fvARB(stage, n, (const float *)&constants.env[n]);
    for (int n = 0; n < std::min(localCount, 256); ++n)
        glProgramLocalParameter4fvARB(stage, n, (const float *)&constants.local[n]);
    glFogfv(GL_FOG_COLOR, (const float *)&constants.state[0]);
    GLCheck("synthetic constants");
}
static V4 GLDraw(const ARBInput& input)
{
    glClearColor(-37, -38, -39, -40);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int n = 1; n < 16; ++n) glVertexAttrib4fvARB(n, (const float *)&input.attrib[n]);
    glBegin(GL_POINTS);
    glVertexAttrib4fvARB(0, (const float *)&input.attrib[0]);
    glEnd();
    V4 result;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, &result);
    GLCheck("synthetic point draw/readback");
    return result;
}
int main(int argc, char **argv)
{
    @autoreleasepool {
        NSMutableDictionary *report = [@{@"status": @"fail", @"samples": @8,
            @"comparisons": @0, @"max_abs": @0, @"max_rel": @0} mutableCopy];
        NSMutableArray *failures = [NSMutableArray array];
        report[@"failures"] = failures;
        CGLContextObj context = nullptr;
        try {
            Require(argc == 4, "usage: semantic program.arb translated.metal metadata.json");
            std::string source = Read(argv[1]);
            std::string msl = Read(argv[2]);
            NSString *metadataText = [NSString stringWithUTF8String:Read(argv[3]).c_str()];
            NSError *error = nil;
            NSDictionary *metadata = [NSJSONSerialization JSONObjectWithData:[metadataText dataUsingEncoding:NSUTF8StringEncoding]
                                                                  options:0 error:&error];
            Require(metadata && !error, "invalid translator metadata");
            report[@"program"] = [[NSString stringWithUTF8String:argv[1]] lastPathComponent];
            report[@"stage"] = metadata[@"stage"];
            bool vertex = [metadata[@"stage"] isEqualToString:@"vertex"];
            GLenum stage = vertex ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB;
            NSArray *states = metadata[@"state_bindings"];
            Require(states.count <= 1 && (states.count == 0 || [states[0] isEqualToString:@"state.fog.color"]),
                    "harness has no setter for a requested state binding");
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            Require(device != nil, "no Metal device");
            report[@"metal_device"] = device.name;
            MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
            options.preprocessorMacros = @{@"ARB_SEMANTIC_TEST": @1};
            if (@available(macOS 15.0, *)) options.mathMode = MTLMathModeSafe;
            else options.fastMathEnabled = NO;
            id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:msl.c_str()]
                                                          options:options error:&error];
            Require(library != nil, std::string("Metal compile: ") + (error ? error.localizedDescription.UTF8String : "no diagnostic"));
            id<MTLFunction> function = [library newFunctionWithName:@"arb_test"];
            Require(function != nil, "translated library has no arb_test kernel");
            id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function error:&error];
            Require(pipeline != nil, std::string("Metal pipeline: ") + (error ? error.localizedDescription.UTF8String : "no diagnostic"));
            id<MTLCommandQueue> queue = [device newCommandQueue];
            id<MTLBuffer> inputBuffer = [device newBufferWithLength:sizeof(ARBInput) options:MTLResourceStorageModeShared];
            id<MTLBuffer> constantsBuffer = [device newBufferWithLength:sizeof(ARBConstants) options:MTLResourceStorageModeShared];
            id<MTLBuffer> outputBuffer = [device newBufferWithLength:sizeof(ARBOutput) options:MTLResourceStorageModeShared];
            Require(queue && inputBuffer && constantsBuffer && outputBuffer, "Metal allocation failed");
            CGLPixelFormatAttribute attributes[] = {kCGLPFAAccelerated, kCGLPFAColorSize, (CGLPixelFormatAttribute)24, (CGLPixelFormatAttribute)0};
            CGLPixelFormatObj format;
            GLint count;
            Require(!CGLChoosePixelFormat(attributes, &format, &count), "CGL pixel format unavailable");
            CGLError createError = CGLCreateContext(format, nullptr, &context);
            CGLDestroyPixelFormat(format);
            Require(!createError && !CGLSetCurrentContext(context), "CGL legacy context unavailable");
            report[@"gl_renderer"] = [NSString stringWithUTF8String:(const char *)glGetString(GL_RENDERER)];
            report[@"gl_version"] = [NSString stringWithUTF8String:(const char *)glGetString(GL_VERSION)];
            glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE);
            glClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE);
            glClampColorARB(GL_CLAMP_READ_COLOR_ARB, GL_FALSE);
            glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_DITHER);
            GLuint target, framebuffer;
            glGenTextures(1, &target);
            glBindTexture(GL_TEXTURE_2D, target);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
            glGenFramebuffersEXT(1, &framebuffer);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
            Require(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT, "float framebuffer incomplete");
            glViewport(0, 0, 1, 1);
            GLCheck("headless GL setup");
            auto textures = Textures(device, metadata);
            auto outputs = Outputs(source);
            std::vector<GLuint> programs;
            if (vertex) {
                GLProgram(GL_FRAGMENT_PROGRAM_ARB, "!!ARBfp1.0\nMOV result.color, fragment.texcoord[0];\nEND\n");
                for (const auto& output : outputs) programs.push_back(GLProgram(stage, VertexRelay(source, output)));
            } else {
                std::string relay = "!!ARBvp1.0\nMOV result.position, {0,0,0,1};\n"
                    "MOV result.color, vertex.attrib[3];\nMOV result.color.secondary, vertex.attrib[4];\n"
                    "MOV result.fogcoord.x, vertex.attrib[5].x;\n";
                for (int n = 0; n < 8; ++n)
                    relay += "MOV result.texcoord[" + std::to_string(n) + "], vertex.attrib[" + std::to_string(n + 8) + "];\n";
                relay += "END\n";
                GLProgram(GL_VERTEX_PROGRAM_ARB, relay);
                programs.push_back(GLProgram(stage, source));
            }
            glEnable(GL_VERTEX_PROGRAM_ARB); glEnable(GL_FRAGMENT_PROGRAM_ARB);
            double maxAbs = 0, maxRel = 0;
            unsigned comparisons = 0, nonfinite = 0;
            for (unsigned sample = 0; sample < 8; ++sample) {
                auto& input = *(ARBInput *)inputBuffer.contents;
                auto& constants = *(ARBConstants *)constantsBuffer.contents;
                Synthetic(sample, input, constants);
                memset(outputBuffer.contents, 0xa5, sizeof(ARBOutput));
                id<MTLCommandBuffer> commands = [queue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [commands computeCommandEncoder];
                [encoder setComputePipelineState:pipeline];
                [encoder setBuffer:inputBuffer offset:0 atIndex:0];
                [encoder setBuffer:constantsBuffer offset:0 atIndex:1];
                [encoder setBuffer:outputBuffer offset:0 atIndex:2];
                for (const auto& texture : textures) {
                    [encoder setTexture:texture.metal atIndex:texture.unit];
                    [encoder setSamplerState:texture.sampler atIndex:texture.unit];
                }
                [encoder dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
                [encoder endEncoding]; [commands commit]; [commands waitUntilCompleted];
                Require(commands.status == MTLCommandBufferStatusCompleted,
                        std::string("Metal GPU execution: ") + (commands.error ? commands.error.localizedDescription.UTF8String : "failed"));
                const auto& metal = *(const ARBOutput *)outputBuffer.contents;
                for (size_t outputIndex = 0; outputIndex < (vertex ? outputs.size() : 1); ++outputIndex) {
                    glBindProgramARB(stage, programs[outputIndex]);
                    GLConstants(stage, constants);
                    V4 actual = GLDraw(input);
                    bool discarded = actual[0] == -37 && actual[1] == -38 && actual[2] == -39 && actual[3] == -40;
                    if (!vertex && discarded != (metal.killed != 0))
                        [failures addObject:@{@"sample": @(sample), @"reason": @"discard mismatch",
                            @"gl_discard": @(discarded), @"metal_discard": @(metal.killed != 0)}];
                    if (discarded) continue;
                    const Output& output = outputs[outputIndex];
                    Require(vertex || output.offset == offsetof(ARBOutput, color), "fragment output requires a new readback attachment");
                    const V4& expected = *(const V4 *)((const char *)&metal + output.offset);
                    for (int component = 0; component < 4; ++component) {
                        if (!(output.mask & (1 << component))) continue;
                        ++comparisons;
                        if (!std::isfinite(actual[component]) || !std::isfinite(expected[component])) {
                            ++nonfinite;
                            [failures addObject:@{@"sample": @(sample), @"output": [NSString stringWithUTF8String:output.target.c_str()],
                                @"component": @(component), @"reason": @"nonfinite output"}];
                            continue;
                        }
                        double absolute = std::abs((double)actual[component] - expected[component]);
                        double magnitude = std::max(std::abs((double)actual[component]), std::abs((double)expected[component]));
                        double relative = absolute / std::max(1e-6, magnitude);
                        maxAbs = std::max(maxAbs, absolute); maxRel = std::max(maxRel, relative);
                        if (absolute > 2e-6 + 2e-6 * magnitude)
                            [failures addObject:@{@"sample": @(sample), @"output": [NSString stringWithUTF8String:output.target.c_str()],
                                @"component": @(component), @"gl": @(actual[component]), @"metal": @(expected[component]),
                                @"abs": @(absolute), @"rel": @(relative)}];
                    }
                }
            }
            report[@"comparisons"] = @(comparisons); report[@"nonfinite"] = @(nonfinite);
            report[@"max_abs"] = @(maxAbs); report[@"max_rel"] = @(maxRel);
            report[@"absolute_tolerance"] = @2e-6; report[@"relative_tolerance"] = @2e-6;
            report[@"status"] = failures.count ? @"fail" : @"pass";
        } catch (const std::exception& error) {
            report[@"error"] = [NSString stringWithUTF8String:error.what()];
        }
        if (context) { CGLSetCurrentContext(nullptr); CGLDestroyContext(context); }
        NSData *json = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingSortedKeys error:nil];
        fwrite(json.bytes, 1, json.length, stdout); fputc('\n', stdout);
        return [report[@"status"] isEqualToString:@"pass"] ? 0 : 1;
    }
}
