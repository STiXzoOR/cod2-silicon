#include <speex/speex.h>
#include <dlfcn.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The same driver calls the engine codec and a separately built Xiph library.
 * Libraries run in separate processes, including their libc random state. */
typedef struct CodecAPI {
    __typeof__(&speex_lib_ctl) lib_ctl;
    __typeof__(&speex_encoder_init) encoder_init;
    __typeof__(&speex_encoder_destroy) encoder_destroy;
    __typeof__(&speex_encoder_ctl) encoder_ctl;
    __typeof__(&speex_encode) encode;
    __typeof__(&speex_decoder_init) decoder_init;
    __typeof__(&speex_decoder_destroy) decoder_destroy;
    __typeof__(&speex_decoder_ctl) decoder_ctl;
    __typeof__(&speex_decode) decode;
    __typeof__(&speex_bits_init) bits_init;
    __typeof__(&speex_bits_destroy) bits_destroy;
    __typeof__(&speex_bits_reset) bits_reset;
    __typeof__(&speex_bits_read_from) bits_read_from;
    __typeof__(&speex_bits_write) bits_write;
    __typeof__(&speex_bits_remaining) bits_remaining;
    SpeexMode *mode;
} CodecAPI;

static int LoadAPI(void *library, CodecAPI *api)
{
#define LOAD(name) do { \
        api->name = (__typeof__(api->name))dlsym(library, "speex_" #name); \
        if (!api->name) return 0; \
    } while (0)
    LOAD(lib_ctl);
    LOAD(encoder_init);
    LOAD(encoder_destroy);
    LOAD(encoder_ctl);
    LOAD(encode);
    LOAD(decoder_init);
    LOAD(decoder_destroy);
    LOAD(decoder_ctl);
    LOAD(decode);
    LOAD(bits_init);
    LOAD(bits_destroy);
    LOAD(bits_reset);
    LOAD(bits_read_from);
    LOAD(bits_write);
    LOAD(bits_remaining);
#undef LOAD
    api->mode = (SpeexMode *)dlsym(library, "speex_nb_mode");
    return api->mode != NULL;
}

static void InputFrame(short *pcm, int frame, int count, uint32_t *noise)
{
    for (int i = 0; i < count; ++i) {
        int sample = frame * count + i;
        int phase = sample % 80;
        int triangle = (phase < 40 ? phase : 80 - phase) * 400 - 8000;
        *noise = *noise * UINT32_C(1664525) + UINT32_C(1013904223);
        int jitter = ((int)(*noise >> 24) - 128) * 2;
        pcm[i] = (short)(triangle + (sample % 13 - 6) * 70 + jitter);
    }
}

static int Generate(CodecAPI *api, int mode)
{
    void *state = api->encoder_init(api->mode);
    int count = 0;
    if (!state || api->encoder_ctl(state, SPEEX_SET_MODE, &mode) ||
        api->encoder_ctl(state, SPEEX_GET_FRAME_SIZE, &count) || count != 160)
        return 1;
    SpeexBits bits;
    api->bits_init(&bits);
    uint32_t noise = 1;
    for (int frame = 0; frame < 8; ++frame) {
        short input[160];
        float pcm[160];
        char packet[2000];
        InputFrame(input, frame, count, &noise);
        for (int i = 0; i < count; ++i)
            pcm[i] = (float)input[i];
        api->bits_reset(&bits);
        int result = api->encode(state, pcm, &bits);
        int bytes = api->bits_write(&bits, packet, sizeof(packet));
        printf("{\"frame\":%d,\"result\":%d,\"bits\":%d,\"packet\":\"", frame, result, bits.nbBits);
        for (int i = 0; i < bytes; ++i)
            printf("%02x", (unsigned char)packet[i]);
        puts("\"}");
    }
    api->bits_destroy(&bits);
    api->encoder_destroy(state);
    return 0;
}

static int Nibble(int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

static int Decode(CodecAPI *api, int enhancement)
{
    void *state = api->decoder_init(api->mode);
    int count = 0;
    if (!state || api->decoder_ctl(state, SPEEX_SET_ENH, &enhancement) ||
        api->decoder_ctl(state, SPEEX_GET_FRAME_SIZE, &count) || count != 160)
        return 1;
    SpeexBits bits;
    api->bits_init(&bits);
    char line[4004];
    int frame = 0;
    while (fgets(line, sizeof(line), stdin)) {
        float pcm[160] = {0};
        char packet[2000];
        size_t length = strcspn(line, "\r\n");
        int lost = length == 1 && line[0] == '-';
        if (!lost) {
            if (length % 2 || length > sizeof(packet) * 2)
                return 1;
            for (size_t i = 0; i < length; i += 2) {
                int hi = Nibble(line[i]), lo = Nibble(line[i + 1]);
                if (hi < 0 || lo < 0)
                    return 1;
                packet[i / 2] = (char)(hi * 16 + lo);
            }
            api->bits_read_from(&bits, packet, (int)length / 2);
        }
        int result = api->decode(state, lost ? NULL : &bits, pcm);
        printf("{\"frame\":%d,\"lost\":%s,\"result\":%d,\"remaining\":%d,\"pcm\":[", frame++, lost ? "true" : "false", result, lost ? 0 : api->bits_remaining(&bits));
        for (int i = 0; i < count; ++i) {
            if (!isfinite(pcm[i]))
                return 1;
            printf(i ? ",%.9g" : "%.9g", pcm[i]);
        }
        puts("]}");
    }
    api->bits_destroy(&bits);
    api->decoder_destroy(state);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4 || (strcmp(argv[2], "encode") && strcmp(argv[2], "decode"))) {
        fprintf(stderr, "usage: codec_probe library encode mode | library decode enhancement\n");
        return 1;
    }
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    CodecAPI api;
    if (!library || !LoadAPI(library, &api)) {
        fprintf(stderr, "Cannot load codec: %s\n", dlerror());
        return 1;
    }
    const char *version = NULL;
    if (api.lib_ctl(SPEEX_LIB_GET_VERSION_STRING, &version) || !version)
        return 1;
    printf("{\"version\":\"%s\",\"bitstream_version\":%d}\n", version, api.mode->bitstream_version);
    srand(17);
    int result = !strcmp(argv[2], "encode") ? Generate(&api, atoi(argv[3])) : Decode(&api, atoi(argv[3]));
    dlclose(library);
    return result;
}
