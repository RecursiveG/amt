#ifndef __AHI_MESSAGES_H__
#define __AHI_MESSAGES_H__

#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <cinttypes>
#include <string>

namespace amt {

struct AhiHeader {
  uint8_t ver_major;
  uint8_t ver_minor;
  uint16_t reserved;

  union {
    struct {
      uint32_t cmd_operation : 23;
      uint32_t cmd_is_response : 1;
      uint32_t cmd_class : 8;
    };
    uint32_t cmd;
  };

  uint32_t length;

  std::string ToString() const {
    return absl::StrFormat("AhiHeader{ver=%u.%u, cmd=%#010x(class=%u, op=%u, is_resp=%u), len=%u}",
                           ver_major, ver_minor, cmd, cmd_class, cmd_operation,
                           cmd_is_response, length);
  }

  void Init(uint32_t cmd, uint32_t len) {
    ver_major = 1;
    ver_minor = 1;
    reserved = 0;
    this->cmd = cmd;
    this->length = len;
  }
};
static_assert(sizeof(AhiHeader) == 12);

struct AhiGetLocalSystemAccountResponse {
  AhiHeader header;
  uint32_t amt_status;
  std::string username;
  std::string password;

  bool Deserialize(absl::Span<uint8_t> data);
  
  std::string ToString() const {
    std::string ret = "GetLocalSystemAccountResponse{";
    absl::StrAppendFormat(&ret, "%s, status=%u", header.ToString(), amt_status);
    if (amt_status == 0) {
      absl::StrAppendFormat(&ret, ", user=%s, passwd=%s", username, password);
    }
    absl::StrAppend(&ret, "}");
    return ret;
  }
};

} // namespace amt

#endif // __AHI_MESSAGES_H__
