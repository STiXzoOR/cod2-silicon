#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_native_mouse.h"
#include <limits.h>
#include <string.h>

void Cod2x_MouseStatsReset(Cod2xMouseStats *stats, uint64_t time, uint64_t count)
{
    memset(stats, 0, sizeof(*stats));
    stats->time = time;
    stats->count = count;
}

int Cod2x_MouseStatsUpdate(Cod2xMouseStats *stats, uint64_t time, uint64_t count)
{
    uint64_t events, total = 0;
    if (time < stats->time || count < stats->count) {
        Cod2x_MouseStatsReset(stats, time, count);
        return 1;
    }
    if (time - stats->time < 100000000ULL)
        return 0;
    events = count - stats->count;
    stats->time = time;
    stats->count = count;
    /* CoD2x rinput.cpp: every event received since the last update goes into
       one bucket, however late that update is, and a quiet update clears the
       meter. */
    if (!events) {
        memset(stats->samples, 0, sizeof(stats->samples));
    } else {
        stats->samples[stats->index] = events > INT_MAX ? INT_MAX : (unsigned int)events;
        stats->index = (stats->index + 1) % 10;
    }
    for (unsigned int i = 0; i < 10; ++i)
        total += stats->samples[i];
    stats->hz = total > INT_MAX ? INT_MAX : (int)total;
    if (stats->hz > stats->maxHz)
        stats->maxHz = stats->hz;
    return 1;
}
#endif
