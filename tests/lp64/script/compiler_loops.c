/* Synthetic GSC parsed by the real scanner/parser and local-variable compiler. */
#include "PC/script/scr_yacc.c"
#include "PC/script/scr_parsetree.c"
#ifndef SCR_COMPILER_SOURCE
#define SCR_COMPILER_SOURCE "PC/script/scr_compiler.c"
#endif
#include SCR_COMPILER_SOURCE
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>

int yychar, yynerrs, yyleng, yy_n_chars, yy_init, yy_start;
int yy_did_buffer_switch_on_eof;
stype_t yylval;
FILE *yyin, *yyout;
char *yytext, *yy_c_buf_p, *yy_last_accepting_cpos;
char yy_hold_char;
yy_state_type yy_last_accepting_state;
YY_BUFFER_STATE yy_current_buffer;
sval_t yaccResult, g_dummyVal;
unsigned int g_out_pos, g_sourcePos;
unsigned char g_parse_user;
char ch_buf[0x4002];
scrCompilePub_t scrCompilePub;
scrVarPub_t scrVarPub;
void *const imp_scrVarPub = &scrVarPub;
unsigned char scrMemTreeGlob[sizeof(scrMemTreeGlob_t)];
unsigned char scrCompileGlob[sizeof(scrCompileGlob_t)];
unsigned char scrVarGlob[1048608];
void *imp_scrCompileGlob = scrCompileGlob;
void *imp_scrCompilePub = &scrCompilePub;
static const char *typeNames[24] = {"undefined", "object", "string", "localized string", "vector", "float", "int"};
void *imp_var_typename = typeNames;
const char **var_typename = typeNames;

static const char *script;
static size_t inputPosition, allocationCount;
static void *allocations[4096];
static char interned[256][64];
static unsigned int internCount;
static jmp_buf failure;
static char diagnostic[256];
static int releasedStrings;

int Scr_ScanFile(char *buf, int maximum)
{
    size_t remaining = strlen(script) - inputPosition;
    size_t count = remaining < (size_t)maximum ? remaining : (size_t)maximum;
    memcpy(buf, script + inputPosition, count);
    inputPosition += count;
    return (int)count;
}

void *Hunk_AllocateTempMemoryHighInternal(int size)
{
    assert(allocationCount < 4096);
    void *node = calloc(1, (size_t)size);
    assert(node && (uintptr_t)node > UINT32_MAX);
    allocations[allocationCount++] = node;
    return node;
}

unsigned int SL_GetString_(const char *s, unsigned int user, int type)
{
    (void)user;
    (void)type;
    for (unsigned int i = 0; i < internCount; ++i)
        if (!strcmp(interned[i], s))
            return i + 1;
    assert(internCount < 256 && strlen(s) < sizeof(interned[0]));
    strcpy(interned[internCount], s);
    return ++internCount;
}

unsigned int SL_GetStringOfLen(const char *s, unsigned int user, unsigned int length, int type)
{
    assert(length == strlen(s) + 1);
    return SL_GetString_(s, user, type);
}

unsigned int SL_ConvertToLowercase(unsigned int value, unsigned int user, int type)
{
    (void)user;
    (void)type;
    assert(value && value <= internCount);
    for (char *p = interned[value - 1]; *p; ++p)
        *p = (char)tolower((unsigned char)*p);
    return value;
}

const char *SL_ConvertToString(unsigned int value) { assert(value && value <= internCount); return interned[value - 1]; }
void SL_RemoveRefToString(unsigned int value) { (void)value; ++releasedStrings; }
void AddRefToValue(int type, VariableUnion value) { (void)type; (void)value; }
void RemoveRefToValue(int type, VariableUnion value) { if (type == 2 || type == 3) SL_RemoveRefToString(value.stringValue); }
static void RemoveRefToValue_core(int type, VariableUnion value) { RemoveRefToValue(type, value); }
void RemoveRefToObject(unsigned int id) { (void)id; abort(); }
void RemoveRefToVector(const float *v) { (void)v; abort(); }
const float *Scr_AllocVector(const float *v) { (void)v; abort(); }
char *va(const char *format, ...)
{
    static char text[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(text, sizeof(text), format, ap);
    va_end(ap);
    return text;
}
void Scr_Error(const char *text) { scrVarPub.error_message = text; }
void Scr_UnmatchingTypesError(VariableValue *a, VariableValue *b) { (void)a; (void)b; abort(); }
/* The numeric conversion, equality and Bool conversion bodies come from scr_variable.c. */
#include "compiler_values.h"
void Scr_EvalBinaryOperator(int op, VariableValue *a, VariableValue *b)
{
    assert(op == 0x69);
    Scr_EvalEquality(a, b);
}

void CompileError(unsigned int sourcePosition, const char *message, ...)
{
    (void)sourcePosition;
    va_list ap;
    va_start(ap, message);
    vsnprintf(diagnostic, sizeof(diagnostic), message, ap);
    va_end(ap);
    longjmp(failure, 1);
}

void Com_Error(int code, const char *message, ...)
{
    (void)code;
    snprintf(diagnostic, sizeof(diagnostic), "%s", message);
    longjmp(failure, 1);
}

static void check_locals(const scr_block_t *block, const char *names)
{
    char actual[1024] = "";
    for (int i = 0; i < block->localVarsCount; ++i) {
        if (i)
            strcat(actual, ",");
        strcat(actual, SL_ConvertToString(block->localVars[i].name));
    }
    if (strcmp(actual, names)) {
        fprintf(stderr, "expected locals [%s], got [%s]\n", names, actual);
        abort();
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *expected = "flag,tail";
    const char *expectedDiagnostic = NULL;
    char generated[8192];
    if (!strcmp(argv[1], "while-variable"))
        script = "+main(flag) { while(flag) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "while-true")) {
        script = "+main(flag) { while(1) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "while-float")) {
        script = "+main(flag) { while(0.5) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "while-binary")) {
        script = "+main(flag) { while(2==2) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "while-false"))
        script = "+main(flag) { while(0) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "while-float-zero"))
        script = "+main(flag) { while(-0.0) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "while-binary-false"))
        script = "+main(flag) { while(2==3) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "while-string"))
        script = "+main(flag) { while(\"text\") { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "while-continue")) {
        script = "+main(flag) { while(flag) { inside=7; continue; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "while-nested"))
        script = "+main(flag) { while(flag) { while(1) { inside=7; break; } break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "while-break-intersection")) {
        script = "+main(flag) { while(1) { if(flag) { shared=7; one=1; break; } else { shared=8; two=2; break; } } tail=3; return tail; }";
        expected = "flag,shared,tail";
    } else if (!strcmp(argv[1], "for-variable"))
        script = "+main(flag) { for(;flag;) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "for-true")) {
        script = "+main(flag) { for(;1;) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "for-omitted")) {
        script = "+main(flag) { for(;;) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "for-binary")) {
        script = "+main(flag) { for(;2==2;) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "for-false"))
        script = "+main(flag) { for(;0;) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "for-binary-false"))
        script = "+main(flag) { for(;2==3;) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "for-literal-true")) {
        script = "+main(flag) { for(;true;) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "for-literal-false")) {
        script = "+main(flag) { for(;false;) { inside=7; break; } tail=3; return tail; }";
    } else if (!strcmp(argv[1], "while-literal-true")) {
        script = "+main(flag) { while(true) { inside=7; break; } tail=3; return tail; }";
        expected = "flag,inside,tail";
    } else if (!strcmp(argv[1], "while-literal-false")) {
        script = "+main(flag) { while(false) { inside=7; break; } tail=3; return tail; }";
    }
    else if (!strcmp(argv[1], "for-string"))
        script = "+main(flag) { for(;\"text\";) { inside=7; break; } tail=3; return tail; }";
    else if (!strcmp(argv[1], "loop-local-limit") || !strcmp(argv[1], "loop-true-local-limit")) {
        int constant = !strcmp(argv[1], "loop-true-local-limit");
        if (constant)
            expectedDiagnostic = "LOCAL_VAR_STACK_SIZE exceeded";
        strcpy(generated, "+main(flag) {");
        for (int i = 0; i < 22; ++i) {
            char statement[160];
            snprintf(statement, sizeof(statement), "while(%s){ a%d=1; b%d=2; c%d=3; break; }", constant ? "1" : "flag", i, i, i);
            strcat(generated, statement);
        }
        strcat(generated, "tail=3; return tail;}");
        script = generated;
    } else if (!strcmp(argv[1], "parser-bad-syntax")) {
        script = "+main() { return }";
        expectedDiagnostic = "bad syntax";
    } else if (!strcmp(argv[1], "parser-unexpected-eof")) {
        script = "+main() { return;";
        expectedDiagnostic = "unexpected end of file found";
    } else if (!strcmp(argv[1], "parser-stack-growth")) {
        strcpy(generated, "+main(flag) { tail=");
        for (int i = 0; i < 240; ++i)
            strcat(generated, "(");
        strcat(generated, "3");
        for (int i = 0; i < 240; ++i)
            strcat(generated, ")");
        strcat(generated, ";return tail;}");
        script = generated;
    } else
        abort();

    int failed = setjmp(failure);
    if (!failed) {
        sval_t result;
        ScriptParse(&result, 1);
        assert(yynerrs == 0 && result.node);
        sval_t *root = (sval_t *)result.node;
        intptr_t *entry = ScriptListFirstNode(root[1]);
        assert(entry);
        intptr_t *function = (intptr_t *)entry[0];
        assert(function[0] == 0x44);
        scr_block_t block;
        Scr_InitThreadBlock(function, &block);
        Scr_CalcThreadLocalVars(function, &block);
        check_locals(&block, expected);
        if (strstr(argv[1], "string"))
            assert(releasedStrings == 1);
        assert(SCRCG->breakChildBlocks == NULL && SCRCG->continueChildBlocks == NULL);
        assert(!expectedDiagnostic);
    } else if (!expectedDiagnostic || strcmp(diagnostic, expectedDiagnostic))
        fprintf(stderr, "unexpected compiler diagnostic: %s\n", diagnostic);
    for (size_t i = 0; i < allocationCount; ++i)
        free(allocations[i]);
    if (failed && (!expectedDiagnostic || strcmp(diagnostic, expectedDiagnostic)))
        return 1;
    puts(argv[1]);
    return 0;
}
