#include <string>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/str_format.h>

ABSL_FLAG(std::string, foo, "world", "");

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("Forwards TCP port via MEI");
  absl::ParseCommandLine(argc, argv);

  absl::PrintF("Hello, %s!\n", absl::GetFlag(FLAGS_foo));

  return 0;
}
