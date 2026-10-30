#include "foldersearch.hpp"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include "document.hpp"

namespace foldersearch {
namespace {

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

void listPdfs(const std::wstring& dir, std::vector<std::wstring>& out, int depth) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 8 && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) listPdfs(dir + L"\\" + name, out, depth + 1);
        } else if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".pdf") == 0) out.push_back(dir + L"\\" + name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

std::wstring cacheName(const std::wstring& path, const WIN32_FILE_ATTRIBUTE_DATA& a) {
    std::wstring lo = path;
    for (auto& c : lo) c = static_cast<wchar_t>(towlower(c));
    unsigned long long h = 1469598103934665603ULL;
    auto mix = [&](unsigned long long v) { h ^= v; h *= 1099511628211ULL; };
    for (wchar_t c : lo) mix(c);
    mix(a.nFileSizeLow); mix(a.nFileSizeHigh); mix(a.ftLastWriteTime.dwLowDateTime); mix(a.ftLastWriteTime.dwHighDateTime);
    wchar_t buf[24];
    swprintf(buf, 24, L"%016llx.txt", h);
    return buf;
}

bool readAll(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    out.resize(static_cast<size_t>(sz.QuadPart));
    DWORD got = 0;
    bool ok = out.empty() || (ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size());
    CloseHandle(f);
    return ok;
}

} // namespace

std::vector<Hit> search(const std::wstring& folder, const std::wstring& query, bool matchCase, const std::wstring& cacheDir,
                        const std::function<bool(int, int, const std::wstring&)>& progress, Stats& stats, size_t maxHits) {
    std::vector<Hit> hits;
    std::vector<std::wstring> files;
    listPdfs(folder, files, 0);
    std::sort(files.begin(), files.end());
    stats.files = static_cast<int>(files.size());
    CreateDirectoryW(cacheDir.c_str(), nullptr);
    auto lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; };
    const std::wstring needle = matchCase ? query : lower(query);
    int done = 0;
    for (const std::wstring& f : files) {
        if (progress && !progress(done++, static_cast<int>(files.size()), f)) break;
        WIN32_FILE_ATTRIBUTE_DATA attr{};
        if (!GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &attr)) { ++stats.skipped; continue; }
        const std::wstring cache = cacheDir + L"\\" + cacheName(f, attr);
        std::vector<std::wstring> pages;
        std::string raw;
        if (readAll(cache, raw)) {
            std::string cur;
            for (char c : raw) { if (c == '\f') { pages.push_back(fromUtf8(cur)); cur.clear(); } else cur += c; }
            pages.push_back(fromUtf8(cur));
            ++stats.cached;
        } else {
            Document d;
            if (d.open(f) != OpenResult::Ok) { ++stats.skipped; continue; }   // damaged or password-protected
            std::string blob;
            for (int p = 0; p < d.pageCount(); ++p) {
                std::wstring t = d.textRange(p, 0, d.charCount(p));
                for (auto& c : t) if (c == L'\f') c = L' ';
                pages.push_back(t);
                if (p) blob += '\f';
                blob += toUtf8(t);
            }
            Document::writeFile(cache, std::vector<unsigned char>(blob.begin(), blob.end()));
            ++stats.scanned;
        }
        for (size_t p = 0; p < pages.size() && hits.size() < maxHits; ++p) {
            const std::wstring hay = matchCase ? pages[p] : lower(pages[p]);
            size_t pos = 0, found;
            int perPage = 0;
            while ((found = hay.find(needle, pos)) != std::wstring::npos && perPage < 5 && hits.size() < maxHits) {
                const size_t from = found > 40 ? found - 40 : 0;
                std::wstring snip = pages[p].substr(from, std::min<size_t>(120, pages[p].size() - from));
                for (auto& c : snip) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
                hits.push_back({f, static_cast<int>(p), snip});
                pos = found + needle.size();
                ++perPage;
            }
        }
    }
    return hits;
}

} // namespace foldersearch
