#include "macos_audio.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) && defined(COD2_X64)
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "audio check failed at line %d: %s (%s)\n", __LINE__, #condition, AIL_last_error()); return 1; } } while (0)

static unsigned char wave[44 + 16];
static size_t file_cursor;
static int opens, closes, seeks, reads;

static void Put16(unsigned char *out, unsigned int value)
{
    out[0] = (unsigned char)value;
    out[1] = (unsigned char)(value >> 8);
}

static void Put32(unsigned char *out, unsigned int value)
{
    Put16(out, value);
    Put16(out + 2, value >> 16);
}

static void MakeWave(void)
{
    memcpy(wave, "RIFF", 4);
    Put32(wave + 4, sizeof(wave) - 8);
    memcpy(wave + 8, "WAVEfmt ", 8);
    Put32(wave + 16, 16);
    Put16(wave + 20, 1);
    Put16(wave + 22, 2);
    Put32(wave + 24, 48000);
    Put32(wave + 28, 192000);
    Put16(wave + 32, 4);
    Put16(wave + 34, 16);
    memcpy(wave + 36, "data", 4);
    Put32(wave + 40, 16);
    for (int i = 0; i < 4; ++i) {
        Put16(wave + 44 + i * 4, 8192);
        Put16(wave + 46 + i * 4, (unsigned int)(uint16_t)-16384);
    }
}

static unsigned long Open(const char *name, unsigned long *handle)
{
    if (strcmp(name, "memory.wav"))
        return 0;
    ++opens;
    file_cursor = 0;
    *handle = 7;
    return 1;
}
static void Close(unsigned long handle) { if (handle == 7) ++closes; }
static long Seek(unsigned long handle, long offset, unsigned long origin)
{
    if (handle != 7 || origin > 2)
        return -1;
    ++seeks;
    long position = offset + (origin == 0 ? 0 : (origin == 1 ? (long)file_cursor : (long)sizeof(wave)));
    if (position < 0 || (size_t)position > sizeof(wave))
        return -1;
    file_cursor = (size_t)position;
    return position;
}
static unsigned long Read(unsigned long handle, void *buffer, unsigned long bytes)
{
    if (handle != 7)
        return 0;
    ++reads;
    if (bytes > 7)
        bytes = 7;
    if (bytes > sizeof(wave) - file_cursor)
        bytes = sizeof(wave) - file_cursor;
    memcpy(buffer, wave + file_cursor, bytes);
    file_cursor += bytes;
    return bytes;
}

int main(void)
{
    MakeWave();
    CHECK(AIL_startup(0));
    struct { MacAudioSoundInfo info; uint64_t guard; } metadata = { .guard = UINT64_C(0x123456789abcdef0) };
    CHECK(AIL_WAV_info(wave, &metadata.info));
    CHECK(metadata.guard == UINT64_C(0x123456789abcdef0));
    CHECK(metadata.info.format == 1 && metadata.info.rate == 48000 && metadata.info.channels == 2 && metadata.info.bits == 16);
    CHECK(metadata.info.samples == 4 && metadata.info.data_len == 16 && metadata.info.data_ptr == wave + 44);
    CHECK(AIL_size_processed_digital_audio(24000, 1, 1, &metadata.info) == 4);
    int16_t converted[4] = { 0, 0, 12345, 23456 };
    CHECK(AIL_process_digital_audio(converted, 4, 24000, 1, 1, &metadata.info) == 4);
    CHECK(abs(converted[0] + 4096) <= 1 && abs(converted[1] + 4096) <= 1);
    CHECK(converted[2] == 12345 && converted[3] == 23456);

    void *sample = AIL_allocate_sample_handle((void *)(uintptr_t)1);
    CHECK(sample);
    AIL_set_sample_type(sample, 3, 0);
    AIL_set_sample_playback_rate(sample, 48000);
    AIL_set_sample_address(sample, metadata.info.data_ptr, 16);
    AIL_set_sample_volume_levels(sample, 0.25f, 0.75f);
    AIL_set_sample_loop_count(sample, 0);
    AIL_resume_sample(sample);
    float mixed[32];
    MacAudio_Mix(mixed, 16);
    for (int i = 0; i < 16; ++i) {
        CHECK(fabsf(mixed[i * 2] - 0.0625f) < 0.00001f);
        CHECK(fabsf(mixed[i * 2 + 1] + 0.375f) < 0.00001f);
    }
    CHECK(AIL_sample_status(sample) == 4);
    AIL_stop_sample(sample);
    MacAudio_Mix(mixed, 16);
    for (int i = 0; i < 32; ++i)
        CHECK(mixed[i] == 0);
    CHECK(AIL_sample_status(sample) == 8);
    AIL_end_sample(sample);
    AIL_set_sample_loop_count(sample, 1);
    AIL_resume_sample(sample);
    MacAudio_Mix(mixed, 16);
    CHECK(AIL_sample_status(sample) == 2);
    AIL_release_sample_handle(sample);

    sample = AIL_allocate_sample_handle((void *)(uintptr_t)1);
    CHECK(sample);
    AIL_set_sample_type(sample, 1, 0);
    AIL_set_sample_playback_rate(sample, 48000);
    int16_t first_buffer[4] = { 8192, 8192, 8192, 8192 };
    int16_t second_buffer[4] = { -4096, -4096, -4096, -4096 };
    CHECK(AIL_sample_buffer_ready(sample) == 0);
    AIL_load_sample_buffer(sample, 0, first_buffer, sizeof(first_buffer));
    CHECK(AIL_sample_buffer_ready(sample) == 1);
    AIL_load_sample_buffer(sample, 1, second_buffer, sizeof(second_buffer));
    CHECK(AIL_sample_buffer_ready(sample) == -1);
    MacAudio_Mix(mixed, 16);
    for (int i = 0; i < 16; ++i) {
        float expected = i < 4 ? 0.25f : (i < 8 ? -0.125f : 0);
        CHECK(fabsf(mixed[i * 2] - expected) < 0.00001f);
        CHECK(fabsf(mixed[i * 2 + 1] - expected) < 0.00001f);
    }
    CHECK(AIL_sample_status(sample) == 2 && AIL_sample_buffer_ready(sample) == 0);
    AIL_release_sample_handle(sample);

    AIL_set_file_callbacks(Open, Close, Seek, Read);
    void *stream = AIL_open_stream((void *)(uintptr_t)1, "memory.wav", 0);
    CHECK(stream && opens == 1 && closes == 1 && seeks == 2 && reads > 1);
    int stream_info[6] = { 111, 0, 0, 0, 0, 222 };
    AIL_stream_info(stream, &stream_info[1], &stream_info[2], &stream_info[3], &stream_info[4]);
    CHECK(stream_info[0] == 111 && stream_info[5] == 222);
    CHECK(stream_info[1] == 192000 && stream_info[2] == 3 && stream_info[3] == sizeof(wave) && stream_info[4] == 32);
    AIL_pause_stream(stream, 0);
    MacAudio_Mix(mixed, 16);
    CHECK(fabsf(mixed[0] - 0.25f) < 0.00001f && fabsf(mixed[1] + 0.5f) < 0.00001f);
    CHECK(AIL_stream_status(stream) == 2);
    AIL_close_stream(stream);
    AIL_set_file_callbacks(NULL, NULL, NULL, NULL);

    unsigned char adpcm[256] = { 0 };
    Put16(adpcm, 8192);
    MacAudioSoundInfo compressed = {
        .format = 0x11, .data_ptr = adpcm, .data_len = sizeof(adpcm), .rate = 22050,
        .bits = 4, .channels = 1, .samples = 505, .block_size = sizeof(adpcm), .initial_ptr = adpcm
    };
    int16_t decoded[505];
    CHECK(AIL_size_processed_digital_audio(22050, 1, 1, &compressed) == sizeof(decoded));
    CHECK(AIL_process_digital_audio(decoded, sizeof(decoded), 22050, 1, 1, &compressed) == sizeof(decoded));
    for (int i = 0; i < 505; ++i)
        CHECK(abs(decoded[i] - 8192) <= 1);
    puts("audio PCM, resampling, IMA ADPCM, loops, pause, buffer handoff and file callbacks: PASS");

    CHECK(MacAudio_PlayTone(0.35));
    MacAudioStats stats;
    MacAudio_GetStats(&stats);
    printf("audio output: callbacks=%llu frames=%llu audible=%llu lock_misses=%llu peak=%.6f\n", (unsigned long long)stats.callbacks, (unsigned long long)stats.frames, (unsigned long long)stats.audible_frames, (unsigned long long)stats.lock_misses, stats.peak);
    CHECK(stats.callbacks > 0 && stats.audible_frames > 1000 && stats.peak > 0.1f);
    AIL_shutdown();
    return 0;
}
#else
#error This test requires Apple COD2_X64.
#endif
