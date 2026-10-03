#ifndef COD2_MACOS_JPEG_H
#define COD2_MACOS_JPEG_H
#include <stddef.h>
unsigned char *MacJpeg_Encode(const unsigned char *rgb, int width, int height, int quality, size_t *size);
unsigned char *MacJpeg_Decode(const unsigned char *data, size_t size, int maxDimension, int *width, int *height);
#endif
