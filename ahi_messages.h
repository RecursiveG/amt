#ifndef __AHI_MESSAGES_H__
#define __AHI_MESSAGES_H__

#include "hexdump.h"

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
    return absl::StrFormat(
        "AhiHeader{ver=%u.%u, cmd=%#010x(class=%u, op=%u, is_resp=%u), len=%u}",
        ver_major, ver_minor, cmd, cmd_class, cmd_operation, cmd_is_response, length);
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

struct EnumerateHashHandlesResponse {
  AhiHeader header;
  uint32_t amt_status;
  // --
  std::vector<uint32_t> handles;
  // --
  bool Deserialize(absl::Span<uint8_t> data);
};

struct GetCertificateHashEntryResponse {
  enum HashAlgorithm : uint8_t {
    kSha1 = 1,
    kSha256 = 2,
    kSha384 = 3,
  };

  AhiHeader header;
  uint32_t amt_status;
  // --
  bool is_default;
  bool is_active;
  std::array<uint8_t, 64> certificate_hash;
  // 1=SHA1 2=SHA256 3=SHA384
  HashAlgorithm hash_algorithm;
  std::string name;

  bool Deserialize(absl::Span<uint8_t> data);

  std::string ToString() const {
    if (amt_status != 0) {
      return absl::StrFormat("GetCertificateHashEntryResponse{%s, amt_status=%u}",
                             header.ToString(), amt_status);
    }

    std::string hash_str;
    int hash_len = 64;
    if (hash_algorithm == kSha1) {
      hash_str = "SHA1(";
      hash_len = 20;
    } else if (hash_algorithm == kSha256) {
      hash_str = "SHA256(";
      hash_len = 32;
    } else if (hash_algorithm == kSha384) {
      hash_str = "SHA384(";
      hash_len = 48;
    } else {
      hash_str = "UNKNOWN(";
      hash_len = 64;
    }
    absl::StrAppend(&hash_str, HexString(certificate_hash.data(), hash_len));
    hash_str += ")";

    return absl::StrFormat(
        "GetCertificateHashEntryResponse{%s, %s, %s, %s, algo=%u, name=%s}",
        header.ToString(), is_default ? "default" : "not-default",
        is_active ? "active" : "not-active", hash_str, hash_algorithm, name);
  }
};

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
