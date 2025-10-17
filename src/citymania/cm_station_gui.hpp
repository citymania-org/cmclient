#ifndef CM_STATION_GUI_HPP
#define CM_STATION_GUI_HPP

#include "cm_command_type.hpp"
#include "cm_highlight_type.hpp"

#include "../core/geometry_type.hpp"
#include "../command_type.h"
#include "../road_type.h"
#include "../station_gui.h"
#include "../station_type.h"

#include <concepts>
#include <optional>

struct RailStationGUISettings {
    Axis orientation;                 ///< Currently selected rail station orientation

    bool newstations;                 ///< Are custom station definitions available?
    StationClassID station_class;     ///< Currently selected custom station class (if newstations is \c true )
    uint16_t station_type;            ///< %Station type within the currently selected custom station class (if newstations is \c true )
    uint16_t station_count;           ///< Number of custom stations (if newstations is \c true )
};

struct RoadStopGUISettings {
    DiagDirection orientation;

    RoadStopClassID roadstop_class;
    uint16_t roadstop_type;
    uint16_t roadstop_count;
};

namespace citymania {

const DiagDirection DEPOTDIR_AUTO = DIAGDIR_END;
const DiagDirection STATIONDIR_X = DIAGDIR_END;
const DiagDirection STATIONDIR_Y = (DiagDirection)((uint)DIAGDIR_END + 1);
const DiagDirection STATIONDIR_AUTO = (DiagDirection)((uint)DIAGDIR_END + 2);
const DiagDirection STATIONDIR_AUTO_XY = (DiagDirection)((uint)DIAGDIR_END + 3);

// void SetStationTileSelectSize(int w, int h, int catchment);
bool UseImprovedStationJoin();
void OnStationTileSetChange(const Station *station, bool adding, StationType type);
void OnStationPartBuilt(const Station *station);
void OnStationRemoved(const Station *station);

// void SelectStationToJoin(const Station *station);
// const Station *GetStationToJoin();
void MarkCoverageHighlightDirty();
void AbortStationPlacement();
void ShowCatchmentByClick(StationID station);

std::optional<std::string> GetStationCoverageAreaText(TileIndex tile, int w, int h, int rad, StationCoverageType sct, bool supplies);

bool CheckDriveThroughRoadStopDirection(TileArea area, RoadBits r);
DiagDirection AutodetectRoadObjectDirection(TileIndex tile, Point pt, RoadType roadtype);
DiagDirection AutodetectDriveThroughRoadStopDirection(TileArea area, Point pt, RoadType roadtype);
DiagDirection AutodetectRailObjectDirection(TileIndex tile, Point pt);
void SetSelectedStationToJoin(StationID station_id);
void ResetSelectedStationToJoin();

void SetHighlightCoverageStation(Station *station, bool sel);
bool IsHighlightCoverageStation(const Station *station);

bool HasSelectedStationHighlight();
ToolGUIInfo GetSelectedStationGUIInfo();


struct OverlayParams {
    TileArea area;
    CatchmentArea radius;
    StationCoverageType coverage_type;
};

class RemoveAction : public Action {
protected:
    TileIndex start_tile = INVALID_TILE;
    TileIndex cur_tile = INVALID_TILE;
public:
    ~RemoveAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    std::optional<TileArea> GetArea() const override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *) override;
    virtual up<Command> GetCommand(TileArea area) = 0;
    virtual bool Execute(TileArea area) = 0;
};

class StationSelectAction : public Action {
protected:
    TileIndex cur_tile = INVALID_TILE;
public:
    ~StationSelectAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *station) override;
};

// PlacementAction
class PlacementAction : public Action {
public:
    ~PlacementAction() override = default;
    ToolGUIInfo PrepareGUIInfo(std::optional<ObjectHighlight> ohl, up<Command> cmd, StationCoverageType sct, uint rad);
};

class SizedPlacementAction : public PlacementAction {
protected:
    TileIndex cur_tile = INVALID_TILE;
public:
    ~SizedPlacementAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *) override;
    virtual up<Command> GetCommand(TileIndex tile, StationID to_join) = 0;
    virtual bool Execute(TileIndex tile) = 0;
    virtual std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) = 0;
    virtual std::pair<StationCoverageType, uint> GetCatchmentParams() = 0;
};

class DragNDropPlacementAction : public PlacementAction {
protected:
    TileIndex start_tile = INVALID_TILE;
    TileIndex cur_tile = INVALID_TILE;
public:
    ~DragNDropPlacementAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    std::optional<TileArea> GetArea() const override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *) override;
    virtual up<Command> GetCommand(TileArea area, StationID to_join) = 0;
    virtual bool Execute(TileArea area) = 0;
    virtual std::optional<ObjectHighlight> GetObjectHighlight(TileArea area) = 0;
    virtual std::pair<StationCoverageType, uint> GetCatchmentParams() = 0;
};

class StationBuildTool : public Tool {
public:
    StationBuildTool();
    ~StationBuildTool() override = default;
    ToolGUIInfo GetGUIInfo() override {
        if (!this->action) return {};
        return this->action->GetGUIInfo();
    }
    void OnStationRemoved(const Station *station) override {
        if (this->action) this->action->OnStationRemoved(station);
    }
protected:
};

// RailStationBuildTool
class RailStationBuildTool : public StationBuildTool {
private:
    class RemoveAction : public citymania::RemoveAction {
    public:
        ~RemoveAction() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class SizedPlacementAction : public citymania::SizedPlacementAction {
    public:
        ~SizedPlacementAction() override  = default;
        up<Command> GetCommand(TileIndex tile, StationID to_join) override;
        bool Execute(TileIndex tile) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return {SCT_ALL, CA_TRAIN}; };
        std::optional<TileArea> GetArea() const override;
    };

    class DragNDropPlacementAction: public citymania::DragNDropPlacementAction {
    public:
        ~DragNDropPlacementAction() override  = default;
        up<Command> GetCommand(TileArea area, StationID to_join) override;
        bool Execute(TileArea area) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileArea area) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return {SCT_ALL, CA_TRAIN}; };
    };

public:
    RailStationBuildTool();
    ~RailStationBuildTool() override = default;
    void Update(Point pt, TileIndex tile) override;
    CursorID GetCursor() override;
private:
    enum class Mode { REMOVE, SELECT, DRAGDROP, SIZED };
    Mode mode;
};

// RoadStopBuildTool
class RoadStopBuildTool : public StationBuildTool {
private:
    class RemoveAction : public citymania::RemoveAction {
    public:
        RoadStopType stop_type;
        RemoveAction(RoadStopType stop_type) : stop_type{stop_type} {}
        ~RemoveAction() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class DragNDropPlacementAction: public citymania::DragNDropPlacementAction {
    public:
        RoadType road_type;
        RoadStopType stop_type;
        DiagDirection ddir = DIAGDIR_NE;
        // RoadStopClassID spec_class;
        // uint16_t spec_index;

        DragNDropPlacementAction(RoadType road_type, RoadStopType stop_type)
            :road_type{road_type}, stop_type{stop_type} {}
        // DragNDropPlacementHandler(DiagDirection ddir, RoadStopType stop_type, RoadStopClassID spec_class, uint16_t spec_index;)
        //     :ddir{ddir}, stop_type{stop_type}, spec_class{spec_class}, spec_index{spec_index} {}
        ~DragNDropPlacementAction() override  = default;
        void Update(Point pt, TileIndex tile) override;
        up<Command> GetCommand(TileArea area, StationID to_join) override;
        bool Execute(TileArea area) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileArea area) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override {
            if (this->stop_type == ROADSTOP_BUS) return {SCT_PASSENGERS_ONLY, CA_BUS};
            else return {SCT_NON_PASSENGERS_ONLY, CA_TRUCK};
        }
    };

public:
    RoadStopBuildTool(RoadStopType stop_type);
    ~RoadStopBuildTool() override = default;
    void Update(Point pt, TileIndex tile) override;
    CursorID GetCursor() override;
private:
    enum class Mode { REMOVE, SELECT, DRAGDROP };
    Mode mode;
    RoadStopType stop_type;
};

// --- DockBuildTool ---
class DockBuildTool : public StationBuildTool {
private:
    class RemoveAction : public citymania::RemoveAction {
    public:
        ~RemoveAction() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class SizedPlacementAction : public citymania::SizedPlacementAction {
    public:
        ~SizedPlacementAction() override = default;
        up<Command> GetCommand(TileIndex tile, StationID to_join) override;
        bool Execute(TileIndex tile) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return {SCT_ALL, CA_DOCK}; };
        std::optional<TileArea> GetArea() const override;
        std::optional<DiagDirection> GetDirection(TileIndex tile) const;
    };

public:
    DockBuildTool();
	~DockBuildTool() override = default;
    void Update(Point pt, TileIndex tile) override;
    CursorID GetCursor() override;
private:
    enum class Mode { REMOVE, SELECT, SIZED };
    Mode mode;
};

// --- AirportBuildTool ---
class AirportBuildTool : public StationBuildTool {
private:
    class RemoveAction : public citymania::RemoveAction {
    public:
        ~RemoveAction() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class SizedPlacementAction : public citymania::SizedPlacementAction {
    public:
        ~SizedPlacementAction() override = default;
        up<Command> GetCommand(TileIndex tile, StationID to_join) override;
        bool Execute(TileIndex tile) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override;
        std::optional<TileArea> GetArea() const override;
    };

public:
    AirportBuildTool();
    ~AirportBuildTool() override = default;
    void Update(Point pt, TileIndex tile) override;
    CursorID GetCursor() override;
private:
    enum class Mode { REMOVE, SELECT, SIZED };
    Mode mode;
};

} // namespace citymania

#endif
