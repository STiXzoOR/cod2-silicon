/* The first code of the CoD2x layer (cmake/cod2x-layer.cmake). */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
__attribute__((used, noinline)) void Cod2x_LayerStart(void)
{
    __asm__ volatile("" ::: "memory");
}
#endif
