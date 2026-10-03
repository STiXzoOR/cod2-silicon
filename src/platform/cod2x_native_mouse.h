#ifndef COD2X_NATIVE_MOUSE_H
#define COD2X_NATIVE_MOUSE_H
#include <stdint.h>

typedef struct {
    uint64_t time, count;
    unsigned int samples[10];
    unsigned int index;
    int hz, maxHz;
} Cod2xMouseStats;

void Cod2x_MouseStatsReset(Cod2xMouseStats *stats, uint64_t time, uint64_t count);
int Cod2x_MouseStatsUpdate(Cod2xMouseStats *stats, uint64_t time, uint64_t count);
#endif
