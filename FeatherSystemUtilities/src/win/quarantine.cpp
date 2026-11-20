// Quarantine: a restorable holding area (DataDir\quarantine) for leftovers and stray files. Nothing here is deleted until the user purges it.
#include "app.h"
#include "json.h"
#include "../core/text.h"
#include <shellapi.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <atomic>

namespace fsu {
using std::wstring;

static wstring QDir() { return SubDir(L"quarantine"); }
static wstring IndexPath() { return QDir() + L"\\index.jsonl"; }
static ULONGLONG SizeOf(const wstring& p, int depth = 0) {
    WIN32_FILE_ATTRIBUTE_DATA fa; if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa)) return 0; if (!(fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; if (depth > 40) return 0;
    ULONGLONG t = 0; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((p + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0); if (h == INVALID_HANDLE_VALUE) return 0;
    do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) t += SizeOf(p + L"\\" + n, depth + 1); else t += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; } while (FindNextFileW(h, &fd)); FindClose(h); return t;
}
static bool ShellMove(const wstring& from, const wstring& to, wstring& err) {   // handles folders and other volumes
    wstring f = from; f.push_back(L'\0'); wstring t = to; t.push_back(L'\0');
    SHFILEOPSTRUCTW op{}; op.wFunc = FO_MOVE; op.pFrom = f.c_str(); op.pTo = t.c_str(); op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMMKDIR;
    int r = SHFileOperationW(&op); if (r != 0 || op.fAnyOperationsAborted) { err = L"Windows could not move it (code " + std::to_wstring(r) + L"). It may be in use or protected."; return false; } return true;
}
static std::vector<std::string> ReadIndex() { std::vector<std::string> v; std::ifstream f(std::filesystem::path(IndexPath()), std::ios::binary); std::string l; while (std::getline(f, l)) { if (!l.empty() && l.back() == '\r') l.pop_back(); if (!l.empty()) v.push_back(l); } return v; }
static bool WriteIndexWithout(const wstring& id) {
    std::string out; for (auto& l : ReadIndex()) { js::V v; if (js::Parse(l, v) && v.str("id") == ToUtf8(id)) continue; out += l + "\n"; } return AtomicWrite(IndexPath(), out);
}
static std::atomic<unsigned> g_seq{0};

bool QuarantinePath(const wstring& path, const wstring& source, wstring& err, bool journal) {
    DWORD at = GetFileAttributesW(path.c_str()); if (at == INVALID_FILE_ATTRIBUTES) { err = L"not found"; return false; }
    if (at & FILE_ATTRIBUTE_REPARSE_POINT) { err = L"links are never moved"; return false; }
    if (IsProtectedPath(path)) { err = L"protected location"; return false; }
    wstring qd = QDir(); if (_wcsnicmp(path.c_str(), qd.c_str(), qd.size()) == 0) { err = L"already in quarantine"; return false; }
    bool dir = (at & FILE_ATTRIBUTE_DIRECTORY) != 0; size_t sl = path.find_last_of(L'\\'); wstring leaf = sl == wstring::npos ? path : path.substr(sl + 1);
    char idb[40]; snprintf(idb, sizeof idb, "%llx-%x", (unsigned long long)GetTickCount64() ^ ((unsigned long long)GetCurrentProcessId() << 32), ++g_seq); wstring id = FromUtf8(idb);
    wstring folder = qd + L"\\items\\" + id; SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr); wstring stored = folder + L"\\" + leaf; ULONGLONG bytes = SizeOf(path);
    if (!ShellMove(path, stored, err)) { RemoveDirectoryW(folder.c_str()); return false; }
    std::ofstream f(std::filesystem::path(IndexPath()), std::ios::app | std::ios::binary);
    f << js::Obj().str("id", idb).str("orig", ToUtf8(path)).str("stored", ToUtf8(stored)).str("source", ToUtf8(source)).str("time", ToUtf8(NowIsoUtc())).num("bytes", (double)bytes).boolean("dir", dir).done() << "\n";
    if (journal) WriteJournal(source, path, L"present", L"moved to quarantine", L"ok (restorable from Recovery)", true); return true;
}
std::vector<QItem> QuarantineList() {
    std::vector<QItem> out; for (auto& l : ReadIndex()) { js::V v; if (!js::Parse(l, v) || v.t != js::V::Obj) continue; QItem q; q.id = FromUtf8(v.str("id")); q.orig = FromUtf8(v.str("orig")); q.stored = FromUtf8(v.str("stored")); q.source = FromUtf8(v.str("source")); q.time = FromUtf8(v.str("time")); q.bytes = (ULONGLONG)v.num("bytes"); q.isDir = v.boolean("dir");
        if (!q.id.empty() && GetFileAttributesW(q.stored.c_str()) != INVALID_FILE_ATTRIBUTES) out.push_back(q); } return out;
}
static bool Find(const wstring& id, QItem& q) { for (auto& x : QuarantineList()) if (x.id == id) { q = x; return true; } return false; }
bool QuarantineRestore(const wstring& id, wstring& err) {
    QItem q; if (!Find(id, q)) { err = L"That item is no longer in quarantine."; return false; }
    if (GetFileAttributesW(q.orig.c_str()) != INVALID_FILE_ATTRIBUTES) { err = L"Something already exists at the original location:\r\n" + q.orig; return false; }
    if (IsProtectedPath(q.orig)) { err = L"The original location is protected."; return false; }
    size_t sl = q.orig.find_last_of(L'\\'); if (sl != wstring::npos) SHCreateDirectoryExW(nullptr, q.orig.substr(0, sl).c_str(), nullptr);
    if (!ShellMove(q.stored, q.orig, err)) return false; RemoveDirectoryW((QDir() + L"\\items\\" + id).c_str()); WriteIndexWithout(id);
    WriteJournal(L"Quarantine restore", q.orig, L"in quarantine", L"restored", L"ok", false); return true;
}
bool QuarantinePurge(const wstring& id, wstring& err) {
    QItem q; if (!Find(id, q)) { err = L"That item is no longer in quarantine."; return false; }
    wstring folder = QDir() + L"\\items\\" + id; if (_wcsnicmp(q.stored.c_str(), folder.c_str(), folder.size()) != 0) { err = L"Unexpected location; not deleted."; return false; }   // only ever deletes inside our own folder
    wstring f = folder; f.push_back(L'\0'); SHFILEOPSTRUCTW op{}; op.wFunc = FO_DELETE; op.pFrom = f.c_str(); op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int r = SHFileOperationW(&op); if (r != 0 || op.fAnyOperationsAborted) { err = L"Could not delete it (code " + std::to_wstring(r) + L")."; return false; } WriteIndexWithout(id);
    WriteJournal(L"Quarantine purge", q.orig, L"in quarantine", L"deleted permanently", L"ok (not undoable)", false); return true;
}
}
