#ifndef CM_COLOUR_HPP
#define CM_COLOUR_HPP

#include <cstdint>
#include "../blitter/32bpp_base.hpp"

namespace citymania {

extern const uint8_t RGB_TO_M[];

static inline uint8_t GetMForRGB(uint8_t r, uint8_t g, uint8_t b) {
	return RGB_TO_M[((uint)(r & 0xf0) >> 4) | ((uint)g & 0xf0) | ((uint)(b & 0xf0) << 4)];
}

static inline Colour Remap32RGB(uint r, uint g, uint b, const uint8_t *remap)
{
	return _cur_palette.palette[remap[GetMForRGB(r, g, b)]];
}

static inline Colour Remap32RGBANoCheck(uint r, uint g, uint b, uint a, Colour current, const uint8_t *remap)
{
	return Blitter_32bppBase::ComposeColourPANoCheck(Remap32RGB(r, g, b, remap), a, current);
}

static inline Colour Remap32RGBA(uint r, uint g, uint b, uint a, Colour current, const uint8_t *remap)
{
	if (a == 255) return Remap32RGB(r, g, b, remap);
	return Remap32RGBANoCheck(r, g, b, a, current, remap);
}

} // namespace citymania

#endif
