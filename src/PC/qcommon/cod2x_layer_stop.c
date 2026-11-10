/* The end of the CoD2x layer (cmake/cod2x-layer.cmake): every cod2x unit is linked
   between Cod2x_LayerStart and Cod2x_LayerStop. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
__attribute__((used, noinline)) void Cod2x_LayerStop(void)
{
    __asm__ volatile("" ::: "memory");
}
#endif
