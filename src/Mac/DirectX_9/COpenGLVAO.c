#include "common_types.h"
#include "imports.h"

typedef struct {
    void **vtable;
    bool mEnabled;
    bool mNeedsValidation;
    unsigned short padding;
    GLint mVSize;
    GLenum mVType;
    GLsizei mStride;
    const void *mpStream;
} CBaseVAImpl;

typedef struct {
    void **vtable;
    GLuint *mpVAOID;
    UINT32 mCode;
    UINT32 mReserved0C;
    CBaseVAImpl mColorArray;
    CBaseVAImpl mSecondaryColorArray;
    CBaseVAImpl mNormalArray;
    CBaseVAImpl mVertexArray;
    CBaseVAImpl mTexCoordArrays[8];
    VertexProgramStreamState mGenericArrays[16];
} COpenGLVAOImpl;

typedef struct {
    UINT16 *current;
    UINT16 *first;
    UINT16 *last;
    UINT16 **node;
} UInt16DequeIterator;

typedef struct {
    UINT16 **map;
    UINT32 mapSize;
    UInt16DequeIterator start;
    UInt16DequeIterator finish;
} UInt16Deque;

typedef struct TriangleNode {
    struct TriangleNode *next;
    struct TriangleNode *prev;
    Tuple triangle;
} TriangleNode;

typedef struct COpenGLVAOBindingNode {
    struct COpenGLVAOBindingNode *next;
    struct COpenGLVAOBindingNode *prev;
    GLuint *vaoId;
} COpenGLVAOBindingNode;

extern unsigned char COpenGL_sOpenGLE[];

void *__Znwm(size_t size);
void __ZdlPv(void *ptr);
void __ZNSt15_List_node_base4hookEPS_(void *node, void *position);
void __ZNSt15_List_node_base6unhookEv(void *node);

UINT32 COpenGLVAO_GetCode(const COpenGLVAO *_this);
bool ZNK10COpenGLVAOeqERKS_(const COpenGLVAO *_this, const COpenGLVAO *v);
void COpenGLVAO_CreateNewBinding(const COpenGLVAO *_this);
UINT32 CalculateScore(const UInt16Deque *FIFO, const Tuple *t);
Tuple ChooseAndRemoveBestTriangle(const UInt16Deque *FIFO, TriangleNode *Triangles);
void COpenGLVAO_COpenGLVAO(const COpenGLVAO *_this);
void ZN10COpenGLVAOD2Ev(const COpenGLVAO *_this);
void ZN10COpenGLVAOD1Ev(const COpenGLVAO *_this);
void ZN10COpenGLVAOD0Ev(const COpenGLVAO *_this);
UINT32 Score(const UINT16 *pSrc, UINT32 NumIndices);
void Optimize(UINT16 *pDst, const UINT16 *pSrc, UINT32 NumIndices);
void ZNSt11_Deque_baseItSaItEED2Ev(void);
void ZNSt5dequeItSaItEE17_M_reallocate_mapEmb(void);
void ZNSt5dequeItSaItEE16_M_push_back_auxERKt(void);
void ZNSt5dequeItSaItEE5clearEv(void);
void ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm(void);

extern void *imp___ZTV11CColorArray;
extern void *imp___ZTV20CSecondaryColorArray;
extern void *imp___ZTV12CNormalArray;
extern void *imp___ZTV12CVertexArray;
extern void *imp___ZTV14CTexCoordArray;
extern void *imp___ZTV7CBaseVA;
extern void *imp___ZTV10COpenGLVAO;

void CBaseVA_CBaseVA(const CBaseVA *_this);

static void **getVtblPtr(void *vtbl_ptr)
{
    return (void **)((unsigned char *)vtbl_ptr + 8);
}

UINT32 COpenGLVAO_GetCode(const COpenGLVAO *_this)
{
    return ((const COpenGLVAOImpl *)_this)->mCode;
}

bool ZNK10COpenGLVAOeqERKS_(const COpenGLVAO *_this, const COpenGLVAO *v)
{
    return COpenGLVAO_GetCode(_this) == COpenGLVAO_GetCode(v);
}

void COpenGLVAO_CreateNewBinding(const COpenGLVAO *_this)
{
    COpenGLVAOImpl *vao;
    GLuint *vaoId;
    COpenGLVAOBindingNode *bindingNode;
    void *bindingList;

    vao = (COpenGLVAOImpl *)_this;
    vaoId = (GLuint *)__Znwm(sizeof(*vaoId));
    glGenVertexArraysAPPLE(1, vaoId);

    bindingNode = (COpenGLVAOBindingNode *)__Znwm(sizeof(*bindingNode));
    bindingNode->vaoId = vaoId;
    bindingList = (void *)(COpenGL_sOpenGLE + 0x674);
    __ZNSt15_List_node_base4hookEPS_(bindingNode, bindingList);

    vao->mpVAOID = vaoId;
}

UINT32 CalculateScore(const UInt16Deque *FIFO, const Tuple *t)
{
    const UINT16 *current;
    const UINT16 *blockEnd;
    const UINT16 *finish;
    UINT16 **node;
    unsigned int matches0;
    unsigned int matches1;
    unsigned int matches2;

    current = FIFO->start.current;
    blockEnd = FIFO->start.last;
    node = FIFO->start.node;
    finish = FIFO->finish.current;
    matches0 = 0;
    matches1 = 0;
    matches2 = 0;

    while (current != finish) {
        while (current != blockEnd && current != finish) {
            UINT16 vertex;

            vertex = *current++;
            if (vertex == t->v[0]) {
                matches0 = 1;
            }
            if (vertex == t->v[1]) {
                matches1 = 1;
            }
            if (vertex == t->v[2]) {
                matches2 = 1;
            }
        }

        if (current != finish) {
            ++node;
            current = *node;
            blockEnd = current + 0x100;
        }
    }

    return matches0 + matches1 + matches2;
}

Tuple ChooseAndRemoveBestTriangle(const UInt16Deque *FIFO, TriangleNode *Triangles)
{
    TriangleNode *bestTriangleNode;
    TriangleNode *node;
    Tuple bestTriangle;

    bestTriangleNode = Triangles;
    node = Triangles->next;
    while (node != Triangles) {
        node->triangle.Score = CalculateScore(FIFO, &node->triangle);
        if (bestTriangleNode == Triangles || node->triangle.Score <= bestTriangleNode->triangle.Score) {
            bestTriangleNode = node;
        }
        node = node->next;
    }

    memmove(&bestTriangle, &bestTriangleNode->triangle, sizeof(bestTriangle));
    __ZNSt15_List_node_base6unhookEv(bestTriangleNode);
    __ZdlPv(bestTriangleNode);
    return bestTriangle;
}

void COpenGLVAO_COpenGLVAO(const COpenGLVAO *_this)
{
    COpenGLVAOImpl *vao = (COpenGLVAOImpl *)_this;
    int i;

    vao->vtable = getVtblPtr(imp___ZTV10COpenGLVAO);
    vao->mpVAOID = NULL;
    vao->mCode = 0;
    vao->mReserved0C = 0;

    CBaseVA_CBaseVA((const CBaseVA *)&vao->mColorArray);
    vao->mColorArray.vtable = getVtblPtr(imp___ZTV11CColorArray);

    CBaseVA_CBaseVA((const CBaseVA *)&vao->mSecondaryColorArray);
    vao->mSecondaryColorArray.vtable = getVtblPtr(imp___ZTV20CSecondaryColorArray);

    CBaseVA_CBaseVA((const CBaseVA *)&vao->mNormalArray);
    vao->mNormalArray.vtable = getVtblPtr(imp___ZTV12CNormalArray);

    CBaseVA_CBaseVA((const CBaseVA *)&vao->mVertexArray);
    vao->mVertexArray.vtable = getVtblPtr(imp___ZTV12CVertexArray);

    for (i = 0; i < 8; i++) {
        CBaseVA_CBaseVA((const CBaseVA *)&vao->mTexCoordArrays[i]);
        vao->mTexCoordArrays[i].vtable = getVtblPtr(imp___ZTV14CTexCoordArray);
    }

    for (i = 0; i < 16; i++) {
        vao->mGenericArrays[i].mNeedsValidation = 1;
        vao->mGenericArrays[i].mEnabled = 0;
        vao->mGenericArrays[i].mVSize = 4;
        vao->mGenericArrays[i].mVType = 0x1406;
        vao->mGenericArrays[i].mNormalized = 0;
        vao->mGenericArrays[i].mStride = 0;
        vao->mGenericArrays[i].mpStream = NULL;
    }
}

void ZN10COpenGLVAOD2Ev(const COpenGLVAO *_this)
{
    (void)_this;

}

void ZN10COpenGLVAOD1Ev(const COpenGLVAO *_this)
{

    ZN10COpenGLVAOD2Ev(_this);
}

void ZN10COpenGLVAOD0Ev(const COpenGLVAO *_this)
{

    ZN10COpenGLVAOD2Ev(_this);
    __ZdlPv((void *)_this);
}

UINT32 Score(const UINT16 *pSrc, UINT32 NumIndices)
{

    UINT16 fifo[16];
    UINT32 fifoSize = 0;
    UINT32 fifoStart = 0;
    UINT32 Count = 0;
    UINT32 i, j;
    int found;

    for (i = 0; i < NumIndices; i++) {
        UINT16 vertex = pSrc[i];

        found = 0;
        for (j = 0; j < fifoSize; j++) {
            UINT32 idx = (fifoStart + j) % 16;
            if (fifo[idx] == vertex) {
                found = 1;
                break;
            }
        }

        Count += found;

        if (fifoSize < 10) {
            fifo[(fifoStart + fifoSize) % 16] = vertex;
            fifoSize++;
        } else {

            fifo[(fifoStart + fifoSize) % 16] = vertex;
            fifoStart = (fifoStart + 1) % 16;
        }
    }

    return Count;
}

void Optimize(UINT16 *pDst, const UINT16 *pSrc, UINT32 NumIndices)
{

    if (NumIndices > 0) {
        memmove(pDst, pSrc, NumIndices * sizeof(UINT16));
    }
}

void ZNSt11_Deque_baseItSaItEED2Ev(void)
{

    return;
}

void ZNSt5dequeItSaItEE17_M_reallocate_mapEmb(void)
{

    return;
}

void ZNSt5dequeItSaItEE16_M_push_back_auxERKt(void)
{

    return;
}

void ZNSt5dequeItSaItEE5clearEv(void)
{

    return;
}

void ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm(void)
{

    return;
}
