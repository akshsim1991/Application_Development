// Feather System Utilities - shell + read-only Startup/Services inventory (P0 slice).
// Mutations (enable/disable/start/stop) are intentionally NOT implemented yet: they need the
// backup -> apply -> verify -> journal pipeline (spec 6.1) and VM tests.
#ifndef UNICODE
#define UNICODE
#endif
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <memory>
#include "../core/text.h"

#ifndef SERVICE_USER_SERVICE
#define SERVICE_USER_SERVICE 0x00000040
#endif
#ifndef SERVICE_USERSERVICE_INSTANCE
#define SERVICE_USERSERVICE_INSTANCE 0x00000080
#endif
#ifndef SERVICE_CONFIG_TRIGGER_INFO
#define SERVICE_CONFIG_TRIGGER_INFO 8
#endif
#include <filesystem>
#include <algorithm>

using namespace fsu;
using std::wstring;

static const wchar_t* kAppName = L"Feather System Utilities";
static const wchar_t* kVersion = L"0.1.0";
enum { ID_NAV = 100, ID_LIST, ID_SEARCH, ID_STATUS, ID_ABOUT_TEXT };
enum { WM_SCAN_DONE = WM_APP + 1, WM_ACTIVATE_PAGE = WM_APP + 2 };
enum Page { P_DASH, P_STARTUP, P_SERVICES, P_HISTORY, P_SETTINGS, P_COUNT };
static const wchar_t* kPageNames[P_COUNT] = {L"Dashboard", L"Startup", L"Services", L"History", L"Settings"};

struct Row { std::vector<wstring> c; };
struct Table {
    std::vector<wstring> cols; std::vector<int> widths;
    std::vector<Row> rows; wstring coverage;
};
struct ScanResult { int gen; Page page; Table t; };

static HWND g_main, g_nav, g_list, g_search, g_status, g_info;
static HINSTANCE g_inst;
static Page g_page = P_DASH;
static Table g_data[P_COUNT];
static std::vector<int> g_view;            // indices into g_data[g_page].rows after filtering
static wstring g_query[P_COUNT];
static std::atomic<int> g_gen{0};
static bool g_elevated = false;
static wstring g_userName;

// ---------- helpers ----------
static wstring Win32Err(DWORD e) {
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, (LPWSTR)&buf, 0, nullptr);
    wstring s = buf ? buf : L"";
    if (buf) LocalFree(buf);
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ')) s.pop_back();
    return s + L" (" + std::to_wstring(e) + L")";   // F185: keep the code
}
static wstring ExpandEnv(const wstring& s) {
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
    if (!n) return s;
    std::wstring o(n, L'\0');
    ExpandEnvironmentStringsW(s.c_str(), &o[0], n);
    o.resize(n - 1);
    return o;
}
static bool FileExistsW(const wstring& p) {
    DWORD a = GetFileAttributesW(ExpandEnv(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}
static bool IsElevated() {
    HANDLE t; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return false;
    TOKEN_ELEVATION el{}; DWORD n = 0;
    bool r = GetTokenInformation(t, TokenElevation, &el, sizeof el, &n) && el.TokenIsElevated;
    CloseHandle(t); return r;
}
static wstring DataDir() {   // per-user storage, separate from install dir (F208)
    PWSTR p = nullptr; wstring d;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) { d = wstring(p) + L"\\FeatherSystemUtilities"; CoTaskMemFree(p); }
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

// ---------- startup scan (read-only, nothing is executed) ----------
static wstring ApprovedState(HKEY root, REGSAM view, const wchar_t* approvedSub, const wstring& name) {
    HKEY k;
    if (RegOpenKeyExW(root, approvedSub, 0, KEY_QUERY_VALUE | view, &k) != ERROR_SUCCESS) return L"Enabled";
    BYTE buf[16]{}; DWORD sz = sizeof buf, type = 0;
    LONG r = RegQueryValueExW(k, name.c_str(), nullptr, &type, buf, &sz);
    RegCloseKey(k);
    if (r == ERROR_FILE_NOT_FOUND) return L"Enabled";
    if (r != ERROR_SUCCESS || type != REG_BINARY || sz < 1) return L"Unknown";
    if (buf[0] == 2 || buf[0] == 6) return L"Enabled";
    if (buf[0] == 3 || buf[0] == 7) return L"Disabled";
    return L"Unknown";   // F080/F024: unknown stays unknown
}

static void ScanRegKey(Table& t, HKEY root, const wchar_t* rootName, REGSAM view, const wchar_t* viewName,
                       const wchar_t* sub, const wchar_t* approvedSub, bool runOnce, int& denied, int& scanned) {
    HKEY k;
    LONG r = RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | view, &k);
    if (r == ERROR_FILE_NOT_FOUND) return;
    ++scanned;
    if (r != ERROR_SUCCESS) { ++denied; return; }
    wchar_t name[1024]; DWORD i = 0;
    for (;; ++i) {
        DWORD nl = 1024, type = 0, sz = 0;
        if (RegEnumValueW(k, i, name, &nl, nullptr, &type, nullptr, &sz) != ERROR_SUCCESS) break;
        std::vector<BYTE> data(sz + 2, 0); nl = 1024; sz = (DWORD)data.size() - 2;
        if (RegEnumValueW(k, i, name, &nl, nullptr, &type, data.data(), &sz) != ERROR_SUCCESS) continue;
        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
        wstring raw((wchar_t*)data.data());
        auto pc = ParseCommand(runOnce ? raw : raw, nullptr);
        wstring expanded = ExpandEnv(pc.exe);
        auto pc2 = ParseCommand(type == REG_EXPAND_SZ ? ExpandEnv(raw) : raw, FileExistsW);
        wstring state = runOnce ? L"One-time" : ApprovedState(root, view, approvedSub, name);
        wstring target = pc2.exe.empty() ? L"" : pc2.exe;
        wstring warn = pc2.exeExists ? L"" : L"Target not found (may be an offline drive or env var)";
        wstring src = wstring(rootName) + L"\\" + sub + L" [" + viewName + L"]";
        t.rows.push_back({{name, state, runOnce ? L"RunOnce" : L"Registry Run", rootName[2] == L'C' ? L"Current user" : L"All users",
                           raw, target, warn, src}});
    }
    RegCloseKey(k);
}

static void ScanFolder(Table& t, REFKNOWNFOLDERID id, const wchar_t* scope, int& denied, int& scanned) {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p))) { ++denied; return; }
    wstring dir = p; CoTaskMemFree(p);
    ++scanned;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        wstring n = fd.cFileName;
        if (Lower(n) == L"desktop.ini") continue;
        t.rows.push_back({{n, L"Enabled", L"Startup folder", scope, dir + L"\\" + n, dir + L"\\" + n, L"", dir}});
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static Table ScanStartup() {
    Table t;
    t.cols = {L"Name", L"State", L"Type", L"Scope", L"Command", L"Target", L"Warnings", L"Source"};
    t.widths = {200, 80, 110, 90, 380, 300, 220, 360};
    int denied = 0, scanned = 0;
    const wchar_t* run = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* once = L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
    const wchar_t* appr = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
    // HKCU is not redirected: scan once. HKLM has distinct 64/32-bit views (F028).
    ScanRegKey(t, HKEY_CURRENT_USER, L"HKCU", KEY_WOW64_64KEY, L"64-bit", run, appr, false, denied, scanned);
    ScanRegKey(t, HKEY_CURRENT_USER, L"HKCU", KEY_WOW64_64KEY, L"64-bit", once, appr, true, denied, scanned);
    ScanRegKey(t, HKEY_LOCAL_MACHINE, L"HKLM", KEY_WOW64_64KEY, L"64-bit", run, appr, false, denied, scanned);
    ScanRegKey(t, HKEY_LOCAL_MACHINE, L"HKLM", KEY_WOW64_32KEY, L"32-bit", run, appr, false, denied, scanned);
    ScanRegKey(t, HKEY_LOCAL_MACHINE, L"HKLM", KEY_WOW64_64KEY, L"64-bit", once, appr, true, denied, scanned);
    ScanFolder(t, FOLDERID_Startup, L"Current user", denied, scanned);
    ScanFolder(t, FOLDERID_CommonStartup, L"All users", denied, scanned);
    t.coverage = L"Scanned " + std::to_wstring(scanned) + L" locations (Run, RunOnce, Startup folders); " + std::to_wstring(denied) +
                 L" denied. NOT scanned yet: scheduled tasks, Explorer add-ons, policy Run, other users. This is not a complete persistence audit.";
    return t;
}

// ---------- services scan (read-only) ----------
static const wchar_t* StateName(DWORD s) {
    switch (s) { case SERVICE_STOPPED: return L"Stopped"; case SERVICE_START_PENDING: return L"Starting"; case SERVICE_STOP_PENDING: return L"Stopping";
        case SERVICE_RUNNING: return L"Running"; case SERVICE_CONTINUE_PENDING: return L"Resuming"; case SERVICE_PAUSE_PENDING: return L"Pausing";
        case SERVICE_PAUSED: return L"Paused"; } return L"Unknown";
}
static Table ScanServices() {
    Table t;
    t.cols = {L"Name", L"Display name", L"State", L"Start type", L"PID", L"Account", L"Process type", L"Path", L"Notes"};
    t.widths = {170, 240, 80, 110, 60, 160, 120, 420, 200};
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);
    if (!scm) { t.coverage = L"Cannot open Service Control Manager: " + Win32Err(GetLastError()); return t; }
    DWORD need = 0, cnt = 0, resume = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32 | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE, SERVICE_STATE_ALL, nullptr, 0, &need, &cnt, &resume, nullptr);
    std::vector<BYTE> buf(need + 4096);
    resume = 0;
    if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32 | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE, SERVICE_STATE_ALL, buf.data(), (DWORD)buf.size(), &need, &cnt, &resume, nullptr) && GetLastError() != ERROR_MORE_DATA) {
        t.coverage = L"Enumeration failed: " + Win32Err(GetLastError()); CloseServiceHandle(scm); return t;
    }
    auto* es = (ENUM_SERVICE_STATUS_PROCESSW*)buf.data();
    int noCfg = 0, running = 0;
    for (DWORD i = 0; i < cnt; ++i) {
        const auto& s = es[i];
        wstring start = L"?", acct, path, notes, ptype;
        SC_HANDLE sv = OpenServiceW(scm, s.lpServiceName, SERVICE_QUERY_CONFIG);
        if (sv) {
            DWORD n = 0; QueryServiceConfigW(sv, nullptr, 0, &n);
            std::vector<BYTE> cb(n ? n : 1);
            if (QueryServiceConfigW(sv, (LPQUERY_SERVICE_CONFIGW)cb.data(), n, &n)) {
                auto* c = (LPQUERY_SERVICE_CONFIGW)cb.data();
                switch (c->dwStartType) { case SERVICE_AUTO_START: start = L"Automatic"; break; case SERVICE_DEMAND_START: start = L"Manual"; break;
                    case SERVICE_DISABLED: start = L"Disabled"; break; case SERVICE_BOOT_START: start = L"Boot"; break; case SERVICE_SYSTEM_START: start = L"System"; break; }
                acct = c->lpServiceStartName ? c->lpServiceStartName : L"";
                path = c->lpBinaryPathName ? c->lpBinaryPathName : L"";
                if (c->dwStartType == SERVICE_AUTO_START) {
                    SERVICE_DELAYED_AUTO_START_INFO di{}; DWORD m = 0;
                    if (QueryServiceConfig2W(sv, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (LPBYTE)&di, sizeof di, &m) && di.fDelayedAutostart) start = L"Automatic (Delayed)";
                }
                DWORD tn = 0; QueryServiceConfig2W(sv, SERVICE_CONFIG_TRIGGER_INFO, nullptr, 0, &tn);
                if (tn >= sizeof(DWORD)) {  // SERVICE_TRIGGER_INFO begins with DWORD cTriggers
                    std::vector<BYTE> tb(tn);
                    if (QueryServiceConfig2W(sv, SERVICE_CONFIG_TRIGGER_INFO, tb.data(), tn, &tn) && *(DWORD*)tb.data() > 0) start += L" (Trigger)";
                }
            } else ++noCfg;
            CloseServiceHandle(sv);
        } else ++noCfg;
        DWORD ty = s.ServiceStatusProcess.dwServiceType;
        if (ty & SERVICE_USERSERVICE_INSTANCE) { ptype = L"Per-user instance"; notes = L"Per-user service; do not bulk-edit by name suffix"; }
        else if (ty & SERVICE_USER_SERVICE) ptype = L"Per-user template";
        else if (ty & SERVICE_WIN32_SHARE_PROCESS) { ptype = L"Shared process"; notes = L"Process metrics cover all services in the host"; }
        else if (ty & SERVICE_WIN32_OWN_PROCESS) ptype = L"Own process";
        if (s.ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING) ++running;
        DWORD pid = s.ServiceStatusProcess.dwProcessId;
        t.rows.push_back({{s.lpServiceName, s.lpDisplayName, StateName(s.ServiceStatusProcess.dwCurrentState), start,
                           pid ? std::to_wstring(pid) : L"", acct, ptype, path, notes}});
    }
    CloseServiceHandle(scm);
    t.coverage = L"Enumerated " + std::to_wstring(cnt) + L" Win32 services (" + std::to_wstring(running) + L" running); config unreadable for " +
                 std::to_wstring(noCfg) + L". Drivers not listed yet. Visibility is limited to what your account may query.";
    return t;
}

// ---------- history (journal viewer) ----------
static Table ScanHistory() {
    Table t; t.cols = {L"Entry"}; t.widths = {900};
    std::ifstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"));
    std::string line; while (std::getline(f, line)) t.rows.push_back({{FromUtf8(line)}});
    t.coverage = t.rows.empty() ? L"No changes recorded. This build makes no changes to Windows." : std::to_wstring(t.rows.size()) + L" journal entries.";
    return t;
}

// ---------- UI ----------
static void SetStatus(int part, const wstring& s) { SendMessageW(g_status, SB_SETTEXTW, part, (LPARAM)s.c_str()); }

static void ApplyFilter() {
    Table& t = g_data[g_page];
    g_view.clear();
    for (size_t i = 0; i < t.rows.size(); ++i) if (MatchesQuery(t.rows[i].c, g_query[g_page])) g_view.push_back((int)i);
    ListView_SetItemCountEx(g_list, (int)g_view.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    InvalidateRect(g_list, nullptr, TRUE);
    wstring s = L"Showing " + std::to_wstring(g_view.size()) + L" of " + std::to_wstring(t.rows.size());
    if (!g_query[g_page].empty()) s += L"  (filter: \"" + g_query[g_page] + L"\")";
    SetStatus(0, s);
}

static void BuildColumns() {
    while (ListView_DeleteColumn(g_list, 0)) {}
    Table& t = g_data[g_page];
    UINT dpi = GetDpiForWindow(g_main);
    for (size_t i = 0; i < t.cols.size(); ++i) {
        LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)t.cols[i].c_str();
        c.cx = MulDiv(t.widths[i], dpi, 96);
        SendMessageW(g_list, LVM_INSERTCOLUMNW, i, (LPARAM)&c);
    }
}

static void DashboardText() {
    auto count = [&](Page p, const wchar_t* col, const wchar_t* val) {
        int n = 0; auto& t = g_data[p]; size_t ci = 0;
        for (; ci < t.cols.size(); ++ci) if (t.cols[ci] == col) break;
        if (ci == t.cols.size()) return n;
        for (auto& r : t.rows) { if (r.c[ci] == val) ++n; }
        return n; };
    wstring s = L"Feather System Utilities " + wstring(kVersion) + L"\r\nDeveloped by Akshaya Simha\r\n\r\n";
    s += L"STARTUP\r\n  Entries: " + std::to_wstring(g_data[P_STARTUP].rows.size()) + L"   Disabled: " + std::to_wstring(count(P_STARTUP, L"State", L"Disabled")) +
         L"\r\n  " + g_data[P_STARTUP].coverage + L"\r\n\r\n";
    s += L"SERVICES\r\n  Services: " + std::to_wstring(g_data[P_SERVICES].rows.size()) + L"   Running: " + std::to_wstring(count(P_SERVICES, L"State", L"Running")) +
         L"\r\n  " + g_data[P_SERVICES].coverage + L"\r\n\r\n";
    s += L"This build is read-only: it does not change startup entries or services. Fully offline; no telemetry.\r\n"
         L"Shortcuts: Ctrl+F search, Esc clear, Ctrl+1..5 pages, F5 refresh, Ctrl+E export CSV, Ctrl+C copy rows, Enter details.";
    SetWindowTextW(g_info, s.c_str());
}

static void ShowPage(Page p) {
    g_page = p;
    SendMessageW(g_nav, LB_SETCURSEL, p, 0);
    bool dash = p == P_DASH || p == P_SETTINGS;
    ShowWindow(g_info, dash ? SW_SHOW : SW_HIDE);
    ShowWindow(g_list, dash ? SW_HIDE : SW_SHOW);
    ShowWindow(g_search, dash ? SW_HIDE : SW_SHOW);
    SetWindowTextW(g_search, g_query[p].c_str());
    if (p == P_DASH) DashboardText();
    else if (p == P_SETTINGS) SetWindowTextW(g_info, (wstring(L"Settings\r\n\r\nThemes (light/dark/midnight/warm), density and per-module column memory are planned (F005, F017, F217).\r\nCurrently the app follows the system light theme.\r\n\r\nData folder: ") + DataDir()).c_str());
    else { BuildColumns(); ApplyFilter(); }
    SetStatus(1, g_data[p].coverage.substr(0, 160));
    RECT r; GetClientRect(g_main, &r); SendMessageW(g_main, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
}

static DWORD WINAPI ScanThread(LPVOID) {
    int gen = ++g_gen;                       // F237: off the UI thread; stale results discarded by generation
    auto* a = new ScanResult{gen, P_STARTUP, ScanStartup()};
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM)a);
    auto* b = new ScanResult{gen, P_SERVICES, ScanServices()};
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM)b);
    auto* c = new ScanResult{gen, P_HISTORY, ScanHistory()};
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM)c);
    return 0;
}
static void StartScan() {
    SetStatus(1, L"Scanning...");
    HANDLE h = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

static wstring SelectedRowsText(bool csv) {
    wstring o; Table& t = g_data[g_page];
    if (csv) o += CsvRow(t.cols) + L"\r\n";
    int i = -1; bool any = false;
    while ((i = ListView_GetNextItem(g_list, i, LVNI_SELECTED)) != -1) {
        any = true; auto& r = t.rows[g_view[i]];
        if (csv) o += CsvRow(r.c) + L"\r\n";
        else { for (size_t k = 0; k < r.c.size(); ++k) { if (k) o += L'\t'; o += r.c[k]; } o += L"\r\n"; }
    }
    if (!any && csv) for (int v : g_view) o += CsvRow(t.rows[v].c) + L"\r\n";   // export filtered scope
    return o;
}
static void CopySelection() {
    wstring s = SelectedRowsText(false);
    if (s.empty() || !OpenClipboard(g_main)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (h) { memcpy(GlobalLock(h), s.c_str(), (s.size() + 1) * sizeof(wchar_t)); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
    CloseClipboard();
}
static void ExportCsv() {
    if (g_page != P_STARTUP && g_page != P_SERVICES) return;
    wchar_t file[MAX_PATH] = L"inventory.csv";
    OPENFILENAMEW o{sizeof o}; o.hwndOwner = g_main; o.lpstrFilter = L"CSV (*.csv)\0*.csv\0"; o.lpstrFile = file; o.nMaxFile = MAX_PATH;
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST; o.lpstrDefExt = L"csv";
    if (!GetSaveFileNameW(&o)) return;
    wstring prov = L"# " + wstring(kAppName) + L" " + kVersion + L" | machine: local | page: " + kPageNames[g_page] + L" | rows: " + std::to_wstring(g_view.size()) + L"\r\n";
    std::string bytes = "\xEF\xBB\xBF" + ToUtf8(prov + SelectedRowsText(true));
    if (!AtomicWrite(file, bytes)) MessageBoxW(g_main, L"Could not write the file.", kAppName, MB_ICONERROR);   // F203
}
static void ShowDetails() {
    int i = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    if (i < 0) return;
    Table& t = g_data[g_page]; auto& r = t.rows[g_view[i]];
    wstring s; for (size_t k = 0; k < t.cols.size(); ++k) s += t.cols[k] + L": " + r.c[k] + L"\r\n\r\n";
    MessageBoxW(g_main, s.c_str(), L"Details", MB_OK | MB_ICONINFORMATION);
}

static void Layout(int w, int h) {
    UINT dpi = GetDpiForWindow(g_main);
    int navW = MulDiv(150, dpi, 96), sbH = 0, searchH = MulDiv(26, dpi, 96), pad = MulDiv(6, dpi, 96);
    RECT sr; GetWindowRect(g_status, &sr); sbH = sr.bottom - sr.top;
    SendMessageW(g_status, WM_SIZE, 0, 0);
    MoveWindow(g_nav, 0, 0, navW, h - sbH, TRUE);
    int x = navW + pad, cw = w - x - pad, ch = h - sbH;
    MoveWindow(g_search, x, pad, cw, searchH, TRUE);
    MoveWindow(g_list, x, pad * 2 + searchH, cw, ch - pad * 3 - searchH, TRUE);
    MoveWindow(g_info, x, pad, cw, ch - pad * 2, TRUE);
    int parts[2] = {MulDiv(300, dpi, 96), -1}; SendMessageW(g_status, SB_SETPARTS, 2, (LPARAM)parts);
}

static LRESULT CALLBACK SearchProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { SetWindowTextW(h, L""); return 0; }
    if (m == WM_KEYDOWN && (w == VK_DOWN || w == VK_RETURN)) { SetFocus(g_list); return 0; }
    if (m == WM_CHAR && (w == VK_ESCAPE || w == VK_RETURN)) return 0;
    return DefSubclassProc(h, m, w, l);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_main = h;
        UINT dpi = GetDpiForWindow(h);
        g_nav = CreateWindowExW(0, L"LISTBOX", L"Navigation", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_NAV, g_inst, nullptr);
        for (auto n : kPageNames) SendMessageW(g_nav, LB_ADDSTRING, 0, (LPARAM)n);
        g_search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_SEARCH, g_inst, nullptr);
        SendMessageW(g_search, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search name, path, publisher, source...  (Ctrl+F)");
        SetWindowSubclass(g_search, SearchProc, 1, 0);
        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_OWNERDATA | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_LIST, g_inst, nullptr);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP);
        g_info = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_ABOUT_TEXT, g_inst, nullptr);
        g_status = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, h, (HMENU)ID_STATUS, g_inst, nullptr);
        HFONT f = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        for (HWND c : {g_nav, g_search, g_list, g_info, g_status}) SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
        g_data[P_DASH].coverage = L"Local machine";
        // F012 persistent target indicator
        wchar_t un[256]; DWORD n = 256; GetUserNameW(un, &n); g_userName = un;
        g_elevated = IsElevated();
        SetWindowTextW(h, (wstring(kAppName) + L"  -  Local | " + g_userName + (g_elevated ? L" | Administrator" : L" | Standard rights (read-only)")).c_str());
        ShowPage(P_DASH);
        StartScan();
        return 0; }
    case WM_SIZE: Layout(LOWORD(l), HIWORD(l)); return 0;
    case WM_GETMINMAXINFO: { auto* mi = (MINMAXINFO*)l; mi->ptMinTrackSize = {700, 450}; return 0; }
    case WM_DPICHANGED: { auto* r = (RECT*)l; SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0; }
    case WM_SETFOCUS: SetFocus(g_nav); return 0;
    case WM_SCAN_DONE: {
        std::unique_ptr<ScanResult> r((ScanResult*)l);
        if (r->gen != g_gen) return 0;                         // stale generation
        g_data[r->page] = std::move(r->t);
        if (g_page == r->page || g_page == P_DASH) ShowPage(g_page);
        return 0; }
    case WM_ACTIVATE_PAGE: { if (IsIconic(h)) ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); if (w < P_COUNT) ShowPage((Page)w); return 0; }
    case WM_NOTIFY: {
        auto* nh = (NMHDR*)l;
        if (nh->hwndFrom == g_list) {
            if (nh->code == LVN_GETDISPINFOW) {
                auto* di = (NMLVDISPINFOW*)l;
                Table& t = g_data[g_page];
                if ((di->item.mask & LVIF_TEXT) && di->item.iItem < (int)g_view.size() && di->item.iSubItem < (int)t.cols.size()) {
                    const wstring& s = t.rows[g_view[di->item.iItem]].c[di->item.iSubItem];
                    wcsncpy_s(di->item.pszText, di->item.cchTextMax, s.c_str(), _TRUNCATE);
                }
            } else if (nh->code == LVN_COLUMNCLICK) {
                auto* c = (NMLISTVIEW*)l; Table& t = g_data[g_page]; static int lastCol = -1, asc = 1;
                asc = (lastCol == c->iSubItem) ? -asc : 1; lastCol = c->iSubItem; int col = c->iSubItem;
                std::stable_sort(t.rows.begin(), t.rows.end(), [&](const Row& a, const Row& b) {
                    auto la = Lower(a.c[col]), lb = Lower(b.c[col]);
                    if (la.empty() != lb.empty()) return !la.empty();       // F016: missing values always last
                    return asc > 0 ? la < lb : la > lb; });
                ApplyFilter();
            } else if (nh->code == LVN_ITEMACTIVATE) ShowDetails();
        }
        return 0; }
    case WM_COMMAND:
        if (LOWORD(w) == ID_NAV && HIWORD(w) == LBN_SELCHANGE) { int s = (int)SendMessageW(g_nav, LB_GETCURSEL, 0, 0); if (s >= 0) ShowPage((Page)s); }
        else if (LOWORD(w) == ID_SEARCH && HIWORD(w) == EN_CHANGE) {
            wchar_t b[512]; GetWindowTextW(g_search, b, 512); g_query[g_page] = b; ApplyFilter();
        }
        return 0;
    case WM_DESTROY: ++g_gen; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
    g_inst = inst;
    // F004: single instance; relaunch activates existing window and selects the requested page.
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance");
    wstring c = cmd ? cmd : L""; Page req = P_DASH;
    if (c.find(L"--services") != wstring::npos) req = P_SERVICES; else if (c.find(L"--startup") != wstring::npos) req = P_STARTUP;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND e = FindWindowW(L"FeatherSystemUtilitiesWnd", nullptr)) PostMessageW(e, WM_ACTIVATE_PAGE, req, 0);
        return 0;
    }
    INITCOMMONCONTROLSEX ic{sizeof ic, ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES}; InitCommonControlsEx(&ic);
    WNDCLASSW wc{}; wc.lpfnWndProc = WndProc; wc.hInstance = inst; wc.lpszClassName = L"FeatherSystemUtilitiesWnd";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, kAppName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720, nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show); UpdateWindow(w);
    if (req != P_DASH) PostMessageW(w, WM_ACTIVATE_PAGE, req, 0);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_KEYDOWN && GetForegroundWindow() == w) {
            bool ctrl = GetKeyState(VK_CONTROL) < 0;
            if (ctrl && m.wParam == 'F') { if (g_page == P_STARTUP || g_page == P_SERVICES || g_page == P_HISTORY) { SetFocus(g_search); SendMessageW(g_search, EM_SETSEL, 0, -1); } continue; }
            if (ctrl && m.wParam >= '1' && m.wParam <= '5') { ShowPage((Page)(m.wParam - '1')); continue; }
            if (ctrl && m.wParam == 'E') { ExportCsv(); continue; }
            if (ctrl && m.wParam == 'C' && GetFocus() == g_list) { CopySelection(); continue; }
            if (ctrl && m.wParam == 'A' && GetFocus() == g_list) { ListView_SetItemState(g_list, -1, LVIS_SELECTED, LVIS_SELECTED); continue; }
            if (m.wParam == VK_F5) { StartScan(); continue; }
            if (m.wParam == VK_RETURN && GetFocus() == g_list) { ShowDetails(); continue; }
        }
        if (!IsDialogMessageW(w, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    if (mtx) CloseHandle(mtx);
    return 0;
}
