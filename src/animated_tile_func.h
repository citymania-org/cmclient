/* $Id: animated_tile_func.h 23704 2012-01-01 17:22:32Z alberth $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file animated_tile_func.h %Tile animation! */

#ifndef ANIMATED_TILE_FUNC_H
#define ANIMATED_TILE_FUNC_H

#include "tile_type.h"

void AddAnimatedTile(TileIndex tile);
void DeleteAnimatedTile(TileIndex tile);
void AnimateAnimatedTiles();
void InitializeAnimatedTiles();

#endif /* ANIMATED_TILE_FUNC_H */
