/* Absolute symbols the reconstruction expects from the i386 Mach-O layout.
 *
 * Decompiled data sometimes encodes plain integers as offsets from the image
 * header (for example sample rates in g_encoder_samplerate and fixed_td). The
 * i386 executable's header sat at 0x1000, and upstream's MinGW build pins it
 * with --defsym ___mh_execute_header=0x1000. ld64 has no --defsym, so define
 * the same absolute symbol in assembly. This is not the real Mach-O header
 * (__mh_execute_header, one underscore fewer), which the linker still provides. */
__asm__(".globl ___mh_execute_header\n"
        "___mh_execute_header = 0x1000\n");
