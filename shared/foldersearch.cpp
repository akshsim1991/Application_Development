#include "foldersearch.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cwctype>
#ifndef _WIN32
#include <filesystem>
#include <system_error>
#endif
#include "document.hpp"

namespace foldersearch {
namespace {

std::string toUtf8(const std::wstring& w) { return platform::utf8(w); }
std::wstring fromUtf8(const std::string& s) { return platform::wide(s); }

#ifdef _WIN32
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
#else
void listPdfs(const std::wstring& dir, std::vector<std::wstring>& out, int depth) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(platform::utf8(dir), ec)) {
        const std::string name = e.path().filename().string();
        if (e.is_symlink(ec)) continue;
        if (e.is_directory(ec)) { if (depth < 8) listPdfs(platform::wide(e.path().string()), out, depth + 1); continue; }
        std::string ext = e.path().extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".pdf" && name.size() > 4) out.push_back(platform::wide(e.path().string()));
    }
}

// Size and modification time: what tells a changed file (as Windows' attribute data does there).
struct FileAttrs { unsigned long long size = 0, time = 0; };
bool attrsOf(const std::wstring& f, FileAttrs& a) {
    std::error_code ec;
    const std::filesystem::path p(platform::utf8(f));
    a.size = std::filesystem::file_size(p, ec);
    if (ec) return false;
    a.time = static_cast<unsigned long long>(std::filesystem::last_write_time(p, ec).time_since_epoch().count());
    return !ec;
}
#endif

#ifdef _WIN32
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
#else
std::wstring cacheName(const std::wstring& path, const FileAttrs& a) {
    unsigned long long h = 1469598103934665603ULL;
    auto mix = [&](unsigned long long v) { h ^= v; h *= 1099511628211ULL; };
    for (wchar_t c : path) mix(static_cast<unsigned long long>(c));
    mix(a.size); mix(a.time);
    wchar_t buf[24];
    swprintf(buf, 24, L"%016llx.txt", h);
    return buf;
}
#endif

bool readAll(const std::wstring& path, std::string& out) {
    std::vector<unsigned char> b;
    if (!platform::readFile(path, b)) return false;
    out.assign(b.begin(), b.end());
    return true;
}

} // namespace

std::vector<Hit> search(const std::wstring& folder, const std::wstring& query, bool matchCase, const std::wstring& cacheDir,
                        const std::function<bool(int, int, const std::wstring&)>& progress, Stats& stats, size_t maxHits) {
    std::vector<Hit> hits;
    std::vector<std::wstring> files;
    listPdfs(folder, files, 0);
    std::sort(files.begin(), files.end());
    stats.files = static_cast<int>(files.size());
    platform::makeDirectory(cacheDir);
    auto lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; };
    const std::wstring needle = matchCase ? query : lower(query);
    int done = 0;
    for (const std::wstring& f : files) {
        if (progress && !progress(done++, static_cast<int>(files.size()), f)) break;
#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA attr{};
        if (!GetFileAttributesExW(f.c_str(), GetFileExInfoStandard, &attr)) { ++stats.skipped; continue; }
#else
        FileAttrs attr;
        if (!attrsOf(f, attr)) { ++stats.skipped; continue; }
#endif
        const std::wstring cache = cacheDir + platform::pathSeparator() + cacheName(f, attr);
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
