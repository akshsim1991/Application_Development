// Large-file finder and duplicate finder. Read-only scanning; removal goes to the Recycle Bin only.
#include "app.h"
#include "../core/text.h"
#include <shlobj.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <algorithm>
#include <map>
#include <set>

namespace fsu {
using std::wstring;

static wstring KF(REFKNOWNFOLDERID id) { PWSTR p = nullptr; wstring r; if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); } return r; }
std::vector<wstring> DefaultScanRoots() {
    std::vector<wstring> v; for (auto id : {FOLDERID_Downloads, FOLDERID_Documents, FOLDERID_Desktop, FOLDERID_Pictures, FOLDERID_Videos, FOLDERID_Music}) { wstring p = KF(id); if (!p.empty() && GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) v.push_back(p); }
    return v;
}
static ULONGLONG FtU(const FILETIME& f) { ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime; return u.QuadPart; }
wstring FileDateText(ULONGLONG ft) {
    FILETIME f, l; f.dwLowDateTime = (DWORD)ft; f.dwHighDateTime = (DWORD)(ft >> 32); SYSTEMTIME st; if (!FileTimeToLocalFileTime(&f, &l) || !FileTimeToSystemTime(&l, &st)) return L"";
    wchar_t b[32]; swprintf(b, 32, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute); return b;
}

struct Ctx { const FileScanOpts& o; std::vector<FileEntry> hits; ULONGLONG scanned = 0, lastReport = 0; std::set<wstring> seenRoots; wstring winDir, pfDir, pf86Dir; };
static bool Cancelled(Ctx& c) { return c.o.cancel && c.o.cancel->load(); }
static void Walk(Ctx& c, const wstring& dir, int depth) {
    if (depth > 40 || Cancelled(c)) return;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH); if (h == INVALID_HANDLE_VALUE) return;
    do {
        wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; wstring full = dir + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;                         // never follow junctions/symlinks/OneDrive placeholders
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wstring l = Lower(full); if (l == c.winDir || l == c.pfDir || l == c.pf86Dir) continue; Walk(c, full, depth + 1); if (Cancelled(c)) break; continue; }
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN)) continue;   // cloud placeholders are never downloaded by scanning
        ULONGLONG size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; ++c.scanned;
        if (c.o.progress && c.scanned - c.lastReport >= 2000) { c.lastReport = c.scanned; c.o.progress(L"Scanning\u2026 " + std::to_wstring(c.scanned) + L" files checked \u00B7 " + dir); }
        if (size < c.o.minSize || size == 0) continue;
        FileEntry e; e.path = full; e.name = n; e.folder = dir; e.size = size; e.mtime = FtU(fd.ftLastWriteTime); size_t dot = n.find_last_of(L'.'); e.ext = dot == wstring::npos ? L"" : Lower(n.substr(dot + 1));
        c.hits.push_back(std::move(e));
        if (!c.o.dupes && c.hits.size() > c.o.maxResults * 8) {   // keep memory bounded: retain the biggest
            std::nth_element(c.hits.begin(), c.hits.begin() + c.o.maxResults, c.hits.end(), [](const FileEntry& a, const FileEntry& b) { return a.size > b.size; }); c.hits.resize(c.o.maxResults); }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// ---- hashing (SHA-256, streaming) ----
static bool HashFile(const wstring& path, ULONGLONG limitHead, bool headTail, wstring& out, Ctx& c, ULONGLONG* fileId) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr); if (f == INVALID_HANDLE_VALUE) return false;
    if (fileId) { BY_HANDLE_FILE_INFORMATION bi{}; if (GetFileInformationByHandle(f, &bi)) *fileId = ((ULONGLONG)bi.dwVolumeSerialNumber << 48) ^ ((ULONGLONG)bi.nFileIndexHigh << 32) ^ bi.nFileIndexLow; }
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hh = nullptr; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 && BCryptCreateHash(alg, &hh, nullptr, 0, nullptr, 0, 0) == 0) {
        std::vector<BYTE> buf(1 << 20); DWORD n; ok = true; ULONGLONG done = 0;
        if (headTail) {   // cheap pre-filter: first 64 KB + last 64 KB
            LARGE_INTEGER sz{}; GetFileSizeEx(f, &sz); DWORD want = 65536; if (ReadFile(f, buf.data(), want, &n, nullptr) && n) BCryptHashData(hh, buf.data(), n, 0);
            if ((ULONGLONG)sz.QuadPart > 2 * 65536) { LARGE_INTEGER p; p.QuadPart = sz.QuadPart - 65536; SetFilePointerEx(f, p, nullptr, FILE_BEGIN); if (ReadFile(f, buf.data(), want, &n, nullptr) && n) BCryptHashData(hh, buf.data(), n, 0); }
        } else while (ReadFile(f, buf.data(), (DWORD)buf.size(), &n, nullptr) && n) { if (BCryptHashData(hh, buf.data(), n, 0)) { ok = false; break; } done += n; if (Cancelled(c)) { ok = false; break; } }
        BYTE d[32]; if (ok && BCryptFinishHash(hh, d, 32, 0) == 0) { out.clear(); for (BYTE b : d) { wchar_t t[3]; swprintf(t, 3, L"%02x", b); out += t; } } else ok = false;
    }
    (void)limitHead; if (hh) BCryptDestroyHash(hh); if (alg) BCryptCloseAlgorithmProvider(alg, 0); CloseHandle(f); return ok;
}

// ---- empty folders: a folder is "empty" only if it contains no files at all (hidden/system files count), no links, and only empty subfolders ----
struct ECtx { const FileScanOpts& o; ULONGLONG scanned = 0; wstring winDir, pfDir, pf86Dir; };
static bool EmptyRec(ECtx& c, const wstring& dir, int depth, std::vector<FileEntry>& out, int& nested) {
    nested = 0; if (depth > 40 || (c.o.cancel && c.o.cancel->load())) return false;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0); if (h == INVALID_HANDLE_VALUE) return false;   // unreadable: never assume empty
    bool files = false, anyNonEmpty = false; std::vector<FileEntry> emptyKids; int kidNested = 0;
    do {
        wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; wstring full = dir + L"\\" + n;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) { files = true; continue; }
        wstring l = Lower(full); if ((fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) || l == c.winDir || l == c.pfDir || l == c.pf86Dir) { files = true; continue; }   // protected: counts as content
        ++c.scanned; if (c.o.progress && c.scanned % 2000 == 0) c.o.progress(L"Scanning\u2026 " + std::to_wstring(c.scanned) + L" folders checked");
        int kn = 0; std::vector<FileEntry> sub; bool e = EmptyRec(c, full, depth + 1, sub, kn);
        if (e) { FileEntry fe; fe.path = full; fe.name = n; fe.folder = dir; fe.ext = L"folder"; fe.groupCount = kn; fe.mtime = FtU(fd.ftLastWriteTime); emptyKids.push_back(fe); kidNested += kn + 1; }
        else { anyNonEmpty = true; out.insert(out.end(), sub.begin(), sub.end()); }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!files && !anyNonEmpty) { nested = kidNested; return true; }       // whole tree empty: parent reports it, children are not listed separately
    out.insert(out.end(), emptyKids.begin(), emptyKids.end()); return false;
}
static void ScanEmpty(const FileScanOpts& o, FileScan& out) {
    ECtx c{o}; wchar_t b[MAX_PATH]; GetWindowsDirectoryW(b, MAX_PATH); c.winDir = Lower(b); c.pfDir = Lower(KF(FOLDERID_ProgramFiles)); c.pf86Dir = Lower(KF(FOLDERID_ProgramFilesX86));
    for (auto r : o.roots) { while (r.size() > 3 && r.back() == L'\\') r.pop_back(); int n = 0; std::vector<FileEntry> found; EmptyRec(c, r, 0, found, n); out.files.insert(out.files.end(), found.begin(), found.end()); if (o.cancel && o.cancel->load()) break; }   // the chosen root itself is never listed
    out.scanned = c.scanned; out.cancelled = o.cancel && o.cancel->load(); std::sort(out.files.begin(), out.files.end(), [](const FileEntry& a, const FileEntry& b2) { return Lower(a.path) < Lower(b2.path); });
    out.coverage = L"Checked " + std::to_wstring(c.scanned) + L" folders. A folder counts as empty only if it has no files at all (hidden and system files count as content), no links, and only empty subfolders. Windows, Program Files and system folders are never listed. Folders go to the Recycle Bin." + (out.cancelled ? L" Scan was stopped early." : L"");
}

void ScanFiles(const FileScanOpts& o, FileScan& out) {
    if (o.empty) { ScanEmpty(o, out); return; }
    Ctx c{o}; wchar_t b[MAX_PATH]; GetWindowsDirectoryW(b, MAX_PATH); c.winDir = Lower(b); c.pfDir = Lower(KF(FOLDERID_ProgramFiles)); c.pf86Dir = Lower(KF(FOLDERID_ProgramFilesX86));
    std::set<wstring> done; for (auto r : o.roots) { while (r.size() > 3 && r.back() == L'\\') r.pop_back(); wstring l = Lower(r); bool nested = false; for (auto& d : done) if (l.rfind(d + L"\\", 0) == 0 || l == d) nested = true; if (nested) continue; done.insert(l); Walk(c, r, 0); if (Cancelled(c)) break; }
    out.scanned = c.scanned; out.cancelled = Cancelled(c);
    if (!o.dupes) {
        std::sort(c.hits.begin(), c.hits.end(), [](const FileEntry& a, const FileEntry& b2) { return a.size > b2.size; }); if (c.hits.size() > o.maxResults) c.hits.resize(o.maxResults);
        for (auto& e : c.hits) out.totalBytes += e.size; out.files = std::move(c.hits);
        out.coverage = L"Checked " + std::to_wstring(c.scanned) + L" files in " + std::to_wstring(done.size()) + L" folder(s); showing the " + std::to_wstring(out.files.size()) + L" largest at least " + FormatBytes(o.minSize) + L". Skipped: Windows and Program Files folders, links/junctions, system files and cloud placeholders (never downloaded)." + (out.cancelled ? L" Scan was stopped early." : L"");
        return;
    }
    // duplicates: size -> head/tail hash -> full hash
    std::map<ULONGLONG, std::vector<size_t>> bySize; for (size_t i = 0; i < c.hits.size(); ++i) bySize[c.hits[i].size].push_back(i);
    std::vector<size_t> cand; for (auto& kv : bySize) if (kv.second.size() > 1) cand.insert(cand.end(), kv.second.begin(), kv.second.end());
    std::map<wstring, std::vector<size_t>> byPart; size_t k = 0;
    for (size_t i : cand) { if (Cancelled(c)) break; if (o.progress && (++k % 25 == 0)) o.progress(L"Comparing\u2026 " + std::to_wstring(k) + L" of " + std::to_wstring(cand.size()) + L" candidates"); wstring hsh; if (HashFile(c.hits[i].path, 0, true, hsh, c, nullptr)) byPart[std::to_wstring(c.hits[i].size) + L"|" + hsh].push_back(i); }
    std::map<wstring, std::vector<size_t>> byFull; k = 0; std::vector<ULONGLONG> ids(c.hits.size(), 0);
    for (auto& kv : byPart) { if (kv.second.size() < 2) continue; for (size_t i : kv.second) { if (Cancelled(c)) break; if (o.progress && (++k % 5 == 0)) o.progress(L"Verifying\u2026 " + c.hits[i].name); wstring hsh; ULONGLONG id = 0; if (HashFile(c.hits[i].path, 0, false, hsh, c, &id)) { byFull[hsh].push_back(i); ids[i] = id; } } }
    int g = 0; out.files.clear();
    for (auto& kv : byFull) {
        std::vector<size_t> uniq; std::set<ULONGLONG> seenId; for (size_t i : kv.second) { if (ids[i] && !seenId.insert(ids[i]).second) continue; uniq.push_back(i); }   // hard links to the same data are not duplicates
        if (uniq.size() < 2) continue; ++g; std::sort(uniq.begin(), uniq.end(), [&](size_t a, size_t b2) { return c.hits[a].mtime < c.hits[b2].mtime; });
        for (size_t i : uniq) { FileEntry e = c.hits[i]; e.group = g; e.groupCount = (int)uniq.size(); out.files.push_back(std::move(e)); }
        out.reclaimable += c.hits[uniq[0]].size * (uniq.size() - 1); out.totalBytes += c.hits[uniq[0]].size * uniq.size();
    }
    std::stable_sort(out.files.begin(), out.files.end(), [](const FileEntry& a, const FileEntry& b2) { return a.group < b2.group; });
    out.groups = g; out.cancelled = Cancelled(c);
    out.coverage = L"Checked " + std::to_wstring(c.scanned) + L" files; " + std::to_wstring(cand.size()) + L" had a same-size twin. Duplicates are byte-for-byte identical (SHA-256), hard links are not counted, and at least one copy per group is always kept." + (out.cancelled ? L" Scan was stopped early; results may be incomplete." : L"");
}

bool DeleteFilesPermanently(const std::vector<wstring>& paths, wstring& err) {
    int failed = 0; for (auto& p : paths) {
        DWORD a = GetFileAttributesW(p.c_str()); if (a == INVALID_FILE_ATTRIBUTES) continue; if (a & FILE_ATTRIBUTE_REPARSE_POINT) { ++failed; continue; }   // never follow links
        bool ok = (a & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(p.c_str()) != 0 : (SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL), DeleteFileW(p.c_str()) != 0);   // folders are removed only if still empty
        if (!ok) ++failed;
    }
    if (failed) err = std::to_wstring(failed) + L" item(s) could not be deleted (in use, protected, or a folder that is no longer empty)."; return failed == 0;
}
bool RecycleFiles(HWND owner, const std::vector<wstring>& paths, wstring& err) {
    if (paths.empty()) return true; wstring list; for (auto& p : paths) { list += p; list.push_back(L'\0'); } list.push_back(L'\0');
    SHFILEOPSTRUCTW op{}; op.hwnd = owner; op.wFunc = FO_DELETE; op.pFrom = list.c_str(); op.fFlags = FOF_ALLOWUNDO | FOF_WANTNUKEWARNING | FOF_NOERRORUI;   // Windows itself warns if a file would be destroyed instead of recycled
    int r = SHFileOperationW(&op); if (r != 0 || op.fAnyOperationsAborted) { err = L"Windows could not move some files to the Recycle Bin (code " + std::to_wstring(r) + L"). Large files may exceed the Recycle Bin size limit."; return false; }
    return true;
}

} // namespace fsu
