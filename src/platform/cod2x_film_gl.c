#include "cod2x_film_gl.h"
#if defined(__APPLE__) && defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <stdint.h>
#include <string.h>

/* Independently implemented from the colour operations specified by CoD2x
 * main's filmtweak.cpp/filmtweak.hlsl. No upstream shader assets are used. */
static const char vertexSource[] =
    "#version 120\n"
    "void main() { gl_Position = gl_Vertex; }\n";
static const char fragmentSource[] =
    "#version 120\n"
    "uniform sampler2D image;\n"
    "uniform vec2 pixelSize;\n"
    "uniform vec4 adjustment;\n" /* contrast, bias, saturation, inverse gamma */
    "uniform vec3 shadowGain, highlightGain;\n"
    "void main() {\n"
    " vec3 rgb = texture2D(image, gl_FragCoord.xy * pixelSize).rgb;\n"
    " rgb = rgb * adjustment.x + adjustment.y;\n"
    " float grey = dot(rgb, vec3(0.299, 0.587, 0.114));\n"
    " rgb = vec3(grey) + (rgb - vec3(grey)) * adjustment.z;\n"
    " rgb *= shadowGain + highlightGain * clamp(grey, 0.0, 1.0);\n"
    " gl_FragColor = vec4(pow(clamp(rgb, 0.0, 1.0), vec3(adjustment.w)), 1.0);\n"
    "}\n";

static struct {
    CGLContextObj context;
    GLuint program, texture, framebuffer;
    GLint adjustment, shadowGain, highlightGain, pixelSize;
    int width, height;
} film;

void Cod2xFilm_GLRelease(void)
{
    if (film.context && film.context == CGLGetCurrentContext()) {
        if (film.program) glDeleteProgram(film.program);
        if (film.texture) glDeleteTextures(1, &film.texture);
        if (film.framebuffer) glDeleteFramebuffersEXT(1, &film.framebuffer);
    }
    memset(&film, 0, sizeof(film));
}

static GLuint Compile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int PrepareProgram(void)
{
    if (film.program)
        return 1;
    GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = vertex && fragment ? glCreateProgram() : 0;
    GLint linked = 0;
    if (program) {
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
    }
    if (vertex) glDeleteShader(vertex);
    if (fragment) glDeleteShader(fragment);
    if (!linked) {
        if (program) glDeleteProgram(program);
        return 0;
    }
    film.program = program;
    film.adjustment = glGetUniformLocation(program, "adjustment");
    film.shadowGain = glGetUniformLocation(program, "shadowGain");
    film.highlightGain = glGetUniformLocation(program, "highlightGain");
    film.pixelSize = glGetUniformLocation(program, "pixelSize");
    return 1;
}

static void Draw(const Cod2xFilmPass *pass, int left, int bottom, int right, int top, float saturation)
{
    const Cod2xFilmSettings *s = &pass->settings;
    float exponent = 1.0f / (s->gamma < .5f ? .5f : s->gamma);
    glUniform4f(film.adjustment, s->contrast, s->brightness + .5f - .5f * s->contrast, saturation, exponent);
    float x0 = 2.0f * left / pass->targetWidth - 1.0f;
    float x1 = 2.0f * right / pass->targetWidth - 1.0f;
    float y0 = 2.0f * bottom / pass->targetHeight - 1.0f;
    float y1 = 2.0f * top / pass->targetHeight - 1.0f;
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(x0, y0, 1); glVertex3f(x1, y0, 1);
    glVertex3f(x0, y1, 1); glVertex3f(x1, y1, 1);
    glEnd();
}

const char *Cod2xFilm_GLApply(const Cod2xFilmPass *pass)
{
    if (pass->targetWidth <= 0 || pass->targetHeight <= 0)
        return NULL;
    int64_t endX = (int64_t)pass->x + pass->width, endY = (int64_t)pass->y + pass->height;
    int left = pass->x < 0 ? 0 : pass->x;
    int topDown = pass->y < 0 ? 0 : pass->y;
    int right = endX > pass->targetWidth ? pass->targetWidth : (int)endX;
    int endDown = endY > pass->targetHeight ? pass->targetHeight : (int)endY;
    if (right <= left || endDown <= topDown)
        return NULL;
    int bottom = pass->targetHeight - endDown, top = pass->targetHeight - topDown;
    CGLContextObj context = CGLGetCurrentContext();
    if (!context)
        return "there is no current OpenGL context";
    if (film.context != context) {
        Cod2xFilm_GLRelease();
        film.context = context;
    }
    if (!PrepareProgram())
        return "the film shader could not be compiled";

    GLint readFramebuffer, drawFramebuffer, drawBuffer, program, activeTexture, unpackBuffer;
    GLint sourceReadBuffer = 0;
    int sourceReadChanged = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT, &readFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT, &drawFramebuffer);
    glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    const char *error = NULL;

    if (!film.texture) glGenTextures(1, &film.texture);
    glBindTexture(GL_TEXTURE_2D, film.texture);
    if (!film.framebuffer) glGenFramebuffersEXT(1, &film.framebuffer);
    if (film.width != pass->targetWidth || film.height != pass->targetHeight) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pass->targetWidth, pass->targetHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        film.width = pass->targetWidth;
        film.height = pass->targetHeight;
    }
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, film.framebuffer);
    glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, film.texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    if (glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        error = "the scene copy framebuffer could not be created";
        goto restore;
    }

    /* Resolve the actual drawing target, which can differ from the shim's
     * current reading target during MSAA. The scene depth stays attached. */
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, drawFramebuffer);
    glGetIntegerv(GL_READ_BUFFER, &sourceReadBuffer);
    glReadBuffer(drawBuffer);
    sourceReadChanged = 1;
    glBlitFramebufferEXT(left, bottom, right, top, left, bottom, right, top, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, drawFramebuffer);
    glDrawBuffer(drawBuffer);

    glUseProgram(film.program);
    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FOG);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_STIPPLE);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_SAMPLE_COVERAGE);
    glDisable(GL_DITHER);
    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    for (int i = 0; i < 6; ++i) glDisable(GL_CLIP_PLANE0 + i);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, pass->targetWidth, pass->targetHeight);
    glDepthRange(0, 1);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUniform1i(glGetUniformLocation(film.program, "image"), 0);
    glUniform2f(film.pixelSize, 1.0f / pass->targetWidth, 1.0f / pass->targetHeight);
    float shadow[3], highlight[3];
    for (int i = 0; i < 3; ++i) {
        shadow[i] = pass->settings.color[i] * pass->settings.darkTint[i];
        highlight[i] = pass->settings.color[i] * (pass->settings.lightTint[i] - pass->settings.darkTint[i]);
    }
    glUniform3fv(film.shadowGain, 1, shadow);
    glUniform3fv(film.highlightGain, 1, highlight);

    GLint depthBits = 0;
    glGetIntegerv(GL_DEPTH_BITS, &depthBits);
    if (depthBits && pass->settings.skySaturation != pass->settings.saturation) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        Draw(pass, left, bottom, right, top, pass->settings.skySaturation);
        glDepthFunc(GL_GREATER);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    Draw(pass, left, bottom, right, top, pass->settings.saturation);

restore:
    glUseProgram(program);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
    if (sourceReadChanged) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, drawFramebuffer);
        glReadBuffer(sourceReadBuffer);
    }
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, drawFramebuffer);
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, readFramebuffer);
    glPopAttrib();
    glActiveTexture(activeTexture);
    return error;
}
#endif
