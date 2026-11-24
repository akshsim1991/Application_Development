// Self-test: exercises the read-only parts of the engine on THIS PC and reports what works, how long it took and any Windows error codes.
// Used from Settings and from `FeatherSystemUtilities.exe --selftest`, so problems on a particular machine can be reported precisely.
#include "app.h"
#include "../core/text.h"
#include <shellapi.h>
#include <shlobj.h>
#include <taskschd.h>
#include <cfgmgr32.h>
#include <bcrypt.h>
#include <comdef.h>
#include <fstream>
#include <filesystem>
#include <functional>

namespace fsu {
using std::wstring;
bool WebUiAvailable();

std::vector<TestResult> RunSelfTest(bool network, const std::function<void(const wstring&)>& progress) {
    std::vector<TestResult> out;
    auto check = [&](const wchar_t* name, const std::function<bool(wstring&)>& fn) { if (progress) progress(name); TestResult r; r.name = name; ULONGLONG t0 = GetTickCount64(); try { r.ok = fn(r.detail); } catch (...) { r.ok = false; r.detail = L"exception"; } r.ms = (int)(GetTickCount64() - t0); out.push_back(r); };
    auto N = [](size_t n, const wchar_t* what) { return std::to_wstring(n) + L" " + what; };
    check(L"Environment", [](wstring& d) { d = L"Windows build " + std::to_wstring(WindowsBuild()) + L", " + (IsElevated() ? L"administrator" : L"standard user") + (InSafeMode() ? L", SAFE MODE" : L"") + L", Feather " FSU_VERSION; return WindowsBuild() > 0; });
    check(L"Data folder writable", [](wstring& d) { wstring p = DataDir() + L"\\selftest.tmp"; bool ok = AtomicWrite(p, "x"); DeleteFileW(p.c_str()); d = DataDir(); return ok && JournalWritable(); });
    check(L"WebView2 runtime", [](wstring& d) { bool a = WebUiAvailable(); d = a ? L"available" : L"not found (classic window is used)"; return a; });
    check(L"Startup scan", [&](wstring& d) { auto s = ScanStartup(); d = N(s.entries.size(), L"entries. ") + s.coverage.substr(0, 160); return !s.entries.empty(); });
    check(L"Services scan", [&](wstring& d) { auto s = ScanServices(false); d = N(s.entries.size(), L"services"); return !s.entries.empty(); });
    check(L"Programs scan", [&](wstring& d) { auto s = ScanPrograms(); d = N(s.entries.size(), L"programs"); return !s.entries.empty(); });
    check(L"Cleaner scan", [&](wstring& d) { auto s = ScanCleaner(true); d = N(s.items.size(), L"items, ") + FormatBytes(s.totalBytes); return true; });
    std::vector<DriverRec> drv; check(L"Driver list (WMI)", [&](wstring& d) { wstring e; drv = ListDrivers(e); d = drv.empty() ? e : N(drv.size(), L"drivers"); return !drv.empty(); });
    check(L"Device diagnosis (CfgMgr32)", [&](wstring& d) { for (auto& x : drv) { auto g = DiagnoseDevice(x.devId); if (g.found) { d = x.name + L": code " + std::to_wstring(g.problem) + (g.service.empty() ? L"" : L", service " + g.service); return true; } } d = L"no device found"; return false; });
    check(L"Device problems", [&](wstring& d) { int n = 0; wstring names; for (auto& x : drv) if (x.problem) { ++n; if (names.size() < 200) names += x.name + L" (code " + std::to_wstring(x.problem) + L"); "; } d = n ? names : L"none"; return true; });
    check(L"Computer / BIOS (WMI)", [](wstring& d) { auto c = GetComputerInfo(); auto b = GetBiosInfo(); d = c.maker + L" " + c.model + L" | BIOS " + b.version + L" " + b.date; return !c.maker.empty(); });
    check(L"Windows Update status", [](wstring& d) { auto w = WuLastTimes(); d = L"detect " + w.detect + L", install " + w.install; return true; });
    check(L"Organization info", [](wstring& d) { auto o = GetOrgInfo(); d = wstring(o.domain ? L"domain " : L"") + (o.azureAd ? L"AAD " : L"") + (o.mdm ? L"MDM " : L"") + (o.policies ? L"policies" : L""); if (d.empty()) d = L"unmanaged"; return true; });
    check(L"Event log (System)", [](wstring& d) { wstring e; auto ev = QueryEvents(24, {L"System"}, {1, 2, 3}, 30, e); d = ev.empty() ? (e.empty() ? L"0 events in 24 h" : e) : std::to_wstring(ev.size()) + L" events"; return e.empty(); });
    check(L"Boot diagnostics log", [](wstring& d) { wstring e; auto m = StartupImpact(e); d = e.empty() ? std::to_wstring(m.size()) + L" items recorded" : e; return e.empty(); });
    check(L"Installed Windows updates", [](wstring& d) { wstring e; auto v = ListHotfixes(e); d = v.empty() ? e : std::to_wstring(v.size()) + L" updates"; return !v.empty(); });
    check(L"Disk health (WMI Storage)", [](wstring& d) { auto r = ReadDiskHealth(); d = std::to_wstring(r.disks.size()) + L" disk(s), " + std::to_wstring(r.vols.size()) + L" volume(s)" + (r.error.empty() ? L"" : L" (" + r.error + L")"); return !r.disks.empty(); });
    check(L"Boot history (event 100)", [](wstring& d) { wstring e; auto v = BootHistory(30, e); d = e.empty() ? std::to_wstring(v.size()) + L" boots recorded" : e; return e.empty(); });
    check(L"Security checklist", [](wstring& d) { auto v = SecurityChecklist(); int bad = 0; for (auto& p : v) if (!p.good) ++bad; d = std::to_wstring(v.size()) + L" checks read, " + std::to_wstring(bad) + L" worth reviewing"; return !v.empty(); });
    check(L"Scheduled tasks list", [](wstring& d) { wstring e; auto v = ListAllTasks(e); d = std::to_wstring(v.size()) + L" tasks" + (e.empty() ? L"" : L" (" + e + L")"); return !v.empty(); });
    check(L"Privacy checklist", [](wstring& d) { d = std::to_wstring(PrivacyChecklist().size()) + L" settings read"; return true; });
    check(L"Browser profiles", [](wstring& d) { auto b = ScanBrowsers(); d = std::to_wstring(b.size()) + L" profiles"; return true; });
    check(L"Performance sample", [](wstring& d) { SamplePerf(); Sleep(300); auto p = SamplePerf(); d = L"cpu " + std::to_wstring((int)p.cpu) + L"%, ram " + FormatBytes(p.memTotal); return p.memTotal > 0; });
    check(L"Task Scheduler (COM)", [](wstring& d) { ITaskService* s = nullptr; HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&s); if (FAILED(hr) || !s) { d = L"CoCreateInstance failed"; return false; } VARIANT v; VariantInit(&v); hr = s->Connect(v, v, v, v); s->Release(); wchar_t b[40]; swprintf(b, 40, L"connect 0x%08lX", (unsigned long)hr); d = hr == S_OK ? L"connected" : (wstring(b) + (InSafeMode() ? L" (Task Scheduler is not started in Safe Mode)" : L"")); return hr == S_OK; });
    check(L"Device tree (CfgMgr32)", [](wstring& d) { DEVINST root; CONFIGRET r = CM_Locate_DevNodeW(&root, nullptr, CM_LOCATE_DEVNODE_NORMAL); d = r == CR_SUCCESS ? L"root node found" : L"CM error " + std::to_wstring(r); return r == CR_SUCCESS; });
    check(L"Crypto (CNG)", [](wstring& d) { BCRYPT_ALG_HANDLE h = nullptr, e = nullptr; bool a = BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0, b = BCryptOpenAlgorithmProvider(&e, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) == 0; if (h) BCryptCloseAlgorithmProvider(h, 0); if (e) BCryptCloseAlgorithmProvider(e, 0); d = wstring(a ? L"SHA-256 ok" : L"SHA-256 FAILED") + L", " + (b ? L"ECDSA ok" : L"ECDSA FAILED"); return a && b; });
    check(L"Recycle Bin query", [](wstring& d) { SHQUERYRBINFO q{sizeof q}; HRESULT hr = SHQueryRecycleBinW(nullptr, &q); d = SUCCEEDED(hr) ? std::to_wstring((long long)q.i64NumItems) + L" items" : L"failed"; return SUCCEEDED(hr); });
    check(L"Windows tools present", [](wstring& d) { wchar_t w[MAX_PATH]; GetSystemDirectoryW(w, MAX_PATH); wstring s = w; bool ex = GetFileAttributesW((s + L"\\\\..\\\\explorer.exe").c_str()) != INVALID_FILE_ATTRIBUTES, ps = GetFileAttributesW((s + L"\\\\WindowsPowerShell\\\\v1.0\\\\powershell.exe").c_str()) != INVALID_FILE_ATTRIBUTES, wu = GetFileAttributesW((s + L"\\\\wusa.exe").c_str()) != INVALID_FILE_ATTRIBUTES; d = wstring(L"explorer ") + (ex ? L"yes" : L"NO") + L", powershell " + (ps ? L"yes" : L"no") + L", wusa " + (wu ? L"yes" : L"no"); return ex; });
    check(L"Recommendation + rule lists", [](wstring& d) { auto a = AdviceStatus(); d = L"advice " + a.version + L" (" + a.source + L", " + std::to_wstring(a.startupRules + a.serviceRules) + L" rules), device rules " + DeviceRulesVersion(); return a.startupRules > 0; });
    check(L"License", [](wstring& d) { d = LicenseStatus().state; return true; });
    check(L"Own signature", [](wstring& d) { wchar_t me[MAX_PATH]; GetModuleFileNameW(nullptr, me, MAX_PATH); auto f = InspectFile(me, true); d = f.sig == Sig::Valid ? L"signed by " + f.signer : f.sig == Sig::Unsigned ? L"unsigned build" : L"signature state unknown"; return true; });
    if (network) check(L"Update server reachable", [](wstring& d) { UpdateInfo u; bool ok = FetchLatestVersion(u, Cfg().updateChannel == L"beta"); d = ok ? L"latest " + u.version + L", previous " + u.prevVersion : u.error; return ok; });
    return out;
}
wstring SelfTestReport(const std::vector<TestResult>& r) {
    int bad = 0; for (auto& x : r) if (!x.ok) ++bad; wstring t = L"Feather System Utilities self-test " FSU_VERSION L"\r\n" + NowIsoUtc() + L"\r\n" + std::to_wstring(r.size() - bad) + L" passed, " + std::to_wstring(bad) + L" need attention\r\n\r\n";
    for (auto& x : r) t += (x.ok ? L"PASS  " : L"FAIL  ") + x.name + L"  (" + std::to_wstring(x.ms) + L" ms)  " + x.detail + L"\r\n"; return Redact(t, false);
}
}
