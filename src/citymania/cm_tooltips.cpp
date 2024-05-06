#include "../stdafx.h"

#include "cm_tooltips.hpp"

#include "../company_base.h"
#include "../debug.h"
#include "../house.h"
#include "../industry.h"
#include "../newgrf_callbacks.h"
#include "../newgrf_cargo.h"
#include "../progress.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../tile_type.h"
#include "../town_map.h"
#include "../town.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../zoom_func.h"

#include "../safeguards.h"

namespace citymania {

static const NWidgetPart _nested_land_tooltips_widgets[] = {
    NWidget(WWT_PANEL, COLOUR_GREY, 0), SetMinimalSize(64, 32), EndContainer(),
};

static WindowDesc _land_tooltips_desc(__FILE__, __LINE__,
    WDP_MANUAL, nullptr, 0, 0,
    CM_WC_LAND_TOOLTIPS, WC_NONE,
    0,
    std::begin(_nested_land_tooltips_widgets), std::end(_nested_land_tooltips_widgets)
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

    Point OnInitialPosition(int16 sm_width, int16 sm_height, int /* window_number */) override
    {
        int scr_top = GetMainViewTop() + 2;
        int scr_bot = GetMainViewBottom() - 2;
        Point pt;
        pt.y = Clamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
        if (pt.y + sm_height > scr_bot) pt.y = std::min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
        pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);
        return pt;
    }

    void UpdateWidgetSize(int /* widget */, Dimension *size, const Dimension & /* padding */, Dimension * /* fill */, Dimension * /* resize */) override
    {
        uint icon_size = ScaleGUITrad(10);
        uint line_height = std::max((uint)GetCharacterHeight(FS_NORMAL), icon_size) + WidgetDimensions::scaled.hsep_normal;
        uint text_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.hsep_normal;
        uint icons_width = icon_size * 3 + WidgetDimensions::scaled.vsep_normal;
        size->width = ScaleGUITrad(194);
        size->height = GetCharacterHeight(FS_NORMAL);
        switch(this->tiletype) {
            case MP_HOUSE: {
                const HouseSpec *hs = HouseSpec::Get((HouseID)this->objIndex);
                if(hs == NULL) break;
                SetDParam(0, hs->building_name);
                size->width = std::max(GetStringBoundingBox(CM_STR_LAND_TOOLTIPS_HOUSE_NAME).width, size->width);
                size->height += text_height;
                SetDParam(0, hs->population);
                size->width = std::max(size->width, GetStringBoundingBox(CM_STR_LAND_TOOLTIPS_HOUSE_POPULATION).width);
                break;
            }
            case MP_INDUSTRY: {
                const Industry *ind = Industry::GetIfValid((IndustryID)this->objIndex);
                if(ind == NULL) break;

                SetDParam(0, ind->index);
                size->width = std::max(GetStringBoundingBox(CM_STR_LAND_TOOLTIPS_INDUSTRY_NAME).width, size->width);

                for (auto &p : ind->produced) {
                    if (!IsValidCargoID(p.cargo)) continue;
                    const CargoSpec *cs = CargoSpec::Get(p.cargo);
                    if(cs == NULL) continue;
                    size->height += line_height;
                    SetDParam(0, cs->name);
                    SetDParam(1, cs->Index());
                    SetDParam(2, p.history[LAST_MONTH].production);
                    SetDParam(3, ToPercent8(p.history[LAST_MONTH].PctTransported()));
                    size->width = std::max(GetStringBoundingBox(CM_STR_LAND_TOOLTIPS_INDUSTRY_CARGO).width + icons_width, size->width);
                }
                break;
            }
            case MP_STATION: {
                const Station *st = Station::GetIfValid((StationID)this->objIndex);
                if(st == NULL) break;

                SetDParam(0, st->index);
                size->width = std::max(GetStringBoundingBox(CM_STR_LAND_TOOLTIPS_STATION_NAME).width, size->width);

                for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
                    int cargoid = cs->Index();
                    if (HasBit(st->goods[cargoid].status, GoodsEntry::GES_RATING)) {
                        size->height += line_height;
                        SetDParam(0, cs->name);
                        SetDParam(1, cargoid);
                        SetDParam(2, st->goods[cargoid].cargo.TotalCount());
                        SetDParam(3, ToPercent8(st->goods[cargoid].rating));
                        size->width = std::max(GetStringBoundingBox(CM_STR_LAND_TOOLTIPS_STATION_CARGO).width + icons_width, size->width);
                    }
                }
                break;
            }
            default:
                break;
        }
        size->width  += WidgetDimensions::scaled.framerect.Horizontal() + WidgetDimensions::scaled.fullbevel.Horizontal();
        size->height += WidgetDimensions::scaled.framerect.Vertical() + WidgetDimensions::scaled.fullbevel.Vertical();
    }

    void DrawWidget(const Rect &r, int /* widget */) const override
    {
        uint icon_size = ScaleGUITrad(10);
        uint line_height = std::max((uint)GetCharacterHeight(FS_NORMAL), icon_size) + WidgetDimensions::scaled.hsep_normal;
        uint text_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.hsep_normal;
        uint icons_width = icon_size * 3 + WidgetDimensions::scaled.vsep_normal;
        uint text_ofs = (line_height - GetCharacterHeight(FS_NORMAL)) >> 1;
        uint icon_ofs = (line_height - icon_size) >> 1;

        GfxFillRect(r.left, r.top, r.right, r.top + WidgetDimensions::scaled.bevel.top - 1, PC_BLACK);
        GfxFillRect(r.left, r.bottom - WidgetDimensions::scaled.bevel.bottom + 1, r.right, r.bottom, PC_BLACK);
        GfxFillRect(r.left, r.top, r.left + WidgetDimensions::scaled.bevel.left - 1,  r.bottom, PC_BLACK);
        GfxFillRect(r.right - WidgetDimensions::scaled.bevel.right + 1, r.top, r.right, r.bottom, PC_BLACK);

        auto ir = r.Shrink(WidgetDimensions::scaled.framerect).Shrink(WidgetDimensions::scaled.fullbevel);
        switch(this->tiletype) {
            case MP_HOUSE: {
                const HouseSpec *hs = HouseSpec::Get((HouseID)this->objIndex);
                if(hs == NULL) break;

                SetDParam(0, hs->building_name);
                DrawString(ir, CM_STR_LAND_TOOLTIPS_HOUSE_NAME, TC_BLACK, SA_CENTER);
                ir.top += text_height;
                SetDParam(0, hs->population);
                DrawString(ir, CM_STR_LAND_TOOLTIPS_HOUSE_POPULATION, TC_BLACK, SA_CENTER);
                break;
            }
            case MP_INDUSTRY: {
                const Industry *ind = Industry::GetIfValid((IndustryID)this->objIndex);
                if(ind == NULL) break;

                SetDParam(0, ind->index);
                DrawString(ir, CM_STR_LAND_TOOLTIPS_INDUSTRY_NAME, TC_BLACK, SA_CENTER);
                ir.top += text_height;

                for (auto &p : ind->produced) {
                    if (!IsValidCargoID(p.cargo)) continue;
                    const CargoSpec *cs = CargoSpec::Get(p.cargo);
                    if(cs == NULL) continue;
                    SetDParam(0, cs->name);
                    SetDParam(1, cs->Index());
                    SetDParam(2, p.history[LAST_MONTH].production);
                    SetDParam(3, ToPercent8(p.history[LAST_MONTH].PctTransported()));

                    this->DrawSpriteIcons(cs->GetCargoIcon(), ir.left, ir.top + icon_ofs);
                    DrawString(ir.left + icons_width, ir.right, ir.top + text_ofs, CM_STR_LAND_TOOLTIPS_INDUSTRY_CARGO);
                    ir.top += line_height;
                }
                break;
            }
            case MP_STATION: {
                const Station *st = Station::GetIfValid((StationID)this->objIndex);
                if(st == NULL) break;

                SetDParam(0, st->index);
                DrawString(ir, CM_STR_LAND_TOOLTIPS_STATION_NAME, TC_BLACK, SA_CENTER);
                ir.top += text_height;

                for (const CargoSpec *cs : _sorted_standard_cargo_specs) {
                    int cargoid = cs->Index();
                    if (HasBit(st->goods[cargoid].status, GoodsEntry::GES_RATING)) {
                        SetDParam(0, cs->name);
                        SetDParam(1, cargoid);
                        SetDParam(2, st->goods[cargoid].cargo.TotalCount());
                        SetDParam(3, ToPercent8(st->goods[cargoid].rating));

                        this->DrawSpriteIcons(cs->GetCargoIcon(), ir.left, ir.top + icon_ofs);
                        DrawString(ir.left + icons_width, ir.right, ir.top + text_ofs, CM_STR_LAND_TOOLTIPS_STATION_CARGO);
                        ir.top += line_height;
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
        CloseWindowById(CM_WC_LAND_TOOLTIPS, 0);
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
    CloseWindowById(CM_WC_LAND_TOOLTIPS, 0);

    if (param == 0) return;
    new LandTooltipsWindow(parent, param);
}

static const NWidgetPart _nested_station_rating_tooltip_widgets[] = {
    NWidget(WWT_PANEL, COLOUR_GREY, 0), SetMinimalSize(64, 32), EndContainer(),
};

static WindowDesc _station_rating_tooltip_desc(__FILE__, __LINE__,
    WDP_MANUAL, nullptr, 0, 0,
    WC_STATION_RATING_TOOLTIP, WC_NONE,
    0,
    std::begin(_nested_station_rating_tooltip_widgets), std::end(_nested_station_rating_tooltip_widgets)
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

    static const uint RATING_TOOLTIP_NEWGRF_INDENT = 20;

public:
    std::vector<std::string> data;

    StationRatingTooltipWindow(Window *parent, const Station *st, const CargoSpec *cs, TooltipCloseCondition close_cond) : Window(&_station_rating_tooltip_desc)
    {
        this->parent = parent;
        this->st = st;
        this->cs = cs;
        this->close_cond = close_cond;
        this->InitNested();
        CLRBITS(this->flags, WF_WHITE_BORDER);
    }

    Point OnInitialPosition(int16 sm_width, int16 sm_height, int /* window_number */) override
    {
        int scr_top = GetMainViewTop() + 2;
        int scr_bot = GetMainViewBottom() - 2;
        Point pt;
        pt.y = Clamp(_cursor.pos.y + _cursor.total_size.y + _cursor.total_offs.y + 5, scr_top, scr_bot);
        if (pt.y + sm_height > scr_bot) pt.y = std::min(_cursor.pos.y + _cursor.total_offs.y - 5, scr_bot) - sm_height;
        pt.x = sm_width >= _screen.width ? 0 : Clamp(_cursor.pos.x - (sm_width >> 1), 0, _screen.width - sm_width);
        return pt;
    }

    int RoundRating(int rating) {
        return RoundDivSU(rating * 101, 256);
    }

    void OnInit() override {
        this->data.clear();
        const GoodsEntry *ge = &this->st->goods[this->cs->Index()];

        SetDParam(0, this->cs->name);
        this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_RATING_DETAILS));
        if (!ge->HasRating()) { return; }

        int total_rating = 0;

        if (HasBit(cs->callback_mask, CBM_CARGO_STATION_RATING_CALC)) {
            uint last_speed = ge->HasVehicleEverTriedLoading() ? ge->last_speed : 0xFF;

            uint32 var18 = std::min<uint32>(ge->time_since_pickup, 0xFF) | (std::min<uint32>(ge->max_waiting_cargo, 0xFFFF) << 8) | (std::min<uint32>(last_speed, 0xFF) << 24);
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

                SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_NEWGRF_RATING_0 + (newgrf_rating <= 0 ? 0 : 1));
                SetDParam(1, newgrf_rating);
                this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_NEWGRF_RATING));

                SetDParam(0, std::min<uint>(last_speed, 0xFF));
                this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_NEWGRF_SPEED));

                SetDParam(0, std::min<uint>(ge->max_waiting_cargo, 0xFFFF));
                this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_NEWGRF_WAITUNITS));

                SetDParam(0, (std::min<uint>(ge->time_since_pickup, 0xFF) * 5 + 1) / 2);
                this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_NEWGRF_WAITTIME));
            }
        }

        if (!this->newgrf_rating_used) {

            byte waittime = ge->time_since_pickup;
            if (this->st->last_vehicle_type == VEH_SHIP) waittime >>= 2;
            int waittime_stage = 0;
            if (waittime <= 21) waittime_stage = 1;
            if (waittime <= 12) waittime_stage = 2;
            if (waittime <= 6) waittime_stage = 3;
            if (waittime <= 3) waittime_stage = 4;
            total_rating += STATION_RATING_WAITTIME[waittime_stage];

            SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_WAITTIME_0 + waittime_stage);
            SetDParam(1, (ge->time_since_pickup * 5 + 1) / 2);
            SetDParam(2, this->RoundRating(STATION_RATING_WAITTIME[waittime_stage]));
            this->data.push_back(GetString(this->st->last_vehicle_type == VEH_SHIP ? CM_STR_STATION_RATING_TOOLTIP_WAITTIME_SHIP : CM_STR_STATION_RATING_TOOLTIP_WAITTIME));

            int waitunits_stage = 0;
            if (ge->max_waiting_cargo <= 1500) waitunits_stage = 1;
            if (ge->max_waiting_cargo <= 1000) waitunits_stage = 2;
            if (ge->max_waiting_cargo <= 600) waitunits_stage = 3;
            if (ge->max_waiting_cargo <= 300) waitunits_stage = 4;
            if (ge->max_waiting_cargo <= 100) waitunits_stage = 5;
            total_rating += STATION_RATING_WAITUNITS[waitunits_stage];

            SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_WAITUNITS_0 + waitunits_stage);
            SetDParam(1, ge->max_waiting_cargo);
            SetDParam(2, this->RoundRating(STATION_RATING_WAITUNITS[waitunits_stage]));
            this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_WAITUNITS));

            int b = ge->last_speed - 85;
            int r_speed = b >= 0 ? b >> 2 : 0;
            int r_speed_round = this->RoundRating(r_speed);
            total_rating += r_speed;
            if (ge->last_speed == 255) {
                SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_SPEED_MAX);
            } else if (r_speed_round == 0) {
                SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_SPEED_ZERO);
            } else {
                SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_SPEED_0 + r_speed / 11);
            }
            SetDParam(0, ge->last_speed == 255 ? CM_STR_STATION_RATING_TOOLTIP_SPEED_MAX : CM_STR_STATION_RATING_TOOLTIP_SPEED_0 + r_speed / 11);
            SetDParam(1, ge->last_speed);
            SetDParam(2, r_speed_round);
            this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_SPEED));
        }

        int age_stage = (ge->last_age >= 3 ? 0 : 3 - ge->last_age);
        total_rating += STATION_RATING_AGE[age_stage];
        SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_AGE_0 + age_stage);
        SetDParam(1, ge->last_age);
        SetDParam(2, this->RoundRating(STATION_RATING_AGE[age_stage]));
        this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_AGE));

        if (Company::IsValidID(st->owner) && HasBit(st->town->statues, st->owner)) {
            SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_STATUE_YES);
            total_rating += 26;
        } else {
            SetDParam(0, CM_STR_STATION_RATING_TOOLTIP_STATUE_NO);
        }
        this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_STATUE));

        SetDParam(0, ToPercent8(Clamp(total_rating, 0, 255)));
        this->data.push_back(GetString(CM_STR_STATION_RATING_TOOLTIP_TOTAL_RATING));
    }

    void UpdateWidgetSize(int /* widget */, Dimension *size, const Dimension & /* padding */, Dimension * /* fill */, Dimension * /* resize */) override
    {
        size->height = WidgetDimensions::scaled.framerect.Vertical() + WidgetDimensions::scaled.fullbevel.Vertical();
        for (uint i = 0; i < data.size(); i++) {
            uint width = GetStringBoundingBox(this->data[i]).width + WidgetDimensions::scaled.framerect.Horizontal() + WidgetDimensions::scaled.fullbevel.Horizontal();
            if (this->newgrf_rating_used && i >= 2 && i <= 4)
                width += RATING_TOOLTIP_NEWGRF_INDENT;
            size->width = std::max(size->width, width);
            size->height += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.hsep_normal;
        }
        size->height -= WidgetDimensions::scaled.hsep_normal;
    }

    void DrawWidget(const Rect &r, int /* widget */) const override
    {
        GfxFillRect(r.left, r.top, r.right, r.top + WidgetDimensions::scaled.bevel.top - 1, PC_BLACK);
        GfxFillRect(r.left, r.bottom - WidgetDimensions::scaled.bevel.bottom + 1, r.right, r.bottom, PC_BLACK);
        GfxFillRect(r.left, r.top, r.left + WidgetDimensions::scaled.bevel.left - 1,  r.bottom, PC_BLACK);
        GfxFillRect(r.right - WidgetDimensions::scaled.bevel.right + 1, r.top, r.right, r.bottom, PC_BLACK);

        auto ir = r.Shrink(WidgetDimensions::scaled.framerect).Shrink(WidgetDimensions::scaled.bevel);

        DrawString(ir, this->data[0], TC_LIGHT_BLUE, SA_CENTER);
        ir.top += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
        for (uint i = 1; i < data.size(); i++) {
            auto indent = (this->newgrf_rating_used && i >= 2 && i <= 4 ? RATING_TOOLTIP_NEWGRF_INDENT : 0);
            DrawString(ir.Indent(indent, _current_text_dir == TD_RTL), this->data[i], TC_BLACK);
            ir.top += GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.vsep_normal;
        }
    }

    void OnMouseLoop() override
    {
        /* Always close tooltips when the cursor is not in our window. */
        if (!_cursor.in_window) {
            this->Close();
            return;
        }

        /* We can show tooltips while dragging tools. These are shown as long as
         * we are dragging the tool. Normal tooltips work with hover or rmb. */
        switch (this->close_cond) {
            case TCC_RIGHT_CLICK: if (!_right_button_down) this->Close(); break;
            case TCC_HOVER: if (!_mouse_hovering) this->Close(); break;
            case TCC_NONE: break;

            case TCC_EXIT_VIEWPORT: {
                Window *w = FindWindowFromPt(_cursor.pos.x, _cursor.pos.y);
                if (w == nullptr || IsPtInWindowViewport(w, _cursor.pos.x, _cursor.pos.y) == nullptr) this->Close();
                break;
            }
        }
    }
};

bool ShowStationRatingTooltip(Window *parent, const Station *st, const CargoSpec *cs, TooltipCloseCondition close_cond) {
    CloseWindowById(WC_STATION_RATING_TOOLTIP, 0);
    new StationRatingTooltipWindow(parent, st, cs, close_cond);
    return true;
}

/* copied from window.cpp */
// static bool MayBeShown(const Window *w)
// {
//     /* If we're not modal, everything is okay. */
//     if (!HasModalProgress()) return true;

//     switch (w->window_class) {
//         case WC_MAIN_WINDOW:    ///< The background, i.e. the game.
//         case WC_MODAL_PROGRESS: ///< The actual progress window.
//         case WC_CONFIRM_POPUP_QUERY: ///< The abort window.
//             return true;

//         default:
//             return false;
//     }
// }

Window *FindHoverableWindowFromPt(int x, int y)
{
    for (Window *w : Window::IterateFromFront()) {
        if (MayBeShown(w) && IsInsideBS(x, w->left, w->width) && IsInsideBS(y, w->top, w->height)
                && dynamic_cast<LandTooltipsWindow*>(w) == nullptr
                && dynamic_cast<StationRatingTooltipWindow*>(w) == nullptr) {
            return w;
        }
    }

    return nullptr;
}

} // namespace citymania
