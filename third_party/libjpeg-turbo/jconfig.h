/* jconfig.h for the CoD2 port's COD2_FEATURE_MODERN_LIBS build.
 *
 * Hand-generated from libjpeg-turbo 3.0.4's jconfig.h.in for a generic
 * 32-bit Linux (i386) target, libjpeg v6.2 ABI.  No SIMD, no JPEG7/8.
 */

/* Version ID for the JPEG library. v6.2 ABI = 62 (binary-compatible with the
 * extern jpeg_* decls in PC/gfx_d3d/r_jpeg.c, which pass version 0x3e == 62). */
#define JPEG_LIB_VERSION  62

/* libjpeg-turbo version */
#define LIBJPEG_TURBO_VERSION  "3.0.4"

/* libjpeg-turbo version in integer form */
#define LIBJPEG_TURBO_VERSION_NUMBER  3000004

/* Support arithmetic encoding when using 8-bit samples (upstream default). */
#define C_ARITH_CODING_SUPPORTED 1

/* Support arithmetic decoding when using 8-bit samples (upstream default). */
#define D_ARITH_CODING_SUPPORTED 1

/* Support in-memory source/destination managers. */
#define MEM_SRCDST_SUPPORTED  1

/* No SIMD: WITH_SIMD left undefined so the C fallback paths are used. */
/* #undef WITH_SIMD */

/* Run-time-selectable precision; JSAMPLE is still 8-bit. */
#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE  8
#endif

/* GCC on i386-Linux shifts signed values arithmetically (not unsigned). */
/* #undef RIGHT_SHIFT_IS_UNSIGNED */
