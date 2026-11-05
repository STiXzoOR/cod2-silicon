#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"

#if defined(COD2_X64)
#include "lp64_gl_state.h"
#else
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
#endif

#if defined(COD2_X64)
typedef COpenGLVAONative COpenGLVAOImpl;
#define mCode mpVAStart
#define mReserved0C mSize
#else
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
#if defined(COD2_X64)
    VertexProgramStreamStateNative mGenericArrays[16];
#else
    VertexProgramStreamState mGenericArrays[16];
#endif
} COpenGLVAOImpl;
#endif

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

#if defined(COD2_X64)
extern void *imp__ZN7COpenGL7sOpenGLE;
#define COpenGL_sOpenGLE ((unsigned char *)imp__ZN7COpenGL7sOpenGLE)
#else
extern unsigned char COpenGL_sOpenGLE[];
#endif

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
#if defined(COD2_X64)
void ZNSt11_Deque_baseItSaItEED2Ev(UInt16Deque *deque);
void ZNSt5dequeItSaItEE17_M_reallocate_mapEmb(UInt16Deque *deque, UINT32 count, int atFront);
void ZNSt5dequeItSaItEE16_M_push_back_auxERKt(UInt16Deque *deque, const UINT16 *value);
void ZNSt5dequeItSaItEE5clearEv(UInt16Deque *deque);
void ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm(UInt16Deque *deque, UINT32 count);
#else
void ZNSt11_Deque_baseItSaItEED2Ev(void);
void ZNSt5dequeItSaItEE17_M_reallocate_mapEmb(void);
void ZNSt5dequeItSaItEE16_M_push_back_auxERKt(void);
void ZNSt5dequeItSaItEE5clearEv(void);
void ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm(void);
#endif

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
#if defined(COD2_X64)
    return (void **)((unsigned char *)vtbl_ptr + 2 * sizeof(void *));
#else
    return (void **)((unsigned char *)vtbl_ptr + 8);
#endif
}

UINT32 COpenGLVAO_GetCode(const COpenGLVAO *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    UINT32 macResult = { 0 };
    if (MacShim_Dispatch("COpenGLVAO_GetCode", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return (UINT32)(uintptr_t)((const COpenGLVAOImpl *)_this)->mpVAStart;
#else
    return ((const COpenGLVAOImpl *)_this)->mCode;
#endif
}

bool ZNK10COpenGLVAOeqERKS_(const COpenGLVAO *_this, const COpenGLVAO *v)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &v };
    bool macResult = { 0 };
    if (MacShim_Dispatch("ZNK10COpenGLVAOeqERKS_", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    return ((const COpenGLVAOImpl *)_this)->mpVAStart == ((const COpenGLVAOImpl *)v)->mpVAStart;
#else
    return COpenGLVAO_GetCode(_this) == COpenGLVAO_GetCode(v);
#endif
}

void COpenGLVAO_CreateNewBinding(const COpenGLVAO *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLVAO_CreateNewBinding", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLVAOImpl *vao;
    GLuint *vaoId;
    COpenGLVAOBindingNode *bindingNode;
    void *bindingList;

    vao = (COpenGLVAOImpl *)_this;
    vaoId = (GLuint *)__Znwm(sizeof(*vaoId));
    glGenVertexArraysAPPLE(1, vaoId);

    bindingNode = (COpenGLVAOBindingNode *)__Znwm(sizeof(*bindingNode));
    bindingNode->vaoId = vaoId;
#if defined(COD2_X64)
    bindingList = COpenGL_GetVAOBindingList((const COpenGL *)COpenGL_sOpenGLE);
#else
    bindingList = (void *)(COpenGL_sOpenGLE + 0x674);
#endif
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
#if defined(COD2_X64)
        if (bestTriangleNode == Triangles || node->triangle.Score > bestTriangleNode->triangle.Score) {
#else
        if (bestTriangleNode == Triangles || node->triangle.Score <= bestTriangleNode->triangle.Score) {
#endif
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
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLVAO_COpenGLVAO", _this, macArguments, NULL, 0))
        return;
#endif

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
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN10COpenGLVAOD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    COpenGLVAOImpl *vao = (COpenGLVAOImpl *)_this;
    int i;

    vao->vtable = getVtblPtr(imp___ZTV10COpenGLVAO);
    for (i = 7; i >= 0; --i) {
        CBaseVAImpl *array = &vao->mTexCoordArrays[i];
        ((void (*)(const CBaseVA *))array->vtable[0])((const CBaseVA *)array);
    }
    vao->mVertexArray.vtable = getVtblPtr(imp___ZTV7CBaseVA);
    vao->mNormalArray.vtable = getVtblPtr(imp___ZTV7CBaseVA);
    vao->mSecondaryColorArray.vtable = getVtblPtr(imp___ZTV7CBaseVA);
    vao->mColorArray.vtable = getVtblPtr(imp___ZTV7CBaseVA);
#else
    (void)_this;

#endif
}

void ZN10COpenGLVAOD1Ev(const COpenGLVAO *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN10COpenGLVAOD1Ev", _this, macArguments, NULL, 0))
        return;
#endif


    ZN10COpenGLVAOD2Ev(_this);
}

void ZN10COpenGLVAOD0Ev(const COpenGLVAO *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN10COpenGLVAOD0Ev", _this, macArguments, NULL, 0))
        return;
#endif


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

#if defined(COD2_X64)
static void UInt16Deque_SetNode(UInt16DequeIterator *iterator, UINT16 **node)
{
    iterator->node = node;
    iterator->first = *node;
    iterator->last = *node + 256;
}

static void UInt16Deque_PushBack(UInt16Deque *deque, UINT16 value)
{
    if (deque->finish.current == deque->finish.last - 1) {
        ZNSt5dequeItSaItEE16_M_push_back_auxERKt(deque, &value);
    } else {
        *deque->finish.current++ = value;
    }
}

static void UInt16Deque_PopFront(UInt16Deque *deque)
{
    if (deque->start.current == deque->start.last - 1) {
        __ZdlPv(deque->start.first);
        UInt16Deque_SetNode(&deque->start, deque->start.node + 1);
        deque->start.current = deque->start.first;
    } else {
        ++deque->start.current;
    }
}

static size_t UInt16Deque_Size(const UInt16Deque *deque)
{
    return 256 * (deque->finish.node - deque->start.node - 1) +
        (deque->finish.current - deque->finish.first) +
        (deque->start.last - deque->start.current);
}

void Optimize(UINT16 *pDst, const UINT16 *pSrc, UINT32 NumIndices)
{
    TriangleNode triangles, *node;
    UInt16Deque fifo = { 0 };
    Tuple triangle;
    UINT32 i, j;

    /* Mac requires at least one complete triangle. Retain defined native
     * handling for empty or malformed input instead of reading past it. */
    if (NumIndices == 0 || NumIndices % 3 != 0) {
        if (NumIndices)
            memmove(pDst, pSrc, (size_t)NumIndices * sizeof(*pDst));
        return;
    }
    triangles.next = triangles.prev = &triangles;
    for (i = 0; i < NumIndices; i += 3) {
        node = __Znwm(sizeof(*node));
        node->triangle = (Tuple){ { pSrc[i], pSrc[i + 1], pSrc[i + 2] }, 0 };
        __ZNSt15_List_node_base4hookEPS_(node, &triangles);
    }
    ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm(&fifo, 0);
    node = triangles.next;
    triangle = node->triangle;
    __ZNSt15_List_node_base6unhookEv(node);
    __ZdlPv(node);
    for (j = 0; j < 3; ++j) {
        *pDst++ = triangle.v[j];
        UInt16Deque_PushBack(&fifo, triangle.v[j]);
    }
    /* 0x112dd8 copies every remaining tuple over the local tuple. Keep this
     * walk even though the selection below overwrites the local tuple. */
    for (node = triangles.next; node != &triangles; node = node->next)
        triangle = node->triangle;
    for (i = 1; i < NumIndices / 3; ++i) {
        triangle = ChooseAndRemoveBestTriangle(&fifo, &triangles);
        for (j = 0; j < 3; ++j) {
            *pDst++ = triangle.v[j];
            UInt16Deque_PushBack(&fifo, triangle.v[j]);
            if (UInt16Deque_Size(&fifo) > 10)
                UInt16Deque_PopFront(&fifo);
        }
    }
    ZNSt5dequeItSaItEE5clearEv(&fifo);
    ZNSt11_Deque_baseItSaItEED2Ev(&fifo);
}

void ZNSt11_Deque_baseItSaItEED2Ev(UInt16Deque *deque)
{
    if (deque->map) {
        UINT16 **node;
        for (node = deque->start.node; node <= deque->finish.node; ++node)
            __ZdlPv(*node);
        __ZdlPv(deque->map);
    }
}

void ZNSt5dequeItSaItEE17_M_reallocate_mapEmb(UInt16Deque *deque, UINT32 count, int atFront)
{
    UINT32 oldCount = (UINT32)(deque->finish.node - deque->start.node + 1);
    UINT32 newCount = oldCount + count;
    UINT16 **start;
    if (deque->mapSize > 2 * newCount) {
        start = deque->map + (deque->mapSize - newCount) / 2 + (atFront ? count : 0);
        memmove(start, deque->start.node, oldCount * sizeof(*start));
    } else {
        UINT32 mapSize = deque->mapSize +
            (deque->mapSize > count ? deque->mapSize : count) + 2;
        UINT16 **map = __Znwm((size_t)mapSize * sizeof(*map));
        start = map + (mapSize - newCount) / 2 + (atFront ? count : 0);
        memmove(start, deque->start.node, oldCount * sizeof(*start));
        __ZdlPv(deque->map);
        deque->map = map;
        deque->mapSize = mapSize;
    }
    UInt16Deque_SetNode(&deque->start, start);
    UInt16Deque_SetNode(&deque->finish, start + oldCount - 1);
}

void ZNSt5dequeItSaItEE16_M_push_back_auxERKt(UInt16Deque *deque, const UINT16 *value)
{
    UINT16 copy = *value;
    if (deque->mapSize - (deque->finish.node - deque->map) <= 1)
        ZNSt5dequeItSaItEE17_M_reallocate_mapEmb(deque, 1, 0);
    deque->finish.node[1] = __Znwm(256 * sizeof(UINT16));
    *deque->finish.current = copy;
    UInt16Deque_SetNode(&deque->finish, deque->finish.node + 1);
    deque->finish.current = deque->finish.first;
}

void ZNSt5dequeItSaItEE5clearEv(UInt16Deque *deque)
{
    UINT16 **node;
    for (node = deque->start.node + 1; node <= deque->finish.node; ++node)
        __ZdlPv(*node);
    deque->finish = deque->start;
}

void ZNSt11_Deque_baseItSaItEE17_M_initialize_mapEm(UInt16Deque *deque, UINT32 count)
{
    UINT32 nodes = count / 256 + 1;
    UINT16 **start, **finish, **node;
    deque->mapSize = nodes + 2 > 8 ? nodes + 2 : 8;
    deque->map = __Znwm((size_t)deque->mapSize * sizeof(*deque->map));
    start = deque->map + (deque->mapSize - nodes) / 2;
    finish = start + nodes;
    for (node = start; node != finish; ++node)
        *node = __Znwm(256 * sizeof(UINT16));
    UInt16Deque_SetNode(&deque->start, start);
    UInt16Deque_SetNode(&deque->finish, finish - 1);
    deque->start.current = deque->start.first;
    deque->finish.current = deque->finish.first + count % 256;
}
#else
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

#endif
