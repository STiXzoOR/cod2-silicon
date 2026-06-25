#ifndef CLEAN_PC_UNIVERSAL_Q_SHARED_H
#define CLEAN_PC_UNIVERSAL_Q_SHARED_H

#include "../../cod2_fwd.h"
typedef struct EffectVisInfo EffectVisInfo;
typedef struct ping_t ping_t;
typedef struct qtime_s qtime_t;

struct EffectVisInfo {
    vec3_t origin;
    float distSq;
    float vis;
};

struct ping_t {
    netadr_t adr;
    int start;
    int time;
    char info[1024];
};
#endif
