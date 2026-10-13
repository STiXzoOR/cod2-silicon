/* Reuse the receive fixture's device/capture boundaries, not its test main. */
#define VOICE_RATE_CONTRACT 1
#define main PlaybackInit_ControlMain
#include "playback_init.c"
#undef main

extern int g_sound_recordFrequency;

#define OUTPUT_RATE 48000
#define OUTPUT_FRAMES 65536

static double TonePower(const float *pcm, int start, int count, double frequency)
{
    double angle = 6.283185307179586 * frequency / OUTPUT_RATE;
    double c = cos(angle), s = sin(angle), real = 1, imag = 0, re = 0, im = 0;
    for (int i = 0; i < count; i++) {
        double window = 0.5 - 0.5 * cos(6.283185307179586 * i / (count - 1));
        double value = pcm[(start + i) * 2] * window;
        re += value * real;
        im += value * imag;
        double next = real * c - imag * s;
        imag = imag * c + real * s;
        real = next;
    }
    return re * re + im * im;
}

static double DominantFrequency(const float *pcm)
{
    /* Fixed 0.25 s settled window fits both the correct and accelerated stream. */
    double peak = 0, frequency = 0;
    for (int hz = 400; hz <= 800; hz++) {
        double power = TonePower(pcm, 12000, 12000, hz);
        if (power > peak) {
            peak = power;
            frequency = hz;
        }
    }
    double center = frequency;
    for (int step = -8; step <= 8; step++) {
        double hz = center + step * 0.125;
        double power = TonePower(pcm, 12000, 12000, hz);
        if (power > peak) {
            peak = power;
            frequency = hz;
        }
    }
    assert(peak > 1);
    return frequency;
}

static void CheckGameSound(void *sample)
{
    short input[480];
    for (int i = 0; i < 480; i++)
        input[i] = 4096;
    AIL_set_sample_type(sample, 1, 0);
    AIL_set_sample_address(sample, input, sizeof(input));
    AIL_set_sample_playback_rate(sample, 48000);
    AIL_resume_sample(sample);
    float pcm[962];
    int silent;
    assert(AudioOutput_Render(pcm, 481, &silent) == 0 && !silent);
    assert(AIL_sample_playback_rate(sample) == 48000 && AIL_sample_status(sample) == 2);
    for (int i = 0; i < 960; i++)
        assert(pcm[i] == 0.125f);
    assert(pcm[960] == 0 && pcm[961] == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    if (!strcmp(argv[2], "zero"))
        g_sound_recordFrequency = 0;
    else if (!strcmp(argv[2], "negative"))
        g_sound_recordFrequency = -1;
    else
        assert(!strcmp(argv[2], "default"));
    /* A game sample allocated before voice startup must retain rate and output. */
    void *game = AIL_allocate_sample_handle((void *)(uintptr_t)1);
    assert(game);
    CheckGameSound(game);
    imp_speex_nb_mode = (void *)&speex_nb_mode;
    voiceEnabled.current.enabled = 1;
    srand(17);
    Voice_Init();
    assert(g_voice_initialized && g_frame_size == 160);
    CheckGameSound(game);
    void *newGame = AIL_allocate_sample_handle((void *)(uintptr_t)1);
    assert(newGame);
    CheckGameSound(newGame);
    AIL_release_sample_handle(newGame);
    FILE *packets = fopen(argv[1], "r");
    assert(packets);
    char line[513];
    int frames = 0;
    while (fgets(line, sizeof(line), packets)) {
        unsigned char packet[256];
        int length = (int)strcspn(line, "\n") / 2;
        assert(length > 0 && length < 256);
        for (int i = 0; i < length; i++) {
            unsigned int value;
            assert(sscanf(line + i * 2, "%2x", &value) == 1);
            packet[i] = value;
        }
        Deliver(packet, length, 7);
        frames++;
    }
    fclose(packets);
    assert(frames == 64);
    float *pcm = calloc(OUTPUT_FRAMES * 2, sizeof(float));
    assert(pcm);
    int last = -1;
    for (int offset = 0; offset < OUTPUT_FRAMES; offset += 128) {
        int silent;
        Voice_Playback();
        assert(AudioOutput_Render(pcm + offset * 2, 128, &silent) == 0);
        for (int i = offset; i < offset + 128; i++) {
            assert(isfinite(pcm[i * 2]) && isfinite(pcm[i * 2 + 1]));
            assert(pcm[i * 2] == pcm[i * 2 + 1]);
            if (fabsf(pcm[i * 2]) > 0.0000001f)
                last = i;
        }
    }
    double hz = DominantFrequency(pcm);
    /* 64 * 160 / 8192 = 1.25 s = exactly 60000 output frames. */
    int durationOK = abs(last + 1 - 60000) <= 1;
    int pitchOK = fabs(hz - 512) <= 0.25;
    int rateOK = strcmp(argv[2], "default") || g_sound_recordFrequency == g_encoder_samplerate;
    printf("{\"case\":\"%s\",\"rendered_frames\":%d,\"duration_seconds\":%.9f,"
           "\"dominant_hz\":%.3f,\"duration_pass\":%s,\"pitch_pass\":%s,\"record_rate_pass\":%s}\n",
           argv[2], last + 1, (last + 1.0) / OUTPUT_RATE, hz,
           durationOK ? "true" : "false", pitchOK ? "true" : "false", rateOK ? "true" : "false");
    free(pcm);
    Voice_Shutdown();
    CheckGameSound(game);
    AIL_release_sample_handle(game);
    AIL_shutdown();
    return !(durationOK && pitchOK && rateOK);
}
