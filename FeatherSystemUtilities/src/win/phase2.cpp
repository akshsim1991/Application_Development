// Phase 2 engine: driver inventory with problem codes, browser data profiles, Windows privacy checklist, event-log queries, redaction.
// Everything here is read-only except ClearBrowserCategory (deletes only paths this file itself discovered inside a browser profile).
#include "app.h"
#include "json.h"
#include "../core/text.h"
#include <wbemidl.h>
#include <winevt.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <set>
#include <map>

namespace fsu {
using std::wstring;

// ---------------- WMI helpers ----------------
static wstring VS(VARIANT& v) {
    if (v.vt == VT_BSTR && v.bstrVal) return v.bstrVal;
    if (v.vt == (VT_ARRAY | VT_BSTR) && v.parray) { LONG lo = 0, hi = -1; SafeArrayGetLBound(v.parray, 1, &lo); SafeArrayGetUBound(v.parray, 1, &hi); wstring r; for (LONG i = lo; i <= hi; ++i) { BSTR b = nullptr; if (SUCCEEDED(SafeArrayGetElement(v.parray, &i, &b)) && b) { if (!r.empty()) r += L";"; r += b; SysFreeString(b); } } return r; }
    return L"";
}
static int VI(VARIANT& v) { return (v.vt == VT_I4 || v.vt == VT_UI4) ? (int)v.lVal : (v.vt == VT_I2 || v.vt == VT_UI2) ? (int)v.iVal : v.vt == VT_BOOL ? (v.boolVal ? 1 : 0) : 0; }
static bool Wmi(const wchar_t* ns, const wchar_t* wql, const std::function<void(IWbemClassObject*)>& row, wstring& err) {
    IWbemLocator* loc = nullptr; IWbemServices* svc = nullptr; IEnumWbemClassObject* en = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&loc)) || !loc) { err = L"WMI is not available."; return false; }
    BSTR n = SysAllocString(ns); HRESULT hr = loc->ConnectServer(n, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &svc); SysFreeString(n); if (FAILED(hr) || !svc) { loc->Release(); err = L"Could not connect to WMI."; return false; }
    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    BSTR ql = SysAllocString(L"WQL"), q = SysAllocString(wql); hr = svc->ExecQuery(ql, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en); SysFreeString(ql); SysFreeString(q);
    bool ok = SUCCEEDED(hr) && en; if (ok) { IWbemClassObject* o = nullptr; ULONG got = 0; while (en->Next(WBEM_INFINITE, 1, &o, &got) == S_OK && got) { row(o); o->Release(); } en->Release(); } else err = L"The WMI query failed.";
    svc->Release(); loc->Release(); return ok;
}
static wstring Get(IWbemClassObject* o, const wchar_t* name) { VARIANT v; VariantInit(&v); o->Get(name, 0, &v, nullptr, nullptr); wstring r = VS(v); VariantClear(&v); return r; }
static int GetI(IWbemClassObject* o, const wchar_t* name) { VARIANT v; VariantInit(&v); o->Get(name, 0, &v, nullptr, nullptr); int r = VI(v); VariantClear(&v); return r; }

const wchar_t* ProblemText(int code) {
    switch (code) { case 0: return L"Working properly";
    case 1: return L"Not configured correctly"; case 3: return L"Driver may be corrupted, or the system is low on memory"; case 10: return L"Device cannot start"; case 12: return L"Not enough free resources";
    case 14: return L"Needs a restart to work"; case 18: return L"Drivers need to be reinstalled"; case 19: return L"Registry configuration problem"; case 21: return L"Windows is removing the device"; case 22: return L"Device is disabled";
    case 24: return L"Device is not present or not working"; case 28: return L"No driver is installed"; case 31: return L"Windows cannot load the driver"; case 32: return L"Driver (service) is disabled"; case 33: return L"Windows cannot determine the resources";
    case 34: return L"Needs manual configuration"; case 35: return L"Firmware does not provide required resources"; case 37: return L"Driver failed to initialize"; case 38: return L"Previous driver instance is still in memory"; case 39: return L"Driver is missing or corrupted";
    case 40: return L"Driver service key is missing"; case 41: return L"Driver loaded but the device was not found"; case 43: return L"Windows stopped the device because it reported a problem"; case 44: return L"An application or service shut the device down";
    case 45: return L"Device is not connected"; case 48: return L"Driver blocked from starting (known problems)"; case 52: return L"Driver is not digitally signed"; default: return L"Device reports a problem"; }
}
std::vector<DriverRec> ListDrivers(wstring& err) {
    std::vector<DriverRec> out; std::map<wstring, size_t> byId; int idx = 0;
    if (!Wmi(L"ROOT\\CIMV2", L"SELECT DeviceName,Manufacturer,DriverVersion,DriverDate,DriverProviderName,InfName,IsSigned,DeviceClass,DeviceID,HardWareID FROM Win32_PnPSignedDriver", [&](IWbemClassObject* o) {
        DriverRec d; d.name = Get(o, L"DeviceName"); if (d.name.empty()) return; d.maker = Get(o, L"Manufacturer"); d.version = Get(o, L"DriverVersion"); d.date = Get(o, L"DriverDate"); if (d.date.size() >= 8) d.date = d.date.substr(0, 4) + L"-" + d.date.substr(4, 2) + L"-" + d.date.substr(6, 2);
        d.provider = Get(o, L"DriverProviderName"); d.inf = Get(o, L"InfName"); d.signed_ = GetI(o, L"IsSigned") != 0; d.cls = Get(o, L"DeviceClass"); d.devId = Get(o, L"DeviceID"); d.hwid = Get(o, L"HardWareID"); d.i = idx++; byId[d.devId] = out.size(); out.push_back(d); }, err)) return out;
    wstring e2; Wmi(L"ROOT\\CIMV2", L"SELECT Name,PNPDeviceID,ConfigManagerErrorCode,Status,PNPClass,Manufacturer,HardwareID FROM Win32_PnPEntity", [&](IWbemClassObject* o) {
        wstring id = Get(o, L"PNPDeviceID"); int code = GetI(o, L"ConfigManagerErrorCode"); wstring status = Get(o, L"Status"); auto it = byId.find(id);
        if (it != byId.end()) { out[it->second].problem = code; out[it->second].status = status; return; }
        if (code != 0) { DriverRec d; d.name = Get(o, L"Name"); if (d.name.empty()) d.name = id; d.maker = Get(o, L"Manufacturer"); d.cls = Get(o, L"PNPClass"); d.devId = id; d.hwid = Get(o, L"HardwareID"); d.problem = code; d.status = status; d.missing = true; d.signed_ = true; d.i = idx++; out.push_back(d); } }, e2);
    return out;
}
ComputerInfo GetComputerInfo() {
    ComputerInfo c; wstring e; Wmi(L"ROOT\\CIMV2", L"SELECT Manufacturer,Model FROM Win32_ComputerSystem", [&](IWbemClassObject* o) { c.maker = Get(o, L"Manufacturer"); c.model = Get(o, L"Model"); }, e);
    wstring l = c.maker; for (auto& ch : l) ch = (wchar_t)towlower(ch);
    struct M { const wchar_t* key; const wchar_t* url; }; static const M m[] = {{L"dell", L"https://www.dell.com/support/home"}, {L"hewlett", L"https://support.hp.com/drivers"}, {L"hp", L"https://support.hp.com/drivers"}, {L"lenovo", L"https://support.lenovo.com/"}, {L"asus", L"https://www.asus.com/support/"}, {L"acer", L"https://www.acer.com/support"}, {L"micro-star", L"https://www.msi.com/support"}, {L"msi", L"https://www.msi.com/support"}, {L"microsoft", L"https://support.microsoft.com/surface"}, {L"samsung", L"https://www.samsung.com/support/"}, {L"gigabyte", L"https://www.gigabyte.com/Support"}};
    for (auto& x : m) if (l.find(x.key) != wstring::npos) { c.supportUrl = x.url; break; } return c;
}

// ---------------- browsers ----------------
static ULONGLONG SizeAndCount(const wstring& p, ULONGLONG& files, int depth = 0) {
    WIN32_FILE_ATTRIBUTE_DATA fa; if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa)) return 0; if (fa.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return 0;
    if (!(fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { ++files; return ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; } if (depth > 30) return 0;
    ULONGLONG t = 0; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((p + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH); if (h == INVALID_HANDLE_VALUE) return 0;
    do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; t += SizeAndCount(p + L"\\" + n, files, depth + 1); } while (FindNextFileW(h, &fd)); FindClose(h); return t;
}
static wstring Env(const wchar_t* n) { wchar_t b[MAX_PATH * 2]; DWORD k = GetEnvironmentVariableW(n, b, MAX_PATH * 2); return k && k < MAX_PATH * 2 ? wstring(b) : wstring(); }
static bool IsDir(const wstring& p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) && !(a & FILE_ATTRIBUTE_REPARSE_POINT); }
static void AddCat(BrowserProfile& bp, const wstring& id, const wstring& label, const wstring& note, const std::vector<wstring>& rel, const wstring& base, bool deletable = true) {
    BrowserCat c; c.id = bp.key + L"|" + bp.profile + L"|" + id; c.label = label; c.note = note; c.deletable = deletable;
    for (auto& r : rel) { wstring p = base + L"\\" + r; if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) { ULONGLONG f = 0; c.bytes += SizeAndCount(p, f); c.files += f; c.paths.push_back(p); } }
    c.base = base; bp.cats.push_back(c);
}
static wstring ProfileName(const wstring& dir, const wstring& fallback) {
    std::ifstream f(std::filesystem::path(dir + L"\\Preferences"), std::ios::binary); if (!f) return fallback; std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); if (s.size() > 4000000) return fallback;
    js::V v; if (!js::Parse(s, v)) return fallback; if (auto* p = v.get("profile")) { std::string n = p->str("name"); if (!n.empty()) return FromUtf8(n); } return fallback;
}
std::vector<BrowserProfile> ScanBrowsers() {
    std::vector<BrowserProfile> out; wstring la = Env(L"LOCALAPPDATA"), ra = Env(L"APPDATA");
    struct B { const wchar_t* key; const wchar_t* name; wstring root; }; B chromium[] = {{L"chrome", L"Google Chrome", la + L"\\Google\\Chrome\\User Data"}, {L"edge", L"Microsoft Edge", la + L"\\Microsoft\\Edge\\User Data"}, {L"brave", L"Brave", la + L"\\BraveSoftware\\Brave-Browser\\User Data"}};
    for (auto& b : chromium) { if (!IsDir(b.root)) continue; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((b.root + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) continue;
        do { wstring n = fd.cFileName; if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue; if (n != L"Default" && n.rfind(L"Profile ", 0) != 0) continue;
            wstring dir = b.root + L"\\" + n; if (GetFileAttributesW((dir + L"\\Preferences").c_str()) == INVALID_FILE_ATTRIBUTES) continue;
            BrowserProfile bp; bp.key = b.key; bp.browser = b.name; bp.profile = n; bp.display = ProfileName(dir, n); bp.dir = dir;
            AddCat(bp, L"cache", L"Cache", L"Saved copies of web pages and images. Pages may load a little slower the first time afterwards.", {L"Cache", L"Code Cache", L"GPUCache", L"Service Worker\\CacheStorage", L"Service Worker\\ScriptCache"}, dir);
            AddCat(bp, L"cookies", L"Cookies", L"You will be signed out of websites and some site preferences are forgotten.", {L"Network\\Cookies", L"Network\\Cookies-journal", L"Cookies", L"Cookies-journal"}, dir);
            AddCat(bp, L"history", L"Browsing history and download history", L"Removes the list of visited pages and downloads (one file holds both). Bookmarks, passwords and autofill are NOT touched.", {L"History", L"History-journal", L"Visited Links", L"Top Sites", L"Top Sites-journal", L"Shortcuts", L"Shortcuts-journal"}, dir);
            AddCat(bp, L"sitedata", L"Saved site data", L"Local storage and offline data of websites. Some sites may forget settings or unsent drafts.", {L"Local Storage", L"IndexedDB", L"Session Storage", L"File System"}, dir);
            out.push_back(bp); } while (FindNextFileW(h, &fd)); FindClose(h); }
    wstring ff = ra + L"\\Mozilla\\Firefox\\Profiles", ffl = la + L"\\Mozilla\\Firefox\\Profiles";
    if (IsDir(ff)) { WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((ff + L"\\*").c_str(), &fd); if (h != INVALID_HANDLE_VALUE) { do { wstring n = fd.cFileName; if (n == L"." || n == L".." || !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        wstring dir = ff + L"\\" + n; BrowserProfile bp; bp.key = L"firefox"; bp.browser = L"Mozilla Firefox"; bp.profile = n; bp.display = n.find(L'.') != wstring::npos ? n.substr(n.find(L'.') + 1) : n; bp.dir = dir;
        AddCat(bp, L"cache", L"Cache", L"Saved copies of web pages and images.", {L"cache2", L"startupCache"}, ffl + L"\\" + n);
        AddCat(bp, L"cookies", L"Cookies", L"You will be signed out of websites.", {L"cookies.sqlite", L"cookies.sqlite-wal", L"cookies.sqlite-shm"}, dir);
        AddCat(bp, L"sitedata", L"Saved site data", L"Offline data of websites.", {L"storage\\default", L"webappsstore.sqlite"}, dir);
        AddCat(bp, L"history", L"Browsing history", L"Firefox keeps history and bookmarks in one file, so Feather cannot clear history without risking your bookmarks. Use Firefox's own Clear History (Open browser controls).", {}, dir, false);
        out.push_back(bp); } while (FindNextFileW(h, &fd)); FindClose(h); } }
    return out;
}
bool BrowserRunning(const wstring& key) { return IsProcessRunning(key == L"chrome" ? L"chrome.exe" : key == L"edge" ? L"msedge.exe" : key == L"brave" ? L"brave.exe" : L"firefox.exe"); }
static bool DelTree(const wstring& p, int depth, ULONGLONG& bytes, int& failed) {
    DWORD a = GetFileAttributesW(p.c_str()); if (a == INVALID_FILE_ATTRIBUTES) return true; if (a & FILE_ATTRIBUTE_REPARSE_POINT) { ++failed; return false; }
    if (!(a & FILE_ATTRIBUTE_DIRECTORY)) { WIN32_FILE_ATTRIBUTE_DATA fa; GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fa); SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); if (DeleteFileW(p.c_str())) { bytes += ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow; return true; } ++failed; return false; }
    if (depth > 30) { ++failed; return false; } std::vector<wstring> kids; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((p + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h != INVALID_HANDLE_VALUE) { do { wstring n = fd.cFileName; if (n != L"." && n != L"..") kids.push_back(p + L"\\" + n); } while (FindNextFileW(h, &fd)); FindClose(h); }
    bool ok = true; for (auto& k : kids) if (!DelTree(k, depth + 1, bytes, failed)) ok = false; SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); if (!RemoveDirectoryW(p.c_str())) ok = false; return ok;
}
bool ClearBrowserCategory(const BrowserProfile& bp, const BrowserCat& c, ULONGLONG& bytes, wstring& err) {
    if (!c.deletable) { err = L"This category cannot be cleared safely."; return false; } if (BrowserRunning(bp.key)) { err = bp.browser + L" is running. Close it completely first."; return false; }
    int failed = 0; for (auto& p : c.paths) { if (_wcsnicmp(p.c_str(), c.base.c_str(), c.base.size()) != 0 || p.size() <= c.base.size() + 1) { err = L"Unexpected path; nothing deleted."; return false; } DelTree(p, 0, bytes, failed); }
    if (failed) { err = std::to_wstring(failed) + L" file(s) could not be deleted (in use)."; return false; } return true;
}

// ---------------- Windows privacy checklist ----------------
static bool RegDw(HKEY root, const wchar_t* key, const wchar_t* val, DWORD& out) { HKEY k; if (RegOpenKeyExW(root, key, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false; DWORD n = sizeof out, t = 0; bool ok = RegQueryValueExW(k, val, nullptr, &t, (BYTE*)&out, &n) == ERROR_SUCCESS && t == REG_DWORD; RegCloseKey(k); return ok; }
static bool RegStr(HKEY root, const wchar_t* key, const wchar_t* val, wstring& out) { HKEY k; if (RegOpenKeyExW(root, key, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false; wchar_t b[128]{}; DWORD n = sizeof b - 2; bool ok = RegQueryValueExW(k, val, nullptr, nullptr, (BYTE*)b, &n) == ERROR_SUCCESS; if (ok) out = b; RegCloseKey(k); return ok; }
std::vector<PrivacyItem> PrivacyChecklist() {
    std::vector<PrivacyItem> v; DWORD d = 0; wstring s;
    auto add = [&](const wchar_t* id, const wchar_t* title, const wchar_t* state, const wchar_t* detail, const wchar_t* why, bool good) { PrivacyItem p; p.id = id; p.title = title; p.state = state; p.detail = detail; p.why = why; p.good = good; v.push_back(p); };
    if (RegDw(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", d)) add(L"general", L"Advertising ID", d ? L"On" : L"Off", d ? L"Apps can use an ID to show more relevant ads." : L"Apps cannot use an advertising ID.", L"Turning it off stops apps from tracking you across apps for ads.", d == 0); else add(L"general", L"Advertising ID", L"Unknown", L"Not available on this version of Windows.", L"", true);
    if (RegDw(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry", d) || RegDw(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"AllowTelemetry", d)) add(L"feedback", L"Diagnostic data (set by policy)", d == 0 ? L"Minimal" : d == 1 ? L"Basic" : d == 2 ? L"Enhanced" : L"Full", L"A policy sets how much diagnostic data Windows sends to Microsoft.", L"Lower levels send less. Some editions cannot go below Basic.", d <= 1); else add(L"feedback", L"Diagnostic data", L"Windows default", L"No policy is set; the level is chosen in Windows Settings.", L"Review the level in Settings > Privacy > Diagnostics & feedback.", true);
    if (RegDw(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy", L"TailoredExperiencesWithDiagnosticDataEnabled", d)) add(L"feedback", L"Tailored experiences", d ? L"On" : L"Off", L"Lets Windows use diagnostic data to suggest tips and ads.", L"Off keeps suggestions generic.", d == 0);
    if (RegDw(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", L"PublishUserActivities", d)) add(L"activityhistory", L"Activity history", d ? L"On" : L"Off (policy)", L"Controls whether Windows records the apps and files you open.", L"Off means no timeline of your activity is kept.", d == 0); else add(L"activityhistory", L"Activity history", L"Windows default", L"No policy is set; see Settings > Privacy > Activity history.", L"", true);
    if (RegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\location", L"Value", s)) add(L"location", L"Location access", s == L"Deny" ? L"Off" : L"On", L"Whether apps may ask for this PC's location.", L"Off blocks location for all apps.", s == L"Deny");
    if (RegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\webcam", L"Value", s)) add(L"webcam", L"Camera access", s == L"Deny" ? L"Off" : L"On", L"Whether apps may use the camera.", L"Review which apps are allowed in Settings.", s == L"Deny");
    if (RegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone", L"Value", s)) add(L"microphone", L"Microphone access", s == L"Deny" ? L"Off" : L"On", L"Whether apps may use the microphone.", L"Review which apps are allowed in Settings.", s == L"Deny");
    if (RegDw(HKEY_CURRENT_USER, L"Software\\Microsoft\\Clipboard", L"EnableClipboardHistory", d)) add(L"clipboard", L"Clipboard history", d ? L"On" : L"Off", L"Keeps recent copied items so you can paste them again.", L"Off prevents copied passwords from lingering in history.", d == 0); else add(L"clipboard", L"Clipboard history", L"Off (default)", L"Not enabled.", L"", true);
    if (RegDw(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager", L"SubscribedContent-338388Enabled", d)) add(L"personalization-start", L"Suggestions in Start", d ? L"On" : L"Off", L"Shows suggested apps in the Start menu.", L"", d == 0);
    return v;
}
bool OpenPrivacySetting(const wstring& id) {
    static const wchar_t* ok[] = {L"general", L"feedback", L"activityhistory", L"location", L"webcam", L"microphone"}; wstring uri;
    if (id == L"clipboard") uri = L"ms-settings:clipboard"; else if (id == L"personalization-start") uri = L"ms-settings:personalization-start"; else { bool f = false; for (auto* o : ok) if (id == o) f = true; if (!f) return false; uri = L"ms-settings:privacy-" + id; }
    return (INT_PTR)ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}
bool OpenBrowserPrivacy(const wstring& key, wstring& err) {
    const wchar_t* exe = key == L"chrome" ? L"chrome.exe" : key == L"edge" ? L"msedge.exe" : key == L"brave" ? L"brave.exe" : key == L"firefox" ? L"firefox.exe" : nullptr; const wchar_t* url = key == L"chrome" ? L"chrome://settings/clearBrowserData" : key == L"edge" ? L"edge://settings/clearBrowserData" : key == L"brave" ? L"brave://settings/clearBrowserData" : L"about:preferences#privacy";
    if (!exe) { err = L"Unknown browser."; return false; } wstring path; wstring k = wstring(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") + exe;
    if (!RegStr(HKEY_LOCAL_MACHINE, k.c_str(), L"", path) && !RegStr(HKEY_CURRENT_USER, k.c_str(), L"", path)) { err = L"Could not find the browser's program."; return false; }
    if (!path.empty() && path.front() == L'"') { path.erase(0, 1); size_t q = path.find(L'"'); if (q != wstring::npos) path.erase(q); }
    if ((INT_PTR)ShellExecuteW(nullptr, L"open", path.c_str(), url, nullptr, SW_SHOWNORMAL) <= 32) { err = L"The browser could not be started."; return false; } return true;
}

// ---------------- event logs ----------------
std::vector<EventRec> QueryEvents(int hours, const std::vector<wstring>& logs, const std::vector<int>& levels, size_t maxN, wstring& err) {
    std::vector<EventRec> out; if (levels.empty()) { err = L"Choose at least one level."; return out; }
    wstring lv; for (size_t i = 0; i < levels.size(); ++i) lv += (i ? L" or " : L"") + wstring(L"Level=") + std::to_wstring(levels[i]);
    wstring q = L"*[System[(" + lv + L") and TimeCreated[timediff(@SystemTime) <= " + std::to_wstring((ULONGLONG)hours * 3600000ULL) + L"]]]";
    EVT_HANDLE ctx = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem); std::map<wstring, EVT_HANDLE> metas;
    for (auto& log : logs) { if (log != L"System" && log != L"Application") continue; EVT_HANDLE h = EvtQuery(nullptr, log.c_str(), q.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection); if (!h) { err = L"The " + log + L" log could not be read (" + Win32Err(GetLastError()) + L"). Administrator rights may be needed."; continue; }
        EVT_HANDLE evs[32]; DWORD got = 0; size_t perLog = 0;
        while (perLog < maxN && EvtNext(h, 32, evs, 1000, 0, &got)) { for (DWORD i = 0; i < got; ++i) { if (perLog < maxN) { EventRec r; r.log = log; DWORD need = 0, cnt = 0; EvtRender(ctx, evs[i], EvtRenderEventValues, 0, nullptr, &need, &cnt);
                if (need && need < 100000) { std::vector<BYTE> buf(need); if (EvtRender(ctx, evs[i], EvtRenderEventValues, need, buf.data(), &need, &cnt)) { auto* v = (PEVT_VARIANT)buf.data();
                    if (v[EvtSystemProviderName].Type == EvtVarTypeString && v[EvtSystemProviderName].StringVal) r.provider = v[EvtSystemProviderName].StringVal; if (v[EvtSystemEventID].Type == EvtVarTypeUInt16) r.id = v[EvtSystemEventID].UInt16Val; if (v[EvtSystemLevel].Type == EvtVarTypeByte) r.level = v[EvtSystemLevel].ByteVal;
                    if (v[EvtSystemTimeCreated].Type == EvtVarTypeFileTime) { ULONGLONG ft = v[EvtSystemTimeCreated].FileTimeVal; r.time = FileDateText(ft - 0); r.sort = ft; } } }
                EVT_HANDLE& m = metas[r.provider]; if (!m && !r.provider.empty()) m = EvtOpenPublisherMetadata(nullptr, r.provider.c_str(), nullptr, 0, 0);
                if (m) { DWORD n2 = 0; EvtFormatMessage(m, evs[i], 0, 0, nullptr, EvtFormatMessageEvent, 0, nullptr, &n2); if (n2 && n2 < 60000) { std::vector<wchar_t> t(n2 + 2); if (EvtFormatMessage(m, evs[i], 0, 0, nullptr, EvtFormatMessageEvent, n2, t.data(), &n2)) r.message = t.data(); } }
                if (r.message.size() > 700) r.message = r.message.substr(0, 700) + L"…"; out.push_back(r); ++perLog; } EvtClose(evs[i]); } }
        EvtClose(h); }
    for (auto& kv : metas) if (kv.second) EvtClose(kv.second); if (ctx) EvtClose(ctx);
    std::sort(out.begin(), out.end(), [](const EventRec& a, const EventRec& b) { return a.sort > b.sort; }); if (out.size() > maxN) out.resize(maxN); return out;
}

// ---------------- redaction ----------------
static void ReplaceAllNoCase(wstring& s, const wstring& what, const wstring& with) {
    if (what.size() < 2) return; wstring ls = s, lw = what; for (auto& c : ls) c = (wchar_t)towlower(c); for (auto& c : lw) c = (wchar_t)towlower(c);
    wstring out; size_t pos = 0, last = 0; while ((pos = ls.find(lw, pos)) != wstring::npos) { out.append(s, last, pos - last); out += with; pos += what.size(); last = pos; } if (!last) return; out.append(s, last, wstring::npos); s = out;
}
wstring Redact(wstring s, bool maskPaths) {
    wchar_t user[256] = {}; DWORD n = 256; GetUserNameW(user, &n); wchar_t comp[MAX_COMPUTERNAME_LENGTH + 1] = {}; DWORD cn = MAX_COMPUTERNAME_LENGTH + 1; GetComputerNameW(comp, &cn); wstring prof = Env(L"USERPROFILE");
    if (!prof.empty()) ReplaceAllNoCase(s, prof, L"C:\\Users\\<user>"); ReplaceAllNoCase(s, user, L"<user>"); ReplaceAllNoCase(s, comp, L"<computer>");
    for (size_t p = 0; (p = s.find(L"S-1-5-21-", p)) != wstring::npos;) { size_t e = p + 9; while (e < s.size() && (iswdigit(s[e]) || s[e] == L'-')) ++e; s.replace(p, e - p, L"<sid>"); p += 5; }
    if (maskPaths) { wstring o; size_t i = 0; while (i < s.size()) { if (i + 2 < s.size() && iswalpha(s[i]) && s[i + 1] == L':' && s[i + 2] == L'\\' && (i == 0 || !iswalnum(s[i - 1]))) { size_t e = i; while (e < s.size() && !wcschr(L" \t\r\n\"'<>|,;)]", s[e])) ++e; wstring path = s.substr(i, e - i); size_t sl = path.find_last_of(L'\\'); o += L"<path>\\" + (sl == wstring::npos ? path : path.substr(sl + 1)); i = e; } else o += s[i++]; } s = o; }
    return s;
}
}

namespace fsu {
std::vector<Hotfix> ListHotfixes(std::wstring& err) {
    std::vector<Hotfix> out; Wmi(L"ROOT\\CIMV2", L"SELECT HotFixID,Description,InstalledOn,InstalledBy FROM Win32_QuickFixEngineering", [&](IWbemClassObject* o) { Hotfix h; h.kb = Get(o, L"HotFixID"); if (h.kb.empty()) return; h.desc = Get(o, L"Description"); h.date = Get(o, L"InstalledOn"); h.by = Get(o, L"InstalledBy"); out.push_back(h); }, err); return out;
}
}

namespace fsu {
BiosInfo GetBiosInfo() { BiosInfo b; wstring e; Wmi(L"ROOT\\CIMV2", L"SELECT Manufacturer,SMBIOSBIOSVersion,ReleaseDate FROM Win32_BIOS", [&](IWbemClassObject* o) { b.vendor = Get(o, L"Manufacturer"); b.version = Get(o, L"SMBIOSBIOSVersion"); b.date = Get(o, L"ReleaseDate"); if (b.date.size() >= 8) b.date = b.date.substr(0, 4) + L"-" + b.date.substr(4, 2) + L"-" + b.date.substr(6, 2); }, e); return b; }
}

namespace fsu {
// ---------------- security posture (read-only) ----------------
static bool WmiBool(const wchar_t* ns, const wchar_t* q, const wchar_t* prop, int& out) { int v = -1; wstring e; bool ok = Wmi(ns, q, [&](IWbemClassObject* o) { if (v < 0) { VARIANT x; VariantInit(&x); o->Get(prop, 0, &x, nullptr, nullptr); v = x.vt == VT_NULL || x.vt == VT_EMPTY ? -1 : VI(x); VariantClear(&x); } }, e); out = v; return ok && v >= 0; }
std::vector<PrivacyItem> SecurityChecklist() {
    std::vector<PrivacyItem> v; DWORD d = 0; int w = 0; bool admin = IsElevated();
    auto add = [&](const wchar_t* id, const wchar_t* title, const wchar_t* state, const wchar_t* detail, const wchar_t* why, bool good) { PrivacyItem p; p.id = id; p.title = title; p.state = state; p.detail = detail; p.why = why; p.good = good; v.push_back(p); };
    // Microsoft Defender
    { int av = -1, rt = -1, age = -1; bool ok = WmiBool(L"root\\Microsoft\\Windows\\Defender", L"SELECT AntivirusEnabled FROM MSFT_MpComputerStatus", L"AntivirusEnabled", av);
      if (ok) { WmiBool(L"root\\Microsoft\\Windows\\Defender", L"SELECT RealTimeProtectionEnabled FROM MSFT_MpComputerStatus", L"RealTimeProtectionEnabled", rt); WmiBool(L"root\\Microsoft\\Windows\\Defender", L"SELECT AntivirusSignatureAge FROM MSFT_MpComputerStatus", L"AntivirusSignatureAge", age);
        wchar_t b[160]; swprintf(b, 160, L"Real-time protection %ls; virus definitions %d day(s) old.", rt == 1 ? L"on" : L"off", age < 0 ? 0 : age);
        bool good = av == 1 && rt == 1 && (age < 0 || age <= 7); add(L"sec-defender", L"Microsoft Defender antivirus", av != 1 ? L"Off" : (rt != 1 ? L"Real-time off" : (age > 7 ? L"Out of date" : L"On")), b, L"Real-time protection and fresh definitions are the main defence against malware. If you use another antivirus, Defender normally turns itself off.", good); }
      else add(L"sec-defender", L"Microsoft Defender antivirus", L"Unknown", L"Windows did not report Defender status. Another antivirus may be managing protection, or this Windows version has no Defender service.", L"Open Windows Security to confirm that some antivirus is active.", true); }
    // Firewall (three profiles)
    { const wchar_t* prof[] = {L"DomainProfile", L"StandardProfile", L"PublicProfile"}; const wchar_t* nm[] = {L"Domain", L"Private", L"Public"}; int off = 0, seen = 0; wstring offList;
      for (int i = 0; i < 3; ++i) { wstring k = wstring(L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\") + prof[i]; if (RegDw(HKEY_LOCAL_MACHINE, k.c_str(), L"EnableFirewall", d)) { ++seen; if (!d) { ++off; offList += (offList.empty() ? L"" : L", ") + wstring(nm[i]); } } }
      if (seen) add(L"sec-firewall", L"Windows Firewall", off ? L"Off for some networks" : L"On", off ? (L"Turned off for: " + offList + L".").c_str() : L"On for domain, private and public networks.", L"The firewall blocks unsolicited incoming connections. It should stay on unless another firewall replaces it.", off == 0);
      else add(L"sec-firewall", L"Windows Firewall", L"Unknown", L"The firewall settings could not be read.", L"Open the firewall page to check.", true); }
    // UAC
    if (RegDw(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA", d)) { DWORD pr = 5; RegDw(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"ConsentPromptBehaviorAdmin", pr);
        add(L"sec-uac", L"User Account Control", d ? (pr == 0 ? L"On (never notify)" : L"On") : L"Off", d ? (pr == 0 ? L"UAC is enabled but administrators are elevated without a prompt." : L"Programs must ask before making system-wide changes.") : L"Programs can change the system without asking.", L"UAC is the prompt that stops unwanted programs from changing Windows.", d && pr != 0); }
    // BitLocker (system drive)
    { int ps = -1; wchar_t sd[4] = L"C:"; wchar_t win[MAX_PATH]; if (GetWindowsDirectoryW(win, MAX_PATH) >= 2) { sd[0] = win[0]; sd[1] = L':'; sd[2] = 0; } wstring q = wstring(L"SELECT ProtectionStatus FROM Win32_EncryptableVolume WHERE DriveLetter='") + sd + L"'";
      if (WmiBool(L"root\\CIMV2\\Security\\MicrosoftVolumeEncryption", q.c_str(), L"ProtectionStatus", ps)) add(L"sec-bitlocker", L"Drive encryption (BitLocker)", ps == 1 ? L"On" : L"Off", ps == 1 ? L"The system drive is encrypted and protected." : L"The system drive is not protected by BitLocker.", L"Encryption keeps your files unreadable if the PC is lost or stolen. It is optional on a desktop that never leaves home.", ps == 1);
      else add(L"sec-bitlocker", L"Drive encryption (BitLocker)", L"Unknown", admin ? L"BitLocker is not available on this edition of Windows." : L"Reading BitLocker needs administrator rights (More > Restart as administrator), or this edition of Windows has no BitLocker.", L"Windows Home uses 'device encryption' instead; open the page to check.", true); }
    // Secure Boot
    if (RegDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", L"UEFISecureBootEnabled", d)) add(L"sec-secureboot", L"Secure Boot", d ? L"On" : L"Off", d ? L"Windows only starts with trusted boot software." : L"Secure Boot is supported but turned off in the firmware.", L"Secure Boot blocks boot-time malware. Windows 11 requires it.", d != 0);
    else add(L"sec-secureboot", L"Secure Boot", L"Not available", L"This PC starts in legacy BIOS mode or the firmware does not support Secure Boot.", L"Windows 11 needs UEFI with Secure Boot. Windows 10 and older run without it.", true);
    // TPM
    { int en = -1; wstring ver; wstring e; Wmi(L"root\\CIMV2\\Security\\MicrosoftTpm", L"SELECT IsEnabled_InitialValue, SpecVersion FROM Win32_Tpm", [&](IWbemClassObject* o) { VARIANT x; VariantInit(&x); o->Get(L"IsEnabled_InitialValue", 0, &x, nullptr, nullptr); if (x.vt == VT_BOOL) en = x.boolVal ? 1 : 0; VariantClear(&x); ver = Get(o, L"SpecVersion"); size_t c = ver.find(L','); if (c != wstring::npos) ver.erase(c); }, e);
      if (en >= 0) add(L"sec-tpm", L"TPM security chip", en ? L"Present and on" : L"Present but off", en ? (L"TPM specification " + ver + L".").c_str() : L"The TPM is turned off in the firmware.", L"A TPM stores encryption keys safely. Windows 11 requires TPM 2.0.", en != 0);
      else add(L"sec-tpm", L"TPM security chip", L"Unknown", admin ? L"No TPM was reported by Windows." : L"Reading the TPM needs administrator rights, or this PC has no TPM.", L"Windows 11 requires TPM 2.0. Open the TPM console to check.", true); }
    // SMBv1
    { DWORD sm = 99; bool has = RegDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\LanmanServer\\Parameters", L"SMB1", sm); DWORD st = 4; bool drv = RegDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\mrxsmb10", L"Start", st);
      bool off = (has && sm == 0) || (drv && st == 4) || (!has && WindowsBuild() >= 16299);
      add(L"sec-smb1", L"SMBv1 file sharing (old protocol)", off ? L"Off" : L"On", off ? L"The insecure SMBv1 protocol is not in use." : L"The old SMBv1 protocol is enabled.", L"SMBv1 was used by the WannaCry ransomware. Disable it unless an old NAS or printer needs it.", off); }
    // Remote Desktop
    if (RegDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Terminal Server", L"fDenyTSConnections", d)) add(L"sec-rdp", L"Remote Desktop", d ? L"Off" : L"On", d ? L"Nobody can connect to this PC remotely." : L"This PC accepts Remote Desktop connections.", L"Leave it off unless you use it. An exposed Remote Desktop port is a common way into a PC.", d != 0);
    // Pending reboot / updates
    { bool pend = false; HKEY k; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { pend = true; RegCloseKey(k); }
      if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { pend = true; RegCloseKey(k); }
      add(L"sec-updates", L"Windows updates", pend ? L"Restart needed" : L"No restart pending", pend ? L"An update is waiting for a restart to finish installing." : L"No update is waiting for a restart. Open Windows Update to see whether more are available.", L"Updates fix security holes. A pending restart leaves the old, vulnerable files in use.", !pend); }
    (void)w; return v;
}
bool OpenSecuritySetting(const wstring& id) {
    struct M { const wchar_t* id; const wchar_t* file; const wchar_t* args; }; static const M m[] = {
        {L"sec-defender", L"windowsdefender:", L""}, {L"sec-firewall", L"firewall.cpl", L""}, {L"sec-uac", L"UserAccountControlSettings.exe", L""}, {L"sec-bitlocker", L"control.exe", L"/name Microsoft.BitLockerDriveEncryption"},
        {L"sec-secureboot", L"msinfo32.exe", L""}, {L"sec-tpm", L"tpm.msc", L""}, {L"sec-smb1", L"optionalfeatures.exe", L""}, {L"sec-rdp", L"SystemPropertiesRemote.exe", L""}, {L"sec-updates", L"ms-settings:windowsupdate", L""} };
    for (auto& e : m) if (id == e.id) return (INT_PTR)ShellExecuteW(nullptr, L"open", e.file, *e.args ? e.args : nullptr, nullptr, SW_SHOWNORMAL) > 32; return false;
}

// ---------------- disk health and boot history (read-only) ----------------
static double WNum(IWbemClassObject* o, const wchar_t* name) { VARIANT v; VariantInit(&v); double r = -1; if (SUCCEEDED(o->Get(name, 0, &v, nullptr, nullptr))) { switch (v.vt) { case VT_UI1: r = v.bVal; break; case VT_I2: r = v.iVal; break; case VT_UI2: r = v.uiVal; break; case VT_I4: r = v.lVal; break; case VT_UI4: r = v.ulVal; break; case VT_R4: r = v.fltVal; break; case VT_R8: r = v.dblVal; break; case VT_BSTR: if (v.bstrVal) r = (double)_wtoi64(v.bstrVal); break; case VT_BOOL: r = v.boolVal ? 1 : 0; break; default: break; } } VariantClear(&v); return r; }
DiskReport ReadDiskHealth() {
    DiskReport rep; rep.admin = IsElevated(); wstring e;
    std::map<wstring, DiskInfo> by; std::vector<wstring> order;
    Wmi(L"root\\Microsoft\\Windows\\Storage", L"SELECT DeviceId, FriendlyName, MediaType, BusType, HealthStatus, Size FROM MSFT_PhysicalDisk", [&](IWbemClassObject* o) {
        DiskInfo d; wstring id = Get(o, L"DeviceId"); d.name = Get(o, L"FriendlyName"); int mt = (int)WNum(o, L"MediaType"), bt = (int)WNum(o, L"BusType"), hs = (int)WNum(o, L"HealthStatus");
        d.media = mt == 3 ? L"HDD" : mt == 4 ? L"SSD" : mt == 5 ? L"Storage class memory" : L"Unknown"; d.bus = bt == 17 ? L"NVMe" : bt == 11 ? L"SATA" : bt == 7 ? L"USB" : bt == 3 ? L"ATA" : bt == 8 ? L"RAID" : bt == 10 ? L"SAS" : bt == 16 ? L"Storage Spaces" : L"";
        d.health = hs == 0 ? L"Healthy" : hs == 1 ? L"Warning" : hs == 2 ? L"Unhealthy" : L"Unknown"; d.healthCode = hs; d.size = (ULONGLONG)std::max(0.0, WNum(o, L"Size")); by[id] = d; order.push_back(id); }, e);
    if (order.empty()) rep.error = e.empty() ? L"Windows did not report any physical disks." : L"Disk health is not available on this version of Windows (needs Windows 8 or later).";
    Wmi(L"root\\Microsoft\\Windows\\Storage", L"SELECT DeviceId, Temperature, Wear, PowerOnHours, ReadErrorsTotal, WriteErrorsTotal FROM MSFT_StorageReliabilityCounter", [&](IWbemClassObject* o) {
        auto it = by.find(Get(o, L"DeviceId")); if (it == by.end()) return; auto& d = it->second; d.temp = (int)WNum(o, L"Temperature"); d.wear = (int)WNum(o, L"Wear"); d.hours = (long long)WNum(o, L"PowerOnHours"); d.readErr = (long long)WNum(o, L"ReadErrorsTotal"); d.writeErr = (long long)WNum(o, L"WriteErrorsTotal"); }, e);
    for (auto& id : order) rep.disks.push_back(by[id]);
    { int any = -1; Wmi(L"root\\wmi", L"SELECT PredictFailure FROM MSStorageDriver_FailurePredictStatus", [&](IWbemClassObject* o) { double p = WNum(o, L"PredictFailure"); if (p >= 0) any = std::max(any, p > 0 ? 1 : 0); }, e); rep.predictFail = any; }
    DWORD d = 0; rep.trimKnown = true; rep.trimOn = !(RegDw(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\FileSystem", L"DisableDeleteNotification", d) && d != 0);
    DWORD mask = GetLogicalDrives(); for (int i = 0; i < 26; ++i) if (mask & (1u << i)) { wchar_t root[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(root) != DRIVE_FIXED) continue; ULARGE_INTEGER fr, tot, tf; if (!GetDiskFreeSpaceExW(root, &fr, &tot, &tf) || !tot.QuadPart) continue;
        VolInfo v; v.path = root; wchar_t lab[64] = L"", fs[32] = L""; GetVolumeInformationW(root, lab, 64, nullptr, nullptr, nullptr, fs, 32); v.label = lab; v.fs = fs; v.total = tot.QuadPart; v.free = fr.QuadPart; rep.vols.push_back(v); }
    return rep;
}
static std::string BTag(const std::string& xml, const char* name) { std::string key = std::string("Name='") + name + "'>"; size_t p = xml.find(key); if (p == std::string::npos) return ""; p += key.size(); size_t e = xml.find('<', p); return e == std::string::npos ? "" : xml.substr(p, e - p); }
std::vector<BootRec> BootHistory(size_t maxN, wstring& err) {   // Windows records every boot's duration (Diagnostics-Performance event 100)
    std::vector<BootRec> out; EVT_HANDLE q = EvtQuery(nullptr, L"Microsoft-Windows-Diagnostics-Performance/Operational", L"*[System[(EventID=100)]]", EvtQueryChannelPath | EvtQueryReverseDirection);
    if (!q) { err = L"Windows' boot diagnostics log could not be read (" + Win32Err(GetLastError()) + L"). It may be disabled, or administrator rights may be needed."; return out; }
    EVT_HANDLE evs[16]; DWORD got = 0; while (out.size() < maxN && EvtNext(q, 16, evs, 1000, 0, &got)) { for (DWORD i = 0; i < got; ++i) { DWORD need = 0, props = 0; EvtRender(nullptr, evs[i], EvtRenderEventXml, 0, nullptr, &need, &props);
            if (need && need < 100000) { std::vector<wchar_t> x(need / 2 + 2); if (EvtRender(nullptr, evs[i], EvtRenderEventXml, need, x.data(), &need, &props)) { std::string xml = ToUtf8(x.data()); BootRec b; size_t t = xml.find("SystemTime='"); if (t != std::string::npos) { t += 12; b.time = FromUtf8(xml.substr(t, 19)); } b.bootMs = strtoull(BTag(xml, "BootTime").c_str(), nullptr, 10); b.mainMs = strtoull(BTag(xml, "MainPathBootTime").c_str(), nullptr, 10); b.postMs = strtoull(BTag(xml, "BootPostBootTime").c_str(), nullptr, 10); if (b.bootMs && out.size() < maxN) out.push_back(b); } }
            EvtClose(evs[i]); } }
    EvtClose(q); std::reverse(out.begin(), out.end()); return out;   // oldest first
}
}
