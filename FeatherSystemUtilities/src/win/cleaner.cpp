// Cleaner: temporary-file categories and conservative invalid-registry detection.
// Temp files are deleted permanently (locked/in-use files are skipped). Registry removals are exported first and restorable.
#include "app.h"
#include "../core/text.h"
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <fstream>
#include <filesystem>
#include <set>
#include <algorithm>

namespace fsu {
using std::wstring;

static wstring KnownFolder(REFKNOWNFOLDERID id) { PWSTR p = nullptr; wstring r; if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); } return r; }
static bool IsReparse(DWORD a) { return (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }
static bool FixedLocal(const wstring& p) { if (!IsDriveAbsolute(p)) return false; wchar_t root[4] = {p[0], L':', L'\\', 0}; return GetDriveTypeW(root) == DRIVE_FIXED; }
static ULONGLONG FtU(const FILETIME& f) { ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime; return u.QuadPart; }

// ---- temp walking ----
struct Walk { ULONGLONG bytes = 0, files = 0, skipped = 0, failed = 0; bool del = false; ULONGLONG cutoff = 0; int depth = 0; const wchar_t* pattern = nullptr; };
static void WalkDir(const wstring& dir, Walk& w, int depth) {
    if (depth > 24) return;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return;
    do {
        wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; wstring full = dir + L"\\" + n;
        if (IsReparse(fd.dwFileAttributes)) { ++w.skipped; continue; }                       // never follow junctions/symlinks
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { WalkDir(full, w, depth + 1); if (w.del && !w.pattern) RemoveDirectoryW(full.c_str()); continue; }   // fails harmlessly if not empty
        if (w.pattern && !PathMatchSpecW(n.c_str(), w.pattern)) continue;
        ULONGLONG size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        if (w.cutoff && FtU(fd.ftLastWriteTime) > w.cutoff) { ++w.skipped; continue; }       // recent files may be in use
        if (!w.del) { w.bytes += size; ++w.files; continue; }
        SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
        if (DeleteFileW(full.c_str())) { w.bytes += size; ++w.files; } else ++w.failed;      // in use / denied: skipped, not forced
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static ULONGLONG CutoffFor(bool keepRecent) { if (!keepRecent) return 0; FILETIME ft; GetSystemTimeAsFileTime(&ft); return FtU(ft) - 24ULL * 3600 * 10000000ULL; }

// ---- scan ----
struct Spec { const wchar_t* id; wstring name; std::vector<wstring> roots; bool admin = false; int risk = 0; wstring why; wstring pattern; int minAgeDays = 0; bool ignoreRecent = false; std::vector<wstring> services; const wchar_t* browserExe = nullptr; };
static ULONGLONG CutoffFor(bool keepRecent);
static ULONGLONG ItemCutoff(bool keep, bool ignoreRecent, int minAgeDays) {
    ULONGLONG c = (keep && !ignoreRecent) ? CutoffFor(true) : 0; if (minAgeDays > 0) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULONGLONG a = FtU(ft) - (ULONGLONG)minAgeDays * 24 * 3600 * 10000000ULL; c = c ? std::min(c, a) : a; } return c;
}
static void AddTemp(CleanScan& cs, const Spec& sp, bool keep) {
    CleanItem it; it.id = sp.id; it.name = sp.name; it.type = L"Temporary files"; it.kind = 0; it.needsAdmin = sp.admin; it.why = sp.why; it.risk = sp.risk; it.services = sp.services; it.pattern = sp.pattern; it.minAgeDays = sp.minAgeDays; it.ignoreRecent = sp.ignoreRecent;
    Walk w; w.cutoff = ItemCutoff(keep, sp.ignoreRecent, sp.minAgeDays); w.pattern = sp.pattern.empty() ? nullptr : sp.pattern.c_str();
    for (auto& r : sp.roots) { if (r.empty()) continue; DWORD a = GetFileAttributesW(r.c_str()); if (a == INVALID_FILE_ATTRIBUTES) continue; it.paths.push_back(r);
        if (a & FILE_ATTRIBUTE_DIRECTORY) WalkDir(r, w, 0); else { WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(r.c_str(), GetFileExInfoStandard, &fa) && !(w.cutoff && FtU(fa.ftLastWriteTime) > w.cutoff)) { w.bytes += ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; ++w.files; } } }
    if (it.paths.empty()) return;
    it.bytes = w.bytes; it.files = w.files; it.location = it.paths[0]; for (size_t i = 1; i < it.paths.size() && i < 4; ++i) it.location += L"; " + it.paths[i];
    it.detail = std::to_wstring(w.files) + L" files"; if (sp.admin) it.detail += L" \u00B7 needs administrator";
    if (sp.browserExe && IsProcessRunning(sp.browserExe)) { it.detail += L" \u00B7 browser is running"; it.why += L" The browser is running now: close it first or its files will be skipped."; }
    if (!sp.services.empty()) it.why += L" The related Windows services are stopped briefly and then restarted.";
    cs.totalBytes += it.bytes; cs.items.push_back(std::move(it));
}
static std::vector<wstring> FirefoxCaches(const wstring& local) {
    std::vector<wstring> v; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((local + L"\\Mozilla\\Firefox\\Profiles\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return v;
    do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) v.push_back(local + L"\\Mozilla\\Firefox\\Profiles\\" + fd.cFileName + L"\\cache2"); } while (FindNextFileW(h, &fd) && v.size() < 20); FindClose(h); return v;
}
static ULONGLONG DirSize(const wstring& d, ULONGLONG& files) { Walk w; WalkDir(d, w, 0); files = w.files; return w.bytes; }

static wstring RegStrVal(HKEY k, const wchar_t* name) { wchar_t b[2048]; DWORD sz = sizeof b - 2, ty = 0; if (RegQueryValueExW(k, name, nullptr, &ty, (LPBYTE)b, &sz) == ERROR_SUCCESS && (ty == REG_SZ || ty == REG_EXPAND_SZ)) { b[sz / 2] = 0; return b; } return L""; }
static DWORD RegDword(HKEY k, const wchar_t* name) { DWORD v = 0, sz = 4, ty = 0; if (RegQueryValueExW(k, name, nullptr, &ty, (LPBYTE)&v, &sz) == ERROR_SUCCESS && ty == REG_DWORD) return v; return 0; }
static wstring Unquote(wstring s) { while (!s.empty() && (s.front() == L' ' || s.front() == L'"')) s.erase(s.begin()); size_t q = s.find(L'"'); if (q != wstring::npos) s.resize(q); return s; }
static bool Missing(const wstring& expanded) { return FixedLocal(expanded) && GetFileAttributesW(expanded.c_str()) == INVALID_FILE_ATTRIBUTES; }
static const wchar_t* RootName(HKEY r) { return r == HKEY_CURRENT_USER ? L"HKCU" : L"HKLM"; }

static void ScanUninstall(CleanScan& cs, HKEY root, REGSAM view, const wstring& win) {
    const wchar_t* base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall"; HKEY k; if (RegOpenKeyExW(root, base, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0, n = 0; n < 3000; ++i, ++n) {
        wchar_t sub[512]; DWORD l = 512; if (RegEnumKeyExW(k, i, sub, &l, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        wstring sn = sub; if (sn.empty() || sn[0] == L'{' || sn.rfind(L"KB", 0) == 0) continue;               // MSI products / updates: leave to Windows Installer
        HKEY s; if (RegOpenKeyExW(k, sub, 0, KEY_QUERY_VALUE | view, &s) != ERROR_SUCCESS) continue;
        wstring us = RegStrVal(s, L"UninstallString"), disp = RegStrVal(s, L"DisplayName"), loc = RegStrVal(s, L"InstallLocation"); bool sys = RegDword(s, L"SystemComponent") == 1 || RegDword(s, L"WindowsInstaller") == 1 || !RegStrVal(s, L"ParentKeyName").empty(); RegCloseKey(s);
        if (sys || us.empty() || Lower(us).find(L"msiexec") != wstring::npos) continue;
        auto pc = ParseCommand(ExpandEnv(us), [](const wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; });
        wstring exe = pc.exe; if (!Missing(exe) || Lower(exe).rfind(win, 0) == 0) continue;
        wstring le = ExpandEnv(loc); if (!le.empty() && !(Missing(le + L"\\") || (FixedLocal(le) && GetFileAttributesW(le.c_str()) == INVALID_FILE_ATTRIBUTES))) continue;   // install folder still exists: keep
        CleanItem it; it.id = L"u|" + wstring(RootName(root)) + L"|" + std::to_wstring(view) + L"|" + sn; it.name = disp.empty() ? sn : disp; it.type = L"Registry: leftover uninstall entry"; it.kind = 2; it.root = root; it.view = view;
        it.subKey = wstring(base) + L"\\" + sn; it.location = wstring(RootName(root)) + L"\\" + it.subKey; it.why = L"Uninstaller not found: " + exe; it.needsAdmin = root == HKEY_LOCAL_MACHINE; it.detail = L"key"; it.risk = 1;
        cs.items.push_back(std::move(it)); ++cs.registryCount;
    }
    RegCloseKey(k);
}
static void ScanAppPaths(CleanScan& cs, HKEY root, REGSAM view, const wstring& win) {
    const wchar_t* base = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths"; HKEY k; if (RegOpenKeyExW(root, base, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0, n = 0; n < 3000; ++i, ++n) {
        wchar_t sub[512]; DWORD l = 512; if (RegEnumKeyExW(k, i, sub, &l, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY s; if (RegOpenKeyExW(k, sub, 0, KEY_QUERY_VALUE | view, &s) != ERROR_SUCCESS) continue; wstring def = RegStrVal(s, nullptr); RegCloseKey(s);
        wstring exe = ExpandEnv(Unquote(def)); if (exe.empty() || !Missing(exe) || Lower(exe).rfind(win, 0) == 0) continue;
        CleanItem it; it.id = L"a|" + wstring(RootName(root)) + L"|" + std::to_wstring(view) + L"|" + sub; it.name = sub; it.type = L"Registry: broken App Paths entry"; it.kind = 2; it.root = root; it.view = view;
        it.subKey = wstring(base) + L"\\" + sub; it.location = wstring(RootName(root)) + L"\\" + it.subKey; it.why = L"Program not found: " + exe; it.needsAdmin = root == HKEY_LOCAL_MACHINE; it.detail = L"key"; it.risk = 1;
        cs.items.push_back(std::move(it)); ++cs.registryCount;
    }
    RegCloseKey(k);
}
static void ScanValuesAsPaths(CleanScan& cs, HKEY root, REGSAM view, const wchar_t* base, const wchar_t* tag, const wchar_t* type, bool mui, const wstring& win) {
    HKEY k; if (RegOpenKeyExW(root, base, 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0, n = 0; n < 4000; ++i, ++n) {
        wchar_t name[1024]; DWORD l = 1024; if (RegEnumValueW(k, i, name, &l, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        wstring path; if (mui) { if (!MuiCachePath(name, path)) continue; } else path = name;
        if (!Missing(path) || Lower(path).rfind(win, 0) == 0) continue;
        CleanItem it; it.id = wstring(tag) + L"|" + RootName(root) + L"|" + std::to_wstring(view) + L"|" + name; it.name = path; it.type = type; it.kind = 3; it.root = root; it.view = view; it.subKey = base; it.valueName = name;
        it.location = wstring(RootName(root)) + L"\\" + base; it.why = L"File not found"; it.needsAdmin = root == HKEY_LOCAL_MACHINE; it.detail = L"value"; it.risk = 1;
        cs.items.push_back(std::move(it)); ++cs.registryCount;
    }
    RegCloseKey(k);
}

CleanScan ScanCleaner(bool keep) {
    CleanScan cs; wstring local = KnownFolder(FOLDERID_LocalAppData), win = KnownFolder(FOLDERID_Windows), pd = KnownFolder(FOLDERID_ProgramData); wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); wstring t = tmp; while (t.size() > 3 && t.back() == L'\\') t.pop_back();
    wstring winLow = Lower(win);
    wstring appdata = KnownFolder(FOLDERID_RoamingAppData);
    AddTemp(cs, {L"t.user", L"User temporary files", {t}, false, 0, L"Files apps left in your temp folder. Locked files are skipped" + wstring(keep ? L"; files from the last 24 hours are kept." : L".")}, keep);
    AddTemp(cs, {L"t.win", L"Windows temporary files", {win + L"\\Temp"}, true, 0, L"Windows and installer leftovers in C:\\Windows\\Temp."}, keep);
    AddTemp(cs, {L"t.wu", L"Windows Update download cache", {win + L"\\SoftwareDistribution\\Download"}, true, 0, L"Update files Windows already downloaded. Windows re-downloads whatever it still needs. Skipped while Windows is installing updates.", L"", 0, true, {L"wuauserv", L"bits"}}, keep);
    AddTemp(cs, {L"t.do", L"Delivery Optimization cache", {win + L"\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache"}, true, 0, L"Windows Update/Store files cached for sharing with other PCs.", L"", 0, true, {L"DoSvc"}}, keep);
    AddTemp(cs, {L"t.shader", L"DirectX shader cache", {local + L"\\D3DSCache", local + L"\\NVIDIA\\DXCache", local + L"\\NVIDIA\\GLCache", local + L"\\AMD\\DxCache", local + L"\\AMD\\GLCache"}, false, 0, L"Compiled graphics shaders. Games and apps rebuild them, so the first launch afterwards can stutter briefly."}, keep);
    AddTemp(cs, {L"t.inet", L"Internet cache (Windows)", {local + L"\\Microsoft\\Windows\\INetCache"}, false, 0, L"Cached web content used by Windows components."}, keep);
    AddTemp(cs, {L"t.thumb", L"Thumbnail cache", {local + L"\\Microsoft\\Windows\\Explorer"}, false, 0, L"Explorer rebuilds thumbnails when needed (only thumbcache files are touched)."}, keep);
    AddTemp(cs, {L"t.wer", L"Error reports (Windows Error Reporting)", {local + L"\\Microsoft\\Windows\\WER", pd + L"\\Microsoft\\Windows\\WER\\ReportArchive", pd + L"\\Microsoft\\Windows\\WER\\ReportQueue"}, false, 0, L"Stored crash reports. Some are in the all-users area and need administrator rights."}, keep);
    AddTemp(cs, {L"t.dumps", L"Crash dumps (user)", {local + L"\\CrashDumps"}, false, 0, L"Memory dumps from crashed programs."}, keep);
    AddTemp(cs, {L"t.memdump", L"System memory dumps", {win + L"\\MEMORY.DMP", win + L"\\Minidump", win + L"\\LiveKernelReports"}, true, 0, L"Blue-screen and kernel dumps. Keep them if you are troubleshooting a crash.", L"*.dmp", 0, true}, keep);
    AddTemp(cs, {L"t.logs", L"Windows log files (older than 14 days)", {win + L"\\Logs\\CBS", win + L"\\Logs\\DISM", win + L"\\Logs\\WindowsUpdate"}, true, 0, L"Old servicing/update logs. Newer logs are kept because they help troubleshooting.", L"*.log;*.etl;*.cab", 14, true}, keep);
    AddTemp(cs, {L"t.recent", L"Recent files history", {appdata + L"\\Microsoft\\Windows\\Recent"}, false, 0, L"The list of recently opened files and Jump Lists (privacy). Windows rebuilds it as you work. Your files themselves are not touched.", L"", 0, true}, keep);
    AddTemp(cs, {L"t.prefetch", L"Prefetch (files unused for 30+ days)", {win + L"\\Prefetch"}, true, 1, L"Windows' app-launch hints. Only entries unused for 30 days are removed; Windows recreates them, and the first launch of those programs can be slightly slower.", L"*.pf", 30, true}, keep);
    AddTemp(cs, {L"t.fontcache", L"Font cache (rebuilt automatically)", {win + L"\\ServiceProfiles\\LocalService\\AppData\\Local\\FontCache"}, true, 1, L"Fixes broken or stale font display. The Windows Font Cache service is stopped, the cache cleared and the service restarted; text may flicker briefly.", L"*.dat", 0, true, {L"FontCache"}}, keep);
    AddTemp(cs, {L"t.edge", L"Microsoft Edge cache", {local + L"\\Microsoft\\Edge\\User Data\\Default\\Cache", local + L"\\Microsoft\\Edge\\User Data\\Default\\Code Cache", local + L"\\Microsoft\\Edge\\User Data\\Default\\GPUCache"}, false, 0, L"Browser cache only (not passwords, cookies or history).", L"", 0, false, {}, L"msedge.exe"}, keep);
    AddTemp(cs, {L"t.chrome", L"Google Chrome cache", {local + L"\\Google\\Chrome\\User Data\\Default\\Cache", local + L"\\Google\\Chrome\\User Data\\Default\\Code Cache", local + L"\\Google\\Chrome\\User Data\\Default\\GPUCache"}, false, 0, L"Browser cache only (not passwords, cookies or history).", L"", 0, false, {}, L"chrome.exe"}, keep);
    AddTemp(cs, {L"t.brave", L"Brave cache", {local + L"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Cache", local + L"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Code Cache"}, false, 0, L"Browser cache only (not passwords, cookies or history).", L"", 0, false, {}, L"brave.exe"}, keep);
    AddTemp(cs, {L"t.firefox", L"Mozilla Firefox cache", FirefoxCaches(local), false, 0, L"Browser cache only (not passwords, cookies, bookmarks or history).", L"", 0, false, {}, L"firefox.exe"}, keep);
    { wstring wo = win.substr(0, 3) + L"Windows.old"; if (GetFileAttributesW(wo.c_str()) != INVALID_FILE_ATTRIBUTES) { CleanItem it; it.id = L"t.windowsold"; it.name = L"Previous Windows installation (Windows.old)"; it.type = L"Temporary files"; it.kind = 5; it.risk = 2; it.needsAdmin = true; ULONGLONG f = 0; it.bytes = DirSize(wo, f); it.files = f; it.location = wo; it.detail = L"opens Disk Cleanup"; cs.totalBytes += it.bytes;
        it.why = L"Lets you roll back to the previous Windows version. Deleting its files by hand is unreliable (protected permissions), so this opens Windows' own Disk Cleanup, where you choose 'Previous Windows installation(s)'. You cannot roll back afterwards."; it.cmd = L"cleanmgr.exe"; cs.items.push_back(std::move(it)); } }
    { CleanItem it; it.id = L"t.dns"; it.name = L"DNS resolver cache"; it.type = L"Temporary files"; it.kind = 4; it.risk = 0; it.detail = L"flush"; it.location = L"ipconfig /flushdns"; it.why = L"Clears cached website name lookups. Safe; fixes stale or wrong addresses. Websites are looked up again on the next visit."; it.cmd = L"ipconfig.exe /flushdns"; cs.items.push_back(std::move(it)); }
    SHQUERYRBINFO rb{sizeof rb}; if (SUCCEEDED(SHQueryRecycleBinW(nullptr, &rb)) && rb.i64NumItems > 0) { CleanItem it; it.id = L"t.recycle"; it.name = L"Recycle Bin"; it.type = L"Temporary files"; it.kind = 1; it.bytes = (ULONGLONG)rb.i64Size; it.files = (ULONGLONG)rb.i64NumItems; it.location = L"All drives"; it.detail = std::to_wstring(rb.i64NumItems) + L" items"; it.why = L"Items you deleted. Emptying is permanent - you cannot restore them afterwards."; cs.totalBytes += it.bytes; cs.items.push_back(std::move(it)); }
    // invalid registry entries (conservative: fixed local drives only, never Windows-folder paths, never MSI/system components)
    for (REGSAM v : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) ScanUninstall(cs, HKEY_LOCAL_MACHINE, v, winLow);
    ScanUninstall(cs, HKEY_CURRENT_USER, KEY_WOW64_64KEY, winLow);
    for (REGSAM v : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) ScanAppPaths(cs, HKEY_LOCAL_MACHINE, v, winLow);
    ScanAppPaths(cs, HKEY_CURRENT_USER, KEY_WOW64_64KEY, winLow);
    ScanValuesAsPaths(cs, HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache", L"m", L"Registry: stale MUI cache entry", true, winLow);
    for (REGSAM v : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) ScanValuesAsPaths(cs, HKEY_LOCAL_MACHINE, v, L"Software\\Microsoft\\Windows\\CurrentVersion\\SharedDLLs", L"s", L"Registry: shared DLL reference to a missing file", false, winLow);
    cs.coverage = L"Cleaner looked at temporary-file folders and 4 registry areas (leftover uninstall entries, App Paths, MUI cache, shared DLL references). Registry results are conservative: only entries whose file is missing on a fixed local drive. Entries on network/removable drives, MSI products and Windows-folder files are never listed. " +
                  std::to_wstring(cs.registryCount) + L" invalid registry entries, " + FormatBytes(cs.totalBytes) + L" reclaimable.";
    return cs;
}

// ---- registry backup / restore ----
static wstring Hex(const void* p, size_t n) { wstring o; const BYTE* b = (const BYTE*)p; for (size_t i = 0; i < n; ++i) { wchar_t t[3]; swprintf(t, 3, L"%02X", b[i]); o += t; } return o; }
static bool UnHex(const wstring& h, std::vector<BYTE>& out) { if (h.size() % 2 || h.size() > 140000) return false; for (size_t i = 0; i < h.size(); i += 2) { wchar_t t[3] = {h[i], h[i + 1], 0}; wchar_t* e; unsigned long v = wcstoul(t, &e, 16); if (*e) return false; out.push_back((BYTE)v); } return true; }
static wstring HexStr(const wstring& s) { return Hex(s.c_str(), s.size() * 2); }
static bool UnHexStr(const wstring& h, wstring& s) { std::vector<BYTE> b; if (!UnHex(h, b) || b.size() % 2) return false; s.assign((const wchar_t*)b.data(), b.size() / 2); return true; }
static bool AllowedRegPath(const wstring& p) {
    wstring l = Lower(p); static const wchar_t* ok[] = {L"software\\microsoft\\windows\\currentversion\\uninstall\\", L"software\\microsoft\\windows\\currentversion\\app paths\\", L"software\\microsoft\\windows\\currentversion\\shareddlls",
        L"software\\classes\\local settings\\software\\microsoft\\windows\\shell\\muicache"};
    for (auto o : ok) if (l.rfind(o, 0) == 0) return true; return false;
}
static void ExportKeyTree(HKEY root, REGSAM view, const wstring& sub, wstring& out, int depth, int& budget) {
    if (depth > 6 || budget <= 0) return; HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | view, &k) != ERROR_SUCCESS) return;
    out += L"K\t" + HexStr(sub) + L"\n";
    for (DWORD i = 0; budget > 0; ++i) {
        wchar_t nm[16384 / 8]; DWORD nl = 2048, ty = 0, sz = 0; if (RegEnumValueW(k, i, nm, &nl, nullptr, &ty, nullptr, &sz) != ERROR_SUCCESS) break;
        if (sz > 65536) continue; std::vector<BYTE> d(sz + 2); nl = 2048; sz = (DWORD)d.size() - 2; if (RegEnumValueW(k, i, nm, &nl, nullptr, &ty, d.data(), &sz) != ERROR_SUCCESS) continue;
        out += L"V\t" + HexStr(nm) + L"\t" + std::to_wstring(ty) + L"\t" + Hex(d.data(), sz) + L"\n"; --budget;
    }
    for (DWORD i = 0; budget > 0; ++i) { wchar_t nm[512]; DWORD nl = 512; if (RegEnumKeyExW(k, i, nm, &nl, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break; ExportKeyTree(root, view, sub + L"\\" + nm, out, depth + 1, budget); }
    RegCloseKey(k);
}
static bool ExportValue(HKEY root, REGSAM view, const wstring& sub, const wstring& name, wstring& out) {
    HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return false; DWORD ty = 0, sz = 0; bool ok = false;
    if (RegQueryValueExW(k, name.c_str(), nullptr, &ty, nullptr, &sz) == ERROR_SUCCESS && sz <= 65536) { std::vector<BYTE> d(sz + 2); sz = (DWORD)d.size() - 2; if (RegQueryValueExW(k, name.c_str(), nullptr, &ty, d.data(), &sz) == ERROR_SUCCESS) { out += L"K\t" + HexStr(sub) + L"\nV\t" + HexStr(name) + L"\t" + std::to_wstring(ty) + L"\t" + Hex(d.data(), sz) + L"\n"; ok = true; } }
    RegCloseKey(k); return ok;
}
static wstring RegBackupDir() { return SubDir(L"regbackups"); }

std::vector<BackupInfo> ListRegBackups() {
    std::vector<BackupInfo> v; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((RegBackupDir() + L"\\*.fsureg").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return v;
    do { BackupInfo b; b.path = RegBackupDir() + L"\\" + fd.cFileName; b.written = fd.ftLastWriteTime; b.label = fd.cFileName; v.push_back(b); } while (FindNextFileW(h, &fd)); FindClose(h);
    std::sort(v.begin(), v.end(), [](const BackupInfo& a, const BackupInfo& b) { return CompareFileTime(&a.written, &b.written) > 0; }); if (v.size() > 20) v.resize(20);
    for (auto& b : v) { std::ifstream f(std::filesystem::path(b.path), std::ios::binary); std::string ln; int n = 0; wstring label, time; while (n++ < 8 && std::getline(f, ln)) { auto w = FromUtf8(ln); if (!w.empty() && w.back() == L'\r') w.pop_back(); if (w.rfind(L"label=", 0) == 0) label = w.substr(6); else if (w.rfind(L"time=", 0) == 0) time = w.substr(5); }
        if (!label.empty()) b.label = label + (time.size() >= 16 ? L"   (" + time.substr(0, 10) + L" " + time.substr(11, 5) + L")" : L""); }
    return v;
}
bool RestoreRegBackup(const wstring& path, wstring& err, wstring& summary) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary); if (!f) { err = L"Cannot open the backup."; return false; }
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); if (all.size() > 20 * 1024 * 1024) { err = L"Backup is unexpectedly large; refusing."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    size_t pos = 0; bool first = true; HKEY root = nullptr; REGSAM view = KEY_WOW64_64KEY; HKEY cur = nullptr; wstring curPath; int keys = 0, vals = 0, skipped = 0, lines = 0;
    while (pos < all.size() && ++lines < 400000) {
        size_t nl = all.find('\n', pos); wstring w = FromUtf8(all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos)); pos = nl == std::string::npos ? all.size() : nl + 1; if (!w.empty() && w.back() == L'\r') w.pop_back();
        if (first) { if (w != L"FSUREGBACKUP1") { err = L"Not a valid registry backup."; return false; } first = false; continue; }
        if (w.rfind(L"R\t", 0) == 0) { size_t a = w.find(L'\t', 2); wstring r = w.substr(2, a == wstring::npos ? wstring::npos : a - 2); root = r == L"HKCU" ? HKEY_CURRENT_USER : r == L"HKLM" ? HKEY_LOCAL_MACHINE : nullptr; view = (a != wstring::npos && w.substr(a + 1) == L"32") ? KEY_WOW64_32KEY : KEY_WOW64_64KEY; if (cur) { RegCloseKey(cur); cur = nullptr; } }
        else if (w.rfind(L"K\t", 0) == 0) { if (cur) { RegCloseKey(cur); cur = nullptr; } wstring p; if (!root || !UnHexStr(w.substr(2), p) || !AllowedRegPath(p)) { ++skipped; continue; }
            LONG r = RegCreateKeyExW(root, p.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | view, nullptr, &cur, nullptr); if (r != ERROR_SUCCESS) { cur = nullptr; ++skipped; if (r == ERROR_ACCESS_DENIED) err = L"Administrator rights are required to restore all-users entries."; } else ++keys; }
        else if (w.rfind(L"V\t", 0) == 0 && cur) { size_t a = w.find(L'\t', 2), b = w.find(L'\t', a + 1); if (a == wstring::npos || b == wstring::npos) continue; wstring nm; std::vector<BYTE> d;
            if (!UnHexStr(w.substr(2, a - 2), nm) || !UnHex(w.substr(b + 1), d)) { ++skipped; continue; } DWORD ty = (DWORD)_wtoi(w.substr(a + 1, b - a - 1).c_str());
            if (ty != REG_SZ && ty != REG_EXPAND_SZ && ty != REG_DWORD && ty != REG_BINARY && ty != REG_MULTI_SZ && ty != REG_QWORD) { ++skipped; continue; }
            if (RegQueryValueExW(cur, nm.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) { ++skipped; continue; }       // never overwrite
            if (RegSetValueExW(cur, nm.c_str(), 0, ty, d.data(), (DWORD)d.size()) == ERROR_SUCCESS) ++vals; else ++skipped; }
    }
    if (cur) RegCloseKey(cur);
    summary = std::to_wstring(keys) + L" key(s), " + std::to_wstring(vals) + L" value(s) restored" + (skipped ? L", " + std::to_wstring(skipped) + L" skipped (already present or not allowed)" : L"");
    WriteJournal(L"registry cleaner restore", path, L"removed", summary, L"ok", false); return keys > 0 || vals > 0;
}

// ---- run ----
void RunClean(const std::vector<CleanItem>& items, bool keep, CleanResult& r) {
    wstring backup; HKEY lastRoot = nullptr; REGSAM lastView = 0; std::vector<const CleanItem*> reg;
    for (auto& it : items) {
        if (it.kind == 0) {
            if (it.needsAdmin && !IsElevated()) { ++r.skipped; if (r.firstErr.empty()) r.firstErr = it.name + L": needs administrator rights"; continue; }
            if (!it.services.empty() && (IsProcessRunning(L"TiWorker.exe") || IsProcessRunning(L"TrustedInstaller.exe") || IsProcessRunning(L"wusa.exe"))) { ++r.skipped; if (r.firstErr.empty()) r.firstErr = it.name + L": Windows is installing updates right now"; continue; }
            std::vector<wstring> stopped; bool svcFail = false;
            for (auto& sv : it.services) { wstring e2; int x = SvcStopIfRunning(sv, e2); if (x == 1) stopped.push_back(sv); else if (x < 0) { svcFail = true; if (r.firstErr.empty()) r.firstErr = it.name + L": could not stop service " + sv + L" (" + e2 + L")"; break; } }
            if (svcFail) { for (auto it2 = stopped.rbegin(); it2 != stopped.rend(); ++it2) { wstring e3; SvcStartByName(*it2, e3); } ++r.skipped; continue; }
            Walk w; w.del = true; w.cutoff = ItemCutoff(keep, it.ignoreRecent, it.minAgeDays); w.pattern = it.pattern.empty() ? nullptr : it.pattern.c_str();
            for (auto& p : it.paths) { DWORD at = GetFileAttributesW(p.c_str()); if (at == INVALID_FILE_ATTRIBUTES) continue;
                if (!(at & FILE_ATTRIBUTE_DIRECTORY)) { WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa) && !(w.cutoff && FtU(fa.ftLastWriteTime) > w.cutoff)) { SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); if (DeleteFileW(p.c_str())) { w.bytes += ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; ++w.files; } else ++w.failed; } continue; }
                if (it.id == L"t.thumb") { WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((p + L"\\thumbcache_*.db").c_str(), &fd); if (h != INVALID_HANDLE_VALUE) { do { wstring full = p + L"\\" + fd.cFileName; ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; if (DeleteFileW(full.c_str())) { w.bytes += sz; ++w.files; } else ++w.failed; } while (FindNextFileW(h, &fd)); FindClose(h); } } else WalkDir(p, w, 0); }
            for (auto it2 = stopped.rbegin(); it2 != stopped.rend(); ++it2) { wstring e3; if (!SvcStartByName(*it2, e3) && r.firstErr.empty()) r.firstErr = L"Service " + *it2 + L" could not be restarted automatically (" + e3 + L"); start it from the Services page"; }   // always put services back
            r.bytes += w.bytes; r.files += w.files; ++r.ok; if (w.failed && r.firstErr.empty()) r.firstErr = it.name + L": " + std::to_wstring(w.failed) + L" files in use were skipped";
            WriteJournal(it.name, L"cleaner: temporary files", FormatBytes(it.bytes) + L", " + std::to_wstring(it.files) + L" files", L"deleted " + FormatBytes(w.bytes) + L" in " + std::to_wstring(w.files) + L" files, " + std::to_wstring(w.failed + w.skipped) + L" skipped", L"ok (not undoable)", false);
        } else if (it.kind == 4) {
            STARTUPINFOW si{sizeof si}; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{}; wstring cl = it.cmd; DWORD code = 1; bool ok = false;
            if (CreateProcessW(nullptr, &cl[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { WaitForSingleObject(pi.hProcess, 30000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); ok = code == 0; }
            if (ok) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": command failed"; }
            WriteJournal(it.name, L"cleaner: " + it.cmd, L"", ok ? L"done" : L"failed", ok ? L"ok" : L"failed", false);
        } else if (it.kind == 5) {
            bool ok = (INT_PTR)ShellExecuteW(nullptr, L"open", it.cmd.c_str(), L"/d C", nullptr, SW_SHOWNORMAL) > 32; if (ok) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": could not open Disk Cleanup"; }
            WriteJournal(it.name, L"cleaner: Disk Cleanup", FormatBytes(it.bytes), L"opened", ok ? L"ok (continue in Disk Cleanup)" : L"failed", false);
        } else if (it.kind == 1) {
            HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            if (SUCCEEDED(hr) || hr == S_FALSE) { r.bytes += it.bytes; r.files += it.files; ++r.ok; } else { ++r.failed; if (r.firstErr.empty()) r.firstErr = L"Recycle Bin: could not be emptied"; }
            WriteJournal(L"Recycle Bin", L"cleaner", FormatBytes(it.bytes), L"emptied", SUCCEEDED(hr) ? L"ok (not undoable)" : L"failed", false);
        } else if (it.kind == 2 || it.kind == 3) reg.push_back(&it);
    }
    if (!reg.empty()) {
        if (!JournalWritable()) { r.failed += (int)reg.size(); if (r.firstErr.empty()) r.firstErr = L"Cannot write the change journal; registry entries were not touched."; return; }
        wstring bk = L"FSUREGBACKUP1\nlabel=Registry cleaner (" + std::to_wstring(reg.size()) + L" entries)\ntime=" + NowIsoUtc() + L"\n"; std::vector<const CleanItem*> todo;
        for (auto* it : reg) { if (it->needsAdmin && !IsElevated()) { ++r.skipped; if (r.firstErr.empty()) r.firstErr = it->name + L": needs administrator rights"; continue; }
            if (it->root != lastRoot || it->view != lastView) { bk += wstring(L"R\t") + RootName(it->root) + L"\t" + (it->view == KEY_WOW64_32KEY ? L"32" : L"64") + L"\n"; lastRoot = it->root; lastView = it->view; }
            wstring part; bool ok = it->kind == 2 ? ([&] { int budget = 2000; ExportKeyTree(it->root, it->view, it->subKey, part, 0, budget); return !part.empty(); })() : ExportValue(it->root, it->view, it->subKey, it->valueName, part);
            if (!ok) { ++r.skipped; continue; } bk += part; todo.push_back(it); }
        if (!todo.empty()) {
            wstring fn = NowIsoUtc(); for (auto& c : fn) if (c == L':') c = L'-'; wstring path = RegBackupDir() + L"\\regclean_" + fn + L".fsureg";
            if (!AtomicWrite(path, ToUtf8(bk))) { r.failed += (int)todo.size(); if (r.firstErr.empty()) r.firstErr = L"Backup could not be written; no registry entry was removed."; return; }   // abort without backup
            r.backupPath = path;
            for (auto* it : todo) {
                bool gone = false; wstring err;
                if (it->kind == 2) { size_t sl = it->subKey.find_last_of(L'\\'); wstring parent = it->subKey.substr(0, sl), child = it->subKey.substr(sl + 1); HKEY pk;
                    if (RegOpenKeyExW(it->root, parent.c_str(), 0, KEY_ALL_ACCESS | it->view, &pk) == ERROR_SUCCESS) { LONG x = RegDeleteTreeW(pk, child.c_str()); HKEY chk; gone = x == ERROR_SUCCESS && RegOpenKeyExW(pk, child.c_str(), 0, KEY_READ | it->view, &chk) != ERROR_SUCCESS; if (!gone) err = Win32Err(x); RegCloseKey(pk); } else err = L"cannot open parent key"; }
                else { HKEY k; if (RegOpenKeyExW(it->root, it->subKey.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE | it->view, &k) == ERROR_SUCCESS) { LONG x = RegDeleteValueW(k, it->valueName.c_str()); gone = (x == ERROR_SUCCESS || x == ERROR_FILE_NOT_FOUND) && RegQueryValueExW(k, it->valueName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_FILE_NOT_FOUND; if (!gone) err = Win32Err(x); RegCloseKey(k); } else err = L"cannot open key"; }
                if (gone) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it->name + L": " + err; }
                WriteJournal(it->name, it->location, L"present (" + it->why + L")", gone ? L"removed (backup: " + path + L")" : L"unchanged", gone ? L"ok (restorable)" : L"failed: " + err, true);
            }
        }
    }
}

} // namespace fsu
