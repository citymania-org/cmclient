/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilehighlight_func.h Functions related to tile highlights. */

#ifndef TILEHIGHLIGHT_FUNC_H
#define TILEHIGHLIGHT_FUNC_H

#include "gfx_type.h"
#include "tilearea_type.h"
#include "tilehighlight_type.h"
#include "track_type.h"
#include "industry_type.h"

void PlaceProc_DemolishArea(TileIndex tile);
bool GUIPlaceProcDragXY(ViewportDragDropSelectionProcess proc, TileIndex start_tile, TileIndex end_tile);

bool HandlePlacePushButton(Window *w, WidgetID widget, CursorID cursor, HighLightStyle mode, ViewportDragDropSelectionProcess cm_process);
void SetObjectToPlaceWnd(CursorID icon, PaletteID pal, HighLightStyle mode, Window *w, ViewportDragDropSelectionProcess cm_process);
void SetObjectToPlace(CursorID icon, PaletteID pal, HighLightStyle mode, WindowClass window_class, WindowNumber window_num, ViewportDragDropSelectionProcess cm_process);
void ResetObjectToPlace();

void VpSelectTilesWithMethod(int x, int y, ViewportPlaceMethod method);
void VpStartDragging(ViewportDragDropSelectionProcess process);
void VpStartPlaceSizing(TileIndex tile, ViewportPlaceMethod method, ViewportDragDropSelectionProcess process);
void VpSetPresizeRange(TileIndex from, TileIndex to);
void VpSetPlaceSizingLimit(int limit);

void UpdateTileSelection();

RailSnapMode GetRailSnapMode();
void SetRailSnapMode(RailSnapMode mode);

void StoreRailPlacementEndpoints(TileIndex start_tile, TileIndex end_tile, Track start_track, bool bidirectional = true);
void StoreRailStationPlacementEndpoints(const TileArea &ta, Axis station_axis);
void ResetRailPlacementEndpoints();

extern TileHighlightData _thd;

#endif /* TILEHIGHLIGHT_FUNC_H */
