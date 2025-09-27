#include "../stdafx.h"

#include "cm_misc_gui.hpp"

#include "../landscape_cmd.h"
#include "../command_func.h"
#include "../company_base.h"
#include "../debug.h"
#include "../newgrf_debug.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../tile_cmd.h"
#include "../town.h"
#include "../widget_type.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_func.h"
#include "../widgets/misc_widget.h"

#include <sstream>
#include <iomanip>

namespace citymania {

static const NWidgetPart _nested_land_info_widgets[] = {
    NWidget(WWT_PANEL, COLOUR_GREY, WID_LI_BACKGROUND), SetMinimalSize(64, 32), EndContainer(),
};

static WindowDesc _land_info_desc(
    WDP_MANUAL, {}, 0, 0,
    WC_LAND_INFO, WC_NONE,
    {},
    _nested_land_info_widgets
);

class LandInfoWindow : public Window {
    enum LandInfoLines {
        LAND_INFO_CENTERED_LINES   = 32,                       ///< Up to 32 centered lines (arbitrary limit)
        LAND_INFO_MULTICENTER_LINE = LAND_INFO_CENTERED_LINES, ///< One multicenter line
        LAND_INFO_LINE_END,
    };

    static const uint LAND_INFO_LINE_BUFF_SIZE = 512;

public:
    StringList landinfo_data{}; ///< Info lines to show.
    std::string cargo_acceptance{}; ///< Centered multi-line string for cargo acceptance.
    TileIndex tile;
    TileIndex end_tile;  ///< For use in ruler(dragdrop) mode

    virtual Point OnInitialPosition(int16 sm_width, int16 sm_height, int /* window_number */) override
    {
        int scr_top = GetMainViewTop() + 2;
        int scr_bot = GetMainViewBottom() - 2;
        Point pt;
        pt.y = Clamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
        if (pt.y + sm_height > scr_bot) pt.y = std::min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
        pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);
        return pt;
    }

    void DrawWidget(const Rect &r, WidgetID widget) const override
    {
        if (widget != WID_LI_BACKGROUND) return;

        GfxDrawLine(r.left,  r.top,    r.right, r.top,    PC_BLACK);
        GfxDrawLine(r.left,  r.bottom, r.right, r.bottom, PC_BLACK);
        GfxDrawLine(r.left,  r.top,    r.left,  r.bottom, PC_BLACK);
        GfxDrawLine(r.right, r.top,    r.right, r.bottom, PC_BLACK);

        Rect ir = r.Shrink(WidgetDimensions::scaled.frametext);
        for (size_t i = 0; i < this->landinfo_data.size(); i++) {
            DrawString(ir, this->landinfo_data[i], i == 0 ? TC_LIGHT_BLUE : TC_FROMSTRING, SA_HOR_CENTER);
            ir.top += GetCharacterHeight(FS_NORMAL) + (i == 0 ? WidgetDimensions::scaled.vsep_wide : WidgetDimensions::scaled.vsep_normal);
        }

        if (!this->cargo_acceptance.empty()) {
            DrawStringMultiLine(ir, this->cargo_acceptance, TC_FROMSTRING, SA_CENTER);
        }
    }

    void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
    {
        if (widget != WID_LI_BACKGROUND) return;

        size.height = WidgetDimensions::scaled.frametext.Vertical();
        for (size_t i = 0; i < this->landinfo_data.size(); i++) {
            uint width = GetStringBoundingBox(this->landinfo_data[i]).width + WidgetDimensions::scaled.frametext.Horizontal();
            size.width = std::max(size.width, width);

            size.height += GetCharacterHeight(FS_NORMAL) + (i == 0 ? WidgetDimensions::scaled.vsep_wide : WidgetDimensions::scaled.vsep_normal);
        }

        if (!this->cargo_acceptance.empty()) {
            uint width = GetStringBoundingBox(this->cargo_acceptance).width + WidgetDimensions::scaled.frametext.Horizontal();
            size.width = std::max(size.width, std::min(static_cast<uint>(ScaleGUITrad(300)), width));
            size.height += GetStringHeight(cargo_acceptance, size.width - WidgetDimensions::scaled.frametext.Horizontal());
        }
    }

    LandInfoWindow(Tile tile, Tile end_tile) : Window(_land_info_desc), tile(tile), end_tile(end_tile)
    {
        this->InitNested();
        this->flags.Reset(WindowFlag::WhiteBorder);

#if defined(_DEBUG)
#   define LANDINFOD_LEVEL 0
#else
#   define LANDINFOD_LEVEL 1
#endif
        Debug(misc, LANDINFOD_LEVEL, "TILE: 0x{:x} ({},{})", (TileIndex)tile, TileX(tile), TileY(tile));
        Debug(misc, LANDINFOD_LEVEL, "type   = 0x{:x}", tile.type());
        Debug(misc, LANDINFOD_LEVEL, "height = 0x{:x}", tile.height());
        Debug(misc, LANDINFOD_LEVEL, "m1     = 0x{:x}", tile.m1());
        Debug(misc, LANDINFOD_LEVEL, "m2     = 0x{:x}", tile.m2());
        Debug(misc, LANDINFOD_LEVEL, "m3     = 0x{:x}", tile.m3());
        Debug(misc, LANDINFOD_LEVEL, "m4     = 0x{:x}", tile.m4());
        Debug(misc, LANDINFOD_LEVEL, "m5     = 0x{:x}", tile.m5());
        Debug(misc, LANDINFOD_LEVEL, "m6     = 0x{:x}", tile.m6());
        Debug(misc, LANDINFOD_LEVEL, "m7     = 0x{:x}", tile.m7());
        Debug(misc, LANDINFOD_LEVEL, "m8     = 0x{:x}", tile.m8());
#undef LANDINFOD_LEVEL
    }

    void OnInit() override
    {
        Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

        TileDesc td{};

        td.build_date = CalendarTime::INVALID_DATE;

        td.owner_type[0] = STR_LAND_AREA_INFORMATION_OWNER; // At least one owner is displayed, though it might be "N/A".

        CargoArray acceptance{};
        AddAcceptedCargo(tile, acceptance, nullptr);
        GetTileDesc(tile, td);

        this->landinfo_data.clear();

        /* Tiletype */
        this->landinfo_data.push_back(GetString(td.str, td.dparam));

        /* Up to four owners */
        for (uint i = 0; i < 4; i++) {
            if (td.owner_type[i] == STR_NULL) continue;

            if (td.owner[i] == OWNER_NONE || td.owner[i] == OWNER_WATER) {
                this->landinfo_data.push_back(GetString(td.owner_type[i], STR_LAND_AREA_INFORMATION_OWNER_N_A, std::monostate{}));
            } else {
                auto params = GetParamsForOwnedBy(td.owner[i], tile);
                this->landinfo_data.push_back(GetStringWithArgs(td.owner_type[i], params));
            }
        }

        /* Cost to clear/revenue when cleared */
        Company *c = Company::GetIfValid(_local_company);
        if (c != nullptr) {
            assert(_current_company == _local_company);
            CommandCost costclear = Command<CMD_LANDSCAPE_CLEAR>::Do(DoCommandFlag::QueryCost, tile);
            if (costclear.Succeeded()) {
                Money cost = costclear.GetCost();
                StringID str;
                if (cost < 0) {
                    cost = -cost; // Negate negative cost to a positive revenue
                    str = STR_LAND_AREA_INFORMATION_REVENUE_WHEN_CLEARED;
                } else {
                    str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR;
                }
                this->landinfo_data.push_back(GetString(str, cost));
            } else {
                this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A));
            }
        } else {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A));
        }

        /* Location */
        this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LANDINFO_COORDS, TileX(tile), TileY(tile), GetTileZ(tile)));

        /* Tile index */
        this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LANDINFO_INDEX, tile, tile));

        /* Local authority */
        if (t == nullptr) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE, std::monostate{}));
        } else {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, STR_TOWN_NAME, t->index));
        }

        /* Build date */
        if (td.build_date != CalendarTime::INVALID_DATE) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE, td.build_date));
        }

        /* Station class */
        if (td.station_class != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_CLASS, td.station_class));
        }

        /* Station type name */
        if (td.station_name != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_TYPE, td.station_name));
        }

        /* Airport class */
        if (td.airport_class != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_CLASS, td.airport_class));
        }

        /* Airport name */
        if (td.airport_name != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_NAME, td.airport_name));
        }

        /* Airport tile name */
        if (td.airport_tile_name != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME, td.airport_tile_name));
        }

        /* Rail type name */
        if (td.railtype != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_TYPE, td.railtype));
        }

        /* Rail speed limit */
        if (td.rail_speed != 0) {
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT, PackVelocity(td.rail_speed, VEH_TRAIN)));
        }

        /* Road type name */
        if (td.roadtype != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_TYPE, td.roadtype));
        }

        /* Road speed limit */
        if (td.road_speed != 0) {
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_SPEED_LIMIT, PackVelocity(td.road_speed, VEH_ROAD)));
        }

        /* Tram type name */
        if (td.tramtype != STR_NULL) {
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_TYPE, td.tramtype));
        }

        /* Tram speed limit */
        if (td.tram_speed != 0) {
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_SPEED_LIMIT, PackVelocity(td.tram_speed, VEH_ROAD)));
        }

        /* Tile protection status */
        if (td.town_can_upgrade.has_value()) {
            this->landinfo_data.push_back(GetString(td.town_can_upgrade.value() ? STR_LAND_AREA_INFORMATION_TOWN_CAN_UPGRADE : STR_LAND_AREA_INFORMATION_TOWN_CANNOT_UPGRADE));
        }

        /* NewGRF name */
        if (td.grf.has_value()) {
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_NEWGRF_NAME, std::move(*td.grf)));
        }

        /* CityMania code start (House pop) */
        if (td.cm_population != 0) {
            this->landinfo_data.push_back(GetString(CM_STR_LAND_AREA_INFORMATION_POP, td.cm_population));
        }
        /* CityMania code end */

        /* Cargo acceptance is displayed in a extra multiline */
        auto line = BuildCargoAcceptanceString(acceptance, STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED);
        if (line.has_value()) {
            this->cargo_acceptance = std::move(*line);
        } else {
            this->cargo_acceptance.clear();
        }
    }

    bool IsNewGRFInspectable() const override
    {
        return ::IsNewGRFInspectable(GetGrfSpecFeature(this->tile), this->tile.base());
    }

    void ShowNewGRFInspectWindow() const override
    {
        ::ShowNewGRFInspectWindow(GetGrfSpecFeature(this->tile), this->tile.base());
    }

    /**
     * Some data on this window has become invalid.
     * @param data Information about the changed data.
     * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
     */
    void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
    {
        if (!gui_scope) return;

        /* ReInit, "debug" sprite might have changed */
        if (data == 1) this->ReInit();
    }
};

/**
 * Show land information window.
 * @param tile The tile to show information about.
 */
void ShowLandInfo(TileIndex tile, TileIndex end_tile)
{
    static TileIndex last_tooltip_tile = INVALID_TILE;
    if (tile == last_tooltip_tile) return;
    last_tooltip_tile = tile;

    CloseWindowById(WC_LAND_INFO, 0);
    if (tile == INVALID_TILE) return;
    new LandInfoWindow(tile, end_tile);
}

} // namespace citymania
