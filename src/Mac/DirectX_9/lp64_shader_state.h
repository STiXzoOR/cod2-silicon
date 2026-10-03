#if COD2_APPLE_SDK
static GLenum MacShader_BlendEquation(DWORD operation)
{
    switch (operation) {
    case 2: return GL_FUNC_SUBTRACT;
    case 3: return GL_FUNC_REVERSE_SUBTRACT;
    case 4: return GL_MIN;
    case 5: return GL_MAX;
    default: return GL_FUNC_ADD;
    }
}

static void MacShader_ApplyRasterEquations(DWORD cull, DWORD rgbOperation, DWORD alphaOperation)
{
    if (cull == 2 || cull == 3) {
        glEnable(GL_CULL_FACE);
        glFrontFace(cull == 2 ? GL_CCW : GL_CW);
    } else {
        glDisable(GL_CULL_FACE);
    }
    glBlendEquationSeparate(MacShader_BlendEquation(rgbOperation), MacShader_BlendEquation(alphaOperation));
}
#endif
