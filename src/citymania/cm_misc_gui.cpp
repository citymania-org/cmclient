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
    WDP_MANUAL, nullptr, 0, 0,
    WC_LAND_INFO, WC_NONE,
    0,
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
    StringList  landinfo_data;    ///< Info lines to show.
    std::string cargo_acceptance; ///< Centered multi-line string for cargo acceptance.
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
            SetDParamStr(0, this->cargo_acceptance);
            DrawStringMultiLine(ir, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
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
            SetDParamStr(0, cargo_acceptance);
            size.height += GetStringHeight(STR_JUST_RAW_STRING, size.width - WidgetDimensions::scaled.frametext.Horizontal());
        }
    }

    LandInfoWindow(Tile tile, Tile end_tile) : Window(_land_info_desc), tile(tile), end_tile(end_tile)
    {
        this->InitNested();
        CLRBITS(this->flags, WF_WHITE_BORDER);

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

        /* Because build_date is not set yet in every TileDesc, we make sure it is empty */
        TileDesc td;

        td.build_date = CalendarTime::INVALID_DATE;

        /* Most tiles have only one owner, but
         *  - drivethrough roadstops can be build on town owned roads (up to 2 owners) and
         *  - roads can have up to four owners (railroad, road, tram, 3rd-roadtype "highway").
         */
        td.owner_type[0] = STR_LAND_AREA_INFORMATION_OWNER; // At least one owner is displayed, though it might be "N/A".
        td.owner_type[1] = STR_NULL;       // STR_NULL results in skipping the owner
        td.owner_type[2] = STR_NULL;
        td.owner_type[3] = STR_NULL;
        td.owner[0] = OWNER_NONE;
        td.owner[1] = OWNER_NONE;
        td.owner[2] = OWNER_NONE;
        td.owner[3] = OWNER_NONE;

        td.station_class = STR_NULL;
        td.station_name = STR_NULL;
        td.airport_class = STR_NULL;
        td.airport_name = STR_NULL;
        td.airport_tile_name = STR_NULL;
        td.railtype = STR_NULL;
        td.rail_speed = 0;
        td.roadtype = STR_NULL;
        td.road_speed = 0;
        td.tramtype = STR_NULL;
        td.tram_speed = 0;
        td.cm_population = 0;

        td.grf = nullptr;

        CargoArray acceptance{};
        AddAcceptedCargo(tile, acceptance, nullptr);
        GetTileDesc(tile, &td);

        this->landinfo_data.clear();

        /* Tiletype */
        SetDParam(0, td.dparam);
        this->landinfo_data.push_back(GetString(td.str));

        /* Up to four owners */
        for (uint i = 0; i < 4; i++) {
            if (td.owner_type[i] == STR_NULL) continue;

            SetDParam(0, STR_LAND_AREA_INFORMATION_OWNER_N_A);
            if (td.owner[i] != OWNER_NONE && td.owner[i] != OWNER_WATER) SetDParamsForOwnedBy(td.owner[i], tile);
            this->landinfo_data.push_back(GetString(td.owner_type[i]));
        }

        /* Cost to clear/revenue when cleared */
        StringID str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A;
        Company *c = Company::GetIfValid(_local_company);
        if (c != nullptr) {
            assert(_current_company == _local_company);
            CommandCost costclear = Command<CMD_LANDSCAPE_CLEAR>::Do(DC_QUERY_COST, tile);
            if (costclear.Succeeded()) {
                Money cost = costclear.GetCost();
                if (cost < 0) {
                    cost = -cost; // Negate negative cost to a positive revenue
                    str = STR_LAND_AREA_INFORMATION_REVENUE_WHEN_CLEARED;
                } else {
                    str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR;
                }
                SetDParam(0, cost);
            }
        }
        this->landinfo_data.push_back(GetString(str));

        /* Location */
        std::stringstream tile_ss;
        tile_ss << "0x" << std::setfill('0') << std::setw(4) << std::hex << std::uppercase << tile.base(); // 0x%.4X

        SetDParam(0, TileX(tile));
        SetDParam(1, TileY(tile));
        SetDParam(2, GetTileZ(tile));
        SetDParamStr(3, tile_ss.str());
        this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LANDINFO_COORDS));

        /* Local authority */
        SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
        if (t != nullptr) {
            SetDParam(0, STR_TOWN_NAME);
            SetDParam(1, t->index);
        }
        this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY));

        /* Build date */
        if (td.build_date != CalendarTime::INVALID_DATE) {
            SetDParam(0, td.build_date);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_BUILD_DATE));
        }

        /* Station class */
        if (td.station_class != STR_NULL) {
            SetDParam(0, td.station_class);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_CLASS));
        }

        /* Station type name */
        if (td.station_name != STR_NULL) {
            SetDParam(0, td.station_name);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_STATION_TYPE));
        }

        /* Airport class */
        if (td.airport_class != STR_NULL) {
            SetDParam(0, td.airport_class);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_CLASS));
        }

        /* Airport name */
        if (td.airport_name != STR_NULL) {
            SetDParam(0, td.airport_name);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORT_NAME));
        }

        /* Airport tile name */
        if (td.airport_tile_name != STR_NULL) {
            SetDParam(0, td.airport_tile_name);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME));
        }

        /* Rail type name */
        if (td.railtype != STR_NULL) {
            SetDParam(0, td.railtype);
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_TYPE));
        }

        /* Rail speed limit */
        if (td.rail_speed != 0) {
            SetDParam(0, PackVelocity(td.rail_speed, VEH_TRAIN));
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT));
        }

        /* Road type name */
        if (td.roadtype != STR_NULL) {
            SetDParam(0, td.roadtype);
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_TYPE));
        }

        /* Road speed limit */
        if (td.road_speed != 0) {
            SetDParam(0, PackVelocity(td.road_speed, VEH_ROAD));
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_ROAD_SPEED_LIMIT));
        }

        /* Tram type name */
        if (td.tramtype != STR_NULL) {
            SetDParam(0, td.tramtype);
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_TYPE));
        }

        /* Tram speed limit */
        if (td.tram_speed != 0) {
            SetDParam(0, PackVelocity(td.tram_speed, VEH_ROAD));
            this->landinfo_data.push_back(GetString(STR_LANG_AREA_INFORMATION_TRAM_SPEED_LIMIT));
        }

        /* NewGRF name */
        if (td.grf != nullptr) {
            SetDParamStr(0, td.grf);
            this->landinfo_data.push_back(GetString(STR_LAND_AREA_INFORMATION_NEWGRF_NAME));
        }

        /* CityMania code start (House pop) */
        if (td.cm_population != 0) {
            SetDParam(0, td.cm_population);
            this->landinfo_data.push_back(GetString(CM_STR_LAND_AREA_INFORMATION_POP));
        }
        /* CityMania code end */

        /* Cargo acceptance is displayed in a extra multiline */
        std::stringstream line;
        line << GetString(STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED);

        bool found = false;
        for (const CargoSpec *cs : _sorted_cargo_specs) {
            CargoID cid = cs->Index();
            if (acceptance[cid] > 0) {
                /* Add a comma between each item. */
                if (found) line << ", ";
                found = true;

                /* If the accepted value is less than 8, show it in 1/8:ths */
                if (acceptance[cid] < 8) {
                    SetDParam(0, acceptance[cid]);
                    SetDParam(1, cs->name);
                    line << GetString(STR_LAND_AREA_INFORMATION_CARGO_EIGHTS);
                } else {
                    line << GetString(cs->name);
                }
            }
        }
        if (found) {
            this->cargo_acceptance = line.str();
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
