#ifndef __APF_H__
#define __APF_H__

#include <cinttypes>

#include <absl/types/span.h>
#include <optional>
#include <string>
#include <variant>

namespace amt {

// AMT Port Forwarding Protocol Messages
// Deserialize(data) -> ret
//   data: incoming data from ME, must contain exactly one message.
//   ret:  returns true if parsing success.
// Serialize() -> ret
//   ret:  returns the binary representation to be sent to the ME.
// ToString() -> ret
//   ret:  human readable format of this message.

struct ApfDisconnect {
  enum Reason : uint32_t {
    kServiceNotAvailable = 7,
  };
  static constexpr uint8_t kType = 1;
  Reason reason;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfProtocolVersion {
  static constexpr uint8_t kType = 192;
  uint32_t major;
  uint32_t minor;
  uint8_t uuid[16];

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfServiceRequest {
  static constexpr uint8_t kType = 5;
  std::string service_name;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfServiceAccept {
  static constexpr uint8_t kType = 6;
  std::string service_name;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfGlobalMessage {
  static constexpr uint8_t kType = 80;
  std::string request_string;
  bool want_reply;

  // Used for TcpForwardRequest & TcpForwardCancelRequest
  // TODO udp support
  std::string address_to_bind;
  uint32_t port_to_bind;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

// The format of this message two depends on the corresponding GlobalMessage request type.
// Either TcpForwardRequest or TcpForwardCancelRequest.
struct ApfRequestSuccess {
  static constexpr uint8_t kType = 81;

  // Only present if this is TcpForwardReply
  std::optional<uint32_t> port_bound;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfRequestFailure {
  static constexpr uint8_t kType = 82;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfChannelOpenRequest {
  static constexpr uint8_t kType = 90;

  bool is_forwarded; // forwarded or direct
  uint32_t sender_channel;
  uint32_t initial_window_size;
  std::string connected_address;
  uint32_t connected_port;
  std::string originator_address;
  uint32_t originator_port;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfChannelOpenConfirmation {
  static constexpr uint8_t kType = 91;

  uint32_t recipient_channel; // channel number assigned by the receiver.
  uint32_t sender_channel;    // channel number assigned by the sender of this message.
  uint32_t initial_window_size;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfChannelClose {
  static constexpr uint8_t kType = 97;

  uint32_t recipient_channel;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfChannelData {
  static constexpr uint8_t kType = 94;

  uint32_t recipient_channel;
  std::string data;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

struct ApfChannelWindowAdjust {
  static constexpr uint8_t kType = 93;

  uint32_t recipient_channel;
  uint32_t bytes_to_add;

  bool Deserialize(absl::Span<uint8_t> data);
  std::string Serialize() const;
  std::string ToString() const;
};

} // namespace amt

#endif //__APF_H__
