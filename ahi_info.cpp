#include "ahi.h"
#include "ahi_messages.h"

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
    AhiGetLocalSystemAccountResponse rsp;
    bool success = ahi.GetLocalSystemAccount(rsp);
    if (success) {
      std::cout << rsp.ToString() << std::endl;
    } else {
      std::cout << "GetLocalSystemAccount failed." << std::endl;
    }
  }
  return 0;
}
