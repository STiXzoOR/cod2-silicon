#include "common_types.h"
#include <assert.h>
#include <stdlib.h>

/* Link the actual generated BSS object, rather than supplying aligned fixtures. */
extern unsigned char scrMemTreeGlob[], scrStringGlob[], scrVarGlob[], scrVmGlob[];
extern scrVarPub_t scrVarPub;
void *const imp_scrVarPub = &scrVarPub;
char g_EndPos; /* The separate generated data object is outside this storage check. */

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Scr_TerminalError(const char *msg) { (void)msg; abort(); }
void MT_Init(void);
unsigned int *MT_Alloc(int numBytes, int type);
void MT_Free(void *p, int numBytes);

/* STABS i386: nodes 0..0x80000, left/num/log tables each 256 bytes,
   heads 34 bytes, counters at 0x80324/0x80328. These fields contain no pointers. */
_Static_assert(sizeof(MemoryNode) == 8, "fixed-width arena nodes");
_Static_assert(offsetof(scrMemTreeGlob_t, head) == 0x80300, "arena heads");
_Static_assert(offsetof(scrMemTreeGlob_t, totalAlloc) == 0x80324, "arena counters");
_Static_assert(sizeof(scrMemTreeGlob_t) == 0x8032c, "arena struct size");
_Static_assert(sizeof(VariableValueInternal) == 16, "fixed variable slot stride");
_Static_assert(offsetof(VariableValueInternal, w) == 8, "fixed variable type slot");
/* STABS string globals: hashTable 65536 bytes, byte flag, padding, pointer at
   65540. LP64 keeps field order and aligns that pointer to 65544. */
_Static_assert(offsetof(scrStringGlob_t, inited) == 65536, "string flag");
_Static_assert(offsetof(scrStringGlob_t, nextFreeEntry) == 65544, "native string pointer");

int main(void)
{
    char program[64] __attribute__((aligned(8))) = {0};
    VariableUnion value;
    void *allocation;

    assert((uintptr_t)scrMemTreeGlob % _Alignof(scrMemTreeGlob_t) == 0);
    assert((uintptr_t)scrStringGlob % _Alignof(scrStringGlob_t) == 0);
    assert((uintptr_t)scrVarGlob % _Alignof(VariableValueInternal) == 0);
    assert((uintptr_t)scrVmGlob % _Alignof(scrVmGlob_t) == 0);
    MT_Init();
    allocation = MT_Alloc(65535, 1);
    assert((uintptr_t)allocation > UINT32_MAX);
    assert(SCR_ARENA_ENC(allocation) + 65535u <= 0x80000u);
    value.stackValue = SCR_STACK_ENC(allocation);
    assert(SCR_STACK_PTR(value) == allocation);
    value.vectorValue = SCR_VEC_ENC(allocation);
    assert(!(value.vectorValue & SCR_VEC_TAG_PROG));
    assert(SCR_VEC_PTR(value) == allocation);
    scrVarPub.programBuffer = program;
    value.vectorValue = SCR_VEC_ENC(program + 8);
    assert(value.vectorValue == (SCR_VEC_TAG_PROG | 8u));
    assert(SCR_VEC_PTR(value) == (const float *)(program + 8));
    MT_Free(allocation, 65535);
    puts("generated script storage alignment and arena offsets passed");
    return 0;
}
