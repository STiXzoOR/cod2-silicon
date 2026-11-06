#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include "../../../src/PC/script/scr_vm.c"

scrVmPub_t scrVmPub;
scrVarPub_t scrVarPub;
scrCompilePub_t scrCompilePub;
unsigned char scrVmGlob[sizeof(scrVmGlob_t)] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[0x80380] __attribute__((aligned(8)));
char g_EndPos;
jmp_buf g_script_error[33];
int g_script_error_level;
void *const imp_scrVarPub = &scrVarPub;
void *imp_scrCompilePub = &scrCompilePub;
static const char *typeNames[24] = {"undefined", "object", "string", "localized string", "vector", "float", "int"};
void *imp_var_typename = typeNames;
static jmp_buf error;
static int errorExpected;
static unsigned int refAdds, refRemoves;
static unsigned char arenaNext;

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int code, const char *fmt, ...)
{
    (void)code; (void)fmt;
    assert(errorExpected);
    longjmp(error, 1);
}
char *va(const char *fmt, ...)
{
    static char text[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(text, sizeof(text), fmt, ap); va_end(ap);
    return text;
}
void AddRefToValue(int type, VariableUnion u) { (void)type; (void)u; ++refAdds; }
void RemoveRefToValue(int type, VariableUnion u) { (void)type; (void)u; ++refRemoves; }
void AddRefToObject(unsigned int id) { (void)id; }
void RemoveRefToObject(unsigned int id) { (void)id; }
unsigned int GetParentLocalId(unsigned int id) { return id; }
unsigned int FindPrevSibling(unsigned int id) { (void)id; return 0; }
unsigned int FindEntityId(int entnum, int classnum) { (void)entnum; (void)classnum; return 0; }
Bool IsFieldObject(unsigned int id) { (void)id; return 0; }
int GetVarType(unsigned int id) { (void)id; return 6; }
unsigned int *MT_Alloc(int size, int type)
{
    (void)type; assert(size < 128); unsigned char *p = scrMemTreeGlob + 128 * ++arenaNext;
    memset(p, 0, 128); return (unsigned int *)p;
}
void MT_Free(void *p, int size) { (void)p; (void)size; }
int MT_Realloc(int oldBytes, int newBytes) { (void)oldBytes; (void)newBytes; return 1; }
void Scr_EvalEquality(VariableValue *a, VariableValue *b)
{
    assert(a->type == 6 && b->type == 6);
    a->u.intValue = a->u.intValue == b->u.intValue; a->type = 6;
}
void RuntimeError(const char *pos, unsigned int index, const char *message, const char *dialog)
{ (void)pos; (void)index; (void)message; (void)dialog; abort(); }

int main(int argc, char **argv)
{
    assert(argc == 2);
    scrVmPub.maxstack = scrVmPub.stack + 2047;
    scrVmPub.top = scrVmPub.stack;
    scrVmPub.localVars = ((scrVmGlob_t *)scrVmGlob)->localVarsStack;
    scrVmPub.function_frame = scrVmPub.function_frame_start + 1;
    scrVmPub.function_count = 1;
    if (!strcmp(argv[1], "match-short")) {
        VariableStackBuffer *saved = (VariableStackBuffer *)(scrMemTreeGlob + 128);
        const char count[] = {2};
        saved->pos = count; saved->size = 2;
        for (int i = 0; i < 2; ++i) {
            saved->buf[i * 5] = 6;
            int n = 7; memcpy(saved->buf + i * 5 + 1, &n, 4);
        }
        scrVmPub.stack[0].type = 8;
        scrVmPub.stack[1].type = 6; scrVmPub.stack[1].u.intValue = 7;
        assert(!VM_NotifyWaittillMatches(saved, scrVmPub.stack + 1));
        scrVmPub.stack[2] = scrVmPub.stack[1];
        assert(VM_NotifyWaittillMatches(saved, scrVmPub.stack + 2));
        assert(refAdds == 6);
    } else if (!strcmp(argv[1], "archive-packed")) {
        unsigned int id = 9;
        scrVmPub.stack[1].type = 6; scrVmPub.stack[1].u.intValue = 12;
        scrVmPub.stack[2].type = 6; scrVmPub.stack[2].u.intValue = 34;
        VariableStackBuffer *saved = VM_ArchiveStack(2, "", scrVmPub.stack + 2, 0, &id);
        int a, b; memcpy(&a, saved->buf + 1, 4); memcpy(&b, saved->buf + 6, 4);
        assert(a == 12 && b == 34 && saved->size == 2);
    } else if (!strcmp(argv[1], "append-packed")) {
        VariableStackBuffer *saved = (VariableStackBuffer *)(scrMemTreeGlob + 128);
        VariableUnion ref = {.stackValue = SCR_STACK_ENC(saved)};
        saved->size = 1; saved->bufLen = 128;
        scrVmPub.stack[0].type = 8;
        scrVmPub.stack[1].type = 6; scrVmPub.stack[1].u.intValue = 34;
        VM_NotifyAppendStackParams(&ref, &saved, scrVmPub.stack + 1);
        int n; memcpy(&n, saved->buf + 6, 4);
        assert(saved->size == 2 && n == 34 && refAdds == 1);
    } else if (!strcmp(argv[1], "wait-overflow")) {
        VariableValue v = {.type = 6, .u.intValue = 0x40000000};
        assert(VM_CandidateWaitTimeFromValue(&v) == 0);
        v.type = 5; v.u.floatValue = INFINITY;
        assert(VM_CandidateWaitTimeFromValue(&v) == 0x80000000u);
        v.u.floatValue = NAN;
        assert(VM_CandidateWaitTimeFromValue(&v) == 0x80000000u);
    } else if (!strcmp(argv[1], "vector-packed")) {
        char program[32] = {0};
        float expected[3] = {1.0f, -2.0f, 0.25f}, actual[3];
        memcpy(program + 1, expected, sizeof(expected));
        scrVarPub.programBuffer = program;
        scrVmPub.top = scrVmPub.stack + 1;
        scrVmPub.outparamcount = 1;
        scrVmPub.top->type = 4;
        scrVmPub.top->u.vectorValue = SCR_VEC_ENC((const float *)(program + 1));
        Scr_GetVector(0, actual);
        assert(!memcmp(expected, actual, sizeof(expected)));
    } else if (!strcmp(argv[1], "unknown-object")) {
        VariableValue *top = scrVmPub.stack;
        scrVmPub.function_count = 0;
        errorExpected = 1;
        if (!setjmp(error)) { VM_CandidatePushEntityObject(&top, 1, 7); assert(0); }
        assert(scrVarPub.error_message && !strcmp(scrVarPub.error_message, "unknown object"));
        assert(top == scrVmPub.stack + 1 && top->type == 0);
    } else if (!strcmp(argv[1], "field-object")) {
        scrVmPub.function_count = 0;
        errorExpected = 1;
        if (!setjmp(error)) { VM_CandidateFieldObjectCheck(9); assert(0); }
        assert(scrVarPub.error_message && !strcmp(scrVarPub.error_message, "int is not an object"));
    } else { abort(); }
    puts(argv[1]);
    return 0;
}
