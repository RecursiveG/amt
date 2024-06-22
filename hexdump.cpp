#include "hexdump.h"

#include <cinttypes>
#include <sstream>
#include <string>

namespace {

using std::string;
const char *HEX = "0123456789ABCDEF";

// clang-format off
string to_hex_str(size_t number) {
  char buf[9];
  buf[8] = 0;
  buf[7] = *(HEX+(number%16)); number /= 16;
  buf[6] = *(HEX+(number%16)); number /= 16;
  buf[5] = *(HEX+(number%16)); number /= 16;
  buf[4] = *(HEX+(number%16)); number /= 16;
  buf[3] = *(HEX+(number%16)); number /= 16;
  buf[2] = *(HEX+(number%16)); number /= 16;
  buf[1] = *(HEX+(number%16)); number /= 16;
  buf[0] = *(HEX+(number%16));
  return string(buf);
}
string to_hex_byte(uint8_t number) {
  char buf[3];
  buf[2] = 0;
  buf[1] = *(HEX+(number%16)); number /= 16;
  buf[0] = *(HEX+(number%16));
  return string(buf);
}
// clang-format on

void hexdump(const void *data_in, size_t len, std::ostream &printer,
             bool print_header = true) {
  const char *data = static_cast<const char *>(data_in);

  if (len <= 0) {
    printer << "hexdump: empty string" << std::endl;
    return;
  }
  if (print_header) {
    printer << "          +0 +1 +2 +3 +4 +5 +6 +7  +8 +9 +A +B +C +D +E +F" << std::endl;
  }

  size_t pos = 0;
  while (pos < len) {
    size_t data_len = len - pos;
    if (data_len > 16)
      data_len = 16;
    printer << to_hex_str(pos) << "  ";
    for (size_t offset = 0; offset < 16; offset++) {
      if (offset >= data_len) {
        printer << "   ";
      } else {
        printer << to_hex_byte((uint8_t)data[pos + offset]) << ' ';
      }
      if (offset == 7)
        printer << " ";
    }
    printer << " |";
    for (size_t offset = 0; offset < 16; offset++) {
      if (offset >= data_len) {
        printer << " ";
      } else {
        char ch = data[pos + offset];
        if (isgraph(ch)) {
          printer << ch;
        } else {
          printer << ".";
        }
      }
    }
    printer << "|";
    pos += 16;
    if (pos < len)
      printer << std::endl;
  }
}

} // namespace

string Hexdump(const void *data, size_t len) {
  std::stringstream ss;
  hexdump(data, len, ss, true);
  return ss.str();
}

std::string HexString(const void *data, size_t size) {
  std::string ret;
  for (size_t i = 0; i < size; i++) {
    ret += to_hex_byte(static_cast<const uint8_t *>(data)[i]);
  }
  return ret;
}
