/* Microphone capture for voice chat: the platform half of groupvoice/record.c.
 *
 * The Windows and Mac 1.3 games open the microphone once and leave it
 * recording, dropping what they capture while the talk key is up. macOS shows
 * an indicator for as long as a program listens and asks the player's
 * permission first, so the device is opened here only while the engine would
 * send voice (Voice_SendVoiceData) and for a moment after.
 *
 * Opening and closing an input queue can take from tens to hundreds of
 * milliseconds (longer with a Bluetooth headset), so a worker thread does it
 * and the frame thread only says what it wants and reads the captured sound.
 * The worker also closes the queue when the system's default input changes
 * or the queue stops by itself; the usual retry then opens the new device. */
#if defined(__APPLE__) && defined(COD2_X64)
#include "macos_record.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <errno.h>
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
/* A refused or failed open is retried, but no more often than this. */
#define RECORD_RETRY_SECONDS 0.5

extern int g_sound_recordFrequency;
extern int g_sound_channels;
extern int Voice_SendVoiceData(void);
extern int MacMicrophone_Allowed(void);
/* Where Record_Init leaves the engine's callback. */
extern void *const imp_current_audioCallback;
/* Playback voices (macos_voice.c), for hearing one's own microphone. */
extern void *DSound_NewSample(void);
extern int DSound_UpdateSample(void *handle, char *data, unsigned int length);
extern int DSOUNDRecord_DestroySample(void *sample);

int g_sound_recordVolume = 0xff;
char g_recording_initialized;
char g_currently_recording;

/* record_mutex guards everything from here to record_quit. The audio
 * callback, the Core Audio listeners, the worker and the frame thread all
 * take it, and nobody holds it across a Core Audio call that can wait. */
static pthread_mutex_t record_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t record_wake = PTHREAD_COND_INITIALIZER;
static int16_t record_ring[RECORD_RING_SAMPLES];
static size_t record_read, record_count;
/* The queue is capturing; the callback hands its buffers back. */
static int record_running;
/* The frame thread wanted voice since the worker last tried to open. */
static int record_request;
/* Within the linger time of the last frame that wanted voice. */
static int record_keep;
/* The default input changed or the queue stopped: close and reopen. */
static int record_reset;
static int record_quit;

/* Owned by the worker thread, which is the only one to call Core Audio
 * functions that can wait. */
static AudioQueueRef record_queue;
static double record_next_attempt;
static int record_listening;

static pthread_t record_thread;
static int record_thread_started;

/* Owned by the frame thread. */
static double record_last_wanted;
static float record_gain = 1.0f;
static int record_level = 0xffff;
static void *record_monitor;
static struct { int unused; } record_sample;

static const AudioObjectPropertyAddress record_default_input = {
    kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
};

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
    int running = record_running;
    for (size_t i = 0; running && i < samples; ++i) {
        if (record_count == RECORD_RING_SAMPLES) {
            /* The game stopped reading: keep the newest sound. */
            record_read = (record_read + 1) % RECORD_RING_SAMPLES;
            --record_count;
        }
        record_ring[(record_read + record_count++) % RECORD_RING_SAMPLES] = pcm[i];
    }
    pthread_mutex_unlock(&record_mutex);
    if (running)
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static void RecordReset(void)
{
    pthread_mutex_lock(&record_mutex);
    record_reset = 1;
    pthread_cond_signal(&record_wake);
    pthread_mutex_unlock(&record_mutex);
}

/* The queue stops by itself when its device goes away. */
static void RecordRunningChanged(void *user, AudioQueueRef queue, AudioQueuePropertyID property)
{
    (void)user; (void)property;
    UInt32 running = 1, size = sizeof(running);
    if (AudioQueueGetProperty(queue, kAudioQueueProperty_IsRunning, &running, &size) != noErr || running)
        return;
    pthread_mutex_lock(&record_mutex);
    int unexpected = record_running;
    pthread_mutex_unlock(&record_mutex);
    if (unexpected)
        RecordReset();
}

static OSStatus RecordDefaultInputChanged(AudioObjectID object, UInt32 count, const AudioObjectPropertyAddress *addresses,
                                          void *user)
{
    (void)object; (void)count; (void)addresses; (void)user;
    RecordReset();
    return noErr;
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
        pthread_mutex_lock(&record_mutex);
        record_running = 1;
        pthread_mutex_unlock(&record_mutex);
        status = AudioQueueAddPropertyListener(record_queue, kAudioQueueProperty_IsRunning, RecordRunningChanged, NULL);
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
            pthread_mutex_lock(&record_mutex);
            record_running = 0;
            pthread_mutex_unlock(&record_mutex);
            AudioQueueDispose(record_queue, true);
            record_queue = NULL;
        }
        return 0;
    }
    return 1;
}

static void RecordWaitUntil(double when)
{
    double seconds = when - RecordSeconds();
    if (seconds <= 0)
        return;
    struct timespec now, deadline;
    clock_gettime(CLOCK_REALTIME, &now);
    long nanoseconds = now.tv_nsec + (long)((seconds - (double)(long)seconds) * 1e9);
    deadline.tv_sec = now.tv_sec + (time_t)seconds + nanoseconds / 1000000000L;
    deadline.tv_nsec = nanoseconds % 1000000000L;
    pthread_cond_timedwait(&record_wake, &record_mutex, &deadline);
}

static void *RecordWorker(void *unused)
{
    (void)unused;
    /* The first call into the audio hardware layer can itself be slow. */
    record_listening = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &record_default_input,
                                                      RecordDefaultInputChanged, NULL) == noErr;
    pthread_mutex_lock(&record_mutex);
    while (!record_quit) {
        int release = record_queue && (record_reset || !record_keep);
        if (release && record_reset) {
            /* Not a failed open: the new device may open at once. A second
             * report of the same change then costs one more quick reopen. */
            record_next_attempt = 0;
        }
        record_reset = 0;
        if (release) {
            pthread_mutex_unlock(&record_mutex);
            RecordCloseDevice();
            pthread_mutex_lock(&record_mutex);
            continue;
        }
        if (!record_queue && record_keep && record_request) {
            double now = RecordSeconds();
            if (now < record_next_attempt) {
                RecordWaitUntil(record_next_attempt);
                continue;
            }
            record_next_attempt = now + RECORD_RETRY_SECONDS;
            record_request = 0;
            pthread_mutex_unlock(&record_mutex);
            if (MacMicrophone_Allowed())
                RecordOpenDevice();
            pthread_mutex_lock(&record_mutex);
            continue;
        }
        pthread_cond_wait(&record_wake, &record_mutex);
    }
    pthread_mutex_unlock(&record_mutex);
    if (record_listening)
        AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &record_default_input, RecordDefaultInputChanged, NULL);
    record_listening = 0;
    RecordCloseDevice();
    return NULL;
}

static void RecordStartWorker(void)
{
    if (record_thread_started)
        return;
    pthread_mutex_lock(&record_mutex);
    record_read = record_count = 0;
    record_running = record_request = record_keep = record_reset = record_quit = 0;
    pthread_mutex_unlock(&record_mutex);
    record_next_attempt = 0;
    if (pthread_create(&record_thread, NULL, RecordWorker, NULL) != 0) {
        fputs("Voice chat: the microphone thread could not be started.\n", stderr);
        return;
    }
    record_thread_started = 1;
}

static void RecordStopWorker(void)
{
    if (!record_thread_started)
        return;
    pthread_mutex_lock(&record_mutex);
    record_quit = 1;
    pthread_cond_signal(&record_wake);
    pthread_mutex_unlock(&record_mutex);
    pthread_join(record_thread, NULL);
    record_thread_started = 0;
}

/* Ask the worker to close the device; the frame thread never waits for it. */
static void RecordRelease(void)
{
    pthread_mutex_lock(&record_mutex);
    record_keep = record_request = 0;
    pthread_cond_signal(&record_wake);
    pthread_mutex_unlock(&record_mutex);
}

int DSOUNDRecord_Init(int initialize, const void *handle)
{
    (void)initialize;
    (void)handle;
    g_currently_recording = 0;
    RecordStartWorker();
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
    RecordRelease();
    g_currently_recording = 0;
    return 0;
}

void DSOUNDRecord_Shutdown(void)
{
    RecordStopWorker();
    g_recording_initialized = g_currently_recording = 0;
}

void DSOUNDRecord_Frame(void)
{
    if (!g_recording_initialized || !g_currently_recording)
        return;
    double now = RecordSeconds();
    int wanted = Voice_SendVoiceData();
    pthread_mutex_lock(&record_mutex);
    if (wanted) {
        record_last_wanted = now;
        if (!record_request || !record_keep) {
            record_request = record_keep = 1;
            pthread_cond_signal(&record_wake);
        }
    } else if (record_keep && now - record_last_wanted > RECORD_LINGER_SECONDS) {
        record_keep = 0;
        pthread_cond_signal(&record_wake);
    }
    pthread_mutex_unlock(&record_mutex);
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
        if (record_monitor)
            DSound_UpdateSample(record_monitor, (char *)pcm, (unsigned int)(samples * sizeof(int16_t)));
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

/* winvoice_mic_mute. On Windows this mutes the microphone's line in the
 * speakers' mix, the sound card playing the microphone straight back to the
 * player; what is recorded and sent is not affected. Voice_Init passes the
 * setting (1 by default) and Voice_Shutdown passes 1. Here 0 plays the
 * captured sound back through one playback voice while the microphone is
 * open, and 1 stops that; what is sent never changes. Voice_Shutdown has
 * released every playback voice before it calls this with 1. */
int mixerSetMicrophoneMute(int muted)
{
    if (!muted && !record_monitor) {
        record_monitor = DSound_NewSample();
    } else if (muted && record_monitor) {
        DSOUNDRecord_DestroySample(record_monitor);
        record_monitor = NULL;
    }
    return 0;
}
#endif
