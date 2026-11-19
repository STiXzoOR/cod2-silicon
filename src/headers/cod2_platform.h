#ifndef CLEAN_COD2_PLATFORM
#define CLEAN_COD2_PLATFORM

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#ifndef _WIN32
#include <netinet/in.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include "cod2_feature_config.h"

/* The reconciled engine on Windows, under MinGW, on either architecture. */
#ifndef COD2_WINDOWS_NATIVE
#define COD2_WINDOWS_NATIVE 0
#endif

/* Native builds use SDK declarations, never the i386 STABS copies. The name
 * predates the Linux and Windows ports, which take the same engine paths. */
#if ((defined(__APPLE__) || defined(__linux__)) && defined(COD2_X64)) || COD2_WINDOWS_NATIVE
#define COD2_APPLE_SDK 1
#if !COD2_WINDOWS_NATIVE
#include <sys/resource.h>
#endif
#if defined(__APPLE__)
#define COD2_MALLOC_HEADER <malloc/malloc.h>
#else
#define COD2_MALLOC_HEADER <malloc.h>
#endif
#else
#define COD2_APPLE_SDK 0
#endif

/* Spots a 64-bit pointer that passed through a 32-bit slot, zero or sign extended.
 * Every 32-bit pointer would look like one, so this is false there. It is false
 * with the system Direct3D 9 too: with d3d9.dll loaded, 64-bit Windows places
 * engine allocations below 4 GB. The Mac build that draws through Direct3D 9
 * keeps the check: its allocations stay above 4 GB. */
#if __SIZEOF_POINTER__ == 8 && (!COD2_WIN32_D3D9 || defined(__APPLE__))
#define COD2_POINTER_LOOKS_TRUNCATED(pointer) ((uintptr_t)(pointer) + 0x100000000ULL < 0x200000000ULL)
#else
#define COD2_POINTER_LOOKS_TRUNCATED(pointer) 0
#endif

#if defined(__i386__) && defined(__GNUC__) && !defined(__clang__) && !defined(__EMSCRIPTEN__) && !COD2_WINDOWS_NATIVE
#define COD2_GCC_X86_ABI 1
#else
#define COD2_GCC_X86_ABI 0
#endif

#if COD2_GCC_X86_ABI && defined(__APPLE_CC__)
#define COD2_APPLE_GCC_X86_ABI 1
#else
#define COD2_APPLE_GCC_X86_ABI 0
#endif

#if defined(__EMSCRIPTEN__) || ((defined(__APPLE__) || defined(__linux__)) && defined(COD2_X64)) || COD2_WINDOWS_NATIVE
#define __attribute_regparm__(n)
#define __attribute_sseregparm__
#else
#define __attribute_regparm__(n) __attribute__((regparm(n)))
#if COD2_GCC_X86_ABI
#define __attribute_sseregparm__ __attribute__((sseregparm))
#else
#define __attribute_sseregparm__
#endif
#endif

#if COD2_GCC_X86_ABI
#define COD2_REGPARM(n) __attribute__((regparm(n)))
#define COD2_SSEREGPARM __attribute__((sseregparm))
#define COD2_CDECL __attribute__((cdecl))
#define COD2_STDCALL __attribute__((stdcall))
#define COD2_FASTCALL __attribute__((fastcall))
#define COD2_FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#define COD2_ALIGNED(n) __attribute__((aligned(n)))
#else
#define COD2_REGPARM(n)
#define COD2_SSEREGPARM
#define COD2_CDECL
#define COD2_STDCALL
#define COD2_FASTCALL
#define COD2_FORCE_ALIGN_ARG_POINTER
#define COD2_ALIGNED(n)
#endif

#if COD2_APPLE_GCC_X86_ABI
#define COD2_REGPARM_AND_STACKPARM __attribute__((regparmandstackparm))
#else
#define COD2_REGPARM_AND_STACKPARM
#endif

#endif
