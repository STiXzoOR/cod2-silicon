#include "common_types.h"
#include <errno.h>
#include <stdlib.h>

static int CThread_Execute(CThread *self, void *argument) { (void)self; (void)argument; return 0; }
void ZN7CThreadD1Ev(CThread *self);
void ZN7CThreadD0Ev(CThread *self);
static void *threadVtable[] = { (void *)ZN7CThreadD1Ev, (void *)ZN7CThreadD0Ev, (void *)CThread_Execute };

void CMutex_CMutex(const CMutex *object)
{
    CMutex *self = (CMutex *)object;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&self->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}
void ZN6CMutexD1Ev(void *object) { pthread_mutex_destroy(&((CMutex *)object)->mutex); }
void CThread_CThread(const CThread *object)
{
    CThread *self = (CThread *)object;
    self->vtable = threadVtable;
    self->argument = NULL;
    self->running = self->joinable = 0;
    CMutex_CMutex(&self->mutex);
    pthread_mutex_init(&self->lifecycleMutex, NULL);
}
static void *CThread_ExecuteProc(void *argument)
{
    CThread *self = argument;
    ((int (*)(CThread *, void *))self->vtable[2])(self, self->argument);
    __atomic_store_n(&self->running, 0, __ATOMIC_RELEASE);
    return NULL;
}
int CThread_Run(const CThread *object, void *argument)
{
    CThread *self = (CThread *)object;
    pthread_mutex_lock(&self->lifecycleMutex);
    if (__atomic_load_n(&self->running, __ATOMIC_ACQUIRE)) {
        pthread_mutex_unlock(&self->lifecycleMutex);
        return 0;
    }
    if (self->joinable) {
        pthread_join(self->thread, NULL);
        self->joinable = 0;
    }
    self->argument = argument;
    __atomic_store_n(&self->running, 1, __ATOMIC_RELEASE);
    int error = pthread_create(&self->thread, NULL, CThread_ExecuteProc, self);
    if (error)
        __atomic_store_n(&self->running, 0, __ATOMIC_RELEASE);
    else
        self->joinable = 1;
    pthread_mutex_unlock(&self->lifecycleMutex);
    return error;
}
void CThread_Stop(const CThread *object)
{
    CThread *self = (CThread *)object;
    pthread_mutex_lock(&self->lifecycleMutex);
    __atomic_store_n(&self->running, 0, __ATOMIC_RELEASE);
    if (self->joinable && !pthread_equal(pthread_self(), self->thread)) {
        pthread_join(self->thread, NULL);
        self->joinable = 0;
    }
    pthread_mutex_unlock(&self->lifecycleMutex);
}
Boolean CThread_IsRunning(const CThread *self) { return __atomic_load_n(&self->running, __ATOMIC_ACQUIRE) != 0; }
void ZN7CThreadD2Ev(CThread *self) { CThread_Stop(self); pthread_mutex_destroy(&self->lifecycleMutex); ZN6CMutexD1Ev(&self->mutex); }
void ZN7CThreadD1Ev(CThread *self) { ZN7CThreadD2Ev(self); }
void ZN7CThreadD0Ev(CThread *self) { ZN7CThreadD2Ev(self); free(self); }
void StMutexLock_StMutexLock(const StMutexLock *object, CMutex *mutex)
{
    StMutexLock *self = (StMutexLock *)object;
    self->mutex = mutex;
    self->locked = pthread_mutex_lock(&mutex->mutex) == 0;
}
void ZN11StMutexLockD1Ev(void *object)
{
    StMutexLock *self = object;
    if (self->locked) { pthread_mutex_unlock(&self->mutex->mutex); self->locked = 0; }
}
void StThreadLock_StThreadLock(const StThreadLock *object, CThread *thread)
{
    StThreadLock *self = (StThreadLock *)object;
    self->thread = thread;
    self->locked = pthread_mutex_lock(&thread->mutex.mutex) == 0;
}
void ZN12StThreadLockD1Ev(void *object)
{
    StThreadLock *self = object;
    if (self->locked) { pthread_mutex_unlock(&self->thread->mutex.mutex); self->locked = 0; }
}
