/* Direct3D 9 shader model 2/3 bytecode to ARB vertex/fragment program text,
 * plus the constant table the engine reads next to each program. */
#ifndef SHADERGEN_SM2ARB_H
#define SHADERGEN_SM2ARB_H

#include <stddef.h>

typedef struct {
    char *text;            /* ARB program, not NUL-terminated beyond text_size */
    size_t text_size;
    unsigned char *table;  /* constant table (.vc / .pc) */
    size_t table_size;
} Sm2ArbProgram;

/* name is the shader's base name ("lprobe_sm_a_s"); it selects the programs
 * written without the fixed-function fog tail. Returns 1 on success. On
 * failure it returns 0 and writes a short reason to error. */
int Sm2Arb_Translate(const unsigned char *code, size_t size, const char *name,
                     Sm2ArbProgram *out, char *error, size_t error_size);
void Sm2Arb_Free(Sm2ArbProgram *program);

#endif
