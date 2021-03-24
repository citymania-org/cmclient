#ifndef CM_MISC_GUI_HPP
#define CM_MISC_GUI_HPP

#include "../tile_type.h"
#include "../window_type.h"

namespace citymania {

void ShowLandInfo(TileIndex tile, TileIndex end_tile = INVALID_TILE);
void HandleViewportScroll(Window *w);
void EndViewportScroll(Window *w);

} // namespace citymania

#endif
