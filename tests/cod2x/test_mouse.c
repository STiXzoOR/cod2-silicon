#include "cod2x_native_mouse.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void)
{
    Cod2xMouseStats stats;
    Cod2x_MouseStatsReset(&stats, 0, 0);
    for (unsigned int i = 1; i <= 10; ++i) {
        assert(Cod2x_MouseStatsUpdate(&stats, i * 100000000ULL, i * 100));
        assert(stats.hz == (int)i * 100);
    }
    assert(stats.maxHz == 1000);
    assert(!Cod2x_MouseStatsUpdate(&stats, 1050000000ULL, 1100));
    assert(stats.hz == 1000);
    assert(Cod2x_MouseStatsUpdate(&stats, 1100000000ULL, 1000));
    assert(stats.hz == 0 && stats.maxHz == 1000);
    assert(Cod2x_MouseStatsUpdate(&stats, 1200000000ULL, 1100));
    assert(stats.hz == 100);
    Cod2x_MouseStatsReset(&stats, 1200000000ULL, 1100);
    assert(stats.hz == 0 && stats.maxHz == 0);
    assert(Cod2x_MouseStatsUpdate(&stats, 2200000000ULL, 2100));
    assert(stats.hz == 1000); /* ten elapsed buckets, rather than a 10x spike */
    assert(Cod2x_MouseStatsUpdate(&stats, 2300000000ULL, UINT64_MAX));
    assert(stats.hz == INT_MAX && stats.maxHz == INT_MAX);
    assert(Cod2x_MouseStatsUpdate(&stats, 2400000000ULL, 0)); /* device counter reset */
    assert(stats.hz == 0);
    puts("cod2x mouse rate tests passed");
    return 0;
}
