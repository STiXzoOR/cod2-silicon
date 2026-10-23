#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/PC/script/scr_animtree.c"
extern void Scr_EndLoadAnimTrees(void);

scrAnimPub_t scrAnimPub;
scrVarPub_t scrVarPub;
scrParserPub_t scrParserPub;
scrAnimGlob_t scrAnimGlob;
scrCompilePub_t scrCompilePub;
void *const imp_scrVarPub = &scrVarPub;
void *imp_scrCompilePub = &scrCompilePub;
void *imp_scrParserPub = &scrParserPub;
void *imp_scrAnimPub = &scrAnimPub;
char g_EndPos;
static VariableUnion slot;
static const char *errorFormat;
static char program[64];

unsigned int Scr_CreateCanonicalFilename(const char *filename) { (void)filename; return 1; }
void SL_RemoveRefToString(unsigned int id) { (void)id; }
void SL_ShutdownSystem(unsigned int user) { (void)user; }
void ClearObject(unsigned int id) { (void)id; }
void RemoveRefToObject(unsigned int id) { (void)id; }
void *Hunk_AllocLowInternal(int size) { (void)size; return program; }
unsigned int FindObject(unsigned int id) { return id; }
unsigned long long Scr_EvalVariable(unsigned int id) { (void)id; return slot.codePosValue; }
unsigned int FindVariable(unsigned int parent, unsigned int name) { (void)parent; (void)name; return 1; }
VariableUnion *GetVariableValueAddress(unsigned int id) { (void)id; return &slot; }
unsigned int FindNextSibling(unsigned int id) { return id == 9 ? 1 : 0; }
unsigned int GetVariableName(unsigned int id) { (void)id; return 1; }
const char *SL_ConvertToString(unsigned int id) { (void)id; return "fixture"; }
char *va(const char *fmt, ...) { return (char *)fmt; }
void CompileError2(const char *pos, const char *fmt, ...) { (void)pos; (void)fmt; abort(); }
void Com_Error(int code, const char *fmt, ...) { (void)code; errorFormat = fmt; }

int main(int argc, char **argv)
{
    scr_anim_t external = {0};
    assert(argc == 2);
    scrVarPub.programBuffer = program;
    scrCompilePub.programLen = sizeof(program);
    if (!strcmp(argv[1], "anim-error")) {
        slot.codePosValue = AnimRef_Enc((const char *)&external);
        Scr_CheckAnimsDefined(9, 1);
        assert(errorFormat && (unsigned char)errorFormat[0] == 0x15);
        assert(strcmp(errorFormat + 1, "%s") == 0);
    } else if (!strcmp(argv[1], "anim-lifetime")) {
        /* Mac pointers have no accumulated per-process registration limit. */
        for (unsigned int i = 0; i < 17000; i++) {
            slot.codePosValue = AnimRef_Enc((const char *)&external);
            assert(slot.codePosValue != 0);
            assert(AnimRef_Dec(slot.codePosValue) == (const char *)&external);
            Scr_EndLoadAnimTrees();
        }
    } else {
        abort();
    }
    puts("F1 animation services check passed");
    return 0;
}
