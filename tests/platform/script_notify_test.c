#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include "PC/script/scr_vm.c"

scrVarPub_t scrVarPub;
void *const imp_scrVarPub = &scrVarPub;
scrCompilePub_t scrCompilePub;
void *imp_scrCompilePub = &scrCompilePub;
unsigned char scrVmGlob[sizeof(scrVmGlob_t)] __attribute__((aligned(8)));
scrVmPub_t scrVmPub;
jmp_buf g_script_error[33];
int g_script_error_level;
static unsigned int stored, storedNotifyName;

void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
Bool IsFieldObject(unsigned int id) { assert(id == 77); return 1; }
void AddRefToObject(unsigned int id) { assert(id == 13); }
void RemoveRefToObject(unsigned int id) { assert(id == 17); }
unsigned int AllocThread(unsigned int id) { assert(id == 13); return 17; }
unsigned int GetVariable(unsigned int parent, unsigned int name)
{
    if (parent == 77) { assert(name == 0x1fffe); return 11; }
    assert(parent == 11 && name == 9); return 12;
}
unsigned int GetArray(unsigned int id) { return id; }
unsigned int GetObjectVariable(unsigned int parent, unsigned int id)
{
    if (parent == 12) { assert(id == 17); return 99; }
    assert(parent == scrVarPub.pauseArrayId && id == 13); return 43;
}
unsigned int GetNewObjectVariable(unsigned int parent, unsigned int id)
{ assert(parent == 43 && id == 17); return 44; }
void SetNewVariableValue(unsigned int id, VariableValue *value)
{ assert(id == 44 && value->type == VAR_POINTER); stored = value->u.pointerValue; }
void Scr_SetThreadNotifyName(unsigned int id, unsigned int name)
{ assert(id == 17); storedNotifyName = name; }

int main(void)
{
    VariableValue values[3] = {0}, *top = &values[2];
    scrVarPub.levelId = 41;
    scrVarPub.pauseArrayId = 42;
    values[1].type = VAR_STRING;
    values[1].u.stringValue = 9;
    values[2].type = VAR_POINTER;
    values[2].u.pointerValue = 77;
    VM_CandidateHandleEndOnCallback(13, &top);
    assert(top == values && stored == 77 && storedNotifyName == 9);
    puts("script endon owner is stored in the pause array: passed");
}
