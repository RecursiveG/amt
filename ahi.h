#ifndef __AHI_H__
#define __AHI_H__

#include "ahi_messages.h"

#include <absl/types/span.h>

namespace amt {
class AmtHostInterface {
public:
  explicit AmtHostInterface(std::string mei_dev);
  ~AmtHostInterface();

  // Return: true if success
  bool GetLocalSystemAccount(GetLocalSystemAccountResponse &rsp);
  bool EnumerateHashHandles(EnumerateHashHandlesResponse &rsp);
  bool GetCertificateHashEntry(GetCertificateHashEntryResponse &rsp, uint32_t handle);
  bool GetUuid(GetUuidResponse &rsp);

  // Send request, then return raw response.
  std::string CustomCommand(absl::Span<uint8_t> req);

private:
  uint64_t max_msg_length_;
  int fd_ = -1;
};

} // namespace amt

#endif // __AHI_H__
