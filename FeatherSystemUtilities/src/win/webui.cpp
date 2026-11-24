// WebView2 host: shows the HTML/CSS UI (ui/index.html, embedded) and bridges its requests to the C++ engine.
// Every mutating request refers to items from the engine's own last scan (key or generation+index); the page can never name a raw path to delete.
#include "app.h"
#include "json.h"
#include "../core/text.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <WebView2.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wbemidl.h>
#include <wuapi.h>
#include <commdlg.h>
#include <mutex>
#include <tuple>
#include <set>
#include <fstream>
#include <filesystem>
#include <algorithm>

extern const unsigned char kUiHtml[];
extern const unsigned long kUiHtmlSize;
extern const unsigned char kWhatsNewJson[];
extern const unsigned long kWhatsNewJsonSize;

namespace fsu {
using std::wstring; using std::string;

static HWND g_wnd; static ICoreWebView2Controller* g_ctl; static ICoreWebView2* g_wv; static bool g_ready, g_failed; static HANDLE* g_instMutex;
enum { WM_WV_POST = WM_APP + 40, WM_WV_FAIL, WM_WV_PAGE, WM_WV_RELOAD }; static wstring g_html;
static std::mutex g_m;
static std::atomic<bool> g_cancel{false}; static std::atomic<int> g_stGen{0};
static StartupScan g_su; static ProgramScan g_pg; static ServiceScan g_sv; static CleanScan g_cl; static int g_clGen; static std::vector<CleanItem> g_left; static int g_leftGen; static FileScan g_fs[4]; static int g_fsGen[4]; static std::vector<BrowserProfile> g_priv; static int g_privGen; static std::set<string> g_store, g_hot; static std::vector<DriverRec> g_drv;

static string U(const wstring& w) { return ToUtf8(w); }
static wstring W(const string& s) { return FromUtf8(s); }
static string Q(const wstring& w) { return js::Q(U(w)); }
static string Err(const wstring& e) { return js::Obj().boolean("ok", false).str("error", U(e)).done(); }

static wstring LowerW(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static bool HasAny(const wstring& hay, std::initializer_list<const wchar_t*> needles) { for (auto* n : needles) if (hay.find(n) != wstring::npos) return true; return false; }
static void ClassifyStartup(const StartupEntry& e, string& kind, wstring& advice) {
    wstring s = LowerW(e.name + L" " + e.publisher + L" " + e.command); kind.clear(); advice.clear();
    if (HasAny(s, {L"defender", L"windows security", L"securityhealth", L"antivirus", L"kaspersky", L"norton", L"mcafee", L"avast", L"bitdefender", L"eset", L"malwarebytes", L"sophos", L"trend micro", L"crowdstrike", L"webroot", L"f-secure"})) { kind = "security"; advice = L"Security software. Disabling it can leave this PC without real-time protection."; }
    else if (HasAny(s, {L"narrator", L"magnif", L"on-screen keyboard", L"osk.exe", L"utilman", L"voiceaccess", L"nvda", L"jaws"})) { kind = "accessibility"; advice = L"Accessibility tool. Disabling it may remove features someone relies on."; }
    else if (HasAny(s, {L"onedrive", L"dropbox", L"google drive", L"googledrive", L"icloud", L"backup", L"acronis", L"veeam", L"sync"})) { kind = "backup"; advice = L"Backup or sync software. Disabling it can stop your files from being backed up or synced."; }
    else if (HasAny(s, {L"realtek", L"nvidia", L"radeon", L"synaptics", L"touchpad", L"logitech", L"razer", L"corsair", L"steelseries", L"bluetooth", L"wacom", L"hotkey", L"intel(r)", L"intel corporation", L"advanced micro devices"})) { kind = "device"; advice = L"Device software. Disabling it can disable hotkeys, touchpad gestures, audio or graphics controls."; }
}

// ---------- COM callback helper ----------
// MinGW cannot resolve __uuidof for the WebView2 header's interfaces, so the IIDs are listed here (values from WebView2.h).
static GUID ParseGuid(const char* t) { unsigned a, b, c, d[8]; sscanf(t, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]); GUID g{a, (unsigned short)b, (unsigned short)c, {}}; for (int i = 0; i < 8; ++i) g.Data4[i] = (unsigned char)d[i]; return g; }
template <class T> struct IidOf;
#define FSU_IID(T, S) template <> struct IidOf<T> { static const GUID& get() { static GUID g = ParseGuid(S); return g; } };
FSU_IID(ICoreWebView2ProcessFailedEventHandler, "79e0aea4-990b-42d9-aa1d-0fcc2e5bc7f1")
FSU_IID(ICoreWebView2Settings3, "fdb5ab74-af33-4854-84f0-0a631deb5eba")
FSU_IID(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, "6c4819f3-c9b7-4260-8127-c9f5bde7f68c")
FSU_IID(ICoreWebView2NavigationCompletedEventHandler, "d33a35bf-1c49-4f98-93ab-006e0533fe1c")
FSU_IID(ICoreWebView2NewWindowRequestedEventHandler, "d4c185fe-c81c-4989-97af-2d3fa7ab5651")
FSU_IID(ICoreWebView2NavigationStartingEventHandler, "9adbe429-f36d-432b-9ddc-f8881fbd76e3")
FSU_IID(ICoreWebView2WebMessageReceivedEventHandler, "57213f19-00e6-49fa-8e07-898ea01ecbd2")
FSU_IID(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, "4e8a3389-c9d8-4bd2-b6b5-124fee6cc14d")

template <class I, class... A> struct Cb : I {
    std::function<HRESULT(A...)> fn; LONG rc = 1;
    virtual ~Cb() = default;
    explicit Cb(std::function<HRESULT(A...)> f) : fn(std::move(f)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID r, void** p) override { if (IsEqualIID(r, IidOf<I>::get()) || IsEqualIID(r, IID_IUnknown)) { *p = static_cast<I*>(this); AddRef(); return S_OK; } *p = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG n = InterlockedDecrement(&rc); if (!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(A... a) override { return fn(a...); }
};

// ---------- sending to the page ----------
static void PostToPage(const string& json) { if (!g_wnd) return; PostMessageW(g_wnd, WM_WV_POST, 0, (LPARAM) new wstring(W(json))); }
static void Emit(const char* ev, const string& data) { PostToPage(js::Obj().str("event", ev).raw("data", data).done()); }
static void Progress(const wstring& m) { Emit("progress", js::Obj().str("text", U(m)).done()); }

// ---------- JSON builders for engine structs ----------
static bool ProtectedProgram(const ProgramEntry& e, wstring& why) {
    wstring s = LowerW(e.name + L" " + e.publisher);
    if (HasAny(s, {L"defender", L"antivirus", L"anti-virus", L"endpoint", L"kaspersky", L"norton", L"mcafee", L"avast", L"bitdefender", L"eset ", L"malwarebytes", L"sophos", L"trend micro", L"crowdstrike", L"webroot"})) { why = L"a security product"; return true; }
    if (HasAny(s, {L" driver", L"chipset", L"graphics driver", L"management engine", L"rapid storage", L"nvidia graphics", L"amd software", L"radeon", L"realtek audio", L"realtek high definition"})) { why = L"a driver or hardware component"; return true; }
    if (HasAny(s, {L"visual c++", L"microsoft .net", L"windows sdk", L"webview2", L"directx", L"vcredist", L"windows software development kit"})) { why = L"a system component other programs depend on"; return true; }
    return false;
}
static bool CanDeleteEntry(const StartupEntry& e) {
    if (e.managedDelay) return false;
    return e.type == L"Registry" || e.type == L"Registry (RunOnce)" || e.type.rfind(L"Scheduled task", 0) == 0 || e.type.rfind(L"Explorer add-on", 0) == 0 || e.type == L"Startup folder";
}
static string CleanItemJson(const CleanItem& c, size_t i) {
    return js::Obj().num("i", (double)i).str("id", U(c.id.empty() ? c.name : c.id)).str("name", U(c.name)).str("type", U(c.type)).str("detail", U(c.detail)).str("why", U(c.why)).str("loc", U(c.location)).num("bytes", (double)c.bytes).num("files", (double)c.files)
        .num("risk", c.risk).num("kind", c.kind).boolean("admin", c.needsAdmin).done();
}
static string CleanJson() {
    js::Arr a; for (size_t i = 0; i < g_cl.items.size(); ++i) a.add(CleanItemJson(g_cl.items[i], i));
    return js::Obj().num("gen", g_clGen).raw("items", a.done()).str("coverage", U(g_cl.coverage)).num("totalBytes", (double)g_cl.totalBytes).num("registry", g_cl.registryCount).done();
}
static string StartupJson(const StartupEntry& e) {
    wstring prog = e.program; bool exists = prog.empty() ? true : FileExistsExpanded(prog); string kindS; wstring advice; ClassifyStartup(e, kindS, advice); Advice adv = AdviceForStartup(e, !exists);
    return js::Obj().str("key", U(e.key)).str("name", U(e.name)).str("state", U(e.state)).str("type", U(e.type)).str("starts", U(e.starts)).str("scope", U(e.scope)).str("command", U(e.command)).str("program", U(e.program))
        .str("publisher", U(e.publisher)).str("version", U(e.version)).str("signature", U(e.signature)).str("warnings", U(e.warnings)).str("location", U(e.location)).str("firstSeen", U(e.firstSeen))
        .boolean("canToggle", e.toggle != Toggle::None && !e.managedDelay).boolean("canDelete", CanDeleteEntry(e)).boolean("canDelay", CanDelay(e)).boolean("microsoft", e.microsoft).boolean("system", e.isSystem).boolean("recent", e.recent)
        .boolean("missing", !exists).str("exe", U(LowerW(prog.substr(prog.find_last_of(L"\\/") == wstring::npos ? 0 : prog.find_last_of(L"\\/") + 1)))).boolean("delayed", e.managedDelay).boolean("needsAdmin", e.root == HKEY_LOCAL_MACHINE || e.scope.find(L"All") != wstring::npos).str("adv", U(adv.verdict)).str("advWhy", U(adv.reason)).str("advSrc", U(adv.source)).str("kind", kindS).str("advice", U(advice)).str("details", U(StartupDetailsText(e))).done();
}
static string ProgramJson(const ProgramEntry& e) {
    UninstallPlan s = PlanUninstall(e, true); wstring why; bool prot = ProtectedProgram(e, why);
    return js::Obj().str("key", U(e.key)).str("name", U(e.name)).str("publisher", U(e.publisher)).str("version", U(e.version)).str("date", U(e.installDate)).num("size", (double)e.sizeBytes).str("type", U(e.type)).str("scope", U(e.scope))
        .str("location", U(e.installLocation)).boolean("canUninstall", e.canUninstall).boolean("silent", s.available).str("silentNote", U(s.note)).str("uninstall", U(e.uninstallString)).str("quiet", U(e.quietUninstallString)).str("source", U(e.source.empty() ? wstring(e.root == HKEY_CURRENT_USER ? L"HKCU" : L"HKLM") + L"\\...\\Uninstall\\" + e.keyName : e.source)).boolean("protected", prot).str("why", U(why)).done();
}
static string ServiceJson(const ServiceEntry& s) {
    js::Arr d; for (auto& x : s.deps) d.add(Q(x)); Advice adv = AdviceForService(s);
    return js::Obj().str("name", U(s.name)).str("display", U(s.display)).str("state", U(s.state)).str("start", U(s.start)).str("safety", U(s.safety)).str("account", U(s.account)).str("description", U(s.description)).str("warnings", U(s.warnings))
        .str("path", U(s.path)).str("publisher", U(s.publisher)).str("signature", U(s.signature)).num("pid", s.pid).boolean("running", s.stateCode == SERVICE_RUNNING).boolean("stopped", s.stateCode == SERVICE_STOPPED)
        .boolean("canStop", (s.controls & SERVICE_ACCEPT_STOP) != 0).boolean("critical", s.critical).boolean("driver", s.isDriver).boolean("needsAdmin", true).str("adv", U(adv.verdict)).str("advWhy", U(adv.reason)).str("advSrc", U(adv.source)).raw("deps", d.done()).done();
}

// ---------- startup deletion (preview/confirmation is done in the page; backups by the engine) ----------
static bool DeleteStartupCore(const StartupEntry& e, wstring& err) {
    if (!CanDeleteEntry(e)) { err = L"This entry type cannot be deleted here."; return false; }
    if (e.type == L"Registry" || e.type == L"Registry (RunOnce)") return DeleteStartupEntry(e, err);
    if (e.type == L"Startup folder") {
        wstring path = e.key.size() > 7 ? e.key.substr(7) : L""; bool done = !path.empty() && RecycleFiles(g_wnd, {path}, err) && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES;
        if (!done && err.empty()) err = L"The shortcut could not be moved to the Recycle Bin.";
        WriteJournal(e.name, e.location, L"present", done ? L"moved to Recycle Bin" : L"unchanged", done ? L"ok (restorable from Recycle Bin)" : L"failed: " + err, true); return done;
    }
    CleanItem it; it.name = e.name; it.location = e.location; it.risk = 1;
    if (e.type.rfind(L"Scheduled task", 0) == 0) { it.kind = 9; it.cmd = e.taskPath; } else { it.kind = 2; it.root = e.root; it.view = e.view; it.subKey = e.subKey; it.detail = L"key"; it.why = L"Explorer add-on registration"; it.needsAdmin = e.root == HKEY_LOCAL_MACHINE; }
    CleanResult r; RunClean({it}, false, r); bool done = r.ok == 1 && r.failed == 0 && r.skipped == 0; if (!done) err = r.firstErr.empty() ? L"Nothing was changed and Windows reported no error. The entry may already be gone; refresh the list. (Details are in the Journal.)" : r.firstErr; return done;
}

// ---------- storage map: sizes of the immediate children of a folder ----------
static ULONGLONG TreeSize(const wstring& dir, ULONGLONG& files, int gen, int depth = 0) {
    if (gen != g_stGen.load() || depth > 40) return 0; ULONGLONG total = 0; WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH); if (h == INVALID_HANDLE_VALUE) return 0;
    do { if (fd.cFileName[0] == L'.' && (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) total += TreeSize(dir + L"\\" + fd.cFileName, files, gen, depth + 1);
        else { total += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; ++files; }
        if (gen != g_stGen.load()) break;
    } while (FindNextFileW(h, &fd)); FindClose(h); return total;
}


// ---------- drivers (read via WMI) and driver updates (via the Windows Update Agent) ----------
static wstring VStr(VARIANT& v) { return v.vt == VT_BSTR && v.bstrVal ? wstring(v.bstrVal) : wstring(); }
static string DriversListJson(wstring& err) {
    IWbemLocator* loc = nullptr; IWbemServices* svc = nullptr; IEnumWbemClassObject* en = nullptr; js::Arr arr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&loc)) || !loc) { err = L"WMI is not available."; return ""; }
    BSTR ns = SysAllocString(L"ROOT\\CIMV2"); HRESULT hr = loc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &svc); SysFreeString(ns);
    if (FAILED(hr) || !svc) { loc->Release(); err = L"Could not connect to WMI."; return ""; }
    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    BSTR ql = SysAllocString(L"WQL"), q = SysAllocString(L"SELECT DeviceName,Manufacturer,DriverVersion,DriverDate,DriverProviderName,InfName,IsSigned,DeviceClass FROM Win32_PnPSignedDriver");
    hr = svc->ExecQuery(ql, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en); SysFreeString(ql); SysFreeString(q);
    if (SUCCEEDED(hr) && en) { IWbemClassObject* o = nullptr; ULONG n = 0; int i = 0;
        while (en->Next(WBEM_INFINITE, 1, &o, &n) == S_OK && n) { VARIANT v[8]; const wchar_t* names[8] = {L"DeviceName", L"Manufacturer", L"DriverVersion", L"DriverDate", L"DriverProviderName", L"InfName", L"IsSigned", L"DeviceClass"}; for (int k = 0; k < 8; ++k) { VariantInit(&v[k]); o->Get(names[k], 0, &v[k], nullptr, nullptr); }
            wstring name = VStr(v[0]); if (!name.empty()) { wstring d = VStr(v[3]); if (d.size() >= 8) d = d.substr(0, 4) + L"-" + d.substr(4, 2) + L"-" + d.substr(6, 2);
                arr.add(js::Obj().num("i", i++).str("name", U(name)).str("maker", U(VStr(v[1]))).str("version", U(VStr(v[2]))).str("date", U(d)).str("provider", U(VStr(v[4]))).str("inf", U(VStr(v[5]))).boolean("signed", v[6].vt == VT_BOOL ? v[6].boolVal != 0 : true).str("class", U(VStr(v[7]))).done()); }
            for (int k = 0; k < 8; ++k) VariantClear(&v[k]); o->Release(); } en->Release(); }
    svc->Release(); loc->Release(); return arr.done();
}
static const GUID kClsidUpdateSession = {0x4cb43d7f, 0x7eee, 0x4906, {0x86, 0x98, 0x60, 0xda, 0x1c, 0x38, 0xf2, 0xfe}}, kIidUpdateSession = {0x816858a4, 0x260d, 0x4260, {0x93, 0x3a, 0x25, 0x85, 0xf1, 0xab, 0xc7, 0x6b}}, kIidUpdateCollection = {0x07f7438c, 0x7709, 0x4ca5, {0xb5, 0x18, 0x91, 0x27, 0x92, 0x88, 0x13, 0x4e}};
static const GUID kClsidUpdateColl = {0x13639463, 0x00DB, 0x4646, {0x80, 0x3D, 0x52, 0x80, 0x26, 0x14, 0x0D, 0x88}};   // missing from MinGW wuapi.h
static BSTR B(const wchar_t* s) { return SysAllocString(s); }
static IUpdateSearcher* NewSearcher(IUpdateSession** ses) { if (FAILED(CoCreateInstance(kClsidUpdateSession, nullptr, CLSCTX_INPROC_SERVER, kIidUpdateSession, (void**)ses)) || !*ses) return nullptr; IUpdateSearcher* s = nullptr; (*ses)->CreateUpdateSearcher(&s); if (s) s->put_Online(VARIANT_TRUE); return s; }
static wstring TakeBstr(BSTR b) { wstring w = b ? wstring(b) : wstring(); if (b) SysFreeString(b); return w; }
static bool SearchDriverUpdates(IUpdateSession** ses, IUpdateSearcher** srch, IUpdateCollection** col, wstring& err) {
    *srch = NewSearcher(ses); if (!*srch) { err = L"The Windows Update Agent is not available."; return false; }
    BSTR crit = B(L"IsInstalled=0 and Type='Driver' and IsHidden=0"); ISearchResult* r = nullptr; HRESULT hr = (*srch)->Search(crit, &r); SysFreeString(crit);
    if (FAILED(hr) || !r) { char b[40]; snprintf(b, sizeof b, " (0x%08lX)", (unsigned long)hr); err = L"Windows Update could not be searched. Check your internet connection and that the Windows Update service is not disabled" + FromUtf8(b) + L"."; return false; }
    hr = r->get_Updates(col); r->Release(); if (FAILED(hr) || !*col) { err = L"No update list was returned."; return false; } return true;
}
static wstring UpdId(IUpdate* u) { IUpdateIdentity* id = nullptr; wstring r; if (SUCCEEDED(u->get_Identity(&id)) && id) { BSTR b = nullptr; id->get_UpdateID(&b); r = TakeBstr(b); id->Release(); } return r; }
static wstring DateText(DATE d) { SYSTEMTIME st; if (d > 0 && VariantTimeToSystemTime(d, &st)) { wchar_t b[16]; swprintf(b, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); return b; } return L""; }

// ---------- processes ----------
struct ProcRec { DWORD pid; wstring name, path, publisher; ULONGLONG mem, start; DWORD threads; };
static std::vector<ProcRec> g_procs;
static ULONGLONG ProcStart(HANDLE h) { FILETIME c, e, k, u; return GetProcessTimes(h, &c, &e, &k, &u) ? (((ULONGLONG)c.dwHighDateTime << 32) | c.dwLowDateTime) : 0; }
static bool ProtectedProcess(const wstring& n) { static const wchar_t* p[] = {L"system", L"registry", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe", L"services.exe", L"lsass.exe", L"lsm.exe", L"fontdrvhost.exe", L"memory compression", L"secure system"}; for (auto* x : p) if (_wcsicmp(n.c_str(), x) == 0) return true; return false; }
static std::vector<wstring> g_scanSeen;


// ---------- classification, ignore lists, journal housekeeping, restore points ----------
static bool LeftoverSafe(const CleanItem& c) {   // never offer drivers, system folders or system registry as leftovers
    wstring l = LowerW(c.location + L" " + c.subKey);
    if (l.find(L"\\drivers\\") != wstring::npos || l.find(L"\\system32\\") != wstring::npos || l.find(L"\\syswow64\\") != wstring::npos || l.find(L"\\winsxs\\") != wstring::npos || l.find(L"hklm\\system") != wstring::npos || LowerW(c.subKey).rfind(L"system\\", 0) == 0) return false;
    if (c.kind == 8) { if (IsCriticalService(c.cmd)) return false; SC_HANDLE m = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); bool ok = true; if (m) { SC_HANDLE sv = OpenServiceW(m, c.cmd.c_str(), SERVICE_QUERY_CONFIG); if (sv) { DWORD n = 0; QueryServiceConfigW(sv, nullptr, 0, &n); std::vector<BYTE> b(n ? n : 1); if (QueryServiceConfigW(sv, (LPQUERY_SERVICE_CONFIGW)b.data(), n, &n)) { DWORD t = ((LPQUERY_SERVICE_CONFIGW)b.data())->dwServiceType; if (t & (SERVICE_KERNEL_DRIVER | SERVICE_FILE_SYSTEM_DRIVER)) ok = false; } CloseServiceHandle(sv); } CloseServiceHandle(m); } return ok; }
    return true;
}
static string IgnId(const string& kind, const wstring& id) { return kind + "|" + U(id); }
static bool IsIgnored(const string& kind, const wstring& id) { string k = IgnId(kind, id) + "|"; for (auto& x : Cfg().ignored) { string u = U(x); if (u.compare(0, k.size(), k) == 0) return true; } return false; }
static void PruneJournal(int days) {
    if (days <= 0) return; FILETIME ft; GetSystemTimeAsFileTime(&ft); ULONGLONG t = (((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) - (ULONGLONG)days * 864000000000ULL; ft.dwLowDateTime = (DWORD)t; ft.dwHighDateTime = (DWORD)(t >> 32); SYSTEMTIME st; FileTimeToSystemTime(&ft, &st);
    char cut[32]; snprintf(cut, sizeof cut, "%04d-%02d-%02dT%02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::ifstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"), std::ios::binary); if (!f) return; string line, out; bool changed = false; while (std::getline(f, line)) { if (line.empty()) continue; js::V v; string tm = js::Parse(line, v) ? v.str("time") : ""; if (tm.size() >= 19 && tm.compare(0, 19, cut) < 0) { changed = true; continue; } out += line + "\n"; }
    f.close(); if (changed) AtomicWrite(DataDir() + L"\\journal.jsonl", out);
}
static wstring CimDate(const wstring& d) { return d.size() >= 14 ? d.substr(0, 4) + L"-" + d.substr(4, 2) + L"-" + d.substr(6, 2) + L" " + d.substr(8, 2) + L":" + d.substr(10, 2) : d; }
static bool ListRestorePoints(std::vector<std::tuple<long long, wstring, wstring, int>>& out, wstring& err) {
    IWbemLocator* loc = nullptr; IWbemServices* svc = nullptr; IEnumWbemClassObject* en = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&loc)) || !loc) { err = L"WMI is not available."; return false; }
    BSTR ns = SysAllocString(L"ROOT\\DEFAULT"); HRESULT hr = loc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &svc); SysFreeString(ns); if (FAILED(hr) || !svc) { loc->Release(); err = L"Could not read restore points (administrator rights may be required)."; return false; }
    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    BSTR ql = SysAllocString(L"WQL"), q = SysAllocString(L"SELECT SequenceNumber,Description,CreationTime,RestorePointType FROM SystemRestore"); hr = svc->ExecQuery(ql, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en); SysFreeString(ql); SysFreeString(q);
    bool ok = SUCCEEDED(hr) && en; if (ok) { IWbemClassObject* o = nullptr; ULONG n = 0; while (en->Next(WBEM_INFINITE, 1, &o, &n) == S_OK && n) { VARIANT a, b, c, d; VariantInit(&a); VariantInit(&b); VariantInit(&c); VariantInit(&d); o->Get(L"SequenceNumber", 0, &a, nullptr, nullptr); o->Get(L"Description", 0, &b, nullptr, nullptr); o->Get(L"CreationTime", 0, &c, nullptr, nullptr); o->Get(L"RestorePointType", 0, &d, nullptr, nullptr);
            out.emplace_back(a.vt == VT_I4 || a.vt == VT_UI4 ? (long long)a.lVal : 0, VStr(b), CimDate(VStr(c)), d.vt == VT_I4 || d.vt == VT_UI4 ? (int)d.lVal : 0); VariantClear(&a); VariantClear(&b); VariantClear(&c); VariantClear(&d); o->Release(); } en->Release(); } else err = L"Could not read restore points.";
    svc->Release(); loc->Release(); return ok;
}
#pragma pack(push, 1)
struct RPInfo { DWORD ev; DWORD type; INT64 seq; WCHAR desc[256]; };
struct RPStatus { DWORD status; INT64 seq; };
#pragma pack(pop)
typedef BOOL(WINAPI* PFN_SRSet)(RPInfo*, RPStatus*);

static wstring Hex32Str(unsigned v) { wchar_t b[16]; swprintf(b, 16, L"0x%08X", v); return b; }
static int ModeOf(const string& m) { return m == "dupes" ? 1 : m == "empty" ? 2 : m == "downloads" ? 3 : 0; }
static string B64(const std::vector<unsigned char>& d) { static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; string o; size_t i = 0; for (; i + 2 < d.size(); i += 3) { unsigned v = d[i] << 16 | d[i + 1] << 8 | d[i + 2]; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += t[v & 63]; } if (i + 1 == d.size()) { unsigned v = d[i] << 16; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += "=="; } else if (i + 2 == d.size()) { unsigned v = d[i] << 16 | d[i + 1] << 8; o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += "="; } return o; }
static wstring DispStr(IDispatch* d, const wchar_t* name) { if (!d) return L""; DISPID id; LPOLESTR n = const_cast<LPOLESTR>(name); if (FAILED(d->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id))) return L""; DISPPARAMS dp{}; VARIANT r; VariantInit(&r); wstring out; if (SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, &r, nullptr, nullptr))) { if (r.vt == VT_BSTR && r.bstrVal) out = r.bstrVal; else if (r.vt == VT_DATE) { SYSTEMTIME st; if (VariantTimeToSystemTime(r.date, &st)) { wchar_t b[16]; swprintf(b, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); out = b; } } } VariantClear(&r); return out; }
static const GUID kIidWinDriverUpdate = {0xB383CD1A, 0x5CE9, 0x4504, {0x9F, 0x63, 0x76, 0x4B, 0x12, 0x36, 0xF1, 0x91}};
static bool HwidMatch(const wstring& installedList, const wstring& want) { if (want.empty()) return false; wstring w = LowerW(want); size_t p = 0; wstring l = LowerW(installedList); while (p <= l.size()) { size_t e = l.find(L';', p); wstring part = l.substr(p, e == wstring::npos ? wstring::npos : e - p); if (part == w) return true; if (e == wstring::npos) break; p = e + 1; } return false; }
static bool HigherRiskClass(const wstring& c) { wstring l = LowerW(c); return l == L"display" || l == L"net" || l == L"system" || l == L"scsiadapter" || l == L"hdc" || l == L"diskdrive" || l == L"firmware" || l == L"bluetooth"; }
static string DriverJson(const DriverRec& d) { return js::Obj().num("i", d.i).str("name", U(d.name)).str("maker", U(d.maker)).str("version", U(d.version)).str("date", U(d.date)).str("provider", U(d.provider)).str("inf", U(d.inf)).boolean("signed", d.signed_).str("class", U(d.cls)).str("devId", U(d.devId)).str("hwid", U(d.hwid)).num("problem", d.problem).str("problemText", U(d.problem ? ProblemText(d.problem) : L"")).str("status", U(d.status)).boolean("missing", d.missing).boolean("ignored", IsIgnored("driver", d.devId)).boolean("higherRisk", HigherRiskClass(d.cls)).done(); }

// ---------- request dispatch ----------
using Fn = std::function<string(const js::V&)>;
struct AsyncJob { long id; Fn fn; js::V a; };
static DWORD WINAPI AsyncThread(LPVOID p) {
    std::unique_ptr<AsyncJob> j((AsyncJob*)p); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); string out;
    try { out = j->fn(j->a); } catch (const std::exception& e) { out = js::Obj().boolean("ok", false).str("error", e.what()).done(); } catch (...) { out = js::Obj().boolean("ok", false).str("error", "Unexpected error").done(); }
    PostToPage(js::Obj().num("id", (double)j->id).raw("data", out).done()); CoUninitialize(); return 0;
}
static void RunAsync(long id, Fn fn, const js::V a) {
    auto* j = new AsyncJob{id, std::move(fn), a}; HANDLE h = CreateThread(nullptr, 0, AsyncThread, j, 0, nullptr);
    if (h) CloseHandle(h); else { PostToPage(js::Obj().num("id", (double)id).raw("data", Err(L"Could not start a worker thread.")).done()); delete j; }
}
static string Ok(const char* k = nullptr, const string& raw = "") { js::Obj o; o.boolean("ok", true); if (k) o.raw(k, raw); return o.done(); }

static int SafeModeKind() { int k = 0; InSafeMode(&k); return k; }
static bool StartWithWindows() { HKEY k; bool on = false; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &k) == ERROR_SUCCESS) { on = RegQueryValueExW(k, L"FeatherSystemUtilities", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; RegCloseKey(k); } return on; }
static bool SetStartWithWindows(bool on) { HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false; bool ok; if (on) { wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH); wstring v = L"\"" + wstring(exe) + L"\""; ok = RegSetValueExW(k, L"FeatherSystemUtilities", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * 2)) == ERROR_SUCCESS; } else { LONG r = RegDeleteValueW(k, L"FeatherSystemUtilities"); ok = r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND; } RegCloseKey(k); return ok; }
static string ExeDir() { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); PathRemoveFileSpecW(p); return U(p); }
static bool Newer(const wstring& v) { return CompareVersions(v, FSU_VERSION) > 0; }

static std::map<string, Fn>& Handlers() {
    static std::map<string, Fn> h; if (!h.empty()) return h;

    h["app.info"] = [](const js::V&) {
        auto& c = Cfg(); return js::Obj().boolean("ok", true).str("version", U(FSU_VERSION)).boolean("admin", IsElevated()).num("build", WindowsBuild()).str("dataDir", U(DataDir())).str("built", __DATE__).boolean("autoUpdate", c.autoUpdate).boolean("deviceCheck", c.deviceCheck).str("channel", U(c.updateChannel)).str("lastSeenVersion", U(c.lastSeenVersion)).str("uiTheme", U(c.uiTheme)).boolean("helper", BrokerConnected()).boolean("quarantine", c.quarantineLeftovers).boolean("liveMetrics", c.liveMetrics).num("safeMode", SafeModeKind()).boolean("onlineAdvice", c.onlineAdvice).str("adviceVersion", U(AdviceStatus().version)).str("adviceSource", U(AdviceStatus().source)).num("logDays", c.logRetentionDays).boolean("startWithWindows", StartWithWindows()).num("ignoredCount", (double)c.ignored.size()).num("exclusionCount", (double)c.exclusions.size()).boolean("keepRecent", c.keepRecent)
            .num("largeMinMB", c.largeMinMB).num("dupMinKB", c.dupMinKB).boolean("confirmBatch", c.confirmBatch).done(); };
    h["app.set"] = [](const js::V& a) {
        auto& c = Cfg(); string k = a.str("key"); double v = a.num("value");
        if (k == "autoUpdate") c.autoUpdate = v != 0; else if (k == "keepRecent") c.keepRecent = v != 0; else if (k == "confirmBatch") c.confirmBatch = v != 0; else if (k == "quarantine") c.quarantineLeftovers = v != 0; else if (k == "onlineAdvice") c.onlineAdvice = v != 0; else if (k == "deviceCheck") c.deviceCheck = v != 0; else if (k == "liveMetrics") c.liveMetrics = v != 0; else if (k == "logDays") c.logRetentionDays = (int)std::clamp(v, 0.0, 3650.0);
        else if (k == "largeMinMB") c.largeMinMB = (int)std::clamp(v, 1.0, 1000000.0); else if (k == "dupMinKB") c.dupMinKB = (int)std::clamp(v, 1.0, 100000000.0); else return Err(L"Unknown setting.");
        SaveSettings(); return Ok(); };
    h["app.restartAdmin"] = [](const js::V&) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH); if (g_instMutex && *g_instMutex) { CloseHandle(*g_instMutex); *g_instMutex = nullptr; }
        if ((INT_PTR)ShellExecuteW(nullptr, L"runas", exe, nullptr, nullptr, SW_SHOWNORMAL) > 32) { PostMessageW(g_wnd, WM_CLOSE, 0, 0); return Ok(); }
        *g_instMutex = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance"); return Err(L"Administrator permission was not granted."); };
    h["app.theme"] = [](const js::V& a) { string v = a.str("value"); Cfg().uiTheme = (v == "light" || v == "dark") ? W(v) : L"auto"; SaveSettings(); return Ok(); };
    h["app.openData"] = [](const js::V& a) { string w = a.str("what"); wstring p = w == "backups" ? SubDir(L"backups") : w == "regbackups" ? SubDir(L"regbackups") : DataDir(); ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };

    // ---- cleaner ----
    h["clean.scan"] = [](const js::V& a) {
        bool deep = a.boolean("deep"); CleanScan s = ScanCleaner(Cfg().keepRecent);
        if (deep) { g_cancel = false; CleanScan d = ScanDeep(&g_cancel, [](const wstring& m) { Progress(m); }); std::set<wstring> have; for (auto& i : s.items) have.insert(i.id); for (auto& i : d.items) if (!have.count(i.id)) { s.items.push_back(i); s.totalBytes += i.bytes; } s.coverage += L" " + d.coverage; }
        int hidden = 0; { std::vector<CleanItem> keep; for (auto& i : s.items) { bool ex = IsExcluded(i.location); for (auto& p : i.paths) if (IsExcluded(p)) ex = true; if (ex || IsIgnored("clean", i.id.empty() ? i.name : i.id)) { ++hidden; s.totalBytes -= std::min(s.totalBytes, i.bytes); } else keep.push_back(i); } s.items = std::move(keep); }
        std::lock_guard<std::mutex> l(g_m); g_cl = std::move(s); ++g_clGen; string j = CleanJson(); j.pop_back(); j += ",\"hidden\":" + std::to_string(hidden) + "}"; return Ok("scan", j); };
    h["clean.preview"] = [](const js::V& a) {
        std::vector<CleanItem> sel; { std::lock_guard<std::mutex> l(g_m); if ((int)a.num("gen", -1) != g_clGen) return Err(L"The list changed. Scan again."); if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < g_cl.items.size()) sel.push_back(g_cl.items[i]); } }
        js::Arr out; for (auto& it : sel) { std::vector<PreviewRow> rows; ULONGLONG total = 0; PreviewClean(it, Cfg().keepRecent, 300, rows, total); js::Arr fr; for (auto& r : rows) fr.add(js::Obj().str("path", U(r.path)).num("bytes", (double)r.bytes).str("date", U(FileDateText(r.mtime))).done());
            bool recoverable = Cfg().quarantineLeftovers && (it.kind == 6 || it.kind == 7); out.add(js::Obj().str("name", U(it.name)).num("kind", it.kind).num("bytes", (double)it.bytes).num("files", (double)it.files).num("shown", (double)rows.size()).num("total", (double)total).boolean("recoverable", recoverable).str("how", U(it.kind == 1 ? L"Empties the Recycle Bin (not recoverable)." : it.kind == 2 || it.kind == 3 ? L"Registry entry: exported to a backup first, restorable from Recovery." : it.kind == 4 || it.kind == 5 ? L"Runs a Windows maintenance command." : it.kind == 8 ? L"Deletes a Windows service (administrator rights needed)." : it.kind == 9 ? L"Deletes a scheduled task (XML saved first)." : recoverable ? L"Moved to quarantine: restorable from Recovery." : L"Deleted permanently. These are caches/temp files Windows rebuilds.")).raw("rows", fr.done()).done()); }
        return js::Obj().boolean("ok", true).raw("items", out.done()).done(); };
    h["clean.run"] = [](const js::V& a) {
        std::vector<CleanItem> sel; { std::lock_guard<std::mutex> l(g_m); if ((int)a.num("gen", -1) != g_clGen) return Err(L"The list changed. Scan again.");
            if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < g_cl.items.size()) sel.push_back(g_cl.items[i]); } }
        if (sel.empty()) return Err(L"Nothing selected."); CleanResult r; RunClean(sel, Cfg().keepRecent, r, Cfg().quarantineLeftovers);
        return js::Obj().boolean("ok", true).num("bytes", (double)r.bytes).num("files", (double)r.files).num("done", r.ok).num("skipped", r.skipped).num("failed", r.failed).str("firstError", U(r.firstErr)).str("backup", U(r.backupPath)).done(); };
    h["clean.cancel"] = [](const js::V&) { g_cancel = true; return Ok(); };

    // ---- startup ----
    h["startup.list"] = [](const js::V&) { StartupScan s = ScanStartup(); int hidden = 0; std::vector<StartupEntry> keep; for (auto& e : s.entries) { if (IsIgnored("startup", e.key)) ++hidden; else keep.push_back(e); } s.entries = std::move(keep); js::Arr a; for (auto& e : s.entries) a.add(StartupJson(e)); string cov = U(s.coverage); { std::lock_guard<std::mutex> l(g_m); g_su = std::move(s); } return js::Obj().boolean("ok", true).raw("items", a.done()).str("coverage", cov).num("hidden", hidden).done(); };
    auto findSu = [](const string& key, StartupEntry& out) { std::lock_guard<std::mutex> l(g_m); for (auto& e : g_su.entries) if (U(e.key) == key) { out = e; return true; } return false; };
    h["startup.set"] = [findSu](const js::V& a) {
        std::vector<string> keys = a.strs("keys"); bool en = a.boolean("enable"); int ok = 0, bad = 0; wstring first; for (auto& k : keys) { StartupEntry e; if (!findSu(k, e) || e.toggle == Toggle::None) { ++bad; continue; } wstring err; if (SetStartupEnabled(e, en, err)) ++ok; else { ++bad; if (first.empty()) first = e.name + L": " + err; } }
        return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["startup.delete"] = [findSu](const js::V& a) {
        int ok = 0, bad = 0; wstring first; for (auto& k : a.strs("keys")) { StartupEntry e; if (!findSu(k, e)) { ++bad; continue; } wstring err; if (DeleteStartupCore(e, err)) ++ok; else { ++bad; if (first.empty()) first = e.name + L": " + err; } }
        return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["startup.delay"] = [findSu](const js::V& a) {
        int sec = (int)std::clamp(a.num("seconds", 60), 5.0, 3600.0); int ok = 0, bad = 0; wstring first; for (auto& k : a.strs("keys")) { StartupEntry e; if (!findSu(k, e) || !CanDelay(e)) { ++bad; continue; } wstring err; if (DelayStartup(e, sec, err)) ++ok; else { ++bad; if (first.empty()) first = e.name + L": " + err; } }
        return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["startup.add"] = [](const js::V& a) { wstring err; if (AddStartupEntry(W(a.str("name")), W(a.str("command")), a.boolean("allUsers"), err)) return Ok(); return Err(err); };
    h["startup.openLocation"] = [findSu](const js::V& a) { StartupEntry e; if (!findSu(a.str("key"), e)) return Err(L"Entry not found."); wstring er; return OpenFileLocationEx(e.program, er) ? Ok() : Err(er); };
    h["startup.regedit"] = [findSu](const js::V& a) { StartupEntry e; if (!findSu(a.str("key"), e)) return Err(L"Entry not found."); ShowInRegistryEditor(e.location); return Ok(); };
    h["startup.lookup"] = [findSu](const js::V& a) { StartupEntry e; if (!findSu(a.str("key"), e)) return Err(L"Entry not found."); OnlineLookup(g_wnd, a.boolean("virustotal"), e.program, e.name); return Ok(); };

    // ---- programs ----
    h["programs.list"] = [](const js::V&) { ProgramScan s = ScanPrograms(); js::Arr a; for (auto& e : s.entries) a.add(ProgramJson(e)); string cov = U(s.coverage); { std::lock_guard<std::mutex> l(g_m); g_pg = std::move(s); } return js::Obj().boolean("ok", true).raw("items", a.done()).str("coverage", cov).done(); };
    h["programs.uninstall"] = [](const js::V& a) {
        ProgramEntry e; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& p : g_pg.entries) if (U(p.key) == a.str("key")) { e = p; f = true; break; } if (!f) return Err(L"Program not found. Refresh the list."); }
        bool silent = a.boolean("silent"), force = a.boolean("force"); std::vector<CleanItem> left; bool removed = false; wstring note; wstring pwhy; bool prot = ProtectedProgram(e, pwhy);
        if (force && prot) return Err(L"Force remove is disabled for " + e.name + L": it looks like " + pwhy + L". Use its own uninstaller.");
        if (force) { Progress(L"Looking for leftovers of " + e.name + L"…"); left = FindLeftovers(e, true); removed = true; }
        else { UninstallPlan pl = PlanUninstall(e, silent); if (!pl.available) return Err(pl.note.empty() ? L"This uninstall method is not available." : pl.note);
            Progress((silent ? L"Uninstalling silently: " : L"Uninstalling: ") + e.name + L"…"); UninstallResult r; RunUninstall(e, silent, r, nullptr); removed = r.removed; note = r.note;
            if (removed && !prot) { Progress(L"Looking for leftovers…"); left = FindLeftovers(e, false); } else if (removed) note = L"Feather does not search for leftovers of " + pwhy + L"."; }
        { std::vector<CleanItem> safe; for (auto& c : left) if (LeftoverSafe(c) && !IsExcluded(c.location)) safe.push_back(c); left = std::move(safe); }
        std::lock_guard<std::mutex> l(g_m); if (a.boolean("keep")) g_left.insert(g_left.end(), left.begin(), left.end()); else { g_left = left; ++g_leftGen; } js::Arr arr; for (size_t i = 0; i < g_left.size(); ++i) arr.add(CleanItemJson(g_left[i], i));
        return js::Obj().boolean("ok", true).boolean("removed", removed).str("note", U(note)).num("gen", g_leftGen).raw("leftovers", arr.done()).done(); };
    h["programs.openFolder"] = [](const js::V& a) { wstring d; { std::lock_guard<std::mutex> l(g_m); for (auto& p : g_pg.entries) if (U(p.key) == a.str("key")) { d = ExpandEnv(p.installLocation); break; } } if (d.empty() || GetFileAttributesW(d.c_str()) == INVALID_FILE_ATTRIBUTES) return Err(L"The install folder is not recorded or no longer exists."); wstring e; return OpenFileLocationEx(d, e) ? Ok() : Err(e); };
    h["programs.removeLeftovers"] = [](const js::V& a) {
        std::vector<CleanItem> sel; { std::lock_guard<std::mutex> l(g_m); if ((int)a.num("gen", -1) != g_leftGen) return Err(L"The leftover list changed. Run the check again.");
            if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < g_left.size()) sel.push_back(g_left[i]); } }
        if (sel.empty()) return Err(L"Nothing selected."); CleanResult r; RunClean(sel, false, r, Cfg().quarantineLeftovers);
        return js::Obj().boolean("ok", true).boolean("quarantined", Cfg().quarantineLeftovers).num("bytes", (double)r.bytes).num("done", r.ok).num("skipped", r.skipped).num("failed", r.failed).str("firstError", U(r.firstErr)).done(); };

    // ---- services ----
    h["services.list"] = [](const js::V& a) { ServiceScan s = ScanServices(a.boolean("drivers")); js::Arr arr; for (auto& e : s.entries) arr.add(ServiceJson(e)); string cov = U(s.coverage); { std::lock_guard<std::mutex> l(g_m); g_sv = std::move(s); } return js::Obj().boolean("ok", true).raw("items", arr.done()).str("coverage", cov).done(); };
    h["services.op"] = [](const js::V& a) {
        ServiceEntry s; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& e : g_sv.entries) if (U(e.name) == a.str("name")) { s = e; f = true; break; } if (!f) return Err(L"Service not found. Refresh the list."); }
        string op = a.str("op"); wstring err; bool ok;
        if (op == "kill") ok = KillServiceProcess(s, g_sv.entries, err);
        else { SvcOp o = op == "start" ? SvcOp::Start : op == "stop" ? SvcOp::Stop : op == "restart" ? SvcOp::Restart : op == "pause" ? SvcOp::Pause : SvcOp::Resume; ok = ServiceOp(s, o, err); }
        return ok ? Ok() : Err(err); };
    h["services.lookup"] = [](const js::V& a) { ServiceEntry sv; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& e : g_sv.entries) if (U(e.name) == a.str("name")) { sv = e; f = true; break; } if (!f) return Err(L"Service not found."); } OnlineLookup(g_wnd, false, sv.program, sv.display); return Ok(); };
    h["services.profiles"] = [](const js::V&) { js::Arr a; for (auto& p : ListProfiles()) a.add(js::Obj().str("id", U(p.id)).str("label", U(p.label)).str("impact", U(p.impact)).boolean("builtin", p.builtin).done()); return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["services.profilePlan"] = [](const js::V& a) {
        ProfileInfo pi; bool f = false; for (auto& p : ListProfiles()) if (U(p.id) == a.str("id")) { pi = p; f = true; break; } if (!f) return Err(L"Profile not found.");
        std::vector<ServiceEntry> cur; { std::lock_guard<std::mutex> l(g_m); cur = g_sv.entries; } if (cur.empty()) { ServiceScan sc = ScanServices(false); cur = sc.entries; std::lock_guard<std::mutex> l(g_m); g_sv = std::move(sc); }
        std::vector<PlanItem> plan; wstring note, err; if (!BuildPlan(pi, cur, plan, note, err)) return Err(err);
        auto key = [](DWORD t, bool d) { return t == SERVICE_DISABLED ? "disabled" : t == SERVICE_DEMAND_START ? "manual" : (d ? "autodelay" : "auto"); };
        js::Arr arr; for (auto& i : plan) arr.add(js::Obj().str("name", U(i.name)).str("display", U(i.display)).str("from", U(i.from)).str("to", U(i.to)).str("why", U(i.why)).boolean("skip", i.skip).str("oldKey", key(i.oldType, i.oldDelayed)).str("newKey", key(i.type, i.delayed)).done());
        return js::Obj().boolean("ok", true).str("label", U(pi.label)).str("impact", U(pi.impact)).str("note", U(note)).raw("items", arr.done()).done(); };
    h["services.profileApply"] = [](const js::V& a) {   // the plan is rebuilt here from the profile id; the page can only choose which of the planned services to apply
        ProfileInfo pi; bool f = false; for (auto& p : ListProfiles()) if (U(p.id) == a.str("id")) { pi = p; f = true; break; } if (!f) return Err(L"Profile not found.");
        std::vector<ServiceEntry> cur; { std::lock_guard<std::mutex> l(g_m); cur = g_sv.entries; } if (cur.empty()) return Err(L"Refresh the services list first.");
        std::vector<PlanItem> plan; wstring note, err; if (!BuildPlan(pi, cur, plan, note, err)) return Err(err);
        std::set<string> only; for (auto& n : a.strs("names")) only.insert(n); int ok = 0, bad = 0; wstring first;
        for (auto& i : plan) { if (i.skip || (!only.empty() && !only.count(U(i.name)))) continue; wstring e2, before; if (SetServiceStartType(i.name, i.type, i.delayed, e2, before)) ++ok; else { ++bad; if (first.empty()) first = i.display + L": " + e2; } }
        { std::lock_guard<std::mutex> l(g_m); g_sv.entries.clear(); }
        return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["services.profileSave"] = [](const js::V& a) { std::vector<ServiceEntry> cur; { std::lock_guard<std::mutex> l(g_m); cur = g_sv.entries; } if (cur.empty()) return Err(L"Refresh the services list first."); wstring label = W(a.str("label")), path, err; if (label.empty()) return Err(L"Give the profile a name."); return SaveProfile(label, cur, path, err) ? js::Obj().boolean("ok", true).str("path", U(path)).done() : Err(err); };
    h["services.details"] = [](const js::V& a) { ServiceEntry s; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& e : g_sv.entries) if (U(e.name) == a.str("name")) { s = e; f = true; break; } if (!f) return Err(L"Service not found. Refresh the list."); } int t = (int)a.num("tab", 0); return js::Obj().boolean("ok", true).str("text", U(ServiceTabText(s, t < 0 || t > 4 ? 0 : t))).done(); };
    h["services.startType"] = [](const js::V& a) {
        ServiceEntry s; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& e : g_sv.entries) if (U(e.name) == a.str("name")) { s = e; f = true; break; } if (!f) return Err(L"Service not found. Refresh the list."); }
        string t = a.str("type"); if (s.critical && t == "disabled") return Err(L"Blocked: this service is on the critical list (Windows may become unstable).");
        DWORD type = t == "auto" || t == "autodelay" ? SERVICE_AUTO_START : t == "manual" ? SERVICE_DEMAND_START : t == "disabled" ? SERVICE_DISABLED : 0; if (!type) return Err(L"Unknown start type.");
        wstring err, before; return SetServiceStartType(s.name, type, t == "autodelay", err, before) ? Ok() : Err(err); };

    // ---- file finders ----
    h["files.scan"] = [](const js::V& a) {
        string m = a.str("mode"); int mode = ModeOf(m); g_cancel = false; auto& c = Cfg();
        FileScanOpts o; o.roots = c.scanRoots.empty() ? DefaultScanRoots() : c.scanRoots; o.dupes = mode == 1; o.empty = mode == 2; o.minSize = mode == 0 ? (ULONGLONG)c.largeMinMB << 20 : mode == 1 ? (ULONGLONG)c.dupMinKB << 10 : mode == 3 ? 1 : 0; if (mode == 3) { wchar_t* dl = nullptr; o.roots.clear(); if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &dl)) && dl) { o.roots.push_back(dl); CoTaskMemFree(dl); } o.minAgeDays = (int)std::clamp(a.num("days", 60), 7.0, 3650.0); o.exts = {L"exe", L"msi", L"msix", L"appx", L"msixbundle", L"appxbundle", L"iso", L"zip", L"7z", L"rar", L"cab", L"msp"}; } o.maxResults = 2000; o.cancel = &g_cancel; o.progress = [](const wstring& p) { Progress(p); };
        FileScan s; ScanFiles(o, s); js::Arr arr; for (size_t i = 0; i < s.files.size(); ++i) { auto& f = s.files[i]; arr.add(js::Obj().num("i", (double)i).str("name", U(f.name)).str("folder", U(f.folder)).str("ext", U(f.ext)).num("size", (double)f.size).str("date", U(FileDateText(f.mtime))).num("group", f.group).num("count", f.groupCount).str("hash", U(f.hash)).boolean("verified", f.verified).done()); }
        js::Obj r; r.boolean("ok", true).raw("items", arr.done()).str("coverage", U(s.coverage)).num("scanned", (double)s.scanned).num("totalBytes", (double)s.totalBytes).num("reclaimable", (double)s.reclaimable).num("groups", s.groups).boolean("cancelled", s.cancelled);
        std::lock_guard<std::mutex> l(g_m); g_fs[mode] = std::move(s); r.num("gen", ++g_fsGen[mode]); return r.done(); };
    h["files.archive"] = [](const js::V& a) {   // moves chosen files (large-file mode only) to a folder the user picks; nothing is deleted, each move is journaled and reversible by moving the file back
        int mode = ModeOf(a.str("mode")); if (mode != 0) return Err(L"Archiving is only available for large files.");
        std::vector<std::pair<wstring, ULONGLONG>> src; { std::lock_guard<std::mutex> l(g_m); auto& sc = g_fs[mode]; if ((int)a.num("gen", -1) != g_fsGen[mode]) return Err(L"The list changed. Scan again.");
            if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < sc.files.size() && !sc.files[i].path.empty()) src.emplace_back(sc.files[i].path, sc.files[i].size); } }
        if (src.empty()) return Err(L"Nothing selected.");
        IFileOpenDialog* d = nullptr; if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&d)) || !d) return Err(L"The folder picker is not available."); DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM); d->SetTitle(L"Choose the archive folder (an external drive or another disk is best)");
        wstring dest; if (SUCCEEDED(d->Show(g_wnd))) { IShellItem* it = nullptr; if (SUCCEEDED(d->GetResult(&it)) && it) { PWSTR p = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { dest = p; CoTaskMemFree(p); } it->Release(); } } d->Release();
        if (dest.empty()) return js::Obj().boolean("ok", true).boolean("cancelled", true).done();
        while (dest.size() > 3 && dest.back() == L'\\') dest.pop_back(); if (IsProtectedPath(dest)) return Err(L"That folder is protected. Choose a different archive folder.");
        int ok = 0, bad = 0; ULONGLONG moved = 0; wstring first; std::vector<string> gone;
        for (auto& pr : src) { const wstring& p = pr.first; if (IsProtectedPath(p) || IsExcluded(p)) { ++bad; if (first.empty()) first = p + L": protected"; continue; }
            size_t sl = p.find_last_of(L'\\'); wstring nm = sl == wstring::npos ? p : p.substr(sl + 1), to = dest + L"\\" + nm; if (_wcsicmp(to.c_str(), p.c_str()) == 0) { ++bad; if (first.empty()) first = nm + L": already in that folder"; continue; }
            for (int n = 2; GetFileAttributesW(to.c_str()) != INVALID_FILE_ATTRIBUTES && n < 1000; ++n) { size_t dot = nm.find_last_of(L'.'); to = dest + L"\\" + (dot == wstring::npos || dot == 0 ? nm + L" (" + std::to_wstring(n) + L")" : nm.substr(0, dot) + L" (" + std::to_wstring(n) + L")" + nm.substr(dot)); }
            if (MoveFileExW(p.c_str(), to.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) { ++ok; moved += pr.second; WriteJournal(p, L"archive", L"in place", L"moved to " + to, L"ok (move it back to undo)", true); } else { ++bad; if (first.empty()) first = nm + L": " + Win32Err(GetLastError()); } }
        { std::lock_guard<std::mutex> l(g_m); auto& sc = g_fs[mode]; for (auto& f : sc.files) if (!f.path.empty() && GetFileAttributesW(f.path.c_str()) == INVALID_FILE_ATTRIBUTES) { gone.push_back(std::to_string(&f - &sc.files[0])); f.path.clear(); } }
        js::Arr g; for (auto& x : gone) g.add(x); return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).num("bytes", (double)moved).str("dest", U(dest)).str("error", U(first)).raw("gone", g.done()).done(); };
    h["files.cancel"] = [](const js::V&) { g_cancel = true; ++g_stGen; return Ok(); };
    h["files.openLocation"] = [](const js::V& a) { string m = a.str("mode"); int mode = ModeOf(m); wstring p; { std::lock_guard<std::mutex> l(g_m); size_t i = (size_t)a.num("i", -1); if ((int)a.num("gen", -1) != g_fsGen[mode] || i >= g_fs[mode].files.size()) return Err(L"The list changed."); p = g_fs[mode].files[i].path; } if (p.empty()) return Err(L"Already removed."); wstring e; return OpenFileLocationEx(p, e) ? Ok() : Err(e); };
    h["files.remove"] = [](const js::V& a) {
        string m = a.str("mode"); int mode = ModeOf(m); bool permanent = a.boolean("permanent"); std::vector<size_t> idx; std::vector<wstring> paths; ULONGLONG total = 0; wstring warn; std::vector<wstring> okPaths;
        { std::lock_guard<std::mutex> l(g_m); auto& sc = g_fs[mode]; if ((int)a.num("gen", -1) != g_fsGen[mode]) return Err(L"The list changed. Scan again.");
            if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < sc.files.size() && !sc.files[i].path.empty()) idx.push_back(i); }
            if (idx.empty()) return Err(L"Nothing selected."); std::set<size_t> chosen(idx.begin(), idx.end());
            if (mode == 1) { std::map<int, int> per, tot; for (size_t i : idx) ++per[sc.files[i].group]; for (auto& f : sc.files) if (!f.path.empty()) ++tot[f.group];
                for (auto& kv : per) if (kv.second >= tot[kv.first]) return Err(L"Group " + std::to_wstring(kv.first) + L" has all of its copies selected. At least one copy must be kept."); }
            int mism = 0; for (size_t i : idx) { auto& f = sc.files[i];
                if (mode == 1) { const FileEntry* keep = nullptr; for (size_t k = 0; k < sc.files.size(); ++k) if (sc.files[k].group == f.group && !sc.files[k].path.empty() && !chosen.count(k)) { keep = &sc.files[k]; break; }
                    if (keep && VerifyIdentical(f.path, keep->path)) { okPaths.push_back(f.path); total += f.size; } else ++mism; } else { okPaths.push_back(f.path); total += f.size; } }
            if (mode == 2) { int notEmpty = 0; std::vector<wstring> keep; for (auto& pth : okPaths) if (IsFolderTreeEmpty(pth)) keep.push_back(pth); else ++notEmpty; okPaths = keep; total = 0; if (notEmpty) warn = std::to_wstring(notEmpty) + L" folder(s) were NOT removed: they now contain files."; }
            if (mism) warn = std::to_wstring(mism) + L" file(s) were NOT deleted: they are no longer identical to the copy you keep (changed since the scan, or unreadable)."; }
        if (okPaths.empty()) return Err(warn.empty() ? L"Nothing could be removed." : warn);
        wstring err; if (permanent) DeleteFilesPermanently(okPaths, err); else RecycleFiles(g_wnd, okPaths, err);
        std::vector<size_t> gone; ULONGLONG freed = 0; { std::lock_guard<std::mutex> l(g_m); auto& sc = g_fs[mode]; for (size_t i : idx) if (!sc.files[i].path.empty() && GetFileAttributesW(sc.files[i].path.c_str()) == INVALID_FILE_ATTRIBUTES) { freed += sc.files[i].size; gone.push_back(i); sc.files[i].path.clear(); } }
        WriteJournal(L"File finder", mode == 1 ? L"duplicates" : mode == 2 ? L"empty folders" : L"large files", std::to_wstring(okPaths.size()) + L" item(s) selected", std::to_wstring(gone.size()) + (permanent ? L" item(s) permanently deleted (" : L" item(s) moved to Recycle Bin (") + FormatBytes(freed) + L")", gone.size() == okPaths.size() ? (permanent ? L"ok (permanent, not undoable)" : L"ok (restorable from Recycle Bin)") : L"partial: " + err, !permanent);
        js::Arr g; for (size_t i : gone) g.add(std::to_string(i)); return js::Obj().boolean("ok", true).raw("gone", g.done()).num("freed", (double)freed).str("warning", U(warn)).str("error", U(err)).done(); };

    // ---- storage map ----
    h["storage.drives"] = [](const js::V&) {
        js::Arr a; DWORD mask = GetLogicalDrives(); for (int i = 0; i < 26; ++i) if (mask & (1u << i)) { wchar_t r[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(r) != DRIVE_FIXED) continue; ULARGE_INTEGER fr{}, tot{}, nf{}; if (!GetDiskFreeSpaceExW(r, &fr, &tot, &nf)) continue;
            a.add(js::Obj().str("path", U(r)).num("total", (double)tot.QuadPart).num("free", (double)fr.QuadPart).done()); }
        return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["storage.children"] = [](const js::V& a) {
        wstring dir = W(a.str("path")); while (dir.size() > 3 && dir.back() == L'\\') dir.pop_back(); DWORD at = GetFileAttributesW(dir.c_str()); if (at == INVALID_FILE_ATTRIBUTES || !(at & FILE_ATTRIBUTE_DIRECTORY)) return Err(L"Folder not found.");
        int gen = ++g_stGen; struct C { wstring name, path; ULONGLONG bytes, files; bool dir; }; std::vector<C> kids; WIN32_FIND_DATAW fd; wstring base = dir.size() == 2 ? dir + L"\\" : dir; wstring pat = (dir.size() == 2 ? dir + L"\\" : dir + L"\\") + L"*";
        HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h != INVALID_HANDLE_VALUE) { do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; wstring full = (dir.size() == 2 ? dir + L"\\" : dir + L"\\") + n;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) kids.push_back({n, full, 0, 0, true}); else kids.push_back({n, full, ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow, 1, false}); } while (FindNextFileW(h, &fd)); FindClose(h); }
        size_t done = 0; for (auto& k : kids) { if (k.dir) { ULONGLONG f = 0; k.bytes = TreeSize(k.path, f, gen); k.files = f; } if (gen != g_stGen.load()) return Err(L"Cancelled."); Emit("progress", js::Obj().str("text", U(L"Measuring " + k.name + L"… (" + std::to_wstring(++done) + L" of " + std::to_wstring(kids.size()) + L")")).done()); }
        std::sort(kids.begin(), kids.end(), [](const C& x, const C& y) { return x.bytes > y.bytes; }); js::Arr arr; ULONGLONG sum = 0; for (auto& k : kids) { sum += k.bytes; arr.add(js::Obj().str("name", U(k.name)).str("path", U(k.path)).num("bytes", (double)k.bytes).num("files", (double)k.files).boolean("dir", k.dir).done()); }
        return js::Obj().boolean("ok", true).str("path", U(dir)).num("total", (double)sum).raw("items", arr.done()).done(); };

    // ---- history / rollback ----
    h["journal.list"] = [](const js::V& a) {
        size_t lim = (size_t)std::clamp(a.num("limit", 300), 1.0, 5000.0); std::ifstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"), std::ios::binary); string line; std::vector<string> lines; while (std::getline(f, line)) if (!line.empty()) lines.push_back(line);
        js::Arr arr; size_t n = 0; for (auto it = lines.rbegin(); it != lines.rend() && n < lim; ++it) { js::V v; if (js::Parse(*it, v) && v.t == js::V::Obj) { arr.add(js::Obj().str("time", v.str("time")).str("target", v.str("target")).str("source", v.str("source")).str("id", v.str("id")).str("type", v.str("type")).str("rollback", v.str("rollback")).str("before", v.str("before")).str("after", v.str("after")).str("result", v.str("result")).boolean("reversible", v.boolean("reversible")).done()); ++n; } }
        return js::Obj().boolean("ok", true).raw("items", arr.done()).done(); };
    h["backups.list"] = [](const js::V&) {
        js::Arr a; auto add = [&](const std::vector<BackupInfo>& v, const char* kind) { for (auto& b : v) { SYSTEMTIME st; FileTimeToSystemTime(&b.written, &st); char t[32]; snprintf(t, sizeof t, "%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute); a.add(js::Obj().str("path", U(b.path)).str("label", U(b.label)).str("time", t).str("kind", kind).done()); } };
        add(ListBackups(), "startup"); add(ListRegBackups(), "registry"); return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["backups.restore"] = [](const js::V& a) {
        wstring p = W(a.str("path")); string kind = a.str("kind"); bool found = false; for (auto& b : kind == "registry" ? ListRegBackups() : ListBackups()) if (_wcsicmp(b.path.c_str(), p.c_str()) == 0) { found = true; break; }
        if (!found) return Err(L"Backup not found."); wstring err, sum; bool ok = kind == "registry" ? RestoreRegBackup(p, err, sum) : RestoreBackup(p, err, sum); return ok ? js::Obj().boolean("ok", true).str("summary", U(sum)).done() : Err(err); };

    // ---- updates ----
    h["update.check"] = [](const js::V& a) { UpdateInfo u; if (!FetchLatestVersion(u, Cfg().updateChannel == L"beta") || !u.ok) return js::Obj().boolean("ok", false).str("error", U(u.error.empty() ? L"Could not reach the update server." : u.error)).done();
        bool newer = Newer(u.version), launch = a.boolean("launch"); bool suppressed = false; if (launch && newer) { FILETIME ft; GetSystemTimeAsFileTime(&ft); long long now = (long long)((((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000ULL) - 11644473600LL; auto& c = Cfg(); if (c.skipVersion == u.version || now < c.deferUntil) suppressed = true; c.lastUpdateCheck = now; SaveSettings(); }
        return js::Obj().boolean("ok", true).boolean("newer", newer).boolean("suppressed", suppressed).str("version", U(u.version)).str("date", U(u.date)).str("notes", U(u.notes)).num("size", (double)u.size).str("prevVersion", U(u.prevVersion)).done(); };
    h["update.skip"] = [](const js::V& a) { Cfg().skipVersion = W(a.str("version")); SaveSettings(); return Ok(); };
    h["update.defer"] = [](const js::V& a) { FILETIME ft; GetSystemTimeAsFileTime(&ft); long long now = (long long)((((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000ULL) - 11644473600LL; Cfg().deferUntil = now + (long long)std::clamp(a.num("hours", 24), 1.0, 720.0) * 3600; SaveSettings(); return Ok(); };
    h["update.install"] = [](const js::V&) {
        UpdateInfo u; if (!FetchLatestVersion(u, Cfg().updateChannel == L"beta") || !u.ok || !Newer(u.version)) return Err(L"No newer version is available."); wstring path, err;
        if (!DownloadUpdate(u, path, err, nullptr, [](ULONGLONG got, ULONGLONG all) { Emit("download", js::Obj().num("got", (double)got).num("all", (double)all).done()); })) return Err(err);
        if (!LaunchUpdateInstaller(path, err)) return Err(err); PostMessageW(g_wnd, WM_CLOSE, 0, 0); return Ok(); };

    // ---- application settings helpers ----
    h["app.startWithWindows"] = [](const js::V& a) { bool on = a.boolean("on"); return SetStartWithWindows(on) ? Ok() : Err(L"Windows refused the change to your startup list."); };
    h["journal.clear"] = [](const js::V&) { bool ok = AtomicWrite(DataDir() + L"\\journal.jsonl", ""); if (ok) WriteJournal(L"Journal", L"user", L"history present", L"history cleared", L"ok", false); return ok ? Ok() : Err(L"The journal could not be cleared."); };

    // ---- ignored findings and exclusions ----
    h["ignore.list"] = [](const js::V&) { js::Arr arr; size_t i = 0; for (auto& x : Cfg().ignored) { string u = U(x); size_t p1 = u.find('|'), p2 = p1 == string::npos ? p1 : u.find('|', p1 + 1); if (p2 == string::npos) { ++i; continue; } arr.add(js::Obj().num("i", (double)i++).str("kind", u.substr(0, p1)).str("label", u.substr(p2 + 1)).done()); } return js::Obj().boolean("ok", true).raw("items", arr.done()).done(); };
    h["ignore.add"] = [](const js::V& a) { string kind = a.str("kind"); if (kind != "clean" && kind != "startup" && kind != "driver") return Err(L"Unknown finding type."); int n = 0; if (auto* v = a.get("items")) for (auto& x : v->a) { string id = x.str("id"), label = x.str("label"); if (id.empty() || Cfg().ignored.size() >= 500) continue; if (id.find('|') != string::npos) continue; string e = kind + "|" + id + "|" + label; Cfg().ignored.push_back(W(e)); ++n; } SaveSettings(); return js::Obj().boolean("ok", true).num("added", n).done(); };
    h["ignore.remove"] = [](const js::V& a) { size_t i = (size_t)a.num("i", -1); auto& v = Cfg().ignored; if (i < v.size()) v.erase(v.begin() + i); SaveSettings(); return Ok(); };
    h["ignore.clear"] = [](const js::V&) { Cfg().ignored.clear(); SaveSettings(); return Ok(); };
    h["exclusions.list"] = [](const js::V&) { js::Arr a; for (auto& x : Cfg().exclusions) a.add(Q(x)); return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["exclusions.add"] = [](const js::V& a) {
        IFileOpenDialog* d = nullptr; if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&d)) || !d) return Err(L"The folder picker is not available."); DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        wstring path; if (SUCCEEDED(d->Show(g_wnd))) { IShellItem* it = nullptr; if (SUCCEEDED(d->GetResult(&it)) && it) { PWSTR p = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { path = p; CoTaskMemFree(p); } it->Release(); } } d->Release();
        if (path.empty()) return js::Obj().boolean("ok", true).boolean("cancelled", true).done(); auto& c = Cfg(); for (auto& x : c.exclusions) if (_wcsicmp(x.c_str(), path.c_str()) == 0) return Ok(); if (c.exclusions.size() >= 100) return Err(L"Too many exclusions."); c.exclusions.push_back(path); SaveSettings(); return Ok(); };
    h["exclusions.remove"] = [](const js::V& a) { size_t i = (size_t)a.num("i", -1); auto& v = Cfg().exclusions; if (i < v.size()) v.erase(v.begin() + i); SaveSettings(); return Ok(); };

    // ---- quarantine and restore points ----
    h["quarantine.list"] = [](const js::V&) { js::Arr arr; ULONGLONG tot = 0; for (auto& q : QuarantineList()) { tot += q.bytes; arr.add(js::Obj().str("id", U(q.id)).str("orig", U(q.orig)).str("source", U(q.source)).str("time", U(q.time)).num("bytes", (double)q.bytes).boolean("dir", q.isDir).done()); } return js::Obj().boolean("ok", true).raw("items", arr.done()).num("total", (double)tot).done(); };
    h["quarantine.restore"] = [](const js::V& a) { int ok = 0, bad = 0; wstring first; for (auto& id : a.strs("ids")) { wstring e; if (QuarantineRestore(W(id), e)) ++ok; else { ++bad; if (first.empty()) first = e; } } return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["quarantine.purge"] = [](const js::V& a) { int ok = 0, bad = 0; wstring first; for (auto& id : a.strs("ids")) { wstring e; if (QuarantinePurge(W(id), e)) ++ok; else { ++bad; if (first.empty()) first = e; } } return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["recovery.points"] = [](const js::V&) {
        std::vector<std::tuple<long long, wstring, wstring, int>> v; wstring err; bool ok = ListRestorePoints(v, err); bool policyOff = false; HKEY k; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\SystemRestore", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { DWORD d = 0, n = sizeof d; if (RegQueryValueExW(k, L"DisableSR", nullptr, nullptr, (BYTE*)&d, &n) == ERROR_SUCCESS && d == 1) policyOff = true; RegCloseKey(k); }
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return std::get<0>(a) > std::get<0>(b); }); js::Arr arr; for (auto& t : v) arr.add(js::Obj().num("seq", (double)std::get<0>(t)).str("desc", U(std::get<1>(t))).str("date", U(std::get<2>(t))).num("type", std::get<3>(t)).done());
        return js::Obj().boolean("ok", ok).str("error", U(err)).boolean("policyOff", policyOff).boolean("admin", IsElevated()).raw("items", arr.done()).done(); };
    h["recovery.create"] = [](const js::V& a) {
        if (!IsElevated()) return Err(L"Creating a restore point needs administrator rights. Restart as administrator from Settings.");
        HMODULE m = LoadLibraryW(L"srclient.dll"); auto fn = m ? (PFN_SRSet)GetProcAddress(m, "SRSetRestorePointW") : nullptr; if (!fn) return Err(L"System Restore is not available on this PC.");
        std::vector<std::tuple<long long, wstring, wstring, int>> before, after; wstring e; ListRestorePoints(before, e); long long maxBefore = 0; for (auto& t : before) maxBefore = std::max(maxBefore, std::get<0>(t));
        wstring desc = W(a.str("desc", "Feather System Utilities")); RPInfo ri{}; ri.ev = 100; ri.type = 12; wcsncpy(ri.desc, desc.c_str(), 255); RPStatus st{}; BOOL ok = fn(&ri, &st); DWORD s1 = st.status;
        RPInfo re{}; re.ev = 101; re.type = 12; re.seq = st.seq; RPStatus st2{}; if (ok) fn(&re, &st2); FreeLibrary(m); e.clear(); ListRestorePoints(after, e); bool found = false; for (auto& t : after) if (std::get<0>(t) > maxBefore) found = true;
        WriteJournal(L"Restore point", L"System Restore", std::to_wstring(before.size()) + L" restore points", found ? L"created: " + desc : L"not created", found ? L"ok (verified in the restore point list)" : L"failed or skipped by Windows", false);
        if (found) return Ok(); if (!ok || s1 != 0) return Err(L"Windows refused to create the restore point (code " + std::to_wstring(s1) + L"). System Protection may be turned off for the system drive."); return Err(L"Windows accepted the request but no new restore point appeared. By default Windows creates at most one every 24 hours, or System Protection is off.");
    };

    // ---- file preview ----
    h["files.preview"] = [](const js::V& a) {
        int mode = ModeOf(a.str("mode")); wstring p; ULONGLONG sz = 0; wstring date; { std::lock_guard<std::mutex> l(g_m); size_t i = (size_t)a.num("i", -1); if ((int)a.num("gen", -1) != g_fsGen[mode] || i >= g_fs[mode].files.size()) return Err(L"The list changed."); auto& f = g_fs[mode].files[i]; p = f.path; sz = f.size; date = FileDateText(f.mtime); } if (p.empty()) return Err(L"Already removed.");
        size_t dp = p.find_last_of(L'.'); wstring ext = dp == wstring::npos ? L"" : LowerW(p.substr(dp + 1)); js::Obj o; o.boolean("ok", true).str("path", U(p)).num("size", (double)sz).str("date", U(date));
        static const wchar_t* texts[] = {L"txt", L"log", L"md", L"csv", L"json", L"xml", L"ini", L"cfg", L"conf", L"yaml", L"yml", L"bat", L"cmd", L"ps1", L"js", L"py", L"c", L"cpp", L"h", L"html", L"htm", L"css", L"reg", L"sql", L"tsv"}; static const wchar_t* imgs[] = {L"png", L"jpg", L"jpeg", L"gif", L"bmp", L"webp", L"ico"};
        bool isText = false, isImg = false; for (auto* t : texts) if (ext == t) isText = true; for (auto* t : imgs) if (ext == t) isImg = true;
        HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); if (f == INVALID_HANDLE_VALUE) return Err(L"The file cannot be opened: " + Win32Err(GetLastError()));
        if (isText) { std::vector<char> b(4096); DWORD n = 0; ReadFile(f, b.data(), (DWORD)b.size(), &n, nullptr); b.resize(n); bool bin = std::find(b.begin(), b.end(), '\0') != b.end(); if (!bin) { o.str("kind", "text").str("text", U(W(string(b.begin(), b.end())))); CloseHandle(f); return o.done(); } }
        else if (isImg && sz <= 3u * 1024 * 1024) { std::vector<unsigned char> b((size_t)sz); DWORD n = 0; if (ReadFile(f, b.data(), (DWORD)b.size(), &n, nullptr)) { b.resize(n); string mime = ext == L"png" ? "image/png" : ext == L"gif" ? "image/gif" : ext == L"bmp" ? "image/bmp" : ext == L"webp" ? "image/webp" : ext == L"ico" ? "image/x-icon" : "image/jpeg"; CloseHandle(f); return o.str("kind", "image").str("data", "data:" + mime + ";base64," + B64(b)).done(); } }
        CloseHandle(f); FileInfo fi = InspectFile(p, false); return o.str("kind", "info").str("publisher", U(fi.publisher)).str("version", U(fi.version)).done(); };

    // ---- privacy ----
    h["privacy.scan"] = [](const js::V&) {
        auto v = ScanBrowsers(); js::Arr a; for (size_t i = 0; i < v.size(); ++i) { auto& b = v[i]; js::Arr cats; for (size_t k = 0; k < b.cats.size(); ++k) { auto& c = b.cats[k]; cats.add(js::Obj().str("id", U(c.id)).str("label", U(c.label)).str("note", U(c.note)).num("bytes", (double)c.bytes).num("files", (double)c.files).boolean("deletable", c.deletable).done()); }
            a.add(js::Obj().str("key", U(b.key)).str("browser", U(b.browser)).str("profile", U(b.display)).str("folder", U(b.profile)).boolean("running", BrowserRunning(b.key)).raw("cats", cats.done()).done()); }
        std::lock_guard<std::mutex> l(g_m); g_priv = std::move(v); ++g_privGen; return js::Obj().boolean("ok", true).num("gen", g_privGen).raw("items", a.done()).done(); };
    h["privacy.clear"] = [](const js::V& a) {
        std::vector<std::pair<BrowserProfile, BrowserCat>> sel; { std::lock_guard<std::mutex> l(g_m); if ((int)a.num("gen", -1) != g_privGen) return Err(L"The list changed. Scan again."); for (auto& id : a.strs("ids")) { wstring w = W(id); for (auto& b : g_priv) for (auto& c : b.cats) if (c.id == w) sel.emplace_back(b, c); } }
        if (sel.empty()) return Err(L"Nothing selected."); ULONGLONG total = 0; int ok = 0, bad = 0; wstring first; for (auto& pr : sel) { ULONGLONG by = 0; wstring e; if (ClearBrowserCategory(pr.first, pr.second, by, e)) { ++ok; total += by; WriteJournal(pr.first.browser + L" (" + pr.first.display + L")", L"privacy: " + pr.second.label, FormatBytes(pr.second.bytes), L"cleared " + FormatBytes(by), L"ok (not undoable)", false); } else { ++bad; if (first.empty()) first = pr.first.browser + L" / " + pr.second.label + L": " + e; } }
        return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).num("bytes", (double)total).str("error", U(first)).done(); };
    h["privacy.checklist"] = [](const js::V&) { js::Arr a; for (auto& p : PrivacyChecklist()) a.add(js::Obj().str("id", U(p.id)).str("title", U(p.title)).str("state", U(p.state)).str("detail", U(p.detail)).str("why", U(p.why)).boolean("good", p.good).done()); return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["disks.health"] = [](const js::V&) { DiskReport r = ReadDiskHealth(); js::Arr d, v; for (auto& x : r.disks) d.add(js::Obj().str("name", U(x.name)).str("media", U(x.media)).str("bus", U(x.bus)).str("health", U(x.health)).num("healthCode", x.healthCode).num("temp", x.temp).num("wear", x.wear).num("hours", (double)x.hours).num("readErr", (double)x.readErr).num("writeErr", (double)x.writeErr).num("size", (double)x.size).done());
        for (auto& x : r.vols) v.add(js::Obj().str("path", U(x.path)).str("label", U(x.label)).str("fs", U(x.fs)).num("total", (double)x.total).num("free", (double)x.free).done());
        return js::Obj().boolean("ok", true).raw("disks", d.done()).raw("vols", v.done()).num("predictFail", r.predictFail).boolean("trimKnown", r.trimKnown).boolean("trimOn", r.trimOn).boolean("admin", r.admin).str("error", U(r.error)).done(); };
    h["boot.history"] = [](const js::V& a) { wstring err; auto v = BootHistory((size_t)std::clamp(a.num("max", 60), 1.0, 300.0), err); js::Arr arr; for (auto& b : v) arr.add(js::Obj().str("time", U(b.time)).num("bootMs", (double)b.bootMs).num("mainMs", (double)b.mainMs).num("postMs", (double)b.postMs).done()); return js::Obj().boolean("ok", true).str("error", U(err)).raw("items", arr.done()).done(); };
    h["security.checklist"] = [](const js::V&) { js::Arr a; for (auto& p : SecurityChecklist()) a.add(js::Obj().str("id", U(p.id)).str("title", U(p.title)).str("state", U(p.state)).str("detail", U(p.detail)).str("why", U(p.why)).boolean("good", p.good).done()); return js::Obj().boolean("ok", true).boolean("admin", IsElevated()).raw("items", a.done()).done(); };
    h["security.open"] = [](const js::V& a) { return OpenSecuritySetting(W(a.str("id"))) ? Ok() : Err(L"That Windows page could not be opened (it may not exist on this version of Windows)."); };
    h["tasks.list"] = [](const js::V&) { wstring err; auto v = ListAllTasks(err); js::Arr a; for (auto& t : v) a.add(js::Obj().str("path", U(t.path)).str("name", U(t.name)).str("folder", U(t.folder)).str("state", U(t.state)).str("triggers", U(t.triggers)).str("command", U(t.command)).str("author", U(t.author)).str("why", U(t.why)).str("lastRun", U(t.lastRun)).str("nextRun", U(t.nextRun)).num("lastResult", (double)t.lastResult).boolean("enabled", t.enabled).boolean("microsoft", t.microsoft).boolean("startup", t.startup).boolean("hidden", t.hidden).boolean("canToggle", t.canToggle).done()); return js::Obj().boolean("ok", err.empty() || !v.empty()).str("error", U(err)).raw("items", a.done()).done(); };
    h["tasks.set"] = [](const js::V& a) { bool en = a.boolean("enable"); int ok = 0, bad = 0; wstring first; for (auto& p : a.strs("paths")) { wstring err; if (SetTaskEnabledByPath(W(p), en, err)) ++ok; else { ++bad; if (first.empty()) first = W(p) + L": " + err; } } return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    h["tasks.openScheduler"] = [](const js::V&) { ShellExecuteW(nullptr, L"open", L"taskschd.msc", nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };
    h["exclusions.addPattern"] = [](const js::V& a) { wstring p = W(a.str("pattern")); while (!p.empty() && iswspace(p.front())) p.erase(0, 1); while (!p.empty() && iswspace(p.back())) p.pop_back();
        if (p.empty() || p.find_first_of(L"*?") == wstring::npos) return Err(L"A pattern needs at least one * or ? wildcard, for example *.bak or D:\\Projects\\*\\node_modules.");
        if (p.find_first_not_of(L"*?\\/. ") == wstring::npos) return Err(L"That pattern would exclude everything.");
        auto& c = Cfg(); for (auto& x : c.exclusions) if (_wcsicmp(x.c_str(), p.c_str()) == 0) return Ok(); if (c.exclusions.size() >= 100) return Err(L"Too many exclusions."); c.exclusions.push_back(p); SaveSettings(); return Ok(); };
    h["privacy.openSetting"] = [](const js::V& a) { return OpenPrivacySetting(W(a.str("id"))) ? Ok() : Err(L"That Windows settings page could not be opened (it may not exist on this version of Windows)."); };
    h["privacy.openBrowser"] = [](const js::V& a) { wstring e; return OpenBrowserPrivacy(W(a.str("key")), e) ? Ok() : Err(e); };

    // ---- troubleshooting ----
    h["trouble.status"] = [](const js::V&) {
        ServiceScan sv = ScanServices(false); js::Arr svc; for (auto& e : sv.entries) if (e.start.rfind(L"Automatic", 0) == 0 && e.stateCode == SERVICE_STOPPED && !e.trigger && !e.delayed && !e.isDriver) svc.add(js::Obj().str("name", U(e.name)).str("display", U(e.display)).str("start", U(e.start)).done());
        wstring err; auto drv = ListDrivers(err); js::Arr dev; int bad = 0; for (auto& d : drv) if (d.problem) { dev.add(DriverJson(d)); ++bad; }
        js::Arr disks; DWORD mask = GetLogicalDrives(); for (int i = 0; i < 26; ++i) if (mask & (1u << i)) { wchar_t r[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(r) != DRIVE_FIXED) continue; ULARGE_INTEGER fr{}, tot{}, nf{}; if (GetDiskFreeSpaceExW(r, &fr, &tot, &nf) && tot.QuadPart) disks.add(js::Obj().str("path", U(r)).num("free", (double)fr.QuadPart).num("total", (double)tot.QuadPart).done()); }
        bool reboot = false; HKEY k; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { reboot = true; RegCloseKey(k); } else if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { reboot = true; RegCloseKey(k); }
        return js::Obj().boolean("ok", true).raw("services", svc.done()).raw("devices", dev.done()).raw("disks", disks.done()).boolean("pendingReboot", reboot).done(); };
    h["events.query"] = [](const js::V& a) {
        std::vector<wstring> logs; for (auto& x : a.strs("logs")) logs.push_back(W(x)); std::vector<int> lv; if (auto* v = a.get("levels")) for (auto& x : v->a) if (x.t == js::V::Num && x.n >= 1 && x.n <= 3) lv.push_back((int)x.n); bool redact = a.boolean("redact", true), mask = a.boolean("maskPaths");
        wstring err; auto ev = QueryEvents((int)std::clamp(a.num("hours", 24), 1.0, 720.0), logs, lv, 300, err); js::Arr arr; for (auto& e : ev) arr.add(js::Obj().str("time", U(e.time)).str("log", U(e.log)).num("level", e.level).str("provider", U(e.provider)).num("id", e.id).str("message", U(redact ? Redact(e.message, mask) : e.message)).done());
        return js::Obj().boolean("ok", true).raw("items", arr.done()).str("warning", U(err)).done(); };
    h["report.build"] = [](const js::V& a) {
        bool redact = a.boolean("redact", true), mask = a.boolean("maskPaths"); auto R = [&](const wstring& t) { return redact ? Redact(t, mask) : t; }; wstring out = L"Feather System Utilities diagnostic report\r\nVersion " FSU_VERSION L"\r\nCreated " + NowIsoUtc() + L"\r\nThis report stays on this PC. Feather never uploads it.\r\n" + (redact ? L"User name, computer name, user-profile paths and SIDs are replaced with placeholders" + wstring(mask ? L"; all file paths are reduced to file names" : L"") + L".\r\n" : L"NOT redacted: it may contain your user name, computer name and file paths.\r\n");
        auto sec = [&](const wchar_t* t) { out += L"\r\n=== " + wstring(t) + L" ===\r\n"; };
        if (a.boolean("system", true)) { sec(L"System"); MEMORYSTATUSEX ms{sizeof ms}; GlobalMemoryStatusEx(&ms); ComputerInfo ci = GetComputerInfo(); HKEY k; wstring os, cpu; wchar_t b[256]{}; DWORD n = sizeof b - 2; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { if (RegQueryValueExW(k, L"ProductName", nullptr, nullptr, (BYTE*)b, &n) == ERROR_SUCCESS) os = b; RegCloseKey(k); } wchar_t c2[256]{}; n = sizeof c2 - 2; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { if (RegQueryValueExW(k, L"ProcessorNameString", nullptr, nullptr, (BYTE*)c2, &n) == ERROR_SUCCESS) cpu = c2; RegCloseKey(k); }
            out += L"Windows: " + os + L" (build " + std::to_wstring(WindowsBuild()) + L")\r\nPC: " + ci.maker + L" " + ci.model + L"\r\nProcessor: " + cpu + L"\r\nMemory: " + FormatBytes(ms.ullTotalPhys) + L", free " + FormatBytes(ms.ullAvailPhys) + L"\r\nUptime: " + std::to_wstring(GetTickCount64() / 3600000) + L" hours\r\nAdministrator: " + (IsElevated() ? L"yes" : L"no") + L"\r\n";
            DWORD mask2 = GetLogicalDrives(); for (int i = 0; i < 26; ++i) if (mask2 & (1u << i)) { wchar_t r[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(r) != DRIVE_FIXED) continue; ULARGE_INTEGER fr{}, tot{}, nf{}; if (GetDiskFreeSpaceExW(r, &fr, &tot, &nf)) out += wstring(L"Drive ") + r + L": " + FormatBytes(fr.QuadPart) + L" free of " + FormatBytes(tot.QuadPart) + L"\r\n"; } }
        if (a.boolean("problems", true)) { sec(L"Devices reporting problems"); wstring err; auto drv = ListDrivers(err); int n = 0; for (auto& d : drv) if (d.problem) { out += L"- " + d.name + L" [" + d.cls + L"] code " + std::to_wstring(d.problem) + L": " + ProblemText(d.problem) + (d.missing ? L" (no driver)" : L"") + L"\r\n"; ++n; } if (!n) out += L"None.\r\n"; }
        if (a.boolean("services", true)) { sec(L"Automatic services that are not running"); ServiceScan sv = ScanServices(false); int n = 0; for (auto& e : sv.entries) if (e.start.rfind(L"Automatic", 0) == 0 && e.stateCode == SERVICE_STOPPED && !e.trigger && !e.delayed && !e.isDriver) { out += L"- " + e.display + L" (" + e.name + L")\r\n"; ++n; } if (!n) out += L"None.\r\n"; }
        if (a.boolean("drivers", false)) { sec(L"Installed drivers"); wstring err; for (auto& d : ListDrivers(err)) out += L"- " + d.name + L" | " + d.provider + L" | " + d.version + L" | " + d.date + L"\r\n"; }
        if (a.boolean("events", true)) { sec(L"Recent errors and warnings"); std::vector<wstring> logs; for (auto& x : a.strs("logs")) logs.push_back(W(x)); if (logs.empty()) logs = {L"System", L"Application"}; std::vector<int> lv; if (auto* v = a.get("levels")) for (auto& x : v->a) if (x.t == js::V::Num) lv.push_back((int)x.n); if (lv.empty()) lv = {1, 2};
            wstring err; auto ev = QueryEvents((int)std::clamp(a.num("hours", 24), 1.0, 720.0), logs, lv, 150, err); if (!err.empty()) out += err + L"\r\n"; for (auto& e : ev) out += L"[" + e.time + L"] " + e.log + L" " + (e.level == 1 ? L"Critical" : e.level == 2 ? L"Error" : L"Warning") + L" " + e.provider + L" #" + std::to_wstring(e.id) + L"\r\n    " + e.message.substr(0, 400) + L"\r\n"; if (ev.empty() && err.empty()) out += L"None in the chosen period.\r\n"; }
        out = R(out); if (out.size() > 1500000) out.resize(1500000); return js::Obj().boolean("ok", true).str("text", U(out)).done(); };
    h["report.save"] = [](const js::V& a) {
        string text = a.str("text"); if (text.empty() || text.size() > 2000000) return Err(L"There is no report to save.");
        wchar_t file[MAX_PATH] = L"FeatherDiagnosticReport.txt"; OPENFILENAMEW o{sizeof o}; o.hwndOwner = g_wnd; o.lpstrFilter = L"Text (*.txt)\0*.txt\0"; o.lpstrFile = file; o.nMaxFile = MAX_PATH; o.lpstrDefExt = L"txt"; o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&o)) return js::Obj().boolean("ok", true).boolean("cancelled", true).done(); return AtomicWrite(file, "\xEF\xBB\xBF" + text) ? js::Obj().boolean("ok", true).str("path", U(file)).done() : Err(L"The file could not be written."); };
    h["help.open"] = [](const js::V& a) { string id = a.str("id"); const wchar_t* url = id == "windows" ? L"https://support.microsoft.com/windows" : id == "health" ? L"https://learn.microsoft.com/windows/release-health/" : nullptr; if (!url) return Err(L"Unknown help page."); ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };

    // ---- recommendations ("safe to disable?") ----
    h["advice.status"] = [](const js::V&) { auto i = AdviceStatus(); return js::Obj().boolean("ok", true).str("version", U(i.version)).str("updated", U(i.updated)).str("source", U(i.source)).num("startupRules", i.startupRules).num("serviceRules", i.serviceRules).boolean("online", Cfg().onlineAdvice).done(); };
    h["advice.refresh"] = [](const js::V& a) {
        if (!Cfg().onlineAdvice && !a.boolean("force")) return Err(L"Downloading recommendations is turned off in Settings."); AdviceInfo i; wstring err; bool ok = AdviceRefresh(i, err);
        FILETIME ft; GetSystemTimeAsFileTime(&ft); Cfg().adviceCheck = (long long)((((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000000ULL) - 11644473600LL; SaveSettings();
        if (!ok) { auto cur = AdviceStatus(); return js::Obj().boolean("ok", false).str("error", U(err)).str("version", U(cur.version)).str("source", U(cur.source)).done(); } return js::Obj().boolean("ok", true).str("version", U(i.version)).str("updated", U(i.updated)).str("source", U(i.source)).num("startupRules", i.startupRules).num("serviceRules", i.serviceRules).done(); };

    // ---- Safe Mode ----
    h["safemode.status"] = [](const js::V&) {
        int kind = SafeModeKind(); js::Arr arr; static const struct { const wchar_t* name; const wchar_t* label; const wchar_t* needed; } svc[] = {{L"Schedule", L"Task Scheduler", L"scheduled startup entries (list and delete)"}, {L"msiserver", L"Windows Installer", L"uninstalling MSI-based programs"}, {L"Winmgmt", L"WMI", L"the Drivers page and some system info"}, {L"VSS", L"Volume Shadow Copy", L"restore points"}};
        SC_HANDLE m = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); for (auto& x : svc) { bool running = false; if (m) { SC_HANDLE sv = OpenServiceW(m, x.name, SERVICE_QUERY_STATUS); if (sv) { SERVICE_STATUS st{}; if (QueryServiceStatus(sv, &st)) running = st.dwCurrentState == SERVICE_RUNNING; CloseServiceHandle(sv); } } arr.add(js::Obj().str("name", U(x.name)).str("label", U(x.label)).str("needed", U(x.needed)).boolean("running", running).done()); } if (m) CloseServiceHandle(m);
        return js::Obj().boolean("ok", true).num("kind", kind).boolean("admin", IsElevated()).raw("services", arr.done()).done(); };
    h["safemode.fix"] = [](const js::V&) {
        if (!SafeModeKind()) return Err(L"Windows is not in Safe Mode."); if (!IsElevated()) return Err(L"Starting Windows services needs administrator rights. Restart as administrator from Settings."); int ok = 0, bad = 0; wstring first;
        for (const wchar_t* n : {L"Schedule", L"msiserver", L"Winmgmt"}) { wstring e; if (SvcStartByName(n, e)) { ++ok; WriteJournal(n, L"Safe Mode helper", L"stopped", L"started", L"ok", false); } else { ++bad; if (first.empty()) first = wstring(n) + L": " + e; } }
        return js::Obj().boolean("ok", bad == 0).num("started", ok).num("failed", bad).str("error", U(first)).done(); };

    // ---- dashboard ----
    h["dashboard.info"] = [](const js::V&) {
        ComputerInfo ci = GetComputerInfo(); OrgInfo og = GetOrgInfo(); WuLast wu = WuLastTimes(); HKEY k; wstring os; wchar_t b[256]{}; DWORD n = sizeof b - 2; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { if (RegQueryValueExW(k, L"ProductName", nullptr, nullptr, (BYTE*)b, &n) == ERROR_SUCCESS) os = b; wchar_t d[64]{}; DWORD n2 = sizeof d - 2; if (RegQueryValueExW(k, L"DisplayVersion", nullptr, nullptr, (BYTE*)d, &n2) == ERROR_SUCCESS && d[0]) os += L" " + wstring(d); RegCloseKey(k); }
        bool reboot = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS; if (reboot) RegCloseKey(k); else if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { reboot = true; RegCloseKey(k); }
        js::Arr tr; DWORD mask = GetLogicalDrives(); for (int i = 0; i < 26; ++i) if (mask & (1u << i)) { wchar_t r[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(r) != DRIVE_FIXED) continue; js::Arr pts; for (auto& p : StorageTrend(r)) pts.add(js::Obj().str("d", U(p.date)).num("free", (double)p.free).num("total", (double)p.total).done()); tr.add(js::Obj().str("path", U(r)).raw("points", pts.done()).done()); }
        return js::Obj().boolean("ok", true).str("maker", U(ci.maker)).str("model", U(ci.model)).str("os", U(os)).num("build", WindowsBuild()).num("uptimeSec", (double)(GetTickCount64() / 1000)).boolean("domain", og.domain).str("domainName", U(og.domainName)).boolean("azureAd", og.azureAd).boolean("mdm", og.mdm).boolean("policies", og.policies)
            .str("wuDetect", U(wu.detect)).str("wuInstall", U(wu.install)).boolean("pendingReboot", reboot).raw("trend", tr.done()).done(); };
    h["perf.sample"] = [](const js::V&) { auto p = SamplePerf(); return js::Obj().boolean("ok", true).num("cpu", p.cpu).num("memTotal", (double)p.memTotal).num("memAvail", (double)p.memAvail).num("netRx", (double)p.netRx).num("netTx", (double)p.netTx).done(); };
    h["wu.check"] = [](const js::V&) {
        IUpdateSession* ses = nullptr; IUpdateSearcher* sr = NewSearcher(&ses); if (!sr) { if (ses) ses->Release(); return Err(L"The Windows Update Agent is not available."); }
        BSTR crit = B(L"IsInstalled=0 and IsHidden=0"); ISearchResult* r = nullptr; HRESULT hr = sr->Search(crit, &r); SysFreeString(crit); if (FAILED(hr) || !r) { sr->Release(); ses->Release(); return Err(L"Windows Update could not be searched. Check your internet connection and that the Windows Update service is not disabled."); }
        IUpdateCollection* c = nullptr; r->get_Updates(&c); LONG n = 0; js::Arr arr; if (c) { c->get_Count(&n); for (LONG i = 0; i < n && i < 40; ++i) { IUpdate* u = nullptr; if (SUCCEEDED(c->get_Item(i, &u)) && u) { BSTR t = nullptr; u->get_Title(&t); arr.add(Q(TakeBstr(t))); u->Release(); } } c->Release(); } r->Release(); sr->Release(); ses->Release();
        return js::Obj().boolean("ok", true).num("count", n).raw("titles", arr.done()).done(); };

    // ---- startup impact (from Windows' own boot diagnostics) ----
    h["startup.impact"] = [](const js::V&) { wstring err; auto m = StartupImpact(err); js::Arr a; for (auto& kv : m) a.add(js::Obj().str("exe", U(kv.first)).num("boots", kv.second.boots).num("avgMs", kv.second.boots ? (double)kv.second.sumDeg / kv.second.boots : 0).num("maxMs", (double)kv.second.maxTotal).done()); return js::Obj().boolean("ok", true).str("error", U(err)).raw("items", a.done()).done(); };

    // ---- Microsoft Store apps and Windows updates ----
    h["store.list"] = [](const js::V&) {
        if (WindowsBuild() < 9200) return Err(L"Store apps are not available on this version of Windows."); std::string out; DWORD code = 1;
        if (!RunCapture(L"powershell.exe -NoProfile -NonInteractive -Command \"[Console]::OutputEncoding=[Text.Encoding]::UTF8; Get-AppxPackage | Select-Object Name,PackageFullName,Publisher,Version,InstallLocation,IsFramework,NonRemovable,SignatureKind | ConvertTo-Json -Compress\"", out, 60000, &code) || out.empty()) return Err(L"PowerShell could not list Store apps.");
        if (out.compare(0, 3, "\xEF\xBB\xBF") == 0) out.erase(0, 3); js::V v; if (!js::Parse(out, v)) return Err(L"The Store app list could not be read."); std::vector<const js::V*> items; if (v.t == js::V::Arr) for (auto& x : v.a) items.push_back(&x); else if (v.t == js::V::Obj) items.push_back(&v);
        js::Arr arr; std::lock_guard<std::mutex> l(g_m); g_store.clear(); for (auto* x : items) { string full = x->str("PackageFullName"); if (full.empty() || full.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._~-") != string::npos) continue; bool fw = x->boolean("IsFramework"), nonrem = x->boolean("NonRemovable"); int sig = (int)x->num("SignatureKind", 0); bool removable = !fw && !nonrem && sig != 4; if (removable) g_store.insert(full);
            string pub = x->str("Publisher"); size_t cn = pub.find("CN="); if (cn != string::npos) { size_t e = pub.find(',', cn); pub = pub.substr(cn + 3, e == string::npos ? string::npos : e - cn - 3); }
            arr.add(js::Obj().str("full", full).str("name", x->str("Name")).str("publisher", pub).str("version", x->str("Version")).str("location", x->str("InstallLocation")).boolean("framework", fw).boolean("removable", removable).boolean("system", sig == 4).done()); }
        return js::Obj().boolean("ok", true).raw("items", arr.done()).done(); };
    h["store.remove"] = [](const js::V& a) {
        if (!FeatureOn("storeRemove")) return Err(L"Removing Store apps is a beta feature. Turn it on in Settings > Beta features.");
        string full = a.str("full"); { std::lock_guard<std::mutex> l(g_m); if (!g_store.count(full)) return Err(L"That app is not in the removable list. Refresh and try again."); } std::string out; DWORD code = 1;
        wstring cmd = L"powershell.exe -NoProfile -NonInteractive -Command \"try { Remove-AppxPackage -Package '" + W(full) + L"' -ErrorAction Stop; 'OK' } catch { $_.Exception.Message }\""; bool ran = RunCapture(cmd, out, 180000, &code); bool ok = ran && out.find("OK") != string::npos && out.size() < 8;
        WriteJournal(W(full), L"Store app removal", L"installed", ok ? L"removed" : L"unchanged", ok ? L"ok (reinstall from the Microsoft Store)" : L"failed: " + W(out.substr(0, 200)), true); return ok ? Ok() : Err(L"Windows could not remove the app: " + W(out.substr(0, 300))); };
    h["hotfix.list"] = [](const js::V&) { wstring err; auto v = ListHotfixes(err); if (v.empty() && !err.empty()) return Err(err); js::Arr a; std::lock_guard<std::mutex> l(g_m); g_hot.clear(); for (auto& x : v) { string kb = U(x.kb); if (kb.size() > 2 && kb.size() < 12 && kb.compare(0, 2, "KB") == 0 && kb.find_first_not_of("0123456789", 2) == string::npos) g_hot.insert(kb.substr(2)); a.add(js::Obj().str("kb", kb).str("desc", U(x.desc)).str("date", U(x.date)).str("by", U(x.by)).done()); } return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["hotfix.uninstall"] = [](const js::V& a) { if (!FeatureOn("hotfixUninstall")) return Err(L"Uninstalling Windows updates is a beta feature. Turn it on in Settings > Beta features."); string kb = a.str("kb"); if (kb.compare(0, 2, "KB") == 0) kb = kb.substr(2); { std::lock_guard<std::mutex> l(g_m); if (!g_hot.count(kb)) return Err(L"That update is not in the installed list."); }
        wstring args = L"/uninstall /kb:" + W(kb) + L" /norestart"; bool ok = (INT_PTR)ShellExecuteW(nullptr, L"runas", L"wusa.exe", args.c_str(), nullptr, SW_SHOWNORMAL) > 32; WriteJournal(L"KB" + W(kb), L"Windows Update removal", L"installed", ok ? L"uninstall started (Windows asks you to confirm)" : L"not started", ok ? L"ok (Windows Update completes it)" : L"failed", false); return ok ? Ok() : Err(L"The Windows Update uninstaller could not be started (administrator permission may have been declined)."); };

    // ---- license ----
    h["license.status"] = [](const js::V&) { auto L = LicenseStatus(); return js::Obj().boolean("ok", true).boolean("valid", L.valid).str("state", U(L.state)).str("edition", U(L.edition)).str("name", U(L.name)).str("expires", U(L.expires)).str("id", U(L.id)).num("devices", L.devices).done(); };
    h["license.activate"] = [](const js::V& a) { wstring e; return LicenseActivate(a.str("token"), e) ? Ok() : Err(e); };
    h["license.remove"] = [](const js::V&) { LicenseRemove(); return Ok(); };
    // ---- feature flags and the administrator helper ----
    h["flags.list"] = [](const js::V&) { js::Arr a; for (auto& f : FeatureFlags()) a.add(js::Obj().str("id", f.id).str("title", f.title).str("why", f.why).boolean("beta", f.beta).boolean("on", FeatureOn(f.id)).done()); return js::Obj().boolean("ok", true).boolean("beta", Cfg().betaFeatures).raw("items", a.done()).done(); };
    h["flags.beta"] = [](const js::V& a) { Cfg().betaFeatures = a.boolean("on"); Cfg().flags.clear(); SaveSettings(); return Ok(); };   // the master switch resets per-feature overrides
    h["flags.set"] = [](const js::V& a) { string id = a.str("id"); bool known = false; for (auto& f : FeatureFlags()) if (id == f.id) known = true; if (!known) return Err(L"Unknown feature."); Cfg().flags[id] = a.boolean("on") ? 1 : 0; SaveSettings(); return Ok(); };
    h["broker.start"] = [](const js::V&) { if (!FeatureOn("adminHelper")) return Err(L"The administrator helper is a beta feature. Turn it on in Settings > Beta features."); wstring e; return BrokerStart(e) ? Ok() : Err(e); };
    h["broker.stop"] = [](const js::V&) { BrokerStop(); return Ok(); };
    h["update.rollback"] = [](const js::V&) {
        UpdateInfo u; if (!FetchLatestVersion(u, Cfg().updateChannel == L"beta") || !u.ok) return Err(u.error.empty() ? L"Could not reach the update server." : u.error); if (u.prevVersion.empty() || u.prevUrl.empty()) return Err(L"No previous version is published to roll back to.");
        UpdateInfo p; p.ok = true; p.version = u.prevVersion; p.url = u.prevUrl; p.sha256 = u.prevSha; p.size = u.prevSize; p.signer = u.signer; wstring path, err;
        if (!DownloadUpdate(p, path, err, nullptr, [](ULONGLONG got, ULONGLONG all) { Emit("download", js::Obj().num("got", (double)got).num("all", (double)all).done()); })) return Err(err);
        WriteJournal(L"Feather System Utilities", L"application update", FSU_VERSION, p.version, L"rollback started", false); if (!LaunchUpdateInstaller(path, err)) return Err(err); PostMessageW(g_wnd, WM_CLOSE, 0, 0); return Ok(); };
    h["update.openDownload"] = [](const js::V&) { UpdateInfo u; if (!FetchLatestVersion(u, Cfg().updateChannel == L"beta") || !u.ok || u.url.empty()) { wstring e = L"https://github.com/akshsim1991/FeatherSystemUtilities-releases"; ShellExecuteW(nullptr, L"open", e.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return Ok(); } ShellExecuteW(nullptr, L"open", u.url.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };   // opens the installer link (or the releases page) in the browser
    h["app.channel"] = [](const js::V& a) { Cfg().updateChannel = a.str("value") == "beta" ? L"beta" : L"stable"; SaveSettings(); return Ok(); };
    h["whatsnew.get"] = [](const js::V&) { string cur = U(FSU_VERSION), last = U(Cfg().lastSeenVersion); return js::Obj().boolean("ok", true).str("current", cur).str("lastSeen", last).raw("items", string((const char*)kWhatsNewJson, kWhatsNewJsonSize)).done(); };
    h["whatsnew.seen"] = [](const js::V&) { Cfg().lastSeenVersion = FSU_VERSION; SaveSettings(); return Ok(); };
    h["clean.runById"] = [](const js::V& a) {   // used by the administrator helper: the elevated process runs its OWN scan and only removes items it finds there, chosen by id
        std::vector<string> ids = a.strs("ids"); if (ids.empty()) return Err(L"Nothing selected."); CleanScan sc = ScanCleaner(Cfg().keepRecent); std::vector<CleanItem> sel;
        for (auto& i : sc.items) { string key = U(i.id.empty() ? i.name : i.id); if (std::find(ids.begin(), ids.end(), key) != ids.end()) sel.push_back(i); }
        if (sel.empty()) return Err(L"None of the selected items were found by the administrator scan. Scan again."); CleanResult r; RunClean(sel, Cfg().keepRecent, r, Cfg().quarantineLeftovers);
        return js::Obj().boolean("ok", true).num("bytes", (double)r.bytes).num("files", (double)r.files).num("done", r.ok).num("skipped", r.skipped).num("failed", r.failed).num("missing", (double)(ids.size() - sel.size())).str("firstError", U(r.firstErr)).str("backup", U(r.backupPath)).done(); };
    // ---- diagnostics ----
    h["log.write"] = [](const js::V& a) { static std::atomic<int> n{0}; static std::atomic<ULONGLONG> t0{0}; ULONGLONG now = GetTickCount64(); if (now - t0 > 60000) { t0 = now; n = 0; } if (++n > 40) return Ok(); string lv = a.str("level") == "error" ? "ERROR" : "WARN"; Log(lv.c_str(), L"[ui] " + W(a.str("text").substr(0, 1500))); return Ok(); };
    h["log.read"] = [](const js::V& a) { js::Arr arr; for (auto& l : LogTail((size_t)std::clamp(a.num("lines", 200), 10.0, 1000.0))) arr.add(Q(Redact(W(l), false))); return js::Obj().boolean("ok", true).raw("lines", arr.done()).done(); };
    h["log.clear"] = [](const js::V&) { LogClear(); return Ok(); };
    h["selftest.run"] = [](const js::V& a) { auto r = RunSelfTest(a.boolean("network", true), [](const wstring& n) { Progress(L"Self-test: " + n + L"…"); }); wstring rep = SelfTestReport(r); AtomicWrite(DataDir() + L"\\selftest.txt", U(rep)); js::Arr arr; for (auto& x : r) arr.add(js::Obj().str("name", U(x.name)).boolean("ok", x.ok).num("ms", x.ms).str("detail", U(Redact(x.detail, false))).done()); return js::Obj().boolean("ok", true).raw("items", arr.done()).str("text", U(rep)).str("path", U(DataDir() + L"\\selftest.txt")).done(); };
    h["app.copy"] = [](const js::V& a) { wstring t = W(a.str("text").substr(0, 200000)); if (!OpenClipboard(g_wnd)) return Err(L"The clipboard is busy."); EmptyClipboard(); HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * 2); if (g) { memcpy(GlobalLock(g), t.c_str(), (t.size() + 1) * 2); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); } CloseClipboard(); return Ok(); };
    h["shell.open"] = [](const js::V& a) { string w = a.str("what"); if (w == "tasks") ShellExecuteW(nullptr, L"open", L"taskschd.msc", nullptr, nullptr, SW_SHOWNORMAL); else if (w == "services") ShellExecuteW(nullptr, L"open", L"services.msc", nullptr, nullptr, SW_SHOWNORMAL); else if (w == "settings-apps") ShellExecuteW(nullptr, L"open", L"ms-settings:appsfeatures", nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };

    // ---- driver inventory and updates ----
    h["drivers.list"] = [](const js::V&) { wstring err; auto v = ListDrivers(err); if (v.empty() && !err.empty()) return Err(err); { std::lock_guard<std::mutex> l(g_m); g_drv = v; } js::Arr a; int problems = 0; for (auto& d : v) { a.add(DriverJson(d)); if (d.problem) ++problems; } ComputerInfo ci = GetComputerInfo(); return js::Obj().boolean("ok", true).raw("items", a.done()).num("problems", problems).num("pending", (double)Cfg().pendingVerify.size()).str("maker", U(ci.maker)).str("model", U(ci.model)).boolean("support", !ci.supportUrl.empty()).done(); };
    h["device.diagnose"] = [](const js::V& a) {
        wstring id = W(a.str("devId")); DriverRec rec; bool found = false; { std::lock_guard<std::mutex> l(g_m); for (auto& d : g_drv) if (d.devId == id) { rec = d; found = true; break; } } if (!found) return Err(L"That device is not in the current list. Refresh the Drivers page.");
        DeviceDiag d = DiagnoseDevice(id); if (!d.found) return Err(L"Windows no longer lists this device. It may have been unplugged or removed.");
        static const wchar_t* startNames[] = {L"Boot", L"System", L"Automatic", L"Manual", L"Disabled"}; static const wchar_t* stateNames[] = {L"", L"Stopped", L"Starting", L"Stopping", L"Running", L"Continuing", L"Pausing", L"Paused"};
        js::Arr ev; for (auto& e : d.events) ev.add(js::Obj().num("id", e.id).str("time", U(e.time)).str("problem", U(e.problem)).str("status", U(e.status)).str("driver", U(e.driver)).str("service", U(e.service)).done());
        js::Arr se; if (!d.service.empty()) { wstring qe; auto evs = QueryEvents(720, {L"System"}, {1, 2, 3}, 300, qe); wstring sn = LowerW(d.service), dn = LowerW(rec.name); int n = 0; for (auto& e : evs) { wstring m = LowerW(e.message); if (e.provider == L"Service Control Manager" && (m.find(sn) != wstring::npos || m.find(dn) != wstring::npos) && n < 4) { se.add(js::Obj().str("time", U(e.time)).num("id", e.id).str("message", U(e.message.substr(0, 300))).done()); ++n; } } }
        DeviceRule rule; bool haveRule = DeviceRuleFor(rec.hwid, rec.cls, d.problem, d.hasNtStatus ? Hex32Str(d.ntStatus) : L"", rec.name, rule); js::Obj ro; if (haveRule) { js::Arr st, th, ad; for (auto& x : rule.steps) st.add(Q(x)); for (auto& x : rule.then) th.add(Q(x)); for (auto& x : rule.advice) ad.add(Q(x)); ro.str("id", U(rule.id)).str("title", U(rule.title)).str("diagnosis", U(rule.diagnosis)).str("severity", U(rule.severity)).boolean("bios", rule.bios).raw("steps", st.done()).raw("then", th.done()).raw("advice", ad.done()); }
        BiosInfo bi; if (haveRule && rule.bios) bi = GetBiosInfo();
        return js::Obj().boolean("ok", true).raw("rule", haveRule ? ro.done() : "null").str("biosVendor", U(bi.vendor)).str("biosVersion", U(bi.version)).str("biosDate", U(bi.date)).str("rulesVersion", U(DeviceRulesVersion())).str("name", U(rec.name)).str("class", U(rec.cls)).str("provider", U(rec.provider)).str("hwid", U(rec.hwid)).num("problem", d.problem).str("problemText", U(d.problem ? ProblemText(d.problem) : L"Working properly")).boolean("started", d.started).boolean("driverLoaded", d.driverLoaded).boolean("needsRestart", d.needsRestart).boolean("disabledByUser", d.disabledByUser)
            .boolean("hasNt", d.hasNtStatus).str("ntHex", U(d.hasNtStatus ? Hex32Str(d.ntStatus) : L"")).str("ntText", U(d.ntText)).str("service", U(d.service)).boolean("serviceFound", d.serviceFound).str("serviceStart", U(d.serviceFound && d.serviceStartType < 5 ? startNames[d.serviceStartType] : L"")).num("serviceStartType", d.serviceStartType).str("serviceState", U(d.serviceFound && d.serviceState < 8 ? stateNames[d.serviceState] : L"")).boolean("serviceFileExists", d.serviceFileExists).str("servicePath", U(d.servicePath))
            .str("inf", U(d.infPath)).str("upper", U(d.upperFilters)).str("lower", U(d.lowerFilters)).raw("events", ev.done()).raw("sysEvents", se.done()).boolean("allowRestart", DeviceActionAllowed(rec.cls, "restart")).boolean("allowReinstall", DeviceActionAllowed(rec.cls, "reinstall")).done(); };
    h["device.report"] = [](const js::V& a) {
        wstring id = W(a.str("devId")); DriverRec rec; bool found = false; { std::lock_guard<std::mutex> l(g_m); for (auto& d : g_drv) if (d.devId == id) { rec = d; found = true; break; } } if (!found) return Err(L"That device is not in the current list.");
        DeviceDiag d = DiagnoseDevice(id); BiosInfo b = GetBiosInfo(); wstring t = L"Feather System Utilities " FSU_VERSION L" device report\r\nWindows build " + std::to_wstring(WindowsBuild()) + L"\r\nPC: " + GetComputerInfo().maker + L" " + GetComputerInfo().model + L"\r\nBIOS: " + b.vendor + L" " + b.version + L" " + b.date + L"\r\nRules version: " + DeviceRulesVersion() + L"\r\n\r\nDevice: " + rec.name + L"\r\nClass: " + rec.cls + L"\r\nProvider: " + rec.provider + L"\r\nVersion/date: " + rec.version + L" " + rec.date + L"\r\nINF: " + rec.inf + L"\r\nHardware IDs: " + rec.hwid + L"\r\nInstance ID: " + rec.devId + L"\r\nProblem code: " + std::to_wstring(d.problem) + L" (" + ProblemText(d.problem) + L")\r\nFailure status: " + (d.hasNtStatus ? Hex32Str(d.ntStatus) + L" " + d.ntText : L"none") + L"\r\nDriver loaded: " + (d.driverLoaded ? L"yes" : L"no") + L", started: " + (d.started ? L"yes" : L"no") + L"\r\nService: " + d.service + L"\r\n";
        for (auto& e : d.events) t += L"Kernel-PnP #" + std::to_wstring(e.id) + L" " + e.time + L" problem " + e.problem + L" status " + e.status + L" driver " + e.driver + L"\r\n"; t = Redact(t, false);
        if (!OpenClipboard(g_wnd)) return Err(L"The clipboard is busy."); EmptyClipboard(); HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * 2); if (g) { memcpy(GlobalLock(g), t.c_str(), (t.size() + 1) * 2); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); } CloseClipboard(); return Ok(); };
    h["device.fix"] = [](const js::V& a) {
        wstring id = W(a.str("devId")); DriverRec rec; bool found = false; { std::lock_guard<std::mutex> l(g_m); for (auto& d : g_drv) if (d.devId == id) { rec = d; found = true; break; } } if (!found) return Err(L"That device is not in the current list. Refresh the Drivers page.");
        DeviceFixResult r = FixDevice(id, rec.cls, rec.name, a.str("action")); return js::Obj().boolean("ok", r.ok).boolean("fixed", r.fixed).boolean("found", r.found).boolean("needsRestart", r.needsRestart).num("before", r.problemBefore).num("after", r.problemAfter).str("message", U(r.message)).str("error", r.ok ? "" : U(r.message)).done(); };
    h["system.restart"] = [](const js::V& a) { int d = (int)std::clamp(a.num("delay", 30), 10.0, 600.0); wstring cmd = L"shutdown.exe /r /t " + std::to_wstring(d) + L" /c \"Feather System Utilities: restarting to finish a repair. Run shutdown /a to cancel.\""; std::string out; DWORD code = 1; if (!RunCapture(cmd, out, 15000, &code) || code != 0) return Err(L"Windows would not schedule the restart: " + W(out.substr(0, 200))); WriteJournal(L"Windows", L"restart", L"running", L"restart in " + std::to_wstring(d) + L" s", L"scheduled", false); return Ok(); };
    h["system.cancelRestart"] = [](const js::V&) { std::string out; DWORD code = 1; RunCapture(L"shutdown.exe /a", out, 10000, &code); return code == 0 ? Ok() : Err(L"There is no restart to cancel."); };
    h["drivers.support"] = [](const js::V&) { ComputerInfo ci = GetComputerInfo(); if (ci.supportUrl.empty()) return Err(L"Feather does not know the support page for " + (ci.maker.empty() ? wstring(L"this PC's maker") : ci.maker) + L". Search for your PC model on the maker's website."); ShellExecuteW(nullptr, L"open", ci.supportUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };
    h["drivers.verify"] = [](const js::V&) {
        wstring err; auto inst = ListDrivers(err); auto& pv = Cfg().pendingVerify; js::Arr out; std::vector<wstring> keep; for (auto& x : pv) { string u = U(x); size_t p1 = u.find('|'), p2 = u.find('|', p1 + 1); string hw = u.substr(0, p1), title = u.substr(p1 + 1, p2 - p1 - 1); const DriverRec* m = nullptr; for (auto& d : inst) if (HwidMatch(d.hwid, W(hw))) { m = &d; break; }
            bool healthy = m && m->problem == 0; out.add(js::Obj().str("title", title).boolean("found", m != nullptr).boolean("healthy", healthy).num("problem", m ? m->problem : -1).str("text", U(m ? (m->problem ? ProblemText(m->problem) : L"Working properly") : L"Device not found (it may be unplugged)")).str("version", U(m ? m->version : L"")).done()); if (!healthy) keep.push_back(x); }
        pv = keep; SaveSettings(); return js::Obj().boolean("ok", true).raw("items", out.done()).done(); };
    h["drivers.check"] = [](const js::V&) {
        IUpdateSession* ses = nullptr; IUpdateSearcher* sr = nullptr; IUpdateCollection* col = nullptr; wstring err; if (!SearchDriverUpdates(&ses, &sr, &col, err)) { if (sr) sr->Release(); if (ses) ses->Release(); return Err(err); }
        wstring e2; auto installed = ListDrivers(e2); LONG n = 0; col->get_Count(&n); js::Arr arr; for (LONG i = 0; i < n; ++i) { IUpdate* u = nullptr; if (FAILED(col->get_Item(i, &u)) || !u) continue; BSTR t = nullptr; u->get_Title(&t); wstring title = TakeBstr(t); DECIMAL sz{}; double bytes = 0; if (SUCCEEDED(u->get_MaxDownloadSize(&sz))) { VarR8FromDec(&sz, &bytes); }
            wstring maker, model, cls, ver, hw, prov; { void* dv = nullptr; if (SUCCEEDED(u->QueryInterface(kIidWinDriverUpdate, &dv)) && dv) { IDispatch* dd = (IDispatch*)dv; maker = DispStr(dd, L"DriverManufacturer"); model = DispStr(dd, L"DriverModel"); cls = DispStr(dd, L"DriverClass"); hw = DispStr(dd, L"DriverHardwareID"); prov = DispStr(dd, L"DriverProvider"); ((IUnknown*)dv)->Release(); } }
            BSTR dsc = nullptr; u->get_Description(&dsc); wstring desc = TakeBstr(dsc); DATE ld = 0; u->get_LastDeploymentChangeTime(&ld); ver = DateText(ld); VARIANT_BOOL reboot = VARIANT_FALSE;
            string instJson = "null"; { for (auto& d : installed) if (HwidMatch(d.hwid, hw)) { instJson = DriverJson(d); break; } }
            if (IsIgnored("driver", UpdId(u))) { u->Release(); continue; }
            arr.add(js::Obj().str("id", U(UpdId(u))).str("title", U(title)).str("desc", U(desc)).str("date", U(ver)).num("size", bytes).str("maker", U(maker)).str("model", U(model)).str("class", U(cls)).str("hwid", U(hw)).str("provider", U(prov)).raw("installed", instJson).boolean("higherRisk", HigherRiskClass(cls)).done()); u->Release(); }
        col->Release(); sr->Release(); ses->Release(); return js::Obj().boolean("ok", true).raw("items", arr.done()).done(); };
    h["drivers.install"] = [](const js::V& a) {
        if (!IsElevated()) return Err(L"Installing drivers needs administrator rights. Restart as administrator from Settings."); std::set<string> want; for (auto& s : a.strs("ids")) want.insert(s); if (want.empty()) return Err(L"Nothing selected.");
        IUpdateSession* ses = nullptr; IUpdateSearcher* sr = nullptr; IUpdateCollection* col = nullptr; wstring err; if (!SearchDriverUpdates(&ses, &sr, &col, err)) { if (sr) sr->Release(); if (ses) ses->Release(); return Err(err); }
        std::vector<wstring> installedHw; IUpdateCollection* pick = nullptr; CoCreateInstance(kClsidUpdateColl, nullptr, CLSCTX_INPROC_SERVER, kIidUpdateCollection, (void**)&pick); LONG n = 0; col->get_Count(&n); std::vector<wstring> titles; if (pick) for (LONG i = 0; i < n; ++i) { IUpdate* u = nullptr; if (FAILED(col->get_Item(i, &u)) || !u) continue; if (want.count(U(UpdId(u)))) { u->AcceptEula(); LONG idx; pick->Add(u, &idx); BSTR t = nullptr; u->get_Title(&t); wstring ttl = TakeBstr(t); titles.push_back(ttl); { void* dv = nullptr; wstring hw; if (SUCCEEDED(u->QueryInterface(kIidWinDriverUpdate, &dv)) && dv) { hw = DispStr((IDispatch*)dv, L"DriverHardwareID"); ((IUnknown*)dv)->Release(); } if (!hw.empty()) installedHw.push_back(hw + L"|" + ttl + L"|" + NowIsoUtc()); } } u->Release(); }
        LONG pc = 0; if (pick) pick->get_Count(&pc); if (!pc) { if (pick) pick->Release(); col->Release(); sr->Release(); ses->Release(); return Err(L"The selected updates are no longer offered. Check again."); }
        Progress(L"Downloading " + std::to_wstring(pc) + L" driver update(s)…"); IUpdateDownloader* dl = nullptr; ses->CreateUpdateDownloader(&dl); IDownloadResult* dr = nullptr; HRESULT hr = E_FAIL; if (dl) { dl->put_Updates(pick); hr = dl->Download(&dr); }
        bool ok = SUCCEEDED(hr); OperationResultCode rc = orcNotStarted; if (dr) { dr->get_ResultCode(&rc); dr->Release(); } if (dl) dl->Release();
        bool reboot = false; int installed = 0;
        if (ok && rc == orcSucceeded) { Progress(L"Installing…"); IUpdateInstaller* in = nullptr; ses->CreateUpdateInstaller((IUpdateInstaller**)&in); IInstallationResult* ir = nullptr; if (in) { in->put_Updates(pick); in->put_AllowSourcePrompts(VARIANT_FALSE); hr = in->Install(&ir); if (SUCCEEDED(hr) && ir) { OperationResultCode irc = orcNotStarted; ir->get_ResultCode(&irc); VARIANT_BOOL rb = VARIANT_FALSE; ir->get_RebootRequired(&rb); reboot = rb != VARIANT_FALSE; ok = irc == orcSucceeded || irc == orcSucceededWithErrors; if (ok) { installed = (int)pc; for (auto& x : installedHw) Cfg().pendingVerify.push_back(x); SaveSettings(); } } else ok = false; if (ir) ir->Release(); in->Release(); } else ok = false; }
        else ok = false;
        wstring all; for (auto& t : titles) all += (all.empty() ? L"" : L"; ") + t; WriteJournal(L"Driver updates", L"Windows Update", L"" + std::to_wstring(pc) + L" update(s) selected", ok ? std::to_wstring(installed) + L" installed" : L"unchanged/unknown", ok ? (reboot ? L"ok (restart required)" : L"ok") : L"failed", false);
        pick->Release(); col->Release(); sr->Release(); ses->Release();
        if (!ok) return Err(L"Windows Update could not install the driver(s). Nothing was changed by Feather; check Windows Update for details."); return js::Obj().boolean("ok", true).num("installed", installed).boolean("reboot", reboot).done(); };

    // ---- processes ----
    h["processes.list"] = [](const js::V&) {
        std::vector<ProcRec> v; HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (snap == INVALID_HANDLE_VALUE) return Err(L"Could not list processes."); PROCESSENTRY32W pe{sizeof pe};
        if (Process32FirstW(snap, &pe)) do { ProcRec r{pe.th32ProcessID, pe.szExeFile, L"", L"", 0, 0, pe.cntThreads}; HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, r.pid); if (!h) h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, r.pid);
            if (h) { PROCESS_MEMORY_COUNTERS pmc{sizeof pmc}; if (GetProcessMemoryInfo(h, &pmc, sizeof pmc)) r.mem = pmc.WorkingSetSize; wchar_t p[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; if (QueryFullProcessImageNameW(h, 0, p, &n)) r.path = p; r.start = ProcStart(h); CloseHandle(h); }
            if (!r.path.empty()) r.publisher = InspectFile(r.path, false).publisher; v.push_back(r); } while (Process32NextW(snap, &pe)); CloseHandle(snap);
        js::Arr a; for (auto& r : v) a.add(js::Obj().num("pid", r.pid).str("name", U(r.name)).str("path", U(r.path)).str("publisher", U(r.publisher)).num("mem", (double)r.mem).num("threads", r.threads).boolean("protected", ProtectedProcess(r.name) || r.pid <= 4 || r.pid == GetCurrentProcessId()).done());
        { std::lock_guard<std::mutex> l(g_m); g_procs = std::move(v); } return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["processes.end"] = [](const js::V& a) {
        int ok = 0, bad = 0; wstring first; auto* pv = a.get("pids"); if (!pv) return Err(L"Nothing selected.");
        for (auto& x : pv->a) { DWORD pid = (DWORD)x.n; ProcRec rec{}; bool f = false; { std::lock_guard<std::mutex> l(g_m); for (auto& r : g_procs) if (r.pid == pid) { rec = r; f = true; break; } }
            if (!f || ProtectedProcess(rec.name) || pid <= 4 || pid == GetCurrentProcessId()) { ++bad; if (first.empty()) first = (f ? rec.name : L"process") + L" is protected or no longer listed."; continue; }
            HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) { ++bad; if (first.empty()) first = rec.name + L": " + Win32Err(GetLastError()); continue; }
            bool same = rec.start && ProcStart(h) == rec.start; bool done = same && TerminateProcess(h, 1); DWORD e = GetLastError(); CloseHandle(h);
            WriteJournal(rec.name, L"process " + std::to_wstring(pid), L"running", done ? L"terminated" : L"unchanged", done ? L"ok (end task)" : L"failed: " + (same ? Win32Err(e) : wstring(L"process changed")), false); if (done) ++ok; else { ++bad; if (first.empty()) first = rec.name + L": " + (same ? Win32Err(e) : wstring(L"the process changed since the list was made")); } }
        return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };

    // ---- system info and maintenance tools ----
    h["sys.info"] = [](const js::V&) {
        auto reg = [](HKEY root, const wchar_t* key, const wchar_t* val) { wchar_t b[256]{}; DWORD n = sizeof b - 2; HKEY k; wstring r; if (RegOpenKeyExW(root, key, 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { if (RegQueryValueExW(k, val, nullptr, nullptr, (BYTE*)b, &n) == ERROR_SUCCESS) r = b; RegCloseKey(k); } return r; };
        MEMORYSTATUSEX ms{sizeof ms}; GlobalMemoryStatusEx(&ms); wchar_t cn[MAX_COMPUTERNAME_LENGTH + 1]; DWORD cl = MAX_COMPUTERNAME_LENGTH + 1; if (!GetComputerNameW(cn, &cl)) cn[0] = 0; SYSTEM_INFO si; GetNativeSystemInfo(&si);
        wstring cpu = reg(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString"), os = reg(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName"), disp = reg(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");
        return js::Obj().boolean("ok", true).str("computer", U(cn)).str("os", U(os + (disp.empty() ? L"" : L" " + disp))).num("build", WindowsBuild()).str("cpu", U(cpu)).num("cores", si.dwNumberOfProcessors).str("arch", si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "64-bit" : "32-bit").num("ramTotal", (double)ms.ullTotalPhys).num("ramFree", (double)ms.ullAvailPhys).num("uptimeSec", (double)(GetTickCount64() / 1000)).done(); };
    h["tools.run"] = [](const js::V& a) {
        struct T { const char* id; const wchar_t* file; const wchar_t* args; bool admin; const wchar_t* label; };
        static const T tools[] = {
            {"sfc", L"cmd.exe", L"/k sfc /scannow", true, L"System File Checker"}, {"dism", L"cmd.exe", L"/k DISM /Online /Cleanup-Image /RestoreHealth", true, L"DISM repair"}, {"chkdsk", L"cmd.exe", L"/k chkdsk C: /scan", true, L"Check disk (online scan)"},
            {"dns", L"cmd.exe", L"/c ipconfig /flushdns & pause", false, L"Flush DNS cache"}, {"restorepoint", L"powershell.exe", L"-NoProfile -Command \"Checkpoint-Computer -Description 'Feather System Utilities' -RestorePointType MODIFY_SETTINGS; pause\"", true, L"Create restore point"},
            {"memdiag", L"mdsched.exe", L"", true, L"Windows Memory Diagnostic"}, {"devmgmt", L"devmgmt.msc", L"", false, L"Device Manager"}, {"diskmgmt", L"diskmgmt.msc", L"", false, L"Disk Management"}, {"taskschd", L"taskschd.msc", L"", false, L"Task Scheduler"}, {"services", L"services.msc", L"", false, L"Services"}, {"appwiz", L"appwiz.cpl", L"", false, L"Programs and Features"}, {"wu", L"ms-settings:windowsupdate", L"", false, L"Windows Update"}, {"taskmgr", L"taskmgr.exe", L"/7", false, L"Task Manager (Startup)"}, {"startupapps", L"ms-settings:startupapps", L"", false, L"Startup apps settings"}, {"sysprot", L"SystemPropertiesProtection.exe", L"", true, L"System Protection"}, {"troubleshoot", L"ms-settings:troubleshoot", L"", false, L"Windows Troubleshooters"}, {"netstatus", L"ms-settings:network-status", L"", false, L"Network status"}, {"eventvwr", L"eventvwr.msc", L"", false, L"Event Viewer"}, {"reliability", L"perfmon.exe", L"/rel", false, L"Reliability Monitor"}, {"resmon", L"resmon.exe", L"", false, L"Resource Monitor"}, {"msinfo", L"msinfo32.exe", L"", false, L"System Information"} };
        string id = a.str("id"); for (auto& t : tools) if (id == t.id) { bool ok = (INT_PTR)ShellExecuteW(nullptr, t.admin ? L"runas" : L"open", t.file, t.args[0] ? t.args : nullptr, nullptr, SW_SHOWNORMAL) > 32; if (ok && (id == "sfc" || id == "dism" || id == "chkdsk" || id == "restorepoint")) WriteJournal(t.label, L"maintenance tool", L"", L"started", L"ok (runs in its own window)", false); return ok ? Ok() : Err(L"The tool could not be started (administrator permission may have been declined)."); }
        return Err(L"Unknown tool."); };

    // ---- export, scan folders, delay removal ----
    h["export.csv"] = [](const js::V& a) {
        wchar_t file[MAX_PATH] = {}; wstring base = W(a.str("title", "export")); for (auto& c : base) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_'; wcsncpy(file, (base + L".csv").c_str(), MAX_PATH - 1);
        OPENFILENAMEW o{sizeof o}; o.hwndOwner = g_wnd; o.lpstrFilter = L"CSV (*.csv)\0*.csv\0All files\0*.*\0"; o.lpstrFile = file; o.nMaxFile = MAX_PATH; o.lpstrDefExt = L"csv"; o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&o)) return js::Obj().boolean("ok", true).boolean("cancelled", true).done();
        string out = "\xEF\xBB\xBF"; std::vector<wstring> cols; for (auto& c : a.strs("cols")) cols.push_back(W(c)); out += U(CsvRow(cols)) + "\r\n";
        if (auto* rv = a.get("rows")) for (auto& r : rv->a) { std::vector<wstring> f; for (auto& c : r.a) f.push_back(W(c.t == js::V::Str ? c.s : c.t == js::V::Num ? std::to_string((long long)c.n) : string())); out += U(CsvRow(f)) + "\r\n"; }
        return AtomicWrite(file, out) ? js::Obj().boolean("ok", true).str("path", U(file)).done() : Err(L"The file could not be written."); };
    h["scan.folders"] = [](const js::V&) { auto& c = Cfg(); auto v = c.scanRoots.empty() ? DefaultScanRoots() : c.scanRoots; js::Arr a; for (auto& x : v) a.add(Q(x)); return js::Obj().boolean("ok", true).boolean("custom", !c.scanRoots.empty()).raw("items", a.done()).done(); };
    h["scan.add"] = [](const js::V&) {
        IFileOpenDialog* d = nullptr; if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&d)) || !d) return Err(L"The folder picker is not available."); DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        wstring path; if (SUCCEEDED(d->Show(g_wnd))) { IShellItem* it = nullptr; if (SUCCEEDED(d->GetResult(&it)) && it) { PWSTR p = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { path = p; CoTaskMemFree(p); } it->Release(); } } d->Release();
        if (path.empty()) return js::Obj().boolean("ok", true).boolean("cancelled", true).done(); auto& c = Cfg(); if (c.scanRoots.empty()) c.scanRoots = DefaultScanRoots(); for (auto& x : c.scanRoots) if (_wcsicmp(x.c_str(), path.c_str()) == 0) return Ok(); c.scanRoots.push_back(path); SaveSettings(); return Ok(); };
    h["scan.remove"] = [](const js::V& a) { auto& c = Cfg(); if (c.scanRoots.empty()) c.scanRoots = DefaultScanRoots(); size_t i = (size_t)a.num("i", -1); if (i < c.scanRoots.size()) c.scanRoots.erase(c.scanRoots.begin() + i); SaveSettings(); return Ok(); };
    h["scan.reset"] = [](const js::V&) { Cfg().scanRoots.clear(); SaveSettings(); return Ok(); };
    h["startup.removeDelay"] = [findSu](const js::V& a) { int ok = 0, bad = 0; wstring first; for (auto& k : a.strs("keys")) { StartupEntry e; if (!findSu(k, e) || !e.managedDelay) { ++bad; continue; } wstring err; if (RemoveDelay(e, err)) ++ok; else { ++bad; if (first.empty()) first = e.name + L": " + err; } } return js::Obj().boolean("ok", bad == 0).num("done", ok).num("failed", bad).str("error", U(first)).done(); };
    return h;
}

string BrokerExecute(const string& cmd, const string& req) {
    js::V r; js::Parse(req, r); js::V args; if (auto* a = r.get("args")) args = *a; auto& hs = Handlers(); auto it = hs.find(cmd); if (it == hs.end()) return Err(L"Unknown command.");
    if (cmd.rfind("startup.", 0) == 0) { bool empty; { std::lock_guard<std::mutex> l(g_m); empty = g_su.entries.empty(); } if (empty) hs["startup.list"](js::V()); }
    if (cmd.rfind("services.", 0) == 0) { bool empty; { std::lock_guard<std::mutex> l(g_m); empty = g_sv.entries.empty(); } if (empty) hs["services.list"](js::V()); }
    return it->second(args);
}
static void OnMessage(const string& text) {
    js::V v; if (!js::Parse(text, v) || v.t != js::V::Obj) return; long id = (long)v.num("id", -1); string cmd = v.str("cmd"); auto& hs = Handlers(); auto it = hs.find(cmd);
    if (it == hs.end()) { PostToPage(js::Obj().num("id", (double)id).raw("data", Err(L"Unknown command.")).done()); return; }
    js::V args; if (auto* a = v.get("args")) args = *a;
    if (!IsElevated() && BrokerAllows(cmd) && BrokerConnected()) { string c = cmd; RunAsync(id, [c](const js::V& a2) { string res; wstring e; if (!BrokerCall(c, js::Dump(a2), res, e)) return Err(e); return res; }, args); return; }   // administrator helper is running: use it
    RunAsync(id, it->second, args);
}

static void Layout() { if (g_ctl) { RECT r; GetClientRect(g_wnd, &r); g_ctl->put_Bounds(r); } }

static LRESULT CALLBACK HostProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE: Layout(); return 0;
    case WM_GETMINMAXINFO: { auto* mm = (MINMAXINFO*)l; UINT d = DpiForWindow(h); mm->ptMinTrackSize.x = MulDiv(1000, d, 96); mm->ptMinTrackSize.y = MulDiv(640, d, 96); return 0; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); HBRUSH b = CreateSolidBrush(RGB(247, 245, 240)); FillRect((HDC)w, &r, b); DeleteObject(b); return 1; }
    case WM_WV_POST: { std::unique_ptr<wstring> s((wstring*)l); if (g_wv) g_wv->PostWebMessageAsJson(s->c_str()); return 0; }
    case WM_APP + 2: case WM_WV_PAGE: { if (IsIconic(h)) ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); static const char* pages[] = {"today", "speed", "toolbox", "startup", "services", "drivers", "programs", "clean", "large", "dupes", "empty", "history", "settings"};
        if (g_wv && w < 13) { wstring js = L"{\"event\":\"goto\",\"data\":{\"page\":\"" + W(pages[w]) + L"\"}}"; g_wv->PostWebMessageAsJson(js.c_str()); } return 0; }
    case WM_WV_RELOAD: if (g_wv && !g_html.empty()) { Log("WARN", L"The web view's page process crashed; reloading the interface."); g_ready = false; g_wv->NavigateToString(g_html.c_str()); } return 0;
    case WM_WV_FAIL: g_failed = true; DestroyWindow(h); return 0;
    case WM_TIMER: if (w == 77) { KillTimer(h, 77); if (!g_ready) { g_failed = true; DestroyWindow(h); } } return 0;
    case WM_CLOSE: { auto& c = Cfg(); c.maximized = IsZoomed(h) != 0; if (!c.maximized && !IsIconic(h)) { RECT r; GetWindowRect(h, &r); UINT d = DpiForWindow(h); c.winX = r.left; c.winY = r.top; c.winW = MulDiv(r.right - r.left, 96, d); c.winH = MulDiv(r.bottom - r.top, 96, d); } SaveSettings(); DestroyWindow(h); return 0; }
    case WM_DESTROY: g_cancel = true; ++g_stGen; if (g_ctl) { g_ctl->Close(); g_ctl->Release(); g_ctl = nullptr; } if (g_wv) { g_wv->Release(); g_wv = nullptr; } PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

typedef HRESULT(STDAPICALLTYPE* PFN_Create)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
typedef HRESULT(STDAPICALLTYPE* PFN_Version)(PCWSTR, LPWSTR*);

bool WebUiAvailable() {
    wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); PathRemoveFileSpecW(p); wstring dll = wstring(p) + L"\\WebView2Loader.dll"; HMODULE m = LoadLibraryW(dll.c_str()); if (!m) return false;
    auto ver = (PFN_Version)GetProcAddress(m, "GetAvailableCoreWebView2BrowserVersionString"); LPWSTR v = nullptr; bool ok = ver && SUCCEEDED(ver(nullptr, &v)) && v && *v; if (v) CoTaskMemFree(v); FreeLibrary(m); return ok;
}

int RunWebUi(HINSTANCE inst, int show, int firstPage, HANDLE* instMutex) {   // returns 0 = ran and exited, 1 = could not start (caller falls back to the classic UI)
    g_instMutex = instMutex; RecordStorageTrend(); PruneJournal(Cfg().logRetentionDays); if (!WebUiAvailable()) return 1;
    wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); PathRemoveFileSpecW(p); HMODULE ldr = LoadLibraryW((wstring(p) + L"\\WebView2Loader.dll").c_str()); auto create = ldr ? (PFN_Create)GetProcAddress(ldr, "CreateCoreWebView2EnvironmentWithOptions") : nullptr; if (!create) return 1;
    WNDCLASSW wc{}; wc.hInstance = inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1)); wc.lpfnWndProc = HostProc; wc.lpszClassName = L"FeatherSystemUtilitiesWnd"; RegisterClassW(&wc);
    HDC sdc = GetDC(nullptr); int sdpi = GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(nullptr, sdc); auto& cf = Cfg();
    g_wnd = CreateWindowExW(0, wc.lpszClassName, L"Feather System Utilities", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, cf.winX, cf.winY, MulDiv(cf.winW, sdpi, 96), MulDiv(cf.winH, sdpi, 96), nullptr, nullptr, inst, nullptr); if (!g_wnd) return 1;
    ShowWindow(g_wnd, cf.maximized ? SW_SHOWMAXIMIZED : show); UpdateWindow(g_wnd);
    wstring udf = SubDir(L"webview2"); bool devtools = GetCommandLineW() && wcsstr(GetCommandLineW(), L"--devtools");
    auto* onEnv = new Cb<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>([devtools, firstPage](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
        if (FAILED(hr) || !env) { PostMessageW(g_wnd, WM_WV_FAIL, 0, 0); return S_OK; }
        auto* onCtl = new Cb<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*>([devtools, firstPage](HRESULT hr2, ICoreWebView2Controller* ctl) -> HRESULT {
            if (FAILED(hr2) || !ctl) { PostMessageW(g_wnd, WM_WV_FAIL, 0, 0); return S_OK; }
            g_ctl = ctl; g_ctl->AddRef(); g_ctl->get_CoreWebView2(&g_wv); Layout();
            ICoreWebView2Settings* st = nullptr; if (SUCCEEDED(g_wv->get_Settings(&st)) && st) { st->put_AreDefaultContextMenusEnabled(FALSE); st->put_IsStatusBarEnabled(FALSE); st->put_AreDevToolsEnabled(devtools ? TRUE : FALSE); st->put_IsZoomControlEnabled(FALSE); st->put_IsBuiltInErrorPageEnabled(FALSE);
                ICoreWebView2Settings3* s3 = nullptr; if (SUCCEEDED(st->QueryInterface(IidOf<ICoreWebView2Settings3>::get(), (void**)&s3)) && s3) { s3->put_AreBrowserAcceleratorKeysEnabled(devtools ? TRUE : FALSE); s3->Release(); } st->Release(); }
            EventRegistrationToken tk;
            g_wv->add_WebMessageReceived(new Cb<ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*>([](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT {
                LPWSTR j = nullptr; if (SUCCEEDED(a->get_WebMessageAsJson(&j)) && j) { OnMessage(U(j)); CoTaskMemFree(j); } return S_OK; }), &tk);
            g_wv->add_NavigationStarting(new Cb<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*>([](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* a) -> HRESULT {
                LPWSTR u = nullptr; if (SUCCEEDED(a->get_Uri(&u)) && u) { if (wcsncmp(u, L"about:", 6) != 0 && wcsncmp(u, L"data:", 5) != 0) a->put_Cancel(TRUE); CoTaskMemFree(u); } return S_OK; }), &tk);
            g_wv->add_NewWindowRequested(new Cb<ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*>([](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* a) -> HRESULT { a->put_Handled(TRUE); return S_OK; }), &tk);
            g_wv->add_NavigationCompleted(new Cb<ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*>([firstPage](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                if (!g_ready) { g_ready = true; KillTimer(g_wnd, 77); if (firstPage > 0) PostMessageW(g_wnd, WM_WV_PAGE, (WPARAM)firstPage, 0); } return S_OK; }), &tk);
            g_wv->add_ProcessFailed(new Cb<ICoreWebView2ProcessFailedEventHandler, ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*>([](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* a) -> HRESULT {
                COREWEBVIEW2_PROCESS_FAILED_KIND k = COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED; a->get_ProcessFailedKind(&k);
                if (k == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED) { Log("FATAL", L"The WebView2 browser process exited; switching to the classic window."); PostMessageW(g_wnd, WM_WV_FAIL, 0, 0); }
                else if (k == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED) PostMessageW(g_wnd, WM_WV_RELOAD, 0, 0);
                else Log("WARN", L"The web view's page process is not responding."); return S_OK; }), &tk);
            g_html = FromUtf8(string((const char*)kUiHtml, kUiHtmlSize)); g_wv->NavigateToString(g_html.c_str()); return S_OK; });
        env->CreateCoreWebView2Controller(g_wnd, onCtl); onCtl->Release(); return S_OK; });
    SetTimer(g_wnd, 77, 25000, nullptr);   // if the web view never comes up (e.g. Safe Mode, broken runtime) fall back to the classic window
    HRESULT hr = create(nullptr, udf.c_str(), nullptr, onEnv); onEnv->Release();
    if (FAILED(hr)) { DestroyWindow(g_wnd); UnregisterClassW(wc.lpszClassName, inst); return 1; }
    MSG msg; while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    UnregisterClassW(wc.lpszClassName, inst); return g_failed ? 1 : 0;
}
}
