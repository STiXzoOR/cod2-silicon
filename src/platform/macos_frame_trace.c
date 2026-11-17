/* Frame pacing trace. With COD2_FRAME_TRACE_FILE set, every presented frame adds a
 * line to that file: the time Present returned and how long Present took, both in
 * nanoseconds on the monotonic clock. Without the variable it does nothing. The
 * pacing measurements compare renderers with it. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static unsigned long long presentBegin;

void MacFrameTrace_PresentBegin(void)
{
    presentBegin = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

void MacFrameTrace_Mark(void)
{
    static FILE *file;
    static int tried;
    unsigned long long now;
    if (!tried) {
        const char *path = getenv("COD2_FRAME_TRACE_FILE");
        tried = 1;
        if (path)
            file = fopen(path, "w");
    }
    if (!file)
        return;
    now = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    fprintf(file, "%llu %llu\n", now, presentBegin ? now - presentBegin : 0ull);
}
