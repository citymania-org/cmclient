#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <vector>

namespace citymania {

typedef std::vector<uint8> u8vector;

class BitOStream {
protected:
	u8vector f;
	uint32 c;
	uint32 v;

public:
	BitOStream(): c(0), v(0) {}
	virtual ~BitOStream(){}
	void Reserve(int bytes);
	void WriteBytes(uint32 value, int amount);
	void WriteBytes64(uint64 value, int amount);
	const u8vector &GetVector();
	uint GetByteSize() const;
};

class BitIStreamUnexpectedEnd: public std::exception {
	virtual const char* what() const throw() {
		return "Unexpected end of bit input stream";
	}
};

class BitIStream {
protected:
	u8vector &f;
	uint32 i;
public:
	BitIStream(u8vector &data): f(data), i(0) {}
	virtual ~BitIStream(){}
	uint32 ReadBytes(uint amount);
};

} // namespace citymania

#endif
