#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"
#include "Mac/DirectX_9/lp64_gl_state.h"

void glActiveTextureARB(GLenum unit) { (void)unit; }
void glBlendFunc(GLenum src, GLenum dst) { (void)src; (void)dst; }
void glBlendFuncSeparateEXT(GLenum a, GLenum b, GLenum c, GLenum d)
{ (void)a; (void)b; (void)c; (void)d; }
void glGenVertexArraysAPPLE(GLsizei count, GLuint *ids)
{ while (count--) *ids++ = 123; }
void *__Znwm(size_t size) { return calloc(1, size); }
void __ZdlPv(void *ptr) { free(ptr); }
void __ZNSt15_List_node_base4hookEPS_(void *node, void *position)
{
    COpenGLBindingList *n = node, *p = position, *prev = p->prev;
    n->next = p; n->prev = prev; prev->next = n; p->prev = n;
}
void CBaseVA_CBaseVA(const CBaseVA *v) { memset((void *)v, 0, sizeof(CBaseVAImpl)); }
void CBaseVA_Reset(const CBaseVA *v) { CBaseVA_CBaseVA(v); }
void *vtables[16];
void *imp___ZTV11CColorArray = vtables;
void *imp___ZTV20CSecondaryColorArray = vtables;
void *imp___ZTV12CNormalArray = vtables;
void *imp___ZTV12CVertexArray = vtables;
void *imp___ZTV14CTexCoordArray = vtables;
void *imp___ZTV7CBaseVA = vtables;
void *imp___ZTV10COpenGLVAO = vtables;

#include "Mac/DirectX_9/COpenGL.c"
#include "Mac/DirectX_9/COpenGLVAO.c"
#include "Mac/DirectX_9/CVAOPacket.c"
fnptr_t vtbl_CVAOPacket[3];

/* STABS: CBaseVA 24 (stream at 20), CTexUnit 304 (combiner at112,
 * texcoord array at260). Pointers widen; all GL scalars remain four bytes. */
_Static_assert(sizeof(CBaseVAImpl) == 32, "native vertex-array state");
_Static_assert(offsetof(CBaseVAImpl, mpStream) == 24, "native stream pointer");
_Static_assert(offsetof(CTexUnitNative, mTexID) == 8, "texture pointers");
_Static_assert(offsetof(CTexUnitNative, mCombinerColorOp) == 128, "three widened pointers");
_Static_assert(offsetof(CTexUnitNative, mTexCoordArray) == 280, "aligned array state");
_Static_assert(sizeof(CTexUnitNative) == 336, "native texture unit");
_Static_assert(offsetof(CVAOPacketKeyValue, second) == 8, "map value alignment");
_Static_assert(offsetof(CVAOPacketRbTreeNode, second) == 40, "map node value");

int main(void)
{
    COpenGLNative state = {0};
    COpenGLVAOImpl vao = {0};
    COpenGLBindingList *head = COpenGL_GetVAOBindingList((COpenGL *)&state);
    assert(head->next == head && head->prev == head);
    assert(COpenGL_TexUnitBase((COpenGL *)&state, 15) == (byte *)&state.texUnits[15]);
    COpenGL_SetActiveTexUnit((COpenGL *)&state, 9);
    assert(state.activeTexUnit == 9);
    COpenGL_SetBlendEXT((COpenGL *)&state, 1, GL_ONE, GL_ZERO, GL_SRC_ALPHA, GL_DST_ALPHA);
    assert(state.blendSrcRGB == GL_ONE && state.blendDstAlpha == GL_DST_ALPHA);
    COpenGLVAO_COpenGLVAO((COpenGLVAO *)&vao);
    assert(vao.mTexCoordArrays[7].vtable == vtables + 2);
    CVAOPacket_CVAOPacket((CVAOPacket *)s_nativeGenericPacket);
    assert(s_nativeGenericPacket[0].mGenericArrays[15].mpStream == NULL);
    COpenGLVAO_CreateNewBinding((COpenGLVAO *)&vao);
    assert(*vao.mpVAOID == 123);
    COpenGLBindingList *globalHead = COpenGL_GetVAOBindingList(imp__ZN7COpenGL7sOpenGLE);
    COpenGLVAOBindingNode *node = globalHead->next;
    assert(node->vaoId == vao.mpVAOID && node->next == (void *)globalHead);
    free(vao.mpVAOID); free(node);
    puts("renderer texture-unit, VAO and map-node layouts: passed");
}
