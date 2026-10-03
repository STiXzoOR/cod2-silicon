/* Exercise native guard pages and diagnostic stack capture without engine blobs. */
#include "common_types.h"
#include "platform/macos_system.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include "PC/universal/com_memory.c"
#include <malloc/malloc.h>

void Com_Printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
void Com_Memset(void *dest, const int value, int size) { memset(dest, value, (size_t)size); }
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Sys_OutOfMemErrorInternal(const char *file, int line) { (void)file; (void)line; abort(); }

int main(void)
{
    const size_t size = 64;
    byte *buffer = Z_MallocInternal(size);
    void *frames[4];
    struct rlimit coreLimit = {0, 0};
    int status, count;
    pid_t child;
    if (!buffer)
        return 1;
    for (size_t i = 0; i < size; i++) {
        if (buffer[i])
            return 2;
    }
    buffer[0] = 1;
    buffer[size - 1] = 2;
    count = MacSystem_CaptureStack(frames, 4, 0);
    if (count <= 0 || !frames[0] || MacSystem_ImageOffset(frames[0]) == (uintptr_t)frames[0])
        return 3;
    setrlimit(RLIMIT_CORE, &coreLimit);
    child = fork();
    if (child == 0) {
        volatile byte *guard = buffer + size;
        *guard = 3;
        _exit(4);
    }
    if (child < 0 || waitpid(child, &status, 0) < 0 || !WIFSIGNALED(status) ||
        (WTERMSIG(status) != SIGBUS && WTERMSIG(status) != SIGSEGV))
        return 5;
    Z_FreeInternal(buffer);
    buffer = (byte *)CopyStringInternal("unguarded string allocation");
    if (!buffer || strcmp((char *)buffer, "unguarded string allocation"))
        return 8;
    {
        malloc_statistics_t beforeFree, afterFree;
        malloc_zone_t *zone = malloc_zone_from_ptr(buffer);
        malloc_zone_statistics(zone, &beforeFree);
        Z_FreeInternal(buffer);
        malloc_zone_statistics(zone, &afterFree);
        if (beforeFree.blocks_in_use != afterFree.blocks_in_use + 1) {
            printf("FAIL unguarded string free before=%u after=%u\n", beforeFree.blocks_in_use, afterFree.blocks_in_use);
            return 9;
        }
    }

    /* Hunk tracking stores real native return addresses and detects the canary write. */
    setenv("COD2_HUNKGUARD", "1", 1);
    s_hunkTotal = 4096;
    s_hunkData = malloc(s_hunkTotal);
    if (!s_hunkData)
        return 6;
    buffer = Hunk_AllocInternal(size);
    buffer[size] = 0;
    hunk_guard_check("native probe");
    if (hg_count != 1 || *(unsigned long long *)hg_rec[0].end != HG_CANARY || !hg_rec[0].ra[0])
        return 7;
    free(s_hunkData);
    printf("PASS native guard page fault, mmap free, captured stack and hunk canary repair (page=%d frames=%d)\n",
           getpagesize(), count);
    return 0;
}
