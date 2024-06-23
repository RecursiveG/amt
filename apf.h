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

// class AmtPortForwarding
// The caller should monitor the fd() and drive the class by
// calling ProcessOneMessage() when there's data available.
// Caller then do necessary operation according to the returned
// MeRequest object (e.g. open port, forward data etc.)
class AmtPortForwarding {
public:
  // ME requests to open a listen port.
  // Like SSH remote forwarding.
  // Must call accept() or reject().
  struct RequestTcpForward {
    std::string addr;
    uint32_t port;

    std::function<void()> accept;
    std::function<void()> reject;
  };

  // Returned after OpenChannel(), can be successful or failure.
  // SendData() should not be called before receiving a successful
  // result.
  struct OpenChannelResult {
    uint32_t channel_id;
    bool success;
  };

  // Caller must receive the completion before calling the next
  // SendData.
  struct SendDataCompletion {
    uint32_t channel_id;
  };

  // Indicates that new data has arrived.
  // Caller must call PeekData() / ConsumeData()
  struct IncomingData {
    uint32_t channel_id;
  };

  // Indicates that ME closed writer side of a channel.
  // Caller should call CloseChannel() if it hasn't already.
  struct ChannelClosed {
    uint32_t channel_id;
  };

  // ME disconnected, caller should stop calling ProcessOneMessage()
  struct MeDisconnect {};

  // nullopt means no special action is needed.
  typedef std::optional<
      std::variant<RequestTcpForward, OpenChannelResult, SendDataCompletion, IncomingData,
                   ChannelClosed, MeDisconnect>>
      MeRequest;

  explicit AmtPortForwarding(std::string mei_dev);
  ~AmtPortForwarding();

  // Poll one message from MEI and dispatch it to
  // the corresponding Handler function.
  MeRequest ProcessOneMessage();

  // port_from: TCP port of the initiator
  // port_to: port of the ME, must come from RequestTcpForward::port
  // return: an assigned channel id.
  uint32_t OpenChannel(uint32_t port_from, uint32_t port_to);

  // Send data to channel.
  // Caller must wait for SendDataCompletion
  void SendData(uint32_t channel_id, absl::Span<const uint8_t> data);

  // Read data from ME after receiving IncomingData.
  // After finishing using the data, call PopData() to remove the first N bytes.
  const std::string *PeekData(uint32_t channel_id);
  void PopData(uint32_t channel_id, uint32_t bytes_to_pop);

  // Close the channel
  // TODO graceful shutdown.
  void CloseChannel(uint32_t channel_id);

  int fd() const { return fd_; }

private:
  struct OpenedChannel {
    uint32_t peer_channel_id;

    uint32_t send_window;
    // data to be sent to ME
    std::string send_buf;
    // data received from ME
    std::string recv_buf;

    bool want_send_completion;
  };

  // Process message and fill ret.
  // Returns true if processing succeeded.
  bool Process(const ApfDisconnect &msg, MeRequest &ret);
  bool Process(const ApfProtocolVersion &msg, MeRequest &ret);
  bool Process(const ApfServiceRequest &msg, MeRequest &ret);
  bool Process(const ApfGlobalMessage &msg, MeRequest &ret);
  bool Process(const ApfChannelOpenConfirmation &msg, MeRequest &ret);
  bool Process(const ApfChannelClose &msg, MeRequest &ret);
  bool Process(const ApfChannelData &msg, MeRequest &ret);
  bool Process(const ApfChannelWindowAdjust &msg, MeRequest &ret);

  // Send to ME via MEI
  void Send(std::string data);
  // Send send_buf to ME
  void FlushSendBuffer(OpenedChannel &channel);

  uint64_t max_msg_length_;
  uint64_t buffer_length_;
  std::unique_ptr<uint8_t[]> buffer_;

  // channel buffers, key is local channel id.
  std::unordered_map<uint32_t, OpenedChannel> channels_;
  // std::unordered_map<uint32_t, uint32_t> local_to_me_channel_;
  uint32_t next_channel_id_ = 0;
  int fd_ = -1;
};

} // namespace amt

#endif //__APF_H__
