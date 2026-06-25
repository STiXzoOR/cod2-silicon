#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    GLuint *begin;
    GLuint *end;
    GLuint *capacity;
} CFenceUnusedIdsVector;

typedef struct {
    GLuint id;
    const void *start;
    UINT32 sizeInBytes;
    UINT32 frameCount;
} CFenceImpl;

CFenceUnusedIdsVector CFence_sUnusedFenceIDs = { NULL, NULL, NULL };

void CFence_CFence(const CFence *_this, const void *Start, UINT32 SizeInBytes, UINT32 FrameCount);
void CFence_Shutdown(void);
GLuint *ZNSt6vectorImSaImEE5eraseEN9__gnu_cxx17__normal_iteratorIPmS1_EES5_(CFenceUnusedIdsVector *vec, GLuint *first, GLuint *last);

void CFence_CFence(const CFence *_this, const void *Start, UINT32 SizeInBytes, UINT32 FrameCount)
{
    CFenceImpl *fence;

    fence = (CFenceImpl *)_this;
    fence->start = Start;
    fence->sizeInBytes = SizeInBytes;
    fence->frameCount = FrameCount;
    fence->id = 0;

}

void CFence_Shutdown(void)
{

    if (CFence_sUnusedFenceIDs.begin) {
        free(CFence_sUnusedFenceIDs.begin);
    }
    CFence_sUnusedFenceIDs.begin = NULL;
    CFence_sUnusedFenceIDs.end = NULL;
    CFence_sUnusedFenceIDs.capacity = NULL;
}

static void __static_initialization_and_destruction_0(int __initialize_p, int __priority)
{
    if (__priority != 0xffff) {
        return;
    }

    if (__initialize_p == 1) {
        CFence_sUnusedFenceIDs.begin = NULL;
        CFence_sUnusedFenceIDs.end = NULL;
        CFence_sUnusedFenceIDs.capacity = NULL;
        return;
    }

    if (__initialize_p == 0) {
        if (CFence_sUnusedFenceIDs.begin) {
            free(CFence_sUnusedFenceIDs.begin);
            CFence_sUnusedFenceIDs.begin = NULL;
        }
    }
}

void GLOBAL__D__ZN6CFence15sUnusedFenceIDsE(void)
{
    __static_initialization_and_destruction_0(0, 0xffff);
}

void GLOBAL__I__ZN6CFence15sUnusedFenceIDsE(void)
{
    __static_initialization_and_destruction_0(1, 0xffff);
}

GLuint *ZNSt6vectorImSaImEE5eraseEN9__gnu_cxx17__normal_iteratorIPmS1_EES5_(CFenceUnusedIdsVector *vec, GLuint *first, GLuint *last)
{
    memmove(first, last, (size_t)((char *)vec->end - (char *)last));
    vec->end -= (last - first);
    return first;
}
