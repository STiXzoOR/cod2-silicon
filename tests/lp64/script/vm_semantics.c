#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include <time.h>

static unsigned int test_time;
static int VM_TestClock(clockid_t id, struct timespec *now)
{
    assert(id == CLOCK_MONOTONIC);
    now->tv_sec = test_time / 1000;
    now->tv_nsec = (test_time % 1000) * 1000000;
    return 0;
}

#define clock_gettime VM_TestClock
#include VM_SEMANTICS_SOURCE
#undef clock_gettime

unsigned char scrVmGlob[sizeof(scrVmGlob_t)] __attribute__((aligned(8)));
char g_EndPos;
scrVmPub_t scrVmPub;
scrVarPub_t scrVarPub;
scrCompilePub_t scrCompilePub;
jmp_buf g_script_error[33];
int g_script_error_level;
void *const imp_scrVarPub = &scrVarPub;
void *imp_scrCompilePub = &scrCompilePub;

static unsigned int parents[16];
static unsigned int killed[16], killed_count, removed_objects, removed_values;
static unsigned int printed, printed_pos, dumped_threads, dumped_variables;
static int observed_classnum, observed_entnum;
static jmp_buf fatal_env;
static int expect_fatal;

void Com_Printf(const char *fmt, ...) { (void)fmt; printed++; }
void Com_Error(int code, const char *fmt, ...)
{
    (void)code;
    (void)fmt;
    assert(expect_fatal);
    longjmp(fatal_env, 1);
}
unsigned int GetSafeParentLocalId(unsigned int id) { return parents[id]; }
void Scr_KillThread(unsigned int id) { killed[killed_count++] = id; }
void RemoveRefToObject(unsigned int id) { (void)id; removed_objects++; }
void RemoveRefToValue(int type, VariableUnion value)
{
    (void)value;
    assert(type != 7);
    removed_values++;
}
void AddRefToObject(unsigned int id) { assert(id == 9); }
unsigned int FindEntityId(int entnum, int classnum)
{
    observed_entnum = entnum;
    observed_classnum = classnum;
    return 9;
}
void dbg_end_probe(int probe) { (void)probe; }
void Scr_PrintPrevCodePos(print_msg_type_t type, const char *pos, unsigned int index)
{
    (void)type;
    (void)pos;
    (void)index;
    printed_pos++;
}
void Scr_DumpScriptThreads(void) { dumped_threads++; }
void Scr_DumpScriptVariables(void) { dumped_variables++; }

typedef struct TestState {
    const char *pos;
    unsigned int localId, localVarCount, thread_count;
    VariableValue *top, *startTop;
} TestState;

/* Compile the production opcode cases with their real helpers. The remaining
 * interpreter opcodes have unrelated engine link dependencies. */
static unsigned int TestOpcode(TestState *state, ScrVmOpcode opcode)
{
    const char *pos = state->pos;
    unsigned int localId = state->localId;
    unsigned int localVarCount = state->localVarCount;
    unsigned int thread_count = state->thread_count;
    unsigned int resultLocalId = 0;
    VariableValue *top = state->top;
    VariableValue *startTop = state->startTop;
    switch (opcode) {
#include "vm_semantics_cases.inc"
    default:
        abort();
    }
    state->pos = pos;
    state->localId = localId;
    state->localVarCount = localVarCount;
    state->thread_count = thread_count;
    state->top = top;
    state->startTop = startTop;
    return 0;
}

static const char loop[] __attribute__((aligned(2))) = {0, VMOP_JumpBack, 3, 0};
static const char *const loopOpcode = loop + 1;
static const char caller[] = {VMOP_End};

static TestState Setup(void)
{
    TestState state;
    scrVmGlob_t *glob = (scrVmGlob_t *)scrVmGlob;
    memset(&scrVmPub, 0, sizeof(scrVmPub));
    memset(&scrVarPub, 0, sizeof(scrVarPub));
    memset(&scrCompilePub, 0, sizeof(scrCompilePub));
    memset(scrVmGlob, 0, sizeof(scrVmGlob));
    memset(parents, 0, sizeof(parents));
    killed_count = removed_objects = removed_values = 0;
    printed = printed_pos = dumped_threads = dumped_variables = 0;
    expect_fatal = 0;
    g_script_error_level = 1;
    test_time = 10000;
    glob->starttime = 7500;
    scrVmPub.localVars = glob->localVarsStack + 1;
    scrVmPub.maxstack = scrVmPub.stack + 2047;
    scrVmPub.function_count = 1;
    scrVmPub.function_frame = scrVmPub.function_frame_start + 1;
    scrVmPub.stack[0].type = 7;
    scrVmPub.stack[1].type = VAR_INTEGER;
    scrVmPub.stack[1].u.intValue = 11;
    scrVmPub.stack[2].type = VAR_INTEGER;
    scrVmPub.stack[2].u.intValue = 22;
    scrVmPub.stack[3].type = VAR_STRING;
    scrVmPub.stack[3].u.stringValue = 123;
    state.pos = loopOpcode + 1;
    state.localId = 3;
    state.localVarCount = 2;
    state.thread_count = 0;
    state.top = scrVmPub.stack + 3;
    state.startTop = scrVmPub.stack;
    return state;
}

static unsigned int TestReturn(TestState *state)
{
    unsigned int result = 0;
    int finished = VM_CandidateHandleReturn(&state->pos, &state->localId,
                                           &state->localVarCount, &state->top,
                                           &state->startTop, &result,
                                           &state->thread_count);
    return finished ? result : 0;
}

static void SetThreadCaller(TestState *state)
{
    function_frame_t *frame = scrVmPub.function_frame_start + 1;
    scrVmPub.function_count = 2;
    scrVmPub.function_frame++;
    scrVmPub.localVars++;
    state->thread_count = 1;
    state->startTop = scrVmPub.stack + 3;
    state->top = scrVmPub.stack + 4;
    state->top->type = VAR_STRING;
    state->top->u.stringValue = 123;
    state->startTop->type = 7;
    frame->fs.pos = caller;
    frame->fs.localId = 5;
    frame->fs.localVarCount = 1;
    frame->fs.top = state->startTop;
    frame->fs.startTop = scrVmPub.stack;
    frame->topType = 8;
}

int main(int argc, char **argv)
{
    TestState state = Setup();
    scrVmGlob_t *glob = (scrVmGlob_t *)scrVmGlob;
    const char *name;
    assert(argc == 2);
    name = argv[1];
    if (!strcmp(name, "root_return")) {
        assert(TestReturn(&state) == 3);
        assert(scrVmPub.stack[1].type == VAR_STRING);
        assert(scrVmPub.stack[1].u.stringValue == 123);
        assert(removed_values == 2);
        assert(killed_count == 1 && killed[0] == 3);
        assert(scrVmPub.top == scrVmPub.stack);
        assert(scrVmPub.function_count == 0 && g_script_error_level == 0);
    } else if (!strcmp(name, "thread_return")) {
        SetThreadCaller(&state);
        assert(TestReturn(&state) == 0);
        assert(state.top == scrVmPub.stack + 4);
        assert(state.top->type == VAR_STRING && state.top->u.stringValue == 123);
        assert(state.localId == 5 && state.pos == caller);
        assert(state.thread_count == 0 && removed_objects == 1);
        assert(scrVmPub.stack[3].type == 8);
    } else if (!strcmp(name, "child_return")) {
        parents[3] = 5;
        scrVmPub.function_count = 2;
        scrVmPub.function_frame++;
        scrVmPub.function_frame_start[1].fs.pos = caller;
        scrVmPub.function_frame_start[1].fs.localVarCount = 1;
        assert(TestReturn(&state) == 0);
        assert(state.top == scrVmPub.stack);
        assert(state.top->type == VAR_STRING && state.top->u.stringValue == 123);
        assert(state.localId == 5 && state.pos == caller);
        assert(scrVmPub.function_count == 1 && removed_objects == 1);
    } else if (!strcmp(name, "object")) {
        const char operands[] = {2, 0, 0, 0, 17, 0, 0, 0};
        state.pos = operands;
        assert(TestOpcode(&state, VMOP_Object) == 0);
        assert(observed_classnum == 2 && observed_entnum == 17);
        assert(state.pos == operands + sizeof(operands));
        assert(state.top->type == VAR_POINTER && state.top->u.pointerValue == 9);
    } else if (!strcmp(name, "ring")) {
        assert((uintptr_t)loopOpcode > 0xffffffffu);
        dbg_op_ring_idx = 0;
        VM_DebugRecordOpcode(loopOpcode, VMOP_JumpBack);
        assert((uintptr_t)dbg_op_ring[0] == (uintptr_t)loopOpcode);
        assert(dbg_op_ring[1] == VMOP_JumpBack);
    } else if (!strcmp(name, "reset_clock")) {
        Scr_ResetTimeout();
        assert(glob->starttime == test_time);
    } else if (!strcmp(name, "unaligned")) {
        const char operands[] __attribute__((aligned(4))) = {
            0, 0x34, 0x12, 0, 0, 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0x40};
        const char *pos = operands + 1;
        assert(VM_ReadU16(&pos) == 0x1234);
        pos = operands + 5;
        assert(VM_ReadI32(&pos) == 0x12345678);
        pos = operands + 9;
        assert(VM_ReadF32(&pos) == 2.0f);
    } else if (!strcmp(name, "timeout_before")) {
        test_time--;
        assert(TestOpcode(&state, VMOP_JumpBack) == 0);
        assert(state.pos == loopOpcode);
        assert(killed_count == 0 && printed == 0);
    } else if (!strcmp(name, "timeout_loading")) {
        glob->loading = 1;
        scrVmPub.abort_on_error = 1;
        assert(TestOpcode(&state, VMOP_JumpBack) == 0);
        assert(state.pos == loopOpcode);
        assert(printed == 1 && printed_pos == 1 && killed_count == 0);
        assert(glob->starttime == test_time);
        assert(scrVmPub.terminal_error == 0 && dumped_threads == 0);
    } else if (!strcmp(name, "timeout_terminal")) {
        scrVmPub.abort_on_error = 1;
        scrVarPub.evaluate = 1;
        expect_fatal = 1;
        if (setjmp(fatal_env) == 0) {
            TestOpcode(&state, VMOP_JumpBack);
            assert(!"terminal timeout must raise Com_Error");
        }
        assert(scrVmPub.terminal_error == 1);
        assert(dumped_threads == 1 && dumped_variables == 1);
        assert(!strcmp(scrVarPub.error_message, "potential infinite loop in script"));
        assert(killed_count == 0);
    } else if (!strcmp(name, "timeout_terminal_vm")) {
        scrVmPub.abort_on_error = 1;
        if (setjmp(g_script_error[1]) == 0) {
            TestOpcode(&state, VMOP_JumpBack);
            assert(!"terminal timeout must enter VM error path");
        }
        assert(scrVmPub.terminal_error == 1);
        assert(dumped_threads == 1 && dumped_variables == 1);
        assert(!strcmp(scrVarPub.error_message, "potential infinite loop in script"));
        assert(killed_count == 0);
    } else if (!strcmp(name, "timeout_thread")) {
        SetThreadCaller(&state);
        assert(TestOpcode(&state, VMOP_JumpBack) == 0);
        assert(killed_count == 1 && killed[0] == 3);
        assert(state.pos == caller && state.localId == 5);
        assert(state.thread_count == 0 && state.top == scrVmPub.stack + 4);
        assert(state.top->type == VAR_UNDEFINED && scrVmPub.stack[3].type == 8);
        assert(scrVmPub.function_count == 1 && g_script_error_level == 1);
    } else {
        if (!strcmp(name, "timeout_parents")) {
            parents[3] = 5;
            scrVmPub.function_count = 2;
            scrVmPub.function_frame++;
            scrVmPub.localVars++;
            state.top = scrVmPub.stack + 4;
            scrVmPub.stack[3].type = 7;
            scrVmPub.stack[4].type = VAR_INTEGER;
            scrVmPub.function_frame_start[1].fs.localVarCount = 1;
        } else if (!strcmp(name, "timeout_wrap")) {
            test_time = 1000;
            glob->starttime = 0xffffffffu - 1499;
        } else {
            assert(!strcmp(name, "timeout_kill"));
        }
        assert(TestOpcode(&state, VMOP_JumpBack) == (parents[3] ? 5u : 3u));
        assert(killed_count == (parents[3] ? 2u : 1u));
        assert(killed[0] == 3);
        if (parents[3]) assert(killed[1] == 5);
        assert(scrVmPub.top == scrVmPub.stack);
        assert(scrVmPub.stack[1].type == VAR_UNDEFINED);
        assert(scrVmPub.function_count == 0 && g_script_error_level == 0);
        assert(scrVmPub.localVars == glob->localVarsStack - 1);
        assert(glob->starttime == test_time);
        assert(printed == 1 && printed_pos == 1);
        assert(removed_values == 3);
    }
    puts(name);
    return 0;
}
