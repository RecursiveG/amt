#include "apf.h"
#include "die.h"
#include "hexdump.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/mei.h>
#include <linux/mei_uuid.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cinttypes>
#include <cstring>

#include <absl/strings/str_format.h>
#include <absl/time/clock.h>
#include <absl/types/span.h>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>

namespace amt {

// WSMAN XML can be forwarded to ME over APF over MEI.
// LME service handles the AMT Port Forwarding protocol.
#define MEI_LME_GUID                                                                     \
  UUID_LE(0x6733a4db, 0x0476, 0x4e7b, 0xb3, 0xaf, 0xbc, 0xfc, 0x29, 0xbe, 0xe7, 0xa7)

AmtPortForwarding::AmtPortForwarding(std::string mei_dev) {
  fd_ = open("/dev/mei0", O_RDWR);
  die_if(fd_ < 0, "mei fd error");

  mei_connect_client_data data;
  data.in_client_uuid = MEI_LME_GUID;

  int ret = ioctl(fd_, IOCTL_MEI_CONNECT_CLIENT, &data);
  die_if(ret < 0, "ioctl error %d\n", ret);

  absl::PrintF("Connected to LME max_msg_len=%u protocol_ver=%u\n",
               data.out_client_properties.max_msg_length,
               data.out_client_properties.protocol_version);

  int flags = fcntl(fd_, F_GETFL);
  die_if(flags == -1, "fcntl GETFL");
  int err = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
  die_if(err == -1, "fcntl SEFL");

  max_msg_length_ = data.out_client_properties.max_msg_length;
  buffer_length_ = max_msg_length_ + 32;
  buffer_ = std::make_unique<uint8_t[]>(buffer_length_);
}

AmtPortForwarding::~AmtPortForwarding() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

// Overall workflow:
// ME requests pfwd svc -> accept
// ME requests listen on port -> accept/reject
// Caller requests open channel -> ME accept/reject
AmtPortForwarding::MeRequest AmtPortForwarding::ProcessOneMessage() {
  MeRequest ret = std::nullopt;

  ssize_t len = read(fd_, buffer_.get(), buffer_length_);
  die_if(len < 0, "Failed to read ret=%d errno=%d\n", len, errno);
  if (len == 0) {
    absl::PrintF("ME connection closing...\n");
    return std::nullopt;
  }

  auto data = absl::MakeSpan(buffer_.get(), len);
  bool parsing_success = false;
  std::optional<std::string> processing_err = std::nullopt;

#define CASE_MSG_TYPE(T)                                                                 \
  case T::kType: {                                                                       \
    T t{};                                                                               \
    parsing_success = t.Deserialize(data);                                               \
    if (parsing_success) {                                                               \
      bool process_success = Process(t, ret);                                            \
      if (!process_success) {                                                            \
        processing_err = t.ToString();                                                   \
      }                                                                                  \
    }                                                                                    \
  } break
  switch (data[0]) {
    // clang-format off
    CASE_MSG_TYPE(ApfDisconnect);
    CASE_MSG_TYPE(ApfProtocolVersion);
    CASE_MSG_TYPE(ApfServiceRequest);
    CASE_MSG_TYPE(ApfGlobalMessage);
    CASE_MSG_TYPE(ApfChannelOpenConfirmation);
    CASE_MSG_TYPE(ApfChannelClose);
    CASE_MSG_TYPE(ApfChannelData);
    CASE_MSG_TYPE(ApfChannelWindowAdjust);
    default: parsing_success = false;
    // clang-format on
  }
#undef CASE_MSG_TYPE

  if (!parsing_success) {
    absl::PrintF("Invalid message: len=%u\n%s\n", data.size(),
                 Hexdump(data.data(), data.size()));
    return std::nullopt;
  }

  if (processing_err.has_value()) {
    absl::PrintF("Failed to process message: %s\n%s\n", *processing_err,
                 Hexdump(data.data(), data.size()));
    return std::nullopt;
  }

  return ret;
}

//
// Message handlers
//

bool AmtPortForwarding::Process(const ApfDisconnect &msg, MeRequest &ret) {
  absl::PrintF("Received %s\n", msg.ToString());
  ret = MeDisconnect{};
  return true;
}

bool AmtPortForwarding::Process(const ApfProtocolVersion &msg, MeRequest &ret) {
  absl::PrintF("Received %s\n", msg.ToString());

  // send message back
  Send(msg.Serialize());

  return true;
}

bool AmtPortForwarding::Process(const ApfServiceRequest &msg, MeRequest &ret) {
  absl::PrintF("Received %s\n", msg.ToString());

  if (msg.service_name == "pfwd@amt.intel.com") {
    ApfServiceAccept acc{};
    acc.service_name = msg.service_name;
    Send(acc.Serialize());
  } else {
    ApfDisconnect dis{
        .reason = ApfDisconnect::kServiceNotAvailable,
    };
    Send(dis.Serialize());
    ret = MeDisconnect{};
  }
  return true;
}

bool AmtPortForwarding::Process(const ApfGlobalMessage &msg, MeRequest &ret) {
  absl::PrintF("Received %s\n", msg.ToString());

  if (msg.request_string == "tcpip-forward") {
    ret = RequestTcpForward{
        .addr = msg.address_to_bind,
        .port = msg.port_to_bind,
        .accept =
            [this, port = msg.port_to_bind]() {
              Send(ApfRequestSuccess{.port_bound = port}.Serialize());
            },
        .reject = [this]() { Send(ApfRequestFailure().Serialize()); },
    };
  } else if (msg.request_string == "cancel-tcpip-forward") {
    die("unimplemented");
  } else {
    // UDP not implemented.
  }
  return true;
}

bool AmtPortForwarding::Process(const ApfChannelOpenConfirmation &msg, MeRequest &ret) {
  absl::PrintF("Received %s\n", msg.ToString());

  channels_[msg.recipient_channel] = OpenedChannel{
      .peer_channel_id = msg.sender_channel,
      .send_window = msg.initial_window_size,
  };

  ret = OpenChannelResult{
      .channel_id = msg.recipient_channel,
      .success = true,
  };
  return true;
}

bool AmtPortForwarding::Process(const ApfChannelClose &msg, MeRequest &ret) {
  absl::PrintF("Received %s\n", msg.ToString());

  // cleanup is handled in CloseChannel().
  ret = ChannelClosed{
      .channel_id = msg.recipient_channel,
  };
  return true;
}

bool AmtPortForwarding::Process(const ApfChannelData &msg, MeRequest &ret) {
  // absl::PrintF("Received ChannelData\n%s", Hexdump(msg.data.data(), msg.data.size()));
  auto it = channels_.find(msg.recipient_channel);
  if (it == channels_.end()) {
    absl::PrintF("Recipient channel not found.\n");
    return false;
  }

  it->second.recv_buf += msg.data;
  ret = IncomingData{
      .channel_id = msg.recipient_channel,
  };
  return true;
}

bool AmtPortForwarding::Process(const ApfChannelWindowAdjust &msg, MeRequest &ret) {
  // absl::PrintF("Received %s\n", msg.ToString());
  auto it = channels_.find(msg.recipient_channel);
  if (it == channels_.end()) {
    absl::PrintF("Recipient channel not found.\n");
    return false;
  }

  OpenedChannel &channel = it->second;
  channel.send_window += msg.bytes_to_add;

  if (!channel.send_buf.empty()) {
    FlushSendBuffer(channel);
  }

  if (channel.send_buf.empty() && channel.want_send_completion) {
    // std::cerr << "completion raised " << msg.recipient_channel << std::endl;
    ret = SendDataCompletion{
        .channel_id = msg.recipient_channel,
    };
    channel.want_send_completion = false;
  }

  return true;
}

//
// Public APIs
//

uint32_t AmtPortForwarding::OpenChannel(uint32_t port_from, uint32_t port_to) {
  // TODO handle port collision
  ApfChannelOpenRequest req{};
  req.is_forwarded = true;
  req.sender_channel = next_channel_id_++;
  req.initial_window_size = 4096;
  req.connected_address = "127.0.0.1";
  req.connected_port = port_to;
  req.originator_address = "127.0.0.1";
  req.originator_port = port_from;

  absl::PrintF("New channel: %s\n", req.ToString());
  Send(req.Serialize());
  return req.sender_channel;
}

void AmtPortForwarding::CloseChannel(uint32_t channel_id) {
  // TODO pending buffer
  // TODO two-way close bookkeeping.
  auto it = channels_.find(channel_id);
  die_if(it == channels_.end(), "unknown channel to close : %u", channel_id);

  ApfChannelClose req{.recipient_channel = it->second.peer_channel_id};
  Send(req.Serialize());
  channels_.erase(it);
}

bool AmtPortForwarding::SendData(uint32_t channel_id, absl::Span<const uint8_t> data) {
  die_if(data.size() == 0, "Cannot send 0 byte.");
  auto it = channels_.find(channel_id);
  die_if(it == channels_.end(), "Channel %u not found.", channel_id);

  it->second.send_buf.append(reinterpret_cast<const char *>(data.data()), data.size());
  // std::cerr << "send data enqueued " << channel_id << std::endl;
  it->second.want_send_completion = true;
  FlushSendBuffer(it->second);
  return !it->second.send_buf.empty();
}

const std::string *AmtPortForwarding::PeekData(uint32_t channel_id) {
  auto it = channels_.find(channel_id);
  if (it == channels_.end()) {
    absl::PrintF("Channel not found.\n");
    return nullptr;
  }
  return &it->second.recv_buf;
}

void AmtPortForwarding::PopData(uint32_t channel_id, uint32_t bytes_to_pop) {
  auto it = channels_.find(channel_id);
  if (it == channels_.end()) {
    absl::PrintF("Channel not found.\n");
  }

  std::string &buf = it->second.recv_buf;
  die_if(bytes_to_pop > buf.size(), "too many bytes to pop");
  buf = buf.substr(bytes_to_pop);

  ApfChannelWindowAdjust req{
      .recipient_channel = it->second.peer_channel_id,
      .bytes_to_add = bytes_to_pop,
  };
  Send(req.Serialize());
}

//
// Private helper functions
//

void AmtPortForwarding::Send(std::string data) {
  // absl::PrintF("sending data len=%u\n%s\n", data.size(),
  //              Hexdump(data.data(), data.size()));
  size_t sent = write(fd_, data.data(), data.size());
  die_if(sent != data.size(), "write error");
}

void AmtPortForwarding::FlushSendBuffer(OpenedChannel &channel) {
  size_t len = channel.send_window;
  if (channel.send_buf.size() < len) {
    len = channel.send_buf.size();
  }
  ApfChannelData req{
      .recipient_channel = channel.peer_channel_id,
      .data = channel.send_buf.substr(0, len),
  };
  Send(req.Serialize());
  channel.send_window -= len;
  channel.send_buf = channel.send_buf.substr(len);
}

} // namespace amt
