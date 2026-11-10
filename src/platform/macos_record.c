/* Microphone capture for voice chat: the platform half of groupvoice/record.c.
 *
 * The Windows and Mac 1.3 games open the microphone once and leave it
 * recording, dropping what they capture while the talk key is up. macOS shows
 * an indicator for as long as a program listens and asks the player's
 * permission first, so the device is opened here only while the engine would
 * send voice (Voice_SendVoiceData) and for a moment after. */
#if defined(__APPLE__) && defined(COD2_X64)
#include "macos_record.h"
#include <AudioToolbox/AudioToolbox.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define RECORD_RING_SAMPLES 16384
#define RECORD_QUEUE_BUFFERS 3
#define RECORD_BUFFER_BYTES 512
#define RECORD_CHUNK_SAMPLES 2048
/* Keep the device open this long after the last frame that wanted voice. */
#define RECORD_LINGER_SECONDS 2.0

extern int g_sound_recordFrequency;
extern int g_sound_channels;
extern int Voice_SendVoiceData(void);
extern int MacMicrophone_Allowed(void);
/* Where Record_Init leaves the engine's callback. */
extern void *const imp_current_audioCallback;

int g_sound_recordVolume = 0xff;
char g_recording_initialized;
char g_currently_recording;

static pthread_mutex_t record_mutex = PTHREAD_MUTEX_INITIALIZER;
static int16_t record_ring[RECORD_RING_SAMPLES];
static size_t record_read, record_count;
static AudioQueueRef record_queue;
static int record_running;
static double record_last_wanted;
static float record_gain = 1.0f;
static int record_level = 0xffff;
static struct { int unused; } record_sample;

static double RecordSeconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

static void RecordInput(void *user, AudioQueueRef queue, AudioQueueBufferRef buffer, const AudioTimeStamp *start,
                        UInt32 packets, const AudioStreamPacketDescription *descriptions)
{
    (void)user; (void)start; (void)packets; (void)descriptions;
    const int16_t *pcm = buffer->mAudioData;
    size_t samples = buffer->mAudioDataByteSize / sizeof(int16_t);
    pthread_mutex_lock(&record_mutex);
    for (size_t i = 0; i < samples; ++i) {
        if (record_count == RECORD_RING_SAMPLES) {
            /* The game stopped reading: keep the newest sound. */
            record_read = (record_read + 1) % RECORD_RING_SAMPLES;
            --record_count;
        }
        record_ring[(record_read + record_count++) % RECORD_RING_SAMPLES] = pcm[i];
    }
    int running = record_running;
    pthread_mutex_unlock(&record_mutex);
    if (running)
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static void RecordCloseDevice(void)
{
    if (!record_queue)
        return;
    pthread_mutex_lock(&record_mutex);
    record_running = 0;
    pthread_mutex_unlock(&record_mutex);
    AudioQueueStop(record_queue, true);
    AudioQueueDispose(record_queue, true);
    record_queue = NULL;
    pthread_mutex_lock(&record_mutex);
    record_read = record_count = 0;
    pthread_mutex_unlock(&record_mutex);
}

static int RecordOpenDevice(void)
{
    AudioStreamBasicDescription format = { 0 };
    format.mSampleRate = g_sound_recordFrequency > 0 ? g_sound_recordFrequency : 8192;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 16;
    format.mBytesPerFrame = format.mBytesPerPacket = 2;
    format.mFramesPerPacket = 1;
    OSStatus status = AudioQueueNewInput(&format, RecordInput, NULL, NULL, NULL, 0, &record_queue);
    if (status == noErr) {
        record_running = 1;
        for (int i = 0; i < RECORD_QUEUE_BUFFERS && status == noErr; ++i) {
            AudioQueueBufferRef buffer;
            status = AudioQueueAllocateBuffer(record_queue, RECORD_BUFFER_BYTES, &buffer);
            if (status == noErr)
                status = AudioQueueEnqueueBuffer(record_queue, buffer, 0, NULL);
        }
        if (status == noErr)
            status = AudioQueueStart(record_queue, NULL);
    }
    if (status != noErr) {
        static int reported;
        if (!reported) {
            fprintf(stderr, "Voice chat: the microphone could not be opened (error %d).\n", (int)status);
            reported = 1;
        }
        if (record_queue) {
            record_running = 0;
            AudioQueueDispose(record_queue, true);
            record_queue = NULL;
        }
        return 0;
    }
    return 1;
}

int DSOUNDRecord_Init(int initialize, const void *handle)
{
    (void)initialize;
    (void)handle;
    g_currently_recording = 0;
    g_recording_initialized = 1;
    return 1;
}

void *DSOUNDRecord_NewSample(void)
{
    return g_recording_initialized ? &record_sample : NULL;
}

int DSOUNDRecord_Start(void *sample)
{
    if (!g_recording_initialized || !sample)
        return -1;
    g_currently_recording = 1;
    return 0;
}

int DSOUNDRecord_Stop(void *sample)
{
    (void)sample;
    if (!g_recording_initialized)
        return -1;
    RecordCloseDevice();
    g_currently_recording = 0;
    return 0;
}

void DSOUNDRecord_Shutdown(void)
{
    RecordCloseDevice();
    g_recording_initialized = g_currently_recording = 0;
}

void DSOUNDRecord_Frame(void)
{
    if (!g_recording_initialized || !g_currently_recording)
        return;
    double now = RecordSeconds();
    if (Voice_SendVoiceData()) {
        static double next_attempt;
        record_last_wanted = now;
        if (!record_queue && now >= next_attempt) {
            /* A refused or failed open is retried, but not every frame. */
            next_attempt = now + 0.5;
            if (MacMicrophone_Allowed())
                RecordOpenDevice();
        }
    } else if (record_queue && now - record_last_wanted > RECORD_LINGER_SECONDS) {
        RecordCloseDevice();
    }
    if (!record_queue)
        return;
    int (*callback)(MacRecordBlock *) = *(int (**)(MacRecordBlock *))imp_current_audioCallback;
    for (;;) {
        int16_t pcm[RECORD_CHUNK_SAMPLES];
        size_t samples = 0;
        pthread_mutex_lock(&record_mutex);
        while (record_count && samples < RECORD_CHUNK_SAMPLES) {
            pcm[samples++] = record_ring[record_read];
            record_read = (record_read + 1) % RECORD_RING_SAMPLES;
            --record_count;
        }
        pthread_mutex_unlock(&record_mutex);
        if (!samples)
            break;
        if (record_gain != 1.0f) {
            for (size_t i = 0; i < samples; ++i)
                pcm[i] = (int16_t)((float)pcm[i] * record_gain);
        }
        MacRecordBlock block;
        memset(&block, 0, sizeof(block));
        block.buffer = (unsigned char *)pcm;
        block.lengthInBytes = (int)(samples * sizeof(int16_t));
        block.lengthInSamples = (int)samples;
        block.bytesPerSample = 2;
        block.frequency = g_sound_recordFrequency;
        block.stereo = 0;
        block.channels = 1;
        block.sampleOffset = 0;
        if (callback)
            callback(&block);
    }
}

/* The Windows mixer calls. macOS has no per-program recording source or
 * input mute, so the level becomes a gain on the captured samples. */
int mixerGetRecordSource(char *source)
{
    if (source)
        memcpy(source, "Mac", 4);
    return 1;
}

int mixerSetRecordSource(const char *source)
{
    (void)source;
    return 1;
}

int mixerGetRecordLevel(const char *source)
{
    (void)source;
    return record_level;
}

int mixerSetRecordLevel(const char *source, int level)
{
    (void)source;
    record_level = level < 0 ? 0 : level > 0xffff ? 0xffff : level;
    record_gain = (float)record_level / 65535.0f;
    return 0;
}

int mixerSetMicrophoneMute(int muted)
{
    (void)muted;
    return 0;
}
#endif
