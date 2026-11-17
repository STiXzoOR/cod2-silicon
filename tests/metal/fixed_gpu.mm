#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include "shader_fixed.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
static const int W=19,H=13;
static void Check(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
static void CheckGL() { Check(glGetError()==GL_NO_ERROR,"GL error"); }
static GLuint Shader(GLenum kind,const std::string &source)
{
    GLuint shader=glCreateShader(kind);const char *text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) { char error[4096];glGetShaderInfoLog(shader,sizeof(error),nullptr,error);throw std::runtime_error(error); }return shader;
}
static GLuint Program(const std::string &fragment)
{
    GLuint vs=Shader(GL_VERTEX_SHADER,"#version 120\nvoid main(){gl_Position=gl_Vertex;gl_TexCoord[0]=gl_MultiTexCoord0;}");
    GLuint fs=Shader(GL_FRAGMENT_SHADER,"#version 120\n"+fragment);GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
    GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);Check(ok,"GL link");glDeleteShader(vs);glDeleteShader(fs);return p;
}
static std::vector<unsigned char> GLPixels(GLuint program)
{
    glUseProgram(program);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);glVertex2f(-1,-1);glVertex2f(1,-1);glVertex2f(1,1);glVertex2f(-1,1);glEnd();
    std::vector<unsigned char> bytes(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,bytes.data());CheckGL();return bytes;
}

static std::vector<unsigned char> GLGeometry(const MacMetalARBInput inputs[6],GLuint program=0)
{
    glUseProgram(program);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glBegin(GL_TRIANGLES);
    for(unsigned i=0;i<6;i++) {
        glColor4fv(inputs[i].attrib[5].v);glNormal3fv(inputs[i].attrib[3].v);glTexCoord4fv(inputs[i].attrib[8].v);glVertex4fv(inputs[i].attrib[0].v);
    }
    glEnd();std::vector<unsigned char> raw(W*H*4),canonical(W*H*4);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,raw.data());CheckGL();
    // Both images compared in Metal/top-left row order. No color/geometry alignment.
    for(unsigned y=0;y<H;y++)memcpy(canonical.data()+y*W*4,raw.data()+(H-1-y)*W*4,W*4);
    return canonical;
}
static std::vector<unsigned char> GLPositionGeometry(const MacMetalARBInput inputs[6])
{
    MacMetalARBInput positions[6];memcpy(positions,inputs,sizeof(positions));
    for(auto &vertex:positions) vertex.attrib[8]=vertex.attrib[0];
    return GLGeometry(positions);
}
static std::string Vector(const float *v) { char s[128];snprintf(s,sizeof(s),"vec4(%.9g,%.9g,%.9g,%.9g)",v[0],v[1],v[2],v[3]);return s; }
static std::string Oracle(unsigned op)
{
    switch(op) {
    case 2:return "a";case 3:return "b";case 4:return "a*b";case 5:return "a*b*2.0";case 6:return "a*b*4.0";
    case 7:return "a+b";case 8:return "a+b-0.5";case 9:return "(a+b-0.5)*2.0";case 10:return "a-b";
    case 11:return "a+b-a*b";case 12:return "mix(b,a,d.a)";case 13:return "mix(b,a,t.a)";case 14:return "mix(b,a,f.a)";
    case 15:return "a+b*(1.0-t.a)";case 16:return "mix(b,a,d.a)";
    case 18:return "vec4(a.rgb+b.rgb*a.a,0.0)";case 19:return "vec4(a.rgb*b.rgb+vec3(a.a),0.0)";
    case 20:return "vec4(a.rgb+b.rgb*(1.0-a.a),0.0)";case 21:return "vec4((1.0-a.rgb)*b.rgb+vec3(a.a),0.0)";
    case 24:return "vec4(dot(a.rgb*2.0-1.0,b.rgb*2.0-1.0))";
    case 25:return "a*b+z";case 26:return "mix(b,a,z)";
    default:throw std::runtime_error("missing oracle");
    }
}
static std::string Argument(unsigned arg)
{
    const char *sources[]={"d","d","t","f","s","vec4(0.0)","k"};
    std::string v=sources[arg&15];if(arg&32)v="vec4(("+v+").a)";if(arg&16)v="(1.0-"+v+")";return v;
}
struct GPU {
    id<MTLDevice> device;
    id<MTLLibrary> library;
    id<MTLCommandQueue> queue;
    id<MTLTexture> target,texture;
    id<MTLSamplerState> sampler;
    std::vector<id<MTLRenderPipelineState>> pipelines;
    GPU() {
        device=MTLCreateSystemDefaultDevice();Check(device!=nil,"Metal device unavailable");queue=[device newCommandQueue];
        MTLCompileOptions *options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;
        options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
        NSError *error=nil;library=[device newLibraryWithSource:[NSString stringWithUTF8String:MacMetalFixed_Source()] options:options error:&error];
        if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
        for(const char *entry:{MAC_METAL_FIXED_FRAGMENT,MAC_METAL_FIXED_COLORED,MAC_METAL_FIXED_SOLID,MAC_METAL_FIXED_COPY,MAC_METAL_FIXED_GAMMA}) {
            MTLRenderPipelineDescriptor *desc=[MTLRenderPipelineDescriptor new];desc.vertexFunction=[library newFunctionWithName:!strcmp(entry,MAC_METAL_FIXED_GAMMA)?@MAC_METAL_FIXED_POST_VERTEX:@MAC_METAL_FIXED_VERTEX];desc.fragmentFunction=[library newFunctionWithName:[NSString stringWithUTF8String:entry]];desc.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
            id<MTLRenderPipelineState> p=[device newRenderPipelineStateWithDescriptor:desc error:&error];if(!p)throw std::runtime_error(error.localizedDescription.UTF8String);pipelines.push_back(p);
        }
        for(unsigned type:{1u,2u}) { uint32_t types[8];std::fill(types,types+8,type);char *source=MacMetalFixed_SourceForTypes(types);id<MTLLibrary> typed=[device newLibraryWithSource:[NSString stringWithUTF8String:source] options:options error:&error];free(source);if(!typed)throw std::runtime_error(error.localizedDescription.UTF8String);
            MTLRenderPipelineDescriptor *desc=[MTLRenderPipelineDescriptor new];desc.vertexFunction=[typed newFunctionWithName:@MAC_METAL_FIXED_VERTEX];desc.fragmentFunction=[typed newFunctionWithName:@MAC_METAL_FIXED_FRAGMENT];desc.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
            id<MTLRenderPipelineState> p=[device newRenderPipelineStateWithDescriptor:desc error:&error];if(!p)throw std::runtime_error(error.localizedDescription.UTF8String);pipelines.push_back(p);
        }
        auto desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:W height:H mipmapped:NO];desc.storageMode=MTLStorageModeShared;desc.usage=MTLTextureUsageRenderTarget;target=[device newTextureWithDescriptor:desc];
        desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];desc.storageMode=MTLStorageModeShared;desc.usage=MTLTextureUsageShaderRead;texture=[device newTextureWithDescriptor:desc];
        unsigned char color[4]={73,131,197,167};[texture replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:color bytesPerRow:4];
        MTLSamplerDescriptor *sd=[MTLSamplerDescriptor new];sd.minFilter=MTLSamplerMinMagFilterNearest;sd.magFilter=MTLSamplerMinMagFilterNearest;sd.sAddressMode=MTLSamplerAddressModeRepeat;sd.tAddressMode=MTLSamplerAddressModeRepeat;sampler=[device newSamplerStateWithDescriptor:sd];
    }
    std::vector<unsigned char> Draw(const MacMetalARBConstants &constants,unsigned pipeline=0,const MacMetalARBInput *custom=nullptr) {
        MacMetalARBInput inputs[6]={};float xy[6][2]={{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(unsigned i=0;i<6;i++) { inputs[i].attrib[0]={{xy[i][0],xy[i][1],.5f,1}};inputs[i].attrib[3]={{0,0,1,0}};
            inputs[i].attrib[5]={{.23f,.47f,.71f,.59f}};inputs[i].attrib[6]={{.12f,.34f,.56f,.81f}};
            for(unsigned s=0;s<8;s++)inputs[i].attrib[8+s]={{.5f,.5f,.5f,1}};
        }
        if(custom)memcpy(inputs,custom,sizeof(inputs));
        id<MTLBuffer> input=[device newBufferWithBytes:inputs length:sizeof(inputs) options:MTLResourceStorageModeShared];
        id<MTLBuffer> c=[device newBufferWithBytes:&constants length:sizeof(constants) options:MTLResourceStorageModeShared];
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=target;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);
        id<MTLCommandBuffer> command=[queue commandBuffer];id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];[encoder setRenderPipelineState:pipelines[pipeline]];[encoder setVertexBuffer:input offset:0 atIndex:0];[encoder setVertexBuffer:c offset:0 atIndex:1];[encoder setFragmentBuffer:c offset:0 atIndex:1];
        for(unsigned i=0;i<8;i++) { [encoder setFragmentTexture:texture atIndex:i];[encoder setFragmentSamplerState:sampler atIndex:i]; }
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];[encoder endEncoding];[command commit];[command waitUntilCompleted];Check(command.status==MTLCommandBufferStatusCompleted,"Metal fixture submission");
        std::vector<unsigned char> result(W*H*4);[target getBytes:result.data() bytesPerRow:W*4 fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];return result;
    }
};
static double maxMAE=0,maxRMSE=0;static unsigned comparisons=0;
static void Compare(const std::vector<unsigned char> &a,const std::vector<unsigned char> &b,const char *name)
{
    double sum=0,squares=0;std::vector<unsigned> differences;unsigned bad=0;
    for(unsigned i=0;i<a.size();i+=4) { bool large=false;for(unsigned c=0;c<4;c++) { unsigned d=abs(int(a[i+c])-int(b[i+c]));sum+=d;squares+=d*d;differences.push_back(d);large|=d>16; }bad+=large; }
    std::sort(differences.begin(),differences.end());double mae=sum/a.size(),rmse=sqrt(squares/a.size());unsigned p99=differences[(differences.size()*99)/100];
    if(mae>1 || rmse>2 || p99>4 || double(bad)/(W*H)>=.001) { fprintf(stderr,"FAIL %s MAE=%g RMSE=%g p99=%u bad=%u\n",name,mae,rmse,p99,bad);throw std::runtime_error("fixed raster parity"); }
    maxMAE=std::max(maxMAE,mae);maxRMSE=std::max(maxRMSE,rmse);comparisons++;
}
int main()
{
    @autoreleasepool { try {
        CGLPixelFormatAttribute attrs[]={kCGLPFAAccelerated,(CGLPixelFormatAttribute)0};CGLPixelFormatObj format;GLint count;Check(CGLChoosePixelFormat(attrs,&format,&count)==kCGLNoError,"CGL pixel format");CGLContextObj context;Check(CGLCreateContext(format,nullptr,&context)==kCGLNoError,"CGL context");CGLDestroyPixelFormat(format);CGLSetCurrentContext(context);
        GLuint target,fbo;glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glGenFramebuffersEXT(1,&fbo);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,target,0);Check(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT,"GL FBO");glViewport(0,0,W,H);glDisable(GL_DITHER);
        GPU gpu;MacMetalFixedState fixed;MacMetalFixed_Init(&fixed);MacMetalState state={};state.viewport[2]=W;state.viewport[3]=H;state.render[60]=0x9d3d7197;
        for(unsigned matrix:{2u,3u,256u})for(unsigned i=0;i<4;i++)state.transform[matrix][i*5]=1;
        float diffuse[]={.23f,.47f,.71f,.59f},secondary[]={.12f,.34f,.56f,.81f},texture[]={73.f/255,131.f/255,197.f/255,167.f/255},factor[]={61.f/255,113.f/255,151.f/255,157.f/255},constant[]={19.f/255,89.f/255,173.f/255,211.f/255};
        std::string uniforms="vec4 d="+Vector(diffuse)+",s="+Vector(secondary)+",t="+Vector(texture)+",f="+Vector(factor)+",k="+Vector(constant)+";";
        char error[256];MacMetalARBConstants packed;
        for(unsigned op:{2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,18u,19u,20u,21u,24u,25u,26u}) {
            for(unsigned modifier:{0u,16u,32u,48u})for(unsigned source=0;source<=6;source++) {
                auto &stage=state.textureStage[0];stage[1]=op;stage[2]=source|modifier;stage[3]=2;stage[26]=6;stage[4]=2;stage[5]=0;stage[32]=0xd31359ad;
                Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
                std::string body="void main(){"+uniforms+"vec4 a="+Argument(source|modifier)+",b=t,z=k;vec4 r=clamp("+Oracle(op)+",0.0,1.0);r.a=d.a;gl_FragColor=r;}";
                GLuint p=Program(body);Compare(GLPixels(p),gpu.Draw(packed),"stage operation/modifier");glDeleteProgram(p);
            }
        }
        for(unsigned func=1;func<=8;func++)for(unsigned reference:{166u,167u,168u}) {
            memset(state.textureStage,0,sizeof(state.textureStage));state.render[15]=1;state.render[25]=func;state.render[24]=reference;Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
            // Direct GL fixed alpha compare on GLSL-produced colors; same normalization.
            glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_NEVER+func-1,reference/255.f);GLuint p=Program("void main(){vec4 r="+Vector(diffuse)+"*"+Vector(texture)+";r.a="+Vector(texture)+".a;gl_FragColor=r;}");Compare(GLPixels(p),gpu.Draw(packed),"alpha compare");glDeleteProgram(p);glDisable(GL_ALPHA_TEST);
        }

        state.render[15]=0;
        // Real GL fixed-function color/texture combiner references (no GLSL).
        glUseProgram(0);glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        glColor4fv(diffuse);glNormal3f(0,0,1);
        GLuint sampleTexture;glGenTextures(1,&sampleTexture);glBindTexture(GL_TEXTURE_2D,sampleTexture);
        unsigned char sampleColor[4]={73,131,197,167};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,sampleColor);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
        for(auto combination:std::vector<std::pair<unsigned,GLenum>>{{2,GL_REPLACE},{4,GL_MODULATE},{7,GL_ADD},{8,GL_ADD_SIGNED},{10,GL_SUBTRACT},{24,GL_DOT3_RGB}}) {
            auto &stage=state.textureStage[0];stage[1]=combination.first;stage[2]=2;stage[3]=0;stage[4]=2;stage[5]=2;
            Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
            glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,combination.second);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PRIMARY_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);
            Compare(GLPixels(0),gpu.Draw(packed),"GL fixed texture combine");
        }
        glDisable(GL_TEXTURE_2D);
        memset(state.textureStage,0,sizeof(state.textureStage));Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        Compare(GLPixels(0),gpu.Draw(packed,1),"GL fixed colored");
        // Vertex fog on constant depth; all three formulas use actual GL fog.
        for(unsigned mode:{1u,2u,3u}) {
            state.render[28]=1;state.render[140]=mode;state.render[34]=0xff315b91;
            float start=.1f,end=1.3f,density=.7f;memcpy(&state.render[36],&start,4);memcpy(&state.render[37],&end,4);memcpy(&state.render[38],&density,4);
            Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
            float color[]={49.f/255,91.f/255,145.f/255,1};glEnable(GL_FOG);glFogfv(GL_FOG_COLOR,color);glFogi(GL_FOG_MODE,mode==1?GL_EXP:mode==2?GL_EXP2:GL_LINEAR);glFogf(GL_FOG_START,start);glFogf(GL_FOG_END,end);glFogf(GL_FOG_DENSITY,density);glFogi(GL_FOG_COORDINATE_SOURCE_EXT,GL_FOG_COORDINATE_EXT);glFogCoordfEXT(.5f);
            Compare(GLPixels(0),gpu.Draw(packed,1),"GL fixed vertex fog");glDisable(GL_FOG);
        }
        state.render[28]=0;state.render[140]=0;
        // Engine source reaches directional diffuse+ambient lighting only.
        state.render[137]=1;state.render[141]=0;state.render[143]=1;state.render[139]=0xff07111b;
        fixed.enabled[0]=1;fixed.lights[0].type=3;fixed.lights[0].direction[2]=-1;
        float md[]={.6f,.4f,.2f,.77f},ma[]={.3f,.5f,.7f,1},me[]={.02f,.03f,.04f,0},ld[]={.4f,.6f,.8f,0},la[]={.13f,.17f,.19f,0};
        memcpy(fixed.material.diffuse,md,16);memcpy(fixed.material.ambient,ma,16);memcpy(fixed.material.emissive,me,16);memcpy(fixed.lights[0].diffuse,ld,16);memcpy(fixed.lights[0].ambient,la,16);
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_NORMALIZE);glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,md);glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,ma);glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,me);
        float globalAmbient[]={7.f/255,17.f/255,27.f/255,1},lightPosition[]={0,0,1,0},zero[]={0,0,0,0};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT,globalAmbient);glLightfv(GL_LIGHT0,GL_POSITION,lightPosition);glLightfv(GL_LIGHT0,GL_DIFFUSE,ld);glLightfv(GL_LIGHT0,GL_AMBIENT,la);glLightfv(GL_LIGHT0,GL_SPECULAR,zero);
        Compare(GLPixels(0),gpu.Draw(packed,1),"GL directional material lighting");glDisable(GL_LIGHTING);glDisable(GL_LIGHT0);glDisable(GL_NORMALIZE);
        state.render[137]=0;
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        // Prepared clear/solid and copy shaders, no draw-time factories.
        glColor4fv(factor);Compare(GLPixels(0),gpu.Draw(packed,2),"solid/partial clear color");
        GLuint p=Program("void main(){gl_FragColor="+Vector(texture)+";}");Compare(GLPixels(p),gpu.Draw(packed,3),"copy/scale color");glDeleteProgram(p);
        // RGB16 LUT channel values are normalized by CPU into fixed local[].
        for(unsigned i=0;i<256;i++)packed.local[i]={{(255-i)/255.f,i*i/65025.f,float(i)/510.f,0}};
        float gamma[]={182.f/255,131.f*131.f/65025.f,197.f/510,.655f};gamma[3]=167.f/255;
        p=Program("void main(){gl_FragColor="+Vector(gamma)+";}");Compare(GLPixels(p),gpu.Draw(packed,4),"RGB gamma LUT");glDeleteProgram(p);

        // Nonuniform interpolation and labeled texture: catches flipped UV or rows.
        MacMetalARBInput inputs[6]={};float positions[6][2]={{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(unsigned i=0;i<6;i++) {
            float x=(positions[i][0]+1)/2,y=(positions[i][1]+1)/2;
            inputs[i].attrib[0]={{positions[i][0],positions[i][1],.5f,1}};
            inputs[i].attrib[3]={{0,0,1,0}};inputs[i].attrib[5]={{.2f+.6f*x,.1f+.7f*y,.4f,1}};
            inputs[i].attrib[8]={{.013f+.91f*x,.027f+.87f*y,0,1}};
        }
        memset(state.textureStage,0,sizeof(state.textureStage));state.textureStage[0][1]=4;state.textureStage[0][2]=2;state.textureStage[0][3]=0;state.textureStage[0][4]=2;state.textureStage[0][5]=2;
        unsigned char labels[4*4*4];for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++) {
            unsigned i=(y*4+x)*4;labels[i]=20+x*51;labels[i+1]=30+y*53;labels[i+2]=210-x*17-y*13;labels[i+3]=255;
        }
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:4 height:4 mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;gpu.texture=[gpu.device newTextureWithDescriptor:td];[gpu.texture replaceRegion:MTLRegionMake2D(0,0,4,4) mipmapLevel:0 withBytes:labels bytesPerRow:16];
        glBindTexture(GL_TEXTURE_2D,sampleTexture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,labels);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        Compare(GLGeometry(inputs),gpu.Draw(packed,0,inputs),"labeled texture/perspective color interpolation");
        // World transform and translated texture matrix, supplied as D3D row matrices.
        state.transform[256][0]=.75f;state.transform[256][5]=.75f;state.transform[256][12]=.1f;
        for(unsigned i=0;i<4;i++)state.transform[16][i*5]=1;state.transform[16][12]=.125f;state.textureStage[0][24]=2;
        glMatrixMode(GL_MODELVIEW);glLoadMatrixf(state.transform[256]);glMatrixMode(GL_TEXTURE);glLoadMatrixf(state.transform[16]);glMatrixMode(GL_MODELVIEW);
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);Compare(GLGeometry(inputs),gpu.Draw(packed,0,inputs),"world/texture matrices");
        glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_TEXTURE_2D);

        // Perspective-correct colors/coordinates with independently supplied clip W.
        state.transform[256][0]=state.transform[256][5]=1;state.transform[256][12]=0;state.textureStage[0][24]=0;
        for(unsigned i=0;i<6;i++) {
            float w=positions[i][0]>0?1.7f:.7f;inputs[i].attrib[0]={{positions[i][0]*w,positions[i][1]*w,.5f*w,w}};
        }
        glEnable(GL_TEXTURE_2D);Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        Compare(GLGeometry(inputs),gpu.Draw(packed,0,inputs),"perspective interpolation");glDisable(GL_TEXTURE_2D);
        // Pretransformed XYZRHW preserves D3D9's half-pixel viewport offset.
        fixed.fvf=4;MacMetalARBInput transformed[6];memcpy(transformed,inputs,sizeof(inputs));
        for(unsigned i=0;i<6;i++) {
            float w=inputs[i].attrib[0].v[3];transformed[i].attrib[0]={{(positions[i][0]+1)*W/2-.5f,(1-positions[i][1])*H/2-.5f,.5f,1/w}};
        }
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        Compare(GLGeometry(inputs),gpu.Draw(packed,1,transformed),"XYZRHW half-pixel/reciprocal W");fixed.fvf=0;
        // TEMP is separate from CURRENT, then selected by the next stage.
        memset(state.textureStage,0,sizeof(state.textureStage));state.textureStage[0][1]=4;state.textureStage[0][2]=0;state.textureStage[0][3]=3;state.textureStage[0][4]=2;state.textureStage[0][5]=0;state.textureStage[0][28]=5;
        state.textureStage[1][1]=2;state.textureStage[1][2]=5;state.textureStage[1][4]=2;state.textureStage[1][5]=5;
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        p=Program("void main(){vec4 r="+Vector(diffuse)+"*"+Vector(factor)+";r.a="+Vector(diffuse)+".a;gl_FragColor=r;}");Compare(GLPixels(p),gpu.Draw(packed),"TEMP result and next stage read");glDeleteProgram(p);
        // All eight enabled stages preserve intermediate clamping and CURRENT.
        for(unsigned i=0;i<8;i++) { state.textureStage[i][1]=7;state.textureStage[i][2]=1;state.textureStage[i][3]=6;state.textureStage[i][4]=2;state.textureStage[i][5]=1;state.textureStage[i][28]=1;state.textureStage[i][32]=0x01130507; }
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        p=Program("void main(){vec4 r="+Vector(diffuse)+";for(int i=0;i<8;i++)r.rgb=clamp(r.rgb+vec3(19.0,5.0,7.0)/255.0,0.0,1.0);gl_FragColor=r;}");Compare(GLPixels(p),gpu.Draw(packed),"eight stages CURRENT and clamping");glDeleteProgram(p);

        // A spatial gamma fixture detects accidental reuse of stage metadata as LUT.
        for(unsigned i=0;i<256;i++)packed.local[i]={{(255-i)/255.f,i*i/65025.f,float(i)/510.f,0}};
        p=Program("uniform sampler2D image;void main(){vec4 t=texture2D(image,gl_TexCoord[0].xy);gl_FragColor=vec4(1.0-t.r,t.g*t.g,t.b*0.5,t.a);}");
        Compare(GLGeometry(inputs,p),gpu.Draw(packed,4,inputs),"spatial RGB16 gamma LUT");glDeleteProgram(p);

        // Source-reachable user planes are world-space on the fixed path.
        state.render[152]=1;fixed.clipPlanes[0][0]=1;fixed.clipPlanes[0][3]=-.17f;
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        GLdouble plane[]={1,0,0,-.17};glClipPlane(GL_CLIP_PLANE0,plane);glEnable(GL_CLIP_PLANE0);
        Compare(GLGeometry(inputs),gpu.Draw(packed,1,inputs),"world user clip plane");glDisable(GL_CLIP_PLANE0);state.render[152]=0;

        // The original Mac 0x20000 path binds position as texcoords and composes
        // world/view/texture matrices, without GL reflection texgen.
        memset(state.textureStage,0,sizeof(state.textureStage));state.textureStage[0][1]=2;state.textureStage[0][2]=2;state.textureStage[0][4]=2;state.textureStage[0][5]=2;
        state.textureStage[0][11]=0x20000;state.textureStage[0][24]=3;
        state.transform[256][0]=.79f;state.transform[256][5]=.83f;state.transform[256][12]=.041f;
        memset(state.transform[2],0,64);state.transform[2][1]=1;state.transform[2][4]=-1;state.transform[2][10]=state.transform[2][15]=1;state.transform[2][12]=.137f;state.transform[2][13]=-.219f;state.transform[2][14]=-.17f;
        memset(state.transform[16],0,64);state.transform[16][1]=-1;state.transform[16][4]=1;state.transform[16][10]=state.transform[16][15]=1;
        MacMetalARBInput positionInputs[6];memcpy(positionInputs,inputs,sizeof(inputs));
        for(unsigned i=0;i<6;i++)positionInputs[i].attrib[0]={{positions[i][0],positions[i][1],.5f,1}};
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,sampleTexture);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        glMatrixMode(GL_MODELVIEW);glLoadMatrixf(state.transform[2]);glMultMatrixf(state.transform[256]);
        glMatrixMode(GL_TEXTURE);glLoadMatrixf(state.transform[16]);glMultMatrixf(state.transform[2]);glMultMatrixf(state.transform[256]);glMatrixMode(GL_MODELVIEW);
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        Compare(GLPositionGeometry(positionInputs),gpu.Draw(packed,0,positionInputs),"GL camera position with inverse-view texture matrix");
        for(unsigned matrix:{2u,16u,256u}) { memset(state.transform[matrix],0,64);for(unsigned i=0;i<4;i++)state.transform[matrix][i*5]=1; }
        glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_TEXTURE_2D);

        // Typed cube and volume variants were linked before the first draw.
        memset(state.textureStage,0,sizeof(state.textureStage));state.textureStage[0][1]=2;state.textureStage[0][2]=2;state.textureStage[0][4]=2;state.textureStage[0][5]=2;
        state.render[152]=0;state.textureStage[0][11]=0x30000;fixed.textureTypes[0]=1;
        auto cubeDesc=[MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm size:1 mipmapped:NO];cubeDesc.storageMode=MTLStorageModeShared;cubeDesc.usage=MTLTextureUsageShaderRead;gpu.texture=[gpu.device newTextureWithDescriptor:cubeDesc];
        const unsigned char faces[6][4]={{201,17,31,255},{23,193,41,255},{43,53,179,255},{163,157,61,255},{73,139,151,255},{97,109,127,255}};
        for(unsigned face=0;face<6;face++)[gpu.texture replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 slice:face withBytes:faces[face] bytesPerRow:4 bytesPerImage:4];
        MacMetalARBInput reflectInputs[6];memcpy(reflectInputs,inputs,sizeof(inputs));
        for(unsigned i=0;i<6;i++) { reflectInputs[i].attrib[0]={{positions[i][0],positions[i][1],2,1}}; }
        state.transform[3][10]=.25f;Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        GLuint cube;glGenTextures(1,&cube);glBindTexture(GL_TEXTURE_CUBE_MAP,cube);
        for(unsigned face=0;face<6;face++)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,faces[face]);
        glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glEnable(GL_TEXTURE_CUBE_MAP);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        for(GLenum coord:{GL_S,GL_T,GL_R})glTexGeni(coord,GL_TEXTURE_GEN_MODE,GL_REFLECTION_MAP);
        glEnable(GL_TEXTURE_GEN_S);glEnable(GL_TEXTURE_GEN_T);glEnable(GL_TEXTURE_GEN_R);
        glMatrixMode(GL_PROJECTION);glLoadMatrixf(state.transform[3]);glMatrixMode(GL_MODELVIEW);
        auto reflectionPixels=GLGeometry(reflectInputs);
        unsigned center=((H/2)*W+W/2)*4;
        Check(!memcmp(reflectionPixels.data()+center,faces[5],4),"GL reflection selects -Z labeled cube face");
        Compare(reflectionPixels,gpu.Draw(packed,5,reflectInputs),"GL camera reflection cube -Z face");
        glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);glDisable(GL_TEXTURE_GEN_R);
        state.textureStage[0][11]=0x20000;Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);
        auto positionPixels=GLPositionGeometry(reflectInputs);
        Check(!memcmp(positionPixels.data()+center,faces[4],4),"GL position selects +Z labeled cube face");
        Compare(positionPixels,gpu.Draw(packed,5,reflectInputs),"GL camera position cube +Z face");
        glDisable(GL_TEXTURE_CUBE_MAP);glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
        fixed.textureTypes[0]=2;state.textureStage[0][11]=0;state.transform[3][10]=1;
        auto volumeDesc=[MTLTextureDescriptor new];volumeDesc.textureType=MTLTextureType3D;volumeDesc.pixelFormat=MTLPixelFormatRGBA8Unorm;volumeDesc.width=volumeDesc.height=volumeDesc.depth=2;volumeDesc.storageMode=MTLStorageModeShared;volumeDesc.usage=MTLTextureUsageShaderRead;gpu.texture=[gpu.device newTextureWithDescriptor:volumeDesc];
        unsigned char volume[32];for(unsigned i=0;i<8;i++)memcpy(volume+i*4,faces[i/4?3:0],4);[gpu.texture replaceRegion:MTLRegionMake3D(0,0,0,2,2,2) mipmapLevel:0 slice:0 withBytes:volume bytesPerRow:8 bytesPerImage:16];
        MacMetalARBInput volumeInputs[6];memcpy(volumeInputs,reflectInputs,sizeof(reflectInputs));for(unsigned i=0;i<6;i++) { volumeInputs[i].attrib[0].v[2]=.5f;volumeInputs[i].attrib[8]={{.23f,.31f,.73f,1}}; }
        Check(MacMetalFixed_Pack(&fixed,&state,&packed,error,sizeof(error)),error);GLuint volumeTexture;glGenTextures(1,&volumeTexture);glBindTexture(GL_TEXTURE_3D,volumeTexture);glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,2,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,volume);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glEnable(GL_TEXTURE_3D);
        Compare(GLGeometry(volumeInputs),gpu.Draw(packed,6,volumeInputs),"GL typed volume upper slice");glDisable(GL_TEXTURE_3D);
        printf("PASS fixed GPU %u fixtures, runtime source libraries=3 linked pipelines=7 MAE<=%g/255 RMSE<=%g/255\n",comparisons,maxMAE,maxRMSE);
        CGLSetCurrentContext(nullptr);CGLDestroyContext(context);return 0;
    }catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;} }
}
