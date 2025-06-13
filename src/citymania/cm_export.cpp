#include "../stdafx.h"

#include "cm_export.hpp"

#include "../cargotype.h"
#include "../debug.h"
#include "../house.h"  // NUM_HOUSES and HZ_* for _town_land.h
#include "../gfx_func.h"
#include "../gfx_type.h"
#include "../engine_base.h"
#include "../palette_func.h"  // GetColourGradient
#include "../screenshot.h"
#include "../spritecache.h"
#include "../strings_func.h"
#include "../strings_type.h"
#include "../table/palettes.h"
#include "../table/sprites.h"
#include "../table/strings.h"  // for town_land.h
#include "../table/train_sprites.h"
//#include "../table/town_land.h"  // _town_draw_tile_data
#include "../timer/timer_game_tick.h"
#include "../viewport_sprite_sorter.h"
#include "../viewport_type.h"
#include "../window_func.h"
#include "../window_gui.h"
#include "../zoom_func.h"

#include <set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "../safeguards.h"

struct StringSpriteToDraw {
    StringID string;
    Colours colour;
    int32 x;
    int32 y;
    uint64 params[2];
    uint16 width;
};

struct TileSpriteToDraw {
    SpriteID image;
    PaletteID pal;
    const SubSprite *sub;           ///< only draw a rectangular part of the sprite
    int32 x;                        ///< screen X coordinate of sprite
    int32 y;                        ///< screen Y coordinate of sprite
};

struct ChildScreenSpriteToDraw {
    SpriteID image;
    PaletteID pal;
    const SubSprite *sub;           ///< only draw a rectangular part of the sprite
    int32 x;
    int32 y;
    bool relative;
    int next;                       ///< next child to draw (-1 at the end)
};

typedef std::vector<TileSpriteToDraw> TileSpriteToDrawVector;
typedef std::vector<ParentSpriteToDraw> ParentSpriteToDrawVector;
typedef std::vector<ChildScreenSpriteToDraw> ChildScreenSpriteToDrawVector;

extern const DrawBuildingsTileStruct _town_draw_tile_data[(NEW_HOUSE_OFFSET) * 4 * 4];
Viewport SetupScreenshotViewport(ScreenshotType t, uint32_t width = 0, uint32_t height = 0);

namespace citymania {

extern SpriteID (*GetDefaultTrainSprite)(uint8_t, Direction);  // train_cmd.cpp

namespace data_export {

class JsonWriter {
protected:
    int i = 0;
    bool no_comma = true;
    char buffer[128];
    bool js = false;

public:
    std::ofstream f;

    JsonWriter(const std::string &fname, bool js=false) {
        this->js = js;
        f.open(fname.c_str());
        if (this->js) f << "OPENTTD = {";
        no_comma = true;
    }

    ~JsonWriter() {
        this->ident(false);
        if (this->js) f << "}" << std::endl;
        f.close();
    }

    void ident(bool comma=true) {
        if (comma && !no_comma) f << ",";
        no_comma = false;
        f << std::endl;
        for(int j = 0; j < i; j++) f << "  ";
    }

    void key(const char *k) {
        const char *kn;
        for (kn = k + strlen(k); kn >= k && *kn != '>' && *kn != '.'; kn--);
        kn++;
        this->ident();
        f << "\"" << kn << "\": ";
    }

    void value(bool val) {
        f << (val ? "true" : "false");
    }

    void value(unsigned int val) {
        f << val;
    }

    void value(uint64_t val) {
        f << val;
    }

    void value(Money val) {
        f << val;
    }

    void value(int val) {
        f << val;
    }

    void value(const char *v) {
        f << "\"" << v << "\"";
    }

    void value(const std::string &s) {
        f << "\"" << s << "\"";
    }

    template<typename T>
    void kv(const char *k, T v) {
        key(k);
        value(v);
    }

    void ks(const char *k, StringID s) {
        key(k);
        value(GetString(s));
    }

    void begin_dict_with_key(const char *k) {
        key(k);
        f << "{";
        no_comma = true;
        i++;
    }

    void begin_dict() {
        this->ident();
        f << "{";
        no_comma = true;
        i++;
    }

    void end_dict() {
        i--;
        this->ident(false);
        f << "}";
    }

    void begin_list_with_key(const char *k) {
        key(k);
        f << "[";
        no_comma = true;
        i++;
    }

    void end_list() {
        i--;
        this->ident(false);
        f << "]";
    }
};

#define JKV(j, field) j.kv(#field, field)

void WriteHouseSpecInfo(JsonWriter &j) {
    j.begin_list_with_key("house_specs");
    for (uint i = 0; i < NUM_HOUSES; i++) {
        const HouseSpec *hs = HouseSpec::Get(i);
        j.begin_dict();
        j.kv("min_year", hs->min_year.base());
        j.kv("max_year", hs->max_year.base());
        j.kv("population", hs->population);
        j.kv("removal_cost", hs->removal_cost);
        j.kv("name", hs->building_name);
        j.kv("mail_generation", hs->mail_generation);
        j.kv("flags", hs->building_flags.base());
        j.kv("availability", hs->building_availability.base());
        j.kv("enabled", hs->enabled);
        j.end_dict();
    }
    j.end_list();
    j.begin_list_with_key("house_draw_tile_data");
    for (auto &d : _town_draw_tile_data) {
        j.begin_dict();
        j.kv("ground_sprite", d.ground.sprite);
        j.kv("ground_pal", d.ground.pal);
        j.kv("building_sprite", d.building.sprite);
        j.kv("building_pal", d.building.pal);
        j.kv("origin_x", d.origin.x);
        j.kv("origin_y", d.origin.y);
        j.kv("extent_x", d.extent.x);
        j.kv("extent_y", d.extent.y);
        j.kv("extent_z", d.extent.z);
        j.kv("draw_proc", d.draw_proc);
        j.end_dict();
    }
    j.end_list();
}

void WriteCargoSpecInfo(JsonWriter &j) {
    j.begin_list_with_key("cargo_specs");
    char cargo_label[16];
    for (const CargoSpec *cs : CargoSpec::Iterate()) {
        j.begin_dict();
        JKV(j, cs->initial_payment);
        j.kv("id", cs->bitnum);
        j.kv("transit_periods_1", cs->transit_periods[0]);
        j.kv("transit_periods_2", cs->transit_periods[1]);
        JKV(j, cs->weight);
        JKV(j, cs->multiplier);
        JKV(j, cs->is_freight);
        JKV(j, cs->legend_colour.p);
        JKV(j, cs->rating_colour.p);
        JKV(j, cs->sprite);
        j.ks("name", cs->name);
        j.ks("name_single", cs->name_single);
        j.ks("units_volume", cs->units_volume);
        j.ks("quantifier", cs->quantifier);
        j.ks("abbrev", cs->abbrev);

        auto label = cs->label.base();
        for (uint i = 0; i < sizeof(cs->label); i++) {
            cargo_label[i] = GB(label, (uint8_t)(sizeof(label) - i - 1) * 8, 8);
        }
        cargo_label[sizeof(label)] = '\0';
        JKV(j, label);
        j.kv("label_str", cargo_label);
        j.end_dict();
    }
    j.end_list();
}

// extern const Palette _palette;
void WritePaletteInfo(JsonWriter &j) {
    j.begin_list_with_key("palette");
    for (uint i = 0; i < 256; i++) {
        j.begin_dict();
        auto &c = _palette.palette[i];
        JKV(j, c.r);
        JKV(j, c.g);
        JKV(j, c.b);
        std::ostringstream hex;
        hex << "#" << std::hex << std::setfill('0');
        hex << std::setw(2) << (int)c.r;
        hex << std::setw(2) << (int)c.g;
        hex << std::setw(2) << (int)c.b;
        j.kv("hex", hex.str());
        j.end_dict();
    }
    j.end_list();
    j.begin_list_with_key("gradients");
    for (auto i = 0; i < COLOUR_END; i++) {
        if (i != 0) j.f << ",";
        j.f << std::endl << "[";
        for (auto k = 0; k < 8; k++) {
            if (k != 0) j.f << ", ";
            j.f << GetColourGradient((Colours)i, (ColourShade)k).p << " ";
        }
        j.f << "]";
    }
    j.end_list();
    // const byte *remap = GetNonSprite(GB(PALETTE_TO_RED, 0, PALETTE_WIDTH), SpriteType::Recolour) + 1;
}

void WriteEngineInfo(JsonWriter &j) {
    j.begin_list_with_key("engines");
    for (const Engine *e : Engine::Iterate()) {
        if (e->type != VEH_TRAIN) continue;
        j.begin_dict();
        JKV(j, e->index.base());
        j.kv("name", e->name);
        j.kv("cost", e->GetCost());
        j.kv("running_cost", e->GetRunningCost());
        {
            j.begin_dict_with_key("info");
            JKV(j, e->info.cargo_type);
            JKV(j, e->info.cargo_age_period);
            JKV(j, e->info.climates.base());
            JKV(j, e->info.base_intro.base());
            JKV(j, e->info.lifelength.base());
            JKV(j, e->info.base_life.base());
            JKV(j, e->info.refit_mask);
            JKV(j, e->info.refit_cost);
            JKV(j, e->info.load_amount);
            j.ks("name", e->info.string_id);
            j.end_dict();
        }
        {
            const RailVehicleInfo *rvi = &e->u.rail;
            j.begin_dict_with_key("rail");
            JKV(j, rvi->image_index);
            JKV(j, rvi->railveh_type);
            JKV(j, rvi->railtypes.base());
            JKV(j, rvi->max_speed);
            JKV(j, rvi->power);
            JKV(j, rvi->weight);
            JKV(j, rvi->running_cost);
            JKV(j, rvi->running_cost_class);
            JKV(j, rvi->engclass);
            JKV(j, rvi->tractive_effort);
            JKV(j, rvi->air_drag);
            JKV(j, rvi->capacity);
            SpriteID sprite = GetDefaultTrainSprite(e->original_image_index, DIR_W);
            JKV(j, sprite);
            if (rvi->railveh_type == RAILVEH_MULTIHEAD) {
                j.kv("rev_sprite",
                     GetDefaultTrainSprite(e->original_image_index + 1, DIR_W));
            } else if (rvi->railveh_type == RAILVEH_WAGON) {
                j.kv("full_sprite",
                     sprite + _wagon_full_adder[e->original_image_index]);
            }
            j.end_dict();
        }
        j.end_dict();
    }
    j.end_list();
}

} // namespace export

void ExportOpenttdData(const std::string &filename) {
    data_export::JsonWriter j(filename, true);
    data_export::WriteHouseSpecInfo(j);
    data_export::WriteCargoSpecInfo(j);
    data_export::WritePaletteInfo(j);
    data_export::WriteEngineInfo(j);
}

extern void ViewportExportDrawBegin(const Viewport &vp, int left, int top, int right, int bottom);
extern void ViewportExportDrawEnd();

extern TileSpriteToDrawVector &ViewportExportGetTileSprites();
extern ParentSpriteToSortVector &ViewportExportGetSortedParentSprites();
extern ChildScreenSpriteToDrawVector &ViewportExportGetChildSprites();

void ViewportExportJson(const Viewport &vp, int left, int top, int right, int bottom) {
    ViewportExportDrawBegin(vp, left, top, right, bottom);

    auto fname = fmt::format("snaps/tick_{}.json", TimerGameTick::counter);
    Debug(misc, 0, "Exporting tick {} into {} box ({},{})-({},{}) ", TimerGameTick::counter, fname, left, top, right, bottom);
    data_export::JsonWriter j(fname);
    j.begin_dict();
    j.kv("tick", TimerGameTick::counter);
    j.begin_list_with_key("tile_sprites");
    for (auto &ts : ViewportExportGetTileSprites()) {
        j.begin_dict();
        j.kv("image", ts.image);
        j.kv("pal", ts.pal);
        j.kv("x", ts.x);
        j.kv("y", ts.y);
        if (ts.sub) {
            j.begin_dict_with_key("sub");
            j.kv("left", ts.sub->left);
            j.kv("top", ts.sub->top);
            j.kv("right", ts.sub->right);
            j.kv("bottom", ts.sub->bottom);
            j.end_dict();
        }
        j.end_dict();
    }
    j.end_list();
    j.begin_list_with_key("parent_sprites");
    auto &child_sprites = ViewportExportGetChildSprites();
    for (const ParentSpriteToDraw *s : ViewportExportGetSortedParentSprites()) {
        j.begin_dict();
        j.kv("image", s->image);
        j.kv("pal", s->pal);
        j.kv("x", s->x);
        j.kv("y", s->y);
        j.kv("left", s->left);
        j.kv("top", s->top);
        j.kv("xmin", s->xmin);
        j.kv("ymin", s->ymin);
        j.kv("zmin", s->zmin);
        j.kv("xmax", s->xmax);
        j.kv("ymax", s->ymax);
        j.kv("zmax", s->zmax);
        if (s->sub) {
            j.begin_dict_with_key("sub");
            j.kv("left", s->sub->left);
            j.kv("top", s->sub->top);
            j.kv("right", s->sub->right);
            j.kv("bottom", s->sub->bottom);
            j.end_dict();
        }
        int child_idx = s->first_child;
        if (child_idx >= 0) {
            j.begin_list_with_key("children");
            while (child_idx >= 0) {
                const ChildScreenSpriteToDraw *cs = &child_sprites[child_idx];
                child_idx = cs->next;
                j.begin_dict();
                j.kv("image", cs->image);
                j.kv("pal", cs->pal);
                j.kv("x", cs->x);
                j.kv("y", cs->y);
                if (cs->sub) {
                    j.begin_dict_with_key("sub");
                    j.kv("left", cs->sub->left);
                    j.kv("top", cs->sub->top);
                    j.kv("right", cs->sub->right);
                    j.kv("bottom", cs->sub->bottom);
                    j.end_dict();
                }
                j.end_dict();
            }
            j.end_dict();
        }
        j.end_dict();

    }
    j.end_list();
    j.end_dict();

    ViewportExportDrawEnd();
}

void ExportFrameSpritesJson() {
    Viewport vp = SetupScreenshotViewport(SC_VIEWPORT);
    Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
    vp.zoom = w->viewport->zoom;
    ViewportExportJson(vp,
        vp.virtual_left,
        vp.virtual_top,
        vp.virtual_left + vp.virtual_width,
        vp.virtual_top + vp.virtual_height
    );
}

void ExportSprite(SpriteID sprite, SpriteType type) {
    static std::set<SpriteID> exported;
    if (exported.find(sprite) != exported.end()) return;
    auto fname = fmt::format("snaps/sprite_{}.bin", sprite);
    std::ofstream f(fname, std::ios::binary);
    uint size;
    void *raw = GetRawSprite(sprite, type);
    if (type == SpriteType::Recolour) size = 257;
    else size = *(((size_t *)raw) - 1);
    f.write((char *)raw, size);
    exported.insert(sprite);
}

void ExportSpriteAndPal(SpriteID img, SpriteID pal) {
    SpriteID real_sprite = GB(img, 0, SPRITE_WIDTH);
    if (HasBit(img, PALETTE_MODIFIER_TRANSPARENT)) {
        ExportSprite(GB(pal, 0, PALETTE_WIDTH), SpriteType::Recolour);
        ExportSprite(real_sprite, SpriteType::Normal);
    } else if (pal != PAL_NONE) {
        if (HasBit(pal, PALETTE_TEXT_RECOLOUR)) {
            //SetColourRemap((TextColour)GB(pal, 0, PALETTE_WIDTH));
        } else {
            ExportSprite(GB(pal, 0, PALETTE_WIDTH), SpriteType::Recolour);
        }
        ExportSprite(real_sprite, SpriteType::Normal);
    } else {
        ExportSprite(real_sprite, SpriteType::Normal);
    }
}

void ViewportExport(const Viewport &vp, int left, int top, int right, int bottom) {
    ViewportExportDrawBegin(vp, left, top, right, bottom);

    auto fname = fmt::format("snaps/tick_{}.bin", TimerGameTick::counter);
    Debug(misc, 0, "Exporting tick {} into {} box ({},{})-({},{}) ", TimerGameTick::counter, fname, left, top, right, bottom);
    std::ofstream f(fname, std::ios::binary);
    auto &tile_sprites = ViewportExportGetTileSprites();
    uint64 n = tile_sprites.size();
    f.write((char *)&n, 8);
    f.write((const char *)tile_sprites.data(), n * sizeof(TileSpriteToDraw));
    for (const auto &ts : tile_sprites) ExportSpriteAndPal(ts.image, ts.pal);

    auto &parent_sprites = ViewportExportGetSortedParentSprites();
    n = parent_sprites.size();
    f.write((char *)&n, 8);
    for (const ParentSpriteToDraw *s : ViewportExportGetSortedParentSprites()) {
        f.write((const char *)s, sizeof(ParentSpriteToDraw) - 4);
        ExportSpriteAndPal(s->image, s->pal);
    }

    auto &child_sprites = ViewportExportGetChildSprites();
    n = child_sprites.size();
    f.write((char *)&n, 8);
    f.write((const char *)child_sprites.data(), n * sizeof(ChildScreenSpriteToDraw));
    for (const auto &cs : child_sprites) ExportSpriteAndPal(cs.image, cs.pal);

    ViewportExportDrawEnd();
}

bool _is_recording = false;

void ExportFrameSprites() {
    if (!_is_recording) return;
    Viewport vp = SetupScreenshotViewport(SC_VIEWPORT);
    Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
    vp.zoom = w->viewport->zoom;
    ViewportExport(vp,
        vp.virtual_left,
        vp.virtual_top,
        vp.virtual_left + vp.virtual_width,
        vp.virtual_top + vp.virtual_height
    );
}

void StartRecording() {
    _is_recording = true;
}

void StopRecording() {
    _is_recording = false;
}

} // namespace citymania
