#include "common_types.h"
#include "imports.h"
#include "headers/PC/cgame_mp/cg_local.h"

extern void Scr_BeginLoadAnimTrees(int param);
extern void Com_SafeClientDObjFree(int entNum);
extern void XAnimFreeTree(void *tree, Free_t Free);
extern void MT_Free(void *ptr, int size);

void CGScr_LoadAnimTrees(void)
{
    Scr_BeginLoadAnimTrees(0);
}

void CG_SetDObjInfo(int iEntNum, int iEntType, struct XModel *pXModel)
{
    cg->iEntityLastType[iEntNum] = iEntType;
    cg->pEntityLastXModel[iEntNum] = pXModel;
}

qboolean CG_CheckDObjInfoMatches(int iEntNum, int iEntType, struct XModel *pXModel)
{
    if (cg->iEntityLastType[iEntNum] != iEntType)
        return 0;
    if (cg->pEntityLastXModel[iEntNum] != pXModel)
        return 0;
    return 1;
}

void CG_SafeDObjFree(int iEntNum)
{
    Com_SafeClientDObjFree(iEntNum);

    cg->iEntityLastType[iEntNum] = 0;
    cg->pEntityLastXModel[iEntNum] = 0;

    if (cg_entities[iEntNum].tree) {
        XAnimFreeTree((void *)cg_entities[iEntNum].tree, (Free_t)MT_Free);
        cg_entities[iEntNum].tree = 0;
    }
}

void CG_FreeClientDObjInfo(void)
{
    int i;
    for (i = 0; i < 0x40; i++) {
        Com_SafeClientDObjFree(i);

        cg->iEntityLastType[i] = 0;
        cg->pEntityLastXModel[i] = 0;

        if (cg_entities[i].tree) {
            XAnimFreeTree((void *)cg_entities[i].tree, (Free_t)MT_Free);
            cg_entities[i].tree = 0;
        }
    }
}

void CG_FreeEntityDObjInfo(void)
{
    int i;
    for (i = 0x40; i < 0x400; i++) {
        Com_SafeClientDObjFree(i);

        cg->iEntityLastType[i] = 0;
        cg->pEntityLastXModel[i] = 0;

        if (cg_entities[i].tree) {
            XAnimFreeTree((void *)cg_entities[i].tree, (Free_t)MT_Free);
            cg_entities[i].tree = 0;
        }
    }
}
