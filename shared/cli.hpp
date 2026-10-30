#pragma once
// The command line (shared/cli.cpp): FeatherPDF --export ..., --compress ..., --protect ..., and the test commands.
#include <string>
#include <vector>

namespace cli {

// `args` as given, without the program. The exit code (0 success, 2 bad arguments, 3 the PDF could not be opened,
// 4 the work or the writing failed, ...), or -1 when args[0] is not a command.
int run(std::vector<std::wstring> args);

} // namespace cli
