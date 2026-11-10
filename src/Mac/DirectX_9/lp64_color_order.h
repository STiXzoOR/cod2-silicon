#if defined(COD2_X64)
/* Cached static vertices have already passed through StdConverterARGB into
 * BGRA: at offset 24 of the 64-byte Direct3D 9 vertex, and at offset 12 of
 * the 24-byte Direct3D 7 one. Skeletal vertices keep their ARGB color at
 * offset 12.
 *
 * That holds for the Direct3D 7 vertex only. Model files store colors blue
 * first. R_SkinStaticModelCachedCmd rotates them to alpha first before the
 * converter reverses the four bytes, so the 64-byte vertex ends up red
 * first; the Direct3D 7 branch writes alpha, red, green, blue and ends up
 * blue first. */
static int MacShader_ColorByteOrder(UINT stride, int colorOffset, UINT positionComponents)
{
#if COD2_APPLE_SDK
    if (stride == 0x44 || stride == 0x20)
#else
    if (stride == 0x44 || stride == 0x20 || stride == 0x18)
#endif
        return COLOR_BYTES_RGBA;
#if COD2_APPLE_SDK
    if (stride == 0x40 && colorOffset == 0x18)
        return COLOR_BYTES_RGBA;
#endif
    if (stride == 0x40 && positionComponents == 3 && colorOffset != 0x18)
        return COLOR_BYTES_ARGB;
    return COLOR_BYTES_BGRA;
}
#endif
