/* jconfigint.h for the CoD2 port's COD2_FEATURE_MODERN_LIBS build.
 *
 * Hand-generated from libjpeg-turbo 3.0.4's jconfigint.h.in for a generic
 * 32-bit Linux (i386) GCC target.
 */

/* libjpeg-turbo build number */
#define BUILD  "cod2-modern-libs"

/* How to hide global symbols (no hiding; standard linkage). */
#define HIDDEN

/* Compiler's inline keyword (GCC supports `inline`; leave as-is). */
/* #undef inline */

/* How to obtain function inlining. */
#define INLINE  __inline__ __attribute__((always_inline))

/* How to obtain thread-local storage. */
#define THREAD_LOCAL  __thread

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "3.0.4"

/* The size of `size_t', as computed by sizeof (4 on i386). */
#define SIZEOF_SIZE_T  4

/* GCC has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t) on i386. */
#define HAVE_BUILTIN_CTZL

/* No <intrin.h> (MSVC only). */
/* #undef HAVE_INTRIN_H */

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH  __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

#ifndef BITS_IN_JSAMPLE
#define BITS_IN_JSAMPLE  8      /* use 8 or 12 */
#endif

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#undef WITH_SIMD

#if BITS_IN_JSAMPLE == 8

/* Support arithmetic encoding. */
#define C_ARITH_CODING_SUPPORTED 1

/* Support arithmetic decoding. */
#define D_ARITH_CODING_SUPPORTED 1

/* No SIMD. */
/* #undef WITH_SIMD */

#endif
