// Startup inventory + reversible changes. Nothing inspected is ever executed or loaded.
#include "app.h"
#include "../core/text.h"
#include <taskschd.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <set>
#include <map>
#include <fstream>
#include <atomic>
#include <filesystem>
#include <algorithm>

namespace fsu {
using std::wstring;

static const wchar_t* kRun = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kRunOnce = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
static const wchar_t* kApprovedRun = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
static const wchar_t* kApprovedRun32 = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run32";
static const wchar_t* kBlocked = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Blocked";

// ---------- journal (F158) ----------
static std::string ActionType(const wstring& src, const wstring& tgt) {
    wstring s = src + L"|" + tgt; auto has = [&](const wchar_t* x) { return s.find(x) != wstring::npos; };
    if (has(L"Quarantine")) return "quarantine"; if (has(L"Restore point") || has(L"System Restore")) return "recovery"; if (has(L"privacy:")) return "privacy"; if (has(L"Store app")) return "store"; if (has(L"Windows Update removal") || has(L"Driver updates")) return "update";
    if (has(L"registry cleaner")) return "registry"; if (has(L"cleaner") || has(L"deep scan")) return "clean"; if (has(L"File finder")) return "file"; if (has(L"SCM") || has(L"service")) return "service"; if (has(L"process")) return "process";
    if (has(L"maintenance tool") || has(L"Safe Mode")) return "tool"; if (has(L"License")) return "license"; if (has(L"Registry") || has(L"HKCU") || has(L"HKLM") || has(L"Startup") || has(L"task")) return "startup"; return "other";
}
static std::string RollbackText(const std::string& type, bool reversible) {
    if (!reversible) return "Not undoable";
    if (type == "startup" || type == "registry" || type == "clean") return "Recovery > Backups (Restore)"; if (type == "quarantine") return "Recovery > Quarantine (Restore)"; if (type == "file") return "Windows Recycle Bin";
    if (type == "service") return "Set the previous start type on the Services page"; if (type == "store") return "Reinstall from the Microsoft Store"; return "See the Result column";
}
void WriteJournal(const wstring& target, const wstring& source, const wstring& before, const wstring& after, const wstring& result, bool reversible) {
    static std::atomic<unsigned> seq{0}; char idb[24]; snprintf(idb, sizeof idb, "%08llx%02x", (unsigned long long)(GetTickCount64() & 0xFFFFFFFFULL), (unsigned)(++seq & 0xFF));
    JournalEntry j{NowIsoUtc(), L"user", target, source, before, after, result, reversible, idb, ActionType(source, target), ""}; j.rollback = RollbackText(j.type, reversible);
    std::ofstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"), std::ios::app | std::ios::binary);
    if (f) f << JournalLine(j);
}
bool JournalWritable() {
    std::ofstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"), std::ios::app | std::ios::binary);
    return (bool)f;
}

// ---------- first-seen store ----------
struct Seen { wstring time; bool baseline; };
static std::map<wstring, Seen> LoadSeen(bool& existed) {
    std::map<wstring, Seen> m;
    std::ifstream f(std::filesystem::path(DataDir() + L"\\firstseen.tsv"), std::ios::binary);
    existed = (bool)f;
    std::string line;
    while (std::getline(f, line)) {
        auto w = FromUtf8(line); size_t a = w.find(L'\t'), b = w.rfind(L'\t');
        if (a == wstring::npos || a == b) continue;
        m[w.substr(0, a)] = {w.substr(a + 1, b - a - 1), w.substr(b + 1) == L"B"};
    }
    return m;
}
static void SaveSeen(const std::map<wstring, Seen>& m) {
    std::string o; for (auto& kv : m) o += ToUtf8(kv.first + L"\t" + kv.second.time + L"\t" + (kv.second.baseline ? L"B" : L"N")) + "\n";
    AtomicWrite(DataDir() + L"\\firstseen.tsv", o);
}

// ---------- helpers ----------
static bool IsScriptHost(const wstring& prog) {
    wstring l = Lower(prog); size_t s = l.find_last_of(L"\\/"); if (s != wstring::npos) l = l.substr(s + 1);
    static const wchar_t* hosts[] = {L"powershell.exe", L"pwsh.exe", L"wscript.exe", L"cscript.exe", L"cmd.exe", L"mshta.exe"};
    for (auto h : hosts) if (l == h) return true;
    static const wchar_t* ext[] = {L".bat", L".cmd", L".vbs", L".js", L".ps1", L".vbe", L".wsf"};
    for (auto e : ext) { wstring x = e; if (l.size() > x.size() && l.compare(l.size() - x.size(), x.size(), x) == 0) return true; }
    return false;
}
static bool IsMicrosoft(const wstring& pub) { return Lower(pub).find(L"microsoft") != wstring::npos; }

static void Finish(StartupEntry& e) {   // inspect target, build warnings
    auto pc = ParseCommand(ExpandEnv(e.command), FileExistsExpanded);
    e.program = pc.exe;
    wstring warn;
    FileInfo fi;
    if (!pc.exe.empty()) fi = InspectFile(pc.exe);
    e.publisher = fi.publisher; e.version = fi.version;
    e.microsoft = IsMicrosoft(fi.publisher);
    switch (fi.sig) {
        case Sig::Valid: e.signature = L"Valid" + (fi.signer.empty() ? wstring() : L", signed by " + fi.signer) + (fi.catalogSigned ? L" (catalog)" : L""); break;
        case Sig::Unsigned: e.signature = L"Not signed"; break;
        case Sig::Invalid: e.signature = L"Signature present but not trusted (offline check)"; break;
        default: e.signature = fi.exists ? L"Could not check" : L"Not checked (file missing)"; }
    if (!pc.exe.empty() && !fi.exists && !FileExistsExpanded(pc.exe)) warn = L"File missing";
    else if (fi.sig == Sig::Unsigned) warn = L"Unsigned";
    else if (fi.sig == Sig::Invalid) warn = L"Signature not trusted";
    if (IsScriptHost(pc.exe) || IsScriptHost(pc.args)) warn += (warn.empty() ? L"" : L", ") + wstring(L"Script");
    e.warnings = warn; e.hasWarning = !warn.empty();
    static wstring win = [] { wchar_t b[MAX_PATH]; GetWindowsDirectoryW(b, MAX_PATH); return Lower(b); }();
    if (e.microsoft && !e.program.empty() && Lower(ExpandEnv(e.program)).rfind(win, 0) == 0) e.isSystem = true;
}

// ---------- registry Run / RunOnce ----------
static wstring ApprovedState(HKEY root, REGSAM view, const wchar_t* sub, const wstring& name) {
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return L"Enabled";
    BYTE buf[16]{}; DWORD sz = sizeof buf, type = 0;
    LONG r = RegQueryValueExW(k, name.c_str(), nullptr, &type, buf, &sz);
    RegCloseKey(k);
    if (r == ERROR_FILE_NOT_FOUND) return L"Enabled";
    if (r != ERROR_SUCCESS || type != REG_BINARY || sz < 1) return L"Unknown";
    if (buf[0] == 2 || buf[0] == 6) return L"Enabled";
    if (buf[0] == 3 || buf[0] == 7) return L"Disabled";
    return L"Unknown";
}

static void ScanRun(std::vector<StartupEntry>& out, HKEY root, REGSAM view, const wchar_t* sub, bool once, int& scanned, int& denied) {
    HKEY k; LONG r = RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | view, &k);
    if (r == ERROR_FILE_NOT_FOUND) { ++scanned; return; }
    if (r != ERROR_SUCCESS) { ++denied; return; }
    ++scanned;
    bool hk = root == HKEY_CURRENT_USER; bool v32 = view == KEY_WOW64_32KEY;
    for (DWORD i = 0;; ++i) {
        wchar_t name[1024]; DWORD nl = 1024, type = 0, sz = 0;
        if (RegEnumValueW(k, i, name, &nl, nullptr, &type, nullptr, &sz) != ERROR_SUCCESS) break;
        std::vector<BYTE> d(sz + 4, 0); nl = 1024; sz = (DWORD)d.size() - 4;
        if (RegEnumValueW(k, i, name, &nl, nullptr, &type, d.data(), &sz) != ERROR_SUCCESS) continue;
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
        StartupEntry e; e.name = name; e.command = (wchar_t*)d.data(); e.type = once ? L"Registry (RunOnce)" : L"Registry";
        e.starts = L"At sign-in"; e.scope = hk ? L"Current user" : L"All users";
        e.root = root; e.view = view; e.subKey = sub; e.valueName = name; e.runOnce = once;
        e.location = wstring(hk ? L"HKCU" : L"HKLM") + L"\\" + sub + (v32 ? L" [32-bit view]" : L"");
        e.state = once ? L"One-time" : ApprovedState(root, view, v32 && !hk ? kApprovedRun32 : kApprovedRun, name);
        e.toggle = (once || e.state == L"Unknown") ? Toggle::None : Toggle::RunApproved;
        e.key = L"reg|" + e.location + L"|" + Lower(e.name);
        Finish(e); out.push_back(std::move(e));
    }
    RegCloseKey(k);
}

// ---------- startup folders ----------
static bool ResolveLnk(const wstring& lnk, wstring& target, wstring& args) {
    IShellLinkW* sl = nullptr; IPersistFile* pf = nullptr; bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) {
        if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf)) && SUCCEEDED(pf->Load(lnk.c_str(), STGM_READ))) {   // Load only: no Resolve/search, no execution
            wchar_t p[MAX_PATH * 2] = L"", a[2048] = L"";
            sl->GetPath(p, MAX_PATH * 2, nullptr, SLGP_RAWPATH); sl->GetArguments(a, 2048);
            target = p; args = a; ok = !target.empty();
        }
        if (pf) pf->Release(); sl->Release();
    }
    return ok;
}
static void ScanFolder(std::vector<StartupEntry>& out, REFKNOWNFOLDERID id, const wchar_t* scope, int& scanned, int& denied) {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p))) { ++denied; return; }
    wstring dir = p; CoTaskMemFree(p); ++scanned;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        wstring n = fd.cFileName; if (Lower(n) == L"desktop.ini") continue;
        StartupEntry e; wstring full = dir + L"\\" + n, t, a;
        e.name = n.size() > 4 && Lower(n.substr(n.size() - 4)) == L".lnk" ? n.substr(0, n.size() - 4) : n;
        if (Lower(n).size() > 4 && Lower(n).substr(n.size() - 4) == L".lnk" && ResolveLnk(full, t, a)) e.command = L"\"" + t + L"\" " + a; else e.command = L"\"" + full + L"\"";
        e.state = L"Enabled"; e.type = L"Startup folder"; e.starts = L"At sign-in"; e.scope = scope; e.location = dir;
        e.key = L"folder|" + full; Finish(e); out.push_back(std::move(e));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

// ---------- scheduled tasks (boot/logon triggers only, F030) ----------
static void WalkFolder(ITaskFolder* f, std::vector<StartupEntry>& out, int& scanned, int& denied, int depth) {
    if (!f || depth > 8) return;
    IRegisteredTaskCollection* tc = nullptr;
    if (SUCCEEDED(f->GetTasks(TASK_ENUM_HIDDEN, &tc)) && tc) {
        LONG n = 0; tc->get_Count(&n);
        for (LONG i = 1; i <= n; ++i) {
            IRegisteredTask* t = nullptr; VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = i;
            if (FAILED(tc->get_Item(v, &t)) || !t) continue;
            BSTR nm = nullptr, path = nullptr; VARIANT_BOOL en = VARIANT_TRUE; ITaskDefinition* def = nullptr;
            t->get_Name(&nm); t->get_Path(&path); t->get_Enabled(&en);
            if (SUCCEEDED(t->get_Definition(&def)) && def) {
                int startKind = 0; wstring scope = L"All users";
                ITriggerCollection* trs = nullptr;
                if (SUCCEEDED(def->get_Triggers(&trs)) && trs) {
                    LONG tn = 0; trs->get_Count(&tn);
                    for (LONG k = 1; k <= tn; ++k) {
                        ITrigger* tr = nullptr; if (FAILED(trs->get_Item(k, &tr)) || !tr) continue;
                        TASK_TRIGGER_TYPE2 ty; if (SUCCEEDED(tr->get_Type(&ty))) {
                            if (ty == TASK_TRIGGER_LOGON) {
                                startKind = startKind ? startKind : 1;

                            } else if (ty == TASK_TRIGGER_BOOT) startKind = 2;
                        }
                        tr->Release();
                    }
                    trs->Release();
                }
                if (startKind) {
                    StartupEntry e; e.name = nm ? nm : L""; e.taskPath = path ? path : L"";
                    e.type = L"Scheduled task"; e.starts = startKind == 2 ? L"At startup" : L"At sign-in"; e.scope = scope;
                    e.state = en == VARIANT_FALSE ? L"Disabled" : L"Enabled"; e.location = L"Task Scheduler: " + e.taskPath;
                    IActionCollection* acs = nullptr;
                    if (SUCCEEDED(def->get_Actions(&acs)) && acs) {
                        LONG an = 0; acs->get_Count(&an);
                        for (LONG k = 1; k <= an; ++k) {
                            IAction* ac = nullptr; if (FAILED(acs->get_Item(k, &ac)) || !ac) continue;
                            IExecAction* ex = nullptr;
                            if (SUCCEEDED(ac->QueryInterface(IID_IExecAction, (void**)&ex)) && ex) {
                                BSTR p = nullptr, a = nullptr; ex->get_Path(&p); ex->get_Arguments(&a);
                                wstring cmd = p ? p : L""; if (!cmd.empty() && cmd.find(L' ') != wstring::npos && cmd[0] != L'"') cmd = L"\"" + cmd + L"\"";
                                if (a && *a) cmd += wstring(L" ") + a;
                                if (e.command.empty()) e.command = cmd; else e.command += L"  (+ more actions)";
                                if (p) SysFreeString(p); if (a) SysFreeString(a); ex->Release();
                            }
                            ac->Release();
                        }
                        acs->Release();
                    }
                    e.isSystem = e.taskPath.rfind(L"\\Microsoft\\", 0) == 0;
                    if (e.taskPath.rfind(L"\\FeatherSystemUtilities\\", 0) == 0) {
                        e.managedDelay = true; e.type = L"Scheduled task (delayed by Feather)"; e.starts = L"At sign-in (delayed)";
                        IRegistrationInfo* ri = nullptr; BSTR d = nullptr;
                        if (SUCCEEDED(def->get_RegistrationInfo(&ri)) && ri) { ri->get_Description(&d); if (d) { e.delayNote = d; SysFreeString(d); } ri->Release(); }
                    }
                    e.toggle = Toggle::Task; e.key = L"task|" + Lower(e.taskPath);
                    Finish(e); out.push_back(std::move(e));
                }
                def->Release();
            } else ++denied;
            if (nm) SysFreeString(nm); if (path) SysFreeString(path);
            t->Release();
        }
        tc->Release();
    }
    ITaskFolderCollection* fc = nullptr;
    if (SUCCEEDED(f->GetFolders(0, &fc)) && fc) {
        LONG n = 0; fc->get_Count(&n);
        for (LONG i = 1; i <= n; ++i) {
            ITaskFolder* sub = nullptr; VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = i;
            if (SUCCEEDED(fc->get_Item(v, &sub)) && sub) { WalkFolder(sub, out, scanned, denied, depth + 1); sub->Release(); }
        }
        fc->Release();
    }
    ++scanned;
}
static void ScanTasks(std::vector<StartupEntry>& out, int& scanned, int& denied) {
    ITaskService* svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc)) || !svc) { ++denied; return; }
    VARIANT e; VariantInit(&e);
    if (SUCCEEDED(svc->Connect(e, e, e, e))) {
        ITaskFolder* root = nullptr; BSTR r = SysAllocString(L"\\");
        if (SUCCEEDED(svc->GetFolder(r, &root)) && root) { WalkFolder(root, out, scanned, denied, 0); root->Release(); } else ++denied;
        SysFreeString(r);
    } else ++denied;
    svc->Release();
}

// ---------- Explorer add-ons ----------
static wstring RegStr(HKEY root, REGSAM view, const wstring& sub, const wchar_t* val = nullptr) {
    HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return L"";
    wchar_t b[1024]; DWORD sz = sizeof b - 2, ty = 0; wstring r;
    if (RegQueryValueExW(k, val, nullptr, &ty, (LPBYTE)b, &sz) == ERROR_SUCCESS && (ty == REG_SZ || ty == REG_EXPAND_SZ)) { b[sz / 2] = 0; r = b; }
    RegCloseKey(k); return r;
}
static std::vector<wstring> RegSubKeys(HKEY root, REGSAM view, const wstring& sub) {
    std::vector<wstring> v; HKEY k;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_ENUMERATE_SUB_KEYS | view, &k) != ERROR_SUCCESS) return v;
    for (DWORD i = 0;; ++i) { wchar_t n[512]; DWORD l = 512; if (RegEnumKeyExW(k, i, n, &l, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break; v.push_back(n); }
    RegCloseKey(k); return v;
}
static bool BlockedHas(HKEY root, const wstring& clsid) {
    HKEY k; if (RegOpenKeyExW(root, kBlocked, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false;
    bool r = RegQueryValueExW(k, clsid.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; RegCloseKey(k); return r;
}
static void ScanShell(std::vector<StartupEntry>& out, int& scanned, int& denied) {
    struct Src { const wchar_t* path; bool overlay; };
    static const Src srcs[] = {
        {L"Software\\Classes\\*\\shellex\\ContextMenuHandlers", false}, {L"Software\\Classes\\AllFilesystemObjects\\shellex\\ContextMenuHandlers", false},
        {L"Software\\Classes\\Directory\\shellex\\ContextMenuHandlers", false}, {L"Software\\Classes\\Directory\\Background\\shellex\\ContextMenuHandlers", false},
        {L"Software\\Classes\\Folder\\shellex\\ContextMenuHandlers", false}, {L"Software\\Classes\\Drive\\shellex\\ContextMenuHandlers", false},
        {L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers", true}};
    std::set<wstring> seen;
    for (REGSAM view : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) for (auto& s : srcs) {
        auto subs = RegSubKeys(HKEY_LOCAL_MACHINE, view, s.path); ++scanned;
        for (auto& sk : subs) {
            wstring full = wstring(s.path) + L"\\" + sk, id = RegStr(HKEY_LOCAL_MACHINE, view, full);
            if (id.empty() && sk.size() > 2 && sk[0] == L'{') id = sk;
            if (id.empty() || id[0] != L'{') continue;
            wstring dk = view == KEY_WOW64_32KEY ? L"32" : L"64";
            wstring uniq = Lower(id) + (s.overlay ? L"|o|" : L"|m|") + dk;
            if (!seen.insert(uniq).second) continue;
            wstring clsPath = L"Software\\Classes\\CLSID\\" + id;
            wstring dll = RegStr(HKEY_LOCAL_MACHINE, view, clsPath + L"\\InprocServer32");
            wstring cname = RegStr(HKEY_LOCAL_MACHINE, view, clsPath);
            StartupEntry e; e.name = !cname.empty() ? cname : sk; e.clsid = id;
            e.type = s.overlay ? L"Explorer add-on: icon overlay" : L"Explorer add-on: right-click menu";
            e.starts = L"With File Explorer"; e.scope = L"All users"; e.command = dll;
            e.root = HKEY_LOCAL_MACHINE; e.view = view; e.subKey = full;
            e.location = L"HKLM\\" + full + (view == KEY_WOW64_32KEY ? L" [32-bit view]" : L"");
            e.state = BlockedHas(HKEY_LOCAL_MACHINE, id) ? L"Disabled" : L"Enabled";
            e.toggle = Toggle::ShellBlocked; e.key = L"shell|" + uniq;
            Finish(e); out.push_back(std::move(e));
        }
    }
}

StartupScan ScanStartup() {
    StartupScan r; int scanned = 0, denied = 0;
    ScanRun(r.entries, HKEY_CURRENT_USER, KEY_WOW64_64KEY, kRun, false, scanned, denied);
    ScanRun(r.entries, HKEY_CURRENT_USER, KEY_WOW64_64KEY, kRunOnce, true, scanned, denied);
    ScanRun(r.entries, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, kRun, false, scanned, denied);
    ScanRun(r.entries, HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, kRun, false, scanned, denied);
    ScanRun(r.entries, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, kRunOnce, true, scanned, denied);
    ScanFolder(r.entries, FOLDERID_Startup, L"Current user", scanned, denied);
    ScanFolder(r.entries, FOLDERID_CommonStartup, L"All users", scanned, denied);
    ScanTasks(r.entries, scanned, denied);
    ScanShell(r.entries, scanned, denied);
    // first-seen tracking
    bool existed = false; auto seen = LoadSeen(existed); wstring now = NowIsoUtc(); bool changed = false;
    SYSTEMTIME st; GetSystemTime(&st); FILETIME ft; SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER nowU; nowU.LowPart = ft.dwLowDateTime; nowU.HighPart = ft.dwHighDateTime;
    for (auto& e : r.entries) {
        auto it = seen.find(e.key);
        if (it == seen.end()) { seen[e.key] = {now, !existed}; it = seen.find(e.key); changed = true; }
        wstring t = it->second.time; e.baseline = it->second.baseline;
        e.firstSeen = e.baseline ? L"before " + t.substr(0, 10) + L" " + t.substr(11, 5) + L" (it was already there when Feather System Utilities first looked)" : t.substr(0, 10) + L" " + t.substr(11, 5);
        // recent = first seen (non-baseline) within 7 days: compare date strings via FILETIME
        SYSTEMTIME s2{}; s2.wYear = (WORD)_wtoi(t.substr(0, 4).c_str()); s2.wMonth = (WORD)_wtoi(t.substr(5, 2).c_str()); s2.wDay = (WORD)_wtoi(t.substr(8, 2).c_str());
        FILETIME f2; if (SystemTimeToFileTime(&s2, &f2)) { ULARGE_INTEGER u; u.LowPart = f2.dwLowDateTime; u.HighPart = f2.dwHighDateTime; e.recent = !e.baseline && (nowU.QuadPart - u.QuadPart) < 7ULL * 24 * 3600 * 10000000ULL; }
    }
    if (changed) SaveSeen(seen);
    r.coverage = L"Scanned " + std::to_wstring(scanned) + L" locations: Run/RunOnce (both registry views), Startup folders, scheduled tasks with sign-in/boot triggers, Explorer context-menu and icon-overlay add-ons; " +
                 std::to_wstring(denied) + L" unreadable. Not covered: policy Run, Winlogon/AppInit/IFEO/WMI, packaged-app startup, other users. This is not a complete persistence audit.";
    return r;
}

// ---------- changes: preview(UI) -> backup(journal) -> apply -> read-back -> journal ----------
static bool WriteApproved(const StartupEntry& e, bool enable, wstring& err, wstring& before, wstring& after) {
    bool hk = e.root == HKEY_CURRENT_USER;
    const wchar_t* sub = (e.view == KEY_WOW64_32KEY && !hk) ? kApprovedRun32 : kApprovedRun;
    HKEY k; LONG r = RegCreateKeyExW(e.root, sub, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | e.view, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) { err = r == ERROR_ACCESS_DENIED ? L"Administrator rights are required for all-users entries. Use More > Restart as administrator." : Win32Err(r); return false; }
    BYTE cur[16]{}; DWORD sz = sizeof cur, ty = 0;
    LONG q = RegQueryValueExW(k, e.valueName.c_str(), nullptr, &ty, cur, &sz);
    if (q == ERROR_SUCCESS && !(ty == REG_BINARY && sz == 12 && (cur[0] == 2 || cur[0] == 3 || cur[0] == 6 || cur[0] == 7))) {
        RegCloseKey(k); err = L"Windows stores this entry's state in a format this version does not recognise; left unchanged (read-only for safety)."; return false;
    }
    before = q == ERROR_SUCCESS ? (cur[0] == 3 || cur[0] == 7 ? L"Disabled" : L"Enabled") : L"Enabled (no state value)";
    BYTE nv[12]{};
    if (enable) nv[0] = 2; else { nv[0] = 3; FILETIME ft; GetSystemTimeAsFileTime(&ft); memcpy(nv + 4, &ft, 8); }
    r = RegSetValueExW(k, e.valueName.c_str(), 0, REG_BINARY, nv, 12);
    BYTE chk[16]{}; sz = sizeof chk; ty = 0;
    LONG rr = RegQueryValueExW(k, e.valueName.c_str(), nullptr, &ty, chk, &sz);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS) { err = Win32Err(r); return false; }
    bool ok = rr == ERROR_SUCCESS && sz == 12 && (enable ? (chk[0] == 2) : (chk[0] == 3));
    after = ok ? (enable ? L"Enabled" : L"Disabled") : L"Unverified";
    if (!ok) err = L"Change was written but read-back did not match.";
    return ok;
}
static bool WriteTask(const StartupEntry& e, bool enable, wstring& err, wstring& before, wstring& after) {
    ITaskService* svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc))) { err = L"Task Scheduler unavailable."; return false; }
    VARIANT v; VariantInit(&v); bool ok = false;
    if (SUCCEEDED(svc->Connect(v, v, v, v))) {
        ITaskFolder* root = nullptr; BSTR r = SysAllocString(L"\\"), p = SysAllocString(e.taskPath.c_str());
        if (SUCCEEDED(svc->GetFolder(r, &root))) {
            IRegisteredTask* t = nullptr;
            if (SUCCEEDED(root->GetTask(p, &t)) && t) {
                VARIANT_BOOL cur = VARIANT_TRUE; t->get_Enabled(&cur); before = cur == VARIANT_FALSE ? L"Disabled" : L"Enabled";
                HRESULT hr = t->put_Enabled(enable ? VARIANT_TRUE : VARIANT_FALSE);
                if (FAILED(hr)) err = hr == (HRESULT)0x80070005 ? L"Access denied. Administrator rights are required for this task." : L"Task Scheduler refused the change (0x" + std::to_wstring((unsigned)hr) + L").";
                else { VARIANT_BOOL now = VARIANT_TRUE; t->get_Enabled(&now); ok = (now != VARIANT_FALSE) == enable; after = now == VARIANT_FALSE ? L"Disabled" : L"Enabled"; if (!ok) err = L"Read-back did not match."; }
                t->Release();
            } else err = L"Task no longer exists.";
            root->Release();
        } else err = L"Cannot open task folder.";
        SysFreeString(r); SysFreeString(p);
    } else err = L"Cannot connect to Task Scheduler.";
    svc->Release(); return ok;
}
static bool WriteBlocked(const StartupEntry& e, bool enable, wstring& err, wstring& before, wstring& after) {
    HKEY k; LONG r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, kBlocked, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) { err = r == ERROR_ACCESS_DENIED ? L"Administrator rights are required. Use More > Restart as administrator." : Win32Err(r); return false; }
    before = BlockedHas(HKEY_LOCAL_MACHINE, e.clsid) ? L"Disabled" : L"Enabled";
    if (enable) r = RegDeleteValueW(k, e.clsid.c_str()); else { wstring d = e.name; r = RegSetValueExW(k, e.clsid.c_str(), 0, REG_SZ, (const BYTE*)d.c_str(), (DWORD)((d.size() + 1) * 2)); }
    if (r == ERROR_FILE_NOT_FOUND && enable) r = ERROR_SUCCESS;
    RegCloseKey(k);
    if (r != ERROR_SUCCESS) { err = Win32Err(r); return false; }
    bool now = BlockedHas(HKEY_LOCAL_MACHINE, e.clsid); bool ok = now == !enable; after = now ? L"Disabled" : L"Enabled";
    if (!ok) err = L"Read-back did not match."; else err = L"Takes effect after Explorer restarts or you sign out.";
    return ok;
}

bool SetStartupEnabled(const StartupEntry& e, bool enable, wstring& err) {
    if (e.toggle == Toggle::None) { err = L"This kind of entry cannot be changed here."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }   // backup/journal first
    wstring before, after, note; bool ok = false;
    switch (e.toggle) {
        case Toggle::RunApproved: ok = WriteApproved(e, enable, err, before, after); break;
        case Toggle::Task: ok = WriteTask(e, enable, err, before, after); break;
        case Toggle::ShellBlocked: ok = WriteBlocked(e, enable, err, before, after); if (ok) { note = err; err.clear(); } break;
        default: break;
    }
    WriteJournal(e.name, e.location, before, after, ok ? L"ok" + (note.empty() ? wstring() : L": " + note) : L"failed: " + err, true);
    return ok;
}


// ---------- delete / restore / add (registry Run values only) ----------
static wstring BackupDir() { wstring d = DataDir() + L"\\backups"; CreateDirectoryW(d.c_str(), nullptr); return d; }
static wstring Hex(const std::vector<BYTE>& b) { wstring o; for (BYTE x : b) { wchar_t t[3]; swprintf(t, 3, L"%02X", x); o += t; } return o; }
static bool UnHex(const wstring& h, std::vector<BYTE>& b) {
    if (h.size() % 2 || h.size() > 65536) return false;
    for (size_t i = 0; i < h.size(); i += 2) { wchar_t t[3] = {h[i], h[i + 1], 0}; wchar_t* end; unsigned long v = wcstoul(t, &end, 16); if (*end) return false; b.push_back((BYTE)v); }
    return true;
}
static bool IsRunSubKey(const wstring& s) {
    wstring l = Lower(s);
    return l == L"software\\microsoft\\windows\\currentversion\\run" || l == L"software\\microsoft\\windows\\currentversion\\runonce";
}

bool DeleteStartupEntry(const StartupEntry& e, wstring& err) {
    if (!(e.type == L"Registry" || e.type == L"Registry (RunOnce)") || !e.root) { err = L"Only registry Run entries can be deleted in this version."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    HKEY k; LONG r = RegOpenKeyExW(e.root, e.subKey.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE | e.view, &k);
    if (r != ERROR_SUCCESS) { err = r == ERROR_ACCESS_DENIED ? L"Administrator rights are required for all-users entries." : Win32Err(r); return false; }
    DWORD ty = 0, sz = 0;
    if (RegQueryValueExW(k, e.valueName.c_str(), nullptr, &ty, nullptr, &sz) != ERROR_SUCCESS) { RegCloseKey(k); err = L"The entry no longer exists."; return false; }
    std::vector<BYTE> data(sz); DWORD sz2 = sz;
    if (RegQueryValueExW(k, e.valueName.c_str(), nullptr, &ty, data.data(), &sz2) != ERROR_SUCCESS) { RegCloseKey(k); err = L"Could not read the entry for backup."; return false; }
    data.resize(sz2);
    wstring rec = L"FSUBACKUP1\nroot=" + wstring(e.root == HKEY_CURRENT_USER ? L"HKCU" : L"HKLM") + L"\nview=" + (e.view == KEY_WOW64_32KEY ? L"32" : L"64") +
                  L"\nsubkey=" + e.subKey + L"\nname=" + e.valueName + L"\ntype=" + std::to_wstring(ty) + L"\ntime=" + NowIsoUtc() + L"\ndata=" + Hex(data) + L"\n";
    wstring fn = NowIsoUtc(); for (auto& c : fn) if (c == L':') c = L'-';
    wstring path = BackupDir() + L"\\" + fn + L"_" + std::to_wstring(GetTickCount() % 100000) + L".fsub";
    if (!AtomicWrite(path, ToUtf8(rec))) { RegCloseKey(k); err = L"Backup could not be written; nothing was deleted."; return false; }   // abort if backup fails
    r = RegDeleteValueW(k, e.valueName.c_str());
    bool gone = r == ERROR_SUCCESS && RegQueryValueExW(k, e.valueName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_FILE_NOT_FOUND;
    RegCloseKey(k);
    if (!gone) err = r != ERROR_SUCCESS ? Win32Err(r) : L"Read-back shows the entry still exists.";
    WriteJournal(e.name, e.location, L"present: " + e.command, gone ? L"deleted (backup: " + path + L")" : L"unchanged", gone ? L"ok" : L"failed: " + err, true);
    return gone;
}

std::vector<BackupInfo> ListBackups() {
    std::vector<BackupInfo> v;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((BackupDir() + L"\\*.fsub").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return v;
    do { BackupInfo b; b.path = BackupDir() + L"\\" + fd.cFileName; b.label = fd.cFileName; b.written = fd.ftLastWriteTime; v.push_back(b); } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(v.begin(), v.end(), [](const BackupInfo& a, const BackupInfo& b) { return CompareFileTime(&a.written, &b.written) > 0; });
    if (v.size() > 20) v.resize(20);
    for (auto& b : v) {   // friendly label from the file (bounded read)
        std::ifstream f(std::filesystem::path(b.path), std::ios::binary); std::string line;
        wstring name, time; int n = 0;
        while (n++ < 10 && std::getline(f, line)) { auto w = FromUtf8(line); if (!w.empty() && w.back() == L'\r') w.pop_back();
            if (w.rfind(L"name=", 0) == 0) name = w.substr(5); else if (w.rfind(L"time=", 0) == 0) time = w.substr(5); }
        if (!name.empty()) b.label = name + L"   (deleted " + (time.size() >= 16 ? time.substr(0, 10) + L" " + time.substr(11, 5) : time) + L")";
    }
    return v;
}

bool RestoreBackup(const wstring& path, wstring& err, wstring& summary) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) { err = L"Cannot open the backup."; return false; }
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (all.size() > 100000) { err = L"Backup file is unexpectedly large; refusing."; return false; }   // F214
    std::map<wstring, wstring> kv; size_t pos = 0; bool first = true;
    while (pos < all.size()) {
        size_t nl = all.find('\n', pos); std::string ln = all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos); pos = nl == std::string::npos ? all.size() : nl + 1;
        auto w = FromUtf8(ln); if (!w.empty() && w.back() == L'\r') w.pop_back();
        if (first) { if (w != L"FSUBACKUP1") { err = L"Not a valid backup file."; return false; } first = false; continue; }
        size_t eq = w.find(L'='); if (eq != wstring::npos) kv[w.substr(0, eq)] = w.substr(eq + 1);
    }
    if (!kv.count(L"root") || !kv.count(L"subkey") || !kv.count(L"name") || !kv.count(L"type") || !kv.count(L"data")) { err = L"Backup is incomplete."; return false; }
    HKEY root = kv[L"root"] == L"HKCU" ? HKEY_CURRENT_USER : kv[L"root"] == L"HKLM" ? HKEY_LOCAL_MACHINE : nullptr;
    DWORD type = (DWORD)_wtoi(kv[L"type"].c_str()); REGSAM view = kv[L"view"] == L"32" ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
    std::vector<BYTE> data;
    if (!root || !IsRunSubKey(kv[L"subkey"]) || (type != REG_SZ && type != REG_EXPAND_SZ) || kv[L"name"].empty() || kv[L"name"].size() > 256 || !UnHex(kv[L"data"], data)) { err = L"Backup failed validation; it was not applied."; return false; }
    HKEY k; LONG r = RegCreateKeyExW(root, kv[L"subkey"].c_str(), 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | view, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) { err = r == ERROR_ACCESS_DENIED ? L"Administrator rights are required to restore an all-users entry." : Win32Err(r); return false; }
    if (RegQueryValueExW(k, kv[L"name"].c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) { RegCloseKey(k); err = L"An entry with this name already exists now; not overwritten."; return false; }
    if (!JournalWritable()) { RegCloseKey(k); err = L"Cannot write the change journal; nothing was changed."; return false; }
    r = RegSetValueExW(k, kv[L"name"].c_str(), 0, type, data.data(), (DWORD)data.size());
    std::vector<BYTE> chk(data.size() + 4); DWORD cs = (DWORD)chk.size(), ct = 0;
    bool ok = r == ERROR_SUCCESS && RegQueryValueExW(k, kv[L"name"].c_str(), nullptr, &ct, chk.data(), &cs) == ERROR_SUCCESS && ct == type && cs == data.size() && memcmp(chk.data(), data.data(), cs) == 0;
    RegCloseKey(k);
    if (!ok) err = r != ERROR_SUCCESS ? Win32Err(r) : L"Read-back did not match.";
    summary = kv[L"name"];
    WriteJournal(kv[L"name"], kv[L"root"] + L"\\" + kv[L"subkey"], L"absent", ok ? L"restored" : L"unchanged", ok ? L"ok (restore)" : L"failed: " + err, true);
    return ok;
}

bool AddStartupEntry(const wstring& name, const wstring& command, bool allUsers, wstring& err) {
    if (name.empty() || name.size() > 200 || name.find(L'\\') != wstring::npos) { err = L"Enter a name (no backslashes, up to 200 characters)."; return false; }
    if (command.empty() || command.size() > 1900) { err = L"Enter a command."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY k; LONG r = RegCreateKeyExW(root, kRun, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr);
    if (r != ERROR_SUCCESS) { err = r == ERROR_ACCESS_DENIED ? L"Administrator rights are required for all-users entries." : Win32Err(r); return false; }
    if (RegQueryValueExW(k, name.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) { RegCloseKey(k); err = L"An entry with this name already exists."; return false; }
    r = RegSetValueExW(k, name.c_str(), 0, REG_SZ, (const BYTE*)command.c_str(), (DWORD)((command.size() + 1) * 2));
    bool ok = r == ERROR_SUCCESS && RegQueryValueExW(k, name.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(k);
    if (!ok) err = Win32Err(r);
    WriteJournal(name, wstring(allUsers ? L"HKLM\\" : L"HKCU\\") + kRun, L"absent", ok ? command : L"unchanged", ok ? L"ok (add)" : L"failed: " + err, true);
    return ok;
}

// ---------- delayed launch (managed per-user scheduled task; original is disabled, never deleted) ----------
static wstring XmlEsc(const wstring& s) { wstring o; for (wchar_t c : s) { if (c == L'&') o += L"&amp;"; else if (c == L'<') o += L"&lt;"; else if (c == L'>') o += L"&gt;"; else if (c == L'"') o += L"&quot;"; else if (c == L'\'') o += L"&apos;"; else o += c; } return o; }
static wstring CurrentUserSam() { wchar_t u[256], d[256]; DWORD n = 256, m = 256; GetUserNameW(u, &n); GetEnvironmentVariableW(L"USERDOMAIN", d, m); return wstring(d) + L"\\" + u; }

bool CanDelay(const StartupEntry& e) { return e.type == L"Registry" && e.root == HKEY_CURRENT_USER && e.state == L"Enabled" && !e.runOnce; }

static ITaskFolder* OpenRoot(ITaskService** psvc) {
    ITaskService* svc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc)) || !svc) return nullptr;
    VARIANT v; VariantInit(&v);
    if (FAILED(svc->Connect(v, v, v, v))) { svc->Release(); return nullptr; }
    ITaskFolder* root = nullptr; BSTR r = SysAllocString(L"\\"); HRESULT hr = svc->GetFolder(r, &root); SysFreeString(r);
    if (FAILED(hr)) { svc->Release(); return nullptr; }
    *psvc = svc; return root;
}

bool DelayStartup(const StartupEntry& e, int seconds, wstring& err) {
    if (!CanDelay(e)) { err = L"Only enabled current-user Run entries can be delayed."; return false; }
    if (seconds < 5 || seconds > 3600) { err = L"Delay must be 5 to 3600 seconds."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    auto pc = ParseCommand(ExpandEnv(e.command), FileExistsExpanded);
    if (pc.exe.empty()) { err = L"The command could not be understood; not delayed."; return false; }
    wstring workDir; size_t sl = pc.exe.find_last_of(L"\\/"); if (sl != wstring::npos) workDir = pc.exe.substr(0, sl);
    wstring safe = e.name; for (auto& c : safe) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_';
    wstring taskName = L"Delay - " + safe, user = CurrentUserSam();
    wstring marker = L"FSU-DELAY1|HKCU|" + wstring(e.view == KEY_WOW64_32KEY ? L"32" : L"64") + L"|" + e.subKey + L"|" + e.valueName + L"|" + e.command;
    wstring xml = L"<?xml version=\"1.0\" encoding=\"UTF-16\"?><Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\"><RegistrationInfo><Description>" + XmlEsc(marker) +
        L"</Description><Author>Feather System Utilities</Author></RegistrationInfo><Triggers><LogonTrigger><Enabled>true</Enabled><UserId>" + XmlEsc(user) + L"</UserId><Delay>PT" + std::to_wstring(seconds) +
        L"S</Delay></LogonTrigger></Triggers><Principals><Principal id=\"A\"><UserId>" + XmlEsc(user) + L"</UserId><LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>"
        L"<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
        L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit><StartWhenAvailable>true</StartWhenAvailable><Enabled>true</Enabled></Settings><Actions Context=\"A\"><Exec><Command>" + XmlEsc(pc.exe) + L"</Command>" +
        (pc.args.empty() ? L"" : L"<Arguments>" + XmlEsc(pc.args) + L"</Arguments>") + (workDir.empty() ? L"" : L"<WorkingDirectory>" + XmlEsc(workDir) + L"</WorkingDirectory>") + L"</Exec></Actions></Task>";
    ITaskService* svc = nullptr; ITaskFolder* root = OpenRoot(&svc);
    if (!root) { err = L"Task Scheduler is unavailable."; return false; }
    ITaskFolder* f = nullptr; BSTR fn = SysAllocString(L"\\FeatherSystemUtilities");
    if (FAILED(root->GetFolder(fn, &f))) { VARIANT sd; VariantInit(&sd); BSTR nn = SysAllocString(L"FeatherSystemUtilities"); root->CreateFolder(nn, sd, &f); SysFreeString(nn); }
    SysFreeString(fn);
    bool ok = false;
    if (!f) err = L"Could not create the task folder (try again; no changes were made).";
    else {
        BSTR tn = SysAllocString(taskName.c_str()), tx = SysAllocString(xml.c_str()); VARIANT e1, e2, e3; VariantInit(&e1); VariantInit(&e2); VariantInit(&e3);
        IRegisteredTask* rt = nullptr; HRESULT hr = f->RegisterTask(tn, tx, TASK_CREATE_OR_UPDATE, e1, e2, TASK_LOGON_INTERACTIVE_TOKEN, e3, &rt);
        SysFreeString(tn); SysFreeString(tx);
        if (FAILED(hr) || !rt) err = L"Task Scheduler refused the delayed task (0x" + std::to_wstring((unsigned)hr) + L"). The original entry is unchanged.";
        else {
            rt->Release(); wstring e2s;
            if (SetStartupEnabled(e, false, e2s)) ok = true;
            else { err = L"Could not disable the original entry: " + e2s; VARIANT_BOOL dummy; (void)dummy; BSTR tn2 = SysAllocString(taskName.c_str()); f->DeleteTask(tn2, 0); SysFreeString(tn2); }   // compensate
        }
        f->Release();
    }
    root->Release(); svc->Release();
    WriteJournal(e.name, L"delayed launch", L"Run key (enabled)", ok ? L"managed task, " + std::to_wstring(seconds) + L" s after sign-in; original disabled" : L"unchanged", ok ? L"ok" : L"failed: " + err, true);
    return ok;
}

bool RemoveDelay(const StartupEntry& t, wstring& err) {
    if (!t.managedDelay) { err = L"Not a delayed entry made by this app."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    // marker: FSU-DELAY1|HKCU|64|subKey|valueName|command
    std::vector<wstring> p; size_t pos = 0; wstring d = t.delayNote;
    for (int i = 0; i < 5; ++i) { size_t n = d.find(L'|', pos); if (n == wstring::npos) break; p.push_back(d.substr(pos, n - pos)); pos = n + 1; } p.push_back(d.substr(pos));
    if (p.size() < 6 || p[0] != L"FSU-DELAY1" || p[1] != L"HKCU" || !IsRunSubKey(p[3])) { err = L"The task's restore information is missing or invalid; use Delete in Task Scheduler instead."; return false; }
    StartupEntry orig; orig.root = HKEY_CURRENT_USER; orig.view = p[2] == L"32" ? KEY_WOW64_32KEY : KEY_WOW64_64KEY; orig.subKey = p[3]; orig.valueName = p[4]; orig.name = p[4]; orig.location = L"HKCU\\" + p[3];
    orig.type = L"Registry"; orig.toggle = Toggle::RunApproved; orig.command = p[5];
    HKEY k; bool exists = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, orig.subKey.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE | orig.view, &k) == ERROR_SUCCESS) {
        exists = RegQueryValueExW(k, orig.valueName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        if (!exists) RegSetValueExW(k, orig.valueName.c_str(), 0, REG_SZ, (const BYTE*)orig.command.c_str(), (DWORD)((orig.command.size() + 1) * 2));   // original was deleted meanwhile: recreate it
        RegCloseKey(k);
    }
    wstring e2; if (!SetStartupEnabled(orig, true, e2)) { err = L"Could not re-enable the original entry: " + e2 + L" The delay task was kept so nothing is lost."; return false; }
    ITaskService* svc = nullptr; ITaskFolder* root = OpenRoot(&svc); bool ok = false;
    if (root) { BSTR tp = SysAllocString(t.taskPath.c_str()); ok = SUCCEEDED(root->DeleteTask(tp, 0)); SysFreeString(tp); root->Release(); svc->Release(); }
    if (!ok) err = L"The original was restored but the delay task could not be deleted; remove it in Task Scheduler.";
    WriteJournal(t.name, L"delayed launch", L"managed task", ok ? L"original restored, task removed" : L"original restored, task remains", ok ? L"ok" : L"partial: " + err, false);
    return ok;
}

int CleanupManagedTasks(wstring& report) {
    int n = 0; auto scan = ScanStartup();
    for (auto& e : scan.entries) if (e.managedDelay) { wstring err; if (RemoveDelay(e, err)) ++n; else report += e.name + L": " + err + L"\n"; }
    ITaskService* svc = nullptr; ITaskFolder* root = OpenRoot(&svc);
    if (root) { BSTR fn = SysAllocString(L"FeatherSystemUtilities"); root->DeleteFolder(fn, 0); SysFreeString(fn); root->Release(); svc->Release(); }   // only succeeds if empty
    return n;
}

} // namespace fsu
