/* Shared helpers for the network-parser fuzz harnesses. */
#ifndef COD2_FUZZ_H
#define COD2_FUZZ_H

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Engine dvars as harness-owned storage: FUZZ_DVAR(sv_voice) defines both
 * `const dvar_t *sv_voice` and the writable `sv_voice_value`. */
#define FUZZ_DVAR(name)          \
    static dvar_t name##_value;  \
    const dvar_t *name = &name##_value

static inline void FuzzDvarInt(dvar_t *dvar, const char *name, int value)
{
    dvar->name = name;
    dvar->type = 5;
    dvar->current.integer = value;
    dvar->latched.integer = value;
    dvar->reset.integer = value;
}

static inline void FuzzDvarBool(dvar_t *dvar, const char *name, int value)
{
    dvar->name = name;
    dvar->type = 0;
    dvar->current.integer = value != 0;
    dvar->latched.integer = value != 0;
    dvar->reset.integer = value != 0;
}

static inline void FuzzDvarString(dvar_t *dvar, const char *name, const char *value)
{
    dvar->name = name;
    dvar->type = 7;
    dvar->current.string = value;
    dvar->latched.string = value;
    dvar->reset.string = value;
}

/* The engine's bit masks (src/blobs/data.c). Index 13 is 16383 in the retail
 * table as well; usercmd keys only use small widths. */
#define FUZZ_KBITMASK                                                                   \
    unsigned int kbitmask[33] = { 0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095,   \
        16383, 32767, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, \
        16777215, 33554431, 67108863, 134217727, 268435455, 536870911, 1073741823,       \
        2147483647, 4294967295u, 0 }

/* Structured input reader. Reads past the end return zeros. */
typedef struct {
    const uint8_t *p;
    const uint8_t *end;
} FuzzReader;

static inline size_t FuzzLeft(const FuzzReader *r)
{
    return (size_t)(r->end - r->p);
}

static inline uint8_t FuzzU8(FuzzReader *r)
{
    return r->p < r->end ? *r->p++ : 0;
}

static inline uint16_t FuzzU16(FuzzReader *r)
{
    uint16_t lo = FuzzU8(r);
    return (uint16_t)(lo | (FuzzU8(r) << 8));
}

static inline uint32_t FuzzU32(FuzzReader *r)
{
    uint32_t lo = FuzzU16(r);
    return lo | ((uint32_t)FuzzU16(r) << 16);
}

/* Takes up to `want` bytes; returns how many were available. */
static inline size_t FuzzBytes(FuzzReader *r, const uint8_t **out, size_t want)
{
    size_t n = FuzzLeft(r) < want ? FuzzLeft(r) : want;
    *out = r->p;
    r->p += n;
    return n;
}

/* Com_Error policy. ERR_DROP/ERR_DISCONNECT unwind to the harness when it has
 * set fuzz_error_armed; otherwise every engine error is a finding: a remote
 * peer must not be able to stop a server (or crash a client) that way. */
static jmp_buf fuzz_error_jump;
static int fuzz_error_armed;
static int fuzz_error_allow_drop;

static inline void FuzzEngineError(int code, const char *message)
{
    if (fuzz_error_armed && fuzz_error_allow_drop && code != 0)
        longjmp(fuzz_error_jump, 1);
    fprintf(stderr, "==fuzz== engine error %d reached from network input: %s\n", code, message);
    abort();
}

#endif
