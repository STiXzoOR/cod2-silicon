#ifndef COD2_PORT_DEBUG_H
#define COD2_PORT_DEBUG_H

#if defined(COD2_X64)
#include <stdlib.h>

/* One startup read keeps environment lookup out of renderer hot paths. Weak
 * imports let isolated production-function fixtures run with tracing off. */
#if defined(__APPLE__)
extern int cod2_native_trace_enabled __attribute__((weak_import));
#else
extern int cod2_native_trace_enabled __attribute__((weak));
#endif
void COD2_InitNativeTracing(void);
static inline int COD2_NativeTraceEnabled(void)
{
    return &cod2_native_trace_enabled && cod2_native_trace_enabled;
}

#define COD2_NATIVE_TRACE_ENABLED() COD2_NativeTraceEnabled()
#define COD2_NATIVE_TRACE_ONLY(...) do { if (COD2_NATIVE_TRACE_ENABLED()) { __VA_ARGS__ } } while (0);

#endif
/* Keep the reconstruction's probes available without charging every frame
 * and script opcode in the native port. The legacy build retains its probes. */
#ifndef COD2_PORT_DEBUG
#if defined(COD2_X64)
#define COD2_PORT_DEBUG 0
#else
#define COD2_PORT_DEBUG 1
#endif
#endif

#if COD2_PORT_DEBUG
#define COD2_DEBUG_ENV(name) getenv(name)
#define COD2_DEBUG_ONLY(...) __VA_ARGS__
#else
#define COD2_DEBUG_ENV(name) ((char *)0)
#define COD2_DEBUG_ONLY(...)
#endif

#endif
