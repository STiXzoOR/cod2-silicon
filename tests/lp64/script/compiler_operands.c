/* Real builtin emitters and packed allocator; catches misaligned or widened index stores. */
#include <assert.h>
#include <stdlib.h>
#ifndef SCR_COMPILER_SOURCE
#define SCR_COMPILER_SOURCE "PC/script/scr_compiler.c"
#endif
#include SCR_COMPILER_SOURCE
#include "PC/script/scr_tempmemory.c"

_Static_assert(sizeof(short) == 2, "builtin table indices occupy two bytes");

scrCompilePub_t scrCompilePub;
scrVarPub_t scrVarPub;
void *const imp_scrVarPub = &scrVarPub;
unsigned char scrMemTreeGlob[0x80330];
static scrCompileGlob_t compile_state;
void *imp_scrCompileGlob = &compile_state;
int currentPos;

static uint64_t buffer_words[16];
static byte *const buffer = (byte *)buffer_words;
static int errors;

void *Hunk_ReallocateTempMemoryInternal(int minimumSize)
{
    assert(minimumSize >= 0 && minimumSize <= (int)sizeof(buffer_words));
    return buffer;
}

void Com_Error(int code, const char *fmt, ...)
{
    (void)code;
    fprintf(stderr, "%s\n", fmt);
    abort();
}

void CompileError(unsigned int sourcePos, const char *fmt, ...)
{
    assert(sourcePos == 47);
    assert(strcmp(fmt, "parameter count exceeds 256") == 0);
    ++errors;
}

void AddOpcodePos(unsigned int sourcePos, int type)
{
    assert(sourcePos == 47 && type == 1);
}

/* No pending values or folding in these cases; reject unexpected dependency paths. */
void RemoveOpcodePos(void) { abort(); }
void SL_RemoveRefToString(unsigned int value) { (void)value; abort(); }
void SL_AddRefToString(unsigned int value) { (void)value; abort(); }
void SL_TransferRefToUser(unsigned int value, unsigned int user)
{
    (void)value; (void)user; abort();
}
void RemoveRefToVector(const float *value) { (void)value; abort(); }

static void reset(int prefix, int index)
{
    memset(&scrCompilePub, 0, sizeof(scrCompilePub));
    memset(&scrVarPub, 0, sizeof(scrVarPub));
    memset(&compile_state, 0, sizeof(compile_state));
    memset(buffer, 0xa5, sizeof(buffer_words));
    TempMemoryReset();
    if (prefix) {
        /* The actual GetSelf opcode gives both possible operand address parities. */
        *TempMalloc(1) = 0x0f;
    }
    for (int i = 0; i < index; ++i)
        scrCompilePub.func_table[i] = (intptr_t)UINT64_C(0x100001000) + i;
    scrCompilePub.func_table_size = index;
    errors = 0;
}

static void check_builtin(int method, int odd)
{
    static const struct {
        int param_count;
        byte function_opcode;
        byte method_opcode;
    } calls[] = {
        {0, 0x3e, 0x45}, {1, 0x3f, 0x46}, {2, 0x40, 0x47},
        {3, 0x41, 0x48}, {4, 0x42, 0x49}, {5, 0x43, 0x4a},
        {6, 0x44, 0x4b}, {255, 0x44, 0x4b},
    };
    static const struct { int index; byte low; byte high; } indices[] = {
        {0, 0x00, 0x00}, {0x12, 0x12, 0x00},
        {0x123, 0x23, 0x01}, {0x3ff, 0xff, 0x03},
    };
    const intptr_t target = (intptr_t)UINT64_C(0x200002222);

    for (size_t c = 0; c < sizeof(calls) / sizeof(calls[0]); ++c) {
        for (size_t i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i) {
            int params = calls[c].param_count;
            int extra = params > 5;
            /* One opcode, optional count byte, then exactly two index bytes. */
            int prefix = odd ? extra : !extra;
            int operand = prefix + 1 + extra;
            int end = operand + 2;
            reset(prefix, indices[i].index);
            if (method)
                EmitBuiltinMethodOpcode(params, 47, target);
            else
                EmitBuiltinFunctionOpcode(params, 47, target);

            assert(errors == 0);
            assert(currentPos == end);
            assert((uintptr_t)(buffer + operand) % 2 == (unsigned int)odd);
            assert(compile_state.codePos == buffer + operand);
            assert(scrCompilePub.opcodePos == buffer + prefix);
            if (prefix) assert(buffer[0] == 0x0f);
            assert(buffer[prefix] == (method ? calls[c].method_opcode : calls[c].function_opcode));
            if (extra) assert(buffer[prefix + 1] == (byte)params);
            assert(buffer[operand] == indices[i].low);
            assert(buffer[operand + 1] == indices[i].high);
            for (size_t tail = end; tail < sizeof(buffer_words); ++tail)
                assert(buffer[tail] == 0xa5);
            assert(scrCompilePub.func_table_size == indices[i].index + 1);
            assert(scrCompilePub.func_table[indices[i].index] == target);
        }
    }
    reset(0, 0);
    if (method) EmitBuiltinMethodOpcode(256, 47, target);
    else EmitBuiltinFunctionOpcode(256, 47, target);
    assert(errors == 1 && currentPos == 0 && scrCompilePub.func_table_size == 0);
    for (size_t i = 0; i < sizeof(buffer_words); ++i) assert(buffer[i] == 0xa5);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "builtin-method-odd")) check_builtin(1, 1);
    else if (!strcmp(argv[1], "builtin-method-aligned")) check_builtin(1, 0);
    else if (!strcmp(argv[1], "builtin-function-odd")) check_builtin(0, 1);
    else if (!strcmp(argv[1], "builtin-function-aligned")) check_builtin(0, 0);
    else return 2;
    puts("packed builtin operands: PASS (32 byte layouts and invalid-count control)");
    return 0;
}
