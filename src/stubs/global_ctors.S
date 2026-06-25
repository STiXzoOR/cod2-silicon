/* Register C++ global constructors in .init_array */
/* These were originally in __mod_init_func in the Mach-O binary */

.section .init_array,"aw",@init_array

#if defined(__x86_64__)
.quad GLOBAL__I__ZN7COpenGL7sOpenGLE
#else
.long GLOBAL__I__ZN7COpenGL7sOpenGLE
#endif
