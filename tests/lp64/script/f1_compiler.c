/* Regression expectations: Mac 1.3 EmitCall, EvalPrimitiveExpression,
 * EmitIfElseStatement and EmitStatement (waittillmatch). */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>

static scrCompilePub_t compilePub;
static scrCompileGlob_t compileGlob;
static scrVarPub_t variable_state;
void *const imp_scrVarPub = &variable_state;
#define scrCompilePub compilePub
#define SCRCP (&compilePub)
#define SCRCG (&compileGlob)
#define SCRVP (&variable_state)
static const char *typeNames[7] = {"undefined", "object", "string", "localized string", "vector", "float", "int"};
void *imp_var_typename = typeNames;
unsigned char scrMemTreeGlob[sizeof(scrMemTreeGlob_t)];
static unsigned char program[256];
static int position;
static unsigned int opcodes[16];
static int offsets[16], opcodeCount;
static unsigned int sourcePositions[16];
static int sourceCount, sourceTypes[16];
static intptr_t expressions[16];
static int expressionCount;
static float *vector = (float *)(scrMemTreeGlob + 64);

static Bool EvalPrimitiveExpression(sval_t, VariableCompileValue *);
static Bool EvalBinaryOperatorExpression(sval_t, sval_t, sval_t, sval_t, VariableCompileValue *);
static char *TempMalloc(int size) { char *p = (char *)program + position; position += size; assert(position <= 256); return p; }
static char *TempMallocAlign(int size) { return TempMalloc(size); }
static void TempMemorySetPos(char *p) { position = (int)(p - (char *)program); }
static void CompileError(unsigned int pos, const char *fmt, ...) { (void)pos; fprintf(stderr, "%s\n", fmt); abort(); }
static void AddRefToValue(int type, VariableUnion u) { (void)type; (void)u; abort(); }
static void Scr_EvalBinaryOperator(int op, VariableValue *a, VariableValue *b) { (void)op; (void)a; (void)b; abort(); }
static const float *Scr_AllocVector(const float *v) { memcpy(vector, v, 12); return vector; }
static unsigned int EmitOpcode(unsigned int op, int offset, int callType)
{
    (void)callType;
    assert(opcodeCount < 16);
    opcodes[opcodeCount] = op;
    offsets[opcodeCount++] = offset;
    *TempMalloc(1) = (char)op;
    return 0;
}
static void AddOpcodePos(unsigned int pos, int type) { sourceTypes[sourceCount] = type; sourcePositions[sourceCount++] = pos; }
static Bool EmitOrEvalExpression(sval_t expr, VariableCompileValue *value, scr_block_t *block)
{
    (void)value; (void)block;
    expressions[expressionCount++] = expr.node;
    return 0;
}
static Bool EmitOrEvalPrimitiveExpression(sval_t expr, VariableCompileValue *value, scr_block_t *block)
{ return EmitOrEvalExpression(expr, value, block); }
static unsigned int EmitValue(VariableCompileValue *value) { (void)value; abort(); }
static scrVmPub_t vm_state;
void *imp_scrVmPub = &vm_state;
#define scrVarPub variable_state
static char runtimeMessage[256];
static void Com_Printf(const char *format, ...) { (void)format; }
static void Com_PrintMessage(int type, const char *text) { (void)type; (void)text; }
static void Scr_PrintPrevCodePos(int type, const char *pos, unsigned int index) { (void)type; (void)pos; (void)index; }
static char *va(const char *format, ...)
{
    static char text[256];
    va_list ap;
    va_start(ap, format); vsnprintf(text, sizeof(text), format, ap); va_end(ap);
    return text;
}
static void Com_Error(int code, const char *format, ...)
{
    assert(code == 5);
    va_list ap;
    va_start(ap, format); vsnprintf(runtimeMessage, sizeof(runtimeMessage), format, ap); va_end(ap);
}
#include "f1_compiler_under_test.h"

static void check_developer(const char *kind)
{
    int type = 1;
    char *saved = NULL;
    position = 11;
    compilePub.programLen = 867;
    if (!strcmp(kind, "developer-nested")) {
        compilePub.developer_statement = 2;
        assert(!EmitPrepareDeveloperCall(&type, &saved));
        assert(type == 0 && compilePub.developer_statement == 2);
    } else {
        variable_state.developer_script = !strcmp(kind, "developer-enabled");
        assert(EmitPrepareDeveloperCall(&type, &saved));
        assert(compilePub.developer_statement == (variable_state.developer_script ? 1 : 2));
        TempMalloc(3);
        EmitFinishDeveloperCall(type, saved);
        assert(compilePub.developer_statement == 0);
        assert(position == (variable_state.developer_script ? 14 : 11));
    }
    assert(compilePub.programLen == 867);
}

static void check_vector(void)
{
    sval_t primitive[3][3] = {{{0}}}, expr[3][2] = {{{0}}}, pair[3][2] = {{{0}}}, link[3][2] = {{{0}}};
    sval_t list[1] = {{0}}, outer[3] = {{0}}, value = {0};
    for (int i = 0; i < 3; ++i) {
        primitive[i][0].type = 7;
        primitive[i][1].intValue = 3 - i;
        primitive[i][2].sourcePosValue = 10 + i;
        expr[i][0].type = 6; expr[i][1].node = (intptr_t)primitive[i];
        pair[i][0].node = (intptr_t)expr[i];
        link[i][0].node = (intptr_t)pair[i];
        link[i][1].node = i == 2 ? 0 : (intptr_t)link[i + 1];
    }
    list[0].node = (intptr_t)link;
    outer[0].type = 46; outer[1].node = (intptr_t)list; outer[2].sourcePosValue = 55;
    value.node = (intptr_t)outer;
    VariableCompileValue result;
    assert(EvalPrimitiveExpression(value, &result));
    assert(result.value.type == 4 && result.sourcePos.sourcePosValue == 55);
    const float *actual = SCR_VEC_PTR(result.value.u);
    assert(actual[0] == 1 && actual[1] == 2 && actual[2] == 3);
}

static void check_negative_minimum(void)
{
    sval_t primitive[3] = {{0}}, expression = {0};
    primitive[0].type = 9;
    primitive[1].intValue = (-2147483647 - 1);
    expression.node = (intptr_t)primitive;
    VariableCompileValue value;
    assert(EvalPrimitiveExpression(expression, &value));
    assert(value.value.type == 6 && value.value.u.intValue == (-2147483647 - 1));
}

static void check_branch(void)
{
    scr_block_t parent = {0}, a = {0}, b = {0};
    scr_block_t *children[] = {&a, &b};
    a.localVarsPublicCount = 2; a.localVarsCreateCount = 4;
    b.localVarsPublicCount = 1; b.localVarsCreateCount = 3;
    a.localVarsInitBits[0] = 7; b.localVarsInitBits[0] = 7;
    Scr_MergeBranchChildBlocks(&parent, children, 2);
    assert(parent.localVarsCreateCount == 1);
    assert(parent.localVarsInitBits[0] == 1);
}

static void check_waitmatch(int count)
{
    sval_t node[5] = {{0}}, list[1] = {{0}}, links[4][2] = {{{0}}}, pairs[3][2] = {{{0}}};
    list[0].node = (intptr_t)links;
    links[0][1].node = (intptr_t)links[1];
    for (int i = 0; i <= count; ++i) {
        pairs[i][0].node = 100 + i;
        pairs[i][1].sourcePosValue = 50 + i;
        links[i + 1][0].node = (intptr_t)pairs[i];
        links[i + 1][1].node = i == count ? 0 : (intptr_t)links[i + 2];
    }
    node[1].node = 200; node[2].node = (intptr_t)list;
    node[3].sourcePosValue = 60; node[4].sourcePosValue = 70;
    emit_waitmatch(node, NULL);
    assert(opcodeCount == 2 && opcodes[0] == 0x77 && opcodes[1] == 0x34);
    assert(offsets[0] == -2 - count && offsets[1] == 0);
    assert(position == 3 && program[1] == count);
    assert(expressionCount == count + 2 && expressions[count] == 100 && expressions[count + 1] == 200);
    for (int i = 0; i < count; ++i) assert(expressions[i] == 101 + i);
    assert(sourceCount == count + 4);
    assert(sourcePositions[0] == 70 && sourcePositions[1] == 70 && sourcePositions[2] == 60);
    for (int i = 0; i <= count; ++i) assert(sourcePositions[i + 3] == 50 + i);
}

static void check_notify(int count)
{
    sval_t node[5] = {{0}}, list[1] = {{0}}, links[2][2] = {{{0}}}, pairs[2][2] = {{{0}}};
    list[0].node = (intptr_t)links;
    for (int i = 0; i < count; ++i) {
        pairs[i][0].node = 100 + i;
        pairs[i][1].sourcePosValue = 50 + i;
        links[i][0].node = (intptr_t)pairs[i];
        links[i][1].node = i + 1 == count ? 0 : (intptr_t)links[i + 1];
    }
    node[1].node = 200; node[2].node = (intptr_t)list;
    node[3].sourcePosValue = 60; node[4].sourcePosValue = 70;
    emit_notify(node, NULL);
    assert(opcodeCount == 2 && opcodes[0] == 0x7b && opcodes[1] == 0x79);
    assert(offsets[0] == 1 && offsets[1] == -2 - count);
    assert(expressionCount == count + 1 && expressions[count] == 200);
    for (int i = 0; i < count; ++i) assert(expressions[i] == 100 + i);
    assert(sourceCount == 4 && sourcePositions[2] == 50 + count - 1);
    assert(sourceTypes[0] == 1);
}

static void check_runtime_dialog(int present)
{
    variable_state.developer = 1;
    vm_state.terminal_error = 1;
    vm_state.debugCode = 1;
    RuntimeError(NULL, 0, "example", present ? "" : NULL);
    assert(!strcmp(runtimeMessage, present
        ? "\x15script runtime error\n(see console for details)\nexample\n"
        : "\x15script runtime error\n(see console for details)\nexample"));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strncmp(argv[1], "developer-", 10)) check_developer(argv[1]);
    else if (!strcmp(argv[1], "vector-fold")) check_vector();
    else if (!strcmp(argv[1], "negative-minimum")) check_negative_minimum();
    else if (!strcmp(argv[1], "branch-public")) check_branch();
    else if (!strcmp(argv[1], "waitmatch-zero")) check_waitmatch(0);
    else if (!strcmp(argv[1], "waitmatch-two")) check_waitmatch(2);
    else if (!strcmp(argv[1], "notify-one")) check_notify(1);
    else if (!strcmp(argv[1], "notify-two")) check_notify(2);
    else if (!strcmp(argv[1], "runtime-dialog-empty")) check_runtime_dialog(1);
    else if (!strcmp(argv[1], "runtime-dialog-null")) check_runtime_dialog(0);
    else abort();
    puts(argv[1]);
    return 0;
}
