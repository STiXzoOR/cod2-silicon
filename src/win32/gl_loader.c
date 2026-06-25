#include <SDL2/SDL.h>

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef float         GLfloat;
typedef float         GLclampf;
typedef double        GLdouble;
typedef double        GLclampd;
typedef unsigned int  GLbitfield;
typedef void          GLvoid;

extern void * __stdcall LoadLibraryA(const char *name);
extern void * __stdcall GetProcAddress(void *module, const char *name);

static void *gl_resolve(const char *name)
{
    void *p = SDL_GL_GetProcAddress(name);
    if (!p) {
        static void *opengl32;
        if (!opengl32) opengl32 = LoadLibraryA("opengl32.dll");
        if (opengl32) p = GetProcAddress(opengl32, name);
    }
    return p;
}

#ifdef GL_NULLTRACE
#include <stdio.h>
#define GLW_NULLLOG(name) do { static int warned; if(!warned){warned=1; \
    fprintf(stderr,"[gl-null] %s absent under this GL (no-op)\n", name); fflush(stderr);} } while(0)
#else
#define GLW_NULLLOG(name) ((void)0)
#endif

#define GLW_VOID(name, proto, args)                                            \
    typedef void(__stdcall *PFN_##name) proto;                                 \
    void name proto {                                                          \
        static PFN_##name p;                                                   \
        if (!p) p = (PFN_##name)gl_resolve(#name);                  \
        if (p) p args; else GLW_NULLLOG(#name);                                \
    }

#define GLW_RET(ret, name, proto, args)                                        \
    typedef ret(__stdcall *PFN_##name) proto;                                  \
    ret name proto {                                                           \
        static PFN_##name p;                                                   \
        if (!p) p = (PFN_##name)gl_resolve(#name);                  \
        if (p) return p args;                                                  \
        GLW_NULLLOG(#name);                                                    \
        return (ret)0;                                                         \
    }

GLW_VOID(glAlphaFunc, (GLenum func, GLclampf ref), (func, ref))
GLW_VOID(glBegin, (GLenum mode), (mode))
GLW_VOID(glBindTexture, (GLenum target, GLuint texture), (target, texture))
GLW_VOID(glBlendFunc, (GLenum sfactor, GLenum dfactor), (sfactor, dfactor))
GLW_VOID(glClear, (GLbitfield mask), (mask))
GLW_VOID(glClearColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha),
    (red, green, blue, alpha))
GLW_VOID(glClearDepth, (GLclampd depth), (depth))
GLW_VOID(glClearStencil, (GLint s), (s))
GLW_VOID(glClipPlane, (GLenum plane, const GLdouble *equation), (plane, equation))
GLW_VOID(glColor4f, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha),
    (red, green, blue, alpha))
GLW_VOID(glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha),
    (red, green, blue, alpha))
GLW_VOID(glColorMaterial, (GLenum face, GLenum mode), (face, mode))
GLW_VOID(glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr),
    (size, type, stride, ptr))
GLW_VOID(glCopyTexSubImage2D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset,
     GLint x, GLint y, GLsizei width, GLsizei height),
    (target, level, xoffset, yoffset, x, y, width, height))
GLW_VOID(glDeleteTextures, (GLsizei n, const GLuint *textures), (n, textures))
GLW_VOID(glDepthFunc, (GLenum func), (func))
GLW_VOID(glDepthMask, (GLboolean flag), (flag))
GLW_VOID(glDepthRange, (GLclampd near_val, GLclampd far_val), (near_val, far_val))
GLW_VOID(glDisable, (GLenum cap), (cap))
GLW_VOID(glDisableClientState, (GLenum cap), (cap))
GLW_VOID(glDrawBuffer, (GLenum mode), (mode))
GLW_VOID(glEnable, (GLenum cap), (cap))
GLW_VOID(glEnableClientState, (GLenum cap), (cap))
GLW_VOID(glEnd, (void), ())
GLW_VOID(glFinish, (void), ())
GLW_VOID(glFogf, (GLenum pname, GLfloat param), (pname, param))
GLW_VOID(glFogfv, (GLenum pname, const GLfloat *params), (pname, params))
GLW_VOID(glFogi, (GLenum pname, GLint param), (pname, param))
GLW_VOID(glFrontFace, (GLenum mode), (mode))
GLW_VOID(glGenTextures, (GLsizei n, GLuint *textures), (n, textures))
GLW_RET(GLenum, glGetError, (void), ())
GLW_VOID(glGetFloatv, (GLenum pname, GLfloat *params), (pname, params))
GLW_VOID(glGetIntegerv, (GLenum pname, GLint *params), (pname, params))
GLW_RET(const GLubyte *, glGetString, (GLenum name), (name))
GLW_VOID(glHint, (GLenum target, GLenum mode), (target, mode))
GLW_VOID(glLightModelfv, (GLenum pname, const GLfloat *params), (pname, params))
GLW_VOID(glLightModeli, (GLenum pname, GLint param), (pname, param))
GLW_VOID(glLightf, (GLenum light, GLenum pname, GLfloat param), (light, pname, param))
GLW_VOID(glLightfv, (GLenum light, GLenum pname, const GLfloat *params),
    (light, pname, params))
GLW_VOID(glLoadIdentity, (void), ())
GLW_VOID(glLoadMatrixf, (const GLfloat *m), (m))
GLW_VOID(glMaterialf, (GLenum face, GLenum pname, GLfloat param), (face, pname, param))
GLW_VOID(glMaterialfv, (GLenum face, GLenum pname, const GLfloat *params),
    (face, pname, params))
GLW_VOID(glMatrixMode, (GLenum mode), (mode))
GLW_VOID(glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *ptr),
    (type, stride, ptr))
GLW_VOID(glPixelStorei, (GLenum pname, GLint param), (pname, param))
GLW_VOID(glPointSize, (GLfloat size), (size))
GLW_VOID(glPolygonMode, (GLenum face, GLenum mode), (face, mode))
GLW_VOID(glPolygonOffset, (GLfloat factor, GLfloat units), (factor, units))
GLW_VOID(glPopAttrib, (void), ())
GLW_VOID(glPopClientAttrib, (void), ())
GLW_VOID(glPopMatrix, (void), ())
GLW_VOID(glPushAttrib, (GLbitfield mask), (mask))
GLW_VOID(glPushClientAttrib, (GLbitfield mask), (mask))
GLW_VOID(glPushMatrix, (void), ())
GLW_VOID(glReadBuffer, (GLenum src), (src))
GLW_VOID(glReadPixels,
    (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels),
    (x, y, width, height, format, type, pixels))
GLW_VOID(glScalef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
GLW_VOID(glScissor, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height))
GLW_VOID(glShadeModel, (GLenum mode), (mode))
GLW_VOID(glStencilFunc, (GLenum func, GLint ref, GLuint mask), (func, ref, mask))
GLW_VOID(glStencilMask, (GLuint mask), (mask))
GLW_VOID(glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass), (fail, zfail, zpass))
GLW_VOID(glTexCoord2f, (GLfloat s, GLfloat t), (s, t))
GLW_VOID(glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr),
    (size, type, stride, ptr))
GLW_VOID(glTexEnvf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param))
GLW_VOID(glTexEnvfv, (GLenum target, GLenum pname, const GLfloat *params),
    (target, pname, params))
GLW_VOID(glTexEnvi, (GLenum target, GLenum pname, GLint param), (target, pname, param))
GLW_VOID(glTexGenfv, (GLenum coord, GLenum pname, const GLfloat *params),
    (coord, pname, params))
GLW_VOID(glTexGeni, (GLenum coord, GLenum pname, GLint param), (coord, pname, param))
GLW_VOID(glTexImage2D,
    (GLenum target, GLint level, GLint internalformat, GLsizei width,
     GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels),
    (target, level, internalformat, width, height, border, format, type, pixels))
GLW_VOID(glTexParameterf, (GLenum target, GLenum pname, GLfloat param),
    (target, pname, param))
GLW_VOID(glTexParameterfv, (GLenum target, GLenum pname, const GLfloat *params),
    (target, pname, params))
GLW_VOID(glTexParameteri, (GLenum target, GLenum pname, GLint param),
    (target, pname, param))
GLW_VOID(glTexSubImage2D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
     GLsizei height, GLenum format, GLenum type, const GLvoid *pixels),
    (target, level, xoffset, yoffset, width, height, format, type, pixels))
GLW_VOID(glVertex2f, (GLfloat x, GLfloat y), (x, y))
GLW_VOID(glVertex3f, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
GLW_VOID(glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr),
    (size, type, stride, ptr))
GLW_VOID(glViewport, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height))

GLW_VOID(glDrawRangeElements,
    (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices),
    (mode, start, end, count, type, indices))
GLW_VOID(glTexImage3D,
    (GLenum target, GLint level, GLint internalformat, GLsizei width,
     GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
     const GLvoid *pixels),
    (target, level, internalformat, width, height, depth, border, format, type, pixels))
GLW_VOID(glTexSubImage3D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
     GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
     const GLvoid *pixels),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))

GLW_VOID(glActiveTextureARB, (GLenum texture), (texture))
GLW_VOID(glClientActiveTextureARB, (GLenum texture), (texture))
GLW_VOID(glBindProgramARB, (GLenum target, GLuint program), (target, program))
GLW_VOID(glDeleteProgramsARB, (GLsizei n, const GLuint *programs), (n, programs))
GLW_VOID(glGenProgramsARB, (GLsizei n, GLuint *programs), (n, programs))
GLW_VOID(glProgramStringARB,
    (GLenum target, GLenum format, GLsizei len, const GLvoid *string),
    (target, format, len, string))
GLW_VOID(glProgramEnvParameter4fvARB,
    (GLenum target, GLuint index, const GLfloat *params),
    (target, index, params))
GLW_VOID(glGetProgramivARB, (GLenum target, GLenum pname, GLint *params),
    (target, pname, params))
GLW_VOID(glEnableVertexAttribArrayARB, (GLuint index), (index))
GLW_VOID(glDisableVertexAttribArrayARB, (GLuint index), (index))
GLW_VOID(glVertexAttribPointerARB,
    (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer),
    (index, size, type, normalized, stride, pointer))
GLW_VOID(glPointParameterfARB, (GLenum pname, GLfloat param), (pname, param))
GLW_VOID(glPointParameterfvARB, (GLenum pname, const GLfloat *params), (pname, params))
GLW_VOID(glCompressedTexImage2DARB,
    (GLenum target, GLint level, GLenum internalformat, GLsizei width,
     GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data),
    (target, level, internalformat, width, height, border, imageSize, data))
GLW_VOID(glCompressedTexImage3DARB,
    (GLenum target, GLint level, GLenum internalformat, GLsizei width,
     GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data),
    (target, level, internalformat, width, height, depth, border, imageSize, data))
GLW_VOID(glCompressedTexSubImage2D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
     GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data),
    (target, level, xoffset, yoffset, width, height, format, imageSize, data))
GLW_VOID(glCompressedTexSubImage3D,
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
     GLsizei width, GLsizei height, GLsizei depth, GLenum format,
     GLsizei imageSize, const GLvoid *data),
    (target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data))

GLW_VOID(glBlendEquationEXT, (GLenum mode), (mode))
GLW_VOID(glBlendFuncSeparateEXT,
    (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha),
    (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))

GLW_VOID(glCombinerParameterfvNV, (GLenum pname, const GLfloat *params), (pname, params))
GLW_VOID(glCombinerStageParameterfvNV,
    (GLenum stage, GLenum pname, const GLfloat *params),
    (stage, pname, params))

GLW_VOID(glGenFencesAPPLE, (GLsizei n, GLuint *fences), (n, fences))
GLW_VOID(glDeleteFencesAPPLE, (GLsizei n, const GLuint *fences), (n, fences))
GLW_VOID(glSetFenceAPPLE, (GLuint fence), (fence))

typedef GLboolean(__stdcall *PFN_glTestFenceAPPLE)(GLuint fence);
GLboolean glTestFenceAPPLE(GLuint fence) {
    static PFN_glTestFenceAPPLE p;
    static int resolved;
    if (!resolved) { p = (PFN_glTestFenceAPPLE)gl_resolve("glTestFenceAPPLE"); resolved = 1; }
    if (p) return p(fence);
    return (GLboolean)1;
}
GLW_VOID(glFinishFenceAPPLE, (GLuint fence), (fence))

GLW_VOID(glBindVertexArrayAPPLE, (GLuint array), (array))
GLW_VOID(glGenVertexArraysAPPLE, (GLsizei n, GLuint *arrays), (n, arrays))
GLW_VOID(glDeleteVertexArraysAPPLE, (GLsizei n, const GLuint *arrays), (n, arrays))
GLW_VOID(glVertexArrayParameteriAPPLE, (GLenum pname, GLint param), (pname, param))
GLW_VOID(glVertexArrayRangeAPPLE, (GLsizei length, GLvoid *pointer), (length, pointer))
GLW_VOID(glFlushVertexArrayRangeAPPLE, (GLsizei length, GLvoid *pointer), (length, pointer))

GLW_VOID(glBindVertexArray, (GLuint array), (array))
GLW_VOID(glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices), (mode, count, type, indices))
GLW_VOID(glGetBooleanv, (GLenum pname, GLboolean *params), (pname, params))
GLW_VOID(glGetTexImage, (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels), (target, level, format, type, pixels))
GLW_VOID(glGetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint *params), (target, level, pname, params))
GLW_VOID(glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l, r, b, t, n, f))
