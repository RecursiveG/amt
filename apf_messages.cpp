#include "apf.h"
#include "die.h"
#include "hexdump.h"
#include "mem_extract.h"

#include <arpa/inet.h>

#include <cinttypes>
#include <cstring>

#include <absl/strings/str_format.h>
#include <absl/types/span.h>
#include <string>
#include <type_traits>

namespace amt {
namespace {

// 4 bytes big-endian string length followed by data
void FillStringWithHeader(absl::Span<uint8_t> to, const std::string &from) {
  die_if(to.size() != from.size() + 4, "size mismatch");
  Fill(to.subspan(0, 4), htonl(from.size()));
  FillRaw(to.subspan(4), from.data());
}

// Return true iff data[0] == expected_id
bool VerifyType(absl::Span<uint8_t> data, uint8_t expected_id) {
  if (data.empty()) {
    return false;
  }
  return data[0] == expected_id;
}

} // namespace

bool ApfDisconnect::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() != 7)
    return false;

  reason =
      static_cast<ApfDisconnect::Reason>(ntohl(Extract<uint32_t>(data.subspan(1, 4))));
  return true;
}

std::string ApfDisconnect::Serialize() const {
  std::string ret(7, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), 7);

  Fill(data.subspan(0, 1), kType);
  Fill(data.subspan(1, 4), htonl(reason));
  return ret;
}

std::string ApfDisconnect::ToString() const {
  return absl::StrFormat("ApfDisconnect{reason=%u}", reason);
}

bool ApfProtocolVersion::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() != 93)
    return false;

  major = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  minor = ntohl(Extract<uint32_t>(data.subspan(5, 4)));
  ExtractRaw(uuid, data.subspan(13, 16));
  return true;
}

std::string ApfProtocolVersion::Serialize() const {
  std::string ret(93, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), 93);
  Fill(data.subspan(0, 1), kType);
  Fill(data.subspan(1, 4), htonl(major));
  Fill(data.subspan(5, 4), htonl(minor));
  FillRaw(data.subspan(13, 16), uuid);
  return ret;
}

std::string ApfProtocolVersion::ToString() const {
  return absl::StrFormat("ApfProtocolVersion{major=%u,minor=%u,uuid=%s}", major, minor,
                         HexString(uuid, 16));
}

bool ApfServiceRequest::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() < 5)
    return false;

  uint32_t name_len = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  if (data.size() != 5 + name_len)
    return false;
  service_name = ExtractString(data.subspan(5, name_len));
  return true;
}

std::string ApfServiceRequest::Serialize() const {
  const uint32_t len = 5 + service_name.size();
  std::string ret(len, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), len);

  Fill(data.subspan(0, 1), kType);
  FillStringWithHeader(data.subspan(1), service_name);
  return ret;
}

std::string ApfServiceRequest::ToString() const {
  return absl::StrFormat("ApfServiceRequest{service=%s}", service_name);
}

bool ApfServiceAccept::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() < 5)
    return false;

  uint32_t name_len = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  if (data.size() != 5 + name_len)
    return false;
  service_name = ExtractString(data.subspan(5, name_len));
  return true;
}

std::string ApfServiceAccept::Serialize() const {
  const uint32_t len = 5 + service_name.size();
  std::string ret(len, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), len);

  Fill(data.subspan(0, 1), kType);
  FillStringWithHeader(data.subspan(1), service_name);
  return ret;
}

std::string ApfServiceAccept::ToString() const {
  return absl::StrFormat("ApfServiceAccept{service=%s}", service_name);
}

bool ApfGlobalMessage::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() < 5)
    return false;

  uint32_t name_len = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  if (data.size() < 5 + name_len + 1)
    return false;
  request_string = ExtractString(data.subspan(5, name_len));
  want_reply = Extract<uint8_t>(data.subspan(5 + name_len, 1)) == 1;

  if ((request_string == "tcpip-forward" || request_string == "cancel-tcpip-forward") &&
      want_reply) {
    // parse address_to_bind & port
    if (data.size() < 5 + name_len + 5)
      return false;
    uint32_t addr_len = ntohl(Extract<uint32_t>(data.subspan(5 + name_len + 1, 4)));
    if (data.size() != 5 + name_len + 5 + addr_len + 4)
      return false;
    address_to_bind = ExtractString(data.subspan(5 + name_len + 5, addr_len));
    port_to_bind = ntohl(Extract<uint32_t>(data.subspan(5 + name_len + 5 + addr_len, 4)));
  } else {
    // TODO UdpSendTo is not supported yet
    return false;
  }
  return true;
}

std::string ApfGlobalMessage::Serialize() const { die("unimplemented"); }

std::string ApfGlobalMessage::ToString() const {
  return absl::StrFormat(
      "ApfGlobalMessage{request=%s, %s, address_to_bind=%s, port_to_bind=%u}",
      request_string, want_reply ? "want_reply" : "dont_want_reply", address_to_bind,
      port_to_bind);
}

bool ApfRequestSuccess::Deserialize(absl::Span<uint8_t> data) {
  // TODO unimplemented
  return false;
}

std::string ApfRequestSuccess::Serialize() const {
  std::string ret(1, static_cast<char>(kType));
  if (port_bound.has_value()) {
    uint32_t be = htonl(*port_bound);
    ret += std::string(reinterpret_cast<char *>(&be), 4);
  }
  return ret;
}

std::string ApfRequestSuccess::ToString() const {
  if (port_bound.has_value()) {
    return absl::StrFormat("ApfRequestSuccess{port_bound=%u}", *port_bound);
  } else {
    return "ApfRequestSuccess{}";
  }
}

bool ApfRequestFailure::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() != 1)
    return false;
  return true;
}

std::string ApfRequestFailure::Serialize() const {
  std::string ret(1, static_cast<char>(kType));
  return ret;
}

std::string ApfRequestFailure::ToString() const { return "ApfRequestFailure{}"; }

bool ApfChannelOpenRequest::Deserialize(absl::Span<uint8_t> data) {
  // TODO unimplemented
  return false;
}

std::string ApfChannelOpenRequest::Serialize() const {
  constexpr uint32_t kReserved = 0xFFFFFFFFul;
  size_t lenT = is_forwarded ? 15 : 12;
  size_t lenC = connected_address.size();
  size_t lenO = originator_address.size();
  size_t return_len = lenT + lenC + lenO + 33;
  std::string ret(return_len, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), return_len);

  Fill(data.subspan(0, 1), kType);
  if (is_forwarded) {
    FillStringWithHeader(data.subspan(1, 4 + lenT), "forwarded-tcpip");
  } else {
    FillStringWithHeader(data.subspan(1, 4 + lenT), "direct-tcpip");
  }
  Fill(data.subspan(5 + lenT, 4), htonl(sender_channel));
  Fill(data.subspan(5 + lenT + 4, 4), htonl(initial_window_size));
  Fill(data.subspan(5 + lenT + 8, 4), kReserved);
  FillStringWithHeader(data.subspan(5 + lenT + 12, 4 + lenC), connected_address);
  Fill(data.subspan(5 + lenT + 16 + lenC, 4), htonl(connected_port));
  FillStringWithHeader(data.subspan(5 + lenT + 16 + lenC + 4, 4 + lenO),
                       originator_address);
  Fill(data.subspan(5 + lenT + 16 + lenC + 8 + lenO, 4), htonl(originator_port));

  return ret;
}

std::string ApfChannelOpenRequest::ToString() const {
  return absl::StrFormat("ApfChannelOpenRequest{type=%s,sender_channel=%u,initial_window_"
                         "size=%u,connected=%s:%u,originator=%s:%u}",
                         is_forwarded ? "forwarded-tcpip" : "direct-tcpip",
                         sender_channel, initial_window_size, connected_address,
                         connected_port, originator_address, originator_port);
}

bool ApfChannelOpenConfirmation::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() != 17)
    return false;

  recipient_channel = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  sender_channel = ntohl(Extract<uint32_t>(data.subspan(5, 4)));
  initial_window_size = ntohl(Extract<uint32_t>(data.subspan(9, 4)));
  return true;
}

std::string ApfChannelOpenConfirmation::Serialize() const { die("unimplemented"); }

std::string ApfChannelOpenConfirmation::ToString() const {
  return absl::StrFormat("ApfChannelOpenConfirmation{recipient_channel=%u,sender_channel="
                         "%u,initial_window_size=%u}",
                         recipient_channel, sender_channel, initial_window_size);
}

bool ApfChannelClose::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() != 5)
    return false;
  recipient_channel = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  return true;
}

std::string ApfChannelClose::Serialize() const {
  std::string ret(1, static_cast<char>(kType));
  uint32_t be = htonl(recipient_channel);
  ret += std::string(reinterpret_cast<char *>(&be), 4);
  return ret;
}

std::string ApfChannelClose::ToString() const {
  return absl::StrFormat("ApfChannelClose{recipient_channel=%u}", recipient_channel);
}

bool ApfChannelData::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() < 9)
    return false;

  uint32_t datalen = ntohl(Extract<uint32_t>(data.subspan(5, 4)));
  if (data.size() != 9 + datalen) {
    return false;
  }

  recipient_channel = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  this->data = ExtractString(data.subspan(9));

  return true;
}

std::string ApfChannelData::Serialize() const {
  uint32_t len = 9 + this->data.size();
  std::string ret(len, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), len);

  Fill(data.subspan(0, 1), kType);
  Fill(data.subspan(1, 4), htonl(recipient_channel));
  FillStringWithHeader(data.subspan(5), this->data);
  return ret;
}

std::string ApfChannelData::ToString() const {
  return absl::StrFormat("ApfChannelData{recipient_channel=%u,data_len=%u}",
                         recipient_channel, data.size());
}

bool ApfChannelWindowAdjust::Deserialize(absl::Span<uint8_t> data) {
  if (!VerifyType(data, kType) || data.size() != 9)
    return false;

  recipient_channel = ntohl(Extract<uint32_t>(data.subspan(1, 4)));
  bytes_to_add = ntohl(Extract<uint32_t>(data.subspan(5, 4)));

  return true;
}

std::string ApfChannelWindowAdjust::Serialize() const {
  std::string ret(9, '\0');
  auto data = absl::MakeSpan(reinterpret_cast<uint8_t *>(ret.data()), 9);
  Fill(data.subspan(0, 1), kType);
  Fill(data.subspan(1, 4), htonl(recipient_channel));
  Fill(data.subspan(5, 4), htonl(bytes_to_add));
  return ret;
}

std::string ApfChannelWindowAdjust::ToString() const {
  return absl::StrFormat("ApfChannelWindowAdjust{recipient_channel=%u,bytes_to_add=%u}",
                         recipient_channel, bytes_to_add);
}
} // namespace amt
