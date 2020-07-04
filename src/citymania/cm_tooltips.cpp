#include "../stdafx.h"

#include "cm_tooltips.hpp"

#include "../house.h"
#include "../industry.h"
#include "../station_base.h"
#include "../station_map.h"
#include "../strings_func.h"
#include "../tile_type.h"
#include "../town_map.h"
#include "../window_func.h"
#include "../window_gui.h"
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

    // virtual void OnMouseLoop()
    // {
    //     if (!_cursor.in_window || !_mouse_hovering) {
    //         delete this;
    //     }
    // }

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

} // namespace citymania
