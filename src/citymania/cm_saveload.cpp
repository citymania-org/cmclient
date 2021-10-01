#include "../stdafx.h"

#include "../company_base.h"
#include "../date_func.h"
#include "../debug.h"
#include "../saveload/compat/storage_sl_compat.h"
#include "../saveload/saveload.h"
#include "../town.h"

#include "cm_bitstream.hpp"
#include "cm_saveload.hpp"
#include "cm_settings.hpp"
#include "cm_game.hpp"
#include "cm_main.hpp"

namespace citymania {

void SlTownGrowthTiles::Save(Town *t) const {
    SlSetStructListLength(t->cm.growth_tiles.size());
    for (auto &p : t->cm.growth_tiles) SlObject(&p, this->GetDescription());
}

void SlTownGrowthTiles::Load(Town *t) const {
    citymania::TownsGrowthTilesIndex::value_type tmp;
    size_t length = SlGetStructListLength(10000);
    for (size_t i = 0; i < length; i++) {
        SlObject(&tmp, this->GetLoadDescription());
        t->cm.growth_tiles.insert(tmp);
    }
}

void SlTownGrowthTilesLastMonth::Save(Town *t) const {
    SlSetStructListLength(t->cm.growth_tiles_last_month.size());
    for (auto &p : t->cm.growth_tiles_last_month) SlObject(&p, this->GetDescription());
}

void SlTownGrowthTilesLastMonth::Load(Town *t) const {
    citymania::TownsGrowthTilesIndex::value_type tmp;
    size_t length = SlGetStructListLength(10000);
    for (size_t i = 0; i < length; i++) {
        SlObject(&tmp, this->GetLoadDescription());
        t->cm.growth_tiles_last_month.insert(tmp);
    }
}

} // namespace citymania
