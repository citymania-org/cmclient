/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command_type.h Types related to commands. */

#ifndef COMMAND_TYPE_H
#define COMMAND_TYPE_H

#include "company_type.h"
#include "economy_type.h"
#include "strings_type.h"
#include "tile_type.h"

#include "citymania/extensions/cmext_commandcost.hpp"

struct GRFFile;

/**
 * Common return value for all commands. Wraps the cost and
 * a possible error message/state together.
 */
class CommandCost {
	Money cost;                                 ///< The cost of this action
	StringID message;                           ///< Warning message for when success is unset
	ExpensesType expense_type;                  ///< the type of expence as shown on the finances view
	bool success;                               ///< Whether the command went fine up to this moment
	Owner owner = CompanyID::Invalid(); ///< Originator owner of error.
	StringID extra_message = INVALID_STRING_ID; ///< Additional warning message for when success is unset
	EncodedString encoded_message{}; ///< Encoded error message, used if the error message includes parameters.

public:
	citymania::ext::CommandCost cm;

	/**
	 * Creates a command cost return with no cost and no error
	 */
	CommandCost() : cost(0), message(INVALID_STRING_ID), expense_type(INVALID_EXPENSES), success(true) {}

	/**
	 * Creates a command return value with one, or optionally two, error message strings.
	 */
	explicit CommandCost(StringID msg, StringID extra_msg = INVALID_STRING_ID) : cost(0), message(msg), expense_type(INVALID_EXPENSES), success(false), extra_message(extra_msg) {}

	/**
	 * Creates a command cost with given expense type and start cost of 0
	 * @param ex_t the expense type
	 */
	explicit CommandCost(ExpensesType ex_t) : cost(0), message(INVALID_STRING_ID), expense_type(ex_t), success(true) {}

	/**
	 * Creates a command return value with the given start cost and expense type
	 * @param ex_t the expense type
	 * @param cst the initial cost of this command
	 */
	CommandCost(ExpensesType ex_t, const Money &cst) : cost(cst), message(INVALID_STRING_ID), expense_type(ex_t), success(true) {}

	/**
	 * Set the 'owner' (the originator) of this error message. This is used to show a company owner's face if you
	 * attempt an action on something owned by other company.
	 */
	inline void SetErrorOwner(Owner owner)
	{
		this->owner = owner;
	}

	/**
	 * Set the encoded message string. If set, this is used by the error message window instead of the error StringID,
	 * to allow more information to be displayed to the local player.
	 * @note Do not set an encoded message if the error is not for the local player, as it will never be seen.
	 * @param message EncodedString message to set.
	 */
	void SetEncodedMessage(EncodedString &&message)
	{
		this->encoded_message = std::move(message);
	}

	/**
	 * Get the last encoded error message.
	 * @returns Reference to the encoded message.
	 */
	EncodedString &GetEncodedMessage()
	{
		return this->encoded_message;
	}

	/**
	 * Get the originator owner for this error.
	 */
	inline CompanyID GetErrorOwner() const
	{
		return this->owner;
	}

	/**
	 * Adds the given cost to the cost of the command.
	 * @param cost the cost to add
	 */
	inline void AddCost(const Money &cost)
	{
		this->cost += cost;
	}

	void AddCost(CommandCost &&cmd_cost);

	/**
	 * Multiplies the cost of the command by the given factor.
	 * @param factor factor to multiply the costs with
	 */
	inline void MultiplyCost(int factor)
	{
		this->cost *= factor;
	}

	/**
	 * The costs as made up to this moment
	 * @return the costs
	 */
	inline Money GetCost() const
	{
		return this->cost;
	}

	/**
	 * The expense type of the cost
	 * @return the expense type
	 */
	inline ExpensesType GetExpensesType() const
	{
		return this->expense_type;
	}

	/**
	 * Makes this #CommandCost behave like an error command.
	 * @param message The error message.
	 */
	void MakeError(StringID message)
	{
		assert(message != INVALID_STRING_ID);
		this->success = false;
		this->message = message;
		this->extra_message = INVALID_STRING_ID;
	}

	/**
	 * Returns the error message of a command
	 * @return the error message, if succeeded #INVALID_STRING_ID
	 */
	StringID GetErrorMessage() const
	{
		if (this->success) return INVALID_STRING_ID;
		return this->message;
	}

	/**
	 * Returns the extra error message of a command
	 * @return the extra error message, if succeeded #INVALID_STRING_ID
	 */
	StringID GetExtraErrorMessage() const
	{
		if (this->success) return INVALID_STRING_ID;
		return this->extra_message;
	}

	/**
	 * Did this command succeed?
	 * @return true if and only if it succeeded
	 */
	inline bool Succeeded() const
	{
		return this->success;
	}

	/**
	 * Did this command fail?
	 * @return true if and only if it failed
	 */
	inline bool Failed() const
	{
		return !this->success;
	}
};

CommandCost CommandCostWithParam(StringID str, uint64_t value);
CommandCost CommandCostWithParam(StringID str, ConvertibleThroughBase auto value) { return CommandCostWithParam(str, value.base()); }

/**
 * List of commands.
 *
 * This enum defines all possible commands which can be executed to the game
 * engine. Observing the game like the query-tool or checking the profit of a
 * vehicle don't result in a command which should be executed in the engine
 * nor send to the server in a network game.
 *
 * @see _command_proc_table
 */
enum Commands : uint8_t {
	CMD_BUILD_RAILROAD_TRACK,         ///< build a rail track
	CMD_REMOVE_RAILROAD_TRACK,        ///< remove a rail track
	CMD_BUILD_SINGLE_RAIL,            ///< build a single rail track
	CMD_REMOVE_SINGLE_RAIL,           ///< remove a single rail track
	CMD_LANDSCAPE_CLEAR,              ///< demolish a tile
	CMD_BUILD_BRIDGE,                 ///< build a bridge
	CMD_BUILD_RAIL_STATION,           ///< build a rail station
	CMD_BUILD_TRAIN_DEPOT,            ///< build a train depot
	CMD_BUILD_SINGLE_SIGNAL,          ///< build a signal
	CMD_REMOVE_SINGLE_SIGNAL,         ///< remove a signal
	CMD_TERRAFORM_LAND,               ///< terraform a tile
	CMD_BUILD_OBJECT,                 ///< build an object
	CMD_BUILD_OBJECT_AREA,            ///< build an area of objects
	CMD_BUILD_TUNNEL,                 ///< build a tunnel

	CMD_REMOVE_FROM_RAIL_STATION,     ///< remove a (rectangle of) tiles from a rail station
	CMD_CONVERT_RAIL,                 ///< convert a rail type

	CMD_BUILD_RAIL_WAYPOINT,          ///< build a waypoint
	CMD_RENAME_WAYPOINT,              ///< rename a waypoint
	CMD_REMOVE_FROM_RAIL_WAYPOINT,    ///< remove a (rectangle of) tiles from a rail waypoint

	CMD_BUILD_ROAD_WAYPOINT,          ///< build a road waypoint
	CMD_REMOVE_FROM_ROAD_WAYPOINT,    ///< remove a (rectangle of) tiles from a road waypoint

	CMD_BUILD_ROAD_STOP,              ///< build a road stop
	CMD_REMOVE_ROAD_STOP,             ///< remove a road stop
	CMD_BUILD_LONG_ROAD,              ///< build a complete road (not a "half" one)
	CMD_REMOVE_LONG_ROAD,             ///< remove a complete road (not a "half" one)
	CMD_BUILD_ROAD,                   ///< build a "half" road
	CMD_BUILD_ROAD_DEPOT,             ///< build a road depot
	CMD_CONVERT_ROAD,                 ///< convert a road type

	CMD_BUILD_AIRPORT,                ///< build an airport

	CMD_BUILD_DOCK,                   ///< build a dock

	CMD_BUILD_SHIP_DEPOT,             ///< build a ship depot
	CMD_BUILD_BUOY,                   ///< build a buoy

	CMD_PLANT_TREE,                   ///< plant a tree

	CMD_BUILD_VEHICLE,                ///< build a vehicle
	CMD_SELL_VEHICLE,                 ///< sell a vehicle
	CMD_REFIT_VEHICLE,                ///< refit the cargo space of a vehicle
	CMD_SEND_VEHICLE_TO_DEPOT,        ///< send a vehicle to a depot
	CMD_SET_VEHICLE_VISIBILITY,       ///< hide or unhide a vehicle in the build vehicle and autoreplace GUIs

	CMD_MOVE_RAIL_VEHICLE,            ///< move a rail vehicle (in the depot)
	CMD_FORCE_TRAIN_PROCEED,          ///< proceed a train to pass a red signal
	CMD_REVERSE_TRAIN_DIRECTION,      ///< turn a train around

	CMD_CLEAR_ORDER_BACKUP,           ///< clear the order backup of a given user/tile
	CMD_MODIFY_ORDER,                 ///< modify an order (like set full-load)
	CMD_SKIP_TO_ORDER,                ///< skip an order to the next of specific one
	CMD_DELETE_ORDER,                 ///< delete an order
	CMD_INSERT_ORDER,                 ///< insert a new order

	CMD_CHANGE_SERVICE_INT,           ///< change the server interval of a vehicle

	CMD_BUILD_INDUSTRY,               ///< build a new industry
	CMD_INDUSTRY_SET_FLAGS,           ///< change industry control flags
	CMD_INDUSTRY_SET_EXCLUSIVITY,     ///< change industry exclusive consumer/supplier
	CMD_INDUSTRY_SET_TEXT,            ///< change additional text for the industry
	CMD_INDUSTRY_SET_PRODUCTION,      ///< change industry production

	CMD_SET_COMPANY_MANAGER_FACE,     ///< set the manager's face of the company
	CMD_SET_COMPANY_COLOUR,           ///< set the colour of the company

	CMD_INCREASE_LOAN,                ///< increase the loan from the bank
	CMD_DECREASE_LOAN,                ///< decrease the loan from the bank
	CMD_SET_COMPANY_MAX_LOAN,         ///< sets the max loan for the company

	CMD_WANT_ENGINE_PREVIEW,          ///< confirm the preview of an engine
	CMD_ENGINE_CTRL,                  ///< control availability of the engine for companies

	CMD_RENAME_VEHICLE,               ///< rename a whole vehicle
	CMD_RENAME_ENGINE,                ///< rename a engine (in the engine list)
	CMD_RENAME_COMPANY,               ///< change the company name
	CMD_RENAME_PRESIDENT,             ///< change the president name
	CMD_RENAME_STATION,               ///< rename a station
	CMD_RENAME_DEPOT,                 ///< rename a depot

	CMD_PLACE_SIGN,                   ///< place a sign
	CMD_RENAME_SIGN,                  ///< rename a sign

	CMD_TURN_ROADVEH,                 ///< turn a road vehicle around

	CMD_PAUSE,                        ///< pause the game

	CMD_BUY_COMPANY,                  ///< buy a company which is bankrupt

	CMD_FOUND_TOWN,                   ///< found a town
	CMD_RENAME_TOWN,                  ///< rename a town
	CMD_DO_TOWN_ACTION,               ///< do a action from the town detail window (like advertises or bribe)
	CMD_TOWN_CARGO_GOAL,              ///< set the goal of a cargo for a town
	CMD_TOWN_GROWTH_RATE,             ///< set the town growth rate
	CMD_TOWN_RATING,                  ///< set rating of a company in a town
	CMD_TOWN_SET_TEXT,                ///< set the custom text of a town
	CMD_EXPAND_TOWN,                  ///< expand a town
	CMD_DELETE_TOWN,                  ///< delete a town
	CMD_PLACE_HOUSE,                  ///< place a house

	CMD_ORDER_REFIT,                  ///< change the refit information of an order (for "goto depot" )
	CMD_CLONE_ORDER,                  ///< clone (and share) an order
	CMD_CLEAR_AREA,                   ///< clear an area

	CMD_MONEY_CHEAT,                  ///< do the money cheat
	CMD_CHANGE_BANK_BALANCE,          ///< change bank balance to charge costs or give money from a GS
	CMD_BUILD_CANAL,                  ///< build a canal

	CMD_CREATE_SUBSIDY,               ///< create a new subsidy
	CMD_COMPANY_CTRL,                 ///< used in multiplayer to create a new companies etc.
	CMD_COMPANY_ALLOW_LIST_CTRL, ///< Used in multiplayer to add/remove a client's public key to/from the company's allow list.
	CMD_CUSTOM_NEWS_ITEM,             ///< create a custom news message
	CMD_CREATE_GOAL,                  ///< create a new goal
	CMD_REMOVE_GOAL,                  ///< remove a goal
	CMD_SET_GOAL_DESTINATION,         ///< update goal destination of a goal
	CMD_SET_GOAL_TEXT,                ///< update goal text of a goal
	CMD_SET_GOAL_PROGRESS,            ///< update goal progress text of a goal
	CMD_SET_GOAL_COMPLETED,           ///< update goal completed status of a goal
	CMD_GOAL_QUESTION,                ///< ask a goal related question
	CMD_GOAL_QUESTION_ANSWER,         ///< answer(s) to CMD_GOAL_QUESTION
	CMD_CREATE_STORY_PAGE,            ///< create a new story page
	CMD_CREATE_STORY_PAGE_ELEMENT,    ///< create a new story page element
	CMD_UPDATE_STORY_PAGE_ELEMENT,    ///< update a story page element
	CMD_SET_STORY_PAGE_TITLE,         ///< update title of a story page
	CMD_SET_STORY_PAGE_DATE,          ///< update date of a story page
	CMD_SHOW_STORY_PAGE,              ///< show a story page
	CMD_REMOVE_STORY_PAGE,            ///< remove a story page
	CMD_REMOVE_STORY_PAGE_ELEMENT,    ///< remove a story page element
	CMD_SCROLL_VIEWPORT,              ///< scroll main viewport of players
	CMD_STORY_PAGE_BUTTON,            ///< selection via story page button

	CMD_LEVEL_LAND,                   ///< level land

	CMD_BUILD_LOCK,                   ///< build a lock

	CMD_BUILD_SIGNAL_TRACK,           ///< add signals along a track (by dragging)
	CMD_REMOVE_SIGNAL_TRACK,          ///< remove signals along a track (by dragging)

	CMD_GIVE_MONEY,                   ///< give money to another company
	CMD_CHANGE_SETTING,               ///< change a setting
	CMD_CHANGE_COMPANY_SETTING,       ///< change a company setting

	CMD_SET_AUTOREPLACE,              ///< set an autoreplace entry

	CMD_CLONE_VEHICLE,                ///< clone a vehicle
	CMD_START_STOP_VEHICLE,           ///< start or stop a vehicle
	CMD_MASS_START_STOP,              ///< start/stop all vehicles (in a depot)
	CMD_AUTOREPLACE_VEHICLE,          ///< replace/renew a vehicle while it is in a depot
	CMD_DEPOT_SELL_ALL_VEHICLES,      ///< sell all vehicles which are in a given depot
	CMD_DEPOT_MASS_AUTOREPLACE,       ///< force the autoreplace to take action in a given depot

	CMD_CREATE_GROUP,                 ///< create a new group
	CMD_DELETE_GROUP,                 ///< delete a group
	CMD_ALTER_GROUP,                  ///< alter a group
	CMD_ADD_VEHICLE_GROUP,            ///< add a vehicle to a group
	CMD_ADD_SHARED_VEHICLE_GROUP,     ///< add all other shared vehicles to a group which are missing
	CMD_REMOVE_ALL_VEHICLES_GROUP,    ///< remove all vehicles from a group
	CMD_SET_GROUP_FLAG,               ///< set/clear a flag for a group
	CMD_SET_GROUP_LIVERY,             ///< set the livery for a group

	CMD_MOVE_ORDER,                   ///< move an order
	CMD_CHANGE_TIMETABLE,             ///< change the timetable for a vehicle
	CMD_BULK_CHANGE_TIMETABLE,        ///< change the timetable for all orders of a vehicle
	CMD_SET_VEHICLE_ON_TIME,          ///< set the vehicle on time feature (timetable)
	CMD_AUTOFILL_TIMETABLE,           ///< autofill the timetable
	CMD_SET_TIMETABLE_START,          ///< set the date that a timetable should start

	CMD_OPEN_CLOSE_AIRPORT,           ///< open/close an airport to incoming aircraft

	CMD_CREATE_LEAGUE_TABLE,               ///< create a new league table
	CMD_CREATE_LEAGUE_TABLE_ELEMENT,       ///< create a new element in a league table
	CMD_UPDATE_LEAGUE_TABLE_ELEMENT_DATA,  ///< update the data fields of a league table element
	CMD_UPDATE_LEAGUE_TABLE_ELEMENT_SCORE, ///< update the score of a league table element
	CMD_REMOVE_LEAGUE_TABLE_ELEMENT,       ///< remove a league table element

	CMD_END,                          ///< Must ALWAYS be on the end of this list!! (period)
};

/**
 * List of flags for a command.
 *
 * This enums defines some flags which can be used for the commands.
 */
enum DoCommandFlag : uint8_t {
	Execute, ///< execute the given command
	Auto, ///< don't allow building on structures
	QueryCost, ///< query cost only,  don't build.
	NoWater, ///< don't allow building on water
	NoTestTownRating, ///< town rating does not disallow you from building
	Bankrupt, ///< company bankrupts, skip money check, skip vehicle on tile check in some cases
	AutoReplace, ///< autoreplace/autorenew is in progress, this shall disable vehicle limits when building, and ignore certain restrictions when undoing things (like vehicle attach callback)
	NoCargoCapacityCheck, ///< when autoreplace/autorenew is in progress, this shall prevent truncating the amount of cargo in the vehicle to prevent testing the command to remove cargo
	AllTiles, ///< allow this command also on MP_VOID tiles
	NoModifyTownRating, ///< do not change town rating
	ForceClearTile, ///< do not only remove the object on the tile, but also clear any water left on it
};
using DoCommandFlags = EnumBitSet<DoCommandFlag, uint16_t>;

/**
 * Command flags for the command table _command_proc_table.
 *
 * This enumeration defines flags for the _command_proc_table.
 */
enum class CommandFlag : uint8_t {
	Server, ///< the command can only be initiated by the server
	Spectator, ///< the command may be initiated by a spectator
	Offline, ///< the command cannot be executed in a multiplayer game; single-player only
	Auto, ///< set the DoCommandFlag::Auto flag on this command
	AllTiles, ///< allow this command also on MP_VOID tiles
	NoTest, ///< the command's output may differ between test and execute due to town rating changes etc.
	NoWater, ///< set the DoCommandFlag::NoWater flag on this command
	ClientID, ///< set p2 with the ClientID of the sending client.
	Deity, ///< the command may be executed by COMPANY_DEITY
	StrCtrl, ///< the command's string may contain control strings
	NoEst, ///< the command is never estimated.
	Location, ///< the command has implicit location argument.
};
using CommandFlags = EnumBitSet<CommandFlag, uint16_t>;

/** Types of commands we have. */
enum CommandType : uint8_t {
	CMDT_LANDSCAPE_CONSTRUCTION, ///< Construction and destruction of objects on the map.
	CMDT_VEHICLE_CONSTRUCTION,   ///< Construction, modification (incl. refit) and destruction of vehicles.
	CMDT_MONEY_MANAGEMENT,       ///< Management of money, i.e. loans.
	CMDT_VEHICLE_MANAGEMENT,     ///< Stopping, starting, sending to depot, turning around, replace orders etc.
	CMDT_ROUTE_MANAGEMENT,       ///< Modifications to route management (orders, groups, etc).
	CMDT_OTHER_MANAGEMENT,       ///< Renaming stuff, changing company colours, placing signs, etc.
	CMDT_COMPANY_SETTING,        ///< Changing settings related to a company.
	CMDT_SERVER_SETTING,         ///< Pausing/removing companies/server settings.
	CMDT_CHEAT,                  ///< A cheat of some sorts.

	CMDT_END,                    ///< Magic end marker.
};

/** Different command pause levels. */
enum CommandPauseLevel : uint8_t {
	CMDPL_NO_ACTIONS,      ///< No user actions may be executed.
	CMDPL_NO_CONSTRUCTION, ///< No construction actions may be executed.
	CMDPL_NO_LANDSCAPING,  ///< No landscaping actions may be executed.
	CMDPL_ALL_ACTIONS,     ///< All actions may be executed.
};


template <typename T> struct CommandFunctionTraitHelper;
template <typename... Targs>
struct CommandFunctionTraitHelper<CommandCost(*)(DoCommandFlags, Targs...)> {
	using Args = std::tuple<std::decay_t<Targs>...>;
	using RetTypes = void;
	using CbArgs = Args;
	using CbProcType = void(*)(Commands, const CommandCost &);
};
template <template <typename...> typename Tret, typename... Tretargs, typename... Targs>
struct CommandFunctionTraitHelper<Tret<CommandCost, Tretargs...>(*)(DoCommandFlags, Targs...)> {
	using Args = std::tuple<std::decay_t<Targs>...>;
	using RetTypes = std::tuple<std::decay_t<Tretargs>...>;
	using CbArgs = std::tuple<std::decay_t<Tretargs>..., std::decay_t<Targs>...>;
	using CbProcType = void(*)(Commands, const CommandCost &, Tretargs...);
};

/** Defines the traits of a command. */
template <Commands Tcmd> struct CommandTraits;

#define DEF_CMD_TRAIT(cmd_, proc_, flags_, type_) \
	template <> struct CommandTraits<cmd_> { \
		using ProcType = decltype(&proc_); \
		using Args = typename CommandFunctionTraitHelper<ProcType>::Args; \
		using RetTypes = typename CommandFunctionTraitHelper<ProcType>::RetTypes; \
		using CbArgs = typename CommandFunctionTraitHelper<ProcType>::CbArgs; \
		using RetCallbackProc = typename CommandFunctionTraitHelper<ProcType>::CbProcType; \
		static constexpr Commands cmd = cmd_; \
		static constexpr auto &proc = proc_; \
		static constexpr CommandFlags flags = flags_; \
		static constexpr CommandType type = type_; \
		static inline constexpr const char *name = #proc_; \
	};

/** Storage buffer for serialized command data. */
typedef std::vector<uint8_t> CommandDataBuffer;

/**
 * Define a callback function for the client, after the command is finished.
 *
 * Functions of this type are called after the command is finished. The parameters
 * are from the #CommandProc callback type. The boolean parameter indicates if the
 * command succeeded or failed.
 *
 * @param cmd The command that was executed
 * @param result The result of the executed command
 * @param tile The tile of the command action
 * @see CommandProc
 */
typedef void CommandCallback(Commands cmd, const CommandCost &result, TileIndex tile);

/**
 * Define a callback function for the client, after the command is finished.
 *
 * Functions of this type are called after the command is finished. The parameters
 * are from the #CommandProc callback type. The boolean parameter indicates if the
 * command succeeded or failed.
 *
 * @param cmd The command that was executed
 * @param result The result of the executed command
 * @param tile The tile of the command action
 * @param data Additional data of the command
 * @param result_data Additional returned data from the command
 * @see CommandProc
 */
typedef void CommandCallbackData(Commands cmd, const CommandCost &result, const CommandDataBuffer &data, CommandDataBuffer result_data);

#endif /* COMMAND_TYPE_H */
