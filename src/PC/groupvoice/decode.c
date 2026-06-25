
#include "common_types.h"
#include "imports.h"


int g_current_decode_bandwidth_setting = 0x0;

extern void *speex_decoder_init(const void *mode);
extern int speex_decoder_ctl(void *state, int request, void *ptr);
extern int speex_encoder_ctl(void *state, int request, void *ptr);
extern void speex_decoder_destroy(void *state);
extern void speex_bits_init(SpeexBits *bits);
extern void speex_bits_destroy(SpeexBits *bits);
extern void speex_bits_read_from(SpeexBits *bits, char *buffer, int len);
extern int speex_decode(void *state, SpeexBits *bits, float *out);
extern void Com_Printf(const char *fmt, ...);

extern const void *speex_nb_mode_ptr;
extern const void *speex_wb_mode_ptr;
extern const void *speex_uwb_mode_ptr;
extern int *speex_quality_ptr;

extern void *g_decoder;
extern int g_current_decode_bandwidth_setting;
extern int g_decode_frame_size;
static SpeexBits decodeBits;

Bool Decode_Init(int bandwidthEnum)
{
    const void *mode;
    int tmp = 1;

    switch (bandwidthEnum) {
    case 0:
        mode = speex_nb_mode_ptr;
        break;
    case 1:
        mode = speex_wb_mode_ptr;
        break;
    case 2:
        mode = speex_uwb_mode_ptr;
        break;
    default:
        Com_Printf("Unknown bandwidth mode %i\n", bandwidthEnum);
        return 0;
    }

    g_decoder = speex_decoder_init(mode);
    speex_decoder_ctl(g_decoder, 0, &tmp);
    speex_decoder_ctl(g_decoder, 0x18, speex_quality_ptr);
    speex_encoder_ctl(g_decoder, 3, &g_decode_frame_size);
    g_current_decode_bandwidth_setting = bandwidthEnum;
    speex_bits_init(&decodeBits);
    return 1;
}

void Decode_Shutdown(void)
{
    if (g_decoder) {
        speex_bits_destroy(&decodeBits);
        speex_decoder_destroy(g_decoder);
    }
    g_decoder = 0;
}

int Decode_Sample(char *buffer, int maxLength, short *out, int frame_size)
{
    float output[4096];
    int i, count;

    speex_bits_read_from(&decodeBits, buffer, maxLength);

    if (speex_decode(g_decoder, &decodeBits, output) != 0)
        return 0;

    count = frame_size * 2;
    for (i = 0; i < count; i++) {
        out[i] = (short)(int)output[i];
    }

    return count;
}
