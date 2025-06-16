#ifndef CM_MINIMAP_HPP
#define CM_MINIMAP_HPP

#include "../smallmap_gui.h"
#include "../core/endian_func.hpp"
#include "../timer/timer.h"
#include "../timer/timer_window.h"
#include "../blitter/base.hpp"
#include "../industry.h"

extern CompanyID _local_company;

namespace citymania {

#define MKCOLOUR(x) TO_LE32X(x)
#define MKCOLOURGROUP(x, y) \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(x) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(y) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(x) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(x) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(y) << 16) | ((uint32)(x) << 8) | (uint32)(x)), \
    MKCOLOUR(((uint32)(y) << 24) | ((uint32)(y) << 16) | ((uint32)(y) << 8) | (uint32)(x))


static const  uint32 _yellow_map_heights[] = {
    MKCOLOURGROUP(0x22, 0x23),
    MKCOLOURGROUP(0x23, 0x24),
    MKCOLOURGROUP(0x24, 0x25),
    MKCOLOURGROUP(0x25, 0x26),
    MKCOLOURGROUP(0x27, 0x27),
    MKCOLOUR(0x27272727),
};

static const  uint32 _orange_map_heights[] = {
    MKCOLOURGROUP(0xC0, 0xC1),
    MKCOLOURGROUP(0xC1, 0xC2),
    MKCOLOURGROUP(0xC2, 0xC3),
    MKCOLOURGROUP(0xC3, 0xC4),
    MKCOLOURGROUP(0xC4, 0xC5),
    MKCOLOUR(0xC5C5C5C5),
};

void BuildLinkStatsLegend();

void BuildIndustriesLegend();
void ShowSmallMap();
void BuildLandLegend();
void BuildOwnerLegend();

void minimap_add_industry(const Industry *ind);
void minimap_remove_industry(const Industry *ind);
void minimap_init_industries();


class NWidgetSmallmapDisplay;

struct LegendAndColour {
    uint8_t colour;            ///< Colour of the item on the map.
    StringID legend;           ///< String corresponding to the coloured item.
    IndustryType type;         ///< Type of industry. Only valid for industry entries.
    uint8_t height;            ///< Height in tiles. Only valid for height legend entries.
    CompanyID company;         ///< Company to display. Only valid for company entries of the owner legend.
    bool show_on_map;          ///< For filtering industries, if \c true, industry is shown on the map in colour.
    bool end;                  ///< This is the end of the list.
    bool col_break;            ///< Perform a column break and go further at the next column.
};

/** Types of legends in the #WID_SM_LEGEND widget. */
enum SmallMapType : byte {
    SMT_CONTOUR,
    SMT_VEHICLES,
    SMT_INDUSTRY,
    SMT_LINKSTATS,
    SMT_ROUTES,
    SMT_VEGETATION,
    SMT_OWNER,
    CM_SMT_IMBA,
};
DECLARE_ENUM_AS_ADDABLE(SmallMapType)

/** Class managing the smallmap window. */
class SmallMapWindow : public Window {
protected:

    /** Available kinds of zoomlevel changes. */
    enum ZoomLevelChange {
        ZLC_INITIALIZE, ///< Initialize zoom level.
        ZLC_ZOOM_OUT,   ///< Zoom out.
        ZLC_ZOOM_IN,    ///< Zoom in.
    };

    static SmallMapType map_type; ///< Currently displayed legends.
    static bool show_towns;       ///< Display town names in the smallmap.
    static int map_height_limit;   ///< Currently used/cached maximum heightlevel.

    static const uint LEGEND_BLOB_WIDTH = 8;              ///< Width of the coloured blob in front of a line text in the #WID_SM_LEGEND widget.
    static const uint INDUSTRY_MIN_NUMBER_OF_COLUMNS = 2; ///< Minimal number of columns in the #WID_SM_LEGEND widget for the #SMT_INDUSTRY legend.
    static const uint FORCE_REFRESH_PERIOD = 930; ///< map is redrawn after that many milliseconds.
    static const uint BLINK_PERIOD         = 450; ///< highlight blinking interval in milliseconds.

    uint min_number_of_columns;    ///< Minimal number of columns in legends.
    uint min_number_of_fixed_rows; ///< Minimal number of rows in the legends for the fixed layouts only (all except #SMT_INDUSTRY).
    uint column_width;             ///< Width of a column in the #WID_SM_LEGEND widget.
    uint legend_width;             ///< Width of legend 'blob'.

    int32 scroll_x;  ///< Horizontal world coordinate of the base tile left of the top-left corner of the smallmap display.
    int32 scroll_y;  ///< Vertical world coordinate of the base tile left of the top-left corner of the smallmap display.
    int32 subscroll; ///< Number of pixels (0..3) between the right end of the base tile and the pixel at the top-left corner of the smallmap display.
    int tile_zoom;        ///< Zoom level. Bigger number means more zoom-out (further away).
    int ui_zoom;        ///< Zoom level. Bigger number means more zoom-out (further away).
    int zoom = 1;        ///< Zoom level. Bigger number means more zoom-out (further away).

    LinkGraphOverlay *overlay;

    struct {
        int32 scroll_x;
        int32 scroll_y;
        uint width = 0;
        uint height = 0;
        int zoom;
        Dimension max_sign;
        std::vector<std::tuple<const Town *, uint32, uint>> towns;
    } town_cache;
    mutable Dimension industry_max_sign;

    static void BreakIndustryChainLink();
    Point SmallmapRemapCoords(int x, int y) const;

    /**
     * Draws vertical part of map indicator
     * @param x X coord of left/right border of main viewport
     * @param y Y coord of top border of main viewport
     * @param y2 Y coord of bottom border of main viewport
     */
    static inline void DrawVertMapIndicator(int x, int y, int y2)
    {
        GfxFillRect(x, y,      x, y + 3, PC_VERY_LIGHT_YELLOW);
        GfxFillRect(x, y2 - 3, x, y2,    PC_VERY_LIGHT_YELLOW);
    }

    /**
     * Draws horizontal part of map indicator
     * @param x X coord of left border of main viewport
     * @param x2 X coord of right border of main viewport
     * @param y Y coord of top/bottom border of main viewport
     */
    static inline void DrawHorizMapIndicator(int x, int x2, int y)
    {
        GfxFillRect(x,      y, x + 3, y, PC_VERY_LIGHT_YELLOW);
        GfxFillRect(x2 - 3, y, x2,    y, PC_VERY_LIGHT_YELLOW);
    }

    /**
     * Compute minimal required width of the legends.
     * @return Minimally needed width for displaying the smallmap legends in pixels.
     */
    inline uint GetMinLegendWidth() const
    {
        return WidgetDimensions::scaled.framerect.left + this->min_number_of_columns * this->column_width;
    }

    /**
     * Return number of columns that can be displayed in \a width pixels.
     * @return Number of columns to display.
     */
    inline uint GetNumberColumnsLegend(uint width) const
    {
        return width / this->column_width;
    }

    /**
     * Compute height given a number of columns.
     * @param num_columns Number of columns.
     * @return Needed height for displaying the smallmap legends in pixels.
     */
    inline uint GetLegendHeight(uint num_columns) const
    {
        return WidgetDimensions::scaled.framerect.Vertical() +
                this->GetNumberRowsLegend(num_columns) * GetCharacterHeight(FS_SMALL);
    }

    /**
     * Get a bitmask for company links to be displayed. Usually this will be
     * the _local_company. Spectators get to see all companies' links.
     * @return Company mask.
     */
    inline uint32 GetOverlayCompanyMask() const
    {
        return Company::IsValidID(_local_company) ? 1U << _local_company : 0xffffffff;
    }

    /** Blink the industries (if selected) on a regular interval. */
    IntervalTimer<TimerWindow> blink_interval = {TIMER_BLINK_INTERVAL, [this](auto) {
        Blink();
    }};

    /** Update the whole map on a regular interval. */
    IntervalTimer<TimerWindow> refresh_interval = {std::chrono::milliseconds(930), [this](auto) {
        ForceRefresh();
    }};

    void UpdateLinks();
    void Blink();
    void ForceRefresh();
    void RebuildColourIndexIfNecessary();
    uint GetNumberRowsLegend(uint columns) const;
    void SelectLegendItem(int click_pos, LegendAndColour *legend, int end_legend_item, int begin_legend_item = 0);
    void SwitchMapType(SmallMapType map_type);
    void SetNewScroll(int sx, int sy, int sub);

    void DrawMapIndicators() const;
    void DrawSmallMapColumn(void *dst, uint xc, uint yc, int pitch, int reps, int start_pos, int end_pos, int y, int end_y, Blitter *blitter) const;
    void DrawVehicles(const DrawPixelInfo *dpi, Blitter *blitter) const;
    void DrawIndustryProduction(const DrawPixelInfo *dpi) const;
    void DrawTowns(const DrawPixelInfo *dpi) const;
    void DrawSmallMap(DrawPixelInfo *dpi) const;

    Point TileToPixel(int tx, int ty) const;
    Point PixelToTile(int tx, int ty) const;
    void SetZoomLevel(ZoomLevelChange change, const Point *zoom_pt);
    void SetOverlayCargoMask();
    void SetupWidgetData();
    uint32 GetTileColours(const TileArea &ta) const;

    int GetPositionOnLegend(Point pt);

    void UpdateTownCache(bool force);

public:
    friend class citymania::NWidgetSmallmapDisplay;

    SmallMapWindow(WindowDesc *desc, int window_number);
    virtual ~SmallMapWindow();

    void SmallMapCenterOnCurrentPos();
    Point GetStationMiddle(const Station *st) const;

    void SetStringParameters(int widget) const override;
    void OnInit() override;
    void OnPaint() override;
    void DrawWidget(const Rect &r, int widget) const override;
    void OnClick(Point pt, int widget, int click_count) override;
    void OnInvalidateData(int data = 0, bool gui_scope = true) override;
    bool OnRightClick(Point pt, int widget) override;
    void OnMouseWheel(int wheel) override;
    // void OnHundredthTick() override;
    void OnScroll(Point delta) override;
    void OnMouseOver(Point pt, int widget) override;
};

} // namespace citymania

#endif  /* CITYMANIA_MINIMAP_HPP */
