#include "../stdafx.h"

#include "cm_bitstream.hpp"

#include "../safeguards.h"

namespace citymania {

// Returns 8 higher bits of s-bit integer x
#define FIRST8(x, s) (((x) >> ((s) - 8)) & 0xFF)

void BitOStream::WriteBytes(uint32 value, int amount)
{
	do {
		v = (v << 8) | FIRST8(value, amount << 3);
		f.push_back(FIRST8(v, c + 8));
	} while (--amount);
}

void BitOStream::WriteBytes64(uint64 value, int amount)
{
	do {
		v = (v << 8) | FIRST8(value, amount << 3);
		f.push_back(FIRST8(v, c + 8));
	} while (--amount);
}

void BitOStream::WriteMoney(Money value) {
	this->WriteBytes64(value, 8);
}


/**
 * Reserves memory for encoded data. Use to avoid reallocation of vector.
 */
void BitOStream::Reserve(int bytes)
{
	f.reserve(bytes);
}

/**
 * Returns encoded data
 */
const u8vector &BitOStream::GetVector()
{
	return f;
}

/**
 * Size of encoded data in bytes
 */
uint BitOStream::GetByteSize() const
{
	return f.size() + (c ? 1 : 0);
}

BitIStreamUnexpectedEnd _bit_i_stream_unexpected_end;

uint32 BitIStream::ReadBytes(uint amount)
{
	uint32 res = 0;
	while (amount--) {
		if (i >= f.size()) {
			throw _bit_i_stream_unexpected_end;
		}
		res = (res << 8) | f[i++];
	}
	return res;
}

uint64 BitIStream::ReadBytes64(uint amount)
{
	uint64 res = 0;
	while (amount--) {
		if (i >= f.size()) {
			throw _bit_i_stream_unexpected_end;
		}
		res = (res << 8) | f[i++];
	}
	return res;
}

Money BitIStream::ReadMoney() {
	return (Money)this->ReadBytes64(8);
}

} // namespace citymania
