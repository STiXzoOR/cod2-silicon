#include "common_types.h"
#include "imports.h"

void CCircularBuffer_Write(const CCircularBuffer *_this, const void *inBuffer, UInt32 *ioSize);

static Bool dsoundplay_initialized;

int DSound_UpdateSample(sample_t *sample, char *data, unsigned int data_len);
sample_t *DSound_NewSample(void);
int DSound_Init(void);
void DSound_Shutdown(void);
void DSound_Frame(void);
void DSound_SampleFrame(sample_t *sample);

int DSound_UpdateSample(sample_t *sample, char *data, unsigned int data_len)
{
    if (!dsoundplay_initialized)
        return -1;

    if (!data_len)
        return 0;

    UInt32 size = data_len;
    CCircularBuffer_Write(sample->mssBuffer, data, &size);
    return data_len;
}

sample_t *DSound_NewSample(void)
{
    return 0;
}
