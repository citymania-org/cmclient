#ifndef CITYMANIA_SAVELOAD_HPP
#define CITYMANIA_SAVELOAD_HPP

#include "../saveload/saveload.h"
#include "../town.h"

#define CM_SLE_GENERAL(name, cmd, base, variable, type, length, from, to, extra) \
    SaveLoad {name, cmd, type, length, from, to, [] (void *b, size_t) -> void * { \
        static_assert(SlCheckVarSize(cmd, type, length, sizeof(static_cast<base *>(b)->variable))); \
        assert(b != nullptr); \
        return const_cast<void *>(static_cast<const void *>(std::addressof(static_cast<base *>(b)->variable))); \
    }, extra, nullptr}
#define CM_SLE_VAR(name, base, variable, type) CM_SLE_GENERAL(name, SL_VAR, base, variable, type, 0, SLV_TABLE_CHUNKS, SL_MAX_VERSION, 0)
#define CM_SLE_VAR(name, base, variable, type) CM_SLE_GENERAL(name, SL_VAR, base, variable, type, 0, SLV_TABLE_CHUNKS, SL_MAX_VERSION, 0)
#define CM_SLE_ARR(name, base, variable, type, length) CM_SLE_GENERAL(name, SL_ARR, base, variable, type, length, SLV_TABLE_CHUNKS, SL_MAX_VERSION, 0)


namespace citymania {

class SlTownGrowthTiles : public DefaultSaveLoadHandler<SlTownGrowthTiles, Town> {
public:
    inline static const SaveLoad description[] = {
        CM_SLE_VAR("tile", TownsGrowthTilesIndex::value_type, first, SLE_UINT32),
        CM_SLE_VAR("state", TownsGrowthTilesIndex::value_type, second, SLE_UINT8),
    };
    inline const static SaveLoadCompatTable compat_description;

    void Save(Town *t) const override;
    void Load(Town *t) const override;
};

class SlTownGrowthTilesLastMonth : public DefaultSaveLoadHandler<SlTownGrowthTilesLastMonth, Town> {
public:
    inline static const SaveLoad description[] = {
        CM_SLE_VAR("tile", TownsGrowthTilesIndex::value_type, first, SLE_UINT32),
        CM_SLE_VAR("state", TownsGrowthTilesIndex::value_type, second, SLE_UINT8),
    };
    inline const static SaveLoadCompatTable compat_description;

    void Save(Town *t) const override;
    void Load(Town *t) const override;
};

}  // namespace citymania
#endif
