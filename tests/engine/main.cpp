// The engine check: the shared PDF engine built and run off Windows (on Linux here, and in the macOS build check), with
// the same test commands as the Windows app, so their results can be compared.
#include <cstdio>
#include <string>
#include <vector>
#include "pdfium_api.hpp"
#include "platform.hpp"
#include "cli.hpp"

int main(int argc, char** argv) {
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) args.push_back(platform::wide(argv[i]));
    std::wstring err;
    if (!pdfium::load(err)) { std::fprintf(stderr, "%s\n", platform::utf8(err).c_str()); return 10; }
    const int rc = cli::run(args);
    if (rc < 0) { std::fprintf(stderr, "usage: engine_check --tables|--tableedit|--hfremove ...\n"); return 2; }
    return rc;
}
