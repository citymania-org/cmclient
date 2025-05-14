#include "../stdafx.h"

#include "cm_bitstream.hpp"

#include "../safeguards.h"

namespace citymania {

// Returns 8 higher bits of s-bit integer x
#define FIRST8(x, s) (((x) >> ((s) - 8)) & 0xFF)

void BitOStream::WriteBytes(uint32_t value, int amount)
{
	do {
		v = (v << 8) | FIRST8(value, amount << 3);
		f.push_back(FIRST8(v, c + 8));
	} while (--amount);
}

void BitOStream::WriteBytes64(uint64_t value, int amount)
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

uint32_t BitIStream::ReadBytes(uint amount)
{
	uint32_t res = 0;
	while (amount--) {
		if (i >= f.size()) {
			throw _bit_i_stream_unexpected_end;
		}
		res = (res << 8) | f[i++];
	}
	return res;
}

uint64_t BitIStream::ReadBytes64(uint amount)
{
	uint64_t res = 0;
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

std::vector<uint8_t> BitIStream::ReadData() {
	auto len = this->ReadBytes(2);
	std::vector<uint8_t> res;
	for (uint i = 0; i < len; i++) res.push_back(this->ReadBytes(1));
	return res;
}

} // namespace citymania
