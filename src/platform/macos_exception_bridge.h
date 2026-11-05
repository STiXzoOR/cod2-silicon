#ifndef COD2_MACOS_EXCEPTION_BRIDGE_H
#define COD2_MACOS_EXCEPTION_BRIDGE_H

#include <stdint.h>

#if defined(__APPLE__) && defined(COD2_X64)
#ifdef __cplusplus
extern "C" {
#endif

/* The context outlives both callbacks. Cleanup must not throw; it runs once
 * after the body returns or while the body's original exception unwinds. */
void Cod2Mac_RunWithCleanup(void (*body)(void *), void (*cleanup)(void *), void *context);

/* Retail throws an i386 signed long. Preserve that exception type and the
 * signed 32-bit status value when native long is wider. */
void Cod2Mac_ThrowStatus(int32_t status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif
#endif
#endif
