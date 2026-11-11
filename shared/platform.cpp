#include "platform.hpp"
#ifndef _WIN32
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <filesystem>
#include <system_error>
#endif

namespace platform {

std::string utf8(const std::wstring& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t c = static_cast<uint32_t>(s[i]);
        if (sizeof(wchar_t) == 2 && c >= 0xD800 && c < 0xDC00 && i + 1 < s.size()) {   // UTF-16 pair (Windows)
            const uint32_t lo = static_cast<uint32_t>(s[i + 1]);
            if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); ++i; }
        }
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) { out += static_cast<char>(0xC0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

std::wstring wide(const std::string& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        uint32_t c = 0xFFFD;
        int n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + n > s.size()) { ++i; out += static_cast<wchar_t>(0xFFFD); continue; }
        c = n == 1 ? b : n == 2 ? (b & 0x1F) : n == 3 ? (b & 0x0F) : (b & 0x07);
        for (int k = 1; k < n; ++k) c = (c << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        i += n;
        if (sizeof(wchar_t) == 2 && c >= 0x10000) {
            c -= 0x10000;
            out += static_cast<wchar_t>(0xD800 + (c >> 10));
            out += static_cast<wchar_t>(0xDC00 + (c & 0x3FF));
        } else {
            out += static_cast<wchar_t>(c);
        }
    }
    return out;
}

bool fileExists(const std::wstring& path) {
#ifdef _WIN32
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st{};
    return stat(utf8(path).c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

bool readFile(const std::wstring& path, std::vector<unsigned char>& out) {
#ifdef _WIN32
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    out.assign(static_cast<size_t>(sz.QuadPart), 0);
    DWORD got = 0;
    const BOOL ok = out.empty() || ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
    CloseHandle(f);
    out.resize(got);
    return ok != FALSE;
#else
    FILE* f = std::fopen(utf8(path).c_str(), "rb");
    if (!f) return false;
    out.clear();
    unsigned char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    const bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
#endif
}

bool writeFile(const std::wstring& path, const std::vector<unsigned char>& data) {
#ifdef _WIN32
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const BOOL ok = data.empty() || WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &put, nullptr);
    CloseHandle(f);
    return ok && put == data.size();
#else
    FILE* f = std::fopen(utf8(path).c_str(), "wb");
    if (!f) return false;
    const bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return std::fclose(f) == 0 && ok;
#endif
}

std::wstring readTextFile(const std::wstring& path) {
    std::vector<unsigned char> raw;
    if (!readFile(path, raw)) return {};
    if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {   // UTF-16 little endian
        std::wstring w;
        for (size_t i = 2; i + 1 < raw.size(); i += 2) w += static_cast<wchar_t>(raw[i] | (raw[i + 1] << 8));
        return sizeof(wchar_t) == 2 ? w : wide(utf8(w));   // off Windows, join the surrogate pairs
    }
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()), nullptr, 0);
    UINT cp = CP_UTF8;
    if (n <= 0) { cp = CP_ACP; n = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()), nullptr, 0); }
    std::wstring w(n, L'\0');
    MultiByteToWideChar(cp, 0, reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()), w.data(), n);
    return w;
#else
    const std::string s(raw.begin(), raw.end());
    const std::wstring w = wide(s);
    if (w.find(static_cast<wchar_t>(0xFFFD)) == std::wstring::npos || s.find("\xEF\xBF\xBD") != std::string::npos) return w;
    return std::wstring(raw.begin(), raw.end());   // not UTF-8: Latin-1
#endif
}

std::wstring appDataDir() {
#ifdef _WIN32
    wchar_t base[MAX_PATH];
    if (GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH) == 0) return L"";
    std::wstring dir = std::wstring(base) + L"\\FeatherPDF";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
#else
    const char* home = std::getenv("HOME");
    if (!home) return L"";
#ifdef __APPLE__
    std::string dir = std::string(home) + "/Library/Application Support/Feather PDF";
#else
    std::string dir = std::string(home) + "/.config/feather-pdf";
#endif
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);   // with its parents (~/.config may not exist yet)
    return wide(dir);
#endif
}

bool makeDirectory(const std::wstring& path) {
#ifdef _WIN32
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st{};
    return mkdir(utf8(path).c_str(), 0755) == 0 || (stat(utf8(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

wchar_t pathSeparator() {
#ifdef _WIN32
    return L'\\';
#else
    return L'/';
#endif
}

} // namespace platform
