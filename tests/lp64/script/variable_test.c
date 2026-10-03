#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include "../../../src/PC/script/scr_variable.c"

unsigned char scrVarGlob[1048608] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[0x80380] __attribute__((aligned(8)));
scrMemTreePub_t scrMemTreePub;
scrVarPub_t scrVarPub;
void *const imp_scrVarPub = &scrVarPub;
char g_EndPos;
scr_classStruct_t g_classMap[5];
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(errorParm_t code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Scr_Error(const char *msg) { (void)msg; abort(); }
void Scr_TerminalError(const char *msg) { (void)msg; abort(); }
void SL_AddRefToString(unsigned int id) { (void)id; }
void SL_RemoveRefToString(unsigned int id) { (void)id; }
void Scr_CancelNotifyList(unsigned int id) { (void)id; }
void VM_CancelNotify(unsigned int a, unsigned int b) { (void)a; (void)b; }
void MT_Init(void);
unsigned int Scr_GetStringUsage(void);

int main(void)
{
    const float input[3] = {1.0f, 2.0f, 3.0f};
    const float *vector;
    unsigned int src, dst, id;
    VariableValue value;

    MT_Init();
    Var_Init();
    src = Scr_AllocArray();
    dst = Scr_AllocArray();
    vector = Scr_AllocVector(input);
    value.type = SCRVL_VAR_VECTOR;
    value.u.vectorValue = SCR_VEC_ENC(vector);
    id = GetArrayVariable(src, 0);
    SetNewVariableValue(id, &value);
    CopyArray(src, dst);
    id = GetArrayVariable(dst, 0);
    assert(SCR_VEC_PTR(*GetVariableValueAddress(id)) == vector);
    assert(*(unsigned short *)((const byte *)vector - 4) == 1);
    ClearObject(src);
    assert(SCR_VEC_PTR(*GetVariableValueAddress(id))[2] == 3.0f);
    ClearObject(dst);
    assert(Scr_GetStringUsage() == 0);
    puts("encoded vector array copy passed");
    return 0;
}
