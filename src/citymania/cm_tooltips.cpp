#include "../stdafx.h"

#include "cm_tooltips.hpp"

#include "../company_base.h"
#include "../house.h"
#include "../industry.h"
#include "../newgrf_callbacks.h"
#include "../newgrf_cargo.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../tile_type.h"
#include "../town_map.h"
#include "../town.h"
#include "../window_func.h"
#include "../zoom_func.h"

#include "../safeguards.h"

namespace citymania {

static const NWidgetPart _nested_land_tooltips_widgets[] = {
    NWidget(WWT_PANEL, COLOUR_GREY, 0), SetMinimalSize(64, 32), EndContainer(),
};

static WindowDesc _land_tooltips_desc(
    WDP_MANUAL, NULL, 0, 0,
    CM_WC_LAND_TOOLTIPS, WC_NONE,
    0,
    _nested_land_tooltips_widgets, lengthof(_nested_land_tooltips_widgets)
);

struct LandTooltipsWindow : public Window
{
    TileType tiletype;
    uint16 objIndex;

    LandTooltipsWindow(Window *parent, uint param) : Window(&_land_tooltips_desc)
    {
        this->parent = parent;
        this->tiletype = (TileType)(param & 0xFFFF);
        this->objIndex = (uint16)((param >> 16) & 0xFFFF);
        this->InitNested();
        CLRBITS(this->flags, WF_WHITE_BORDER);
    }

    virtual ~LandTooltipsWindow() {}

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

    virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
    {
        uint icon_size = ScaleGUITrad(10);
        uint line_height = max((uint)FONT_HEIGHT_NORMAL, icon_size) + 2;
        uint icons_width = icon_size * 3 + 20;
        size->width = 200;
        size->height = 6 + FONT_HEIGHT_NORMAL;
        switch(this->tiletype) {
            case MP_HOUSE: {
                const HouseSpec *hs = HouseSpec::Get((HouseID)this->objIndex);
                if(hs == NULL) break;
                SetDParam(0, hs->building_name);
                size->width = GetStringBoundingBox(STR_CM_LAND_TOOLTIPS_HOUSE_NAME).width;
                size->height += line_height;
                SetDParam(0, hs->population);
                size->width = max(size->width, GetStringBoundingBox(STR_CM_LAND_TOOLTIPS_HOUSE_POPULATION).width);
                break;
            }
            case MP_INDUSTRY: {
                const Industry *ind = Industry::GetIfValid((IndustryID)this->objIndex);
                if(ind == NULL) break;

                SetDParam(0, ind->index);
                size->width = max(GetStringBoundingBox(STR_CM_LAND_TOOLTIPS_INDUSTRY_NAME).width, size->width);

                for (CargoID i = 0; i < lengthof(ind->produced_cargo); i++) {
                    if (ind->produced_cargo[i] == CT_INVALID) continue;
                    const CargoSpec *cs = CargoSpec::Get(ind->produced_cargo[i]);
                    if(cs == NULL) continue;
                    size->height += line_height;
                    SetDParam(0, cs->name);
                    SetDParam(1, cs->Index());
                    SetDParam(2, ind->last_month_production[i]);
                    SetDParam(3, ToPercent8(ind->last_month_pct_transported[i]));
                    size->width = max(GetStringBoundingBox(STR_CM_LAND_TOOLTIPS_INDUSTRY_CARGO).width + icons_width, size->width);
                }
                break;
            }
            case MP_STATION: {
                const Station *st = Station::GetIfValid((StationID)this->objIndex);
                if(st == NULL) break;

                SetDParam(0, st->index);
                size->width = max(GetStringBoundingBox(STR_CM_LAND_TOOLTIPS_STATION_NAME).width, size->width);

                for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
                    const CargoSpec *cs = _sorted_cargo_specs[i];
                    if(cs == NULL) continue;
                    int cargoid = cs->Index();
                    if (HasBit(st->goods[cargoid].status, GoodsEntry::GES_RATING)) {
                        size->height += line_height;
                        SetDParam(0, cs->name);
                        SetDParam(1, cargoid);
                        SetDParam(2, st->goods[cargoid].cargo.TotalCount());
                        SetDParam(3, ToPercent8(st->goods[cargoid].rating));
                        size->width = max(GetStringBoundingBox(STR_CM_LAND_TOOLTIPS_STATION_CARGO).width + icons_width, size->width);
                    }
                }
                break;
            }
            default:
                break;
        }
        size->width  += 8 + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
        size->height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
    }

    virtual void DrawWidget(const Rect &r, int widget) const
    {
        uint icon_size = ScaleGUITrad(10);
        uint line_height = max((uint)FONT_HEIGHT_NORMAL, icon_size) + 2;
        uint icons_width = icon_size * 3 + 10;
        uint text_ofs = (line_height - FONT_HEIGHT_NORMAL) >> 1;
        uint icon_ofs = (line_height - icon_size) >> 1;

        GfxDrawLine(r.left,  r.top,    r.right, r.top,    PC_BLACK);
        GfxDrawLine(r.left,  r.bottom, r.right, r.bottom, PC_BLACK);
        GfxDrawLine(r.left,  r.top,    r.left,  r.bottom, PC_BLACK);
        GfxDrawLine(r.right, r.top,    r.right, r.bottom, PC_BLACK);

        int y = r.top + WD_FRAMERECT_TOP + 4;
        int left = r.left + WD_FRAMERECT_LEFT + 4;
        int right = r.right - WD_FRAMERECT_RIGHT - 4;

        switch(this->tiletype) {
            case MP_HOUSE: {
                const HouseSpec *hs = HouseSpec::Get((HouseID)this->objIndex);
                if(hs == NULL) break;

                SetDParam(0, hs->building_name);
                DrawString(left, right, y, STR_CM_LAND_TOOLTIPS_HOUSE_NAME, TC_BLACK, SA_CENTER);
                y += FONT_HEIGHT_NORMAL + 2;
                SetDParam(0, hs->population);
                DrawString(left, right, y, STR_CM_LAND_TOOLTIPS_HOUSE_POPULATION, TC_BLACK, SA_CENTER);
                break;
            }
            case MP_INDUSTRY: {
                const Industry *ind = Industry::GetIfValid((IndustryID)this->objIndex);
                if(ind == NULL) break;

                SetDParam(0, ind->index);
                DrawString(left, right, y, STR_CM_LAND_TOOLTIPS_INDUSTRY_NAME, TC_BLACK, SA_CENTER);
                y += FONT_HEIGHT_NORMAL + 2;

                for (CargoID i = 0; i < lengthof(ind->produced_cargo); i++) {
                    if (ind->produced_cargo[i] == CT_INVALID) continue;
                    const CargoSpec *cs = CargoSpec::Get(ind->produced_cargo[i]);
                    if(cs == NULL) continue;
                    SetDParam(0, cs->name);
                    SetDParam(1, cs->Index());
                    SetDParam(2, ind->last_month_production[i]);
                    SetDParam(3, ToPercent8(ind->last_month_pct_transported[i]));

                    this->DrawSpriteIcons(cs->GetCargoIcon(), left, y + icon_ofs);
                    DrawString(left + icons_width, right, y + text_ofs, STR_CM_LAND_TOOLTIPS_INDUSTRY_CARGO);
                    y += line_height;
                }
                break;
            }
            case MP_STATION: {
                const Station *st = Station::GetIfValid((StationID)this->objIndex);
                if(st == NULL) break;

                SetDParam(0, st->index);
                DrawString(left, right, y, STR_CM_LAND_TOOLTIPS_STATION_NAME, TC_BLACK, SA_CENTER);
                y += FONT_HEIGHT_NORMAL + 2;

                for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
                    const CargoSpec *cs = _sorted_cargo_specs[i];
                    if(cs == NULL) continue;
                    int cargoid = cs->Index();
                    if (HasBit(st->goods[cargoid].status, GoodsEntry::GES_RATING)) {
                        SetDParam(0, cs->name);
                        SetDParam(1, cargoid);
                        SetDParam(2, st->goods[cargoid].cargo.TotalCount());
                        SetDParam(3, ToPercent8(st->goods[cargoid].rating));

                        this->DrawSpriteIcons(cs->GetCargoIcon(), left, y + icon_ofs);
                        DrawString(left + icons_width, right, y + text_ofs, STR_CM_LAND_TOOLTIPS_STATION_CARGO);
                        y += line_height;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    void DrawSpriteIcons(SpriteID sprite, int left, int top) const
    {
        uint step = ScaleGUITrad(10);
        for(int i = 0; i < 3; i++) {
            DrawSprite(sprite, PAL_NONE, left + i * step, top);
        }
    }
};

void ShowLandTooltips(TileIndex tile, Window *parent) {
    static TileIndex last_tooltip_tile = INVALID_TILE;
    if (tile == last_tooltip_tile) return;
    last_tooltip_tile = tile;

    if (tile == INVALID_TILE) {
        DeleteWindowById(CM_WC_LAND_TOOLTIPS, 0);
        return;
    }

    uint param = 0;
    switch (GetTileType(tile)) {
        case MP_HOUSE: {
            if (_settings_client.gui.cm_land_tooltips_for_houses) {
                const HouseID house = GetHouseType(tile);
                param = ((house & 0xFFFF) << 16) | MP_HOUSE;
            }
            break;
        }
        case MP_INDUSTRY: {
            if (_settings_client.gui.cm_land_tooltips_for_industries) {
                const Industry *ind = Industry::GetByTile(tile);
                // if(ind->produced_cargo[0] == CT_INVALID && ind->produced_cargo[1] == CT_INVALID) return;
                param = ((ind->index & 0xFFFF) << 16) | MP_INDUSTRY;
            }
            break;
        }
        case MP_STATION: {
            if (_settings_client.gui.cm_land_tooltips_for_stations) {
                if (IsRailWaypoint(tile) || HasTileWaterGround(tile)) break;
                const Station *st = Station::GetByTile(tile);
                param |= ((st->index & 0xFFFF) << 16) | MP_STATION;
                break;
            }
        }
        default:
            break;
    }
    DeleteWindowById(CM_WC_LAND_TOOLTIPS, 0);

    if (param == 0) return;
    new LandTooltipsWindow(parent, param);
}

static const NWidgetPart _nested_station_rating_tooltip_widgets[] = {
    NWidget(WWT_PANEL, COLOUR_GREY, 0), SetMinimalSize(64, 32), EndContainer(),
};

static WindowDesc _station_rating_tooltip_desc(
    WDP_MANUAL, NULL, 0, 0,
    WC_STATION_RATING_TOOLTIP, WC_NONE,
    0,
    _nested_station_rating_tooltip_widgets, lengthof(_nested_station_rating_tooltip_widgets)
);

static const int STATION_RATING_AGE[] = {0, 10, 20, 33};
static const int STATION_RATING_WAITTIME[] = {0, 25, 50, 95, 130};
static const int STATION_RATING_WAITUNITS[] = {-90, -35, 0, 10, 30, 40};


struct StationRatingTooltipWindow : public Window
{
    TileType tiletype;
    uint16 objIndex;
    TooltipCloseCondition close_cond;
    const Station *st;
    const CargoSpec *cs;
    bool newgrf_rating_used;

    static const uint RATING_TOOLTIP_LINE_BUFF_SIZE = 512;
    static const uint RATING_TOOLTIP_MAX_LINES = 8;
    static const uint RATING_TOOLTIP_NEWGRF_INDENT = 20;

public:
    char data[RATING_TOOLTIP_MAX_LINES + 1][RATING_TOOLTIP_LINE_BUFF_SIZE];

    StationRatingTooltipWindow(Window *parent, const Station *st, const CargoSpec *cs, TooltipCloseCondition close_cond) : Window(&_station_rating_tooltip_desc)
    {
        this->parent = parent;
        this->st = st;
        this->cs = cs;
        this->close_cond = close_cond;
        this->InitNested();
        CLRBITS(this->flags, WF_WHITE_BORDER);
    }

    Point OnInitialPosition(int16 sm_width, int16 sm_height, int window_number) override
    {
        int scr_top = GetMainViewTop() + 2;
        int scr_bot = GetMainViewBottom() - 2;
        Point pt;
        pt.y = Clamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
        if (pt.y + sm_height > scr_bot) pt.y = min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
        pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);
        return pt;
    }

    int RoundRating(int rating) {
        return RoundDivSU(rating * 101, 256);
    }

    void OnInit() override {
        const GoodsEntry *ge = &this->st->goods[this->cs->Index()];

        SetDParam(0, this->cs->name);
        GetString(this->data[0], STR_STATION_RATING_TOOLTIP_RATING_DETAILS, lastof(this->data[0]));
        if (!ge->HasRating()) {
            this->data[1][0] = '\0';
            return;
        }

        uint line_nr = 1;
        int total_rating = 0;

        if (HasBit(cs->callback_mask, CBM_CARGO_STATION_RATING_CALC)) {
            uint last_speed = ge->HasVehicleEverTriedLoading() ? ge->last_speed : 0xFF;

            uint32 var18 = min(ge->time_since_pickup, 0xFF) | (min(ge->max_waiting_cargo, 0xFFFF) << 8) | (min(last_speed, 0xFF) << 24);
            uint32 var10 = (this->st->last_vehicle_type == VEH_INVALID) ? 0x0 : (this->st->last_vehicle_type + 0x10);
            // TODO can desync
            uint16 callback = GetCargoCallback(CBID_CARGO_STATION_RATING_CALC, var10, var18, this->cs);
            int newgrf_rating = 0;
            if (callback != CALLBACK_FAILED) {
                newgrf_rating = GB(callback, 0, 14);
                if (HasBit(callback, 14)) newgrf_rating -= 0x4000;

                this->newgrf_rating_used = true;

                total_rating += newgrf_rating;
                newgrf_rating = this->RoundRating(newgrf_rating);

                SetDParam(0, STR_STATION_RATING_TOOLTIP_NEWGRF_RATING_0 + (newgrf_rating <= 0 ? 0 : 1));
                SetDParam(1, newgrf_rating);
                GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_NEWGRF_RATING, lastof(this->data[line_nr]));
                line_nr++;

                SetDParam(0, min(last_speed, 0xFF));
                GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_NEWGRF_SPEED, lastof(this->data[line_nr]));
                line_nr++;

                SetDParam(0, min(ge->max_waiting_cargo, 0xFFFF));
                GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_NEWGRF_WAITUNITS, lastof(this->data[line_nr]));
                line_nr++;

                SetDParam(0, (min(ge->time_since_pickup, 0xFF) * 5 + 1) / 2);
                GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_NEWGRF_WAITTIME, lastof(this->data[line_nr]));
                line_nr++;
            }
        }

        if (!this->newgrf_rating_used) {

            byte waittime = ge->time_since_pickup;
            if (this->st->last_vehicle_type == VEH_SHIP) waittime >>= 2;
            int waittime_stage = 0;
            (waittime > 21) ||
            (waittime_stage = 1, waittime > 12) ||
            (waittime_stage = 2, waittime > 6) ||
            (waittime_stage = 3, waittime > 3) ||
            (waittime_stage = 4, true);
            total_rating += STATION_RATING_WAITTIME[waittime_stage];

            SetDParam(0, STR_STATION_RATING_TOOLTIP_WAITTIME_0 + waittime_stage);
            SetDParam(1, (ge->time_since_pickup * 5 + 1) / 2);
            SetDParam(2, this->RoundRating(STATION_RATING_WAITTIME[waittime_stage]));
            GetString(this->data[line_nr], this->st->last_vehicle_type == VEH_SHIP ? STR_STATION_RATING_TOOLTIP_WAITTIME_SHIP : STR_STATION_RATING_TOOLTIP_WAITTIME, lastof(this->data[line_nr]));
            line_nr++;

            uint waitunits = ge->max_waiting_cargo;
            int waitunits_stage = 0;
            (waitunits > 1500) ||
            (waitunits_stage = 1, waitunits > 1000) ||
            (waitunits_stage = 2, waitunits > 600) ||
            (waitunits_stage = 3, waitunits > 300) ||
            (waitunits_stage = 4, waitunits > 100) ||
            (waitunits_stage = 5, true);
            total_rating += STATION_RATING_WAITUNITS[waitunits_stage];

            SetDParam(0, STR_STATION_RATING_TOOLTIP_WAITUNITS_0 + waitunits_stage);
            SetDParam(1, ge->max_waiting_cargo);
            SetDParam(2, this->RoundRating(STATION_RATING_WAITUNITS[waitunits_stage]));
            GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_WAITUNITS, lastof(this->data[line_nr]));
            line_nr++;

            int b = ge->last_speed - 85;
            int r_speed = b >= 0 ? b >> 2 : 0;
            int r_speed_round = this->RoundRating(r_speed);
            total_rating += r_speed;
            if (ge->last_speed == 255) {
                SetDParam(0, STR_STATION_RATING_TOOLTIP_SPEED_MAX);
            } else if (r_speed_round == 0) {
                SetDParam(0, STR_STATION_RATING_TOOLTIP_SPEED_ZERO);
            } else {
                SetDParam(0, STR_STATION_RATING_TOOLTIP_SPEED_0 + r_speed / 11);
            }
            SetDParam(0, ge->last_speed == 255 ? STR_STATION_RATING_TOOLTIP_SPEED_MAX : STR_STATION_RATING_TOOLTIP_SPEED_0 + r_speed / 11);
            SetDParam(1, ge->last_speed);
            SetDParam(2, r_speed_round);
            GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_SPEED, lastof(this->data[line_nr]));
            line_nr++;
        }

        int age_stage = (ge->last_age >= 3 ? 0 : 3 - ge->last_age);
        total_rating += STATION_RATING_AGE[age_stage];
        SetDParam(0, STR_STATION_RATING_TOOLTIP_AGE_0 + age_stage);
        SetDParam(1, ge->last_age);
        SetDParam(2, this->RoundRating(STATION_RATING_AGE[age_stage]));
        GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_AGE, lastof(this->data[line_nr]));
        line_nr++;

        if (Company::IsValidID(st->owner) && HasBit(st->town->statues, st->owner)) {
            SetDParam(0, STR_STATION_RATING_TOOLTIP_STATUE_YES);
            total_rating += 26;
        } else {
            SetDParam(0, STR_STATION_RATING_TOOLTIP_STATUE_NO);
        }
        GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_STATUE, lastof(this->data[line_nr]));
        line_nr++;

        SetDParam(0, ToPercent8(Clamp(total_rating, 0, 255)));
        GetString(this->data[line_nr], STR_STATION_RATING_TOOLTIP_TOTAL_RATING, lastof(this->data[line_nr]));
        line_nr++;

        this->data[line_nr][0] = '\0';
    }

    void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize) override
    {
        size->height = WD_FRAMETEXT_TOP + WD_FRAMETEXT_BOTTOM + 2;
        for (uint i = 0; i <= RATING_TOOLTIP_MAX_LINES; i++) {
            if (StrEmpty(this->data[i])) break;

            uint width = GetStringBoundingBox(this->data[i]).width + WD_FRAMETEXT_LEFT + WD_FRAMETEXT_RIGHT + 2;
            if (this->newgrf_rating_used && i >= 2 && i <= 4)
                width += RATING_TOOLTIP_NEWGRF_INDENT;
            size->width = max(size->width, width);
            size->height += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
        }
        size->height -= WD_PAR_VSEP_NORMAL;
    }

    void DrawWidget(const Rect &r, int widget) const override
    {
        GfxDrawLine(r.left,  r.top,    r.right, r.top,    PC_BLACK);
        GfxDrawLine(r.left,  r.bottom, r.right, r.bottom, PC_BLACK);
        GfxDrawLine(r.left,  r.top,    r.left,  r.bottom, PC_BLACK);
        GfxDrawLine(r.right, r.top,    r.right, r.bottom, PC_BLACK);

        int y = r.top + WD_FRAMETEXT_TOP + 1;
        int left0 = r.left + WD_FRAMETEXT_LEFT + 1;
        int right0 = r.right - WD_FRAMETEXT_RIGHT - 1;
        DrawString(left0, right0, y, this->data[0], TC_LIGHT_BLUE, SA_CENTER);
        y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
        for (uint i = 1; i <= RATING_TOOLTIP_MAX_LINES; i++) {
            if (StrEmpty(this->data[i])) break;
            int left = left0, right = right0;
            if (this->newgrf_rating_used && i >= 2 && i <= 4) {
                if (_current_text_dir == TD_RTL) {
                    right -= RATING_TOOLTIP_NEWGRF_INDENT;
                } else {
                    left += RATING_TOOLTIP_NEWGRF_INDENT;
                }
            }
            DrawString(left, right, y, this->data[i], TC_BLACK);
            y += FONT_HEIGHT_NORMAL + WD_PAR_VSEP_NORMAL;
        }
    }

    void OnMouseLoop() override
    {
        /* Always close tooltips when the cursor is not in our window. */
        if (!_cursor.in_window) {
            delete this;
            return;
        }

        /* We can show tooltips while dragging tools. These are shown as long as
         * we are dragging the tool. Normal tooltips work with hover or rmb. */
        switch (this->close_cond) {
            case TCC_RIGHT_CLICK: if (!_right_button_down) delete this; break;
            case TCC_HOVER: if (!_mouse_hovering) delete this; break;
            case TCC_NONE: break;
        }
    }
};

bool ShowStationRatingTooltip(Window *parent, const Station *st, const CargoSpec *cs, TooltipCloseCondition close_cond) {
    DeleteWindowById(WC_STATION_RATING_TOOLTIP, 0);
    new StationRatingTooltipWindow(parent, st, cs, close_cond);
    return true;
}


} // namespace citymania
