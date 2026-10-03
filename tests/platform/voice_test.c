#include "common_types.h"
#include "macos_audio.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(COD2_X64)
extern int DSound_Init(int channels, const unsigned char *handle);
extern void *DSound_NewSample(void);
extern int DSound_UpdateSample(void *handle, char *data, unsigned int size);
extern void DSound_Shutdown(void);
extern int Record_Init(int initialize, const void *handle);
extern int Record_DestroySample(void *handle);
extern int g_sound_recordFrequency;

int main(void)
{
    _Static_assert(sizeof(AILSOUNDINFO) == sizeof(MacAudioSoundInfo), "native sound metadata size");
    _Static_assert(offsetof(AILSOUNDINFO, initial_ptr) == offsetof(MacAudioSoundInfo, initial_ptr), "native metadata field offsets");
    if (Record_Init(0, NULL))
        return 1;
    g_sound_recordFrequency = 11025;
    if (!DSound_Init(1, NULL)) {
        fprintf(stderr, "%s\n", AIL_last_error());
        return 1;
    }
    void *voice = DSound_NewSample();
    if (!voice)
        return 1;
    int16_t pcm[2205];
    for (size_t i = 0; i < sizeof(pcm) / sizeof(pcm[0]); ++i)
        pcm[i] = (int16_t)(sin(i * 660.0 * 6.283185307179586 / 11025) * 4000);
    MacAudioStats before, after;
    MacAudio_GetStats(&before);
    if (DSound_UpdateSample(voice, (char *)pcm, sizeof(pcm)) != sizeof(pcm))
        return 1;
    usleep(300000);
    MacAudio_GetStats(&after);
    int result = after.audible_frames > before.audible_frames && after.callbacks > before.callbacks;
    printf("voice PCM output: %s callbacks=%llu audible=%llu peak=%.6f\n", result ? "PASS" : "FAIL", (unsigned long long)(after.callbacks - before.callbacks), (unsigned long long)(after.audible_frames - before.audible_frames), after.peak);
    if (!Record_DestroySample(voice))
        result = 0;
    DSound_Shutdown();
    AIL_shutdown();
    return !result;
}
#else
#error This test requires Apple COD2_X64.
#endif
