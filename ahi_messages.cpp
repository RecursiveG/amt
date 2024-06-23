#include "ahi_messages.h"
#include "die.h"
#include "mem_extract.h"

namespace amt {

bool AhiGetLocalSystemAccountResponse::Deserialize(absl::Span<uint8_t> data) {
  if (data.size() != 16 + 66 + 2) {
    return false;
  }

  Extract(&header, data.subspan(0, 12));
  Extract(&amt_status, data.subspan(12, 4));

  // 33B username + 33B passwd + 2B padding?
  const char *username_data = reinterpret_cast<const char *>(data.data() + 16);
  const char *password_data = username_data + 33;

  uint32_t username_len = strnlen(username_data, 33);
  uint32_t password_len = strnlen(password_data, 33);

  if (username_len > 32 || password_len > 32) {
    absl::PrintF("wrong string len\n");
    return false;
  }

  username = std::string(username_data, username_len);
  password = std::string(password_data, password_len);

  uint16_t padding_val = *reinterpret_cast<const uint16_t *>(username_data + 66);
  if (padding_val != 0) {
    absl::PrintF("unexpected padding val %u\n", padding_val);
  }
  return true;
}

} // namespace amt
