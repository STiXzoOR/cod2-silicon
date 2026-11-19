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

/* GL_APPLE_fence is a macOS-only GPU-sync extension. On Windows OpenGL it is
 * either absent or the driver's stub busy-waits forever -> hang on map load
 * (RB_ExecuteRenderCommands spins in glSetFenceAPPLE -> ZwYieldExecution). No-op
 * it: the driver synchronizes implicitly on buffer reuse / SwapBuffers, and
 * glTestFenceAPPLE always reports "complete" so the engine's GPU-wait loops exit.
 * (Drops the CPU/GPU-throttle optimization only; safe.) */
void glGenFencesAPPLE(GLsizei n, GLuint *fences) { GLsizei i; for (i = 0; i < n; i++) fences[i] = 1; }
void glDeleteFencesAPPLE(GLsizei n, const GLuint *fences) { (void)n; (void)fences; }
void glSetFenceAPPLE(GLuint fence) { (void)fence; }
GLboolean glTestFenceAPPLE(GLuint fence) { (void)fence; return (GLboolean)1; }
void glFinishFenceAPPLE(GLuint fence) { (void)fence; }

#if defined(COD2_X64)
/* GL_APPLE_vertex_array_object exists only on Apple's GL. The ARB entry points are
 * the same objects with the same arguments on a compatibility context. */
static void *gl_resolve_vao(const char *apple, const char *arb)
{
    void *p = gl_resolve(apple);
    return p ? p : gl_resolve(arb);
}
void glBindVertexArrayAPPLE(GLuint array)
{
    typedef void(__stdcall *PFN)(GLuint);
    static PFN p;
    if (!p) p = (PFN)gl_resolve_vao("glBindVertexArrayAPPLE", "glBindVertexArray");
    if (p) p(array);
}
void glGenVertexArraysAPPLE(GLsizei n, GLuint *arrays)
{
    typedef void(__stdcall *PFN)(GLsizei, GLuint *);
    static PFN p;
    if (!p) p = (PFN)gl_resolve_vao("glGenVertexArraysAPPLE", "glGenVertexArrays");
    if (p) p(n, arrays);
}
void glDeleteVertexArraysAPPLE(GLsizei n, const GLuint *arrays)
{
    typedef void(__stdcall *PFN)(GLsizei, const GLuint *);
    static PFN p;
    if (!p) p = (PFN)gl_resolve_vao("glDeleteVertexArraysAPPLE", "glDeleteVertexArrays");
    if (p) p(n, arrays);
}
#else
GLW_VOID(glBindVertexArrayAPPLE, (GLuint array), (array))
GLW_VOID(glGenVertexArraysAPPLE, (GLsizei n, GLuint *arrays), (n, arrays))
GLW_VOID(glDeleteVertexArraysAPPLE, (GLsizei n, const GLuint *arrays), (n, arrays))
#endif
#if !defined(COD2_X64) || !COD2_WINDOWS_NATIVE /* macos_display.c has this one */
GLW_VOID(glVertexArrayParameteriAPPLE, (GLenum pname, GLint param), (pname, param))
#endif
GLW_VOID(glVertexArrayRangeAPPLE, (GLsizei length, GLvoid *pointer), (length, pointer))
GLW_VOID(glFlushVertexArrayRangeAPPLE, (GLsizei length, GLvoid *pointer), (length, pointer))

GLW_VOID(glBindVertexArray, (GLuint array), (array))
GLW_VOID(glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices), (mode, count, type, indices))
GLW_VOID(glGetBooleanv, (GLenum pname, GLboolean *params), (pname, params))
GLW_VOID(glGetTexImage, (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels), (target, level, format, type, pixels))
GLW_VOID(glGetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint *params), (target, level, pname, params))
GLW_VOID(glOrtho, (GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f), (l, r, b, t, n, f))
#if defined(COD2_X64)

/* The reconciled renderer's framebuffer, shader and fixed-function entry points. */
GLW_VOID(glActiveTexture, (GLenum texture), (texture))
GLW_VOID(glAttachShader, (GLuint program, GLuint shader), (program, shader))
GLW_VOID(glBindBuffer, (GLenum target, GLuint buffer), (target, buffer))
GLW_VOID(glBindBufferARB, (GLenum target, GLuint buffer), (target, buffer))
GLW_VOID(glBindFramebufferEXT, (GLenum target, GLuint framebuffer), (target, framebuffer))
GLW_VOID(glBindRenderbufferEXT, (GLenum target, GLuint renderbuffer), (target, renderbuffer))
GLW_VOID(glBlitFramebufferEXT,
    (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
     GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter),
    (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter))
GLW_RET(GLenum, glCheckFramebufferStatusEXT, (GLenum target), (target))
GLW_VOID(glCompileShader, (GLuint shader), (shader))
GLW_RET(GLuint, glCreateProgram, (void), ())
GLW_RET(GLuint, glCreateShader, (GLenum type), (type))
GLW_VOID(glDeleteFramebuffersEXT, (GLsizei n, const GLuint *framebuffers), (n, framebuffers))
GLW_VOID(glDeleteProgram, (GLuint program), (program))
GLW_VOID(glDeleteRenderbuffersEXT, (GLsizei n, const GLuint *renderbuffers), (n, renderbuffers))
GLW_VOID(glDeleteShader, (GLuint shader), (shader))
GLW_VOID(glFramebufferRenderbufferEXT,
    (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer),
    (target, attachment, renderbuffertarget, renderbuffer))
GLW_VOID(glGenFramebuffersEXT, (GLsizei n, GLuint *framebuffers), (n, framebuffers))
GLW_VOID(glGenRenderbuffersEXT, (GLsizei n, GLuint *renderbuffers), (n, renderbuffers))
GLW_VOID(glGetDoublev, (GLenum pname, GLdouble *params), (pname, params))
GLW_VOID(glGetProgramInfoLog, (GLuint program, GLsizei size, GLsizei *length, char *log),
    (program, size, length, log))
GLW_VOID(glGetProgramiv, (GLuint program, GLenum pname, GLint *params), (program, pname, params))
GLW_VOID(glGetShaderInfoLog, (GLuint shader, GLsizei size, GLsizei *length, char *log),
    (shader, size, length, log))
GLW_VOID(glGetShaderiv, (GLuint shader, GLenum pname, GLint *params), (shader, pname, params))
GLW_VOID(glGetTexParameterfv, (GLenum target, GLenum pname, GLfloat *params), (target, pname, params))
GLW_VOID(glGetTexParameteriv, (GLenum target, GLenum pname, GLint *params), (target, pname, params))
GLW_RET(GLint, glGetUniformLocation, (GLuint program, const char *name), (program, name))
GLW_RET(GLboolean, glIsEnabled, (GLenum cap), (cap))
GLW_VOID(glLinkProgram, (GLuint program), (program))
GLW_VOID(glLoadMatrixd, (const GLdouble *m), (m))
GLW_VOID(glMultiTexCoord2f, (GLenum target, GLfloat s, GLfloat t), (target, s, t))
GLW_VOID(glRenderbufferStorageEXT, (GLenum target, GLenum format, GLsizei width, GLsizei height),
    (target, format, width, height))
#if COD2_WINDOWS_NATIVE /* multisampled render targets, native builds only */
GLW_VOID(glRenderbufferStorageMultisampleEXT,
    (GLenum target, GLsizei samples, GLenum format, GLsizei width, GLsizei height),
    (target, samples, format, width, height))
#endif
GLW_VOID(glShaderSource, (GLuint shader, GLsizei count, const char *const *source, const GLint *length),
    (shader, count, source, length))
GLW_VOID(glTexImage1D,
    (GLenum target, GLint level, GLint internal, GLsizei width, GLint border,
     GLenum format, GLenum type, const GLvoid *pixels),
    (target, level, internal, width, border, format, type, pixels))
GLW_VOID(glUniform1i, (GLint location, GLint value), (location, value))
GLW_VOID(glUseProgram, (GLuint program), (program))
GLW_VOID(glVertexAttrib4fARB, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (index, x, y, z, w))
#endif

#if defined(COD2_X64)
/* What the OpenGL device layer calls beyond the list above: two-sided stencil,
 * occlusion queries, alpha-tested edges under multisampling, and the state it
 * reads back before a copy. */
GLW_VOID(glBeginQueryARB, (GLenum target, GLuint id), (target, id))
GLW_VOID(glBlendEquationSeparate, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha))
GLW_VOID(glDeleteQueriesARB, (GLsizei n, const GLuint *ids), (n, ids))
GLW_VOID(glEndQueryARB, (GLenum target), (target))
GLW_VOID(glFogCoordPointer, (GLenum type, GLsizei stride, const GLvoid *pointer), (type, stride, pointer))
GLW_VOID(glFramebufferTexture2DEXT, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level),
    (target, attachment, textarget, texture, level))
GLW_VOID(glGenQueriesARB, (GLsizei n, GLuint *ids), (n, ids))
GLW_VOID(glGetFramebufferAttachmentParameterivEXT, (GLenum target, GLenum attachment, GLenum pname, GLint *params),
    (target, attachment, pname, params))
GLW_VOID(glGetProgramEnvParameterfvARB, (GLenum target, GLuint index, GLfloat *params), (target, index, params))
GLW_VOID(glGetProgramStringARB, (GLenum target, GLenum pname, GLvoid *string), (target, pname, string))
GLW_VOID(glGetQueryObjectuivARB, (GLuint id, GLenum pname, GLuint *params), (id, pname, params))
GLW_VOID(glGetRenderbufferParameterivEXT, (GLenum target, GLenum pname, GLint *params), (target, pname, params))
GLW_VOID(glGetVertexAttribPointervARB, (GLuint index, GLenum pname, GLvoid **pointer), (index, pname, pointer))
GLW_VOID(glGetVertexAttribivARB, (GLuint index, GLenum pname, GLint *params), (index, pname, params))
GLW_VOID(glProgramLocalParameter4fARB, (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w),
    (target, index, x, y, z, w))
GLW_VOID(glSampleCoverage, (GLclampf value, GLboolean invert), (value, invert))
GLW_VOID(glStencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask))
GLW_VOID(glStencilOpSeparate, (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass), (face, sfail, dpfail, dppass))
#endif
