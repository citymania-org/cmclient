#include "../stdafx.h"

#include "cm_misc_gui.hpp"

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

namespace citymania {

static const NWidgetPart _nested_land_info_widgets[] = {
    NWidget(WWT_PANEL, COLOUR_GREY, WID_LI_BACKGROUND), SetMinimalSize(64, 32), EndContainer(),
};

static WindowDesc _land_info_desc(
    WDP_MANUAL, "land_info", 0, 0,
    WC_LAND_INFO, WC_NONE,
    0,
    _nested_land_info_widgets, lengthof(_nested_land_info_widgets)
);

class LandInfoWindow : public Window {
    enum LandInfoLines {
        LAND_INFO_CENTERED_LINES   = 32,                       ///< Up to 32 centered lines (arbitrary limit)
        LAND_INFO_MULTICENTER_LINE = LAND_INFO_CENTERED_LINES, ///< One multicenter line
        LAND_INFO_LINE_END,
    };

    static const uint LAND_INFO_LINE_BUFF_SIZE = 512;

public:
    char landinfo_data[LAND_INFO_LINE_END][LAND_INFO_LINE_BUFF_SIZE];
    TileIndex tile;
    TileIndex end_tile;  ///< For use in ruler(dragdrop) mode

    virtual Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number)
    {
        int scr_top = GetMainViewTop() + 2;
        int scr_bot = GetMainViewBottom() - 2;
        Point pt;
        pt.y = Clamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
        if (pt.y + sm_height > scr_bot) pt.y = min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
        pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);
        return pt;
    }

    void DrawWidget(const Rect &r, int widget) const override
    {
        if (widget != WID_LI_BACKGROUND) return;

        GfxDrawLine(r.left,  r.top,    r.right, r.top,    PC_BLACK);
        GfxDrawLine(r.left,  r.bottom, r.right, r.bottom, PC_BLACK);
        GfxDrawLine(r.left,  r.top,    r.left,  r.bottom, PC_BLACK);
        GfxDrawLine(r.right, r.top,    r.right, r.bottom, PC_BLACK);

        uint y = r.top + WD_TEXTPANEL_TOP;
        for (uint i = 0; i < LAND_INFO_CENTERED_LINES; i++) {
            if (StrEmpty(this->landinfo_data[i])) break;

            DrawString(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, this->landinfo_data[i], i == 0 ? TC_LIGHT_BLUE : TC_FROMSTRING, SA_HOR_CENTER);
            y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
            if (i == 0) y += 4;
        }

        if (!StrEmpty(this->landinfo_data[LAND_INFO_MULTICENTER_LINE])) {
            SetDParamStr(0, this->landinfo_data[LAND_INFO_MULTICENTER_LINE]);
            DrawStringMultiLine(r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT, y, r.bottom - WD_TEXTPANEL_BOTTOM, STR_JUST_RAW_STRING, TC_FROMSTRING, SA_CENTER);
        }
    }

    void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
    {
        if (widget != WID_LI_BACKGROUND) return;

        size->height = WD_TEXTPANEL_TOP + WD_TEXTPANEL_BOTTOM;
        for (uint i = 0; i < LAND_INFO_CENTERED_LINES; i++) {
            if (StrEmpty(this->landinfo_data[i])) break;

            uint width = GetStringBoundingBox(this->landinfo_data[i]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
            size->width = max(size->width, width);

            size->height += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
            if (i == 0) size->height += 4;
        }

        if (!StrEmpty(this->landinfo_data[LAND_INFO_MULTICENTER_LINE])) {
            uint width = GetStringBoundingBox(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT;
            size->width = max(size->width, min<uint>(ScaleGUITrad(300), width));
            SetDParamStr(0, this->landinfo_data[LAND_INFO_MULTICENTER_LINE]);
            size->height += GetStringHeight(STR_JUST_RAW_STRING, size->width - WD_FRAMETEXT_LEFT - WD_FRAMETEXT_RIGHT);
        }
    }

    LandInfoWindow(TileIndex tile, TileIndex end_tile=INVALID_TILE) :
        Window(&_land_info_desc), tile(tile), end_tile(end_tile)
    {
        this->InitNested();
        CLRBITS(this->flags, WF_WHITE_BORDER);

#if defined(_DEBUG)
#   define LANDINFOD_LEVEL 0
#else
#   define LANDINFOD_LEVEL 1
#endif
        DEBUG(misc, LANDINFOD_LEVEL, "TILE: %#x (%i,%i)", tile, TileX(tile), TileY(tile));
        DEBUG(misc, LANDINFOD_LEVEL, "type   = %#x", _m[tile].type);
        DEBUG(misc, LANDINFOD_LEVEL, "height = %#x", _m[tile].height);
        DEBUG(misc, LANDINFOD_LEVEL, "m1     = %#x", _m[tile].m1);
        DEBUG(misc, LANDINFOD_LEVEL, "m2     = %#x", _m[tile].m2);
        DEBUG(misc, LANDINFOD_LEVEL, "m3     = %#x", _m[tile].m3);
        DEBUG(misc, LANDINFOD_LEVEL, "m4     = %#x", _m[tile].m4);
        DEBUG(misc, LANDINFOD_LEVEL, "m5     = %#x", _m[tile].m5);
        DEBUG(misc, LANDINFOD_LEVEL, "m6     = %#x", _me[tile].m6);
        DEBUG(misc, LANDINFOD_LEVEL, "m7     = %#x", _me[tile].m7);
        DEBUG(misc, LANDINFOD_LEVEL, "m8     = %#x", _me[tile].m8);
#undef LANDINFOD_LEVEL
    }

    void OnInit() override
    {
        Town *t = ClosestTownFromTile(tile, _settings_game.economy.dist_local_authority);

        /* Because build_date is not set yet in every TileDesc, we make sure it is empty */
        TileDesc td;

        td.build_date = INVALID_DATE;

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
        td.population = 0;

        td.grf = nullptr;

        CargoArray acceptance;
        AddAcceptedCargo(tile, acceptance, nullptr);
        GetTileDesc(tile, &td);

        uint line_nr = 0;

        /* Tiletype */
        SetDParam(0, td.dparam[0]);
        GetString(this->landinfo_data[line_nr], td.str, lastof(this->landinfo_data[line_nr]));
        line_nr++;

        /* Up to four owners */
        for (uint i = 0; i < 4; i++) {
            if (td.owner_type[i] == STR_NULL) continue;

            SetDParam(0, STR_LAND_AREA_INFORMATION_OWNER_N_A);
            if (td.owner[i] != OWNER_NONE && td.owner[i] != OWNER_WATER) GetNameOfOwner(td.owner[i], tile);
            GetString(this->landinfo_data[line_nr], td.owner_type[i], lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Cost to clear/revenue when cleared */
        StringID str = STR_LAND_AREA_INFORMATION_COST_TO_CLEAR_N_A;
        Company *c = Company::GetIfValid(_local_company);
        if (c != nullptr) {
            assert(_current_company == _local_company);
            CommandCost costclear = DoCommand(tile, 0, 0, DC_QUERY_COST, CMD_LANDSCAPE_CLEAR);
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
        GetString(this->landinfo_data[line_nr], str, lastof(this->landinfo_data[line_nr]));
        line_nr++;

        /* Location */
        char tmp[16];
        seprintf(tmp, lastof(tmp), "0x%.4X", tile);
        SetDParam(0, TileX(tile));
        SetDParam(1, TileY(tile));
        SetDParam(2, GetTileZ(tile));
        SetDParamStr(3, tmp);
        GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_LANDINFO_COORDS, lastof(this->landinfo_data[line_nr]));
        line_nr++;

        /* Local authority */
        SetDParam(0, STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY_NONE);
        if (t != nullptr) {
            SetDParam(0, STR_TOWN_NAME);
            SetDParam(1, t->index);
        }
        GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_LOCAL_AUTHORITY, lastof(this->landinfo_data[line_nr]));
        line_nr++;

        /* Build date */
        if (td.build_date != INVALID_DATE) {
            SetDParam(0, td.build_date);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_BUILD_DATE, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Station class */
        if (td.station_class != STR_NULL) {
            SetDParam(0, td.station_class);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_STATION_CLASS, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Station type name */
        if (td.station_name != STR_NULL) {
            SetDParam(0, td.station_name);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_STATION_TYPE, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Airport class */
        if (td.airport_class != STR_NULL) {
            SetDParam(0, td.airport_class);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORT_CLASS, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Airport name */
        if (td.airport_name != STR_NULL) {
            SetDParam(0, td.airport_name);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORT_NAME, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Airport tile name */
        if (td.airport_tile_name != STR_NULL) {
            SetDParam(0, td.airport_tile_name);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_AIRPORTTILE_NAME, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Rail type name */
        if (td.railtype != STR_NULL) {
            SetDParam(0, td.railtype);
            GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_RAIL_TYPE, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Rail speed limit */
        if (td.rail_speed != 0) {
            SetDParam(0, td.rail_speed);
            GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_RAIL_SPEED_LIMIT, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Road type name */
        if (td.roadtype != STR_NULL) {
            SetDParam(0, td.roadtype);
            GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_ROAD_TYPE, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Road speed limit */
        if (td.road_speed != 0) {
            SetDParam(0, td.road_speed);
            GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_ROAD_SPEED_LIMIT, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Tram type name */
        if (td.tramtype != STR_NULL) {
            SetDParam(0, td.tramtype);
            GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_TRAM_TYPE, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* Tram speed limit */
        if (td.tram_speed != 0) {
            SetDParam(0, td.tram_speed);
            GetString(this->landinfo_data[line_nr], STR_LANG_AREA_INFORMATION_TRAM_SPEED_LIMIT, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* NewGRF name */
        if (td.grf != nullptr) {
            SetDParamStr(0, td.grf);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_NEWGRF_NAME, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        /* House pop */
        if (td.population != 0) {
            SetDParam(0, td.population);
            GetString(this->landinfo_data[line_nr], STR_LAND_AREA_INFORMATION_POP, lastof(this->landinfo_data[line_nr]));
            line_nr++;
        }

        assert(line_nr < LAND_INFO_CENTERED_LINES);

        /* Mark last line empty */
        this->landinfo_data[line_nr][0] = '\0';

        /* Cargo acceptance is displayed in a extra multiline */
        char *strp = GetString(this->landinfo_data[LAND_INFO_MULTICENTER_LINE], STR_LAND_AREA_INFORMATION_CARGO_ACCEPTED, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
        bool found = false;

        for (CargoID i = 0; i < NUM_CARGO; ++i) {
            if (acceptance[i] > 0) {
                /* Add a comma between each item. */
                if (found) strp = strecpy(strp, ", ", lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
                found = true;

                /* If the accepted value is less than 8, show it in 1/8:ths */
                if (acceptance[i] < 8) {
                    SetDParam(0, acceptance[i]);
                    SetDParam(1, CargoSpec::Get(i)->name);
                    strp = GetString(strp, STR_LAND_AREA_INFORMATION_CARGO_EIGHTS, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
                } else {
                    strp = GetString(strp, CargoSpec::Get(i)->name, lastof(this->landinfo_data[LAND_INFO_MULTICENTER_LINE]));
                }
            }
        }
        if (!found) this->landinfo_data[LAND_INFO_MULTICENTER_LINE][0] = '\0';
    }

    bool IsNewGRFInspectable() const override
    {
        return ::IsNewGRFInspectable(GetGrfSpecFeature(this->tile), this->tile);
    }

    void ShowNewGRFInspectWindow() const override
    {
        ::ShowNewGRFInspectWindow(GetGrfSpecFeature(this->tile), this->tile);
    }

    /**
     * Some data on this window has become invalid.
     * @param data Information about the changed data.
     * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
     */
    void OnInvalidateData(int data = 0, bool gui_scope = true) override
    {
        if (!gui_scope) return;
        switch (data) {
            case 1:
                /* ReInit, "debug" sprite might have changed */
                this->ReInit();
                break;
        }
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

    DeleteWindowById(WC_LAND_INFO, 0);
    if (tile == INVALID_TILE) return;
    new LandInfoWindow(tile, end_tile);
}

} // namespace citymania
