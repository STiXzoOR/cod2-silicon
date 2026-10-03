#include "macos_audio.h"

#if defined(__APPLE__) && defined(COD2_X64)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AUDIO_RATE 48000
#define AUDIO_SAMPLES 256
#define AUDIO_MAX_BYTES (256u * 1024u * 1024u)
#define AUDIO_DRIVER ((void *)(uintptr_t)1)
#define AUDIO_PROVIDER ((void *)(uintptr_t)2)
#define AUDIO_DONE 2
#define AUDIO_PLAYING 4
#define AUDIO_STOPPED 8
#define REVERB_FRAMES 8192

typedef struct {
    int allocated, status, format, rate, channels, block_size;
    int spatial, loops, remaining_loops, buffered, active_buffer, prepared;
    const void *source;
    size_t source_bytes;
    float *pcm[2];
    size_t frames[2];
    double cursor;
    float left, right, dry, wet, position[3], min_distance, max_distance;
} MacAudioSample;

typedef struct {
    const unsigned char *data;
    size_t size;
} MacAudioFile;

static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static AudioUnit output_unit;
static MacAudioSample samples[AUDIO_SAMPLES];
static int driver_rate = 44100;
static int preferences[64];
static float listener[3], rolloff = 1.0f, distance_factor = 1.0f;
static float master_dry = 1.0f, master_wet = 0.0f;
static float reverb[REVERB_FRAMES * 2];
static size_t reverb_cursor;
static int reverb_delay = 4093;
static char last_error[256];
static MacAudioOpenCallback file_open;
static MacAudioCloseCallback file_close;
static MacAudioSeekCallback file_seek;
static MacAudioReadCallback file_read;
static atomic_uint_fast64_t output_callbacks, output_frames, audible_frames, lock_misses;
static _Atomic(float) output_peak;

static int AudioError(const char *operation, OSStatus error)
{
    snprintf(last_error, sizeof(last_error), "%s: OSStatus %d", operation, (int)error);
    return 0;
}

static float Clamp(float value, float min, float max)
{
    return isfinite(value) ? fmaxf(min, fminf(max, value)) : min;
}

static unsigned int Read16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int Read32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static void Write16(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void Write32(unsigned char *p, unsigned int value)
{
    Write16(p, value);
    Write16(p + 2, value >> 16);
}

static MacAudioSample *FindSample(void *handle)
{
    for (int i = 0; i < AUDIO_SAMPLES; ++i) {
        if (handle == &samples[i] && samples[i].allocated)
            return &samples[i];
    }
    return NULL;
}

static void ResetSample(MacAudioSample *sample)
{
    int spatial = sample->spatial;
    free(sample->pcm[0]);
    free(sample->pcm[1]);
    memset(sample, 0, sizeof(*sample));
    sample->allocated = 1;
    sample->spatial = spatial;
    sample->status = AUDIO_DONE;
    sample->rate = driver_rate;
    sample->channels = 1;
    sample->left = sample->right = sample->dry = 1.0f;
    sample->loops = sample->remaining_loops = 1;
    sample->min_distance = 1.0f;
    sample->max_distance = 100000.0f;
}

static OSStatus FileRead(void *client, SInt64 position, UInt32 count, void *buffer, UInt32 *actual)
{
    MacAudioFile *file = client;
    *actual = 0;
    if (position < 0 || (uint64_t)position > file->size)
        return kAudioFilePositionError;
    size_t available = file->size - (size_t)position;
    if (count > available)
        count = (UInt32)available;
    memcpy(buffer, file->data + (size_t)position, count);
    *actual = count;
    return noErr;
}

static SInt64 FileSize(void *client)
{
    return (SInt64)((MacAudioFile *)client)->size;
}

static int DecodeFile(const void *data, size_t size, float **pcm, size_t *frames, int *channels, int *rate)
{
    AudioFileID file = NULL;
    ExtAudioFileRef decoder = NULL;
    MacAudioFile memory = { data, size };
    OSStatus error = AudioFileOpenWithCallbacks(&memory, FileRead, NULL, FileSize, NULL, 0, &file);
    if (error)
        return AudioError("AudioFileOpenWithCallbacks", error);
    error = ExtAudioFileWrapAudioFileID(file, false, &decoder);
    if (error) {
        AudioFileClose(file);
        return AudioError("ExtAudioFileWrapAudioFileID", error);
    }
    AudioStreamBasicDescription input = { 0 };
    UInt32 property_size = sizeof(input);
    error = ExtAudioFileGetProperty(decoder, kExtAudioFileProperty_FileDataFormat, &property_size, &input);
    if (!error && (input.mChannelsPerFrame < 1 || input.mChannelsPerFrame > 2 || input.mSampleRate < 1000 || input.mSampleRate > 384000))
        error = kAudio_ParamError;
    AudioStreamBasicDescription output = {
        .mSampleRate = input.mSampleRate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked,
        .mBytesPerPacket = input.mChannelsPerFrame * sizeof(float),
        .mFramesPerPacket = 1,
        .mBytesPerFrame = input.mChannelsPerFrame * sizeof(float),
        .mChannelsPerFrame = input.mChannelsPerFrame,
        .mBitsPerChannel = 32
    };
    if (!error)
        error = ExtAudioFileSetProperty(decoder, kExtAudioFileProperty_ClientDataFormat, sizeof(output), &output);
    SInt64 length = 0;
    property_size = sizeof(length);
    if (!error)
        error = ExtAudioFileGetProperty(decoder, kExtAudioFileProperty_FileLengthFrames, &property_size, &length);
    if (!error && (length < 0 || (uint64_t)length > AUDIO_MAX_BYTES / output.mBytesPerFrame))
        error = kAudio_ParamError;
    size_t capacity = length > 0 ? (size_t)length : 4096;
    float *result = !error ? malloc(capacity * output.mBytesPerFrame) : NULL;
    if (!error && !result)
        error = -ENOMEM;
    size_t used = 0;
    while (!error) {
        if (used == capacity) {
            if (capacity >= AUDIO_MAX_BYTES / output.mBytesPerFrame) {
                error = -ENOMEM;
                break;
            }
            size_t new_capacity = capacity * 2;
            if (new_capacity > AUDIO_MAX_BYTES / output.mBytesPerFrame)
                new_capacity = AUDIO_MAX_BYTES / output.mBytesPerFrame;
            float *resized = realloc(result, new_capacity * output.mBytesPerFrame);
            if (!resized) {
                error = -ENOMEM;
                break;
            }
            result = resized;
            capacity = new_capacity;
        }
        UInt32 read_frames = (UInt32)fmin((double)(capacity - used), 4096.0);
        AudioBufferList buffers = { .mNumberBuffers = 1, .mBuffers = {{ output.mChannelsPerFrame, read_frames * output.mBytesPerFrame, result + used * output.mChannelsPerFrame }} };
        error = ExtAudioFileRead(decoder, &read_frames, &buffers);
        used += read_frames;
        if (!read_frames)
            break;
    }
    ExtAudioFileDispose(decoder);
    AudioFileClose(file);
    if (error) {
        free(result);
        return AudioError("ExtAudioFile decode", error);
    }
    *pcm = result;
    *frames = used;
    *channels = (int)output.mChannelsPerFrame;
    *rate = (int)output.mSampleRate;
    return 1;
}

static int DecodeInfo(const MacAudioSoundInfo *info, float **pcm, size_t *frames, int *channels, int *rate)
{
    if (!info || !info->data_ptr || info->data_len > AUDIO_MAX_BYTES || info->channels < 1 || info->channels > 2 || info->rate < 1000 || info->rate > 384000)
        return AudioError("Invalid sample metadata", kAudio_ParamError);
    if (info->format != 1 && info->format != 3 && info->format != 0x11)
        return AudioError("Unsupported WAV format", kAudioFileUnsupportedDataFormatError);
    if ((info->format == 1 && info->bits != 8 && info->bits != 16 && info->bits != 24 && info->bits != 32) || (info->format == 3 && info->bits != 32))
        return AudioError("Unsupported PCM bit depth", kAudioFileUnsupportedDataFormatError);
    if (info->format != 0x11) {
        size_t bytes_per_sample = (size_t)info->bits / 8;
        size_t alignment = (size_t)info->channels * bytes_per_sample;
        size_t frame_count = info->data_len / alignment;
        if (info->data_len % alignment || frame_count > AUDIO_MAX_BYTES / sizeof(float) / info->channels)
            return AudioError("Invalid PCM sample size", kAudio_ParamError);
        float *result = malloc((frame_count ? frame_count : 1) * info->channels * sizeof(float));
        if (!result)
            return AudioError("PCM allocation", -ENOMEM);
        const unsigned char *source = info->data_ptr;
        for (size_t i = 0; i < frame_count * info->channels; ++i) {
            const unsigned char *value = source + i * bytes_per_sample;
            if (info->format == 3) {
                uint32_t bits = Read32(value);
                memcpy(&result[i], &bits, sizeof(float));
                result[i] = Clamp(result[i], -1, 1);
            } else if (info->bits == 8) {
                result[i] = ((int)value[0] - 128) / 128.0f;
            } else if (info->bits == 16) {
                result[i] = (int16_t)Read16(value) / 32768.0f;
            } else if (info->bits == 24) {
                int32_t sample = (int32_t)((uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16));
                if (sample & 0x800000)
                    sample |= (int32_t)0xff000000;
                result[i] = sample / 8388608.0f;
            } else {
                result[i] = (int32_t)Read32(value) / 2147483648.0f;
            }
        }
        *pcm = result;
        *frames = frame_count;
        *channels = (int)info->channels;
        *rate = (int)info->rate;
        return 1;
    }
    size_t header_size = info->format == 0x11 ? 60 : 44;
    size_t wave_size = header_size + info->data_len + (info->data_len & 1);
    unsigned char *wave = calloc(1, wave_size);
    if (!wave)
        return AudioError("WAV allocation", -ENOMEM);
    memcpy(wave, "RIFF", 4);
    Write32(wave + 4, (unsigned int)(wave_size - 8));
    memcpy(wave + 8, "WAVEfmt ", 8);
    Write32(wave + 16, info->format == 0x11 ? 20 : 16);
    Write16(wave + 20, (unsigned int)info->format);
    Write16(wave + 22, (unsigned int)info->channels);
    Write32(wave + 24, (unsigned int)info->rate);
    unsigned int alignment = (unsigned int)(info->channels * info->bits / 8);
    unsigned int samples_per_block = 1;
    if (info->format == 0x11) {
        alignment = (unsigned int)info->block_size;
        if (alignment < (unsigned int)info->channels * 4 || alignment > 65535) {
            free(wave);
            return AudioError("Invalid IMA ADPCM block size", kAudio_ParamError);
        }
        samples_per_block = (alignment - 4 * (unsigned int)info->channels) * 2 / (unsigned int)info->channels + 1;
        Write16(wave + 36, 2);
        Write16(wave + 38, samples_per_block);
        memcpy(wave + 40, "fact", 4);
        Write32(wave + 44, 4);
        Write32(wave + 48, (unsigned int)(info->samples ? info->samples : info->data_len / alignment * samples_per_block));
    }
    Write32(wave + 28, (unsigned int)(info->rate * alignment / samples_per_block));
    Write16(wave + 32, alignment);
    Write16(wave + 34, (unsigned int)info->bits);
    memcpy(wave + header_size - 8, "data", 4);
    Write32(wave + header_size - 4, (unsigned int)info->data_len);
    memcpy(wave + header_size, info->data_ptr, info->data_len);
    int result = DecodeFile(wave, wave_size, pcm, frames, channels, rate);
    free(wave);
    return result;
}

static MacAudioSoundInfo SampleInfo(MacAudioSample *sample, const void *data, size_t bytes)
{
    int channels = (sample->format & 2) ? 2 : 1;
    int bits = (sample->format & 8) ? 32 : ((sample->format & 1) ? 16 : 8);
    MacAudioSoundInfo info = {
        .format = (sample->format & 4) ? 0x11 : 1,
        .data_ptr = data, .data_len = bytes, .rate = (unsigned long)sample->rate,
        .bits = (sample->format & 4) ? 4 : bits, .channels = channels,
        .block_size = (unsigned long)sample->block_size, .initial_ptr = data
    };
    return info;
}

static void PrepareSample(MacAudioSample *sample)
{
    if (sample->prepared || sample->buffered || !sample->source_bytes)
        return;
    MacAudioSoundInfo info = SampleInfo(sample, sample->source, sample->source_bytes);
    free(sample->pcm[0]);
    sample->pcm[0] = NULL;
    sample->frames[0] = 0;
    int rate;
    sample->prepared = DecodeInfo(&info, &sample->pcm[0], &sample->frames[0], &sample->channels, &rate);
}

static void MixLocked(float *stereo, size_t frame_count)
{
    memset(stereo, 0, frame_count * 2 * sizeof(float));
    float wet_buffer[1024 * 2];
    for (size_t chunk = 0; chunk < frame_count; chunk += 1024) {
        size_t count = frame_count - chunk;
        if (count > 1024)
            count = 1024;
        memset(wet_buffer, 0, count * 2 * sizeof(float));
        for (int n = 0; n < AUDIO_SAMPLES; ++n) {
            MacAudioSample *sample = &samples[n];
            if (!sample->allocated || sample->status != AUDIO_PLAYING)
                continue;
            float left = sample->left, right = sample->right;
            if (sample->spatial) {
                float x = sample->position[0] - listener[0];
                float y = sample->position[1] - listener[1];
                float z = sample->position[2] - listener[2];
                float distance = sqrtf(x * x + y * y + z * z);
                float pan = distance > 0 ? Clamp(x / distance, -1, 1) : 0;
                float attenuation = 1.0f;
                if (rolloff > 0 && distance * distance_factor > sample->min_distance) {
                    float range = fmaxf(sample->max_distance - sample->min_distance, 1);
                    attenuation = powf(Clamp(1 - (distance * distance_factor - sample->min_distance) / range, 0, 1), rolloff);
                }
                left *= sqrtf((1 - pan) * 0.5f) * attenuation;
                right *= sqrtf((1 + pan) * 0.5f) * attenuation;
            }
            for (size_t i = 0; i < count; ++i) {
                int index = sample->active_buffer;
                size_t length = sample->frames[index];
                if (sample->cursor >= length) {
                    if (sample->buffered) {
                        sample->frames[index] = 0;
                        sample->cursor -= length;
                        index = sample->active_buffer = 1 - index;
                        length = sample->frames[index];
                        if (!length) {
                            sample->cursor = 0;
                            sample->status = AUDIO_DONE;
                            break;
                        }
                    } else if (length && (sample->loops == 0 || sample->remaining_loops > 1)) {
                        if (sample->remaining_loops > 1)
                            --sample->remaining_loops;
                        sample->cursor = fmod(sample->cursor, (double)length);
                    } else {
                        sample->status = AUDIO_DONE;
                        break;
                    }
                }
                size_t current = (size_t)sample->cursor;
                size_t next = current + 1 < length ? current + 1 : current;
                float fraction = (float)(sample->cursor - current);
                float l = sample->pcm[index][current * sample->channels];
                l += (sample->pcm[index][next * sample->channels] - l) * fraction;
                float r = l;
                if (sample->channels == 2) {
                    r = sample->pcm[index][current * 2 + 1];
                    r += (sample->pcm[index][next * 2 + 1] - r) * fraction;
                }
                size_t output = (chunk + i) * 2;
                stereo[output] += l * left * sample->dry;
                stereo[output + 1] += r * right * sample->dry;
                wet_buffer[i * 2] += l * left * sample->wet;
                wet_buffer[i * 2 + 1] += r * right * sample->wet;
                sample->cursor += (double)sample->rate / AUDIO_RATE;
            }
        }
        for (size_t i = 0; i < count; ++i) {
            size_t delayed = (reverb_cursor + REVERB_FRAMES - (size_t)reverb_delay) % REVERB_FRAMES;
            float l = reverb[delayed * 2], r = reverb[delayed * 2 + 1];
            size_t output = (chunk + i) * 2;
            reverb[reverb_cursor * 2] = wet_buffer[i * 2] + l * 0.45f;
            reverb[reverb_cursor * 2 + 1] = wet_buffer[i * 2 + 1] + r * 0.45f;
            stereo[output] = Clamp(stereo[output] * master_dry + l * master_wet, -1, 1);
            stereo[output + 1] = Clamp(stereo[output + 1] * master_dry + r * master_wet, -1, 1);
            reverb_cursor = (reverb_cursor + 1) % REVERB_FRAMES;
        }
    }
}

void MacAudio_Mix(float *stereo, size_t frames)
{
    pthread_mutex_lock(&audio_mutex);
    MixLocked(stereo, frames);
    pthread_mutex_unlock(&audio_mutex);
}

static OSStatus Render(void *refcon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *timestamp, UInt32 bus, UInt32 frames, AudioBufferList *buffers)
{
    (void)refcon; (void)timestamp; (void)bus;
    atomic_fetch_add_explicit(&output_callbacks, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&output_frames, frames, memory_order_relaxed);
    for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i)
        memset(buffers->mBuffers[i].mData, 0, buffers->mBuffers[i].mDataByteSize);
    if (buffers->mNumberBuffers != 1 || buffers->mBuffers[0].mNumberChannels != 2 || buffers->mBuffers[0].mDataByteSize < frames * 2 * sizeof(float))
        return kAudio_ParamError;
    if (pthread_mutex_trylock(&audio_mutex)) {
        atomic_fetch_add_explicit(&lock_misses, 1, memory_order_relaxed);
        *flags |= kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }
    float *pcm = buffers->mBuffers[0].mData;
    MixLocked(pcm, frames);
    pthread_mutex_unlock(&audio_mutex);
    float peak = 0;
    uint64_t audible = 0;
    for (UInt32 i = 0; i < frames; ++i) {
        float value = fmaxf(fabsf(pcm[i * 2]), fabsf(pcm[i * 2 + 1]));
        peak = fmaxf(peak, value);
        audible += value > 0.00001f;
    }
    atomic_fetch_add_explicit(&audible_frames, audible, memory_order_relaxed);
    if (peak > atomic_load_explicit(&output_peak, memory_order_relaxed))
        atomic_store_explicit(&output_peak, peak, memory_order_relaxed);
    if (!audible)
        *flags |= kAudioUnitRenderAction_OutputIsSilence;
    return noErr;
}

void MacAudio_GetStats(MacAudioStats *stats)
{
    stats->callbacks = atomic_load(&output_callbacks);
    stats->frames = atomic_load(&output_frames);
    stats->audible_frames = atomic_load(&audible_frames);
    stats->lock_misses = atomic_load(&lock_misses);
    stats->peak = atomic_load(&output_peak);
}

int AIL_startup(int flags)
{
    (void)flags;
    last_error[0] = 0;
    return 1;
}

void *AIL_open_digital_driver(int frequency, int bits, int channels, int flags)
{
    (void)flags;
    if (frequency < 1000 || frequency > 384000 || (bits != 8 && bits != 16 && bits != 32) || channels < 1 || channels > 2) {
        AudioError("Invalid digital driver format", kAudio_ParamError);
        return NULL;
    }
    driver_rate = frequency;
    if (output_unit)
        return AUDIO_DRIVER;
    AudioComponentDescription description = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple
    };
    AudioComponent component = AudioComponentFindNext(NULL, &description);
    if (!component) {
        AudioError("Default output AudioUnit unavailable", kAudio_ParamError);
        return NULL;
    }
    OSStatus error = AudioComponentInstanceNew(component, &output_unit);
    AudioStreamBasicDescription format = {
        .mSampleRate = AUDIO_RATE, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked,
        .mBytesPerPacket = 8, .mFramesPerPacket = 1, .mBytesPerFrame = 8,
        .mChannelsPerFrame = 2, .mBitsPerChannel = 32
    };
    AURenderCallbackStruct callback = { Render, NULL };
    if (!error)
        error = AudioUnitSetProperty(output_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));
    if (!error)
        error = AudioUnitSetProperty(output_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback));
    if (!error)
        error = AudioUnitInitialize(output_unit);
    if (!error)
        error = AudioOutputUnitStart(output_unit);
    if (error) {
        if (output_unit)
            AudioComponentInstanceDispose(output_unit);
        output_unit = NULL;
        AudioError("AudioUnit output initialization", error);
        return NULL;
    }
    return AUDIO_DRIVER;
}

void AIL_shutdown(void)
{
    if (output_unit) {
        AudioOutputUnitStop(output_unit);
        AudioUnitUninitialize(output_unit);
        AudioComponentInstanceDispose(output_unit);
        output_unit = NULL;
    }
    pthread_mutex_lock(&audio_mutex);
    for (int i = 0; i < AUDIO_SAMPLES; ++i) {
        free(samples[i].pcm[0]);
        free(samples[i].pcm[1]);
    }
    memset(samples, 0, sizeof(samples));
    memset(reverb, 0, sizeof(reverb));
    reverb_cursor = 0;
    pthread_mutex_unlock(&audio_mutex);
}

int AIL_set_preference(int number, int value)
{
    if (number < 0 || number >= (int)(sizeof(preferences) / sizeof(preferences[0])))
        return 0;
    int old = preferences[number];
    preferences[number] = value;
    return old;
}

const char *AIL_last_error(void) { return last_error; }
void AIL_set_redist_directory(const char *directory) { (void)directory; }
int AIL_digital_CPU_percent(void *driver)
{
    (void)driver;
    Float32 load = 0;
    UInt32 size = sizeof(load);
    if (output_unit)
        AudioUnitGetProperty(output_unit, kAudioUnitProperty_CPULoad, kAudioUnitScope_Global, 0, &load, &size);
    return (int)(Clamp(load, 0, 1) * 100);
}

void *AIL_allocate_sample_handle(void *driver)
{
    if (driver != AUDIO_DRIVER)
        return NULL;
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = NULL;
    for (int i = 0; i < AUDIO_SAMPLES; ++i) {
        if (!samples[i].allocated) {
            sample = &samples[i];
            ResetSample(sample);
            break;
        }
    }
    pthread_mutex_unlock(&audio_mutex);
    if (!sample)
        AudioError("Audio sample limit reached", -ENOMEM);
    return sample;
}

void AIL_release_sample_handle(void *handle)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample) {
        ResetSample(sample);
        sample->allocated = 0;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_init_sample(void *handle)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample)
        ResetSample(sample);
    pthread_mutex_unlock(&audio_mutex);
}

#define SAMPLE_SETTER(name, args, body) \
    void name args { \
        pthread_mutex_lock(&audio_mutex); \
        MacAudioSample *sample = FindSample(handle); \
        if (sample) { body; } \
        pthread_mutex_unlock(&audio_mutex); \
    }
#define SAMPLE_GETTER(type, name, fallback, expression) \
    type name(void *handle) { \
        pthread_mutex_lock(&audio_mutex); \
        MacAudioSample *sample = FindSample(handle); \
        type result = sample ? (expression) : (fallback); \
        pthread_mutex_unlock(&audio_mutex); \
        return result; \
    }

SAMPLE_SETTER(AIL_set_sample_type, (void *handle, int format, int flags), (void)flags; sample->format = format; sample->prepared = 0)
SAMPLE_SETTER(AIL_set_sample_adpcm_block_size, (void *handle, int size), sample->block_size = size; sample->prepared = 0)
SAMPLE_SETTER(AIL_set_sample_playback_rate, (void *handle, int rate), sample->rate = rate > 0 ? rate : driver_rate)
SAMPLE_GETTER(int, AIL_sample_playback_rate, 0, sample->rate)
SAMPLE_SETTER(AIL_stop_sample, (void *handle), sample->status = AUDIO_STOPPED)
SAMPLE_SETTER(AIL_end_sample, (void *handle), sample->status = AUDIO_DONE; sample->cursor = 0)
SAMPLE_GETTER(unsigned long, AIL_sample_status, 1, (unsigned long)sample->status)
SAMPLE_SETTER(AIL_set_sample_volume_levels, (void *handle, float left, float right), sample->left = Clamp(left, 0, 4); sample->right = Clamp(right, 0, 4))
SAMPLE_SETTER(AIL_set_sample_reverb_levels, (void *handle, float dry, float wet), sample->dry = Clamp(dry, 0, 1); sample->wet = Clamp(wet, 0, 1))
SAMPLE_SETTER(AIL_set_sample_loop_count, (void *handle, int count), sample->loops = sample->remaining_loops = count > 0 ? count : 0)
SAMPLE_SETTER(AIL_set_sample_ms_position, (void *handle, int ms), sample->cursor = fmax(0, (double)ms * sample->rate / 1000.0))

void AIL_set_sample_address(void *handle, const void *data, int size)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample) {
        free(sample->pcm[0]); free(sample->pcm[1]);
        sample->pcm[0] = sample->pcm[1] = NULL;
        sample->frames[0] = sample->frames[1] = 0;
        sample->source = data;
        sample->source_bytes = size > 0 ? (size_t)size : 0;
        sample->prepared = sample->buffered = 0;
        sample->cursor = 0;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_resume_sample(void *handle)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample) {
        PrepareSample(sample);
        if (sample->frames[sample->active_buffer])
            sample->status = AUDIO_PLAYING;
    }
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_sample_volume_levels(void *handle, float *left, float *right)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (left) *left = sample ? sample->left : 0;
    if (right) *right = sample ? sample->right : 0;
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_sample_volume_pan(void *handle, float *volume, float *pan)
{
    float left, right;
    AIL_sample_volume_levels(handle, &left, &right);
    if (volume) *volume = fmaxf(left, right);
    if (pan) *pan = left + right ? right / (left + right) : 0.5f;
}

unsigned long AIL_sample_position(void *handle)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    unsigned long result = 0;
    if (sample) {
        size_t frames = sample->frames[sample->active_buffer];
        result = frames ? (unsigned long)(fmin(sample->cursor, frames) * sample->source_bytes / frames) : 0;
    }
    pthread_mutex_unlock(&audio_mutex);
    return result;
}

void AIL_sample_ms_position(void *handle, long *total, long *current)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample)
        PrepareSample(sample);
    if (total) *total = sample && sample->rate ? (long)(sample->frames[sample->active_buffer] * 1000.0 / sample->rate) : 0;
    if (current) *current = sample && sample->rate ? (long)(sample->cursor * 1000.0 / sample->rate) : 0;
    pthread_mutex_unlock(&audio_mutex);
}

int AIL_sample_buffer_ready(void *handle)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    int ready = -1;
    if (sample) {
        for (int i = 0; i < 2; ++i)
            if (!sample->frames[i]) { ready = i; break; }
    }
    pthread_mutex_unlock(&audio_mutex);
    return ready;
}

void AIL_load_sample_buffer(void *handle, int index, const void *data, int size)
{
    if (index < 0 || index > 1 || size < 0)
        return;
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample && !sample->frames[index]) {
        MacAudioSoundInfo info = SampleInfo(sample, data, (size_t)size);
        float *pcm = NULL;
        size_t frames = 0;
        int channels, rate;
        if (size && DecodeInfo(&info, &pcm, &frames, &channels, &rate)) {
            free(sample->pcm[index]);
            sample->pcm[index] = pcm;
            sample->frames[index] = frames;
            sample->channels = channels;
            sample->source_bytes = (size_t)size;
            sample->buffered = sample->prepared = 1;
            if (sample->status != AUDIO_PLAYING) {
                sample->active_buffer = index;
                sample->cursor = 0;
                sample->status = AUDIO_PLAYING;
            }
        }
    }
    pthread_mutex_unlock(&audio_mutex);
}

int AIL_minimum_sample_buffer_size(void *driver, int rate, int format)
{
    (void)driver;
    int bytes = ((format & 2) ? 2 : 1) * ((format & 1) ? 2 : 1);
    return rate > 0 ? (rate / 20 + 1) * bytes : 2048;
}

void AIL_set_digital_master_room_type(void *driver, int room)
{
    (void)driver;
    pthread_mutex_lock(&audio_mutex);
    reverb_delay = 2401 + (int)((unsigned int)room % 16) * 337;
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_set_digital_master_reverb_levels(void *driver, float dry, float wet)
{
    (void)driver;
    pthread_mutex_lock(&audio_mutex);
    master_dry = Clamp(dry, 0, 1);
    master_wet = Clamp(wet, 0, 1);
    pthread_mutex_unlock(&audio_mutex);
}

int AIL_WAV_info(const void *data, void *result)
{
    if (!data || !result)
        return 0;
    const unsigned char *wave = data;
    if (memcmp(wave, "RIFF", 4) || memcmp(wave + 8, "WAVE", 4))
        return AudioError("Not a RIFF WAVE file", kAudioFileUnsupportedFileTypeError);
    size_t length = (size_t)Read32(wave + 4) + 8;
    if (length < 12 || length > AUDIO_MAX_BYTES)
        return AudioError("Invalid RIFF length", kAudio_ParamError);
    MacAudioSoundInfo info = { 0 };
    for (size_t offset = 12; offset + 8 <= length;) {
        size_t bytes = Read32(wave + offset + 4);
        if (bytes > length - offset - 8)
            return AudioError("WAV chunk exceeds RIFF length", kAudio_ParamError);
        const unsigned char *chunk = wave + offset + 8;
        if (!memcmp(wave + offset, "fmt ", 4) && bytes >= 16) {
            info.format = Read16(chunk);
            info.channels = Read16(chunk + 2);
            info.rate = Read32(chunk + 4);
            info.block_size = Read16(chunk + 12);
            info.bits = Read16(chunk + 14);
            /* WAVE_FORMAT_EXTENSIBLE (used by some mod IWDs): the real format
               is the first word of a KSDATAFORMAT_SUBTYPE GUID. */
            static const unsigned char subtype_tail[14] = { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
                                                            0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };
            if (info.format == 0xfffe && bytes >= 40 && !memcmp(chunk + 26, subtype_tail, sizeof(subtype_tail)))
                info.format = Read16(chunk + 24);
        } else if (!memcmp(wave + offset, "data", 4)) {
            info.data_ptr = info.initial_ptr = chunk;
            info.data_len = bytes;
        } else if (!memcmp(wave + offset, "fact", 4) && bytes >= 4) {
            info.samples = Read32(chunk);
        }
        offset += 8 + bytes + (bytes & 1);
    }
    if (!info.data_ptr || !info.block_size || info.channels < 1 || info.channels > 2 || !info.rate || (info.format != 1 && info.format != 3 && info.format != 0x11))
        return AudioError("Unsupported or incomplete WAV", kAudioFileUnsupportedDataFormatError);
    if (!info.samples) {
        if (info.format == 0x11) {
            if (info.block_size < (unsigned long)info.channels * 4)
                return AudioError("Invalid IMA ADPCM header", kAudio_ParamError);
            info.samples = info.data_len / info.block_size * ((info.block_size - 4 * info.channels) * 2 / info.channels + 1);
        } else {
            info.samples = info.data_len / info.block_size;
        }
    }
    memcpy(result, &info, sizeof(info));
    return 1;
}

int AIL_size_processed_digital_audio(int rate, int format, int count, const void *mixinfo)
{
    if (rate <= 0 || count != 1 || !mixinfo)
        return AudioError("Unsupported digital mix request", kAudio_ParamError);
    const MacAudioSoundInfo *info = mixinfo;
    if (!info->rate)
        return 0;
    uint64_t frames = (uint64_t)ceil((double)info->samples * rate / info->rate);
    unsigned int bytes = ((format & 2) ? 2 : 1) * ((format & 8) ? 4 : ((format & 1) ? 2 : 1));
    if (frames > INT_MAX / bytes)
        return 0;
    return (int)(frames * bytes);
}

long AIL_process_digital_audio(void *dest, long size, unsigned long rate, unsigned long format, long count, const void *mixinfo)
{
    if (!dest || size < 0 || rate < 1000 || rate > 384000 || count != 1 || !mixinfo)
        return AudioError("Unsupported digital mix request", kAudio_ParamError);
    float *pcm = NULL;
    size_t frames;
    int channels, source_rate;
    if (!DecodeInfo(mixinfo, &pcm, &frames, &channels, &source_rate))
        return 0;
    int output_channels = (format & 2) ? 2 : 1;
    int bits = (format & 8) ? 32 : ((format & 1) ? 16 : 8);
    size_t bytes_per_frame = (size_t)output_channels * bits / 8;
    size_t output_frames = (size_t)ceil((double)frames * rate / source_rate);
    if (output_frames > (size_t)size / bytes_per_frame)
        output_frames = (size_t)size / bytes_per_frame;
    for (size_t i = 0; i < output_frames; ++i) {
        double position = (double)i * source_rate / rate;
        size_t a = (size_t)position;
        if (a >= frames)
            a = frames ? frames - 1 : 0;
        size_t b = a + 1 < frames ? a + 1 : a;
        float fraction = (float)(position - a);
        for (int c = 0; c < output_channels; ++c) {
            int input = channels == 1 ? 0 : c;
            float value = frames ? pcm[a * channels + input] * (1 - fraction) + pcm[b * channels + input] * fraction : 0;
            if (channels == 2 && output_channels == 1)
                value = (value + pcm[a * 2 + 1] * (1 - fraction) + pcm[b * 2 + 1] * fraction) * 0.5f;
            value = Clamp(value, -1, 1);
            unsigned char *out = (unsigned char *)dest + (i * output_channels + c) * bits / 8;
            if (bits == 8)
                *out = (unsigned char)lrintf((value + 1) * 127.5f);
            else if (bits == 16)
                Write16(out, (unsigned int)(int16_t)lrintf(value * 32767));
            else
                Write32(out, (unsigned int)(int32_t)llrint((double)value * 2147483647));
        }
    }
    free(pcm);
    return (long)(output_frames * bytes_per_frame);
}

void AIL_set_file_callbacks(MacAudioOpenCallback open_cb, MacAudioCloseCallback close_cb, MacAudioSeekCallback seek_cb, MacAudioReadCallback read_cb)
{
    file_open = open_cb; file_close = close_cb; file_seek = seek_cb; file_read = read_cb;
}

static unsigned char *ReadStreamFile(const char *name, size_t *size)
{
    unsigned char *data = NULL;
    if (file_open && file_close && file_seek && file_read) {
        unsigned long handle;
        if (!file_open(name, &handle))
            return NULL;
        long length = file_seek(handle, 0, 2);
        if (length > 0 && length <= AUDIO_MAX_BYTES && file_seek(handle, 0, 0) >= 0) {
            data = malloc((size_t)length);
            size_t used = 0;
            while (data && used < (size_t)length) {
                unsigned long got = file_read(handle, data + used, (unsigned long)((size_t)length - used));
                if (!got || got > (size_t)length - used)
                    break;
                used += got;
            }
            if (used != (size_t)length) { free(data); data = NULL; }
            *size = used;
        }
        file_close(handle);
    } else {
        FILE *file = fopen(name, "rb");
        if (!file)
            return NULL;
        if (!fseek(file, 0, SEEK_END)) {
            long length = ftell(file);
            if (length > 0 && length <= AUDIO_MAX_BYTES && !fseek(file, 0, SEEK_SET)) {
                data = malloc((size_t)length);
                if (data && fread(data, 1, (size_t)length, file) != (size_t)length) { free(data); data = NULL; }
                *size = (size_t)length;
            }
        }
        fclose(file);
    }
    return data;
}

void *AIL_open_stream(void *driver, const char *name, int memory)
{
    (void)memory;
    size_t size = 0;
    if (!name || !name[0])
        return NULL;
    unsigned char *data = ReadStreamFile(name, &size);
    if (!data) {
        AudioError("Stream read failed", kAudioFileFileNotFoundError);
        return NULL;
    }
    float *pcm = NULL;
    size_t frames;
    int channels, rate;
    int decoded = DecodeFile(data, size, &pcm, &frames, &channels, &rate);
    free(data);
    if (!decoded)
        return NULL;
    void *handle = AIL_allocate_sample_handle(driver);
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample) {
        sample->pcm[0] = pcm;
        sample->frames[0] = frames;
        sample->rate = rate;
        sample->channels = channels;
        sample->source_bytes = size;
        sample->format = channels == 2 ? 3 : 1;
        sample->prepared = 1;
    } else {
        free(pcm);
    }
    pthread_mutex_unlock(&audio_mutex);
    return handle;
}

void AIL_close_stream(void *stream) { AIL_release_sample_handle(stream); }
void AIL_pause_stream(void *stream, long pause) { if (pause) AIL_stop_sample(stream); else AIL_resume_sample(stream); }
void AIL_set_stream_volume_levels(void *stream, float l, float r) { AIL_set_sample_volume_levels(stream, l, r); }
void AIL_stream_volume_levels(void *stream, float *l, float *r) { AIL_sample_volume_levels(stream, l, r); }
void AIL_stream_volume_pan(void *stream, float *v, float *p) { AIL_sample_volume_pan(stream, v, p); }
void AIL_set_stream_reverb_levels(void *stream, float d, float w) { AIL_set_sample_reverb_levels(stream, d, w); }
void AIL_set_stream_playback_rate(void *stream, int rate) { AIL_set_sample_playback_rate(stream, rate); }
int AIL_stream_playback_rate(void *stream) { return AIL_sample_playback_rate(stream); }
void AIL_set_stream_loop_count(void *stream, int loops) { AIL_set_sample_loop_count(stream, loops); }
long AIL_stream_status(void *stream) { return (long)AIL_sample_status(stream); }
void AIL_set_stream_ms_position(void *stream, int ms) { AIL_set_sample_ms_position(stream, ms); }
void AIL_stream_ms_position(void *stream, long *total, long *current) { AIL_sample_ms_position(stream, total, current); }
SAMPLE_GETTER(int, AIL_is_3D_stream, 0, sample->spatial)

void AIL_stream_info(void *handle, int *datarate, int *format, int *length, int *memory)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (format) *format = sample ? sample->format : 0;
    if (datarate) *datarate = sample ? sample->rate * sample->channels * 2 : 0;
    if (length) *length = sample ? (int)sample->source_bytes : 0;
    if (memory) *memory = sample ? (int)(sample->frames[0] * sample->channels * sizeof(float)) : 0;
    pthread_mutex_unlock(&audio_mutex);
}

int AIL_enumerate_3D_providers(void *next, void **provider, const char **name)
{
    unsigned long *enumerator = next;
    if (!enumerator || *enumerator)
        return 0;
    *enumerator = 1;
    *provider = AUDIO_PROVIDER;
    *name = "Miles Fast 2D Positional Audio";
    return 1;
}

int AIL_open_3D_provider(void *provider) { return provider == AUDIO_PROVIDER ? 0 : 1; }
void AIL_close_3D_provider(void *provider) { (void)provider; }
void AIL_3D_provider_attribute(void *provider, const char *name, void *value)
{
    if (provider == AUDIO_PROVIDER && value && name && !strcmp(name, "Maximum supported samples"))
        *(int *)value = AUDIO_SAMPLES;
}
void *AIL_allocate_3D_sample_handle(void *provider)
{
    void *handle = provider == AUDIO_PROVIDER ? AIL_allocate_sample_handle(AUDIO_DRIVER) : NULL;
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample)
        sample->spatial = 1;
    pthread_mutex_unlock(&audio_mutex);
    return handle;
}
void AIL_stop_3D_sample(void *sample) { AIL_stop_sample(sample); }
void AIL_resume_3D_sample(void *sample) { AIL_resume_sample(sample); }
void AIL_end_3D_sample(void *sample) { AIL_end_sample(sample); }
void AIL_set_3D_sample_volume(void *sample, float volume) { AIL_set_sample_volume_levels(sample, volume, volume); }
SAMPLE_GETTER(float, AIL_3D_sample_volume, 0, sample->left)
void AIL_set_3D_sample_playback_rate(void *sample, int rate) { AIL_set_sample_playback_rate(sample, rate); }
int AIL_3D_sample_playback_rate(void *sample) { return AIL_sample_playback_rate(sample); }
void AIL_set_3D_sample_loop_count(void *sample, int loops) { AIL_set_sample_loop_count(sample, loops); }
unsigned long AIL_3D_sample_status(void *sample) { return AIL_sample_status(sample); }
SAMPLE_GETTER(int, AIL_3D_sample_length, 0, (int)sample->source_bytes)
unsigned long AIL_3D_sample_offset(void *sample) { return AIL_sample_position(sample); }
SAMPLE_SETTER(AIL_set_3D_sample_offset, (void *handle, int offset), sample->cursor = sample->source_bytes ? fmax(0, (double)offset) * sample->frames[0] / sample->source_bytes : 0)
void AIL_set_3D_room_type(void *provider, int room) { AIL_set_digital_master_room_type(provider, room); }
void AIL_set_3D_rolloff_factor(void *provider, float factor)
{
    (void)provider;
    pthread_mutex_lock(&audio_mutex); rolloff = Clamp(factor, 0, 10); pthread_mutex_unlock(&audio_mutex);
}
void AIL_set_3D_distance_factor(void *provider, float factor)
{
    (void)provider;
    pthread_mutex_lock(&audio_mutex); distance_factor = Clamp(factor, 0.00001f, 1000); pthread_mutex_unlock(&audio_mutex);
}
SAMPLE_SETTER(AIL_set_3D_sample_distances, (void *handle, float min, float max), sample->min_distance = fmaxf(min, 0); sample->max_distance = fmaxf(max, sample->min_distance + 1))
SAMPLE_SETTER(AIL_set_3D_sample_effects_level, (void *handle, float wet), sample->wet = Clamp(wet, 0, 1))

int AIL_set_3D_sample_info(void *handle, const void *metadata)
{
    float *pcm = NULL;
    size_t frames;
    int channels, rate;
    if (!DecodeInfo(metadata, &pcm, &frames, &channels, &rate))
        return 0;
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample) {
        ResetSample(sample);
        sample->pcm[0] = pcm;
        sample->frames[0] = frames;
        sample->channels = channels;
        sample->rate = rate;
        sample->source_bytes = ((const MacAudioSoundInfo *)metadata)->data_len;
        sample->prepared = 1;
    } else {
        free(pcm);
    }
    pthread_mutex_unlock(&audio_mutex);
    return sample != NULL;
}

void AIL_set_3D_position(void *handle, float x, float y, float z)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    float *position = handle == (void *)(intptr_t)-1 ? listener : (sample ? sample->position : NULL);
    if (position) { position[0] = x; position[1] = y; position[2] = z; }
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_3D_position(void *handle, float *x, float *y, float *z)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    float *position = handle == (void *)(intptr_t)-1 ? listener : (sample ? sample->position : NULL);
    if (x) *x = position ? position[0] : 0;
    if (y) *y = position ? position[1] : 0;
    if (z) *z = position ? position[2] : 0;
    pthread_mutex_unlock(&audio_mutex);
}
void AIL_set_3D_stream_position(void *stream, float x, float y, float z) { AIL_set_3D_position(stream, x, y, z); }

int MacAudio_PlayTone(double seconds)
{
    if (seconds <= 0 || seconds > 10)
        return 0;
    if (!AIL_startup(0))
        return 0;
    void *driver = AIL_open_digital_driver(AUDIO_RATE, 16, 2, 0);
    if (!driver)
        return 0;
    void *sample = AIL_allocate_sample_handle(driver);
    if (!sample)
        return 0;
    size_t count = (size_t)(AUDIO_RATE * seconds);
    int16_t *tone = malloc(count * sizeof(*tone));
    if (!tone) { AIL_release_sample_handle(sample); return 0; }
    for (size_t i = 0; i < count; ++i) {
        double envelope = fmin(1.0, fmin(i / 480.0, (count - i) / 480.0));
        tone[i] = (int16_t)(sin(i * 440.0 * 6.283185307179586 / AUDIO_RATE) * 5000 * envelope);
    }
    AIL_set_sample_type(sample, 1, 0);
    AIL_set_sample_address(sample, tone, (int)(count * sizeof(*tone)));
    AIL_set_sample_playback_rate(sample, AUDIO_RATE);
    MacAudioStats before, after;
    MacAudio_GetStats(&before);
    AIL_resume_sample(sample);
    for (int i = 0; i < (int)(seconds * 100) + 100 && AIL_sample_status(sample) == AUDIO_PLAYING; ++i)
        usleep(10000);
    MacAudio_GetStats(&after);
    int success = after.callbacks > before.callbacks && after.audible_frames > before.audible_frames && AIL_sample_status(sample) == AUDIO_DONE;
    AIL_release_sample_handle(sample);
    free(tone);
    return success;
}
#endif
