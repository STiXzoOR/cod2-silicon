
#include "common_types.h"
#include "imports.h"

extern int DSound_UpdateSample(sample_t *sample, char *data, unsigned int data_len);
extern sample_t *DSound_NewSample(void);
extern int Record_DestroySample(sample_t *sample);
extern void DSound_Frame(void);
extern void DSound_SampleFrame(sample_t *sample);
extern int DSound_Init(int numChannels, const unsigned char *handle);
extern void DSound_Shutdown(void);

int Sound_UpdateSample(sample_t *sample, char *data, unsigned int data_len)
{
    return DSound_UpdateSample(sample, data, data_len);
}

sample_t *Sound_NewSample(void)
{
    return DSound_NewSample();
}

int Sound_DestroySample(sample_t *sample)
{
    return Record_DestroySample(sample);
}

void Sound_Frame(void)
{
    DSound_Frame();
}

void Sound_SampleFrame(sample_t *sample)
{
    return DSound_SampleFrame(sample);
}

int Sound_Init(const unsigned char *handle)
{
    return DSound_Init(1, handle);
}

void Sound_Shutdown(void)
{
    DSound_Shutdown();
}
