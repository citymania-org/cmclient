#ifndef SAVELOAD_CITYMANIA_SL_H
#define SAVELOAD_CITYMANIA_SL_H

#include "../bitstream.h"

u8vector CM_EncodeData();
void CM_DecodeData(u8vector &data);

#endif
