#include "common_types.h"
#include <assert.h>
#include <setjmp.h>

extern qboolean Scr_SetObjectField(unsigned int, int, int);
extern void Scr_GetGenericField(byte *, fieldtype_t, int);
gentity_t g_entities[1024];
void * const imp_g_entities = g_entities;
static gclient_t client;
static vec3_t value = {1069, 2761, 87};
static int integer = 63, links;
static int added, addedType;
static unsigned int addedValue;
static jmp_buf readonly;

void Scr_GetVector(unsigned int index, float *out) { assert(!index); memcpy(out, value, sizeof(value)); }
int Scr_GetInt(unsigned int index) { assert(!index); return integer; }
float Scr_GetFloat(unsigned int index) { return 0; }
unsigned int Scr_GetConstStringIncludeNull(unsigned int index) { return 0; }
void Scr_SetString(scr_string_t *out, unsigned int id) { *out = id; }
scr_entref_t Scr_GetEntityRef(unsigned int index) { scr_entref_t ref = {0}; return ref; }
void Scr_ParamError(unsigned int index, const char *message) { assert(0); }
void Scr_SetHudElemField(int entnum, int offset) { assert(0); }
void Scr_SetClientField(gclient_t *c, int offset) { assert(0); }
void Scr_AddInt(int v) { ++added; addedType = F_INT; addedValue = v; }
void Scr_AddFloat(float v) { ++added; addedType = F_FLOAT; }
void Scr_AddString(const char *v) { ++added; addedType = F_LSTRING; }
void Scr_AddConstString(unsigned int v) { ++added; addedType = F_STRING; addedValue = v; }
void Scr_AddVector(const float *v) { ++added; addedType = F_VECTOR; }
void Scr_AddObject(unsigned int v) { ++added; addedType = F_OBJECT; addedValue = v; }
void Scr_AddEntityNum(int v, int classnum) { ++added; addedType = F_ENTITY; addedValue = v; }
void Scr_AddUndefined(void) { ++added; addedType = -1; }
const char *SV_GetConfigstringConst(int index) { return "model"; }
void SV_LinkEntity(gentity_t *ent) { assert(ent == &g_entities[617]); ++links; }
void Scr_Error(const char *message) {
    assert(!strcmp(message, "Tried to set a read only entity field"));
    longjmp(readonly, 1);
}

int main(void)
{
    gentity_t *ent = &g_entities[617];
    ent->r.linked = 1;
    ent->s.pos.trTime = 91;
    ent->s.pos.trDelta[0] = 12;
    assert(Scr_SetObjectField(0, 617, 1));
    assert(!memcmp(ent->r.currentOrigin, value, sizeof(value)));
    assert(!memcmp(ent->s.pos.trBase, value, sizeof(value)));
    assert(!ent->s.pos.trTime && !ent->s.pos.trDelta[0] && links == 1);
    ent->r.linked = 0;
    assert(Scr_SetObjectField(0, 617, 1) && links == 1);
    assert(Scr_SetObjectField(0, 617, 9));
    assert(!memcmp(ent->r.currentAngles, value, sizeof(value)));
    assert(!memcmp(ent->s.apos.trBase, value, sizeof(value)));
    assert(Scr_SetObjectField(0, 617, 7));
    assert(ent->health == integer && ent->maxHealth == integer);
    ent->client = &client;
    integer = 41;
    assert(Scr_SetObjectField(0, 617, 7));
    assert(ent->health == integer && client.ps.stats[0] == integer && ent->maxHealth == 63);
    assert(Scr_SetObjectField(0, 617, 6) && ent->count == integer);
    const int immutable[] = {0, 2, 3};
    for (unsigned int i = 0; i < sizeof(immutable) / sizeof(*immutable); ++i) {
        if (!setjmp(readonly)) {
            Scr_SetObjectField(0, 617, immutable[i]);
            assert(!"read-only field accepted assignment");
        }
    }
    scr_string_t string = 0;
    Scr_GetGenericField((byte *)&string, F_STRING, 0);
    assert(!added);
    string = 17;
    Scr_GetGenericField((byte *)&string, F_STRING, 0);
    assert(added == 1 && addedType == F_STRING && addedValue == 17);
    gentity_t *reference = NULL;
    Scr_GetGenericField((byte *)&reference, F_ENTITY, 0);
    assert(added == 1);
    reference = ent;
    ent->s.number = 617;
    Scr_GetGenericField((byte *)&reference, F_ENTITY, 0);
    assert(added == 2 && addedType == F_ENTITY && addedValue == 617);
    puts("entity fields: networked transforms, health and read-only callbacks pass");
}
