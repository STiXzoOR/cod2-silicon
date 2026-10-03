#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include "../../../src/PC/script/scr_stringlist.c"

unsigned char scrStringGlob[65664] __attribute__((aligned(8)));
unsigned char scrMemTreeGlob[0x80380] __attribute__((aligned(8)));
scrMemTreePub_t scrMemTreePub;
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Scr_TerminalError(const char *msg) { (void)msg; abort(); }
void Scr_DumpScriptThreads(void) {}
void Scr_DumpScriptVariables(void) {}
char *va(const char *fmt, ...) { return (char *)fmt; }
void *Z_VirtualAllocInternal(int size) { return calloc(1, (size_t)size); }
void Z_VirtualFreeInternal(void *ptr) { free(ptr); }
unsigned int Scr_GetStringUsage(void);

int main(void)
{
    scrStringGlob_t *glob = (scrStringGlob_t *)scrStringGlob;
    unsigned int id, again;
    HashEntry *marker = &glob->hashTable[9];

    assert(sizeof(HashEntry) == 4);
    assert(offsetof(scrStringGlob_t, inited) == 65536);
    assert(offsetof(scrStringGlob_t, nextFreeEntry) == 65544);
    SG_RESTART = marker;
    assert(glob->nextFreeEntry == marker);
    SG_RESTART = NULL;
    SL_Init();
    id = SL_GetString_("lp64-script", 0, 1);
    again = SL_GetString_("lp64-script", 0, 1);
    assert(id != 0 && id == again);
    assert(strcmp(SL_ConvertToString(id), "lp64-script") == 0);
    SL_RemoveRefToString(id);
    assert(strcmp(SL_ConvertToString(again), "lp64-script") == 0);
    SL_RemoveRefToString(again);
    /* Native 1.3 semantics reclaim the final reference and unlink the name. */
    assert(SL_FindString("lp64-script") == 0);
    for (int i = 0; i < 100; ++i) {
        id = SL_GetString_("lp64-script", 0, 1);
        assert(id && strcmp(SL_ConvertToString(id), "lp64-script") == 0);
        assert(SL_FindString("lp64-script") == id);
        SL_RemoveRefToString(id);
        assert(SL_FindString("lp64-script") == 0);
    }
    puts("string pointer layout, live identity and final-reference reclamation passed");
    return 0;
}
