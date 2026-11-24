// A stand-in FeatherPDF.exe for tests/windows/menucheck.cpp: copies the list it is given with --combine-list to
// combine-received.txt next to itself.
#include <windows.h>
#include <string>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 3 || std::wstring(argv[1]) != L"--combine-list") return 2;
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring out(self);
    out = out.substr(0, out.find_last_of(L'\\') + 1) + L"combine-received.txt";
    return CopyFileW(argv[2], out.c_str(), FALSE) ? 0 : 3;
}
