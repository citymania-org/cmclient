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

// Remove action classes
class RemoveHandler {
public:
    virtual ~RemoveHandler() = default;
    virtual up<Command> GetCommand(TileArea area) = 0;
    virtual bool Execute(TileArea area) = 0;
};
template<typename Handler>
concept ImplementsRemoveHandler = std::derived_from<Handler, RemoveHandler>;

template<ImplementsRemoveHandler Handler>
class RemoveAction : public Action {
private:
    Handler handler;
    TileIndex start_tile = INVALID_TILE;
    TileIndex cur_tile = INVALID_TILE;
public:
    RemoveAction(const Handler &handler) : handler{handler} {}
    ~RemoveAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    std::optional<TileArea> GetArea() const override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *) override;
};


// StationSelect classes
class StationSelectHandler {
public:
    virtual ~StationSelectHandler() = default;
};
template<typename Handler>
concept ImplementsStationSelectHandler = std::derived_from<Handler, StationSelectHandler>;

template<ImplementsStationSelectHandler Handler>
class StationSelectAction : public Action {
private:
    Handler handler;
    TileIndex cur_tile = INVALID_TILE;
public:
    StationSelectAction(const Handler &handler) : handler{handler} {}
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

// SizedPlacement classes
class SizedPlacementHandler {
public:
    virtual ~SizedPlacementHandler() = default;
    virtual up<Command> GetCommand(TileIndex tile, StationID to_join) = 0;
    virtual bool Execute(TileIndex tile) = 0;
    virtual std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) = 0;
    virtual std::pair<StationCoverageType, uint> GetCatchmentParams() = 0;
    virtual std::optional<TileArea> GetArea(TileIndex tile) const = 0;
};
template<typename Handler>
concept ImplementsSizedPlacementHandler = std::derived_from<Handler, SizedPlacementHandler>;

template<ImplementsSizedPlacementHandler Handler>
class SizedPlacementAction : public PlacementAction {
private:
    Handler handler;
    TileIndex cur_tile = INVALID_TILE;
public:
    SizedPlacementAction(const Handler &handler) : handler{handler} {}
    ~SizedPlacementAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    std::optional<TileArea> GetArea() const override { return this->handler.GetArea(this->cur_tile); }
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *) override;
};

// DragNDropPlacement classes
class DragNDropPlacementHandler {
public:
    virtual ~DragNDropPlacementHandler() = default;
    virtual up<Command> GetCommand(TileArea area, StationID to_join) = 0;
    virtual bool Execute(TileArea area) = 0;
    virtual std::optional<ObjectHighlight> GetObjectHighlight(TileArea area) = 0;
    virtual std::pair<StationCoverageType, uint> GetCatchmentParams() = 0;
};
template<typename Handler>
concept ImplementsDragNDropPlacementHandler = std::derived_from<Handler, DragNDropPlacementHandler>;

template<ImplementsDragNDropPlacementHandler Handler>
class DragNDropPlacementAction : public PlacementAction {
private:
    TileIndex start_tile = INVALID_TILE;
    TileIndex cur_tile = INVALID_TILE;
    Handler handler;
public:
    DragNDropPlacementAction(const Handler &handler) :handler{handler} {};
    ~DragNDropPlacementAction() override = default;
    void Update(Point pt, TileIndex tile) override;
    std::optional<TileArea> GetArea() const override;
    bool HandleMousePress() override;
    void HandleMouseRelease() override;
    ToolGUIInfo GetGUIInfo() override;
    void OnStationRemoved(const Station *) override;
};

class StationBuildTool : public Tool {
public:
    // static StationID station_to_join;
    // static bool ambigous_join;

    class StationSelectHandler : public citymania::StationSelectHandler {
    public:
        StationBuildTool &tool;
        StationSelectHandler(StationBuildTool &tool) : tool(tool) {}
        ~StationSelectHandler() {}
    };

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
    template<typename Thandler, typename Tcallback, typename Targ>
    bool ExecuteBuildCommand(Thandler *handler, Tcallback callback, Targ arg);
};

// RailStationBuildTool
class RailStationBuildTool : public StationBuildTool {
private:
    class RemoveHandler : public citymania::RemoveHandler {
    public:
        RailStationBuildTool &tool;
        RemoveHandler(RailStationBuildTool &tool) : tool(tool) {}
        ~RemoveHandler() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class SizedPlacementHandler : public citymania::SizedPlacementHandler {
    public:
        RailStationBuildTool &tool;
        SizedPlacementHandler(RailStationBuildTool &tool) : tool(tool) {}
        ~SizedPlacementHandler() override  = default;
        up<Command> GetCommand(TileIndex tile, StationID to_join) override;
        bool Execute(TileIndex tile) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return {this->tool.GetCatchmentParams()}; };
        std::optional<TileArea> GetArea(TileIndex tile) const override;
    };

    class DragNDropPlacementHandler: public citymania::DragNDropPlacementHandler {
    public:
        RailStationBuildTool &tool;
        DragNDropPlacementHandler(RailStationBuildTool &tool) :tool{tool} {}
        ~DragNDropPlacementHandler() override  = default;
        up<Command> GetCommand(TileArea area, StationID to_join) override;
        bool Execute(TileArea area) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileArea area) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return {this->tool.GetCatchmentParams()}; };
    };

    std::optional<ObjectHighlight> GetStationObjectHighlight(TileIndex start_tile, TileIndex end_tile) const;
    std::pair<StationCoverageType, uint> GetCatchmentParams() { return {SCT_ALL, CA_TRAIN}; };

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
    class RemoveHandler : public citymania::RemoveHandler {
    public:
        RoadStopBuildTool &tool;
        RemoveHandler(RoadStopBuildTool &tool) : tool(tool) {}
        ~RemoveHandler() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class DragNDropPlacementHandler: public citymania::DragNDropPlacementHandler {
    public:
        RoadStopBuildTool &tool;
        DragNDropPlacementHandler(RoadStopBuildTool &tool) :tool{tool} {}
        ~DragNDropPlacementHandler() override  = default;
        up<Command> GetCommand(TileArea area, StationID to_join) override;
        bool Execute(TileArea area) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileArea area) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return this->tool.GetCatchmentParams(); };
    };

    std::pair<StationCoverageType, uint>  GetCatchmentParams() {
        if (this->stop_type == ROADSTOP_BUS) return {SCT_PASSENGERS_ONLY, CA_BUS};
        else return {SCT_NON_PASSENGERS_ONLY, CA_TRUCK};
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
    DiagDirection ddir = DIAGDIR_NE;
};

// --- DockBuildTool ---
class DockBuildTool : public StationBuildTool {
private:
    class RemoveHandler : public citymania::RemoveHandler {
    public:
        DockBuildTool &tool;
        RemoveHandler(DockBuildTool &tool) : tool(tool) {}
        ~RemoveHandler() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class SizedPlacementHandler : public citymania::SizedPlacementHandler {
    public:
        DockBuildTool &tool;
        SizedPlacementHandler(DockBuildTool &tool) : tool(tool) {}
        ~SizedPlacementHandler() override = default;
        up<Command> GetCommand(TileIndex tile, StationID to_join) override;
        bool Execute(TileIndex tile) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override { return {SCT_ALL, CA_DOCK}; };
        std::optional<TileArea> GetArea(TileIndex tile) const override;
    };

public:
    DockBuildTool();
	~DockBuildTool() override = default;
    void Update(Point pt, TileIndex tile) override;
    CursorID GetCursor() override;
private:
    enum class Mode { REMOVE, SELECT, SIZED };
    Mode mode;
    DiagDirection ddir;
};

// --- AirportBuildTool ---
class AirportBuildTool : public StationBuildTool {
private:
    class RemoveHandler : public citymania::RemoveHandler {
    public:
        AirportBuildTool &tool;
        RemoveHandler(AirportBuildTool &tool) : tool(tool) {}
        ~RemoveHandler() override = default;
        up<Command> GetCommand(TileArea area) override;
        bool Execute(TileArea area) override;
    };

    class SizedPlacementHandler : public citymania::SizedPlacementHandler {
    public:
        AirportBuildTool &tool;
        SizedPlacementHandler(AirportBuildTool &tool) : tool(tool) {}
        ~SizedPlacementHandler() override = default;
        up<Command> GetCommand(TileIndex tile, StationID to_join) override;
        bool Execute(TileIndex tile) override;
        std::optional<ObjectHighlight> GetObjectHighlight(TileIndex tile) override;
        std::pair<StationCoverageType, uint> GetCatchmentParams() override;
        std::optional<TileArea> GetArea(TileIndex tile) const override;
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
