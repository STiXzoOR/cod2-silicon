#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

extern void CMutex_CMutex(const CMutex *);
extern void ZN6CMutexD1Ev(void *);
extern void CThread_CThread(const CThread *);
extern int CThread_Run(const CThread *, void *);
extern void CThread_Stop(const CThread *);
extern Boolean CThread_IsRunning(const CThread *);
extern void ZN7CThreadD1Ev(CThread *);
extern void CCircularBuffer_CCircularBuffer(CCircularBuffer *);
extern void CCircularBuffer_Alloc(CCircularBuffer *, UInt32);
extern void CCircularBuffer_Write(const CCircularBuffer *, const void *, UInt32 *);
extern void *CCircularBuffer_ReadPtr(CCircularBuffer *, UInt32 *);
extern void ZN15CCircularBufferD1Ev(CCircularBuffer *);
extern DWORD GetCurrentThreadId(void);
static DWORD workerID;

static int worker(CThread *thread, void *argument)
{
    int *count = argument;
    workerID = GetCurrentThreadId();
    while (CThread_IsRunning(thread)) {
        __atomic_add_fetch(count, 1, __ATOMIC_RELAXED);
        usleep(100);
    }
    return 0;
}

static void *stopper(void *thread) { CThread_Stop(thread); return NULL; }

int main(void)
{
    CMutex mutex;
    CMutex_CMutex(&mutex);
    assert(pthread_mutex_lock(&mutex.mutex) == 0);
    assert(pthread_mutex_lock(&mutex.mutex) == 0);
    assert(pthread_mutex_unlock(&mutex.mutex) == 0);
    assert(pthread_mutex_unlock(&mutex.mutex) == 0);
    ZN6CMutexD1Ev(&mutex);
    CThread thread;
    int count = 0;
    void *vtable[] = {NULL, NULL, (void *)worker};
    CThread_CThread(&thread);
    assert(thread.vtable[0] && thread.vtable[1]);
    DWORD mainID = GetCurrentThreadId();
    assert(mainID && mainID == GetCurrentThreadId());
    thread.vtable = vtable;
    for (int i = 0; i < 20; ++i) {
        int before = __atomic_load_n(&count, __ATOMIC_RELAXED);
        assert(CThread_Run(&thread, &count) == 0);
        usleep(2000);
        pthread_t stopA, stopB;
        assert(pthread_create(&stopA, NULL, stopper, &thread) == 0);
        assert(pthread_create(&stopB, NULL, stopper, &thread) == 0);
        pthread_join(stopA, NULL); pthread_join(stopB, NULL);
        assert(!CThread_IsRunning(&thread) && !thread.joinable);
        assert(workerID && workerID != mainID);
        assert(__atomic_load_n(&count, __ATOMIC_RELAXED) > before);
        int stopped = count;
        usleep(1000);
        assert(count == stopped);
    }
    ZN7CThreadD1Ev(&thread);
    CThread base;
    CThread_CThread(&base);
    ((void (*)(CThread *))base.vtable[0])(&base);
    CThread *heapThread = calloc(1, sizeof(*heapThread));
    assert(heapThread);
    CThread_CThread(heapThread);
    ((void (*)(CThread *))heapThread->vtable[1])(heapThread);
    CCircularBuffer ring;
    CCircularBuffer_CCircularBuffer(&ring);
    CCircularBuffer_Alloc(&ring, 31);
    unsigned int written = 0, read = 0;
    while (read < 10000) {
        unsigned char input[13];
        UInt32 size = 13;
        for (UInt32 i = 0; i < size; ++i)
            input[i] = (unsigned char)(written + i);
        CCircularBuffer_Write(&ring, input, &size);
        written += size;
        size = 7;
        const unsigned char *output = CCircularBuffer_ReadPtr(&ring, &size);
        for (UInt32 i = 0; i < size; ++i)
            assert(output[i] == (unsigned char)(read++));
    }
    ZN15CCircularBufferD1Ev(&ring);
    puts("threads: recursive mutex, native IDs, virtual destructors and 20 dual-stop restarts passed; ring: 10000 wrapped bytes passed");
    return 0;
}
