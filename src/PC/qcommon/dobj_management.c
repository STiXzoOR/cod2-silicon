#include "common_types.h"
#include "imports.h"

extern void DObjSkelClear(struct DObj_s *dobj);
extern void DObjCreate(DObjModel_s *models, int numModels, struct XAnimTree_s *tree, void *buf, int entNum);
extern void DObjFree(struct DObj_s *dobj);
extern void Com_Error(int level, const char *fmt, ...);
extern void Com_Memset(void *dest, int val, int count);

static qboolean g_bDObjInited;
static int com_lastDObjIndex;
static short int serverObjMap[1024];
static short int clientObjMap[1152];
static int objFreeCount;
static Bool objAlloced[2048];
static objBufEntry objBuf[2048];

struct DObj_s *Com_GetClientDObj(int handle, int localClientNum);
struct DObj_s *Com_GetServerDObj(int handle);
void Com_ClientDObjClearAllSkel(void);
void Com_ServerDObjCreate(DObjModel_s *dobjModels, int numModels, struct XAnimTree_s *tree, int handle);
void Com_SafeClientDObjFree(int handle);
void Com_SafeServerDObjFree(int handle);
void Com_InitDObj(void);
void Com_ShutdownDObj(void);
void Com_AbortDObj(void);
void Com_ClientDObjCreate(DObjModel_s *dobjModels, int numModels, struct XAnimTree_s *tree, int handle);

static inline __attribute__((always_inline)) int Com_AllocDObj(void)
{
    int idx = com_lastDObjIndex + 1;

    while (idx <= 2047) {
        if (!objAlloced[idx]) {
            com_lastDObjIndex = idx;
            objAlloced[idx] = 1;
            objFreeCount--;
            return idx;
        }
        idx++;
    }

    if (com_lastDObjIndex > 0) {
        for (idx = 1; idx <= com_lastDObjIndex; idx++) {
            if (!objAlloced[idx]) {
                com_lastDObjIndex = idx;
                objAlloced[idx] = 1;
                objFreeCount--;
                return idx;
            }
        }
    }

    return 0;
}

struct DObj_s *Com_GetClientDObj(int handle, int localClientNum)
{
    int index = localClientNum * 1152 + handle;
    short mapVal = clientObjMap[index];
    if (mapVal == 0)
        return NULL;
    return (struct DObj_s *)&objBuf[mapVal];
}

struct DObj_s *Com_GetServerDObj(int handle)
{
    short mapVal = serverObjMap[handle];
    if (mapVal == 0)
        return NULL;
    return (struct DObj_s *)&objBuf[mapVal];
}

void Com_ClientDObjClearAllSkel(void)
{
    int i;
    for (i = 0; i < 1152; i++) {
        short mapVal = clientObjMap[i];
        int idx = mapVal;
        if (mapVal != 0) {
            DObjSkelClear((struct DObj_s *)&objBuf[idx]);
        }
    }
}

void Com_ServerDObjCreate(DObjModel_s *dobjModels, int numModels, struct XAnimTree_s *tree, int handle)
{
    unsigned short nm = (unsigned short)numModels;
    int idx = Com_AllocDObj();

    serverObjMap[handle] = (short)idx;
    DObjCreate(dobjModels, nm, tree, (void *)&objBuf[idx], (unsigned short)(handle + 1));

    if (objFreeCount == 0)
        Com_Error(1, "exceeded maximum number of DObjs");
}

void Com_SafeClientDObjFree(int handle)
{
    short mapVal = clientObjMap[handle];
    int idx = mapVal;
    if (mapVal == 0)
        return;

    clientObjMap[handle] = 0;
    objAlloced[idx] = 0;
    objFreeCount++;
    DObjFree((struct DObj_s *)&objBuf[idx]);
}

void Com_SafeServerDObjFree(int handle)
{
    short mapVal = serverObjMap[handle];
    int idx = mapVal;
    if (mapVal == 0)
        return;

    serverObjMap[handle] = 0;
    objAlloced[idx] = 0;
    objFreeCount++;
    DObjFree((struct DObj_s *)&objBuf[idx]);
}

void Com_InitDObj(void)
{
    Com_Memset(objAlloced, 0, 0x800);
    objFreeCount = 2047;
    Com_Memset(clientObjMap, 0, 0x900);
    Com_Memset(serverObjMap, 0, 0x800);
    com_lastDObjIndex = 1;
    g_bDObjInited = 1;
}

void Com_ShutdownDObj(void)
{
    g_bDObjInited = 0;
}

void Com_AbortDObj(void)
{
    g_bDObjInited = 0;
}

void Com_ClientDObjCreate(DObjModel_s *dobjModels, int numModels, struct XAnimTree_s *tree, int handle)
{
    unsigned short nm = (unsigned short)numModels;
    int idx = Com_AllocDObj();

    clientObjMap[handle] = (short)idx;
    DObjCreate(dobjModels, nm, tree, (void *)&objBuf[idx], 0);

    if (objFreeCount == 0)
        Com_Error(1, "exceeded maximum number of DObjs");
}
