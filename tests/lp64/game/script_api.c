#include "common_types.h"
#include <assert.h>
#include <stdarg.h>

extern void GScr_AddTestClient(void);
extern void GScr_CastInt(void);
extern void GScr_GetPartName(void);
extern void Scr_GetWeaponModel(void);

static gentity_t entity;
static XModel model;
static XModelParts modelParts;
static XBoneHierarchy hierarchy;
static unsigned short names[2] = { 11, 29 };
static WeaponDef weapon;
static const char typeName[] = "entity";
static char modelName[] = "xmodel/test";
static gentity_t *added;
static const char *errorType;
static const char *addedString;
static unsigned int addedConst;

gentity_t *SV_AddTestClient(void) { return &entity; }
void Scr_AddEntity(gentity_t *ent) { added = ent; }
int Scr_GetType(unsigned int arg) { return 1; }
const char *Scr_GetTypeName(unsigned int arg) { return typeName; }
const char *va(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    errorType = va_arg(args, const char *);
    va_end(args);
    return "error";
}
void Scr_ParamError(unsigned int arg, const char *error) {}
int Scr_GetInt(unsigned int arg) { return 1; }
float Scr_GetFloat(unsigned int arg) { return 0; }
unsigned int Scr_AddInt(int value) { return 0; }
const char *Scr_GetString(unsigned int arg) { return "test"; }
XModel *SV_XModelGet(const char *name) { return &model; }
int XModelNumBones(const XModel *m) { return 2; }
/* Model agent has not repaired this retail-era signature yet. */
int XModelBoneNames(XModel *m) { return (int)(uintptr_t)names; }
unsigned int Scr_AddConstString(unsigned int name) { addedConst = name; return 0; }
int G_GetWeaponIndexForName(const char *name) { return 1; }
WeaponDef *BG_GetWeaponDef(int index) { return &weapon; }
unsigned int Scr_AddString(const char *str) { addedString = str; return 0; }
int I_stricmp(const char *a, const char *b) { return strcmp(a, b); }
void Com_Printf(const char *fmt, ...) {}

int main(void)
{
    model.parts = (void (*)())&modelParts;
    modelParts.hierarchy = &hierarchy;
    hierarchy.names = names;
    weapon.szWorldModel = modelName;
    assert((uintptr_t)&entity > UINT32_MAX);
    GScr_AddTestClient();
    assert(added == &entity);
    GScr_CastInt();
    assert(errorType == typeName);
    GScr_GetPartName();
    assert(addedConst == 29);
    Scr_GetWeaponModel();
    assert(addedString == modelName);
    puts("game script API: entity/type/model pointers pass");
}
