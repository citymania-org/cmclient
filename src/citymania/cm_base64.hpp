#ifndef CM_BASE64_HPP
#define CM_BASE64_HPP

#include <string>

std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);

#endif //BASE_64_H

