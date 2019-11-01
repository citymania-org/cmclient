/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file dropdown_type.h Types related to the drop down widget. */

#ifndef WIDGETS_DROPDOWN_TYPE_H
#define WIDGETS_DROPDOWN_TYPE_H

#include "../window_type.h"
#include "../gfx_func.h"
#include "../core/smallvec_type.hpp"
#include "table/strings.h"
#include "../stdafx.h"
#include "../window_gui.h"
#include "../string_func.h"
#include "../strings_func.h"
#include "../window_func.h"

/**
 * Base list item class from which others are derived. If placed in a list it
 * will appear as a horizontal line in the menu.
 */
class DropDownListItem {
public:
	int result;  ///< Result code to return to window on selection
	bool masked; ///< Masked and unselectable item

	DropDownListItem(int result, bool masked) : result(result), masked(masked) {}
	virtual ~DropDownListItem() {}

	virtual bool Selectable() const { return false; }
	virtual uint Height(uint width) const { return FONT_HEIGHT_NORMAL; }
	virtual uint Width() const { return 0; }
	virtual void Draw(int left, int right, int top, int bottom, bool sel, Colours bg_colour) const;
};

/**
 * Common string list item.
 */
class DropDownListStringItem : public DropDownListItem {
public:
	StringID string; ///< String ID of item

	DropDownListStringItem(StringID string, int result, bool masked) : DropDownListItem(result, masked), string(string) {}

	bool Selectable() const override { return true; }
	uint Width() const override;
	void Draw(int left, int right, int top, int bottom, bool sel, Colours bg_colour) const override;
	virtual StringID String() const { return this->string; }

	static bool NatSortFunc(std::unique_ptr<const DropDownListItem> const &first, std::unique_ptr<const DropDownListItem> const &second);
};

/**
 * Drop down list entry for showing a checked/unchecked toggle item. Use
 * DropDownListCheckedItem or DropDownListCharStringCheckedItem depending of
 * type of string used (either StringID or const char*).
 */

template <class T, typename S>
class DropDownListCheckedItemT : public T {
	uint checkmark_width;
public:
	bool checked;
	DropDownListCheckedItemT<T,S>(S string, int result, bool masked, bool checked) : T(string, result, masked), checked(checked)
	{
		this->checkmark_width = GetStringBoundingBox(STR_JUST_CHECKMARK).width + 3;
	}
	virtual ~DropDownListCheckedItemT<T,S>() {}
	virtual uint Width() const
	{
		return T::Width() + this->checkmark_width;
	}

	virtual void Draw(int left, int right, int top, int bottom, bool sel, int bg_colour) const
	{
		bool rtl = _current_text_dir == TD_RTL;
		if (this->checked) {
			DrawString(left + WD_FRAMERECT_LEFT, right - WD_FRAMERECT_RIGHT, top, STR_JUST_CHECKMARK, sel ? TC_WHITE : TC_BLACK);
		}
		DrawString(left + WD_FRAMERECT_LEFT + (rtl ? 0 : this->checkmark_width), right - WD_FRAMERECT_RIGHT - (rtl ? this->checkmark_width : 0), top, this->String(), sel ? TC_WHITE : TC_BLACK);
	}
};

#define DropDownListCheckedItem DropDownListCheckedItemT<DropDownListStringItem,StringID>
#define DropDownListCharStringCheckedItem DropDownListCheckedItemT<DropDownListCharStringItem,const char*>
/**
 * String list item with parameters.
 */
class DropDownListParamStringItem : public DropDownListStringItem {
public:
	uint64 decode_params[10]; ///< Parameters of the string

	DropDownListParamStringItem(StringID string, int result, bool masked) : DropDownListStringItem(string, result, masked) {}

	StringID String() const override;
	void SetParam(uint index, uint64 value) { decode_params[index] = value; }
};

/**
 * List item containing a C char string.
 */
class DropDownListCharStringItem : public DropDownListStringItem {
public:
	const char *raw_string;

	DropDownListCharStringItem(const char *raw_string, int result, bool masked) : DropDownListStringItem(STR_JUST_RAW_STRING, result, masked), raw_string(raw_string) {}

	StringID String() const override;
};

/**
 * List item with icon and string.
 */
class DropDownListIconItem : public DropDownListParamStringItem {
	SpriteID sprite;
	PaletteID pal;
	Dimension dim;
	uint sprite_y;
	uint text_y;
public:
	DropDownListIconItem(SpriteID sprite, PaletteID pal, StringID string, int result, bool masked);

	uint Height(uint width) const override;
	uint Width() const override;
	void Draw(int left, int right, int top, int bottom, bool sel, Colours bg_colour) const override;
	void SetDimension(Dimension d);
};

/**
 * A drop down list is a collection of drop down list items.
 */
typedef std::vector<std::unique_ptr<const DropDownListItem>> DropDownList;

void ShowDropDownListAt(Window *w, DropDownList &&list, int selected, int button, Rect wi_rect, Colours wi_colour, bool auto_width = false, bool instant_close = false);

void ShowDropDownList(Window *w, DropDownList &&list, int selected, int button, uint width = 0, bool auto_width = false, bool instant_close = false);

#endif /* WIDGETS_DROPDOWN_TYPE_H */
