#ifndef BYTEMATCH_H
#define BYTEMATCH_H

#if defined(__GNUC__) && !defined(__clang__) && defined(__i386__) \
    && !defined(__EMSCRIPTEN__) && !defined(_MSC_VER)
#  define BM_GCC_X86 1
#else
#  define BM_GCC_X86 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define BM_NOINLINE       __attribute__((noinline))
#  define BM_ALWAYS_INLINE  __attribute__((always_inline)) inline
#else
#  define BM_NOINLINE
#  define BM_ALWAYS_INLINE  inline
#endif

#if BM_GCC_X86
#  define BM_REGPARM(n)     __attribute__((regparm(n)))
#else
#  define BM_REGPARM(n)
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define BM_ALIGNED(n)     __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#  define BM_ALIGNED(n)     __declspec(align(n))
#else
#  define BM_ALIGNED(n)
#endif

#endif
