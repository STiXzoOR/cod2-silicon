/* Windows 1.3 transparency AA: ADAPTIVETESS_Y = ATOC or SSAA.
 * The legacy GL context has no sample-shading extension. SSAA replays the
 * unshifted primitive once per sample, selecting that sample with private
 * stencil tags and evaluating the original fragment program's interpolants
 * there. Scene color, depth, coverage, and stencil storage stay independent.
 * This scope deliberately bypasses COpenGL's persistent state caches. */
#if COD2_APPLE_SDK
typedef struct DeviceAlphaAAProgram {
    struct DeviceAlphaAAProgram *next;
    char *source;
    GLuint program;
} DeviceAlphaAAProgram;

typedef struct DeviceAlphaAA {
    GLuint framebuffer, stencil;
    GLint width, height, samples;
    GLuint color, depth;
    GLenum colorType, depthType;
    int tagsValid;
    DeviceAlphaAAProgram *programs;
    unsigned warned;
} DeviceAlphaAA;

typedef struct {
    char *text;
    size_t used, capacity;
    int failed;
} AlphaAAText;

static void AlphaAA_Append(AlphaAAText *out, const char *format, ...)
{
    if (out->failed) return;
    va_list arguments;
    va_start(arguments, format);
    int size = vsnprintf(out->text + out->used, out->capacity - out->used, format, arguments);
    va_end(arguments);
    if (size < 0 || (size_t)size >= out->capacity - out->used) out->failed = 1;
    else out->used += size;
}

static const char *AlphaAA_Space(const char *p)
{
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        if (*p != '#') return p;
        while (*p && *p != '\n') ++p;
    }
}

static int AlphaAA_Attribute(const char *p, size_t *length)
{
    if (!strncmp(p, "fragment.texcoord[", 18)) {
        const char *end = p + 18;
        while (*end == ' ') ++end;
        int index = *end++ - '0';
        while (*end == ' ') ++end;
        if (index >= 0 && index < 8 && *end == ']') {
            *length = (size_t)(end + 1 - p); return index;
        }
    }
    const char *names[] = {"fragment.color.primary", "fragment.color.secondary",
        "fragment.fogcoord", "fragment.position", "fragment.color"};
    for (int j = 0; j < 5; ++j) {
        size_t n = strlen(names[j]);
        if (!strncmp(p, names[j], n)) { *length = n; return j == 4 ? 8 : 8 + j; }
    }
    return -1;
}

static void AlphaAA_Rewrite(AlphaAAText *out, const char *begin, const char *end)
{
    while (begin < end) {
        size_t length;
        int attribute = AlphaAA_Attribute(begin, &length);
        if (attribute >= 0) {
            AlphaAA_Append(out, "_cod2aaV%d", attribute); begin += length;
        } else if (!strncmp(begin, "ATTRIB ", 7)) {
            /* ARB attribute aliases become aliases of the corrected TEMP. */
            AlphaAA_Append(out, "ALIAS "); begin += 7;
        } else {
            AlphaAA_Append(out, "%c", *begin++);
        }
    }
}

static char *AlphaAA_SampleProgram(const char *source)
{
    if (strncmp(source, "!!ARBfp1.0", 10) || strstr(source, "program.local[0]")) return NULL;
    const char *declarations = strchr(source, '\n');
    if (!declarations) return NULL;
    ++declarations;
    const char *body = declarations;
    for (;;) {
        const char *token = AlphaAA_Space(body);
        const char *names[] = {"OPTION", "ATTRIB", "OUTPUT", "PARAM", "TEMP", "ALIAS", "ADDRESS"};
        int declaration = 0;
        for (int j = 0; j < 7; ++j) {
            size_t n = strlen(names[j]);
            if (!strncmp(token, names[j], n) && (token[n] == ' ' || token[n] == '\t')) declaration = 1;
        }
        if (!declaration) { body = token; break; }
        body = strchr(token, ';');
        if (!body) return NULL;
        ++body;
    }
    unsigned attributes = 0;
    for (const char *p = source; *p; ++p) {
        size_t length;
        int attribute = AlphaAA_Attribute(p, &length);
        if (attribute >= 0) { attributes |= 1u << attribute; p += length - 1; }
    }
    AlphaAAText out = {0};
    out.capacity = strlen(source) * 3 + 16384;
    out.text = calloc(1, out.capacity);
    if (!out.text) return NULL;
    AlphaAA_Append(&out, "!!ARBfp1.0\nOPTION NV_fragment_program;\n");
    const char *optionEnd=declarations;
    while (!strncmp(AlphaAA_Space(optionEnd),"OPTION ",7)) {
        const char *end=strchr(AlphaAA_Space(optionEnd),';');
        if (!end) break;
        optionEnd=end+1;
    }
    AlphaAA_Append(&out,"%.*s\nPARAM _cod2aaOffset = program.local[0];\nTEMP _cod2aaD, _cod2aaDx, _cod2aaDy;\n",
        (int)(optionEnd-declarations),declarations);
    for (int j = 0; j < 12; ++j)
        if (attributes & (1u << j)) AlphaAA_Append(&out, "TEMP _cod2aaV%d;\n", j);
    AlphaAA_Rewrite(&out, optionEnd, body);
    AlphaAA_Append(&out, "\nMOV _cod2aaD, fragment.position.wwww;\n"
        "DDX _cod2aaDx, _cod2aaD;\nDDY _cod2aaDy, _cod2aaD;\n"
        "MAD _cod2aaD, _cod2aaDx, _cod2aaOffset.x, _cod2aaD;\n"
        "MAD _cod2aaD, _cod2aaDy, _cod2aaOffset.y, _cod2aaD;\n"
        "RCP _cod2aaD.x, _cod2aaD.x;\n");
    for (int j = 0; j < 11; ++j) if (attributes & (1u << j)) {
        char attribute[48];
        if (j < 8) snprintf(attribute, sizeof(attribute), "fragment.texcoord[%d]", j);
        else snprintf(attribute, sizeof(attribute), "%s", j == 8 ? "fragment.color.primary" :
            j == 9 ? "fragment.color.secondary" : "fragment.fogcoord");
        AlphaAA_Append(&out, "MUL _cod2aaV%d, %s, fragment.position.w;\n"
            "DDX _cod2aaDx, _cod2aaV%d;\nDDY _cod2aaDy, _cod2aaV%d;\n"
            "MAD _cod2aaV%d, _cod2aaDx, _cod2aaOffset.x, _cod2aaV%d;\n"
            "MAD _cod2aaV%d, _cod2aaDy, _cod2aaOffset.y, _cod2aaV%d;\n"
            "MUL _cod2aaV%d, _cod2aaV%d, _cod2aaD.x;\n", j, attribute,
            j,j,j,j,j,j,j,j);
    }
    if (attributes & (1u << 11)) AlphaAA_Append(&out,
        "MOV _cod2aaV11, fragment.position;\n"
        "ADD _cod2aaV11.xy, _cod2aaV11, _cod2aaOffset;\n"
        "DDX _cod2aaDx, fragment.position;\nDDY _cod2aaDy, fragment.position;\n"
        "MAD _cod2aaV11.zw, _cod2aaDx, _cod2aaOffset.x, _cod2aaV11;\n"
        "MAD _cod2aaV11.zw, _cod2aaDy, _cod2aaOffset.y, _cod2aaV11;\n");
    AlphaAA_Rewrite(&out, body, source + strlen(source));
    if (out.failed) { free(out.text); return NULL; }
    return out.text;
}

static void AlphaAA_Quad(float right, float top)
{
    glBegin(GL_QUADS);
    glVertex2f(-1,-1); glVertex2f(right,-1); glVertex2f(right,top); glVertex2f(-1,top);
    glEnd();
}

/* The initial value N includes every sample. Each smaller coverage mask
 * decrements its nested prefix, leaving exactly one sample at each tag 1..N.
 * An optional predicate leaves rejected samples at zero throughout. */
static void AlphaAA_Tags(int samples)
{
    glEnable(GL_SAMPLE_COVERAGE);
    glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
    for (int tag = samples; tag > 1; --tag) {
        glStencilFunc(GL_EQUAL, tag, 255);
        glSampleCoverage((float)(tag - 1) / samples, GL_FALSE);
        AlphaAA_Quad(1,1);
    }
    glDisable(GL_SAMPLE_COVERAGE);
    glStencilMask(0); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}

typedef struct {
    int checked, valid;
    GLfloat position[4][2];
} AlphaAASamplePattern;
static AlphaAASamplePattern sAlphaAAPattern[2];

/* Query-free legacy contexts do not expose sample positions. Calibrate their
 * exact raster locations and tag partition in an 8x8 private FBO. Reject a
 * driver with varying positions or overlapping/non-unit tags. No drawable. */
static AlphaAASamplePattern *AlphaAA_Pattern(int samples)
{
    if (samples != 2 && samples != 4) return NULL;
    AlphaAASamplePattern *pattern = &sAlphaAAPattern[samples == 4];
    if (pattern->checked) return pattern->valid ? pattern : NULL;
    pattern->checked = 1;
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (!extensions || !strstr(extensions, "GL_NV_fragment_program_option")) return NULL;
    GLint maximumSamples; glGetIntegerv(GL_MAX_SAMPLES_EXT,&maximumSamples);
    if (samples>maximumSamples) return NULL;
    GLint draw, read, renderbuffer, matrix, vp, fp;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&read);
    glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT,&renderbuffer); glGetIntegerv(GL_MATRIX_MODE,&matrix);
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&vp);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&fp);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT,1); glPixelStorei(GL_PACK_ROW_LENGTH,0);
    glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_DEPTH_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glDisable(GL_FOG); glDisable(GL_LIGHTING); glDisable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_ALPHA_TO_ONE); glEnable(GL_MULTISAMPLE);
    GLint units; glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);
    for (int unit=0;unit<units;++unit) { glActiveTextureARB(GL_TEXTURE0_ARB+unit); glDisable(GL_TEXTURE_2D); glDisable(GL_TEXTURE_3D); glDisable(GL_TEXTURE_CUBE_MAP); }
    GLuint framebuffer[2], buffers[3];
    glGenFramebuffersEXT(2,framebuffer); glGenRenderbuffersEXT(3,buffers);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer[0]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,buffers[0]); glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_RGBA8,8,8);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,buffers[0]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,buffers[1]); glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_STENCIL_INDEX8_EXT,8,8);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,buffers[1]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    int valid = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer[1]);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,buffers[2]); glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_RGBA8,8,8);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,buffers[2]);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    valid &= glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer[0]); glViewport(0,0,8,8);
    glEnable(GL_STENCIL_TEST); glStencilMask(255); glClearStencil(samples); glClear(GL_STENCIL_BUFFER_BIT);
    glColorMask(0,0,0,0); AlphaAA_Tags(samples); glColorMask(1,1,1,1); glColor4f(1,1,1,1);
    for (int tag=1;tag<=samples && valid;++tag) {
        glStencilFunc(GL_EQUAL,tag,255);
        for (int axis=0;axis<2 && valid;++axis) {
            int lower=0, upper=256;
            for (int iteration=0;iteration<9;++iteration) {
                int edge = iteration==0 ? 256 : (lower+upper)/2;
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer[0]); glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
                glBegin(GL_QUADS);
                for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
                    float right=x+(axis==0 ? edge/256.0f : 1), top=y+(axis==1 ? edge/256.0f : 1);
                    glVertex2f(x*.25f-1,y*.25f-1); glVertex2f(right*.25f-1,y*.25f-1);
                    glVertex2f(right*.25f-1,top*.25f-1); glVertex2f(x*.25f-1,top*.25f-1);
                }
                glEnd();
                glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,framebuffer[0]); glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,framebuffer[1]);
                glBlitFramebufferEXT(0,0,8,8,0,0,8,8,GL_COLOR_BUFFER_BIT,GL_NEAREST);
                glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,framebuffer[1]);
                unsigned char pixels[8*8*4]; glReadPixels(0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
                for (int pixel=0;pixel<64;++pixel) if (pixels[pixel*4]!=pixels[0]) valid=0;
                if (iteration==0) { if (abs((int)pixels[0]-(int)lroundf(255.0f/samples))>1) valid=0; }
                else if (pixels[0]) upper=edge; else lower=edge;
            }
            /* Edge samples obey the top/right exclusion rule. The last
             * rejected edge is the sample's coordinate on this 1/256 grid. */
            pattern->position[tag-1][axis]=lower/256.0f;
        }
    }
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,draw); glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,read);
    glDeleteFramebuffersEXT(2,framebuffer); glDeleteRenderbuffersEXT(3,buffers); glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,renderbuffer);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB,vp); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp);
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(matrix);
    glPopClientAttrib(); glPopAttrib();
    pattern->valid=valid;
    return valid ? pattern : NULL;
}

int CDirect3DDevice_SupportsTransparencyAA(void)
{
    return glGetString(GL_VERSION) && AlphaAA_Pattern(2) && AlphaAA_Pattern(4);
}

static void AlphaAA_Diagnostic(const char *mode, int samples)
{
    static int enabled=-1;
    static unsigned reported;
    if (enabled<0) enabled=getenv("COD2_MAC_ALPHA_AA_DIAGNOSTICS")!=NULL;
    unsigned bit=1u<<((mode[0]=='S' ? 2 : 0)+(samples==4));
    if (enabled && !(reported&bit)) {
        fprintf(stderr,"alpha AA: %s active at %dx MSAA\n",mode,samples); reported|=bit;
    }
}

static void DeviceAlphaAA_Destroy(DeviceImpl *dev)
{
    DeviceAlphaAA *aa=dev->alphaAA;
    if (!aa) return;
    glDeleteFramebuffersEXT(1,&aa->framebuffer); glDeleteRenderbuffersEXT(1,&aa->stencil);
    while (aa->programs) {
        DeviceAlphaAAProgram *program=aa->programs; aa->programs=program->next;
        glDeleteProgramsARB(1,&program->program); free(program->source); free(program);
    }
    free(aa); dev->alphaAA=NULL;
}

static void AlphaAA_FixedArgument(AlphaAAText *out, int unit, int argument, GLenum source, GLenum operand)
{
    char name[64];
    if (source==GL_TEXTURE) snprintf(name,sizeof(name),"_cod2ffT%d",unit);
    else if (source==GL_PRIMARY_COLOR) snprintf(name,sizeof(name),"fragment.color.primary");
    else if (source==GL_PREVIOUS) snprintf(name,sizeof(name),"_cod2ffPrevious");
    else if (source==GL_CONSTANT) snprintf(name,sizeof(name),"state.texenv[%d].color",unit);
    else if (source>=GL_TEXTURE0_ARB && source<GL_TEXTURE0_ARB+8) snprintf(name,sizeof(name),"_cod2ffT%d",source-GL_TEXTURE0_ARB);
    else { out->failed=1; return; }
    AlphaAA_Append(out,"MOV _cod2ffA%d, %s%s;\n",argument,name,
        operand==GL_SRC_ALPHA || operand==GL_ONE_MINUS_SRC_ALPHA ? ".wwww" : "");
    if (operand==GL_ONE_MINUS_SRC_COLOR || operand==GL_ONE_MINUS_SRC_ALPHA)
        AlphaAA_Append(out,"SUB _cod2ffA%d, 1, _cod2ffA%d;\n",argument,argument);
}

static void AlphaAA_FixedCombine(AlphaAAText *out, int unit, int alpha)
{
    GLint operation,scale;
    glGetTexEnviv(GL_TEXTURE_ENV,alpha ? GL_COMBINE_ALPHA : GL_COMBINE_RGB,&operation);
    glGetTexEnviv(GL_TEXTURE_ENV,alpha ? GL_ALPHA_SCALE : GL_RGB_SCALE,&scale);
    int arguments=operation==GL_REPLACE ? 1 : operation==GL_INTERPOLATE ? 3 : 2;
    for (int argument=0;argument<arguments;++argument) {
        GLint source,operand;
        glGetTexEnviv(GL_TEXTURE_ENV,(alpha ? GL_SOURCE0_ALPHA : GL_SOURCE0_RGB)+argument,&source);
        glGetTexEnviv(GL_TEXTURE_ENV,(alpha ? GL_OPERAND0_ALPHA : GL_OPERAND0_RGB)+argument,&operand);
        AlphaAA_FixedArgument(out,unit,argument,source,operand);
    }
    const char *mask=alpha ? "w" : operation==GL_DOT3_RGBA ? "xyzw" : "xyz";
    switch (operation) {
    case GL_REPLACE: AlphaAA_Append(out,"MOV _cod2ffNext.%s,_cod2ffA0;\n",mask); break;
    case GL_MODULATE: AlphaAA_Append(out,"MUL _cod2ffNext.%s,_cod2ffA0,_cod2ffA1;\n",mask); break;
    case GL_ADD: AlphaAA_Append(out,"ADD _cod2ffNext.%s,_cod2ffA0,_cod2ffA1;\n",mask); break;
    case GL_ADD_SIGNED: AlphaAA_Append(out,"ADD _cod2ffNext.%s,_cod2ffA0,_cod2ffA1;\nSUB _cod2ffNext.%s,_cod2ffNext,.5;\n",mask,mask); break;
    case GL_SUBTRACT: AlphaAA_Append(out,"SUB _cod2ffNext.%s,_cod2ffA0,_cod2ffA1;\n",mask); break;
    case GL_INTERPOLATE: AlphaAA_Append(out,"LRP _cod2ffNext.%s,_cod2ffA2,_cod2ffA0,_cod2ffA1;\n",mask); break;
    case GL_DOT3_RGB: case GL_DOT3_RGBA:
        AlphaAA_Append(out,"MAD _cod2ffA0.xyz,_cod2ffA0,2,-1;\nMAD _cod2ffA1.xyz,_cod2ffA1,2,-1;\n"
            "DP3 _cod2ffNext.%s,_cod2ffA0,_cod2ffA1;\n",operation==GL_DOT3_RGBA ? "xyzw" : "xyz"); break;
    default: out->failed=1;
    }
    AlphaAA_Append(out,"MUL_SAT _cod2ffNext.%s,_cod2ffNext,%d;\n",mask,scale);
}

/* DX7's fixed-function stage equations are evaluated by the same sample
 * wrapper. Stage constants remain GL state bindings, not copied uniforms. */
static char *AlphaAA_FixedSource(DeviceImpl *dev)
{
    AlphaAAText out={0}; out.capacity=32768; out.text=calloc(1,out.capacity);
    if (!out.text) return NULL;
    GLint active,units; glGetIntegerv(GL_ACTIVE_TEXTURE_ARB,&active); glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);
    if (units>8) units=8;
    GLenum target[8]={0};
    AlphaAA_Append(&out,"!!ARBfp1.0\nTEMP _cod2ffPrevious,_cod2ffNext,_cod2ffA0,_cod2ffA1,_cod2ffA2,_cod2ffCoord,_cod2ffFog;\n");
    for (int unit=0;unit<units;++unit) {
        glActiveTextureARB(GL_TEXTURE0_ARB+unit);
        target[unit]=glIsEnabled(GL_TEXTURE_CUBE_MAP) ? GL_TEXTURE_CUBE_MAP :
            glIsEnabled(GL_TEXTURE_3D) ? GL_TEXTURE_3D : glIsEnabled(GL_TEXTURE_2D) ? GL_TEXTURE_2D : 0;
        if (target[unit]) AlphaAA_Append(&out,"TEMP _cod2ffT%d;\n",unit);
    }
    for (int unit=0;unit<units;++unit) if (target[unit]) {
        const char *kind=target[unit]==GL_TEXTURE_CUBE_MAP ? "CUBE" : target[unit]==GL_TEXTURE_3D ? "3D" : "2D";
        GLfloat bias=CDirect3DDevice_GL()->texUnits[unit].mLodBias;
        if (bias!=0) {
            if (target[unit]==GL_TEXTURE_CUBE_MAP) AlphaAA_Append(&out,"MOV _cod2ffCoord,fragment.texcoord[%d];\n",unit);
            else AlphaAA_Append(&out,"RCP _cod2ffCoord.w,fragment.texcoord[%d].w;\nMUL _cod2ffCoord,fragment.texcoord[%d],_cod2ffCoord.w;\n",unit,unit);
            AlphaAA_Append(&out,"MOV _cod2ffCoord.w,%.9g;\nTXB _cod2ffT%d,_cod2ffCoord,texture[%d],%s;\n",bias,unit,unit,kind);
        } else AlphaAA_Append(&out,"%s _cod2ffT%d,fragment.texcoord[%d],texture[%d],%s;\n",
            target[unit]==GL_TEXTURE_CUBE_MAP ? "TEX" : "TXP",unit,unit,unit,kind);
    }
    AlphaAA_Append(&out,"MOV _cod2ffPrevious,fragment.color.primary;\n");
    for (int unit=0;unit<units;++unit) if (target[unit]) {
        GLint mode; glActiveTextureARB(GL_TEXTURE0_ARB+unit); glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&mode);
        AlphaAA_Append(&out,"MOV _cod2ffNext,_cod2ffPrevious;\n");
        if (mode==GL_COMBINE) {
            GLint rgb; glGetTexEnviv(GL_TEXTURE_ENV,GL_COMBINE_RGB,&rgb);
            AlphaAA_FixedCombine(&out,unit,0);
            if (rgb!=GL_DOT3_RGBA) AlphaAA_FixedCombine(&out,unit,1);
        } else if (mode==GL_MODULATE) AlphaAA_Append(&out,"MUL_SAT _cod2ffNext,_cod2ffPrevious,_cod2ffT%d;\n",unit);
        else if (mode==GL_REPLACE) AlphaAA_Append(&out,"MOV _cod2ffNext,_cod2ffT%d;\n",unit);
        else if (mode==GL_ADD) AlphaAA_Append(&out,"ADD_SAT _cod2ffNext.xyz,_cod2ffPrevious,_cod2ffT%d;\nMUL _cod2ffNext.w,_cod2ffPrevious,_cod2ffT%d;\n",unit,unit);
        else if (mode==GL_DECAL) AlphaAA_Append(&out,"LRP _cod2ffNext.xyz,_cod2ffT%d.w,_cod2ffT%d,_cod2ffPrevious;\n",unit,unit);
        else if (mode==GL_BLEND) AlphaAA_Append(&out,"LRP _cod2ffNext.xyz,_cod2ffT%d,state.texenv[%d].color,_cod2ffPrevious;\nMUL _cod2ffNext.w,_cod2ffPrevious,_cod2ffT%d;\n",unit,unit,unit);
        else out.failed=1;
        AlphaAA_Append(&out,"MOV _cod2ffPrevious,_cod2ffNext;\n");
    }
    if (glIsEnabled(GL_COLOR_SUM_EXT)) AlphaAA_Append(&out,"ADD_SAT _cod2ffPrevious.xyz,_cod2ffPrevious,fragment.color.secondary;\n");
    if (glIsEnabled(GL_FOG)) {
        GLint mode; glGetIntegerv(GL_FOG_MODE,&mode);
        AlphaAA_Append(&out,"ABS _cod2ffFog.x,fragment.fogcoord.x;\n");
        if (mode==GL_LINEAR) AlphaAA_Append(&out,"SUB _cod2ffFog.x,state.fog.params.z,_cod2ffFog.x;\nMUL_SAT _cod2ffFog.x,_cod2ffFog.x,state.fog.params.w;\n");
        else {
            AlphaAA_Append(&out,"MUL _cod2ffFog.x,_cod2ffFog.x,state.fog.params.x;\n");
            if (mode==GL_EXP2) AlphaAA_Append(&out,"MUL _cod2ffFog.x,_cod2ffFog.x,_cod2ffFog.x;\n");
            AlphaAA_Append(&out,"MUL _cod2ffFog.x,_cod2ffFog.x,-1.44269504089;\nEX2_SAT _cod2ffFog.x,_cod2ffFog.x;\n");
        }
        AlphaAA_Append(&out,"LRP _cod2ffPrevious.xyz,_cod2ffFog.x,_cod2ffPrevious,state.fog.color;\n");
    }
    AlphaAA_Append(&out,"MOV result.color,_cod2ffPrevious;\nEND\n");
    glActiveTextureARB(active); (void)dev;
    if (out.failed) { free(out.text); return NULL; }
    return out.text;
}

static GLuint AlphaAA_Program(DeviceAlphaAA *aa, DeviceImpl *dev)
{
    char *source;
    if (!glIsEnabled(GL_FRAGMENT_PROGRAM_ARB)) source=AlphaAA_FixedSource(dev);
    else {
        GLint length; glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_LENGTH_ARB,&length);
        if (length<=0 || length>1024*1024) return 0;
        source=calloc(1,(size_t)length+1);
        if (source) glGetProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_STRING_ARB,source);
    }
    if (!source) return 0;
    for (DeviceAlphaAAProgram *entry=aa->programs;entry;entry=entry->next)
        if (!strcmp(entry->source,source)) { free(source); return entry->program; }
    char *converted=AlphaAA_SampleProgram(source);
    if (!converted) { free(source); return 0; }
    GLint previous; glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&previous);
    GLuint program; glGenProgramsARB(1,&program); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,program);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(converted),converted);
    GLint error; glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&error);
    if (error!=-1) {
        fprintf(stderr,"alpha AA sample program failed at %d: %s\n",error,(const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB));
        glGetError(); glDeleteProgramsARB(1,&program); program=0;
    }
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,previous); free(converted);
    if (!program) { free(source); return 0; }
    DeviceAlphaAAProgram *entry=calloc(1,sizeof(*entry));
    if (!entry) { glDeleteProgramsARB(1,&program); free(source); return 0; }
    entry->source=source; entry->program=program; entry->next=aa->programs; aa->programs=entry;
    return program;
}

static void AlphaAA_Attach(GLenum attachment, GLenum type, GLuint name)
{
    if (type==GL_RENDERBUFFER_EXT) glFramebufferRenderbufferEXT(GL_DRAW_FRAMEBUFFER_EXT,attachment,GL_RENDERBUFFER_EXT,name);
    else glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT,attachment,GL_TEXTURE_2D,name,0);
}

static int AlphaAA_Target(DeviceAlphaAA *aa, GLint samples, GLint drawBuffer)
{
    GLint colorType,color,depthType,depth,renderbuffer,width,height;
    glGetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER_EXT,drawBuffer,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT,&colorType);
    glGetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER_EXT,drawBuffer,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT,&color);
    glGetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT,&depthType);
    glGetFramebufferAttachmentParameterivEXT(GL_DRAW_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT,&depth);
    if (colorType!=GL_RENDERBUFFER_EXT || (depthType!=GL_RENDERBUFFER_EXT && depthType!=GL_NONE)) return 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING_EXT,&renderbuffer); glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,color);
    glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT,GL_RENDERBUFFER_WIDTH_EXT,&width);
    glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT,GL_RENDERBUFFER_HEIGHT_EXT,&height);
    if (!aa->framebuffer) glGenFramebuffersEXT(1,&aa->framebuffer);
    if (!aa->stencil) glGenRenderbuffersEXT(1,&aa->stencil);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,aa->framebuffer);
    int changed=aa->samples!=samples || aa->width!=width || aa->height!=height;
    if (changed) {
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,aa->stencil);
        /* Metal-backed legacy GL accepts separate depth/stencil attachments
         * at FBO validation, but aborts on the first actual draw. Keep the
         * private pair packed, and copy per-sample depth around the replay. */
        glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,samples,GL_DEPTH24_STENCIL8_EXT,width,height);
        aa->tagsValid=0;
    }
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,renderbuffer);
    AlphaAA_Attach(GL_COLOR_ATTACHMENT0_EXT,colorType,color);
    glFramebufferRenderbufferEXT(GL_DRAW_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,aa->stencil);
    glFramebufferRenderbufferEXT(GL_DRAW_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,aa->stencil);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    aa->color=color; aa->depth=depth; aa->colorType=colorType; aa->depthType=depthType;
    GLenum status=glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER_EXT);
    if (status!=GL_FRAMEBUFFER_COMPLETE_EXT) {
        if (!(aa->warned&4)) fprintf(stderr,"alpha AA: sample target unavailable (framebuffer 0x%x)\n",status);
        aa->warned|=4;
        /* Keep the last successful extent so a failed resize is retried. */
        return 0;
    }
    aa->samples=samples; aa->width=width; aa->height=height;
    return 1;
}

static int DeviceAlphaAA_Draw(DeviceImpl *dev, UINT32 first, UINT32 last, UINT32 count, const byte *indices)
{
    if (!dev->alphaAAFormat || !g_alphaTestEnable || dev->alphaBlendEnable) return 0;
    GLint samples; glGetIntegerv(GL_SAMPLES,&samples);
    if (samples<2) return 0;
    if (dev->alphaAAFormat==0x434f5441u) {
        GLboolean alpha=glIsEnabled(GL_ALPHA_TEST),coverage=glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_ALPHA_TEST); glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDrawRangeElements(GL_TRIANGLES,first,last,count,GL_UNSIGNED_SHORT,indices);
        if (alpha) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST);
        if (!coverage) glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        AlphaAA_Diagnostic("ATOC",samples);
        return 1;
    }
    if (dev->alphaAAFormat!=0x41415353u) return 0;
    if (!dev->alphaAA) dev->alphaAA=calloc(1,sizeof(*dev->alphaAA));
    DeviceAlphaAA *aa=dev->alphaAA;
    if (!aa) return 0;
    /* Stock alpha-tested materials use no stencil or sunpartition EQUAL/KEEP.
     * Keep arbitrary mod stencil mutations intact; never replace their tests
     * with our private sample tags. This is not general D3D9 emulation. */
    int stencil=dev->stencilEnable && !dev->depthStencilDetached;
    if (stencil && (dev->twoSidedStencil || dev->stencilFail!=1 || dev->stencilZFail!=1 || dev->stencilPass!=1 ||
        (dev->stencilFunc!=3 && dev->stencilFunc!=8 && dev->stencilFunc!=1))) {
        if (!(aa->warned&1)) fprintf(stderr,"alpha AA: mod stencil mutation/compare retains ordinary drawing\n");
        aa->warned|=1; return 0;
    }
    if (stencil && dev->stencilFunc==1) return 1;
    AlphaAASamplePattern *pattern=AlphaAA_Pattern(samples);
    GLuint program=pattern ? AlphaAA_Program(aa,dev) : 0;
    if (!program) {
        if (!(aa->warned&2)) fprintf(stderr,"alpha AA: sample program/pattern unavailable\n");
        aa->warned|=2; return 0;
    }
    GLint draw,read,drawBuffer,matrix,vp,fp;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_EXT,&draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_EXT,&read);
    glGetIntegerv(GL_DRAW_BUFFER,&drawBuffer); glGetIntegerv(GL_MATRIX_MODE,&matrix);
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&vp); glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&fp);
    if (!draw || drawBuffer<GL_COLOR_ATTACHMENT0_EXT || drawBuffer>GL_COLOR_ATTACHMENT15_EXT) return 0;
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    if (!AlphaAA_Target(aa,samples,drawBuffer)) { glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,draw); glPopAttrib(); return 0; }
    if (!dev->depthStencilDetached && aa->depth) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,draw);
        glBlitFramebufferEXT(0,0,aa->width,aa->height,0,0,aa->width,aa->height,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    }
    glDisable(GL_SCISSOR_TEST); glViewport(0,0,aa->width,aa->height);
    glDisable(GL_VERTEX_PROGRAM_ARB); glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_ALPHA_TO_ONE); glDisable(GL_SAMPLE_COVERAGE);
    glColorMask(0,0,0,0); glDepthMask(0); glEnable(GL_STENCIL_TEST); glStencilMask(255);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    if (stencil && dev->stencilFunc==3 && (dev->stencilMask&255)) {
        aa->tagsValid=0;
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,draw);
        glBlitFramebufferEXT(0,0,aa->width,aa->height,0,0,aa->width,aa->height,GL_STENCIL_BUFFER_BIT,GL_NEAREST);
        unsigned mask=dev->stencilMask&255, ref=dev->stencilRef&mask;
        if (!ref) {
            glStencilFunc(GL_ALWAYS,0,255); glStencilMask(mask); glStencilOp(GL_KEEP,GL_KEEP,GL_INVERT); AlphaAA_Quad(1,1); ref=mask;
        }
        glStencilMask(255); glStencilFunc(GL_NOTEQUAL,ref,mask); glStencilOp(GL_KEEP,GL_KEEP,GL_ZERO); AlphaAA_Quad(1,1);
        glStencilFunc(GL_ALWAYS,0,255); glStencilMask((~mask)&255); glStencilOp(GL_KEEP,GL_KEEP,GL_ZERO); AlphaAA_Quad(1,1);
        glStencilFunc(GL_EQUAL,ref,255); glStencilMask(ref^(unsigned)samples); glStencilOp(GL_KEEP,GL_KEEP,GL_INVERT); AlphaAA_Quad(1,1);
        glStencilMask(255);
    } else if (!aa->tagsValid) { glClearStencil(samples); glClear(GL_STENCIL_BUFFER_BIT); }
    if (!aa->tagsValid) {
        AlphaAA_Tags(samples);
        aa->tagsValid=!(stencil && dev->stencilFunc==3 && (dev->stencilMask&255));
    }
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(matrix);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB,vp); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp);
    /* Pop all actual draw state before the replay, then scope just the
     * sample-selector overrides. No persistent COpenGL cache was changed. */
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,draw); glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,read); glPopAttrib();
    glPushAttrib(GL_ENABLE_BIT|GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT|GL_MULTISAMPLE_BIT);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,aa->framebuffer); glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glEnable(GL_STENCIL_TEST); glStencilMask(0); glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE); glDisable(GL_SAMPLE_ALPHA_TO_ONE); glDisable(GL_SAMPLE_COVERAGE);
    glEnable(GL_ALPHA_TEST); glEnable(GL_FRAGMENT_PROGRAM_ARB); glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,program);
    for (int sample=0;sample<samples;++sample) {
        glStencilFunc(GL_EQUAL,sample+1,255);
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0,pattern->position[sample][0]-.5f,pattern->position[sample][1]-.5f,0,0);
        glDrawRangeElements(GL_TRIANGLES,first,last,count,GL_UNSIGNED_SHORT,indices);
    }
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB,fp); glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,draw);
    if (!dev->depthStencilDetached && dev->zEnable && dev->zWriteEnable && aa->depth) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,aa->framebuffer);
        glBlitFramebufferEXT(0,0,aa->width,aa->height,0,0,aa->width,aa->height,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT,read);
    }
    glPopAttrib();
    AlphaAA_Diagnostic("SSAA",samples);
    return 1;
}
#else
static int DeviceAlphaAA_Draw(DeviceImpl *dev, UINT32 first, UINT32 last, UINT32 count, const byte *indices)
{ (void)dev; (void)first; (void)last; (void)count; (void)indices; return 0; }
#endif
