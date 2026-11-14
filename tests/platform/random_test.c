#include "PC/universal/com_math.c"
#include <assert.h>
#include <pthread.h>
#include "com_random.h"

/* Windows 1.3 0x57baad / 0x425fc0, 16 executed draws for each seed. */
static const struct { unsigned int seed; int draws[16]; } crtVectors[] = {
    { 1u, {41, 18467, 6334, 26500, 19169, 15724, 11478, 29358, 26962, 24464, 5705, 28145, 23281, 16827, 9961, 491} },
    { 17u, {94, 26602, 30017, 18297, 20363, 13015, 28509, 15290, 29003, 24399, 3339, 28849, 17055, 19424, 4588, 15756} },
    { 2147483647u, {35, 29739, 3374, 11141, 31308, 7870, 5253, 2445, 26706, 3992, 6001, 28056, 11771, 16503, 31113, 31351} },
    { 4294967295u, {35, 29739, 3374, 11141, 31308, 7870, 5253, 2445, 26706, 3992, 6001, 28056, 11771, 16503, 31113, 31351} },
};

static void *ThreadSequence(void *unused)
{
    (void)unused;
    assert(Com_CrtRand() == 41);
    assert(Com_CrtRand() == 18467);
    Com_CrtSrand(0);
    assert(Com_CrtRand() == 38);
    return NULL;
}

static void CheckCrt(void)
{
    assert(Com_CrtRand() == 41); /* Default per-thread seed, before any seed call. */
    pthread_t thread;
    assert(pthread_create(&thread, NULL, ThreadSequence, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(Com_CrtRand() == 18467); /* Child seeding must not alter this stream. */
    for (unsigned int i = 0; i < sizeof(crtVectors)/sizeof(crtVectors[0]); ++i) {
        Com_CrtSrand(crtVectors[i].seed);
        for (int j = 0; j < 16; ++j) assert(Com_CrtRand() == crtVectors[i].draws[j]);
        Com_CrtSrand(crtVectors[i].seed);
        for (int j = 0; j < 16; ++j) assert(randomf() == crtVectors[i].draws[j] / 32768.0f);
        Com_CrtSrand(crtVectors[i].seed);
        for (int j = 0; j < 16; ++j) assert(crandom() == crtVectors[i].draws[j] / 16384.0f - 1.0f);
    }
    Com_CrtSrand(1);
    Rand_Init(17);
    (void)irand(0, 1000);
    assert(Com_CrtRand() == 41);
    Rand_Init(1);
    Com_CrtSrand(0);
    assert(Com_CrtRand() == 38);
    assert(irand(0, 32768) == 20);
#if defined(COD2_CODX) && COD2_CODX
    assert(Cod2x_CrtRand() == 41);
    Com_CrtSrand(99);
    (void)Com_CrtRand();
    assert(Cod2x_CrtRand() == 18467);
    assert(Cod2x_CrtRandomf() == 6334 / 32767.0f);
#endif
}
int main(void)
{
    CheckCrt();
    Rand_Init(1);
    for (int i=0; i<10000; ++i) {
        int index=irand(0,17);
        float unit=flrand(-1,1);
        if (index<0 || index>=17 || unit < -1 || unit >= 1) {
            fprintf(stderr,"random range violated: index=%d float=%f\n",index,unit);
            return 1;
        }
    }
    Rand_Init(1);
    assert(irand(0,32768)==20);
    assert(irand(0,32768)==25617);
    puts("Windows CRT seed vectors, TLS/stream isolation and bounded legacy generator: PASS");
}
