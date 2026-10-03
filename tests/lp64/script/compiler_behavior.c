/* Exercises real compiler routines; unused engine dependencies are dead-stripped. */
#include <assert.h>
#include <stdlib.h>
#define qsort compiler_legacy_qsort
#include "PC/script/scr_compiler.c"
#undef qsort
#include "compiler_allocations.h"

/* STABS i386 field order: filename 0, include 2, sourcePos 4, next 8, size 12. */
_Static_assert(offsetof(PrecacheEntry, filename) == 0, "precache filename order");
_Static_assert(offsetof(PrecacheEntry, include) == 2, "precache include order");
_Static_assert(offsetof(PrecacheEntry, sourcePos) == 4, "precache source position order");
_Static_assert(offsetof(PrecacheEntry, next) == 8, "precache native pointer alignment");
_Static_assert(sizeof(PrecacheEntry) == 16, "precache native pointer size");
_Static_assert(sizeof(sval_t) == sizeof(void *), "syntax nodes retain native pointers");
_Static_assert(sizeof(((stype_t *)0)->pos) == 4, "source positions stay scalar");

scrCompilePub_t scrCompilePub;
scrVarPub_t scrVarPub;
unsigned char scrCompileGlob[sizeof(scrCompileGlob_t)];
void *imp_scrCompileGlob = scrCompileGlob;
void *const imp_scrVarPub = &scrVarPub;
char g_EndPos;

static VariableUnion callsite;
static uintptr_t error_position;
static int freed_count;

void Com_Error(int code, const char *fmt, ...)
{
    (void)code;
    fprintf(stderr, "%s\n", fmt);
    abort();
}

void CompileError2(const char *codePos, const char *msg, ...)
{
    (void)msg;
    error_position = (uintptr_t)codePos;
}

unsigned int FindVariable(unsigned int parentId, unsigned int value)
{
    (void)parentId;
    return value ? 2 : 1;
}

unsigned long long Scr_EvalVariable(unsigned int id)
{
    (void)id;
    return 1;
}

VariableUnion *GetVariableValueAddress(unsigned int id)
{
    (void)id;
    return &callsite;
}

int GetVarType(unsigned int id)
{
    (void)id;
    return SCRCOMP_VAR_CODEPOS;
}

void Z_FreeInternal(void *ptr)
{
    ++freed_count;
    free(ptr);
}

static void check_function_identity(void)
{
    /* Distinct full addresses with equal low words must remain distinct. */
    intptr_t first = (intptr_t)UINT64_C(0x100001234);
    intptr_t second = (intptr_t)UINT64_C(0x200001234);
    assert(EmitFunctionTableIndex(first) == 0);
    assert(EmitFunctionTableIndex(second) == 1);
    assert(EmitFunctionTableIndex(first) == 0);
    assert(scrCompilePub.func_table_size == 2);
    assert(scrCompilePub.func_table[0] == first);
    assert(scrCompilePub.func_table[1] == second);
}

static void check_builtin_cache(void)
{
    intptr_t pointer = (intptr_t)UINT64_C(0x100001234);
    VariableValue cached = {0};
    assert(Scr_BuiltinPointerToIndex(0) == 0);
    assert(Scr_BuiltinPointerFromIndex(0) == 0);
    assert(scrCompilePub.func_table_size == 0);
    cached.u.intValue = Scr_BuiltinPointerToIndex(pointer);
    assert(cached.u.intValue == 1);
    assert(Scr_BuiltinPointerFromIndex(cached.u.intValue) == pointer);
    assert(Scr_BuiltinPointerToIndex(pointer) == 1);
    for (int i = 1; i < 1024; ++i) {
        cached.u.intValue = Scr_BuiltinPointerToIndex(pointer + i);
        assert(cached.u.intValue == i + 1);
        assert(Scr_BuiltinPointerFromIndex(cached.u.intValue) == pointer + i);
    }
    assert(scrCompilePub.func_table_size == 1024);
}

static void check_shutdown(void)
{
    PrecacheEntry *first = calloc(1, sizeof(*first));
    PrecacheEntry *second = calloc(1, sizeof(*second));
    assert(first && second);
    first->next = second;
    SCRCG->precachescriptListHead = first;
    Scr_CompileShutdown();
    assert(freed_count == 2);
    assert(SCRCG->precachescriptListHead == NULL);
}

static void check_error_position(void)
{
    char program[16] = {0};
    VariableValue target = {0};
    assert((uintptr_t)program > UINT32_MAX);
    scrVarPub.programBuffer = program;
    callsite.codePosValue = 4;
    target.type = SCRCOMP_VAR_DEVELOPER_CODEPOS;
    LinkThread(1, &target, 1);
    assert(error_position == (uintptr_t)(program + 4));
    error_position = 0;
    target.type = 0;
    LinkThread(1, &target, 1);
    assert(error_position == (uintptr_t)(program + 4));
    error_position = 0;
    target.type = SCRCOMP_VAR_CODEPOS;
    *(int *)(program + 4) = 1;
    LinkThread(1, &target, 0);
    assert(error_position == (uintptr_t)(program + 4));
}

static void check_child_arrays(void)
{
    scr_block_t block = {0};
    for (size_t i = 0; i < sizeof(compiler_child_array_sizes) / sizeof(size_t); ++i) {
        scr_block_t **children = malloc(compiler_child_array_sizes[i]);
        int count = 0;
        assert(children);
        for (int j = 0; j < 1024; ++j)
            Scr_CalcLocalVarsAddChildBlock(children, &count, &block);
        assert(count == 1024);
        assert(children[1023] == &block);
        free(children);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "identity"))
        check_function_identity();
    else if (!strcmp(argv[1], "shutdown"))
        check_shutdown();
    else if (!strcmp(argv[1], "error-position"))
        check_error_position();
    else if (!strcmp(argv[1], "child-arrays"))
        check_child_arrays();
    else if (!strcmp(argv[1], "builtin-cache"))
        check_builtin_cache();
    else
        abort();
    puts(argv[1]);
    return 0;
}
