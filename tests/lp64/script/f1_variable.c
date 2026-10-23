#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifndef F1_VARIABLE_SOURCE
#define F1_VARIABLE_SOURCE "../../../src/PC/script/scr_variable.c"
#endif
#include F1_VARIABLE_SOURCE

unsigned char scrVarGlob[1048608] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[0x80380] __attribute__((aligned(8)));
scrMemTreePub_t scrMemTreePub;
scrVarPub_t scrVarPub;
void *const imp_scrVarPub = &scrVarPub;
char g_EndPos;

static jmp_buf errorJump;
static char errorText[512];
static char outputText[4096];
static unsigned int removedStrings;
static unsigned int fieldType;
static unsigned int fieldPayload;
static unsigned int fieldWrites;
static unsigned int errorReturns;
static unsigned int errorCalls;
static unsigned int watchedChildren[2];
static unsigned int detachedBeforeRelease;
static const char *printedPositions[32];
static unsigned int printedPositionCount;
static char programBytes[0x30000];

void Com_Printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(outputText + strlen(outputText), sizeof(outputText) - strlen(outputText), fmt, ap);
    va_end(ap);
}
void Com_Error(errorParm_t severity, const char *fmt, ...)
{
    (void)severity; (void)fmt; abort();
}
char *va(const char *fmt, ...)
{
    static char text[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    return text;
}
void Scr_Error(const char *msg)
{
    errorCalls++;
    if (!scrVarPub.error_message) {
        snprintf(errorText, sizeof(errorText), "%s", msg);
        scrVarPub.error_message = errorText;
    }
    if (errorReturns)
        return;
    longjmp(errorJump, 1);
}
void Scr_TerminalError(const char *msg) { Scr_Error(msg); }
void SL_AddRefToString(unsigned int id) { (void)id; }
void SL_RemoveRefToString(unsigned int id)
{
    removedStrings++;
    if (id == 101 && watchedChildren[0] && removedStrings == 1) {
        detachedBeforeRelease =
            (VG_STATUS(watchedChildren[0]) & SCRVL_VAR_ALLOCATED) == SCRVL_VAR_ALLOCATED &&
            (VG_STATUS(watchedChildren[1]) & SCRVL_VAR_ALLOCATED) == SCRVL_VAR_ALLOCATED;
    }
}
const char *SL_ConvertToString(unsigned int id) { (void)id; return "value"; }
int SL_GetStringLen(unsigned int id) { (void)id; return 5; }
int stricmp(const char *first, const char *second) { return strcmp(first, second); }
unsigned int SL_GetString_(const char *s, unsigned int user, int type)
{ (void)s; (void)user; (void)type; return 100; }
unsigned int SL_GetStringForInt(int i) { (void)i; return 100; }
unsigned int SL_GetStringForFloat(float f) { (void)f; return 100; }
unsigned int SL_GetStringForVector(const float *v) { (void)v; return 100; }
unsigned int SL_GetStringOfLen(const char *s, unsigned int u, unsigned int n, int t)
{ (void)s; (void)u; (void)n; (void)t; return 100; }
XAnim *Scr_GetAnims(int n) { (void)n; return NULL; }
const char *XAnimGetAnimDebugName(const XAnim *a, unsigned int i)
{ (void)a; (void)i; return "animation"; }
void Scr_CancelNotifyList(unsigned int id) { (void)id; }
void VM_CancelNotify(unsigned int a, unsigned int b) { (void)a; (void)b; }
unsigned long long GetEntityFieldValue(unsigned int classnum, int entnum, int offset)
{
    assert(classnum == 0 && entnum == 7 && offset == 9);
    return ((unsigned long long)fieldType << 32) | fieldPayload;
}
Bool SetEntityFieldValue(unsigned int classnum, int entnum, int offset, VariableValue *value)
{ (void)classnum; (void)entnum; (void)offset; (void)value; fieldWrites++; return 1; }
void *Z_TryMallocInternal(int size) { return malloc(size); }
void Z_FreeInternal(void *ptr) { free(ptr); }
void Scr_PrintPrevCodePos(print_msg_type_t type, const char *pos, unsigned int index)
{
    assert(type == 0 && index == 0 && printedPositionCount < 32);
    printedPositions[printedPositionCount++] = pos;
}
void MT_Init(void);

static unsigned int entity(void)
{
    unsigned int id, field;
    Scr_SetClassMap(0);
    id = Scr_GetEntityId(7, 0);
    field = GetArrayVariable(g_classMap[0].id, 23);
    VG_STATUS(field) |= SCRVL_VAR_INTEGER;
    VG_U32(field) = 9;
    scrVarPub.entId = id;
    scrVarPub.entFieldName = 23;
    return id;
}

int main(int argc, char **argv)
{
    unsigned int id, owner;
    VariableValue first, second;
    const char *name;
    assert(argc == 2);
    name = argv[1];
    MT_Init();
    Var_Init();
    scrVarPub.programBuffer = programBytes;

    if (!strcmp(name, "adjacent_collision")) {
        unsigned int a, b, current, count = 0;
        owner = Scr_AllocArray();
        first.type = SCRVL_VAR_INTEGER;
        first.u.intValue = 7;
        a = GetNewArrayVariable(owner, 0);
        SetNewVariableValue(a, &first);
        b = GetNewArrayVariable(owner, 65533);
        SetNewVariableValue(b, &first);
        RemoveVariable(owner, GetInternalVariableIndex(0));
        assert(GetArraySize(owner) == 1 && FindNextSibling(owner) == b);
        assert(FindPrevSibling(owner) == b && !FindNextSibling(b) && !FindPrevSibling(b));
        assert(VG_U32(b) == 7 && FindArrayVariable(owner, 65533) == b);
        RemoveRefToObject(owner);
        current = VG_U16(0);
        while (current) {
            assert(++count <= SCRVL_MAX_VARIABLES);
            current = VG_U16(VG_ID(current));
        }
        assert(count == SCRVL_MAX_VARIABLES);
    } else if (!strcmp(name, "mixed_parent_churn")) {
        unsigned int parents[32], present[32] = {0}, seed = 0x12345678;
        unsigned int step, p, k, current, previous, count;
        for (p = 0; p < 32; ++p) parents[p] = Scr_AllocArray();
        first.type = SCRVL_VAR_INTEGER;
        first.u.intValue = 7;
        for (step = 0; step < 100000; ++step) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            p = seed & 31; k = (seed >> 8) & 31;
            unsigned int key = k < 16 ? 100 + k : 0x20000 + (k - 16) * 65533;
            if (present[p] & (1u << k)) {
                RemoveVariable(parents[p], key);
                present[p] &= ~(1u << k);
            } else {
                SetNewVariableValue(GetVariable(parents[p], key), &first);
                present[p] |= 1u << k;
            }
            if ((step & 255) == 0) {
                for (p = 0; p < 32; ++p) {
                    unsigned int expected = 0;
                    for (k = 0; k < 32; ++k) expected += (present[p] >> k) & 1;
                    assert(GetArraySize(parents[p]) == expected);
                    current = FindNextSibling(parents[p]); count = 0;
                    while (current) { assert(++count <= expected); current = FindNextSibling(current); }
                    assert(count == expected);
                    current = FindPrevSibling(parents[p]); count = 0;
                    while (current) { assert(++count <= expected); current = FindPrevSibling(current); }
                    assert(count == expected);
                }
                current = VG_U16(0); previous = count = 0;
                while (current) {
                    assert(++count <= SCRVL_MAX_VARIABLES);
                    assert(!(VG_STATUS(VG_ID(current)) & SCRVL_VAR_ALLOCATED));
                    assert(VG_PREV(current) == previous);
                    previous = current; current = VG_U16(VG_ID(current));
                }
                assert(VG_PREV(0) == previous && count > 64000);
            }
        }
        for (p = 0; p < 32; ++p) RemoveRefToObject(parents[p]);
        current = VG_U16(0); count = 0;
        while (current) { assert(++count <= SCRVL_MAX_VARIABLES); current = VG_U16(VG_ID(current)); }
        assert(count == SCRVL_MAX_VARIABLES);
    } else if (!strcmp(name, "hash_collision_stress")) {
        unsigned int cycle;
        for (cycle = 0; cycle < 128; cycle++) {
            unsigned int parents[2] = {Scr_AllocArray(), Scr_AllocArray()};
            unsigned int external[64];
            unsigned int i, p, current, count;
            first.type = SCRVL_VAR_INTEGER;
            first.u.intValue = 7;
            for (i = 0; i < 64; i++) external[i] = AllocValue();
            for (i = 0; i < 64; i++) {
                for (p = 0; p < 2; p++) {
                    int key = -8257536 + (int)i * 65533;
                    SetNewVariableValue(GetNewArrayVariable(parents[p], (unsigned int)key), &first);
                }
            }
            for (i = 0; i < 64; i++) {
                int key = -8257536 + (int)((i * 13) & 63) * 65533;
                for (p = 0; p < 2; p++) {
                    current = FindArrayVariable(parents[p], key);
                    assert(current && VG_U32(current) == 7);
                    RemoveVariable(parents[p], GetInternalVariableIndex((unsigned int)key));
                }
                FreeValue(external[i]);
            }
            RemoveRefToObject(parents[0]);
            RemoveRefToObject(parents[1]);
            current = VG_U16(0);
            count = 0;
            while (current) {
                assert(++count <= SCRVL_MAX_VARIABLES);
                current = VG_U16(VG_ID(current));
            }
            assert(count == SCRVL_MAX_VARIABLES);
        }
    } else if (!strcmp(name, "object_sibling_stress")) {
        unsigned int cycle;
        for (cycle = 0; cycle < 256; cycle++) {
            unsigned int normal = Scr_AllocArray();
            unsigned int reverse = Scr_AllocArray();
            unsigned int keys[64];
            unsigned int i, current, count;
            first.type = SCRVL_VAR_INTEGER;
            first.u.intValue = 7;
            for (i = 0; i < 64; i++) {
                keys[i] = AllocObject();
                SetNewVariableValue(GetNewObjectVariable(normal, keys[i]), &first);
                SetNewVariableValue(GetNewObjectVariableReverse(reverse, keys[i]), &first);
            }
            assert(GetArraySize(normal) == 64 && GetArraySize(reverse) == 64);
            current = FindNextSibling(normal);
            for (i = 64; i; i--) {
                assert(GetVariableKeyObject(current) == keys[i - 1]);
                assert(VG_U32(current) == 7);
                current = FindNextSibling(current);
            }
            assert(!current);
            current = FindNextSibling(reverse);
            for (i = 0; i < 64; i++) {
                assert(GetVariableKeyObject(current) == keys[i]);
                current = FindNextSibling(current);
                assert(FindObjectVariable(normal, keys[i]));
                assert(FindObjectVariable(reverse, keys[i]));
                assert(VG_U16(keys[i]) == 2);
            }
            assert(!current);
            current = FindPrevSibling(normal);
            for (i = 0; i < 64; i++) {
                assert(GetVariableKeyObject(current) == keys[i]);
                current = FindPrevSibling(current);
            }
            assert(!current);
            current = FindPrevSibling(reverse);
            for (i = 64; i; i--) {
                assert(GetVariableKeyObject(current) == keys[i - 1]);
                current = FindPrevSibling(current);
            }
            assert(!current);
            ClearObject(normal);
            for (i = 0; i < 64; i++) {
                unsigned int index = (i * 13) & 63;
                assert(VG_U16(keys[index]) == 1);
                RemoveObjectVariable(reverse, keys[index]);
                assert(VG_U16(keys[index]) == 0);
                RemoveRefToObject(keys[index]);
            }
            assert(GetArraySize(normal) == 0 && GetArraySize(reverse) == 0);
            RemoveRefToObject(normal);
            RemoveRefToObject(reverse);
            current = VG_U16(0);
            count = 0;
            while (current) {
                assert(++count <= SCRVL_MAX_VARIABLES);
                current = VG_U16(VG_ID(current));
            }
            assert(count == SCRVL_MAX_VARIABLES);
        }
    } else if (!strcmp(name, "packed_notify")) {
        id = AllocThread(AllocObject());
        Scr_SetThreadNotifyName(id, 0xf123);
        assert(Scr_GetThreadNotifyName(id) == 0xf123);
        Scr_RemoveThreadNotifyName(id);
        assert(removedStrings == 1 && GetVarType(id) == 15);
        Scr_SetClassMap(1);
        id = Scr_GetEntityId(7, 1);
        assert(Scr_GetEntityIdRef(id) == 0x10007);
    } else if (!strcmp(name, "packed_field")) {
        char packed[7] __attribute__((aligned(4))) = {'a', 'b', 0, 0x23, 0x41, SCRVL_VAR_FLOAT, 0};
        int type;
        scrVarPub.fieldBuffer = packed;
        assert(Scr_FindField("ab", &type) == 0x4123 && type == SCRVL_VAR_FLOAT);
    } else if (!strcmp(name, "variable_object_error")) {
        id = AllocValue();
        VG_STATUS(id) |= SCRVL_VAR_INTEGER;
        if (!setjmp(errorJump)) {
            Scr_EvalVariableObject(id);
            assert(!"missing script error");
        }
        assert(!strcmp(errorText, "int is not a field object"));
        assert(!outputText[0]);
    } else if (!strcmp(name, "field_object_error")) {
        first.type = SCRVL_VAR_POINTER;
        first.u.pointerValue = id = Scr_AllocArray();
        if (!setjmp(errorJump)) {
            Scr_EvalFieldObject(AllocValue(), &first);
            assert(!"missing script error");
        }
        assert(!strcmp(errorText, "array is not a field object"));
        assert(IsVarFree(id));
        assert(!outputText[0]);
    } else if (!strcmp(name, "equality_developer") || !strcmp(name, "equality_evaluate")) {
        id = Scr_AllocArray();
        AddRefToObject(id);
        AddRefToObject(id);
        first.type = second.type = SCRVL_VAR_POINTER;
        first.u.pointerValue = second.u.pointerValue = id;
        scrVarPub.developer = !strcmp(name, "equality_developer");
        scrVarPub.evaluate = !scrVarPub.developer;
        if (!setjmp(errorJump)) {
            Scr_EvalEquality(&first, &second);
            assert(scrVarPub.evaluate && first.type == SCRVL_VAR_INTEGER && first.u.intValue == 1);
        } else {
            assert(!scrVarPub.evaluate);
            assert(strstr(errorText, "unmatching types"));
        }
        assert(VG_U16(id) == 0);
    } else if (!strcmp(name, "evaluate_error_flow")) {
        void (*operators[])(VariableValue *, VariableValue *) = {
            Scr_EvalLess, Scr_EvalGreater, Scr_EvalMinus,
            Scr_EvalMultiply, Scr_EvalDivide, Scr_EvalPlus
        };
        unsigned int i;
        errorReturns = scrVarPub.evaluate = 1;
        first.type = SCRVL_VAR_INTEGER;
        first.u.intValue = 7;
        second.type = 11;
        second.u.intValue = 0;
        Scr_EvalEquality(&first, &second);
        assert(errorCalls == 1 && scrVarPub.error_message);
        assert(first.type == SCRVL_VAR_INTEGER && first.u.intValue == 1);
        assert(second.type == SCRVL_VAR_UNDEFINED);
        for (i = 0; i < sizeof(operators) / sizeof(operators[0]); i++) {
            first.type = SCRVL_VAR_INTEGER;
            first.u.intValue = 7;
            second.type = 11;
            second.u.intValue = 0;
            errorCalls = 0;
            scrVarPub.error_message = NULL;
            operators[i](&first, &second);
            assert(errorCalls == 2 && scrVarPub.error_message);
            assert(first.type == SCRVL_VAR_UNDEFINED && second.type == SCRVL_VAR_UNDEFINED);
        }
    } else if (!strcmp(name, "array_ref_string")) {
        id = AllocValue();
        VG_STATUS(id) |= SCRVL_VAR_STRING;
        if (!setjmp(errorJump)) {
            Scr_EvalArrayRef(id);
            assert(!"missing script error");
        }
        assert(!strcmp(errorText, "string characters cannot be individually changed"));
        assert(removedStrings == 0 && scrVarPub.error_index == 1);
    } else if (!strcmp(name, "entity_array_ref_string") || !strcmp(name, "entity_clear_string")) {
        entity();
        fieldType = SCRVL_VAR_STRING;
        fieldPayload = 100;
        first.type = SCRVL_VAR_INTEGER;
        first.u.intValue = 0;
        if (!setjmp(errorJump)) {
            if (!strcmp(name, "entity_clear_string"))
                ClearArray(0xfffe, &first);
            else
                Scr_EvalArrayRef(0xfffe);
            assert(!"missing script error");
        }
        assert(removedStrings == 1 && scrVarPub.error_index == 1 && fieldWrites == 0);
        assert(!strcmp(errorText, !strcmp(name, "entity_clear_string") ?
                       "string is not an array" : "string characters cannot be individually changed"));
    } else if (!strcmp(name, "entity_array_ref_vector")) {
        const float vector[3] = {1.0f, 2.0f, 3.0f};
        const float *allocated;
        entity();
        allocated = Scr_AllocVector(vector);
        fieldType = SCRVL_VAR_VECTOR;
        fieldPayload = SCR_VEC_ENC(allocated);
        first.type = SCRVL_VAR_VECTOR;
        first.u.vectorValue = fieldPayload;
        AddRefToValue(first.type, first.u);
        if (!setjmp(errorJump)) {
            Scr_EvalArrayRef(0xfffe);
            assert(!"missing script error");
        }
        assert(!strcmp(errorText, "vector components cannot be individually changed"));
        assert(*(const unsigned short *)((const char *)allocated - 4) == 0);
    } else if (!strcmp(name, "entity_array_writable")) {
        entity();
        fieldType = SCRVL_VAR_POINTER;
        fieldPayload = id = Scr_AllocArray();
        AddRefToObject(id);
        assert(Scr_EvalArrayRef(0xfffe) == id);
        assert(VG_U16(id) == 0 && fieldWrites == 0);
    } else if (!strcmp(name, "array_copy_on_write")) {
        unsigned int copy;
        owner = AllocValue();
        id = Scr_AllocArray();
        first.type = SCRVL_VAR_POINTER;
        first.u.pointerValue = id;
        SetNewVariableValue(owner, &first);
        first.type = SCRVL_VAR_INTEGER;
        first.u.intValue = 42;
        SetNewVariableValue(GetArrayVariable(id, 0), &first);
        AddRefToObject(id);
        copy = Scr_EvalArrayRef(owner);
        assert(copy != id && VG_U32(owner) == copy && VG_U16(id) == 0);
        assert(VG_U32(FindArrayVariable(copy, 0)) == 42);
        first.u.intValue = 99;
        SetVariableValue(FindArrayVariable(copy, 0), &first);
        assert(VG_U32(FindArrayVariable(id, 0)) == 42);
    } else if (!strcmp(name, "clear_array_missing")) {
        owner = AllocValue();
        first.type = SCRVL_VAR_POINTER;
        first.u.pointerValue = Scr_AllocArray();
        SetNewVariableValue(owner, &first);
        first.type = SCRVL_VAR_STRING;
        first.u.stringValue = 23;
        id = VG_U16(0);
        ClearArray(owner, &first);
        assert(id == VG_U16(0) && removedStrings == 1);
    } else if (!strcmp(name, "entity_readonly")) {
        entity();
        fieldType = SCRVL_VAR_POINTER;
        fieldPayload = id = Scr_AllocArray();
        if (!setjmp(errorJump)) {
            Scr_EvalArrayRef(0xfffe);
            assert(!"missing script error");
        }
        assert(!strcmp(errorText, "read-only array cannot be changed"));
        assert(IsVarFree(id) && scrVarPub.error_index == 1);
    } else if (!strcmp(name, "entity_clear_undefined")) {
        id = entity();
        fieldType = SCRVL_VAR_UNDEFINED;
        first.type = SCRVL_VAR_INTEGER;
        first.u.intValue = 0;
        owner = VG_U16(0);
        if (!setjmp(errorJump)) {
            ClearArray(0xfffe, &first);
            assert(!"missing script error");
        }
        assert(!strcmp(errorText, "undefined is not an array"));
        assert(!FindVariable(id, 23) && owner == VG_U16(0) && fieldWrites == 0);
    } else if (!strncmp(name, "unaligned_vector_", 17)) {
        const float lhs[3] = {1.0f, 2.0f, 3.0f};
        const float rhs[3] = {5.0f, 6.0f, 7.0f};
        const float *literal = (const float *)(programBytes + 1);
        const float *other = (const float *)(programBytes + 18);
        const char *operation = name + 17;
        programBytes[0] = programBytes[17] = 1;
        memcpy(programBytes + 1, lhs, sizeof(lhs));
        memcpy(programBytes + 18, !strcmp(operation, "equal") ? lhs : rhs, sizeof(rhs));
        first.type = second.type = SCRVL_VAR_VECTOR;
        first.u.vectorValue = SCR_VEC_ENC(literal);
        second.u.vectorValue = SCR_VEC_ENC(other);
        if (!strcmp(operation, "alloc")) {
            const float *copy = Scr_AllocVector(literal);
            assert(copy[0] == 1.0f && copy[1] == 2.0f && copy[2] == 3.0f);
        } else if (!strcmp(operation, "eval")) {
            second.type = SCRVL_VAR_INTEGER;
            second.u.intValue = 1;
            Scr_EvalArray(&first, &second);
            assert(second.type == SCRVL_VAR_FLOAT && second.u.floatValue == 2.0f);
        } else if (!strcmp(operation, "equal")) {
            Scr_EvalEquality(&first, &second);
            assert(first.type == SCRVL_VAR_INTEGER && first.u.intValue == 1);
        } else {
            if (!strcmp(operation, "plus")) {
                Scr_EvalPlus(&first, &second);
                assert(SCR_VEC_PTR(first.u)[0] == 6.0f && SCR_VEC_PTR(first.u)[2] == 10.0f);
            } else {
                Scr_EvalMinus(&first, &second);
                assert(SCR_VEC_PTR(first.u)[0] == -4.0f && SCR_VEC_PTR(first.u)[2] == -4.0f);
            }
        }
    } else if (!strcmp(name, "clear_detaches_children")) {
        owner = AllocObject();
        first.type = SCRVL_VAR_STRING;
        first.u.stringValue = 101;
        watchedChildren[0] = GetVariable(owner, 23);
        SetNewVariableValue(watchedChildren[0], &first);
        watchedChildren[1] = GetVariable(owner, 24);
        SetNewVariableValue(watchedChildren[1], &first);
        ClearObjectInternal(owner);
        assert(detachedBeforeRelease && !FindNextSibling(owner));
    } else if (!strcmp(name, "thread_compare")) {
        ThreadDebugInfo a = {0}, b = {0};
        a.posSize = b.posSize = 1;
        a.pos[0] = programBytes + 0x20000;
        b.pos[0] = programBytes + 0x10000;
        assert(ThreadInfoCompare(&a, &b) == 0x10000);
        assert(ThreadInfoCompare(&b, &a) == -0x10000);
    } else if (!strcmp(name, "thread_dump")) {
        VariableStackBuffer *stack;
        unsigned int rootThread, childThread, value;
        owner = AllocObject();
        rootThread = AllocThread(owner);
        childThread = AllocChildThread(owner, rootThread);
        stack = (VariableStackBuffer *)MT_Alloc(sizeof(*stack) + 5, 2);
        stack->localId = childThread;
        stack->size = 1;
        stack->pos = programBytes + 0x20;
        stack->buf[0] = 7;
        value = 0x10;
        memcpy(stack->buf + 1, &value, 4);
        id = AllocValue();
        VG_STATUS(id) |= 10;
        VG_U32(id) = SCR_STACK_ENC(stack);
        Scr_DumpScriptThreads();
        assert(strstr(outputText, "count: 1, var usage: 2, endon usage: 0\n"));
        assert(strstr(outputText, "called from:\n"));
        assert(printedPositionCount == 2);
        assert(printedPositions[0] == programBytes + 0x20 && printedPositions[1] == programBytes + 0x10);
    } else {
        assert(!"unknown case");
    }
    puts(name);
    return 0;
}
