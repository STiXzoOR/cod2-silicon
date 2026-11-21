/* Drives the production microphone capture (src/platform/macos_record.c)
 * against a fake Core Audio. Nothing here opens a device or asks macOS for
 * microphone permission: this file defines the AudioQueue and audio-object
 * functions the capture code calls, the permission check, the engine's talk
 * gate and the playback voices, and the test links without AudioToolbox.
 *
 * The fake queue runs its own capture thread, which fills buffers with a
 * running sample counter and calls the production input callback, as the
 * real queue does, so the thread sanitizer sees the same three threads
 * (frame, worker, audio callback) the game has. */
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "platform/macos_record.h"

/* Production entry points. */
int DSOUNDRecord_Init(int initialize, const void *handle);
void *DSOUNDRecord_NewSample(void);
int DSOUNDRecord_Start(void *sample);
int DSOUNDRecord_Stop(void *sample);
void DSOUNDRecord_Shutdown(void);
void DSOUNDRecord_Frame(void);
int mixerSetMicrophoneMute(int muted);
int mixerSetRecordLevel(const char *source, int level);

#define CHECK(condition, ...) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s: ", __FILE__, __LINE__, #condition); \
        fprintf(stderr, __VA_ARGS__); \
        fputc('\n', stderr); \
        exit(1); \
    } \
} while (0)

/* ---- Engine and platform boundaries ---------------------------------- */

int g_sound_recordFrequency = 8192;
int g_sound_channels = 1;

static atomic_int fake_talking;
static atomic_int fake_allowed;
static atomic_int fake_allowed_calls;

int Voice_SendVoiceData(void) { return atomic_load(&fake_talking); }

/* Stands in for macos_microphone.m, which would ask macOS. */
int MacMicrophone_Allowed(void)
{
    atomic_fetch_add(&fake_allowed_calls, 1);
    return atomic_load(&fake_allowed);
}

/* Blocks received by the engine's recording callback. Frame thread only. */
static long sent_samples;
static int sent_expected = -1;
static int sent_gaps;
static int OnBlock(MacRecordBlock *block)
{
    CHECK(block->bytesPerSample == 2 && block->channels == 1 && !block->stereo, "block format");
    CHECK(block->lengthInBytes == block->lengthInSamples * 2, "block length");
    CHECK(block->frequency == 8192, "block rate %d", block->frequency);
    const int16_t *pcm = (const int16_t *)block->buffer;
    for (int i = 0; i < block->lengthInSamples; ++i) {
        if (sent_expected >= 0 && pcm[i] != (int16_t)sent_expected)
            ++sent_gaps;
        sent_expected = (uint16_t)(pcm[i] + 1);
    }
    sent_samples += block->lengthInSamples;
    return 0;
}
static int (*callback_slot)(MacRecordBlock *) = OnBlock;
void *const imp_current_audioCallback = &callback_slot;

/* Playback voices, for the own-microphone monitor. Frame thread only. */
static int monitor_voice;
static int monitor_created, monitor_destroyed;
static long monitor_bytes;
void *DSound_NewSample(void) { ++monitor_created; return &monitor_voice; }
int DSound_UpdateSample(void *handle, char *data, unsigned int length)
{
    CHECK(handle == &monitor_voice && data && length % 2 == 0, "monitor update");
    monitor_bytes += length;
    return (int)length;
}
int DSOUNDRecord_DestroySample(void *sample)
{
    CHECK(sample == &monitor_voice, "monitor destroy");
    ++monitor_destroyed;
    return 1;
}

/* ---- Fake Core Audio ------------------------------------------------- */

typedef struct {
    UInt32 capacity;
    void *data;
    UInt32 size;
    void *user;
    UInt32 packetCapacity;
    AudioStreamPacketDescription *packets;
    UInt32 packetCount;
} FakeBufferLayout;
_Static_assert(sizeof(FakeBufferLayout) == sizeof(AudioQueueBuffer), "AudioQueueBuffer layout");

#define FAKE_BUFFERS 8
typedef struct FakeQueue {
    AudioQueueInputCallback callback;
    void *user;
    AudioQueuePropertyListenerProc listener;
    void *listener_user;
    AudioQueueBufferRef buffers[FAKE_BUFFERS];
    int allocated;
    AudioQueueBufferRef ready[FAKE_BUFFERS];
    int ready_count;
    pthread_mutex_t lock;
    pthread_t thread;
    int thread_started;
    atomic_int running;
    atomic_int stop;
    int device;
} FakeQueue;

static pthread_mutex_t fake_lock = PTHREAD_MUTEX_INITIALIZER;
static FakeQueue *fake_current;
static atomic_int fake_open_delay_ms;
static atomic_int fake_open_failures;
static atomic_int fake_new_calls, fake_start_calls, fake_dispose_calls;
static atomic_int fake_default_device = 1;
static atomic_int fake_counter;
static AudioObjectPropertyListenerProc fake_hal_listener;
static void *fake_hal_user;
static int fake_hal_added, fake_hal_removed;

static void SleepMs(int ms)
{
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

static double Now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static void *FakeCapture(void *arg)
{
    FakeQueue *queue = arg;
    while (!atomic_load(&queue->stop)) {
        AudioQueueBufferRef buffer = NULL;
        pthread_mutex_lock(&queue->lock);
        if (queue->ready_count) {
            buffer = queue->ready[0];
            memmove(queue->ready, queue->ready + 1, (size_t)--queue->ready_count * sizeof(queue->ready[0]));
        }
        pthread_mutex_unlock(&queue->lock);
        if (buffer) {
            int16_t *pcm = buffer->mAudioData;
            UInt32 samples = buffer->mAudioDataBytesCapacity / 2;
            for (UInt32 i = 0; i < samples; ++i)
                pcm[i] = (int16_t)atomic_fetch_add(&fake_counter, 1);
            buffer->mAudioDataByteSize = samples * 2;
            AudioTimeStamp stamp;
            memset(&stamp, 0, sizeof(stamp));
            queue->callback(queue->user, (AudioQueueRef)queue, buffer, &stamp, samples, NULL);
        }
        /* 256 samples at 8192 Hz is 31 ms; run faster so the ring is busy. */
        SleepMs(4);
    }
    return NULL;
}

static void FakeStopThread(FakeQueue *queue)
{
    if (!queue->thread_started)
        return;
    atomic_store(&queue->stop, 1);
    pthread_join(queue->thread, NULL);
    queue->thread_started = 0;
}

OSStatus AudioQueueNewInput(const AudioStreamBasicDescription *format, AudioQueueInputCallback callback, void *user,
                            CFRunLoopRef loop, CFStringRef mode, UInt32 flags, AudioQueueRef *out)
{
    (void)loop; (void)mode; (void)flags;
    atomic_fetch_add(&fake_new_calls, 1);
    CHECK(format->mSampleRate == 8192 && format->mChannelsPerFrame == 1 && format->mBitsPerChannel == 16, "format");
    int delay = atomic_load(&fake_open_delay_ms);
    if (delay)
        SleepMs(delay);
    if (atomic_load(&fake_open_failures) > 0) {
        atomic_fetch_sub(&fake_open_failures, 1);
        *out = NULL;
        return -66681; /* kAudioQueueErr_CannotStart */
    }
    FakeQueue *queue = calloc(1, sizeof(*queue));
    queue->callback = callback;
    queue->user = user;
    queue->device = atomic_load(&fake_default_device);
    pthread_mutex_init(&queue->lock, NULL);
    pthread_mutex_lock(&fake_lock);
    CHECK(!fake_current, "a second queue was opened while one is open");
    fake_current = queue;
    pthread_mutex_unlock(&fake_lock);
    *out = (AudioQueueRef)queue;
    return noErr;
}

OSStatus AudioQueueAddPropertyListener(AudioQueueRef ref, AudioQueuePropertyID id, AudioQueuePropertyListenerProc proc,
                                       void *user)
{
    FakeQueue *queue = (FakeQueue *)ref;
    CHECK(id == kAudioQueueProperty_IsRunning, "listener property");
    queue->listener = proc;
    queue->listener_user = user;
    return noErr;
}

OSStatus AudioQueueGetProperty(AudioQueueRef ref, AudioQueuePropertyID id, void *data, UInt32 *size)
{
    FakeQueue *queue = (FakeQueue *)ref;
    CHECK(id == kAudioQueueProperty_IsRunning && *size == sizeof(UInt32), "get property");
    *(UInt32 *)data = (UInt32)atomic_load(&queue->running);
    return noErr;
}

OSStatus AudioQueueAllocateBuffer(AudioQueueRef ref, UInt32 bytes, AudioQueueBufferRef *out)
{
    FakeQueue *queue = (FakeQueue *)ref;
    CHECK(queue->allocated < FAKE_BUFFERS, "too many buffers");
    FakeBufferLayout layout = { bytes, calloc(1, bytes), 0, NULL, 0, NULL, 0 };
    AudioQueueBufferRef buffer = malloc(sizeof(AudioQueueBuffer));
    memcpy(buffer, &layout, sizeof(layout));
    queue->buffers[queue->allocated++] = buffer;
    *out = buffer;
    return noErr;
}

OSStatus AudioQueueEnqueueBuffer(AudioQueueRef ref, AudioQueueBufferRef buffer, UInt32 packets,
                                 const AudioStreamPacketDescription *descriptions)
{
    (void)packets; (void)descriptions;
    FakeQueue *queue = (FakeQueue *)ref;
    pthread_mutex_lock(&queue->lock);
    CHECK(queue->ready_count < FAKE_BUFFERS, "enqueue overflow");
    queue->ready[queue->ready_count++] = buffer;
    pthread_mutex_unlock(&queue->lock);
    return noErr;
}

OSStatus AudioQueueStart(AudioQueueRef ref, const AudioTimeStamp *start)
{
    (void)start;
    FakeQueue *queue = (FakeQueue *)ref;
    atomic_fetch_add(&fake_start_calls, 1);
    atomic_store(&queue->running, 1);
    queue->thread_started = 1;
    pthread_create(&queue->thread, NULL, FakeCapture, queue);
    if (queue->listener)
        queue->listener(queue->listener_user, ref, kAudioQueueProperty_IsRunning);
    return noErr;
}

OSStatus AudioQueueStop(AudioQueueRef ref, Boolean immediate)
{
    FakeQueue *queue = (FakeQueue *)ref;
    CHECK(immediate, "stop must be immediate");
    FakeStopThread(queue);
    int was = atomic_exchange(&queue->running, 0);
    if (was && queue->listener)
        queue->listener(queue->listener_user, ref, kAudioQueueProperty_IsRunning);
    return noErr;
}

OSStatus AudioQueueDispose(AudioQueueRef ref, Boolean immediate)
{
    (void)immediate;
    FakeQueue *queue = (FakeQueue *)ref;
    FakeStopThread(queue);
    pthread_mutex_lock(&fake_lock);
    CHECK(fake_current == queue, "disposed an unknown queue");
    fake_current = NULL;
    pthread_mutex_unlock(&fake_lock);
    for (int i = 0; i < queue->allocated; ++i) {
        free(queue->buffers[i]->mAudioData);
        free(queue->buffers[i]);
    }
    pthread_mutex_destroy(&queue->lock);
    free(queue);
    atomic_fetch_add(&fake_dispose_calls, 1);
    return noErr;
}

OSStatus AudioObjectAddPropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                        AudioObjectPropertyListenerProc proc, void *user)
{
    CHECK(object == kAudioObjectSystemObject && address->mSelector == kAudioHardwarePropertyDefaultInputDevice,
          "system listener");
    pthread_mutex_lock(&fake_lock);
    fake_hal_listener = proc;
    fake_hal_user = user;
    ++fake_hal_added;
    pthread_mutex_unlock(&fake_lock);
    return noErr;
}

OSStatus AudioObjectRemovePropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                           AudioObjectPropertyListenerProc proc, void *user)
{
    (void)address; (void)user;
    CHECK(object == kAudioObjectSystemObject, "system listener removal");
    pthread_mutex_lock(&fake_lock);
    CHECK(fake_hal_listener == proc, "removed an unknown listener");
    fake_hal_listener = NULL;
    ++fake_hal_removed;
    pthread_mutex_unlock(&fake_lock);
    return noErr;
}

/* The system's default input changes, reported on a Core Audio thread. */
static void *FakeDefaultInputChanges(void *unused)
{
    (void)unused;
    atomic_fetch_add(&fake_default_device, 1);
    pthread_mutex_lock(&fake_lock);
    AudioObjectPropertyListenerProc proc = fake_hal_listener;
    void *user = fake_hal_user;
    pthread_mutex_unlock(&fake_lock);
    CHECK(proc, "no default-input listener is registered");
    AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
                                           kAudioObjectPropertyElementMain };
    proc(kAudioObjectSystemObject, 1, &address, user);
    return NULL;
}

/* The device goes away: the queue stops by itself and says so. */
static void *FakeDeviceLost(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&fake_lock);
    FakeQueue *queue = fake_current;
    pthread_mutex_unlock(&fake_lock);
    CHECK(queue, "no queue to lose");
    atomic_store(&queue->stop, 1);
    atomic_store(&queue->running, 0);
    if (queue->listener)
        queue->listener(queue->listener_user, (AudioQueueRef)queue, kAudioQueueProperty_IsRunning);
    return NULL;
}

static void OnOtherThread(void *(*body)(void *))
{
    pthread_t thread;
    pthread_create(&thread, NULL, body, NULL);
    pthread_join(thread, NULL);
}

/* ---- Helpers ---------------------------------------------------------- */

static int QueueOpen(void)
{
    pthread_mutex_lock(&fake_lock);
    int open = fake_current != NULL && atomic_load(&fake_current->running);
    pthread_mutex_unlock(&fake_lock);
    return open;
}

static int QueueDevice(void)
{
    pthread_mutex_lock(&fake_lock);
    int device = fake_current ? fake_current->device : 0;
    pthread_mutex_unlock(&fake_lock);
    return device;
}

static double frame_worst;
/* Run game frames every 5 ms for the given time, or until the condition holds. */
static double RunFrames(double seconds, int (*until)(void))
{
    double start = Now();
    while (Now() - start < seconds) {
        double before = Now();
        DSOUNDRecord_Frame();
        double spent = Now() - before;
        if (spent > frame_worst)
            frame_worst = spent;
        if (until && until())
            return Now() - start;
        SleepMs(5);
    }
    return until ? -1.0 : seconds;
}

static int IsOpen(void) { return QueueOpen(); }
static int IsClosed(void) { return !QueueOpen(); }

static void *sample;
static void Begin(int mute)
{
    atomic_store(&fake_talking, 0);
    CHECK(DSOUNDRecord_Init(1, NULL) == 1, "init");
    mixerSetRecordLevel("Mic", 0xffff);
    mixerSetMicrophoneMute(mute);
    sample = DSOUNDRecord_NewSample();
    CHECK(sample && DSOUNDRecord_Start(sample) == 0, "start");
}

static void End(void)
{
    double before = Now();
    CHECK(DSOUNDRecord_Stop(sample) == 0, "stop");
    double stop = Now() - before;
    CHECK(stop < 0.05, "Stop waited %.3f s", stop);
    DSOUNDRecord_Shutdown();
    mixerSetMicrophoneMute(1);
    CHECK(!fake_current, "the device is still open after shutdown");
    CHECK(fake_hal_added == fake_hal_removed, "default-input listener left registered");
}

/* ---- Cases ------------------------------------------------------------ */

static void PermissionRefused(void)
{
    int allowed = atomic_load(&fake_allowed_calls), opened = atomic_load(&fake_new_calls);
    atomic_store(&fake_allowed, 0);
    Begin(1);
    atomic_store(&fake_talking, 1);
    RunFrames(1.25, NULL);
    int asked = atomic_load(&fake_allowed_calls) - allowed;
    CHECK(asked >= 2 && asked <= 4, "asked %d times in 1.25 s, expected about one every 0.5 s", asked);
    CHECK(atomic_load(&fake_new_calls) == opened, "opened the device without permission");
    CHECK(sent_samples == 0, "sent sound without permission");
    /* Turned on in System Settings while talking: the retry opens it. */
    atomic_store(&fake_allowed, 1);
    double took = RunFrames(1.0, IsOpen);
    CHECK(took >= 0 && took <= 0.7, "opened %.3f s after permission", took);
    RunFrames(0.2, NULL);
    CHECK(sent_samples > 0, "no sound after permission");
    atomic_store(&fake_talking, 0);
    End();
    printf("permission_refused asked=%d reopen_after=%.3f\n", asked, took);
}

static void SlowOpenDoesNotStallFrames(void)
{
    atomic_store(&fake_allowed, 1);
    atomic_store(&fake_open_delay_ms, 400);
    Begin(1);
    frame_worst = 0;
    atomic_store(&fake_talking, 1);
    double took = RunFrames(2.0, IsOpen);
    double worst = frame_worst;
    CHECK(took >= 0.35, "open finished in %.3f s although it takes 0.4 s", took);
    CHECK(worst < 0.02, "a frame waited %.3f s for the open", worst);
    /* A stop during a slow open returns at once; shutdown waits for it. */
    atomic_store(&fake_talking, 0);
    End();
    Begin(1);
    atomic_store(&fake_talking, 1);
    RunFrames(0.1, NULL);
    End();
    atomic_store(&fake_open_delay_ms, 0);
    printf("slow_open open_after=%.3f worst_frame_ms=%.2f\n", took, worst * 1000.0);
}

static void LingerAndRelease(void)
{
    atomic_store(&fake_allowed, 1);
    Begin(1);
    sent_expected = -1; sent_gaps = 0;
    atomic_store(&fake_talking, 1);
    CHECK(RunFrames(1.0, IsOpen) >= 0, "did not open");
    RunFrames(0.3, NULL);
    atomic_store(&fake_talking, 0);
    double closed = RunFrames(4.0, IsClosed);
    CHECK(closed >= 1.9 && closed <= 2.4, "closed %.3f s after release, expected 2 s", closed);
    CHECK(sent_gaps == 0, "%d gaps in the captured sound", sent_gaps);
    /* Not talking: never opened. */
    int opened = atomic_load(&fake_new_calls);
    RunFrames(0.6, NULL);
    CHECK(atomic_load(&fake_new_calls) == opened, "opened while nobody talks");
    End();
    printf("linger closed_after=%.3f\n", closed);
}

static void OpenFailureRetries(void)
{
    atomic_store(&fake_allowed, 1);
    atomic_store(&fake_open_failures, 2);
    int opened = atomic_load(&fake_new_calls);
    Begin(1);
    atomic_store(&fake_talking, 1);
    double took = RunFrames(2.0, IsOpen);
    CHECK(took >= 0.9 && took <= 1.4, "opened %.3f s after two failures, expected about 1 s", took);
    CHECK(atomic_load(&fake_new_calls) - opened == 3, "tried %d times", atomic_load(&fake_new_calls) - opened);
    atomic_store(&fake_talking, 0);
    End();
    printf("open_failure open_after=%.3f\n", took);
}

static void DeviceLostWhileTalking(void)
{
    atomic_store(&fake_allowed, 1);
    Begin(1);
    atomic_store(&fake_talking, 1);
    CHECK(RunFrames(1.0, IsOpen) >= 0, "did not open");
    RunFrames(0.6, NULL);
    int disposed = atomic_load(&fake_dispose_calls), opened = atomic_load(&fake_new_calls);
    OnOtherThread(FakeDeviceLost);
    double took = RunFrames(1.5, IsOpen);
    CHECK(took >= 0 && took <= 0.7, "reopened %.3f s after the device went away", took);
    CHECK(atomic_load(&fake_dispose_calls) == disposed + 1 && atomic_load(&fake_new_calls) == opened + 1,
          "lost device not closed and reopened once");
    long before = sent_samples;
    RunFrames(0.2, NULL);
    CHECK(sent_samples > before, "no sound after reopening");
    atomic_store(&fake_talking, 0);
    End();
    printf("device_lost reopen_after=%.3f\n", took);
}

static void DefaultInputChangesWhileTalking(void)
{
    atomic_store(&fake_allowed, 1);
    Begin(1);
    atomic_store(&fake_talking, 1);
    CHECK(RunFrames(1.0, IsOpen) >= 0, "did not open");
    RunFrames(0.6, NULL);
    int device = QueueDevice();
    OnOtherThread(FakeDefaultInputChanges);
    double reopened = -1;
    /* Wait for a queue on the new device. */
    double start = Now();
    while (Now() - start < 1.5) {
        DSOUNDRecord_Frame();
        if (QueueOpen() && QueueDevice() == device + 1) { reopened = Now() - start; break; }
        SleepMs(5);
    }
    CHECK(reopened >= 0 && reopened <= 0.7, "still on device %d %.3f s after the default changed", QueueDevice(),
          Now() - start);
    /* A second change right after the reopen is not held back by the retry
     * time, which is only for failed opens. */
    OnOtherThread(FakeDefaultInputChanges);
    start = Now();
    double again = -1;
    while (Now() - start < 1.0) {
        DSOUNDRecord_Frame();
        if (QueueOpen() && QueueDevice() == device + 2) { again = Now() - start; break; }
        SleepMs(5);
    }
    CHECK(again >= 0 && again <= 0.2, "second change reopened after %.3f s", again);
    ++device;
    /* Changed while not talking: closed now, and the next talk opens the new one. */
    atomic_store(&fake_talking, 0);
    CHECK(RunFrames(3.0, IsClosed) >= 0, "did not close after release");
    OnOtherThread(FakeDefaultInputChanges);
    int opened = atomic_load(&fake_new_calls);
    RunFrames(0.6, NULL);
    CHECK(atomic_load(&fake_new_calls) == opened, "a change while closed opened the device");
    atomic_store(&fake_talking, 1);
    CHECK(RunFrames(1.0, IsOpen) >= 0 && QueueDevice() == device + 2, "next talk not on the new device");
    atomic_store(&fake_talking, 0);
    End();
    printf("default_input_changed reopen_after=%.3f second_change_reopen_after=%.3f\n", reopened, again);
}

static void MuteSetting(void)
{
    atomic_store(&fake_allowed, 1);
    /* winvoice_mic_mute 1, the default: nothing played back to the player. */
    int created = monitor_created;
    Begin(1);
    atomic_store(&fake_talking, 1);
    CHECK(RunFrames(1.0, IsOpen) >= 0, "did not open");
    long sent = sent_samples;
    RunFrames(0.3, NULL);
    CHECK(sent_samples > sent, "muted: nothing sent");
    CHECK(monitor_created == created && monitor_bytes == 0, "muted: played the microphone back");
    atomic_store(&fake_talking, 0);
    End();
    /* winvoice_mic_mute 0: the player hears the microphone; sending is unchanged. */
    Begin(0);
    CHECK(monitor_created == created + 1, "unmuted: no playback voice");
    atomic_store(&fake_talking, 1);
    CHECK(RunFrames(1.0, IsOpen) >= 0, "did not open");
    sent = sent_samples;
    long heard = monitor_bytes;
    RunFrames(0.3, NULL);
    CHECK(sent_samples > sent, "unmuted: nothing sent");
    CHECK(monitor_bytes - heard == (sent_samples - sent) * 2, "unmuted: heard %ld bytes, sent %ld samples",
          monitor_bytes - heard, sent_samples - sent);
    atomic_store(&fake_talking, 0);
    int destroyed = monitor_destroyed;
    End();
    CHECK(monitor_destroyed == destroyed + 1, "unmuted: playback voice not released");
    printf("mute muted_monitor_bytes=0 unmuted_monitor_matches_sent=yes\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    PermissionRefused();
    SlowOpenDoesNotStallFrames();
    LingerAndRelease();
    OpenFailureRetries();
    DeviceLostWhileTalking();
    DefaultInputChangesWhileTalking();
    MuteSetting();
    CHECK(atomic_load(&fake_start_calls) == atomic_load(&fake_dispose_calls), "a started queue was not disposed");
    printf("passed opens=%d starts=%d disposes=%d\n", atomic_load(&fake_new_calls), atomic_load(&fake_start_calls),
           atomic_load(&fake_dispose_calls));
    return 0;
}
