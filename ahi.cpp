#include "ahi.h"
#include "die.h"
#include "hexdump.h"

#include <cerrno>

#include <fcntl.h>
#include <linux/mei.h>
#include <linux/mei_uuid.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <absl/strings/str_format.h>

// Define GUID used to connect to the PTHI client (via the HECI device)
// {12F80028-B4B7-4B2D-ACA8-46E0FF65814C}
// https://software.intel.com/sites/manageability/AMT_Implementation_and_Reference_Guide/WordDocuments/meicommands1.htm
// AMT Host Interface
#define MEI_AMTHI_GUID                                                                   \
  UUID_LE(0x12F80028, 0xB4B7, 0x4b2d, 0xAC, 0xA8, 0x46, 0xE0, 0xFF, 0x65, 0x81, 0x4c)

namespace amt {

namespace {
template <typename ReqT, typename RspT>
bool RunExchange(const ReqT &req, RspT &rsp, int fd, uint64_t max_msg_length) {
  static_assert(std::is_pod_v<ReqT>);
  size_t write_len = sizeof(ReqT);
  die_if(write_len > max_msg_length, "write message too large");
  auto recv_buf = std::make_unique<uint8_t[]>(max_msg_length + 1);

  // std::printf("AmtHostInterface write:\n %s\n", Hexdump(write_data,
  // write_len).c_str());
  ssize_t written = write(fd, &req, write_len);
  die_if(written < 0, "write errno=%u", errno);
  die_if(static_cast<size_t>(written) != write_len, "write error");

  ssize_t r = read(fd, recv_buf.get(), max_msg_length + 1);
  die_if(r <= 0, "read errno=%u", errno);
  // std::printf("AmtHostInterface read:\n %s\n", Hexdump(buffer_.get(), r).c_str());
  die_if(static_cast<size_t>(r) > max_msg_length, "reply too large:\n%s",
         Hexdump(recv_buf.get(), r));

  bool success = rsp.Deserialize(absl::MakeSpan(recv_buf.get(), r));
  if (!success) {
    absl::PrintF("Failed to parse reply:\n%s\n", Hexdump(recv_buf.get(), r));
  }

  return success;
}
} // namespace

AmtHostInterface::AmtHostInterface(std::string mei_dev) {
  fd_ = open(mei_dev.c_str(), O_RDWR);
  printf("Opened mei fd %d\n", fd_);
  die_if(fd_ < 0, "mei fd error");

  mei_connect_client_data data;
  data.in_client_uuid = MEI_AMTHI_GUID;

  int ret = ioctl(fd_, IOCTL_MEI_CONNECT_CLIENT, &data);
  die_if(ret < 0, "ioctl error %d\n", ret);

  absl::PrintF("Connected to AMTHI max_msg_len=%u protocol_ver=%u\n",
               data.out_client_properties.max_msg_length,
               data.out_client_properties.protocol_version);

  max_msg_length_ = data.out_client_properties.max_msg_length;
}

AmtHostInterface::~AmtHostInterface() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

bool AmtHostInterface::GetLocalSystemAccount(AhiGetLocalSystemAccountResponse &rsp) {
  struct {
    AhiHeader header;
    uint8_t reserved[40];
  } __attribute__((packed)) req{};
  static_assert(sizeof(req) == 12 + 40);
  req.header.Init(0x04000067, 40);
  return RunExchange(req, rsp, fd_, max_msg_length_);
}

bool AmtHostInterface::EnumerateHashHandles(EnumerateHashHandlesResponse &rsp) {
  AhiHeader req{};
  req.Init(0x400002c, 0);
  return RunExchange(req, rsp, fd_, max_msg_length_);
}

bool AmtHostInterface::GetCertificateHashEntry(GetCertificateHashEntryResponse &rsp,
                                               uint32_t handle) {
  struct {
    AhiHeader header;
    uint32_t handle;
  } __attribute__((packed)) req;
  static_assert(sizeof(req) == 12 + 4);

  req.header.Init(0x0400002D, 4);
  req.handle = handle;
  return RunExchange(req, rsp, fd_, max_msg_length_);
}

} // namespace amt
