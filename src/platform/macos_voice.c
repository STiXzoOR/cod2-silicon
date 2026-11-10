#if defined(__APPLE__) && defined(COD2_X64)
#include "common_types.h"
#include "macos_audio.h"
#include "macos_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(MacRecordBlock) == sizeof(audioSample_t) &&
               offsetof(MacRecordBlock, lengthInBytes) == offsetof(audioSample_t, lengthInBytes) &&
               offsetof(MacRecordBlock, frequency) == offsetof(audioSample_t, frequency) &&
               offsetof(MacRecordBlock, stereo) == offsetof(audioSample_t, stereo) &&
               offsetof(MacRecordBlock, channels) == offsetof(audioSample_t, channels) &&
               offsetof(MacRecordBlock, sampleOffset) == offsetof(audioSample_t, sampleOffset),
               "the capture block matches the engine's audioSample_t");

#define VOICE_QUEUE_BYTES 32768
#define VOICE_PACKET_BYTES 4096
#define VOICE_SAMPLES 65

typedef struct {
    dsound_sample_t sample;
    unsigned char queue[VOICE_QUEUE_BYTES];
    size_t read_cursor;
    size_t queued;
    int allocated;
} MacVoiceSample;

/* Mac 1.3 narrowband PCM rate, shared by recording and voice playback. */
int g_sound_recordFrequency = 8192;
int g_sound_channels = 1;

static MacVoiceSample voice_samples[VOICE_SAMPLES];
static int voice_initialized;

static MacVoiceSample *FindVoice(void *handle)
{
    for (int i = 0; i < VOICE_SAMPLES; ++i) {
        if (handle == &voice_samples[i].sample && voice_samples[i].allocated)
            return &voice_samples[i];
    }
    return NULL;
}

int DSound_Init(int channels, const unsigned char *handle)
{
    (void)channels;
    (void)handle;
    int rate = g_sound_recordFrequency > 0 ? g_sound_recordFrequency : 8192;
    voice_initialized = AIL_open_digital_driver(rate, 16, 2, 0) != NULL;
    return voice_initialized;
}

void *DSound_NewSample(void)
{
    if (!voice_initialized)
        return NULL;
    for (int i = 0; i < VOICE_SAMPLES; ++i) {
        MacVoiceSample *voice = &voice_samples[i];
        if (voice->allocated)
            continue;
        memset(voice, 0, sizeof(*voice));
        voice->sample.mssSample = AIL_allocate_sample_handle((void *)(uintptr_t)1);
        if (!voice->sample.mssSample)
            return NULL;
        voice->allocated = 1;
        voice->sample.frequency = g_sound_recordFrequency > 0 ? g_sound_recordFrequency : 8192;
        voice->sample.channels = 1;
        voice->sample.volume = 255;
        voice->sample.pan = 128;
        AIL_set_sample_type(voice->sample.mssSample, 1, 0);
        AIL_set_sample_playback_rate(voice->sample.mssSample, voice->sample.frequency);
        return &voice->sample;
    }
    return NULL;
}

void DSound_SampleFrame(void *handle)
{
    MacVoiceSample *voice = FindVoice(handle);
    if (!voice)
        return;
    while (voice->queued) {
        int buffer = AIL_sample_buffer_ready(voice->sample.mssSample);
        if (buffer < 0)
            break;
        unsigned char pcm[VOICE_PACKET_BYTES];
        size_t size = voice->queued < sizeof(pcm) ? voice->queued : sizeof(pcm);
        size_t first = VOICE_QUEUE_BYTES - voice->read_cursor;
        if (first > size)
            first = size;
        memcpy(pcm, voice->queue + voice->read_cursor, first);
        memcpy(pcm + first, voice->queue, size - first);
        AIL_load_sample_buffer(voice->sample.mssSample, buffer, pcm, (int)size);
        voice->read_cursor = (voice->read_cursor + size) % VOICE_QUEUE_BYTES;
        voice->queued -= size;
    }
    voice->sample.playing = AIL_sample_status(voice->sample.mssSample) == 4;
}

int DSound_UpdateSample(void *handle, char *data, unsigned int length)
{
    MacVoiceSample *voice = FindVoice(handle);
    if (!voice || !data || length % 2)
        return -1;
    size_t available = VOICE_QUEUE_BYTES - voice->queued;
    size_t accepted = length < available ? length : available;
    size_t cursor = (voice->read_cursor + voice->queued) % VOICE_QUEUE_BYTES;
    size_t first = VOICE_QUEUE_BYTES - cursor;
    if (first > accepted)
        first = accepted;
    memcpy(voice->queue + cursor, data, first);
    memcpy(voice->queue, data + first, accepted - first);
    voice->queued += accepted;
    DSound_SampleFrame(handle);
    return (int)accepted;
}

void DSound_Frame(void)
{
    for (int i = 0; i < VOICE_SAMPLES; ++i) {
        if (voice_samples[i].allocated)
            DSound_SampleFrame(&voice_samples[i].sample);
    }
}

static int DestroyVoice(void *handle)
{
    MacVoiceSample *voice = FindVoice(handle);
    if (!voice)
        return 0;
    AIL_release_sample_handle(voice->sample.mssSample);
    memset(voice, 0, sizeof(*voice));
    return 1;
}

void DSound_Shutdown(void)
{
    for (int i = 0; i < VOICE_SAMPLES; ++i)
        DestroyVoice(&voice_samples[i].sample);
    voice_initialized = 0;
}

/* The engine destroys playback voices through the recorder's entry, as the
 * Windows layer does, where both come from one pool. The one capture sample
 * is not in the pool and needs no release. */
int DSOUNDRecord_DestroySample(void *sample)
{
    DestroyVoice(sample);
    return 1;
}
#endif
