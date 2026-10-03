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
    uint64_t elapsed, intervals, events, total = 0;
    unsigned int sample;
    if (time < stats->time || count < stats->count) {
        Cod2x_MouseStatsReset(stats, time, count);
        return 1;
    }
    elapsed = time - stats->time;
    if (elapsed < 100000000ULL)
        return 0;
    intervals = elapsed / 100000000ULL;
    events = count - stats->count;
    stats->time = time;
    stats->count = count;
    if (!events) {
        memset(stats->samples, 0, sizeof(stats->samples));
    } else {
        /* Spread delayed reads across elapsed buckets so a stalled frame cannot
           invent a high mouse refresh rate. Normal 100 ms reads match CoD2x. */
        double perInterval = (double)events * 100000000.0 / (double)elapsed;
        sample = perInterval >= INT_MAX ? INT_MAX : (unsigned int)(perInterval + 0.5);
        if (intervals > 10) intervals = 10;
        for (uint64_t i = 0; i < intervals; ++i) {
            stats->samples[stats->index] = sample;
            stats->index = (stats->index + 1) % 10;
        }
    }
    for (unsigned int i = 0; i < 10; ++i)
        total += stats->samples[i];
    stats->hz = total > INT_MAX ? INT_MAX : (int)total;
    if (stats->hz > stats->maxHz)
        stats->maxHz = stats->hz;
    return 1;
}
#endif
