#include "ahi_messages.h"
#include "die.h"
#include "mem_extract.h"

namespace amt {

#define ParseHeader()                                                                    \
  do {                                                                                   \
    if (data.size() < 16) {                                                              \
      return false;                                                                      \
    }                                                                                    \
    Extract(&header, data.subspan(0, 12));                                               \
    Extract(&amt_status, data.subspan(12, 4));                                           \
    if (amt_status != 0) {                                                               \
      return true;                                                                       \
    }                                                                                    \
  } while (0)

bool GetLocalSystemAccountResponse::Deserialize(absl::Span<uint8_t> data) {
  ParseHeader();

  if (data.size() != 16 + 66 + 2) {
    return false;
  }

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

bool EnumerateHashHandlesResponse::Deserialize(absl::Span<uint8_t> data) {
  ParseHeader();

  if (data.size() < 20) {
    return false;
  }

  uint32_t entry_count = Extract<uint32_t>(data.subspan(16, 4));
  if (data.size() != 20 + 4 * entry_count) {
    return false;
  }

  handles.clear();
  for (uint32_t i = 0; i < entry_count; i++) {
    handles.push_back(Extract<uint32_t>(data.subspan(20 + 4 * i, 4)));
  }
  return true;
}

bool GetCertificateHashEntryResponse::Deserialize(absl::Span<uint8_t> data) {
  ParseHeader();

  if (data.size() < 91) {
    return false;
  }
  uint32_t name_len = Extract<uint16_t>(data.subspan(89, 2));
  if (data.size() != 91 + name_len) {
    return false;
  }

  is_default = Extract<uint32_t>(data.subspan(16, 4)) == 1;
  is_active = Extract<uint32_t>(data.subspan(20, 4)) == 1;
  ExtractRaw(certificate_hash.data(), data.subspan(24, 64));
  Extract(&hash_algorithm, data.subspan(88, 1));
  name = ExtractString(data.subspan(91, name_len));
  return true;
}

bool GetUuidResponse::Deserialize(absl::Span<uint8_t> data) {
  ParseHeader();
  if (data.size() < 16 + 16) {
    return false;
  }
  ExtractRaw(uuid.data(), data.subspan(16, 16));
  return true;
}

} // namespace amt
