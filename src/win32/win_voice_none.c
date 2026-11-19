/* The Windows client without a voice device layer. Voice chat stays off: nothing is
 * captured and nothing is played. CoD2x turns voice chat off in any case (Voice_Init),
 * so a CoD2x build loses nothing here. cmake/windows-native.cmake swaps this file for a
 * real layer when a checkout provides one. */
#include <stddef.h>

int g_sound_recordFrequency = 8192;
int g_sound_recordVolume = 0xff;
int g_sound_channels = 1;

int DSound_Init(int channels, const unsigned char *handle)
{
    (void)channels; (void)handle;
    return 0;
}

void *DSound_NewSample(void)
{
    return NULL;
}

int DSound_UpdateSample(void *sample, char *data, unsigned int length)
{
    (void)sample; (void)data; (void)length;
    return 0;
}

void DSound_SampleFrame(void *sample)
{
    (void)sample;
}

void DSound_Frame(void)
{
}

void DSound_Shutdown(void)
{
}

int DSOUNDRecord_Init(int initialize, const void *handle)
{
    (void)initialize; (void)handle;
    return 0;
}

void *DSOUNDRecord_NewSample(void)
{
    return NULL;
}

int DSOUNDRecord_DestroySample(void *sample)
{
    (void)sample;
    return 0;
}

int DSOUNDRecord_Start(void *sample)
{
    (void)sample;
    return -1;
}

int DSOUNDRecord_Stop(void *sample)
{
    (void)sample;
    return -1;
}

void DSOUNDRecord_Frame(void)
{
}

void DSOUNDRecord_Shutdown(void)
{
}

int mixerGetRecordSource(char *source)
{
    if (source)
        source[0] = '\0';
    return 0;
}

int mixerSetRecordSource(const char *source)
{
    (void)source;
    return 0;
}

int mixerGetRecordLevel(const char *source)
{
    (void)source;
    return 0;
}

int mixerSetRecordLevel(const char *source, int level)
{
    (void)source; (void)level;
    return 0;
}

int mixerSetMicrophoneMute(int mute)
{
    (void)mute;
    return 0;
}
