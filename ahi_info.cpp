#include "ahi.h"
#include "ahi_messages.h"
#include "die.h"

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>

ABSL_FLAG(std::string, mei_device, "/dev/mei0", "Path to the MEI chardev");

using namespace amt;

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("Dump ME info");
  absl::ParseCommandLine(argc, argv);

  AmtHostInterface ahi(absl::GetFlag(FLAGS_mei_device));

  {
    GetUuidResponse rsp;
    bool success = ahi.GetUuid(rsp);
    die_if(!success, "GetUuid");
    std::cout << rsp.ToString() << std::endl;
  }

  {
    GetLocalSystemAccountResponse rsp;
    bool success = ahi.GetLocalSystemAccount(rsp);
    die_if(!success, "GetLocalSystemAccount");
    std::cout << rsp.ToString() << std::endl;
  }

  {
    EnumerateHashHandlesResponse rsp;
    bool success = ahi.EnumerateHashHandles(rsp);
    die_if(!success, "EnumerateHashHandles");
    die_if(rsp.amt_status != 0, "EnumerateHashHandles status=%u", rsp.amt_status);

    for (uint32_t handle : rsp.handles) {
      GetCertificateHashEntryResponse rsp2;
      bool success2 = ahi.GetCertificateHashEntry(rsp2, handle);
      die_if(!success2, "GetCertificateHashEntry");
      std::cout << absl::StrFormat("Handle %#010x %s\n", handle, rsp2.ToString());
    }
  }

  {
    AhiHeader req{};
    req.Init(0x4000011, 0);
    std::string rsp =
        ahi.CustomCommand(absl::MakeSpan(reinterpret_cast<uint8_t *>(&req), sizeof(req)));
    std::cout << "GetProvisioningState:" << std::endl
              << Hexdump(rsp.data(), rsp.size()) << std::endl;
  }

  {
    AhiHeader req{};
    req.Init(0x400006b, 0);
    std::string rsp =
        ahi.CustomCommand(absl::MakeSpan(reinterpret_cast<uint8_t *>(&req), sizeof(req)));
    std::cout << "GetControlMode:" << std::endl
              << Hexdump(rsp.data(), rsp.size()) << std::endl;
  }

  return 0;
}
