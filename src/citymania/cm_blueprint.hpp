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
bool HasActiveBlueprint();

/* CityMania: named slot management, extending the save/load slots above. */
extern const size_t MAX_BLUEPRINT_SLOTS;
bool IsBlueprintSlotEmpty(uint slot);
std::string GetBlueprintSlotName(uint slot);
void RenameBlueprintSlot(uint slot, const std::string &name);
void ClearBlueprintSlot(uint slot);

/* CityMania: import/export blueprint slots to/from files on disk. */
std::string GetBlueprintsDir();
std::vector<std::string> ListBlueprintFiles();
bool ExportBlueprintSlot(uint slot, const std::string &filename);
bool ImportBlueprintSlot(uint slot, const std::string &filename, std::string &error);

void CommandExecuted(bool res, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd);

}  // namespace citymania

#endif
