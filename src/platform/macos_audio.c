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

/* The selected Miles rate is the mix rate. CoreAudio converts that rate to
 * the device format; matching rates leave source samples unchanged. */
#define AUDIO_RATE 44100
#define AUDIO_SAMPLES 256
#define AUDIO_MAX_BYTES (256u * 1024u * 1024u)
#define AUDIO_DRIVER ((void *)(uintptr_t)1)
#define AUDIO_PROVIDER ((void *)(uintptr_t)2)
#define AUDIO_DONE 2
#define AUDIO_PLAYING 4
#define AUDIO_STOPPED 8
#define STREAM_FRAMES 32768
#define STREAM_READ_BYTES (64u * 1024u)

typedef struct MacAudioStream MacAudioStream;

typedef struct {
    int allocated, status, format, rate, channels, block_size;
    int stream_rate;
    int spatial, loops, remaining_loops, buffered, active_buffer, prepared;
    uint64_t generation;
    const void *source;
    size_t source_bytes;
    float *pcm[2];
    size_t frames[2];
    double cursor;
    float resample_previous[2];
    float left, right, dry, wet, position[3], min_distance, max_distance;
    MacAudioStream *stream;
} MacAudioSample;

struct MacAudioStream {
    MacAudioSample *sample;
    AudioFileID file;
    ExtAudioFileRef decoder;
    FILE *disk;
    unsigned long handle;
    MacAudioCloseCallback close;
    MacAudioSeekCallback seek;
    MacAudioReadCallback read;
    size_t bytes, length;
    unsigned char *encoded;
    size_t encoded_capacity, encoded_first, encoded_count;
    int channels, stop, failed, started;
    uint64_t serial, first, end;
    float *ring;
    pthread_t thread;
    pthread_cond_t wake;
};

static int StreamFrame(MacAudioSample *sample, float *left, float *right);
static void CloseStream(void *handle);

typedef struct {
    const unsigned char *data;
    size_t size;
} MacAudioFile;

static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static AudioUnit output_unit;
static MacAudioSample samples[AUDIO_SAMPLES];
/* Guarded by audio_mutex and retained across shutdown to reject stale decodes. */
static uint64_t sample_generation;
static int driver_rate = 44100, driver_channels = 2;
static int preferences[64];
static float listener[3], rolloff = 1.0f, distance_factor = 1.0f;
static float master_dry = 1.0f, master_wet = 0.0f;
static char last_error[256];
static MacAudioOpenCallback file_open;
static MacAudioCloseCallback file_close;
static MacAudioSeekCallback file_seek;
static MacAudioReadCallback file_read;
static atomic_uint_fast64_t output_callbacks, output_frames, audible_frames, lock_misses;
static _Atomic(float) output_peak;

/* Measured software-reverb network: four damped combs followed by two
 * allpass stages. Coefficients and single-precision state reproduce the
 * original impulse responses; accumulation uses double precision. */
#define REVERB_LINE_FRAMES 16782 /* longest delay at 384 kHz */
#define REVERB_PRE_FRAMES 115202 /* largest preset predelay at 384 kHz */
typedef struct {
    float line[6][REVERB_LINE_FRAMES], gain[6];
    int length[6], position[6];
    float a, b, c, x1, x2, y1, y2;
    int32_t predelay[REVERB_PRE_FRAMES];
    int pre_position, pre_length;
    uint64_t tail, tail_length;
} MacReverb;
static MacReverb room_reverb;
static const float room_parameters[26][4] = {
    {0,1.493f,.5f,.011f}, {.250f,.1f,0,.002f}, {.417f,.4f,.666f,.003f},
    {.750f,1.499f,.166f,.011f}, {.208f,.478f,0,.004f}, {.300f,2.309f,.680f,.017f},
    {.403f,4.279f,.5f,.030f}, {.500f,3.961f,.5f,.029f}, {.500f,2.886f,1.304f,.022f},
    {.361f,7.284f,.332f,.030f}, {.500f,10,.300f,.030f}, {.153f,.259f,.550f,.030f},
    {.361f,1.493f,.5f,.011f}, {.444f,2.697f,.638f,.020f}, {.250f,1.752f,.776f,.011f},
    {.111f,3.145f,.472f,.088f}, {.111f,2.767f,.224f,.011f}, {.200f,.8f,.2f,.300f},
    {.550f,3,.550f,.043f}, {.120f,.8f,.180f,.100f}, {.208f,1.652f,1.5f,.012f},
    {.652f,2.886f,.250f,.021f}, {1,1.499f,.1f,.011f}, {.875f,8.392f,1.388f,.011f},
    {.139f,10,1.5f,.011f}, {.486f,7.563f,4,.011f}
};

static void ConfigureReverb(int room)
{
    static const float delay[] = {.0297f,.0371f,.0411f,.0437f,.005f,.0017f};
    MacReverb *r = &room_reverb;
    const float *p = room_parameters[room];
    float reciprocal_time = 1.0f / p[1];
    for (int i = 0; i < 6; ++i) {
        r->length[i] = (int)lrint((double)delay[i] * driver_rate);
        if (r->position[i] >= r->length[i]) r->position[i] = 0;
    }
    for (int i = 0; i < 4; ++i)
        r->gain[i] = pow((double).001f, (double)delay[i] * reciprocal_time);
    r->gain[4] = pow((double).001f, (double).005f / .09685350209474564f);
    r->gain[5] = pow((double).001f, (double).0017f / .032924000173807144f);
    double cotangent = 1.0 / tan((double)p[2] * .39269909262657166f);
    double a = 1.0 / (1.0 + sqrt(2.0) * cotangent + cotangent * cotangent);
    r->a = a;
    r->b = 2.0 * (1.0 - cotangent * cotangent) * a;
    r->c = (1.0 - sqrt(2.0) * cotangent + cotangent * cotangent) * a;
    r->pre_length = (int)lrint((double)p[3] * driver_rate);
    r->tail_length = (uint64_t)ceil(((double)p[1] + p[3] + .1) * driver_rate);
}

/* The original conversion rounds to a 64-bit integer and retains its low
 * 32 bits. Its indefinite value has zero low bits, including NaN/overflow
 * from the zero-damping and maximum-damping presets. Avoid undefined casts. */
static int32_t ReverbInteger(double value)
{
    if (!isfinite(value) || value >= 0x1p63 || value < -0x1p63) return 0;
    uint32_t low = (uint32_t)(uint64_t)llrint(value);
    int32_t result;
    memcpy(&result, &low, sizeof(result));
    return result;
}

static void ReverbFrame(int32_t input, int32_t output[2])
{
    MacReverb *r = &room_reverb;
    int write = (r->pre_position + r->pre_length) % REVERB_PRE_FRAMES;
    r->predelay[write] = (int32_t)((uint32_t)r->predelay[write] + (uint32_t)input);
    int32_t delayed = r->predelay[r->pre_position];
    r->predelay[r->pre_position] = 0;
    r->pre_position = (r->pre_position + 1) % REVERB_PRE_FRAMES;
    double x = (double)delayed * r->a;
    double y = x + 2.0 * r->x1 + r->x2 - (double)r->b * r->y1 - (double)r->c * r->y2;
    r->x2 = r->x1; r->x1 = x;
    r->y2 = r->y1; r->y1 = y + (double)1.0e-10f;
    float sum = (double)r->line[1][r->position[1]] + r->line[0][r->position[0]]
        + r->line[3][r->position[3]] + r->line[2][r->position[2]];
    for (int i = 0; i < 4; ++i)
        r->line[i][r->position[i]] = (double)r->gain[i] * r->line[i][r->position[i]] + y;
    double old_first = r->line[4][r->position[4]];
    double new_first = sum + (double)r->gain[4] * old_first;
    r->line[4][r->position[4]] = new_first;
    float first = old_first - (double)r->gain[4] * new_first;
    double old_second = r->line[5][r->position[5]];
    float feedback = (double)r->gain[5] * old_second;
    double new_second = (double)first * .25f + feedback;
    r->line[5][r->position[5]] = new_second;
    output[0] = ReverbInteger(old_second - (double)r->gain[5] * new_second);
    output[1] = ReverbInteger(old_second - (double)r->gain[5] * ((double)first * .15f + feedback));
    for (int i = 0; i < 6; ++i)
        if (++r->position[i] == r->length[i]) r->position[i] = 0;
}

static float ReverbVolume(float left, float right)
{
    if (left <= 0) return right;
    if (right <= 0) return left;
    double ratio = pow((double)right / left, 10.0 / 3.0);
    double pan = ratio / (1.0 + ratio);
    return right / pow(pan, .3);
}

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
    sample->generation = ++sample_generation;
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

/* Miles integrates the source interval when reducing the rate. The first
 * interval starts with a zero history sample; subsequent intervals retain the
 * preceding sample across buffer/loop boundaries. Fractional PCM contributions
 * are truncated before accumulation, as measured at 3:2 and 5:2 rates. */
static int DownsampleFrame(MacAudioSample *sample, double step, float *left, float *right)
{
    double start = sample->cursor, end = start + step;
    double sum[2] = {0, 0};
    int index = sample->active_buffer;
    size_t length = sample->frames[index];
    double limit = sample->stream ? (double)sample->stream->length * sample->loops : length;
    int finite = sample->stream ? sample->loops != 0 : !sample->buffered && sample->remaining_loops == 1;
    if (finite && end - 1 >= limit) {
        sample->status = AUDIO_DONE;
        return 0;
    }
    for (double position = floor(start); position < end; position += 1) {
        double weight = fmin(end, position + 1) - fmax(start, position);
        float value[2] = {0, 0};
        int64_t frame = (int64_t)position - 1;
        if (sample->stream) {
            MacAudioStream *stream = sample->stream;
            uint64_t limit = sample->loops ? (uint64_t)stream->length * sample->loops : UINT64_MAX;
            if (frame >= 0 && (uint64_t)frame >= stream->first && (uint64_t)frame < limit) {
                if ((uint64_t)frame >= stream->end) return 0;
                size_t at = ((uint64_t)frame % STREAM_FRAMES) * stream->channels;
                value[0] = stream->ring[at];
                value[1] = stream->channels == 2 ? stream->ring[at + 1] : value[0];
            }
        } else if (frame < 0) {
            value[0] = sample->resample_previous[0];
            value[1] = sample->resample_previous[1];
        } else {
            const float *pcm = sample->pcm[index];
            size_t at = (size_t)frame;
            if (at >= length) {
                if (sample->buffered) {
                    at -= length;
                    pcm = sample->pcm[1 - index];
                    if (at >= sample->frames[1 - index]) pcm = NULL;
                } else if (!sample->loops || sample->remaining_loops > 1) {
                    at %= length;
                } else {
                    pcm = NULL;
                }
            }
            if (pcm) {
                value[0] = pcm[at * sample->channels];
                value[1] = sample->channels == 2 ? pcm[at * 2 + 1] : value[0];
            }
        }
        sum[0] += floor((double)value[0] * 32768 * weight);
        sum[1] += floor((double)value[1] * 32768 * weight);
    }
    *left = sum[0] / 32768;
    *right = sum[1] / 32768;
    return 1;
}

static void MixLocked(float *stereo, size_t frame_count, int output_channels)
{
    memset(stereo, 0, frame_count * output_channels * sizeof(float));
    int32_t wet_buffer[1024];
    unsigned char wet_active[1024];
    for (size_t chunk = 0; chunk < frame_count; chunk += 1024) {
        size_t count = frame_count - chunk;
        if (count > 1024)
            count = 1024;
        memset(wet_buffer, 0, count * sizeof(*wet_buffer));
        memset(wet_active, 0, count);
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
                /* Windows 1.3 plays 3D sounds through Miles' "Fast 2D Positional
                 * Audio" provider. Measured there: the pan is linear in the angle
                 * off the median plane (not a constant-power curve), elevation only
                 * counts through that angle, and a source behind the ear line plays
                 * at three quarters, as a step. position[] is right, up, forward. */
                float lateral = distance > 0 ? Clamp(x / distance, -1, 1) : 0;
                float pan = asinf(lateral) * (float)(2.0 / M_PI);
                float rear = z < 0 ? 0.75f : 1.0f;
                float attenuation = 1.0f;
                if (rolloff > 0 && distance * distance_factor > sample->min_distance) {
                    float range = fmaxf(sample->max_distance - sample->min_distance, 1);
                    attenuation = powf(Clamp(1 - (distance * distance_factor - sample->min_distance) / range, 0, 1), rolloff);
                }
                left *= (1 - pan) * 0.5f * rear * attenuation;
                right *= (1 + pan) * 0.5f * rear * attenuation;
            }
            /* Miles rounds the composed dry channel gain to 1/2048 before
             * applying it to source PCM. Keep this after spatial attenuation. */
            float dry_left = (float)lrintf(left * sample->dry * master_dry * 2048.0f) / 2048.0f;
            float dry_right = (float)lrintf(right * sample->dry * master_dry * 2048.0f) / 2048.0f;
            double step = (double)sample->rate / driver_rate;
            int wet_gain = (int)lrintf(Clamp(ReverbVolume(left, right) * sample->wet * master_wet, 0, 1) * 2048.0f);
            if (step > 1) {
                dry_left = (int)(dry_left * 2048 / step) / 2048.0f;
                dry_right = (int)(dry_right * 2048 / step) / 2048.0f;
                wet_gain = (int)(wet_gain / step);
            }
            for (size_t i = 0; i < count; ++i) {
                float l, r;
                if (sample->stream) {
                    if (!StreamFrame(sample, &l, &r)) break;
                } else {
                int index = sample->active_buffer;
                size_t length = sample->frames[index];
                if (sample->cursor >= length) {
                    if (length) {
                        sample->resample_previous[0] = sample->pcm[index][(length - 1) * sample->channels];
                        sample->resample_previous[1] = sample->pcm[index][length * sample->channels - 1];
                    }
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
                l = sample->pcm[index][current * sample->channels];
                l += (sample->pcm[index][next * sample->channels] - l) * fraction;
                r = l;
                if (sample->channels == 2) {
                    r = sample->pcm[index][current * 2 + 1];
                    r += (sample->pcm[index][next * 2 + 1] - r) * fraction;
                }
                }
                if (step > 1 && !DownsampleFrame(sample, step, &l, &r)) break;
                size_t output = (chunk + i) * output_channels;
                /* The original mono mixer takes a mono source's left gain;
                 * stereo sources add both independently weighted channels. */
                float dry_l = l * dry_left;
                float dry_r = r * dry_right;
                if (driver_channels == 1) {
                    if (sample->channels == 2) dry_l += dry_r;
                    dry_r = dry_l;
                }
                stereo[output] += dry_l;
                if (output_channels == 2) stereo[output + 1] += dry_r;
                int32_t send = ReverbInteger((double)(sample->channels == 2 ? l + r : l) * 32768.0 * wet_gain);
                wet_buffer[i] = (int32_t)((uint32_t)wet_buffer[i] + (uint32_t)send);
                if (wet_gain) wet_active[i] = 1;
                sample->cursor += step;
            }
            if (sample->stream) pthread_cond_signal(&sample->stream->wake);
        }
        for (size_t i = 0; i < count; ++i) {
            int32_t wet[2] = {0, 0};
            if (wet_active[i]) room_reverb.tail = room_reverb.tail_length;
            if (room_reverb.length[0] && room_reverb.tail) {
                ReverbFrame(wet_buffer[i], wet);
                --room_reverb.tail;
            }
            size_t output = (chunk + i) * output_channels;
            stereo[output] = Clamp(stereo[output] + (double)wet[0] / 67108864.0, -1, 1);
            if (output_channels == 2)
                stereo[output + 1] = Clamp(stereo[output + 1] + (double)wet[driver_channels == 1 ? 0 : 1] / 67108864.0, -1, 1);
        }
    }
}

void MacAudio_Mix(float *stereo, size_t frames)
{
    pthread_mutex_lock(&audio_mutex);
    MixLocked(stereo, frames, 2);
    pthread_mutex_unlock(&audio_mutex);
}

static OSStatus Render(void *refcon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *timestamp, UInt32 bus, UInt32 frames, AudioBufferList *buffers)
{
    (void)refcon; (void)timestamp; (void)bus;
    atomic_fetch_add_explicit(&output_callbacks, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&output_frames, frames, memory_order_relaxed);
    for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i)
        memset(buffers->mBuffers[i].mData, 0, buffers->mBuffers[i].mDataByteSize);
    if (buffers->mNumberBuffers != 1 || buffers->mBuffers[0].mNumberChannels != (UInt32)driver_channels || buffers->mBuffers[0].mDataByteSize < frames * driver_channels * sizeof(float))
        return kAudio_ParamError;
    if (pthread_mutex_trylock(&audio_mutex)) {
        atomic_fetch_add_explicit(&lock_misses, 1, memory_order_relaxed);
        *flags |= kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }
    float *pcm = buffers->mBuffers[0].mData;
    MixLocked(pcm, frames, driver_channels);
    pthread_mutex_unlock(&audio_mutex);
    float peak = 0;
    uint64_t audible = 0;
    for (UInt32 i = 0; i < frames; ++i) {
        float value = fabsf(pcm[i * driver_channels]);
        if (driver_channels == 2) value = fmaxf(value, fabsf(pcm[i * 2 + 1]));
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
    if (output_unit)
        return AUDIO_DRIVER;
    driver_rate = frequency;
    driver_channels = channels;
    memset(&room_reverb, 0, sizeof(room_reverb));
    ConfigureReverb(0);
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
        .mSampleRate = driver_rate, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked,
        .mBytesPerPacket = channels * sizeof(float), .mFramesPerPacket = 1,
        .mBytesPerFrame = channels * sizeof(float),
        .mChannelsPerFrame = channels, .mBitsPerChannel = 32
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
    for (int i = 0; i < AUDIO_SAMPLES; ++i) CloseStream(&samples[i]);
    pthread_mutex_lock(&audio_mutex);
    for (int i = 0; i < AUDIO_SAMPLES; ++i) {
        free(samples[i].pcm[0]);
        free(samples[i].pcm[1]);
    }
    memset(samples, 0, sizeof(samples));
    memset(&room_reverb, 0, sizeof(room_reverb));
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
            /* Fresh 2D allocation must not inherit a released 3D slot's type.
               In-place resets still preserve the type for 3D sample setup. */
            sample->spatial = 0;
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
    CloseStream(handle);
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
    CloseStream(handle);
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
SAMPLE_SETTER(AIL_end_sample, (void *handle), sample->status = AUDIO_DONE; sample->cursor = 0; memset(sample->resample_previous, 0, sizeof(sample->resample_previous)))
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
        sample->generation = ++sample_generation;
        free(sample->pcm[0]); free(sample->pcm[1]);
        sample->pcm[0] = sample->pcm[1] = NULL;
        sample->frames[0] = sample->frames[1] = 0;
        sample->source = data;
        sample->source_bytes = size > 0 ? (size_t)size : 0;
        sample->prepared = sample->buffered = 0;
        memset(sample->resample_previous, 0, sizeof(sample->resample_previous));
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
            sample->generation = ++sample_generation;
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
    if ((unsigned int)room >= 26) room = 0;
    master_dry = 1.0f;
    master_wet = room_parameters[room][0];
    ConfigureReverb(room);
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

/* Miles opens a seekable stream and services bounded buffers on a worker.
 * CoreAudio supplies the decoder here; neither file reads nor decoding run in
 * the render callback. CoreAudio may scan the encoded stream for its duration
 * at open; retain overlapping reads so compressed IWD callbacks stay forward. */
static OSStatus StreamRead(void *client, SInt64 position, UInt32 count, void *buffer, UInt32 *actual)
{
    MacAudioStream *stream = client;
    *actual = 0;
    if (position < 0 || (uint64_t)position > stream->bytes) return kAudioFilePositionError;
    if (count > stream->bytes - (size_t)position) count = (UInt32)(stream->bytes - (size_t)position);
    if (stream->disk) {
        if (fseeko(stream->disk, position, SEEK_SET)) return kAudioFilePositionError;
        *actual = (UInt32)fread(buffer, 1, count, stream->disk);
        return ferror(stream->disk) ? kAudioFileUnspecifiedError : noErr;
    }
    while (*actual < count) {
        size_t offset = (size_t)position + *actual;
        size_t end = stream->encoded_first + stream->encoded_count;
        if (offset < stream->encoded_first || offset > end) {
            stream->encoded_first = offset;
            stream->encoded_count = 0;
            end = offset;
        }
        if (offset < end) {
            size_t available = end - offset;
            if (available > count - *actual) available = count - *actual;
            memcpy((char *)buffer + *actual,
                stream->encoded + offset - stream->encoded_first, available);
            *actual += (UInt32)available;
            continue;
        }
        /* Keep the previous half when advancing the window. MP3 metadata
         * scans reread packet headers behind their preceding read's end;
         * discarding the whole window would still rewind an IWD at each edge. */
        if (stream->encoded_count == stream->encoded_capacity) {
            size_t retain = stream->encoded_count / 2;
            size_t discard = stream->encoded_count - retain;
            memmove(stream->encoded, stream->encoded + discard, retain);
            stream->encoded_first += discard;
            stream->encoded_count = retain;
        }
        if (end > LONG_MAX || stream->seek(stream->handle, (long)end, 0) < 0)
            return kAudioFilePositionError;
        size_t fill = stream->encoded_capacity - stream->encoded_count;
        if (fill > stream->bytes - end) fill = stream->bytes - end;
        size_t added = 0;
        while (added < fill) {
            unsigned long got = stream->read(stream->handle,
                stream->encoded + stream->encoded_count, fill - added);
            if (!got) break;
            if (got > fill - added) return kAudioFileUnspecifiedError;
            stream->encoded_count += got;
            added += got;
        }
        if (!added) break;
    }
    return noErr;
}

static SInt64 StreamSize(void *client) { return (SInt64)((MacAudioStream *)client)->bytes; }

static void DisposeStream(MacAudioStream *stream)
{
    if (stream->decoder) ExtAudioFileDispose(stream->decoder);
    if (stream->file) AudioFileClose(stream->file);
    if (stream->disk) fclose(stream->disk);
    else if (stream->close) stream->close(stream->handle);
    pthread_cond_destroy(&stream->wake);
    free(stream->ring);
    free(stream->encoded);
    free(stream);
}

static void CloseStream(void *handle)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    MacAudioStream *stream = sample ? sample->stream : NULL;
    if (stream) {
        sample->status = AUDIO_STOPPED;
        stream->stop = 1;
        pthread_cond_signal(&stream->wake);
    }
    pthread_mutex_unlock(&audio_mutex);
    if (!stream) return;
    if (stream->started) pthread_join(stream->thread, NULL);
    pthread_mutex_lock(&audio_mutex);
    sample->stream = NULL;
    pthread_mutex_unlock(&audio_mutex);
    DisposeStream(stream);
}

static void *StreamWorker(void *context)
{
    MacAudioStream *stream = context;
    uint64_t serial = 0;
    float pcm[4096 * 2];
    pthread_mutex_lock(&audio_mutex);
    while (!stream->stop) {
        uint64_t cursor = (uint64_t)stream->sample->cursor;
        int reposition = serial != stream->serial;
        if (reposition) {
            serial = stream->serial;
            stream->first = stream->end = cursor;
        }
        size_t available = stream->end > cursor ? (size_t)(stream->end - cursor) : 0;
        if (available >= STREAM_FRAMES - 1) {
            pthread_cond_wait(&stream->wake, &audio_mutex);
            continue;
        }
        uint64_t start = stream->end;
        uint64_t limit = stream->sample->loops ? (uint64_t)stream->length * stream->sample->loops : UINT64_MAX;
        if (start >= limit) {
            pthread_cond_wait(&stream->wake, &audio_mutex);
            continue;
        }
        size_t position = start % stream->length;
        UInt32 frames = (UInt32)(STREAM_FRAMES - 1 - available);
        if (frames > 4096) frames = 4096;
        if (frames > stream->length - position) frames = (UInt32)(stream->length - position);
        if (frames > limit - start) frames = (UInt32)(limit - start);
        pthread_mutex_unlock(&audio_mutex);
        OSStatus error = noErr;
        if (reposition || position == 0) error = ExtAudioFileSeek(stream->decoder, (SInt64)position);
        AudioBufferList buffers = { .mNumberBuffers = 1,
            .mBuffers = {{ (UInt32)stream->channels, frames * stream->channels * sizeof(float), pcm }} };
        if (!error) error = ExtAudioFileRead(stream->decoder, &frames, &buffers);
        pthread_mutex_lock(&audio_mutex);
        if (stream->stop) break;
        if (serial != stream->serial) continue; /* A seek invalidated this decode. */
        if (error || !frames) {
            stream->failed = 1;
            break;
        }
        for (UInt32 i = 0; i < frames; ++i)
            memcpy(stream->ring + ((start + i) % STREAM_FRAMES) * stream->channels,
                pcm + i * stream->channels, stream->channels * sizeof(float));
        stream->end += frames;
        if (stream->end - stream->first > STREAM_FRAMES)
            stream->first = stream->end - STREAM_FRAMES;
    }
    pthread_mutex_unlock(&audio_mutex);
    return NULL;
}

/* Called under audio_mutex; an empty buffer pauses consumption until the worker
 * catches up. A finite stream stops at its media length; loop prefetch wraps
 * the decoder without inserting silence at the loop boundary. */
static int StreamFrame(MacAudioSample *sample, float *left, float *right)
{
    MacAudioStream *stream = sample->stream;
    if (stream->failed || (sample->loops && sample->cursor >= (double)stream->length * sample->loops)) {
        sample->status = AUDIO_DONE;
        return 0;
    }
    uint64_t current = (uint64_t)sample->cursor, next = current + 1;
    if (current % stream->length == stream->length - 1) next = current;
    if (sample->loops && next >= (uint64_t)stream->length * sample->loops) next = current;
    if (current < stream->first || next >= stream->end) return 0;
    float fraction = (float)(sample->cursor - current);
    size_t a = (current % STREAM_FRAMES) * stream->channels;
    size_t b = (next % STREAM_FRAMES) * stream->channels;
    *left = stream->ring[a] + (stream->ring[b] - stream->ring[a]) * fraction;
    *right = stream->channels == 2 ? stream->ring[a + 1] + (stream->ring[b + 1] - stream->ring[a + 1]) * fraction : *left;
    return 1;
}

extern FILE *FS_FileOpen(const char *filename, const char *mode);

void *AIL_open_stream(void *driver, const char *name, int memory)
{
    (void)memory;
    if (!name || !*name) return NULL;
    MacAudioStream *stream = calloc(1, sizeof(*stream));
    if (!stream) return NULL;
    pthread_cond_init(&stream->wake, NULL);
    if (file_open && file_close && file_seek && file_read) {
        if (!file_open(name, &stream->handle)) { DisposeStream(stream); return NULL; }
        stream->close = file_close; stream->seek = file_seek; stream->read = file_read;
        long length = stream->seek(stream->handle, 0, 2);
        if (length > 0) stream->bytes = (size_t)length;
    } else {
        stream->disk = FS_FileOpen(name, "rb");
        if (stream->disk && !fseeko(stream->disk, 0, SEEK_END)) {
            off_t length = ftello(stream->disk);
            if (length > 0) stream->bytes = (size_t)length;
        }
    }
    if (stream->bytes && !stream->disk) {
        stream->encoded_capacity = stream->bytes < STREAM_READ_BYTES ? stream->bytes : STREAM_READ_BYTES;
        stream->encoded = malloc(stream->encoded_capacity);
        if (!stream->encoded) { DisposeStream(stream); return NULL; }
    }
    OSStatus error = stream->bytes ? AudioFileOpenWithCallbacks(stream, StreamRead, NULL, StreamSize, NULL, 0, &stream->file) : kAudioFileFileNotFoundError;
    if (!error) error = ExtAudioFileWrapAudioFileID(stream->file, false, &stream->decoder);
    AudioStreamBasicDescription input = {0};
    UInt32 size = sizeof(input);
    if (!error) error = ExtAudioFileGetProperty(stream->decoder, kExtAudioFileProperty_FileDataFormat, &size, &input);
    if (!error && (input.mChannelsPerFrame < 1 || input.mChannelsPerFrame > 2 || input.mSampleRate < 1000 || input.mSampleRate > 384000)) error = kAudio_ParamError;
    AudioStreamBasicDescription output = { .mSampleRate = input.mSampleRate, .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked, .mBytesPerPacket = input.mChannelsPerFrame * sizeof(float),
        .mFramesPerPacket = 1, .mBytesPerFrame = input.mChannelsPerFrame * sizeof(float),
        .mChannelsPerFrame = input.mChannelsPerFrame, .mBitsPerChannel = 32 };
    if (!error) error = ExtAudioFileSetProperty(stream->decoder, kExtAudioFileProperty_ClientDataFormat, sizeof(output), &output);
    SInt64 length = 0; size = sizeof(length);
    if (!error) error = ExtAudioFileGetProperty(stream->decoder, kExtAudioFileProperty_FileLengthFrames, &size, &length);
    if (!error && length <= 0) error = kAudio_ParamError;
    if (!error) {
        stream->length = (size_t)length;
        stream->channels = (int)input.mChannelsPerFrame;
        stream->ring = malloc(STREAM_FRAMES * stream->channels * sizeof(float));
        if (!stream->ring) error = -ENOMEM;
    }
    if (error) { AudioError("Stream open", error); DisposeStream(stream); return NULL; }
    void *handle = AIL_allocate_sample_handle(driver);
    if (!handle) { DisposeStream(stream); return NULL; }
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    sample->stream = stream;
    stream->sample = sample;
    stream->serial = 1;
    sample->frames[0] = stream->length;
    sample->rate = sample->stream_rate = (int)input.mSampleRate;
    sample->channels = stream->channels;
    sample->source_bytes = stream->bytes;
    sample->format = stream->channels == 2 ? 3 : 1;
    sample->prepared = 1;
    pthread_mutex_unlock(&audio_mutex);
    int result = pthread_create(&stream->thread, NULL, StreamWorker, stream);
    if (result) { AudioError("Stream worker", result); AIL_release_sample_handle(handle); return NULL; }
    stream->started = 1;
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
void AIL_set_stream_loop_count(void *handle, int loops)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (sample) {
        sample->loops = sample->remaining_loops = loops > 0 ? loops : 0;
        if (sample->stream) pthread_cond_signal(&sample->stream->wake);
    }
    pthread_mutex_unlock(&audio_mutex);
}
long AIL_stream_status(void *stream) { return (long)AIL_sample_status(stream); }
/* Miles stream time uses file metadata independently of playback pitch.
   Keep the decoded media rate for streams; ordinary samples retain their rate. */
void AIL_set_stream_ms_position(void *stream, int ms)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(stream);
    if (sample) {
        int rate = sample->stream_rate ? sample->stream_rate : sample->rate;
        sample->cursor = fmax(0, (double)ms * rate / 1000.0);
        if (sample->stream) {
            sample->stream->first = sample->stream->end = (uint64_t)sample->cursor;
            ++sample->stream->serial;
            pthread_cond_signal(&sample->stream->wake);
        }
    }
    pthread_mutex_unlock(&audio_mutex);
}

void AIL_stream_ms_position(void *stream, long *total, long *current)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(stream);
    if (sample)
        PrepareSample(sample);
    int rate = sample ? (sample->stream_rate ? sample->stream_rate : sample->rate) : 0;
    if (total) *total = rate ? (long)(sample->frames[sample->active_buffer] * 1000.0 / rate) : 0;
    double position = sample ? sample->cursor : 0;
    if (sample && sample->stream && sample->status != AUDIO_DONE)
        position = fmod(position, (double)sample->stream->length);
    if (current) *current = rate ? (long)(position * 1000.0 / rate) : 0;
    pthread_mutex_unlock(&audio_mutex);
}
SAMPLE_GETTER(int, AIL_is_3D_stream, 0, sample->spatial)

void AIL_stream_info(void *handle, int *datarate, int *format, int *length, int *memory)
{
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (format) *format = sample ? sample->format : 0;
    if (datarate) *datarate = sample ? sample->rate * sample->channels * 2 : 0;
    if (length) *length = sample ? (int)sample->source_bytes : 0;
    if (memory) *memory = sample ? (int)((sample->stream ? STREAM_FRAMES : sample->frames[0]) * sample->channels * sizeof(float)) : 0;
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
    pthread_mutex_lock(&audio_mutex);
    MacAudioSample *sample = FindSample(handle);
    if (!sample) {
        pthread_mutex_unlock(&audio_mutex);
        return 0;
    }
    /* Alias start may resume even after failure, so discard the old sound first. */
    ResetSample(sample);
    uint64_t generation = sample->generation;
    pthread_mutex_unlock(&audio_mutex);

    float *pcm = NULL;
    size_t frames;
    int channels, rate;
    if (!DecodeInfo(metadata, &pcm, &frames, &channels, &rate))
        return 0;

    pthread_mutex_lock(&audio_mutex);
    sample = FindSample(handle);
    int result = sample && sample->generation == generation;
    if (result) {
        sample->pcm[0] = pcm;
        sample->frames[0] = frames;
        sample->channels = channels;
        sample->rate = rate;
        sample->source_bytes = ((const MacAudioSoundInfo *)metadata)->data_len;
        sample->prepared = 1;
    }
    pthread_mutex_unlock(&audio_mutex);
    if (!result)
        free(pcm);
    return result;
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
