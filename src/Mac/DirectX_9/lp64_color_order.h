#if defined(COD2_X64)
/* Cached static vertices have already passed through StdConverterARGB into
 * BGRA at offset 24. Skeletal vertices keep their ARGB color at offset 12. */
static int MacShader_ColorByteOrder(UINT stride, int colorOffset, UINT positionComponents)
{
    if (stride == 0x44 || stride == 0x20 || stride == 0x18)
        return COLOR_BYTES_RGBA;
    if (stride == 0x40 && positionComponents == 3 && colorOffset != 0x18)
        return COLOR_BYTES_ARGB;
    return COLOR_BYTES_BGRA;
}
#endif
