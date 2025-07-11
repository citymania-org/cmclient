/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file autoreplace_gui.cpp GUI for autoreplace handling. */

#include "stdafx.h"
#include "command_func.h"
#include "vehicle_gui.h"
#include "newgrf_badge.h"
#include "newgrf_engine.h"
#include "rail.h"
#include "road.h"
#include "strings_func.h"
#include "window_func.h"
#include "autoreplace_func.h"
#include "company_func.h"
#include "engine_base.h"
#include "window_gui.h"
#include "engine_gui.h"
#include "settings_func.h"
#include "core/geometry_func.hpp"
#include "rail_gui.h"
#include "road_gui.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "autoreplace_cmd.h"
#include "group_cmd.h"
#include "settings_cmd.h"

#include "widgets/autoreplace_widget.h"

#include "table/strings.h"

#include "citymania/cm_hotkeys.hpp"

#include "safeguards.h"

static bool EngineNumberSorter(const GUIEngineListItem &a, const GUIEngineListItem &b)
{
	return Engine::Get(a.engine_id)->list_position < Engine::Get(b.engine_id)->list_position;
}

/**
 * Rebuild the left autoreplace list if an engine is removed or added
 * @param e Engine to check if it is removed or added
 * @param id_g The group the engine belongs to
 *  Note: this function only works if it is called either
 *   - when a new vehicle is build, but before it's counted in num_engines
 *   - when a vehicle is deleted and after it's subtracted from num_engines
 *   - when not changing the count (used when changing replace orders)
 */
void InvalidateAutoreplaceWindow(EngineID e, GroupID id_g)
{
	if (GetGroupNumEngines(_local_company, id_g, e) == 0 || GetGroupNumEngines(_local_company, ALL_GROUP, e) == 0) {
		/* We don't have any of this engine type.
		 * Either we just sold the last one, we build a new one or we stopped replacing it.
		 * In all cases, we need to update the left list */
		InvalidateWindowData(WC_REPLACE_VEHICLE, Engine::Get(e)->type, 1);
	}
}

/**
 * When an engine is made buildable or is removed from being buildable, add/remove it from the build/autoreplace lists
 * @param type The type of engine
 */
void AddRemoveEngineFromAutoreplaceAndBuildWindows(VehicleType type)
{
	InvalidateWindowData(WC_REPLACE_VEHICLE, type, 0); // Update the autoreplace window
	InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
}

static const StringID _start_replace_dropdown[] = {
	STR_REPLACE_VEHICLES_NOW,
	STR_REPLACE_VEHICLES_WHEN_OLD,
};

/**
 * Window for the autoreplacing of vehicles.
 */
class ReplaceVehicleWindow : public Window {
	std::array<EngineID, 2> sel_engine{}; ///< Selected engine left and right.
	std::array<GUIEngineList, 2> engines{}; ///< Left and right list of engines.
	bool replace_engines = true; ///< If \c true, engines are replaced, if \c false, wagons are replaced (only for trains).
	bool reset_sel_engine = true; ///< Also reset #sel_engine while updating left and/or right and no valid engine selected.
	GroupID sel_group = GroupID::Invalid(); ///< Group selected to replace.
	int details_height = 0; ///< Minimal needed height of the details panels, in text lines (found so far).
	VehicleType vehicle_type = VEH_INVALID; ///< Type of vehicle in this window.
	uint8_t sort_criteria = 0; ///< Criteria of sorting vehicles.
	bool descending_sort_order = false; ///< Order of sorting vehicles.
	bool show_hidden_engines = false; ///< Whether to show the hidden engines.
	RailType sel_railtype = INVALID_RAILTYPE; ///< Type of rail tracks selected. #INVALID_RAILTYPE to show all.
	RoadType sel_roadtype = INVALID_ROADTYPE; ///< Type of road selected. #INVALID_ROADTYPE to show all.
	std::array<Scrollbar *, 2> vscroll{};
	GUIBadgeClasses badge_classes{};

	uint cm_num_hidden_engines;

	/**
	 * Figure out if an engine should be added to a list.
	 * @param e            The EngineID.
	 * @param draw_left    If \c true, the left list is drawn (the engines specific to the railtype you selected).
	 * @param show_engines If \c true, the locomotives are drawn, else the wagons are drawn (never both).
	 * @return \c true if the engine should be in the list (based on this check), else \c false.
	 */
	bool GenerateReplaceRailList(EngineID e, bool draw_left, bool show_engines)
	{
		const RailVehicleInfo *rvi = RailVehInfo(e);

		/* Ensure that the wagon/engine selection fits the engine. */
		if ((rvi->railveh_type == RAILVEH_WAGON) == show_engines) return false;

		if (draw_left && this->sel_railtype != INVALID_RAILTYPE) {
			/* Ensure that the railtype is specific to the selected one */
			if (rvi->railtype != this->sel_railtype) return false;
		}
		return true;
	}

	/**
	 * Generate an engines list
	 * @param draw_left true if generating the left list, otherwise false
	 */
	void GenerateReplaceVehList(bool draw_left)
	{
		std::vector<EngineID> variants;
		EngineID selected_engine = EngineID::Invalid();
		VehicleType type = this->window_number;
		uint8_t side = draw_left ? 0 : 1;

		GUIEngineList list;
		this->cm_num_hidden_engines = 0;

		for (const Engine *e : Engine::IterateType(type)) {
			// CM num_hidden if (!draw_left && !this->show_hidden_engines && e->IsVariantHidden(_local_company)) continue;
			EngineID eid = e->index;
			switch (type) {
				case VEH_TRAIN:
					if (!this->GenerateReplaceRailList(eid, draw_left, this->replace_engines)) continue; // special rules for trains
					break;

				case VEH_ROAD:
					if (draw_left && this->sel_roadtype != INVALID_ROADTYPE) {
						/* Ensure that the roadtype is specific to the selected one */
						if (e->u.road.roadtype != this->sel_roadtype) continue;
					}
					break;

				default:
					break;
			}

			if (draw_left) {
				const uint num_engines = GetGroupNumEngines(_local_company, this->sel_group, eid);

				/* Skip drawing the engines we don't have any of and haven't set for replacement */
				if (num_engines == 0 && EngineReplacementForCompany(Company::Get(_local_company), eid, this->sel_group) == EngineID::Invalid()) continue;
			} else {
				if (!CheckAutoreplaceValidity(this->sel_engine[0], eid, _local_company)) continue;
				if (e->IsVariantHidden(_local_company)) {
					this->cm_num_hidden_engines++;
					if (!this->show_hidden_engines) continue;
				}
			}

			list.emplace_back(eid, e->info.variant_id, (side == 0) ? EngineDisplayFlags{} : e->display_flags, 0);

			if (side == 1) {
				EngineID parent = e->info.variant_id;
				while (parent != EngineID::Invalid()) {
					variants.push_back(parent);
					parent = Engine::Get(parent)->info.variant_id;
				}
			}
			if (eid == this->sel_engine[side]) selected_engine = eid; // The selected engine is still in the list
		}

		if (side == 1) {
			/* ensure primary engine of variant group is in list */
			for (const auto &variant : variants) {
				if (std::ranges::find(list, variant, &GUIEngineListItem::engine_id) == list.end()) {
					const Engine *e = Engine::Get(variant);
					list.emplace_back(variant, e->info.variant_id, e->display_flags | EngineDisplayFlag::Shaded, 0);
				}
			}
		}

		this->sel_engine[side] = selected_engine; // update which engine we selected (the same or none, if it's not in the list anymore)
		if (draw_left) {
			EngList_Sort(list, &EngineNumberSorter);
		} else {
			_engine_sort_direction = this->descending_sort_order;
			EngList_Sort(list, _engine_sort_functions[this->window_number][this->sort_criteria]);
		}

		this->engines[side].clear();
		if (side == 1) {
			GUIEngineListAddChildren(this->engines[side], list);
		} else {
			this->engines[side].swap(list);
		}
	}

	/** Generate the lists */
	void GenerateLists()
	{
		EngineID e = this->sel_engine[0];

		if (this->engines[0].NeedRebuild()) {
			/* We need to rebuild the left engines list */
			this->GenerateReplaceVehList(true);
			this->vscroll[0]->SetCount(this->engines[0].size());
			if (this->reset_sel_engine && this->sel_engine[0] == EngineID::Invalid() && !this->engines[0].empty()) {
				this->sel_engine[0] = this->engines[0][0].engine_id;
			}
		}

		if (this->engines[1].NeedRebuild() || e != this->sel_engine[0]) {
			/* Either we got a request to rebuild the right engines list, or the left engines list selected a different engine */
			if (this->sel_engine[0] == EngineID::Invalid()) {
				/* Always empty the right engines list when nothing is selected in the left engines list */
				this->engines[1].clear();
				this->sel_engine[1] = EngineID::Invalid();
				this->vscroll[1]->SetCount(this->engines[1].size());
			} else {
				if (this->reset_sel_engine && this->sel_engine[0] != EngineID::Invalid()) {
					/* Select the current replacement for sel_engine[0]. */
					const Company *c = Company::Get(_local_company);
					this->sel_engine[1] = EngineReplacementForCompany(c, this->sel_engine[0], this->sel_group);
				}
				/* Regenerate the list on the right. Note: This resets sel_engine[1] to EngineID::Invalid(), if it is no longer available. */
				this->GenerateReplaceVehList(false);
				this->vscroll[1]->SetCount(this->engines[1].size());
				if (this->reset_sel_engine && this->sel_engine[1] != EngineID::Invalid()) {
					int position = 0;
					for (const auto &item : this->engines[1]) {
						if (item.engine_id == this->sel_engine[1]) break;
						++position;
					}
					this->vscroll[1]->ScrollTowards(position);
				}
			}
		}
		/* Reset the flags about needed updates */
		this->engines[0].RebuildDone();
		this->engines[1].RebuildDone();
		this->reset_sel_engine = false;
	}

	/**
	 * Handle click on the start replace button.
	 * @param replace_when_old Replace now or only when old?
	 */
	void ReplaceClick_StartReplace(bool replace_when_old)
	{
		EngineID veh_from = this->sel_engine[0];
		EngineID veh_to = this->sel_engine[1];
		Command<CMD_SET_AUTOREPLACE>::Post(this->sel_group, veh_from, veh_to, replace_when_old);
	}

	/**
	 * Perform tasks after rail or road type is changed.
	 */
	void OnRailRoadTypeChange()
	{
		/* Reset scrollbar positions */
		this->vscroll[0]->SetPosition(0);
		this->vscroll[1]->SetPosition(0);
		/* Rebuild the lists */
		this->engines[0].ForceRebuild();
		this->engines[1].ForceRebuild();
		this->reset_sel_engine = true;
		this->SetDirty();
	}

public:
	ReplaceVehicleWindow(WindowDesc &desc, VehicleType vehicletype, GroupID id_g) : Window(desc)
	{
		this->vehicle_type = vehicletype;
		this->engines[0].ForceRebuild();
		this->engines[1].ForceRebuild();
		this->details_height   = ((vehicletype == VEH_TRAIN) ? 10 : 9);
		this->sel_engine[0] = EngineID::Invalid();
		this->sel_engine[1] = EngineID::Invalid();
		this->show_hidden_engines = _engine_sort_show_hidden_engines[vehicletype];

		this->CreateNestedTree();
		this->vscroll[0] = this->GetScrollbar(WID_RV_LEFT_SCROLLBAR);
		this->vscroll[1] = this->GetScrollbar(WID_RV_RIGHT_SCROLLBAR);

		NWidgetCore *widget = this->GetWidget<NWidgetCore>(WID_RV_SHOW_HIDDEN_ENGINES);
		widget->SetStringTip(CM_STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN + vehicletype, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP + vehicletype);
		widget->SetLowered(this->show_hidden_engines);
		this->FinishInitNested(vehicletype);

		this->sort_criteria = _engine_sort_last_criteria[vehicletype];
		this->descending_sort_order = _engine_sort_last_order[vehicletype];
		this->owner = _local_company;
		this->sel_group = id_g;
	}

	void OnInit() override
	{
		this->badge_classes = GUIBadgeClasses(static_cast<GrfSpecFeature>(GSF_TRAINS + this->vehicle_type));
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_RV_SORT_ASCENDING_DESCENDING: {
				Dimension d = GetStringBoundingBox(this->GetWidget<NWidgetCore>(widget)->GetString());
				d.width += padding.width + Window::SortButtonWidth() * 2; // Doubled since the string is centred and it also looks better.
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_RV_LEFT_MATRIX:
			case WID_RV_RIGHT_MATRIX:
				resize.height = GetEngineListHeight(this->window_number);
				size.height = (this->window_number <= VEH_ROAD ? 8 : 4) * resize.height;
				break;

			case WID_RV_LEFT_DETAILS:
			case WID_RV_RIGHT_DETAILS:
				size.height = GetCharacterHeight(FS_NORMAL) * this->details_height + padding.height;
				break;

			case WID_RV_TRAIN_WAGONREMOVE_TOGGLE: {
				StringID str = this->GetWidget<NWidgetCore>(widget)->GetString();
				StringID group_str = Group::IsValidID(this->sel_group) ? STR_GROUP_NAME : STR_GROUP_DEFAULT_TRAINS + this->window_number;
				Dimension d = GetStringBoundingBox(GetString(str, group_str, this->sel_group, STR_CONFIG_SETTING_ON));
				d = maxdim(d, GetStringBoundingBox(GetString(str, group_str, this->sel_group, STR_CONFIG_SETTING_OFF)));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN: {
				Dimension d = GetStringBoundingBox(STR_REPLACE_ENGINES);
				d = maxdim(d, GetStringBoundingBox(STR_REPLACE_WAGONS));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_RV_INFO_TAB: {
				Dimension d = GetStringBoundingBox(STR_REPLACE_NOT_REPLACING);
				d = maxdim(d, GetStringBoundingBox(STR_REPLACE_NOT_REPLACING_VEHICLE_SELECTED));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_RV_RAIL_TYPE_DROPDOWN: {
				Dimension d = {0, 0};
				for (const RailType &rt : _sorted_railtypes) {
					d = maxdim(d, GetStringBoundingBox(GetRailTypeInfo(rt)->strings.replace_text));
				}
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_RV_ROAD_TYPE_DROPDOWN: {
				Dimension d = {0, 0};
				for (const RoadType &rt : _sorted_roadtypes) {
					d = maxdim(d, GetStringBoundingBox(GetRoadTypeInfo(rt)->strings.replace_text));
				}
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_RV_START_REPLACE: {
				Dimension d = GetStringBoundingBox(STR_REPLACE_VEHICLES_START);
				d = maxdim(d, GetStringListBoundingBox(_start_replace_dropdown));
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_RV_CAPTION:
				switch (this->sel_group.base()) {
					case ALL_GROUP.base():
						return GetString(STR_REPLACE_VEHICLES_WHITE, STR_REPLACE_VEHICLE_TRAIN + this->window_number, STR_GROUP_ALL_TRAINS + this->window_number, std::monostate{});
						break;

					case DEFAULT_GROUP.base():
						return GetString(STR_REPLACE_VEHICLES_WHITE, STR_REPLACE_VEHICLE_TRAIN + this->window_number, STR_GROUP_DEFAULT_TRAINS + this->window_number, std::monostate{});
						break;

					default:
						return GetString(STR_REPLACE_VEHICLES_WHITE, STR_REPLACE_VEHICLE_TRAIN + this->window_number, STR_GROUP_NAME, sel_group);
				}
				break;

			case WID_RV_SORT_DROPDOWN:
				return GetString(std::data(_engine_sort_listing[this->window_number])[this->sort_criteria]);

			case WID_RV_TRAIN_WAGONREMOVE_TOGGLE:
				if (const Group *g = Group::GetIfValid(this->sel_group); g != nullptr) {
					bool remove_wagon = g->flags.Test(GroupFlag::ReplaceWagonRemoval);
					return GetString(STR_GROUP_NAME, sel_group, remove_wagon ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
				} else {
					const Company *c = Company::Get(_local_company);
					bool remove_wagon = c->settings.renew_keep_length;
					return GetString(STR_GROUP_DEFAULT_TRAINS + this->window_number, std::monostate{}, remove_wagon ? STR_CONFIG_SETTING_ON : STR_CONFIG_SETTING_OFF);
				}
				break;

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN:
				return GetString(this->replace_engines ? STR_REPLACE_ENGINES : STR_REPLACE_WAGONS);

			case WID_RV_RAIL_TYPE_DROPDOWN:
				return GetString(this->sel_railtype == INVALID_RAILTYPE ? STR_REPLACE_ALL_RAILTYPE : GetRailTypeInfo(this->sel_railtype)->strings.replace_text);

			case WID_RV_ROAD_TYPE_DROPDOWN:
				return GetString(this->sel_roadtype == INVALID_ROADTYPE ? STR_REPLACE_ALL_ROADTYPE : GetRoadTypeInfo(this->sel_roadtype)->strings.replace_text);

			case WID_RV_SHOW_HIDDEN_ENGINES:
				return GetString(CM_STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN + this->vehicle_type, this->cm_num_hidden_engines);

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		switch (widget) {
			case WID_RV_SORT_ASCENDING_DESCENDING:
				this->DrawSortButtonState(WID_RV_SORT_ASCENDING_DESCENDING, this->descending_sort_order ? SBS_DOWN : SBS_UP);
				break;

			case WID_RV_INFO_TAB: {
				const Company *c = Company::Get(_local_company);
				std::string str;
				if (this->sel_engine[0] != EngineID::Invalid()) {
					if (!EngineHasReplacementForCompany(c, this->sel_engine[0], this->sel_group)) {
						str = GetString(STR_REPLACE_NOT_REPLACING);
					} else {
						bool when_old = false;
						EngineID e = EngineReplacementForCompany(c, this->sel_engine[0], this->sel_group, &when_old);
						str = GetString(when_old ? STR_REPLACE_REPLACING_WHEN_OLD : STR_ENGINE_NAME, PackEngineNameDParam(e, EngineNameContext::PurchaseList));
					}
				} else {
					str = GetString(STR_REPLACE_NOT_REPLACING_VEHICLE_SELECTED);
				}

				DrawString(r.Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect), std::move(str), TC_BLACK, SA_HOR_CENTER);
				break;
			}

			case WID_RV_LEFT_MATRIX:
			case WID_RV_RIGHT_MATRIX: {
				int side = (widget == WID_RV_LEFT_MATRIX) ? 0 : 1;

				/* Do the actual drawing */
				DrawEngineList(this->window_number, r, this->engines[side], *this->vscroll[side], this->sel_engine[side], side == 0, this->sel_group, this->badge_classes);
				break;
			}
		}
	}

	void OnPaint() override
	{
		/* CityMania code start */
		//if (this->engines[0].NeedRebuild() || this->engines[1].NeedRebuild()) this->GenerateLists();
		if (this->engines[0].NeedRebuild() || this->engines[1].NeedRebuild()) {
			auto old_num_hidden_engines = this->cm_num_hidden_engines;
			this->GenerateLists();
			if (old_num_hidden_engines != this->cm_num_hidden_engines) {
				this->ReInit();
				return;
			}
		}
		/* CityMania code end */

		Company *c = Company::Get(_local_company);

		/* Disable the "Start Replacing" button if:
		 *    Either engines list is empty
		 * or The selected replacement engine has a replacement (to prevent loops). */
		this->SetWidgetDisabledState(WID_RV_START_REPLACE,
				this->sel_engine[0] == EngineID::Invalid() || this->sel_engine[1] == EngineID::Invalid() || EngineReplacementForCompany(c, this->sel_engine[1], this->sel_group) != EngineID::Invalid());

		/* Disable the "Stop Replacing" button if:
		 *   The left engines list (existing vehicle) is empty
		 *   or The selected vehicle has no replacement set up */
		this->SetWidgetDisabledState(WID_RV_STOP_REPLACE, this->sel_engine[0] == EngineID::Invalid() || !EngineHasReplacementForCompany(c, this->sel_engine[0], this->sel_group));

		this->DrawWidgets();

		if (!this->IsShaded()) {
			int needed_height = this->details_height;
			/* Draw details panels. */
			for (int side = 0; side < 2; side++) {
				if (this->sel_engine[side] != EngineID::Invalid()) {
					/* Use default engine details without refitting */
					const Engine *e = Engine::Get(this->sel_engine[side]);
					TestedEngineDetails ted;
					ted.cost = 0;
					ted.FillDefaultCapacities(e);

					const Rect r = this->GetWidget<NWidgetBase>(side == 0 ? WID_RV_LEFT_DETAILS : WID_RV_RIGHT_DETAILS)->GetCurrentRect()
							.Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect);
					int text_end = DrawVehiclePurchaseInfo(r.left, r.right, r.top, this->sel_engine[side], ted);
					needed_height = std::max(needed_height, (text_end - r.top) / GetCharacterHeight(FS_NORMAL));
				}
			}
			if (needed_height != this->details_height) { // Details window are not high enough, enlarge them.
				this->details_height = needed_height;
				this->ReInit();
				return;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_RV_SORT_ASCENDING_DESCENDING:
				this->descending_sort_order ^= true;
				_engine_sort_last_order[this->window_number] = this->descending_sort_order;
				this->engines[1].ForceRebuild();
				this->SetDirty();
				break;

			case WID_RV_SHOW_HIDDEN_ENGINES:
				this->show_hidden_engines ^= true;
				_engine_sort_show_hidden_engines[this->window_number] = this->show_hidden_engines;
				this->engines[1].ForceRebuild();
				this->SetWidgetLoweredState(widget, this->show_hidden_engines);
				this->SetDirty();
				break;

			case WID_RV_SORT_DROPDOWN:
				DisplayVehicleSortDropDown(this, this->window_number, this->sort_criteria, WID_RV_SORT_DROPDOWN);
				break;

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN: {
				DropDownList list;
				list.push_back(MakeDropDownListStringItem(STR_REPLACE_ENGINES, 1));
				list.push_back(MakeDropDownListStringItem(STR_REPLACE_WAGONS, 0));
				ShowDropDownList(this, std::move(list), this->replace_engines ? 1 : 0, WID_RV_TRAIN_ENGINEWAGON_DROPDOWN);
				break;
			}

			case WID_RV_RAIL_TYPE_DROPDOWN: // Railtype selection dropdown menu
				ShowDropDownList(this, GetRailTypeDropDownList(true, true), this->sel_railtype, widget);
				break;

			case WID_RV_ROAD_TYPE_DROPDOWN: // Roadtype selection dropdown menu
				ShowDropDownList(this, GetRoadTypeDropDownList(RTTB_ROAD | RTTB_TRAM, true, true), this->sel_roadtype, widget);
				break;

			case WID_RV_TRAIN_WAGONREMOVE_TOGGLE: {
				const Group *g = Group::GetIfValid(this->sel_group);
				if (g != nullptr) {
					Command<CMD_SET_GROUP_FLAG>::Post(this->sel_group, GroupFlag::ReplaceWagonRemoval, !g->flags.Test(GroupFlag::ReplaceWagonRemoval), citymania::_fn_mod);
				} else {
					// toggle renew_keep_length
					Command<CMD_CHANGE_COMPANY_SETTING>::Post("company.renew_keep_length", Company::Get(_local_company)->settings.renew_keep_length ? 0 : 1);
				}
				break;
			}

			case WID_RV_START_REPLACE: { // Start replacing
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->HandleButtonClick(WID_RV_START_REPLACE);
					ReplaceClick_StartReplace(false);
				} else {
					bool replacment_when_old = EngineHasReplacementWhenOldForCompany(Company::Get(_local_company), this->sel_engine[0], this->sel_group);
					ShowDropDownMenu(this, _start_replace_dropdown, replacment_when_old ? 1 : 0, WID_RV_START_REPLACE, !this->replace_engines ? 1 << 1 : 0, 0);
				}
				break;
			}

			case WID_RV_STOP_REPLACE: { // Stop replacing
				EngineID veh_from = this->sel_engine[0];
				Command<CMD_SET_AUTOREPLACE>::Post(this->sel_group, veh_from, EngineID::Invalid(), false);
				break;
			}

			case WID_RV_LEFT_MATRIX:
			case WID_RV_RIGHT_MATRIX: {
				uint8_t click_side;
				if (widget == WID_RV_LEFT_MATRIX) {
					click_side = 0;
				} else {
					click_side = 1;
				}

				EngineID e = EngineID::Invalid();
				const auto it = this->vscroll[click_side]->GetScrolledItemFromWidget(this->engines[click_side], pt.y, this, widget);
				if (it != this->engines[click_side].end()) {
					const auto &item = *it;
					const Rect r = this->GetWidget<NWidgetBase>(widget)->GetCurrentRect().Shrink(WidgetDimensions::scaled.matrix).WithWidth(WidgetDimensions::scaled.hsep_indent * (item.indent + 1), _current_text_dir == TD_RTL);
					if (item.flags.Test(EngineDisplayFlag::HasVariants) && IsInsideMM(r.left, r.right, pt.x)) {
						/* toggle folded flag on engine */
						assert(item.variant_id != EngineID::Invalid());
						Engine *engine = Engine::Get(item.variant_id);
						engine->display_flags.Flip(EngineDisplayFlag::IsFolded);

						InvalidateWindowData(WC_REPLACE_VEHICLE, this->window_number, 0); // Update the autoreplace window
						InvalidateWindowClassesData(WC_BUILD_VEHICLE); // The build windows needs updating as well
						return;
					}
					if (!item.flags.Test(EngineDisplayFlag::Shaded)) e = item.engine_id;
				}

				/* If Ctrl is pressed on the left side and we don't have any engines of the selected type, stop autoreplacing.
				 * This is most common when we have finished autoreplacing the engine and want to remove it from the list. */
				if (click_side == 0 && citymania::_fn_mod && e != EngineID::Invalid() &&
					(GetGroupNumEngines(_local_company, sel_group, e) == 0 || GetGroupNumEngines(_local_company, ALL_GROUP, e) == 0)) {
						EngineID veh_from = e;
						Command<CMD_SET_AUTOREPLACE>::Post(this->sel_group, veh_from, EngineID::Invalid(), false);
						break;
				}

				if (e == this->sel_engine[click_side]) break; // we clicked the one we already selected
				this->sel_engine[click_side] = e;
				if (click_side == 0) {
					this->engines[1].ForceRebuild();
					this->reset_sel_engine = true;
				}
				this->SetDirty();
				break;
			}
		}
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_RV_SORT_DROPDOWN:
				if (this->sort_criteria != index) {
					this->sort_criteria = index;
					_engine_sort_last_criteria[this->window_number] = this->sort_criteria;
					this->engines[1].ForceRebuild();
					this->SetDirty();
				}
				break;

			case WID_RV_RAIL_TYPE_DROPDOWN: {
				RailType temp = (RailType)index;
				if (temp == this->sel_railtype) return; // we didn't select a new one. No need to change anything
				this->sel_railtype = temp;
				this->OnRailRoadTypeChange();
				break;
			}

			case WID_RV_ROAD_TYPE_DROPDOWN: {
				RoadType temp = (RoadType)index;
				if (temp == this->sel_roadtype) return; // we didn't select a new one. No need to change anything
				this->sel_roadtype = temp;
				this->OnRailRoadTypeChange();
				break;
			}

			case WID_RV_TRAIN_ENGINEWAGON_DROPDOWN: {
				this->replace_engines = index != 0;
				this->engines[0].ForceRebuild();
				this->reset_sel_engine = true;
				this->SetDirty();
				break;
			}

			case WID_RV_START_REPLACE:
				this->ReplaceClick_StartReplace(index != 0);
				break;
		}
	}

	bool OnTooltip([[maybe_unused]] Point pt, WidgetID widget, TooltipCloseCondition close_cond) override
	{
		if (widget != WID_RV_TRAIN_WAGONREMOVE_TOGGLE) return false;

		if (Group::IsValidID(this->sel_group)) {
			GuiShowTooltips(this, GetEncodedString(STR_REPLACE_REMOVE_WAGON_GROUP_HELP, STR_REPLACE_REMOVE_WAGON_TOOLTIP), close_cond);
		} else {
			GuiShowTooltips(this, GetEncodedString(STR_REPLACE_REMOVE_WAGON_TOOLTIP), close_cond);
		}
		return true;
	}

	void OnResize() override
	{
		this->vscroll[0]->SetCapacityFromWidget(this, WID_RV_LEFT_MATRIX);
		this->vscroll[1]->SetCapacityFromWidget(this, WID_RV_RIGHT_MATRIX);
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		if (data != 0) {
			/* This needs to be done in command-scope to enforce rebuilding before resorting invalid data */
			this->engines[0].ForceRebuild();
		} else {
			this->engines[1].ForceRebuild();
		}
	}
};

static constexpr NWidgetPart _nested_replace_rail_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_RV_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_REPLACE_VEHICLE_VEHICLES_IN_USE, STR_REPLACE_VEHICLE_VEHICLES_IN_USE_TOOLTIP), SetFill(1, 1), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES, STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES_TOOLTIP), SetFill(1, 1), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_RAIL_TYPE_DROPDOWN), SetMinimalSize(136, 12), SetToolTip(STR_REPLACE_RAILTYPE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_TRAIN_ENGINEWAGON_DROPDOWN), SetToolTip(STR_REPLACE_ENGINE_WAGON_SELECT_TOOLTIP),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER), SetFill(1, 1),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 1), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_RV_SHOW_HIDDEN_ENGINES), SetToolTip(STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_LEFT_ARRAY_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_RV_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_RIGHT_ARRAY_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_RV_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_LEFT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_RIGHT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_TRAIN_WAGONREMOVE_TOGGLE), SetMinimalSize(138, 12), SetStringTip(STR_REPLACE_REMOVE_WAGON, STR_REPLACE_REMOVE_WAGON_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_PUSHBUTTON_DROPDOWN, COLOUR_GREY, WID_RV_START_REPLACE), SetMinimalSize(139, 12), SetStringTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_START_BUTTON_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_INFO_TAB), SetMinimalSize(167, 12), SetToolTip(STR_REPLACE_REPLACE_INFO_TAB_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_STOP_REPLACE), SetMinimalSize(150, 12), SetStringTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_STOP_BUTTON_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _replace_rail_vehicle_desc(
	WDP_AUTO, "replace_vehicle_train", 500, 140,
	WC_REPLACE_VEHICLE, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_replace_rail_vehicle_widgets
);

static constexpr NWidgetPart _nested_replace_road_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_RV_CAPTION),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_REPLACE_VEHICLE_VEHICLES_IN_USE, STR_REPLACE_VEHICLE_VEHICLES_IN_USE_TOOLTIP), SetFill(1, 1), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES, STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES_TOOLTIP), SetFill(1, 1), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_ROAD_TYPE_DROPDOWN), SetMinimalSize(136, 12), SetToolTip(STR_REPLACE_ROADTYPE_TOOLTIP), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER), SetFill(1, 1),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 1), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_RV_SHOW_HIDDEN_ENGINES), SetStringTip(CM_STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_LEFT_ARRAY_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_RV_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_RIGHT_ARRAY_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_RV_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_LEFT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_RIGHT_DETAILS), SetMinimalSize(240, 122), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_PUSHBUTTON_DROPDOWN, COLOUR_GREY, WID_RV_START_REPLACE), SetMinimalSize(139, 12), SetStringTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_START_BUTTON_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_INFO_TAB), SetMinimalSize(167, 12), SetToolTip(STR_REPLACE_REPLACE_INFO_TAB_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_STOP_REPLACE), SetMinimalSize(150, 12), SetStringTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_STOP_BUTTON_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _replace_road_vehicle_desc(
	WDP_AUTO, "replace_vehicle_road", 500, 140,
	WC_REPLACE_VEHICLE, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_replace_road_vehicle_widgets
);

static constexpr NWidgetPart _nested_replace_vehicle_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_RV_CAPTION), SetMinimalSize(433, 14),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_REPLACE_VEHICLE_VEHICLES_IN_USE, STR_REPLACE_VEHICLE_VEHICLES_IN_USE_TOOLTIP), SetFill(1, 1), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(WWT_LABEL, INVALID_COLOUR), SetStringTip(STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES, STR_REPLACE_VEHICLE_AVAILABLE_VEHICLES_TOOLTIP), SetFill(1, 1), SetMinimalTextLines(1, WidgetDimensions::unscaled.framerect.Vertical()), SetResize(1, 0),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_SORT_ASCENDING_DESCENDING), SetStringTip(STR_BUTTON_SORT_BY, STR_TOOLTIP_SORT_ORDER),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_RV_SORT_DROPDOWN), SetResize(1, 0), SetFill(1, 1), SetToolTip(STR_TOOLTIP_SORT_CRITERIA),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_TEXTBTN, COLOUR_GREY, WID_RV_SHOW_HIDDEN_ENGINES), SetStringTip(CM_STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN, STR_SHOW_HIDDEN_ENGINES_VEHICLE_TRAIN_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0), SetFill(1, 1), EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_LEFT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_LEFT_ARRAY_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_RV_LEFT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_LEFT_SCROLLBAR),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_RV_RIGHT_MATRIX), SetMinimalSize(216, 0), SetFill(1, 1), SetMatrixDataTip(1, 0, STR_REPLACE_RIGHT_ARRAY_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_RV_RIGHT_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_RV_RIGHT_SCROLLBAR),
	EndContainer(),
	NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_LEFT_DETAILS), SetMinimalSize(228, 92), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_RIGHT_DETAILS), SetMinimalSize(228, 92), SetResize(1, 0), EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_PUSHBUTTON_DROPDOWN, COLOUR_GREY, WID_RV_START_REPLACE), SetMinimalSize(139, 12), SetStringTip(STR_REPLACE_VEHICLES_START, STR_REPLACE_START_BUTTON_TOOLTIP),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_RV_INFO_TAB), SetMinimalSize(167, 12), SetToolTip(STR_REPLACE_REPLACE_INFO_TAB_TOOLTIP), SetResize(1, 0), EndContainer(),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_RV_STOP_REPLACE), SetMinimalSize(138, 12), SetStringTip(STR_REPLACE_VEHICLES_STOP, STR_REPLACE_STOP_BUTTON_TOOLTIP),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _replace_vehicle_desc(
	WDP_AUTO, "replace_vehicle", 456, 118,
	WC_REPLACE_VEHICLE, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_replace_vehicle_widgets
);

/**
 * Show the autoreplace configuration window for a particular group.
 * @param id_g The group to replace the vehicles for.
 * @param vehicletype The type of vehicles in the group.
 */
void ShowReplaceGroupVehicleWindow(GroupID id_g, VehicleType vehicletype)
{
	CloseWindowById(WC_REPLACE_VEHICLE, vehicletype);
	switch (vehicletype) {
		case VEH_TRAIN: new ReplaceVehicleWindow(_replace_rail_vehicle_desc, vehicletype, id_g); break;
		case VEH_ROAD:  new ReplaceVehicleWindow(_replace_road_vehicle_desc, vehicletype, id_g); break;
		default:        new ReplaceVehicleWindow(_replace_vehicle_desc, vehicletype, id_g);      break;
	}
}
