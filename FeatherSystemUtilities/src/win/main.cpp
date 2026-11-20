// Feather System Utilities 1.5 - one window: Dashboard, Startup, Services, Drivers, History, Settings.
// Native Win32, offline. All Windows changes go through the journal (see startup.cpp / services.cpp).
#include "app.h"
#include "../core/text.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <set>
#include <fstream>
#include <filesystem>

using namespace fsu;
using std::wstring;

static const wchar_t* kApp = L"Feather System Utilities";

// ---- palette (switchable theme) ----
static Pal P = PalFor(0);
#define cBg (P.bg)
#define cBar (P.bar)
#define cText (P.text)
#define cDim (P.dim)
#define cGrid (P.grid)
#define cGreen (P.green)
#define cRed (P.red)
#define cLav (P.lav)
#define cAmber (P.amber)
#define cHot (P.hot)
#define cSel (P.sel)
#define cAccent (P.accent)
#define cDisabledTb (P.disabledTb)
#define cRail (P.rail)
#define cRailSel (P.railSel)

enum Page { P_DASH, P_STARTUP, P_SERVICES, P_DRIVERS, P_CLEAN, P_LARGE, P_DUPES, P_EMPTY, P_HISTORY, P_SETTINGS, P_COUNT };
static const wchar_t* kPageNames[P_COUNT] = {L"Dashboard", L"Startup", L"Services", L"Drivers", L"Cleaner", L"Large files", L"Duplicates", L"Empty folders", L"History", L"Settings"};
enum Cmd { C_REFRESH = 1000, C_ENABLE, C_DISABLE, C_DELETE, C_STARTLATER, C_ADD, C_RESTORE, C_UNDO, C_FILTER, C_MORE, C_SVC_START, C_SVC_STOP, C_SVC_RESTART, C_SVC_PAUSE, C_SVC_RESUME, C_SVC_KILL,
    C_STARTTYPE, C_PROFILES, C_DETAILS, C_COPYNAME, C_COPYDETAILS, C_OPENLOC, C_REGEDIT, C_VT, C_SEARCHONLINE, C_SELALL, C_ADMIN, C_SETTINGS, C_ABOUT, C_EXIT, C_OPENBACKUPS, C_OPENDATA, C_EXPORT,
    C_ST_AUTO, C_ST_AUTODELAY, C_ST_MANUAL, C_ST_DISABLED, C_PROF_SAVESEL, C_PROF_SNAPSHOT, C_PROF_OPENDIR, C_REMOVE_DELAY, C_CLEAN, C_CLEAN_SELALL, C_CLEAN_RESTORE, C_FS_SCAN, C_FS_STOP, C_FS_FOLDERS, C_FS_MIN, C_FS_AUTO, C_FS_RECYCLE, C_FS_FOLDER_ADD, C_FS_FOLDER_DEFAULT,
    C_FILTER_BASE = 1100, C_PROF_BASE = 1150, C_RESTORE_BASE = 1200, C_DELAY_BASE = 1300, C_REGRESTORE_BASE = 1400, C_FS_MIN_BASE = 1500, C_FS_AUTO_BASE = 1520, C_FS_RM_BASE = 1560 };
enum { ID_NAV = 100, ID_SEARCH = 102, ID_LIST, ID_STATUS, ID_PANEL };
enum { WM_SCAN_DONE = WM_APP + 1, WM_ACTIVATE_PAGE, WM_JOB_DONE, WM_FILE_DONE, WM_FILE_PROGRESS };
static const int kDelays[] = {15, 30, 60, 120, 300};

// ---- state ----
static HINSTANCE g_inst; static HWND g_main, g_nav, g_search, g_list, g_hdr, g_status, g_panel;
static HFONT g_font, g_fontBig, g_fontTitle; static UINT g_dpi = 96; static HANDLE g_mtx;
static HBRUSH g_brBg, g_brBar, g_brRail;
static Page g_page = P_DASH; static bool g_elevated = false; static wstring g_user;
static StartupScan g_su; static ServiceScan g_sv, g_dv; static CleanScan g_cl; static FileScan g_large, g_dupes, g_empty; static bool g_largeDone = false, g_dupesDone = false, g_emptyDone = false, g_fsBusy = false; static std::atomic<bool> g_fsCancel{false};
static std::vector<std::vector<wstring>> g_cells[P_COUNT];
static std::vector<int> g_view;
static int g_filter[P_COUNT]; static wstring g_query[P_COUNT];
static int g_sortCol[P_COUNT] = {0, 1, 0, 0, 3, 1, 3, 0, 0, 0}; static bool g_sortAsc[P_COUNT] = {true, true, true, true, false, false, true, true, false, true};
static std::atomic<int> g_gen{0}; static bool g_scanning = false;
static wstring g_stMsg; static std::set<wstring> g_keepSel;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

struct UndoItem { int kind; StartupEntry se; bool wasEnabled; wstring name; DWORD type; bool delayed; int group; };
static std::vector<UndoItem> g_undo; static int g_groupSeq = 0;
struct ScanMsg { int gen; StartupScan su; ServiceScan sv, dv; CleanScan cl; int which; };
struct JobDone { wstring summary; };

// ---- columns ----
struct Col { const wchar_t* name; int w; bool right; };
static const Col kStartupCols[] = {{L"Name", 230, false}, {L"Status", 80, false}, {L"Sign-in impact", 105, false}, {L"Warnings", 150, false}, {L"Publisher", 190, false}, {L"Type", 200, false},
    {L"Starts", 130, false}, {L"For", 100, false}, {L"Command", 640, false}, {L"Location", 330, false}};
static const Col kServiceCols[] = {{L"Name", 250, false}, {L"Service name", 170, false}, {L"Status", 80, false}, {L"Start type", 150, false}, {L"Safety", 85, false}, {L"PID", 60, true},
    {L"Log on as", 130, false}, {L"Description", 520, false}, {L"Warnings", 200, false}};
static const Col kDriverCols[] = {{L"Name", 250, false}, {L"Driver name", 170, false}, {L"Status", 80, false}, {L"Start type", 110, false}, {L"Class", 100, false}, {L"Type", 130, false},
    {L"Description", 360, false}, {L"Path", 460, false}, {L"Warnings", 160, false}};
static const Col kCleanCols[] = {{L"Item", 290, false}, {L"Type", 200, false}, {L"Risk", 70, false}, {L"Size", 90, true}, {L"Detail", 150, false}, {L"Location", 400, false}, {L"Why / note", 480, false}};
static const Col kEmptyCols[] = {{L"Folder", 300, false}, {L"Subfolders", 90, true}, {L"Modified", 130, false}, {L"Location", 700, false}};
static const Col kLargeCols[] = {{L"Name", 320, false}, {L"Size", 90, true}, {L"Modified", 130, false}, {L"Type", 80, false}, {L"Location", 620, false}};
static const Col kDupCols[] = {{L"Name", 320, false}, {L"Size", 90, true}, {L"Modified", 130, false}, {L"Group", 150, false}, {L"Location", 620, false}};
static const Col kHistCols[] = {{L"Time (UTC)", 160, false}, {L"Item", 260, false}, {L"Before", 160, false}, {L"After", 240, false}, {L"Result", 520, false}};
static const Col* ColsFor(Page p, int& n) {
    if (p == P_STARTUP) { n = 10; return kStartupCols; } if (p == P_SERVICES) { n = 9; return kServiceCols; } if (p == P_DRIVERS) { n = 9; return kDriverCols; } if (p == P_CLEAN) { n = 7; return kCleanCols; } if (p == P_EMPTY) { n = 4; return kEmptyCols; } if (p == P_LARGE) { n = 5; return kLargeCols; } if (p == P_DUPES) { n = 5; return kDupCols; } n = 5; return kHistCols;
}
static const wchar_t* kStartupFilters[] = {L"Everything", L"Enabled", L"Disabled", L"Needs attention (warnings)", L"New in the last 7 days", L"Not from Microsoft", L"Registry (Run keys)", L"Startup folders", L"Scheduled tasks", L"Explorer add-ons", L"Delayed by Feather", L"Windows system items only"};
static const wchar_t* kServiceFilters[] = {L"All services", L"Running", L"Stopped", L"Automatic but not running", L"Disabled", L"Third-party only", L"Critical to Windows", L"Needs attention (warnings)"};
static const wchar_t* kDriverFilters[] = {L"All drivers", L"Running", L"Stopped", L"Boot/System start", L"Third-party only", L"Needs attention (warnings)"};
static const wchar_t* kCleanFilters[] = {L"Everything", L"Temporary files", L"Registry entries"};
static const wchar_t* kLargeFilters[] = {L"Everything", L"Videos", L"Archives and disk images", L"Installers and apps", L"Pictures", L"Audio", L"Documents", L"Other"};
static int FilterCount(Page p) { return p == P_STARTUP ? 12 : p == P_SERVICES ? 8 : p == P_DRIVERS ? 6 : p == P_CLEAN ? 3 : p == P_LARGE ? 8 : 0; }
static const wchar_t* FilterName(Page p) { return p == P_STARTUP ? kStartupFilters[g_filter[p]] : p == P_SERVICES ? kServiceFilters[g_filter[p]] : p == P_DRIVERS ? kDriverFilters[g_filter[p]] : p == P_CLEAN ? kCleanFilters[g_filter[p]] : p == P_LARGE ? kLargeFilters[g_filter[p]] : L""; }
static bool IsSvcPage(Page p) { return p == P_SERVICES || p == P_DRIVERS; }
static bool IsFilePage(Page p) { return p == P_LARGE || p == P_DUPES || p == P_EMPTY; }
static bool IsRegItem(const CleanItem& e) { return e.kind == 2 || e.kind == 3; }
static const wchar_t* RiskName(int r) { return r == 2 ? L"High" : r == 1 ? L"Medium" : L"Low"; }
static bool IsActionList(Page p) { return p == P_STARTUP || p == P_SERVICES || p == P_DRIVERS || p == P_CLEAN || IsFilePage(p); }
static FileScan& FS(Page p) { return p == P_DUPES ? g_dupes : p == P_EMPTY ? g_empty : g_large; }
static int ExtCat(const wstring& e) {
    static const std::map<wstring, int> m = {{L"mp4",1},{L"mkv",1},{L"avi",1},{L"mov",1},{L"wmv",1},{L"flv",1},{L"webm",1},{L"m4v",1},{L"zip",2},{L"rar",2},{L"7z",2},{L"tar",2},{L"gz",2},{L"iso",2},{L"vhd",2},{L"vhdx",2},{L"img",2},{L"wim",2},{L"cab",2},
        {L"exe",3},{L"msi",3},{L"msix",3},{L"appx",3},{L"jpg",4},{L"jpeg",4},{L"png",4},{L"gif",4},{L"bmp",4},{L"tif",4},{L"tiff",4},{L"heic",4},{L"psd",4},{L"raw",4},{L"mp3",5},{L"wav",5},{L"flac",5},{L"aac",5},{L"m4a",5},
        {L"pdf",6},{L"doc",6},{L"docx",6},{L"xls",6},{L"xlsx",6},{L"ppt",6},{L"pptx",6},{L"txt",6}};
    auto it = m.find(e); return it == m.end() ? 7 : it->second;
}
static std::vector<ServiceEntry>& SvcVec(Page p) { return p == P_DRIVERS ? g_dv.entries : g_sv.entries; }

// ================= toolbar: native owner-drawn buttons (keyboard + screen-reader accessible) =================
struct TbItem { int id; wstring text; bool enabled = true, dropdown = false, sep = false, right = false; HWND h = nullptr; RECT r{}; };
static std::vector<TbItem> g_items; static HWND g_hotBtn = nullptr; static bool g_kbd = false; static int g_tbLeft = 0;
static RECT TbStrip() { RECT r; GetClientRect(g_main, &r); r.left = S(120); r.bottom = S(40); return r; }

static LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_MOUSEMOVE && g_hotBtn != h) { HWND o = g_hotBtn; g_hotBtn = h; InvalidateRect(h, nullptr, FALSE); if (o) InvalidateRect(o, nullptr, FALSE); TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0}; TrackMouseEvent(&t); }
    else if (m == WM_MOUSELEAVE && g_hotBtn == h) { g_hotBtn = nullptr; InvalidateRect(h, nullptr, FALSE); }
    else if (m == WM_LBUTTONDOWN) g_kbd = false; else if (m == WM_KEYDOWN) g_kbd = true;
    return DefSubclassProc(h, m, w, l);
}
static void TbLayout() {
    HDC dc = GetDC(g_main); HFONT of = (HFONT)SelectObject(dc, g_font); RECT st = TbStrip(); int x = st.left + S(6), rw = 0;
    auto widthOf = [&](TbItem& it) { if (it.sep) return S(10); SIZE sz; wstring t = it.text + (it.dropdown ? L" ▾" : L""); GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz); return (int)sz.cx + S(26); };
    for (auto& it : g_items) if (it.right) rw += widthOf(it);
    int rx = st.right - rw - S(6); g_tbLeft = rx;
    for (auto& it : g_items) { int w = widthOf(it); int& px = it.right ? rx : x; it.r = {px, st.top, px + w, st.bottom}; if (it.h) MoveWindow(it.h, px, st.top, w, st.bottom, TRUE); px += w; }
    SelectObject(dc, of); ReleaseDC(g_main, dc); InvalidateRect(g_main, &st, TRUE);
}
static void TbSet(std::vector<TbItem> items) {
    for (auto& i : g_items) if (i.h) DestroyWindow(i.h);
    g_items = std::move(items); g_hotBtn = nullptr; HWND prev = g_nav;
    for (auto& it : g_items) if (!it.sep) {
        it.h = CreateWindowExW(0, L"BUTTON", it.text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, g_main, (HMENU)(INT_PTR)it.id, g_inst, nullptr);
        SendMessageW(it.h, WM_SETFONT, (WPARAM)g_font, TRUE); SetWindowSubclass(it.h, BtnProc, 1, 0); EnableWindow(it.h, it.enabled);
        SetWindowPos(it.h, prev, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); prev = it.h;
    }
    if (g_search) SetWindowPos(g_search, prev, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    TbLayout();
}
static TbItem* TbFind(int id) { for (auto& i : g_items) if (i.id == id) return &i; return nullptr; }
static void TbEnable(int id, bool en) { if (auto* i = TbFind(id)) if (i->enabled != en) { i->enabled = en; if (i->h) EnableWindow(i->h, en); } }
static void TbText(int id, const wstring& t) { if (auto* i = TbFind(id)) { i->text = t; if (i->h) SetWindowTextW(i->h, t.c_str()); TbLayout(); } }
static bool TbOn(int id) { auto* i = TbFind(id); return i && i->enabled; }
static void DrawToolbarButton(DRAWITEMSTRUCT* di) {
    auto* it = TbFind((int)di->CtlID); if (!it) return;
    bool dis = (di->itemState & ODS_DISABLED) != 0, hot = g_hotBtn == di->hwndItem, press = (di->itemState & ODS_SELECTED) != 0;
    HBRUSH b = CreateSolidBrush(!dis && press ? cSel : (!dis && hot) ? cHot : cBar); FillRect(di->hDC, &di->rcItem, b); DeleteObject(b);
    SelectObject(di->hDC, g_font); SetBkMode(di->hDC, TRANSPARENT); SetTextColor(di->hDC, dis ? cDisabledTb : cText);
    wstring t = it->text + (it->dropdown ? L" ▾" : L""); RECT tr = di->rcItem; DrawTextW(di->hDC, t.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((di->itemState & ODS_FOCUS) && g_kbd) { RECT f = di->rcItem; InflateRect(&f, -S(3), -S(7)); HPEN p = CreatePen(PS_DOT, 1, cAccent); HGDIOBJ op = SelectObject(di->hDC, p), ob = SelectObject(di->hDC, GetStockObject(NULL_BRUSH)); Rectangle(di->hDC, f.left, f.top, f.right, f.bottom); SelectObject(di->hDC, op); SelectObject(di->hDC, ob); DeleteObject(p); }
}

// ================= status bar =================
static LRESULT CALLBACK StProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static RECT linkR{};
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); FillRect(dc, &cr, g_brBar); SelectObject(dc, g_font); SetBkMode(dc, TRANSPARENT);
        wchar_t left[512]; GetWindowTextW(h, left, 512);
        wstring link = g_elevated ? L"Running as administrator" : L"Restart as administrator"; SIZE sz; GetTextExtentPoint32W(dc, link.c_str(), (int)link.size(), &sz);
        linkR = {cr.right - S(18) - sz.cx - S(24), 0, cr.right - S(18), cr.bottom};
        SetTextColor(dc, g_elevated ? cDim : cAccent); DrawTextW(dc, link.c_str(), -1, &linkR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        SetTextColor(dc, cText); RECT lr{S(8), 0, cr.right / 2, cr.bottom}; DrawTextW(dc, left, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, g_stMsg.empty() ? cDim : cText); RECT mr{cr.right / 2 - S(40), 0, linkR.left - S(12), cr.bottom};
        wstring msg = g_stMsg.empty() ? (g_page == P_STARTUP ? L"Tip: right-click an entry. Every change is journaled; Ctrl+Z undoes it." : g_page == P_SERVICES ? L"Tip: select several services (Ctrl/Shift) to act on them together." : g_page == P_DRIVERS ? L"Drivers are shown read-only." : IsFilePage(g_page) ? L"Files go to the Recycle Bin (restorable). At least one copy per duplicate group is always kept." : g_page == P_CLEAN ? L"Select items, then Clean selected. Temp files are deleted permanently; registry entries are backed up first." : L"") : g_stMsg;
        DrawTextW(dc, msg.c_str(), -1, &mr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        EndPaint(h, &ps); return 0; }
    case WM_SETCURSOR: { POINT p; GetCursorPos(&p); ScreenToClient(h, &p); if (!g_elevated && PtInRect(&linkR, p)) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; } break; }
    case WM_LBUTTONUP: { POINT p{(short)LOWORD(l), (short)HIWORD(l)}; if (!g_elevated && PtInRect(&linkR, p)) PostMessageW(GetParent(h), WM_COMMAND, C_ADMIN, 0); return 0; }
    case WM_SETTEXT: { LRESULT r = DefWindowProcW(h, m, w, l); InvalidateRect(h, nullptr, FALSE); return r; }
    }
    return DefWindowProcW(h, m, w, l);
}
static void SetMsg(const wstring& s) { g_stMsg = s; InvalidateRect(g_status, nullptr, FALSE); }

// ================= helpers =================
static void SetClip(const wstring& s) {
    if (s.empty() || !OpenClipboard(g_main)) return; EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * 2); if (g) { memcpy(GlobalLock(g), s.c_str(), (s.size() + 1) * 2); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); } CloseClipboard();
}
static std::vector<int> Selected() {
    std::vector<int> r; if (!IsActionList(g_page)) return r;
    int i = -1; while ((i = ListView_GetNextItem(g_list, i, LVNI_SELECTED)) != -1) if (i < (int)g_view.size()) r.push_back(g_view[i]); return r;
}
static wstring Plural(size_t n, const wchar_t* one, const wchar_t* many) { return std::to_wstring(n) + L" " + (n == 1 ? one : many); }
static wstring BulletList(const std::vector<wstring>& v) {
    wstring o; size_t n = 0; for (auto& s : v) { if (n++ == 8) { o += L"\r\n  • …and " + std::to_wstring(v.size() - 8) + L" more"; break; } o += L"\r\n  • " + s; } return o;
}
static bool Confirm(const wstring& q, const std::vector<wstring>& items) { return DarkMsg(g_main, kApp, q + L"\r\n" + BulletList(items), 2) == IDYES; }
static void Warn(const wstring& t) { DarkMsg(g_main, kApp, t, 1); }
static int Rank(Page p, const wstring& v) {
    if (p == P_STARTUP) return v == L"Enabled" ? 0 : v == L"One-time" ? 1 : v == L"Disabled" ? 2 : 3;
    return v == L"Running" ? 0 : v == L"Paused" ? 1 : v == L"Stopped" ? 3 : 2;
}
static wstring EntryName(Page p, int i) { return p == P_STARTUP ? g_su.entries[i].name : p == P_CLEAN ? g_cl.items[i].name : IsFilePage(p) ? FS(p).files[i].path : SvcVec(p)[i].display; }

// ================= data -> rows =================
static void BuildCells() {
    g_cells[P_STARTUP].clear(); for (auto& e : g_su.entries) g_cells[P_STARTUP].push_back({e.name, e.state, L"Not measured", e.warnings, e.publisher, e.type, e.starts, e.scope, e.command, e.location});
    g_cells[P_SERVICES].clear(); for (auto& e : g_sv.entries) g_cells[P_SERVICES].push_back({e.display, e.name, e.state, e.start, e.safety, e.pid ? std::to_wstring(e.pid) : L"", e.account, e.description, e.warnings});
    g_cells[P_CLEAN].clear(); for (auto& e : g_cl.items) g_cells[P_CLEAN].push_back({e.name, e.type, RiskName(e.risk), FormatBytes(e.bytes), e.detail, e.location, e.why});
    g_cells[P_EMPTY].clear(); for (auto& e : g_empty.files) g_cells[P_EMPTY].push_back({e.name, std::to_wstring(e.groupCount), FileDateText(e.mtime), e.folder});
    g_cells[P_LARGE].clear(); for (auto& e : g_large.files) g_cells[P_LARGE].push_back({e.name, FormatBytes(e.size), FileDateText(e.mtime), e.ext.empty() ? wstring(L"(none)") : e.ext, e.folder});
    g_cells[P_DUPES].clear(); for (auto& e : g_dupes.files) g_cells[P_DUPES].push_back({e.name, FormatBytes(e.size), FileDateText(e.mtime), L"Group " + std::to_wstring(e.group) + L" \u00B7 " + std::to_wstring(e.groupCount) + L" copies", e.folder});
    g_cells[P_DRIVERS].clear(); for (auto& e : g_dv.entries) g_cells[P_DRIVERS].push_back({e.display, e.name, e.state, e.start, e.safety, e.ptype, e.description, e.path, e.warnings});
}
static bool PassFilter(Page p, int idx) {
    int f = g_filter[p];
    if (p == P_LARGE) { int c = ExtCat(g_large.files[idx].ext); return f == 0 || c == f; }
    if (p == P_DUPES) return true;
    if (p == P_CLEAN) { auto& e = g_cl.items[idx]; return f == 0 || (f == 1 && !IsRegItem(e)) || (f == 2 && IsRegItem(e)); }
    if (p == P_EMPTY) return true;
    if (p == P_STARTUP) { auto& e = g_su.entries[idx];
        if (Cfg().hideSystem && e.isSystem && f != 11) return false;
        switch (f) { case 0: return true; case 1: return e.state == L"Enabled"; case 2: return e.state == L"Disabled"; case 3: return e.hasWarning; case 4: return e.recent; case 5: return !e.microsoft;
            case 6: return e.type.rfind(L"Registry", 0) == 0; case 7: return e.type == L"Startup folder"; case 8: return e.type.rfind(L"Scheduled task", 0) == 0; case 9: return e.type.rfind(L"Explorer", 0) == 0; case 10: return e.managedDelay; case 11: return e.isSystem; } }
    else if (p == P_SERVICES) { auto& e = g_sv.entries[idx];
        switch (f) { case 1: return e.stateCode == SERVICE_RUNNING; case 2: return e.stateCode == SERVICE_STOPPED; case 3: return e.startType == SERVICE_AUTO_START && e.stateCode != SERVICE_RUNNING;
            case 4: return e.startType == SERVICE_DISABLED; case 5: return e.safety == L"Third-party"; case 6: return e.critical; case 7: return !e.warnings.empty(); } }
    else if (p == P_DRIVERS) { auto& e = g_dv.entries[idx];
        switch (f) { case 1: return e.stateCode == SERVICE_RUNNING; case 2: return e.stateCode == SERVICE_STOPPED; case 3: return e.startType == SERVICE_BOOT_START || e.startType == SERVICE_SYSTEM_START; case 4: return e.safety == L"Third-party"; case 5: return !e.warnings.empty(); } }
    return true;
}
static wstring KeyOf(Page p, int idx) { return p == P_STARTUP ? g_su.entries[idx].key : p == P_HISTORY ? L"" : p == P_CLEAN ? g_cl.items[idx].id : IsFilePage(p) ? FS(p).files[idx].path : SvcVec(p)[idx].name; }
static void UpdateStatus(); static void UpdateToolbar();
static void ApplyView() {
    Page p = g_page; if (!IsActionList(p) && p != P_HISTORY) return;
    g_view.clear(); auto& cells = g_cells[p];
    for (size_t i = 0; i < cells.size(); ++i) if (PassFilter(p, (int)i) && MatchesQuery(cells[i], g_query[p])) g_view.push_back((int)i);
    int col = g_sortCol[p]; bool asc = g_sortAsc[p]; bool statusCol = ((p == P_STARTUP && col == 1) || (IsSvcPage(p) && col == 2)); bool numCol = p == P_SERVICES && col == 5; bool sizeCol = p == P_CLEAN && col == 3; bool fsSize = IsFilePage(p) && p != P_EMPTY && col == 1, fsDate = IsFilePage(p) && col == 2, grpCol = p == P_DUPES && col == 3, emptyCnt = p == P_EMPTY && col == 1;
    if (p == P_HISTORY && col == 0) { if (!asc) {} }   // journal is stored oldest->newest; cells are newest-first already
    std::stable_sort(g_view.begin(), g_view.end(), [&](int a, int b) {
        const wstring &x = cells[a][col], &y = cells[b][col];
        if (x.empty() != y.empty()) return !x.empty();
        int c;
        if (statusCol) c = Rank(p, x) - Rank(p, y);
        else if (numCol) c = _wtoi(x.c_str()) - _wtoi(y.c_str());
        else if (emptyCnt) c = FS(p).files[a].groupCount - FS(p).files[b].groupCount;
        else if (fsSize) { ULONGLONG bx = FS(p).files[a].size, by = FS(p).files[b].size; c = bx < by ? -1 : bx > by ? 1 : 0; }
        else if (fsDate) { ULONGLONG bx = FS(p).files[a].mtime, by = FS(p).files[b].mtime; c = bx < by ? -1 : bx > by ? 1 : 0; }
        else if (grpCol) c = FS(p).files[a].group - FS(p).files[b].group;
        else if (sizeCol) { ULONGLONG bx = g_cl.items[a].bytes, by = g_cl.items[b].bytes; c = bx < by ? -1 : bx > by ? 1 : 0; }
        else { wstring lx = Lower(x), ly = Lower(y); c = lx < ly ? -1 : lx > ly ? 1 : 0; }
        if (!c && col != 0) { wstring lx = Lower(cells[a][0]), ly = Lower(cells[b][0]); return lx < ly; }
        return asc ? c < 0 : c > 0; });
    ListView_SetItemCountEx(g_list, (int)g_view.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    if (!g_keepSel.empty() && p != P_HISTORY) { ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED);
        for (size_t i = 0; i < g_view.size(); ++i) if (g_keepSel.count(KeyOf(p, g_view[i]))) ListView_SetItemState(g_list, (int)i, LVIS_SELECTED, LVIS_SELECTED); g_keepSel.clear(); }
    InvalidateRect(g_list, nullptr, TRUE); InvalidateRect(g_hdr, nullptr, TRUE); UpdateStatus(); UpdateToolbar();
}
static void SaveColumnWidths() {
    if (!IsActionList(g_page) && g_page != P_HISTORY) return;
    int n = Header_GetItemCount(g_hdr); std::vector<int> w; for (int i = 0; i < n; ++i) w.push_back(MulDiv(ListView_GetColumnWidth(g_list, i), 96, g_dpi)); Cfg().colw[kPageNames[g_page]] = w;
}
static void BuildColumns() {
    while (ListView_DeleteColumn(g_list, 0)) {}
    int n; const Col* cs = ColsFor(g_page, n); auto it = Cfg().colw.find(kPageNames[g_page]); bool saved = it != Cfg().colw.end() && (int)it->second.size() == n;
    for (int i = 0; i < n; ++i) { LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; c.pszText = (LPWSTR)cs[i].name; c.cx = S(saved ? it->second[i] : cs[i].w); c.fmt = cs[i].right ? LVCFMT_RIGHT : LVCFMT_LEFT; SendMessageW(g_list, LVM_INSERTCOLUMNW, i, (LPARAM)&c); }
}
static void ApplyDensity() {
    static HIMAGELIST il = nullptr; if (il) ImageList_Destroy(il);
    il = ImageList_Create(1, S(Cfg().density ? 24 : 19), ILC_COLOR32, 1, 1); HBITMAP bm = CreateBitmap(1, 1, 1, 32, nullptr); (void)bm; DeleteObject(bm);
    ListView_SetImageList(g_list, il, LVSIL_SMALL); InvalidateRect(g_list, nullptr, TRUE);
}

static void UpdateStatus() {
    wstring s; Page p = g_page;
    if (p == P_STARTUP) {
        size_t en = 0, di = 0; for (auto& e : g_su.entries) { if (e.state == L"Enabled") ++en; else if (e.state == L"Disabled") ++di; }
        s = std::to_wstring(g_su.entries.size()) + L" entries · " + std::to_wstring(en) + L" enabled · " + std::to_wstring(di) + L" disabled · " + std::to_wstring(g_view.size()) + L" shown";
    } else if (IsSvcPage(p)) {
        auto& v = SvcVec(p); size_t run = 0; for (auto& e : v) if (e.stateCode == SERVICE_RUNNING) ++run;
        s = std::to_wstring(v.size()) + (p == P_DRIVERS ? L" drivers · " : L" services · ") + std::to_wstring(run) + L" running"; if (g_view.size() != v.size()) s += L" · " + std::to_wstring(g_view.size()) + L" shown";
    } else if (IsFilePage(p)) {
        bool done = p == P_DUPES ? g_dupesDone : p == P_EMPTY ? g_emptyDone : g_largeDone; if (g_fsBusy) s = L"Scanning\u2026 press Stop to cancel"; else if (!done) s = p == P_DUPES ? L"Not scanned yet \u00B7 press Scan to look for duplicate files" : p == P_EMPTY ? L"Not scanned yet \u00B7 press Scan to find empty folders" : L"Not scanned yet \u00B7 press Scan to find large files";
        else if (p == P_EMPTY) s = std::to_wstring(g_empty.files.size()) + L" empty folders";
        else if (p == P_LARGE) s = std::to_wstring(g_large.files.size()) + L" large files \u00B7 " + FormatBytes(g_large.totalBytes) + L" total"; else s = std::to_wstring(g_dupes.groups) + L" duplicate groups \u00B7 " + std::to_wstring(g_dupes.files.size()) + L" files \u00B7 " + FormatBytes(g_dupes.reclaimable) + L" reclaimable";
        if (g_view.size() != FS(p).files.size() && done) s += L" \u00B7 " + std::to_wstring(g_view.size()) + L" shown";
    } else if (p == P_CLEAN) { s = std::to_wstring(g_cl.items.size()) + L" items \u00B7 " + FormatBytes(g_cl.totalBytes) + L" reclaimable \u00B7 " + std::to_wstring(g_cl.registryCount) + L" invalid registry entries"; if (g_view.size() != g_cl.items.size()) s += L" \u00B7 " + std::to_wstring(g_view.size()) + L" shown";
    } else if (p == P_HISTORY) s = std::to_wstring(g_cells[P_HISTORY].size()) + L" journal entries" + (g_view.size() != g_cells[P_HISTORY].size() ? L" · " + std::to_wstring(g_view.size()) + L" shown" : L"");
    else s = g_scanning ? L"Scanning…" : L"Ready";
    UINT sel = IsActionList(p) ? ListView_GetSelectedCount(g_list) : 0; if (sel > 1) s += L" · " + std::to_wstring(sel) + L" selected";
    if (g_scanning && IsActionList(p) && !IsFilePage(p)) s += L" · scanning…";
    SetWindowTextW(g_status, s.c_str());
}

// ================= toolbar content per page =================
static void BuildToolbar() {
    std::vector<TbItem> it;
    auto add = [&](int id, const wchar_t* t, bool dd = false, bool right = false) { TbItem x; x.id = id; x.text = t; x.dropdown = dd; x.right = right; it.push_back(x); };
    auto sep = [&]() { TbItem x; x.id = 0; x.sep = true; it.push_back(x); };
    if (g_page == P_STARTUP) { add(C_REFRESH, L"Refresh"); sep(); add(C_ENABLE, L"Enable"); add(C_DISABLE, L"Disable"); add(C_DELETE, L"Delete"); add(C_STARTLATER, L"Start later", true); sep(); add(C_ADD, L"Add…"); add(C_RESTORE, L"Restore…"); add(C_UNDO, L"Undo"); }
    else if (g_page == P_SERVICES) { add(C_REFRESH, L"Refresh"); sep(); add(C_SVC_START, L"Start"); add(C_SVC_STOP, L"Stop"); add(C_SVC_RESTART, L"Restart"); add(C_SVC_PAUSE, L"Pause"); add(C_SVC_RESUME, L"Resume"); add(C_SVC_KILL, L"Kill"); sep(); add(C_STARTTYPE, L"Start type", true); add(C_PROFILES, L"Profiles", true); add(C_UNDO, L"Undo"); }
    else if (g_page == P_DRIVERS) { add(C_REFRESH, L"Refresh"); add(C_DETAILS, L"Properties"); }
    else if (IsFilePage(g_page)) { add(C_FS_SCAN, L"Scan"); add(C_FS_STOP, L"Stop"); sep(); add(C_FS_FOLDERS, L"Folders\u2026", true); if (g_page != P_EMPTY) add(C_FS_MIN, L"Minimum size", true); sep(); if (g_page == P_DUPES) add(C_FS_AUTO, L"Auto-select", true); add(C_FS_RECYCLE, L"Move to Recycle Bin"); add(C_OPENLOC, L"Open location"); }
    else if (g_page == P_CLEAN) { add(C_REFRESH, L"Scan again"); sep(); add(C_CLEAN, L"Clean selected"); add(C_CLEAN_SELALL, L"Select all"); sep(); add(C_CLEAN_RESTORE, L"Restore registry\u2026", true); }
    else if (g_page == P_HISTORY) { add(C_REFRESH, L"Refresh"); add(C_EXPORT, L"Export…"); }
    else add(C_REFRESH, L"Refresh");
    if (FilterCount(g_page)) add(C_FILTER, FilterName(g_page), true, true);
    add(C_MORE, L"More", false, true);
    TbSet(std::move(it));
}
static void UpdateToolbar() {
    auto sel = Selected(); bool any = !sel.empty();
    if (g_page == P_STARTUP) {
        bool canEn = false, canDis = false, canDel = false, canDelay = false, canUndelay = false;
        for (int i : sel) { auto& e = g_su.entries[i]; if (e.toggle != Toggle::None) { if (e.state == L"Disabled") canEn = true; if (e.state == L"Enabled") canDis = true; }
            if (e.type == L"Registry" || e.type == L"Registry (RunOnce)") canDel = true; if (CanDelay(e)) canDelay = true; if (e.managedDelay) canUndelay = true; }
        TbEnable(C_ENABLE, canEn); TbEnable(C_DISABLE, canDis); TbEnable(C_DELETE, canDel); TbEnable(C_STARTLATER, canDelay || canUndelay); TbEnable(C_ADD, true); TbEnable(C_RESTORE, true);
    } else if (g_page == P_SERVICES) {
        bool st = false, sp = false, rs = false, pa = false, re = false, kl = false;
        for (int i : sel) { auto& e = g_sv.entries[i]; if (e.stateCode == SERVICE_STOPPED) st = true; if (e.stateCode == SERVICE_RUNNING) { rs = true; if (e.controls & SERVICE_ACCEPT_STOP) sp = true; if (e.controls & SERVICE_ACCEPT_PAUSE_CONTINUE) pa = true; if (sel.size() == 1 && e.pid && (e.svcType & SERVICE_WIN32_OWN_PROCESS) && !e.critical) kl = true; } if (e.stateCode == SERVICE_PAUSED) re = true; }
        TbEnable(C_SVC_START, st); TbEnable(C_SVC_STOP, sp); TbEnable(C_SVC_RESTART, sp && rs); TbEnable(C_SVC_PAUSE, pa); TbEnable(C_SVC_RESUME, re); TbEnable(C_SVC_KILL, kl); TbEnable(C_STARTTYPE, any); TbEnable(C_PROFILES, true);
    } else if (g_page == P_DRIVERS) TbEnable(C_DETAILS, any);
    else if (IsFilePage(g_page)) { TbEnable(C_FS_SCAN, !g_fsBusy); TbEnable(C_FS_STOP, g_fsBusy); TbEnable(C_FS_FOLDERS, !g_fsBusy); TbEnable(C_FS_MIN, !g_fsBusy); TbEnable(C_FS_AUTO, !g_fsBusy && !g_dupes.files.empty()); TbEnable(C_FS_RECYCLE, any && !g_fsBusy); TbEnable(C_OPENLOC, sel.size() == 1); }
    else if (g_page == P_CLEAN) { TbEnable(C_CLEAN, any && !g_scanning); TbEnable(C_CLEAN_SELALL, !g_cl.items.empty()); TbEnable(C_CLEAN_RESTORE, true); }
    TbEnable(C_UNDO, !g_undo.empty());
}

// ================= scan =================
static wstring JGet(const std::string& line, const char* key) {
    std::string k = std::string("\"") + key + "\":\""; size_t p = line.find(k); if (p == std::string::npos) return L""; p += k.size(); std::string o;
    for (; p < line.size() && line[p] != '"'; ++p) { if (line[p] == '\\' && p + 1 < line.size()) { ++p; o += line[p] == 'n' ? ' ' : line[p]; } else o += line[p]; }
    return FromUtf8(o);
}
static void ReadHistory() {
    auto& rows = g_cells[P_HISTORY]; rows.clear(); std::ifstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"), std::ios::binary); std::string line; std::vector<std::string> lines;
    while (std::getline(f, line)) lines.push_back(line);
    for (auto it = lines.rbegin(); it != lines.rend() && rows.size() < 5000; ++it) { auto t = JGet(*it, "time"); if (t.size() >= 19) t = t.substr(0, 10) + L" " + t.substr(11, 8); rows.push_back({t, JGet(*it, "target"), JGet(*it, "before"), JGet(*it, "after"), JGet(*it, "result")}); }
}
static DWORD WINAPI ScanThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); int gen = g_gen.load();
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, ScanStartup(), {}, {}, {}, 0});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, ScanServices(false), {}, {}, 1});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, {}, ScanServices(true), {}, 2});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, {}, {}, ScanCleaner(Cfg().keepRecent), 3});
    CoUninitialize(); return 0;
}
static void StartScan() {
    if (g_scanning) return;
    for (int i : Selected()) g_keepSel.insert(KeyOf(g_page, i));
    g_scanning = true; ++g_gen; UpdateStatus();
    HANDLE h = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr); if (h) CloseHandle(h); else g_scanning = false;
}

// ================= panel: dashboard + settings (custom, mouse + keyboard) =================
struct Tile { RECT r; wstring big, cap; Page page; int filter; COLORREF col; int action; };
static std::vector<Tile> g_tiles; static int g_panelFocus = -1;
enum { A_NONE, A_THEME, A_DENSITY, A_HIDESYS, A_CONFIRM, A_KEEPRECENT, A_STARTPAGE, A_RESETCOLS, A_OPENDATA, A_CLEARHIST, A_ADMIN };

static size_t CountIf(const std::vector<ServiceEntry>& v, bool (*f)(const ServiceEntry&)) { size_t n = 0; for (auto& e : v) if (f(e)) ++n; return n; }
static void RoundFill(HDC dc, RECT r, COLORREF fill, COLORREF line) { HBRUSH b = CreateSolidBrush(fill); HPEN p = CreatePen(PS_SOLID, 1, line); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p); RoundRect(dc, r.left, r.top, r.right, r.bottom, S(10), S(10)); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p); }
static void BuildTiles(int panelW) {
    g_tiles.clear(); int y = S(104), x0 = S(24), tw = S(168), th = S(80), gap = S(12);
    auto row = [&](std::vector<Tile> ts) { int x = x0; for (auto& t : ts) { if (x + tw > panelW - S(16)) { x = x0; y += th + gap; } t.r = {x, y, x + tw, y + th}; g_tiles.push_back(t); x += tw + gap; } y += th + S(40); };
    size_t dis = 0, warn = 0, recent = 0, delayed = 0; for (auto& e : g_su.entries) { if (e.state == L"Disabled") ++dis; if (e.hasWarning) ++warn; if (e.recent) ++recent; if (e.managedDelay) ++delayed; }
    row({{{}, std::to_wstring(g_su.entries.size()), L"startup entries", P_STARTUP, 0, cText, 0}, {{}, std::to_wstring(dis), L"disabled", P_STARTUP, 2, cDim, 0}, {{}, std::to_wstring(warn), L"need attention", P_STARTUP, 3, warn ? cRed : cGreen, 0},
         {{}, std::to_wstring(recent), L"new in 7 days", P_STARTUP, 4, recent ? cAmber : cDim, 0}, {{}, std::to_wstring(delayed), L"delayed by Feather", P_STARTUP, 10, cAccent, 0}});
    size_t run = CountIf(g_sv.entries, [](const ServiceEntry& e) { return e.stateCode == SERVICE_RUNNING; }), third = CountIf(g_sv.entries, [](const ServiceEntry& e) { return e.safety == L"Third-party"; }),
           crit = CountIf(g_sv.entries, [](const ServiceEntry& e) { return e.critical; }), sw = CountIf(g_sv.entries, [](const ServiceEntry& e) { return !e.warnings.empty(); });
    row({{{}, std::to_wstring(g_sv.entries.size()), L"services", P_SERVICES, 0, cText, 0}, {{}, std::to_wstring(run), L"running", P_SERVICES, 1, cGreen, 0}, {{}, std::to_wstring(third), L"third-party", P_SERVICES, 5, cLav, 0},
         {{}, std::to_wstring(crit), L"critical to Windows", P_SERVICES, 6, cRed, 0}, {{}, std::to_wstring(sw), L"need attention", P_SERVICES, 7, sw ? cRed : cGreen, 0}});
    size_t drun = CountIf(g_dv.entries, [](const ServiceEntry& e) { return e.stateCode == SERVICE_RUNNING; }), dboot = CountIf(g_dv.entries, [](const ServiceEntry& e) { return e.startType == SERVICE_BOOT_START || e.startType == SERVICE_SYSTEM_START; });
    row({{{}, FormatBytes(g_cl.totalBytes), L"reclaimable temp files", P_CLEAN, 1, g_cl.totalBytes ? cAmber : cDim, 0}, {{}, std::to_wstring(g_cl.registryCount), L"invalid registry entries", P_CLEAN, 2, g_cl.registryCount ? cAmber : cGreen, 0}});
    row({{{}, g_largeDone ? std::to_wstring(g_large.files.size()) : wstring(L"\u2014"), g_largeDone ? L"large files found" : L"large files: not scanned", P_LARGE, 0, g_largeDone ? cAmber : cDim, 0}, {{}, g_dupesDone ? std::to_wstring(g_dupes.groups) : wstring(L"\u2014"), g_dupesDone ? L"duplicate groups" : L"duplicates: not scanned", P_DUPES, 0, g_dupesDone && g_dupes.groups ? cAmber : cDim, 0}, {{}, g_emptyDone ? std::to_wstring(g_empty.files.size()) : wstring(L"\u2014"), g_emptyDone ? L"empty folders" : L"empty folders: not scanned", P_EMPTY, 0, g_emptyDone && g_empty.files.size() ? cAmber : cDim, 0}});
    row({{{}, std::to_wstring(g_dv.entries.size()), L"drivers (read-only)", P_DRIVERS, 0, cText, 0}, {{}, std::to_wstring(drun), L"loaded", P_DRIVERS, 1, cGreen, 0}, {{}, std::to_wstring(dboot), L"boot/system start", P_DRIVERS, 3, cAmber, 0}});
}
static const wchar_t* DensityName() { return Cfg().density ? L"Comfortable" : L"Compact"; }
static void BuildSettingsRows(int panelW) {
    g_tiles.clear(); int y = S(86), x = S(24), w = panelW - S(48), h = S(40);
    struct R { const wchar_t* label; wstring value; int action; } rows[] = {
        {L"Theme", PalFor(Cfg().theme).name, A_THEME}, {L"Row density", DensityName(), A_DENSITY}, {L"Hide Windows system items on the Startup page", Cfg().hideSystem ? L"On" : L"Off", A_HIDESYS},
        {L"Confirm before changing several items at once", Cfg().confirmBatch ? L"On" : L"Off", A_CONFIRM}, {L"Cleaner: keep temporary files from the last 24 hours", Cfg().keepRecent ? L"On" : L"Off", A_KEEPRECENT}, {L"Open on page", kPageNames[Cfg().startPage], A_STARTPAGE},
        {L"Column widths", L"Reset", A_RESETCOLS}, {L"Data folder (journal, backups, profiles)", L"Open", A_OPENDATA}, {L"App history and caches (journal, first-seen list)", L"Clear…", A_CLEARHIST}};
    for (auto& r : rows) { Tile t; t.r = {x, y, x + w, y + h}; t.big = r.value; t.cap = r.label; t.page = P_SETTINGS; t.filter = 0; t.col = cAccent; t.action = r.action; g_tiles.push_back(t); y += h + S(6); }
}
static void PanelAction(int a) {
    Settings& c = Cfg(); auto repaint = [&] { InvalidateRect(g_panel, nullptr, TRUE); };
    switch (a) {
    case A_THEME: { HMENU m = CreatePopupMenu(); for (int i = 0; i < ThemeCount(); ++i) AppendMenuW(m, MF_STRING | (i == c.theme ? MF_CHECKED : 0), 1 + i, PalFor(i).name);
        POINT pt; GetCursorPos(&pt); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) { c.theme = id - 1; PostMessageW(g_main, WM_COMMAND, 0xE001, 0); } break; }
    case A_DENSITY: c.density = !c.density; ApplyDensity(); SaveSettings(); repaint(); break;
    case A_HIDESYS: c.hideSystem = !c.hideSystem; SaveSettings(); repaint(); break;
    case A_CONFIRM: c.confirmBatch = !c.confirmBatch; SaveSettings(); repaint(); break;
    case A_KEEPRECENT: c.keepRecent = !c.keepRecent; SaveSettings(); repaint(); break;
    case A_STARTPAGE: { HMENU m = CreatePopupMenu(); for (int i = 0; i < P_COUNT; ++i) AppendMenuW(m, MF_STRING | (i == c.startPage ? MF_CHECKED : 0), 1 + i, kPageNames[i]);
        POINT pt; GetCursorPos(&pt); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) { c.startPage = id - 1; SaveSettings(); repaint(); } break; }
    case A_RESETCOLS: c.colw.clear(); SaveSettings(); SetMsg(L"Column widths reset. They apply the next time you open each page."); break;
    case A_OPENDATA: ShellExecuteW(nullptr, L"open", DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case A_CLEARHIST: if (DarkMsg(g_main, L"Clear app history", L"This deletes the change journal (History page) and the first-seen list. It does NOT touch Windows startup entries or services, and it does not delete restore backups or profiles.\r\n\r\nClear now?", 2) == IDYES) {
            DeleteFileW((DataDir() + L"\\journal.jsonl").c_str()); DeleteFileW((DataDir() + L"\\firstseen.tsv").c_str()); g_cells[P_HISTORY].clear(); SetMsg(L"App history cleared."); StartScan(); } break;
    case A_ADMIN: PostMessageW(g_main, WM_COMMAND, C_ADMIN, 0); break;
    }
}
static void ShowPage(Page p);
static LRESULT CALLBACK PanelProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_brBg); return 1; }
    case WM_SIZE: if (g_page == P_SETTINGS) BuildSettingsRows(LOWORD(l)); else BuildTiles(LOWORD(l)); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); FillRect(dc, &cr, g_brBg); SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_fontTitle); SetTextColor(dc, cText); TextOutW(dc, S(24), S(18), g_page == P_SETTINGS ? L"Settings" : kApp, (int)wcslen(g_page == P_SETTINGS ? L"Settings" : kApp));
        SelectObject(dc, g_font); SetTextColor(dc, cDim);
        if (g_page == P_SETTINGS) { wstring sub = L"Saved automatically in " + DataDir() + L"\\settings.ini"; TextOutW(dc, S(26), S(52), sub.c_str(), (int)sub.size()); }
        else { wstring sub = L"Version " FSU_VERSION L"  ·  Developed by Akshaya Simha  ·  Offline, no telemetry"; TextOutW(dc, S(26), S(52), sub.c_str(), (int)sub.size()); }
        for (size_t i = 0; i < g_tiles.size(); ++i) {
            auto& t = g_tiles[i]; bool foc = (int)i == g_panelFocus && GetFocus() == h;
            if (g_page == P_SETTINGS) {
                RoundFill(dc, t.r, cBar, foc ? cAccent : cGrid); RECT lr = t.r; lr.left += S(14); SetTextColor(dc, cText); DrawTextW(dc, t.cap.c_str(), -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SIZE sz; GetTextExtentPoint32W(dc, t.big.c_str(), (int)t.big.size(), &sz); RECT pill{t.r.right - sz.cx - S(46), t.r.top + S(7), t.r.right - S(12), t.r.bottom - S(7)};
                bool toggle = t.action == A_HIDESYS || t.action == A_CONFIRM || t.action == A_KEEPRECENT; bool on = t.big == L"On"; RoundFill(dc, pill, toggle && on ? cSel : cHot, toggle && on ? cAccent : cGrid);
                SetTextColor(dc, toggle ? (on ? cGreen : cDim) : cAccent); DrawTextW(dc, (t.big + ((t.action == A_THEME || t.action == A_STARTPAGE) ? L" ▾" : L"")).c_str(), -1, &pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            } else {
                RoundFill(dc, t.r, cBar, foc ? cAccent : cGrid); SelectObject(dc, g_fontBig); SetTextColor(dc, t.col); RECT br = t.r; br.top += S(8); br.bottom = br.top + S(40); DrawTextW(dc, t.big.c_str(), -1, &br, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(dc, g_font); SetTextColor(dc, cDim); RECT cp = t.r; cp.top += S(48); DrawTextW(dc, t.cap.c_str(), -1, &cp, DT_CENTER | DT_SINGLELINE);
            }
        }
        SelectObject(dc, g_font);
        if (g_page == P_DASH) {
            // section headings aligned to tile rows
            const wchar_t* heads[] = {L"STARTUP", L"SERVICES", L"CLEANER", L"FILES", L"DRIVERS"}; int hi = 0, lastY = -1; SetTextColor(dc, cAccent);
            for (auto& t : g_tiles) if (t.r.top != lastY) { lastY = t.r.top; if (hi < 5) TextOutW(dc, S(26), t.r.top - S(22), heads[hi], (int)wcslen(heads[hi])); ++hi; }
            int y = g_tiles.empty() ? S(100) : g_tiles.back().r.bottom + S(20); RECT tr{S(26), y, cr.right - S(26), cr.bottom - S(8)}; SetTextColor(dc, cDim);
            wstring note = (g_elevated ? L"Running as administrator.\r\n" : L"Running with standard rights: reading works everywhere; all-users and service changes need \"Restart as administrator\" (bottom right).\r\n");
            note += L"Every change is journaled first, verified by reading it back, and can be undone where Windows allows (Ctrl+Z - configuration only).\r\n";
            note += g_su.coverage.empty() ? L"" : L"Startup: " + g_su.coverage + L"\r\n"; note += g_sv.coverage.empty() ? L"" : L"Services: " + g_sv.coverage + L"\r\n"; note += MigrationNote();
            DrawTextW(dc, note.c_str(), -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        } else {
            int y = g_tiles.empty() ? S(100) : g_tiles.back().r.bottom + S(16); RECT tr{S(26), y, cr.right - S(26), cr.bottom - S(8)}; SetTextColor(dc, cDim);
            wstring note = L"Privacy: nothing leaves this PC. No telemetry, no automatic updates, no online advice. \"Check on VirusTotal\" and \"Search online\" only open your browser after you confirm.\r\n" + MigrationNote() +
                           L"\r\nShortcuts: Ctrl+F search · F5 refresh · Enter details · Del delete · Ctrl+A select all · Ctrl+C copy name · Ctrl+Shift+C copy details · Ctrl+E export · Ctrl+Z undo · Ctrl+1..0 pages";
            DrawTextW(dc, note.c_str(), -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        }
        EndPaint(h, &ps); return 0; }
    case WM_LBUTTONUP: { POINT p{(short)LOWORD(l), (short)HIWORD(l)}; SetFocus(h);
        for (size_t i = 0; i < g_tiles.size(); ++i) if (PtInRect(&g_tiles[i].r, p)) { g_panelFocus = (int)i; auto t = g_tiles[i]; if (g_page == P_SETTINGS) PanelAction(t.action); else { g_filter[t.page] = t.filter; ShowPage(t.page); } break; } return 0; }
    case WM_SETCURSOR: { POINT p; GetCursorPos(&p); ScreenToClient(h, &p); for (auto& t : g_tiles) if (PtInRect(&t.r, p)) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; } break; }
    case WM_SETFOCUS: case WM_KILLFOCUS: if (g_panelFocus < 0 && !g_tiles.empty()) g_panelFocus = 0; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_KEYDOWN: { int n = (int)g_tiles.size(); if (!n) break;
        if (w == VK_RIGHT || w == VK_DOWN) { g_panelFocus = (g_panelFocus + 1) % n; InvalidateRect(h, nullptr, FALSE); return 0; }
        if (w == VK_LEFT || w == VK_UP) { g_panelFocus = (g_panelFocus + n - 1) % n; InvalidateRect(h, nullptr, FALSE); return 0; }
        if ((w == VK_RETURN || w == VK_SPACE) && g_panelFocus >= 0) { auto t = g_tiles[g_panelFocus]; if (g_page == P_SETTINGS) PanelAction(t.action); else { g_filter[t.page] = t.filter; ShowPage(t.page); } return 0; }
        break; }
    }
    return DefWindowProcW(h, m, w, l);
}

// ================= pages =================
static void RebuildPanel() {
    RECT r; GetClientRect(g_panel, &r); if (g_page == P_SETTINGS) BuildSettingsRows(r.right); else BuildTiles(r.right); g_panelFocus = -1; InvalidateRect(g_panel, nullptr, TRUE);
}
static void Layout(int w, int h);
static void ShowPage(Page p) {
    SaveColumnWidths(); g_page = p; SendMessageW(g_nav, LB_SETCURSEL, p, 0); InvalidateRect(g_nav, nullptr, TRUE);
    bool listy = IsActionList(p) || p == P_HISTORY;
    ShowWindow(g_list, listy ? SW_SHOW : SW_HIDE); ShowWindow(g_panel, listy ? SW_HIDE : SW_SHOW); ShowWindow(g_search, listy ? SW_SHOW : SW_HIDE);
    wstring title = wstring(kApp) + L"  —  " + kPageNames[p] + (g_elevated ? L" (Administrator)" : L""); SetWindowTextW(g_main, title.c_str());
    BuildToolbar();
    if (listy) {
        SetWindowTextW(g_search, g_query[p].c_str());
        SendMessageW(g_search, EM_SETCUEBANNER, TRUE, (LPARAM)(p == P_SERVICES ? L"Search services (Ctrl+F)" : p == P_DRIVERS ? L"Search drivers (Ctrl+F)" : p == P_CLEAN ? L"Search items (Ctrl+F)" : IsFilePage(p) ? (p == P_EMPTY ? L"Search folders (Ctrl+F)" : L"Search files (Ctrl+F)") : p == P_HISTORY ? L"Search history (Ctrl+F)" : L"Search (Ctrl+F)"));
        BuildColumns(); if (p == P_HISTORY) ReadHistory(); ApplyView(); InvalidateRect(g_search, nullptr, TRUE);
    } else { RebuildPanel(); UpdateStatus(); UpdateToolbar(); }
    RECT r; GetClientRect(g_main, &r); Layout(r.right, r.bottom);
}

// ================= actions =================
static const StartupEntry* OneStartup() { if (g_page != P_STARTUP) return nullptr; auto s = Selected(); return s.empty() ? nullptr : &g_su.entries[s[0]]; }
static const ServiceEntry* OneService() { if (!IsSvcPage(g_page)) return nullptr; auto s = Selected(); return s.empty() ? nullptr : &SvcVec(g_page)[s[0]]; }

static void ServiceEditors(std::vector<ReportBtn>& b, const ServiceEntry& c) {
    b.push_back({L"Name & description…", [c](const wstring&) {
        std::vector<FormField> f = {{L"Display name", c.display, 0, {}}, {L"Description", c.description, 0, {}}}; HWND ow = GetActiveWindow();
        if (!FormDialog(ow, L"Change name and description", L"Save", f, L"Only the readable name and description change; the service's identity (its service name) stays the same.")) return;
        wstring err; if (EditServiceNames(c, f[0].value, f[1].value, err)) { SetMsg(L"Saved name/description for " + f[0].value + L"."); StartScan(); } else DarkMsg(ow, kApp, err, 1); }});
    b.push_back({L"Recovery…", [c](const wstring&) {
        Recovery r; GetRecovery(c.name, r); const std::vector<wstring> opts = {L"Take no action", L"Restart the service", L"Run a program"}; auto idx = [](int a) { return a == 1 ? 1 : a == 3 ? 2 : 0; };
        std::vector<FormField> f = {{L"First failure", std::to_wstring(idx(r.act[0])), 1, opts}, {L"Second failure", std::to_wstring(idx(r.act[1])), 1, opts}, {L"Later failures", std::to_wstring(idx(r.act[2])), 1, opts},
            {L"Restart the service after (minutes)", std::to_wstring(r.delayMin[0]), 0, {}}, {L"Reset the failure count after (days)", std::to_wstring(r.resetDays), 0, {}}, {L"Program to run (for \"Run a program\")", r.cmd, 0, {}},
            {L"Also act when the service stops with an error (not only when it crashes)", r.onNonCrash ? L"1" : L"0", 2, {}}};
        HWND ow = GetActiveWindow(); if (!FormDialog(ow, L"Recovery - " + c.display, L"Save", f, L"What Windows does when the service fails. A typical setting: restart, restart, take no action. 'Restart the computer' is deliberately not offered.")) return;
        static const int map[] = {0, 1, 3}; Recovery n; for (int i = 0; i < 3; ++i) { n.act[i] = map[_wtoi(f[i].value.c_str()) % 3]; n.delayMin[i] = (DWORD)std::max(0, std::min(1440, _wtoi(f[3].value.c_str()))); }
        n.resetDays = (DWORD)std::max(0, std::min(365, _wtoi(f[4].value.c_str()))); n.cmd = f[5].value; n.onNonCrash = f[6].value == L"1";
        wstring err; if (SetRecovery(c.name, n, err)) SetMsg(L"Recovery settings saved for " + c.display + L"."); else DarkMsg(ow, kApp, err, 1); }});
    b.push_back({L"Log-on account…", [c](const wstring&) {
        std::vector<FormField> f = {{L"Run this service as", L"0", 1, {L"Local System (full control of the PC)", L"Local Service (limited rights)", L"Network Service (limited rights, can use the network)"}}};
        HWND ow = GetActiveWindow(); if (!FormDialog(ow, L"Log-on account - " + c.display, L"Change", f, L"Currently: " + c.account + L". Takes effect the next time the service starts. Custom accounts and passwords are not supported here, and this change cannot be undone automatically.")) return;
        if (DarkMsg(ow, kApp, L"Change the account for '" + c.display + L"'?", 2) != IDYES) return;
        wstring err; if (SetServiceAccount(c, _wtoi(f[0].value.c_str()), err)) { SetMsg(L"Account changed for " + c.display + L" (applies at next start)."); StartScan(); } else DarkMsg(ow, kApp, err, 1); }});
}
static void ShowFileDetails();
static void ShowDetails() {
    if (IsFilePage(g_page)) { ShowFileDetails(); return; }
    if (g_page == P_CLEAN) { auto s = Selected(); if (s.empty()) return; auto& e = g_cl.items[s[0]];
        wstring t = e.name + L"\r\n\r\nTYPE\r\n" + e.type + L" \u00B7 " + RiskName(e.risk) + L" risk\r\n\r\nSIZE\r\n" + FormatBytes(e.bytes) + (e.files ? L" in " + std::to_wstring(e.files) + L" files/items" : L"") + L"\r\n\r\nWHERE\r\n" + e.location + L"\r\n\r\nWHY IT IS LISTED\r\n" + e.why + L"\r\n\r\n" +
            (IsRegItem(e) ? L"WHAT HAPPENS\r\nThe entry is exported to a backup file, then removed. Use Restore registry... to put it back. Only entries whose file is missing on a fixed local drive are ever listed.\r\n" : L"WHAT HAPPENS\r\nFiles are deleted permanently (not moved to the Recycle Bin). Files in use are skipped.\r\n") + (e.needsAdmin ? L"\r\nNeeds administrator rights.\r\n" : L"");
        ShowReport(g_main, e.name + L" \u2014 Details", {{L"Details", t}}, {{L"Copy", [](const wstring& x) { SetClip(x); }}, {L"Close", nullptr}}); return; }
    if (g_page == P_STARTUP) {
        auto* e = OneStartup(); if (!e) return; StartupEntry c = *e; HWND owner = g_main;
        ShowReport(owner, c.name + L" — Details", {{L"Details", StartupDetailsText(c)}}, {
            {L"Open file location", [c](const wstring&) { OpenFileLocation(c.program); }}, {L"Show where it is set", [c](const wstring&) { ShowInRegistryEditor(c.location); }},
            {L"Check on VirusTotal", [c](const wstring&) { OnlineLookup(GetActiveWindow(), true, c.program, c.name); }}, {L"Search online", [c](const wstring&) { OnlineLookup(GetActiveWindow(), false, c.program, c.name); }},
            {L"Copy", [](const wstring& t) { SetClip(t); }}, {L"Close", nullptr}});
    } else if (IsSvcPage(g_page)) {
        auto* e = OneService(); if (!e) return; ServiceEntry c = *e; HWND owner = g_main;
        std::vector<ReportTab> tabs = {{L"General", ServiceTabText(c, 0)}, {L"Configuration", ServiceTabText(c, 1)}, {L"Recovery", ServiceTabText(c, 2)}, {L"Dependencies", ServiceTabText(c, 3)}, {L"History", ServiceTabText(c, 4)}};
        std::vector<ReportBtn> b; b.push_back({L"Open file location", [c](const wstring&) { OpenFileLocation(c.program); }});
        if (!c.isDriver) ServiceEditors(b, c);
        b.push_back({L"Check on VirusTotal", [c](const wstring&) { OnlineLookup(GetActiveWindow(), true, c.program, c.display); }}); b.push_back({L"Search online", [c](const wstring&) { OnlineLookup(GetActiveWindow(), false, c.program, c.display); }});
        b.push_back({L"Copy", [](const wstring& t) { SetClip(t); }}); b.push_back({L"Close", nullptr});
        ShowReport(owner, c.display + (c.isDriver ? L" — Driver (read-only)" : L" — Properties"), tabs, b);
    }
}
static void CopyNames() { wstring o; for (int i : Selected()) o += EntryName(g_page, i) + L"\r\n"; SetClip(o); }
static void CopyDetails() { wstring o; for (int i : Selected()) o += (g_page == P_STARTUP ? StartupDetailsText(g_su.entries[i]) : IsFilePage(g_page) ? FS(g_page).files[i].path + L"  " + FormatBytes(FS(g_page).files[i].size) : g_page == P_CLEAN ? g_cl.items[i].name + L"\r\n" + g_cl.items[i].location + L"\r\n" + g_cl.items[i].why : ServiceTabText(SvcVec(g_page)[i], 0)) + L"\r\n"; SetClip(o); }
static void SelectAll() { ListView_SetItemState(g_list, -1, LVIS_SELECTED, LVIS_SELECTED); UpdateStatus(); UpdateToolbar(); }
static void ExportList() {
    if (!IsActionList(g_page) && g_page != P_HISTORY) return;
    int n; const Col* cs = ColsFor(g_page, n); std::vector<wstring> cols; for (int i = 0; i < n; ++i) cols.push_back(cs[i].name);
    std::vector<std::vector<wstring>> rows; for (int v : g_view) rows.push_back(g_cells[g_page][v]);
    wstring prov = wstring(kApp) + L" " FSU_VERSION L" | machine: local | user: " + g_user + L" | page: " + kPageNames[g_page] + (FilterCount(g_page) ? L" | filter: " + wstring(FilterName(g_page)) : L"") +
                   (g_query[g_page].empty() ? L"" : L" | search: " + g_query[g_page]) + L" | rows: " + std::to_wstring(rows.size()) + L" | exported " + NowIsoUtc();
    ExportTable(g_main, kPageNames[g_page], cols, rows, prov);
}

static void ToggleStartup(bool enable) {
    auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) { auto& e = g_su.entries[i]; if (e.toggle != Toggle::None && e.state == (enable ? L"Disabled" : L"Enabled")) { todo.push_back(i); names.push_back(e.name + L"  (" + e.type + L")"); } }
    if (todo.empty()) return;
    if (todo.size() > 1 && Cfg().confirmBatch && !Confirm((enable ? L"Enable these " : L"Disable these ") + std::to_wstring(todo.size()) + L" startup entries?", names)) return;
    int ok = 0, bad = 0; wstring firstErr, firstName; int grp = ++g_groupSeq;
    for (int i : todo) { wstring err; StartupEntry e = g_su.entries[i];
        if (SetStartupEnabled(e, enable, err)) { ++ok; g_undo.push_back({1, e, !enable, L"", 0, false, grp}); } else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = e.name; } } }
    wstring msg = ok ? (ok == 1 ? g_su.entries[todo[0]].name : std::to_wstring(ok) + L" entries") + (enable ? L" enabled." : L" disabled.") : L"";
    if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
    SetMsg(msg); StartScan();
}
static void DeleteStartup() {
    auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) { auto& e = g_su.entries[i]; if (e.type == L"Registry" || e.type == L"Registry (RunOnce)") { todo.push_back(i); names.push_back(e.name + L"  —  " + e.command); } }
    if (todo.empty()) return;
    if (!Confirm(L"Delete " + Plural(todo.size(), L"startup entry", L"startup entries") + L"? Only the startup registration is removed (the program is not uninstalled). A backup is saved and you can restore it.", names)) return;
    int ok = 0, bad = 0; wstring firstErr, firstName;
    for (int i : todo) { wstring err; if (DeleteStartupEntry(g_su.entries[i], err)) ++ok; else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = g_su.entries[i].name; } } }
    wstring msg = ok ? std::to_wstring(ok) + L" deleted (backup saved; use Restore…)." : L""; if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
    SetMsg(msg); StartScan();
}
static void AddEntry() {
    std::vector<FormField> f = {{L"Name", L"", 0, {}}, {L"Command (full path; use quotes if it contains spaces)", L"", 0, {}}, {L"For", L"0", 1, {L"Current user", L"All users (needs administrator rights)"}}};
    if (!FormDialog(g_main, L"Add a startup entry", L"Add", f, L"Adds a Run entry. Nothing is started now; it runs at the next sign-in.")) return;
    wstring err; if (AddStartupEntry(f[0].value, f[1].value, f[2].value == L"1", err)) { SetMsg(L"Added \"" + f[0].value + L"\"."); StartScan(); } else Warn(err);
}
static void DelayEntries(int seconds) {
    auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) if (CanDelay(g_su.entries[i])) { todo.push_back(i); names.push_back(g_su.entries[i].name); }
    if (todo.empty()) { Warn(L"Only enabled current-user Run entries can be delayed."); return; }
    if (!Confirm(L"Start these " + Plural(todo.size(), L"entry", L"entries") + L" " + std::to_wstring(seconds) + L" seconds after sign-in? A per-user scheduled task is created and the original entry is disabled (not deleted). \"Remove delay\" restores it.", names)) return;
    int ok = 0, bad = 0; wstring firstErr, firstName;
    for (int i : todo) { wstring err; if (DelayStartup(g_su.entries[i], seconds, err)) ++ok; else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = g_su.entries[i].name; } } }
    wstring msg = ok ? std::to_wstring(ok) + L" delayed by " + std::to_wstring(seconds) + L" s." : L""; if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
    SetMsg(msg); StartScan();
}
static void RemoveDelays() {
    auto sel = Selected(); int ok = 0, bad = 0; wstring firstErr;
    for (int i : sel) if (g_su.entries[i].managedDelay) { wstring err; if (RemoveDelay(g_su.entries[i], err)) ++ok; else { ++bad; if (firstErr.empty()) firstErr = err; } }
    if (!ok && !bad) { Warn(L"Select an entry that was delayed by Feather System Utilities."); return; }
    SetMsg(std::to_wstring(ok) + L" delay(s) removed; originals restored." + (bad ? L" " + std::to_wstring(bad) + L" failed — " + firstErr : L"")); StartScan();
}
static void RestoreMenu(POINT pt) {
    auto b = ListBackups(); HMENU m = CreatePopupMenu();
    if (b.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 1, L"No deleted entries to restore"); else for (size_t i = 0; i < b.size(); ++i) AppendMenuW(m, MF_STRING, C_RESTORE_BASE + i, b[i].label.c_str());
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m);
    if (id >= C_RESTORE_BASE && id < C_RESTORE_BASE + (int)b.size()) {
        wstring err, name; if (DarkMsg(g_main, kApp, L"Restore this entry?\r\n\r\n" + b[id - C_RESTORE_BASE].label, 2) != IDYES) return;
        if (RestoreBackup(b[id - C_RESTORE_BASE].path, err, name)) { SetMsg(L"Restored \"" + name + L"\"."); StartScan(); } else Warn(err);
    }
}

// ---- services ----
struct SvcJob { std::vector<ServiceEntry> items; SvcOp op; };
static DWORD WINAPI SvcJobThread(LPVOID p) {
    std::unique_ptr<SvcJob> j((SvcJob*)p); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::map<wstring, std::vector<wstring>> deps; std::map<wstring, const ServiceEntry*> by; std::vector<wstring> names;
    for (auto& s : j->items) { by[s.name] = &s; names.push_back(s.name); }
    for (auto& s : j->items) { auto& v = deps[s.name]; for (auto& d : s.deps) if (by.count(d)) v.push_back(d); }
    OrderResult o = (j->op == SvcOp::Start) ? StartOrder(names, deps) : (j->op == SvcOp::Stop || j->op == SvcOp::Restart) ? StopOrder(names, deps) : OrderResult();
    std::vector<wstring> order = (o.ok && !o.order.empty()) ? o.order : names; std::set<wstring> sel(names.begin(), names.end());
    int ok = 0, bad = 0; wstring firstErr, firstName;
    for (auto& n : order) { if (!sel.count(n)) continue; wstring err; if (ServiceOp(*by[n], j->op, err)) ++ok; else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = by[n]->display; } } }
    static const wchar_t* pasts[] = {L"started", L"stopped", L"restarted", L"paused", L"resumed"};
    wstring msg = ok ? (ok == 1 && j->items.size() == 1 ? j->items[0].display : std::to_wstring(ok) + L" services") + L" " + pasts[(int)j->op] + L"." : L"";
    if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
    PostMessageW(g_main, WM_JOB_DONE, 0, (LPARAM) new JobDone{msg}); CoUninitialize(); return 0;
}
static void RunSvcOp(SvcOp op) {
    if (g_page != P_SERVICES) return; auto sel = Selected(); auto* job = new SvcJob{{}, op}; std::vector<wstring> names, extra;
    for (int i : sel) { auto& e = g_sv.entries[i];
        bool elig = op == SvcOp::Start ? e.stateCode == SERVICE_STOPPED : op == SvcOp::Resume ? e.stateCode == SERVICE_PAUSED : e.stateCode == SERVICE_RUNNING && ((op == SvcOp::Pause) ? (e.controls & SERVICE_ACCEPT_PAUSE_CONTINUE) != 0 : (e.controls & SERVICE_ACCEPT_STOP) != 0);
        if (!elig) continue; job->items.push_back(e); names.push_back(e.display + L" (" + e.name + L")");
        if (op == SvcOp::Stop || op == SvcOp::Restart) for (auto& d : ActiveDependents(e.name)) extra.push_back(L"also stops dependent: " + d); }
    if (job->items.empty()) { delete job; return; }
    static const wchar_t* verbs[] = {L"Start", L"Stop", L"Restart", L"Pause", L"Resume"}; bool risky = op == SvcOp::Stop || op == SvcOp::Restart;
    if ((job->items.size() > 1 && Cfg().confirmBatch) || (risky && !extra.empty())) { auto all = names; all.insert(all.end(), extra.begin(), extra.end());
        if (!Confirm(wstring(verbs[(int)op]) + L" these " + std::to_wstring(job->items.size()) + L" services?", all)) { delete job; return; } }
    SetMsg(wstring(verbs[(int)op]) + L"… working"); HANDLE h = CreateThread(nullptr, 0, SvcJobThread, job, 0, nullptr); if (h) CloseHandle(h); else delete job;
}
static void KillService() {
    auto* s = OneService(); if (!s || g_page != P_SERVICES) return;
    if (!Confirm(L"Force-terminate the process of this service? Unsaved data in it is lost and Windows may restart it. Use Stop first whenever possible.", {s->display + L" (" + s->name + L"), PID " + std::to_wstring(s->pid)})) return;
    wstring err; if (KillServiceProcess(*s, g_sv.entries, err)) { SetMsg(s->display + L" process terminated."); StartScan(); } else Warn(err);
}
static void SetStartType(DWORD type, bool delayed, const wchar_t* label) {
    if (g_page != P_SERVICES) return; auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) { auto& e = g_sv.entries[i]; todo.push_back(i); names.push_back(e.display + L" (" + e.name + L")"); }
    if (todo.empty()) return;
    if ((todo.size() > 1 && Cfg().confirmBatch) || type == SERVICE_DISABLED) if (!Confirm(L"Set the start type to " + wstring(label) + L" for " + (todo.size() == 1 ? wstring(L"this service") : L"these " + std::to_wstring(todo.size()) + L" services") + L"?", names)) return;
    int ok = 0, bad = 0, grp = ++g_groupSeq; wstring firstErr, firstName;
    for (int i : todo) { auto e = g_sv.entries[i]; wstring err, before;
        if (SetServiceStartType(e.name, type, delayed, err, before)) { ++ok; g_undo.push_back({2, {}, false, e.name, e.startType, e.delayed, grp}); } else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = e.display; } } }
    wstring msg = ok ? (ok == 1 ? g_sv.entries[todo[0]].display : std::to_wstring(ok) + L" services") + L" set to " + label + L"." : L"";
    if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
    SetMsg(msg); StartScan();
}
static std::vector<ProfileInfo> g_profs;
static void ApplyProfile(const ProfileInfo& p) {
    std::vector<PlanItem> plan; wstring note, err; if (!BuildPlan(p, g_sv.entries, plan, note, err)) { Warn(err); return; }
    size_t changes = 0; bool anyDisable = false; for (auto& i : plan) if (!i.skip) { ++changes; if (i.type == SERVICE_DISABLED) anyDisable = true; }
    if (!changes) { DarkMsg(g_main, kApp, L"Nothing to change: every service in this profile is already set or not present.\r\n\r\n" + PlanText(plan), 0); return; }
    bool apply = false; wstring head = note + L"\r\n\r\n" + std::to_wstring(changes) + L" change(s)" + (anyDisable ? L", including services that will be DISABLED" : L"") + L". Nothing is applied until you press Apply. Undo (Ctrl+Z) reverts start types only.\r\n\r\n";
    ShowReport(g_main, L"Review profile: " + p.label, {{L"Plan", head + PlanText(plan)}}, {{L"Apply these changes", [&apply](const wstring&) { apply = true; }, true}, {L"Cancel", nullptr}});
    if (!apply) return;
    int ok = 0, bad = 0, grp = ++g_groupSeq; wstring firstErr;
    for (auto& i : plan) if (!i.skip) { wstring e2, before; if (SetServiceStartType(i.name, i.type, i.delayed, e2, before)) { ++ok; g_undo.push_back({2, {}, false, i.name, i.oldType, i.oldDelayed, grp}); } else { ++bad; if (firstErr.empty()) firstErr = i.display + L": " + e2; } }
    SetMsg(L"Profile applied: " + std::to_wstring(ok) + L" changed" + (bad ? L", " + std::to_wstring(bad) + L" failed — " + firstErr : L"."));
    StartScan();
}
static void SaveProfileFlow(bool snapshot) {
    std::vector<ServiceEntry> items; if (snapshot) items = g_sv.entries; else for (int i : Selected()) items.push_back(g_sv.entries[i]);
    if (items.empty()) { Warn(L"Select one or more services first."); return; }
    SYSTEMTIME t; GetLocalTime(&t); wchar_t d[64]; swprintf(d, 64, L"%04d-%02d-%02d %02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
    std::vector<FormField> f = {{L"Name", snapshot ? wstring(L"Snapshot ") + d : wstring(L"My profile"), 0, {}}};
    if (!FormDialog(g_main, snapshot ? L"Save a snapshot of all start types" : L"Save selected services as a profile", L"Save", f, L"Saves service names and start types only. No passwords or other settings are stored. Applying it later always shows a review first.")) return;
    wstring path, err; if (SaveProfile(f[0].value, items, path, err)) SetMsg(L"Saved " + Plural(items.size(), L"service", L"services") + L" to \"" + f[0].value + L"\"."); else Warn(err);
}
static void CleanSelectAll() { ListView_SetItemState(g_list, -1, LVIS_SELECTED, LVIS_SELECTED); UpdateStatus(); UpdateToolbar(); }
struct CleanJob { std::vector<CleanItem> items; bool keep; };
static DWORD WINAPI CleanThread(LPVOID p) {
    std::unique_ptr<CleanJob> j((CleanJob*)p); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); CleanResult r; RunClean(j->items, j->keep, r);
    wstring msg = L"Cleaned: " + FormatBytes(r.bytes) + L" freed, " + std::to_wstring(r.ok) + L" item(s) done";
    if (r.skipped || r.failed) msg += L", " + std::to_wstring(r.skipped + r.failed) + L" skipped/failed" + (r.firstErr.empty() ? L"" : L" (" + r.firstErr + L")");
    if (!r.backupPath.empty()) msg += L". Registry backup saved - use Restore registry\u2026 to undo.";
    PostMessageW(g_main, WM_JOB_DONE, 0, (LPARAM) new JobDone{msg}); CoUninitialize(); return 0;
}
static void CleanSelected() {
    auto sel = Selected(); if (sel.empty() || g_page != P_CLEAN) return; auto* job = new CleanJob{{}, Cfg().keepRecent}; std::vector<wstring> names; ULONGLONG total = 0; int regs = 0, risky = 0; bool recycle = false;
    for (int i : sel) { auto& e = g_cl.items[i]; job->items.push_back(e); total += e.bytes; if (IsRegItem(e)) ++regs; if (e.kind == 1) recycle = true; if (e.risk >= 1) ++risky; names.push_back(e.name + L"  (" + (IsRegItem(e) ? e.detail : FormatBytes(e.bytes)) + L", " + RiskName(e.risk) + L" risk)"); }
    wstring q = L"Clean " + Plural(sel.size(), L"item", L"items") + L"?";
    if (total) q += L" About " + FormatBytes(total) + L" of temporary data will be deleted permanently (files in use are skipped).";
    if (recycle) q += L" The Recycle Bin will be emptied for good.";
    if (risky) q += L" " + Plural(risky, L"selected item is", L"selected items are") + L" Medium/High risk: read each description (Details) before confirming.";
    if (regs) q += L" " + Plural(regs, L"registry entry", L"registry entries") + L" will be removed after saving a backup you can restore.";
    if (!Confirm(q, names)) { delete job; return; }
    SetMsg(L"Cleaning\u2026"); HANDLE h = CreateThread(nullptr, 0, CleanThread, job, 0, nullptr); if (h) CloseHandle(h); else delete job;
}
static void RestoreRegMenu(POINT pt) {
    auto b = ListRegBackups(); HMENU m = CreatePopupMenu();
    if (b.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 1, L"No registry backups yet"); else for (size_t i = 0; i < b.size(); ++i) AppendMenuW(m, MF_STRING, C_REGRESTORE_BASE + i, b[i].label.c_str());
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m);
    if (id >= C_REGRESTORE_BASE && id < C_REGRESTORE_BASE + (int)b.size()) {
        if (DarkMsg(g_main, kApp, L"Restore these registry entries?\r\n\r\n" + b[id - C_REGRESTORE_BASE].label + L"\r\n\r\nExisting values are never overwritten.", 2) != IDYES) return;
        wstring err, sum; if (RestoreRegBackup(b[id - C_REGRESTORE_BASE].path, err, sum)) { SetMsg(sum + L"."); StartScan(); } else Warn(err.empty() ? L"Nothing was restored." : err);
    }
}
// ---- large files / duplicates ----
struct FsJob { FileScanOpts o; int mode; };
struct FsDone { int mode; FileScan scan; };
static DWORD WINAPI FsThread(LPVOID p) {
    std::unique_ptr<FsJob> j((FsJob*)p); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); auto* d = new FsDone{j->mode, {}}; ScanFiles(j->o, d->scan);
    PostMessageW(g_main, WM_FILE_DONE, 0, (LPARAM)d); CoUninitialize(); return 0;
}
static std::vector<wstring> ScanRoots() { return Cfg().scanRoots.empty() ? DefaultScanRoots() : Cfg().scanRoots; }
static void StartFileScan(int mode) {
    bool dupes = mode == 1;
    if (g_fsBusy) return; auto roots = ScanRoots(); if (roots.empty()) { Warn(L"No folders to scan. Use Folders… to add one."); return; }
    auto* j = new FsJob{{}, mode}; j->o.roots = roots; j->o.dupes = dupes; j->o.empty = mode == 2; j->o.minSize = dupes ? (ULONGLONG)Cfg().dupMinKB * 1024 : (ULONGLONG)Cfg().largeMinMB * 1024 * 1024; j->o.maxResults = 1000;
    g_fsCancel = false; j->o.cancel = &g_fsCancel; j->o.progress = [](const wstring& m) { PostMessageW(g_main, WM_FILE_PROGRESS, 0, (LPARAM) new wstring(m)); };
    g_fsBusy = true; SetMsg(L"Scanning " + Plural(roots.size(), L"folder", L"folders") + L"…"); UpdateStatus(); UpdateToolbar();
    HANDLE h = CreateThread(nullptr, 0, FsThread, j, 0, nullptr); if (h) CloseHandle(h); else { delete j; g_fsBusy = false; UpdateToolbar(); }
}
static void PruneDupes() {
    std::map<int, int> cnt; for (auto& e : g_dupes.files) ++cnt[e.group]; std::vector<FileEntry> keep; g_dupes.reclaimable = 0; g_dupes.totalBytes = 0; std::set<int> groups; std::map<int, ULONGLONG> sz;
    for (auto& e : g_dupes.files) if (cnt[e.group] > 1) { FileEntry x = e; x.groupCount = cnt[e.group]; keep.push_back(x); groups.insert(e.group); sz[e.group] = e.size; }
    for (auto& g : groups) { g_dupes.reclaimable += sz[g] * (cnt[g] - 1); g_dupes.totalBytes += sz[g] * cnt[g]; } g_dupes.files = keep; g_dupes.groups = (int)groups.size();
}
static void RecycleSelected() {
    if (!IsFilePage(g_page) || g_fsBusy) return; auto sel = Selected(); if (sel.empty()) return; auto& scan = FS(g_page);
    if (g_page == P_DUPES) {   // never allow removing every copy of a file
        std::map<int, int> chosen; for (int i : sel) ++chosen[scan.files[i].group];
        for (auto& kv : chosen) { int total = 0; for (auto& e : scan.files) if (e.group == kv.first) ++total; if (kv.second >= total) { Warn(L"Group " + std::to_wstring(kv.first) + L" has all of its copies selected. At least one copy must be kept - deselect one and try again."); return; } }
    }
    std::vector<wstring> names, paths; ULONGLONG total = 0; for (int i : sel) { auto& e = scan.files[i]; names.push_back(e.path + L"  (" + FormatBytes(e.size) + L")"); paths.push_back(e.path); total += e.size; }
    if (!Confirm(L"Move " + Plural(sel.size(), g_page == P_EMPTY ? L"empty folder" : L"file", g_page == P_EMPTY ? L"empty folders" : L"files") + (g_page == P_EMPTY ? wstring() : L" (" + FormatBytes(total) + L")") + L" to the Recycle Bin? You can restore them from there.", names)) return;
    wstring err; RecycleFiles(g_main, paths, err);
    std::set<wstring> gone; ULONGLONG freed = 0; for (int i : sel) if (GetFileAttributesW(scan.files[i].path.c_str()) == INVALID_FILE_ATTRIBUTES) { gone.insert(scan.files[i].path); freed += scan.files[i].size; }
    std::vector<FileEntry> rest; for (auto& e : scan.files) if (!gone.count(e.path)) rest.push_back(e); scan.files = rest;
    if (g_page == P_DUPES) PruneDupes(); else { scan.totalBytes = 0; for (auto& e : scan.files) scan.totalBytes += e.size; }
    WriteJournal(L"File finder", wstring(g_page == P_DUPES ? L"duplicates" : g_page == P_EMPTY ? L"empty folders" : L"large files"), Plural(sel.size(), L"file", L"files") + L" selected", Plural(gone.size(), L"file", L"files") + L" moved to Recycle Bin (" + FormatBytes(freed) + L")", gone.size() == sel.size() ? L"ok (restorable from Recycle Bin)" : L"partial: " + err, true);
    SetMsg(Plural(gone.size(), L"file", L"files") + L" moved to the Recycle Bin (" + FormatBytes(freed) + L")." + (gone.size() < sel.size() ? L" " + std::to_wstring(sel.size() - gone.size()) + L" could not be moved." : L""));
    BuildCells(); ApplyView();
}
static void AutoSelect(int mode) {
    if (g_page != P_DUPES || g_dupes.files.empty()) return; std::vector<DupRec> recs; for (auto& e : g_dupes.files) recs.push_back({e.group, e.mtime, e.path.size()});
    auto rm = SelectDuplicatesToRemove(recs, mode); std::set<int> want(rm.begin(), rm.end()); ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED);
    for (size_t i = 0; i < g_view.size(); ++i) if (want.count(g_view[i])) ListView_SetItemState(g_list, (int)i, LVIS_SELECTED, LVIS_SELECTED);
    UpdateStatus(); UpdateToolbar(); SetMsg(Plural(want.size(), L"file", L"files") + L" selected (one copy per group kept). Review, then Move to Recycle Bin.");
}
static void AddScanFolder() {
    BROWSEINFOW bi{}; bi.hwndOwner = g_main; bi.lpszTitle = L"Choose a folder to include in the scan"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi); if (!pidl) return; wchar_t path[MAX_PATH]; if (SHGetPathFromIDListW(pidl, path)) { auto& r = Cfg().scanRoots; if (r.empty()) r = DefaultScanRoots(); if (std::find(r.begin(), r.end(), wstring(path)) == r.end() && r.size() < 20) r.push_back(path); SaveSettings(); SetMsg(L"Folder added: " + wstring(path)); } CoTaskMemFree(pidl);
}
static void ShowFileDetails() {
    auto s = Selected(); if (s.empty()) return; auto& e = FS(g_page).files[s[0]];
    wstring t = e.name + L"\r\n\r\nPATH\r\n" + e.path + L"\r\n\r\nSIZE\r\n" + FormatBytes(e.size) + L" (" + std::to_wstring(e.size) + L" bytes)\r\n\r\nMODIFIED\r\n" + FileDateText(e.mtime) + L"\r\n";
    if (g_page == P_EMPTY) t = e.name + L"\r\n\r\nPATH\r\n" + e.path + L"\r\n\r\nMODIFIED\r\n" + FileDateText(e.mtime) + L"\r\n\r\nCONTAINS\r\nNo files. " + std::to_wstring(e.groupCount) + L" empty subfolder(s) inside.\r\n";
    if (g_page == P_DUPES) { t += L"\r\nIDENTICAL COPIES (same content, SHA-256)\r\n"; for (auto& o : g_dupes.files) if (o.group == e.group) t += (o.path == e.path ? L"  • (this one) " : L"  • ") + o.path + L"   " + FileDateText(o.mtime) + L"\r\n"; }
    wstring path = e.path;
    ShowReport(g_main, e.name + L" — Details", {{L"Details", t}}, {{L"Open file location", [path](const wstring&) { OpenFileLocation(path); }}, {L"Copy", [](const wstring& x) { SetClip(x); }}, {L"Close", nullptr}});
}
static void Undo() {
    if (g_undo.empty()) return; int grp = g_undo.back().group; int ok = 0, bad = 0; wstring firstErr;
    while (!g_undo.empty() && g_undo.back().group == grp) { auto u = g_undo.back(); g_undo.pop_back(); wstring err, before; bool r = u.kind == 1 ? SetStartupEnabled(u.se, u.wasEnabled, err) : SetServiceStartType(u.name, u.type, u.delayed, err, before);
        if (r) ++ok; else { ++bad; if (firstErr.empty()) firstErr = err; } }
    SetMsg(L"Undo: " + std::to_wstring(ok) + L" reverted" + (bad ? L", " + std::to_wstring(bad) + L" failed — " + firstErr : L".") + L" (configuration only; running state is not restored)"); StartScan();
}
static void RestartAsAdmin() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH); wstring arg = g_page == P_SERVICES ? L"--services" : g_page == P_STARTUP ? L"--startup" : L"";
    if (g_mtx) { CloseHandle(g_mtx); g_mtx = nullptr; }
    if ((INT_PTR)ShellExecuteW(nullptr, L"runas", exe, arg.c_str(), nullptr, SW_SHOWNORMAL) > 32) DestroyWindow(g_main);
    else g_mtx = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance");
}
static void About() {
    DarkMsg(g_main, L"About", wstring(kApp) + L" " FSU_VERSION L"\r\nDeveloped by Akshaya Simha\r\n\r\nBuilt " + L"" __DATE__ L" " __TIME__ L"\r\nWindows 10/11 x64 · native Win32 · offline · no telemetry\r\nUnsigned build (Windows SmartScreen may warn).", 0);
}

// ================= menus =================
static void AddItem(HMENU m, UINT id, const wchar_t* text, bool enabled = true, bool def = false) { AppendMenuW(m, MF_STRING | (enabled ? 0 : MF_GRAYED), id, text); if (def) SetMenuDefaultItem(m, id, FALSE); }
static void Sep(HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
static HMENU StartTypeMenu() { HMENU m = CreatePopupMenu(); AddItem(m, C_ST_AUTO, L"Automatic"); AddItem(m, C_ST_AUTODELAY, L"Automatic (delayed)"); AddItem(m, C_ST_MANUAL, L"Manual"); AddItem(m, C_ST_DISABLED, L"Disabled"); return m; }
static HMENU DelayMenu() {
    bool can = false, un = false; for (int i : Selected()) { if (CanDelay(g_su.entries[i])) can = true; if (g_su.entries[i].managedDelay) un = true; }
    HMENU m = CreatePopupMenu(); static const wchar_t* lab[] = {L"15 seconds after sign-in", L"30 seconds after sign-in", L"1 minute after sign-in", L"2 minutes after sign-in", L"5 minutes after sign-in"};
    for (int i = 0; i < 5; ++i) AddItem(m, C_DELAY_BASE + i, lab[i], can); Sep(m); AddItem(m, C_REMOVE_DELAY, L"Remove delay (restore the original)", un); return m;
}
static void ContextMenu(POINT pt) {
    auto sel = Selected(); if (sel.empty()) return; HMENU m = CreatePopupMenu(); bool one = sel.size() == 1;
    if (g_page == P_STARTUP) {
        AddItem(m, C_DETAILS, L"Details\tEnter", true, true); Sep(m);
        AddItem(m, C_ENABLE, L"Enable", TbOn(C_ENABLE)); AddItem(m, C_DISABLE, L"Disable", TbOn(C_DISABLE)); AppendMenuW(m, MF_POPUP, (UINT_PTR)DelayMenu(), L"Start later");
        AddItem(m, C_DELETE, L"Delete…\tDel", TbOn(C_DELETE)); Sep(m);
        AddItem(m, C_OPENLOC, L"Open file location", one); AddItem(m, C_REGEDIT, L"Show where it is set", one); AddItem(m, C_VT, L"Check on VirusTotal…", one); AddItem(m, C_SEARCHONLINE, L"Search online…", one); Sep(m);
        AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C");
    } else if (g_page == P_SERVICES) {
        AddItem(m, C_DETAILS, L"Properties\tEnter", true, true); Sep(m);
        AddItem(m, C_SVC_START, L"Start\tCtrl+Shift+S", TbOn(C_SVC_START)); AddItem(m, C_SVC_STOP, L"Stop\tCtrl+Shift+T", TbOn(C_SVC_STOP)); AddItem(m, C_SVC_RESTART, L"Restart\tCtrl+Shift+R", TbOn(C_SVC_RESTART));
        AddItem(m, C_SVC_PAUSE, L"Pause", TbOn(C_SVC_PAUSE)); AddItem(m, C_SVC_RESUME, L"Resume", TbOn(C_SVC_RESUME)); AddItem(m, C_SVC_KILL, L"Kill process\tCtrl+Shift+K", TbOn(C_SVC_KILL)); Sep(m);
        AppendMenuW(m, MF_POPUP, (UINT_PTR)StartTypeMenu(), L"Start type"); Sep(m);
        AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C"); AddItem(m, C_OPENLOC, L"Open file location", one); AddItem(m, C_VT, L"Check on VirusTotal…", one); AddItem(m, C_SEARCHONLINE, L"Search online…", one); Sep(m);
        AddItem(m, C_PROF_SAVESEL, L"Save as a profile…"); 
    } else if (IsFilePage(g_page)) {
        AddItem(m, C_DETAILS, L"Details\tEnter", true, true); Sep(m); AddItem(m, C_OPENLOC, L"Open file location", one); AddItem(m, C_FS_RECYCLE, L"Move to Recycle Bin\tDel", TbOn(C_FS_RECYCLE)); Sep(m); AddItem(m, C_COPYNAME, L"Copy path\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C");
    } else if (g_page == P_CLEAN) {
        AddItem(m, C_DETAILS, L"Details\tEnter", true, true); Sep(m); AddItem(m, C_CLEAN, L"Clean selected\u2026"); Sep(m); AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C");
    } else {
        AddItem(m, C_DETAILS, L"Properties\tEnter", true, true); Sep(m); AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C"); AddItem(m, C_OPENLOC, L"Open file location", one);
        AddItem(m, C_VT, L"Check on VirusTotal…", one); AddItem(m, C_SEARCHONLINE, L"Search online…", one);
    }
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
}
static void DropMenu(int cmd) {
    auto* it = TbFind(cmd); if (!it) return; RECT wr; GetWindowRect(it->h, &wr); POINT pt{wr.left, wr.bottom};
    if (cmd == C_FILTER) {
        HMENU m = CreatePopupMenu(); int n = FilterCount(g_page); const wchar_t** names = g_page == P_STARTUP ? kStartupFilters : g_page == P_SERVICES ? kServiceFilters : g_page == P_CLEAN ? kCleanFilters : g_page == P_LARGE ? kLargeFilters : kDriverFilters;
        for (int i = 0; i < n; ++i) { if ((g_page == P_STARTUP && (i == 3 || i == 6 || i == 10)) || (g_page == P_SERVICES && (i == 3 || i == 5 || i == 7)) || (g_page == P_DRIVERS && (i == 3 || i == 5))) Sep(m);
            AppendMenuW(m, MF_STRING | (i == g_filter[g_page] ? MF_CHECKED : 0), C_FILTER_BASE + i, names[i]); }
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, wr.right, wr.bottom, 0, g_main, nullptr); DestroyMenu(m);
        if (id >= C_FILTER_BASE && id < C_FILTER_BASE + n) { g_filter[g_page] = id - C_FILTER_BASE; TbText(C_FILTER, FilterName(g_page)); ApplyView(); }
    } else if (cmd == C_MORE) {
        HMENU m = CreatePopupMenu(); auto sel = Selected(); bool listy = IsActionList(g_page);
        if (listy) { AddItem(m, C_DETAILS, IsSvcPage(g_page) ? L"Properties\tEnter" : L"Details\tEnter", !sel.empty()); } AddItem(m, C_UNDO, L"Undo\tCtrl+Z", !g_undo.empty()); Sep(m);
        if (g_page == P_STARTUP) { AddItem(m, C_ADD, L"Add an entry…\tCtrl+N"); AddItem(m, C_RESTORE, L"Restore deleted entries…"); AddItem(m, C_OPENBACKUPS, L"Open the backup folder"); Sep(m); }
        if (g_page == P_CLEAN) { AddItem(m, C_CLEAN, L"Clean selected\u2026", !sel.empty()); AddItem(m, C_CLEAN_RESTORE, L"Restore registry entries\u2026"); AddItem(m, C_OPENBACKUPS, L"Open the backup folders"); Sep(m); }
        AddItem(m, C_REFRESH, L"Refresh\tF5"); if (listy) AddItem(m, C_SELALL, L"Select all\tCtrl+A"); if (listy) AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C", !sel.empty());
        if (listy || g_page == P_HISTORY) AddItem(m, C_EXPORT, L"Export the list…\tCtrl+E"); Sep(m); AddItem(m, C_OPENDATA, L"Open the data folder");
        if (!g_elevated) AddItem(m, C_ADMIN, L"Restart as administrator"); AddItem(m, C_SETTINGS, L"Settings…\tCtrl+,"); AddItem(m, C_ABOUT, L"About Feather System Utilities\tF1"); Sep(m); AddItem(m, C_EXIT, L"Exit");
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, wr.right, wr.bottom, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    } else if (cmd == C_STARTTYPE) { HMENU m = StartTypeMenu(); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0); }
    else if (cmd == C_STARTLATER) { HMENU m = DelayMenu(); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0); }
    else if (cmd == C_RESTORE) RestoreMenu(pt);
    else if (cmd == C_CLEAN_RESTORE) RestoreRegMenu(pt);
    else if (cmd == C_FS_FOLDERS) {
        HMENU m = CreatePopupMenu(); AddItem(m, C_FS_FOLDER_ADD, L"Add a folder\u2026"); AddItem(m, C_FS_FOLDER_DEFAULT, L"Use the default folders (Downloads, Documents, Desktop, Pictures, Videos, Music)"); Sep(m);
        auto roots = ScanRoots(); bool custom = !Cfg().scanRoots.empty(); for (size_t i = 0; i < roots.size() && i < 20; ++i) AddItem(m, custom ? C_FS_RM_BASE + (UINT)i : 0, ((custom ? L"Remove: " : L"Scanning: ") + roots[i]).c_str(), custom);
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    } else if (cmd == C_FS_MIN) {
        HMENU m = CreatePopupMenu(); bool d = g_page == P_DUPES; static const int mb[] = {50, 100, 250, 500, 1024}, kb[] = {100, 1024, 10240, 102400}; int cur = d ? Cfg().dupMinKB : Cfg().largeMinMB;
        for (int i = 0; i < (d ? 4 : 5); ++i) { int v = d ? kb[i] : mb[i]; wchar_t lab[48]; if (d) swprintf(lab, 48, v >= 1024 ? L"At least %d MB" : L"At least %d KB", v >= 1024 ? v / 1024 : v); else swprintf(lab, 48, v >= 1024 ? L"At least %d GB" : L"At least %d MB", v >= 1024 ? v / 1024 : v); AppendMenuW(m, MF_STRING | (v == cur ? MF_CHECKED : 0), C_FS_MIN_BASE + i, lab); }
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    } else if (cmd == C_FS_AUTO) {
        HMENU m = CreatePopupMenu(); AddItem(m, C_FS_AUTO_BASE, L"Keep the oldest copy, select the rest"); AddItem(m, C_FS_AUTO_BASE + 1, L"Keep the newest copy, select the rest"); AddItem(m, C_FS_AUTO_BASE + 2, L"Keep the copy with the shortest path, select the rest");
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    }
    else if (cmd == C_PROFILES) {
        g_profs = ListProfiles(); HMENU m = CreatePopupMenu(); AddItem(m, 0, L"Built-in profiles (you review every change first):", false);
        size_t nb = 0; for (auto& p : g_profs) if (p.builtin) ++nb;
        for (size_t i = 0; i < nb; ++i) AddItem(m, C_PROF_BASE + (UINT)i, (g_profs[i].label + L"…").c_str());
        Sep(m); AddItem(m, C_PROF_SAVESEL, L"Save selected services as a profile…", !Selected().empty()); AddItem(m, C_PROF_OPENDIR, L"Open the profiles folder"); Sep(m);
        AddItem(m, C_PROF_SNAPSHOT, L"Save a snapshot of all start types…");
        HMENU rs = CreatePopupMenu(); bool any = false; for (size_t i = nb; i < g_profs.size() && i < 30; ++i) { AppendMenuW(rs, MF_STRING, C_PROF_BASE + (UINT)i, g_profs[i].label.c_str()); any = true; }
        if (!any) AddItem(rs, 0, L"No saved profiles or snapshots", false); AppendMenuW(m, MF_POPUP, (UINT_PTR)rs, L"Restore a snapshot or profile…"); Sep(m); AddItem(m, C_UNDO, L"Undo the last start-type change\tCtrl+Z", !g_undo.empty());
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    }
}
static void ApplyTheme(int t);
static void DoCommand(int id) {
    switch (id) {
    case C_REFRESH: g_stMsg.clear(); StartScan(); if (g_page == P_HISTORY) { ReadHistory(); ApplyView(); } break;
    case C_ENABLE: ToggleStartup(true); break; case C_DISABLE: ToggleStartup(false); break; case C_DELETE: DeleteStartup(); break; case C_ADD: AddEntry(); break;
    case C_RESTORE: case C_FILTER: case C_MORE: case C_STARTTYPE: case C_STARTLATER: case C_PROFILES: case C_CLEAN_RESTORE: DropMenu(id); break;
    case C_CLEAN: CleanSelected(); break; case C_CLEAN_SELALL: CleanSelectAll(); break;
    case C_FS_SCAN: StartFileScan(g_page == P_DUPES ? 1 : g_page == P_EMPTY ? 2 : 0); break; case C_FS_STOP: g_fsCancel = true; SetMsg(L"Stopping\u2026"); break;
    case C_FS_FOLDERS: case C_FS_MIN: case C_FS_AUTO: DropMenu(id); break; case C_FS_RECYCLE: RecycleSelected(); break;
    case C_FS_FOLDER_ADD: AddScanFolder(); break; case C_FS_FOLDER_DEFAULT: Cfg().scanRoots.clear(); SaveSettings(); SetMsg(L"Using the default folders."); break;
    case C_UNDO: Undo(); break; case C_REMOVE_DELAY: RemoveDelays(); break;
    case C_SVC_START: RunSvcOp(SvcOp::Start); break; case C_SVC_STOP: RunSvcOp(SvcOp::Stop); break; case C_SVC_RESTART: RunSvcOp(SvcOp::Restart); break;
    case C_SVC_PAUSE: RunSvcOp(SvcOp::Pause); break; case C_SVC_RESUME: RunSvcOp(SvcOp::Resume); break; case C_SVC_KILL: KillService(); break;
    case C_ST_AUTO: SetStartType(SERVICE_AUTO_START, false, L"Automatic"); break; case C_ST_AUTODELAY: SetStartType(SERVICE_AUTO_START, true, L"Automatic (delayed)"); break;
    case C_ST_MANUAL: SetStartType(SERVICE_DEMAND_START, false, L"Manual"); break; case C_ST_DISABLED: SetStartType(SERVICE_DISABLED, false, L"Disabled"); break;
    case C_PROF_SAVESEL: SaveProfileFlow(false); break; case C_PROF_SNAPSHOT: SaveProfileFlow(true); break; case C_PROF_OPENDIR: ShellExecuteW(nullptr, L"open", SubDir(L"profiles").c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case C_DETAILS: ShowDetails(); break; case C_COPYNAME: CopyNames(); break; case C_COPYDETAILS: CopyDetails(); break; case C_SELALL: SelectAll(); break; case C_EXPORT: ExportList(); break;
    case C_OPENLOC: if (IsFilePage(g_page)) { auto s = Selected(); if (!s.empty()) OpenFileLocation(FS(g_page).files[s[0]].path); } else if (g_page == P_STARTUP) { if (auto* e = OneStartup()) OpenFileLocation(e->program); } else if (auto* s = OneService()) OpenFileLocation(s->program); break;
    case C_REGEDIT: if (auto* e = OneStartup()) ShowInRegistryEditor(e->location); break;
    case C_VT: case C_SEARCHONLINE: { wstring prog, name; if (g_page == P_STARTUP) { if (auto* e = OneStartup()) { prog = e->program; name = e->name; } } else if (auto* s = OneService()) { prog = s->program; name = s->display; }
        if (!prog.empty() || !name.empty()) OnlineLookup(g_main, id == C_VT, prog, name); break; }
    case C_ADMIN: RestartAsAdmin(); break; case C_SETTINGS: ShowPage(P_SETTINGS); break; case C_ABOUT: About(); break;
    case C_OPENBACKUPS: ShellExecuteW(nullptr, L"open", SubDir(g_page == P_CLEAN ? L"regbackups" : L"backups").c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case C_OPENDATA: ShellExecuteW(nullptr, L"open", DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case C_EXIT: DestroyWindow(g_main); break;
    case 0xE001: ApplyTheme(Cfg().theme); break;
    default:
        if (id >= C_FS_MIN_BASE && id < C_FS_MIN_BASE + 8) { static const int mb[] = {50, 100, 250, 500, 1024}, kb[] = {100, 1024, 10240, 102400}; int i = id - C_FS_MIN_BASE; if (g_page == P_DUPES && i < 4) Cfg().dupMinKB = kb[i]; else if (g_page == P_LARGE && i < 5) Cfg().largeMinMB = mb[i]; SaveSettings(); SetMsg(L"Minimum size changed. Press Scan to apply it."); }
        else if (id >= C_FS_AUTO_BASE && id < C_FS_AUTO_BASE + 3) AutoSelect(id - C_FS_AUTO_BASE);
        else if (id >= C_FS_RM_BASE && id < C_FS_RM_BASE + 20) { auto& r = Cfg().scanRoots; size_t i = id - C_FS_RM_BASE; if (i < r.size()) { r.erase(r.begin() + i); SaveSettings(); SetMsg(L"Folder removed from the scan list."); } }
        else if (id >= C_DELAY_BASE && id < C_DELAY_BASE + 5) DelayEntries(kDelays[id - C_DELAY_BASE]);
        else if (id >= C_PROF_BASE && id < C_PROF_BASE + 64 && (size_t)(id - C_PROF_BASE) < g_profs.size()) ApplyProfile(g_profs[id - C_PROF_BASE]);
    }
}

// ================= list drawing =================
static COLORREF CellColor(Page p, int row, int col, bool& dim) {
    dim = false;
    if (p == P_STARTUP) { auto& e = g_su.entries[row]; if (e.state == L"Disabled") dim = true;
        switch (col) { case 1: return e.state == L"Enabled" ? cGreen : e.state == L"Disabled" ? cDim : cAmber; case 2: return cDim; case 3: return cRed; case 4: return e.microsoft ? cText : cLav; }
        return dim ? cDim : cText; }
    if (p == P_EMPTY) return col == 2 ? cDim : col == 1 ? cDim : cText;
    if (IsFilePage(p)) { if (col == 1) return cGreen; if (col == 3 && p == P_DUPES) return (g_dupes.files[row].group & 1) ? cAmber : cLav; return col == 2 ? cDim : cText; }
    if (p == P_CLEAN) { auto& e = g_cl.items[row]; if (col == 1) return IsRegItem(e) ? cAmber : cText; if (col == 2) return e.risk == 2 ? cRed : e.risk == 1 ? cAmber : cGreen; if (col == 3) return e.bytes ? cGreen : cDim; if (col == 4) return e.needsAdmin && !g_elevated ? cAmber : cDim; return col == 6 ? cDim : cText; }
    if (p == P_SERVICES || p == P_DRIVERS) { auto& e = SvcVec(p)[row]; int wcol = p == P_SERVICES ? 8 : 8;
        if (col == 2) return e.stateCode == SERVICE_RUNNING ? cGreen : e.stateCode == SERVICE_STOPPED ? cDim : cAmber;
        if (col == 4) return e.safety == L"Critical" ? cRed : e.safety == L"Boot/System" ? cAmber : e.safety == L"Third-party" ? cLav : cDim;
        if (col == wcol) return cRed;
        return (e.stateCode == SERVICE_STOPPED && col != 0) ? cDim : cText; }
    return cText;
}
static LRESULT ListCustomDraw(LPNMLVCUSTOMDRAW cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    case CDDS_ITEMPREPAINT: cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS); return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        int vi = (int)cd->nmcd.dwItemSpec, col = cd->iSubItem; if (vi >= (int)g_view.size()) return CDRF_DODEFAULT; cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
        bool dim; cd->clrText = g_page == P_HISTORY ? (col == 4 && g_cells[P_HISTORY][g_view[vi]][4].rfind(L"failed", 0) == 0 ? cRed : cText) : CellColor(g_page, g_view[vi], col, dim);
        bool sel = ListView_GetItemState(g_list, vi, LVIS_SELECTED) != 0; cd->clrTextBk = sel ? cSel : cBg; return CDRF_NEWFONT; }
    case CDDS_POSTPAINT: {
        HDC dc = cd->nmcd.hdc; RECT cr; GetClientRect(g_list, &cr); RECT hr; GetWindowRect(g_hdr, &hr); int top = hr.bottom - hr.top;
        HPEN p = CreatePen(PS_SOLID, 1, cGrid); HPEN op = (HPEN)SelectObject(dc, p); int n = Header_GetItemCount(g_hdr);
        for (int i = 0; i < n; ++i) { RECT r; Header_GetItemRect(g_hdr, i, &r); MoveToEx(dc, r.right - 1, top, nullptr); LineTo(dc, r.right - 1, cr.bottom); }
        SelectObject(dc, op); DeleteObject(p); return CDRF_DODEFAULT; }
    }
    return CDRF_DODEFAULT;
}
static void PaintHeaderItem(HDC dc, int i) {
    RECT r; Header_GetItemRect(g_hdr, i, &r); FillRect(dc, &r, g_brBar);
    HPEN p = CreatePen(PS_SOLID, 1, cGrid); HPEN op = (HPEN)SelectObject(dc, p); MoveToEx(dc, r.right - 1, r.top, nullptr); LineTo(dc, r.right - 1, r.bottom); MoveToEx(dc, r.left, r.bottom - 1, nullptr); LineTo(dc, r.right, r.bottom - 1); SelectObject(dc, op); DeleteObject(p);
    wchar_t t[128] = L""; HDITEMW hi{}; hi.mask = HDI_TEXT; hi.pszText = t; hi.cchTextMax = 128; SendMessageW(g_hdr, HDM_GETITEMW, i, (LPARAM)&hi);
    SelectObject(dc, g_font); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, cText); RECT tr = r; tr.left += S(6); tr.right -= S(4);
    int n; const Col* cs = ColsFor(g_page, n); bool right = i < n && cs[i].right; DrawTextW(dc, t, -1, &tr, (right ? DT_RIGHT : DT_LEFT) | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (i == g_sortCol[g_page]) { SetTextColor(dc, cDim); RECT ar = r; ar.right -= S(6); DrawTextW(dc, g_sortAsc[g_page] ? L"▲" : L"▼", -1, &ar, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); }
}
static LRESULT CALLBACK HeaderProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); FillRect(dc, &cr, g_brBar); for (int i = 0; i < Header_GetItemCount(h); ++i) PaintHeaderItem(dc, i); EndPaint(h, &ps); return 0; }
    return DefSubclassProc(h, m, w, l);
}

// ================= window =================
static void MakeFonts() {
    if (g_font) DeleteObject(g_font); if (g_fontBig) DeleteObject(g_fontBig); if (g_fontTitle) DeleteObject(g_fontTitle);
    g_font = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBig = CreateFontW(-MulDiv(22, g_dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-MulDiv(16, g_dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}
static RECT g_searchBox;
static void Layout(int w, int h) {
    int railW = S(120), tbH = S(40), stH = S(26), searchW = S(220), searchH = S(26);
    MoveWindow(g_nav, 0, 0, railW, h - stH, TRUE); TbLayout();
    int sx = g_tbLeft - searchW - S(6); if (sx < railW + S(300)) sx = railW + S(300);
    g_searchBox = {sx, (tbH - searchH) / 2, sx + searchW, (tbH - searchH) / 2 + searchH};
    MoveWindow(g_search, sx + S(8), g_searchBox.top + S(5), searchW - S(16), searchH - S(10), TRUE);
    MoveWindow(g_list, railW, tbH, w - railW, h - tbH - stH, TRUE); MoveWindow(g_panel, railW, tbH, w - railW, h - tbH - stH, TRUE);
    MoveWindow(g_status, 0, h - stH, w, stH, TRUE); BringWindowToTop(g_search); InvalidateRect(g_main, nullptr, TRUE);
    if (g_page == P_DASH || g_page == P_SETTINGS) RebuildPanel();
}
static LRESULT CALLBACK SearchProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { SetWindowTextW(h, L""); return 0; }
    if (m == WM_KEYDOWN && (w == VK_DOWN || w == VK_RETURN)) { SetFocus(g_list); return 0; }
    if (m == WM_CHAR && (w == VK_ESCAPE || w == VK_RETURN)) return 0;
    return DefSubclassProc(h, m, w, l);
}
static void DarkModeInit(int theme) {
    if (WindowsBuild() < 17763) return;   // undocumented dark-menu hooks exist only on Windows 10 1809+; Windows 7/8 keep native menus
    HMODULE ux = LoadLibraryW(L"uxtheme.dll"); if (!ux) return;
    auto setMode = (int(WINAPI*)(int))GetProcAddress(ux, MAKEINTRESOURCEA(135)); auto flush = (void(WINAPI*)())GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (setMode) setMode(PalFor(theme).dark ? 2 : 3); if (flush) flush();
}
static void ApplyTheme(int t) {
    Cfg().theme = t; P = PalFor(t);
    if (g_brBg) DeleteObject(g_brBg); if (g_brBar) DeleteObject(g_brBar); if (g_brRail) DeleteObject(g_brRail);
    g_brBg = CreateSolidBrush(cBg); g_brBar = CreateSolidBrush(cBar); g_brRail = CreateSolidBrush(cRail);
    if (g_main) {
        SetDarkTitleBar(g_main, P.dark); DarkModeInit(t);
        ListView_SetBkColor(g_list, cBg); ListView_SetTextBkColor(g_list, cBg); ListView_SetTextColor(g_list, cText);
        SetWindowTheme(g_list, P.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr); SetWindowTheme(g_hdr, P.dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
        RedrawWindow(g_main, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
    SaveSettings();
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_main = h; g_dpi = DpiForWindow(h); MakeFonts(); g_brBg = CreateSolidBrush(cBg); g_brBar = CreateSolidBrush(cBar); g_brRail = CreateSolidBrush(cRail); SetDarkTitleBar(h, P.dark);
        g_nav = CreateWindowExW(0, L"LISTBOX", L"Navigation", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_NAV, g_inst, nullptr);
        for (auto n : kPageNames) SendMessageW(g_nav, LB_ADDSTRING, 0, (LPARAM)n);
        g_search = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_SEARCH, g_inst, nullptr); SetWindowSubclass(g_search, SearchProc, 1, 0);
        g_list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_LIST, g_inst, nullptr);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP);
        ListView_SetBkColor(g_list, cBg); ListView_SetTextBkColor(g_list, cBg); ListView_SetTextColor(g_list, cText); SetWindowTheme(g_list, P.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        g_hdr = ListView_GetHeader(g_list); SetWindowTheme(g_hdr, P.dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr); SetWindowSubclass(g_hdr, HeaderProc, 2, 0);
        g_panel = CreateWindowExW(0, L"FsuPanel", L"Overview", WS_CHILD | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_PANEL, g_inst, nullptr);
        g_status = CreateWindowExW(0, L"FsuStatus", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, h, (HMENU)ID_STATUS, g_inst, nullptr);
        for (HWND c : {g_nav, g_search, g_list, g_panel, g_status}) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_search, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(2, 2)); ApplyDensity();
        wchar_t un[256]; DWORD n = 256; GetUserNameW(un, &n); g_user = un; g_elevated = IsElevated();
        ShowPage(P_DASH); StartScan(); return 0; }
    case WM_SIZE: Layout(LOWORD(l), HIWORD(l)); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = {S(820), S(480)}; return 0;
    case WM_DPICHANGED: { g_dpi = HIWORD(w); MakeFonts(); for (HWND c : {g_nav, g_search, g_list, g_panel, g_status}) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE); for (auto& it : g_items) if (it.h) SendMessageW(it.h, WM_SETFONT, (WPARAM)g_font, TRUE);
        ApplyDensity(); auto* r = (RECT*)l; SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_brBg); RECT st = TbStrip(); FillRect((HDC)w, &st, g_brBar); return 1; }
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT st = TbStrip(); FillRect(dc, &st, g_brBar);
        HPEN p = CreatePen(PS_SOLID, 1, cGrid); HGDIOBJ op = SelectObject(dc, p);
        for (auto& it : g_items) if (it.sep) { int x = (it.r.left + it.r.right) / 2; MoveToEx(dc, x, S(10), nullptr); LineTo(dc, x, st.bottom - S(10)); }
        MoveToEx(dc, st.left, st.bottom - 1, nullptr); LineTo(dc, st.right, st.bottom - 1); SelectObject(dc, op); DeleteObject(p);
        if (IsActionList(g_page) || g_page == P_HISTORY) { HPEN p2 = CreatePen(PS_SOLID, 1, cGrid); HBRUSH b2 = CreateSolidBrush(cBg); HGDIOBJ o1 = SelectObject(dc, p2), o2 = SelectObject(dc, b2);
            RoundRect(dc, g_searchBox.left, g_searchBox.top, g_searchBox.right, g_searchBox.bottom, S(6), S(6)); SelectObject(dc, o1); SelectObject(dc, o2); DeleteObject(p2); DeleteObject(b2); }
        EndPaint(h, &ps); return 0; }
    case WM_SETFOCUS: SetFocus(g_nav); return 0;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { HDC dc = (HDC)w; SetTextColor(dc, cText); SetBkColor(dc, cBg); return (LRESULT)g_brBg; }
    case WM_MEASUREITEM: { auto* mi = (MEASUREITEMSTRUCT*)l; if (mi->CtlID == ID_NAV) { mi->itemHeight = S(34); return TRUE; } break; }
    case WM_DRAWITEM: { auto* di = (DRAWITEMSTRUCT*)l;
        if (di->CtlID != ID_NAV) { DrawToolbarButton(di); return TRUE; } if (di->itemID == (UINT)-1) break;
        bool sel = (int)di->itemID == (int)g_page; HBRUSH b = CreateSolidBrush(sel ? cRailSel : cRail); FillRect(di->hDC, &di->rcItem, b); DeleteObject(b);
        if (sel) { RECT bar = di->rcItem; bar.right = bar.left + S(3); HBRUSH a = CreateSolidBrush(cAccent); FillRect(di->hDC, &bar, a); DeleteObject(a); }
        SetBkMode(di->hDC, TRANSPARENT); SetTextColor(di->hDC, sel ? (P.dark ? RGB(255, 255, 255) : RGB(0, 0, 0)) : cText); SelectObject(di->hDC, g_font); RECT tr = di->rcItem; tr.left += S(16);
        DrawTextW(di->hDC, kPageNames[di->itemID], -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE); if (di->itemState & ODS_FOCUS) { RECT f = di->rcItem; InflateRect(&f, -2, -2); DrawFocusRect(di->hDC, &f); } return TRUE; }
    case WM_SCAN_DONE: {
        std::unique_ptr<ScanMsg> r((ScanMsg*)l); if (r->gen != g_gen.load()) return 0;
        if (r->which == 0) g_su = std::move(r->su); else if (r->which == 1) g_sv = std::move(r->sv); else if (r->which == 2) g_dv = std::move(r->dv); else { g_cl = std::move(r->cl); g_scanning = false; }
        BuildCells();
        if (IsActionList(g_page)) ApplyView(); else if (g_page == P_DASH) { RebuildPanel(); UpdateStatus(); } else UpdateStatus();
        return 0; }
    case WM_FILE_PROGRESS: { std::unique_ptr<wstring> m2((wstring*)l); if (g_fsBusy) SetMsg(*m2); return 0; }
    case WM_FILE_DONE: { std::unique_ptr<FsDone> d((FsDone*)l); g_fsBusy = false; if (d->mode == 1) { g_dupes = std::move(d->scan); g_dupesDone = true; } else if (d->mode == 2) { g_empty = std::move(d->scan); g_emptyDone = true; } else { g_large = std::move(d->scan); g_largeDone = true; }
        auto& sc = d->mode == 1 ? g_dupes : d->mode == 2 ? g_empty : g_large; SetMsg(d->mode == 2 ? std::to_wstring(sc.files.size()) + L" empty folders found. " + sc.coverage : d->mode == 1 ? std::to_wstring(sc.groups) + L" duplicate groups found, " + FormatBytes(sc.reclaimable) + L" reclaimable. " + sc.coverage : std::to_wstring(sc.files.size()) + L" large files found. " + sc.coverage);
        BuildCells(); if (IsActionList(g_page)) ApplyView(); else if (g_page == P_DASH) RebuildPanel(); UpdateStatus(); UpdateToolbar(); return 0; }
    case WM_JOB_DONE: { std::unique_ptr<JobDone> j((JobDone*)l); SetMsg(j->summary); g_scanning = false; StartScan(); return 0; }
    case WM_ACTIVATE_PAGE: { if (IsIconic(h)) ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); if (w < P_COUNT) ShowPage((Page)w); return 0; }
    case WM_NOTIFY: {
        auto* nh = (NMHDR*)l;
        if (nh->hwndFrom == g_list) {
            switch (nh->code) {
            case NM_CUSTOMDRAW: return ListCustomDraw((LPNMLVCUSTOMDRAW)l);
            case LVN_GETDISPINFOW: { auto* di = (NMLVDISPINFOW*)l; auto& cells = g_cells[g_page];
                if ((di->item.mask & LVIF_TEXT) && di->item.iItem < (int)g_view.size() && g_view[di->item.iItem] < (int)cells.size() && di->item.iSubItem < (int)cells[g_view[di->item.iItem]].size())
                    wcsncpy_s(di->item.pszText, di->item.cchTextMax, cells[g_view[di->item.iItem]][di->item.iSubItem].c_str(), _TRUNCATE); return 0; }
            case LVN_ODFINDITEMW: return -1;
            case LVN_COLUMNCLICK: { int c = ((NMLISTVIEW*)l)->iSubItem; if (g_sortCol[g_page] == c) g_sortAsc[g_page] = !g_sortAsc[g_page]; else { g_sortCol[g_page] = c; g_sortAsc[g_page] = true; }
                for (int i : Selected()) g_keepSel.insert(KeyOf(g_page, i)); ApplyView(); return 0; }
            case LVN_ITEMCHANGED: if (((NMLISTVIEW*)l)->uChanged & LVIF_STATE) { UpdateStatus(); UpdateToolbar(); } return 0;
            case LVN_ITEMACTIVATE: ShowDetails(); return 0;
            case NM_RCLICK: { auto* a = (NMITEMACTIVATE*)l; LVHITTESTINFO ht{}; ht.pt = a->ptAction; int i = ListView_HitTest(g_list, &ht);
                if (i >= 0 && !(ListView_GetItemState(g_list, i, LVIS_SELECTED) & LVIS_SELECTED)) { ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED); ListView_SetItemState(g_list, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); }
                return 0; }
            }
        }
        return 0; }
    case WM_CONTEXTMENU: if ((HWND)w == g_list && IsActionList(g_page)) {
            POINT pt{(short)LOWORD(l), (short)HIWORD(l)};
            if (l == -1) { int f = ListView_GetNextItem(g_list, -1, LVNI_FOCUSED | LVNI_SELECTED); RECT r{}; if (f >= 0) ListView_GetItemRect(g_list, f, &r, LVIR_BOUNDS); pt = {r.left + S(20), r.bottom}; ClientToScreen(g_list, &pt); }
            ContextMenu(pt); return 0; } break;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == ID_NAV && code == LBN_SELCHANGE) { int s = (int)SendMessageW(g_nav, LB_GETCURSEL, 0, 0); if (s >= 0 && s != g_page) ShowPage((Page)s); return 0; }
        if (id == ID_SEARCH && code == EN_CHANGE) { wchar_t b[512]; GetWindowTextW(g_search, b, 512); g_query[g_page] = b; ApplyView(); return 0; }
        if (id >= C_REFRESH) DoCommand(id); return 0; }
    case WM_CLOSE: { SaveColumnWidths(); WINDOWPLACEMENT wp{sizeof wp}; if (GetWindowPlacement(h, &wp)) { auto& c = Cfg(); c.maximized = wp.showCmd == SW_SHOWMAXIMIZED; RECT r = wp.rcNormalPosition;
            c.winX = r.left; c.winY = r.top; c.winW = MulDiv(r.right - r.left, 96, g_dpi); c.winH = MulDiv(r.bottom - r.top, 96, g_dpi); } SaveSettings(); DestroyWindow(h); return 0; }
    case WM_DESTROY: ++g_gen; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
    g_inst = inst; wstring c = cmd ? cmd : L"";
    if (c.find(L"--cleanup") != wstring::npos) {   // used by the uninstaller: restore delayed entries, remove our tasks
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); wstring rep; CleanupManagedTasks(rep); return 0; }
    g_mtx = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance");
    Page req = P_DASH; bool haveReq = true; if (c.find(L"--services") != wstring::npos) req = P_SERVICES; else if (c.find(L"--startup") != wstring::npos) req = P_STARTUP; else if (c.find(L"--drivers") != wstring::npos) req = P_DRIVERS; else if (c.find(L"--cleaner") != wstring::npos) req = P_CLEAN; else if (c.find(L"--large") != wstring::npos) req = P_LARGE; else if (c.find(L"--duplicates") != wstring::npos) req = P_DUPES; else haveReq = false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { if (HWND e = FindWindowW(L"FeatherSystemUtilitiesWnd", nullptr)) PostMessageW(e, WM_ACTIVATE_PAGE, req, 0); return 0; }   // F004
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadSettings(); MigrateLegacyData(); P = PalFor(Cfg().theme);
    INITCOMMONCONTROLSEX ic{sizeof ic, ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES}; InitCommonControlsEx(&ic);
    DarkModeInit(Cfg().theme);
    WNDCLASSW wc{}; wc.hInstance = inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpfnWndProc = StProc; wc.lpszClassName = L"FsuStatus"; RegisterClassW(&wc);
    wc.lpfnWndProc = PanelProc; wc.lpszClassName = L"FsuPanel"; RegisterClassW(&wc);
    wc.lpfnWndProc = WndProc; wc.lpszClassName = L"FeatherSystemUtilitiesWnd"; wc.hbrBackground = nullptr; RegisterClassW(&wc);
    HDC sdc = GetDC(nullptr); int sdpi = GetDeviceCaps(sdc, LOGPIXELSX); ReleaseDC(nullptr, sdc);
    auto& cf = Cfg(); int W = MulDiv(cf.winW, sdpi, 96), H = MulDiv(cf.winH, sdpi, 96);
    HWND w = CreateWindowExW(0, wc.lpszClassName, kApp, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, cf.winX, cf.winY, W, H, nullptr, nullptr, inst, nullptr);
    ShowWindow(w, cf.maximized ? SW_SHOWMAXIMIZED : show); UpdateWindow(w);
    PostMessageW(w, WM_ACTIVATE_PAGE, haveReq ? req : (Page)cf.startPage, 0);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_KEYDOWN && GetForegroundWindow() == w) {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0; HWND f = GetFocus(); bool onList = f == g_list; bool listy = IsActionList(g_page) || g_page == P_HISTORY; bool inEdit = f == g_search;
            if (ctrl && m.wParam == 'F' && listy) { SetFocus(g_search); SendMessageW(g_search, EM_SETSEL, 0, -1); continue; }
            if (ctrl && m.wParam >= '0' && m.wParam <= '9') { ShowPage((Page)(m.wParam == '0' ? 9 : m.wParam - '1')); continue; }
            if (m.wParam == VK_F5) { DoCommand(C_REFRESH); continue; } if (m.wParam == VK_F1) { About(); continue; } if (ctrl && m.wParam == VK_OEM_COMMA) { ShowPage(P_SETTINGS); continue; }
            if (ctrl && !shift && m.wParam == 'Z' && !inEdit) { Undo(); continue; }
            if (ctrl && m.wParam == 'E' && listy) { ExportList(); continue; }
            if (!inEdit && IsActionList(g_page)) {
                if (ctrl && m.wParam == 'A') { SelectAll(); continue; }
                if (ctrl && shift && m.wParam == 'C') { CopyDetails(); continue; } if (ctrl && !shift && m.wParam == 'C' && onList) { CopyNames(); continue; }
                if (ctrl && m.wParam == 'N' && g_page == P_STARTUP) { AddEntry(); continue; }
                if (m.wParam == VK_DELETE && g_page == P_STARTUP && onList) { DeleteStartup(); continue; }
                if (m.wParam == VK_DELETE && IsFilePage(g_page) && onList) { RecycleSelected(); continue; }
                if (m.wParam == VK_RETURN && onList) { ShowDetails(); continue; }
                if (g_page == P_SERVICES && ctrl && shift) { if (m.wParam == 'S') { RunSvcOp(SvcOp::Start); continue; } if (m.wParam == 'T') { RunSvcOp(SvcOp::Stop); continue; } if (m.wParam == 'R') { RunSvcOp(SvcOp::Restart); continue; } if (m.wParam == 'K') { KillService(); continue; } }
            }
        }
        if (!IsDialogMessageW(w, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    if (g_mtx) CloseHandle(g_mtx); return 0;
}
