/* Keep SDK types separate from the reconstructed engine declarations. */
#include "platform/macos_audio.c"

int AudioOutput_Render(float *pcm, unsigned int frames, int *silent)
{
    AudioUnitRenderActionFlags flags = 0;
    AudioBufferList buffers = {.mNumberBuffers = 1};
    buffers.mBuffers[0].mNumberChannels = 2;
    buffers.mBuffers[0].mDataByteSize = frames * 2 * sizeof(float);
    buffers.mBuffers[0].mData = pcm;
    OSStatus result = Render(NULL, &flags, NULL, 0, frames, &buffers);
    *silent = (flags & kAudioUnitRenderAction_OutputIsSilence) != 0;
    return result;
}
