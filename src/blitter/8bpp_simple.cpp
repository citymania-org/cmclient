/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 8bpp_simple.cpp Implementation of the simple 8 bpp blitter. */

#include "../stdafx.h"
#include "../zoom_func.h"
#include "8bpp_simple.hpp"

#include "../safeguards.h"

/** Instantiation of the simple 8bpp blitter factory. */
static FBlitter_8bppSimple iFBlitter_8bppSimple;

void Blitter_8bppSimple::Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom)
{
	const uint8_t *src, *src_line;
	uint8_t *dst, *dst_line;

	/* Find where to start reading in the source sprite */
	src_line = (const uint8_t *)bp->sprite + (bp->skip_top * bp->sprite_width + bp->skip_left) * ScaleByZoom(1, zoom);
	dst_line = (uint8_t *)bp->dst + bp->top * bp->pitch + bp->left;

	for (int y = 0; y < bp->height; y++) {
		dst = dst_line;
		dst_line += bp->pitch;

		src = src_line;
		src_line += bp->sprite_width * ScaleByZoom(1, zoom);

		for (int x = 0; x < bp->width; x++) {
			uint colour = 0;

			switch (mode) {
				case BlitterMode::ColourRemap:
				case BlitterMode::CMTintRemap:
				case BlitterMode::CrashRemap:
					colour = bp->remap[*src];
					break;

				case BlitterMode::Transparent:
				case BlitterMode::TransparentRemap:
					if (*src != 0) colour = bp->remap[*dst];
					break;

				case BlitterMode::BlackRemap:
					if (*src != 0) *dst = 0;
					break;

				default:
					colour = *src;
					break;
			}
			if (colour != 0) *dst = colour;
			dst++;
			src += ScaleByZoom(1, zoom);
		}
	}
}

Sprite *Blitter_8bppSimple::Encode(const SpriteLoader::SpriteCollection &sprite, SpriteAllocator &allocator)
{
	Sprite *dest_sprite;
	dest_sprite = allocator.Allocate<Sprite>(sizeof(*dest_sprite) + static_cast<size_t>(sprite[ZOOM_LVL_MIN].height) * static_cast<size_t>(sprite[ZOOM_LVL_MIN].width));

	dest_sprite->height = sprite[ZOOM_LVL_MIN].height;
	dest_sprite->width  = sprite[ZOOM_LVL_MIN].width;
	dest_sprite->x_offs = sprite[ZOOM_LVL_MIN].x_offs;
	dest_sprite->y_offs = sprite[ZOOM_LVL_MIN].y_offs;

	/* Copy over only the 'remap' channel, as that is what we care about in 8bpp */
	for (int i = 0; i < sprite[ZOOM_LVL_MIN].height * sprite[ZOOM_LVL_MIN].width; i++) {
		dest_sprite->data[i] = sprite[ZOOM_LVL_MIN].data[i].m;
	}

	return dest_sprite;
}
