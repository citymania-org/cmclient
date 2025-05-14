#ifndef BITSTREAM_H
#define BITSTREAM_H

#include "../economy_type.h"

#include <vector>

namespace citymania {

typedef std::vector<uint8_t> u8vector;

class BitOStream {
protected:
	u8vector f;
	uint32_t c;
	uint32_t v;

public:
	BitOStream(): c(0), v(0) {}
	virtual ~BitOStream(){}
	void Reserve(int bytes);
	void WriteBytes(uint32_t value, int amount);
	void WriteBytes64(uint64_t value, int amount);
	void WriteMoney(Money value);
	const u8vector &GetVector();
	uint GetByteSize() const;
};

class BitIStreamUnexpectedEnd: public std::exception {
	const char* what() const noexcept override {
		return "Unexpected end of bit input stream";
	}
};

class BitIStream {
protected:
	u8vector &f;
	uint32_t i;
public:
	BitIStream(u8vector &data): f(data), i(0) {}
	virtual ~BitIStream(){}
	uint32_t ReadBytes(uint amount);
	uint64_t ReadBytes64(uint amount);
	Money ReadMoney();
	std::vector<uint8_t> ReadData();
	bool eof() {
		return this->i >= f.size();
	}
};

} // namespace citymania

#endif
