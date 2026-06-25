#include "common_types.h"
#include "imports.h"

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
