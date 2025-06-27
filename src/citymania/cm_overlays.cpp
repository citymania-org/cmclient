#include "../stdafx.h"

#include "cm_overlays.hpp"

#include "cm_client_list_gui.hpp"

#include "../blitter/factory.hpp"  // Blitter BlitterFactory
#include "../core/geometry_func.hpp"  // maxdim
#include "../core/geometry_type.hpp"  // PointDimension
#include "../core/alloc_type.hpp"  // ReusableBuffer
#include "../gfx_func.h"  // _screen
#include "../network/network.h"  // _networking
#include "../video/video_driver.hpp"  // VideoDriver
#include "../sprite.h"  // PALETTE_TO_TRANSPARENT
#include "../window_gui.h"  // Window
#include "../window_func.h"  // FindWindowByID
#include "../zoom_func.h"

#include "../safeguards.h"
#include "../gfx_type.h"
#include "../table/sprites.h"

namespace citymania {

[[maybe_unused]] static bool Intersects(PointDimension rect, Point pos, Point size) {
    return (
        pos.x + size.x >= rect.x &&
        pos.x <= rect.x + rect.width &&
        pos.y + size.y >= rect.y &&
        pos.y <= rect.y + rect.height
    );
}

static bool Intersects(PointDimension rect, int left, int top, int right, int bottom) {
    return (
        right >= rect.x &&
        left <= rect.x + rect.width &&
        bottom >= rect.y &&
        top <= rect.y + rect.height
    );
}


class OverlayWindow {
protected:
    PointDimension box;
    bool drawn = false;
    bool dirty = true;
    PointDimension backup_box;
    ReusableBuffer<uint8_t> backup;
    int padding;
    int x = 0;
    int y = 0;

public:
    OverlayWindow() {}
	virtual ~OverlayWindow() {}

    void SetDirty() {
        this->dirty = true;
    }

    void UpdateSize() {
        this->padding = ScaleGUITrad(3);
        auto dim = this->GetContentDimension();
        this->box.width = dim.width + 2 * this->padding;
        this->box.height = dim.height + 2 * this->padding;
        this->box.x = this->x - this->box.width / 2;
        this->box.y = this->y - this->box.height;

        // Window *toolbar = FindWindowById(WC_MAIN_TOOLBAR, 0);
        // if (toolbar != nullptr && toolbar->left < this->box.x + this->box.width + this->padding) {
        //     this->box.y = toolbar->top + toolbar->height + this->padding;
        // }

        if (this->box.x < 0) this->box.x = 0;
        if (this->box.y < 0) this->box.y = 0;
        if (this->box.x >= _screen.width) this->box.x = _screen.width - 1;
        if (this->box.y >= _screen.height) this->box.y = _screen.height - 1;

        if (this->box.x + this->box.width > _screen.width)
            this->box.width = _screen.width - this->box.x;
        if (this->box.y + this->box.height > _screen.height)
            this->box.height = _screen.height - this->box.y;
    }

    virtual Dimension GetContentDimension()=0;

    void Undraw(int left, int top, int right, int bottom) {
        auto &box = this->backup_box;
        if (!this->drawn) return;

        if (!Intersects(box, left, top, right, bottom)) return;

        if (box.x + box.width > _screen.width) return;
        if (box.y + box.height > _screen.height) return;

        Blitter *blitter = BlitterFactory::GetCurrentBlitter();

        /* Put our 'shot' back to the screen */
        blitter->CopyFromBuffer(blitter->MoveTo(_screen.dst_ptr, box.x, box.y), this->backup.GetBuffer(), box.width, box.height);
        /* And make sure it is updated next time */
        VideoDriver::GetInstance()->MakeDirty(box.x, box.y, box.width, box.height);

        this->drawn = false;
        this->dirty = true;
    }

    virtual bool IsVisible()=0;
    virtual void DrawContent(Rect rect)=0;

    void Draw() {
        if (!this->dirty) return;
        this->dirty = false;

        Blitter *blitter = BlitterFactory::GetCurrentBlitter();

        bool was_drawn = this->drawn;

        if (this->drawn) {
            /* Put our 'shot' back to the screen */
            blitter->CopyFromBuffer(blitter->MoveTo(_screen.dst_ptr, this->backup_box.x, this->backup_box.y), this->backup.GetBuffer(), this->backup_box.width, this->backup_box.height);
            /* And make sure it is updated next time */
            VideoDriver::GetInstance()->MakeDirty(this->backup_box.x, this->backup_box.y, this->backup_box.width, this->backup_box.height);

            this->drawn = false;
        }

        if (!this->IsVisible()) return;

        this->UpdateSize();
        if (this->box.width <= 0 || this->box.height <= 0) return;

        if (!was_drawn ||
                this->box.x != this->backup_box.x ||
                this->box.y != this->backup_box.y ||
                this->box.width != this->backup_box.width ||
                this->box.height != this->backup_box.height) {
            uint8_t *buffer = this->backup.Allocate(BlitterFactory::GetCurrentBlitter()->BufferSize(this->box.width, this->box.height));
            blitter->CopyToBuffer(blitter->MoveTo(_screen.dst_ptr, this->box.x, this->box.y), buffer, this->box.width, this->box.height);
            this->backup_box = this->box;
        }

        _cur_dpi = &_screen; // switch to _screen painting

        auto rect = Rect{this->box.x, this->box.y, this->box.x + this->box.width, this->box.y + this->box.height};

        /* Paint a half-transparent box behind the client list */
        GfxFillRect(rect.left, rect.top, rect.right - 1, rect.bottom - 1,
            PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR // black, but with some alpha for background
        );

        this->DrawContent(rect.Shrink(this->padding));

        /* Make sure the data is updated next flush */
        VideoDriver::GetInstance()->MakeDirty(this->box.x, this->box.y, this->box.width, this->box.height);

        this->drawn = true;
    }
};

class IconTextAligner {
public:
    int line_height;
    int max_height;
    int padding;
    int text_ofs_x;
    int text_ofs_y;

    Dimension Init(BuildInfoOverlayData &data, int padding, FontSize font_size) {
        // TODO rtl
        auto font_height = GetCharacterHeight(font_size);
        this->max_height = font_height;
        this->line_height = this->max_height + padding;
        this->text_ofs_x = 0;
        this->text_ofs_y = 0;
        this->padding = padding;

        if (data.size() == 0) { return {0, 0}; }

        Dimension text_dim{0, 0};
        Dimension icon_dim{0, 0};
        for (const auto &[icon, s] : data) {
            text_dim = maxdim(text_dim, GetStringBoundingBox(s));
            if (icon != PAL_NONE)
                icon_dim = maxdim(icon_dim, GetSpriteSize(icon));
        }
        this->max_height = std::max<int>(this->max_height, icon_dim.height);
        this->text_ofs_y = (this->max_height - font_height) / 2;
        this->line_height = max_height + padding;
        if (icon_dim.width > 0)
            this->text_ofs_x = icon_dim.width + this->padding;
        return {
            text_dim.width + this->text_ofs_x,
            (uint)data.size() * this->line_height - padding
        };
    }

    std::pair<Point, Rect> GetPositions(Rect rect, int row, SpriteID icon) {
        auto icon_height = this->max_height;
        auto ofs_x = 0;
        if (icon != PAL_NONE) {
            icon_height = GetSpriteSize(icon).height;
            ofs_x = this->text_ofs_x;
        }
        return {
            {
                rect.left,
                rect.top + row * this->line_height + (this->max_height - icon_height) / 2,
            },
            {
                rect.left + ofs_x,
                rect.top + row * this->line_height + this->text_ofs_y,
                rect.right,
                rect.bottom
            },
        };
    }
};

class BuildInfoOverlay: public OverlayWindow {
	bool visible = false;
	BuildInfoOverlayData data;
    IconTextAligner aligner;
public:
    ~BuildInfoOverlay() override {}

	void Show(int x, int y, BuildInfoOverlayData data) {
		this->x = x;
		this->y = y;
		this->data = data;
		this->visible = true;
		this->SetDirty();
	}

	void Hide() {
		this->visible = false;
	}

    bool IsVisible() override {
    	return this->visible;
    }

    Dimension GetContentDimension() override {
        return aligner.Init(this->data, this->padding, FS_NORMAL);
    }

    void DrawContent(Rect rect) override {
        int row = 0;
    	for (const auto &[icon, s] : this->data) {
            auto [ipos, srect] = this->aligner.GetPositions(rect, row, icon);

            DrawString(srect.left, srect.right, srect.top, s);
            if (icon != PAL_NONE)
                DrawSprite(icon, PAL_NONE, ipos.x, ipos.y);

            row++;
    	}
    }
};

static BuildInfoOverlay _build_info_overlay{};

void UndrawOverlays(int left, int top, int right, int bottom) {
	_build_info_overlay.Undraw(left, top, right, bottom);
	if (_networking) UndrawClientList(left, top, right, bottom);
}

void DrawOverlays() {
	_build_info_overlay.Draw();
	if (_networking) DrawClientList();
}

void ShowBuildInfoOverlay(int x, int y, BuildInfoOverlayData data) {
	_build_info_overlay.Show(x, y, data);
}

void HideBuildInfoOverlay() {
	_build_info_overlay.Hide();
}


} // namespace citymania
