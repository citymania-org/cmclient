/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout.h Functions related to laying out the texts. */

#ifndef GFX_LAYOUT_H
#define GFX_LAYOUT_H

#include "fontcache.h"
#include "gfx_func.h"
#include "core/math_func.hpp"

#include <stack>
#include <string_view>

/**
 * Text drawing parameters, which can change while drawing a line, but are kept between multiple parts
 * of the same text, e.g. on line breaks.
 */
struct FontState {
	FontSize fontsize;       ///< Current font size.
	TextColour cur_colour;   ///< Current text colour.

	std::stack<TextColour, std::vector<TextColour>> colour_stack; ///< Stack of colours to assist with colour switching.

	FontState() : fontsize(FS_END), cur_colour(TC_INVALID) {}
	FontState(TextColour colour, FontSize fontsize) : fontsize(fontsize), cur_colour(colour) {}

	/**
	 * Switch to new colour \a c.
	 * @param c New colour to use.
	 */
	inline void SetColour(TextColour c)
	{
		// assert(((c & TC_COLOUR_MASK) >= TC_BLUE && (c & TC_COLOUR_MASK) <= TC_BLACK) || (c & TC_COLOUR_MASK) == TC_INVALID);  CM seems redundant and crashes custom colors (in graphs)
		// assert((c & (TC_COLOUR_MASK | TC_FLAGS_MASK)) == c);  TODO may or may not be redundant as well
		if ((this->cur_colour & TC_FORCED) == 0) this->cur_colour = c;
	}

	/**
	 * Switch to and pop the last saved colour on the stack.
	 */
	inline void PopColour()
	{
		if (colour_stack.empty()) return;
		SetColour(colour_stack.top());
		colour_stack.pop();
	}

	/**
	 * Push the current colour on to the stack.
	 */
	inline void PushColour()
	{
		colour_stack.push(this->cur_colour);
	}

	/**
	 * Switch to using a new font \a f.
	 * @param f New font to use.
	 */
	inline void SetFontSize(FontSize f)
	{
		this->fontsize = f;
	}
};

/**
 * Container with information about a font.
 */
class Font {
public:
	FontCache *fc;     ///< The font we are using.
	TextColour colour; ///< The colour this font has to be.

	Font(FontSize size, TextColour colour);
};

/** Mapping from index to font. The pointer is owned by FontColourMap. */
using FontMap = std::vector<std::pair<int, Font *>>;

/**
 * Interface to glue fallback and normal layouter into one.
 */
class ParagraphLayouter {
public:
	virtual ~ParagraphLayouter() = default;

	/** Position of a glyph within a VisualRun. */
	class Position {
	public:
		int16_t left; ///< Left-most position of glyph.
		int16_t right; ///< Right-most position of glyph.
		int16_t top; ///< Top-most position of glyph.

		constexpr inline Position(int16_t left, int16_t right, int16_t top) : left(left), right(right), top(top) { }

		/** Conversion from a single point to a Position. */
		constexpr inline Position(const Point &pt) : left(pt.x), right(pt.x), top(pt.y) { }
	};

	/** Visual run contains data about the bit of text with the same font. */
	class VisualRun {
	public:
		virtual ~VisualRun() = default;
		virtual const Font *GetFont() const = 0;
		virtual int GetGlyphCount() const = 0;
		virtual std::span<const GlyphID> GetGlyphs() const = 0;
		virtual std::span<const Position> GetPositions() const = 0;
		virtual int GetLeading() const = 0;
		virtual std::span<const int> GetGlyphToCharMap() const = 0;
	};

	/** A single line worth of VisualRuns. */
	class Line {
	public:
		virtual ~Line() = default;
		virtual int GetLeading() const = 0;
		virtual int GetWidth() const = 0;
		virtual int CountRuns() const = 0;
		virtual const VisualRun &GetVisualRun(int run) const = 0;
		virtual int GetInternalCharLength(char32_t c) const = 0;
	};

	virtual void Reflow() = 0;
	virtual std::unique_ptr<const Line> NextLine(int max_width) = 0;
};

/**
 * The layouter performs all the layout work.
 *
 * It also accounts for the memory allocations and frees.
 */
class Layouter : public std::vector<std::unique_ptr<const ParagraphLayouter::Line>> {
	std::string_view string; ///< Pointer to the original string.

	/** Key into the linecache */
	struct LineCacheKey {
		FontState state_before;  ///< Font state at the beginning of the line.
		std::string str;         ///< Source string of the line (including colour and font size codes).
	};

	struct LineCacheQuery {
		const FontState &state_before; ///< Font state at the beginning of the line.
		std::string_view str;    ///< Source string of the line (including colour and font size codes).
	};

	/** Comparator for std::map */
	struct LineCacheCompare {
		using is_transparent = void; ///< Enable map queries with various key types

		/** Comparison operator for LineCacheKey and LineCacheQuery */
		template <typename Key1, typename Key2>
		bool operator()(const Key1 &lhs, const Key2 &rhs) const
		{
			if (lhs.state_before.fontsize != rhs.state_before.fontsize) return lhs.state_before.fontsize < rhs.state_before.fontsize;
			if (lhs.state_before.cur_colour != rhs.state_before.cur_colour) return lhs.state_before.cur_colour < rhs.state_before.cur_colour;
			if (lhs.state_before.colour_stack != rhs.state_before.colour_stack) return lhs.state_before.colour_stack < rhs.state_before.colour_stack;
			return lhs.str < rhs.str;
		}
	};
public:
	/** Item in the linecache */
	struct LineCacheItem {
		/* Due to the type of data in the buffer differing depending on the Layouter, we need to pass our own deleter routine. */
		using Buffer = std::unique_ptr<void, void(*)(void *)>;
		/* Stuff that cannot be freed until the ParagraphLayout is freed */
		Buffer buffer{nullptr, [](void *){}}; ///< Accessed by our ParagraphLayout::nextLine.
		FontMap runs;              ///< Accessed by our ParagraphLayout::nextLine.

		FontState state_after;     ///< Font state after the line.
		std::unique_ptr<ParagraphLayouter> layout = nullptr; ///< Layout of the line.
	};
private:
	typedef std::map<LineCacheKey, LineCacheItem, LineCacheCompare> LineCache;
	static LineCache *linecache;

	static LineCacheItem &GetCachedParagraphLayout(std::string_view str, const FontState &state);

	using FontColourMap = std::map<TextColour, std::unique_ptr<Font>>;
	static FontColourMap fonts[FS_END];
public:
	static Font *GetFont(FontSize size, TextColour colour);

	Layouter(std::string_view str, int maxw = INT32_MAX, FontSize fontsize = FS_NORMAL);
	Dimension GetBounds();
	ParagraphLayouter::Position GetCharPosition(std::string_view::const_iterator ch) const;
	ptrdiff_t GetCharAtPosition(int x, size_t line_index) const;

	static void Initialize();
	static void ResetFontCache(FontSize size);
	static void ResetLineCache();
	static void ReduceLineCache();
};

ParagraphLayouter::Position GetCharPosInString(std::string_view str, const char *ch, FontSize start_fontsize = FS_NORMAL);
ptrdiff_t GetCharAtPosition(std::string_view str, int x, FontSize start_fontsize = FS_NORMAL);

#endif /* GFX_LAYOUT_H */
