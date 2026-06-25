#include "common_types.h"
#include "imports.h"

#include <pthread.h>
#include <unistd.h>

extern void __ZdlPv(void *);

static void *CThread_ExecuteProc(void *inArg);

void ZN6CMutexD1Ev(void *mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
}

int CThread_Run(const CThread *_this, void *inArg)
{
    CThread *self = (CThread *)_this;
    char *p = (char *)self;

    if (*(char *)(p + 0x38))
        return 0;

    *(void **)(p + 8) = inArg;
    *(char *)(p + 0x38) = 1;

    int err = pthread_create((pthread_t *)(p + 4), NULL, CThread_ExecuteProc, self);
    if (err != 0) {
        *(char *)(p + 0x38) = 0;
    }
    return err;
}

void CThread_Stop(const CThread *_this)
{
    char *p = (char *)_this;
    *(char *)(p + 0x38) = 0;
    usleep(100000);
}

Boolean CThread_IsRunning(const CThread *_this)
{
    const char *p = (const char *)_this;
    return *(unsigned char *)(p + 0x38);
}

static void *CThread_ExecuteProc(void *inArg)
{
    char *self = (char *)inArg;
    void **vtable = *(void ***)self;

    typedef int (*ExecuteFn)(void *, void *);
    ExecuteFn execute = (ExecuteFn)vtable[2];
    execute(self, *(void **)(self + 8));
    *(char *)(self + 0x38) = 0;
    return NULL;
}

void CMutex_CMutex(const CMutex *_this)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init((pthread_mutex_t *)_this, &attr);
    pthread_mutexattr_destroy(&attr);
}

void CThread_CThread(const CThread *_this)
{
    char *p = (char *)_this;
    *(void **)(p + 0) = (void *)0x332348;
    *(int *)(p + 4) = 0;
    *(void **)(p + 8) = NULL;
    CMutex_CMutex((const CMutex *)(p + 0xc));
    *(char *)(p + 0x38) = 0;
}

void ZN11StMutexLockD1Ev(void *_this)
{
    char *p = (char *)_this;
    if (!*(char *)(p + 4))
        return;
    pthread_mutex_unlock(*(pthread_mutex_t **)p);
}

void ZN12StThreadLockD1Ev(void *_this)
{
    char *p = (char *)_this;
    if (!*(char *)(p + 4))
        return;
    pthread_mutex_unlock((pthread_mutex_t *)(*(char **)p + 0xc));
}

void ZN7CThreadD2Ev(void)
{

}

void ZN7CThreadD1Ev(void)
{

}

void ZN7CThreadD0Ev(void)
{

}

void StMutexLock_StMutexLock(const StMutexLock *_this, CMutex *inMutex)
{
    char *p = (char *)_this;
    *(CMutex **)(p + 0) = inMutex;
    int err = pthread_mutex_lock((pthread_mutex_t *)inMutex);
    *(char *)(p + 4) = (err == 0) ? 1 : 0;
}

void StThreadLock_StThreadLock(const StThreadLock *_this, CThread *inThread)
{
    char *p = (char *)_this;
    *(CThread **)(p + 0) = inThread;
    int err = pthread_mutex_lock((pthread_mutex_t *)((char *)inThread + 0xc));
    *(char *)(p + 4) = (err == 0) ? 1 : 0;
}
