#ifndef CM_COLOUR_HPP
#define CM_COLOUR_HPP

#include "../blitter/32bpp_base.hpp"
#include "../palette_func.h"  // PC_WHITE, PC_BLACK
#include <cstdint>

namespace citymania {

extern const uint8_t RGB_TO_M[];

// ', '.join(["true" if grf.srgb_color_distance(c, (255, 255, 255)) > grf.srgb_color_distance(c, (0, 0, 0)) else "false" for c in grf.PALETTE])
constexpr std::array<bool, 256> IS_M_CLOSER_TO_BLACK = { true, true, true, true, true, true, true, true, false, false, false, false, false, false, false, false, true, true, true, true, false, false, false, false, true, true, true, true, true, false, false, false, true, true, true, true, true, false, false, false, true, true, true, true, true, true, false, false, false, false, false, false, false, true, true, true, true, true, false, false, true, true, true, true, true, false, false, false, false, false, true, true, true, true, true, true, true, true, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, true, true, true, true, true, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, true, true, true, true, false, true, true, true, true, true, false, false, false, true, true, true, true, false, false, false, false, true, true, true, true, true, true, false, false, false, false, true, true, true, true, false, false, false, false, true, true, true, false, false, false, false, false, true, true, true, false, false, false, false, false, true, true, true, true, true, true, true, true, true, false, false, false, false, false, true, false, false, false, false, false, true, true, true, true, true, true, false, false, true, true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, true, true, true, true, false, true, true, true, false, false, false, false, true, true, true, true, true, false, true, true, true, true, true, true, false, false, false, true, false };

constexpr std::array<uint8_t, 256> BLINK_COLOUR = { 141, 7, 9, 10, 11, 12, 13, 14, 2, 2, 3, 4, 5, 6, 6, 8, 22, 22, 135, 210, 2, 3, 16, 18, 29, 30, 59, 31, 69, 24, 25, 26, 37, 38, 38, 39, 104, 32, 33, 35, 77, 47, 166, 166, 49, 49, 41, 41, 42, 125, 55, 85, 86, 37, 120, 65, 69, 104, 24, 25, 78, 120, 121, 197, 60, 54, 55, 27, 85, 28, 126, 78, 79, 79, 167, 167, 167, 178, 71, 72, 94, 94, 95, 95, 95, 209, 80, 81, 101, 102, 31, 103, 69, 80, 81, 82, 101, 102, 103, 103, 69, 96, 97, 98, 36, 111, 37, 120, 121, 121, 39, 105, 79, 120, 167, 167, 167, 178, 122, 122, 61, 108, 79, 79, 167, 167, 168, 71, 133, 134, 134, 177, 177, 128, 16, 18, 142, 142, 143, 143, 143, 136, 136, 171, 150, 150, 151, 151, 152, 152, 144, 200, 154, 156, 213, 213, 212, 153, 128, 154, 155, 157, 166, 166, 180, 180, 180, 124, 35, 7, 174, 143, 176, 176, 170, 130, 131, 132, 77, 166, 166, 166, 165, 165, 180, 180, 180, 180, 62, 63, 27, 65, 53, 61, 62, 62, 55, 55, 204, 151, 151, 152, 152, 152, 198, 154, 80, 81, 83, 84, 19, 157, 156, 154, 154, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 78, 79, 167, 168, 72, 180, 180, 180, 180, 62, 62, 63, 77, 181, 29, 30, 95, 86, 152, 152, 152, 152, 152, 153, 128, 19, 161, 153, 8 };

static inline uint8_t GetBlinkColour(uint8_t m) {
	return BLINK_COLOUR[m];
}

static inline uint8_t GetMForRGB(uint8_t r, uint8_t g, uint8_t b) {
	return RGB_TO_M[((uint)(r & 0xf0) >> 4) | ((uint)g & 0xf0) | ((uint)(b & 0xf0) << 4)];
}

static inline Colour Remap32RGB(uint r, uint g, uint b, const byte *remap)
{
	return _cur_palette.palette[remap[GetMForRGB(r, g, b)]];
}

static inline Colour Remap32RGBANoCheck(uint r, uint g, uint b, uint a, Colour current, const byte *remap)
{
	return Blitter_32bppBase::ComposeColourPANoCheck(Remap32RGB(r, g, b, remap), a, current);
}

static inline Colour Remap32RGBA(uint r, uint g, uint b, uint a, Colour current, const byte *remap)
{
	if (a == 255) return Remap32RGB(r, g, b, remap);
	return Remap32RGBANoCheck(r, g, b, a, current, remap);
}

} // namespace citymania

#endif
