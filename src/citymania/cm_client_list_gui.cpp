#include "../stdafx.h"

#include "cm_client_list_gui.hpp"

#include "../blitter/factory.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_gui.h"
#include "../console_func.h"
#include "../debug.h"
#include "../core/geometry_type.hpp"
#include "../gfx_func.h"
#include "../network/network.h"
#include "../network/network_base.h"
#include "../network/network_func.h"
#include "../sprite.h"
#include "../strings_func.h"
#include "../table/sprites.h"
#include "../video/video_driver.hpp"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_func.h"

#include "../safeguards.h"

namespace citymania {

// static bool Intersects(PointDimension rect, Point pos, Point size) {
//     return (
//         pos.x + size.x >= rect.x &&
//         pos.x <= rect.x + rect.width &&
//         pos.y + size.y >= rect.y &&
//         pos.y <= rect.y + rect.height
//     );
// }

static bool Intersects(PointDimension rect, int left, int top, int right, int bottom) {
    return (
        right >= rect.x &&
        left <= rect.x + rect.width &&
        bottom >= rect.y &&
        top <= rect.y + rect.height
    );
}


class ClientListOverlay {
protected:
    PointDimension box;
    bool drawn = false;
    bool dirty = true;
    PointDimension backup_box;
    ReusableBuffer<uint8_t> backup;
    int padding;
    int line_height;
    int text_offset_y;
    int icon_offset_y;

public:
    void SetDirty() {
        this->dirty = true;
    }
    void UpdateSize() {
        this->padding = ScaleGUITrad(3);
        auto icon_size = GetSpriteSize(CM_SPR_COMPANY_ICON);
        auto common_height = std::max<int>(icon_size.height, GetCharacterHeight(FS_NORMAL));
        this->line_height = common_height + this->padding;
        this->text_offset_y = (common_height - GetCharacterHeight(FS_NORMAL)) / 2;
        this->icon_offset_y = (common_height - icon_size.height) / 2;
        this->box.height = this->padding;
        this->box.width = 0;
        for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
            this->box.width = std::max<int>(this->box.width, GetStringBoundingBox(ci->client_name).width);
            this->box.height += this->line_height;
        }
        this->box.width += this->padding * 3 + icon_size.width;
        this->box.x = this->padding;
        this->box.y = this->padding;
        Window *toolbar = FindWindowById(WC_MAIN_TOOLBAR, 0);
        if (toolbar != nullptr && toolbar->left < this->box.x + this->box.width + this->padding) {
            this->box.y = toolbar->top + toolbar->height + this->padding;
        }

        if (this->box.x + this->box.width > _screen.width)
            this->box.width = _screen.width - this->box.x;
        if (this->box.y + this->box.height > _screen.height)
            this->box.height = _screen.height - this->box.y;
    }

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

        if (_game_mode != GM_NORMAL) return;
        if (_iconsole_mode != ICONSOLE_CLOSED) return;
        if (!_networking) return;
        if (!_settings_client.gui.cm_show_client_overlay) return;

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

        /* Paint a half-transparent box behind the client list */
        GfxFillRect(this->box.x, this->box.y, this->box.x + this->box.width - 1, this->box.y + this->box.height - 1,
            PALETTE_TO_TRANSPARENT, FILLRECT_RECOLOUR // black, but with some alpha for background
        );

        int y = this->box.y + this->padding;
        int x = this->box.x + this->padding;

        auto text_left = x + GetSpriteSize(CM_SPR_COMPANY_ICON).width + this->padding;
        auto text_right = this->box.x + this->box.width - 1 - this->padding;

        const std::pair<TextColour, SpriteID> STYLES[] = {
            {TC_SILVER, PAL_NONE},
            {TC_ORANGE, CM_SPR_HOST_WHITE},
            {TC_WHITE , CM_SPR_PLAYER_WHITE},
            {TC_SILVER, CM_SPR_COMPANY_ICON},
            {TC_ORANGE, CM_SPR_COMPANY_ICON_HOST},
            {TC_WHITE , CM_SPR_COMPANY_ICON_PLAYER},
        };
        std::vector<std::tuple<CompanyID, std::string, uint8>> clients;
        for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
            uint8 style = 0;
            if (ci->client_id == _network_own_client_id) style = 2;
            if (ci->client_id == CLIENT_ID_SERVER) style = 1;
            if (Company::IsValidID(ci->client_playas)) style += 3;
            clients.emplace_back(ci->client_playas, ci->client_name, style);

        }
        std::sort(clients.begin(), clients.end());
        for (const auto &[playas, name, style] : clients) {
            auto [colour, icon] = STYLES[style];
            // auto colour = TC_SILVER;
            // if (ci->client_id == _network_own_client_id) colour = TC_WHITE;
            // if (ci->client_id == CLIENT_ID_SERVER) colour = TC_ORANGE;
            DrawString(text_left, text_right, y + this->text_offset_y, name, colour);

            if (icon != PAL_NONE) DrawSprite(icon, GetCompanyPalette(playas), x, y + this->icon_offset_y);
                // DrawCompanyIcon(playas, x, y + this->icon_offset_y);
            y += this->line_height;
        }

        /* Make sure the data is updated next flush */
        VideoDriver::GetInstance()->MakeDirty(this->box.x, this->box.y, this->box.width, this->box.height);

        this->drawn = true;
    }

};

ClientListOverlay _client_list_overlay;

void SetClientListDirty() {
    _client_list_overlay.SetDirty();
}

void UndrawClientList(int left, int top, int right, int bottom) {
    _client_list_overlay.Undraw(left, top, right, bottom);
}

void DrawClientList() {
    _client_list_overlay.Draw();
}

}  // namespace citymania
