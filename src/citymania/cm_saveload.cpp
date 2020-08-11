#include "../stdafx.h"

#include "../company_base.h"
#include "../date_func.h"
#include "../debug.h"
#include "../saveload/saveload.h"
#include "../town.h"

#include "cm_bitstream.hpp"
#include "cm_saveload.hpp"
#include "cm_settings.hpp"
#include "cm_game.hpp"
#include "cm_main.hpp"

namespace citymania {

static const uint SAVEGAME_DATA_FORMAT_VERSION = 2;
static const uint CITYMANIA_GRFID = 0x534B0501U;  // Luukland_Citybuilder grf id actually


// copied form storage_sl.cpp
static const SaveLoad _storage_desc[] = {
     SLE_CONDVAR(PersistentStorage, grfid,    SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
     SLE_CONDARR(PersistentStorage, storage,  SLE_UINT32,  16,           SLV_161, SLV_EXTEND_PERSISTENT_STORAGE),
     SLE_CONDARR(PersistentStorage, storage,  SLE_UINT32, 256,           SLV_EXTEND_PERSISTENT_STORAGE, SL_MAX_VERSION),
     SLE_END()
};


static void EncodeTowns(BitOStream &bs)
{
    for (const Town *t : Town::Iterate()) {
        bs.WriteBytes(t->cm.growing_by_chance, 1);
        bs.WriteBytes(t->cm.houses_reconstructed_this_month, 2);
        bs.WriteBytes(t->cm.houses_reconstructed_last_month, 2);
        bs.WriteBytes(t->cm.houses_demolished_this_month, 2);
        bs.WriteBytes(t->cm.houses_demolished_last_month, 2);
        bs.WriteBytes(t->cm.hs_total, 4);
        bs.WriteBytes(t->cm.hs_this_month, 2);
        bs.WriteBytes(t->cm.hs_last_month, 2);
        bs.WriteBytes(t->cm.cs_total, 4);
        bs.WriteBytes(t->cm.cs_this_month, 2);
        bs.WriteBytes(t->cm.cs_last_month, 2);
        bs.WriteBytes(t->cm.hr_total, 4);
        bs.WriteBytes(t->cm.hr_this_month, 2);
        bs.WriteBytes(t->cm.hr_last_month, 2);
    }
}

static void DecodeTowns(BitIStream &bs)
{
    for (Town *t : Town::Iterate()) {
        t->cm.growing_by_chance = bs.ReadBytes(1);
        t->cm.houses_reconstructed_this_month = bs.ReadBytes(2);
        t->cm.houses_reconstructed_last_month = bs.ReadBytes(2);
        t->cm.houses_demolished_this_month = bs.ReadBytes(2);
        t->cm.houses_demolished_last_month = bs.ReadBytes(2);
        t->cm.hs_total = bs.ReadBytes(4);
        t->cm.hs_this_month = bs.ReadBytes(2);
        t->cm.hs_last_month = bs.ReadBytes(2);
        t->cm.cs_total = bs.ReadBytes(4);
        t->cm.cs_this_month = bs.ReadBytes(2);
        t->cm.cs_last_month = bs.ReadBytes(2);
        t->cm.hr_total = bs.ReadBytes(4);
        t->cm.hr_this_month = bs.ReadBytes(2);
        t->cm.hr_last_month = bs.ReadBytes(2);
    }
}

static void EncodeTownsGrowthTiles(BitOStream &bs, Game::TownsGrowthTilesIndex &tiles)
{
    bs.WriteBytes(tiles.size(), 4);
    for (Game::TownsGrowthTilesIndex::iterator p = tiles.begin();
            p != tiles.end(); p++) {
        bs.WriteBytes(p->first, 4);
        bs.WriteBytes((uint8)p->second, 1);
    }
}

static void DecodeTownsGrowthTiles(BitIStream &bs, Game::TownsGrowthTilesIndex &tiles)
{
    uint n = bs.ReadBytes(4);
    for (uint i = 0; i < n; i++) {
        TileIndex tile = bs.ReadBytes(4);
        TownGrowthTileState state = (TownGrowthTileState)bs.ReadBytes(1);
        tiles[tile] = state;
    }
}

static void EncodeCompanies(BitOStream &bs)
{
    for (const Company *c : Company::Iterate()) {
        bs.WriteBytes(c->cm.is_server, 1);
        bs.WriteBytes(c->cm.is_scored, 1);
        for (uint i = 0; i < NUM_CARGO; i++)
            bs.WriteMoney(c->cur_economy.cm.cargo_income[i]);
        for (uint j = 0; j < MAX_HISTORY_QUARTERS; j++)
            for (uint i = 0; i < NUM_CARGO; i++)
                bs.WriteMoney(c->old_economy[j].cm.cargo_income[i]);
    }
}

static void DecodeCompanies(BitIStream &bs)
{
    for (Company *c : Company::Iterate()) {
        c->cm.is_server = bs.ReadBytes(1);
        c->cm.is_scored = bs.ReadBytes(1);
        for (uint i = 0; i < NUM_CARGO; i++)
            c->cur_economy.cm.cargo_income[i] = bs.ReadMoney();
        for (uint j = 0; j < MAX_HISTORY_QUARTERS; j++)
            for (uint i = 0; i < NUM_CARGO; i++)
                c->old_economy[j].cm.cargo_income[i] = bs.ReadMoney();
    }
}

void CBController_saveload_encode(BitOStream &bs) {
    bs.WriteBytes(0 /* version */, 2);
    // Controller::saveload_encode(bs);
    bs.WriteBytes(0, 1);  // bs.WriteBytes(_settings_client.gui.cb_distance_check, 1);
    // bs.WriteBytes(0, 1);
    // bs.WriteBytes(0, 1);
    // bs.WriteBytes(0, 1);
    // bs.WriteBytes(0, 1);
    // bs.WriteBytes(0, 1);
    // bs.WriteBytes(0, 2);
    // bs.WriteBytes(0, 1);
    // bs.WriteBytes(0, 1);
    // std::vector<CargoID> cb_cargos;
    // for(CargoID cargo = 0; cargo < NUM_CARGO; cargo++) {
    //  if (CB_GetReq(cargo) > 0)
    //      cb_cargos.push_back(cargo);
    // }

 //    for (auto cargo_id : cb_cargos) {
 //        // bs.WriteBytes(req.cargo_id, 1);
 //        // bs.WriteBytes(req.amount, 4);
 //        // bs.WriteBytes(req.from, 4);
 //        // bs.WriteBytes(req.decay, 1);
 //        bs.WriteBytes(cargo_id, 1);
 //        bs.WriteBytes(CB_GetReq(cargo_id), 4);
 //        bs.WriteBytes(CB_GetFrom(cargo_id), 4);
 //        bs.WriteBytes(CB_GetDecay(cargo_id), 1);
 //    }
 //    // uint16 cb_towns = 0;
 //    // ForEachCBTown([this, &cb_towns](Town *, Company *) { cb_towns++; });
 //    bs.WriteBytes(Town::GetNumItems(), 2);
    // for (Town *t : Town::Iterate()) {
 //        auto &tcb = t->cb;
 //        bs.WriteBytes(t->index, 2);
 //        bs.WriteBytes(tcb.pax_delivered, 4);
 //        bs.WriteBytes(tcb.mail_delivered, 4);
 //        bs.WriteBytes(tcb.pax_delivered_last_month, 4);
 //        bs.WriteBytes(tcb.mail_delivered_last_month, 4);
 //        bs.WriteBytes((uint8)tcb.growth_state, 1);
 //        bs.WriteBytes(tcb.shrink_effeciency, 1);
 //        bs.WriteBytes(tcb.shrink_rate, 2);
 //        bs.WriteBytes(tcb.shrink_counter, 2);
 //        for (auto cargo_id : cb_cargos) {
 //            bs.WriteBytes(tcb.stored[cargo_id], 4);
 //            bs.WriteBytes(tcb.delivered[cargo_id], 4);
 //            bs.WriteBytes(tcb.required[cargo_id], 4);
 //            bs.WriteBytes(tcb.delivered_last_month[cargo_id], 4);
 //            bs.WriteBytes(tcb.required_last_month[cargo_id], 4);
 //        }
 //    }
}

bool CBController_saveload_decode(BitIStream &bs) {
    auto version = bs.ReadBytes(2);
    // if (version != 0)
    //     ConsoleError("Save uses incompatible CB data version {}", version);
    // ConsoleError("Server is not supposed to load CB games");
    // if (!Controller::saveload_decode(bs))
    //     return false;
    bs.ReadBytes(1);  /* _settings_game.citymania.cb.acceptance_range / _settings_client.gui.cb_distance_check */
    return true;
 //    bs.ReadBytes(1);  /* _settings_game.citymania.cb.requirements_type */
 //    bs.ReadBytes(1); /* _settings_game.citymania.cb.storage_size */
 //    bs.ReadBytes(1); /* _settings_game.citymania.cb.town_protection_range */
 //    bs.ReadBytes(2); /* _settings_game.citymania.cb.claim_max_houses */
 //    bs.ReadBytes(1); /* _settings_game.citymania.cb.smooth_growth_rate */
 //    bs.ReadBytes(1); /* _settings_game.citymania.cb.allow_negative_growth */
 //    // _settings_game.citymania.cb.requirements.clear();
 //    auto n_cargos = bs.ReadBytes(1);
    // std::vector<CargoID> cb_cargos;
 //    for (uint i = 0; i < n_cargos; i++) {
 //        auto cargo_id = (CargoID)bs.ReadBytes(1);
 //        if (cargo_id >= NUM_CARGO) {
 //            DEBUG(sl, 0, "Invalid CargoID in CB towns cargo data (%u)", cargo_id);
 //            // ConsoleError("Invalid CargoID in CB towns cargo data ({})", cargo_id);
 //            return false;
 //        }
    //  cb_cargos.push_back(cargo_id);
 //        auto required = bs.ReadBytes(4);
 //        auto from = bs.ReadBytes(4);
 //        auto decay = bs.ReadBytes(1);
    //  // CB_SetRequirements(cargo_id, required, from, decay);
 //        // _settings_game.citymania.cb.requirements.push_back(CBRequirement(cargo_id, from, required, decay, i, ""));
 //    }

 //    auto n_towns = bs.ReadBytes(2);
 //    for (uint i = 0; i < n_towns; i++) {
 //        auto town_id = bs.ReadBytes(2);
 //        auto t = Town::GetIfValid(town_id);
 //        if (!t) {
 //            DEBUG(sl, 0, "Invalid TownID in CB towns cargo data (%u)", town_id);
 //            // ConsoleError("Invalid TownID in CB towns cargo data ({})", town_id);
 //            return false;
 //        }

 //        // auto &tcb = t->cb;
 //        // tcb.pax_delivered = bs.ReadBytes(4);
 //        // tcb.mail_delivered = bs.ReadBytes(4);
 //        // tcb.pax_delivered_last_month = bs.ReadBytes(4);
 //        // tcb.mail_delivered_last_month = bs.ReadBytes(4);
 //        // tcb.growth_state = (TownGrowthState)bs.ReadBytes(1);
 //        // tcb.shrink_effeciency = bs.ReadBytes(1);
 //        // tcb.shrink_rate = bs.ReadBytes(2);
 //        // tcb.shrink_counter = bs.ReadBytes(2);
 //        // for (auto cargo_id : cb_cargos) {
 //        //     tcb.stored[cargo_id] = bs.ReadBytes(4);
 //        //     tcb.delivered[cargo_id] = bs.ReadBytes(4);
 //        //     tcb.required[cargo_id] = bs.ReadBytes(4);
 //        //     tcb.delivered_last_month[cargo_id] = bs.ReadBytes(4);
 //        //     tcb.required_last_month[cargo_id] = bs.ReadBytes(4);
 //        // }
 //    };
 //    return true;
}

void EncodeSettings(BitOStream &bs, Settings &settings) {
    bs.WriteBytes(settings.max_players_in_company, 1);
    bs.WriteBytes(settings.destroyed_houses_per_month, 2);
    bs.WriteBytes(settings.game_length_years, 2);
    bs.WriteBytes(settings.protect_funded_industries, 1);
    bs.WriteBytes(settings.same_depot_sell_years, 2);
    bs.WriteBytes(settings.economy.cashback_for_extra_land_clear, 1);
    bs.WriteBytes(settings.economy.cashback_for_bridges_and_tunnels, 1);
    bs.WriteBytes(settings.economy.cashback_for_foundations, 1);
    bs.WriteBytes(settings.limits.max_airports, 2);
    bs.WriteBytes(settings.limits.disable_canals, 1);
    bs.WriteBytes(settings.limits.min_distance_between_docks, 2);
}

void DecodeSettings(BitIStream &bs, Settings &settings) {
    settings.max_players_in_company = bs.ReadBytes(1);
    settings.destroyed_houses_per_month = bs.ReadBytes(2);
    settings.game_length_years = bs.ReadBytes(2);
    settings.protect_funded_industries = bs.ReadBytes(1);
    settings.same_depot_sell_years = bs.ReadBytes(2);
    settings.economy.cashback_for_extra_land_clear = bs.ReadBytes(1);
    settings.economy.cashback_for_bridges_and_tunnels = bs.ReadBytes(1);
    settings.economy.cashback_for_foundations = bs.ReadBytes(1);
    settings.limits.max_airports = bs.ReadBytes(2);
    settings.limits.disable_canals = bs.ReadBytes(1);
    settings.limits.min_distance_between_docks = bs.ReadBytes(2);
}

uint16 _last_client_version = 1512;

static u8vector EncodeData() {
    BitOStream bs;
    bs.Reserve(1000);
    bs.WriteBytes(SAVEGAME_DATA_FORMAT_VERSION, 2);
    bs.WriteBytes(_last_client_version, 2);
    bs.WriteBytes((uint8)_settings_game.citymania.controller_type, 1);
    bs.WriteBytes(_date, 4);  // Just in case we'll need to detect that game
    bs.WriteBytes(_date_fract, 1);  // was saved by unmodified client
    bs.WriteBytes((uint8)_settings_game.citymania.game_type, 1);
    bs.WriteBytes(0, 3);  // Reserved
    bs.WriteBytes(0, 4);  // Reserved
    EncodeSettings(bs, _settings_game.citymania);
    EncodeCompanies(bs);
    EncodeTowns(bs);
    EncodeTownsGrowthTiles(bs, _game->towns_growth_tiles);
    EncodeTownsGrowthTiles(bs, _game->towns_growth_tiles_last_month);
    if (_settings_game.citymania.controller_type == ControllerType::CB)
        CBController_saveload_encode(bs);

    return bs.GetVector();
}

static void DecodeDataV2(BitIStream &bs) {
    DecodeSettings(bs, _settings_game.citymania);
    DecodeCompanies(bs);
    DecodeTowns(bs);
    DecodeTownsGrowthTiles(bs, _game->towns_growth_tiles);
    DecodeTownsGrowthTiles(bs, _game->towns_growth_tiles_last_month);
    if (_settings_game.citymania.controller_type == ControllerType::CB) CBController_saveload_decode(bs);
}

static void DecodeTownsCargoV1(BitIStream &bs)
{
    bs.ReadBytes(1);bs.ReadBytes(1);
    // CB_SetStorage(bs.ReadBytes(1));
    // _settings_client.gui.cb_distance_check = bs.ReadBytes(1);
    uint n_cb_cargos = bs.ReadBytes(1);
    // CB_ResetRequirements();
    std::vector<CargoID> cb_cargos;
    for (uint i = 0; i < n_cb_cargos; i++) {
        uint cargo = bs.ReadBytes(1);
        if (cargo >= NUM_CARGO) {
            DEBUG(sl, 0, "Invalid CargoID in CB towns cargo data (%u)", cargo);
            return;
        }
        cb_cargos.push_back(cargo);
        uint req = bs.ReadBytes(4);
        uint from = bs.ReadBytes(4);
        uint decay = bs.ReadBytes(1);
        // CB_SetRequirements(cargo, req, from, decay);
    }
    Town *t;
    uint n_affected_towns = bs.ReadBytes(2);
    for (uint i = 0; i < n_affected_towns; i++) {
        uint town_id = bs.ReadBytes(2);
        t = Town::GetIfValid(town_id);
        if (!t) {
            DEBUG(sl, 0, "Invalid TownID in CB towns cargo data (%u)", town_id);
            return;
        }
  //       auto &tcb = t->cb;
        for (auto cargo_id : cb_cargos) {

            bs.ReadBytes(4);  // tcb.stored[cargo_id]
            bs.ReadBytes(4);  // tcb.delivered_last_month[cargo_id]
            bs.ReadBytes(4);  // tcb.delivered[cargo_id]
            bs.ReadBytes(1); /* delivered enough */

  //           tcb.required[cargo_id] = 0;
  //           tcb.required_last_month[cargo_id] = 0;
        }
    }
}

static void DecodeDataV1(BitIStream &bs) {
    if (_settings_game.citymania.controller_type != ControllerType::GENERIC) DecodeTownsCargoV1(bs);
    for (Town *t : Town::Iterate()) {
        t->cm.growing_by_chance = bs.ReadBytes(1);
        t->cm.houses_reconstructed_this_month = bs.ReadBytes(2);
        t->cm.houses_reconstructed_last_month = bs.ReadBytes(2);
        t->cm.houses_demolished_this_month = bs.ReadBytes(2);
        t->cm.houses_demolished_last_month = bs.ReadBytes(2);
    }

    Town *t;
    uint n_affected_towns = bs.ReadBytes(2);
    for (uint i = 0; i < n_affected_towns; i++) {
        uint town_id = bs.ReadBytes(2);
        t = Town::GetIfValid(town_id);
        if (!t) {
            DEBUG(sl, 0, "Invalid TownID in CB towns layout errors (%u)", town_id);
            continue;
        }
        t->cm.hs_total = bs.ReadBytes(4);
        t->cm.hs_this_month = t->cm.hs_total - bs.ReadBytes(2);
        t->cm.hs_last_month = bs.ReadBytes(2);
        t->cm.cs_total = bs.ReadBytes(4);
        t->cm.cs_this_month = t->cm.cs_total - bs.ReadBytes(2);
        t->cm.cs_last_month = bs.ReadBytes(2);
        t->cm.hr_total = bs.ReadBytes(4);
        t->cm.hr_this_month = t->cm.hr_total -bs.ReadBytes(2);
        t->cm.hr_last_month = bs.ReadBytes(2);
    }
    DecodeTownsGrowthTiles(bs, _game->towns_growth_tiles);
    DecodeTownsGrowthTiles(bs, _game->towns_growth_tiles_last_month);
}

static void DecodeData(u8vector &data) {
    if (data.size() == 0) {
        DEBUG(sl, 2, "No CityMania save data");
        return;
    }
    DEBUG(sl, 2, "CityMania save data takes %lu bytes", data.size());
    BitIStream bs(data);
    try {
        uint version = bs.ReadBytes(2);
        if (version > SAVEGAME_DATA_FORMAT_VERSION) {
            DEBUG(sl, 0, "Savegame was made with newer version of client, extra save data was not loaded");
            return;
        }
        DEBUG(sl, 2, "CityMania savegame data version %u", version);
        _last_client_version = bs.ReadBytes(2);
        _settings_game.citymania.controller_type = (ControllerType)bs.ReadBytes(1);
        if (version <= 1 && _settings_game.citymania.controller_type != ControllerType::GENERIC)
            _settings_game.citymania.controller_type = ControllerType::CLASSIC_CB;
        int32 date = bs.ReadBytes(4);
        uint32 date_fract = bs.ReadBytes(1);
        if (date != _date || date_fract != _date_fract) {
            DEBUG(sl, 0, "Savegame was run in unmodified client, extra save data "
                  "preserved, but may not be accurate");
        }
        _settings_game.citymania.game_type = (GameType)bs.ReadBytes(1);
        bs.ReadBytes(3);  // reserved
        bs.ReadBytes(4);  // reserved
        if (version == 1) DecodeDataV1(bs);
        else DecodeDataV2(bs);
    }
    catch (BitIStreamUnexpectedEnd &e) {
        DEBUG(sl, 0, "Invalid CityMania save data");
    }
}

void Save_PSAC() {
    /* Write the industries */
    for (PersistentStorage *ps : PersistentStorage::Iterate()) {
        if (ps->grfid == CITYMANIA_GRFID) {
            continue;
        }
        ps->ClearChanges();
        SlSetArrayIndex(ps->index);
        SlObject(ps, _storage_desc);
    }

    if (_game_mode == GM_EDITOR) {
        DEBUG(sl, 2, "Saving scenario, skip CityMania extra data");
        return;
    }

    uint32 grfid = CITYMANIA_GRFID;
    u8vector data = EncodeData();
    int n_chunks = (data.size() + 1023) / 1024;
    data.resize(n_chunks * 1024);
    DEBUG(sl, 2, "Citybuilder data takes %u bytes", (uint)data.size());
    uint8 *ptr = &data[0];
    SaveLoadGlobVarList _desc[] = {
        SLEG_CONDVAR(grfid, SLE_UINT32, SLV_6, SL_MAX_VERSION),
        SLEG_CONDARR(*ptr,  SLE_UINT32, 256, SLV_EXTEND_PERSISTENT_STORAGE, SL_MAX_VERSION),
        SLEG_END()
    };

    uint index = 0;
    for (PersistentStorage *ps : PersistentStorage::Iterate()) {
        if (ps->grfid != CITYMANIA_GRFID)
            index = max(index, ps->index + 1);
    }

    for (int i = 0; i < n_chunks; i++, ptr += 1024) {
        _desc[1].address = (void *)ptr;
        SlSetArrayIndex(index + i);
        SlGlobList(_desc);
    }
}

void Load_PSAC()
{
    int index;

    /*
        CITYMANIA_GRFID is used to hide extra data in persitant storages.
        To save a bit of memory we only keep at most one PS with this
        grfid and it is later discarded on save.
    */
    PersistentStorage *ps = NULL;
    u8vector cmdata;
    uint chunk_size = IsSavegameVersionBefore(SLV_EXTEND_PERSISTENT_STORAGE) ? 64 : 1024;

    while ((index = SlIterateArray()) != -1) {
        if (ps == NULL) {
            assert(PersistentStorage::CanAllocateItem());
            ps = new (index) PersistentStorage(0, 0, 0);
        }
        SlObject(ps, _storage_desc);

        if (ps->grfid == CITYMANIA_GRFID) {
            uint8 *data = (uint8 *)(ps->storage);
            cmdata.insert(cmdata.end(), data, data + chunk_size);
        } else {
            ps = NULL;
        }
    }

    DecodeData(cmdata);
}

} // namespace citymania
