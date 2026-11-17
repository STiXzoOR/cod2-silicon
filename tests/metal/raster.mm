/* Original synthetic GPU fixtures, independent of the licensed shader cache. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <simd/simd.h>
#include "arb_to_msl.h"
#include "metal_abi.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace cod2::arb;
using V4 = simd_float4;
static constexpr unsigned width = 32, height = 24;
static void Require(bool value, const std::string &message)
{
    if (!value) throw std::runtime_error(message);
}
static GLuint GLProgram(GLenum stage, const std::string &source)
{
    GLuint result;
    glGenProgramsARB(1, &result);
    glBindProgramARB(stage, result);
    glProgramStringARB(stage, GL_PROGRAM_FORMAT_ASCII_ARB, source.size(), source.data());
    GLenum error = glGetError();
    Require(!error, std::string("GL ARB compilation: ") +
        (const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB));
    return result;
}
static GLuint GLSLProgram(const std::string &fragment)
{
    GLuint result = glCreateProgram();
    const std::string vertex = "#version 120\nvarying vec4 uv;"
        "void main() { gl_Position = gl_Vertex; uv = gl_MultiTexCoord0; }";
    for (unsigned n = 0; n < 2; ++n) {
        GLuint shader = glCreateShader(n ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
        std::string source = n ? "#version 120\n#extension GL_ARB_shader_texture_lod : require\n"
            "varying vec4 uv; uniform sampler2D tex; void main() { " + fragment + " }" : vertex;
        const char *text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr); glCompileShader(shader);
        GLint status; glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            char detail[4096]; glGetShaderInfoLog(shader, sizeof(detail), nullptr, detail);
            throw std::runtime_error(std::string("GLSL independent oracle compile: ") + detail);
        }
        glAttachShader(result, shader); glDeleteShader(shader);
    }
    glLinkProgram(result);
    GLint status; glGetProgramiv(result, GL_LINK_STATUS, &status);
    Require(status != 0, "GLSL independent oracle link failed");
    return result;
}
static id<MTLLibrary> Library(id<MTLDevice> device, const std::string &source, bool semantic = false)
{
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    if (semantic) options.preprocessorMacros = @{@"ARB_SEMANTIC_TEST": @1};
    if (@available(macOS 15.0, *)) options.mathMode = MTLMathModeSafe;
    else options.fastMathEnabled = NO;
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()]
                                                options:options error:&error];
    Require(library != nil, std::string("MSL runtime compilation: ") +
        (error ? error.localizedDescription.UTF8String : "no diagnostic"));
    return library;
}
struct Vertex { V4 position, texcoord, color, fog; };
static MacMetalARBConstants Constants(bool vertex)
{
    MacMetalARBConstants result = {};
    result.env[1] = vertex ? MacMetalFloat4{{.07f,.11f,.19f,.23f}} : MacMetalFloat4{{.13f,.17f,.29f,.31f}};
    result.local[3] = vertex ? MacMetalFloat4{{.02f,.04f,.06f,.08f}} : MacMetalFloat4{{.05f,.09f,.15f,.21f}};
    result.state[0] = MacMetalFloat4{{.25f,.37f,.49f,.61f}};
    return result;
}
static std::vector<Vertex> Vertices(bool perspective)
{
    std::vector<Vertex> result(3);
    const V4 positions[] = {{-1,-1,-.4f,1}, {3,-1,.2f,1}, {-1,3,.6f,1}};
    const V4 coords[] = {{.13f,.23f,0,1}, {1.73f,.23f,0,1}, {.13f,1.83f,0,1}};
    const V4 colors[] = {{29/255.0f,73/255.0f,151/255.0f,1},
                         {177/255.0f,31/255.0f,107/255.0f,1},
                         {91/255.0f,213/255.0f,53/255.0f,1}};
    for (unsigned n = 0; n < 3; ++n) {
        float w = perspective ? float(1u << n) : 1.0f;
        result[n] = {positions[n] * w, coords[n], colors[n], V4{.15f + .2f*n,0,0,1}};
    }
    return result;
}
static const char *vertexProgram = "!!ARBvp1.0\n"
    "MOV result.position, vertex.position; MOV result.color, vertex.color;"
    "MOV result.color.secondary, vertex.attrib[7];"
    "MOV result.texcoord[0], vertex.texcoord[0];"
    "MOV result.fogcoord.x, vertex.fogcoord.x; END";
struct Fixture {
    std::string name, fragment, vertex = vertexProgram;
    bool perspective = true, mipmaps = false;
    ColorOrder order = ColorOrder::BGRA;
    std::string glslFragment;
    uint32_t attributeType = 17;
    std::vector<uint8_t> attributeBytes;
    V4 attributeExpected = {0,0,0,1};
    bool truncateAttribute = false, overflowAttributeOffset = false;
    float samplerBias = 0;
    uint32_t alphaFunction = 0, alphaReference = 128;
};
struct Harness {
    CGLContextObj context = nullptr;
    GLuint framebuffer = 0, target = 0, depth = 0, sampled = 0;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLTexture> colorTexture, depthTexture, sampleTexture;
    id<MTLSamplerState> sampler;
    Harness()
    {
        CGLPixelFormatAttribute attributes[] = {kCGLPFAAccelerated, kCGLPFAColorSize,
            (CGLPixelFormatAttribute)24, (CGLPixelFormatAttribute)0};
        CGLPixelFormatObj format;
        GLint count;
        Require(!CGLChoosePixelFormat(attributes, &format, &count), "no legacy CGL pixel format");
        CGLError error = CGLCreateContext(format, nullptr, &context);
        CGLDestroyPixelFormat(format);
        Require(!error && !CGLSetCurrentContext(context), "no legacy CGL context");
        glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE);
        glClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE);
        glClampColorARB(GL_CLAMP_READ_COLOR_ARB, GL_FALSE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_DITHER);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); glDepthMask(GL_TRUE);
        glGenTextures(1, &target); glBindTexture(GL_TEXTURE_2D, target);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glGenTextures(1, &depth); glBindTexture(GL_TEXTURE_2D, depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glGenFramebuffersEXT(1, &framebuffer);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depth, 0);
        Require(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT,
                "GL framebuffer incomplete");
        glViewport(0, 0, width, height);
        device = MTLCreateSystemDefaultDevice();
        Require(device != nil, "no Metal device");
        queue = [device newCommandQueue];
        auto texture = [&](MTLPixelFormat pixelFormat) {
            MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                width:width height:height mipmapped:NO];
            descriptor.storageMode = MTLStorageModeShared;
            descriptor.usage = MTLTextureUsageRenderTarget;
            return [device newTextureWithDescriptor:descriptor];
        };
        colorTexture = texture(MTLPixelFormatRGBA32Float);
        depthTexture = texture(MTLPixelFormatDepth32Float);
        Require(queue && colorTexture && depthTexture, "Metal allocation failed");
    }
    ~Harness()
    {
        glDeleteTextures(1, &sampled); glDeleteTextures(1, &target); glDeleteTextures(1, &depth);
        glDeleteFramebuffersEXT(1, &framebuffer);
        CGLSetCurrentContext(nullptr); CGLDestroyContext(context);
    }
    void Textures()
    {
        glGenTextures(1, &sampled); glBindTexture(GL_TEXTURE_2D, sampled);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
            width:64 height:64 mipmapped:YES];
        descriptor.storageMode = MTLStorageModeShared; descriptor.usage = MTLTextureUsageShaderRead;
        sampleTexture = [device newTextureWithDescriptor:descriptor];
        for (unsigned level = 0, size = 64; size; ++level, size /= 2) {
            std::vector<V4> pixels(size * size, V4{.07f + .12f*level, .14f + .1f*level, .8f - .09f*level, 1});
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA32F_ARB, size, size, 0, GL_RGBA, GL_FLOAT, pixels.data());
            [sampleTexture replaceRegion:MTLRegionMake2D(0,0,size,size) mipmapLevel:level
                               withBytes:pixels.data() bytesPerRow:size*sizeof(V4)];
        }
        MTLSamplerDescriptor *s = [[MTLSamplerDescriptor alloc] init];
        s.minFilter = s.magFilter = MTLSamplerMinMagFilterNearest;
        s.mipFilter = MTLSamplerMipFilterNearest;
        s.sAddressMode = s.tAddressMode = MTLSamplerAddressModeRepeat;
        sampler = [device newSamplerStateWithDescriptor:s];
        Require(sampleTexture && sampler && glGetError() == GL_NO_ERROR, "mip fixtures upload failed");
    }
    std::vector<V4> GL(const Fixture &fixture, const std::vector<Vertex> &vertices,
                       std::vector<float> &depthResult)
    {
        GLuint vs = 0, fs = 0, glsl = 0;
        glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, fixture.samplerBias);
        if (fixture.alphaFunction) {
            const GLenum functions[] = {GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};
            /* D3D ALPHAREF is normalized/clamped before entering the GL state
             * contract; ARB_color_buffer_float permits unclamped GL refs. */
            glAlphaFunc(functions[fixture.alphaFunction-1], float(std::min(fixture.alphaReference,255u))/255.0f);
            glEnable(GL_ALPHA_TEST);
        } else glDisable(GL_ALPHA_TEST);
        if (fixture.glslFragment.empty()) {
            vs = GLProgram(GL_VERTEX_PROGRAM_ARB, fixture.vertex);
            fs = GLProgram(GL_FRAGMENT_PROGRAM_ARB, fixture.fragment);
            glEnable(GL_VERTEX_PROGRAM_ARB); glEnable(GL_FRAGMENT_PROGRAM_ARB);
            for (unsigned n = 0; n < 2; ++n) {
                auto constants = Constants(n == 0);
                GLenum stage = n == 0 ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB;
                glProgramEnvParameter4fvARB(stage, 1, constants.env[1].v);
                glProgramLocalParameter4fvARB(stage, 3, constants.local[3].v);
                glFogfv(GL_FOG_COLOR, constants.state[0].v);
            }
        } else {
            glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
            glsl = GLSLProgram(fixture.glslFragment); glUseProgram(glsl);
            glUniform1i(glGetUniformLocation(glsl, "tex"), 0);
        }
        glClearColor(-37,-38,-39,-40); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBegin(GL_TRIANGLES);
        for (const auto &v : vertices) {
            glColor4fv((const float *)&v.color);
            glMultiTexCoord4fvARB(GL_TEXTURE0_ARB, (const float *)&v.texcoord);
            glFogCoordfEXT(v.fog.x);
            glVertexAttrib4fvARB(3, (const float *)&v.color);
            glVertexAttrib4fvARB(5, (const float *)&v.fog);
            glVertexAttrib4fvARB(7, (const float *)&fixture.attributeExpected);
            glVertexAttrib4fvARB(8, (const float *)&v.texcoord);
            if (glsl) glVertex4fv((const float *)&v.position);
            else glVertexAttrib4fvARB(0, (const float *)&v.position);
        }
        glEnd();
        std::vector<V4> result(width * height);
        glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,result.data());
        depthResult.resize(width * height);
        glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depthResult.data());
        Require(glGetError() == GL_NO_ERROR, fixture.name + ": GL draw/readback failed");
        glDeleteProgramsARB(1, &vs); glDeleteProgramsARB(1, &fs);
        if (glsl) { glUseProgram(0); glDeleteProgram(glsl); }
        return result;
    }
    std::vector<V4> Metal(const Fixture &fixture, const std::vector<Vertex> &vertices,
                         std::vector<float> &depthResult)
    {
        id<MTLLibrary> vs = Library(device, Translate(fixture.vertex).source);
        id<MTLLibrary> fs = Library(device, Translate(fixture.fragment).source);
        MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = [vs newFunctionWithName:@"arb_vertex"];
        pipelineDescriptor.fragmentFunction = [fs newFunctionWithName:@"arb_fragment"];
        pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
        pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        NSError *error = nil;
        id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        Require(pipeline != nil, fixture.name + ": linked raster PSO: " +
            (error ? error.localizedDescription.UTF8String : "no diagnostic"));
        RasterDescriptor fetch;
        fetch.framebufferHeight = height;
        fetch.samplerLODBias[0] = fixture.samplerBias;
        fetch.alphaEnabled = fixture.alphaFunction != 0;
        fetch.alphaFunction = fixture.alphaFunction;
        fetch.alphaReference = fixture.alphaReference;
        fetch.attributes[0] = {0,5,3,ColorOrder::RGBA};
        fetch.attributes[8] = {1,0,3,ColorOrder::RGBA};
        fetch.attributes[3] = {1,16,4,fixture.order};
        fetch.attributes[5] = {1,20,0,ColorOrder::RGBA};
        std::vector<uint8_t> positions(3 + 3*20), values(2 + 3*24);
        fetch.streams[0] = {3,positions.size(),20,0};
        fetch.streams[1] = {2,values.size(),24,0};
        /* Include deliberately unaligned offsets, independently serialized bytes. */
        positions.resize(3 + 5 + 3*20); fetch.streams[0].length = positions.size();
        for (unsigned n = 0; n < 3; ++n) {
            memcpy(positions.data() + 8 + n*20, &vertices[n].position, sizeof(V4));
            memcpy(values.data() + 2 + n*24, &vertices[n].texcoord, sizeof(V4));
            uint8_t rgba[4];
            for (unsigned c = 0; c < 4; ++c) rgba[c] = uint8_t(std::lround(vertices[n].color[c] * 255));
            uint8_t *color = values.data() + 2 + n*24 + 16;
            if (fixture.order == ColorOrder::RGBA) memcpy(color, rgba, 4);
            if (fixture.order == ColorOrder::BGRA) { color[0]=rgba[2]; color[1]=rgba[1]; color[2]=rgba[0]; color[3]=rgba[3]; }
            if (fixture.order == ColorOrder::ARGB) { color[0]=rgba[3]; color[1]=rgba[0]; color[2]=rgba[1]; color[3]=rgba[2]; }
            memcpy(values.data() + 2 + n*24 + 20, &vertices[n].fog, sizeof(float));
        }
        id<MTLBuffer> descriptor = [device newBufferWithBytes:&fetch length:sizeof(fetch) options:MTLResourceStorageModeShared];
        auto constants = Constants(true), fragmentConstants = Constants(false);
        id<MTLBuffer> registers = [device newBufferWithBytes:&constants length:sizeof(constants) options:MTLResourceStorageModeShared];
        id<MTLBuffer> fragmentRegisters = [device newBufferWithBytes:&fragmentConstants length:sizeof(fragmentConstants) options:MTLResourceStorageModeShared];
        id<MTLBuffer> positionBuffer = [device newBufferWithBytes:positions.data() length:positions.size() options:MTLResourceStorageModeShared];
        id<MTLBuffer> valueBuffer = [device newBufferWithBytes:values.data() length:values.size() options:MTLResourceStorageModeShared];
        id<MTLBuffer> attributeBuffer = nil;
        if (fixture.attributeType < 17) {
            std::vector<uint8_t> bytes(3 + fixture.attributeBytes.size());
            memcpy(bytes.data()+3, fixture.attributeBytes.data(), fixture.attributeBytes.size());
            fetch.attributes[7] = {2,1,fixture.attributeType,ColorOrder::RGBA};
            fetch.streams[2] = {fixture.overflowAttributeOffset ? UINT64_MAX : 2,
                bytes.size() - (fixture.truncateAttribute ? 1u : 0u),0,0};
            attributeBuffer = [device newBufferWithBytes:bytes.data() length:bytes.size() options:MTLResourceStorageModeShared];
            memcpy(descriptor.contents, &fetch, sizeof(fetch));
        }
        MTLDepthStencilDescriptor *depthState = [[MTLDepthStencilDescriptor alloc] init];
        depthState.depthCompareFunction = MTLCompareFunctionAlways; depthState.depthWriteEnabled = YES;
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = colorTexture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear; pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(-37,-38,-39,-40);
        pass.depthAttachment.texture = depthTexture;
        pass.depthAttachment.loadAction = MTLLoadActionClear; pass.depthAttachment.storeAction = MTLStoreActionStore;
        pass.depthAttachment.clearDepth = 1;
        id<MTLCommandBuffer> commands = [queue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:pipeline];
        [encoder setDepthStencilState:[device newDepthStencilStateWithDescriptor:depthState]];
        [encoder setVertexBuffer:descriptor offset:0 atIndex:0]; [encoder setFragmentBuffer:descriptor offset:0 atIndex:0];
        [encoder setVertexBuffer:registers offset:0 atIndex:1]; [encoder setFragmentBuffer:fragmentRegisters offset:0 atIndex:1];
        [encoder setVertexBuffer:positionBuffer offset:0 atIndex:2]; [encoder setVertexBuffer:valueBuffer offset:0 atIndex:3];
        if (attributeBuffer) [encoder setVertexBuffer:attributeBuffer offset:0 atIndex:4];
        if (fixture.mipmaps) { [encoder setFragmentTexture:sampleTexture atIndex:0]; [encoder setFragmentSamplerState:sampler atIndex:0]; }
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        id<MTLBuffer> depthBuffer = [device newBufferWithLength:height*256 options:MTLResourceStorageModeShared];
        id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
        [blit copyFromTexture:depthTexture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                  sourceSize:MTLSizeMake(width,height,1) toBuffer:depthBuffer destinationOffset:0
      destinationBytesPerRow:256 destinationBytesPerImage:height*256];
        [blit endEncoding]; [commands commit]; [commands waitUntilCompleted];
        Require(commands.status == MTLCommandBufferStatusCompleted, fixture.name + ": GPU execution failed");
        std::vector<V4> result(width * height);
        [colorTexture getBytes:result.data() bytesPerRow:width*sizeof(V4)
                   fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
        depthResult.resize(width * height);
        for (unsigned y = 0; y < height; ++y)
            memcpy(depthResult.data()+y*width, (const char *)depthBuffer.contents+y*256, width*sizeof(float));
        return result;
    }
    void Run(const Fixture &fixture)
    {
        auto vertices = Vertices(fixture.perspective);
        if (fixture.mipmaps) {
            vertices[0].texcoord = {0.13f,0.23f,0,.75f};
            vertices[1].texcoord = {12.13f,0.23f,0,.75f};
            vertices[2].texcoord = {0.13f,12.23f,0,.75f};
        }
        std::vector<float> glDepth, metalDepth;
        auto gl = GL(fixture, vertices, glDepth), metal = Metal(fixture, vertices, metalDepth);
        double maximum = 0, maximumDepth = 0;
        for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
            unsigned a = y*width+x, b = (height-1-y)*width+x;
            for (unsigned c = 0; c < 4; ++c) {
                Require(std::isfinite(gl[a][c]) && std::isfinite(metal[b][c]), fixture.name + ": nonfinite pixel");
                maximum = std::max(maximum, double(std::abs(gl[a][c]-metal[b][c])));
            }
            maximumDepth = std::max(maximumDepth, double(std::abs(glDepth[a]-metalDepth[b])));
        }
        std::cout << fixture.name << ": 768 pixels, max color=" << maximum << ", max depth=" << maximumDepth << '\n';
        if (maximum > 2e-5) std::cerr << "first GL/Metal pixels: " << gl[0].w << " / " << metal[(height-1)*width].w << '\n';
        Require(maximum <= 2e-5 && maximumDepth <= 2e-5, fixture.name + ": GL/Metal raster parity failed");
    }
};
static Fixture Attribute(unsigned type)
{
    Fixture result{"declaration type " + std::to_string(type),
        "!!ARBfp1.0\nMOV result.color, fragment.color.secondary; END"};
    result.attributeType = type;
    auto u16 = [&](unsigned value) { result.attributeBytes.push_back(value & 255); result.attributeBytes.push_back((value >> 8) & 255); };
    auto u32 = [&](unsigned value) { u16(value & 65535); u16(value >> 16); };
    if (type <= 3) {
        float components[] = {-.75f,.5f,1.25f,-1};
        for (unsigned n = 0; n <= type; ++n) {
            uint32_t bits; memcpy(&bits, &components[n], 4); u32(bits);
            result.attributeExpected[n] = components[n];
        }
    } else if (type == 4 || type == 5 || type == 8) {
        result.attributeBytes = {0,17,128,255};
        for (unsigned n = 0; n < 4; ++n)
            result.attributeExpected[n] = float(result.attributeBytes[n]) / (type == 5 ? 1 : 255);
    } else if (type >= 6 && type <= 12) {
        unsigned count = type == 6 || type == 9 || type == 11 ? 2 : 4;
        const int signedValues[] = {-32768,-123,32767,17};
        const unsigned unsignedValues[] = {0,65535,12345,40000};
        for (unsigned n = 0; n < count; ++n) {
            if (type >= 11) { u16(unsignedValues[n]); result.attributeExpected[n] = float(unsignedValues[n]) / 65535; }
            else { u16(unsigned(signedValues[n]) & 65535); result.attributeExpected[n] = type >= 9 ?
                std::max(float(signedValues[n])/32767, -1.0f) : float(signedValues[n]); }
        }
    } else if (type == 13) {
        u32((512u << 10) | (1023u << 20)); result.attributeExpected = {0,512,1023,1};
    } else if (type == 14) {
        u32(512u | (511u << 10) | (700u << 20)); result.attributeExpected = {-1,1,-324.0f/511,1};
    } else {
        const unsigned bits[] = {0xba00,0x3800,0x3d00,0xbc00};
        const float values[] = {-.75f,.5f,1.25f,-1};
        for (unsigned n = 0; n < (type == 15 ? 2u : 4u); ++n) { u16(bits[n]); result.attributeExpected[n] = values[n]; }
    }
    return result;
}
int main()
{
    @autoreleasepool {
        try {
            Harness h; h.Textures();
            for (const char *target : {"2D", "3D", "CUBE"}) {
                auto translated = Translate(std::string("!!ARBfp1.0\nTXD result.color, fragment.texcoord[0],") +
                    "{.1,0,0,0}, {0,.1,0,0}, texture[0], " + target + "; END");
                Library(h.device, translated.source); Library(h.device, translated.source, true);
            }
            bool rejectsDerivativeCompute = false;
            try {
                Library(h.device, Translate("!!ARBfp1.0\nDDY result.color, fragment.position; END").source, true);
            } catch (const std::runtime_error &error) {
                rejectsDerivativeCompute = std::string(error.what()).find("raster neighbors") != std::string::npos;
            }
            Require(rejectsDerivativeCompute, "isolated compute must reject raster derivatives");
            const std::vector<Fixture> fixtures = {
                {"window position / reciprocal W", "!!ARBfp1.0\nMOV result.color, fragment.position; END"},
                {"perspective UV", "!!ARBfp1.0\nMOV result.color, fragment.texcoord[0]; END"},
                {"BGRA color", "!!ARBfp1.0\nMOV result.color, fragment.color; END"},
                {"ARGB color", "!!ARBfp1.0\nMOV result.color, fragment.color; END", vertexProgram, true, false, ColorOrder::ARGB},
                {"RGBA color", "!!ARBfp1.0\nMOV result.color, fragment.color; END", vertexProgram, true, false, ColorOrder::RGBA},
                {"fog input components", "!!ARBfp1.0\nMOV result.color, fragment.fogcoord; END"},
                {"missing attribute defaults", "!!ARBfp1.0\nMOV result.color, fragment.color.secondary; END"},
                {"depth replacement", "!!ARBfp1.0\nMOV result.color, fragment.color; MOV result.depth.z, .25; END"},
                {"unused depth declaration", "!!ARBfp1.0\nOUTPUT d = result.depth; MOV result.color, fragment.color; END"},
                {"depth clamped high", "!!ARBfp1.0\nMOV result.color, fragment.color; MOV result.depth.z, 2; END"},
                {"implicit LOD", "!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], 2D; END", vertexProgram, false, true},
                {"biased LOD", "!!ARBfp1.0\nTXB result.color, fragment.texcoord[0], texture[0], 2D; END", vertexProgram, false, true},
                {"projected LOD", "!!ARBfp1.0\nTXP result.color, fragment.texcoord[0], texture[0], 2D; END", vertexProgram, false, true},
                {"X derivative / independent GLSL", "!!ARBfp1.0\nDDX result.color, fragment.texcoord[0]; END", vertexProgram,
                    false, false, ColorOrder::BGRA, "gl_FragColor = dFdx(uv);"},
                {"GL Y derivative / independent GLSL", "!!ARBfp1.0\nDDY result.color, fragment.texcoord[0]; END", vertexProgram,
                    false, false, ColorOrder::BGRA, "gl_FragColor = dFdy(uv);"},
                {"explicit LOD / independent GLSL", "!!ARBfp1.0\nTXL result.color, fragment.texcoord[0], texture[0], 2D; END", vertexProgram,
                    false, true, ColorOrder::BGRA, "gl_FragColor = texture2DLod(tex, uv.xy, uv.w);"},
                {"explicit gradients / independent GLSL", "!!ARBfp1.0\nTXD result.color, fragment.texcoord[0], {0.1,0,0,0}, {0,0.1,0,0}, texture[0], 2D; END", vertexProgram,
                    false, true, ColorOrder::BGRA, "gl_FragColor = texture2DGradARB(tex, uv.xy, vec2(0.1,0), vec2(0,0.1));"}
            };
            for (const auto &fixture : fixtures) h.Run(fixture);
            for (unsigned n = 10; n < 13; ++n) {
                auto biased = fixtures[n]; biased.name += " + sampler bias"; biased.samplerBias = 1.25f;
                h.Run(biased);
            }
            for (unsigned n = 15; n < 17; ++n) {
                auto biased = fixtures[n]; biased.name += " + sampler bias"; biased.samplerBias = 1.25f;
                h.Run(biased);
            }
            for (unsigned type = 0; type < 17; ++type) h.Run(Attribute(type));
            auto truncated = Attribute(3);
            truncated.name = "truncated attribute range"; truncated.truncateAttribute = true;
            truncated.attributeExpected = {0,0,0,1}; h.Run(truncated);
            auto overflow = Attribute(3);
            overflow.name = "overflow stream offset"; overflow.overflowAttributeOffset = true;
            overflow.attributeExpected = {0,0,0,1}; h.Run(overflow);
            for (unsigned function = 1; function <= 8; ++function) {
                Fixture alpha{"alpha function " + std::to_string(function),
                    "!!ARBfp1.0\nMOV result.color, fragment.color;"
                    "MUL result.color.w, fragment.texcoord[0].x, .5; END"};
                alpha.alphaFunction = function; h.Run(alpha);
                alpha.name += " at equal reference";
                alpha.fragment = "!!ARBfp1.0\nMOV result.color, fragment.color;"
                    "MOV result.color.w, .50196078431372549; END";
                h.Run(alpha);
            }
            Fixture clamped{"alpha reference clamped to 255", "!!ARBfp1.0\nMOV result.color, fragment.color; MOV result.color.w, 1; END"};
            clamped.alphaFunction = 3; clamped.alphaReference = 300; h.Run(clamped);
            Fixture banks{"separate stage environment/local banks", "!!ARBfp1.0\nTEMP r;"
                "ADD r, fragment.color, program.env[1]; ADD result.color, r, program.local[3]; END"};
            const std::string original = "MOV result.color, vertex.color;";
            banks.vertex.replace(banks.vertex.find(original), original.size(),
                "ADD result.color, vertex.color, program.env[1];");
            h.Run(banks);
            Fixture state{"fragment local and fog state binding", "!!ARBfp1.0\nPARAM f = state.fog.color;"
                "ADD result.color, program.local[3], f; END"};
            h.Run(state);
            std::cout << "Metal linked raster synthetic fixtures passed\n";
        } catch (const std::exception &error) {
            std::cerr << error.what() << '\n'; return 1;
        }
    }
}
