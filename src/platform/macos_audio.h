#ifndef COD2_MACOS_AUDIO_H
#define COD2_MACOS_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#if defined(__APPLE__) && defined(COD2_X64)

/* Matches the native AILSOUNDINFO fields, without importing reconstructed SDK types. */
typedef struct {
    long format;
    const void *data_ptr;
    unsigned long data_len;
    unsigned long rate;
    long bits;
    long channels;
    unsigned long samples;
    unsigned long block_size;
    const void *initial_ptr;
} MacAudioSoundInfo;

typedef struct {
    uint64_t callbacks;
    uint64_t frames;
    uint64_t audible_frames;
    uint64_t lock_misses;
    float peak;
} MacAudioStats;

int MacAudio_PlayTone(double seconds);
void MacAudio_GetStats(MacAudioStats *stats);
void MacAudio_Mix(float *stereo, size_t frames);

int AIL_startup(int flags);
void AIL_shutdown(void);
int AIL_set_preference(int pref, int value);
const char *AIL_last_error(void);
void AIL_set_redist_directory(const char *dir);
void *AIL_open_digital_driver(int frequency, int bits, int channels, int flags);
int AIL_digital_CPU_percent(void *driver);
void *AIL_allocate_sample_handle(void *driver);
void AIL_release_sample_handle(void *sample);
void AIL_init_sample(void *sample);
void AIL_set_sample_type(void *sample, int format, int flags);
void AIL_set_sample_address(void *sample, const void *data, int size);
void AIL_set_sample_adpcm_block_size(void *sample, int size);
void AIL_set_sample_playback_rate(void *sample, int rate);
int AIL_sample_playback_rate(void *sample);
void AIL_stop_sample(void *sample);
void AIL_resume_sample(void *sample);
void AIL_end_sample(void *sample);
unsigned long AIL_sample_status(void *sample);
void AIL_set_sample_volume_levels(void *sample, float left, float right);
void AIL_sample_volume_levels(void *sample, float *left, float *right);
void AIL_sample_volume_pan(void *sample, float *volume, float *pan);
void AIL_set_sample_reverb_levels(void *sample, float dry, float wet);
void AIL_set_sample_loop_count(void *sample, int count);
unsigned long AIL_sample_position(void *sample);
void AIL_set_sample_ms_position(void *sample, int ms);
void AIL_sample_ms_position(void *sample, long *total, long *current);
int AIL_sample_buffer_ready(void *sample);
void AIL_load_sample_buffer(void *sample, int index, const void *data, int size);
int AIL_minimum_sample_buffer_size(void *driver, int rate, int format);
void AIL_set_digital_master_room_type(void *driver, int room);
void AIL_set_digital_master_reverb_levels(void *driver, float dry, float wet);
int AIL_WAV_info(const void *data, void *info);
int AIL_size_processed_digital_audio(int rate, int format, int count, const void *info);
long AIL_process_digital_audio(void *dest, long size, unsigned long rate, unsigned long format, long count, const void *info);

typedef unsigned long (*MacAudioOpenCallback)(const char *, unsigned long *);
typedef void (*MacAudioCloseCallback)(unsigned long);
typedef long (*MacAudioSeekCallback)(unsigned long, long, unsigned long);
typedef unsigned long (*MacAudioReadCallback)(unsigned long, void *, unsigned long);
void AIL_set_file_callbacks(MacAudioOpenCallback open_cb, MacAudioCloseCallback close_cb, MacAudioSeekCallback seek_cb, MacAudioReadCallback read_cb);
void *AIL_open_stream(void *driver, const char *filename, int memory);
void AIL_close_stream(void *stream);
void AIL_pause_stream(void *stream, long pause);
void AIL_set_stream_volume_levels(void *stream, float left, float right);
void AIL_stream_volume_levels(void *stream, float *left, float *right);
void AIL_stream_volume_pan(void *stream, float *volume, float *pan);
void AIL_set_stream_reverb_levels(void *stream, float dry, float wet);
void AIL_set_stream_playback_rate(void *stream, int rate);
int AIL_stream_playback_rate(void *stream);
void AIL_set_stream_loop_count(void *stream, int count);
long AIL_stream_status(void *stream);
void AIL_stream_info(void *stream, int *datarate, int *format, int *length, int *memory);
void AIL_set_stream_ms_position(void *stream, int ms);
void AIL_stream_ms_position(void *stream, long *total, long *current);
int AIL_is_3D_stream(void *stream);
void AIL_set_3D_stream_position(void *stream, float x, float y, float z);

int AIL_enumerate_3D_providers(void *next, void **provider, const char **name);
int AIL_open_3D_provider(void *provider);
void AIL_close_3D_provider(void *provider);
void AIL_3D_provider_attribute(void *provider, const char *name, void *value);
void *AIL_allocate_3D_sample_handle(void *provider);
void AIL_stop_3D_sample(void *sample);
void AIL_resume_3D_sample(void *sample);
void AIL_end_3D_sample(void *sample);
int AIL_set_3D_sample_info(void *sample, const void *info);
void AIL_set_3D_sample_volume(void *sample, float volume);
float AIL_3D_sample_volume(void *sample);
void AIL_set_3D_sample_offset(void *sample, int offset);
unsigned long AIL_3D_sample_offset(void *sample);
void AIL_set_3D_sample_playback_rate(void *sample, int rate);
int AIL_3D_sample_playback_rate(void *sample);
void AIL_set_3D_sample_loop_count(void *sample, int count);
unsigned long AIL_3D_sample_status(void *sample);
int AIL_3D_sample_length(void *sample);
void AIL_set_3D_room_type(void *provider, int room);
void AIL_set_3D_rolloff_factor(void *provider, float factor);
void AIL_set_3D_distance_factor(void *provider, float factor);
void AIL_set_3D_sample_distances(void *sample, float min_distance, float max_distance);
void AIL_set_3D_sample_effects_level(void *sample, float wet);
void AIL_set_3D_position(void *object, float x, float y, float z);
void AIL_3D_position(void *object, float *x, float *y, float *z);

#endif
#endif
