/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file order_gui.cpp GUI related to orders. */

#include "stdafx.h"
#include "command_func.h"
#include "viewport_func.h"
#include "depot_map.h"
#include "roadveh.h"
#include "timetable.h"
#include "strings_func.h"
#include "company_func.h"
#include "dropdown_type.h"
#include "dropdown_func.h"
#include "textbuf_gui.h"
#include "string_func.h"
#include "tilehighlight_func.h"
#include "network/network.h"
#include "station_base.h"
#include "industry.h"
#include "waypoint_base.h"
#include "core/geometry_func.hpp"
#include "hotkeys.h"
#include "aircraft.h"
#include "engine_func.h"
#include "vehicle_func.h"
#include "vehiclelist.h"
#include "vehicle_func.h"
#include "error.h"
#include "order_cmd.h"
#include "company_cmd.h"

#include "widgets/order_widget.h"

#include "table/strings.h"

#include "citymania/cm_commands.hpp"
#include "citymania/cm_hotkeys.hpp"

#include "safeguards.h"


/** Order load types that could be given to station orders. */
static const StringID _station_load_types[][5][5] = {
	{
		/* No refitting. */
		{
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			STR_ORDER_FULL_LOAD,
			STR_ORDER_FULL_LOAD_ANY,
			STR_ORDER_NO_LOAD,
		}, {
			STR_ORDER_UNLOAD,
			INVALID_STRING_ID,
			STR_ORDER_UNLOAD_FULL_LOAD,
			STR_ORDER_UNLOAD_FULL_LOAD_ANY,
			STR_ORDER_UNLOAD_NO_LOAD,
		}, {
			STR_ORDER_TRANSFER,
			INVALID_STRING_ID,
			STR_ORDER_TRANSFER_FULL_LOAD,
			STR_ORDER_TRANSFER_FULL_LOAD_ANY,
			STR_ORDER_TRANSFER_NO_LOAD,
		}, {
			/* Unload and transfer do not work together. */
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			INVALID_STRING_ID,
		}, {
			STR_ORDER_NO_UNLOAD,
			INVALID_STRING_ID,
			STR_ORDER_NO_UNLOAD_FULL_LOAD,
			STR_ORDER_NO_UNLOAD_FULL_LOAD_ANY,
			STR_ORDER_NO_UNLOAD_NO_LOAD,
		}
	}, {
		/* With auto-refitting. No loading and auto-refitting do not work together. */
		{
			STR_ORDER_AUTO_REFIT,
			INVALID_STRING_ID,
			STR_ORDER_FULL_LOAD_REFIT,
			STR_ORDER_FULL_LOAD_ANY_REFIT,
			INVALID_STRING_ID,
		}, {
			STR_ORDER_UNLOAD_REFIT,
			INVALID_STRING_ID,
			STR_ORDER_UNLOAD_FULL_LOAD_REFIT,
			STR_ORDER_UNLOAD_FULL_LOAD_ANY_REFIT,
			INVALID_STRING_ID,
		}, {
			STR_ORDER_TRANSFER_REFIT,
			INVALID_STRING_ID,
			STR_ORDER_TRANSFER_FULL_LOAD_REFIT,
			STR_ORDER_TRANSFER_FULL_LOAD_ANY_REFIT,
			INVALID_STRING_ID,
		}, {
			/* Unload and transfer do not work together. */
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			INVALID_STRING_ID,
			INVALID_STRING_ID,
		}, {
			STR_ORDER_NO_UNLOAD_REFIT,
			INVALID_STRING_ID,
			STR_ORDER_NO_UNLOAD_FULL_LOAD_REFIT,
			STR_ORDER_NO_UNLOAD_FULL_LOAD_ANY_REFIT,
			INVALID_STRING_ID,
		}
	}
};

static const StringID _order_non_stop_drowdown[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_NON_STOP_TO,
	STR_ORDER_GO_VIA,
	STR_ORDER_GO_NON_STOP_VIA,
};

static const StringID _order_full_load_drowdown[] = {
	STR_ORDER_DROP_LOAD_IF_POSSIBLE,
	STR_EMPTY,
	STR_ORDER_DROP_FULL_LOAD_ALL,
	STR_ORDER_DROP_FULL_LOAD_ANY,
	STR_ORDER_DROP_NO_LOADING,
};

static const StringID _order_unload_drowdown[] = {
	STR_ORDER_DROP_UNLOAD_IF_ACCEPTED,
	STR_ORDER_DROP_UNLOAD,
	STR_ORDER_DROP_TRANSFER,
	STR_EMPTY,
	STR_ORDER_DROP_NO_UNLOADING,
};

static const StringID _order_goto_dropdown[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_DEPOT,
	STR_ORDER_CONDITIONAL,
	STR_ORDER_SHARE,
};

static const StringID _order_goto_dropdown_aircraft[] = {
	STR_ORDER_GO_TO,
	STR_ORDER_GO_TO_NEAREST_HANGAR,
	STR_ORDER_CONDITIONAL,
	STR_ORDER_SHARE,
};

/** Variables for conditional orders; this defines the order of appearance in the dropdown box */
static const OrderConditionVariable _order_conditional_variable[] = {
	OCV_LOAD_PERCENTAGE,
	OCV_RELIABILITY,
	OCV_MAX_RELIABILITY,
	OCV_MAX_SPEED,
	OCV_AGE,
	OCV_REMAINING_LIFETIME,
	OCV_REQUIRES_SERVICE,
	OCV_UNCONDITIONALLY,
};

static const StringID _order_conditional_condition[] = {
	STR_ORDER_CONDITIONAL_COMPARATOR_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_NOT_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_LESS_THAN,
	STR_ORDER_CONDITIONAL_COMPARATOR_LESS_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_MORE_THAN,
	STR_ORDER_CONDITIONAL_COMPARATOR_MORE_EQUALS,
	STR_ORDER_CONDITIONAL_COMPARATOR_IS_TRUE,
	STR_ORDER_CONDITIONAL_COMPARATOR_IS_FALSE,
};

enum class FeederOrderMod{
	NONE,
	LOAD,
	UNLOAD
};

struct OrdersFromSettings {
	OrderLoadFlags load;
	OrderUnloadFlags unload;
	FeederOrderMod mod;
};

enum  GetOrderFromSettingsTypes {
	GOFS_NONE = 0,
	GOFS_FULL,
	GOFS_XFER,
	GOFS_UNLOAD,
	GOFS_FEEDLOAD,
	GOFS_FEEDUNLOAD,
	GOFS_NOLOAD
};

#define GOFSFEEDER_ORDERMOD_RESET gofsfeeder_ordermod = GOFS_FEEDER_NULL


/* fetch and compute orders set from settings */

static OrdersFromSettings GetOrdersFromSettings(const Vehicle *v, uint8 setting)
{
	OrdersFromSettings res = {
		OLF_LOAD_IF_POSSIBLE,
		OUF_UNLOAD_IF_POSSIBLE,
		FeederOrderMod::NONE
	};

	switch(setting) {

	case GOFS_FEEDLOAD:
		if (v->GetNumOrders() > 0) res.mod = FeederOrderMod::LOAD;
		res.unload = OUFB_NO_UNLOAD;
		res.load = OLF_FULL_LOAD_ANY;
		break;
	case GOFS_FULL:
		res.load = OLF_FULL_LOAD_ANY;
		break;

	case GOFS_UNLOAD:
		res.unload = OUFB_UNLOAD;
		if (_settings_client.gui.cm_no_loading_on_unload_order)
			res.load = OLFB_NO_LOAD;
		break;

	case GOFS_FEEDUNLOAD:
		if (v->GetNumOrders() > 0) res.mod = FeederOrderMod::UNLOAD;
		res.unload = OUFB_TRANSFER;
		res.load = OLFB_NO_LOAD;
		break;

	case GOFS_XFER:
		res.unload = OUFB_TRANSFER;
		if (_settings_client.gui.cm_no_loading_on_transfer_order)
			res.load = OLFB_NO_LOAD;
		break;

	case GOFS_NOLOAD:
		res.load = OLFB_NO_LOAD;
		break;

	case GOFS_NONE:
		break;

	default: NOT_REACHED();
	}
	return res;
}

extern uint ConvertSpeedToDisplaySpeed(uint speed, VehicleType type);
extern uint ConvertDisplaySpeedToSpeed(uint speed, VehicleType type);

static const StringID _order_depot_action_dropdown[] = {
	STR_ORDER_DROP_GO_ALWAYS_DEPOT,
	STR_ORDER_DROP_SERVICE_DEPOT,
	STR_ORDER_DROP_HALT_DEPOT,
	STR_ORDER_DROP_UNBUNCH,
};

static int DepotActionStringIndex(const Order *order)
{
	if (order->GetDepotActionType() & ODATFB_HALT) {
		return DA_STOP;
	} else if (order->GetDepotOrderType() & ODTFB_SERVICE) {
		return DA_SERVICE;
	} else if (order->GetDepotActionType() & ODATFB_UNBUNCH) {
		return DA_UNBUNCH;
	} else {
		return DA_ALWAYS_GO;
	}
}

static const StringID _order_refit_action_dropdown[] = {
	STR_ORDER_DROP_REFIT_AUTO,
	STR_ORDER_DROP_REFIT_AUTO_ANY,
};

static StringID GetOrderGoToString(const Order &order)
{
	if (order.GetDepotOrderType() & ODTFB_SERVICE) {
		return (order.GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_SERVICE_NON_STOP_AT : STR_ORDER_SERVICE_AT;
	} else {
		return (order.GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO : STR_ORDER_GO_TO;
	}
}

/**
 * Draws an order in order or timetable GUI
 * @param v Vehicle the order belongs to
 * @param order The order to draw
 * @param order_index Index of the order in the orders of the vehicle
 * @param y Y position for drawing
 * @param selected True, if the order is selected
 * @param timetable True, when drawing in the timetable GUI
 * @param left Left border for text drawing
 * @param middle X position between order index and order text
 * @param right Right border for text drawing
 */
void DrawOrderString(const Vehicle *v, const Order *order, int order_index, int y, bool selected, bool timetable, int left, int middle, int right)
{
	bool rtl = _current_text_dir == TD_RTL;

	SpriteID sprite = rtl ? SPR_ARROW_LEFT : SPR_ARROW_RIGHT;
	Dimension sprite_size = GetSpriteSize(sprite);
	if (v->cur_real_order_index == order_index) {
		/* Draw two arrows before the next real order. */
		DrawSprite(sprite, PAL_NONE, rtl ? right -     sprite_size.width : left,                     y + ((int)GetCharacterHeight(FS_NORMAL) - (int)sprite_size.height) / 2);
		DrawSprite(sprite, PAL_NONE, rtl ? right - 2 * sprite_size.width : left + sprite_size.width, y + ((int)GetCharacterHeight(FS_NORMAL) - (int)sprite_size.height) / 2);
	} else if (v->cur_implicit_order_index == order_index) {
		/* Draw one arrow before the next implicit order; the next real order will still get two arrows. */
		DrawSprite(sprite, PAL_NONE, rtl ? right -     sprite_size.width : left,                     y + ((int)GetCharacterHeight(FS_NORMAL) - (int)sprite_size.height) / 2);
	}

	TextColour colour = TC_BLACK;
	if (order->IsType(OT_IMPLICIT)) {
		colour = (selected ? TC_SILVER : TC_GREY) | TC_NO_SHADE;
	} else if (selected) {
		colour = TC_WHITE;
	}

	DrawString(left, rtl ? right - 2 * sprite_size.width - 3 : middle, y, GetString(STR_ORDER_INDEX, order_index + 1), colour, SA_RIGHT | SA_FORCE);

	std::string line;

	switch (order->GetType()) {
		case OT_DUMMY:
			line = GetString(STR_INVALID_ORDER);
			break;

		case OT_IMPLICIT:
			line = GetString(STR_ORDER_GO_TO_STATION, STR_ORDER_GO_TO, order->GetDestination());
			if (!timetable) line += GetString(STR_ORDER_IMPLICIT);
			break;

		case OT_GOTO_STATION: {
			OrderLoadFlags load = order->GetLoadType();
			OrderUnloadFlags unload = order->GetUnloadType();
			bool valid_station = CanVehicleUseStation(v, Station::Get(order->GetDestination().ToStationID()));

			line = GetString(valid_station ? STR_ORDER_GO_TO_STATION : STR_ORDER_GO_TO_STATION_CAN_T_USE_STATION, STR_ORDER_GO_TO + (v->IsGroundVehicle() ? order->GetNonStopType() : 0), order->GetDestination());
			if (timetable) {
				/* Show only wait time in the timetable window. */
				if (order->GetWaitTime() > 0) {
					auto [str, value] = GetTimetableParameters(order->GetWaitTime());
					line += GetString(order->IsWaitTimetabled() ? STR_TIMETABLE_STAY_FOR : STR_TIMETABLE_STAY_FOR_ESTIMATED, str, value);
				}
			} else {
				/* Show non-stop, refit and stop location only in the order window. */
				if (!(order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION)) {
					StringID str = _station_load_types[order->IsRefit()][unload][load];
					if (str != INVALID_STRING_ID) {
						if (order->IsRefit()) {
							line += GetString(str, order->IsAutoRefit() ? STR_ORDER_AUTO_REFIT_ANY : CargoSpec::Get(order->GetRefitCargo())->name);
						} else {
							line += GetString(str);
						}
					}
				}

				if (v->type == VEH_TRAIN && (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) == 0) {
					/* Only show the stopping location if other than the default chosen by the player. */
					if (order->GetStopLocation() != (OrderStopLocation)(_settings_client.gui.stop_location)) {
						line += GetString(STR_ORDER_STOP_LOCATION_NEAR_END + order->GetStopLocation());
					}
				}
			}
			break;
		}

		case OT_GOTO_DEPOT:
			if (!(order->GetDepotActionType() & ODATFB_NEAREST_DEPOT)) {
				/* Going to a specific depot. */
				line = GetString(STR_ORDER_GO_TO_DEPOT_FORMAT, GetOrderGoToString(*order), v->type, order->GetDestination());
			} else if (v->type == VEH_AIRCRAFT) {
				/* Going to the nearest hangar. */
				line = GetString(STR_ORDER_GO_TO_NEAREST_HANGAR_FORMAT, GetOrderGoToString(*order));
			} else {
				/* Going to the nearest depot. */
				line = GetString(STR_ORDER_GO_TO_NEAREST_DEPOT_FORMAT, GetOrderGoToString(*order), STR_ORDER_TRAIN_DEPOT + v->type);
			}

			/* Do not show stopping in the depot in the timetable window. */
			if (!timetable && (order->GetDepotActionType() & ODATFB_HALT)) {
				line += GetString(STR_ORDER_STOP_ORDER);
			}

			/* Do not show refitting in the depot in the timetable window. */
			if (!timetable && order->IsRefit()) {
				line += GetString((order->GetDepotActionType() & ODATFB_HALT) ? STR_ORDER_REFIT_STOP_ORDER : STR_ORDER_REFIT_ORDER, CargoSpec::Get(order->GetRefitCargo())->name);
			}

			/* Show unbunching depot in both order and timetable windows. */
			if (order->GetDepotActionType() & ODATFB_UNBUNCH) {
				line += GetString(STR_ORDER_WAIT_TO_UNBUNCH);
			}
			break;

		case OT_GOTO_WAYPOINT:
			line = GetString((order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS) ? STR_ORDER_GO_NON_STOP_TO_WAYPOINT : STR_ORDER_GO_TO_WAYPOINT, order->GetDestination());
			break;

		case OT_CONDITIONAL:
			if (order->GetConditionVariable() == OCV_UNCONDITIONALLY) {
				line = GetString(STR_ORDER_CONDITIONAL_UNCONDITIONAL, order->GetConditionSkipToOrder() + 1);
			} else {
				OrderConditionComparator occ = order->GetConditionComparator();

				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value, v->type);

				line = GetString((occ == OCC_IS_TRUE || occ == OCC_IS_FALSE) ? STR_ORDER_CONDITIONAL_TRUE_FALSE : STR_ORDER_CONDITIONAL_NUM,
					order->GetConditionSkipToOrder() + 1,
					STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + order->GetConditionVariable(),
					STR_ORDER_CONDITIONAL_COMPARATOR_EQUALS + occ,
					value);
			}

			if (timetable && order->GetWaitTime() > 0) {
				auto [str, value] = GetTimetableParameters(order->GetWaitTime());
				line += GetString(order->IsWaitTimetabled() ? STR_TIMETABLE_AND_TRAVEL_FOR : STR_TIMETABLE_AND_TRAVEL_FOR_ESTIMATED, str, value);
			}
			break;

		default: NOT_REACHED();
	}

	/* Check range for aircraft. */
	if (v->type == VEH_AIRCRAFT && Aircraft::From(v)->GetRange() > 0 && order->IsGotoOrder()) {
		const Order *next = order->next != nullptr ? order->next : v->GetFirstOrder();
		if (GetOrderDistance(order, next, v) > Aircraft::From(v)->acache.cached_max_range_sqr) {
			line += GetString(STR_ORDER_OUT_OF_RANGE);
		}
	}

	DrawString(rtl ? left : middle, rtl ? middle : right, y, line, colour);

	uint order_dist_sq = 0;
	uint order_dist_mh = 0;
	const Order *next2 = order->next != NULL ? order->next : v->GetFirstOrder();
	TileIndex prev_tile = order->GetLocation(v, true);
	TileIndex cur_tile = next2->GetLocation(v, true);
	if (prev_tile != INVALID_TILE && cur_tile != INVALID_TILE){
		order_dist_sq = IntSqrt(DistanceSquare(prev_tile, cur_tile));
		order_dist_mh = DistanceManhattan(prev_tile, cur_tile);
	}

	// FIXME rtl
	DrawString(middle, right, y, GetString(CM_STR_ORDER_DIST, order_dist_sq, order_dist_mh), TC_WHITE, SA_RIGHT);
}

/**
 * Get the order command a vehicle can do in a given tile.
 * @param v Vehicle involved.
 * @param tile Tile being queried.
 * @return The order associated to vehicle v in given tile (or empty order if vehicle can do nothing in the tile).
 */
static std::pair<Order, FeederOrderMod> GetOrderCmdFromTile(const Vehicle *v, TileIndex tile)
{
	/* Override the index as it is not coming from a pool, so would not be initialised correctly. */
	Order order;
	order.index = OrderID::Begin();

	/* check depot first */
	if (IsDepotTypeTile(tile, (TransportType)(uint)v->type) && IsTileOwner(tile, _local_company)) {
		order.MakeGoToDepot(GetDepotDestinationIndex(tile),
				ODTFB_PART_OF_ORDERS,
				(_settings_client.gui.new_nonstop && v->IsGroundVehicle()) ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);

		if (citymania::_fn_mod) {
			/* Check to see if we are allowed to make this an unbunching order. */
			bool failed = false;
			if (v->HasFullLoadOrder()) {
				/* We don't allow unbunching if the vehicle has a full load order. */
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_INSERT_NEW_ORDER), GetEncodedString(STR_ERROR_UNBUNCHING_NO_UNBUNCHING_FULL_LOAD), WL_INFO);
				failed = true;
			} else if (v->HasUnbunchingOrder()) {
				/* Don't allow a new unbunching order if we already have one. */
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_INSERT_NEW_ORDER), GetEncodedString(STR_ERROR_UNBUNCHING_ONLY_ONE_ALLOWED), WL_INFO);
				failed = true;
			} else if (v->HasConditionalOrder()) {
				/* We don't allow unbunching if the vehicle has a conditional order. */
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_INSERT_NEW_ORDER), GetEncodedString(STR_ERROR_UNBUNCHING_NO_UNBUNCHING_CONDITIONAL), WL_INFO);
				failed = true;
			}

			/* Return an empty order to bail out. */
			if (failed) {
				order.Free();
				return {order, FeederOrderMod::NONE};;
			}

			/* Now we are allowed to set the action type. */
			order.SetDepotActionType(ODATFB_UNBUNCH);
		}

		return {order, FeederOrderMod::NONE};
	}

	/* check rail waypoint */
	if (IsRailWaypointTile(tile) &&
			v->type == VEH_TRAIN &&
			IsTileOwner(tile, _local_company)) {
		order.MakeGoToWaypoint(GetStationIndex(tile));
		if (_settings_client.gui.new_nonstop != citymania::_fn_mod) order.SetNonStopType(ONSF_NO_STOP_AT_ANY_STATION);
		return std::make_pair(order, FeederOrderMod::NONE);
	}

	/* check road waypoint */
	if (IsRoadWaypointTile(tile) &&
			v->type == VEH_ROAD &&
			IsTileOwner(tile, _local_company)) {
		order.MakeGoToWaypoint(GetStationIndex(tile));
		if (_settings_client.gui.new_nonstop != _ctrl_pressed) order.SetNonStopType(ONSF_NO_STOP_AT_ANY_STATION);
		return std::make_pair(order, FeederOrderMod::NONE);
	}

	/* check buoy (no ownership) */
	if (IsBuoyTile(tile) && v->type == VEH_SHIP) {
		order.MakeGoToWaypoint(GetStationIndex(tile));
		return std::make_pair(order, FeederOrderMod::NONE);
	}

	/* check for station or industry with neutral station */
	if (IsTileType(tile, MP_STATION) || IsTileType(tile, MP_INDUSTRY)) {
		const Station *st = nullptr;

		if (IsTileType(tile, MP_STATION)) {
			st = Station::GetByTile(tile);
		} else {
			const Industry *in = Industry::GetByTile(tile);
			st = in->neutral_station;
		}
		if (st != nullptr && (st->owner == _local_company || st->owner == OWNER_NONE)) {
			StationFacilities facil;
			switch (v->type) {
				case VEH_SHIP:     facil = StationFacility::Dock;    break;
				case VEH_TRAIN:    facil = StationFacility::Train;   break;
				case VEH_AIRCRAFT: facil = StationFacility::Airport; break;
				case VEH_ROAD:     facil = {StationFacility::BusStop, StationFacility::TruckStop}; break;
				default: NOT_REACHED();
			}
			if (st->facilities.Any(facil)) {
				order.MakeGoToStation(st->index);

				uint8 os = 0xff;
				if (_ctrl_pressed) {
					if (_shift_pressed)
						os = _settings_client.gui.cm_ctrl_shift_order_mod;
					else if (_alt_pressed)
						os = _settings_client.gui.cm_alt_ctrl_order_mod;
					else
						os = _settings_client.gui.cm_ctrl_order_mod;
				}
				else if (_shift_pressed) {
					if (_alt_pressed)
						os = _settings_client.gui.cm_alt_shift_order_mod;
					else
						os = _settings_client.gui.cm_shift_order_mod;
				}
				else if (_alt_pressed)
					os = _settings_client.gui.cm_alt_order_mod;

				auto feeder_mod = FeederOrderMod::NONE;
				if (os != 0xff) {
					auto ofs = GetOrdersFromSettings(v, os);
					if (ofs.load != (enum OrderLoadFlags)-1)
						order.SetLoadType(ofs.load);
					if (ofs.unload != (enum OrderUnloadFlags)-1)
					order.SetUnloadType(ofs.unload);
					feeder_mod = ofs.mod;
				}

				if (_settings_client.gui.new_nonstop && v->IsGroundVehicle()) order.SetNonStopType(ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
				order.SetStopLocation(v->type == VEH_TRAIN ? (OrderStopLocation)(_settings_client.gui.stop_location) : OSL_PLATFORM_FAR_END);
				return std::make_pair(order, feeder_mod);
			}
		}
	}

	/* not found */
	order.Free();
	return std::make_pair(order, FeederOrderMod::NONE);
}

/** Hotkeys for order window. */
enum OrderHotKeys : int32_t {
	OHK_SKIP,
	OHK_DELETE,
	OHK_GOTO,
	OHK_NONSTOP,
	OHK_FULLLOAD,
	OHK_UNLOAD,
	OHK_NEAREST_DEPOT,
	OHK_ALWAYS_SERVICE,
	OHK_TRANSFER,
	OHK_NO_UNLOAD,
	OHK_NO_LOAD,
	OHK_CLOSE,
};

/**
 * %Order window code for all vehicles.
 *
 * At the bottom of the window two button rows are located for changing the orders of the vehicle.
 *
 * \section top-row Top row
 * The top-row is for manipulating an individual order. What row is displayed depends on the type of vehicle, and whether or not you are the owner of the vehicle.
 *
 * The top-row buttons of one of your trains or road vehicles is one of the following three cases:
 * \verbatim
 * +-----------------+-----------------+-----------------+-----------------+
 * |    NON-STOP     |    FULL_LOAD    |     UNLOAD      |      REFIT      | (normal)
 * +-----------------+-----+-----------+-----------+-----+-----------------+
 * |       COND_VAR        |    COND_COMPARATOR    |      COND_VALUE       | (for conditional orders)
 * +-----------------+-----+-----------+-----------+-----+-----------------+
 * |    NON-STOP     |      REFIT      |     SERVICE     |     (empty)     | (for depot orders)
 * +-----------------+-----------------+-----------------+-----------------+
 * \endverbatim
 *
 * Airplanes and ships have one of the following three top-row button rows:
 * \verbatim
 * +-----------------+-----------------+-----------------+
 * |    FULL_LOAD    |     UNLOAD      |      REFIT      | (normal)
 * +-----------------+-----------------+-----------------+
 * |    COND_VAR     | COND_COMPARATOR |   COND_VALUE    | (for conditional orders)
 * +-----------------+--------+--------+-----------------+
 * |            REFIT         |          SERVICE         | (for depot order)
 * +--------------------------+--------------------------+
 * \endverbatim
 *
 * \section bottom-row Bottom row
 * The second row (the bottom row) is for manipulating the list of orders:
 * \verbatim
 * +-----------------+-----------------+-----------------+
 * |      SKIP       |     DELETE      |      GOTO       |
 * +-----------------+-----------------+-----------------+
 * \endverbatim
 *
 * For vehicles of other companies, both button rows are not displayed.
 */
struct OrdersWindow : public Window {
private:
	/** Under what reason are we using the PlaceObject functionality? */
	enum OrderPlaceObjectState : uint8_t {
		OPOS_NONE,
		OPOS_GOTO,
		OPOS_CONDITIONAL,
		OPOS_SHARE,
		OPOS_END,
	};

	/** Displayed planes of the #NWID_SELECTION widgets. */
	enum DisplayPane : uint8_t {
		/* WID_O_SEL_TOP_ROW_GROUNDVEHICLE */
		DP_GROUNDVEHICLE_ROW_NORMAL      = 0, ///< Display the row for normal/depot orders in the top row of the train/rv order window.
		DP_GROUNDVEHICLE_ROW_CONDITIONAL = 1, ///< Display the row for conditional orders in the top row of the train/rv order window.

		/* WID_O_SEL_TOP_LEFT */
		DP_LEFT_LOAD       = 0, ///< Display 'load' in the left button of the top row of the train/rv order window.
		DP_LEFT_REFIT      = 1, ///< Display 'refit' in the left button of the top row of the train/rv order window.

		/* WID_O_SEL_TOP_MIDDLE */
		DP_MIDDLE_UNLOAD   = 0, ///< Display 'unload' in the middle button of the top row of the train/rv order window.
		DP_MIDDLE_SERVICE  = 1, ///< Display 'service' in the middle button of the top row of the train/rv order window.

		/* WID_O_SEL_TOP_RIGHT */
		DP_RIGHT_EMPTY     = 0, ///< Display an empty panel in the right button of the top row of the train/rv order window.
		DP_RIGHT_REFIT     = 1, ///< Display 'refit' in the right button of the top  row of the train/rv order window.

		/* WID_O_SEL_TOP_ROW */
		DP_ROW_LOAD        = 0, ///< Display 'load' / 'unload' / 'refit' buttons in the top row of the ship/airplane order window.
		DP_ROW_DEPOT       = 1, ///< Display 'refit' / 'service' buttons in the top row of the ship/airplane order window.
		DP_ROW_CONDITIONAL = 2, ///< Display the conditional order buttons in the top row of the ship/airplane order window.

		/* WID_O_SEL_BOTTOM_MIDDLE */
		DP_BOTTOM_MIDDLE_DELETE       = 0, ///< Display 'delete' in the middle button of the bottom row of the vehicle order window.
		DP_BOTTOM_MIDDLE_STOP_SHARING = 1, ///< Display 'stop sharing' in the middle button of the bottom row of the vehicle order window.
	};

	int selected_order = -1;
	VehicleOrderID order_over = INVALID_VEH_ORDER_ID; ///< Order over which another order is dragged, \c INVALID_VEH_ORDER_ID if none.
	OrderPlaceObjectState goto_type = OPOS_NONE;
	const Vehicle *vehicle = nullptr; ///< Vehicle owning the orders being displayed and manipulated.
	Scrollbar *vscroll = nullptr;
	bool can_do_refit = false; ///< Vehicle chain can be refitted in depot.
	bool can_do_autorefit = false; ///< Vehicle chain can be auto-refitted.

	/**
	 * Return the memorised selected order.
	 * @return the memorised order if it is a valid one
	 *  else return the number of orders
	 */
	VehicleOrderID OrderGetSel() const
	{
		int num = this->selected_order;
		return (num >= 0 && num < vehicle->GetNumOrders()) ? num : vehicle->GetNumOrders();
	}

	/**
	 * Calculate the selected order.
	 * The calculation is based on the relative (to the window) y click position and
	 *  the position of the scrollbar.
	 *
	 * @param y Y-value of the click relative to the window origin
	 * @return The selected order if the order is valid, else return \c INVALID_VEH_ORDER_ID.
	 */
	VehicleOrderID GetOrderFromPt(int y)
	{
		int32_t sel = this->vscroll->GetScrolledRowFromWidget(y, this, WID_O_ORDER_LIST, WidgetDimensions::scaled.framerect.top);
		if (sel == INT32_MAX) return INVALID_VEH_ORDER_ID;
		/* One past the orders is the 'End of Orders' line. */
		assert(IsInsideBS(sel, 0, vehicle->GetNumOrders() + 1));
		return sel;
	}

	/**
	 * Handle the click on the goto button.
	 */
	void OrderClick_Goto(OrderPlaceObjectState type)
	{
		assert(type > OPOS_NONE && type < OPOS_END);

		static const HighLightStyle goto_place_style[OPOS_END - 1] = {
			HT_RECT | HT_VEHICLE, // OPOS_GOTO
			HT_NONE,              // OPOS_CONDITIONAL
			HT_VEHICLE,           // OPOS_SHARE
		};
		SetObjectToPlaceWnd(ANIMCURSOR_PICKSTATION, PAL_NONE, goto_place_style[type - 1], this, CM_DDSP_PICK_STATION);
		this->goto_type = type;
		this->SetWidgetDirty(WID_O_GOTO);
	}

	/**
	 * Handle the click on the full load button.
	 * @param load_type Load flag to apply. If matches existing load type, toggles to default of 'load if possible'.
	 * @param toggle If we toggle or not (used for hotkey behavior)
	 */
	void OrderClick_FullLoad(OrderLoadFlags load_type, bool toggle = false)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == nullptr) return;

		if (toggle && order->GetLoadType() == load_type) {
			load_type = OLF_LOAD_IF_POSSIBLE; // reset to 'default'
		}
		if (order->GetLoadType() == load_type) return; // If we still match, do nothing

		Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, sel_ord, MOF_LOAD, load_type);
	}

	/**
	 * Handle the click on the service.
	 */
	void OrderClick_Service(int i)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();

		if (i < 0) {
			const Order *order = this->vehicle->GetOrder(sel_ord);
			if (order == nullptr) return;
			i = (order->GetDepotOrderType() & ODTFB_SERVICE) ? DA_ALWAYS_GO : DA_SERVICE;
		}
		Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, sel_ord, MOF_DEPOT_ACTION, i);
	}

	/**
	 * Handle the click on the service in nearest depot button.
	 */
	void OrderClick_NearestDepot()
	{
		Order order;
		order.next = nullptr;
		order.index = OrderID::Begin();
		order.MakeGoToDepot(DepotID::Invalid(), ODTFB_PART_OF_ORDERS,
				_settings_client.gui.new_nonstop && this->vehicle->IsGroundVehicle() ? ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS : ONSF_STOP_EVERYWHERE);
		order.SetDepotActionType(ODATFB_NEAREST_DEPOT);

		Command<CMD_INSERT_ORDER>::Post(STR_ERROR_CAN_T_INSERT_NEW_ORDER, this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), order);
	}

	/**
	 * Handle the click on the unload button.
	 * @param unload_type Unload flag to apply. If matches existing unload type, toggles to default of 'unload if possible'.
	 * @param toggle If we toggle or not (used for hotkey behavior)
	 */
	void OrderClick_Unload(OrderUnloadFlags unload_type, bool toggle = false)
	{
		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == nullptr) return;

		if (toggle && order->GetUnloadType() == unload_type) {
			unload_type = OUF_UNLOAD_IF_POSSIBLE;
		}
		if (order->GetUnloadType() == unload_type) return; // If we still match, do nothing

		Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, sel_ord, MOF_UNLOAD, unload_type);

		bool set_no_load = false;
		if (unload_type == OUFB_TRANSFER){
			set_no_load = _settings_client.gui.cm_no_loading_on_transfer_order;
		}
		else if (unload_type == OUFB_UNLOAD){
			set_no_load = _settings_client.gui.cm_no_loading_on_unload_order;
		}
		/* Transfer orders with leave empty as default */
		if (set_no_load) {
			Command<CMD_MODIFY_ORDER>::Post(this->vehicle->tile, this->vehicle->index, sel_ord, MOF_LOAD, OLFB_NO_LOAD);
			this->SetWidgetDirty(WID_O_FULL_LOAD);
		}
	}

	/**
	 * Handle the click on the nonstop button.
	 * @param non_stop what non-stop type to use; -1 to use the 'next' one.
	 */
	void OrderClick_Nonstop(int non_stop)
	{
		if (!this->vehicle->IsGroundVehicle()) return;

		VehicleOrderID sel_ord = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel_ord);

		if (order == nullptr || order->GetNonStopType() == non_stop) return;

		/* Keypress if negative, so 'toggle' to the next */
		if (non_stop < 0) {
			non_stop = order->GetNonStopType() ^ ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS;
		}

		this->SetWidgetDirty(WID_O_NON_STOP);
		Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, sel_ord, MOF_NON_STOP, non_stop);
	}

	/**
	 * Handle the click on the skip button.
	 * If ctrl is pressed, skip to selected order, else skip to current order + 1
	 */
	void OrderClick_Skip()
	{
		/* Don't skip when there's nothing to skip */
		if (citymania::_fn_mod && this->vehicle->cur_implicit_order_index == this->OrderGetSel()) return;
		if (this->vehicle->GetNumOrders() <= 1) return;

		citymania::cmd::SkipToOrder(
				this->vehicle->tile,
				this->vehicle->index,
				citymania::_fn_mod ? this->OrderGetSel() : ((this->vehicle->cur_implicit_order_index + 1) % this->vehicle->GetNumOrders()))
			.with_error(citymania::_fn_mod ? STR_ERROR_CAN_T_SKIP_TO_ORDER : STR_ERROR_CAN_T_SKIP_ORDER)
			.no_estimate()
			.post();
	}

	/**
	 * Handle the click on the delete button.
	 */
	void OrderClick_Delete()
	{
		/* When networking, move one order lower */
		int selected = this->selected_order + (int)_networking;

		if (Command<CMD_DELETE_ORDER>::Post(STR_ERROR_CAN_T_DELETE_THIS_ORDER, this->vehicle->tile, this->vehicle->index, this->OrderGetSel())) {
			this->selected_order = selected >= this->vehicle->GetNumOrders() ? -1 : selected;
			this->UpdateButtonState();
		}
	}

	/**
	 * Handle the click on the 'stop sharing' button.
	 * If 'End of Shared Orders' isn't selected, do nothing. If Ctrl is pressed, call OrderClick_Delete and exit.
	 * To stop sharing this vehicle order list, we copy the orders of a vehicle that share this order list. That way we
	 * exit the group of shared vehicles while keeping the same order list.
	 */
	void OrderClick_StopSharing()
	{
		/* Don't try to stop sharing orders if 'End of Shared Orders' isn't selected. */
		if (!this->vehicle->IsOrderListShared() || this->selected_order != this->vehicle->GetNumOrders()) return;
		/* If Ctrl is pressed, delete the order list as if we clicked the 'Delete' button. */
		if (citymania::_fn_mod) {
			this->OrderClick_Delete();
			return;
		}

		/* Get another vehicle that share orders with this vehicle. */
		Vehicle *other_shared = (this->vehicle->FirstShared() == this->vehicle) ? this->vehicle->NextShared() : this->vehicle->PreviousShared();
		/* Copy the order list of the other vehicle. */
		if (Command<CMD_CLONE_ORDER>::Post(STR_ERROR_CAN_T_STOP_SHARING_ORDER_LIST, this->vehicle->tile, CO_COPY, this->vehicle->index, other_shared->index)) {
			this->UpdateButtonState();
		}
	}

	/**
	 * Handle the click on the refit button.
	 * If ctrl is pressed, cancel refitting, else show the refit window.
	 * @param i Selected refit command.
	 * @param auto_refit Select refit for auto-refitting.
	 */
	void OrderClick_Refit(int i, bool auto_refit)
	{
		if (citymania::_fn_mod) {
			/* Cancel refitting */
			Command<CMD_ORDER_REFIT>::Post(this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), CARGO_NO_REFIT);
		} else {
			if (i == 1) { // Auto-refit to available cargo type.
				Command<CMD_ORDER_REFIT>::Post(this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), CARGO_AUTO_REFIT);
			} else {
				ShowVehicleRefitWindow(this->vehicle, this->OrderGetSel(), this, auto_refit);
			}
		}
	}

	/** Cache auto-refittability of the vehicle chain. */
	void UpdateAutoRefitState()
	{
		this->can_do_refit = false;
		this->can_do_autorefit = false;
		for (const Vehicle *w = this->vehicle; w != nullptr; w = w->IsGroundVehicle() ? w->Next() : nullptr) {
			if (IsEngineRefittable(w->engine_type)) this->can_do_refit = true;
			if (Engine::Get(w->engine_type)->info.misc_flags.Test(EngineMiscFlag::AutoRefit)) this->can_do_autorefit = true;
		}
	}

public:
	OrdersWindow(WindowDesc &desc, const Vehicle *v) : Window(desc)
	{
		this->vehicle = v;

		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_O_SCROLLBAR);
		if (NWidgetCore *nwid = this->GetWidget<NWidgetCore>(WID_O_DEPOT_ACTION); nwid != nullptr) {
			nwid->SetToolTip(STR_ORDER_TRAIN_DEPOT_ACTION_TOOLTIP + v->type);
		}
		this->FinishInitNested(v->index);

		this->owner = v->owner;

		this->UpdateAutoRefitState();

		if (_settings_client.gui.quick_goto && v->owner == _local_company) {
			/* If there are less than 2 station, make Go To active. */
			int station_orders = 0;
			for (const Order *order : v->Orders()) {
				if (order->IsType(OT_GOTO_STATION)) station_orders++;
			}

			if (station_orders < 2) this->OrderClick_Goto(OPOS_GOTO);
		}
		this->OnInvalidateData(VIWD_MODIFY_ORDERS);
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		switch (widget) {
			case WID_O_ORDER_LIST:
				resize.height = GetCharacterHeight(FS_NORMAL);
				size.height = 6 * resize.height + padding.height;
				break;

			case WID_O_COND_VARIABLE: {
				Dimension d = {0, 0};
				for (const auto &ocv : _order_conditional_variable) {
					d = maxdim(d, GetStringBoundingBox(STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + ocv));
				}
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}

			case WID_O_COND_COMPARATOR: {
				Dimension d = GetStringListBoundingBox(_order_conditional_condition);
				d.width += padding.width;
				d.height += padding.height;
				size = maxdim(size, d);
				break;
			}
		}
	}

	/**
	 * Some data on this window has become invalid.
	 * @param data Information about the changed data.
	 * @param gui_scope Whether the call is done from GUI scope. You may not do everything when not in GUI scope. See #InvalidateWindowData() for details.
	 */
	void OnInvalidateData([[maybe_unused]] int data = 0, [[maybe_unused]] bool gui_scope = true) override
	{
		VehicleOrderID from = INVALID_VEH_ORDER_ID;
		VehicleOrderID to   = INVALID_VEH_ORDER_ID;

		switch (data) {
			case VIWD_AUTOREPLACE:
				/* Autoreplace replaced the vehicle */
				this->vehicle = Vehicle::Get(this->window_number);
				[[fallthrough]];

			case VIWD_CONSIST_CHANGED:
				/* Vehicle composition was changed. */
				this->UpdateAutoRefitState();
				break;

			case VIWD_REMOVE_ALL_ORDERS:
				/* Removed / replaced all orders (after deleting / sharing) */
				if (this->selected_order == -1) break;

				this->CloseChildWindows();
				this->selected_order = -1;
				break;

			case VIWD_MODIFY_ORDERS:
				/* Some other order changes */
				break;

			default:
				if (data < 0) break;

				if (gui_scope) break; // only do this once; from command scope
				from = GB(data, 0, 8);
				to   = GB(data, 8, 8);
				/* Moving an order. If one of these is INVALID_VEH_ORDER_ID, then
				 * the order is being created / removed */
				if (this->selected_order == -1) break;

				if (from == to) break; // no need to change anything

				if (from != this->selected_order) {
					/* Moving from preceding order? */
					this->selected_order -= (int)(from <= this->selected_order);
					/* Moving to   preceding order? */
					this->selected_order += (int)(to   <= this->selected_order);
					break;
				}

				/* Now we are modifying the selected order */
				if (to == INVALID_VEH_ORDER_ID) {
					/* Deleting selected order */
					this->CloseChildWindows();
					this->selected_order = -1;
					break;
				}

				/* Moving selected order */
				this->selected_order = to;
				break;
		}

		this->vscroll->SetCount(this->vehicle->GetNumOrders() + 1);
		if (gui_scope) this->UpdateButtonState();

		/* Scroll to the new order. */
		if (from == INVALID_VEH_ORDER_ID && to != INVALID_VEH_ORDER_ID && !this->vscroll->IsVisible(to)) {
			this->vscroll->ScrollTowards(to);
		}
	}

	void UpdateButtonState()
	{
		if (this->vehicle->owner != _local_company) return; // No buttons are displayed with competitor order windows.

		bool shared_orders = this->vehicle->IsOrderListShared();
		VehicleOrderID sel = this->OrderGetSel();
		const Order *order = this->vehicle->GetOrder(sel);

		/* Second row. */
		/* skip */
		this->SetWidgetDisabledState(WID_O_SKIP, this->vehicle->GetNumOrders() <= 1);

		/* delete / stop sharing */
		NWidgetStacked *delete_sel = this->GetWidget<NWidgetStacked>(WID_O_SEL_BOTTOM_MIDDLE);
		if (shared_orders && this->selected_order == this->vehicle->GetNumOrders()) {
			/* The 'End of Shared Orders' order is selected, show the 'stop sharing' button. */
			delete_sel->SetDisplayedPlane(DP_BOTTOM_MIDDLE_STOP_SHARING);
		} else {
			/* The 'End of Shared Orders' order isn't selected, show the 'delete' button. */
			delete_sel->SetDisplayedPlane(DP_BOTTOM_MIDDLE_DELETE);
			this->SetWidgetDisabledState(WID_O_DELETE,
				(uint)this->vehicle->GetNumOrders() + ((shared_orders || this->vehicle->GetNumOrders() != 0) ? 1 : 0) <= (uint)this->selected_order);

			/* Set the tooltip of the 'delete' button depending on whether the
			 * 'End of Orders' order or a regular order is selected. */
			NWidgetCore *nwi = this->GetWidget<NWidgetCore>(WID_O_DELETE);
			if (this->selected_order == this->vehicle->GetNumOrders()) {
				nwi->SetStringTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_ALL_TOOLTIP);
			} else {
				nwi->SetStringTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP);
			}
		}

		/* First row. */
		this->RaiseWidget(WID_O_FULL_LOAD);
		this->RaiseWidget(WID_O_UNLOAD);

		/* Selection widgets. */
		/* Train or road vehicle. */
		NWidgetStacked *train_row_sel = this->GetWidget<NWidgetStacked>(WID_O_SEL_TOP_ROW_GROUNDVEHICLE);
		NWidgetStacked *left_sel      = this->GetWidget<NWidgetStacked>(WID_O_SEL_TOP_LEFT);
		NWidgetStacked *middle_sel    = this->GetWidget<NWidgetStacked>(WID_O_SEL_TOP_MIDDLE);
		NWidgetStacked *right_sel     = this->GetWidget<NWidgetStacked>(WID_O_SEL_TOP_RIGHT);
		/* Ship or airplane. */
		NWidgetStacked *row_sel = this->GetWidget<NWidgetStacked>(WID_O_SEL_TOP_ROW);
		assert(row_sel != nullptr || (train_row_sel != nullptr && left_sel != nullptr && middle_sel != nullptr && right_sel != nullptr));


		if (order == nullptr) {
			if (row_sel != nullptr) {
				row_sel->SetDisplayedPlane(DP_ROW_LOAD);
			} else {
				train_row_sel->SetDisplayedPlane(DP_GROUNDVEHICLE_ROW_NORMAL);
				left_sel->SetDisplayedPlane(DP_LEFT_LOAD);
				middle_sel->SetDisplayedPlane(DP_MIDDLE_UNLOAD);
				right_sel->SetDisplayedPlane(DP_RIGHT_EMPTY);
				this->DisableWidget(WID_O_NON_STOP);
				this->RaiseWidget(WID_O_NON_STOP);
			}
			this->DisableWidget(WID_O_FULL_LOAD);
			this->DisableWidget(WID_O_UNLOAD);
			this->DisableWidget(WID_O_REFIT_DROPDOWN);
		} else {
			this->SetWidgetDisabledState(WID_O_FULL_LOAD, (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) != 0); // full load
			this->SetWidgetDisabledState(WID_O_UNLOAD,    (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) != 0); // unload

			switch (order->GetType()) {
				case OT_GOTO_STATION:
					if (row_sel != nullptr) {
						row_sel->SetDisplayedPlane(DP_ROW_LOAD);
					} else {
						train_row_sel->SetDisplayedPlane(DP_GROUNDVEHICLE_ROW_NORMAL);
						left_sel->SetDisplayedPlane(DP_LEFT_LOAD);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_UNLOAD);
						right_sel->SetDisplayedPlane(DP_RIGHT_REFIT);
						this->EnableWidget(WID_O_NON_STOP);
						this->SetWidgetLoweredState(WID_O_NON_STOP, order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
					}
					this->SetWidgetLoweredState(WID_O_FULL_LOAD, order->GetLoadType() == OLF_FULL_LOAD_ANY);
					this->SetWidgetLoweredState(WID_O_UNLOAD, order->GetUnloadType() == OUFB_UNLOAD);

					/* Can only do refitting when stopping at the destination and loading cargo.
					 * Also enable the button if a refit is already set to allow clearing it. */
					this->SetWidgetDisabledState(WID_O_REFIT_DROPDOWN,
							order->GetLoadType() == OLFB_NO_LOAD || (order->GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) ||
							((!this->can_do_refit || !this->can_do_autorefit) && !order->IsRefit()));

					break;

				case OT_GOTO_WAYPOINT:
					if (row_sel != nullptr) {
						row_sel->SetDisplayedPlane(DP_ROW_LOAD);
					} else {
						train_row_sel->SetDisplayedPlane(DP_GROUNDVEHICLE_ROW_NORMAL);
						left_sel->SetDisplayedPlane(DP_LEFT_LOAD);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_UNLOAD);
						right_sel->SetDisplayedPlane(DP_RIGHT_EMPTY);
						this->EnableWidget(WID_O_NON_STOP);
						this->SetWidgetLoweredState(WID_O_NON_STOP, order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
					}
					this->DisableWidget(WID_O_FULL_LOAD);
					this->DisableWidget(WID_O_UNLOAD);
					this->DisableWidget(WID_O_REFIT_DROPDOWN);
					break;

				case OT_GOTO_DEPOT:
					if (row_sel != nullptr) {
						row_sel->SetDisplayedPlane(DP_ROW_DEPOT);
					} else {
						train_row_sel->SetDisplayedPlane(DP_GROUNDVEHICLE_ROW_NORMAL);
						left_sel->SetDisplayedPlane(DP_LEFT_REFIT);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_SERVICE);
						right_sel->SetDisplayedPlane(DP_RIGHT_EMPTY);
						this->EnableWidget(WID_O_NON_STOP);
						this->SetWidgetLoweredState(WID_O_NON_STOP, order->GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS);
					}
					/* Disable refit button if the order is no 'always go' order.
					 * However, keep the service button enabled for refit-orders to allow clearing refits (without knowing about ctrl). */
					this->SetWidgetDisabledState(WID_O_REFIT,
							(order->GetDepotOrderType() & ODTFB_SERVICE) || (order->GetDepotActionType() & ODATFB_HALT) ||
							(!this->can_do_refit && !order->IsRefit()));
					break;

				case OT_CONDITIONAL: {
					if (row_sel != nullptr) {
						row_sel->SetDisplayedPlane(DP_ROW_CONDITIONAL);
					} else {
						train_row_sel->SetDisplayedPlane(DP_GROUNDVEHICLE_ROW_CONDITIONAL);
					}
					OrderConditionVariable ocv = order->GetConditionVariable();
					/* Set the strings for the dropdown boxes. */
					this->GetWidget<NWidgetCore>(WID_O_COND_VARIABLE)->SetString(STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + ocv);
					this->GetWidget<NWidgetCore>(WID_O_COND_COMPARATOR)->SetString(_order_conditional_condition[order->GetConditionComparator()]);
					this->SetWidgetDisabledState(WID_O_COND_COMPARATOR, ocv == OCV_UNCONDITIONALLY);
					this->SetWidgetDisabledState(WID_O_COND_VALUE, ocv == OCV_REQUIRES_SERVICE || ocv == OCV_UNCONDITIONALLY);
					break;
				}

				default: // every other order
					if (row_sel != nullptr) {
						row_sel->SetDisplayedPlane(DP_ROW_LOAD);
					} else {
						train_row_sel->SetDisplayedPlane(DP_GROUNDVEHICLE_ROW_NORMAL);
						left_sel->SetDisplayedPlane(DP_LEFT_LOAD);
						middle_sel->SetDisplayedPlane(DP_MIDDLE_UNLOAD);
						right_sel->SetDisplayedPlane(DP_RIGHT_EMPTY);
						this->DisableWidget(WID_O_NON_STOP);
					}
					this->DisableWidget(WID_O_FULL_LOAD);
					this->DisableWidget(WID_O_UNLOAD);
					this->DisableWidget(WID_O_REFIT_DROPDOWN);
					break;
			}
		}

		/* Disable list of vehicles with the same shared orders if there is no list */
		this->SetWidgetDisabledState(WID_O_SHARED_ORDER_LIST, !shared_orders);

		this->SetDirty();
	}

	void OnPaint() override
	{
		if (this->vehicle->owner != _local_company) {
			this->selected_order = -1; // Disable selection any selected row at a competitor order window.
		} else {
			this->SetWidgetLoweredState(WID_O_GOTO, this->goto_type != OPOS_NONE);
		}
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WID_O_ORDER_LIST) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.frametext, WidgetDimensions::scaled.framerect);
		bool rtl = _current_text_dir == TD_RTL;
		uint64_t max_value = GetParamMaxValue(this->vehicle->GetNumOrders(), 2);
		int index_column_width = GetStringBoundingBox(GetString(STR_ORDER_INDEX, max_value)).width + 2 * GetSpriteSize(rtl ? SPR_ARROW_RIGHT : SPR_ARROW_LEFT).width + WidgetDimensions::scaled.hsep_normal;
		int middle = rtl ? ir.right - index_column_width : ir.left + index_column_width;

		int y = ir.top;
		int line_height = this->GetWidget<NWidgetBase>(WID_O_ORDER_LIST)->resize_y;

		int i = this->vscroll->GetPosition();
		const Order *order = this->vehicle->GetOrder(i);
		/* First draw the highlighting underground if it exists. */
		if (this->order_over != INVALID_VEH_ORDER_ID) {
			while (order != nullptr) {
				/* Don't draw anything if it extends past the end of the window. */
				if (!this->vscroll->IsVisible(i)) break;

				if (i != this->selected_order && i == this->order_over) {
					/* Highlight dragged order destination. */
					int top = (this->order_over < this->selected_order ? y : y + line_height) - WidgetDimensions::scaled.framerect.top;
					int bottom = std::min(top + 2, ir.bottom);
					top = std::max(top - 3, ir.top);
					GfxFillRect(ir.left, top, ir.right, bottom, GetColourGradient(COLOUR_GREY, SHADE_LIGHTEST));
					break;
				}
				y += line_height;

				i++;
				order = order->next;
			}

			/* Reset counters for drawing the orders. */
			y = ir.top;
			i = this->vscroll->GetPosition();
			order = this->vehicle->GetOrder(i);
		}

		/* Draw the orders. */
		while (order != nullptr) {
			/* Don't draw anything if it extends past the end of the window. */
			if (!this->vscroll->IsVisible(i)) break;

			DrawOrderString(this->vehicle, order, i, y, i == this->selected_order, false, ir.left, middle, ir.right);
			y += line_height;

			i++;
			order = order->next;
		}

		if (this->vscroll->IsVisible(i)) {
			StringID str = this->vehicle->IsOrderListShared() ? STR_ORDERS_END_OF_SHARED_ORDERS : STR_ORDERS_END_OF_ORDERS;
			DrawString(rtl ? ir.left : middle, rtl ? middle : ir.right, y, str, (i == this->selected_order) ? TC_WHITE : TC_BLACK);
		}
	}

	std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		switch (widget) {
			case WID_O_COND_VALUE: {
				VehicleOrderID sel = this->OrderGetSel();
				const Order *order = this->vehicle->GetOrder(sel);

				if (order != nullptr && order->IsType(OT_CONDITIONAL)) {
					uint value = order->GetConditionValue();
					if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value, this->vehicle->type);
					return GetString(STR_JUST_COMMA, value);
				}
				return {};
			}

			case WID_O_CAPTION:
				return GetString(STR_ORDERS_CAPTION, this->vehicle->index);

			case WID_O_DEPOT_ACTION: {
				VehicleOrderID sel = this->OrderGetSel();
				const Order *order = this->vehicle->GetOrder(sel);
				if (order == nullptr || !order->IsType(OT_GOTO_DEPOT)) return {};

				/* Select the current action selected in the dropdown. The flags don't match the dropdown so we can't just use an index. */
				if (order->GetDepotOrderType() & ODTFB_SERVICE) return GetString(STR_ORDER_DROP_SERVICE_DEPOT);
				if (order->GetDepotActionType() & ODATFB_HALT) return GetString(STR_ORDER_DROP_HALT_DEPOT);
				if (order->GetDepotActionType() & ODATFB_UNBUNCH) return GetString(STR_ORDER_DROP_UNBUNCH);

				return GetString(STR_ORDER_DROP_GO_ALWAYS_DEPOT);
			}

			default:
				return this->Window::GetWidgetString(widget, stringid);
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_O_ORDER_LIST: {
				if (this->goto_type == OPOS_CONDITIONAL) {
					VehicleOrderID order_id = this->GetOrderFromPt(_cursor.pos.y - this->top);
					if (order_id != INVALID_VEH_ORDER_ID) {
						Order order;
						order.next = nullptr;
						order.index = OrderID::Begin();
						order.MakeConditional(order_id);

						Command<CMD_INSERT_ORDER>::Post(STR_ERROR_CAN_T_INSERT_NEW_ORDER, this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), order);
					}
					ResetObjectToPlace();
					break;
				}

				VehicleOrderID sel = this->GetOrderFromPt(pt.y);

				if (citymania::_fn_mod && sel < this->vehicle->GetNumOrders()) {
					TileIndex xy = this->vehicle->GetOrder(sel)->GetLocation(this->vehicle);
					if (xy != INVALID_TILE) ScrollMainWindowToTile(xy);
					return;
				}

				/* This order won't be selected any more, close all child windows and dropdowns */
				this->CloseChildWindows();

				if (sel == INVALID_VEH_ORDER_ID || this->vehicle->owner != _local_company) {
					/* Deselect clicked order */
					this->selected_order = -1;
				} else if (sel == this->selected_order) {
					if (click_count > 1 && this->vehicle->type == VEH_TRAIN && sel < this->vehicle->GetNumOrders()) {
						Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER,
								this->vehicle->tile, this->vehicle->index, sel,
								MOF_STOP_LOCATION, (this->vehicle->GetOrder(sel)->GetStopLocation() + 1) % OSL_END);
					}
				} else {
					/* Select clicked order */
					this->selected_order = sel;

					if (this->vehicle->owner == _local_company) {
						/* Activate drag and drop */
						SetObjectToPlaceWnd(SPR_CURSOR_MOUSE, PAL_NONE, HT_DRAG, this, CM_DDSP_SELECT_ORDER);
					}
				}

				this->UpdateButtonState();
				break;
			}

			case WID_O_SKIP:
				this->OrderClick_Skip();
				break;

			case WID_O_DELETE:
				this->OrderClick_Delete();
				break;

			case WID_O_STOP_SHARING:
				this->OrderClick_StopSharing();
				break;

			case WID_O_NON_STOP:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Nonstop(-1);
				} else {
					const Order *o = this->vehicle->GetOrder(this->OrderGetSel());
					assert(o != nullptr);
					ShowDropDownMenu(this, _order_non_stop_drowdown, o->GetNonStopType(), WID_O_NON_STOP, 0,
													o->IsType(OT_GOTO_STATION) ? 0 : (o->IsType(OT_GOTO_WAYPOINT) ? 3 : 12));
				}
				break;

			case WID_O_GOTO:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					if (this->goto_type != OPOS_NONE) {
						ResetObjectToPlace();
					} else {
						this->OrderClick_Goto(OPOS_GOTO);
					}
				} else {
					int sel;
					switch (this->goto_type) {
						case OPOS_NONE:        sel = -1; break;
						case OPOS_GOTO:        sel =  0; break;
						case OPOS_CONDITIONAL: sel =  2; break;
						case OPOS_SHARE:       sel =  3; break;
						default: NOT_REACHED();
					}
					ShowDropDownMenu(this, this->vehicle->type == VEH_AIRCRAFT ? _order_goto_dropdown_aircraft : _order_goto_dropdown, sel, WID_O_GOTO, 0, 0);
				}
				break;

			case WID_O_FULL_LOAD:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_FullLoad(OLF_FULL_LOAD_ANY, true);
				} else {
					ShowDropDownMenu(this, _order_full_load_drowdown, this->vehicle->GetOrder(this->OrderGetSel())->GetLoadType(), WID_O_FULL_LOAD, 0, 2);
				}
				break;

			case WID_O_UNLOAD:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Unload(OUFB_UNLOAD, true);
				} else {
					ShowDropDownMenu(this, _order_unload_drowdown, this->vehicle->GetOrder(this->OrderGetSel())->GetUnloadType(), WID_O_UNLOAD, 0, 8);
				}
				break;

			case WID_O_REFIT:
				this->OrderClick_Refit(0, false);
				break;

			case WID_O_DEPOT_ACTION:
				ShowDropDownMenu(this, _order_depot_action_dropdown, DepotActionStringIndex(this->vehicle->GetOrder(this->OrderGetSel())), WID_O_DEPOT_ACTION, 0, 0);
				break;

			case WID_O_REFIT_DROPDOWN:
				if (this->GetWidget<NWidgetLeaf>(widget)->ButtonHit(pt)) {
					this->OrderClick_Refit(0, true);
				} else {
					ShowDropDownMenu(this, _order_refit_action_dropdown, 0, WID_O_REFIT_DROPDOWN, 0, 0);
				}
				break;

			case WID_O_TIMETABLE_VIEW:
				ShowTimetableWindow(this->vehicle);
				break;

			case WID_O_COND_VARIABLE: {
				DropDownList list;
				for (const auto &ocv : _order_conditional_variable) {
					list.push_back(MakeDropDownListStringItem(STR_ORDER_CONDITIONAL_LOAD_PERCENTAGE + ocv, ocv));
				}
				ShowDropDownList(this, std::move(list), this->vehicle->GetOrder(this->OrderGetSel())->GetConditionVariable(), WID_O_COND_VARIABLE);
				break;
			}

			case WID_O_COND_COMPARATOR: {
				const Order *o = this->vehicle->GetOrder(this->OrderGetSel());
				assert(o != nullptr);
				ShowDropDownMenu(this, _order_conditional_condition, o->GetConditionComparator(), WID_O_COND_COMPARATOR, 0, (o->GetConditionVariable() == OCV_REQUIRES_SERVICE) ? 0x3F : 0xC0);
				break;
			}

			case WID_O_COND_VALUE: {
				const Order *order = this->vehicle->GetOrder(this->OrderGetSel());
				assert(order != nullptr);
				uint value = order->GetConditionValue();
				if (order->GetConditionVariable() == OCV_MAX_SPEED) value = ConvertSpeedToDisplaySpeed(value, this->vehicle->type);
				ShowQueryString(GetString(STR_JUST_INT, value), STR_ORDER_CONDITIONAL_VALUE_CAPT, 5, this, CS_NUMERAL, {});
				break;
			}

			case WID_O_SHARED_ORDER_LIST:
				ShowVehicleListWindow(this->vehicle);
				break;
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value() || str->empty()) return;

		VehicleOrderID sel = this->OrderGetSel();
		uint value = atoi(str->c_str());

		switch (this->vehicle->GetOrder(sel)->GetConditionVariable()) {
			case OCV_MAX_SPEED:
				value = ConvertDisplaySpeedToSpeed(value, this->vehicle->type);
				break;

			case OCV_RELIABILITY:
			case OCV_LOAD_PERCENTAGE:
				value = Clamp(value, 0, 100);
				break;

			default:
				break;
		}
		Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, sel, MOF_COND_VALUE, Clamp(value, 0, 2047));
	}

	void OnDropdownSelect(WidgetID widget, int index) override
	{
		switch (widget) {
			case WID_O_NON_STOP:
				this->OrderClick_Nonstop(index);
				break;

			case WID_O_FULL_LOAD:
				this->OrderClick_FullLoad((OrderLoadFlags)index);
				break;

			case WID_O_UNLOAD:
				this->OrderClick_Unload((OrderUnloadFlags)index);
				break;

			case WID_O_GOTO:
				switch (index) {
					case 0: this->OrderClick_Goto(OPOS_GOTO); break;
					case 1: this->OrderClick_NearestDepot(); break;
					case 2: this->OrderClick_Goto(OPOS_CONDITIONAL); break;
					case 3: this->OrderClick_Goto(OPOS_SHARE); break;
					default: NOT_REACHED();
				}
				break;

			case WID_O_DEPOT_ACTION:
				this->OrderClick_Service(index);
				break;

			case WID_O_REFIT_DROPDOWN:
				this->OrderClick_Refit(index, true);
				break;

			case WID_O_COND_VARIABLE:
				Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), MOF_COND_VARIABLE, index);
				break;

			case WID_O_COND_COMPARATOR:
				Command<CMD_MODIFY_ORDER>::Post(STR_ERROR_CAN_T_MODIFY_THIS_ORDER, this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), MOF_COND_COMPARATOR, index);
				break;
		}
	}

	void OnDragDrop(Point pt, WidgetID widget) override
	{
		switch (widget) {
			case WID_O_ORDER_LIST: {
				VehicleOrderID from_order = this->OrderGetSel();
				VehicleOrderID to_order = this->GetOrderFromPt(pt.y);

				if (!(from_order == to_order || from_order == INVALID_VEH_ORDER_ID || from_order > this->vehicle->GetNumOrders() || to_order == INVALID_VEH_ORDER_ID || to_order > this->vehicle->GetNumOrders()) &&
						Command<CMD_MOVE_ORDER>::Post(STR_ERROR_CAN_T_MOVE_THIS_ORDER, this->vehicle->tile, this->vehicle->index, from_order, to_order)) {
					this->selected_order = -1;
					this->UpdateButtonState();
				}
				break;
			}

			case WID_O_DELETE:
				this->OrderClick_Delete();
				break;

			case WID_O_STOP_SHARING:
				this->OrderClick_StopSharing();
				break;
		}

		ResetObjectToPlace();

		if (this->order_over != INVALID_VEH_ORDER_ID) {
			/* End of drag-and-drop, hide dragged order destination highlight. */
			this->order_over = INVALID_VEH_ORDER_ID;
			this->SetWidgetDirty(WID_O_ORDER_LIST);
		}
	}

	EventState OnHotkey(int hotkey) override
	{
		if (this->vehicle->owner != _local_company) return ES_NOT_HANDLED;
		if(hotkey == OHK_GOTO && this->goto_type != OPOS_NONE){
			this->RaiseWidget(WID_O_GOTO);
			ResetObjectToPlace();
			return ES_NOT_HANDLED;
		}

		switch (hotkey) {
			case OHK_SKIP:           this->OrderClick_Skip(); break;
			case OHK_DELETE:         this->OrderClick_Delete(); break;
			case OHK_GOTO:           this->OrderClick_Goto(OPOS_GOTO); break;
			case OHK_NONSTOP:        this->OrderClick_Nonstop(-1); break;
			case OHK_FULLLOAD:       this->OrderClick_FullLoad(OLF_FULL_LOAD_ANY, true); break;
			case OHK_UNLOAD:         this->OrderClick_Unload(OUFB_UNLOAD, true); break;
			case OHK_NEAREST_DEPOT:  this->OrderClick_NearestDepot(); break;
			case OHK_ALWAYS_SERVICE: this->OrderClick_Service(-1); break;
			case OHK_TRANSFER:       this->OrderClick_Unload(OUFB_TRANSFER, true); break;
			case OHK_NO_UNLOAD:      this->OrderClick_Unload(OUFB_NO_UNLOAD, true); break;
			case OHK_NO_LOAD:        this->OrderClick_FullLoad(OLFB_NO_LOAD, true); break;
			case OHK_CLOSE:          this->Close(); break;
			default: return ES_NOT_HANDLED;
		}
		return ES_HANDLED;
	}

	void OnPlaceObject([[maybe_unused]] Point pt, TileIndex tile) override
	{
		if (this->goto_type == OPOS_GOTO) {
			auto res = GetOrderCmdFromTile(this->vehicle, tile);
			const Order cmd = res.first;
			auto feeder_mod = res.second;
			if (cmd.IsType(OT_NOTHING)) return;

			if (feeder_mod != FeederOrderMod::NONE) {
				if (feeder_mod == FeederOrderMod::LOAD) {
					if (citymania::cmd::InsertOrder(this->vehicle->tile, this->vehicle->index, 1, cmd)
							.with_error(STR_ERROR_CAN_T_INSERT_NEW_ORDER)
							.no_estimate()
							.post()) {
						citymania::cmd::DeleteOrder(this->vehicle->tile, this->vehicle->index, 0)
							.with_error(STR_ERROR_CAN_T_DELETE_THIS_ORDER)
							.no_estimate()
							.post();
					}

				} else if (feeder_mod == FeederOrderMod::UNLOAD) { // still flushes the whole order table
					if (citymania::cmd::InsertOrder(this->vehicle->tile, this->vehicle->index, this->vehicle->GetNumOrders(), cmd)
							.with_error(STR_ERROR_CAN_T_INSERT_NEW_ORDER)
							.no_estimate()
							.post()) {
						citymania::cmd::DeleteOrder(this->vehicle->tile, this->vehicle->index, this->vehicle->GetNumOrders() + (int)_networking - 2)
							.with_error(STR_ERROR_CAN_T_DELETE_THIS_ORDER)
							.no_estimate()
							.post();
					}
				}
			} else if (citymania::cmd::InsertOrder(this->vehicle->tile, this->vehicle->index, this->OrderGetSel(), cmd)
							.with_error(STR_ERROR_CAN_T_INSERT_NEW_ORDER)
							.no_estimate()
							.post()) {
				/* With quick goto the Go To button stays active */
				if (!_settings_client.gui.quick_goto) ResetObjectToPlace();
			}
		}
	}

	bool OnVehicleSelect(const Vehicle *v) override
	{
		/* v is vehicle getting orders. Only copy/clone orders if vehicle doesn't have any orders yet.
		 * We disallow copying orders of other vehicles if we already have at least one order entry
		 * ourself as it easily copies orders of vehicles within a station when we mean the station.
		 * Obviously if you press CTRL on a non-empty orders vehicle you know what you are doing
		 * TODO: give a warning message */
		bool share_order = citymania::_fn_mod || this->goto_type == OPOS_SHARE;
		if (this->vehicle->GetNumOrders() != 0 && !share_order) return false;

		if (Command<CMD_CLONE_ORDER>::Post(share_order ? STR_ERROR_CAN_T_SHARE_ORDER_LIST : STR_ERROR_CAN_T_COPY_ORDER_LIST,
				this->vehicle->tile, share_order ? CO_SHARE : CO_COPY, this->vehicle->index, v->index)) {
			this->selected_order = -1;
			ResetObjectToPlace();
		}
		return true;
	}

	/**
	 * Clones an order list from a vehicle list.  If this doesn't make sense (because not all vehicles in the list have the same orders), then it displays an error.
	 * @return This always returns true, which indicates that the contextual action handled the mouse click.
	 *         Note that it's correct behaviour to always handle the click even though an error is displayed,
	 *         because users aren't going to expect the default action to be performed just because they overlooked that cloning doesn't make sense.
	 */
	bool OnVehicleSelect(VehicleList::const_iterator begin, VehicleList::const_iterator end) override
	{
		bool share_order = citymania::_fn_mod || this->goto_type == OPOS_SHARE;
		if (this->vehicle->GetNumOrders() != 0 && !share_order) return false;

		if (!share_order) {
			/* If CTRL is not pressed: If all the vehicles in this list have the same orders, then copy orders */
			if (AllEqual(begin, end, [](const Vehicle *v1, const Vehicle *v2) {
				return VehiclesHaveSameOrderList(v1, v2);
			})) {
				OnVehicleSelect(*begin);
			} else {
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_COPY_ORDER_LIST), GetEncodedString(STR_ERROR_CAN_T_COPY_ORDER_VEHICLE_LIST), WL_INFO);
			}
		} else {
			/* If CTRL is pressed: If all the vehicles in this list share orders, then copy orders */
			if (AllEqual(begin, end, [](const Vehicle *v1, const Vehicle *v2) {
				return v1->FirstShared() == v2->FirstShared();
			})) {
				OnVehicleSelect(*begin);
			} else {
				ShowErrorMessage(GetEncodedString(STR_ERROR_CAN_T_SHARE_ORDER_LIST), GetEncodedString(STR_ERROR_CAN_T_SHARE_ORDER_VEHICLE_LIST), WL_INFO);
			}
		}

		return true;
	}

	void OnPlaceObjectAbort() override
	{
		this->goto_type = OPOS_NONE;
		this->SetWidgetDirty(WID_O_GOTO);

		/* Remove drag highlighting if it exists. */
		if (this->order_over != INVALID_VEH_ORDER_ID) {
			this->order_over = INVALID_VEH_ORDER_ID;
			this->SetWidgetDirty(WID_O_ORDER_LIST);
		}
	}

	void OnMouseDrag(Point pt, WidgetID widget) override
	{
		if (this->selected_order != -1 && widget == WID_O_ORDER_LIST) {
			/* An order is dragged.. */
			VehicleOrderID from_order = this->OrderGetSel();
			VehicleOrderID to_order = this->GetOrderFromPt(pt.y);
			uint num_orders = this->vehicle->GetNumOrders();

			if (from_order != INVALID_VEH_ORDER_ID && from_order <= num_orders) {
				if (to_order != INVALID_VEH_ORDER_ID && to_order <= num_orders) { // ..over an existing order.
					this->order_over = to_order;
					this->SetWidgetDirty(widget);
				} else if (from_order != to_order && this->order_over != INVALID_VEH_ORDER_ID) { // ..outside of the order list.
					this->order_over = INVALID_VEH_ORDER_ID;
					this->SetWidgetDirty(widget);
				}
			}
		}
	}

	void OnResize() override
	{
		/* Update the scroll bar */
		this->vscroll->SetCapacityFromWidget(this, WID_O_ORDER_LIST, WidgetDimensions::scaled.framerect.Vertical());
	}

	static inline HotkeyList hotkeys{"order", {
		Hotkey('D', "skip", OHK_SKIP),
		Hotkey('F', "delete", OHK_DELETE),
		Hotkey('G', "goto", OHK_GOTO),
		Hotkey('H', "nonstop", OHK_NONSTOP),
		Hotkey('J', "fullload", OHK_FULLLOAD),
		Hotkey('K', "unload", OHK_UNLOAD),
		Hotkey(0, "nearest_depot", OHK_NEAREST_DEPOT),
		Hotkey(0, "always_service", OHK_ALWAYS_SERVICE),
		Hotkey(0, "transfer", OHK_TRANSFER),
		Hotkey(0, "no_unload", OHK_NO_UNLOAD),
		Hotkey(0, "no_load", OHK_NO_LOAD),
		Hotkey('Q', "cm_close", OHK_CLOSE),
	}};
};

/** Nested widget definition for "your" train orders. */
static constexpr NWidgetPart _nested_orders_train_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_O_CAPTION),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetStringTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_O_ORDER_LIST), SetMinimalSize(372, 62), SetToolTip(STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_O_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_O_SCROLLBAR),
	EndContainer(),

	/* First button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_TOP_ROW_GROUNDVEHICLE),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_NON_STOP), SetMinimalSize(93, 12), SetFill(1, 0),
															SetStringTip(STR_ORDER_NON_STOP, STR_ORDER_TOOLTIP_NON_STOP), SetResize(1, 0),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_TOP_LEFT),
					NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_FULL_LOAD), SetMinimalSize(93, 12), SetFill(1, 0),
															SetStringTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_REFIT), SetMinimalSize(93, 12), SetFill(1, 0),
															SetStringTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
				EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_TOP_MIDDLE),
					NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_UNLOAD), SetMinimalSize(93, 12), SetFill(1, 0),
															SetStringTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
					NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_DEPOT_ACTION), SetMinimalSize(93, 12), SetFill(1, 0),
															SetStringTip(STR_JUST_STRING), SetResize(1, 0),
				EndContainer(),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_TOP_RIGHT),
					NWidget(WWT_PANEL, COLOUR_GREY), SetMinimalSize(93, 12), SetFill(1, 0), SetResize(1, 0), EndContainer(),
					NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_REFIT_DROPDOWN), SetMinimalSize(93, 12), SetFill(1, 0),
															SetStringTip(STR_ORDER_REFIT_AUTO, STR_ORDER_REFIT_AUTO_TOOLTIP), SetResize(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_O_COND_VARIABLE), SetMinimalSize(124, 12), SetFill(1, 0),
															SetToolTip(STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_O_COND_COMPARATOR), SetMinimalSize(124, 12), SetFill(1, 0),
															SetToolTip(STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_COND_VALUE), SetMinimalSize(124, 12), SetFill(1, 0),
															SetToolTip(STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
		EndContainer(),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_O_SHARED_ORDER_LIST), SetAspect(1), SetSpriteTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
	EndContainer(),

	/* Second button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_SKIP), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_BOTTOM_MIDDLE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_DELETE), SetMinimalSize(124, 12), SetFill(1, 0),
														SetStringTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_STOP_SHARING), SetMinimalSize(124, 12), SetFill(1, 0),
														SetStringTip(STR_ORDERS_STOP_SHARING_BUTTON, STR_ORDERS_STOP_SHARING_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_GOTO), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _orders_train_desc(
	WDP_AUTO, "view_vehicle_orders_train", 384, 100,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WindowDefaultFlag::Construction,
	_nested_orders_train_widgets,
	&OrdersWindow::hotkeys
);

/** Nested widget definition for "your" orders (non-train). */
static constexpr NWidgetPart _nested_orders_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_O_CAPTION), SetStringTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetStringTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_O_ORDER_LIST), SetMinimalSize(372, 62), SetToolTip(STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_O_SCROLLBAR), EndContainer(),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_O_SCROLLBAR),
	EndContainer(),

	/* First button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_TOP_ROW),
			/* Load + unload + refit buttons. */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_FULL_LOAD), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDER_TOGGLE_FULL_LOAD, STR_ORDER_TOOLTIP_FULL_LOAD), SetResize(1, 0),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_UNLOAD), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDER_TOGGLE_UNLOAD, STR_ORDER_TOOLTIP_UNLOAD), SetResize(1, 0),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_REFIT_DROPDOWN), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDER_REFIT_AUTO, STR_ORDER_REFIT_AUTO_TOOLTIP), SetResize(1, 0),
			EndContainer(),
			/* Refit + service buttons. */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_REFIT), SetMinimalSize(186, 12), SetFill(1, 0),
													SetStringTip(STR_ORDER_REFIT, STR_ORDER_REFIT_TOOLTIP), SetResize(1, 0),
				NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_DEPOT_ACTION), SetMinimalSize(124, 12), SetFill(1, 0),
													SetResize(1, 0),
			EndContainer(),

			/* Buttons for setting a condition. */
			NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_O_COND_VARIABLE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetToolTip(STR_ORDER_CONDITIONAL_VARIABLE_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_DROPDOWN, COLOUR_GREY, WID_O_COND_COMPARATOR), SetMinimalSize(124, 12), SetFill(1, 0),
													SetToolTip(STR_ORDER_CONDITIONAL_COMPARATOR_TOOLTIP), SetResize(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_COND_VALUE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_JUST_COMMA, STR_ORDER_CONDITIONAL_VALUE_TOOLTIP), SetResize(1, 0),
			EndContainer(),
		EndContainer(),

		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, WID_O_SHARED_ORDER_LIST), SetAspect(1), SetSpriteTip(SPR_SHARED_ORDERS_ICON, STR_ORDERS_VEH_WITH_SHARED_ORDERS_LIST_TOOLTIP),
	EndContainer(),

	/* Second button row. */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_SKIP), SetMinimalSize(124, 12), SetFill(1, 0),
											SetStringTip(STR_ORDERS_SKIP_BUTTON, STR_ORDERS_SKIP_TOOLTIP), SetResize(1, 0),
		NWidget(NWID_SELECTION, INVALID_COLOUR, WID_O_SEL_BOTTOM_MIDDLE),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_DELETE), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDERS_DELETE_BUTTON, STR_ORDERS_DELETE_TOOLTIP), SetResize(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_STOP_SHARING), SetMinimalSize(124, 12), SetFill(1, 0),
													SetStringTip(STR_ORDERS_STOP_SHARING_BUTTON, STR_ORDERS_STOP_SHARING_TOOLTIP), SetResize(1, 0),
		EndContainer(),
		NWidget(NWID_BUTTON_DROPDOWN, COLOUR_GREY, WID_O_GOTO), SetMinimalSize(124, 12), SetFill(1, 0),
											SetStringTip(STR_ORDERS_GO_TO_BUTTON, STR_ORDERS_GO_TO_TOOLTIP), SetResize(1, 0),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

static WindowDesc _orders_desc(
	WDP_AUTO, "view_vehicle_orders", 384, 100,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WindowDefaultFlag::Construction,
	_nested_orders_widgets,
	&OrdersWindow::hotkeys
);

/** Nested widget definition for competitor orders. */
static constexpr NWidgetPart _nested_other_orders_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_O_CAPTION), SetStringTip(STR_ORDERS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_O_TIMETABLE_VIEW), SetMinimalSize(61, 14), SetStringTip(STR_ORDERS_TIMETABLE_VIEW, STR_ORDERS_TIMETABLE_VIEW_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WID_O_ORDER_LIST), SetMinimalSize(372, 72), SetToolTip(STR_ORDERS_LIST_TOOLTIP), SetResize(1, 1), SetScrollbar(WID_O_SCROLLBAR), EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_O_SCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _other_orders_desc(
	WDP_AUTO, "view_vehicle_orders_competitor", 384, 86,
	WC_VEHICLE_ORDERS, WC_VEHICLE_VIEW,
	WindowDefaultFlag::Construction,
	_nested_other_orders_widgets,
	&OrdersWindow::hotkeys
);

void ShowOrdersWindow(const Vehicle *v)
{
	CloseWindowById(WC_VEHICLE_DETAILS, v->index, false);
	CloseWindowById(WC_VEHICLE_TIMETABLE, v->index, false);
	if (BringWindowToFrontById(WC_VEHICLE_ORDERS, v->index) != nullptr) return;

	/* Using a different WindowDescs for _local_company causes problems.
	 * Due to this we have to close order windows in ChangeWindowOwner/CloseCompanyWindows,
	 * because we cannot change switch the WindowDescs and keeping the old WindowDesc results
	 * in crashed due to missing widges.
	 * TODO Rewrite the order GUI to not use different WindowDescs.
	 */
	if (v->owner != _local_company) {
		new OrdersWindow(_other_orders_desc, v);
	} else {
		new OrdersWindow(v->IsGroundVehicle() ? _orders_train_desc : _orders_desc, v);
	}
}
