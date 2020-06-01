/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file table/settings.h Settings to save in the savegame and config file. */

/* Callback function used in _settings[] as well as _company_settings[] */
static size_t ConvertLandscape(const char *value);


/****************************
 * OTTD specific INI stuff
 ****************************/

/**
 * Settings-macro usage:
 * The list might look daunting at first, but is in general easy to understand.
 * The macros can be grouped depending on where the config variable is
 * stored:
 * 1. SDTG_something
 *    These are for global variables, so this is the one you will use
 *    for a #SettingDescGlobVarList section. Here 'var' refers to a
 *    global variable.
 * 2. SDTC_something
 *    These are for client-only variables. Here the 'var' refers to an
 *    entry inside _settings_client.
 * 3. SDT_something
 *    Thse are for members in the struct described by the current
 *    #SettingDesc list / .ini file.  Here, 'base' specifies type of the
 *    struct while 'var' points out the member of the struct (the actual
 *    struct to store it in is implicitly defined by the #SettingDesc
 *    list / .ini file preamble the entry is in).
 *
 * The something part defines the type of variable to store. There are a
 * lot of types. Easy ones are:
 * - VAR:  any number type, 'type' field specifies what number. eg int8 or uint32
 * - BOOL: a boolean number type
 * - STR:  a string or character. 'type' field specifies what string. Normal, string, or quoted
 * A bit more difficult to use are MMANY (meaning ManyOfMany) and OMANY (OneOfMany)
 * These are actually normal numbers, only bitmasked. In MMANY several bits can
 * be set, in the other only one.
 * If nothing fits you, you can use the GENERAL macros, but it exposes the
 * internal structure somewhat so it needs a little looking. There are _NULL()
 * macros as well, these fill up space so you can add more settings there (in
 * place) and you DON'T have to increase the savegame version.
 *
 * While reading values from openttd.cfg, some values may not be converted
 * properly, for any kind of reasons.  In order to allow a process of self-cleaning
 * mechanism, a callback procedure is made available.  You will have to supply the function, which
 * will work on a string, one function per setting. And of course, enable the callback param
 * on the appropriate macro.
 */

#define NSD_GENERAL(name, def, cmd, guiflags, min, max, interval, many, str, strhelp, strval, proc, load, cat)\
	{name, (const void*)(size_t)(def), cmd, guiflags, min, max, interval, many, str, strhelp, strval, proc, load, cat}

/* Macros for various objects to go in the configuration file.
 * This section is for global variables */
#define SDTG_GENERAL(name, sdt_cmd, sle_cmd, type, flags, guiflags, var, length, def, min, max, interval, full, str, strhelp, strval, proc, from, to, cat)\
	{NSD_GENERAL(name, def, sdt_cmd, guiflags, min, max, interval, full, str, strhelp, strval, proc, nullptr, cat), SLEG_GENERAL(sle_cmd, var, type | flags, length, from, to)}

#define SDTG_VAR(name, type, flags, guiflags, var, def, min, max, interval, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(name, SDT_NUMX, SL_VAR, type, flags, guiflags, var, 0, def, min, max, interval, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTG_BOOL(name, flags, guiflags, var, def, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(name, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, var, 0, def, 0, 1, 0, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTG_LIST(name, type, length, flags, guiflags, var, def, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(name, SDT_INTLIST, SL_ARR, type, flags, guiflags, var, length, def, 0, 0, 0, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTG_STR(name, type, flags, guiflags, var, def, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(name, SDT_STRING, SL_STR, type, flags, guiflags, var, sizeof(var), def, 0, 0, 0, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTG_OMANY(name, type, flags, guiflags, var, def, max, full, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(name, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, var, 0, def, 0, max, 0, full, str, strhelp, strval, proc, from, to, cat)

#define SDTG_MMANY(name, type, flags, guiflags, var, def, full, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(name, SDT_MANYOFMANY, SL_VAR, type, flags, guiflags, var, 0, def, 0, 0, 0, full, str, strhelp, strval, proc, from, to, cat)

#define SDTG_NULL(length, from, to)\
	{{"", nullptr, SDT_NUMX, SGF_NONE, 0, 0, 0, nullptr, STR_NULL, STR_NULL, STR_NULL, nullptr, nullptr, SC_NONE}, SLEG_NULL(length, from, to)}

#define SDTG_END() {{nullptr, nullptr, SDT_NUMX, SGF_NONE, 0, 0, 0, nullptr, STR_NULL, STR_NULL, STR_NULL, nullptr, nullptr, SC_NONE}, SLEG_END()}

/* Macros for various objects to go in the configuration file.
 * This section is for structures where their various members are saved */
#define SDT_GENERAL(name, sdt_cmd, sle_cmd, type, flags, guiflags, base, var, length, def, min, max, interval, full, str, strhelp, strval, proc, load, from, to, cat)\
	{NSD_GENERAL(name, def, sdt_cmd, guiflags, min, max, interval, full, str, strhelp, strval, proc, load, cat), SLE_GENERAL(sle_cmd, base, var, type | flags, length, from, to)}

#define SDT_VAR(base, var, type, flags, guiflags, def, min, max, interval, str, strhelp, strval, proc, from, to, cat)\
	SDT_GENERAL(#var, SDT_NUMX, SL_VAR, type, flags, guiflags, base, var, 1, def, min, max, interval, nullptr, str, strhelp, strval, proc, nullptr, from, to, cat)

#define SDT_BOOL(base, var, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDT_GENERAL(#var, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, base, var, 1, def, 0, 1, 0, nullptr, str, strhelp, strval, proc, nullptr, from, to, cat)

#define SDT_LIST(base, var, type, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDT_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, base, var, lengthof(((base*)8)->var), def, 0, 0, 0, nullptr, str, strhelp, strval, proc, nullptr, from, to, cat)

#define SDT_STR(base, var, type, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDT_GENERAL(#var, SDT_STRING, SL_STR, type, flags, guiflags, base, var, sizeof(((base*)8)->var), def, 0, 0, 0, nullptr, str, strhelp, strval, proc, nullptr, from, to, cat)

#define SDT_CHR(base, var, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDT_GENERAL(#var, SDT_STRING, SL_VAR, SLE_CHAR, flags, guiflags, base, var, 1, def, 0, 0, 0, nullptr, str, strhelp, strval, proc, nullptr, from, to, cat)

#define SDT_OMANY(base, var, type, flags, guiflags, def, max, full, str, strhelp, strval, proc, from, to, load, cat)\
	SDT_GENERAL(#var, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, base, var, 1, def, 0, max, 0, full, str, strhelp, strval, proc, load, from, to, cat)

#define SDT_MMANY(base, var, type, flags, guiflags, def, full, str, proc, strhelp, strval, from, to, cat)\
	SDT_GENERAL(#var, SDT_MANYOFMANY, SL_VAR, type, flags, guiflags, base, var, 1, def, 0, 0, 0, full, str, strhelp, strval, proc, nullptr, from, to, cat)

#define SDT_NULL(length, from, to)\
	{{"", nullptr, SDT_NUMX, SGF_NONE, 0, 0, 0, nullptr, STR_NULL, STR_NULL, STR_NULL, nullptr, nullptr, SC_NONE}, SLE_CONDNULL(length, from, to)}


#define SDTC_VAR(var, type, flags, guiflags, def, min, max, interval, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(#var, SDT_NUMX, SL_VAR, type, flags, guiflags, _settings_client.var, 1, def, min, max, interval, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTC_BOOL(var, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(#var, SDT_BOOLX, SL_VAR, SLE_BOOL, flags, guiflags, _settings_client.var, 1, def, 0, 1, 0, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTC_LIST(var, type, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(#var, SDT_INTLIST, SL_ARR, type, flags, guiflags, _settings_client.var, lengthof(_settings_client.var), def, 0, 0, 0, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTC_STR(var, type, flags, guiflags, def, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(#var, SDT_STRING, SL_STR, type, flags, guiflags, _settings_client.var, sizeof(_settings_client.var), def, 0, 0, 0, nullptr, str, strhelp, strval, proc, from, to, cat)

#define SDTC_OMANY(var, type, flags, guiflags, def, max, full, str, strhelp, strval, proc, from, to, cat)\
	SDTG_GENERAL(#var, SDT_ONEOFMANY, SL_VAR, type, flags, guiflags, _settings_client.var, 1, def, 0, max, 0, full, str, strhelp, strval, proc, from, to, cat)

#define SDT_END() {{nullptr, nullptr, SDT_NUMX, SGF_NONE, 0, 0, 0, nullptr, STR_NULL, STR_NULL, STR_NULL, nullptr, nullptr, SC_NONE}, SLE_END()}

static bool CheckInterval(int32 p1);
static bool InvalidateDetailsWindow(int32 p1);
static bool UpdateIntervalTrains(int32 p1);
static bool UpdateIntervalRoadVeh(int32 p1);
static bool UpdateIntervalShips(int32 p1);
static bool UpdateIntervalAircraft(int32 p1);

static const SettingDesc _company_settings[] = {
SDT_BOOL(CompanySettings, engine_renew,        0, SGF_PER_COMPANY, false,                        STR_CONFIG_SETTING_AUTORENEW_VEHICLE, STR_CONFIG_SETTING_AUTORENEW_VEHICLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDT_VAR(CompanySettings, engine_renew_months, SLE_INT16, 0, SGF_PER_COMPANY | SGF_DISPLAY_ABS, 6, -12, 12, 0, STR_CONFIG_SETTING_AUTORENEW_MONTHS, STR_CONFIG_SETTING_AUTORENEW_MONTHS_HELPTEXT, STR_CONFIG_SETTING_AUTORENEW_MONTHS_VALUE_BEFORE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(CompanySettings, engine_renew_money, SLE_UINT, 0, SGF_PER_COMPANY | SGF_CURRENCY, 100000, 0, 2000000, 0, STR_CONFIG_SETTING_AUTORENEW_MONEY, STR_CONFIG_SETTING_AUTORENEW_MONEY_HELPTEXT, STR_JUST_CURRENCY_LONG, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_BOOL(CompanySettings, renew_keep_length,        0, SGF_PER_COMPANY, false,                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_BOOL(CompanySettings, vehicle.servint_ispercent,        0, SGF_PER_COMPANY, false,                        STR_CONFIG_SETTING_SERVINT_ISPERCENT, STR_CONFIG_SETTING_SERVINT_ISPERCENT_HELPTEXT, STR_NULL, CheckInterval, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(CompanySettings, vehicle.servint_trains, SLE_UINT16, 0, SGF_PER_COMPANY | SGF_0ISDISABLED, 150, 5, 800, 0, STR_CONFIG_SETTING_SERVINT_TRAINS, STR_CONFIG_SETTING_SERVINT_TRAINS_HELPTEXT, STR_CONFIG_SETTING_SERVINT_VALUE, UpdateIntervalTrains, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(CompanySettings, vehicle.servint_roadveh, SLE_UINT16, 0, SGF_PER_COMPANY | SGF_0ISDISABLED, 150, 5, 800, 0, STR_CONFIG_SETTING_SERVINT_ROAD_VEHICLES, STR_CONFIG_SETTING_SERVINT_ROAD_VEHICLES_HELPTEXT, STR_CONFIG_SETTING_SERVINT_VALUE, UpdateIntervalRoadVeh, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(CompanySettings, vehicle.servint_ships, SLE_UINT16, 0, SGF_PER_COMPANY | SGF_0ISDISABLED, 360, 5, 800, 0, STR_CONFIG_SETTING_SERVINT_SHIPS, STR_CONFIG_SETTING_SERVINT_SHIPS_HELPTEXT, STR_CONFIG_SETTING_SERVINT_VALUE, UpdateIntervalShips, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(CompanySettings, vehicle.servint_aircraft, SLE_UINT16, 0, SGF_PER_COMPANY | SGF_0ISDISABLED, 100, 5, 800, 0, STR_CONFIG_SETTING_SERVINT_AIRCRAFT, STR_CONFIG_SETTING_SERVINT_AIRCRAFT_HELPTEXT, STR_CONFIG_SETTING_SERVINT_VALUE, UpdateIntervalAircraft, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_END()
};
static const SettingDesc _currency_settings[] = {
SDT_VAR(CurrencySpec, rate, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 1, 0, UINT16_MAX, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_CHR(CurrencySpec, separator,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, ".",                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDT_VAR(CurrencySpec, to_euro, SLE_INT32, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0, MIN_YEAR, MAX_YEAR, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_STR(CurrencySpec, prefix, SLE_STRBQ, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_STR(CurrencySpec, suffix, SLE_STRBQ, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, " credits",                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_END()
};
static const uint GAME_DIFFICULTY_NUM = 18;
static uint16 _old_diff_custom[GAME_DIFFICULTY_NUM];
uint8 _old_diff_level;                                 ///< Old difficulty level from old savegames
uint8 _old_units;                                      ///< Old units from old savegames

/* Most of these strings are used both for gameopt-backward compatibility
 * and the settings tables. The rest is here for consistency. */
static const char *_locale_currencies = "GBP|USD|EUR|YEN|ATS|BEF|CHF|CZK|DEM|DKK|ESP|FIM|FRF|GRD|HUF|ISK|ITL|NLG|NOK|PLN|RON|RUR|SIT|SEK|YTL|SKK|BRL|EEK|custom";
static const char *_locale_units = "imperial|metric|si";
static const char *_town_names = "english|french|german|american|latin|silly|swedish|dutch|finnish|polish|slovak|norwegian|hungarian|austrian|romanian|czech|swiss|danish|turkish|italian|catalan";
static const char *_climates = "temperate|arctic|tropic|toyland";
static const char *_autosave_interval = "off|monthly|quarterly|half year|yearly";
static const char *_roadsides = "left|right";
static const char *_savegame_date = "long|short|iso";
static const char *_server_langs = "ANY|ENGLISH|GERMAN|FRENCH|BRAZILIAN|BULGARIAN|CHINESE|CZECH|DANISH|DUTCH|ESPERANTO|FINNISH|HUNGARIAN|ICELANDIC|ITALIAN|JAPANESE|KOREAN|LITHUANIAN|NORWEGIAN|POLISH|PORTUGUESE|ROMANIAN|RUSSIAN|SLOVAK|SLOVENIAN|SPANISH|SWEDISH|TURKISH|UKRAINIAN|AFRIKAANS|CROATIAN|CATALAN|ESTONIAN|GALICIAN|GREEK|LATVIAN";
static const char *_osk_activation = "disabled|double|single|immediately";
static const char *_settings_profiles = "easy|medium|hard";
static const char *_news_display = "off|summarized|full";

static const SettingDesc _gameopt_settings[] = {
/* In version 4 a new difficulty setting has been added to the difficulty settings,
 * town attitude towards demolishing. Needs special handling because some dimwit thought
 * it funny to have the GameDifficulty struct be an array while it is a struct of
 * same-sized members
 * XXX - To save file-space and since values are never bigger than about 10? only
 * save the first 16 bits in the savegame. Question is why the values are still int32
 * and why not byte for example?
 * 'SLE_FILE_I16 | SLE_VAR_U16' in "diff_custom" is needed to get around SlArray() hack
 * for savegames version 0 - though it is an array, it has to go through the byteswap process */
SDTG_GENERAL("diff_custom", SDT_INTLIST, SL_ARR, SLE_FILE_I16 | SLE_VAR_U16, SLF_NOT_IN_CONFIG, SGF_NONE, _old_diff_custom, 17, 0, 0, 0, 0, nullptr, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_4, SC_ADVANCED),
SDTG_GENERAL("diff_custom", SDT_INTLIST, SL_ARR, SLE_UINT16, SLF_NOT_IN_CONFIG, SGF_NONE, _old_diff_custom, 18, 0, 0, 0, 0, nullptr, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_4, SL_MAX_VERSION, SC_ADVANCED),
SDTG_VAR("diff_level",                     SLE_UINT8, SLF_NOT_IN_CONFIG, SGF_NONE, _old_diff_level,          SP_CUSTOM, SP_EASY, SP_CUSTOM, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDT_OMANY(GameSettings, locale.currency, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       CURRENCY_END - 1, _locale_currencies,            STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, nullptr, SC_BASIC),
SDTG_OMANY("units",       SLE_UINT8, SLF_NOT_IN_CONFIG, SGF_NONE, _old_units, 1, 2, _locale_units,            STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_OMANY(GameSettings, game_creation.town_name, SLE_UINT8, 0, SGF_NONE, 0,       255, _town_names,            STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, game_creation.landscape, SLE_UINT8, 0, SGF_NONE, 0,       3, _climates,            STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, ConvertLandscape, SC_BASIC),
SDT_VAR(GameSettings, game_creation.snow_line_height, SLE_UINT8, 0, SGF_NONE, DEF_SNOWLINE_HEIGHT * TILE_HEIGHT, MIN_SNOWLINE_HEIGHT * TILE_HEIGHT, MAX_SNOWLINE_HEIGHT * TILE_HEIGHT,        0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_22,        SC_ADVANCED),
SDT_NULL(1, SLV_22, SLV_165),
SDT_NULL(1, SL_MIN_VERSION, SLV_23),
SDTC_OMANY(       gui.autosave, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 1,       4, _autosave_interval,            STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_23, SL_MAX_VERSION,        SC_BASIC),
SDT_OMANY(GameSettings, vehicle.road_side, SLE_UINT8, 0, SGF_NONE, 1,       1, _roadsides,            STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_END()
};
extern char _config_language_file[MAX_PATH];

static const char *_support8bppmodes = "no|system|hardware";

static const SettingDescGlobVarList _misc_settings[] = {
SDTG_MMANY("display_opt", SLE_UINT8,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _display_opt, (1 << DO_SHOW_TOWN_NAMES | 1 << DO_SHOW_STATION_NAMES | 1 << DO_SHOW_SIGNS | 1 << DO_FULL_ANIMATION | 1 << DO_FULL_DETAIL | 1 << DO_SHOW_WAYPOINT_NAMES | 1 << DO_SHOW_COMPETITOR_SIGNS),                        "SHOW_TOWN_NAMES|SHOW_STATION_NAMES|SHOW_SIGNS|FULL_ANIMATION||FULL_DETAIL|WAYPOINTS|SHOW_COMPETITOR_SIGNS", STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDTG_BOOL("fullscreen",                 SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _fullscreen, false,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_OMANY("support8bpp", SLE_UINT8,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _support8bpp, 0,       2,            _support8bppmodes, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_STR("graphicsset", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, BaseGraphics::ini_set, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_STR("soundsset", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, BaseSounds::ini_set, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_STR("musicset", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, BaseMusic::ini_set, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_STR("videodriver", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _ini_videodriver, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_STR("musicdriver", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _ini_musicdriver, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_STR("sounddriver", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _ini_sounddriver, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_STR("blitter", SLE_STRQ,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _ini_blitter, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDTG_STR("language", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _config_language_file, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_LIST("resolution", SLE_INT, 2, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _cur_resolution, "640,480",                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_STR("screenshot_format", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _screenshot_format_name, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_STR("savegame_format", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _savegame_format, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_BOOL("rightclick_emulate",                 SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _rightclick_emulate, false,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#ifdef HAS_TRUETYPE_FONT
SDTG_STR("small_font", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.small.font, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_STR("medium_font", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.medium.font, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_STR("large_font", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.large.font, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_STR("mono_font", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.mono.font, nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_VAR("small_size", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.small.size, 0, 0, 72, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_VAR("medium_size", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.medium.size, 0, 0, 72, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_VAR("large_size", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.large.size, 0, 0, 72, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_VAR("mono_size", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.mono.size, 0, 0, 72, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_BOOL("small_aa",                 SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.small.aa, false,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_BOOL("medium_aa",                 SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.medium.aa, false,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_BOOL("large_aa",                 SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.large.aa, false,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
#ifdef HAS_TRUETYPE_FONT
SDTG_BOOL("mono_aa",                 SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _freetype.mono.aa, false,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
#endif
SDTG_VAR("sprite_cache_size_px", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _sprite_cache_size, 128, 1, 512, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_VAR("player_face", SLE_UINT32,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _company_manager_face, 0, 0, 0xFFFFFFFF, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_VAR("transparency_options", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _transparency_opt, 0, 0, 0x1FF, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_VAR("transparency_locks", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _transparency_lock, 0, 0, 0x1FF, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_VAR("invisibility_options", SLE_UINT,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _invisibility_opt, 0, 0, 0xFF, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_STR("keyboard", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _keyboard_opt[0], nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_STR("keyboard_caps", SLE_STRB,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _keyboard_opt[1], nullptr,                               STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_VAR("last_newgrf_count", SLE_UINT32,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _settings_client.gui.last_newgrf_count, 100, 0, UINT32_MAX, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_VAR("gui_zoom", SLE_UINT8,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _gui_zoom, ZOOM_LVL_OUT_4X, ZOOM_LVL_MIN, ZOOM_LVL_OUT_4X, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_VAR("font_zoom", SLE_UINT8,          SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _font_zoom, ZOOM_LVL_OUT_4X, ZOOM_LVL_MIN, ZOOM_LVL_OUT_4X, 0,        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_END()
};
/* Begin - Callback Functions for the various settings */
static bool v_PositionMainToolbar(int32 p1);
static bool v_PositionStatusbar(int32 p1);
static bool PopulationInLabelActive(int32 p1);
static bool RedrawScreen(int32 p1);
static bool RedrawSmallmap(int32 p1);
static bool StationSpreadChanged(int32 p1);
static bool InvalidateBuildIndustryWindow(int32 p1);
static bool CloseSignalGUI(int32 p1);
static bool InvalidateTownViewWindow(int32 p1);
static bool DeleteSelectStationWindow(int32 p1);
static bool UpdateConsists(int32 p1);
static bool TrainAccelerationModelChanged(int32 p1);
static bool RoadVehAccelerationModelChanged(int32 p1);
static bool TrainSlopeSteepnessChanged(int32 p1);
static bool RoadVehSlopeSteepnessChanged(int32 p1);
static bool DragSignalsDensityChanged(int32);
static bool TownFoundingChanged(int32 p1);
static bool DifficultyNoiseChange(int32 i);
static bool MaxNoAIsChange(int32 i);
static bool CheckRoadSide(int p1);
static bool ChangeMaxHeightLevel(int32 p1);
static bool CheckFreeformEdges(int32 p1);
static bool ChangeDynamicEngines(int32 p1);
static bool StationCatchmentChanged(int32 p1);
static bool InvalidateVehTimetableWindow(int32 p1);
static bool InvalidateCompanyLiveryWindow(int32 p1);
static bool InvalidateNewGRFChangeWindows(int32 p1);
static bool InvalidateIndustryViewWindow(int32 p1);
static bool InvalidateAISettingsWindow(int32 p1);
static bool RedrawTownAuthority(int32 p1);
static bool InvalidateCompanyInfrastructureWindow(int32 p1);
static bool InvalidateCompanyWindow(int32 p1);
static bool ZoomMinMaxChanged(int32 p1);
static bool MaxVehiclesChanged(int32 p1);
static bool InvalidateShipPathCache(int32 p1);

static bool UpdateClientName(int32 p1);
static bool UpdateServerPassword(int32 p1);
static bool UpdateRconPassword(int32 p1);
static bool UpdateClientConfigValues(int32 p1);

extern int32 _old_ending_year_slv_105;

/* End - Callback Functions for the various settings */

/* Some settings do not need to be synchronised when playing in multiplayer.
 * These include for example the GUI settings and will not be saved with the
 * savegame.
 * It is also a bit tricky since you would think that service_interval
 * for example doesn't need to be synched. Every client assigns the
 * service_interval value to the v->service_interval, meaning that every client
 * assigns his value. If the setting was company-based, that would mean that
 * vehicles could decide on different moments that they are heading back to a
 * service depot, causing desyncs on a massive scale. */
const SettingDesc _settings[] = {
SDT_VAR(GameSettings, difficulty.max_no_competitors, SLE_UINT8, 0, SGF_NONE, 0,       0, MAX_COMPANIES - 1, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, MaxNoAIsChange, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_NULL(1, SLV_97, SLV_110),
SDT_VAR(GameSettings, difficulty.number_towns, SLE_UINT8, 0, SGF_NEWGAME_ONLY, 2,       0, 4, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NUM_VERY_LOW, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.industry_density, SLE_UINT8, 0, SGF_MULTISTRING, ID_END - 1,       0, ID_END - 1, 1, STR_CONFIG_SETTING_INDUSTRY_DENSITY, STR_CONFIG_SETTING_INDUSTRY_DENSITY_HELPTEXT, STR_FUNDING_ONLY, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.max_loan, SLE_UINT32, 0, SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO | SGF_CURRENCY, 300000,       100000, 500000, 50000, STR_CONFIG_SETTING_MAXIMUM_INITIAL_LOAN, STR_CONFIG_SETTING_MAXIMUM_INITIAL_LOAN_HELPTEXT, STR_JUST_CURRENCY_LONG, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.initial_interest, SLE_UINT8, 0, SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO, 2,       2, 4, 1, STR_CONFIG_SETTING_INTEREST_RATE, STR_CONFIG_SETTING_INTEREST_RATE_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, nullptr, SLV_97, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, difficulty.vehicle_costs, SLE_UINT8, 0, SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO | SGF_MULTISTRING, 0,       0, 2, 1, STR_CONFIG_SETTING_RUNNING_COSTS, STR_CONFIG_SETTING_RUNNING_COSTS_HELPTEXT, STR_SEA_LEVEL_LOW, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.competitor_speed, SLE_UINT8, 0, SGF_MULTISTRING, 2,       0, 4, 1, STR_CONFIG_SETTING_CONSTRUCTION_SPEED, STR_CONFIG_SETTING_CONSTRUCTION_SPEED_HELPTEXT, STR_AI_SPEED_VERY_SLOW, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_NULL(1, SLV_97, SLV_110),
SDT_VAR(GameSettings, difficulty.vehicle_breakdowns, SLE_UINT8, 0, SGF_MULTISTRING, 1,       0, 2, 1, STR_CONFIG_SETTING_VEHICLE_BREAKDOWNS, STR_CONFIG_SETTING_VEHICLE_BREAKDOWNS_HELPTEXT, STR_DISASTER_NONE, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.subsidy_multiplier, SLE_UINT8, 0, SGF_MULTISTRING, 2,       0, 3, 1, STR_CONFIG_SETTING_SUBSIDY_MULTIPLIER, STR_CONFIG_SETTING_SUBSIDY_MULTIPLIER_HELPTEXT, STR_SUBSIDY_X1_5, nullptr, SLV_97, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, difficulty.construction_cost, SLE_UINT8, 0, SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO | SGF_MULTISTRING, 0,       0, 2, 1, STR_CONFIG_SETTING_CONSTRUCTION_COSTS, STR_CONFIG_SETTING_CONSTRUCTION_COSTS_HELPTEXT, STR_SEA_LEVEL_LOW, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.terrain_type, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY, 1,       0, 4, 1, STR_CONFIG_SETTING_TERRAIN_TYPE, STR_CONFIG_SETTING_TERRAIN_TYPE_HELPTEXT, STR_TERRAIN_TYPE_VERY_FLAT, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.quantity_sea_lakes, SLE_UINT8, 0, SGF_NEWGAME_ONLY, 0,       0, 4, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_SEA_LEVEL_VERY_LOW, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, difficulty.economy,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_RECESSIONS, STR_CONFIG_SETTING_RECESSIONS_HELPTEXT, STR_NULL, nullptr, SLV_97, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, difficulty.line_reverse_mode,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_TRAIN_REVERSING, STR_CONFIG_SETTING_TRAIN_REVERSING_HELPTEXT, STR_NULL, nullptr, SLV_97, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, difficulty.disasters,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_DISASTERS, STR_CONFIG_SETTING_DISASTERS_HELPTEXT, STR_NULL, nullptr, SLV_97, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, difficulty.town_council_tolerance, SLE_UINT8, 0, SGF_MULTISTRING, 0,       0, 2, 1, STR_CONFIG_SETTING_CITY_APPROVAL, STR_CONFIG_SETTING_CITY_APPROVAL_HELPTEXT, STR_CITY_APPROVAL_PERMISSIVE, DifficultyNoiseChange, SLV_97, SL_MAX_VERSION,        SC_ADVANCED),
SDTG_VAR("diff_level",       SLE_UINT8, SLF_NOT_IN_CONFIG, SGF_NONE, _old_diff_level, 3, 0, 3, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_97, SLV_178,        SC_BASIC),
SDT_OMANY(GameSettings, game_creation.town_name, SLE_UINT8, 0, SGF_NO_NETWORK, 0,             255, _town_names,     STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_97, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, game_creation.landscape, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY, 0,             3, _climates,     STR_CONFIG_SETTING_LANDSCAPE, STR_CONFIG_SETTING_LANDSCAPE_HELPTEXT, STR_CHEAT_SWITCH_CLIMATE_TEMPERATE_LANDSCAPE, nullptr, SLV_97, SL_MAX_VERSION, ConvertLandscape, SC_BASIC),
SDT_NULL(1, SLV_97, SLV_164),
SDT_OMANY(GameSettings, vehicle.road_side, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NO_NETWORK, 1,             1, _roadsides,     STR_CONFIG_SETTING_ROAD_SIDE, STR_CONFIG_SETTING_ROAD_SIDE_HELPTEXT, STR_GAME_OPTIONS_ROAD_VEHICLES_DROPDOWN_LEFT, CheckRoadSide, SLV_97, SL_MAX_VERSION, nullptr, SC_ADVANCED),
SDT_VAR(GameSettings, construction.max_heightlevel, SLE_UINT8, 0, SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO, DEF_MAX_HEIGHTLEVEL,       MIN_MAX_HEIGHTLEVEL, MAX_MAX_HEIGHTLEVEL, 1, STR_CONFIG_SETTING_MAX_HEIGHTLEVEL, STR_CONFIG_SETTING_MAX_HEIGHTLEVEL_HELPTEXT, STR_JUST_INT, ChangeMaxHeightLevel, SLV_194, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, construction.build_on_slopes,        0, SGF_NO_NETWORK, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.command_pause_level, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NO_NETWORK, 1,       0, 3, 1, STR_CONFIG_SETTING_COMMAND_PAUSE_LEVEL, STR_CONFIG_SETTING_COMMAND_PAUSE_LEVEL_HELPTEXT, STR_CONFIG_SETTING_COMMAND_PAUSE_LEVEL_NO_ACTIONS, nullptr, SLV_154, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, construction.terraform_per_64k_frames, SLE_UINT32, 0, SGF_NONE, 64 << 16,       0, 1 << 30, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_156, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.terraform_frame_burst, SLE_UINT16, 0, SGF_NONE, 4096,       0, 1 << 30, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_156, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.clear_per_64k_frames, SLE_UINT32, 0, SGF_NONE, 64 << 16,       0, 1 << 30, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_156, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.clear_frame_burst, SLE_UINT16, 0, SGF_NONE, 4096,       0, 1 << 30, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_156, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.tree_per_64k_frames, SLE_UINT32, 0, SGF_NONE, 64 << 16,       0, 1 << 30, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_175, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.tree_frame_burst, SLE_UINT16, 0, SGF_NONE, 4096,       0, 1 << 30, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_175, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, construction.autoslope,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_AUTOSLOPE, STR_CONFIG_SETTING_AUTOSLOPE_HELPTEXT, STR_NULL, nullptr, SLV_75, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, construction.extra_dynamite,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_EXTRADYNAMITE, STR_CONFIG_SETTING_EXTRADYNAMITE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, construction.max_bridge_length, SLE_UINT16, 0, SGF_NO_NETWORK, 64,       1, MAX_MAP_SIZE, 1, STR_CONFIG_SETTING_MAX_BRIDGE_LENGTH, STR_CONFIG_SETTING_MAX_BRIDGE_LENGTH_HELPTEXT, STR_CONFIG_SETTING_TILE_LENGTH, nullptr, SLV_159, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, construction.max_bridge_height, SLE_UINT8, 0, SGF_NO_NETWORK, 12,       1, MAX_TILE_HEIGHT, 1, STR_CONFIG_SETTING_MAX_BRIDGE_HEIGHT, STR_CONFIG_SETTING_MAX_BRIDGE_HEIGHT_HELPTEXT, STR_JUST_COMMA, nullptr, SLV_194, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, construction.max_tunnel_length, SLE_UINT16, 0, SGF_NO_NETWORK, 64,       1, MAX_MAP_SIZE, 1, STR_CONFIG_SETTING_MAX_TUNNEL_LENGTH, STR_CONFIG_SETTING_MAX_TUNNEL_LENGTH_HELPTEXT, STR_CONFIG_SETTING_TILE_LENGTH, nullptr, SLV_159, SL_MAX_VERSION,        SC_ADVANCED),
SDT_NULL(1, SL_MIN_VERSION, SLV_159),
SDT_VAR(GameSettings, construction.train_signal_side, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NO_NETWORK, 1,       0, 2, 0, STR_CONFIG_SETTING_SIGNALSIDE, STR_CONFIG_SETTING_SIGNALSIDE_HELPTEXT, STR_CONFIG_SETTING_SIGNALSIDE_LEFT, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, station.never_expire_airports,        0, SGF_NO_NETWORK, false,                              STR_CONFIG_SETTING_NEVER_EXPIRE_AIRPORTS, STR_CONFIG_SETTING_NEVER_EXPIRE_AIRPORTS_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, economy.town_layout, SLE_UINT8, 0, SGF_MULTISTRING, TL_ORIGINAL,       TL_BEGIN, NUM_TLS - 1, 1, STR_CONFIG_SETTING_TOWN_LAYOUT, STR_CONFIG_SETTING_TOWN_LAYOUT_HELPTEXT, STR_CONFIG_SETTING_TOWN_LAYOUT_DEFAULT, TownFoundingChanged, SLV_59, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, economy.allow_town_roads,        0, SGF_NO_NETWORK, true,                              STR_CONFIG_SETTING_ALLOW_TOWN_ROADS, STR_CONFIG_SETTING_ALLOW_TOWN_ROADS_HELPTEXT, STR_NULL, nullptr, SLV_113, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, economy.found_town, SLE_UINT8, 0, SGF_MULTISTRING, TF_FORBIDDEN,       TF_BEGIN, TF_END - 1, 1, STR_CONFIG_SETTING_TOWN_FOUNDING, STR_CONFIG_SETTING_TOWN_FOUNDING_HELPTEXT, STR_CONFIG_SETTING_TOWN_FOUNDING_FORBIDDEN, TownFoundingChanged, SLV_128, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, economy.allow_town_level_crossings,        0, SGF_NO_NETWORK, true,                              STR_CONFIG_SETTING_ALLOW_TOWN_LEVEL_CROSSINGS, STR_CONFIG_SETTING_ALLOW_TOWN_LEVEL_CROSSINGS_HELPTEXT, STR_NULL, nullptr, SLV_143, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, economy.town_cargogen_mode, SLE_UINT8, 0, SGF_MULTISTRING, TCGM_BITCOUNT,       TCGM_BEGIN, TCGM_END - 1, 1, STR_CONFIG_SETTING_TOWN_CARGOGENMODE, STR_CONFIG_SETTING_TOWN_CARGOGENMODE_HELPTEXT, STR_CONFIG_SETTING_TOWN_CARGOGENMODE_ORIGINAL, nullptr, SLV_TOWN_CARGOGEN, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.recalc_interval, SLE_UINT16, 0, SGF_NONE, 4,       2, 32, 2, STR_CONFIG_SETTING_LINKGRAPH_INTERVAL, STR_CONFIG_SETTING_LINKGRAPH_INTERVAL_HELPTEXT, STR_JUST_COMMA, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.recalc_time, SLE_UINT16, 0, SGF_NONE, 16,       1, 4096, 1, STR_CONFIG_SETTING_LINKGRAPH_TIME, STR_CONFIG_SETTING_LINKGRAPH_TIME_HELPTEXT, STR_JUST_COMMA, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.distribution_pax, SLE_UINT8, 0, SGF_MULTISTRING, DT_MANUAL,       DT_MIN, DT_MAX, 1, STR_CONFIG_SETTING_DISTRIBUTION_PAX, STR_CONFIG_SETTING_DISTRIBUTION_PAX_HELPTEXT, STR_CONFIG_SETTING_DISTRIBUTION_MANUAL, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.distribution_mail, SLE_UINT8, 0, SGF_MULTISTRING, DT_MANUAL,       DT_MIN, DT_MAX, 1, STR_CONFIG_SETTING_DISTRIBUTION_MAIL, STR_CONFIG_SETTING_DISTRIBUTION_MAIL_HELPTEXT, STR_CONFIG_SETTING_DISTRIBUTION_MANUAL, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.distribution_armoured, SLE_UINT8, 0, SGF_MULTISTRING, DT_MANUAL,       DT_MIN, DT_MAX, 1, STR_CONFIG_SETTING_DISTRIBUTION_ARMOURED, STR_CONFIG_SETTING_DISTRIBUTION_ARMOURED_HELPTEXT, STR_CONFIG_SETTING_DISTRIBUTION_MANUAL, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.distribution_default, SLE_UINT8, 0, SGF_MULTISTRING, DT_MANUAL,       DT_BEGIN, DT_MAX_NONSYMMETRIC, 1, STR_CONFIG_SETTING_DISTRIBUTION_DEFAULT, STR_CONFIG_SETTING_DISTRIBUTION_DEFAULT_HELPTEXT, STR_CONFIG_SETTING_DISTRIBUTION_MANUAL, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.accuracy, SLE_UINT8, 0, SGF_NONE, 16,       2, 64, 1, STR_CONFIG_SETTING_LINKGRAPH_ACCURACY, STR_CONFIG_SETTING_LINKGRAPH_ACCURACY_HELPTEXT, STR_JUST_COMMA, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.demand_distance, SLE_UINT8, 0, SGF_NONE, 100,       0, 255, 5, STR_CONFIG_SETTING_DEMAND_DISTANCE, STR_CONFIG_SETTING_DEMAND_DISTANCE_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.demand_size, SLE_UINT8, 0, SGF_NONE, 100,       0, 100, 5, STR_CONFIG_SETTING_DEMAND_SIZE, STR_CONFIG_SETTING_DEMAND_SIZE_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, linkgraph.short_path_saturation, SLE_UINT8, 0, SGF_NONE, 80,       0, 250, 5, STR_CONFIG_SETTING_SHORT_PATH_SATURATION, STR_CONFIG_SETTING_SHORT_PATH_SATURATION_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, nullptr, SLV_183, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, vehicle.train_acceleration_model, SLE_UINT8, 0, SGF_MULTISTRING, 1,       0, 1, 1, STR_CONFIG_SETTING_TRAIN_ACCELERATION_MODEL, STR_CONFIG_SETTING_TRAIN_ACCELERATION_MODEL_HELPTEXT, STR_CONFIG_SETTING_ORIGINAL, TrainAccelerationModelChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, vehicle.roadveh_acceleration_model, SLE_UINT8, 0, SGF_MULTISTRING, 1,       0, 1, 1, STR_CONFIG_SETTING_ROAD_VEHICLE_ACCELERATION_MODEL, STR_CONFIG_SETTING_ROAD_VEHICLE_ACCELERATION_MODEL_HELPTEXT, STR_CONFIG_SETTING_ORIGINAL, RoadVehAccelerationModelChanged, SLV_139, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, vehicle.train_slope_steepness, SLE_UINT8, 0, SGF_NONE, 3,       0, 10, 1, STR_CONFIG_SETTING_TRAIN_SLOPE_STEEPNESS, STR_CONFIG_SETTING_TRAIN_SLOPE_STEEPNESS_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, TrainSlopeSteepnessChanged, SLV_133, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, vehicle.roadveh_slope_steepness, SLE_UINT8, 0, SGF_NONE, 7,       0, 10, 1, STR_CONFIG_SETTING_ROAD_VEHICLE_SLOPE_STEEPNESS, STR_CONFIG_SETTING_ROAD_VEHICLE_SLOPE_STEEPNESS_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, RoadVehSlopeSteepnessChanged, SLV_139, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.forbid_90_deg,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_FORBID_90_DEG, STR_CONFIG_SETTING_FORBID_90_DEG_HELPTEXT, STR_NULL, InvalidateShipPathCache, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, vehicle.max_train_length, SLE_UINT8, 0, SGF_NONE, 7,       1, 64, 1, STR_CONFIG_SETTING_TRAIN_LENGTH, STR_CONFIG_SETTING_TRAIN_LENGTH_HELPTEXT, STR_CONFIG_SETTING_TILE_LENGTH, nullptr, SLV_159, SL_MAX_VERSION,        SC_BASIC),
SDT_NULL(1, SL_MIN_VERSION, SLV_159),
SDT_VAR(GameSettings, vehicle.smoke_amount, SLE_UINT8, 0, SGF_MULTISTRING, 1,       0, 2, 0, STR_CONFIG_SETTING_SMOKE_AMOUNT, STR_CONFIG_SETTING_SMOKE_AMOUNT_HELPTEXT, STR_CONFIG_SETTING_NONE, nullptr, SLV_145, SL_MAX_VERSION,        SC_ADVANCED),
SDT_NULL(1, SL_MIN_VERSION, SLV_159),
SDT_BOOL(GameSettings, pf.roadveh_queue,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.new_pathfinding_all,        0, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_87,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.yapf.ship_use_yapf,        0, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SLV_87,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.yapf.road_use_yapf,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SLV_87,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.yapf.rail_use_yapf,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SLV_87,        SC_EXPERT),
SDT_VAR(GameSettings, pf.pathfinder_for_trains, SLE_UINT8, 0, SGF_MULTISTRING, 2,       1, 2, 1, STR_CONFIG_SETTING_PATHFINDER_FOR_TRAINS, STR_CONFIG_SETTING_PATHFINDER_FOR_TRAINS_HELPTEXT, STR_CONFIG_SETTING_PATHFINDER_NPF, nullptr, SLV_87, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.pathfinder_for_roadvehs, SLE_UINT8, 0, SGF_MULTISTRING, 2,       1, 2, 1, STR_CONFIG_SETTING_PATHFINDER_FOR_ROAD_VEHICLES, STR_CONFIG_SETTING_PATHFINDER_FOR_ROAD_VEHICLES_HELPTEXT, STR_CONFIG_SETTING_PATHFINDER_NPF, nullptr, SLV_87, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.pathfinder_for_ships, SLE_UINT8, 0, SGF_MULTISTRING, 2,       1, 2, 1, STR_CONFIG_SETTING_PATHFINDER_FOR_SHIPS, STR_CONFIG_SETTING_PATHFINDER_FOR_SHIPS_HELPTEXT, STR_CONFIG_SETTING_PATHFINDER_NPF, InvalidateShipPathCache, SLV_87, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, vehicle.never_expire_vehicles,        0, SGF_NO_NETWORK, false,                              STR_CONFIG_SETTING_NEVER_EXPIRE_VEHICLES, STR_CONFIG_SETTING_NEVER_EXPIRE_VEHICLES_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, vehicle.max_trains, SLE_UINT16, 0, SGF_NONE, 500,       0, 5000, 0, STR_CONFIG_SETTING_MAX_TRAINS, STR_CONFIG_SETTING_MAX_TRAINS_HELPTEXT, STR_JUST_COMMA, MaxVehiclesChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, vehicle.max_roadveh, SLE_UINT16, 0, SGF_NONE, 500,       0, 5000, 0, STR_CONFIG_SETTING_MAX_ROAD_VEHICLES, STR_CONFIG_SETTING_MAX_ROAD_VEHICLES_HELPTEXT, STR_JUST_COMMA, MaxVehiclesChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, vehicle.max_aircraft, SLE_UINT16, 0, SGF_NONE, 200,       0, 5000, 0, STR_CONFIG_SETTING_MAX_AIRCRAFT, STR_CONFIG_SETTING_MAX_AIRCRAFT_HELPTEXT, STR_JUST_COMMA, MaxVehiclesChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, vehicle.max_ships, SLE_UINT16, 0, SGF_NONE, 300,       0, 5000, 0, STR_CONFIG_SETTING_MAX_SHIPS, STR_CONFIG_SETTING_MAX_SHIPS_HELPTEXT, STR_JUST_COMMA, MaxVehiclesChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTG_BOOL(nullptr,              0, SGF_NO_NETWORK, _old_vds.servint_ispercent, false,                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_120,        SC_ADVANCED),
SDTG_VAR(nullptr,       SLE_UINT16, 0, SGF_0ISDISABLED, _old_vds.servint_trains, 150, 5, 800, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_120,        SC_ADVANCED),
SDTG_VAR(nullptr,       SLE_UINT16, 0, SGF_0ISDISABLED, _old_vds.servint_roadveh, 150, 5, 800, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_120,        SC_ADVANCED),
SDTG_VAR(nullptr,       SLE_UINT16, 0, SGF_0ISDISABLED, _old_vds.servint_ships, 360, 5, 800, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_120,        SC_ADVANCED),
SDTG_VAR(nullptr,       SLE_UINT16, 0, SGF_0ISDISABLED, _old_vds.servint_aircraft, 150, 5, 800, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_120,        SC_ADVANCED),
SDT_BOOL(GameSettings, order.no_servicing_if_no_breakdowns,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_NOSERVICE, STR_CONFIG_SETTING_NOSERVICE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, vehicle.wagon_speed_limits,        0, SGF_NO_NETWORK, true,                              STR_CONFIG_SETTING_WAGONSPEEDLIMITS, STR_CONFIG_SETTING_WAGONSPEEDLIMITS_HELPTEXT, STR_NULL, UpdateConsists, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, vehicle.disable_elrails,        0, SGF_NO_NETWORK, false,                              STR_CONFIG_SETTING_DISABLE_ELRAILS, STR_CONFIG_SETTING_DISABLE_ELRAILS_HELPTEXT, STR_NULL, SettingsDisableElrail, SLV_38, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, vehicle.freight_trains, SLE_UINT8, 0, SGF_NO_NETWORK, 1,       1, 255, 1, STR_CONFIG_SETTING_FREIGHT_TRAINS, STR_CONFIG_SETTING_FREIGHT_TRAINS_HELPTEXT, STR_JUST_COMMA, UpdateConsists, SLV_39, SL_MAX_VERSION,        SC_ADVANCED),
SDT_NULL(1, SLV_67, SLV_159),
SDT_VAR(GameSettings, vehicle.plane_speed, SLE_UINT8, 0, SGF_NO_NETWORK, 4,       1, 4, 0, STR_CONFIG_SETTING_PLANE_SPEED, STR_CONFIG_SETTING_PLANE_SPEED_HELPTEXT, STR_CONFIG_SETTING_PLANE_SPEED_VALUE, nullptr, SLV_90, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, vehicle.dynamic_engines,        0, SGF_NO_NETWORK, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, ChangeDynamicEngines, SLV_95, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, vehicle.plane_crashes, SLE_UINT8, 0, SGF_MULTISTRING, 2,       0, 2, 1, STR_CONFIG_SETTING_PLANE_CRASHES, STR_CONFIG_SETTING_PLANE_CRASHES_HELPTEXT, STR_CONFIG_SETTING_PLANE_CRASHES_NONE, nullptr, SLV_138, SL_MAX_VERSION,        SC_BASIC),
SDT_NULL(1, SL_MIN_VERSION, SLV_159),
SDTC_BOOL(       gui.sg_full_load_any,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_22, SLV_93,        SC_ADVANCED),
SDT_BOOL(GameSettings, order.improved_load,        0, SGF_NO_NETWORK, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, order.selectgoods,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       gui.sg_new_nonstop,        0, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_22, SLV_93,        SC_ADVANCED),
SDT_NULL(1, SL_MIN_VERSION, SLV_159),
SDT_VAR(GameSettings, station.station_spread, SLE_UINT8, 0, SGF_NONE, 12,       4, 64, 0, STR_CONFIG_SETTING_STATION_SPREAD, STR_CONFIG_SETTING_STATION_SPREAD_HELPTEXT, STR_CONFIG_SETTING_TILE_LENGTH, StationSpreadChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, order.serviceathelipad,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_SERVICEATHELIPAD, STR_CONFIG_SETTING_SERVICEATHELIPAD_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, station.modified_catchment,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_CATCHMENT, STR_CONFIG_SETTING_CATCHMENT_HELPTEXT, STR_NULL, StationCatchmentChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, station.serve_neutral_industries,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_SERVE_NEUTRAL_INDUSTRIES, STR_CONFIG_SETTING_SERVE_NEUTRAL_INDUSTRIES_HELPTEXT, STR_NULL, StationCatchmentChanged, SLV_SERVE_NEUTRAL_INDUSTRIES, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, order.gradual_loading,        0, SGF_NO_NETWORK, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_40, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, construction.road_stop_on_town_road,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_STOP_ON_TOWN_ROAD, STR_CONFIG_SETTING_STOP_ON_TOWN_ROAD_HELPTEXT, STR_NULL, nullptr, SLV_47, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, construction.road_stop_on_competitor_road,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_STOP_ON_COMPETITOR_ROAD, STR_CONFIG_SETTING_STOP_ON_COMPETITOR_ROAD_HELPTEXT, STR_NULL, nullptr, SLV_114, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, station.adjacent_stations,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_62, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, economy.station_noise_level,        0, SGF_NO_NETWORK, false,                              STR_CONFIG_SETTING_NOISE_LEVEL, STR_CONFIG_SETTING_NOISE_LEVEL_HELPTEXT, STR_NULL, InvalidateTownViewWindow, SLV_96, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, station.distant_join_stations,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_DISTANT_JOIN_STATIONS, STR_CONFIG_SETTING_DISTANT_JOIN_STATIONS_HELPTEXT, STR_NULL, DeleteSelectStationWindow, SLV_106, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, economy.inflation,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_INFLATION, STR_CONFIG_SETTING_INFLATION_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, construction.raw_industry_construction, SLE_UINT8, 0, SGF_MULTISTRING, 0,       0, 2, 0, STR_CONFIG_SETTING_RAW_INDUSTRY_CONSTRUCTION_METHOD, STR_CONFIG_SETTING_RAW_INDUSTRY_CONSTRUCTION_METHOD_HELPTEXT, STR_CONFIG_SETTING_RAW_INDUSTRY_CONSTRUCTION_METHOD_NONE, InvalidateBuildIndustryWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, construction.industry_platform, SLE_UINT8, 0, SGF_NONE, 1,       0, 4, 0, STR_CONFIG_SETTING_INDUSTRY_PLATFORM, STR_CONFIG_SETTING_INDUSTRY_PLATFORM_HELPTEXT, STR_CONFIG_SETTING_TILE_LENGTH, nullptr, SLV_148, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, economy.multiple_industry_per_town,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_MULTIPINDTOWN, STR_CONFIG_SETTING_MULTIPINDTOWN_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_NULL(1, SL_MIN_VERSION, SLV_141),
SDT_BOOL(GameSettings, economy.bribe,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_BRIBE, STR_CONFIG_SETTING_BRIBE_HELPTEXT, STR_NULL, RedrawTownAuthority, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, economy.exclusive_rights,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_ALLOW_EXCLUSIVE, STR_CONFIG_SETTING_ALLOW_EXCLUSIVE_HELPTEXT, STR_NULL, RedrawTownAuthority, SLV_79, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, economy.fund_buildings,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_ALLOW_FUND_BUILDINGS, STR_CONFIG_SETTING_ALLOW_FUND_BUILDINGS_HELPTEXT, STR_NULL, RedrawTownAuthority, SLV_165, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, economy.fund_roads,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_ALLOW_FUND_ROAD, STR_CONFIG_SETTING_ALLOW_FUND_ROAD_HELPTEXT, STR_NULL, RedrawTownAuthority, SLV_160, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, economy.give_money,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_ALLOW_GIVE_MONEY, STR_CONFIG_SETTING_ALLOW_GIVE_MONEY_HELPTEXT, STR_NULL, nullptr, SLV_79, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.snow_line_height, SLE_UINT8, 0, SGF_NO_NETWORK, DEF_SNOWLINE_HEIGHT,       MIN_SNOWLINE_HEIGHT, MAX_SNOWLINE_HEIGHT, 1, STR_CONFIG_SETTING_SNOWLINE_HEIGHT, STR_CONFIG_SETTING_SNOWLINE_HEIGHT_HELPTEXT, STR_JUST_COMMA, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_NULL(4, SL_MIN_VERSION, SLV_144),
SDT_VAR(GameSettings, game_creation.starting_year, SLE_INT32, 0, SGF_NONE, DEF_START_YEAR,       MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_SETTING_STARTING_YEAR, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_JUST_INT, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTG_VAR("old_ending_year_slv_105",       SLE_INT32, SLF_NOT_IN_CONFIG, SGF_NONE, _old_ending_year_slv_105, DEF_END_YEAR, MIN_YEAR, MAX_YEAR, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SLV_105,        SC_ADVANCED),
SDT_VAR(GameSettings, game_creation.ending_year, SLE_INT32, 0, SGF_0ISDISABLED, DEF_END_YEAR,       MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_SETTING_ENDING_YEAR, STR_CONFIG_SETTING_ENDING_YEAR_HELPTEXT, STR_CONFIG_SETTING_ENDING_YEAR_VALUE, nullptr, SLV_ENDING_YEAR, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, economy.smooth_economy,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_SMOOTH_ECONOMY, STR_CONFIG_SETTING_SMOOTH_ECONOMY_HELPTEXT, STR_NULL, InvalidateIndustryViewWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, economy.allow_shares,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_ALLOW_SHARES, STR_CONFIG_SETTING_ALLOW_SHARES_HELPTEXT, STR_NULL, InvalidateCompanyWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, economy.min_years_for_shares, SLE_UINT8, 0, SGF_NONE, 6,       0, 255, 1, STR_CONFIG_SETTING_MIN_YEARS_FOR_SHARES, STR_CONFIG_SETTING_MIN_YEARS_FOR_SHARES_HELPTEXT, STR_JUST_INT, nullptr, SLV_TRADING_AGE, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, economy.feeder_payment_share, SLE_UINT8, 0, SGF_NONE, 75,       0, 100, 0, STR_CONFIG_SETTING_FEEDER_PAYMENT_SHARE, STR_CONFIG_SETTING_FEEDER_PAYMENT_SHARE_HELPTEXT, STR_CONFIG_SETTING_PERCENTAGE, nullptr, SLV_134, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, economy.town_growth_rate, SLE_UINT8, 0, SGF_MULTISTRING, 2,       0, 4, 0, STR_CONFIG_SETTING_TOWN_GROWTH, STR_CONFIG_SETTING_TOWN_GROWTH_HELPTEXT, STR_CONFIG_SETTING_TOWN_GROWTH_NONE, nullptr, SLV_54, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, economy.larger_towns, SLE_UINT8, 0, SGF_0ISDISABLED, 4,       0, 255, 1, STR_CONFIG_SETTING_LARGER_TOWNS, STR_CONFIG_SETTING_LARGER_TOWNS_HELPTEXT, STR_CONFIG_SETTING_LARGER_TOWNS_VALUE, nullptr, SLV_54, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, economy.initial_city_size, SLE_UINT8, 0, SGF_NONE, 2,       1, 10, 1, STR_CONFIG_SETTING_CITY_SIZE_MULTIPLIER, STR_CONFIG_SETTING_CITY_SIZE_MULTIPLIER_HELPTEXT, STR_JUST_COMMA, nullptr, SLV_56, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, economy.mod_road_rebuild,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_77, SL_MAX_VERSION,        SC_EXPERT),
SDT_NULL(1, SL_MIN_VERSION, SLV_107),
SDT_OMANY(GameSettings, script.settings_profile, SLE_UINT8, 0, SGF_MULTISTRING, SP_EASY,             SP_HARD, _settings_profiles,     STR_CONFIG_SETTING_AI_PROFILE, STR_CONFIG_SETTING_AI_PROFILE_HELPTEXT, STR_CONFIG_SETTING_AI_PROFILE_EASY, nullptr, SLV_178, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_BOOL(GameSettings, ai.ai_in_multiplayer,        0, SGF_NONE, true,                              STR_CONFIG_SETTING_AI_IN_MULTIPLAYER, STR_CONFIG_SETTING_AI_IN_MULTIPLAYER_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, ai.ai_disable_veh_train,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_AI_BUILDS_TRAINS, STR_CONFIG_SETTING_AI_BUILDS_TRAINS_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, ai.ai_disable_veh_roadveh,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_AI_BUILDS_ROAD_VEHICLES, STR_CONFIG_SETTING_AI_BUILDS_ROAD_VEHICLES_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, ai.ai_disable_veh_aircraft,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_AI_BUILDS_AIRCRAFT, STR_CONFIG_SETTING_AI_BUILDS_AIRCRAFT_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_BOOL(GameSettings, ai.ai_disable_veh_ship,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_AI_BUILDS_SHIPS, STR_CONFIG_SETTING_AI_BUILDS_SHIPS_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, script.script_max_opcode_till_suspend, SLE_UINT32, 0, SGF_NEWGAME_ONLY, 10000,       500, 250000, 2500, STR_CONFIG_SETTING_SCRIPT_MAX_OPCODES, STR_CONFIG_SETTING_SCRIPT_MAX_OPCODES_HELPTEXT, STR_JUST_COMMA, nullptr, SLV_107, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, script.script_max_memory_megabytes, SLE_UINT32, 0, SGF_NEWGAME_ONLY, 1024,       8, 8192, 8, STR_CONFIG_SETTING_SCRIPT_MAX_MEMORY, STR_CONFIG_SETTING_SCRIPT_MAX_MEMORY_HELPTEXT, STR_CONFIG_SETTING_SCRIPT_MAX_MEMORY_VALUE, nullptr, SLV_SCRIPT_MEMLIMIT, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, vehicle.extend_vehicle_life, SLE_UINT8, 0, SGF_NONE, 0,       0, 100, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, economy.dist_local_authority, SLE_UINT8, 0, SGF_NONE, 20,       5, 60, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.reverse_at_signals,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_REVERSE_AT_SIGNALS, STR_CONFIG_SETTING_REVERSE_AT_SIGNALS_HELPTEXT, STR_NULL, nullptr, SLV_159, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, pf.wait_oneway_signal, SLE_UINT8, 0, SGF_NONE, 15,       2, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.wait_twoway_signal, SLE_UINT8, 0, SGF_NONE, 41,       2, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, economy.town_noise_population[0], SLE_UINT16, 0, SGF_NONE, 800,       200, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_96, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, economy.town_noise_population[1], SLE_UINT16, 0, SGF_NONE, 2000,       400, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_96, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, economy.town_noise_population[2], SLE_UINT16, 0, SGF_NONE, 4000,       800, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_96, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, economy.infrastructure_maintenance,        0, SGF_NONE, false,                              STR_CONFIG_SETTING_INFRASTRUCTURE_MAINTENANCE, STR_CONFIG_SETTING_INFRASTRUCTURE_MAINTENANCE_HELPTEXT, STR_NULL, InvalidateCompanyInfrastructureWindow, SLV_166, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, pf.wait_for_pbs_path, SLE_UINT8, 0, SGF_NONE, 30,       2, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.reserve_paths,        0, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.path_backoff_interval, SLE_UINT8, 0, SGF_NONE, 20,       1, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_NULL(3, SL_MIN_VERSION, SLV_REMOVE_OPF),
SDT_VAR(GameSettings, pf.npf.npf_max_search_nodes, SLE_UINT, 0, SGF_NONE, 10000,       500, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_firstred_penalty, SLE_UINT, 0, SGF_NONE, 10 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_firstred_exit_penalty, SLE_UINT, 0, SGF_NONE, 100 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_lastred_penalty, SLE_UINT, 0, SGF_NONE, 10 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_station_penalty, SLE_UINT, 0, SGF_NONE, 1 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_slope_penalty, SLE_UINT, 0, SGF_NONE, 1 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_curve_penalty, SLE_UINT, 0, SGF_NONE, 1 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_depot_reverse_penalty, SLE_UINT, 0, SGF_NONE, 50 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_pbs_cross_penalty, SLE_UINT, 0, SGF_NONE, 3 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_rail_pbs_signal_back_penalty, SLE_UINT, 0, SGF_NONE, 15 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_buoy_penalty, SLE_UINT, 0, SGF_NONE, 2 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_water_curve_penalty, SLE_UINT, 0, SGF_NONE, 1 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_road_curve_penalty, SLE_UINT, 0, SGF_NONE, 1 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_crossing_penalty, SLE_UINT, 0, SGF_NONE, 3 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_road_drive_through_penalty, SLE_UINT, 0, SGF_NONE, 8 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_47, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_road_dt_occupied_penalty, SLE_UINT, 0, SGF_NONE, 8 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_130, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.npf_road_bay_occupied_penalty, SLE_UINT, 0, SGF_NONE, 15 * NPF_TILE_LENGTH,       0, 100000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_130, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.npf.maximum_go_to_depot_penalty, SLE_UINT, 0, SGF_NONE, 20 * NPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_131, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.yapf.disable_node_optimization,        0, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.max_search_nodes, SLE_UINT, 0, SGF_NONE, 10000,       500, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_BOOL(GameSettings, pf.yapf.rail_firstred_twoway_eol,        0, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_firstred_penalty, SLE_UINT, 0, SGF_NONE, 10 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_firstred_exit_penalty, SLE_UINT, 0, SGF_NONE, 100 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_lastred_penalty, SLE_UINT, 0, SGF_NONE, 10 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_lastred_exit_penalty, SLE_UINT, 0, SGF_NONE, 100 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_station_penalty, SLE_UINT, 0, SGF_NONE, 10 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_slope_penalty, SLE_UINT, 0, SGF_NONE, 2 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_curve45_penalty, SLE_UINT, 0, SGF_NONE, 1 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_curve90_penalty, SLE_UINT, 0, SGF_NONE, 6 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_depot_reverse_penalty, SLE_UINT, 0, SGF_NONE, 50 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_crossing_penalty, SLE_UINT, 0, SGF_NONE, 3 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_look_ahead_max_signals, SLE_UINT, 0, SGF_NONE, 10,       1, 100, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_look_ahead_signal_p0, SLE_INT, 0, SGF_NONE, 500,       -1000000, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_look_ahead_signal_p1, SLE_INT, 0, SGF_NONE, -100,       -1000000, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_look_ahead_signal_p2, SLE_INT, 0, SGF_NONE, 5,       -1000000, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_28, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_pbs_cross_penalty, SLE_UINT, 0, SGF_NONE, 3 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_pbs_station_penalty, SLE_UINT, 0, SGF_NONE, 8 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_pbs_signal_back_penalty, SLE_UINT, 0, SGF_NONE, 15 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_doubleslip_penalty, SLE_UINT, 0, SGF_NONE, 1 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_100, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_longer_platform_penalty, SLE_UINT, 0, SGF_NONE, 8 * YAPF_TILE_LENGTH,       0, 20000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_longer_platform_per_tile_penalty, SLE_UINT, 0, SGF_NONE, 0 * YAPF_TILE_LENGTH,       0, 20000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_shorter_platform_penalty, SLE_UINT, 0, SGF_NONE, 40 * YAPF_TILE_LENGTH,       0, 20000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.rail_shorter_platform_per_tile_penalty, SLE_UINT, 0, SGF_NONE, 0 * YAPF_TILE_LENGTH,       0, 20000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.road_slope_penalty, SLE_UINT, 0, SGF_NONE, 2 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.road_curve_penalty, SLE_UINT, 0, SGF_NONE, 1 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.road_crossing_penalty, SLE_UINT, 0, SGF_NONE, 3 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_33, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.road_stop_penalty, SLE_UINT, 0, SGF_NONE, 8 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_47, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.road_stop_occupied_penalty, SLE_UINT, 0, SGF_NONE, 8 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_130, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.road_stop_bay_occupied_penalty, SLE_UINT, 0, SGF_NONE, 15 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_130, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.maximum_go_to_depot_penalty, SLE_UINT, 0, SGF_NONE, 20 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_131, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.ship_curve45_penalty, SLE_UINT, 0, SGF_NONE, 1 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_SHIP_CURVE_PENALTY, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, pf.yapf.ship_curve90_penalty, SLE_UINT, 0, SGF_NONE, 6 * YAPF_TILE_LENGTH,       0, 1000000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_SHIP_CURVE_PENALTY, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, game_creation.land_generator, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY, 1,       0, 1, 0, STR_CONFIG_SETTING_LAND_GENERATOR, STR_CONFIG_SETTING_LAND_GENERATOR_HELPTEXT, STR_CONFIG_SETTING_LAND_GENERATOR_ORIGINAL, nullptr, SLV_30, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, game_creation.oil_refinery_limit, SLE_UINT8, 0, SGF_NONE, 32,       12, 48, 0, STR_CONFIG_SETTING_OIL_REF_EDGE_DISTANCE, STR_CONFIG_SETTING_OIL_REF_EDGE_DISTANCE_HELPTEXT, STR_CONFIG_SETTING_TILE_LENGTH, nullptr, SLV_30, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, game_creation.tgen_smoothness, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY, 1,       TGEN_SMOOTHNESS_BEGIN, TGEN_SMOOTHNESS_END - 1, 0, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_HELPTEXT, STR_CONFIG_SETTING_ROUGHNESS_OF_TERRAIN_VERY_SMOOTH, nullptr, SLV_30, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.variety, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY, 0,       0, 5, 0, STR_CONFIG_SETTING_VARIETY, STR_CONFIG_SETTING_VARIETY_HELPTEXT, STR_VARIETY_NONE, nullptr, SLV_197, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, game_creation.generation_seed, SLE_UINT32, 0, SGF_NONE, GENERATE_NEW_SEED,       0, UINT32_MAX, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_30, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, game_creation.tree_placer, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY | SGF_SCENEDIT_TOO, 2,       0, 2, 0, STR_CONFIG_SETTING_TREE_PLACER, STR_CONFIG_SETTING_TREE_PLACER_HELPTEXT, STR_CONFIG_SETTING_TREE_PLACER_NONE, nullptr, SLV_30, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, game_creation.heightmap_rotation, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 1, 0, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_CONFIG_SETTING_HEIGHTMAP_ROTATION_COUNTER_CLOCKWISE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.se_flat_world_height, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 1,       0, 15, 0, STR_CONFIG_SETTING_SE_FLAT_WORLD_HEIGHT, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_JUST_COMMA, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.map_x, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 8,       MIN_MAP_SIZE_BITS, MAX_MAP_SIZE_BITS, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.map_y, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 8,       MIN_MAP_SIZE_BITS, MAX_MAP_SIZE_BITS, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDT_BOOL(GameSettings, construction.freeform_edges,        0, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, CheckFreeformEdges, SLV_111, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, game_creation.water_borders, SLE_UINT8, 0, SGF_NONE, 15,       0, 16, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_111, SL_MAX_VERSION,        SC_ADVANCED),
SDT_VAR(GameSettings, game_creation.custom_town_number, SLE_UINT16, 0, SGF_NONE, 1,       1, 5000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_115, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, construction.extra_tree_placement, SLE_UINT8, 0, SGF_MULTISTRING, 2,       0, 2, 0, STR_CONFIG_SETTING_EXTRA_TREE_PLACEMENT, STR_CONFIG_SETTING_EXTRA_TREE_PLACEMENT_HELPTEXT, STR_CONFIG_SETTING_EXTRA_TREE_PLACEMENT_NONE, nullptr, SLV_132, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.custom_sea_level, SLE_UINT8, 0, SGF_NONE, CUSTOM_SEA_LEVEL_MIN_PERCENTAGE,       CUSTOM_SEA_LEVEL_MIN_PERCENTAGE, CUSTOM_SEA_LEVEL_MAX_PERCENTAGE, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_149, SL_MAX_VERSION,        SC_BASIC),
SDT_VAR(GameSettings, game_creation.min_river_length, SLE_UINT8, 0, SGF_NONE, 16,       2, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_163, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, game_creation.river_route_random, SLE_UINT8, 0, SGF_NONE, 5,       1, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SLV_163, SL_MAX_VERSION,        SC_EXPERT),
SDT_VAR(GameSettings, game_creation.amount_of_rivers, SLE_UINT8, 0, SGF_MULTISTRING | SGF_NEWGAME_ONLY, 2,       0, 3, 0, STR_CONFIG_SETTING_RIVER_AMOUNT, STR_CONFIG_SETTING_RIVER_AMOUNT_HELPTEXT, STR_RIVERS_NONE, nullptr, SLV_163, SL_MAX_VERSION,        SC_ADVANCED),
SDT_OMANY(GameSettings, locale.currency, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_NONE, 0,             CURRENCY_END - 1, _locale_currencies,     STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, RedrawScreen, SLV_97, SL_MAX_VERSION, nullptr, SC_BASIC),
SDTG_OMANY("units",       SLE_UINT8, SLF_NOT_IN_CONFIG, SGF_NONE, _old_units, 1,       2, _locale_units,     STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, RedrawScreen, SLV_97, SLV_184,        SC_BASIC),
SDT_OMANY(GameSettings, locale.units_velocity, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _locale_units,     STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY, STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_HELPTEXT, STR_CONFIG_SETTING_LOCALISATION_UNITS_VELOCITY_IMPERIAL, RedrawScreen, SLV_184, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, locale.units_power, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _locale_units,     STR_CONFIG_SETTING_LOCALISATION_UNITS_POWER, STR_CONFIG_SETTING_LOCALISATION_UNITS_POWER_HELPTEXT, STR_CONFIG_SETTING_LOCALISATION_UNITS_POWER_IMPERIAL, RedrawScreen, SLV_184, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, locale.units_weight, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _locale_units,     STR_CONFIG_SETTING_LOCALISATION_UNITS_WEIGHT, STR_CONFIG_SETTING_LOCALISATION_UNITS_WEIGHT_HELPTEXT, STR_CONFIG_SETTING_LOCALISATION_UNITS_WEIGHT_IMPERIAL, RedrawScreen, SLV_184, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, locale.units_volume, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _locale_units,     STR_CONFIG_SETTING_LOCALISATION_UNITS_VOLUME, STR_CONFIG_SETTING_LOCALISATION_UNITS_VOLUME_HELPTEXT, STR_CONFIG_SETTING_LOCALISATION_UNITS_VOLUME_IMPERIAL, RedrawScreen, SLV_184, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, locale.units_force, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _locale_units,     STR_CONFIG_SETTING_LOCALISATION_UNITS_FORCE, STR_CONFIG_SETTING_LOCALISATION_UNITS_FORCE_HELPTEXT, STR_CONFIG_SETTING_LOCALISATION_UNITS_FORCE_IMPERIAL, RedrawScreen, SLV_184, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_OMANY(GameSettings, locale.units_height, SLE_UINT8, SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _locale_units,     STR_CONFIG_SETTING_LOCALISATION_UNITS_HEIGHT, STR_CONFIG_SETTING_LOCALISATION_UNITS_HEIGHT_HELPTEXT, STR_CONFIG_SETTING_LOCALISATION_UNITS_HEIGHT_IMPERIAL, RedrawScreen, SLV_184, SL_MAX_VERSION, nullptr, SC_BASIC),
SDT_STR(GameSettings, locale.digit_group_separator, SLE_STRQ, SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, RedrawScreen, SLV_118, SL_MAX_VERSION,        SC_BASIC),
SDT_STR(GameSettings, locale.digit_group_separator_currency, SLE_STRQ, SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, RedrawScreen, SLV_118, SL_MAX_VERSION,        SC_BASIC),
SDT_STR(GameSettings, locale.digit_decimal_separator, SLE_STRQ, SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, RedrawScreen, SLV_126, SL_MAX_VERSION,        SC_BASIC),
SDTC_OMANY(       gui.autosave, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             4, _autosave_interval,     STR_CONFIG_SETTING_AUTOSAVE, STR_CONFIG_SETTING_AUTOSAVE_HELPTEXT, STR_GAME_OPTIONS_AUTOSAVE_DROPDOWN_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.threaded_saves,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_OMANY(       gui.date_format_in_default_names, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,             2, _savegame_date,     STR_CONFIG_SETTING_DATE_FORMAT_IN_SAVE_NAMES, STR_CONFIG_SETTING_DATE_FORMAT_IN_SAVE_NAMES_HELPTEXT, STR_CONFIG_SETTING_DATE_FORMAT_IN_SAVE_NAMES_LONG, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.show_finances,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SHOWFINANCES, STR_CONFIG_SETTING_SHOWFINANCES_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.auto_scrolling, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 3, 0, STR_CONFIG_SETTING_AUTOSCROLL, STR_CONFIG_SETTING_AUTOSCROLL_HELPTEXT, STR_CONFIG_SETTING_AUTOSCROLL_DISABLED, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.scroll_mode, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 3, 0, STR_CONFIG_SETTING_SCROLLMODE, STR_CONFIG_SETTING_SCROLLMODE_HELPTEXT, STR_CONFIG_SETTING_SCROLLMODE_DEFAULT, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.smooth_scroll,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_SMOOTH_SCROLLING, STR_CONFIG_SETTING_SMOOTH_SCROLLING_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.right_mouse_wnd_close,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_RIGHT_MOUSE_WND_CLOSE, STR_CONFIG_SETTING_RIGHT_MOUSE_WND_CLOSE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.measure_tooltip,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_MEASURE_TOOLTIP, STR_CONFIG_SETTING_MEASURE_TOOLTIP_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.errmsg_duration, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 5,       0, 20, 0, STR_CONFIG_SETTING_ERRMSG_DURATION, STR_CONFIG_SETTING_ERRMSG_DURATION_HELPTEXT, STR_CONFIG_SETTING_ERRMSG_DURATION_VALUE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.hover_delay_ms, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED, 250,       50, 6000, 50, STR_CONFIG_SETTING_HOVER_DELAY, STR_CONFIG_SETTING_HOVER_DELAY_HELPTEXT, STR_CONFIG_SETTING_HOVER_DELAY_VALUE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       gui.osk_activation, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             3, _osk_activation,     STR_CONFIG_SETTING_OSK_ACTIVATION, STR_CONFIG_SETTING_OSK_ACTIVATION_HELPTEXT, STR_CONFIG_SETTING_OSK_ACTIVATION_DISABLED, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.toolbar_pos, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,       0, 2, 0, STR_CONFIG_SETTING_TOOLBAR_POS, STR_CONFIG_SETTING_TOOLBAR_POS_HELPTEXT, STR_CONFIG_SETTING_HORIZONTAL_POS_LEFT, v_PositionMainToolbar, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.statusbar_pos, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,       0, 2, 0, STR_CONFIG_SETTING_STATUSBAR_POS, STR_CONFIG_SETTING_STATUSBAR_POS_HELPTEXT, STR_CONFIG_SETTING_HORIZONTAL_POS_LEFT, v_PositionStatusbar, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.window_snap_radius, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED, 10,       1, 32, 0, STR_CONFIG_SETTING_SNAP_RADIUS, STR_CONFIG_SETTING_SNAP_RADIUS_HELPTEXT, STR_CONFIG_SETTING_SNAP_RADIUS_VALUE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.window_soft_limit, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED, 20,       5, 255, 1, STR_CONFIG_SETTING_SOFT_LIMIT, STR_CONFIG_SETTING_SOFT_LIMIT_HELPTEXT, STR_CONFIG_SETTING_SOFT_LIMIT_VALUE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.zoom_min, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, ZOOM_LVL_MIN,       ZOOM_LVL_MIN, ZOOM_LVL_OUT_4X, 0, STR_CONFIG_SETTING_ZOOM_MIN, STR_CONFIG_SETTING_ZOOM_MIN_HELPTEXT, STR_CONFIG_SETTING_ZOOM_LVL_MIN, ZoomMinMaxChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.zoom_max, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, ZOOM_LVL_MAX,       ZOOM_LVL_OUT_8X, ZOOM_LVL_MAX, 0, STR_CONFIG_SETTING_ZOOM_MAX, STR_CONFIG_SETTING_ZOOM_MAX_HELPTEXT, STR_CONFIG_SETTING_ZOOM_LVL_OUT_2X, ZoomMinMaxChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.population_in_label,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_POPULATION_IN_LABEL, STR_CONFIG_SETTING_POPULATION_IN_LABEL_HELPTEXT, STR_NULL, PopulationInLabelActive, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.link_terraform_toolbar,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_LINK_TERRAFORM_TOOLBAR, STR_CONFIG_SETTING_LINK_TERRAFORM_TOOLBAR_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.smallmap_land_colour, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 2, 0, STR_CONFIG_SETTING_SMALLMAP_LAND_COLOUR, STR_CONFIG_SETTING_SMALLMAP_LAND_COLOUR_HELPTEXT, STR_CONFIG_SETTING_SMALLMAP_LAND_COLOUR_GREEN, RedrawSmallmap, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.liveries, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,       0, 2, 0, STR_CONFIG_SETTING_LIVERIES, STR_CONFIG_SETTING_LIVERIES_HELPTEXT, STR_CONFIG_SETTING_LIVERIES_NONE, InvalidateCompanyLiveryWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.starting_colour, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, COLOUR_END,       0, COLOUR_END, 0, STR_CONFIG_SETTING_COMPANY_STARTING_COLOUR, STR_CONFIG_SETTING_COMPANY_STARTING_COLOUR_HELPTEXT, STR_COLOUR_DARK_BLUE, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.prefer_teamchat,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_PREFER_TEAMCHAT, STR_CONFIG_SETTING_PREFER_TEAMCHAT_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.scrollwheel_scrolling, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 2, 0, STR_CONFIG_SETTING_SCROLLWHEEL_SCROLLING, STR_CONFIG_SETTING_SCROLLWHEEL_SCROLLING_HELPTEXT, STR_CONFIG_SETTING_SCROLLWHEEL_ZOOM, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.scrollwheel_multiplier, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 5,       1, 15, 1, STR_CONFIG_SETTING_SCROLLWHEEL_MULTIPLIER, STR_CONFIG_SETTING_SCROLLWHEEL_MULTIPLIER_HELPTEXT, STR_JUST_COMMA, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.pause_on_newgame,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_PAUSE_ON_NEW_GAME, STR_CONFIG_SETTING_PAUSE_ON_NEW_GAME_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.advanced_vehicle_list, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,       0, 2, 0, STR_CONFIG_SETTING_ADVANCED_VEHICLE_LISTS, STR_CONFIG_SETTING_ADVANCED_VEHICLE_LISTS_HELPTEXT, STR_CONFIG_SETTING_COMPANIES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.timetable_in_ticks,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_TIMETABLE_IN_TICKS, STR_CONFIG_SETTING_TIMETABLE_IN_TICKS_HELPTEXT, STR_NULL, InvalidateVehTimetableWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       gui.timetable_arrival_departure,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_TIMETABLE_SHOW_ARRIVAL_DEPARTURE, STR_CONFIG_SETTING_TIMETABLE_SHOW_ARRIVAL_DEPARTURE_HELPTEXT, STR_NULL, InvalidateVehTimetableWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.quick_goto,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_QUICKGOTO, STR_CONFIG_SETTING_QUICKGOTO_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.loading_indicators, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,       0, 2, 0, STR_CONFIG_SETTING_LOADING_INDICATORS, STR_CONFIG_SETTING_LOADING_INDICATORS_HELPTEXT, STR_CONFIG_SETTING_COMPANIES_OFF, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.default_rail_type, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 2, 0, STR_CONFIG_SETTING_DEFAULT_RAIL_TYPE, STR_CONFIG_SETTING_DEFAULT_RAIL_TYPE_HELPTEXT, STR_CONFIG_SETTING_DEFAULT_RAIL_TYPE_FIRST, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.enable_signal_gui,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_ENABLE_SIGNAL_GUI, STR_CONFIG_SETTING_ENABLE_SIGNAL_GUI_HELPTEXT, STR_NULL, CloseSignalGUI, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.coloured_news_year, SLE_INT32, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 2000,       MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_SETTING_COLOURED_NEWS_YEAR, STR_CONFIG_SETTING_COLOURED_NEWS_YEAR_HELPTEXT, STR_JUST_INT, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.drag_signals_density, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 4,       1, 20, 0, STR_CONFIG_SETTING_DRAG_SIGNALS_DENSITY, STR_CONFIG_SETTING_DRAG_SIGNALS_DENSITY_HELPTEXT, STR_CONFIG_SETTING_DRAG_SIGNALS_DENSITY_VALUE, DragSignalsDensityChanged, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.drag_signals_fixed_distance,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_DRAG_SIGNALS_FIXED_DISTANCE, STR_CONFIG_SETTING_DRAG_SIGNALS_FIXED_DISTANCE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.semaphore_build_before, SLE_INT32, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 1950,       MIN_YEAR, MAX_YEAR, 1, STR_CONFIG_SETTING_SEMAPHORE_BUILD_BEFORE_DATE, STR_CONFIG_SETTING_SEMAPHORE_BUILD_BEFORE_DATE_HELPTEXT, STR_JUST_INT, ResetSignalVariant, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.vehicle_income_warn,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_WARN_INCOME_LESS, STR_CONFIG_SETTING_WARN_INCOME_LESS_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.order_review_system, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,       0, 2, 0, STR_CONFIG_SETTING_ORDER_REVIEW, STR_CONFIG_SETTING_ORDER_REVIEW_HELPTEXT, STR_CONFIG_SETTING_ORDER_REVIEW_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.lost_vehicle_warn,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_WARN_LOST_VEHICLE, STR_CONFIG_SETTING_WARN_LOST_VEHICLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.disable_unsuitable_building,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_DISABLE_UNSUITABLE_BUILDING, STR_CONFIG_SETTING_DISABLE_UNSUITABLE_BUILDING_HELPTEXT, STR_NULL, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       gui.new_nonstop,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_NONSTOP_BY_DEFAULT, STR_CONFIG_SETTING_NONSTOP_BY_DEFAULT_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.stop_location, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,       0, 2, 1, STR_CONFIG_SETTING_STOP_LOCATION, STR_CONFIG_SETTING_STOP_LOCATION_HELPTEXT, STR_CONFIG_SETTING_STOP_LOCATION_NEAR_END, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.keep_all_autosave,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.autosave_on_exit,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.autosave_on_network_disconnect,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.max_num_autosaves, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 16,       0, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.auto_euro,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.news_message_timeout, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 2,       1, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.show_track_reservation,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_SHOW_TRACK_RESERVATION, STR_CONFIG_SETTING_SHOW_TRACK_RESERVATION_HELPTEXT, STR_NULL, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.default_signal_type, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,       0, 2, 1, STR_CONFIG_SETTING_DEFAULT_SIGNAL_TYPE, STR_CONFIG_SETTING_DEFAULT_SIGNAL_TYPE_HELPTEXT, STR_CONFIG_SETTING_DEFAULT_SIGNAL_NORMAL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       gui.cycle_signal_types, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,       0, 2, 1, STR_CONFIG_SETTING_CYCLE_SIGNAL_TYPES, STR_CONFIG_SETTING_CYCLE_SIGNAL_TYPES_HELPTEXT, STR_CONFIG_SETTING_CYCLE_SIGNAL_NORMAL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.station_numtracks, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 1,       1, 7, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.station_platlength, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 5,       1, 7, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.station_dragdrop,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.station_show_coverage,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.persistent_buildingtools,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_PERSISTENT_BUILDINGTOOLS, STR_CONFIG_SETTING_PERSISTENT_BUILDINGTOOLS_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       gui.expenses_layout,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_EXPENSES_LAYOUT, STR_CONFIG_SETTING_EXPENSES_LAYOUT_HELPTEXT, STR_NULL, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.station_gui_group_order, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       0, 5, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.station_gui_sort_by, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       0, 3, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.station_gui_sort_order, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       0, 1, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.missing_strings_threshold, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 25,       1, UINT8_MAX, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.graph_line_thickness, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 3,       1, 5, 0, STR_CONFIG_SETTING_GRAPH_LINE_THICKNESS, STR_CONFIG_SETTING_GRAPH_LINE_THICKNESS_HELPTEXT, STR_JUST_COMMA, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.show_newgrf_name,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_CONFIG_SETTING_SHOW_NEWGRF_NAME, STR_CONFIG_SETTING_SHOW_NEWGRF_NAME_HELPTEXT, STR_NULL, RedrawScreen, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
#ifdef DEDICATED
SDTC_BOOL(       gui.show_date_in_logs,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
#endif
#ifndef DEDICATED
SDTC_BOOL(       gui.show_date_in_logs,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
#endif
SDTC_VAR(       gui.settings_restriction_mode, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       0, 2, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.developer, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 1,       0, 2, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       gui.newgrf_developer_tools,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, InvalidateNewGRFChangeWindows, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       gui.ai_developer_tools,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, InvalidateAISettingsWindow, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       gui.scenario_developer,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, InvalidateNewGRFChangeWindows, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       gui.newgrf_show_old_versions,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.newgrf_default_palette, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,       0, 1, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, UpdateNewGRFConfigPalette, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.console_backlog_timeout, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 100,       10, 65500, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.console_backlog_length, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 100,       10, 65500, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.news_ticker,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_TICKER, STR_CONFIG_SETTING_SOUND_TICKER_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.news_full,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_NEWS, STR_CONFIG_SETTING_SOUND_NEWS_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.new_year,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_NEW_YEAR, STR_CONFIG_SETTING_SOUND_NEW_YEAR_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.confirm,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_CONFIRM, STR_CONFIG_SETTING_SOUND_CONFIRM_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.click_beep,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_CLICK, STR_CONFIG_SETTING_SOUND_CLICK_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.disaster,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_DISASTER, STR_CONFIG_SETTING_SOUND_DISASTER_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.vehicle,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_VEHICLE, STR_CONFIG_SETTING_SOUND_VEHICLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       sound.ambient,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_CONFIG_SETTING_SOUND_AMBIENT, STR_CONFIG_SETTING_SOUND_AMBIENT_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       music.playlist, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       0, 5, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       music.music_vol, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 127,       0, 127, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       music.effect_vol, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 127,       0, 127, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_LIST(       music.custom_1, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_LIST(       music.custom_2, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       music.playing,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       music.shuffle,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_OMANY(       news_display.arrival_player, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_ARRIVAL_FIRST_VEHICLE_OWN, STR_CONFIG_SETTING_NEWS_ARRIVAL_FIRST_VEHICLE_OWN_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.arrival_other, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _news_display,     STR_CONFIG_SETTING_NEWS_ARRIVAL_FIRST_VEHICLE_OTHER, STR_CONFIG_SETTING_NEWS_ARRIVAL_FIRST_VEHICLE_OTHER_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.accident, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_ACCIDENTS_DISASTERS, STR_CONFIG_SETTING_NEWS_ACCIDENTS_DISASTERS_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.company_info, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_COMPANY_INFORMATION, STR_CONFIG_SETTING_NEWS_COMPANY_INFORMATION_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.open, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _news_display,     STR_CONFIG_SETTING_NEWS_INDUSTRY_OPEN, STR_CONFIG_SETTING_NEWS_INDUSTRY_OPEN_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.close, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _news_display,     STR_CONFIG_SETTING_NEWS_INDUSTRY_CLOSE, STR_CONFIG_SETTING_NEWS_INDUSTRY_CLOSE_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.economy, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_ECONOMY_CHANGES, STR_CONFIG_SETTING_NEWS_ECONOMY_CHANGES_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.production_player, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _news_display,     STR_CONFIG_SETTING_NEWS_INDUSTRY_CHANGES_COMPANY, STR_CONFIG_SETTING_NEWS_INDUSTRY_CHANGES_COMPANY_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.production_other, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,             2, _news_display,     STR_CONFIG_SETTING_NEWS_INDUSTRY_CHANGES_OTHER, STR_CONFIG_SETTING_NEWS_INDUSTRY_CHANGES_OTHER_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.production_nobody, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,             2, _news_display,     STR_CONFIG_SETTING_NEWS_INDUSTRY_CHANGES_UNSERVED, STR_CONFIG_SETTING_NEWS_INDUSTRY_CHANGES_UNSERVED_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.advice, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_ADVICE, STR_CONFIG_SETTING_NEWS_ADVICE_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.new_vehicles, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_NEW_VEHICLES, STR_CONFIG_SETTING_NEWS_NEW_VEHICLES_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.acceptance, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_CHANGES_ACCEPTANCE, STR_CONFIG_SETTING_NEWS_CHANGES_ACCEPTANCE_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.subsidies, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 1,             2, _news_display,     STR_CONFIG_SETTING_NEWS_SUBSIDIES, STR_CONFIG_SETTING_NEWS_SUBSIDIES_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       news_display.general, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 2,             2, _news_display,     STR_CONFIG_SETTING_NEWS_GENERAL_INFORMATION, STR_CONFIG_SETTING_NEWS_GENERAL_INFORMATION_HELPTEXT, STR_CONFIG_SETTING_NEWS_MESSAGES_OFF, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       gui.network_chat_box_width_pct, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 40,       10, 100, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.network_chat_box_height, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 25,       5, 255, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       gui.network_chat_timeout, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 20,       1, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.sync_freq, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NOT_IN_CONFIG | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 100,       0, 100, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.frame_freq, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NOT_IN_CONFIG | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 0,       0, 100, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.commands_per_frame, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 2,       1, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.max_commands_in_queue, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 16,       1, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.bytes_per_frame, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 8,       1, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.bytes_per_frame_burst, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 256,       1, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.max_init_time, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 100,       0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.max_join_time, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 500,       0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.max_download_time, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 1000,       0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.max_password_time, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 2000,       0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.max_lag_time, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 500,       0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       network.pause_on_join,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.server_port, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, NETWORK_DEFAULT_PORT,       0, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.server_admin_port, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, NETWORK_ADMIN_PORT,       0, 65535, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       network.server_admin_chat,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, true,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       network.server_advertise,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.lan_internet, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 1,       0, 1, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_STR(       network.client_name, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, UpdateClientName, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_STR(       network.server_password, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, UpdateServerPassword, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_STR(       network.rcon_password, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, UpdateRconPassword, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_STR(       network.admin_password, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_STR(       network.default_company_pass, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_STR(       network.server_name, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_STR(       network.connect_to_ip, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_STR(       network.network_id, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, nullptr,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_BOOL(       network.autoclean_companies,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.autoclean_unprotected, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED | SGF_NETWORK_ONLY, 12,       0, 240, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.autoclean_protected, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED | SGF_NETWORK_ONLY, 36,       0, 240, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.autoclean_novehicles, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED | SGF_NETWORK_ONLY, 0,       0, 240, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.max_companies, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 15,       1, MAX_COMPANIES, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, UpdateClientConfigValues, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       network.max_clients, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 25,       2, MAX_CLIENTS, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       network.max_spectators, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 15,       0, MAX_CLIENTS, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, UpdateClientConfigValues, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_VAR(       network.restart_game_year, SLE_INT32, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_0ISDISABLED | SGF_NETWORK_ONLY, 0,       MIN_YEAR, MAX_YEAR, 1, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_VAR(       network.min_active_clients, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 0,       0, MAX_CLIENTS, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_ADVANCED),
SDTC_OMANY(       network.server_lang, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, 0,             35, _server_langs,     STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
SDTC_BOOL(       network.reload_cfg,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NETWORK_ONLY, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_STR(       network.last_host, SLE_STRB, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, "",                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_VAR(       network.last_port, SLE_UINT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0,       0, UINT16_MAX, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
SDTC_BOOL(       network.no_http_content_downloads,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                              STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_EXPERT),
#ifdef __APPLE__
SDTC_VAR(       gui.right_mouse_btn_emulation, SLE_UINT8, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_MULTISTRING, 0,       0, 2, 0, STR_CONFIG_SETTING_RIGHT_MOUSE_BTN_EMU, STR_CONFIG_SETTING_RIGHT_MOUSE_BTN_EMU_HELPTEXT, STR_CONFIG_SETTING_RIGHT_MOUSE_BTN_EMU_COMMAND, nullptr, SL_MIN_VERSION, SL_MAX_VERSION,        SC_BASIC),
#endif
SDT_END()
};
/* win32_v.cpp only settings */
#if defined(_WIN32) && !defined(DEDICATED)
extern bool _force_full_redraw, _window_maximize;
extern uint _display_hz;

static const SettingDescGlobVarList _win32_settings[] = {
SDTG_VAR("display_hz", SLE_UINT, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _display_hz, 0, 0, 120, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_BOOL("force_full_redraw",        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _force_full_redraw, false,                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_EXPERT),
SDTG_BOOL("window_maximize",        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, _window_maximize, false,                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_BASIC),
SDTG_END()
};
#endif /* _WIN32 */

static const SettingDesc _window_settings[] = {
SDT_BOOL(WindowDesc, pref_sticky,        SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, false,                        STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(WindowDesc, pref_width, SLE_INT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0, 0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_VAR(WindowDesc, pref_height, SLE_INT16, SLF_NOT_IN_SAVE | SLF_NO_NETWORK_SYNC, SGF_NONE, 0, 0, 32000, 0, STR_NULL, STR_CONFIG_SETTING_NO_EXPLANATION_AVAILABLE_HELPTEXT, STR_NULL, nullptr, SL_MIN_VERSION, SL_MAX_VERSION, SC_ADVANCED),
SDT_END()
};
