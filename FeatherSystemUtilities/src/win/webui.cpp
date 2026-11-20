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
#include <set>
#include <fstream>
#include <filesystem>
#include <algorithm>

extern const unsigned char kUiHtml[];
extern const unsigned long kUiHtmlSize;

namespace fsu {
using std::wstring; using std::string;

static HWND g_wnd; static ICoreWebView2Controller* g_ctl; static ICoreWebView2* g_wv; static bool g_ready, g_failed; static HANDLE* g_instMutex;
enum { WM_WV_POST = WM_APP + 40, WM_WV_FAIL, WM_WV_PAGE };
static std::mutex g_m;
static std::atomic<bool> g_cancel{false}; static std::atomic<int> g_stGen{0};
static StartupScan g_su; static ProgramScan g_pg; static ServiceScan g_sv; static CleanScan g_cl; static int g_clGen; static std::vector<CleanItem> g_left; static int g_leftGen; static FileScan g_fs[3]; static int g_fsGen[3];

static string U(const wstring& w) { return ToUtf8(w); }
static wstring W(const string& s) { return FromUtf8(s); }
static string Q(const wstring& w) { return js::Q(U(w)); }
static string Err(const wstring& e) { return js::Obj().boolean("ok", false).str("error", U(e)).done(); }

// ---------- COM callback helper ----------
// MinGW cannot resolve __uuidof for the WebView2 header's interfaces, so the IIDs are listed here (values from WebView2.h).
static GUID ParseGuid(const char* t) { unsigned a, b, c, d[8]; sscanf(t, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]); GUID g{a, (unsigned short)b, (unsigned short)c, {}}; for (int i = 0; i < 8; ++i) g.Data4[i] = (unsigned char)d[i]; return g; }
template <class T> struct IidOf;
#define FSU_IID(T, S) template <> struct IidOf<T> { static const GUID& get() { static GUID g = ParseGuid(S); return g; } };
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
static void PostToPage(const string& json) { PostMessageW(g_wnd, WM_WV_POST, 0, (LPARAM) new wstring(W(json))); }
static void Emit(const char* ev, const string& data) { PostToPage(js::Obj().str("event", ev).raw("data", data).done()); }
static void Progress(const wstring& m) { Emit("progress", js::Obj().str("text", U(m)).done()); }

// ---------- JSON builders for engine structs ----------
static bool CanDeleteEntry(const StartupEntry& e) {
    if (e.managedDelay) return false;
    return e.type == L"Registry" || e.type == L"Registry (RunOnce)" || e.type.rfind(L"Scheduled task", 0) == 0 || e.type.rfind(L"Explorer add-on", 0) == 0 || e.type == L"Startup folder";
}
static string CleanItemJson(const CleanItem& c, size_t i) {
    return js::Obj().num("i", (double)i).str("name", U(c.name)).str("type", U(c.type)).str("detail", U(c.detail)).str("why", U(c.why)).str("loc", U(c.location)).num("bytes", (double)c.bytes).num("files", (double)c.files)
        .num("risk", c.risk).num("kind", c.kind).boolean("admin", c.needsAdmin).done();
}
static string CleanJson() {
    js::Arr a; for (size_t i = 0; i < g_cl.items.size(); ++i) a.add(CleanItemJson(g_cl.items[i], i));
    return js::Obj().num("gen", g_clGen).raw("items", a.done()).str("coverage", U(g_cl.coverage)).num("totalBytes", (double)g_cl.totalBytes).num("registry", g_cl.registryCount).done();
}
static string StartupJson(const StartupEntry& e) {
    wstring prog = e.program; bool exists = prog.empty() ? true : FileExistsExpanded(prog);
    return js::Obj().str("key", U(e.key)).str("name", U(e.name)).str("state", U(e.state)).str("type", U(e.type)).str("starts", U(e.starts)).str("scope", U(e.scope)).str("command", U(e.command)).str("program", U(e.program))
        .str("publisher", U(e.publisher)).str("version", U(e.version)).str("signature", U(e.signature)).str("warnings", U(e.warnings)).str("location", U(e.location)).str("firstSeen", U(e.firstSeen))
        .boolean("canToggle", e.toggle != Toggle::None && !e.managedDelay).boolean("canDelete", CanDeleteEntry(e)).boolean("canDelay", CanDelay(e)).boolean("microsoft", e.microsoft).boolean("system", e.isSystem).boolean("recent", e.recent)
        .boolean("missing", !exists).boolean("delayed", e.managedDelay).str("details", U(StartupDetailsText(e))).done();
}
static string ProgramJson(const ProgramEntry& e) {
    UninstallPlan s = PlanUninstall(e, true);
    return js::Obj().str("key", U(e.key)).str("name", U(e.name)).str("publisher", U(e.publisher)).str("version", U(e.version)).str("date", U(e.installDate)).num("size", (double)e.sizeBytes).str("type", U(e.type)).str("scope", U(e.scope))
        .str("location", U(e.installLocation)).boolean("canUninstall", e.canUninstall).boolean("silent", s.available).str("silentNote", U(s.note)).done();
}
static string ServiceJson(const ServiceEntry& s) {
    js::Arr d; for (auto& x : s.deps) d.add(Q(x));
    return js::Obj().str("name", U(s.name)).str("display", U(s.display)).str("state", U(s.state)).str("start", U(s.start)).str("safety", U(s.safety)).str("account", U(s.account)).str("description", U(s.description)).str("warnings", U(s.warnings))
        .str("path", U(s.path)).str("publisher", U(s.publisher)).str("signature", U(s.signature)).num("pid", s.pid).boolean("running", s.stateCode == SERVICE_RUNNING).boolean("stopped", s.stateCode == SERVICE_STOPPED)
        .boolean("canStop", (s.controls & SERVICE_ACCEPT_STOP) != 0).boolean("critical", s.critical).boolean("driver", s.isDriver).raw("deps", d.done()).done();
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
    CleanResult r; RunClean({it}, false, r); bool done = r.ok == 1 && r.failed == 0 && r.skipped == 0; if (!done) err = r.firstErr.empty() ? L"Windows refused the change (administrator rights may be required)." : r.firstErr; return done;
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

static string ExeDir() { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); PathRemoveFileSpecW(p); return U(p); }
static bool Newer(const wstring& v) { return CompareVersions(v, FSU_VERSION) > 0; }

static std::map<string, Fn>& Handlers() {
    static std::map<string, Fn> h; if (!h.empty()) return h;

    h["app.info"] = [](const js::V&) {
        auto& c = Cfg(); return js::Obj().boolean("ok", true).str("version", U(FSU_VERSION)).boolean("admin", IsElevated()).num("build", WindowsBuild()).str("dataDir", U(DataDir())).str("built", __DATE__).boolean("autoUpdate", c.autoUpdate).boolean("keepRecent", c.keepRecent)
            .num("largeMinMB", c.largeMinMB).num("dupMinKB", c.dupMinKB).boolean("confirmBatch", c.confirmBatch).done(); };
    h["app.set"] = [](const js::V& a) {
        auto& c = Cfg(); string k = a.str("key"); double v = a.num("value");
        if (k == "autoUpdate") c.autoUpdate = v != 0; else if (k == "keepRecent") c.keepRecent = v != 0; else if (k == "confirmBatch") c.confirmBatch = v != 0;
        else if (k == "largeMinMB") c.largeMinMB = (int)std::clamp(v, 1.0, 1000000.0); else if (k == "dupMinKB") c.dupMinKB = (int)std::clamp(v, 1.0, 100000000.0); else return Err(L"Unknown setting.");
        SaveSettings(); return Ok(); };
    h["app.restartAdmin"] = [](const js::V&) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH); if (g_instMutex && *g_instMutex) { CloseHandle(*g_instMutex); *g_instMutex = nullptr; }
        if ((INT_PTR)ShellExecuteW(nullptr, L"runas", exe, nullptr, nullptr, SW_SHOWNORMAL) > 32) { PostMessageW(g_wnd, WM_CLOSE, 0, 0); return Ok(); }
        *g_instMutex = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance"); return Err(L"Administrator permission was not granted."); };
    h["app.openData"] = [](const js::V& a) { string w = a.str("what"); wstring p = w == "backups" ? SubDir(L"backups") : w == "regbackups" ? SubDir(L"regbackups") : DataDir(); ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };

    // ---- cleaner ----
    h["clean.scan"] = [](const js::V& a) {
        bool deep = a.boolean("deep"); CleanScan s = ScanCleaner(Cfg().keepRecent);
        if (deep) { g_cancel = false; CleanScan d = ScanDeep(&g_cancel, [](const wstring& m) { Progress(m); }); std::set<wstring> have; for (auto& i : s.items) have.insert(i.id); for (auto& i : d.items) if (!have.count(i.id)) { s.items.push_back(i); s.totalBytes += i.bytes; } s.coverage += L" " + d.coverage; }
        std::lock_guard<std::mutex> l(g_m); g_cl = std::move(s); ++g_clGen; return Ok("scan", CleanJson()); };
    h["clean.run"] = [](const js::V& a) {
        std::vector<CleanItem> sel; { std::lock_guard<std::mutex> l(g_m); if ((int)a.num("gen", -1) != g_clGen) return Err(L"The list changed. Scan again.");
            if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < g_cl.items.size()) sel.push_back(g_cl.items[i]); } }
        if (sel.empty()) return Err(L"Nothing selected."); CleanResult r; RunClean(sel, Cfg().keepRecent, r);
        return js::Obj().boolean("ok", true).num("bytes", (double)r.bytes).num("files", (double)r.files).num("done", r.ok).num("skipped", r.skipped).num("failed", r.failed).str("firstError", U(r.firstErr)).str("backup", U(r.backupPath)).done(); };
    h["clean.cancel"] = [](const js::V&) { g_cancel = true; return Ok(); };

    // ---- startup ----
    h["startup.list"] = [](const js::V&) { StartupScan s = ScanStartup(); js::Arr a; for (auto& e : s.entries) a.add(StartupJson(e)); string cov = U(s.coverage); { std::lock_guard<std::mutex> l(g_m); g_su = std::move(s); } return js::Obj().boolean("ok", true).raw("items", a.done()).str("coverage", cov).done(); };
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
        bool silent = a.boolean("silent"), force = a.boolean("force"); std::vector<CleanItem> left; bool removed = false; wstring note;
        if (force) { Progress(L"Looking for leftovers of " + e.name + L"…"); left = FindLeftovers(e, true); removed = true; }
        else { UninstallPlan pl = PlanUninstall(e, silent); if (!pl.available) return Err(pl.note.empty() ? L"This uninstall method is not available." : pl.note);
            Progress((silent ? L"Uninstalling silently: " : L"Uninstalling: ") + e.name + L"…"); UninstallResult r; RunUninstall(e, silent, r, nullptr); removed = r.removed; note = r.note;
            if (removed) { Progress(L"Looking for leftovers…"); left = FindLeftovers(e, false); } }
        std::lock_guard<std::mutex> l(g_m); if (a.boolean("keep")) g_left.insert(g_left.end(), left.begin(), left.end()); else { g_left = left; ++g_leftGen; } js::Arr arr; for (size_t i = 0; i < g_left.size(); ++i) arr.add(CleanItemJson(g_left[i], i));
        return js::Obj().boolean("ok", true).boolean("removed", removed).str("note", U(note)).num("gen", g_leftGen).raw("leftovers", arr.done()).done(); };
    h["programs.removeLeftovers"] = [](const js::V& a) {
        std::vector<CleanItem> sel; { std::lock_guard<std::mutex> l(g_m); if ((int)a.num("gen", -1) != g_leftGen) return Err(L"The leftover list changed. Run the check again.");
            if (auto* v = a.get("ids")) for (auto& x : v->a) { size_t i = (size_t)x.n; if (x.t == js::V::Num && i < g_left.size()) sel.push_back(g_left[i]); } }
        if (sel.empty()) return Err(L"Nothing selected."); CleanResult r; RunClean(sel, false, r);
        return js::Obj().boolean("ok", true).num("bytes", (double)r.bytes).num("done", r.ok).num("skipped", r.skipped).num("failed", r.failed).str("firstError", U(r.firstErr)).done(); };

    // ---- services ----
    h["services.list"] = [](const js::V& a) { ServiceScan s = ScanServices(a.boolean("drivers")); js::Arr arr; for (auto& e : s.entries) arr.add(ServiceJson(e)); string cov = U(s.coverage); { std::lock_guard<std::mutex> l(g_m); g_sv = std::move(s); } return js::Obj().boolean("ok", true).raw("items", arr.done()).str("coverage", cov).done(); };
    h["services.op"] = [](const js::V& a) {
        ServiceEntry s; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& e : g_sv.entries) if (U(e.name) == a.str("name")) { s = e; f = true; break; } if (!f) return Err(L"Service not found. Refresh the list."); }
        string op = a.str("op"); wstring err; bool ok;
        if (op == "kill") ok = KillServiceProcess(s, g_sv.entries, err);
        else { SvcOp o = op == "start" ? SvcOp::Start : op == "stop" ? SvcOp::Stop : op == "restart" ? SvcOp::Restart : op == "pause" ? SvcOp::Pause : SvcOp::Resume; ok = ServiceOp(s, o, err); }
        return ok ? Ok() : Err(err); };
    h["services.startType"] = [](const js::V& a) {
        ServiceEntry s; { std::lock_guard<std::mutex> l(g_m); bool f = false; for (auto& e : g_sv.entries) if (U(e.name) == a.str("name")) { s = e; f = true; break; } if (!f) return Err(L"Service not found. Refresh the list."); }
        string t = a.str("type"); if (s.critical && t == "disabled") return Err(L"Blocked: this service is on the critical list (Windows may become unstable).");
        DWORD type = t == "auto" || t == "autodelay" ? SERVICE_AUTO_START : t == "manual" ? SERVICE_DEMAND_START : t == "disabled" ? SERVICE_DISABLED : 0; if (!type) return Err(L"Unknown start type.");
        wstring err, before; return SetServiceStartType(s.name, type, t == "autodelay", err, before) ? Ok() : Err(err); };

    // ---- file finders ----
    h["files.scan"] = [](const js::V& a) {
        string m = a.str("mode"); int mode = m == "dupes" ? 1 : m == "empty" ? 2 : 0; g_cancel = false; auto& c = Cfg();
        FileScanOpts o; o.roots = c.scanRoots.empty() ? DefaultScanRoots() : c.scanRoots; o.dupes = mode == 1; o.empty = mode == 2; o.minSize = mode == 0 ? (ULONGLONG)c.largeMinMB << 20 : mode == 1 ? (ULONGLONG)c.dupMinKB << 10 : 0; o.maxResults = 2000; o.cancel = &g_cancel; o.progress = [](const wstring& p) { Progress(p); };
        FileScan s; ScanFiles(o, s); js::Arr arr; for (size_t i = 0; i < s.files.size(); ++i) { auto& f = s.files[i]; arr.add(js::Obj().num("i", (double)i).str("name", U(f.name)).str("folder", U(f.folder)).str("ext", U(f.ext)).num("size", (double)f.size).str("date", U(FileDateText(f.mtime))).num("group", f.group).num("count", f.groupCount).str("hash", U(f.hash)).boolean("verified", f.verified).done()); }
        js::Obj r; r.boolean("ok", true).raw("items", arr.done()).str("coverage", U(s.coverage)).num("scanned", (double)s.scanned).num("totalBytes", (double)s.totalBytes).num("reclaimable", (double)s.reclaimable).num("groups", s.groups).boolean("cancelled", s.cancelled);
        std::lock_guard<std::mutex> l(g_m); g_fs[mode] = std::move(s); r.num("gen", ++g_fsGen[mode]); return r.done(); };
    h["files.cancel"] = [](const js::V&) { g_cancel = true; ++g_stGen; return Ok(); };
    h["files.openLocation"] = [](const js::V& a) { string m = a.str("mode"); int mode = m == "dupes" ? 1 : m == "empty" ? 2 : 0; wstring p; { std::lock_guard<std::mutex> l(g_m); size_t i = (size_t)a.num("i", -1); if ((int)a.num("gen", -1) != g_fsGen[mode] || i >= g_fs[mode].files.size()) return Err(L"The list changed."); p = g_fs[mode].files[i].path; } if (p.empty()) return Err(L"Already removed."); wstring e; return OpenFileLocationEx(p, e) ? Ok() : Err(e); };
    h["files.remove"] = [](const js::V& a) {
        string m = a.str("mode"); int mode = m == "dupes" ? 1 : m == "empty" ? 2 : 0; bool permanent = a.boolean("permanent"); std::vector<size_t> idx; std::vector<wstring> paths; ULONGLONG total = 0; wstring warn; std::vector<wstring> okPaths;
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
        js::Arr arr; size_t n = 0; for (auto it = lines.rbegin(); it != lines.rend() && n < lim; ++it) { js::V v; if (js::Parse(*it, v) && v.t == js::V::Obj) { arr.add(js::Obj().str("time", v.str("time")).str("target", v.str("target")).str("source", v.str("source")).str("before", v.str("before")).str("after", v.str("after")).str("result", v.str("result")).boolean("reversible", v.boolean("reversible")).done()); ++n; } }
        return js::Obj().boolean("ok", true).raw("items", arr.done()).done(); };
    h["backups.list"] = [](const js::V&) {
        js::Arr a; auto add = [&](const std::vector<BackupInfo>& v, const char* kind) { for (auto& b : v) { SYSTEMTIME st; FileTimeToSystemTime(&b.written, &st); char t[32]; snprintf(t, sizeof t, "%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute); a.add(js::Obj().str("path", U(b.path)).str("label", U(b.label)).str("time", t).str("kind", kind).done()); } };
        add(ListBackups(), "startup"); add(ListRegBackups(), "registry"); return js::Obj().boolean("ok", true).raw("items", a.done()).done(); };
    h["backups.restore"] = [](const js::V& a) {
        wstring p = W(a.str("path")); string kind = a.str("kind"); bool found = false; for (auto& b : kind == "registry" ? ListRegBackups() : ListBackups()) if (_wcsicmp(b.path.c_str(), p.c_str()) == 0) { found = true; break; }
        if (!found) return Err(L"Backup not found."); wstring err, sum; bool ok = kind == "registry" ? RestoreRegBackup(p, err, sum) : RestoreBackup(p, err, sum); return ok ? js::Obj().boolean("ok", true).str("summary", U(sum)).done() : Err(err); };

    // ---- updates ----
    h["update.check"] = [](const js::V&) { UpdateInfo u; if (!FetchLatestVersion(u) || !u.ok) return js::Obj().boolean("ok", false).str("error", U(u.error.empty() ? L"Could not reach the update server." : u.error)).done();
        return js::Obj().boolean("ok", true).boolean("newer", Newer(u.version)).str("version", U(u.version)).str("date", U(u.date)).str("notes", U(u.notes)).num("size", (double)u.size).done(); };
    h["update.install"] = [](const js::V&) {
        UpdateInfo u; if (!FetchLatestVersion(u) || !u.ok || !Newer(u.version)) return Err(L"No newer version is available."); wstring path, err;
        if (!DownloadUpdate(u, path, err, nullptr, [](ULONGLONG got, ULONGLONG all) { Emit("download", js::Obj().num("got", (double)got).num("all", (double)all).done()); })) return Err(err);
        if (!LaunchUpdateInstaller(path, err)) return Err(err); PostMessageW(g_wnd, WM_CLOSE, 0, 0); return Ok(); };
    h["shell.open"] = [](const js::V& a) { string w = a.str("what"); if (w == "tasks") ShellExecuteW(nullptr, L"open", L"taskschd.msc", nullptr, nullptr, SW_SHOWNORMAL); else if (w == "services") ShellExecuteW(nullptr, L"open", L"services.msc", nullptr, nullptr, SW_SHOWNORMAL); else if (w == "settings-apps") ShellExecuteW(nullptr, L"open", L"ms-settings:appsfeatures", nullptr, nullptr, SW_SHOWNORMAL); return Ok(); };

    // ---- driver inventory and updates ----
    h["drivers.list"] = [](const js::V&) { wstring err; string j = DriversListJson(err); if (j.empty()) return Err(err); return js::Obj().boolean("ok", true).raw("items", j).done(); };
    h["drivers.check"] = [](const js::V&) {
        IUpdateSession* ses = nullptr; IUpdateSearcher* sr = nullptr; IUpdateCollection* col = nullptr; wstring err; if (!SearchDriverUpdates(&ses, &sr, &col, err)) { if (sr) sr->Release(); if (ses) ses->Release(); return Err(err); }
        LONG n = 0; col->get_Count(&n); js::Arr arr; for (LONG i = 0; i < n; ++i) { IUpdate* u = nullptr; if (FAILED(col->get_Item(i, &u)) || !u) continue; BSTR t = nullptr; u->get_Title(&t); wstring title = TakeBstr(t); DECIMAL sz{}; double bytes = 0; if (SUCCEEDED(u->get_MaxDownloadSize(&sz))) { VarR8FromDec(&sz, &bytes); }
            wstring maker, model, cls, ver; BSTR dsc = nullptr; u->get_Description(&dsc); wstring desc = TakeBstr(dsc); DATE ld = 0; u->get_LastDeploymentChangeTime(&ld); ver = DateText(ld); VARIANT_BOOL reboot = VARIANT_FALSE;
            arr.add(js::Obj().str("id", U(UpdId(u))).str("title", U(title)).str("desc", U(desc)).str("date", U(ver)).num("size", bytes).done()); u->Release(); }
        col->Release(); sr->Release(); ses->Release(); return js::Obj().boolean("ok", true).raw("items", arr.done()).done(); };
    h["drivers.install"] = [](const js::V& a) {
        if (!IsElevated()) return Err(L"Installing drivers needs administrator rights. Restart as administrator from Settings."); std::set<string> want; for (auto& s : a.strs("ids")) want.insert(s); if (want.empty()) return Err(L"Nothing selected.");
        IUpdateSession* ses = nullptr; IUpdateSearcher* sr = nullptr; IUpdateCollection* col = nullptr; wstring err; if (!SearchDriverUpdates(&ses, &sr, &col, err)) { if (sr) sr->Release(); if (ses) ses->Release(); return Err(err); }
        IUpdateCollection* pick = nullptr; CoCreateInstance(kClsidUpdateColl, nullptr, CLSCTX_INPROC_SERVER, kIidUpdateCollection, (void**)&pick); LONG n = 0; col->get_Count(&n); std::vector<wstring> titles; if (pick) for (LONG i = 0; i < n; ++i) { IUpdate* u = nullptr; if (FAILED(col->get_Item(i, &u)) || !u) continue; if (want.count(U(UpdId(u)))) { u->AcceptEula(); LONG idx; pick->Add(u, &idx); BSTR t = nullptr; u->get_Title(&t); titles.push_back(TakeBstr(t)); } u->Release(); }
        LONG pc = 0; if (pick) pick->get_Count(&pc); if (!pc) { if (pick) pick->Release(); col->Release(); sr->Release(); ses->Release(); return Err(L"The selected updates are no longer offered. Check again."); }
        Progress(L"Downloading " + std::to_wstring(pc) + L" driver update(s)…"); IUpdateDownloader* dl = nullptr; ses->CreateUpdateDownloader(&dl); IDownloadResult* dr = nullptr; HRESULT hr = E_FAIL; if (dl) { dl->put_Updates(pick); hr = dl->Download(&dr); }
        bool ok = SUCCEEDED(hr); OperationResultCode rc = orcNotStarted; if (dr) { dr->get_ResultCode(&rc); dr->Release(); } if (dl) dl->Release();
        bool reboot = false; int installed = 0;
        if (ok && rc == orcSucceeded) { Progress(L"Installing…"); IUpdateInstaller* in = nullptr; ses->CreateUpdateInstaller((IUpdateInstaller**)&in); IInstallationResult* ir = nullptr; if (in) { in->put_Updates(pick); in->put_AllowSourcePrompts(VARIANT_FALSE); hr = in->Install(&ir); if (SUCCEEDED(hr) && ir) { OperationResultCode irc = orcNotStarted; ir->get_ResultCode(&irc); VARIANT_BOOL rb = VARIANT_FALSE; ir->get_RebootRequired(&rb); reboot = rb != VARIANT_FALSE; ok = irc == orcSucceeded || irc == orcSucceededWithErrors; if (ok) installed = (int)pc; } else ok = false; if (ir) ir->Release(); in->Release(); } else ok = false; }
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
            {"memdiag", L"mdsched.exe", L"", true, L"Windows Memory Diagnostic"}, {"devmgmt", L"devmgmt.msc", L"", false, L"Device Manager"}, {"diskmgmt", L"diskmgmt.msc", L"", false, L"Disk Management"}, {"taskschd", L"taskschd.msc", L"", false, L"Task Scheduler"}, {"services", L"services.msc", L"", false, L"Services"}, {"appwiz", L"appwiz.cpl", L"", false, L"Programs and Features"}, {"wu", L"ms-settings:windowsupdate", L"", false, L"Windows Update"} };
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

static void OnMessage(const string& text) {
    js::V v; if (!js::Parse(text, v) || v.t != js::V::Obj) return; long id = (long)v.num("id", -1); string cmd = v.str("cmd"); auto& hs = Handlers(); auto it = hs.find(cmd);
    if (it == hs.end()) { PostToPage(js::Obj().num("id", (double)id).raw("data", Err(L"Unknown command.")).done()); return; }
    js::V args; if (auto* a = v.get("args")) args = *a; RunAsync(id, it->second, args);
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
    case WM_WV_FAIL: g_failed = true; DestroyWindow(h); return 0;
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
    g_instMutex = instMutex; if (!WebUiAvailable()) return 1;
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
                if (!g_ready) { g_ready = true; if (firstPage > 0) PostMessageW(g_wnd, WM_WV_PAGE, (WPARAM)firstPage, 0); } return S_OK; }), &tk);
            wstring html = FromUtf8(string((const char*)kUiHtml, kUiHtmlSize)); g_wv->NavigateToString(html.c_str()); return S_OK; });
        env->CreateCoreWebView2Controller(g_wnd, onCtl); onCtl->Release(); return S_OK; });
    HRESULT hr = create(nullptr, udf.c_str(), nullptr, onEnv); onEnv->Release();
    if (FAILED(hr)) { DestroyWindow(g_wnd); UnregisterClassW(wc.lpszClassName, inst); return 1; }
    MSG msg; while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    UnregisterClassW(wc.lpszClassName, inst); return g_failed ? 1 : 0;
}
}
