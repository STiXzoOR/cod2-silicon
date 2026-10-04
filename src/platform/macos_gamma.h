#ifndef COD2_MACOS_GAMMA_H
#define COD2_MACOS_GAMMA_H
#if defined(COD2_X64)

typedef struct {
    GLuint program, scene, ramp;
    int width, height, failed;
    unsigned int revision;
} MacPresentationGamma;

static unsigned short presentationGammaRamp[768];
static unsigned int presentationGammaRevision;
static int presentationGammaIdentity = 1;

static GLuint GammaCompile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    GLint compiled;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "CoD2 Silicon presentation gamma shader: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int GammaCreate(MacPresentationGamma *gamma)
{
    static const char vertexSource[] =
        "#version 120\n"
        "void main() { gl_Position = gl_Vertex; gl_TexCoord[0] = gl_MultiTexCoord0; }\n";
    static const char fragmentSource[] =
        "#version 120\n"
        "uniform sampler2D scene; uniform sampler1D ramp;\n"
        "void main() {\n"
        " vec4 pixel = texture2D(scene, gl_TexCoord[0].xy);\n"
        " vec3 index = (pixel.rgb * 255.0 + 0.5) / 256.0;\n"
        " gl_FragColor = vec4(texture1D(ramp, index.r).r,\n"
        "                     texture1D(ramp, index.g).g,\n"
        "                     texture1D(ramp, index.b).b, pixel.a);\n"
        "}\n";
    GLuint vertex = GammaCompile(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = GammaCompile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        gamma->failed = 1;
        return 0;
    }
    gamma->program = glCreateProgram();
    glAttachShader(gamma->program, vertex);
    glAttachShader(gamma->program, fragment);
    glLinkProgram(gamma->program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked;
    glGetProgramiv(gamma->program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(gamma->program, sizeof(log), NULL, log);
        fprintf(stderr, "CoD2 Silicon presentation gamma link: %s\n", log);
        glDeleteProgram(gamma->program);
        gamma->program = 0;
        gamma->failed = 1;
        return 0;
    }
    glGenTextures(1, &gamma->scene);
    glGenTextures(1, &gamma->ramp);
    return 1;
}

/* The scene FBO stays unchanged for screenshots, AUX reads and later draws. */
static int GammaPresent(MacPresentationGamma *gamma, int width, int height,
                        int x, int y, int destWidth, int destHeight)
{
    if (presentationGammaIdentity || gamma->failed)
        return 0;
    if (!gamma->program && !GammaCreate(gamma))
        return 0;

    GLint program, activeTexture, unpackBuffer;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gamma->scene);
    if (gamma->width != width || gamma->height != height) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        gamma->width = width;
        gamma->height = height;
    }
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_1D, gamma->ramp);
    if (gamma->revision != presentationGammaRevision) {
        unsigned short pixels[768];
        for (int i = 0; i < 256; ++i)
            for (int channel = 0; channel < 3; ++channel)
                pixels[i * 3 + channel] = presentationGammaRamp[channel * 256 + i];
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB16, 256, 0, GL_RGB, GL_UNSIGNED_SHORT, pixels);
        gamma->revision = presentationGammaRevision;
    }
    glPopClientAttrib();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);

    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    for (int i = 0; i < 6; ++i)
        glDisable(GL_CLIP_PLANE0 + i);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(x, y, destWidth, destHeight);
    glUseProgram(gamma->program);
    glUniform1i(glGetUniformLocation(gamma->program, "scene"), 0);
    glUniform1i(glGetUniformLocation(gamma->program, "ramp"), 1);
    glBegin(GL_QUADS);
    glMultiTexCoord2f(GL_TEXTURE0, 0, 0); glVertex2f(-1, -1);
    glMultiTexCoord2f(GL_TEXTURE0, 1, 0); glVertex2f(1, -1);
    glMultiTexCoord2f(GL_TEXTURE0, 1, 1); glVertex2f(1, 1);
    glMultiTexCoord2f(GL_TEXTURE0, 0, 1); glVertex2f(-1, 1);
    glEnd();
    glUseProgram(program);
    glActiveTexture(activeTexture);
    return 1;
}

static void GammaRelease(MacPresentationGamma *gamma)
{
    glDeleteProgram(gamma->program);
    glDeleteTextures(1, &gamma->scene);
    glDeleteTextures(1, &gamma->ramp);
}

#endif
#endif
