#include "../stdafx.h"

#include "../date_func.h"
#include "../debug.h"
#include "../saveload/saveload.h"
#include "../town.h"

#include "cm_bitstream.hpp"
#include "cm_saveload.hpp"

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


static void EncodeTownsExtraInfo(BitOStream &bs)
{
	uint n_affected_towns = 0;
	for (const Town *t : Town::Iterate()) {
		if (t->growing_by_chance || t->houses_reconstruction ||
				t->houses_demolished)
			n_affected_towns++;
	}
	bs.WriteBytes(n_affected_towns, 2);
	for (const Town *t : Town::Iterate()) {
		if (t->growing_by_chance || t->houses_reconstruction ||
				t->houses_demolished) {
			bs.WriteBytes(t->index, 2);
			bs.WriteBytes(t->growing_by_chance, 1);
			bs.WriteBytes(t->houses_reconstruction, 2);
			bs.WriteBytes(t->houses_demolished, 2);
		}
	}
}

static void EncodeTownsGrowthTiles(BitOStream &bs, TownsGrowthTilesIndex &tiles)
{
	bs.WriteBytes(tiles.size(), 4);
	for (TownsGrowthTilesIndex::iterator p = tiles.begin();
			p != tiles.end(); p++) {
		bs.WriteBytes(p->first, 4);
		bs.WriteBytes(p->second, 1);
	}
}

static void EncodeTownsLayoutErrors(BitOStream &bs)
{
	uint n_affected_towns = 0;
	for (const Town *t : Town::Iterate()) {
		if (t->cb_houses_removed || t->houses_skipped || t->cycles_skipped)
			n_affected_towns++;
	}
	bs.WriteBytes(n_affected_towns, 2);
	for (const Town *t : Town::Iterate()) {
		if (t->cb_houses_removed || t->houses_skipped || t->cycles_skipped) {
			bs.WriteBytes(t->index, 2);
			bs.WriteBytes(t->houses_skipped, 2);
			bs.WriteBytes(t->houses_skipped_prev, 2);
			bs.WriteBytes(t->houses_skipped_last_month, 2);
			bs.WriteBytes(t->cycles_skipped, 2);
			bs.WriteBytes(t->cycles_skipped_prev, 2);
			bs.WriteBytes(t->cycles_skipped_last_month, 2);
			bs.WriteBytes(t->cb_houses_removed, 2);
			bs.WriteBytes(t->cb_houses_removed_prev, 2);
			bs.WriteBytes(t->cb_houses_removed_last_month, 2);
		}
	}
	EncodeTownsGrowthTiles(bs, _towns_growth_tiles);
	EncodeTownsGrowthTiles(bs, _towns_growth_tiles_last_month);
}

static void DecodeTownsExtraInfo(BitIStream &bs)
{
	Town *t;
	uint n_affected_towns = bs.ReadBytes(2);
	for (uint i = 0; i < n_affected_towns; i++) {
		uint town_id = bs.ReadBytes(2);
		t = Town::Get(town_id);
		if (!t) {
			DEBUG(sl, 0, "Invalid TownID in CM extra towns info (%u)", town_id);
			continue;
		}
		t->growing_by_chance = bs.ReadBytes(1);
		t->houses_reconstruction = bs.ReadBytes(2);
		t->houses_demolished = bs.ReadBytes(2);
	}
}

static void DecodeTownsGrowthTiles(BitIStream &bs, TownsGrowthTilesIndex &tiles)
{
	uint n = bs.ReadBytes(4);
	for (uint i = 0; i < n; i++) {
		TileIndex tile = bs.ReadBytes(4);
		TownGrowthTileState state = (TownGrowthTileState)bs.ReadBytes(1);
		tiles[tile] = state;
	}
}

static void DecodeTownsLayoutErrors(BitIStream &bs)
{
	Town *t;
	uint n_affected_towns = bs.ReadBytes(2);
	for (uint i = 0; i < n_affected_towns; i++) {
		uint town_id = bs.ReadBytes(2);
		t = Town::Get(town_id);
		if (!t) {
			DEBUG(sl, 0, "Invalid TownID in CB towns layout errors (%u)", town_id);
			continue;
		}
		t->houses_skipped = bs.ReadBytes(2);
		t->houses_skipped_prev = bs.ReadBytes(2);
		t->houses_skipped_last_month = bs.ReadBytes(2);
		t->cycles_skipped = bs.ReadBytes(2);
		t->cycles_skipped_prev = bs.ReadBytes(2);
		t->cycles_skipped_last_month = bs.ReadBytes(2);
		t->cb_houses_removed = bs.ReadBytes(2);
		t->cb_houses_removed_prev = bs.ReadBytes(2);
		t->cb_houses_removed_last_month = bs.ReadBytes(2);
	}
	DecodeTownsGrowthTiles(bs, _towns_growth_tiles);
	DecodeTownsGrowthTiles(bs, _towns_growth_tiles_last_month);
}

void CBController_saveload_encode(BitOStream &bs) {
    bs.WriteBytes(0 /* version */, 2);
    // Controller::saveload_encode(bs);
    bs.WriteBytes(0, 1);
    bs.WriteBytes(_settings_client.gui.cb_distance_check, 1);
    bs.WriteBytes(0, 1);
    bs.WriteBytes(0, 1);
    bs.WriteBytes(0, 1);
    bs.WriteBytes(0, 1);
    bs.WriteBytes(0, 1);
    bs.WriteBytes(0, 1);
    std::vector<CargoID> cb_cargos;
	for(CargoID cargo = 0; cargo < NUM_CARGO; cargo++) {
		if (CB_GetReq(cargo) > 0)
			cb_cargos.push_back(cargo);
	}

    for (auto cargo_id : cb_cargos) {
        // bs.WriteBytes(req.cargo_id, 1);
        // bs.WriteBytes(req.amount, 4);
        // bs.WriteBytes(req.from, 4);
        // bs.WriteBytes(req.decay, 1);
        bs.WriteBytes(cargo_id, 1);
        bs.WriteBytes(CB_GetReq(cargo_id), 4);
        bs.WriteBytes(CB_GetFrom(cargo_id), 4);
        bs.WriteBytes(CB_GetDecay(cargo_id), 1);
    }
    // uint16 cb_towns = 0;
    // ForEachCBTown([this, &cb_towns](Town *, Company *) { cb_towns++; });
    bs.WriteBytes(Town::GetNumItems(), 2);
	for (Town *t : Town::Iterate()) {
        auto &tcb = t->cb;
        bs.WriteBytes(t->index, 2);
        bs.WriteBytes(tcb.pax_delivered, 4);
        bs.WriteBytes(tcb.mail_delivered, 4);
        bs.WriteBytes(tcb.pax_delivered_last_month, 4);
        bs.WriteBytes(tcb.mail_delivered_last_month, 4);
        bs.WriteBytes((uint8)tcb.growth_state, 1);
        bs.WriteBytes(tcb.shrink_effeciency, 1);
        bs.WriteBytes(tcb.shrink_rate, 2);
        bs.WriteBytes(tcb.shrink_counter, 2);
        for (auto cargo_id : cb_cargos) {
            bs.WriteBytes(tcb.stored[cargo_id], 4);
            bs.WriteBytes(tcb.delivered[cargo_id], 4);
            bs.WriteBytes(tcb.required[cargo_id], 4);
            bs.WriteBytes(tcb.delivered_last_month[cargo_id], 4);
            bs.WriteBytes(tcb.required_last_month[cargo_id], 4);
        }
    }
}

bool CBController_saveload_decode(BitIStream &bs) {
    auto version = bs.ReadBytes(2);
    // if (version != 0)
    //     ConsoleError("Save uses incompatible CB data version {}", version);
    // ConsoleError("Server is not supposed to load CB games");
    // if (!Controller::saveload_decode(bs))
    //     return false;
    bs.ReadBytes(1);  /* _settings_game.citymania.cb.requirements_type */
    _settings_client.gui.cb_distance_check = bs.ReadBytes(1); /* _settings_game.citymania.cb.acceptance_range */
    bs.ReadBytes(1); /* _settings_game.citymania.cb.storage_size */
    bs.ReadBytes(1); /* _settings_game.citymania.cb.town_protection_range */
    bs.ReadBytes(2); /* _settings_game.citymania.cb.claim_max_houses */
    bs.ReadBytes(1); /* _settings_game.citymania.cb.smooth_growth_rate */
    bs.ReadBytes(1); /* _settings_game.citymania.cb.allow_negative_growth */
    // _settings_game.citymania.cb.requirements.clear();
    auto n_cargos = bs.ReadBytes(1);
	std::vector<CargoID> cb_cargos;
    for (uint i = 0; i < n_cargos; i++) {
        auto cargo_id = (CargoID)bs.ReadBytes(1);
        if (cargo_id >= NUM_CARGO) {
            DEBUG(sl, 0, "Invalid CargoID in CB towns cargo data (%u)", cargo_id);
            // ConsoleError("Invalid CargoID in CB towns cargo data ({})", cargo_id);
            return false;
        }
		cb_cargos.push_back(cargo_id);
        auto required = bs.ReadBytes(4);
        auto from = bs.ReadBytes(4);
        auto decay = bs.ReadBytes(1);
		CB_SetRequirements(cargo_id, required, from, decay);
        // _settings_game.citymania.cb.requirements.push_back(CBRequirement(cargo_id, from, required, decay, i, ""));
    }

    auto n_towns = bs.ReadBytes(2);
    for (uint i = 0; i < n_towns; i++) {
        auto town_id = bs.ReadBytes(2);
        auto t = Town::GetIfValid(town_id);
        if (!t) {
            DEBUG(sl, 0, "Invalid TownID in CB towns cargo data (%u)", town_id);
            // ConsoleError("Invalid TownID in CB towns cargo data ({})", town_id);
            return false;
        }

        auto &tcb = t->cb;
        tcb.pax_delivered = bs.ReadBytes(4);
        tcb.mail_delivered = bs.ReadBytes(4);
        tcb.pax_delivered_last_month = bs.ReadBytes(4);
        tcb.mail_delivered_last_month = bs.ReadBytes(4);
        tcb.growth_state = (TownGrowthState)bs.ReadBytes(1);
        tcb.shrink_effeciency = bs.ReadBytes(1);
        tcb.shrink_rate = bs.ReadBytes(2);
        tcb.shrink_counter = bs.ReadBytes(2);
        for (auto cargo_id : cb_cargos) {
            tcb.stored[cargo_id] = bs.ReadBytes(4);
            tcb.delivered[cargo_id] = bs.ReadBytes(4);
            tcb.required[cargo_id] = bs.ReadBytes(4);
            tcb.delivered_last_month[cargo_id] = bs.ReadBytes(4);
            tcb.required_last_month[cargo_id] = bs.ReadBytes(4);
        }
    };
    return true;
}

uint8 _controller_type = 0;
uint8 _game_type = 0;
uint16 _last_client_version = 1512;

static u8vector EncodeData() {
	BitOStream bs;
	bs.Reserve(1000);
	bs.WriteBytes(SAVEGAME_DATA_FORMAT_VERSION, 2);
	bs.WriteBytes(_last_client_version, 2);
	bs.WriteBytes(_controller_type, 1);
	bs.WriteBytes(_date, 4);  // Just in case we'll need to detect that game
	bs.WriteBytes(_date_fract, 1);  // was saved by unmodified client
	bs.WriteBytes(_game_type, 1);
	bs.WriteBytes(0, 3);  // Reserved
	bs.WriteBytes(0, 4);  // Reserved

	EncodeTownsExtraInfo(bs);
	EncodeTownsLayoutErrors(bs);
	if (_controller_type == 4)
		CBController_saveload_encode(bs);

	return bs.GetVector();
}

static void DecodeTownsCargoV1(BitIStream &bs)
{
	CB_SetStorage(bs.ReadBytes(1));
	_settings_client.gui.cb_distance_check = bs.ReadBytes(1);
	uint n_cb_cargos = bs.ReadBytes(1);
	CB_ResetRequirements();
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
		CB_SetRequirements(cargo, req, from, decay);
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
        auto &tcb = t->cb;
		for (auto cargo_id : cb_cargos) {
            tcb.stored[cargo_id] = bs.ReadBytes(4);
            tcb.delivered_last_month[cargo_id] = bs.ReadBytes(4);
            tcb.delivered[cargo_id] = bs.ReadBytes(4);
            bs.ReadBytes(1); /* delivered enough */

            tcb.required[cargo_id] = 0;
            tcb.required_last_month[cargo_id] = 0;
		}
	}
}

static void DecodeData(u8vector &data) {
	ResetTownsGrowthTiles();
	if (data.size() == 0) {
		DEBUG(sl, 2, "No CityMania save data");
		return;
	} else {
		DEBUG(sl, 2, "Ignoring CityMania data in savegame");
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
		_last_client_version = bs.ReadBytes(2);
		_controller_type = bs.ReadBytes(1);
		if (version <= 1) _controller_type = (_controller_type ? 4 : 0);
		int32 date = bs.ReadBytes(4);
		uint32 date_fract = bs.ReadBytes(1);
		if (date != _date || date_fract != _date_fract) {
			DEBUG(sl, 0, "Savegame was run in unmodified client, extra save data "
			      "preserved, but may not be accurate");
		}
		_game_type = bs.ReadBytes(1);
		bs.ReadBytes(3);  // reserved
		bs.ReadBytes(4);  // reserved
		if (version <= 1 && _controller_type != 0) DecodeTownsCargoV1(bs);
		DecodeTownsExtraInfo(bs);
		DecodeTownsLayoutErrors(bs);
		if (version > 1 && _controller_type == 4) CBController_saveload_decode(bs);
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
