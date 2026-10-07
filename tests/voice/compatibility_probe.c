/* Synthetic compatibility utility. The principal driver remains shared by
 * the engine and unmodified independent Xiph regeneration. */
#define main CodecProbeMain
#include "codec_probe.c"
#undef main
#include "nb_celp.h"

static int CombContracts(void *library)
{
    __typeof__(&comb_filter) filter = (__typeof__(&comb_filter))dlsym(library, "comb_filter");
    if (!filter)
        return 1;
    /* Constant histories make the raw comb output constant too. Expected
     * scaling and smoothing are evaluated independently in contracts.py. */
    const float cases[][5] = {
        {.3f, 0, 0, 0, 1},
        {37.5f, 0, 0, 0, 1},
        {100, -.5f, .75f, -.5f, .8f},
        {100, .75f, 1, .75f, .8f}
    };
    for (int test = 0; test < 4; ++test) {
        float history[240], out[40], lpc[11] = {1};
        float gains[3] = {cases[test][1], cases[test][2], cases[test][3]};
        CombFilterMem memory = {40, {gains[0], gains[1], gains[2]}, cases[test][4]};
        for (int i = 0; i < 240; ++i)
            history[i] = cases[test][0];
        filter(history + 180, out, lpc, 10, 40, 40, gains, .25f, &memory);
        printf("{\"contract\":\"comb\",\"case\":%d,\"pcm\":[", test);
        for (int i = 0; i < 40; ++i) {
            if (!isfinite(out[i]))
                return 1;
            printf(i ? ",%.9g" : "%.9g", out[i]);
        }
        printf("],\"smooth_gain\":%.9g}\n", memory.smooth_gain);
    }
    return 0;
}

static int PitchHistory(CodecAPI *api)
{
    void *state = api->decoder_init(api->mode);
    int enhancement = 0;
    if (!state || api->decoder_ctl(state, SPEEX_SET_ENH, &enhancement))
        return 1;
    SpeexBits bits;
    api->bits_init(&bits);
    char line[4004];
    int frame = 0;
    while (fgets(line, sizeof(line), stdin)) {
        char packet[2000];
        float pcm[160];
        size_t length = strcspn(line, "\r\n");
        if (length % 2 || length > sizeof(packet) * 2)
            return 1;
        for (size_t i = 0; i < length; i += 2) {
            int hi = Nibble(line[i]), lo = Nibble(line[i + 1]);
            if (hi < 0 || lo < 0)
                return 1;
            packet[i / 2] = (char)(hi * 16 + lo);
        }
        api->bits_read_from(&bits, packet, (int)length / 2);
        int result = api->decode(state, &bits, pcm);
        DecState *decoder = (DecState *)state;
        printf("{\"frame\":%d,\"result\":%d,\"pitch_gain\":%.9g,\"last_pitch\":%d}\n",
               frame++, result, decoder->last_pitch_gain, decoder->last_pitch);
    }
    api->bits_destroy(&bits);
    api->decoder_destroy(state);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4 || strcmp(argv[2], "contracts"))
        return 1;
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    CodecAPI api;
    if (!library || !LoadAPI(library, &api))
        return 1;
    puts("{}");
    srand(17);
    int result = CombContracts(library) || PitchHistory(&api);
    dlclose(library);
    return result;
}
