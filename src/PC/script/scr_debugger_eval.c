#include "cod2_feature_config.h"

#if COD2_FEATURE_SCRIPT_DEBUGGER

#    include "common_types.h"

#    include <string.h>

extern void *imp_scrCompilePub;
extern void *imp_scrVarPub;
extern void *imp_scrVmPub;

extern void ScriptParse(sval_t *parseData, int user);

extern void Scr_ClearErrorMessage(void);
extern void Scr_Error(const char *msg);
extern const char *va(const char *fmt, ...);

extern void SL_ShutdownSystem(int user);
extern unsigned int SL_GetString_(const char *str, int user, int flags);
extern const char *SL_ConvertToString(unsigned int stringValue);

extern unsigned long long Scr_EvalVariable(unsigned int id);
extern unsigned long long Scr_EvalVariableField(unsigned int id);
extern unsigned int Scr_EvalArrayRef(unsigned int parentId);
extern void Scr_EvalArray(VariableValue *value, VariableValue *index);
extern unsigned long long Scr_FindVariableField(unsigned int parentId, unsigned int name);
extern unsigned int FindVariable(unsigned int parentId, unsigned int name);
extern unsigned int FindObjectVariable(unsigned int parentId, unsigned int id);
extern unsigned int FindObject(unsigned int id);
extern unsigned int FindNextSibling(unsigned int id);
extern unsigned int FindEntityId(int entnum, int classnum);
extern VariableUnion *GetVariableValueAddress(unsigned int id);
extern int GetVarType(unsigned int id);
extern unsigned int Scr_GetSelf(unsigned int threadId);
extern unsigned int GetStartLocalId(unsigned int threadId);
extern unsigned int GetSafeParentLocalId(unsigned int threadId);
extern Bool IsFieldObject(unsigned int id);
extern unsigned int Scr_GetEntityIdRef(unsigned int entId);

extern void AddRefToValue(int type, VariableUnion u);
extern void RemoveRefToValue(int type, VariableUnion u);
extern void AddRefToObject(unsigned int id);
extern void RemoveRefToObject(unsigned int id);

extern void Scr_CastBool(VariableValue *value);
extern void Scr_CastVector(VariableValue *value);
extern const float *Scr_AllocVector(const float *v);

extern const char *var_typename[24];

extern void Com_sprintf(char *dst, int size, const char *fmt, ...);
extern void I_strncpyz(char *dest, const char *src, int destsize);

extern unsigned int AllocThread(unsigned int self);
extern void Scr_KillThread(unsigned int parentId);
extern void Scr_ResetTimeout(void);

typedef struct ScrDebugEvalGlob_s {
    int errored;
    char inCondition;
    char inExec;
} ScrDebugEvalGlob_s;

static ScrDebugEvalGlob_s s_evalGlob;

typedef struct ScriptExpression_t {
    uintptr_t expr;
    uintptr_t debugExpr;
} ScriptExpression_t;

static void EvalError(VariableValue_s *out, const char *msg)
{
    if (out)
        out->type = 0;
    s_evalGlob.errored = 1;
    Scr_Error(msg);
}

static void Scr_EvalExpression(sval_u expr, unsigned int threadId, VariableValue_s *out);
static void Scr_EvalPrimitiveExpression(sval_u expr, unsigned int threadId, VariableValue_s *out);
static void Scr_EvalVariableExpression(sval_u expr, unsigned int threadId, VariableValue_s *out);

#    define SVAL_NODE(s) ((sval_u *)(uintptr_t)(s).node)

static unsigned int Scr_EvalFieldObject(unsigned int fieldName, VariableValue_s *out)
{
    int type = out->type;
    int objType = GetVarType(out->u.intValue);
    unsigned int objId = out->u.intValue;

    if (type == 1 && objType < 22) {

        unsigned int ref = FindVariable(objId, fieldName);
        (void)ref;
        return objId;
    }

    if ((unsigned int)(type - 1) > 3) {
        EvalError(NULL, va("%s is not a field object", var_typename[objType]));
        return 0;
    }
    RemoveRefToObject(objId);
    EvalError(NULL, va("%s is not a field object", var_typename[objType]));
    return 0;
}

void Scr_EvalFieldVariable(unsigned int fieldName, VariableValue_s *out, unsigned int objectId)
{
    if (!IsFieldObject(objectId)) {
        out->type = 0;
        Scr_Error("not a field object");
        return;
    }
    {
        unsigned long long packed = Scr_FindVariableField(objectId, fieldName);
        out->u.intValue = (unsigned int)packed;
        out->type = (int)(packed >> 32);
    }
    if (out->type != 0)
        return;
    if (FindVariable(objectId, fieldName) == 0)
        Scr_Error("not defined");
}

void Scr_EvalArrayVariable(unsigned int objectId, VariableValue_s *out)
{
    VariableValue_s base;

    base.type = 1;
    base.u.intValue = objectId;
    AddRefToObject(objectId);

    if (s_evalGlob.errored) {
        RemoveRefToValue(out->type, out->u);
        out->type = 0;
        RemoveRefToValue(1, base.u);
        return;
    }
    Scr_EvalArray(&base, out);
    if (s_evalGlob.errored) {
        RemoveRefToValue(out->type, out->u);
        out->type = 0;
        RemoveRefToValue(1, base.u);
    }
}

int Scr_GetArrayIndexValue(unsigned int id)
{
    if (id == 0)
        return (int)(id & 0xFFFF);
    if (id < 0x10000)
        return (int)(id & 0xFFFF);
    if (id >= 0x1FFFE)
        return (int)(id - 0x800000);
    return (int)(id - 0x10000);
}

static void Scr_EvalExpression(sval_u expr, unsigned int threadId, VariableValue_s *out)
{
    sval_u *node = SVAL_NODE(expr);
    if ((unsigned int)(node[0].type - 6) > 75) {
        out->type = 0;
        Scr_Error("invalid expression");
        return;
    }
    Scr_EvalPrimitiveExpression(node[1], threadId, out);
}

static void Scr_EvalPrimitiveExpression(sval_u expr, unsigned int threadId, VariableValue_s *out)
{
    sval_u *node = SVAL_NODE(expr);
    int type = node[0].type;

    if ((unsigned int)(type - 7) > 76)
        return;

    switch (type) {
    case 7:
        out->u.intValue = node[1].intValue;
        out->type = 6;
        return;
    case 8:
        out->u.floatValue = node[1].floatValue;
        out->type = 5;
        return;
    case 9:
        out->u.intValue = -node[1].intValue;
        out->type = 6;
        return;
    case 10:
        out->u.floatValue = -node[1].floatValue;
        out->type = 5;
        return;
    case 11:
        out->u.stringValue = SL_GetString_(SL_ConvertToString(node[1].stringValue), 2, 13);
        out->type = 2;
        return;
    case 12:
        out->u.stringValue = SL_GetString_(SL_ConvertToString(node[1].stringValue), 3, 13);
        out->type = 3;
        return;

    case 17:
        Scr_EvalVariableExpression(node[1], threadId, out);
        return;

    case 19:
        EvalError(out, "cannot call a function from a watch expression");
        return;

    case 31:
        out->type = 0;
        return;

    case 32:
    {
        unsigned int self = Scr_GetSelf(threadId);
        out->u.intValue = self;
        out->type = 1;
        AddRefToObject(self);
        return;
    }

    case 34:
    {
        unsigned int levelId = scrVarPub->levelId;
        out->u.intValue = levelId;
        out->type = 1;
        AddRefToObject(levelId);
        return;
    }

    case 35:
    {
        unsigned int gameId = scrVarPub->gameId;
        unsigned long long packed = Scr_EvalVariable(gameId);
        out->u.intValue = (unsigned int)packed;
        out->type = (int)(packed >> 32);
        return;
    }

    case 36:
    {
        unsigned int animId = scrVarPub->animId;
        out->u.intValue = animId;
        out->type = 1;
        AddRefToObject(animId);
        return;
    }

    case 46:
        Scr_EvalExpression(node[1], threadId, out);
        return;

    case 72:
        out->u.intValue = 0;
        out->type = 6;
        return;
    case 73:
        out->u.intValue = 1;
        out->type = 6;
        return;

    case 66:
    case 83:
        EvalError(out, "invalid expression");
        return;

    default:

        return;
    }
}

static void Scr_EvalVariableExpression(sval_u expr, unsigned int threadId, VariableValue_s *out)
{
    sval_u *node = SVAL_NODE(expr);
    int type = node[0].type;

    if ((unsigned int)(type - 3) > 80)
        return;

    switch (type) {
    case 4:
    {
        unsigned int name = node[1].stringValue;
        unsigned int levelId = scrVarPub->levelId;
        unsigned int id = FindVariable(levelId, name);
        if (id == 0) {
            out->type = 0;
            Scr_Error(va("%s doesn't exist", SL_ConvertToString(name)));
            return;
        }
        {
            unsigned long long packed = Scr_EvalVariable(id);
            out->u.intValue = (unsigned int)packed;
            out->type = (int)(packed >> 32);
        }
        return;
    }

    case 5:
        out->type = 1;
        out->u.intValue = node[1].intValue;
        AddRefToObject(node[1].intValue);
        return;

    case 3:
    default:
        out->type = 0;
        Scr_Error("cannot evaluate this variable in a watch");
        return;
    }
}

void Scr_GetValueString(unsigned int threadId, VariableValue_s *v, int outLen, char *out)
{
    int type;
    (void)threadId;

    if (!out || outLen <= 0)
        return;

    type = v->type;

    switch (type) {
    case 0:
        I_strncpyz(out, "undefined", outLen);
        return;
    case 6:
        Com_sprintf(out, outLen, "%i", v->u.intValue);
        return;
    case 5:
        Com_sprintf(out, outLen, "%g", v->u.floatValue);
        return;
    case 2:
        Com_sprintf(out, outLen, "\"%s\"", SL_ConvertToString(v->u.stringValue));
        return;
    case 3:
        Com_sprintf(out, outLen, "&\"%s\"", SL_ConvertToString(v->u.stringValue));
        return;
    case 4:
        Com_sprintf(out, outLen, "(%g, %g, %g)",
                    SCR_VEC_PTR(v->u)[0], SCR_VEC_PTR(v->u)[1], SCR_VEC_PTR(v->u)[2]);
        return;
    default:

        if (type >= 0 && type < 24)
            Com_sprintf(out, outLen, "%s", var_typename[type]);
        else
            out[0] = '\0';
        return;
    }
}

static uintptr_t Scr_DebugNode0(int type)
{

    extern uintptr_t node1(int type, uintptr_t val1);
    return node1(type, 0);
}

void Scr_CompileTextInternal(const char *text, ScriptExpression_t *out)
{
    scrCompilePub_t *cp = (scrCompilePub_t *)imp_scrCompilePub;
    sval_t parseData;
    sval_u *root;

    if (!text || !*text) {
        out->expr = Scr_DebugNode0(81);
        return;
    }

    cp->in_ptr = "+";
    cp->parseBuf = text;
    cp->developer_statement = 3;

    parseData.node = 0;
    ScriptParse(&parseData, 2);

    if (scrVarPub->error_message) {
        Scr_ClearErrorMessage();
        out->expr = Scr_DebugNode0(83);
        SL_ShutdownSystem(2);
        return;
    }

    root = SVAL_NODE(parseData);
    if (!root) {
        out->expr = Scr_DebugNode0(83);
        SL_ShutdownSystem(2);
        return;
    }

    {
        sval_u inner = root[1];
        sval_u *innerNode = SVAL_NODE(inner);
        int innerType = innerNode ? innerNode[0].type : 83;

        if (innerType == 65)
        {
            out->expr = inner.node;
        } else
        {
            out->expr = inner.node;
        }
    }

    out->debugExpr = 0;
    SL_ShutdownSystem(2);
}

void Scr_CompileText(const char *text, ScriptExpression_t *out)
{
    out->debugExpr = 0;
    Scr_CompileTextInternal(text, out);
    if (out->debugExpr == 0)
        out->debugExpr = 0;
}

int Scr_EvalScriptExpression(ScriptExpression_t *expr, unsigned int threadId,
                             VariableValue_s *out, int inCondition, int inExec)
{
    sval_u node;

    s_evalGlob.inCondition = (char)inCondition;
    s_evalGlob.inExec = (char)inExec;
    s_evalGlob.errored = 0;

    out->type = 0;
    out->u.intValue = 0;

    if (!expr || expr->expr == 0)
        return 0;

    node.node = expr->expr;
    Scr_EvalExpression(node, threadId, out);
    return s_evalGlob.errored;
}

int Scr_RefScriptExpression(ScriptExpression_t *expr)
{
    (void)expr;
    return 0;
}

void Scr_FreeDebugExprValue(sval_u expr)
{
    sval_u *node = SVAL_NODE(expr);
    if (!node)
        return;
    if ((unsigned int)(node[0].type - 4) > 76)
        return;
    if (node[1].intValue != 0) {
        RemoveRefToObject(node[1].intValue);
        node[1].node = 0;
    }
}

void Scr_FreeDebugExpr(unsigned int handle)
{
    int *node = (int *)(uintptr_t)handle;
    extern void Z_FreeInternal(void *ptr);
    while (node) {
        sval_u v;
        int *next = (int *)(uintptr_t)node[0];
        v.node = (intptr_t)(node + 1);
        Scr_FreeDebugExprValue(v);
        Z_FreeInternal(node);
        node = next;
    }
}

void Scr_ExecCode(const char *compiled, unsigned int threadId)
{
    (void)threadId;
    if (!compiled)
        return;
    Scr_ResetTimeout();

}

int Scr_RefToVariable(unsigned int value)
{
    (void)value;
    return 0;
}

int Scr_FindAllVariableField(unsigned int parentId, unsigned int *outList)
{
    int count = 0;
    unsigned int child;
    int objType;

    objType = GetVarType(parentId) & 0x1F;
    if (objType < 15 || objType > 22)
        return 0;

    child = FindNextSibling(FindObject(parentId));
    while (child) {
        VariableUnion *va_addr = GetVariableValueAddress(child);
        (void)va_addr;
        if (outList)
            outList[count] = child;
        ++count;
        child = FindNextSibling(child);
    }
    return count;
}

static int Scr_FindAllThreadsInternal(unsigned int objectId, unsigned int parentId,
                                      int count, int checkSelf, unsigned int *outList)
{
    unsigned int sib = FindNextSibling(FindObject(parentId));
    while (sib) {
        if (GetVarType(sib) == 10)
        {
            VariableUnion *addr = GetVariableValueAddress(sib);
            unsigned int threadId = addr ? addr->pointerValue : 0;
            while (threadId) {
                if (!checkSelf || objectId == Scr_GetSelf(threadId)) {
                    if (outList)
                        outList[count] = threadId;
                    ++count;
                    break;
                }
                threadId = GetSafeParentLocalId(threadId);
            }
        }
        sib = FindNextSibling(sib);
    }
    return count;
}

int Scr_FindAllThreads(unsigned int objectId, unsigned int *outList, unsigned int threadId)
{
    int count = 0;
    unsigned int sib;
    unsigned int waitVar;

    if (threadId && objectId == Scr_GetSelf(threadId)) {
        if (outList)
            outList[count] = threadId;
        ++count;
    }

    sib = FindNextSibling(scrVarPub->timeArrayId);
    while (sib) {
        count = Scr_FindAllThreadsInternal(objectId, sib, count, 1, outList);
        sib = FindNextSibling(sib);
    }

    waitVar = FindVariable(objectId, 0x1FFFE);
    if (waitVar) {
        sib = FindNextSibling(FindObject(waitVar));
        while (sib) {
            count = Scr_FindAllThreadsInternal(objectId, sib, count, 0, outList);
            sib = FindNextSibling(sib);
        }
    }
    return count;
}

unsigned int Scr_GetThreadPos(unsigned int objectId)
{
    (void)objectId;
    return 0;
}

int Scr_IsEndonThread(unsigned int objectId)
{
    if (GetVarType(objectId) != 0x10)
        return 0;
    if (GetStartLocalId(objectId) != objectId)
        return 0;

    {
        unsigned int waitVar = FindVariable(objectId, 0x1FFFE);
        return waitVar != 0;
    }
}

#endif
