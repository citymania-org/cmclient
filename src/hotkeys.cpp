/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file hotkeys.cpp Implementation of hotkey related functions */

#include "stdafx.h"
#include "openttd.h"
#include "hotkeys.h"
#include "ini_type.h"
#include "string_func.h"
#include "window_gui.h"

#include "citymania/cm_hotkeys.hpp"

#include "safeguards.h"

std::string _hotkeys_file;

/**
 * List of all HotkeyLists.
 * This is a pointer to ensure initialisation order with the various static HotkeyList instances.
 */
static std::vector<HotkeyList*> *_hotkey_lists = nullptr;

/** String representation of a keycode */
struct KeycodeNames {
	const std::string_view name; ///< Name of the keycode
	WindowKeyCodes keycode; ///< The keycode
};

/** Array of non-standard keycodes that can be used in the hotkeys config file. */
static const std::initializer_list<KeycodeNames> _keycode_to_name = {
	{"SHIFT", WKC_SHIFT},
	{"CTRL", WKC_CTRL},
	{"ALT", WKC_ALT},
	{"META", WKC_META},
	{"GLOBAL", WKC_GLOBAL_HOTKEY},
	{"ESC", WKC_ESC},
	{"BACKSPACE", WKC_BACKSPACE},
	{"INS", WKC_INSERT},
	{"DEL", WKC_DELETE},
	{"PAGEUP", WKC_PAGEUP},
	{"PAGEDOWN", WKC_PAGEDOWN},
	{"END", WKC_END},
	{"HOME", WKC_HOME},
	{"RETURN", WKC_RETURN},
	{"SPACE", WKC_SPACE},
	{"F1", WKC_F1},
	{"F2", WKC_F2},
	{"F3", WKC_F3},
	{"F4", WKC_F4},
	{"F5", WKC_F5},
	{"F6", WKC_F6},
	{"F7", WKC_F7},
	{"F8", WKC_F8},
	{"F9", WKC_F9},
	{"F10", WKC_F10},
	{"F11", WKC_F11},
	{"F12", WKC_F12},
	{"BACKQUOTE", WKC_BACKQUOTE},
	{"PAUSE", WKC_PAUSE},
	{"NUM_DIV", WKC_NUM_DIV},
	{"NUM_MUL", WKC_NUM_MUL},
	{"NUM_MINUS", WKC_NUM_MINUS},
	{"NUM_PLUS", WKC_NUM_PLUS},
	{"NUM_ENTER", WKC_NUM_ENTER},
	{"NUM_DOT", WKC_NUM_DECIMAL},
	{"SLASH", WKC_SLASH},
	{"/", WKC_SLASH}, /* deprecated, use SLASH */
	{"SEMICOLON", WKC_SEMICOLON},
	{";", WKC_SEMICOLON}, /* deprecated, use SEMICOLON */
	{"EQUALS", WKC_EQUALS},
	{"=", WKC_EQUALS}, /* deprecated, use EQUALS */
	{"L_BRACKET", WKC_L_BRACKET},
	{"[", WKC_L_BRACKET}, /* deprecated, use L_BRACKET */
	{"BACKSLASH", WKC_BACKSLASH},
	{"\\", WKC_BACKSLASH}, /* deprecated, use BACKSLASH */
	{"R_BRACKET", WKC_R_BRACKET},
	{"]", WKC_R_BRACKET}, /* deprecated, use R_BRACKET */
	{"SINGLEQUOTE", WKC_SINGLEQUOTE},
	{"'", WKC_SINGLEQUOTE}, /* deprecated, use SINGLEQUOTE */
	{"COMMA", WKC_COMMA},
	{"PERIOD", WKC_PERIOD},
	{".", WKC_PERIOD}, /* deprecated, use PERIOD */
	{"MINUS", WKC_MINUS},
	{"-", WKC_MINUS}, /* deprecated, use MINUS */

	/* CityMania additions */
	{"TAB", WKC_TAB},
	{"L_BRACE", WKC_L_BRACE},
	{"R_BRACE", WKC_R_BRACE},
	{"L_PAREN", WKC_L_PAREN},
	{"R_PAREN", WKC_R_PAREN},
	{"EXCLAIM", WKC_EXCLAIM},
	{"ASTERISK", WKC_ASTERISK},

	{"MOUSE_MIDDLE", CM_WKC_MOUSE_MIDDLE},
	{"MOUSE_4", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 0)},
	{"MOUSE_5", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 1)},
	{"MOUSE_6", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 2)},
	{"MOUSE_7", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 3)},
	{"MOUSE_8", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 4)},
	{"MOUSE_9", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 5)},
	{"MOUSE_10", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 6)},
	{"MOUSE_11", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 7)},
	{"MOUSE_12", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 8)},
	{"MOUSE_13", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 9)},
	{"MOUSE_14", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 10)},
	{"MOUSE_15", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 11)},
	{"MOUSE_16", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 12)},
	{"MOUSE_17", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 13)},
	{"MOUSE_18", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 14)},
	{"MOUSE_19", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 15)},
	{"MOUSE_20", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 16)},
	{"MOUSE_21", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 17)},
	{"MOUSE_22", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 18)},
	{"MOUSE_23", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 19)},
	{"MOUSE_24", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 20)},
	{"MOUSE_25", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 21)},
	{"MOUSE_26", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 22)},
	{"MOUSE_27", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 23)},
	{"MOUSE_28", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 24)},
	{"MOUSE_29", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 25)},
	{"MOUSE_30", (WindowKeyCodes)(CM_WKC_MOUSE_OTHER_START + 26)},
};

/**
 * Try to parse a single part of a keycode.
 * @param start Start of the string to parse.
 * @param end End of the string to parse.
 * @return A keycode if a match is found or 0.
 */
static uint16_t ParseCode(const char *start, const char *end)
{
	assert(start <= end);
	while (start < end && *start == ' ') start++;
	while (end > start && *end == ' ') end--;
	std::string_view str{start, end};
	for (const auto &kn : _keycode_to_name) {
		if (StrEqualsIgnoreCase(str, kn.name)) {
			return kn.keycode;
		}
	}
	if (end - start == 1) {
		if (*start >= 'a' && *start <= 'z') return *start - ('a'-'A');
		/* Ignore invalid keycodes */
		if (*(const uint8_t *)start < 128) return *start;
	}
	return 0;
}

/**
 * Parse a string representation of a keycode.
 * @param start Start of the input.
 * @param end End of the input.
 * @return A valid keycode or 0.
 */
static uint16_t ParseKeycode(const char *start, const char *end)
{
	assert(start <= end);
	uint16_t keycode = 0;
	for (;;) {
		const char *cur = start;
		while (*cur != '+' && cur != end) cur++;
		uint16_t code = ParseCode(start, cur);
		if (code == 0) return 0;
		if (code & WKC_SPECIAL_KEYS) {
			/* Some completely wrong keycode we don't support. */
			if (code & ~WKC_SPECIAL_KEYS) return 0;
			keycode |= code;
		} else {
			/* Ignore the code if it has more then 1 letter. */
			if (keycode & ~WKC_SPECIAL_KEYS) return 0;
			keycode |= code;
		}
		if (cur == end) break;
		assert(cur < end);
		start = cur + 1;
	}
	return keycode;
}

/**
 * Parse a string to the keycodes it represents
 * @param hotkey The hotkey object to add the keycodes to
 * @param value The string to parse
 */
static void ParseHotkeys(Hotkey &hotkey, const char *value)
{
	const char *start = value;
	while (*start != '\0') {
		const char *end = start;
		while (*end != '\0' && *end != ',') end++;
		uint16_t keycode = ParseKeycode(start, end);
		if (keycode != 0) hotkey.AddKeycode(keycode);
		start = (*end == ',') ? end + 1: end;
	}
}

/**
 * Convert a hotkey to it's string representation so it can be written to the
 * config file. Separate parts of the keycode (like "CTRL" and "F1" are split
 * by a '+'.
 * @param keycode The keycode to convert to a string.
 * @return A string representation of this keycode.
 */
static std::string KeycodeToString(uint16_t keycode)
{
	std::string str;
	if (keycode & WKC_GLOBAL_HOTKEY) {
		str += "GLOBAL";
	}
	if (keycode & WKC_SHIFT) {
		if (!str.empty()) str += "+";
		str += "SHIFT";
	}
	if (keycode & WKC_CTRL) {
		if (!str.empty()) str += "+";
		str += "CTRL";
	}
	if (keycode & WKC_ALT) {
		if (!str.empty()) str += "+";
		str += "ALT";
	}
	if (keycode & WKC_META) {
		if (!str.empty()) str += "+";
		str += "META";
	}
	if (!str.empty()) str += "+";
	keycode = keycode & ~WKC_SPECIAL_KEYS;

	for (const auto &kn : _keycode_to_name) {
		if (kn.keycode == keycode) {
			str += kn.name;
			return str;
		}
	}
	assert(keycode < 128);
	str.push_back(keycode);
	return str;
}

/**
 * Convert all keycodes attached to a hotkey to a single string. If multiple
 * keycodes are attached to the hotkey they are split by a comma.
 * @param hotkey The keycodes of this hotkey need to be converted to a string.
 * @return A string representation of all keycodes.
 */
std::string SaveKeycodes(const Hotkey &hotkey)
{
	std::string str;
	for (auto keycode : hotkey.keycodes) {
		if (!str.empty()) str += ",";
		str += KeycodeToString(keycode);
	}
	return str;
}

/**
 * Create a new Hotkey object with a single default keycode.
 * @param default_keycode The default keycode for this hotkey.
 * @param name The name of this hotkey.
 * @param num Number of this hotkey, should be unique within the hotkey list.
 */
Hotkey::Hotkey(uint16_t default_keycode, const std::string &name, int num) :
	name(name),
	num(num)
{
	if (default_keycode != 0) this->AddKeycode(default_keycode);
}

/**
 * Create a new Hotkey object with multiple default keycodes.
 * @param default_keycodes An array of default keycodes terminated with 0.
 * @param name The name of this hotkey.
 * @param num Number of this hotkey, should be unique within the hotkey list.
 */
Hotkey::Hotkey(const std::vector<uint16_t> &default_keycodes, const std::string &name, int num) :
	name(name),
	num(num)
{
	for (uint16_t keycode : default_keycodes) {
		this->AddKeycode(keycode);
	}
}

/**
 * Add a keycode to this hotkey, from now that keycode will be matched
 * in addition to any previously added keycodes.
 * @param keycode The keycode to add.
 */
void Hotkey::AddKeycode(uint16_t keycode)
{
	this->keycodes.insert(keycode);
}

HotkeyList::HotkeyList(const std::string &ini_group, const std::vector<Hotkey> &items, GlobalHotkeyHandlerFunc global_hotkey_handler) :
	global_hotkey_handler(global_hotkey_handler), ini_group(ini_group), items(items)
{
	if (_hotkey_lists == nullptr) _hotkey_lists = new std::vector<HotkeyList*>();
	_hotkey_lists->push_back(this);
}

HotkeyList::~HotkeyList()
{
	_hotkey_lists->erase(std::ranges::find(*_hotkey_lists, this));
}

/* CityMania code start */
std::optional<std::pair<std::string, Hotkey>> HotkeyList::CMGetHotkey(int hotkey) const {
	for (const Hotkey &h : this->items) {
		if (h.num != hotkey) continue;
		return {{this->ini_group, h}};
	}
	return std::nullopt;
}
/* CityMania code end */


/**
 * Load HotkeyList from IniFile.
 * @param ini IniFile to load from.
 */
void HotkeyList::Load(const IniFile &ini)
{
	const IniGroup *group = ini.GetGroup(this->ini_group);
	if (group == nullptr) return;
	for (Hotkey &hotkey : this->items) {
		const IniItem *item = group->GetItem(hotkey.name);
		if (item != nullptr) {
			hotkey.keycodes.clear();
			if (item->value.has_value()) ParseHotkeys(hotkey, item->value->c_str());
		}
	}
}

/**
 * Save HotkeyList to IniFile.
 * @param ini IniFile to save to.
 */
void HotkeyList::Save(IniFile &ini) const
{
	IniGroup &group = ini.GetOrCreateGroup(this->ini_group);
	for (const Hotkey &hotkey : this->items) {
		IniItem &item = group.GetOrCreateItem(hotkey.name);
		item.SetValue(SaveKeycodes(hotkey));
	}
}

/**
 * Check if a keycode is bound to something.
 * @param keycode The keycode that was pressed
 * @param global_only Limit the search to hotkeys defined as 'global'.
 * @return The number of the matching hotkey or -1.
 */
int HotkeyList::CheckMatch(uint16_t keycode, bool global_only) const
{
	for (const Hotkey &hotkey : this->items) {
		auto begin = hotkey.keycodes.begin();
		auto end = hotkey.keycodes.end();
		if (std::find(begin, end, keycode | WKC_GLOBAL_HOTKEY) != end || (!global_only && std::find(begin, end, keycode) != end)) {
			return hotkey.num;
		}
	}
	return -1;
}


static void SaveLoadHotkeys(bool save)
{
	IniFile ini{};
	ini.LoadFromDisk(_hotkeys_file, NO_DIRECTORY);

	for (HotkeyList *list : *_hotkey_lists) {
		if (save) {
			list->Save(ini);
		} else {
			list->Load(ini);
		}
	}

	if (save) ini.SaveToDisk(_hotkeys_file);
}


/** Load the hotkeys from the config file */
void LoadHotkeysFromConfig()
{
	SaveLoadHotkeys(false);
}

/** Save the hotkeys to the config file */
void SaveHotkeysToConfig()
{
	SaveLoadHotkeys(true);
}

void HandleGlobalHotkeys([[maybe_unused]] char32_t key, uint16_t keycode)
{
	for (HotkeyList *list : *_hotkey_lists) {
		if (list->global_hotkey_handler == nullptr) continue;

		int hotkey = list->CheckMatch(keycode, true);
		if (hotkey >= 0 && (list->global_hotkey_handler(hotkey) == ES_HANDLED)) {
			citymania::CountHotkeyStats(list, hotkey);
			return;
		}
	}
}

