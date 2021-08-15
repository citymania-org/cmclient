#ifndef CITYMANIA_SAVELOAD_HPP
#define CITYMANIA_SAVELOAD_HPP

#include "../saveload/saveload.h"

namespace citymania {

struct PSACChunkHandler : ChunkHandler {
    PSACChunkHandler() : ChunkHandler('PSAC', CH_TABLE) {}

    void Load() const override;
    void Save() const override;
};

} // namespace citymania

#endif
