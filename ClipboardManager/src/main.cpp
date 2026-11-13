#include "app.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdline, int) {
    SetProcessDPIAware();
    std::wstring cmd = cmdline ? cmdline : L"";
    size_t bp = cmd.find(L"--bench");
    if (bp != std::wstring::npos) {
        std::wstring rest = core::Trim(cmd.substr(bp + 7));
        size_t sp = rest.find(L' ');
        int n = _wtoi(rest.substr(0, sp).c_str());
        std::wstring out = sp == std::wstring::npos ? L"bench.txt" : core::Trim(rest.substr(sp + 1));
        if (out.size() > 1 && out.front() == L'"' && out.back() == L'"') out = out.substr(1, out.size() - 2);
        return RunBench(n > 0 ? n : 100000, out);
    }
    if (cmd.find(L"--seed-demo") != std::wstring::npos) return RunSeedDemo();
    size_t p = cmd.find(L"--selftest");
    if (p != std::wstring::npos) {
        std::wstring out = core::Trim(cmd.substr(p + 10));
        if (out.size() > 1 && out.front() == L'"' && out.back() == L'"') out = out.substr(1, out.size() - 2);
        return RunSelfTest(out.empty() ? L"selftest.txt" : out);
    }
    return RunApp(inst, cmd);
}
