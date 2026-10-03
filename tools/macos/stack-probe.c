/* In-process fallback when external macOS profiler attachment is unavailable.
 * Optional main-thread CPU samples; never enabled for FPS comparisons. */
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <ptrauth.h>

static uintptr_t stacks[32768][16];
static volatile sig_atomic_t sampleCount;
static uintptr_t stackLow, stackHigh;
static const char *filename;

static void sampleCPU(int signal, siginfo_t *info, void *context)
{
    (void)signal;
    (void)info;
    ucontext_t *uc = context;
    const _STRUCT_ARM_THREAD_STATE64 *state = &uc->uc_mcontext->__ss;
    uintptr_t sp = __darwin_arm_thread_state64_get_sp(*state);
    uintptr_t fp = __darwin_arm_thread_state64_get_fp(*state);
    if (sp < stackLow || sp >= stackHigh || sampleCount >= 32768)
        return;
    uintptr_t *sample = stacks[sampleCount++];
    sample[0] = __darwin_arm_thread_state64_get_pc(*state);
    for (int depth = 1; depth < 16; ++depth) {
        if ((fp & 15) || fp < sp || fp < stackLow || fp > stackHigh - 16)
            break;
        const uintptr_t *frame = (const uintptr_t *)fp;
        sample[depth] = (uintptr_t)ptrauth_strip((void *)frame[1], ptrauth_key_return_address);
        if (frame[0] <= fp)
            break;
        fp = frame[0];
    }
}

void MacProbe_BeginCPU(void)
{
    filename = getenv("COD2_CPU_PROFILE");
    if (!filename)
        return;
    stackHigh = (uintptr_t)pthread_get_stackaddr_np(pthread_self());
    stackLow = stackHigh - pthread_get_stacksize_np(pthread_self());
    struct sigaction action = { 0 };
    action.sa_sigaction = sampleCPU;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGPROF, &action, NULL);
    struct itimerval timer = { { 0, 1000 }, { 0, 1000 } };
    setitimer(ITIMER_PROF, &timer, NULL);
}

void MacProbe_SaveCPU(void)
{
    if (!filename)
        return;
    struct itimerval timer = { 0 };
    setitimer(ITIMER_PROF, &timer, NULL);
    FILE *stream = fopen(filename, "w");
    if (!stream)
        return;
    fprintf(stream, "sample,depth,pc,image_base,image,symbol\n");
    for (int i = 0; i < sampleCount; ++i) {
        for (int depth = 0; depth < 16 && stacks[i][depth]; ++depth) {
            Dl_info info = { 0 };
            dladdr((void *)stacks[i][depth], &info);
            fprintf(stream, "%d,%d,0x%lx,0x%lx,%s,%s\n", i, depth,
                    (unsigned long)stacks[i][depth], (unsigned long)info.dli_fbase,
                    info.dli_fname ? info.dli_fname : "", info.dli_sname ? info.dli_sname : "");
        }
    }
    fclose(stream);
}
