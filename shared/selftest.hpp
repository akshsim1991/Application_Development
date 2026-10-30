#pragma once
// The engine's test commands, run by the tests (tests/smoke.sh on Windows, tests/engine and the macOS build check
// elsewhere), the same on every system:
//   --tables in.pdf report.txt                     the tables found on every page
//   --tableedit in.pdf out.pdf page x y op args... changes the table there (out.pdf.txt: what happened, or why not)
//   --hfremove in.pdf out.pdf report.txt what pages [area l t r b top|bottom]   headers and footers found / deleted
#include <string>
#include <vector>

namespace selftest {

// `args` as on the command line, without the program. The exit code, or -1 when args[0] is not one of these commands.
int run(const std::vector<std::wstring>& args);

} // namespace selftest
