#include "common_types.h"
#include "imports.h"
#if defined(COD2_X64)
#include <stdlib.h>
#endif

#if defined(COD2_X64)
unsigned int g_NumShadowCookies = 0;
unsigned char *g_ShadowCookieMemory;
static unsigned char *sShadowCookieMemoryEnd;
static unsigned char *sShadowCookieMemoryCapacity;

extern int MacDisplay_GetCardType(void);
extern UINT32 MacOpenGLUtils_GetPCPixelShaderVersion(void);
void __ZdlPv(void *ptr);

static void D3DXTex_DisablePrograms(void)
{
    glDisable(0x8620);
    if (MacDisplay_GetCardType() == 2) {
        glDisable(0x86de);
        glDisable(0x8522);
    } else if (MacDisplay_GetCardType() == 1) {
        glDisable(0x8200);
    }
    if (MacOpenGLUtils_GetPCPixelShaderVersion() > 0xffff01ffu) {
        glDisable(0x8804);
    }
}

static void D3DXTex_SetupView(double right, double bottom, double top)
{
    glMatrixMode(0x1700);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(0x1701);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, right, bottom, top);
    glDisable(0x0b50);
    glDisable(0x0be2);
    glDisable(0x0bc0);
    glDisable(0x0b71);
    glDisable(0x0bd0);
    glColorMask(1, 1, 1, 1);
#if COD2_APPLE_SDK
    extern void MacGL_DrawBuffer(unsigned int);
    MacGL_DrawBuffer(0x0405);
#else
    glDrawBuffer(0x0405);
#endif
    glDepthMask(1);
    glDisable(0x0b44);
    glDisable(0x0b60);
}

static void D3DXTex_SetupUnit(int unit, int rectangle)
{
    if (unit == 0) {
        glEnable(0x0de1);
        if (rectangle) glEnable(0x84f5);
        else glDisable(0x84f5);
    } else {
        glDisable(0x0de1);
        glDisable(0x84f5);
    }
    glDisable(0x806f);
    glDisable(0x8513);
    if (unit == 0) {
        glTexEnvi(0x2300, 0x8571, 0x1e01);
        glTexEnvi(0x2300, 0x8572, 0x1e01);
        glTexEnvi(0x2300, 0x8580, 0x1702);
        glTexEnvi(0x2300, 0x8590, 0x0300);
        glTexEnvi(0x2300, 0x8588, 0x1702);
        glTexEnvi(0x2300, 0x8598, 0x0302);
        glTexEnvf(0x2300, 0x8573, 1.0f);
        glTexEnvf(0x2300, 0x0d1c, 1.0f);
    }
}

static void D3DXTex_DrawQuad(float x, float y, float extent, float textureWidth, float textureHeight)
{
    glBegin(7);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(x, y, 0.0f);
    glTexCoord2f(textureWidth, 0.0f);
    glVertex3f(x + extent, y, 0.0f);
    glTexCoord2f(textureWidth, textureHeight);
    glVertex3f(x + extent, y + extent, 0.0f);
    glTexCoord2f(0.0f, textureHeight);
    glVertex3f(x, y + extent, 0.0f);
    glEnd();
}

static void D3DXTex_RestoreMatrices(int textureUnits, const float (*textureMatrices)[16])
{
    glMatrixMode(0x1702);
    for (int unit = 0; unit < textureUnits; ++unit) {
        glActiveTextureARB(0x84c0 + unit);
        glLoadMatrixf(textureMatrices[unit]);
    }
    glMatrixMode(0x1701);
    glPopMatrix();
    glMatrixMode(0x1700);
    glPopMatrix();
}

void ViewSingleTexture(GLuint TextureID)
{
    GLint textureUnits;
    glPushAttrib(0x000fffff);
    glPushClientAttrib(0xffffffffu);
    D3DXTex_DisablePrograms();
    D3DXTex_SetupView(1.5, 0.0, 1.5);
    glGetIntegerv(0x84e2, &textureUnits);
    /* Mac reserves eight matrices; native storage follows the live GL count. */
    float textureMatrices[textureUnits > 0 ? textureUnits : 1][16];
    glMatrixMode(0x1702);
    for (int unit = textureUnits - 1; unit >= 0; --unit) {
        glActiveTextureARB(0x84c0 + unit);
        glGetFloatv(0x0ba8, textureMatrices[unit]);
        glLoadIdentity();
    }
    glDisable(0x0b90);
    for (int unit = 0; unit < textureUnits; ++unit) {
        glActiveTextureARB(0x84c0 + unit);
        glDisable(0x0c60);
        glDisable(0x0c61);
        glDisable(0x0c62);
        glDisable(0x0c63);
        glBindTexture(0x0de1, TextureID);
        D3DXTex_SetupUnit(unit, 0);
    }
    D3DXTex_DrawQuad(0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    D3DXTex_RestoreMatrices(textureUnits, textureMatrices);
    glPopClientAttrib();
    glPopAttrib();
}

void ShowShadowCookies(void)
{
    GLint textureUnits;
    glPushAttrib(0x000fffff);
    D3DXTex_DisablePrograms();
    D3DXTex_SetupView(1.0, 1.0, 0.0);
    glGetIntegerv(0x84e2, &textureUnits);
    float textureMatrices[textureUnits > 0 ? textureUnits : 1][16];
    glMatrixMode(0x1702);
    for (int unit = textureUnits - 1; unit >= 0; --unit) {
        glActiveTextureARB(0x84c0 + unit);
        glGetFloatv(0x0ba8, textureMatrices[unit]);
        glLoadIdentity();
    }
    glDisable(0x0b90);
    for (unsigned int cookie = 0; cookie < g_NumShadowCookies; ++cookie) {
        for (int unit = 0; unit < textureUnits; ++unit) {
            glActiveTextureARB(0x84c0 + unit);
            glDisable(0x0c60);
            glDisable(0x0c61);
            glDisable(0x0c62);
            glDisable(0x0c63);
            glTexImage2D(0x84f5, 0, 0x1908, 1024, 768, 0, 0x1908, 0x1401,
                         g_ShadowCookieMemory + (size_t)cookie * 0x00c00000);
            D3DXTex_SetupUnit(unit, 1);
        }
        float x = (float)(cookie & 7u) * 0.125f + 0.0f;
        float y = 0.0f + (float)(cookie >> 3) * 0.125f;
        D3DXTex_DrawQuad(x, y, 0.09375f, 1024.0f, 768.0f);
    }
    D3DXTex_RestoreMatrices(textureUnits, textureMatrices);
    glPopAttrib();
    g_NumShadowCookies = 0;
}

static void D3DXTex_StaticInitialization(int initialize, int priority)
{
    if (priority != 0xffff) return;
    if (initialize == 1) {
        g_ShadowCookieMemory = NULL;
        sShadowCookieMemoryEnd = NULL;
        sShadowCookieMemoryCapacity = NULL;
    } else if (initialize == 0 && g_ShadowCookieMemory) {
        __ZdlPv(g_ShadowCookieMemory);
    }
}

static void D3DXTex_GlobalDestroy(void)
{
    D3DXTex_StaticInitialization(0, 0xffff);
}

#ifdef _MSC_VER
COD2_CONSTRUCTOR(_ctor_D3DXTex)
#else
__attribute__((constructor)) static void _ctor_D3DXTex(void)
#endif
{
    D3DXTex_StaticInitialization(1, 0xffff);
    atexit(D3DXTex_GlobalDestroy);
}
#else
int g_NumShadowCookies = 0;
int g_ShadowCookieMemory = 0;

void ViewSingleTexture(GLuint TextureID, IDirect3DDevice9 *pDevice, LPCVOID pSrcData, UINT SrcDataSize, IDirect3DCubeTexture9 **ppCubeTexture, IDirect3DDevice9 *pDevice_5, LPCVOID pSrcData_6, UINT SrcDataSize_7, IDirect3DTexture9 **ppTexture)
{
    (void)TextureID;
    (void)pDevice;
    (void)pSrcData;
    (void)SrcDataSize;
    (void)ppCubeTexture;
    (void)pDevice_5;
    (void)pSrcData_6;
    (void)SrcDataSize_7;
    (void)ppTexture;
}

void ShowShadowCookies(void)
{
    g_NumShadowCookies = 0;
}
#endif
