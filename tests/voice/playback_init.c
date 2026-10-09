#include "common_types.h"
#include "imports.h"
#include "platform/macos_audio.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Bool Voice_Init(void);
extern void Voice_Shutdown(void);
extern void Voice_Playback(void);
extern int Voice_GetLocalVoiceData(ClientVoicePacket_t *packet);
extern float Voice_GetVoiceLevel(void);
extern Bool Voice_IsClientTalking(int client);
extern void Voice_IncomingVoiceData(int talker, unsigned char *packet, int length);
extern int MSG_ReadByte(msg_t *message);
extern void MSG_ReadData(msg_t *message, void *data, int length);
extern int Decode_Sample(char *packet, int length, short *output, int frameSize);
extern int g_voice_initialized, g_frame_size, g_speex_initialized;
void *g_decoder;
int g_decode_frame_size;
extern char g_recording_initialized, g_currently_recording;
extern float voice_current_voicelevel;
extern const dvar_t *winvoice_mic_reclevel;
extern int AudioOutput_Render(float *pcm, unsigned int frames, int *silent);
extern int Native_Record_Init(int initialize, const void *handle);

int g_encoder_samplerate = 8192, g_encoder_quality = 1;
void *imp_speex_nb_mode, *imp_speex_wb_mode, *imp_speex_uwb_mode;
void *imp_frame_msec;
extern const SpeexMode speex_nb_mode;
static dvar_t dvars[4], voiceEnabled;
const dvar_t *cl_voice = &voiceEnabled;
static int dvarCount, now = 1000, muted, captureAvailable, captureCalls, mixerCalls, driverCalls;
static int driverAvailable = 1;
static kbutton_t kb[28];
static recordingSample_t captureSample;

void Com_Printf(const char *format, ...) { (void)format; }
int Sys_Milliseconds(void) { return now; }
const char *Cmd_Argv(int index) { (void)index; return ""; }
Bool CL_IsPlayerMuted(int client) { (void)client; return muted; }
const dvar_t *Dvar_RegisterBool(const char *name, int value, int flags)
{
    (void)name; (void)flags;
    dvar_t *dvar = &dvars[dvarCount++];
    dvar->current.enabled = value;
    return dvar;
}
const dvar_t *Dvar_RegisterFloat(const char *name, float value, float min, float max, int flags)
{
    (void)name; (void)min; (void)max; (void)flags;
    dvar_t *dvar = &dvars[dvarCount++];
    dvar->current.value = value;
    return dvar;
}

/* Only device creation and unavailable capture boundaries are controlled.
   Decode, queue ownership, sample buffering, resampling and Render are real. */
void *AIL_open_digital_driver(int rate, int bits, int channels, int flags)
{
    (void)flags;
    assert(rate == 11025 && bits == 16 && channels == 2);
    driverCalls++;
    return driverAvailable ? (void *)(uintptr_t)1 : NULL;
}
int Record_Init(int initialize, const void *handle)
{
    int result = Native_Record_Init(initialize, handle);
    if (captureAvailable) {
        g_recording_initialized = 1;
        return 1;
    }
    return result;
}
recordingSample_t *Record_NewSample(void) { captureCalls++; return &captureSample; }
int Record_Start(recordingSample_t *sample) { assert(sample == &captureSample); captureCalls++; return 0; }
int Record_Stop(recordingSample_t *sample) { assert(sample == &captureSample); captureCalls++; return 0; }
void Record_Frame(void) { captureCalls++; }
void Record_Shutdown(void) { captureCalls++; g_recording_initialized = g_currently_recording = 0; }
int mixerGetRecordSource(char *source) { mixerCalls++; strcpy(source, "test"); return 1; }
int mixerSetRecordSource(const char *source) { (void)source; mixerCalls++; return 1; }
int mixerGetRecordLevel(const char *source) { (void)source; mixerCalls++; return 1234; }
int mixerSetRecordLevel(const char *source, int level) { (void)source; (void)level; mixerCalls++; return 0; }
int mixerSetMicrophoneMute(int mute) { (void)mute; mixerCalls++; return 0; }

#include "client_receive.h"

static void Deliver(const unsigned char *packet, int length, int talker)
{
    byte bytes[259] = {1, (byte)talker, (byte)length};
    memcpy(bytes + 3, packet, length);
    msg_t message = {0};
    message.data = bytes;
    message.cursize = length + 3;
    CL_VoicePacket(&message);
    assert(message.readcount == message.cursize && !message.overflowed);
}

static int RenderVoice(void)
{
    float output[2048];
    int silent, audible = 0;
    assert(AudioOutput_Render(output, 1024, &silent) == 0);
    for (int i = 0; i < 2048; i++) {
        assert(isfinite(output[i]));
        audible += fabsf(output[i]) > 0.000001f;
    }
    assert(silent == !audible);
    return audible;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    unsigned char packet[256];
    int length = (int)strlen(argv[1]) / 2;
    assert(length > 0 && length < 256);
    for (int i = 0; i < length; i++) {
        unsigned int byte;
        assert(sscanf(argv[1] + i * 2, "%2x", &byte) == 1);
        packet[i] = byte;
    }
    imp_speex_nb_mode = (void *)&speex_nb_mode;
    voiceEnabled.current.enabled = 1;
    for (int cycle = 0; cycle < 2; cycle++) {
        dvarCount = 0;
        /* The historical return value is zero even after successful init. */
        assert(Voice_Init() == 0);
        Deliver(packet, length, 7);
        assert(g_voice_initialized && g_decoder && g_speex_initialized && g_frame_size == 160);
        assert(!g_recording_initialized && !g_currently_recording);
        Voice_Playback();
        assert(RenderVoice() > 0);
        short decoded[320];
        for (int i = 0; i < 320; i++)
            decoded[i] = 12345;
        assert(Decode_Sample((char *)packet, length, decoded, 160) == 320);
        for (int i = 160; i < 320; i++)
            assert(decoded[i] == 12345);
        assert(Voice_IsClientTalking(7));
        now += 299;
        assert(Voice_IsClientTalking(7));
        now++;
        assert(!Voice_IsClientTalking(7) && !Voice_IsClientTalking(0));
        for (int talker = 0; talker < 64; talker++) {
            Deliver(packet, length, talker);
            Voice_Playback();
            assert(RenderVoice() > 0 && Voice_IsClientTalking(talker));
        }
        muted = 1;
        Deliver(packet, length, 7);
        assert(RenderVoice() == 0);
        muted = 0;
        voiceEnabled.current.enabled = 0;
        Deliver(packet, length, 7);
        assert(RenderVoice() == 0);
        voiceEnabled.current.enabled = 1;

        ClientVoicePacket_t local, unchanged;
        memset(&local, 0xa5, sizeof(local));
        unchanged = local;
        IN_TalkDown();
        assert(IsTalking());
        for (int frame = 0; frame < 100; frame++) {
            assert(Voice_GetLocalVoiceData(&local) == 0);
            Voice_Playback();
        }
        IN_TalkUp();
        assert(!IsTalking() && !memcmp(&local, &unchanged, sizeof(local)));
        assert(Voice_GetVoiceLevel() == 0 && voice_current_voicelevel == 0);
        ((dvar_t *)winvoice_mic_reclevel)->current.value = 2345;
        Voice_Playback();
        Voice_Shutdown();
        Voice_Shutdown();
        assert(!g_voice_initialized && !g_decoder && !g_speex_initialized);
        assert(!g_recording_initialized && !g_currently_recording);
        now += 300;
        Deliver(packet, length, 7);
        assert(!Voice_IsClientTalking(7));
        assert(RenderVoice() == 0 && captureCalls == 0 && mixerCalls == 0);
    }
    assert(driverCalls == 2);
    puts("PASS unavailable capture: Voice_Init -> CL_VoicePacket -> Speex -> 64 native queues -> Render; mute/cl_voice, talk timeout, +talk, two lifecycles, no capture/mixer calls");

    driverAvailable = 0;
    dvarCount = 0;
    Voice_Init();
    assert(!g_voice_initialized && !g_decoder && !g_speex_initialized);
    Deliver(packet, length, 7);
    Voice_Playback();
    assert(RenderVoice() == 0 && captureCalls == 0 && mixerCalls == 0);
    Voice_Shutdown();
    driverAvailable = 1;
    puts("PASS output-device failure and late packets after shutdown remain inactive");

    /* A controlled successful recorder checks capture/mixer call counts. */
    captureAvailable = 1;
    dvarCount = 0;
    Voice_Init();
    assert(g_voice_initialized && mixerCalls == 5);
    ClientVoicePacket_t local = {0};
    Voice_GetLocalVoiceData(&local);
    assert(captureCalls == 3);
    Voice_Shutdown();
    assert(captureCalls == 5 && mixerCalls == 8);
    puts("PASS successful-capture control: original mixer setup, record start/frame/stop and teardown call counts");
    return 0;
}
