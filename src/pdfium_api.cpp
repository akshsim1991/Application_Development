#include "pdfium_api.hpp"
#include <windows.h>

namespace pdfium {
namespace {
Api g_api{};

template <typename T>
bool bind(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(GetProcAddress(dll, name));
    return out != nullptr;
}
} // namespace

const Api& api() { return g_api; }

bool load(std::wstring& error) {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe);
    dir.erase(dir.find_last_of(L"\\/") + 1);
    const std::wstring path = dir + L"pdfium.dll";

    HMODULE dll = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) {
        error = L"pdfium.dll could not be loaded from:\n" + dir +
                L"\n\nThe file is missing or damaged. Reinstall Feather PDF to restore it.";
        return false;
    }
    Api a{};
    bool ok = true;
#define PDFIUM_BIND(name) ok = bind(dll, #name, a.name) && ok;
    PDFIUM_FUNCS(PDFIUM_BIND)
#undef PDFIUM_BIND
    if (!ok) {
        error = L"pdfium.dll is not the version Feather PDF expects (functions are missing).\n"
                L"Reinstall Feather PDF to restore the right file.";
        return false;
    }
    g_api = a;
    g_api.FPDF_InitLibrary();
    return true;
}

} // namespace pdfium
