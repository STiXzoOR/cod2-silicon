#ifndef COD2_PORT_DEBUG_H
#define COD2_PORT_DEBUG_H

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
