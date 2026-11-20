// Full software uninstaller: inventory, standard (interactive) and silent uninstall, evidence-based leftover detection.
// Residue is returned as CleanItems; deletion happens through the Cleaner pipeline (confirmation, registry backup, journal).
#include "app.h"
#include "../core/text.h"
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <filesystem>
#include <set>
#include <algorithm>

namespace fsu {
using std::wstring;

static wstring KF(REFKNOWNFOLDERID id) { PWSTR p = nullptr; wstring r; if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); } return r; }
static wstring Trim(wstring s) { while (!s.empty() && (s.back() == L'\\' || s.back() == L'/' || s.back() == L' ' || s.back() == L'"')) s.pop_back(); while (!s.empty() && (s.front() == L' ' || s.front() == L'"')) s.erase(s.begin()); return s; }
static wstring StrVal(HKEY k, const wchar_t* n) { wchar_t b[4096]; DWORD sz = sizeof b - 2, ty = 0; if (RegQueryValueExW(k, n, nullptr, &ty, (LPBYTE)b, &sz) == ERROR_SUCCESS && (ty == REG_SZ || ty == REG_EXPAND_SZ)) { b[sz / 2] = 0; return b; } return L""; }
static DWORD DwVal(HKEY k, const wchar_t* n) { DWORD v = 0, sz = 4, ty = 0; if (RegQueryValueExW(k, n, nullptr, &ty, (LPBYTE)&v, &sz) == ERROR_SUCCESS && ty == REG_DWORD) return v; return 0; }
static bool Under(const wstring& path, const wstring& root) { return IsPathUnder(path, root); }
static bool FixedDrive(const wstring& p) { if (!IsDriveAbsolute(p)) return false; wchar_t r[4] = {p[0], L':', L'\\', 0}; return GetDriveTypeW(r) == DRIVE_FIXED; }
static ULONGLONG DirBytes(const wstring& dir, int depth, ULONGLONG& files, ULONGLONG cap) {
    if (depth > 20 || files > cap) return 0; ULONGLONG total = 0; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return 0;
    do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) total += DirBytes(dir + L"\\" + n, depth + 1, files, cap); else { total += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; ++files; } } while (FindNextFileW(h, &fd));
    FindClose(h); return total;
}

// ---- protected paths: leftover deletion must never touch these ----
bool IsProtectedPath(const wstring& pathIn) {
    wstring p = Trim(pathIn); if (!IsDriveAbsolute(p)) return true; wstring l = Lower(p);
    if (l.size() <= 3) return true;                                                    // drive root
    static std::vector<wstring> exact, trees; static bool init = false;
    if (!init) { init = true;
        for (auto id : {FOLDERID_Windows, FOLDERID_System, FOLDERID_SystemX86}) trees.push_back(Lower(KF(id)));
        wchar_t me[MAX_PATH]; GetModuleFileNameW(nullptr, me, MAX_PATH); wstring m = me; size_t sl = m.find_last_of(L'\\'); if (sl != wstring::npos) trees.push_back(Lower(m.substr(0, sl)));
        trees.push_back(Lower(DataDir()));
        for (auto id : {FOLDERID_ProgramFiles, FOLDERID_ProgramFilesX86, FOLDERID_ProgramFilesCommon, FOLDERID_ProgramFilesCommonX86, FOLDERID_ProgramData, FOLDERID_Profile, FOLDERID_UserProfiles, FOLDERID_LocalAppData, FOLDERID_RoamingAppData, FOLDERID_Desktop, FOLDERID_Documents, FOLDERID_Downloads, FOLDERID_Pictures, FOLDERID_Music, FOLDERID_Videos, FOLDERID_PublicDocuments, FOLDERID_Public, FOLDERID_StartMenu, FOLDERID_Programs, FOLDERID_Startup, FOLDERID_Fonts})
            exact.push_back(Lower(KF(id)));
        PWSTR lp = nullptr; (void)lp;
        wstring la = KF(FOLDERID_LocalAppData); exact.push_back(Lower(la + L"\\Programs")); exact.push_back(Lower(la + L"\\Microsoft")); exact.push_back(Lower(KF(FOLDERID_RoamingAppData) + L"\\Microsoft")); exact.push_back(Lower(KF(FOLDERID_ProgramData) + L"\\Microsoft"));
        exact.push_back(Lower(la + L"\\Temp")); exact.push_back(Lower(la + L"\\Packages")); }
    for (auto& t : trees) if (!t.empty() && (l == t || l.rfind(t + L"\\", 0) == 0)) return true;
    for (auto& e : exact) if (!e.empty() && l == e) return true;
    size_t b = l.find_last_of(L'\\'); wstring leaf = l.substr(b + 1);
    static const wchar_t* bad[] = {L"system volume information", L"$recycle.bin", L"recovery", L"perflogs", L"boot", L"documents and settings", L"windowsapps", L"microsoft", L"windows", L"common files", L"temp", L"packages", L"users", L"appdata", L"all users"};
    for (auto x : bad) if (leaf == x) return true;
    return false;
}

// ---- inventory ----
static wstring YmdOrEmpty(const wstring& s) { if (s.size() == 8 && s.find_first_not_of(L"0123456789") == wstring::npos) return s.substr(0, 4) + L"-" + s.substr(4, 2) + L"-" + s.substr(6, 2); return L""; }
static wstring FtDate(const FILETIME& f) { FILETIME l; SYSTEMTIME st; if (!FileTimeToLocalFileTime(&f, &l) || !FileTimeToSystemTime(&l, &st)) return L""; wchar_t b[16]; swprintf(b, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); return b; }
static wstring GuidIn(const wstring& s) { return ExtractGuid(s); }

ProgramScan ScanPrograms() {
    ProgramScan out; const wchar_t* base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall"; struct Src { HKEY root; REGSAM view; const wchar_t* scope; const wchar_t* arch; };
    static const Src srcs[] = {{HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, L"All users", L"64-bit"}, {HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, L"All users", L"32-bit"}, {HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"Current user", L"64-bit"}};
    std::set<wstring> seen; wstring win = Lower(KF(FOLDERID_Windows));
    for (auto& s : srcs) {
        HKEY k; if (RegOpenKeyExW(s.root, base, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | s.view, &k) != ERROR_SUCCESS) continue;
        for (DWORD i = 0, n = 0; n < 5000; ++i, ++n) {
            wchar_t sub[512]; DWORD l = 512; if (RegEnumKeyExW(k, i, sub, &l, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY e; if (RegOpenKeyExW(k, sub, 0, KEY_QUERY_VALUE | s.view, &e) != ERROR_SUCCESS) continue;
            ProgramEntry p; p.name = StrVal(e, L"DisplayName"); wstring rel = StrVal(e, L"ReleaseType"), parent = StrVal(e, L"ParentKeyName");
            bool sys = DwVal(e, L"SystemComponent") == 1; p.uninstallString = StrVal(e, L"UninstallString"); p.quietUninstallString = StrVal(e, L"QuietUninstallString");
            FILETIME lw{}; RegQueryInfoKeyW(e, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &lw);
            p.publisher = StrVal(e, L"Publisher"); p.version = StrVal(e, L"DisplayVersion"); p.installDate = YmdOrEmpty(StrVal(e, L"InstallDate")); if (p.installDate.empty()) p.installDate = FtDate(lw);
            p.installLocation = Trim(StrVal(e, L"InstallLocation")); p.displayIcon = StrVal(e, L"DisplayIcon"); p.sizeBytes = (ULONGLONG)DwVal(e, L"EstimatedSize") * 1024;
            bool msi = DwVal(e, L"WindowsInstaller") == 1 || Lower(p.uninstallString).find(L"msiexec") != wstring::npos; DWORD noRemove = DwVal(e, L"NoRemove"); RegCloseKey(e);
            wstring sn = sub;
            if (p.name.empty() || sys || !parent.empty() || sn.rfind(L"KB", 0) == 0 || rel == L"Update" || rel == L"Hotfix" || rel == L"Security Update" || rel == L"Update Rollup" || rel == L"Service Pack") { ++out.skipped; continue; }
            if (p.uninstallString.empty() && p.quietUninstallString.empty()) { ++out.skipped; continue; }
            if (!seen.insert(Lower(p.name) + L"|" + p.version + L"|" + s.scope + L"|" + s.arch + L"|" + (msi ? L"m" : L"e")).second) { ++out.skipped; continue; }
            p.isMsi = msi; p.type = msi ? L"MSI (Windows Installer)" : L"Desktop app (EXE)"; p.scope = s.scope; p.arch = s.arch; p.keyName = sn; p.root = s.root; p.view = s.view; p.canUninstall = noRemove != 1;
            p.productCode = GuidIn(sn); if (p.productCode.empty()) p.productCode = GuidIn(p.uninstallString);
            p.key = wstring(s.root == HKEY_CURRENT_USER ? L"HKCU" : L"HKLM") + L"|" + (s.view == KEY_WOW64_32KEY ? L"32" : L"64") + L"|" + sn;
            if (p.installLocation.empty()) {   // fall back to the folder of the icon/uninstaller when it is clearly an app folder
                auto pc = ParseCommand(ExpandEnv(p.displayIcon.empty() ? p.uninstallString : p.displayIcon), nullptr); wstring ex = pc.exe; size_t c = ex.find(L','); if (c != wstring::npos && p.displayIcon.size()) ex = ex.substr(0, c);
                size_t sl = ex.find_last_of(L'\\'); if (sl != wstring::npos) { wstring d = Trim(ex.substr(0, sl)); if (FixedDrive(d) && !IsProtectedPath(d)) p.source = L"folder inferred from the uninstaller", p.installLocation = d; } }
            else p.source = L"registry";
            if (!p.sizeBytes && !p.installLocation.empty() && FixedDrive(ExpandEnv(p.installLocation)) && GetFileAttributesW(ExpandEnv(p.installLocation).c_str()) != INVALID_FILE_ATTRIBUTES) { ULONGLONG f = 0; p.sizeBytes = DirBytes(ExpandEnv(p.installLocation), 0, f, 30000); }
            out.entries.push_back(std::move(p));
        }
        RegCloseKey(k);
    }
    out.coverage = L"Listed " + std::to_wstring(out.entries.size()) + L" programs from the Windows uninstall registry (machine 64/32-bit and current user). Hidden " + std::to_wstring(out.skipped) + L" updates, system components and entries without an uninstaller. Microsoft Store apps and portable apps are not covered here.";
    return out;
}

// ---- uninstall ----
static bool FileContains(const wstring& path, const char* needle) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); if (f == INVALID_HANDLE_VALUE) return false;
    std::vector<char> buf(1 << 16); DWORD n; size_t nl = strlen(needle); ULONGLONG total = 0; bool found = false; std::string tail;
    while (!found && total < 4ull << 20 && ReadFile(f, buf.data(), (DWORD)buf.size(), &n, nullptr) && n) { total += n; std::string chunk = tail + std::string(buf.data(), n); if (chunk.find(needle) != std::string::npos) found = true; tail = chunk.size() > nl ? chunk.substr(chunk.size() - nl) : chunk; }
    CloseHandle(f); return found;
}
UninstallPlan PlanUninstall(const ProgramEntry& e, bool silent) {
    UninstallPlan pl; pl.elevate = e.scope == L"All users" && !IsElevated();
    if (!e.canUninstall) { pl.note = L"Windows marks this program as not removable."; return pl; }
    if (e.isMsi && !e.productCode.empty()) { pl.file = L"msiexec.exe"; pl.args = L"/x " + e.productCode + (silent ? L" /qn /norestart" : L""); pl.available = true; pl.note = silent ? L"Windows Installer silent removal (/qn)" : L"Windows Installer"; return pl; }
    wstring raw = silent && !e.quietUninstallString.empty() ? e.quietUninstallString : e.uninstallString;
    wstring ex = ExpandEnv(raw);
    if (Lower(ex).find(L"msiexec") != wstring::npos) {
        size_t sp0 = Lower(ex).find(L"msiexec"), sp = ex.find(L' ', sp0); wstring args = sp == wstring::npos ? L"" : ex.substr(sp + 1);
        pl.file = L"msiexec.exe"; pl.args = MsiRemoveArgs(args, silent); pl.available = true; pl.note = silent ? L"Windows Installer silent removal" : L"Windows Installer"; return pl; }
    auto pc = ParseCommand(ex, [](const wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; });
    if (pc.exe.empty()) { pl.note = L"The uninstall command could not be understood."; return pl; }
    if (GetFileAttributesW(pc.exe.c_str()) == INVALID_FILE_ATTRIBUTES && Lower(pc.exe).find(L"rundll32") == wstring::npos && Lower(pc.exe).find(L"cmd") == wstring::npos) { pl.note = L"The uninstaller file is missing: " + pc.exe + L". Use Force remove."; return pl; }
    pl.file = pc.exe; pl.args = pc.args;
    if (!silent) { pl.available = true; pl.note = L"Official uninstaller (interactive)"; return pl; }
    if (!e.quietUninstallString.empty()) { pl.available = true; pl.note = L"Publisher-provided silent command"; return pl; }
    wstring leaf = Lower(pc.exe.substr(pc.exe.find_last_of(L'\\') + 1));
    if (leaf.rfind(L"unins", 0) == 0 && leaf.size() >= 8 && GetFileAttributesW((pc.exe.substr(0, pc.exe.size() - 4) + L".dat").c_str()) != INVALID_FILE_ATTRIBUTES) { pl.args += L" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART"; pl.available = true; pl.note = L"Inno Setup silent switches"; return pl; }
    if (FileContains(pc.exe, "Nullsoft")) { pl.args += (pl.args.empty() ? L"" : L" ") + wstring(L"/S"); pl.available = true; pl.note = L"NSIS silent switch (/S)"; return pl; }
    pl.note = L"No documented silent switch for this uninstaller. Use the standard (long) method."; return pl;
}
bool ProgramStillInstalled(const ProgramEntry& e) {
    HKEY k; wstring path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + e.keyName; bool present = RegOpenKeyExW(e.root, path.c_str(), 0, KEY_QUERY_VALUE | e.view, &k) == ERROR_SUCCESS; if (present) RegCloseKey(k); return present;
}
void RunUninstall(const ProgramEntry& e, bool silent, UninstallResult& r, std::atomic<bool>* cancel) {
    UninstallPlan pl = PlanUninstall(e, silent); if (!pl.available) { r.note = pl.note; return; }
    if (InSafeMode() && (e.isMsi || _wcsicmp(pl.file.c_str(), L"msiexec.exe") == 0 || pl.file.find(L"msiexec") != wstring::npos)) { wstring se; SvcStartByName(L"msiserver", se); }   // Windows Installer is not started in Safe Mode
    SHELLEXECUTEINFOW sei{sizeof sei}; sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI; sei.lpVerb = pl.elevate ? L"runas" : L"open"; sei.lpFile = pl.file.c_str(); sei.lpParameters = pl.args.empty() ? nullptr : pl.args.c_str(); sei.nShow = silent ? SW_HIDE : SW_SHOWNORMAL;
    wstring dir; size_t sl = pl.file.find_last_of(L'\\'); if (sl != wstring::npos) { dir = pl.file.substr(0, sl); sei.lpDirectory = dir.c_str(); }
    if (!ShellExecuteExW(&sei) || !sei.hProcess) { r.note = L"Could not start the uninstaller (" + Win32Err(GetLastError()) + L")" + (pl.elevate ? L" - the administrator prompt may have been declined." : L""); WriteJournal(e.name, L"uninstall", L"installed", L"unchanged", L"failed: " + r.note, false); return; }
    r.started = true; DWORD waited = 0; while (WaitForSingleObject(sei.hProcess, 500) == WAIT_TIMEOUT) { waited += 500; if (cancel && cancel->load()) { r.note = L"Stopped waiting (the uninstaller keeps running)."; break; } if (waited > 3 * 3600 * 1000) { r.note = L"Gave up waiting after 3 hours."; break; } }
    GetExitCodeProcess(sei.hProcess, &r.exitCode); CloseHandle(sei.hProcess);
    for (int i = 0; i < 40 && ProgramStillInstalled(e); ++i) { Sleep(750); if (cancel && cancel->load()) break; }     // some uninstallers hand off to a child process
    r.removed = !ProgramStillInstalled(e);
    if (r.note.empty()) r.note = r.removed ? L"Uninstalled" : (r.exitCode == 0 ? L"The uninstaller finished but the program is still listed (cancelled or needs a restart)" : L"The uninstaller reported exit code " + std::to_wstring(r.exitCode) + L"; the program is still listed");
    WriteJournal(e.name, wstring(L"uninstall (") + (silent ? L"silent" : L"standard") + L"): " + pl.file + (pl.args.empty() ? L"" : L" " + pl.args), L"installed " + e.version, r.removed ? L"removed" : L"still installed", (r.removed ? L"ok: " : L"not removed: ") + r.note + L" (exit " + std::to_wstring(r.exitCode) + L")", false);
}

// ---- leftovers ----
static bool GenericName(const wstring& n) {
    static const wchar_t* g[] = {L"microsoft", L"windows", L"common files", L"programs", L"packages", L"temp", L"cache", L"data", L"app", L"apps", L"setup", L"installer", L"update", L"updates", L"tools", L"software", L"games", L"office", L"classes", L"policies", L"wow6432node", L"google", L"mozilla", L"adobe", L"intel", L"amd", L"nvidia", L"realtek", L"oracle", L"java"};
    wstring l = Lower(n); if (l.size() < 4) return true; for (auto x : g) if (l == x) return true; return false;
}
static wstring StripVersion(wstring n) { return StripVersionSuffix(n); }
std::vector<CleanItem> FindLeftovers(const ProgramEntry& e, bool includeKey) {
    std::vector<CleanItem> out; std::set<wstring> seen; wstring who = e.name;
    ProgramScan others = ScanPrograms();   // never delete a folder that another installed program still uses
    auto shared = [&](const wstring& dir) { for (auto& o : others.entries) { if (o.key == e.key) continue; wstring ol = ExpandEnv(o.installLocation); if (ol.empty()) continue; if (Under(ol, dir) || Under(dir, ol)) return true; auto pc = ParseCommand(ExpandEnv(o.uninstallString), nullptr); if (Under(pc.exe, dir)) return true; } return false; };
    auto addFolder = [&](const wstring& dir, const wstring& why) {
        if (dir.empty() || !FixedDrive(dir) || IsProtectedPath(dir) || shared(dir)) return; DWORD a = GetFileAttributesW(dir.c_str()); if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY) || (a & FILE_ATTRIBUTE_REPARSE_POINT)) return;
        wstring l = Lower(dir); for (auto& s : seen) if (l == s || l.rfind(s + L"\\", 0) == 0) return; seen.insert(l);
        CleanItem it; it.id = L"lf|" + e.key + L"|" + l; it.name = dir.substr(dir.find_last_of(L'\\') + 1); it.type = L"Leftover: folder of " + who; it.kind = 6; it.risk = 1; ULONGLONG f = 0; it.bytes = DirBytes(dir, 0, f, 500000); it.files = f; it.location = dir; it.detail = std::to_wstring(f) + L" files"; it.why = why + L" May contain data you created; review before deleting.";
        wstring pf = Lower(KF(FOLDERID_ProgramFiles)), pf86 = Lower(KF(FOLDERID_ProgramFilesX86)), pd = Lower(KF(FOLDERID_ProgramData)); it.needsAdmin = l.rfind(pf, 0) == 0 || l.rfind(pf86, 0) == 0 || l.rfind(pd, 0) == 0; out.push_back(std::move(it));
    };
    wstring loc = ExpandEnv(e.installLocation); bool locOk = FixedDrive(loc) && !IsProtectedPath(loc);
    if (locOk) addFolder(loc, L"The program's own install folder is still present after uninstall.");
    { auto pc = ParseCommand(ExpandEnv(e.uninstallString), nullptr); size_t sl = pc.exe.find_last_of(L'\\'); if (sl != wstring::npos) { wstring d = pc.exe.substr(0, sl); if (!locOk || !Under(d, loc)) addFolder(d, L"Folder of the program's uninstaller is still present."); } }
    wstring nm = StripVersion(e.name), pub = StripVersion(e.publisher);
    if (!GenericName(nm)) {
        std::vector<wstring> bases = {KF(FOLDERID_ProgramFiles), KF(FOLDERID_ProgramFilesX86), KF(FOLDERID_ProgramData), KF(FOLDERID_LocalAppData), KF(FOLDERID_RoamingAppData), KF(FOLDERID_LocalAppData) + L"\\Programs"};
        for (auto& b : bases) { if (b.empty()) continue; addFolder(b + L"\\" + nm, L"A folder named after the program exists in a standard application location."); if (!pub.empty() && !GenericName(pub) && Lower(pub) != Lower(nm)) addFolder(b + L"\\" + pub + L"\\" + nm, L"A publisher\\program folder exists in a standard application location."); }
    }
    auto addKey = [&](HKEY root, REGSAM view, const wstring& sub, const wstring& why, int risk, bool admin) {
        HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return; RegCloseKey(k);
        wstring id = L"lk|" + wstring(root == HKEY_CURRENT_USER ? L"HKCU" : L"HKLM") + L"|" + std::to_wstring(view) + L"|" + Lower(sub); if (!seen.insert(id).second) return;
        CleanItem it; it.id = id + L"|" + e.key; it.name = sub.substr(sub.find_last_of(L'\\') + 1); it.type = L"Leftover: registry of " + who; it.kind = 2; it.risk = risk; it.root = root; it.view = view; it.subKey = sub; it.location = wstring(root == HKEY_CURRENT_USER ? L"HKCU\\" : L"HKLM\\") + sub; it.detail = L"key"; it.why = why; it.needsAdmin = admin; out.push_back(std::move(it));
    };
    if (includeKey) addKey(e.root, e.view, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + e.keyName, L"The program's uninstall registration (it is no longer needed once the program is gone).", 1, e.root == HKEY_LOCAL_MACHINE);
    if (!GenericName(nm)) {
        wstring m = Lower(pub); bool msPub = m.find(L"microsoft") != wstring::npos;
        for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) for (REGSAM v : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) {
            if (root == HKEY_CURRENT_USER && v == KEY_WOW64_32KEY) continue; bool adm = root == HKEY_LOCAL_MACHINE;
            addKey(root, v, L"Software\\" + nm, L"A settings key named after the program.", 1, adm);
            if (!msPub && !pub.empty() && !GenericName(pub) && Lower(pub) != Lower(nm)) addKey(root, v, L"Software\\" + pub + L"\\" + nm, L"A settings key under the publisher named after the program.", 1, adm);
        }
    }
    if (locOk) {   // items that point INTO the install folder (hard evidence)
        wstring ll = Lower(loc);
        StartupScan su = ScanStartup();
        for (auto& s : su.entries) {
            wstring prog = ExpandEnv(s.program); if (!Under(prog, loc)) continue;
            if (s.type.rfind(L"Registry", 0) == 0 && s.root) { CleanItem it; it.id = L"ls|" + s.key + L"|" + e.key; it.name = s.name; it.type = L"Leftover: startup entry of " + who; it.kind = 3; it.risk = 1; it.root = s.root; it.view = s.view; it.subKey = s.subKey; it.valueName = s.valueName; it.location = s.location; it.detail = L"value"; it.why = L"Starts a program from the removed install folder."; it.needsAdmin = s.root == HKEY_LOCAL_MACHINE; out.push_back(std::move(it)); }
            else if (s.type == L"Startup folder") { CleanItem it; it.id = L"lsf|" + s.key + L"|" + e.key; it.name = s.name; it.type = L"Leftover: startup shortcut of " + who; it.kind = 0; it.risk = 1; it.ignoreRecent = true; it.paths = {s.location + L"\\" + s.name + L".lnk"}; if (GetFileAttributesW(it.paths[0].c_str()) == INVALID_FILE_ATTRIBUTES) it.paths[0] = s.location + L"\\" + s.name; if (GetFileAttributesW(it.paths[0].c_str()) == INVALID_FILE_ATTRIBUTES) continue; it.location = it.paths[0]; it.detail = L"1 file"; it.files = 1; it.why = L"Startup shortcut that points into the removed install folder."; it.needsAdmin = Under(s.location, KF(FOLDERID_CommonStartup)); out.push_back(std::move(it)); }
            else if (s.type.rfind(L"Scheduled task", 0) == 0 && !s.managedDelay) { CleanItem it; it.id = L"lt|" + s.key + L"|" + e.key; it.name = s.name; it.type = L"Leftover: scheduled task of " + who; it.kind = 9; it.risk = 2; it.cmd = s.taskPath; it.location = s.location; it.detail = L"task"; it.why = L"Runs a program from the removed install folder. The task definition is saved as XML before deletion."; it.needsAdmin = true; out.push_back(std::move(it)); }
            else if (s.type.rfind(L"Explorer add-on", 0) == 0 && s.root) addKey(s.root, s.view, s.subKey, L"Explorer add-on whose file lives in the removed install folder.", 1, true);
        }
        ServiceScan sv = ScanServices(false);
        for (auto& s : sv.entries) if (!s.critical && Under(ExpandEnv(s.program), loc)) { CleanItem it; it.id = L"lv|" + s.name + L"|" + e.key; it.name = s.display; it.type = L"Leftover: service of " + who; it.kind = 8; it.risk = 2; it.cmd = s.name; it.location = L"Service '" + s.name + L"' → " + s.path; it.detail = L"service"; it.why = L"A Windows service whose program is in the removed install folder. Deleting the service cannot be undone automatically."; it.needsAdmin = true; out.push_back(std::move(it)); }
        (void)ll;
    }
    return out;
}

} // namespace fsu
