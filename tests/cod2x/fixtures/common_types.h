#ifndef COD2X_TEST_DVARS_H
#define COD2X_TEST_DVARS_H

/* Minimal dvar API fixture for policy tests. Production declarations are
   checked separately against the engine's headers; no engine headers change. */
typedef unsigned char byte;
typedef union {
    int integer;
    unsigned char enabled;
    const char *string;
} DvarValue;
typedef union {
    struct { int min, max; } integer;
} DvarLimits;
typedef struct {
    const char *name;
    unsigned short flags;
    byte type;
    DvarValue current;
    DvarLimits domain;
} dvar_t;

#endif
