#ifndef COD2_COM_RANDOM_H
#define COD2_COM_RANDOM_H

#if defined(COD2_X64) && COD2_X64
/* Windows 1.3's executable CRT owns one seed per thread. */
#define COM_CRT_RAND_MAX 32767
int Com_CrtRand(void);
void Com_CrtSrand(unsigned int seed);

#if defined(COD2_CODX) && COD2_CODX
/* Upstream's separately linked DLL has its own unseeded CRT stream. */
int Cod2x_CrtRand(void);
float Cod2x_CrtRandomf(void);
#endif
#else
#include <stdlib.h>
#define COM_CRT_RAND_MAX RAND_MAX
#define Com_CrtRand rand
#define Com_CrtSrand srand
#endif

#endif
