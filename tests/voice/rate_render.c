#include "../audio/render.c"

void *VoiceRate_OpenDigitalDriver(int rate, int bits, int channels, int flags)
{
    /* Model an existing game output unit: retain the real driver's validation
       and default-rate update without creating or starting hardware output. */
    output_unit = (AudioUnit)(uintptr_t)1;
    void *driver = AIL_open_digital_driver(rate, bits, channels, flags);
    output_unit = NULL;
    return driver;
}
