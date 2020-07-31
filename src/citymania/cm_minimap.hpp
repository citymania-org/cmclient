#ifndef CM_MINIMAP_HPP
#define CM_MINIMAP_HPP

#include "../core/endian_func.hpp"

namespace citymania {

#define MKCOLOUR(x) TO_LE32X(x)
#define MKCOLOURGROUP(x, y) \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(x) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(y) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(x) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(y) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x))


static const  uint32 _yellow_map_heights[] = {
    MKCOLOURGROUP(0x22, 0x23),
    MKCOLOURGROUP(0x23, 0x24),
    MKCOLOURGROUP(0x24, 0x25),
    MKCOLOURGROUP(0x25, 0x26),
    MKCOLOURGROUP(0x27, 0x27),
    MKCOLOUR(0x27272727),
};

static const  uint32 _orange_map_heights[] = {
    MKCOLOURGROUP(0xC0, 0xC1),
    MKCOLOURGROUP(0xC1, 0xC2),
    MKCOLOURGROUP(0xC2, 0xC3),
    MKCOLOURGROUP(0xC3, 0xC4),
    MKCOLOURGROUP(0xC4, 0xC5),
    MKCOLOUR(0xC5C5C5C5),
};

} // namespace citymania

#endif  /* CITYMANIA_MINIMAP_HPP */
