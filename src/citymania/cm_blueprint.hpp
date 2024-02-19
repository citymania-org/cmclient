#ifndef CM_BLUEPRINT_HPP
#define CM_BLUEPRINT_HPP

#include "cm_highlight.hpp"

#include <functional>

namespace citymania {

void BlueprintCopyArea(TileIndex start, TileIndex end);
void ResetActiveBlueprint();
void SetBlueprintHighlight(const TileInfo *ti, TileHighlight &th);


void UpdateBlueprintTileSelection(TileIndex tile);
void BuildActiveBlueprint(TileIndex start);
void RotateActiveBlueprint();
void SaveBlueprint(uint slot);
bool LoadBlueprint(uint slot);

void CommandExecuted(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd);

}  // namespace citymania

#endif
