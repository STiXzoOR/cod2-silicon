/* JPEG for the Windows client, on the src/PC/jpeg-6 objects it links. Unlike
 * r_jpeg.c's original path, every field comes from that library's jpeglib.h,
 * so the layout fits the i686 and x86_64 builds alike. */
#if defined(COD2_X64)
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "PC/jpeg-6/jpeglib.h"
#include "PC/jpeg-6/jerror.h"
#include "win32/win_jpeg.h"

typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
    void (*print)(const char *);
} WinJpegError;

typedef struct {
    struct jpeg_destination_mgr pub;
    unsigned char *buffer;
    size_t capacity;
    volatile int overflow;
} WinJpegDestination;

static void WinJpeg_Output(j_common_ptr cinfo)
{
    WinJpegError *error = (WinJpegError *)cinfo->err;
    char message[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, message);
    if (error->print)
        error->print(message);
}

/* libjpeg's own error_exit calls exit(). Return to the caller instead. */
static void WinJpeg_Exit(j_common_ptr cinfo)
{
    (*cinfo->err->output_message)(cinfo);
    longjmp(((WinJpegError *)cinfo->err)->jump, 1);
}

static struct jpeg_error_mgr *WinJpeg_Errors(WinJpegError *error, void (*print)(const char *))
{
    struct jpeg_error_mgr *pub = jpeg_std_error(&error->pub);
    pub->error_exit = WinJpeg_Exit;
    pub->output_message = WinJpeg_Output;
    error->print = print;
    return pub;
}

static void WinJpeg_InitDestination(j_compress_ptr cinfo)
{
    WinJpegDestination *dest = (WinJpegDestination *)cinfo->dest;
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = dest->capacity;
}

static boolean WinJpeg_EmptyOutput(j_compress_ptr cinfo)
{
    ((WinJpegDestination *)cinfo->dest)->overflow = 1;
    longjmp(((WinJpegError *)cinfo->err)->jump, 1);
    return FALSE;
}

static void WinJpeg_TermDestination(j_compress_ptr cinfo)
{
    (void)cinfo;
}

int WinJpeg_Encode(const unsigned char *rgb, int width, int height, int quality,
                   unsigned char *out, size_t capacity, size_t *size, void (*print)(const char *))
{
    struct jpeg_compress_struct cinfo;
    WinJpegError error;
    WinJpegDestination dest;

    *size = 0;
    memset(&cinfo, 0, sizeof(cinfo));
    memset(&dest, 0, sizeof(dest));
    cinfo.err = WinJpeg_Errors(&error, print);
    if (setjmp(error.jump)) {
        jpeg_destroy_compress(&cinfo);
        return dest.overflow ? WIN_JPEG_OVERFLOW : WIN_JPEG_ERROR;
    }
    jpeg_create_compress(&cinfo);
    dest.pub.init_destination = WinJpeg_InitDestination;
    dest.pub.empty_output_buffer = WinJpeg_EmptyOutput;
    dest.pub.term_destination = WinJpeg_TermDestination;
    dest.buffer = out;
    dest.capacity = capacity;
    cinfo.dest = &dest.pub;
    cinfo.image_width = (JDIMENSION)width;
    cinfo.image_height = (JDIMENSION)height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = (JSAMPROW)(rgb + (size_t)cinfo.next_scanline * (size_t)width * 3);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    *size = capacity - dest.pub.free_in_buffer;
    jpeg_destroy_compress(&cinfo);
    return WIN_JPEG_OK;
}

static void WinJpeg_InitSource(j_decompress_ptr cinfo)
{
    (void)cinfo;
}

/* Truncated data: end the image as libjpeg's stdio source does. */
static boolean WinJpeg_FillInput(j_decompress_ptr cinfo)
{
    static const JOCTET end[2] = { 0xff, JPEG_EOI };
    WARNMS(cinfo, JWRN_JPEG_EOF);
    cinfo->src->next_input_byte = end;
    cinfo->src->bytes_in_buffer = 2;
    return TRUE;
}

static void WinJpeg_SkipInput(j_decompress_ptr cinfo, long count)
{
    struct jpeg_source_mgr *src = cinfo->src;
    if (count <= 0)
        return;
    if ((size_t)count > src->bytes_in_buffer)
        count = (long)src->bytes_in_buffer;
    src->next_input_byte += count;
    src->bytes_in_buffer -= (size_t)count;
}

static void WinJpeg_TermSource(j_decompress_ptr cinfo)
{
    (void)cinfo;
}

/* On WIN_JPEG_ERROR, *pixels may hold an allocation for the caller to free. */
int WinJpeg_Decode(const unsigned char *data, size_t size, int maxDimension, void *(*alloc)(int),
                   unsigned char **pixels, int *width, int *height, void (*print)(const char *))
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_source_mgr src;
    WinJpegError error;
    unsigned char *volatile buffer = NULL;
    int result = WIN_JPEG_OK;

    *pixels = NULL;
    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = WinJpeg_Errors(&error, print);
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&cinfo);
        *pixels = buffer;
        return WIN_JPEG_ERROR;
    }
    jpeg_create_decompress(&cinfo);
    src.next_input_byte = data;
    src.bytes_in_buffer = size;
    src.init_source = WinJpeg_InitSource;
    src.fill_input_buffer = WinJpeg_FillInput;
    src.skip_input_data = WinJpeg_SkipInput;
    src.resync_to_restart = jpeg_resync_to_restart;
    src.term_source = WinJpeg_TermSource;
    cinfo.src = &src;
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    long long w = cinfo.output_width, h = cinfo.output_height;
    if (w <= 0 || h <= 0 || w * h * 4 > INT_MAX)
        result = WIN_JPEG_BAD_SIZE;
    else if (w > maxDimension || h > maxDimension)
        result = WIN_JPEG_TOO_LARGE;
    else if (cinfo.output_components != 3)
        result = WIN_JPEG_NOT_RGB;
    if (result != WIN_JPEG_OK) {
        jpeg_destroy_decompress(&cinfo);
        return result;
    }

    buffer = alloc((int)(w * h * 4));
    unsigned char *row = buffer;
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW line = row;
        jpeg_read_scanlines(&cinfo, &line, 1);
        /* Widen RGB to RGBA in place, from the right. */
        for (long long x = w - 1; x >= 0; --x) {
            row[x * 4 + 3] = 0xff;
            row[x * 4 + 2] = row[x * 3 + 2];
            row[x * 4 + 1] = row[x * 3 + 1];
            row[x * 4 + 0] = row[x * 3 + 0];
        }
        row += w * 4;
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    *pixels = buffer;
    *width = (int)w;
    *height = (int)h;
    return WIN_JPEG_OK;
}
#endif
