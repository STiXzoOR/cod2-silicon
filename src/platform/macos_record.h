/* The block of captured sound handed to the engine's recording callback.
 * It must match audioSample_t; macos_voice.c, which can include the engine's
 * types, checks that. The capture code cannot include them next to Apple's
 * audio headers, whose type names collide. */
#ifndef MACOS_RECORD_H
#define MACOS_RECORD_H
typedef struct {
    unsigned char *buffer;
    int lengthInBytes;
    int lengthInSamples;
    int bytesPerSample;
    int frequency;
    unsigned char stereo;
    int channels;
    int sampleOffset;
} MacRecordBlock;
#endif
