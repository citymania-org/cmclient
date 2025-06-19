#ifndef CM_STATION_GUI_HPP
#define CM_STATION_GUI_HPP

#include "cm_highlight_type.hpp"

#include "../core/geometry_type.hpp"
#include "../command_type.h"
#include "../road_type.h"
#include "../station_gui.h"
#include "../station_type.h"

namespace citymania {

const DiagDirection DEPOTDIR_AUTO = DIAGDIR_END;
const DiagDirection STATIONDIR_X = DIAGDIR_END;
const DiagDirection STATIONDIR_Y = (DiagDirection)((uint)DIAGDIR_END + 1);
const DiagDirection STATIONDIR_AUTO = (DiagDirection)((uint)DIAGDIR_END + 2);
const DiagDirection STATIONDIR_AUTO_XY = (DiagDirection)((uint)DIAGDIR_END + 3);

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    uint16_t station_type;            ///< %Station type within the currently selected custom station class (if newstations is \c true )
    uint16_t station_count;           ///< Number of custom stations (if newstations is \c true )
};

enum class StationBuildingStatus {
    IMPOSSIBLE = 0,
    QUERY = 1,
    JOIN = 2,
    NEW = 3,
};

// void SetStationBiildingStatus(StationBuildingStatus status);
// void SetStationTileSelectSize(int w, int h, int catchment);
bool UseImprovedStationJoin();
void OnStationTileSetChange(const Station *station, bool adding, StationType type);
void OnStationPartBuilt(const Station *station);
void OnStationRemoved(const Station *station);
void PlaceRoadStop(TileIndex start_tile, TileIndex end_tile, RoadStopType stop_type, bool adjacent, RoadType rt, StringID err_msg);
void HandleStationPlacement(TileIndex start, TileIndex end);
void PlaceRail_Station(TileIndex tile);
void PlaceDock(TileIndex tile, TileIndex tile_to);
void PlaceAirport(TileIndex tile);

void SelectStationToJoin(const Station *station);
// const Station *GetStationToJoin();
void MarkCoverageHighlightDirty();
bool CheckRedrawStationCoverage();
void AbortStationPlacement();

std::optional<std::string> GetStationCoverageAreaText(TileIndex tile, int w, int h, int rad, StationCoverageType sct, bool supplies);

bool CheckDriveThroughRoadStopDirection(TileArea area, RoadBits r);
DiagDirection AutodetectRoadObjectDirection(TileIndex tile, Point pt, RoadType roadtype);
DiagDirection AutodetectDriveThroughRoadStopDirection(TileArea area, Point pt, RoadType roadtype);
DiagDirection AutodetectRailObjectDirection(TileIndex tile, Point pt);
void SetSelectedStationToJoin(StationID station_id);


struct OverlayParams {
    TileArea area;
    CatchmentArea radius;
    StationCoverageType coverage_type;
};


class PreviewStationType {
public:
    TileIndex start_tile = INVALID_TILE;
    TileIndex cur_tile = INVALID_TILE;

    virtual ~PreviewStationType() {};

    TileIndex GetStartTile() const { return start_tile == INVALID_TILE ? cur_tile : start_tile; }
    virtual bool IsDragDrop() const { return true; };
    virtual CursorID GetCursor() const =0;
    virtual TileArea GetArea(bool remove_mode) const =0;
    virtual void Update(Point /* pt */, TileIndex /* tile */) {};
    virtual up<Command> GetCommand(bool adjacent, StationID join_to) const =0;
    virtual up<Command> GetRemoveCommand() const =0;
    virtual void AddPreviewTiles(HighlightMap &hlmap, SpriteID palette) const =0;
    virtual bool Execute(up<Command> cmd, bool remove_mode) const =0;
    virtual OverlayParams GetOverlayParams() const =0;
};

class RailStationPreview : public PreviewStationType {
public:
    virtual ~RailStationPreview() {};

    // RailPreviewStation(RailStationGUISettings &settings) :settings{settings} {}
    bool IsDragDrop() const override;
    CursorID GetCursor() const override;
    TileArea GetArea(bool remove_mode) const override;
    up<Command> GetCommand(bool adjacent, StationID join_to) const override;
    up<Command> GetRemoveCommand() const override;
    void AddPreviewTiles(HighlightMap &hlmap, SpriteID palette) const override;
    bool Execute(up<Command> cmd, bool remove_mode) const override;
    OverlayParams GetOverlayParams() const override;
};

class RoadStationPreview : public PreviewStationType {
protected:
    DiagDirection ddir;
    RoadStopType stop_type;

public:
    RoadStationPreview(RoadStopType stop_type) :stop_type{stop_type} {}
    virtual ~RoadStationPreview() {};

    bool IsDragDrop() const override;
    CursorID GetCursor() const override;
    TileArea GetArea(bool remove_mode) const override;
    void Update(Point pt, TileIndex tile) override;
    up<Command> GetCommand(bool adjacent, StationID join_to) const override;
    up<Command> GetRemoveCommand() const override;
    void AddPreviewTiles(HighlightMap &hlmap, SpriteID palette) const override;
    bool Execute(up<Command> cmd, bool remove_mode) const override;
    OverlayParams GetOverlayParams() const override;
};

class DockPreview : public PreviewStationType {
protected:
    DiagDirection ddir;

public:
    DockPreview() {}
    virtual ~DockPreview() {};

    bool IsDragDrop() const override;
    CursorID GetCursor() const override;
    TileArea GetArea(bool remove_mode) const override;
    void Update(Point pt, TileIndex tile) override;
    up<Command> GetCommand(bool adjacent, StationID join_to) const override;
    up<Command> GetRemoveCommand() const override;
    void AddPreviewTiles(HighlightMap &hlmap, SpriteID palette) const override;
    bool Execute(up<Command> cmd, bool remove_mode) const override;
    OverlayParams GetOverlayParams() const override;
};

class StationPreviewBase : public Preview {
protected:
    sp<PreviewStationType> type;
    bool remove_mode = false;
    bool keep_rail = true;  // whether to keep rail in remove mode
    StationID station_to_join = INVALID_STATION;
    bool adjacent_stations = false;
    bool show_coverage = true;

    void AddAreaTiles(HighlightMap &hlmap, bool add_current, bool show_join_area);
    virtual void Execute() = 0;
    up<Command> GetCommand(bool adjacent, StationID join_to);
    void AddStationPreview(HighlightMap &hlmap, SpriteID palette);

public:
    StationPreviewBase(sp<PreviewStationType> type) :type{type} {};
    CursorID GetCursor() override { return this->type->GetCursor(); };
    void Update(Point pt, TileIndex tile) override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    std::optional<std::string> GetStationCoverageAreaText(int rad, StationCoverageType sct, bool supplies);
    std::vector<std::pair<SpriteID, std::string>> GetOverlayData();
};


class VanillaStationPreview : public StationPreviewBase {
protected:
    SpriteID palette;

    void Execute() override;

public:
    StationID selected_station_to_join = INVALID_STATION;

    VanillaStationPreview(sp<PreviewStationType> type) :StationPreviewBase{type} {};
    virtual ~VanillaStationPreview() {};
    void Update(Point pt, TileIndex tile) override;

    HighlightMap GetHighlightMap() override;
    void OnStationRemoved(const Station *station) override;
};


class StationPreview : public StationPreviewBase {
protected:
    bool select_mode = false;

    void Execute() override;
    up<Command> GetCommand();

public:
    StationPreview(sp<PreviewStationType> type);
    virtual ~StationPreview();
    void Update(Point pt, TileIndex tile) override;
    bool HandleMousePress() override;

    HighlightMap GetHighlightMap() override;
    void OnStationRemoved(const Station *station) override;
};

// SPR_CURSOR_BUS_STATION SPR_CURSOR_TRUCK_STATION

bool HandleStationPlacePushButton(Window *w, WidgetID widget, sp<PreviewStationType> type);


} // namespace citymania

#endif
