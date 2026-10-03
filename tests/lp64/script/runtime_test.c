#include "common_types.h"
#include <assert.h>
#include <stdlib.h>

/* Include the real VM to exercise its archive path without platform startup. */
#include "../../../src/PC/script/scr_vm.c"

unsigned char scrVmGlob[sizeof(scrVmGlob_t)] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[0x80380] __attribute__((aligned(8)));
scrMemTreePub_t scrMemTreePub;
scrVmPub_t scrVmPub;
scrVarPub_t scrVarPub;
scrCompilePub_t scrCompilePub;
scrAnimPub_t scrAnimPub;
jmp_buf g_script_error[33];
int g_script_error_level;
void *const imp_scrVarPub = &scrVarPub;
void *imp_scrCompilePub = &scrCompilePub;
void *imp_scrAnimPub = &scrAnimPub;

void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
unsigned int FindPrevSibling(unsigned int id) { (void)id; return 0; }
unsigned int GetParentLocalId(unsigned int id) { return id; }
void AddRefToObject(unsigned int id) { (void)id; }
void SL_Init(void) {}
void Var_Init(void) {}
void Scr_DumpScriptThreads(void) {}
void Scr_DumpScriptVariables(void) {}
unsigned int AllocValue(void) { return 1; }
void MT_Init(void);

int main(void)
{
    scrVmGlob_t *glob = (scrVmGlob_t *)scrVmGlob;
    VariableValue *values = scrVmPub.stack;
    const char program[] = {0, 1, 2, 3};
    unsigned int localId = 3;
    VariableStackBuffer *archived;
    uintptr_t result;

    assert(sizeof(VariableUnion) == 4);
    assert(sizeof(VariableValue) == 8);
    assert(offsetof(VariableStackBuffer, buf) == 15);
    assert(offsetof(scrVmGlob_t, dialog_error_message) == 16);
    assert(offsetof(scrVmGlob_t, loading) == 24);
    assert(offsetof(scrVmGlob_t, starttime) == 28);
    assert(offsetof(scrVmGlob_t, localVarsStack) == 32);

    MT_Init();
    scrVarPub.programBuffer = program;
    scrVmPub.localVars = glob->localVarsStack - 1;
    scrVmPub.function_count = 1;
    scrVmPub.function_frame = scrVmPub.function_frame_start + 1;
    assert(VM_CurrentFrameLocalCacheCount() == 0);
    values[1].type = VAR_INTEGER;
    values[1].u.intValue = 123;
    result = (uintptr_t)VM_CandidateSuspendCurrentStack(program + 1, 0,
                                                     values + 1, values, &localId);
    assert(result >= (uintptr_t)scrMemTreeGlob);
    assert(result < (uintptr_t)scrMemTreeGlob + 0x80000);
    archived = (VariableStackBuffer *)result;
    assert(archived->pos == program + 1);
    assert(archived->size == 1);
    assert(archived->buf[0] == VAR_INTEGER);
    assert(*(int *)(archived->buf + 1) == 123);
    {
        VariableUnion slot;
        slot.stackValue = SCR_STACK_ENC(archived);
        assert(SCR_STACK_PTR(slot) == archived);
    }
    MT_Free(archived, archived->bufLen);

    glob->dialog_error_message = "stale";
    glob->loading = 1;
    scrVarPub.bInited = 0;
    Scr_Init();
    assert(glob->dialog_error_message == NULL);
    assert(glob->loading == 0);
    assert(scrVmPub.localVars == glob->localVarsStack - 1);
    puts("VM archive and global layout passed");
    return 0;
}
