#ifndef CM_TOOLTIPS_HPP
#define CM_TOOLTIPS_HPP

#include "../cargotype.h"
#include "../station_type.h"
#include "../tile_type.h"
#include "../window_type.h"
#include "../window_gui.h"

class LandInfoWindow;

namespace citymania {

void ShowLandTooltips(TileIndex tile, Window *parent);
bool ShowStationRatingTooltip(Window *parent, const Station *st, const CargoSpec *cs, TooltipCloseCondition close_cond);

} // namespace citymania

#endif
