#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include "../../../src/PC/game_mp/g_scr_main_mp.c"

extern gentity_t testEntity;
static gentity_t *addedEntity;
static int addedBool;
static const char *typeLabel;
void Scr_AddEntity(gentity_t *ent) { addedEntity = ent; }
unsigned int Scr_AddBool(int value) { addedBool = value; return (unsigned int)value; }
int Scr_GetType(unsigned int index) { (void)index; return 0; }
const char *Scr_GetString(unsigned int index) { (void)index; return "alias"; }
float Scr_GetFloat(unsigned int index) { (void)index; return 0; }
int Scr_GetInt(int index) { (void)index; return 0; }
unsigned int Scr_AddInt(int value) { return (unsigned int)value; }
unsigned int Scr_ParamError(unsigned int index, const char *msg) { (void)index; (void)msg; return 0; }
const char *va(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    typeLabel = va_arg(args, const char *);
    va_end(args);
    return fmt;
}

int main(void)
{
    GScr_AddTestClient();
    assert(addedEntity == &testEntity);
    ScrCmd_SoundExists();
    assert(addedBool == 1);
    GScr_CastInt();
    assert(strcmp(typeLabel, "undefined") == 0);
    puts("GSC pointer marshalling passed");
    return 0;
}
