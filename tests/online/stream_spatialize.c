#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

snd_local_t g_snd;
#define SND_GLOB_PTR (&g_snd)
static struct MssLocal milesGlob;
static float channelVolumes[16 * 3];
static dvar_t q3fs, reverb;
static dvar_t *reverbPtr = &reverb;
const dvar_t *mss_q3fs = &q3fs;
void *imp_snd_enableReverb = &reverbPtr;
static int streamObject;
static float leftLevel = -1.0f, rightLevel = -1.0f;
void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Printf(const char *format, ...) { (void)format; }
const char *Com_GetSoundFileName(const void *alias) { (void)alias; return "music/stream.mp3"; }
const char *FS_ShortOSFilePath(const char *name) { return name; }
const char *AIL_last_error(void) { return ""; }
void AIL_close_stream(void *stream) { (void)stream; }
void *AIL_open_stream(void *dig, const char *filename, int flags)
{ (void)dig; (void)filename; (void)flags; return &streamObject; }
void AIL_stream_info(void *stream, int *a, int *b, int *c, int *d)
{ (void)stream; (void)a; *b = 0; (void)c; (void)d; }
int AIL_stream_playback_rate(void *stream) { (void)stream; return 44100; }
void AIL_set_stream_playback_rate(void *stream, int rate) { (void)stream; (void)rate; }
void AIL_set_stream_volume_levels(void *S, float left, float right)
{ (void)S; leftLevel = left; rightLevel = right; }
void AIL_set_stream_loop_count(void *S, int count) { (void)S; (void)count; }
void AIL_set_stream_reverb_levels(void *S, float dry, float wet) { (void)S; (void)dry; (void)wet; }
void AIL_stream_ms_position(void *S, long int *total, long int *current)
{ (void)S; *total = 5000; (void)current; }
void AIL_set_stream_ms_position(void *S, int ms) { (void)S; (void)ms; }
void AIL_pause_stream(void *stream, long int onoff) { (void)stream; (void)onoff; }
int AIL_is_3D_stream(void *stream) { (void)stream; return 0; }
void AIL_set_3D_stream_position(void *S, float x, float y, float z) { (void)S; (void)x; (void)y; (void)z; }
float SND_GetLerpedSlavePercentage(float slave) { return slave; }
Bool SND_IsAliasChannel3D(int channel) { return channel == 1; }
int SND_GetListenerIndexNearestToOrigin(const float *org) { (void)org; return 0; }
const vec_t Vec3Normalize(vec_t *v)
{
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length) { v[0] /= length; v[1] /= length; v[2] /= length; }
    return length;
}
float SND_Attenuate(void *curve, float dist, float min, float max)
{ (void)curve; (void)min; return 1.0f - dist / max; }
void SND_SetChannelInfo(int index, int entnum, const void *pAlias0, const void *pAlias1, float lerp,
                        const float *org, float volume, float pitch, int channels, int rate, int total,
                        int start, int startDelay, int master, int system)
{
    snd_channel_info_t *info = &g_snd.chaninfo[index];
    (void)entnum; (void)pitch; (void)rate; (void)total; (void)start; (void)startDelay; (void)system;
    info->pAlias0 = pAlias0;
    info->pAlias1 = pAlias1;
    info->lerp = lerp;
    info->basevolume = volume;
    info->srcChannelCount = channels;
    info->master = master;
    memcpy(info->org, org, sizeof(info->org));
}
#include "stream_spatialize_source.h"
int main(void)
{
    SoundFile file = {0};
    snd_alias_t alias = {0};
    float org[3] = {100.0f, 0.0f, 5.0f};
    const int channel = 0x21;

    file.isStreamFound = 1;
    alias.soundFile = &file;
    alias.fDistMin = 10.0f;
    alias.fDistMax = 1000.0f;
    alias.flags = 1 << 7; /* 3D channel */
    for (int i = 0; i < 16 * 3; ++i)
        channelVolumes[i] = 1.0f;
    g_snd.channelvol = (snd_channelvolgroup *)channelVolumes;
    g_snd.volume = 1.0f;
    g_snd.timescale = 1.0f;
    g_snd.listeners[0].orient.origin[2] = 5.0f;
    g_snd.listeners[0].orient.axis[0][0] = 1.0f;
    g_snd.listeners[0].orient.axis[1][1] = 1.0f;
    g_snd.listeners[0].orient.axis[2][2] = 1.0f;

    /* chaninfo[channel - 0x20] is an unrelated, empty 2D channel. */
    assert(SND_StartAliasStreamOnChannel(&alias, &alias, 0.0f, 0, org, 0.5f, 1.0f, 0, 0.0f, 0, 1,
                                         channel, SASYS_CGAME) == 5000);
    /* 100 units away straight ahead: attenuated by distance, centred pan. */
    assert(fabsf(leftLevel - 0.225f) < 1e-5f && fabsf(rightLevel - 0.225f) < 1e-5f);
    puts("online: streamed 3D sounds spatialize their own channel by distance and listener axis");
    return 0;
}
