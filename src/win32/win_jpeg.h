#ifndef COD2_WIN_JPEG_H
#define COD2_WIN_JPEG_H
#include <stddef.h>

/* The src/PC/jpeg-6 library, through its own jpeglib.h. The renderer headers
 * describe the original i386 library instead, which doesn't fit either MinGW
 * build. 'print' receives libjpeg's messages. */
enum {
    WIN_JPEG_OK,
    WIN_JPEG_ERROR,     /* libjpeg raised an error */
    WIN_JPEG_OVERFLOW,  /* the output didn't fit in 'capacity' */
    WIN_JPEG_BAD_SIZE,  /* zero size, or more than INT_MAX bytes of RGBA */
    WIN_JPEG_TOO_LARGE, /* a side is larger than maxDimension */
    WIN_JPEG_NOT_RGB    /* not three components */
};

int WinJpeg_Encode(const unsigned char *rgb, int width, int height, int quality,
                   unsigned char *out, size_t capacity, size_t *size, void (*print)(const char *));
/* On WIN_JPEG_OK, *pixels holds width * height RGBA texels from alloc. */
int WinJpeg_Decode(const unsigned char *data, size_t size, int maxDimension, void *(*alloc)(int),
                   unsigned char **pixels, int *width, int *height, void (*print)(const char *));
#endif
