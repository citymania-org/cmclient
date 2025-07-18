/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file train_gui.cpp GUI for trains. */

#include "stdafx.h"
#include "window_gui.h"
#include "command_func.h"
#include "train.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "zoom_func.h"
#include "train_cmd.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Callback for building wagons.
 * @param result The result of the command.
 * @param new_veh_id ID of the ne vehicle.
 * @param tile   The tile the command was executed on.
 */
void CcBuildWagon(Commands, const CommandCost &result, VehicleID new_veh_id, uint, uint16_t, CargoArray, TileIndex tile, EngineID, bool, CargoType, ClientID)
{
	if (result.Failed()) return;

	/* find a locomotive in the depot. */
	const Vehicle *found = nullptr;
	for (const Train *t : Train::Iterate()) {
		if (t->IsFrontEngine() && t->tile == tile && t->IsStoppedInDepot()) {
			if (found != nullptr) return; // must be exactly one.
			found = t;
		}
	}

	/* if we found a loco, */
	if (found != nullptr) {
		found = found->Last();
		/* put the new wagon at the end of the loco. */
		Command<CMD_MOVE_RAIL_VEHICLE>::Post(found->tile, new_veh_id, found->index, false);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	}
}

/**
 * Highlight the position where a rail vehicle is dragged over by drawing a light gray background.
 * @param px        The current x position to draw from.
 * @param max_width The maximum space available to draw.
 * @param y         The vertical centre position to draw from.
 * @param selection Selected vehicle that is dragged.
 * @param chain     Whether a whole chain is dragged.
 * @return The width of the highlight mark.
 */
static int HighlightDragPosition(int px, int max_width, int y, VehicleID selection, bool chain)
{
	bool rtl = _current_text_dir == TD_RTL;

	assert(selection != VehicleID::Invalid());
	int dragged_width = 0;
	for (Train *t = Train::Get(selection); t != nullptr; t = chain ? t->Next() : (t->HasArticulatedPart() ? t->GetNextArticulatedPart() : nullptr)) {
		dragged_width += t->GetDisplayImageWidth(nullptr);
	}

	int drag_hlight_left = rtl ? std::max(px - dragged_width + 1, 0) : px;
	int drag_hlight_right = rtl ? px : std::min(px + dragged_width, max_width) - 1;
	int drag_hlight_width = std::max(drag_hlight_right - drag_hlight_left + 1, 0);

	if (drag_hlight_width > 0) {
		int height = ScaleSpriteTrad(12);
		int top = y - height / 2;
		Rect r = {drag_hlight_left, top, drag_hlight_right, top + height - 1};
		/* Sprite-scaling is used here as the area is from sprite size */
		GfxFillRect(r.Shrink(ScaleSpriteTrad(1)), GetColourGradient(COLOUR_GREY, SHADE_LIGHTEST));
	}

	return drag_hlight_width;
}

/**
 * Draws an image of a whole train
 * @param v         Front vehicle
 * @param r         Rect to draw at
 * @param selection Selected vehicle to draw a frame around
 * @param skip      Number of pixels to skip at the front (for scrolling)
 * @param drag_dest The vehicle another one is dragged over, \c VehicleID::Invalid() if none.
 */
void DrawTrainImage(const Train *v, const Rect &r, VehicleID selection, EngineImageType image_type, int skip, VehicleID drag_dest)
{
	bool rtl = _current_text_dir == TD_RTL;
	Direction dir = rtl ? DIR_E : DIR_W;

	DrawPixelInfo tmp_dpi;
	/* Position of highlight box */
	int highlight_l = 0;
	int highlight_r = 0;
	int max_width = r.Width();

	if (!FillDrawPixelInfo(&tmp_dpi, r)) return;

	{
		AutoRestoreBackup dpi_backup(_cur_dpi, &tmp_dpi);

		bool do_overlays = ShowCargoIconOverlay();
		/* List of overlays, only used if cargo icon overlays are enabled. */
		static std::vector<CargoIconOverlay> overlays;

		int px = rtl ? max_width + skip : -skip;
		int y = r.Height() / 2;
		bool sel_articulated = false;
		bool dragging = (drag_dest != VehicleID::Invalid());
		bool drag_at_end_of_train = (drag_dest == v->index); // Head index is used to mark dragging at end of train.
		for (; v != nullptr && (rtl ? px > 0 : px < max_width); v = v->Next()) {
			if (dragging && !drag_at_end_of_train && drag_dest == v->index) {
				/* Highlight the drag-and-drop destination inside the train. */
				int drag_hlight_width = HighlightDragPosition(px, max_width, y, selection, _cursor.vehchain);
				px += rtl ? -drag_hlight_width : drag_hlight_width;
			}

			Point offset;
			int width = Train::From(v)->GetDisplayImageWidth(&offset);

			if (rtl ? px + width > 0 : px - width < max_width) {
				PaletteID pal = v->vehstatus.Test(VehState::Crashed) ? PALETTE_CRASH : GetVehiclePalette(v);
				VehicleSpriteSeq seq;
				v->GetImage(dir, image_type, &seq);
				seq.Draw(px + (rtl ? -offset.x : offset.x), y + offset.y, pal, v->vehstatus.Test(VehState::Crashed));
			}

			if (!v->IsArticulatedPart()) sel_articulated = false;

			if (v->index == selection) {
				/* Set the highlight position */
				highlight_l = rtl ? px - width : px;
				highlight_r = rtl ? px - 1 : px + width - 1;
				sel_articulated = true;
			} else if ((_cursor.vehchain && highlight_r != 0) || sel_articulated) {
				if (rtl) {
					highlight_l -= width;
				} else {
					highlight_r += width;
				}
			}

			if (do_overlays) AddCargoIconOverlay(overlays, px, width, v);
			px += rtl ? -width : width;
		}

		if (do_overlays) {
			DrawCargoIconOverlays(overlays, y);
			overlays.clear();
		}

		if (dragging && drag_at_end_of_train) {
			/* Highlight the drag-and-drop destination at the end of the train. */
			HighlightDragPosition(px, max_width, y, selection, _cursor.vehchain);
		}
	}

	if (highlight_l != highlight_r) {
		/* Draw the highlight. Now done after drawing all the engines, as
		 * the next engine after the highlight could overlap it. */
		int height = ScaleSpriteTrad(12);
		Rect hr = {highlight_l, 0, highlight_r, height - 1};
		DrawFrameRect(hr.Translate(r.left, CenterBounds(r.top, r.bottom, height)).Expand(WidgetDimensions::scaled.bevel), COLOUR_WHITE, FrameFlag::BorderOnly);
	}
}

/** Helper struct for the cargo details information */
struct CargoSummaryItem {
	CargoType cargo;    ///< The cargo that is carried
	StringID subtype; ///< STR_EMPTY if none
	uint capacity;    ///< Amount that can be carried
	uint amount;      ///< Amount that is carried
	StationID source; ///< One of the source stations

	/** Used by std::find() and similar functions */
	inline bool operator == (const CargoSummaryItem &other) const
	{
		return !(this->cargo != other.cargo);
	}
};

static const uint TRAIN_DETAILS_MIN_INDENT  = 32; ///< Minimum indent level in the train details window
static const uint TRAIN_DETAILS_MAX_INDENT  = 72; ///< Maximum indent level in the train details window; wider than this and we start on a new line

/** Container for the cargo summary information. */
typedef std::vector<CargoSummaryItem> CargoSummary;
/** Reused container of cargo details */
static CargoSummary _cargo_summary;

/**
 * Draw the details cargo tab for the given vehicle at the given position
 *
 * @param item  Data to draw
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsCargoTab(const CargoSummaryItem *item, int left, int right, int y)
{
	std::string str;
	if (!IsValidCargoType(item->cargo)) {
		str = GetString(STR_QUANTITY_N_A);
	} else if (item->amount == 0) {
		str = GetString(STR_VEHICLE_DETAILS_CARGO_EMPTY);
	} else if (FreightWagonMult(item->cargo) > 1) {
		str = GetString(STR_VEHICLE_DETAILS_CARGO_FROM_MULT, item->cargo, item->amount, item->source, _settings_game.vehicle.freight_trains);
	} else {
		str = GetString(STR_VEHICLE_DETAILS_CARGO_FROM, item->cargo, item->amount, item->source);
	}
	DrawString(left, right, y, str, TC_LIGHT_BLUE);
}

/**
 * Draw the details info tab for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsInfoTab(const Vehicle *v, int left, int right, int y)
{
	std::string str;
	if (RailVehInfo(v->engine_type)->railveh_type == RAILVEH_WAGON) {
		if (_settings_client.gui.developer < 1) str = GetString(STR_VEHICLE_DETAILS_TRAIN_WAGON_VALUE, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails), v->value);
		else str = GetString(CM_STR_VEHICLE_DETAILS_TRAIN_WAGON_VALUE_WITH_ID, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails), v->value, v->index);
	} else {
		if (_settings_client.gui.developer < 1) str = GetString(STR_VEHICLE_DETAILS_TRAIN_ENGINE_BUILT_AND_VALUE, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails), v->build_year, v->value);
		else str = GetString(CM_STR_VEHICLE_DETAILS_TRAIN_ENGINE_BUILT_AND_VALUE_WITH_ID, PackEngineNameDParam(v->engine_type, EngineNameContext::VehicleDetails), v->build_year, v->value, v->index);
	}
	DrawString(left, right, y, str);
}

/**
 * Draw the details capacity tab for the given vehicle at the given position
 *
 * @param item  Data to draw
 * @param left  The left most coordinate to draw
 * @param right The right most coordinate to draw
 * @param y     The y coordinate
 */
static void TrainDetailsCapacityTab(const CargoSummaryItem *item, int left, int right, int y)
{
	std::string str;
	if (!IsValidCargoType(item->cargo)) {
		/* Draw subtype only */
		str = GetString(STR_VEHICLE_INFO_NO_CAPACITY, item->subtype);
	} else if (FreightWagonMult(item->cargo) > 1) {
		str = GetString(STR_VEHICLE_INFO_CAPACITY_MULT, item->cargo, item->capacity, item->subtype, _settings_game.vehicle.freight_trains);
	} else {
		str = GetString(STR_VEHICLE_INFO_CAPACITY, item->cargo, item->capacity, item->subtype);
	}
	DrawString(left, right, y, str);
}

/**
 * Collects the cargo transported
 * @param v Vehicle to process
 * @param summary Space for the result
 */
static void GetCargoSummaryOfArticulatedVehicle(const Train *v, CargoSummary &summary)
{
	summary.clear();
	do {
		if (!v->GetEngine()->CanCarryCargo()) continue;

		CargoSummaryItem new_item;
		new_item.cargo = v->cargo_cap > 0 ? v->cargo_type : INVALID_CARGO;
		new_item.subtype = GetCargoSubtypeText(v);
		if (!IsValidCargoType(new_item.cargo) && new_item.subtype == STR_EMPTY) continue;

		auto item = std::ranges::find(summary, new_item);
		if (item == std::end(summary)) {
			item = summary.emplace(std::end(summary));
			item->cargo = new_item.cargo;
			item->subtype = new_item.subtype;
			item->capacity = 0;
			item->amount = 0;
			item->source = StationID::Invalid();
		}

		item->capacity += v->cargo_cap;
		item->amount += v->cargo.StoredCount();
		if (item->source == StationID::Invalid()) item->source = v->cargo.GetFirstStation();
	} while ((v = v->Next()) != nullptr && v->IsArticulatedPart());
}

/**
 * Get the length of an articulated vehicle.
 * @param v the vehicle to get the length of.
 * @return the length in pixels.
 */
static uint GetLengthOfArticulatedVehicle(const Train *v)
{
	uint length = 0;

	do {
		length += v->GetDisplayImageWidth();
	} while ((v = v->Next()) != nullptr && v->IsArticulatedPart());

	return length;
}

/**
 * Determines the number of lines in the train details window
 * @param veh_id Train
 * @param det_tab Selected details tab
 * @return Number of line
 */
int GetTrainDetailsWndVScroll(VehicleID veh_id, TrainDetailsWindowTabs det_tab)
{
	int num = 0;

	if (det_tab == TDW_TAB_TOTALS) { // Total cargo tab
		CargoArray max_cargo{};
		for (const Vehicle *v = Vehicle::Get(veh_id); v != nullptr; v = v->Next()) {
			max_cargo[v->cargo_type] += v->cargo_cap;
		}

		num = max_cargo.GetCount();
		num++; // needs one more because first line is description string
	} else {
		for (const Train *v = Train::Get(veh_id); v != nullptr; v = v->GetNextVehicle()) {
			GetCargoSummaryOfArticulatedVehicle(v, _cargo_summary);
			num += std::max(1u, (unsigned)_cargo_summary.size());

			uint length = GetLengthOfArticulatedVehicle(v);
			if (length > (uint)ScaleSpriteTrad(TRAIN_DETAILS_MAX_INDENT)) num++;
		}
	}

	return num;
}

/**
 * Draw the details for the given vehicle at the given position
 *
 * @param v     current vehicle
 * @param r     the Rect to draw within
 * @param vscroll_pos Position of scrollbar
 * @param vscroll_cap Number of lines currently displayed
 * @param det_tab Selected details tab
 */
void DrawTrainDetails(const Train *v, const Rect &r, int vscroll_pos, uint16_t vscroll_cap, TrainDetailsWindowTabs det_tab)
{
	bool rtl = _current_text_dir == TD_RTL;
	int line_height = r.Height();
	int sprite_y_offset = line_height / 2;
	int text_y_offset = (line_height - GetCharacterHeight(FS_NORMAL)) / 2;

	/* draw the first 3 details tabs */
	if (det_tab != TDW_TAB_TOTALS) {
		Direction dir = rtl ? DIR_E : DIR_W;
		int x = rtl ? r.right : r.left;
		for (; v != nullptr && vscroll_pos > -vscroll_cap; v = v->GetNextVehicle()) {
			GetCargoSummaryOfArticulatedVehicle(v, _cargo_summary);

			/* Draw sprites */
			uint dx = 0;
			int px = x;
			const Train *u = v;
			do {
				Point offset;
				int width = u->GetDisplayImageWidth(&offset);
				if (vscroll_pos <= 0 && vscroll_pos > -vscroll_cap) {
					int pitch = 0;
					const Engine *e = Engine::Get(v->engine_type);
					if (e->GetGRF() != nullptr) {
						pitch = ScaleSpriteTrad(e->GetGRF()->traininfo_vehicle_pitch);
					}
					PaletteID pal = v->vehstatus.Test(VehState::Crashed) ? PALETTE_CRASH : GetVehiclePalette(u);
					VehicleSpriteSeq seq;
					u->GetImage(dir, EIT_IN_DETAILS, &seq);
					seq.Draw(px + (rtl ? -offset.x : offset.x), r.top - line_height * vscroll_pos + sprite_y_offset + pitch, pal, v->vehstatus.Test(VehState::Crashed));
				}
				px += rtl ? -width : width;
				dx += width;
				u = u->Next();
			} while (u != nullptr && u->IsArticulatedPart());

			bool separate_sprite_row = (dx > (uint)ScaleSpriteTrad(TRAIN_DETAILS_MAX_INDENT));
			if (separate_sprite_row) {
				vscroll_pos--;
				dx = 0;
			}

			int sprite_width = std::max<int>(dx, ScaleSpriteTrad(TRAIN_DETAILS_MIN_INDENT)) + WidgetDimensions::scaled.hsep_normal;
			Rect dr = r.Indent(sprite_width, rtl);
			uint num_lines = std::max(1u, (unsigned)_cargo_summary.size());
			for (uint i = 0; i < num_lines; i++) {
				if (vscroll_pos <= 0 && vscroll_pos > -vscroll_cap) {
					int py = r.top - line_height * vscroll_pos + text_y_offset;
					if (i > 0 || separate_sprite_row) {
						if (vscroll_pos != 0) GfxFillRect(r.left, py - WidgetDimensions::scaled.matrix.top - 1, r.right, py - WidgetDimensions::scaled.matrix.top, GetColourGradient(COLOUR_GREY, SHADE_LIGHT));
					}
					switch (det_tab) {
						case TDW_TAB_CARGO:
							if (i < _cargo_summary.size()) {
								TrainDetailsCargoTab(&_cargo_summary[i], dr.left, dr.right, py);
							} else {
								DrawString(dr.left, dr.right, py, STR_QUANTITY_N_A, TC_LIGHT_BLUE);
							}
							break;

						case TDW_TAB_INFO:
							if (i == 0) TrainDetailsInfoTab(v, dr.left, dr.right, py);
							break;

						case TDW_TAB_CAPACITY:
							if (i < _cargo_summary.size()) {
								TrainDetailsCapacityTab(&_cargo_summary[i], dr.left, dr.right, py);
							} else {
								DrawString(dr.left, dr.right, py, GetString(STR_VEHICLE_INFO_NO_CAPACITY, STR_EMPTY));
							}
							break;

						default: NOT_REACHED();
					}
				}
				vscroll_pos--;
			}
		}
	} else {
		int y = r.top;
		CargoArray act_cargo{};
		CargoArray max_cargo{};
		Money feeder_share = 0;

		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			act_cargo[u->cargo_type] += u->cargo.StoredCount();
			max_cargo[u->cargo_type] += u->cargo_cap;
			feeder_share             += u->cargo.GetFeederShare();
		}

		/* draw total cargo tab */
		DrawString(r.left, r.right, y + text_y_offset, STR_VEHICLE_DETAILS_TRAIN_TOTAL_CAPACITY_TEXT);
		y += line_height;

		/* Indent the total cargo capacity details */
		Rect ir = r.Indent(WidgetDimensions::scaled.hsep_indent, rtl);
		for (const CargoSpec *cs : _sorted_cargo_specs) {
			CargoType cargo_type = cs->Index();
			if (max_cargo[cargo_type] > 0 && --vscroll_pos < 0 && vscroll_pos > -vscroll_cap) {
				std::string str;
				if (FreightWagonMult(cargo_type) > 1) {
					str = GetString(STR_VEHICLE_DETAILS_TRAIN_TOTAL_CAPACITY_MULT, cargo_type, act_cargo[cargo_type], cargo_type, max_cargo[cargo_type], _settings_game.vehicle.freight_trains);
				} else {
					str = GetString(STR_VEHICLE_DETAILS_TRAIN_TOTAL_CAPACITY, cargo_type, act_cargo[cargo_type], cargo_type, max_cargo[cargo_type]);
				}
				DrawString(ir.left, ir.right, y + text_y_offset, str);
				y += line_height;
			}
		}
		DrawString(r.left, r.right, y + text_y_offset, GetString(STR_VEHICLE_INFO_FEEDER_CARGO_VALUE, feeder_share));
	}
}
