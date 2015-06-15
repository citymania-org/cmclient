#include "citymania_sl.h"
#include "../bitstream.h"
#include "../date_func.h"
#include "../debug.h"
#include "../town.h"

#define CM_DATA_FORMAT_VERSION 1

assert_compile(NUM_CARGO == 32);


static void CM_EncodeTownsExtraInfo(BitOStream &bs)
{
	Town *t;
	uint n_affected_towns = 0;
	FOR_ALL_TOWNS(t) {
		if (t->growing_by_chance || t->houses_reconstruction ||
				t->houses_demolished)
			n_affected_towns++;
	}
	bs.WriteBytes(n_affected_towns, 2);
	FOR_ALL_TOWNS(t) {
		if (t->growing_by_chance || t->houses_reconstruction ||
				t->houses_demolished) {
			bs.WriteBytes(t->index, 2);
			bs.WriteBytes(t->growing_by_chance, 1);
			bs.WriteBytes(t->houses_reconstruction, 2);
			bs.WriteBytes(t->houses_demolished, 2);
		}
	}
}

static void CM_EncodeTownsGrowthTiles(BitOStream &bs, TownsGrowthTilesIndex &tiles)
{
	bs.WriteBytes(tiles.size(), 4);
	for (TownsGrowthTilesIndex::iterator p = tiles.begin();
			p != tiles.end(); p++) {
		bs.WriteBytes(p->first, 4);
		bs.WriteBytes(p->second, 1);
	}
}

static void CM_EncodeTownsLayoutErrors(BitOStream &bs)
{
	Town *t;
	uint n_affected_towns = 0;
	FOR_ALL_TOWNS(t) {
		if (t->cb_houses_removed || t->houses_skipped || t->cycles_skipped)
			n_affected_towns++;
	}
	bs.WriteBytes(n_affected_towns, 2);
	FOR_ALL_TOWNS(t) {
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
	CM_EncodeTownsGrowthTiles(bs, _towns_growth_tiles);
	CM_EncodeTownsGrowthTiles(bs, _towns_growth_tiles_last_month);
}

static void CM_EncodeTownsCargo(BitOStream &bs)
{
	uint n_cb_cargos = 0;
	CargoID cb_cargos[NUM_CARGO];
	for(CargoID cargo = 0; cargo < NUM_CARGO; cargo++) {
		if (CB_GetReq(cargo) > 0)
			cb_cargos[n_cb_cargos++] = cargo;
	}

	bs.WriteBytes(CB_GetStorage(), 1);
	bs.WriteBytes(_settings_client.gui.cb_distance_check, 1);
	bs.WriteBytes(n_cb_cargos, 1);
	for (uint i = 0; i < n_cb_cargos; i++) {
		CargoID cargo = cb_cargos[i];
		bs.WriteBytes(cargo, 1);
		bs.WriteBytes(CB_GetReq(cargo), 4);
		bs.WriteBytes(CB_GetFrom(cargo), 4);
		bs.WriteBytes(CB_GetDecay(cargo), 1);
	}

	Town *t;
	bs.WriteBytes(Town::GetNumItems(), 2);
	FOR_ALL_TOWNS(t) {
		bs.WriteBytes(t->index, 2);
		for (uint i = 0; i < n_cb_cargos; i++) {
			CargoID cargo = cb_cargos[i];
			bs.WriteBytes(t->storage[cargo], 4);
			bs.WriteBytes(t->act_cargo[cargo], 4);
			bs.WriteBytes(t->new_act_cargo[cargo], 4);
			bs.WriteBytes(t->delivered_enough[cargo], 1);
		}
	}
}

static void CM_DecodeTownsExtraInfo(BitIStream &bs)
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

static void CM_DecodeTownsGrowthTiles(BitIStream &bs, TownsGrowthTilesIndex &tiles)
{
	uint n = bs.ReadBytes(4);
	for (uint i = 0; i < n; i++) {
		TileIndex tile = bs.ReadBytes(4);
		TownGrowthTileState state = (TownGrowthTileState)bs.ReadBytes(1);
		tiles[tile] = state;
	}
}

static void CM_DecodeTownsLayoutErrors(BitIStream &bs)
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
	CM_DecodeTownsGrowthTiles(bs, _towns_growth_tiles);
	CM_DecodeTownsGrowthTiles(bs, _towns_growth_tiles_last_month);
}

static void CM_DecodeTownsCargo(BitIStream &bs)
{
	CB_SetStorage(bs.ReadBytes(1));
	_settings_client.gui.cb_distance_check = bs.ReadBytes(1);
	uint n_cb_cargos = bs.ReadBytes(1);
	CB_ResetRequirements();
	CargoID cb_cargos[NUM_CARGO];
	for (uint i = 0; i < n_cb_cargos; i++) {
		uint cargo = bs.ReadBytes(1);
		if (cargo >= NUM_CARGO) {
			DEBUG(sl, 0, "Invalid CargoID in CB towns cargo data (%u)", cargo);
			return;
		}
		cb_cargos[i] = cargo;
		uint req = bs.ReadBytes(4);
		uint from = bs.ReadBytes(4);
		uint decay = bs.ReadBytes(1);
		CB_SetRequirements(cargo, req, from, decay);
	}
	Town *t;
	uint n_affected_towns = bs.ReadBytes(2);
	for (uint i = 0; i < n_affected_towns; i++) {
		uint town_id = bs.ReadBytes(2);
		t = Town::Get(town_id);
		if (!t) {
			DEBUG(sl, 0, "Invalid TownID in CB towns cargo data (%u)", town_id);
			continue;
		}
		for (uint i = 0; i < n_cb_cargos; i++) {
			CargoID cargo = cb_cargos[i];
			t->storage[cargo] = bs.ReadBytes(4);
			t->act_cargo[cargo] = bs.ReadBytes(4);
			t->new_act_cargo[cargo] = bs.ReadBytes(4);
			t->delivered_enough[cargo] = bs.ReadBytes(1);
		}
	}
}

u8vector CM_EncodeData()
{
	BitOStream bs;
	bs.Reserve(1000);
	bs.WriteBytes(CM_DATA_FORMAT_VERSION, 2);
	bs.WriteBytes(1512, 2);  // TODO client version
	bs.WriteBytes(CB_Enabled(), 1);
	bs.WriteBytes(_date, 4);  // Just in case we'll need to detect that game
	bs.WriteBytes(_date_fract, 1);  // was saved by unmodified client
	bs.WriteBytes(0, 4);  // Reserved
	bs.WriteBytes(0, 4);  // Reserved

	CM_EncodeTownsCargo(bs);
	CM_EncodeTownsExtraInfo(bs);
	CM_EncodeTownsLayoutErrors(bs);

	// fill with 0 to fit in persistent storage chunks (16 * 4)
	int mod = bs.GetByteSize() % 64;
	if (mod) {
		for (int i = 0; i < 64 - mod; i++)
			bs.WriteBytes(0, 1);
	}
	DEBUG(sl, 2, "Citybuilder data takes %u bytes", bs.GetByteSize());
	return bs.GetVector();
}

void CM_DecodeData(u8vector &data)
{
	ResetTownsGrowthTiles();
	extern bool _novahost;
	if (data.size() == 0) {
		_novahost = false;
		CB_SetCB(false);
		DEBUG(sl, 2, "No citybuilder data");
		return;
	}
	DEBUG(sl, 2, "Citybuilder data takes %lu bytes", data.size());
	_novahost = true;
	BitIStream bs(data);
	try {
		uint version = bs.ReadBytes(2);
		if (version != CM_DATA_FORMAT_VERSION) {
			DEBUG(sl, 0, "Savegame was made with different version of client, extra citybuilder data was not loaded");
			return;
		}
		bs.ReadBytes(2);  // client version
		bool is_cb = bs.ReadBytes(1);
		CB_SetCB(is_cb);
		int32 date = bs.ReadBytes(4);  // date
		uint32 date_fract = bs.ReadBytes(1);  // date_fract
		if (date != _date || date_fract != _date_fract) {
			DEBUG(sl, 0, "Savegame was run in unmodified client, extra cb data "
			      "preserved, but may not be accurate");
		}
		bs.ReadBytes(4);  // reserved
		bs.ReadBytes(4);  // reserved
		if (is_cb) {
			CM_DecodeTownsCargo(bs);
		}
		CM_DecodeTownsExtraInfo(bs);
		CM_DecodeTownsLayoutErrors(bs);
	}
	catch (BitIStreamUnexpectedEnd &e) {
		DEBUG(sl, 0, "Invalid citybuilder data");
	}
}