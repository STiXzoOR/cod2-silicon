#include "cod2x_xray_gl.h"
#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <math.h>
#include <string.h>

/* Private depth/stencil and colour targets keep the game's shadow stencil and
   depth intact. The passes implement the operations in upstream xray.cpp;
   the shaders and resource management here are native OpenGL implementations. */
enum { MASK, SMALL, LARGE, TEMP, SILHOUETTE, TEXTURES };
static struct {
    CGLContextObj context;
    Cod2xXraySettings settings;
    GLuint mark, textureFbo, depth, color, texture[TEXTURES], fade, white, flat, quad;
    int width, height, samples, marked, active, head;
    float fadeValue;
} xr;
typedef struct { GLint read, draw, readBuffer, drawBuffer, program, fp, vp, active, renderbuffer, unpack; } Saved;
static void Save(Saved *s)
{
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&s->read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&s->draw);
    glGetIntegerv(GL_READ_BUFFER,&s->readBuffer);
    glGetIntegerv(GL_DRAW_BUFFER,&s->drawBuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM,&s->program);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&s->fp);
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&s->vp);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&s->active);
    glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT,&s->renderbuffer);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&s->unpack);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
}
static void Restore(const Saved *s)
{
    glUseProgram(s->program);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,s->fp);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB,s->vp);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,s->read);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,s->draw);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,s->renderbuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,s->unpack);
    glPopAttrib();
    glActiveTexture(s->active);
}
void Cod2xXray_GLRelease(void)
{
    if (xr.context == CGLGetCurrentContext()) {
        glDeleteFramebuffersEXT(1,&xr.mark); glDeleteFramebuffersEXT(1,&xr.textureFbo);
        glDeleteRenderbuffersEXT(1,&xr.depth); glDeleteRenderbuffersEXT(1,&xr.color);
        glDeleteTextures(TEXTURES,xr.texture); glDeleteTextures(1,&xr.fade); glDeleteTextures(1,&xr.white);
        glDeleteProgramsARB(1,&xr.flat); if(xr.quad) glDeleteProgram(xr.quad);
    }
    memset(&xr,0,sizeof(xr));
}
static GLuint Shader(GLenum type,const char *text)
{
    GLuint s=glCreateShader(type); GLint ok=0;
    glShaderSource(s,1,&text,NULL); glCompileShader(s); glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { glDeleteShader(s); return 0; } return s;
}
static int Programs(void)
{
    if(xr.quad) return 1;
    unsigned char white[4] = {255,255,255,255};
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glGenTextures(1,&xr.white);glBindTexture(GL_TEXTURE_2D,xr.white);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    const char *v="#version 120\nvarying vec2 uv; void main(){gl_Position=gl_Vertex; uv=gl_MultiTexCoord0.xy;}";
    const char *f="#version 120\nuniform sampler2D image, fadeImage; uniform vec4 tint; uniform vec2 target; uniform vec2 shift; uniform float radius; uniform int textured; varying vec2 uv; void main(){vec4 c=tint; if(textured!=0)c*=texture2D(image,uv+shift); c=clamp(c,0.0,1.0); if(radius>0.0)c*=texture2D(fadeImage,vec2(.5)+(gl_FragCoord.xy-target*.5)*480.0/target.y/(radius*2.0)); gl_FragColor=clamp(c,0.0,1.0);}";
    GLuint vs=Shader(GL_VERTEX_SHADER,v),fs=Shader(GL_FRAGMENT_SHADER,f);
    if(!vs||!fs) { if(vs)glDeleteShader(vs); if(fs)glDeleteShader(fs); return 0; }
    xr.quad=glCreateProgram(); glAttachShader(xr.quad,vs);glAttachShader(xr.quad,fs);glLinkProgram(xr.quad);
    glDeleteShader(vs);glDeleteShader(fs); GLint ok;glGetProgramiv(xr.quad,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(xr.quad);xr.quad=0;return 0;}
    const char flat[]="!!ARBfp1.0\nMOV result.color, program.local[0];\nEND\n";
    glGenProgramsARB(1,&xr.flat);glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,xr.flat);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,sizeof(flat)-1,flat);
    return 1;
}
static void Texture(GLuint texture,int w,int h)
{
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
static int Targets(int w,int h,int samples)
{
    if(xr.width==w&&xr.height==h&&xr.samples==samples&&xr.mark)return 1;
    glDeleteFramebuffersEXT(1,&xr.mark);glDeleteFramebuffersEXT(1,&xr.textureFbo);
    glDeleteRenderbuffersEXT(1,&xr.depth);glDeleteRenderbuffersEXT(1,&xr.color);
    glDeleteTextures(TEXTURES,xr.texture);
    glGenFramebuffersEXT(1,&xr.mark);glGenFramebuffersEXT(1,&xr.textureFbo);
    glGenRenderbuffersEXT(1,&xr.depth);glGenRenderbuffersEXT(1,&xr.color);
    glGenTextures(TEXTURES,xr.texture);
    xr.width=w;xr.height=h;xr.samples=samples;
    for(int i=0;i<TEXTURES;++i)Texture(xr.texture[i],i>=SMALL&&i<=TEMP?w/2:w,i>=SMALL&&i<=TEMP?h/2:h);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,xr.mark);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,xr.depth);
    if(samples)glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_DEPTH24_STENCIL8_EXT,w,h);
    else glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH24_STENCIL8_EXT,w,h);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,xr.depth);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,xr.depth);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,xr.color);
    if(samples)glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_RGBA8,w,h);
    else glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_RGBA8,w,h);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,xr.color);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT) return 1;
    xr.width = xr.height = 0;
    return 0;
}
static void Flat(void)
{
    glUseProgram(0);glEnable(GL_FRAGMENT_PROGRAM_ARB);glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,xr.flat);
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0,1,1,1,1);
    glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_FOG);glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DITHER);glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
}
static void Quad(float z)
{
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0,0);glVertex3f(-1,-1,z);glTexCoord2f(1,0);glVertex3f(1,-1,z);
    glTexCoord2f(0,1);glVertex3f(-1,1,z);glTexCoord2f(1,1);glVertex3f(1,1,z);glEnd();
}
static float Byte(float v){ return floorf(fmaxf(0,fminf(1,v))*255)/255; }
static void SetupQuad(int w,int h)
{
    glUseProgram(xr.quad);glDisable(GL_VERTEX_PROGRAM_ARB);glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_FOG);glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DITHER);glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    for(int i=0;i<6;++i)glDisable(GL_CLIP_PLANE0+i);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glDepthMask(GL_FALSE);glDepthRange(0,1);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,w,h);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,xr.white);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,xr.white);
    glUniform1i(glGetUniformLocation(xr.quad,"image"),0);glUniform1i(glGetUniformLocation(xr.quad,"fadeImage"),1);
    glUniform2f(glGetUniformLocation(xr.quad,"target"),w,h);
    glUniform1f(glGetUniformLocation(xr.quad,"radius"),0);
    glUniform2f(glGetUniformLocation(xr.quad,"shift"),0,0);
}
static void Tint(const float *rgba)
{
    glUniform4f(glGetUniformLocation(xr.quad,"tint"),Byte(rgba[0]),Byte(rgba[1]),Byte(rgba[2]),Byte(rgba[3]));
}
static void Solid(const float *rgba)
{
    glUniform1i(glGetUniformLocation(xr.quad,"textured"),0);Tint(rgba);Quad(0);
}
static void Stencil(int ref,int mask)
{
    glEnable(GL_STENCIL_TEST);glStencilFunc(GL_EQUAL,ref,mask);glStencilMask(0);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
}
static void BindTextureTarget(int index,int stencil)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,xr.textureFbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,xr.texture[index],0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,stencil?xr.depth:0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,stencil?xr.depth:0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
}
static void Sample(int index,float strength,float u,float v)
{
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,xr.texture[index]);
    glUniform1i(glGetUniformLocation(xr.quad,"textured"),1);
    glUniform4f(glGetUniformLocation(xr.quad,"tint"),strength,strength,strength,strength);
    glUniform2f(glGetUniformLocation(xr.quad,"shift"),u,v);Quad(0);
}
void Cod2xXray_GLBegin(const Cod2xXraySettings *settings)
{
    xr.settings=*settings;xr.marked=0;xr.active=1;
}
void Cod2xXray_GLPlayer(unsigned flags,const float *origin,const float *view,const float *projection,
                       int targetWidth,int targetHeight,unsigned first,unsigned last,unsigned count,const void *indices)
{
    if(!xr.active||targetWidth<2||targetHeight<2)return;
    Saved saved;Save(&saved);
    GLint viewport[4],samples,stencil;glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SAMPLES,&samples);glGetIntegerv(GL_STENCIL_BITS,&stencil);
    if(stencil<8)goto restore;
    CGLContextObj context=CGLGetCurrentContext();
    if(xr.context!=context){Cod2xXraySettings settings=xr.settings;Cod2xXray_GLRelease();xr.context=context;xr.settings=settings;xr.active=1;}
    glActiveTexture(GL_TEXTURE0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    if(!Programs()||!Targets(targetWidth,targetHeight,samples))goto restore;
    glDisable(GL_SCISSOR_TEST);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,saved.draw);glReadBuffer(saved.drawBuffer);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,xr.mark);glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glBlitFramebufferEXT(0,0,xr.width,xr.height,0,0,xr.width,xr.height,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,xr.mark);
    if(!xr.marked){glStencilMask(255);glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);}
    Flat();glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_STENCIL_TEST);glColorMask(0,0,0,0);
    int team=(flags>>25)&3,vis=team==2?0x80:0x40,hid=team==2?0x20:0x10;
    float distance=0;for(int i=0;i<3;++i){float d=origin[i]-xr.settings.origin[i];distance+=d*d;}
    if(distance>1&&xr.settings.threshold>0){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0,-xr.settings.nearPlane*xr.settings.threshold/distance*16777216);}
    glStencilFunc(GL_ALWAYS,vis,255);glStencilMask(vis);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
    glDrawRangeElements(GL_TRIANGLES,first,last,count,GL_UNSIGNED_SHORT,indices);
    glDisable(GL_POLYGON_OFFSET_FILL);glStencilFunc(GL_ALWAYS,hid,255);glStencilMask(hid);glStencilOp(GL_KEEP,GL_REPLACE,GL_KEEP);
    glDrawRangeElements(GL_TRIANGLES,first,last,count,GL_UNSIGNED_SHORT,indices);
    int level=(flags>>27)&31;
    if(level){
        float point[3]={origin[0],origin[1],origin[2]+level*2.5f},v[4];
        for(int c=0;c<4;++c)v[c]=point[0]*view[c]+point[1]*view[4+c]+point[2]*view[8+c]+view[12+c];
        float y=0,w=0;for(int c=0;c<4;++c){y+=v[c]*projection[c*4+1];w+=v[c]*projection[c*4+3];}
        if(w>0){int cut=(int)ceilf((1-y/w)*.5f*viewport[3]);if(cut<0)cut=0;
            if(cut<viewport[3]){glEnable(GL_SCISSOR_TEST);glScissor(viewport[0],viewport[1],viewport[2],viewport[3]-cut);
                glDisable(GL_DEPTH_TEST);glStencilFunc(GL_ALWAYS,8,255);glStencilMask(8);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
                glDrawRangeElements(GL_TRIANGLES,first,last,count,GL_UNSIGNED_SHORT,indices);glDisable(GL_SCISSOR_TEST);}
        }
    }
    if(samples&&xr.settings.mode>=3){
        BindTextureTarget(SILHOUETTE,0);glDisable(GL_STENCIL_TEST);glDisable(GL_DEPTH_TEST);glColorMask(1,1,1,1);
        if(!xr.marked){glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);}
        const float *color=xr.settings.color[team];glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0,Byte(color[0]),Byte(color[1]),Byte(color[2]),1);
        glDrawRangeElements(GL_TRIANGLES,first,last,count,GL_UNSIGNED_SHORT,indices);
    }
    xr.marked=1;
restore:Restore(&saved);
}
static void FadeTexture(void)
{
    if(xr.fade&&xr.fadeValue==xr.settings.fadeAlpha)return;
    unsigned char pixels[128*128*4];
    for(int y=0;y<128;++y)for(int x=0;x<128;++x){
        float a=1;
        if(x&&y&&x!=127&&y!=127){float dx=(x+.5f)/64-1,dy=(y+.5f)/64-1,t=fminf(1,sqrtf(dx*dx+dy*dy));a=xr.settings.fadeAlpha+(1-xr.settings.fadeAlpha)*t*t*(3-2*t);}
        for(int c=0;c<4;++c)pixels[(y*128+x)*4+c]=(unsigned char)(a*255);
    }
    if(!xr.fade)glGenTextures(1,&xr.fade);
    glActiveTexture(GL_TEXTURE1);Texture(xr.fade,128,128);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    xr.fadeValue=xr.settings.fadeAlpha;
}
static void Blur(int image,int spread)
{
    static const float weight[]={.216f,.191f,.131f,.070f};
    for(int axis=0;axis<2;++axis){
        BindTextureTarget(axis?image:TEMP,0);SetupQuad(xr.width/2,xr.height/2);glDisable(GL_STENCIL_TEST);
        glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE);
        for(int k=-3;k<=3;++k)Sample(axis?TEMP:image,Byte(weight[k<0?-k:k]),axis?0:(float)(k*spread)/(xr.width/2),axis?(float)(k*spread)/(xr.height/2):0);
    }
}
static void GlowTextures(int source)
{
    BindTextureTarget(SMALL,0);SetupQuad(xr.width/2,xr.height/2);glDisable(GL_STENCIL_TEST);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_BLEND);glBlendEquation(GL_MAX);glBlendFunc(GL_ONE,GL_ONE);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,xr.texture[source]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    for(int x=0;x<4;++x)for(int y=0;y<4;++y)Sample(source,1,(x-1.5f)/(2*(xr.width/2)),(y-1.5f)/(2*(xr.height/2)));
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    BindTextureTarget(LARGE,0);glDisable(GL_BLEND);Sample(SMALL,1,0,0);
    Blur(SMALL,1);Blur(SMALL,1);Blur(LARGE,1);Blur(LARGE,1);Blur(LARGE,2);
}
static void DrawMask(int hidden,int visible,int fade)
{
    int source=xr.samples?SILHOUETTE:MASK;
    if(!xr.samples){
        BindTextureTarget(MASK,1);SetupQuad(xr.width,xr.height);glDisable(GL_BLEND);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
        for(int team=1;team<=2;++team){int v=team==2?0x80:0x40,h=team==2?0x20:0x10;
            if(hidden){Stencil(h,h|0xc0);Solid(xr.settings.color[team]);}
            if(visible){Stencil(v,v);Solid(xr.settings.color[team]);}
        }
    }
    if(xr.settings.mode==4)GlowTextures(source);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,xr.mark);SetupQuad(xr.width,xr.height);Stencil(0,0xf0);
    glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    if(fade){glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,xr.fade);glUniform1f(glGetUniformLocation(xr.quad,"radius"),xr.settings.fadeRadius);}
    if(xr.settings.mode==4){
        Sample(LARGE,Byte(xr.settings.glow/4)*4,0,0);Sample(LARGE,Byte(xr.settings.glow/4)*4,0,0);
        glBlendFunc(GL_ONE,GL_ONE);Sample(SMALL,Byte(xr.settings.core/2)*2,0,0);
    }else{
        static const float dir[8][2]={{1,0},{-1,0},{0,1},{0,-1},{.7f,.7f},{-.7f,.7f},{.7f,-.7f},{-.7f,-.7f}};
        for(int ring=1;ring<=2;++ring)for(int i=0;i<8;++i)Sample(source,ring==1?1:Byte(.5f),dir[i][0]*xr.settings.outline*ring/xr.width,dir[i][1]*xr.settings.outline*ring/xr.height);
    }
}
void Cod2xXray_GLEnd(void)
{
    xr.active=0;if(!xr.marked)return;
    Saved saved;Save(&saved);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glDisable(GL_SCISSOR_TEST);
    /* Compose against an exact colour copy, with our independent stencil. */
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,saved.draw);glReadBuffer(saved.drawBuffer);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,xr.mark);glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glBlitFramebufferEXT(0,0,xr.width,xr.height,0,0,xr.width,xr.height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,xr.mark);SetupQuad(xr.width,xr.height);glDisable(GL_BLEND);
    if(xr.settings.fadeRadius>0)FadeTexture();
    if(xr.settings.mode==2){
        for(int team=1;team<=2;++team){int h=team==2?0x20:0x10;Stencil(h,h|0xc0);Solid(xr.settings.color[team]);}
    }else if(xr.settings.mode>=3){
        if(xr.settings.fadeRadius>0&&xr.settings.visible&&!xr.samples){DrawMask(0,1,0);DrawMask(1,0,1);}
        else DrawMask(1,xr.settings.visible,xr.settings.fadeRadius>0);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,xr.mark);SetupQuad(xr.width,xr.height);
    if(xr.settings.health){
        glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        if(xr.settings.fadeRadius>0){glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,xr.fade);glUniform1f(glGetUniformLocation(xr.quad,"radius"),xr.settings.fadeRadius);}
        Stencil(8,0xc8);Solid(xr.settings.healthColor);glUniform1f(glGetUniformLocation(xr.quad,"radius"),0);
        if(xr.settings.visible)for(int mask=0x40;mask<=0xc0;mask+=0x40){Stencil(8|mask,0xc8);Solid(xr.settings.healthColor);}
    }
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,xr.mark);glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,saved.draw);glDrawBuffer(saved.drawBuffer);
    glDisable(GL_SCISSOR_TEST);glBlitFramebufferEXT(0,0,xr.width,xr.height,0,0,xr.width,xr.height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    Restore(&saved);xr.marked=0;
}
int Cod2xXray_GLHeadAvailable(void)
{
    GLint bits = 0;
    if (CGLGetCurrentContext()) glGetIntegerv(GL_STENCIL_BITS, &bits);
    return bits >= 8;
}
void Cod2xXray_GLHead(int operation,float depth)
{
    if (!operation && !xr.head) return;
    xr.head=0;
    Saved saved;Save(&saved);GLint stencil,viewport[4];glGetIntegerv(GL_STENCIL_BITS,&stencil);glGetIntegerv(GL_VIEWPORT,viewport);
    if(stencil<8)goto restore;
    if(!xr.context)xr.context=CGLGetCurrentContext();
    if(!Programs())goto restore;
    SetupQuad(viewport[2],viewport[3]);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glDisable(GL_BLEND);glEnable(GL_STENCIL_TEST);glStencilMask(8);glColorMask(0,0,0,0);
    if(operation){
        glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glStencilFunc(GL_ALWAYS,8,255);glStencilOp(GL_KEEP,GL_ZERO,GL_REPLACE);
        glUniform1i(glGetUniformLocation(xr.quad,"textured"),0);Quad(depth*2-1);xr.head=1;
    }else{
        glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);
    }
restore:Restore(&saved);
}
void Cod2xXray_GLHeadDraw(int before)
{
    if(!xr.head)return;
    if(before){glPushAttrib(GL_STENCIL_BUFFER_BIT);Stencil(8,8);}
    else glPopAttrib();
}
#endif
