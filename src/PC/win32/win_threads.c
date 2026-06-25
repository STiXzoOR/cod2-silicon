#include "common_types.h"
#include "imports.h"

extern DWORD GetCurrentThreadId(void);
extern void Com_InitThreadData(int index);

static void *g_threadValues[5];
static DWORD threadId[1];

void Sys_InitMainThread(void)
{
    threadId[0] = GetCurrentThreadId();
    Com_InitThreadData(0);
}

Bool Sys_IsMainThread(void)
{
    return GetCurrentThreadId() == threadId[0];
}

void Sys_SetValue(int valueIndex, void *data)
{
    g_threadValues[valueIndex] = data;
}

void *Sys_GetValue(int valueIndex)
{
    return g_threadValues[valueIndex];
}
