#ifndef __HEXDUMP_H__
#define __HEXDUMP_H__

#include <cinttypes>
#include <string>

// Output format:
//           +0 +1 +2 +3 +4 +5 +6 +7  +8 +9 +A +B +C  D  E  F
// 00000000  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  |0123456789ABCDEF|
std::string Hexdump(const void *data, size_t len);

// 0102030a0b0f
std::string HexString(const void *data, size_t size);

// 00000000-0000-0000-0000-000000000000
// size must be 16
std::string HexUuid(const void *data, size_t size);

#endif // __HEXDUMP_H__
