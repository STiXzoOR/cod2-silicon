#ifndef COD2_ARB_RASTER_H
#define COD2_ARB_RASTER_H
#include <cstddef>
#include <cstdint>
namespace cod2::arb {

/* Provider-local raster payload for ABI 1 buffer 0. D3DDECLTYPE values are
 * retained. Stream buffers 2..17 bind their allocation starts; byte offsets,
 * strides and lengths belong to this immutable per-draw descriptor. */
enum class ColorOrder : uint32_t { RGBA, BGRA, ARGB };
struct RasterAttribute {
    uint32_t stream = 16, offset = 0, type = 17;
    ColorOrder colorOrder = ColorOrder::RGBA;
};
struct RasterStream {
    uint64_t offset = 0, length = 0;
    uint32_t stride = 0, reserved = 0;
};
struct RasterDescriptor {
    RasterAttribute attributes[16];
    RasterStream streams[16];
    float framebufferHeight = 0;
    uint32_t reserved[3] = {};
    /* Bias is applied in MSL; native sampler lodBias must remain zero. */
    float samplerLODBias[16] = {};
    /* D3DCMPFUNC 1..8, post-shader alpha, ALPHAREF normalized from 0..255. */
    uint32_t alphaEnabled = 0, alphaFunction = 8, alphaReference = 0, reservedAlpha = 0;
};
static_assert(sizeof(RasterAttribute) == 16 && sizeof(RasterStream) == 24 &&
              offsetof(RasterDescriptor, framebufferHeight) == 640 &&
              offsetof(RasterDescriptor, samplerLODBias) == 656 &&
              offsetof(RasterDescriptor, alphaEnabled) == 720 &&
              sizeof(RasterDescriptor) == 736, "Raster descriptor layout changed");


} // namespace cod2::arb
#endif
