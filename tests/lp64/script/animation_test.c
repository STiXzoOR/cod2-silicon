#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../../src/PC/script/scr_animtree.c"

scrAnimPub_t scrAnimPub;
scrVarPub_t scrVarPub;
scrParserPub_t scrParserPub;
scrAnimGlob_t scrAnimGlob;
void *const imp_scrVarPub = &scrVarPub;
char g_EndPos;
static VariableUnion slot;
static const char *errorPos;
static int terminalError;
static const char *referencePos;
unsigned int Scr_CreateCanonicalFilename(const char *filename) { (void)filename; return 1; }
void SL_RemoveRefToString(unsigned int id) { (void)id; }
unsigned int FindObject(unsigned int id) { return id; }
unsigned long long Scr_EvalVariable(unsigned int id) { (void)id; return slot.codePosValue; }
unsigned int FindVariable(unsigned int parent, unsigned int name) { (void)parent; (void)name; return 1; }
VariableUnion *GetVariableValueAddress(unsigned int id) { (void)id; return &slot; }
unsigned int FindNextSibling(unsigned int id) { return id == 9 ? 1 : 0; }
unsigned int GetVariableName(unsigned int id) { (void)id; return 1; }
const char *SL_ConvertToString(unsigned int id) { (void)id; return "fixture"; }
char *va(const char *fmt, ...) { return (char *)fmt; }
int Scr_IsInOpcodeMemory(const char *pos) { return pos == referencePos; }
void CompileError2(const char *pos, const char *fmt, ...) { (void)fmt; errorPos = pos; }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; terminalError++; }

int main(void)
{
    char program[64] = {0};
    scr_anim_t external = {0};
    XAnim_s *anims = (XAnim_s *)(void *)&external;
    scrVarPub.programBuffer = program;
    scrAnimPub.xanim_lookup[1][3].anims = anims;
    assert(Scr_GetAnims(3) == anims);

    /* A cached XAnim allocation need not be near the program buffer. */
    anims = malloc(sizeof(*anims));
    assert(anims != NULL);
    slot.codePosValue = AnimRef_Enc((const char *)anims);
    assert(Scr_FindAnimTree("fixture").anims == anims);
    free(anims);

    referencePos = (char *)&external;
    slot.codePosValue = AnimRef_Enc(referencePos);
    Scr_CheckAnimsDefined(9, 1);
    assert(errorPos == referencePos && terminalError == 0);

    slot.codePosValue = AnimRef_Enc(program + 8);
    *(unsigned int *)(program + 8) = AnimRef_Enc((char *)&external);
    *(unsigned int *)&external = 0;
    ConnectScriptToAnim(9, 6, 1, 1, 3);
    assert(((scr_anim_t *)(program + 8))->index == 6);
    assert(((scr_anim_t *)(program + 8))->tree == 3);
    assert(external.index == 6 && external.tree == 3);
    assert(slot.codePosValue == 0);
    puts("animation lookup and encoded chain passed");
    return 0;
}
