#include "pdfium_api.hpp"
#include "platform.hpp"
#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <climits>
#include <cstdlib>
#ifdef __APPLE__
#include <TargetConditionals.h>
#include <mach-o/dyld.h>
#endif
#endif

namespace pdfium {
namespace {
Api g_api{};

#ifdef _WIN32
template <typename T>
bool bind(HMODULE dll, const char* name, T& out) {
    out = reinterpret_cast<T>(GetProcAddress(dll, name));
    return out != nullptr;
}
#else
template <typename T>
bool bind(void* lib, const char* name, T& out) {
    out = reinterpret_cast<T>(dlsym(lib, name));
    return out != nullptr;
}

// The folder of the running program.
std::string programDir() {
    char buf[PATH_MAX] = {};
#ifdef __APPLE__
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return "";
#else
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = 0;
#endif
    char real[PATH_MAX] = {};
    std::string p = realpath(buf, real) ? real : buf;
    p.erase(p.find_last_of('/') + 1);
    return p;
}
#endif
} // namespace

const Api& api() { return g_api; }

bool load(std::wstring& error) {
#ifdef _WIN32
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
    const wchar_t* libName = L"pdfium.dll";
#else
    // macOS: Feather PDF.app/Contents/MacOS/<program> and Contents/Frameworks/libpdfium.dylib.
    // Elsewhere (the engine check): libpdfium.so next to the program, or FEATHER_PDFIUM.
    const std::string dir = programDir();
    std::vector<std::string> tries;
    if (const char* env = std::getenv("FEATHER_PDFIUM")) tries.push_back(env);
#if defined(__APPLE__) && TARGET_OS_IPHONE
    tries.push_back(dir + "Frameworks/pdfium.framework/pdfium");   // iPhone: Feather PDF.app/Frameworks
#elif defined(__APPLE__)
    tries.push_back(dir + "../Frameworks/libpdfium.dylib");
    tries.push_back(dir + "libpdfium.dylib");
#elif defined(__ANDROID__)
    tries.push_back("libpdfium.so");   // the app's own libraries (lib/<abi>/ in the APK): the linker finds it by name
#else
    tries.push_back(dir + "libpdfium.so");
#endif
    void* dll = nullptr;
    for (const std::string& t : tries) if ((dll = dlopen(t.c_str(), RTLD_NOW | RTLD_LOCAL)) != nullptr) break;
    if (!dll) {
        error = L"The PDF library (libpdfium) could not be loaded from:\n" + platform::wide(dir) +
                L"\n\nThe file is missing or damaged. Reinstall Feather PDF to restore it.";
        return false;
    }
    const wchar_t* libName = L"The PDF library (libpdfium)";
#endif
    Api a{};
    bool ok = true;
#define PDFIUM_BIND(name) ok = bind(dll, #name, a.name) && ok;
    PDFIUM_FUNCS(PDFIUM_BIND)
    PDFIUM_FUNCS_SYSTEM(PDFIUM_BIND)
#undef PDFIUM_BIND
    if (!ok) {
        error = std::wstring(libName) + L" is not the version Feather PDF expects (functions are missing).\n"
                L"Reinstall Feather PDF to restore the right file.";
        return false;
    }
    g_api = a;
    g_api.FPDF_InitLibrary();
    return true;
}

} // namespace pdfium
