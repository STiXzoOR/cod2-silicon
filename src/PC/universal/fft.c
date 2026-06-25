#include "common_types.h"
#include "imports.h"

extern float sinf(float);
extern float cosf(float);

void FFT_Init(int *fftBitswap, complex_t *fftTrigTable)
{
    int fftIndex;

    for (fftIndex = 0; fftIndex < 256; fftIndex++) {
        int bit;

        fftBitswap[fftIndex] = 0;
        for (bit = 0; bit < 8; bit++) {
            if ((fftIndex >> bit) & 1) {
                fftBitswap[fftIndex] |= (1 << (7 - bit));
            }
        }

        float radians = (float)((double)fftIndex * 0.02454369260617026);
        fftTrigTable[fftIndex].imag = sinf(radians);
        fftTrigTable[fftIndex].real = cosf(radians);
    }
}

void FFT(complex_t *data, long unsigned int log2_count, long unsigned int stride, int *bitSwap, complex_t *trigTable)
{
    int count = 1 << log2_count;
    int shift = 8 - log2_count;
    unsigned int totalStride = stride * count;

    if (count > 0) {
        int i;
        for (i = 0; i < count; i++) {
            int j = bitSwap[i] >> shift;
            if (i > j) {
                complex_t temp = data[i * stride];
                data[i * stride] = data[j * stride];
                data[j * stride] = temp;
            }
        }
    }

    if (totalStride != 0) {
        unsigned int k;
        for (k = 0; k < totalStride; k += stride * 4) {
            complex_t *pa = &data[k];
            complex_t *pb = &data[k + stride];
            complex_t *pc = &data[k + stride * 2];
            complex_t *pd = &data[k + stride * 3];

            complex_t sa = *pa;
            complex_t sc = *pc;

            float t1r = sa.real + pb->real;
            float t2r = sc.real + pd->real;
            pa->real = t1r + t2r;
            pc->real = t1r - t2r;

            float t1i = sa.imag + pb->imag;
            float t2i = sc.imag + pd->imag;
            pa->imag = t1i + t2i;
            pc->imag = t1i - t2i;

            float dr = sa.real - pb->real;
            float di = sc.imag - pd->imag;
            pb->real = dr - di;

            float cr = sc.real - pd->real;
            pd->real = dr + di;

            float ai = sa.imag - pb->imag;
            pb->imag = cr + ai;
            pd->imag = ai - cr;
        }
    }

    if (count > 4) {
        int bitshave = 5;
        int mmax = 4;

        for (;;) {
            int iStep = mmax * 2;

            if (mmax > 0) {
                unsigned int mmaxStride = mmax * stride;
                unsigned int iStepStride = iStep * stride;
                int m;

                for (m = 0; m < mmax; m++) {
                    int tidx = m << bitshave;
                    float wr = trigTable[tidx].real;
                    float wi = trigTable[tidx].imag;
                    unsigned int j;

                    for (j = m * stride; j < totalStride; j += iStepStride) {
                        unsigned int idx = j + mmaxStride;
                        float tempr = wr * data[idx].real - wi * data[idx].imag;
                        float tempi = wr * data[idx].imag + wi * data[idx].real;
                        data[idx].real = data[j].real - tempr;
                        data[idx].imag = data[j].imag - tempi;
                        data[j].real += tempr;
                        data[j].imag += tempi;
                    }
                }
            }

            bitshave--;
            if (count <= iStep)
                break;
            mmax = iStep;
        }
    }
}
