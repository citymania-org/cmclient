/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file storage_sl.cpp Code handling saving and loading of persistent storages. */

#include "../stdafx.h"
#include "../newgrf_storage.h"
#include "citymania_sl.h"
#include "saveload.h"

#include "../safeguards.h"

// Luukland_Citybuilder grf id actually
#define CITYMANIA_GRFID 0x534B0501U

/** Description of the data to save and load in #PersistentStorage. */
static const SaveLoad _storage_desc[] = {
	 SLE_CONDVAR(PersistentStorage, grfid,    SLE_UINT32,                  SLV_6, SL_MAX_VERSION),
	 SLE_CONDARR(PersistentStorage, storage,  SLE_UINT32,  16,           SLV_161, SLV_EXTEND_PERSISTENT_STORAGE),
	 SLE_CONDARR(PersistentStorage, storage,  SLE_UINT32, 256,           SLV_EXTEND_PERSISTENT_STORAGE, SL_MAX_VERSION),
	 SLE_END()
};

// static void hexdump(uint8 *data) {
// 	uint i;
// 	for (i = 0; i < 20; i++) {
// 		if (i) fprintf(stderr, " : ");
// 		fprintf(stderr, "%02x", data[i]);
// 	}
// 	fprintf(stderr, i >= 20 ? " ...\n" : "\n");
// }

/** Load persistent storage data. */
static void Load_PSAC()
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
	fprintf(stderr, "CHUNK SIZE %u\n", chunk_size);
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

	CM_DecodeData(cmdata);
}

static void Save_CMDataAsPSAC() {
	uint32 grfid = CITYMANIA_GRFID;
	u8vector data = CM_EncodeData();
	uint8 *ptr = &data[0];
	SaveLoadGlobVarList _desc[] = {
		SLEG_CONDVAR(grfid, SLE_UINT32, SLV_6, SL_MAX_VERSION),
		SLEG_CONDARR(*ptr,  SLE_UINT32, 256, SLV_EXTEND_PERSISTENT_STORAGE, SL_MAX_VERSION),
		SLEG_END()
	};

    uint index = 0;
    PersistentStorage *ps;
    FOR_ALL_STORAGES(ps) {
        if (ps->grfid != CITYMANIA_GRFID)
            index = max(index, ps->index + 1);
    }

	int n_chunks = data.size() / 1024;
	for (int i = 0; i < n_chunks; i++, ptr += 1024) {
		_desc[1].address = (void *)ptr;
		SlSetArrayIndex(index + i);
		SlGlobList(_desc);
	}
}

/** Save persistent storage data. */
static void Save_PSAC()
{
	PersistentStorage *ps;
	/* Write the industries */
	FOR_ALL_STORAGES(ps) {
		if (ps->grfid == CITYMANIA_GRFID) {
			continue;
		}
		ps->ClearChanges();
		SlSetArrayIndex(ps->index);
		SlObject(ps, _storage_desc);
	}

	Save_CMDataAsPSAC();
}

/** Chunk handler for persistent storages. */
extern const ChunkHandler _persistent_storage_chunk_handlers[] = {
	{ 'PSAC', Save_PSAC, Load_PSAC, NULL, NULL, CH_ARRAY | CH_LAST},
};
