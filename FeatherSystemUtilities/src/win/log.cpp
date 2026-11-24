// Local-only diagnostics log (DataDir\app.log, rotated at 1 MB) and a crash filter that records where an unhandled exception happened.
#include "app.h"
#include "../core/text.h"
#include <mutex>
#include <fstream>
#include <filesystem>
#include <dbghelp.h>

namespace fsu {
using std::wstring;
static std::mutex g_lm;
void Log(const char* level, const wstring& text) {
    std::lock_guard<std::mutex> l(g_lm); wstring p = DataDir() + L"\\app.log"; WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow > 1024 * 1024) { MoveFileExW(p.c_str(), (p + L".1").c_str(), MOVEFILE_REPLACE_EXISTING); }
    std::ofstream f(std::filesystem::path(p), std::ios::app | std::ios::binary); if (f) f << ToUtf8(NowIsoUtc()) << " " << level << " " << ToUtf8(text.substr(0, 4000)) << "\n";
}
std::vector<std::string> LogTail(size_t n) {
    std::vector<std::string> v; std::ifstream f(std::filesystem::path(DataDir() + L"\\app.log"), std::ios::binary); std::string l; while (std::getline(f, l)) { if (!l.empty() && l.back() == '\r') l.pop_back(); v.push_back(l); if (v.size() > n * 2) v.erase(v.begin(), v.begin() + n); }
    if (v.size() > n) v.erase(v.begin(), v.end() - n); return v;
}
void LogClear() { std::lock_guard<std::mutex> l(g_lm); DeleteFileW((DataDir() + L"\\app.log").c_str()); DeleteFileW((DataDir() + L"\\app.log.1").c_str()); }
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    wchar_t b[256]; HMODULE m = nullptr; wchar_t mod[MAX_PATH] = L"?"; ULONG_PTR off = 0; void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    if (addr && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &m)) { GetModuleFileNameW(m, mod, MAX_PATH); off = (ULONG_PTR)addr - (ULONG_PTR)m; }
    swprintf(b, 256, L"CRASH code 0x%08lX at %ls+0x%llX", ep && ep->ExceptionRecord ? (unsigned long)ep->ExceptionRecord->ExceptionCode : 0UL, wcsrchr(mod, L'\\') ? wcsrchr(mod, L'\\') + 1 : mod, (unsigned long long)off); wstring msg = b;
    void* frames[16]; USHORT n = CaptureStackBackTrace(0, 16, frames, nullptr); for (USHORT i = 0; i < n; ++i) { HMODULE fm = nullptr; if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)frames[i], &fm)) { wchar_t fb[64]; swprintf(fb, 64, L" %llX", (unsigned long long)((ULONG_PTR)frames[i] - (ULONG_PTR)fm)); msg += fb; } }
    msg += L" (version " FSU_VERSION L")"; Log("FATAL", msg); return EXCEPTION_CONTINUE_SEARCH;   // let Windows' normal error handling proceed after we recorded it
}
void InstallCrashLog() { SetUnhandledExceptionFilter(CrashFilter); Log("INFO", L"Feather " FSU_VERSION L" started"); }
}
