// Cleaner: temporary-file categories and conservative invalid-registry detection.
// Temp files are deleted permanently (locked/in-use files are skipped). Registry removals are exported first and restorable.
#include "app.h"
#include "../core/text.h"
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <taskschd.h>
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

// ---- deep scan: whole-system junk (explicit file lists) ----
struct DeepCat { const wchar_t* id; const wchar_t* name; int risk; int minAgeDays; const wchar_t* why; std::vector<wstring> files; ULONGLONG bytes = 0; bool capped = false; };
struct DeepCtx { std::atomic<bool>* cancel; const std::function<void(const wstring&)>* progress; std::vector<DeepCat> cats; ULONGLONG scanned = 0, lastReport = 0; std::set<wstring> skipRoots; ULONGLONG now; };
static void DeepAdd(DeepCtx& c, int cat, const wstring& path, ULONGLONG size, ULONGLONG mtime) {
    DeepCat& d = c.cats[cat]; if (d.minAgeDays && mtime > c.now - (ULONGLONG)d.minAgeDays * 24 * 3600 * 10000000ULL) return; if (d.files.size() >= 200000) { d.capped = true; return; } d.files.push_back(path); d.bytes += size;
}
static void DeepWalk(DeepCtx& c, const wstring& dir, int depth) {
    if (depth > 40 || (c.cancel && c.cancel->load())) return; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH); if (h == INVALID_HANDLE_VALUE) return;
    do {
        wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; wstring full = dir + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            wstring ln = Lower(n), lf = Lower(full); if (ln == L".git" || ln == L".svn" || ln == L"node_modules" || ln == L"$recycle.bin" || ln == L"system volume information" || ln == L"recovery") continue; bool skip = false; for (auto& r : c.skipRoots) if (lf == r) { skip = true; break; } if (skip) continue;
            if (n.size() > 6 && Lower(n.substr(0, 6)) == L"found.") { ULONGLONG f = 0; (void)f; }
            DeepWalk(c, full, depth + 1); if (c.cancel && c.cancel->load()) break; continue; }
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN)) continue;
        ++c.scanned; if (c.progress && *c.progress && c.scanned - c.lastReport >= 5000) { c.lastReport = c.scanned; (*c.progress)(L"Deep scan… " + std::to_wstring(c.scanned) + L" files checked · " + dir); }
        wstring l = Lower(n); size_t dot = l.find_last_of(L'.'); wstring ext = dot == wstring::npos ? L"" : l.substr(dot + 1); ULONGLONG size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow, mt = FtU(fd.ftLastWriteTime);
        if (ext == L"tmp" || ext == L"temp" || (l.size() > 2 && l[0] == L'~' && ext == L"tmp")) DeepAdd(c, 0, full, size, mt);
        else if (l.rfind(L"~$", 0) == 0) DeepAdd(c, 1, full, size, mt);
        else if (l == L"thumbs.db" || l == L"ehthumbs.db" || l == L".ds_store") DeepAdd(c, 2, full, size, mt);
        else if (ext == L"chk") DeepAdd(c, 3, full, size, mt);
        else if (ext == L"dmp" || ext == L"mdmp") DeepAdd(c, 4, full, size, mt);
        else if (ext == L"log" || ext == L"etl") DeepAdd(c, 5, full, size, mt);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
CleanScan ScanDeep(std::atomic<bool>* cancel, const std::function<void(const wstring&)>& progress) {
    CleanScan cs; DeepCtx c; c.cancel = cancel; c.progress = &progress; FILETIME ft; GetSystemTimeAsFileTime(&ft); c.now = FtU(ft);
    c.cats = {{L"d.tmp", L"Stray temporary files (*.tmp, *.temp)", 0, 7, L"Leftover temp files anywhere on your drives, untouched for 7+ days. Files still in use are skipped."},
              {L"d.office", L"Orphaned Office lock files (~$*)", 0, 7, L"Hidden lock files left behind by Word/Excel/PowerPoint after a crash."},
              {L"d.thumbs", L"Thumbnail databases (Thumbs.db, ehthumbs.db)", 0, 0, L"Old Explorer thumbnail caches scattered in your folders; Windows recreates them if needed."},
              {L"d.chk", L"Disk-check fragments (*.chk)", 0, 0, L"Recovered file fragments created by Windows disk checks."},
              {L"d.dmp", L"Old crash dumps (*.dmp) older than 14 days", 0, 14, L"Memory dumps from crashed programs anywhere on your drives."},
              {L"d.log", L"Old log files (*.log, *.etl) older than 90 days", 1, 90, L"Application logs untouched for 90+ days outside Windows and Program Files. Keep them if you troubleshoot software."}};
    for (auto& cat : c.cats) cat.minAgeDays = cat.minAgeDays;
    wstring win = Lower(KnownFolder(FOLDERID_Windows)), pf = Lower(KnownFolder(FOLDERID_ProgramFiles)), pf86 = Lower(KnownFolder(FOLDERID_ProgramFilesX86)); c.skipRoots = {win, pf, pf86, Lower(DataDir())};
    wchar_t drives[256]; DWORD n = GetLogicalDriveStringsW(255, drives); int nd = 0;
    for (wchar_t* d = drives; n && *d && !(cancel && cancel->load()); d += wcslen(d) + 1) { if (GetDriveTypeW(d) != DRIVE_FIXED) continue; wstring root = d; while (root.size() > 2 && root.back() == L'\\') root.pop_back(); ++nd; DeepWalk(c, root, 0); }
    wstring local = KnownFolder(FOLDERID_LocalAppData), appdata = KnownFolder(FOLDERID_RoamingAppData), prof = KnownFolder(FOLDERID_Profile);
    for (auto& cat : c.cats) { if (cat.files.empty()) continue; CleanItem it; it.id = cat.id; it.name = cat.name; it.type = L"Deep scan: stray files"; it.kind = 7; it.risk = cat.risk; it.minAgeDays = cat.minAgeDays; it.ignoreRecent = true; it.paths = cat.files; it.bytes = cat.bytes; it.files = cat.files.size();
        it.location = std::to_wstring(it.files) + L" files across " + std::to_wstring(nd) + L" fixed drive(s); e.g. " + cat.files[0]; it.detail = std::to_wstring(it.files) + L" files" + (cat.capped ? L" (first 200,000)" : L""); it.why = cat.why; it.needsAdmin = false; cs.totalBytes += it.bytes; cs.items.push_back(std::move(it)); }
    // developer / application caches (known folders; caches only, never settings, packages or logins)
    struct Known { const wchar_t* id; const wchar_t* name; std::vector<wstring> roots; const wchar_t* why; };
    std::vector<Known> known = {
        {L"d.npm", L"npm cache", {local + L"\\npm-cache"}, L"Downloaded npm packages. npm downloads them again when needed."}, {L"d.pip", L"pip cache", {local + L"\\pip\\Cache"}, L"Downloaded Python packages and wheels."},
        {L"d.yarn", L"Yarn cache", {local + L"\\Yarn\\Cache"}, L"Downloaded Yarn packages."}, {L"d.nuget", L"NuGet HTTP cache", {local + L"\\NuGet\\v3-cache"}, L"NuGet web cache (your installed packages are not touched)."},
        {L"d.discord", L"Discord cache", {appdata + L"\\discord\\Cache", appdata + L"\\discord\\Code Cache", appdata + L"\\discord\\GPUCache"}, L"Discord image/web cache. You stay signed in."},
        {L"d.slack", L"Slack cache", {appdata + L"\\Slack\\Cache", appdata + L"\\Slack\\Code Cache", appdata + L"\\Slack\\GPUCache"}, L"Slack web cache. You stay signed in."},
        {L"d.vscode", L"VS Code cache", {appdata + L"\\Code\\Cache", appdata + L"\\Code\\CachedData", appdata + L"\\Code\\Code Cache", appdata + L"\\Code\\GPUCache"}, L"Editor caches. Settings, extensions and workspaces are not touched."},
        {L"d.spotify", L"Spotify cache", {local + L"\\Spotify\\Data", local + L"\\Spotify\\Storage"}, L"Cached songs and artwork. Offline downloads in Storage may need re-downloading."},
        {L"d.edgeprof", L"Edge cache (other profiles)", {}, L"Cache of additional Edge profiles (browser cache only)."}, {L"d.chromeprof", L"Chrome cache (other profiles)", {}, L"Cache of additional Chrome profiles (browser cache only)."}};
    auto profCaches = [&](const wstring& userData, std::vector<wstring>& out) { WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((userData + L"\\Profile *").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return; do { if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) for (auto sub : {L"\\Cache", L"\\Code Cache", L"\\GPUCache"}) out.push_back(userData + L"\\" + fd.cFileName + sub); } while (FindNextFileW(h, &fd) && out.size() < 60); FindClose(h); };
    profCaches(local + L"\\Microsoft\\Edge\\User Data", known[8].roots); profCaches(local + L"\\Google\\Chrome\\User Data", known[9].roots);
    for (auto& k : known) { if (cancel && cancel->load()) break; Walk w; w.cutoff = 0; CleanItem it; it.id = k.id; it.name = k.name; it.type = L"Deep scan: app and developer caches"; it.kind = 0; it.ignoreRecent = true; it.why = k.why;
        for (auto& r : k.roots) { if (GetFileAttributesW(r.c_str()) == INVALID_FILE_ATTRIBUTES) continue; it.paths.push_back(r); WalkDir(r, w, 0); } if (it.paths.empty() || !w.bytes) continue;
        it.bytes = w.bytes; it.files = w.files; it.location = it.paths[0]; it.detail = std::to_wstring(w.files) + L" files"; cs.totalBytes += it.bytes; cs.items.push_back(std::move(it)); }
    if (IsElevated() || true) { CleanItem it; it.id = L"d.winsxs"; it.name = L"Windows component store cleanup (DISM)"; it.type = L"Deep scan: Windows system"; it.kind = 4; it.risk = 1; it.needsAdmin = true; it.timeoutSec = 1800; it.cmd = L"dism.exe /Online /Cleanup-Image /StartComponentCleanup"; it.location = L"dism.exe /Online /Cleanup-Image /StartComponentCleanup"; it.detail = L"can take 5-30 min";
        it.why = L"Windows' own cleanup of superseded update components (WinSxS). Safe and supported; it can run for several minutes and may need a restart. Pending updates are left alone."; cs.items.push_back(std::move(it)); }
    cs.coverage = L"Deep scan checked " + std::to_wstring(c.scanned) + L" files on " + std::to_wstring(nd) + L" fixed drive(s) (skipping Windows, Program Files, links, system files and cloud placeholders) plus known app/developer caches. " + FormatBytes(cs.totalBytes) + L" reclaimable." + (cancel && cancel->load() ? L" Stopped early." : L"");
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
    for (auto o : ok) if (l.rfind(o, 0) == 0) return true;
    // vendor/app settings keys written by the uninstaller leftover scan (never persistence or policy locations)
    static const wchar_t* deny[] = {L"software\\microsoft\\windows nt\\currentversion\\winlogon", L"software\\microsoft\\windows nt\\currentversion\\image file execution options", L"software\\microsoft\\windows\\currentversion\\policies",
        L"software\\policies", L"software\\microsoft\\windows\\currentversion\\explorer", L"software\\microsoft\\windows nt", L"software\\microsoft\\cryptography", L"software\\microsoft\\security center", L"software\\classes\\clsid\\{", L"software\\wow6432node\\microsoft\\windows nt"};
    static const wchar_t* allowRun[] = {L"software\\microsoft\\windows\\currentversion\\run", L"software\\microsoft\\windows\\currentversion\\runonce"};
    for (auto a : allowRun) if (l == a) return true;
    for (auto d : deny) if (l.rfind(d, 0) == 0) return false;
    return l.rfind(L"software\\", 0) == 0 && l.find(L'\\') != std::wstring::npos && l.size() > 10;
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
void RunClean(const std::vector<CleanItem>& items, bool keep, CleanResult& r, bool quarantine) {
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
            if (CreateProcessW(nullptr, &cl[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { WaitForSingleObject(pi.hProcess, (DWORD)(it.timeoutSec > 0 ? it.timeoutSec : 30) * 1000); GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); ok = code == 0; }
            if (ok) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": command failed"; }
            WriteJournal(it.name, L"cleaner: " + it.cmd, L"", ok ? L"done" : L"failed", ok ? L"ok" : L"failed", false);
        } else if (it.kind == 5) {
            bool ok = (INT_PTR)ShellExecuteW(nullptr, L"open", it.cmd.c_str(), L"/d C", nullptr, SW_SHOWNORMAL) > 32; if (ok) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": could not open Disk Cleanup"; }
            WriteJournal(it.name, L"cleaner: Disk Cleanup", FormatBytes(it.bytes), L"opened", ok ? L"ok (continue in Disk Cleanup)" : L"failed", false);
        } else if (it.kind == 1) {
            HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            if (SUCCEEDED(hr) || hr == S_FALSE) { r.bytes += it.bytes; r.files += it.files; ++r.ok; } else { ++r.failed; if (r.firstErr.empty()) r.firstErr = L"Recycle Bin: could not be emptied"; }
            WriteJournal(L"Recycle Bin", L"cleaner", FormatBytes(it.bytes), L"emptied", SUCCEEDED(hr) ? L"ok (not undoable)" : L"failed", false);
        } else if (it.kind == 7) {
            if (it.needsAdmin && !IsElevated()) { ++r.skipped; if (r.firstErr.empty()) r.firstErr = it.name + L": needs administrator rights"; continue; }
            ULONGLONG cutoff = ItemCutoff(keep, true, it.minAgeDays); ULONGLONG bytes = 0, files = 0, failed = 0, newer = 0; wstring win = Lower(KnownFolder(FOLDERID_Windows)), pf = Lower(KnownFolder(FOLDERID_ProgramFiles)), pf86 = Lower(KnownFolder(FOLDERID_ProgramFilesX86));
            for (auto& p : it.paths) {
                wstring l = Lower(p); if (l.rfind(win + L"\\", 0) == 0 || l.rfind(pf + L"\\", 0) == 0 || l.rfind(pf86 + L"\\", 0) == 0) continue;           // re-checked at delete time
                WIN32_FILE_ATTRIBUTE_DATA fa; if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa) || (fa.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) continue;
                if (cutoff && FtU(fa.ftLastWriteTime) > cutoff) { ++newer; continue; }
                if (quarantine) { wstring qe; if (QuarantinePath(p, it.name, qe, false)) { bytes += ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; ++files; } else ++failed; continue; }
                SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); if (DeleteFileW(p.c_str())) { bytes += ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; ++files; } else ++failed; }
            r.bytes += bytes; r.files += files; ++r.ok; if (failed && r.firstErr.empty()) r.firstErr = it.name + L": " + std::to_wstring(failed) + L" files in use were skipped";
            WriteJournal(it.name, L"cleaner: deep scan", FormatBytes(it.bytes) + L", " + std::to_wstring(it.files) + L" files", (quarantine ? L"moved to quarantine " : L"deleted ") + FormatBytes(bytes) + L" in " + std::to_wstring(files) + L" files, " + std::to_wstring(failed + newer) + L" skipped", quarantine ? L"ok (restorable from Recovery)" : L"ok (not undoable)", quarantine);
        } else if (it.kind == 6) {
            wstring dir = it.location; if (it.needsAdmin && !IsElevated()) { ++r.skipped; if (r.firstErr.empty()) r.firstErr = it.name + L": needs administrator rights (use Restart as administrator)"; continue; }
            if (IsProtectedPath(dir)) { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": protected location, not deleted"; WriteJournal(it.name, dir, L"present", L"unchanged", L"blocked: protected path", false); continue; }
            if (quarantine) { wstring qe; if (QuarantinePath(dir, it.name, qe)) { r.bytes += it.bytes; r.files += it.files; ++r.ok; } else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": " + qe; } continue; }   // restorable: Recovery > Quarantine
            Walk w; w.del = true; WalkDir(dir, w, 0); SetFileAttributesW(dir.c_str(), FILE_ATTRIBUTE_NORMAL); bool gone = RemoveDirectoryW(dir.c_str()) != 0 || GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES;
            r.bytes += w.bytes; r.files += w.files; if (gone) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": " + std::to_wstring(w.failed) + L" file(s) in use remain; restart and scan again"; }
            WriteJournal(it.name, dir, FormatBytes(it.bytes) + L", " + std::to_wstring(it.files) + L" files", gone ? L"deleted (" + FormatBytes(w.bytes) + L")" : L"partly deleted", gone ? L"ok (leftover folder, not undoable)" : L"partial", false);
        } else if (it.kind == 8) {
            if (!IsElevated()) { ++r.skipped; if (r.firstErr.empty()) r.firstErr = it.name + L": needs administrator rights"; continue; }
            wstring e2; int st = SvcStopIfRunning(it.cmd, e2); bool gone = false; wstring err;
            if (st < 0) err = L"could not stop the service (" + e2 + L")";
            else { SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (scm) { SC_HANDLE sv = OpenServiceW(scm, it.cmd.c_str(), DELETE); if (sv) { gone = DeleteService(sv) != 0 || GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE; if (!gone) err = Win32Err(GetLastError()); CloseServiceHandle(sv); } else gone = GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST; CloseServiceHandle(scm); } }
            if (gone) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": " + err; }
            WriteJournal(it.name, L"service " + it.cmd, it.location, gone ? L"deleted" : L"unchanged", gone ? L"ok (not undoable)" : L"failed: " + err, false);
        } else if (it.kind == 9) {
            bool gone = false; wstring err; ITaskService* svc = nullptr;   // per-user tasks need no elevation; protected tasks fail with a clear message
            if (SUCCEEDED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc)) && svc) { VARIANT v; VariantInit(&v);
                if (SUCCEEDED(svc->Connect(v, v, v, v))) { ITaskFolder* root = nullptr; BSTR rp = SysAllocString(L"\\"); if (SUCCEEDED(svc->GetFolder(rp, &root)) && root) { BSTR tp = SysAllocString(it.cmd.c_str()); IRegisteredTask* t = nullptr;
                        if (SUCCEEDED(root->GetTask(tp, &t)) && t) { BSTR xml = nullptr; if (SUCCEEDED(t->get_Xml(&xml)) && xml) { wstring bd = SubDir(L"taskbackups"); wstring fn = it.name; for (auto& c : fn) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_'; wstring bp = bd + L"\\" + fn + L"_" + std::to_wstring(GetTickCount() % 100000) + L".xml"; if (AtomicWrite(bp, "\xEF\xBB\xBF" + ToUtf8(wstring(xml)))) { gone = SUCCEEDED(root->DeleteTask(tp, 0)); if (!gone) err = L"Task Scheduler refused the deletion"; } else err = L"could not save the task XML backup; task kept"; SysFreeString(xml); } else err = L"could not read the task definition"; t->Release(); } else gone = true;
                        SysFreeString(tp); root->Release(); } SysFreeString(rp); } else err = L"cannot connect to Task Scheduler"; svc->Release(); } else err = L"Task Scheduler unavailable";
            if (!gone && err == L"Task Scheduler refused the deletion" && !IsElevated()) err += L" (administrator rights are likely required: More > Restart as administrator)";
            if (gone) ++r.ok; else { ++r.failed; if (r.firstErr.empty()) r.firstErr = it.name + L": " + err; }
            WriteJournal(it.name, L"task " + it.cmd, L"present", gone ? L"deleted (XML saved in taskbackups)" : L"unchanged", gone ? L"ok" : L"failed: " + err, false);
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

namespace fsu {
// Dry run: lists the exact files a clean of this item would remove (same rules as RunClean), capped at maxRows.
void PreviewClean(const CleanItem& it, bool keep, size_t maxRows, std::vector<PreviewRow>& rows, ULONGLONG& total) {
    total = 0; ULONGLONG cutoff = ItemCutoff(keep, it.ignoreRecent || it.kind == 7, it.minAgeDays);
    auto add = [&](const wstring& p, ULONGLONG sz, ULONGLONG mt) { ++total; if (rows.size() < maxRows) rows.push_back({p, sz, mt}); };
    std::function<void(const wstring&, int)> rec = [&](const wstring& dir, int depth) {
        if (depth > 24) return; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return;
        do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; wstring full = dir + L"\\" + n; if (IsReparse(fd.dwFileAttributes)) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { rec(full, depth + 1); continue; }
            if (!it.pattern.empty() && !PathMatchSpecW(n.c_str(), it.pattern.c_str())) continue; ULONGLONG mt = FtU(fd.ftLastWriteTime); if (cutoff && mt > cutoff) continue;
            add(full, ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow, mt); } while (FindNextFileW(h, &fd)); FindClose(h); };
    if (it.kind == 0) { for (auto& p : it.paths) { DWORD at = GetFileAttributesW(p.c_str()); if (at == INVALID_FILE_ATTRIBUTES) continue; if (at & FILE_ATTRIBUTE_DIRECTORY) rec(p, 0); else { WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa) && !(cutoff && FtU(fa.ftLastWriteTime) > cutoff)) add(p, ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow, FtU(fa.ftLastWriteTime)); } } }
    else if (it.kind == 6) { ULONGLONG save = cutoff; cutoff = 0; rec(it.location, 0); cutoff = save; }
    else if (it.kind == 7) { for (auto& p : it.paths) { WIN32_FILE_ATTRIBUTE_DATA fa; if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa) && !(fa.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) && !(cutoff && FtU(fa.ftLastWriteTime) > cutoff)) add(p, ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow, FtU(fa.ftLastWriteTime)); } }
}
}
