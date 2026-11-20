// Feather System Utilities 1.5 - one window: Dashboard, Startup, Services, Drivers, History, Settings.
// Native Win32, offline. All Windows changes go through the journal (see startup.cpp / services.cpp).
#include "app.h"
#include "../core/text.h"
#include "gfx.h"
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
#include <ctime>

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

enum Page { P_DASH, P_SPEED, P_TOOLBOX, P_STARTUP, P_SERVICES, P_DRIVERS, P_PROGS, P_CLEAN, P_LARGE, P_DUPES, P_EMPTY, P_HISTORY, P_SETTINGS, P_COUNT };
static const wchar_t* kPageNames[P_COUNT] = {L"Care", L"Speed Up", L"Toolbox", L"Startup", L"Services", L"Drivers", L"Programs", L"Custom Clean", L"Large files", L"Duplicates", L"Empty folders", L"Action Center", L"Settings"};
enum Cmd { C_REFRESH = 1000, C_ENABLE, C_DISABLE, C_DELETE, C_STARTLATER, C_ADD, C_RESTORE, C_UNDO, C_FILTER, C_MORE, C_SVC_START, C_SVC_STOP, C_SVC_RESTART, C_SVC_PAUSE, C_SVC_RESUME, C_SVC_KILL,
    C_STARTTYPE, C_PROFILES, C_DETAILS, C_COPYNAME, C_COPYDETAILS, C_OPENLOC, C_REGEDIT, C_VT, C_SEARCHONLINE, C_SELALL, C_ADMIN, C_SETTINGS, C_ABOUT, C_EXIT, C_OPENBACKUPS, C_OPENDATA, C_EXPORT,
    C_ST_AUTO, C_ST_AUTODELAY, C_ST_MANUAL, C_ST_DISABLED, C_PROF_SAVESEL, C_PROF_SNAPSHOT, C_PROF_OPENDIR, C_REMOVE_DELAY, C_CLEAN, C_CLEAN_SELALL, C_CLEAN_RESTORE, C_FS_SCAN, C_FS_STOP, C_FS_FOLDERS, C_FS_MIN, C_FS_AUTO, C_FS_RECYCLE, C_FS_DELETE, C_FS_FOLDER_ADD, C_FS_FOLDER_DEFAULT, C_PG_UNINSTALL, C_PG_SILENT, C_PG_FORCE, C_CHECKUPDATE, C_CLEAN_DEEP, C_CLEAN_ALL, C_CLEAN_NONE, C_CLEAN_REC,
    C_FILTER_BASE = 1100, C_PROF_BASE = 1150, C_RESTORE_BASE = 1200, C_DELAY_BASE = 1300, C_REGRESTORE_BASE = 1400, C_FS_MIN_BASE = 1500, C_FS_AUTO_BASE = 1520, C_FS_RM_BASE = 1560 };
enum { ID_NAV = 100, ID_SEARCH = 102, ID_LIST, ID_STATUS, ID_PANEL, ID_BANBTN = 120, ID_BANLINK = 121, ID_SUB0 = 200 };
enum { WM_SCAN_DONE = WM_APP + 1, WM_ACTIVATE_PAGE, WM_JOB_DONE, WM_FILE_DONE, WM_FILE_PROGRESS, WM_UNINSTALL_DONE, WM_DEEP_DONE, WM_UPDATE_INFO, WM_UPDATE_READY };
static const int kDelays[] = {15, 30, 60, 120, 300};

// ---- state ----
static HINSTANCE g_inst; static HWND g_main, g_nav, g_search, g_list, g_hdr, g_status, g_panel;
static HFONT g_font, g_fontBig, g_fontTitle; static UINT g_dpi = 96; static HANDLE g_mtx;
static HBRUSH g_brBg, g_brBar, g_brRail;
static Page g_page = P_DASH; static bool g_elevated = false; static wstring g_user;
static StartupScan g_su; static ServiceScan g_sv, g_dv; static std::set<wstring> g_checked; static std::vector<CleanItem> g_deep;
static CleanScan g_cl; static ProgramScan g_pg; static std::vector<CleanItem> g_left; static bool g_unBusy = false, g_gotoLeft = false;
static void MergeLeftovers(); static FileScan g_large, g_dupes, g_empty; static bool g_largeDone = false, g_dupesDone = false, g_emptyDone = false, g_fsBusy = false; static std::atomic<bool> g_fsCancel{false};
static std::vector<std::vector<wstring>> g_cells[P_COUNT];
static std::vector<int> g_view;
static int g_filter[P_COUNT]; static wstring g_query[P_COUNT];
static int g_sortCol[P_COUNT] = {0, 0, 0, 1, 0, 0, 0, 4, 1, 3, 0, 0, 0}; static bool g_sortAsc[P_COUNT] = {true, true, true, true, true, true, true, false, false, true, true, false, true};
static std::atomic<int> g_gen{0}; static bool g_scanning = false;
static wstring g_stMsg; static std::set<wstring> g_keepSel;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

struct UndoItem { int kind; StartupEntry se; bool wasEnabled; wstring name; DWORD type; bool delayed; int group; };
static std::vector<UndoItem> g_undo; static int g_groupSeq = 0;
struct ScanMsg { int gen; StartupScan su; ServiceScan sv, dv; CleanScan cl; ProgramScan pr; int which; };
struct JobDone { wstring summary; bool deep = false; ULONGLONG freed = 0; };

// ---- columns ----
struct Col { const wchar_t* name; int w; bool right; };
static const Col kStartupCols[] = {{L"Name", 230, false}, {L"Status", 80, false}, {L"Sign-in impact", 105, false}, {L"Warnings", 150, false}, {L"Publisher", 190, false}, {L"Type", 200, false},
    {L"Starts", 130, false}, {L"For", 100, false}, {L"Command", 640, false}, {L"Location", 330, false}};
static const Col kServiceCols[] = {{L"Name", 250, false}, {L"Service name", 170, false}, {L"Status", 80, false}, {L"Start type", 150, false}, {L"Safety", 85, false}, {L"PID", 60, true},
    {L"Log on as", 130, false}, {L"Description", 520, false}, {L"Warnings", 200, false}};
static const Col kDriverCols[] = {{L"Name", 250, false}, {L"Driver name", 170, false}, {L"Status", 80, false}, {L"Start type", 110, false}, {L"Class", 100, false}, {L"Type", 130, false},
    {L"Description", 360, false}, {L"Path", 460, false}, {L"Warnings", 160, false}};
static const Col kProgCols[] = {{L"Name", 300, false}, {L"Publisher", 190, false}, {L"Version", 110, false}, {L"Installed", 90, false}, {L"Size", 90, true}, {L"Type", 170, false}, {L"For", 90, false}, {L"Location", 380, false}, {L"Uninstall command", 420, false}};
static const Col kCleanCols[] = {{L"", 36, false}, {L"Item", 290, false}, {L"Type", 200, false}, {L"Risk", 70, false}, {L"Size", 90, true}, {L"Detail", 150, false}, {L"Location", 400, false}, {L"Why / note", 480, false}};
static const Col kEmptyCols[] = {{L"Folder", 300, false}, {L"Subfolders", 90, true}, {L"Modified", 130, false}, {L"Location", 700, false}};
static const Col kLargeCols[] = {{L"Name", 320, false}, {L"Size", 90, true}, {L"Modified", 130, false}, {L"Type", 80, false}, {L"Location", 620, false}};
static const Col kDupCols[] = {{L"Name", 300, false}, {L"Size", 90, true}, {L"Modified", 130, false}, {L"Group", 140, false}, {L"Content fingerprint (SHA-256)", 210, false}, {L"Location", 560, false}};
static const Col kHistCols[] = {{L"Time (UTC)", 160, false}, {L"Item", 260, false}, {L"Before", 160, false}, {L"After", 240, false}, {L"Result", 520, false}};
static const Col* ColsFor(Page p, int& n) {
    if (p == P_STARTUP) { n = 10; return kStartupCols; } if (p == P_SERVICES) { n = 9; return kServiceCols; } if (p == P_DRIVERS) { n = 9; return kDriverCols; } if (p == P_PROGS) { n = 9; return kProgCols; } if (p == P_CLEAN) { n = 8; return kCleanCols; } if (p == P_EMPTY) { n = 4; return kEmptyCols; } if (p == P_LARGE) { n = 5; return kLargeCols; } if (p == P_DUPES) { n = 6; return kDupCols; } n = 5; return kHistCols;
}
static const wchar_t* kStartupFilters[] = {L"Everything", L"Enabled", L"Disabled", L"Needs attention (warnings)", L"New in the last 7 days", L"Not from Microsoft", L"Registry (Run keys)", L"Startup folders", L"Scheduled tasks", L"Explorer add-ons", L"Delayed by Feather", L"Windows system items only"};
static const wchar_t* kServiceFilters[] = {L"All services", L"Running", L"Stopped", L"Automatic but not running", L"Disabled", L"Third-party only", L"Critical to Windows", L"Needs attention (warnings)"};
static const wchar_t* kDriverFilters[] = {L"All drivers", L"Running", L"Stopped", L"Boot/System start", L"Third-party only", L"Needs attention (warnings)"};
static const wchar_t* kCleanFilters[] = {L"Everything", L"Temporary files", L"Registry entries", L"Leftovers of uninstalled programs"};
static const wchar_t* kProgFilters[] = {L"All programs", L"Desktop apps (EXE)", L"Windows Installer (MSI)", L"Installed for all users", L"Installed for current user", L"Larger than 500 MB", L"Installed in the last 30 days", L"Not from Microsoft"};
static const wchar_t* kLargeFilters[] = {L"Everything", L"Videos", L"Archives and disk images", L"Installers and apps", L"Pictures", L"Audio", L"Documents", L"Other"};
static int FilterCount(Page p) { return p == P_STARTUP ? 12 : p == P_SERVICES ? 8 : p == P_DRIVERS ? 6 : p == P_CLEAN ? 4 : p == P_LARGE ? 8 : p == P_PROGS ? 8 : 0; }
static const wchar_t* FilterName(Page p) { return p == P_STARTUP ? kStartupFilters[g_filter[p]] : p == P_SERVICES ? kServiceFilters[g_filter[p]] : p == P_DRIVERS ? kDriverFilters[g_filter[p]] : p == P_CLEAN ? kCleanFilters[g_filter[p]] : p == P_LARGE ? kLargeFilters[g_filter[p]] : p == P_PROGS ? kProgFilters[g_filter[p]] : L""; }
static bool IsSvcPage(Page p) { return p == P_SERVICES || p == P_DRIVERS; }
static bool IsFilePage(Page p) { return p == P_LARGE || p == P_DUPES || p == P_EMPTY; }
static bool IsRegItem(const CleanItem& e) { return e.kind == 2 || e.kind == 3; }
static const wchar_t* RiskName(int r) { return r == 2 ? L"High" : r == 1 ? L"Medium" : L"Low"; }
static bool IsActionList(Page p) { return p == P_STARTUP || p == P_SERVICES || p == P_DRIVERS || p == P_PROGS || p == P_CLEAN || IsFilePage(p); }
static FileScan& FS(Page p) { return p == P_DUPES ? g_dupes : p == P_EMPTY ? g_empty : g_large; }
static int ExtCat(const wstring& e) {
    static const std::map<wstring, int> m = {{L"mp4",1},{L"mkv",1},{L"avi",1},{L"mov",1},{L"wmv",1},{L"flv",1},{L"webm",1},{L"m4v",1},{L"zip",2},{L"rar",2},{L"7z",2},{L"tar",2},{L"gz",2},{L"iso",2},{L"vhd",2},{L"vhdx",2},{L"img",2},{L"wim",2},{L"cab",2},
        {L"exe",3},{L"msi",3},{L"msix",3},{L"appx",3},{L"jpg",4},{L"jpeg",4},{L"png",4},{L"gif",4},{L"bmp",4},{L"tif",4},{L"tiff",4},{L"heic",4},{L"psd",4},{L"raw",4},{L"mp3",5},{L"wav",5},{L"flac",5},{L"aac",5},{L"m4a",5},
        {L"pdf",6},{L"doc",6},{L"docx",6},{L"xls",6},{L"xlsx",6},{L"ppt",6},{L"pptx",6},{L"txt",6}};
    auto it = m.find(e); return it == m.end() ? 7 : it->second;
}
static std::vector<ServiceEntry>& SvcVec(Page p) { return p == P_DRIVERS ? g_dv.entries : g_sv.entries; }

static const COLORREF kBtnTop = RGB(122, 190, 255), kBtnBot = RGB(64, 138, 232), kGreenTop = RGB(104, 218, 154), kGreenBot = RGB(48, 168, 108);
// ================= navigation: sidebar groups, sub-tabs, banner =================
struct NavGroup { const wchar_t* label; int icon; std::vector<Page> pages; };
static const std::vector<NavGroup>& Nav() {
    static const std::vector<NavGroup> v = {{L"Care", gfx::I_CARE, {P_DASH}}, {L"Custom Clean", gfx::I_CLEAN, {P_CLEAN, P_LARGE, P_DUPES, P_EMPTY}}, {L"Speed Up", gfx::I_SPEED, {P_SPEED, P_STARTUP, P_SERVICES, P_DRIVERS}},
        {L"Software", gfx::I_SOFT, {P_PROGS}}, {L"Toolbox", gfx::I_TOOLBOX, {P_TOOLBOX}}, {L"Action Center", gfx::I_ACTION, {P_HISTORY}}, {L"Settings", gfx::I_SETTINGS, {P_SETTINGS}}};
    return v;
}
static int GroupOf(Page p) { auto& n = Nav(); for (size_t i = 0; i < n.size(); ++i) for (Page q : n[i].pages) if (q == p) return (int)i; return 0; }
static Page g_lastInGroup[8] = {P_DASH, P_CLEAN, P_SPEED, P_PROGS, P_TOOLBOX, P_HISTORY, P_SETTINGS, P_SETTINGS};
static const wchar_t* SubLabel(Page p) { return p == P_SPEED ? L"Overview" : p == P_CLEAN ? L"Cleaner" : kPageNames[p]; }
static bool IsPanelPage(Page p) { return p == P_DASH || p == P_SPEED || p == P_TOOLBOX || p == P_SETTINGS; }
static bool HasBanner(Page p) { return p == P_CLEAN || p == P_LARGE || p == P_DUPES || p == P_EMPTY || p == P_PROGS; }
static bool g_kbd = false;
static int g_subTop = 0, g_subH = 0, g_banTop = 0, g_banH = 0, g_tbTop = 0, g_tbH = 0, g_bodyTop = 0;
static HWND g_banBtn = nullptr, g_banLink = nullptr, g_hotSub = nullptr; static std::vector<HWND> g_subBtns; static wstring g_banHead, g_banSub, g_banBtnText, g_banLinkText; static int g_banCmd = 0, g_banLinkCmd = 0;
static void UpdateBanner();
static bool CanDeleteEntry(const StartupEntry& e);
static void ComputeLayout() {
    int y = 0; bool sub = Nav()[GroupOf(g_page)].pages.size() > 1; g_subTop = y; g_subH = sub ? S(52) : 0; y += g_subH;
    g_banTop = y; g_banH = HasBanner(g_page) ? S(78) : 0; y += g_banH; g_tbTop = y; g_tbH = IsPanelPage(g_page) ? 0 : S(44); y += g_tbH; g_bodyTop = y;
}
static RECT TbStrip() { RECT r; GetClientRect(g_main, &r); r.left = S(190); r.top = g_tbTop; r.bottom = g_tbTop + g_tbH; return r; }
static LRESULT CALLBACK SubProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_MOUSEMOVE && g_hotSub != h) { HWND o = g_hotSub; g_hotSub = h; InvalidateRect(h, nullptr, FALSE); if (o) InvalidateRect(o, nullptr, FALSE); TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0}; TrackMouseEvent(&t); }
    else if (m == WM_MOUSELEAVE && g_hotSub == h) { g_hotSub = nullptr; InvalidateRect(h, nullptr, FALSE); }
    return DefSubclassProc(h, m, w, l);
}
static void BuildSubTabs() {
    for (HWND b : g_subBtns) DestroyWindow(b); g_subBtns.clear(); auto& pages = Nav()[GroupOf(g_page)].pages; if (pages.size() < 2) return;
    HWND prev = g_nav; for (size_t i = 0; i < pages.size(); ++i) { HWND b = CreateWindowExW(0, L"BUTTON", SubLabel(pages[i]), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, g_main, (HMENU)(INT_PTR)(ID_SUB0 + i), g_inst, nullptr);
        SendMessageW(b, WM_SETFONT, (WPARAM)g_font, TRUE); SetWindowSubclass(b, SubProc, 3, 0); SetWindowPos(b, prev, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); prev = b; g_subBtns.push_back(b); }
}
static void DrawSubTab(DRAWITEMSTRUCT* di) {
    auto& pages = Nav()[GroupOf(g_page)].pages; size_t idx = di->CtlID - ID_SUB0; if (idx >= pages.size()) return; bool sel = pages[idx] == g_page, hot = g_hotSub == di->hwndItem; RECT r = di->rcItem; FillRect(di->hDC, &r, g_brBg);
    int rad = (r.bottom - r.top) / 2; if (sel) gfx::RoundRect(di->hDC, r, rad, kBtnTop, kBtnBot, CLR_INVALID); else gfx::RoundRect(di->hDC, r, rad, hot ? cHot : cBar, hot ? cHot : cBar, hot ? cAccent : cGrid);
    SetBkMode(di->hDC, TRANSPARENT); SelectObject(di->hDC, g_font); SetTextColor(di->hDC, sel ? RGB(10, 24, 60) : cText); DrawTextW(di->hDC, SubLabel(pages[idx]), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((di->itemState & ODS_FOCUS) && g_kbd) { RECT f = r; InflateRect(&f, -S(3), -S(3)); DrawFocusRect(di->hDC, &f); }
}
static void DrawBannerBtn(DRAWITEMSTRUCT* di) {
    RECT r = di->rcItem; bool dis = (di->itemState & ODS_DISABLED) != 0, link = di->CtlID == ID_BANLINK; wchar_t t[128]; GetWindowTextW(di->hwndItem, t, 128); FillRect(di->hDC, &r, g_brBg); SetBkMode(di->hDC, TRANSPARENT); SelectObject(di->hDC, g_font);
    if (link) { SetTextColor(di->hDC, dis ? cDim : cAccent); DrawTextW(di->hDC, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); return; }
    bool press = (di->itemState & ODS_SELECTED) != 0; gfx::RoundRect(di->hDC, r, S(10), dis ? cGrid : (press ? kGreenBot : kGreenTop), dis ? cGrid : kGreenBot, CLR_INVALID); SelectObject(di->hDC, g_fontTitle); SetTextColor(di->hDC, dis ? cDim : RGB(8, 40, 24)); DrawTextW(di->hDC, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((di->itemState & ODS_FOCUS) && g_kbd) { RECT f = r; InflateRect(&f, -S(4), -S(4)); DrawFocusRect(di->hDC, &f); }
}
static void DrawSidebarItem(DRAWITEMSTRUCT* di) {
    if (di->itemID == (UINT)-1) return; auto& g = Nav(); if (di->itemID >= g.size()) return; bool sel = (int)di->itemID == GroupOf(g_page); RECT r = di->rcItem; FillRect(di->hDC, &r, g_brRail);
    RECT tile = {r.left + S(10), r.top + S(3), r.right - S(10), r.bottom - S(3)}; if (sel) gfx::RoundRect(di->hDC, tile, S(8), cRailSel, cRailSel, CLR_INVALID); else if (di->itemState & ODS_SELECTED) gfx::RoundRect(di->hDC, tile, S(8), cHot, cHot, CLR_INVALID);
    RECT ic = {tile.left + S(14), tile.top + S(9), tile.left + S(14) + S(24), tile.top + S(9) + S(24)}; gfx::Icon(di->hDC, g[di->itemID].icon, ic, sel ? RGB(255, 255, 255) : cText, 1.8f);
    SetBkMode(di->hDC, TRANSPARENT); SelectObject(di->hDC, g_font); SetTextColor(di->hDC, sel ? RGB(255, 255, 255) : cText); RECT tr = {tile.left + S(52), tile.top, tile.right - S(6), tile.bottom}; DrawTextW(di->hDC, g[di->itemID].label, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((di->itemState & ODS_FOCUS) && g_kbd) { RECT f = tile; InflateRect(&f, -2, -2); DrawFocusRect(di->hDC, &f); }
}

// ================= toolbar: native owner-drawn buttons (keyboard + screen-reader accessible) =================
struct TbItem { int id; wstring text; bool enabled = true, dropdown = false, sep = false, right = false; HWND h = nullptr; RECT r{}; };
static std::vector<TbItem> g_items; static HWND g_hotBtn = nullptr; static int g_tbLeft = 0;

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
    for (auto& it : g_items) { int w = widthOf(it); int& px = it.right ? rx : x; it.r = {px, st.top, px + w, st.bottom}; if (it.h) MoveWindow(it.h, px, st.top, w, st.bottom - st.top, TRUE); px += w; }
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
        wstring msg = g_stMsg.empty() ? (g_page == P_STARTUP ? L"Tip: right-click an entry. Every change is journaled; Ctrl+Z undoes it." : g_page == P_SERVICES ? L"Tip: select several services (Ctrl/Shift) to act on them together." : g_page == P_DRIVERS ? L"Drivers are shown read-only." : IsFilePage(g_page) ? L"Move to Recycle Bin (restorable) or Delete permanently. At least one copy per duplicate group is always kept." : g_page == P_CLEAN ? L"Select items, then Clean selected. Temp files are deleted permanently; registry entries are backed up first." : L"") : g_stMsg;
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
static wstring EntryName(Page p, int i) { return p == P_STARTUP ? g_su.entries[i].name : p == P_PROGS ? g_pg.entries[i].name : p == P_CLEAN ? g_cl.items[i].name : IsFilePage(p) ? FS(p).files[i].path : SvcVec(p)[i].display; }

// ================= data -> rows =================
static void BuildCells() {
    g_cells[P_STARTUP].clear(); for (auto& e : g_su.entries) g_cells[P_STARTUP].push_back({e.name, e.state, L"Not measured", e.warnings, e.publisher, e.type, e.starts, e.scope, e.command, e.location});
    g_cells[P_SERVICES].clear(); for (auto& e : g_sv.entries) g_cells[P_SERVICES].push_back({e.display, e.name, e.state, e.start, e.safety, e.pid ? std::to_wstring(e.pid) : L"", e.account, e.description, e.warnings});
    g_cells[P_PROGS].clear(); for (auto& e : g_pg.entries) g_cells[P_PROGS].push_back({e.name, e.publisher, e.version, e.installDate, e.sizeBytes ? FormatBytes(e.sizeBytes) : wstring(), e.type, e.scope, e.installLocation, e.uninstallString});
    g_cells[P_CLEAN].clear(); for (auto& e : g_cl.items) g_cells[P_CLEAN].push_back({g_checked.count(e.id) ? L"\u2611" : L"\u2610", e.name, e.type, RiskName(e.risk), FormatBytes(e.bytes), e.detail, e.location, e.why});
    g_cells[P_EMPTY].clear(); for (auto& e : g_empty.files) g_cells[P_EMPTY].push_back({e.name, std::to_wstring(e.groupCount), FileDateText(e.mtime), e.folder});
    g_cells[P_LARGE].clear(); for (auto& e : g_large.files) g_cells[P_LARGE].push_back({e.name, FormatBytes(e.size), FileDateText(e.mtime), e.ext.empty() ? wstring(L"(none)") : e.ext, e.folder});
    g_cells[P_DUPES].clear(); for (auto& e : g_dupes.files) g_cells[P_DUPES].push_back({e.name, FormatBytes(e.size), FileDateText(e.mtime), L"Group " + std::to_wstring(e.group) + L" \u00B7 " + std::to_wstring(e.groupCount) + L" copies", e.hash.size() >= 16 ? e.hash.substr(0, 16) + L"…" : e.hash, e.folder});
    g_cells[P_DRIVERS].clear(); for (auto& e : g_dv.entries) g_cells[P_DRIVERS].push_back({e.display, e.name, e.state, e.start, e.safety, e.ptype, e.description, e.path, e.warnings});
}
static bool PassFilter(Page p, int idx) {
    int f = g_filter[p];
    if (p == P_LARGE) { int c = ExtCat(g_large.files[idx].ext); return f == 0 || c == f; }
    if (p == P_DUPES) return true;
    if (p == P_CLEAN) { auto& e = g_cl.items[idx]; return f == 0 || (f == 1 && !IsRegItem(e)) || (f == 2 && IsRegItem(e)) || (f == 3 && e.type.rfind(L"Leftover", 0) == 0); }
    if (p == P_PROGS) { auto& e = g_pg.entries[idx]; static wstring d30; if (d30.empty() || f == 6) { FILETIME ft; GetSystemTimeAsFileTime(&ft); ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; u.QuadPart -= 30ULL * 24 * 3600 * 10000000ULL; ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart; SYSTEMTIME st; FileTimeToSystemTime(&ft, &st); wchar_t b[16]; swprintf(b, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay); d30 = b; }
        switch (f) { case 1: return !e.isMsi; case 2: return e.isMsi; case 3: return e.scope == L"All users"; case 4: return e.scope == L"Current user"; case 5: return e.sizeBytes > 500ull * 1024 * 1024; case 6: return e.installDate >= d30; case 7: return Lower(e.publisher).find(L"microsoft") == wstring::npos; } return true; }
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
static wstring KeyOf(Page p, int idx) { return p == P_STARTUP ? g_su.entries[idx].key : p == P_HISTORY ? L"" : p == P_CLEAN ? g_cl.items[idx].id : p == P_PROGS ? g_pg.entries[idx].key : IsFilePage(p) ? FS(p).files[idx].path : SvcVec(p)[idx].name; }
static void UpdateStatus(); static void UpdateToolbar();
static void ApplyView() {
    Page p = g_page; if (!IsActionList(p) && p != P_HISTORY) return;
    g_view.clear(); auto& cells = g_cells[p];
    for (size_t i = 0; i < cells.size(); ++i) if (PassFilter(p, (int)i) && MatchesQuery(cells[i], g_query[p])) g_view.push_back((int)i);
    int col = g_sortCol[p]; bool asc = g_sortAsc[p]; bool statusCol = ((p == P_STARTUP && col == 1) || (IsSvcPage(p) && col == 2)); bool numCol = p == P_SERVICES && col == 5; bool sizeCol = (p == P_CLEAN && col == 4) || (p == P_PROGS && col == 4); bool fsSize = IsFilePage(p) && p != P_EMPTY && col == 1, fsDate = IsFilePage(p) && col == 2, grpCol = p == P_DUPES && col == 3, emptyCnt = p == P_EMPTY && col == 1;
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
        else if (sizeCol) { ULONGLONG bx = p == P_PROGS ? g_pg.entries[a].sizeBytes : g_cl.items[a].bytes, by = p == P_PROGS ? g_pg.entries[b].sizeBytes : g_cl.items[b].bytes; c = bx < by ? -1 : bx > by ? 1 : 0; }
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
    } else if (p == P_PROGS) { ULONGLONG tot = 0; for (auto& e : g_pg.entries) tot += e.sizeBytes; s = std::to_wstring(g_pg.entries.size()) + L" programs \u00B7 " + FormatBytes(tot) + L" (sizes reported by the programs)"; if (g_view.size() != g_pg.entries.size()) s += L" \u00B7 " + std::to_wstring(g_view.size()) + L" shown"; if (g_unBusy) s += L" \u00B7 uninstalling\u2026";
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
    if (IsPanelPage(g_page)) { TbSet({}); return; }
    std::vector<TbItem> it;
    auto add = [&](int id, const wchar_t* t, bool dd = false, bool right = false) { TbItem x; x.id = id; x.text = t; x.dropdown = dd; x.right = right; it.push_back(x); };
    auto sep = [&]() { TbItem x; x.id = 0; x.sep = true; it.push_back(x); };
    if (g_page == P_STARTUP) { add(C_REFRESH, L"Refresh"); sep(); add(C_ENABLE, L"Enable"); add(C_DISABLE, L"Disable"); add(C_DELETE, L"Delete"); add(C_STARTLATER, L"Start later", true); sep(); add(C_ADD, L"Add…"); add(C_RESTORE, L"Restore…"); add(C_UNDO, L"Undo"); }
    else if (g_page == P_SERVICES) { add(C_REFRESH, L"Refresh"); sep(); add(C_SVC_START, L"Start"); add(C_SVC_STOP, L"Stop"); add(C_SVC_RESTART, L"Restart"); add(C_SVC_PAUSE, L"Pause"); add(C_SVC_RESUME, L"Resume"); add(C_SVC_KILL, L"Kill"); sep(); add(C_STARTTYPE, L"Start type", true); add(C_PROFILES, L"Profiles", true); add(C_UNDO, L"Undo"); }
    else if (g_page == P_DRIVERS) { add(C_REFRESH, L"Refresh"); add(C_DETAILS, L"Properties"); }
    else if (g_page == P_PROGS) { add(C_REFRESH, L"Refresh"); sep(); add(C_PG_UNINSTALL, L"Uninstall"); add(C_PG_SILENT, L"Uninstall silently"); add(C_PG_FORCE, L"Force remove\u2026"); sep(); add(C_OPENLOC, L"Open folder"); add(C_DETAILS, L"Details"); }
    else if (IsFilePage(g_page)) { add(C_FS_SCAN, L"Scan"); add(C_FS_STOP, L"Stop"); sep(); add(C_FS_FOLDERS, L"Folders\u2026", true); if (g_page != P_EMPTY) add(C_FS_MIN, L"Minimum size", true); sep(); if (g_page == P_DUPES) add(C_FS_AUTO, L"Auto-select", true); add(C_FS_RECYCLE, L"Move to Recycle Bin"); add(C_FS_DELETE, L"Delete permanently"); add(C_OPENLOC, L"Open location"); }
    else if (g_page == P_CLEAN) { add(C_REFRESH, L"Scan again"); add(C_CLEAN_DEEP, L"Deep scan\u2026"); sep(); add(C_CLEAN_ALL, L"Select all"); add(C_CLEAN_REC, L"Recommended"); add(C_CLEAN_NONE, L"Select none"); sep(); add(C_CLEAN_RESTORE, L"Restore registry\u2026", true); }
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
            if (CanDeleteEntry(e)) canDel = true; if (CanDelay(e)) canDelay = true; if (e.managedDelay) canUndelay = true; }
        TbEnable(C_ENABLE, canEn); TbEnable(C_DISABLE, canDis); TbEnable(C_DELETE, canDel); TbEnable(C_STARTLATER, canDelay || canUndelay); TbEnable(C_ADD, true); TbEnable(C_RESTORE, true);
    } else if (g_page == P_SERVICES) {
        bool st = false, sp = false, rs = false, pa = false, re = false, kl = false;
        for (int i : sel) { auto& e = g_sv.entries[i]; if (e.stateCode == SERVICE_STOPPED) st = true; if (e.stateCode == SERVICE_RUNNING) { rs = true; if (e.controls & SERVICE_ACCEPT_STOP) sp = true; if (e.controls & SERVICE_ACCEPT_PAUSE_CONTINUE) pa = true; if (sel.size() == 1 && e.pid && (e.svcType & SERVICE_WIN32_OWN_PROCESS) && !e.critical) kl = true; } if (e.stateCode == SERVICE_PAUSED) re = true; }
        TbEnable(C_SVC_START, st); TbEnable(C_SVC_STOP, sp); TbEnable(C_SVC_RESTART, sp && rs); TbEnable(C_SVC_PAUSE, pa); TbEnable(C_SVC_RESUME, re); TbEnable(C_SVC_KILL, kl); TbEnable(C_STARTTYPE, any); TbEnable(C_PROFILES, true);
    } else if (g_page == P_DRIVERS) TbEnable(C_DETAILS, any);
    else if (g_page == P_PROGS) { TbEnable(C_PG_UNINSTALL, any && !g_unBusy); TbEnable(C_PG_SILENT, any && !g_unBusy); TbEnable(C_PG_FORCE, any && !g_unBusy); TbEnable(C_OPENLOC, sel.size() == 1); TbEnable(C_DETAILS, sel.size() >= 1); }
    else if (IsFilePage(g_page)) { TbEnable(C_FS_SCAN, !g_fsBusy); TbEnable(C_FS_STOP, g_fsBusy); TbEnable(C_FS_FOLDERS, !g_fsBusy); TbEnable(C_FS_MIN, !g_fsBusy); TbEnable(C_FS_AUTO, !g_fsBusy && !g_dupes.files.empty()); TbEnable(C_FS_RECYCLE, any && !g_fsBusy); TbEnable(C_FS_DELETE, any && !g_fsBusy); TbEnable(C_OPENLOC, any); }
    else if (g_page == P_CLEAN) { TbEnable(C_CLEAN_DEEP, !g_fsBusy); TbEnable(C_CLEAN_ALL, !g_cl.items.empty()); TbEnable(C_CLEAN_REC, !g_cl.items.empty()); TbEnable(C_CLEAN_NONE, !g_checked.empty()); TbEnable(C_CLEAN_RESTORE, true); }
    TbEnable(C_UNDO, !g_undo.empty()); UpdateBanner();
}

static void UpdateBanner() {
    if (!g_banBtn) return; Page p = g_page; g_banHead.clear(); g_banSub.clear(); g_banBtnText.clear(); g_banLinkText.clear(); g_banCmd = 0; g_banLinkCmd = 0; bool enable = true, linkOn = true;
    if (p == P_CLEAN) { size_t chk = 0; ULONGLONG cb = 0; for (auto& e : g_cl.items) if (g_checked.count(e.id)) { ++chk; cb += e.bytes; }
        g_banHead = FormatBytes(g_cl.totalBytes) + L" can be cleaned, " + std::to_wstring(g_cl.registryCount) + L" invalid registry entries found."; g_banSub = std::to_wstring(chk) + L" item(s) ticked (" + FormatBytes(cb) + L"). Tick what you want, then press Clean."; g_banBtnText = L"Clean"; g_banCmd = C_CLEAN; enable = chk > 0 && !g_scanning; g_banLinkText = L"Scan again"; g_banLinkCmd = C_REFRESH; }
    else if (IsFilePage(p)) { bool done = p == P_DUPES ? g_dupesDone : p == P_EMPTY ? g_emptyDone : g_largeDone; auto& sc = FS(p);
        g_banHead = !done ? (p == P_DUPES ? L"Find identical files wasting space" : p == P_EMPTY ? L"Find folders with nothing in them" : L"Find your largest files") : p == P_DUPES ? std::to_wstring(sc.groups) + L" duplicate groups, " + FormatBytes(sc.reclaimable) + L" reclaimable" : p == P_EMPTY ? std::to_wstring(sc.files.size()) + L" empty folders" : std::to_wstring(sc.files.size()) + L" large files, " + FormatBytes(sc.totalBytes);
        g_banSub = g_fsBusy ? L"Scanning… you can press Stop at any time." : done ? (p == P_DUPES ? wstring(L"Matched by file CONTENT (SHA-256), never by size alone; verified byte-for-byte again before any deletion.") : sc.coverage.substr(0, 130)) : L"Choose folders and a minimum size in the toolbar, then press Scan."; g_banBtnText = g_fsBusy ? L"Scanning…" : L"Scan"; g_banCmd = C_FS_SCAN; enable = !g_fsBusy; g_banLinkText = g_fsBusy ? L"Stop" : L""; g_banLinkCmd = C_FS_STOP; linkOn = g_fsBusy; }
    else if (p == P_PROGS) { auto sel = Selected(); g_banHead = std::to_wstring(g_pg.entries.size()) + L" programs installed"; g_banSub = L"Select programs, then uninstall them. Leftover files and registry entries are searched for afterwards."; g_banBtnText = L"Uninstall"; g_banCmd = C_PG_UNINSTALL; enable = !sel.empty() && !g_unBusy; g_banLinkText = L"Uninstall silently"; g_banLinkCmd = C_PG_SILENT; linkOn = !sel.empty() && !g_unBusy; }
    bool show = HasBanner(p); ShowWindow(g_banBtn, show ? SW_SHOW : SW_HIDE); ShowWindow(g_banLink, show && !g_banLinkText.empty() ? SW_SHOW : SW_HIDE);
    if (show) { SetWindowTextW(g_banBtn, g_banBtnText.c_str()); SetWindowTextW(g_banLink, g_banLinkText.c_str()); EnableWindow(g_banBtn, enable); EnableWindow(g_banLink, linkOn); InvalidateRect(g_banBtn, nullptr, TRUE); InvalidateRect(g_banLink, nullptr, TRUE); }
    RECT br = {S(190), g_banTop, 4000, g_banTop + g_banH}; InvalidateRect(g_main, &br, FALSE);
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
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, ScanStartup(), {}, {}, {}, {}, 0});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, ScanServices(false), {}, {}, {}, 1});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, {}, ScanServices(true), {}, {}, 2});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, {}, {}, {}, ScanPrograms(), 4});
    PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM) new ScanMsg{gen, {}, {}, {}, ScanCleaner(Cfg().keepRecent), {}, 3});
    CoUninitialize(); return 0;
}
static void StartScan() {
    if (g_scanning) return;
    for (int i : Selected()) g_keepSel.insert(KeyOf(g_page, i));
    g_scanning = true; ++g_gen; UpdateStatus();
    HANDLE h = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr); if (h) CloseHandle(h); else g_scanning = false;
}

// ================= panels: Care (scan), Speed Up hub, Toolbox, Settings (custom painted, mouse + keyboard) =================
struct Tile { RECT r{}; wstring big, cap, desc; Page page = P_DASH; int filter = 0; COLORREF col = 0; int action = 0, style = 0, icon = 0, arg = 0; bool on = false; };
static std::vector<Tile> g_tiles; static int g_panelFocus = -1, g_panelHot = -1;
enum { A_NONE, A_THEME, A_DENSITY, A_HIDESYS, A_CONFIRM, A_KEEPRECENT, A_STARTPAGE, A_RESETCOLS, A_OPENDATA, A_CLEARHIST, A_ADMIN, A_AUTOUPD, A_GOTO, A_CMD, A_SCAN, A_STOPSCAN, A_MODE, A_CHECK, A_REVIEW };
enum { TS_ROW, TS_CARD, TS_BUTTON, TS_CHECK, TS_PILL, TS_SCAN, TS_TILE, TS_LINK, TS_HEAD };
struct CareStep { int id; wstring label, value; int state = 0; };
static std::vector<CareStep> g_steps; static bool g_careRunning = false, g_careDone = false, g_careManual = false, g_careStop = false; static std::vector<int> g_careQueue; static int g_careAngle = 0;
static bool g_careOn[10] = {true, true, true, true, true, true, false, false, false, false};
static const wchar_t* kCareMods[10] = {L"Junk File Clean", L"Registry Clean", L"Privacy Sweep", L"Startup Check", L"Service Check", L"Program Check", L"Large Files", L"Duplicate Files", L"Empty Folders", L"Deep Scan"};
static const wchar_t* kStepNames[10] = {L"Startup items", L"Services", L"Drivers", L"Installed programs", L"Junk files", L"Registry errors", L"Large files", L"Duplicate files", L"Empty folders", L"Deep scan (all drives)"};
static void CareStart(); static void CareStop(); static void CareReview(); static void StartDeepScan(bool ask); static void CheckForUpdates(bool manual); static void ShowPage(Page p); static void StartFileScan(int mode);
static const wchar_t* DensityName() { return Cfg().density ? L"Comfortable" : L"Compact"; }
static size_t CountIf(const std::vector<ServiceEntry>& v, bool (*f)(const ServiceEntry&)) { size_t n = 0; for (auto& e : v) if (f(e)) ++n; return n; }

static void AddTile(Tile t) { g_tiles.push_back(std::move(t)); }
static Tile MkTile(int style, int action, RECT r, const wstring& cap = L"", int arg = 0, Page pg = P_DASH, int filter = 0) { Tile t; t.style = style; t.action = action; t.r = r; t.cap = cap; t.arg = arg; t.page = pg; t.filter = filter; return t; }

static wstring CareValue(int id) {
    size_t warn = 0; for (auto& e : g_su.entries) if (e.hasWarning) ++warn; size_t itemsTemp = 0; for (auto& e : g_cl.items) if (e.kind != 2 && e.kind != 3) ++itemsTemp;
    switch (id) { case 0: return std::to_wstring(g_su.entries.size()) + L" entries, " + std::to_wstring(warn) + L" need attention"; case 1: { size_t t = CountIf(g_sv.entries, [](const ServiceEntry& e) { return e.safety == L"Third-party"; }); return std::to_wstring(g_sv.entries.size()) + L" services, " + std::to_wstring(t) + L" third-party"; }
        case 2: return std::to_wstring(g_dv.entries.size()) + L" drivers (read-only)"; case 3: return std::to_wstring(g_pg.entries.size()) + L" programs installed"; case 4: return FormatBytes(g_cl.totalBytes) + L" in " + std::to_wstring(itemsTemp) + L" groups";
        case 5: return std::to_wstring(g_cl.registryCount) + L" invalid entries"; case 6: return std::to_wstring(g_large.files.size()) + L" large files"; case 7: return std::to_wstring(g_dupes.groups) + L" groups, " + FormatBytes(g_dupes.reclaimable) + L" reclaimable";
        case 8: return std::to_wstring(g_empty.files.size()) + L" empty folders"; case 9: return L"done"; } return L"";
}
static void BuildCare(int w, int h) {
    g_tiles.clear(); int cx = w / 2; (void)h;
    if (g_careRunning) { AddTile(MkTile(TS_BUTTON, A_STOPSCAN, {w - S(160), S(430), w - S(30), S(466)}, L"Stop")); return; }
    if (g_careDone) {
        int y = S(112), rowH = S(40); static const Page pg[10] = {P_STARTUP, P_SERVICES, P_DRIVERS, P_PROGS, P_CLEAN, P_CLEAN, P_LARGE, P_DUPES, P_EMPTY, P_CLEAN};
        for (size_t i = 0; i < g_steps.size() && i < 10; ++i) { int id = g_steps[i].id; Tile t = MkTile(TS_LINK, A_GOTO, {w - S(150), y, w - S(30), y + rowH - S(8)}, L"View", id, pg[id], id == 5 ? 2 : id == 0 ? 3 : 0); AddTile(t); y += rowH; }
        AddTile(MkTile(TS_BUTTON, A_REVIEW, {S(30), y + S(14), S(30) + S(200), y + S(14) + S(40)}, L"Review && Clean")); AddTile(MkTile(TS_LINK, A_SCAN, {S(250), y + S(14), S(380), y + S(54)}, L"Scan again")); return; }
    int R = S(92); AddTile(MkTile(TS_SCAN, A_SCAN, {cx - R, S(70), cx + R, S(70) + 2 * R}, L"SCAN"));
    int py = S(70) + 2 * R + S(26), pw = S(130); AddTile(MkTile(TS_PILL, A_MODE, {cx - pw - S(4), py, cx - S(4), py + S(34)}, L"Smart mode", 0)); AddTile(MkTile(TS_PILL, A_MODE, {cx + S(4), py, cx + pw + S(4), py + S(34)}, L"Manual mode", 1));
    if (g_careManual) { int cols = w > S(900) ? 4 : 3, cw = (w - S(80)) / cols, y = py + S(58); for (int i = 0; i < 10; ++i) { int col = i % cols, row = i / cols; Tile t = MkTile(TS_CHECK, A_CHECK, {S(40) + col * cw, y + row * S(34), S(40) + (col + 1) * cw - S(8), y + row * S(34) + S(30)}, kCareMods[i], i); t.on = g_careOn[i]; AddTile(t); } }
}
static void BuildSpeed(int w, int h) {
    g_tiles.clear(); (void)h; int gap = S(16), cw = (w - S(32) - gap) / 2, ch = S(138), y = S(86); size_t warn = 0; for (auto& e : g_su.entries) if (e.hasWarning) ++warn; size_t run = CountIf(g_sv.entries, [](const ServiceEntry& e) { return e.stateCode == SERVICE_RUNNING; });
    struct C { const wchar_t* title; wstring desc; const wchar_t* btn; int icon; Page pg; int filter; int act; int cmd; } cards[] = {
        {L"Startup Optimizer", std::to_wstring(g_su.entries.size()) + L" startup entries, " + std::to_wstring(warn) + L" with warnings. Disable what you do not need, or delay apps (Start later) to speed up sign-in.", L"Manage", gfx::I_STARTUP, P_STARTUP, warn ? 3 : 0, A_GOTO, 0},
        {L"Services Manager", std::to_wstring(run) + L" of " + std::to_wstring(g_sv.entries.size()) + L" services running. Review third-party services, change start types, apply reviewed profiles.", L"Manage", gfx::I_SERVICES, P_SERVICES, 0, A_GOTO, 0},
        {L"Program Uninstaller", std::to_wstring(g_pg.entries.size()) + L" programs installed. Uninstall silently or step by step and remove leftover files, folders and registry entries.", L"Manage", gfx::I_TRASH, P_PROGS, 0, A_GOTO, 0},
        {L"Driver Inventory", std::to_wstring(g_dv.entries.size()) + L" drivers listed read-only, with signature and path information.", L"View", gfx::I_DRIVER, P_DRIVERS, 0, A_GOTO, 0},
        {L"Disk Cleaner", FormatBytes(g_cl.totalBytes) + L" of temporary and cache files found. Includes Windows Update cache, browser caches and a Deep Scan of all drives.", L"Clean", gfx::I_CLEAN, P_CLEAN, 0, A_GOTO, 0},
        {L"Keep Feather up to date", L"Checks the project's release page for a newer version and installs it after you approve (verified with SHA-256).", L"Check now", gfx::I_UPDATE, P_DASH, C_CHECKUPDATE, A_CMD, C_CHECKUPDATE}};
    for (int i = 0; i < 6; ++i) { int col = i % 2, row = i / 2; RECT r = {S(16) + col * (cw + gap), y + row * (ch + gap), S(16) + col * (cw + gap) + cw, y + row * (ch + gap) + ch}; Tile t = MkTile(TS_CARD, cards[i].act, r, cards[i].btn, 0, cards[i].pg, cards[i].act == A_CMD ? cards[i].cmd : cards[i].filter); t.big = cards[i].title; t.desc = cards[i].desc; t.icon = cards[i].icon; AddTile(t); }
}
static void BuildToolbox(int w, int h) {
    g_tiles.clear(); (void)h; int cols = 3, gap = S(16), cw = (w - S(32) - gap * (cols - 1)) / cols, top = S(120), th = S(66);
    struct T { int col; const wchar_t* label; int icon; Page pg; int filter; int act; int cmd; } t[] = {
        {0, L"Disk Cleaner", gfx::I_CLEAN, P_CLEAN, 0, A_GOTO, 0}, {0, L"Registry Cleaner", gfx::I_REGISTRY, P_CLEAN, 2, A_GOTO, 0}, {0, L"Deep Scan", gfx::I_DEEP, P_CLEAN, 0, A_CMD, C_CLEAN_DEEP}, {0, L"Large File Finder", gfx::I_LARGE, P_LARGE, 0, A_GOTO, 0}, {0, L"Duplicate Finder", gfx::I_DUP, P_DUPES, 0, A_GOTO, 0}, {0, L"Empty Folder Scanner", gfx::I_EMPTY, P_EMPTY, 0, A_GOTO, 0},
        {1, L"Startup Manager", gfx::I_STARTUP, P_STARTUP, 0, A_GOTO, 0}, {1, L"Services Manager", gfx::I_SERVICES, P_SERVICES, 0, A_GOTO, 0}, {1, L"Driver Inventory", gfx::I_DRIVER, P_DRIVERS, 0, A_GOTO, 0},
        {2, L"Program Uninstaller", gfx::I_TRASH, P_PROGS, 0, A_GOTO, 0}, {2, L"Check for updates", gfx::I_UPDATE, P_DASH, 0, A_CMD, C_CHECKUPDATE}, {2, L"Change history", gfx::I_ACTION, P_HISTORY, 0, A_GOTO, 0}, {2, L"Settings", gfx::I_SETTINGS, P_SETTINGS, 0, A_GOTO, 0}};
    int row[3] = {0, 0, 0}; static const wchar_t* heads[3] = {L"System Clean", L"System Optimize", L"Software and Maintenance"};
    for (int c = 0; c < cols; ++c) { Tile hd = MkTile(TS_HEAD, A_NONE, {S(16) + c * (cw + gap), S(84), S(16) + c * (cw + gap) + cw, S(110)}, heads[c]); AddTile(hd); }
    for (auto& x : t) { RECT r = {S(16) + x.col * (cw + gap), top + row[x.col] * (th + S(8)), S(16) + x.col * (cw + gap) + cw, top + row[x.col] * (th + S(8)) + th}; ++row[x.col]; Tile k = MkTile(TS_TILE, x.act, r, x.label, 0, x.pg, x.act == A_CMD ? x.cmd : x.filter); k.icon = x.icon; AddTile(k); }
}
static void BuildSettingsRows(int panelW) {
    g_tiles.clear(); int y = S(86), x = S(24), w = panelW - S(48), h = S(40);
    struct R { const wchar_t* label; wstring value; int action; } rows[] = {
        {L"Theme", PalFor(Cfg().theme).name, A_THEME}, {L"Row density", DensityName(), A_DENSITY}, {L"Hide Windows system items on the Startup page", Cfg().hideSystem ? L"On" : L"Off", A_HIDESYS},
        {L"Confirm before changing several items at once", Cfg().confirmBatch ? L"On" : L"Off", A_CONFIRM}, {L"Cleaner: keep temporary files from the last 24 hours", Cfg().keepRecent ? L"On" : L"Off", A_KEEPRECENT},
        {L"Check for updates automatically (contacts GitHub once a day)", Cfg().autoUpdate ? L"On" : L"Off", A_AUTOUPD}, {L"Open on page", kPageNames[Cfg().startPage], A_STARTPAGE},
        {L"Column widths", L"Reset", A_RESETCOLS}, {L"Data folder (journal, backups, profiles)", L"Open", A_OPENDATA}, {L"App history and caches (journal, first-seen list)", L"Clear…", A_CLEARHIST}};
    for (auto& r : rows) { Tile t = MkTile(TS_ROW, r.action, {x, y, x + w, y + h}, r.label); t.big = r.value; AddTile(t); y += h + S(6); }
}
static void RebuildPanel() {
    RECT r; GetClientRect(g_panel, &r); if (g_page == P_SETTINGS) BuildSettingsRows(r.right); else if (g_page == P_SPEED) BuildSpeed(r.right, r.bottom); else if (g_page == P_TOOLBOX) BuildToolbox(r.right, r.bottom); else BuildCare(r.right, r.bottom);
    g_panelFocus = -1; g_panelHot = -1; InvalidateRect(g_panel, nullptr, TRUE);
}
static void PanelAction(int a) {
    Settings& c = Cfg(); auto repaint = [&] { RebuildPanel(); };
    switch (a) {
    case A_THEME: { HMENU m = CreatePopupMenu(); for (int i = 0; i < ThemeCount(); ++i) AppendMenuW(m, MF_STRING | (i == c.theme ? MF_CHECKED : 0), 1 + i, PalFor(i).name);
        POINT pt; GetCursorPos(&pt); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) { c.theme = id - 1; PostMessageW(g_main, WM_COMMAND, 0xE001, 0); } break; }
    case A_DENSITY: c.density = !c.density; ApplyDensity(); SaveSettings(); repaint(); break;
    case A_HIDESYS: c.hideSystem = !c.hideSystem; SaveSettings(); repaint(); break;
    case A_CONFIRM: c.confirmBatch = !c.confirmBatch; SaveSettings(); repaint(); break;
    case A_KEEPRECENT: c.keepRecent = !c.keepRecent; SaveSettings(); repaint(); break;
    case A_AUTOUPD: c.autoUpdate = !c.autoUpdate; SaveSettings(); repaint(); break;
    case A_STARTPAGE: { HMENU m = CreatePopupMenu(); for (int i = 0; i < P_COUNT; ++i) AppendMenuW(m, MF_STRING | (i == c.startPage ? MF_CHECKED : 0), 1 + i, kPageNames[i]);
        POINT pt; GetCursorPos(&pt); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) { c.startPage = id - 1; SaveSettings(); repaint(); } break; }
    case A_RESETCOLS: c.colw.clear(); SaveSettings(); SetMsg(L"Column widths reset. They apply the next time you open each page."); break;
    case A_OPENDATA: ShellExecuteW(nullptr, L"open", DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case A_CLEARHIST: if (DarkMsg(g_main, L"Clear app history", L"This deletes the change journal (History page) and the first-seen list. It does NOT touch Windows startup entries or services, and it does not delete restore backups or profiles.\r\n\r\nClear now?", 2) == IDYES) {
            DeleteFileW((DataDir() + L"\\journal.jsonl").c_str()); DeleteFileW((DataDir() + L"\\firstseen.tsv").c_str()); g_cells[P_HISTORY].clear(); SetMsg(L"App history cleared."); StartScan(); } break;
    case A_ADMIN: PostMessageW(g_main, WM_COMMAND, C_ADMIN, 0); break;
    }
}
static void TileAction(const Tile& t) {
    switch (t.action) {
    case A_GOTO: g_filter[t.page] = t.filter; ShowPage(t.page); break;
    case A_CMD: PostMessageW(g_main, WM_COMMAND, t.filter, 0); if (t.page != P_DASH) {} break;
    case A_SCAN: CareStart(); break; case A_STOPSCAN: CareStop(); break;
    case A_MODE: g_careManual = t.arg == 1; RebuildPanel(); break;
    case A_CHECK: g_careOn[t.arg] = !g_careOn[t.arg]; RebuildPanel(); break;
    case A_REVIEW: CareReview(); break;
    default: if (g_page == P_SETTINGS) PanelAction(t.action); break;
    }
}
static void PaintTile(HDC dc, const Tile& t, bool hot, bool foc) {
    RECT r = t.r; SetBkMode(dc, TRANSPARENT); wstring cap = t.cap; for (size_t p = 0; (p = cap.find(L"&&", p)) != wstring::npos;) { cap.replace(p, 2, L"&"); ++p; }
    switch (t.style) {
    case TS_ROW: { gfx::RoundRect(dc, r, S(8), cBar, cBar, foc ? cAccent : cGrid); RECT lr = r; lr.left += S(14); SetTextColor(dc, cText); SelectObject(dc, g_font); DrawTextW(dc, cap.c_str(), -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        bool toggle = t.action == A_HIDESYS || t.action == A_CONFIRM || t.action == A_KEEPRECENT || t.action == A_AUTOUPD;
        if (toggle) { RECT sw = {r.right - S(62), r.top + S(10), r.right - S(14), r.bottom - S(10)}; gfx::Toggle(dc, sw, t.big == L"On", cAccent, cGrid, RGB(255, 255, 255)); }
        else { SIZE sz; GetTextExtentPoint32W(dc, t.big.c_str(), (int)t.big.size(), &sz); RECT pill = {r.right - sz.cx - S(46), r.top + S(7), r.right - S(12), r.bottom - S(7)}; gfx::RoundRect(dc, pill, S(8), cHot, cHot, cGrid); SetTextColor(dc, cAccent); DrawTextW(dc, (t.big + ((t.action == A_THEME || t.action == A_STARTPAGE) ? L" ▾" : L"")).c_str(), -1, &pill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); }
        break; }
    case TS_CARD: { gfx::RoundRect(dc, r, S(12), cBar, P.dark ? cBg : cBar, hot || foc ? cAccent : cGrid); RECT ic = {r.left + S(22), r.top + S(24), r.left + S(22) + S(46), r.top + S(24) + S(46)}; gfx::Icon(dc, t.icon, ic, cAccent, 2.2f);
        SelectObject(dc, g_fontTitle); SetTextColor(dc, cText); RECT tr = {r.left + S(84), r.top + S(14), r.right - S(16), r.top + S(44)}; DrawTextW(dc, t.big.c_str(), -1, &tr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(dc, g_font); SetTextColor(dc, cDim); RECT dr = {r.left + S(84), r.top + S(46), r.right - S(16), r.bottom - S(46)}; DrawTextW(dc, t.desc.c_str(), -1, &dr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
        RECT br = {r.right - S(150), r.bottom - S(44), r.right - S(18), r.bottom - S(12)}; gfx::RoundRect(dc, br, S(6), kBtnTop, kBtnBot, CLR_INVALID); SetTextColor(dc, RGB(10, 24, 60)); SelectObject(dc, g_font); DrawTextW(dc, cap.c_str(), -1, &br, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); break; }
    case TS_BUTTON: gfx::RoundRect(dc, r, S(8), hot ? RGB(150, 205, 255) : kBtnTop, kBtnBot, foc ? RGB(255, 255, 255) : CLR_INVALID); SetTextColor(dc, RGB(10, 24, 60)); SelectObject(dc, g_font); DrawTextW(dc, cap.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); break;
    case TS_LINK: SetTextColor(dc, hot ? RGB(170, 215, 255) : cAccent); SelectObject(dc, g_font); DrawTextW(dc, cap.c_str(), -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); break;
    case TS_CHECK: { RECT bx = {r.left, r.top + S(5), r.left + S(20), r.top + S(25)}; gfx::RoundRect(dc, bx, S(4), t.on ? cAccent : cBar, t.on ? cAccent : cBar, foc ? RGB(255, 255, 255) : (t.on ? cAccent : cDim));
        if (t.on) gfx::Icon(dc, gfx::I_CHECK, {bx.left + S(3), bx.top + S(3), bx.right - S(3), bx.bottom - S(3)}, RGB(10, 24, 60), 2.4f); SetTextColor(dc, hot ? RGB(255, 255, 255) : cText); SelectObject(dc, g_font); RECT tr = {r.left + S(30), r.top, r.right, r.bottom}; DrawTextW(dc, cap.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); break; }
    case TS_PILL: { bool sel = (t.arg == 1) == g_careManual; if (sel) gfx::RoundRect(dc, r, S(17), kBtnTop, kBtnBot, CLR_INVALID); else gfx::RoundRect(dc, r, S(17), cBar, cBar, hot ? cAccent : cGrid); SetTextColor(dc, sel ? RGB(10, 24, 60) : cText); SelectObject(dc, g_font); DrawTextW(dc, cap.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); break; }
    case TS_SCAN: { int d = r.right - r.left; RECT o = {r.left - S(14), r.top - S(14), r.right + S(14), r.bottom + S(14)}; gfx::Disc(dc, o, RGB(30, 52, 120), RGB(14, 22, 60), CLR_INVALID);
        gfx::Ring(dc, r, S(14), hot ? RGB(130, 200, 255) : RGB(86, 168, 255), -60, 120, true); gfx::Ring(dc, r, S(14), RGB(60, 120, 215), 60, 120, true); gfx::Ring(dc, r, S(14), RGB(110, 190, 255), 180, 120, true);
        RECT in = {r.left + S(18), r.top + S(18), r.right - S(18), r.bottom - S(18)}; gfx::Disc(dc, in, RGB(54, 70, 150), RGB(26, 34, 90), RGB(120, 170, 255), 2); (void)d;
        SelectObject(dc, g_fontBig); SetTextColor(dc, RGB(255, 255, 255)); DrawTextW(dc, cap.c_str(), -1, &in, DT_CENTER | DT_VCENTER | DT_SINGLELINE); break; }
    case TS_TILE: { gfx::RoundRect(dc, r, S(10), hot ? cHot : cBar, hot ? cHot : cBar, foc || hot ? cAccent : cGrid); RECT ic = {r.left + S(16), r.top + S(14), r.left + S(16) + S(38), r.top + S(14) + S(38)}; gfx::Icon(dc, t.icon, ic, cAccent, 2.0f);
        SelectObject(dc, g_font); SetTextColor(dc, cText); RECT tr = {r.left + S(70), r.top, r.right - S(10), r.bottom}; DrawTextW(dc, cap.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS); break; }
    case TS_HEAD: SelectObject(dc, g_fontTitle); SetTextColor(dc, cDim); DrawTextW(dc, cap.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX); break;
    }
}
static void PaintCareExtras(HDC dc, const RECT& cr) {
    int w = cr.right; SetBkMode(dc, TRANSPARENT);
    if (g_careRunning) {
        int done = 0; for (auto& st : g_steps) if (st.state == 2) ++done; int pct = g_steps.empty() ? 0 : done * 100 / (int)g_steps.size(); wstring cur = L"Preparing"; for (auto& st : g_steps) if (st.state == 1) { cur = st.label; break; }
        int y = S(40); SelectObject(dc, g_fontTitle); SetTextColor(dc, cDim); TextOutW(dc, S(24), y - S(24), L"Scanning…", 11); SelectObject(dc, g_font);
        for (auto& st : g_steps) { RECT row = {S(20), y, S(300), y + S(32)}; gfx::RoundRect(dc, row, S(6), st.state == 1 ? cHot : cBar, st.state == 1 ? cHot : cBar, st.state == 1 ? cAccent : cGrid); RECT lt = {row.left + S(12), row.top, row.right - S(80), row.bottom}; SetTextColor(dc, cText); DrawTextW(dc, st.label.c_str(), -1, &lt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            if (st.state == 2) { gfx::Icon(dc, gfx::I_CHECK, {row.right - S(30), row.top + S(6), row.right - S(10), row.top + S(26)}, cGreen, 2.2f); RECT vt = {row.right - S(190), row.top, row.right - S(36), row.bottom}; SetTextColor(dc, cAmber); /* value is shown in results */ (void)vt; } y += S(36); }
        int cx = S(330) + (w - S(330)) / 2, cy = S(230), R = S(120); RECT ring = {cx - R, cy - R, cx + R, cy + R}; gfx::Ring(dc, ring, S(16), RGB(40, 56, 120), 0, 360, false); gfx::Ring(dc, ring, S(16), RGB(86, 168, 255), -90, (float)(3.6 * pct), true);
        if (pct < 100) gfx::Ring(dc, {ring.left + S(26), ring.top + S(26), ring.right - S(26), ring.bottom - S(26)}, S(4), RGB(120, 190, 255), (float)g_careAngle, 40, true);
        wchar_t b[16]; swprintf(b, 16, L"%d", pct); SelectObject(dc, g_fontBig); SetTextColor(dc, RGB(255, 255, 255)); RECT pr = {cx - S(90), cy - S(40), cx + S(60), cy + S(40)}; DrawTextW(dc, b, -1, &pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE); SelectObject(dc, g_font); SetTextColor(dc, cDim); TextOutW(dc, cx + S(40), cy - S(10), L"%", 1);
        SelectObject(dc, g_fontTitle); SetTextColor(dc, cText); wstring msg = L"Scanning: " + cur; RECT mr = {cx - S(220), cy + R + S(20), cx + S(220), cy + R + S(56)}; DrawTextW(dc, msg.c_str(), -1, &mr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    } else if (g_careDone) {
        SelectObject(dc, g_fontTitle); SetTextColor(dc, cText); gfx::Icon(dc, gfx::I_CHECK, {S(24), S(20), S(24) + S(40), S(60)}, cGreen, 2.6f); TextOutW(dc, S(78), S(26), L"Scan complete", 13);
        SelectObject(dc, g_font); SetTextColor(dc, cDim); TextOutW(dc, S(80), S(58), L"Nothing has been changed yet. Review the findings, then clean what you choose.", 73);
        int y = S(112), rowH = S(40); for (auto& st : g_steps) { RECT row = {S(24), y, w - S(24), y + rowH - S(8)}; gfx::RoundRect(dc, row, S(6), cBar, cBar, cGrid); RECT lt = {row.left + S(14), row.top, row.left + S(240), row.bottom}; SetTextColor(dc, cText); DrawTextW(dc, st.label.c_str(), -1, &lt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            RECT vt = {row.left + S(250), row.top, row.right - S(150), row.bottom}; SetTextColor(dc, cAmber); DrawTextW(dc, st.value.c_str(), -1, &vt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS); y += rowH; }
    } else {
        SelectObject(dc, g_fontTitle); SetTextColor(dc, cText); TextOutW(dc, S(24), S(18), L"Ready to care for your PC", 25); SelectObject(dc, g_font); SetTextColor(dc, cDim);
        wstring s = g_careManual ? L"Manual mode: choose what to scan below. Nothing is changed until you review the results." : L"Smart mode scans junk files, the registry, startup items, services and programs. Nothing is changed until you review the results."; RECT r = {S(24), S(48), w - S(24), S(66)}; DrawTextW(dc, s.c_str(), -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
}
static LRESULT CALLBACK PanelProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_brBg); return 1; }
    case WM_SIZE: RebuildPanel(); return 0;
    case WM_TIMER: if (g_careRunning) { g_careAngle = (g_careAngle + 14) % 360; RECT r; GetClientRect(h, &r); InvalidateRect(h, &r, FALSE); } return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, cr.right, cr.bottom); HGDIOBJ ob = SelectObject(mem, bmp); FillRect(mem, &cr, g_brBg); SetBkMode(mem, TRANSPARENT);   // double-buffered
        if (g_page == P_SETTINGS) { SelectObject(mem, g_fontTitle); SetTextColor(mem, cText); TextOutW(mem, S(24), S(18), L"Settings", 8); SelectObject(mem, g_font); SetTextColor(mem, cDim); wstring sub = L"Saved automatically in " + DataDir() + L"\\settings.ini"; TextOutW(mem, S(26), S(52), sub.c_str(), (int)sub.size()); }
        else if (g_page == P_SPEED) { size_t warn = 0; for (auto& e : g_su.entries) if (e.hasWarning) ++warn; gfx::Icon(mem, gfx::I_CHECK, {S(24), S(18), S(24) + S(44), S(18) + S(44)}, warn ? cAmber : cGreen, 2.6f); SelectObject(mem, g_fontTitle); SetTextColor(mem, cText);
            wstring hd = warn ? std::to_wstring(warn) + L" startup entries need your attention" : L"Your startup looks tidy"; TextOutW(mem, S(80), S(20), hd.c_str(), (int)hd.size()); SelectObject(mem, g_font); SetTextColor(mem, cDim); TextOutW(mem, S(82), S(50), L"Optimize start-up, services, programs and drivers. Every change is journaled and can be undone where Windows allows.", 109); }
        else if (g_page == P_TOOLBOX) { SelectObject(mem, g_fontTitle); SetTextColor(mem, cText); TextOutW(mem, S(24), S(18), L"Toolbox", 7); SelectObject(mem, g_font); SetTextColor(mem, cDim); TextOutW(mem, S(26), S(50), L"All tools in one place. Click a tool to open it.", 48); }
        else PaintCareExtras(mem, cr);
        for (size_t i = 0; i < g_tiles.size(); ++i) PaintTile(mem, g_tiles[i], (int)i == g_panelHot, (int)i == g_panelFocus && GetFocus() == h);
        if (g_page == P_SETTINGS) { int y = g_tiles.empty() ? S(100) : g_tiles.back().r.bottom + S(16); RECT tr{S(26), y, cr.right - S(26), cr.bottom - S(8)}; SetTextColor(mem, cDim); SelectObject(mem, g_font);
            wstring note = L"Privacy: the app works offline. The only network feature is the update check (one small file from GitHub, once a day, if enabled above). No telemetry. \"Check on VirusTotal\" and \"Search online\" only open your browser after you confirm.\r\n" + MigrationNote() + L"\r\nShortcuts: Ctrl+F search · F5 refresh · Enter details · Del delete · Ctrl+A select all · Ctrl+C copy name · Ctrl+Shift+C copy details · Ctrl+E export · Ctrl+Z undo · Ctrl+1..7 sections";
            DrawTextW(mem, note.c_str(), -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX); }
        BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: { POINT p{(short)LOWORD(l), (short)HIWORD(l)}; int hot = -1; for (size_t i = 0; i < g_tiles.size(); ++i) if (g_tiles[i].style != TS_HEAD && PtInRect(&g_tiles[i].r, p)) { hot = (int)i; break; } if (hot != g_panelHot) { g_panelHot = hot; InvalidateRect(h, nullptr, FALSE); TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0}; TrackMouseEvent(&t); } return 0; }
    case WM_MOUSELEAVE: g_panelHot = -1; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: { POINT p{(short)LOWORD(l), (short)HIWORD(l)}; SetFocus(h); for (size_t i = 0; i < g_tiles.size(); ++i) if (g_tiles[i].style != TS_HEAD && PtInRect(&g_tiles[i].r, p)) { g_panelFocus = (int)i; Tile t = g_tiles[i]; TileAction(t); break; } return 0; }
    case WM_SETCURSOR: { POINT p; GetCursorPos(&p); ScreenToClient(h, &p); for (auto& t : g_tiles) if (t.style != TS_HEAD && PtInRect(&t.r, p)) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; } break; }
    case WM_SETFOCUS: case WM_KILLFOCUS: if (g_panelFocus < 0 && !g_tiles.empty()) g_panelFocus = 0; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_KEYDOWN: { int n = (int)g_tiles.size(); if (!n) break; auto step = [&](int d) { int i = g_panelFocus; for (int k = 0; k < n; ++k) { i = (i + d + n) % n; if (g_tiles[i].style != TS_HEAD) break; } g_panelFocus = i; InvalidateRect(h, nullptr, FALSE); };
        if (w == VK_RIGHT || w == VK_DOWN) { step(1); return 0; } if (w == VK_LEFT || w == VK_UP) { step(-1); return 0; }
        if ((w == VK_RETURN || w == VK_SPACE) && g_panelFocus >= 0) { Tile t = g_tiles[g_panelFocus]; TileAction(t); return 0; } break; }
    }
    return DefWindowProcW(h, m, w, l);
}
// ================= pages =================

static void Layout(int w, int h);
static void ShowPage(Page p) {
    SaveColumnWidths(); g_page = p; g_lastInGroup[GroupOf(p)] = p; SendMessageW(g_nav, LB_SETCURSEL, GroupOf(p), 0); InvalidateRect(g_nav, nullptr, TRUE);
    bool listy = !IsPanelPage(p); ComputeLayout(); BuildSubTabs();
    ShowWindow(g_list, listy ? SW_SHOW : SW_HIDE); ShowWindow(g_panel, listy ? SW_HIDE : SW_SHOW); ShowWindow(g_search, listy ? SW_SHOW : SW_HIDE);
    wstring title = wstring(kApp) + L"  —  " + kPageNames[p] + (g_elevated ? L" (Administrator)" : L""); SetWindowTextW(g_main, title.c_str());
    BuildToolbar();
    if (listy) {
        SetWindowTextW(g_search, g_query[p].c_str());
        SendMessageW(g_search, EM_SETCUEBANNER, TRUE, (LPARAM)(p == P_SERVICES ? L"Search services (Ctrl+F)" : p == P_DRIVERS ? L"Search drivers (Ctrl+F)" : p == P_CLEAN ? L"Search items (Ctrl+F)" : IsFilePage(p) ? (p == P_EMPTY ? L"Search folders (Ctrl+F)" : L"Search files (Ctrl+F)") : p == P_HISTORY ? L"Search history (Ctrl+F)" : L"Search (Ctrl+F)"));
        BuildColumns(); if (p == P_HISTORY) ReadHistory(); ApplyView(); InvalidateRect(g_search, nullptr, TRUE);
    } else { RebuildPanel(); UpdateStatus(); UpdateToolbar(); }
    UpdateBanner(); RECT r; GetClientRect(g_main, &r); Layout(r.right, r.bottom);
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
static void ShowProgramDetails();
static void ShowDetails() {
    if (g_page == P_PROGS) { ShowProgramDetails(); return; }
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
static void CopyDetails() { wstring o; for (int i : Selected()) o += (g_page == P_STARTUP ? StartupDetailsText(g_su.entries[i]) : IsFilePage(g_page) ? FS(g_page).files[i].path + L"  " + FormatBytes(FS(g_page).files[i].size) : g_page == P_PROGS ? g_pg.entries[i].name + L"  " + g_pg.entries[i].version + L"\r\n" + g_pg.entries[i].installLocation + L"\r\n" + g_pg.entries[i].uninstallString : g_page == P_CLEAN ? g_cl.items[i].name + L"\r\n" + g_cl.items[i].location + L"\r\n" + g_cl.items[i].why : ServiceTabText(SvcVec(g_page)[i], 0)) + L"\r\n"; SetClip(o); }
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
static bool CanDeleteEntry(const StartupEntry& e) {
    if (e.managedDelay) return false;                                   // use "Remove delay" so the original entry is restored
    return e.type == L"Registry" || e.type == L"Registry (RunOnce)" || e.type.rfind(L"Scheduled task", 0) == 0 || e.type.rfind(L"Explorer add-on", 0) == 0 || e.type == L"Startup folder";
}
static void DeleteStartup() {
    auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) { auto& e = g_su.entries[i]; if (CanDeleteEntry(e)) { todo.push_back(i); names.push_back(e.name + L"  (" + e.type + L")  —  " + e.command + (e.warnings.empty() ? wstring() : L"  [" + e.warnings + L"]")); } }
    if (todo.empty()) { Warn(L"None of the selected entries can be deleted here. Delayed entries made by this app use Start later > Remove delay."); return; }
    if (!Confirm(L"Delete " + Plural(todo.size(), L"startup entry", L"startup entries") + L"? Only the startup registration is removed (the program is not uninstalled). Registry entries and scheduled tasks are backed up so you can restore them (Restore…); startup shortcuts go to the Recycle Bin. This also works when the program file is missing.", names)) return;
    int ok = 0, bad = 0; wstring firstErr, firstName;
    for (int i : todo) { StartupEntry e = g_su.entries[i]; wstring err; bool done = false;
        if (e.type == L"Registry" || e.type == L"Registry (RunOnce)") done = DeleteStartupEntry(e, err);
        else if (e.type == L"Startup folder") { wstring path = e.key.size() > 7 ? e.key.substr(7) : L""; done = !path.empty() && RecycleFiles(g_main, {path}, err) && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES; if (!done && err.empty()) err = L"The shortcut could not be moved to the Recycle Bin."; WriteJournal(e.name, e.location, L"present", done ? L"moved to Recycle Bin" : L"unchanged", done ? L"ok (restorable from Recycle Bin)" : L"failed: " + err, true); }
        else { CleanItem it; it.name = e.name; it.location = e.location; it.risk = 1; if (e.type.rfind(L"Scheduled task", 0) == 0) { it.kind = 9; it.cmd = e.taskPath; } else { it.kind = 2; it.root = e.root; it.view = e.view; it.subKey = e.subKey; it.detail = L"key"; it.why = L"Explorer add-on registration"; it.needsAdmin = e.root == HKEY_LOCAL_MACHINE; }
            CleanResult r; RunClean({it}, false, r); done = r.ok == 1 && r.failed == 0 && r.skipped == 0; err = r.firstErr.empty() ? L"Windows refused the change (administrator rights may be required)." : r.firstErr; }
        if (done) ++ok; else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = e.name; } } }
    wstring msg = ok ? std::to_wstring(ok) + L" deleted (backups saved; use Restore… or the Recycle Bin)." : L""; if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
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
    auto b = ListBackups(); auto rb = ListRegBackups(); HMENU m = CreatePopupMenu(); const int kRegOff = 40;
    if (b.empty() && rb.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 1, L"No deleted entries to restore");
    for (size_t i = 0; i < b.size() && i < 30; ++i) AppendMenuW(m, MF_STRING, C_RESTORE_BASE + i, b[i].label.c_str());
    if (!rb.empty() && !b.empty()) AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    for (size_t i = 0; i < rb.size() && i < 30; ++i) AppendMenuW(m, MF_STRING, C_RESTORE_BASE + kRegOff + i, rb[i].label.c_str());
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m);
    bool isReg = id >= C_RESTORE_BASE + kRegOff && id < C_RESTORE_BASE + kRegOff + (int)rb.size(); bool isRun = id >= C_RESTORE_BASE && id < C_RESTORE_BASE + (int)b.size();
    if (isRun || isReg) { const BackupInfo& bi = isReg ? rb[id - C_RESTORE_BASE - kRegOff] : b[id - C_RESTORE_BASE]; wstring err, name;
        if (DarkMsg(g_main, kApp, L"Restore this entry?\r\n\r\n" + bi.label + L"\r\n\r\nExisting values are never overwritten.", 2) != IDYES) return;
        bool ok = isReg ? RestoreRegBackup(bi.path, err, name) : RestoreBackup(bi.path, err, name); if (ok) { SetMsg(isReg ? name + L"." : L"Restored \"" + name + L"\"."); StartScan(); } else Warn(err.empty() ? L"Nothing was restored." : err); }
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
    bool deep = false; for (auto& it : j->items) if (it.id.rfind(L"d.", 0) == 0) deep = true;
    PostMessageW(g_main, WM_JOB_DONE, 0, (LPARAM) new JobDone{msg, deep, r.bytes}); CoUninitialize(); return 0;
}
static void RefreshCleanCells() { for (size_t i = 0; i < g_cl.items.size() && i < g_cells[P_CLEAN].size(); ++i) g_cells[P_CLEAN][i][0] = g_checked.count(g_cl.items[i].id) ? L"☑" : L"☐"; if (g_page == P_CLEAN) InvalidateRect(g_list, nullptr, FALSE); UpdateBanner(); UpdateToolbar(); }
static void SetRecommended() { g_checked.clear(); for (auto& e : g_cl.items) if (e.risk == 0 && !(e.needsAdmin && !g_elevated)) g_checked.insert(e.id); RefreshCleanCells(); }
static void CleanCheckAll(bool on) { g_checked.clear(); if (on) for (auto& e : g_cl.items) g_checked.insert(e.id); RefreshCleanCells(); }
static void ToggleCleanAtView(int vi) { if (vi < 0 || vi >= (int)g_view.size() || g_page != P_CLEAN) return; auto& id = g_cl.items[g_view[vi]].id; if (g_checked.count(id)) g_checked.erase(id); else g_checked.insert(id); RefreshCleanCells(); }
static void ToggleCleanSelection() { auto sel = Selected(); if (sel.empty()) return; bool allOn = true; for (int i : sel) if (!g_checked.count(g_cl.items[i].id)) allOn = false; for (int i : sel) { auto& id = g_cl.items[i].id; if (allOn) g_checked.erase(id); else g_checked.insert(id); } RefreshCleanCells(); }
static void CleanSelected() {   // cleans the ticked items
    if (g_scanning) return; auto* job = new CleanJob{{}, Cfg().keepRecent}; std::vector<wstring> names; ULONGLONG total = 0; int regs = 0, risky = 0, svcs = 0; bool recycle = false;
    for (auto& e : g_cl.items) if (g_checked.count(e.id)) { job->items.push_back(e); total += e.bytes; if (IsRegItem(e)) ++regs; if (e.kind == 8 || e.kind == 9) ++svcs; if (e.kind == 1) recycle = true; if (e.risk >= 1) ++risky; names.push_back(e.name + L"  (" + (IsRegItem(e) ? e.detail : FormatBytes(e.bytes)) + L", " + RiskName(e.risk) + L" risk)"); }
    if (job->items.empty()) { delete job; Warn(L"Tick the items you want to clean first (or press Recommended)."); return; }
    wstring q = L"Clean " + Plural(job->items.size(), L"item", L"items") + L"?";
    if (total) q += L" About " + FormatBytes(total) + L" of temporary data will be deleted permanently (files in use are skipped).";
    if (recycle) q += L" The Recycle Bin will be emptied for good.";
    if (svcs) q += L" " + Plural(svcs, L"Windows service/scheduled task", L"Windows services/scheduled tasks") + L" will be deleted (task definitions are saved as XML; services cannot be restored automatically).";
    if (risky) q += L" " + Plural(risky, L"ticked item is", L"ticked items are") + L" Medium/High risk: read each description (Details) before confirming.";
    if (regs) q += L" " + Plural(regs, L"registry entry", L"registry entries") + L" will be removed after saving a backup you can restore.";
    if (!Confirm(q, names)) { delete job; return; }
    SetMsg(L"Cleaning…"); HANDLE h = CreateThread(nullptr, 0, CleanThread, job, 0, nullptr); if (h) CloseHandle(h); else delete job;
}
// ---- deep scan (all drives) ----
struct DeepDone { CleanScan scan; };
static DWORD WINAPI DeepThread(LPVOID) { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); auto* d = new DeepDone{ScanDeep(&g_fsCancel, [](const wstring& m) { PostMessageW(g_main, WM_FILE_PROGRESS, 0, (LPARAM) new wstring(m)); })}; PostMessageW(g_main, WM_DEEP_DONE, 0, (LPARAM)d); CoUninitialize(); return 0; }
static void StartDeepScan(bool ask) {
    if (g_fsBusy) return;
    if (ask && DarkMsg(g_main, kApp, L"Deep scan looks through ALL fixed drives for stray temporary files, orphaned lock files, old dumps and logs, plus developer and app caches, and offers Windows' own component-store cleanup.\r\n\r\nIt only searches; nothing is deleted until you tick items and press Clean. It can take several minutes. Continue?", 2) != IDYES) return;
    g_fsCancel = false; g_fsBusy = true; SetMsg(L"Deep scan started… press Stop (in the banner) to cancel."); UpdateStatus(); UpdateToolbar(); HANDLE h = CreateThread(nullptr, 0, DeepThread, nullptr, 0, nullptr); if (h) CloseHandle(h); else g_fsBusy = false;
}
// ---- Care: scan orchestration ----
static void CareValueFill(int id) { for (auto& st : g_steps) if (st.id == id) { st.state = 2; st.value = CareValue(id); } }
static void CareFinish() { g_careRunning = false; g_careDone = true; KillTimer(g_panel, 1); if (!g_cl.items.empty() && g_checked.empty()) SetRecommended(); if (g_page == P_DASH) RebuildPanel(); SetMsg(L"Scan complete. Nothing has been changed yet - review the results."); UpdateBanner(); }
static void CareNext() {
    if (!g_careRunning) return; if (g_careStop || g_careQueue.empty()) { CareFinish(); return; } int id = g_careQueue.front(); g_careQueue.erase(g_careQueue.begin());
    for (auto& st : g_steps) if (st.id == id) st.state = 1; if (g_page == P_DASH) InvalidateRect(g_panel, nullptr, FALSE);
    if (id == 6) StartFileScan(0); else if (id == 7) StartFileScan(1); else if (id == 8) StartFileScan(2); else if (id == 9) StartDeepScan(false);
}
static void CareOnBaseDone(int which) {
    if (!g_careRunning) return; if (which == 0) CareValueFill(0); else if (which == 1) CareValueFill(1); else if (which == 2) CareValueFill(2); else if (which == 4) CareValueFill(3); else if (which == 3) { CareValueFill(4); CareValueFill(5); CareNext(); }
    if (g_page == P_DASH) InvalidateRect(g_panel, nullptr, FALSE);
}
static void CareOnExtraDone(int id) { if (!g_careRunning) return; CareValueFill(id); CareNext(); if (g_page == P_DASH) RebuildPanel(); }
static void CareStart() {
    if (g_scanning || g_fsBusy) { SetMsg(L"Please wait: another scan is still running."); return; }
    if (g_careRunning) return; g_steps.clear(); g_careQueue.clear(); bool man = g_careManual; auto on = [&](int mod) { return !man || g_careOn[mod]; };
    if (on(3)) g_steps.push_back({0, kStepNames[0], L"", 1}); if (on(4)) { g_steps.push_back({1, kStepNames[1], L"", 1}); g_steps.push_back({2, kStepNames[2], L"", 1}); } if (on(5)) g_steps.push_back({3, kStepNames[3], L"", 1});
    if (on(0) || on(2)) g_steps.push_back({4, kStepNames[4], L"", 1}); if (on(1)) g_steps.push_back({5, kStepNames[5], L"", 1});
    if (man) for (int id = 6; id <= 9; ++id) if (g_careOn[id]) { g_steps.push_back({id, kStepNames[id], L"", 0}); g_careQueue.push_back(id); }
    if (g_steps.empty()) { Warn(L"Tick at least one item to scan."); return; }
    g_careRunning = true; g_careDone = false; g_careStop = false; SetTimer(g_panel, 1, 40, nullptr); RebuildPanel(); StartScan();
}
static void CareStop() { g_careStop = true; g_fsCancel = true; SetMsg(L"Stopping after the current step…"); }
static void CareReview() { g_filter[P_CLEAN] = 0; SetRecommended(); ShowPage(P_CLEAN); }
// ---- updates ----
struct UpdMsg { UpdateInfo u; bool manual; };
static DWORD WINAPI UpdateCheckThread(LPVOID p) { UpdMsg* m = new UpdMsg{{}, p != nullptr}; FetchLatestVersion(m->u); PostMessageW(g_main, WM_UPDATE_INFO, 0, (LPARAM)m); return 0; }
static void CheckForUpdates(bool manual) { if (manual) SetMsg(L"Checking for updates…"); HANDLE h = CreateThread(nullptr, 0, UpdateCheckThread, manual ? (LPVOID)1 : nullptr, 0, nullptr); if (h) CloseHandle(h); }
struct UpdDl { UpdateInfo u; };
static DWORD WINAPI UpdateDownloadThread(LPVOID p) {
    std::unique_ptr<UpdDl> d((UpdDl*)p); wstring path, err; bool ok = DownloadUpdate(d->u, path, err, nullptr, [](ULONGLONG got, ULONGLONG tot) { static DWORD last = 0; DWORD n = GetTickCount(); if (n - last < 300) return; last = n; PostMessageW(g_main, WM_FILE_PROGRESS, 0, (LPARAM) new wstring(L"Downloading update… " + FormatBytes(got) + (tot ? L" of " + FormatBytes(tot) : wstring()))); });
    PostMessageW(g_main, WM_UPDATE_READY, ok, (LPARAM) new wstring(ok ? path : err)); return 0;
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
static void RemoveSelected(bool permanent) {
    if (!IsFilePage(g_page) || g_fsBusy) return; auto sel = Selected(); if (sel.empty()) return; auto& scan = FS(g_page);
    if (g_page == P_DUPES) {   // never allow removing every copy of a file
        std::map<int, int> chosen; for (int i : sel) ++chosen[scan.files[i].group];
        for (auto& kv : chosen) { int total = 0; for (auto& e : scan.files) if (e.group == kv.first) ++total; if (kv.second >= total) { Warn(L"Group " + std::to_wstring(kv.first) + L" has all of its copies selected. At least one copy must be kept - deselect one and try again."); return; } }
    }
    std::vector<wstring> names, paths; ULONGLONG total = 0; for (int i : sel) { auto& e = scan.files[i]; names.push_back(e.path + (g_page == P_EMPTY ? wstring() : L"  (" + FormatBytes(e.size) + L")")); paths.push_back(e.path); total += e.size; }
    wstring what = Plural(sel.size(), g_page == P_EMPTY ? L"empty folder" : L"file", g_page == P_EMPTY ? L"empty folders" : L"files") + (g_page == P_EMPTY ? wstring() : L" (" + FormatBytes(total) + L")");
    if (permanent) { if (DarkMsg(g_main, kApp, L"PERMANENTLY DELETE " + what + L"?\r\n\r\nThis does NOT use the Recycle Bin. The " + (sel.size() == 1 ? wstring(L"item") : L"items") + L" cannot be restored afterwards.\r\n" + BulletList(names), 2) != IDYES) return; }
    else if (!Confirm(L"Move " + what + L" to the Recycle Bin? You can restore them from there.", names)) return;
    if (g_page == P_DUPES) {   // re-verify byte-for-byte against a copy that stays, right before deleting
        std::set<int> sel2(sel.begin(), sel.end()); std::vector<wstring> okPaths; int mism = 0;
        for (int i : sel) { const FileEntry* keeper = nullptr; for (size_t k = 0; k < scan.files.size(); ++k) if (scan.files[k].group == scan.files[i].group && !sel2.count((int)k)) { keeper = &scan.files[k]; break; }
            if (keeper && VerifyIdentical(scan.files[i].path, keeper->path)) okPaths.push_back(scan.files[i].path); else ++mism; }
        if (mism) Warn(std::to_wstring(mism) + L" selected file(s) are no longer identical to the copy you keep (they changed since the scan, or cannot be read). They were NOT deleted; the others proceed.");
        paths = okPaths; if (paths.empty()) return;
    }
    wstring err; if (permanent) DeleteFilesPermanently(paths, err); else RecycleFiles(g_main, paths, err);
    std::set<wstring> gone; ULONGLONG freed = 0; for (int i : sel) if (GetFileAttributesW(scan.files[i].path.c_str()) == INVALID_FILE_ATTRIBUTES) { gone.insert(scan.files[i].path); freed += scan.files[i].size; }
    std::vector<FileEntry> rest; for (auto& e : scan.files) if (!gone.count(e.path)) rest.push_back(e); scan.files = rest;
    if (g_page == P_DUPES) PruneDupes(); else { scan.totalBytes = 0; for (auto& e : scan.files) scan.totalBytes += e.size; }
    WriteJournal(L"File finder", wstring(g_page == P_DUPES ? L"duplicates" : g_page == P_EMPTY ? L"empty folders" : L"large files"), Plural(sel.size(), L"item", L"items") + L" selected", Plural(gone.size(), L"item", L"items") + (permanent ? L" permanently deleted (" : L" moved to Recycle Bin (") + FormatBytes(freed) + L")", gone.size() == sel.size() ? (permanent ? L"ok (permanent, not undoable)" : L"ok (restorable from Recycle Bin)") : L"partial: " + err, !permanent);
    SetMsg(Plural(gone.size(), L"item", L"items") + (permanent ? L" permanently deleted (" : L" moved to the Recycle Bin (") + FormatBytes(freed) + L")." + (gone.size() < sel.size() ? L" " + std::to_wstring(sel.size() - gone.size()) + L" could not be removed" + (err.empty() ? wstring() : L": " + err) : wstring()));
    BuildCells(); ApplyView();
}
static void AutoSelect(int mode) {
    if (g_page != P_DUPES || g_dupes.files.empty()) return; std::vector<DupRec> recs; for (auto& e : g_dupes.files) recs.push_back({e.group, e.mtime, e.path.size()});
    auto rm = SelectDuplicatesToRemove(recs, mode); std::set<int> want(rm.begin(), rm.end()); ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED);
    for (size_t i = 0; i < g_view.size(); ++i) if (want.count(g_view[i])) ListView_SetItemState(g_list, (int)i, LVIS_SELECTED, LVIS_SELECTED);
    UpdateStatus(); UpdateToolbar(); SetMsg(Plural(want.size(), L"file", L"files") + L" selected (one copy per group kept). Review, then Move to Recycle Bin or Delete permanently.");
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
// ---- programs: uninstall (silent / standard) + leftovers ----
static bool LeftoverExists(const CleanItem& it) {
    if (it.kind == 6) return GetFileAttributesW(it.location.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (it.kind == 0) return !it.paths.empty() && GetFileAttributesW(it.paths[0].c_str()) != INVALID_FILE_ATTRIBUTES;
    if (it.kind == 2) { HKEY k; if (RegOpenKeyExW(it.root, it.subKey.c_str(), 0, KEY_QUERY_VALUE | it.view, &k) != ERROR_SUCCESS) return false; RegCloseKey(k); return true; }
    if (it.kind == 3) { HKEY k; if (RegOpenKeyExW(it.root, it.subKey.c_str(), 0, KEY_QUERY_VALUE | it.view, &k) != ERROR_SUCCESS) return false; bool e = RegQueryValueExW(k, it.valueName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; RegCloseKey(k); return e; }
    if (it.kind == 8) { SC_HANDLE m = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!m) return true; SC_HANDLE sv = OpenServiceW(m, it.cmd.c_str(), SERVICE_QUERY_STATUS); bool e = sv != nullptr; if (sv) CloseServiceHandle(sv); CloseServiceHandle(m); return e; }
    return true;   // scheduled tasks: kept until a Clean run removes them
}
static void MergeLeftovers() {   // leftovers and deep-scan results survive Cleaner rescans until used
    { std::vector<CleanItem> kept; for (auto& it : g_cl.items) { if (it.id.rfind(L"d.", 0) == 0) { if (g_cl.totalBytes >= it.bytes) g_cl.totalBytes -= it.bytes; } else kept.push_back(it); } g_cl.items = kept; }
    for (auto& it : g_deep) { g_cl.items.push_back(it); g_cl.totalBytes += it.bytes; }
    std::vector<CleanItem> keep; std::set<wstring> ids; for (auto& it : g_cl.items) ids.insert(it.id);
    for (auto& it : g_left) if (LeftoverExists(it)) { keep.push_back(it); if (!ids.count(it.id)) { g_cl.items.push_back(it); g_cl.totalBytes += it.bytes; if (IsRegItem(it)) ++g_cl.registryCount; } }
    g_left = keep;
    if (g_gotoLeft && !g_left.empty()) { /* handled in WM_SCAN_DONE */ }
}
struct UnJob { std::vector<ProgramEntry> items; bool silent, force; };
struct UnDone { wstring summary; std::vector<CleanItem> left; };
static DWORD WINAPI UninstallThread(LPVOID p) {
    std::unique_ptr<UnJob> j((UnJob*)p); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); int removed = 0, failed = 0, skipped = 0; wstring firstNote; std::vector<CleanItem> left; size_t i = 0;
    for (auto& e : j->items) {
        ++i; PostMessageW(g_main, WM_FILE_PROGRESS, 0, (LPARAM) new wstring((j->force ? L"Looking for leftovers of " : j->silent ? L"Uninstalling silently: " : L"Uninstalling: ") + e.name + L" (" + std::to_wstring(i) + L" of " + std::to_wstring(j->items.size()) + L")…"));
        if (j->force) { auto l = FindLeftovers(e, true); left.insert(left.end(), l.begin(), l.end()); ++removed; continue; }
        UninstallPlan pl = PlanUninstall(e, j->silent); if (!pl.available) { ++skipped; if (firstNote.empty()) firstNote = e.name + L": " + pl.note; continue; }
        UninstallResult r; RunUninstall(e, j->silent, r, nullptr);
        if (r.removed) { ++removed; auto l = FindLeftovers(e, false); left.insert(left.end(), l.begin(), l.end()); } else { ++failed; if (firstNote.empty()) firstNote = e.name + L": " + r.note; }
    }
    wstring msg = j->force ? std::to_wstring(removed) + L" program(s) analysed for leftovers" : std::to_wstring(removed) + L" uninstalled";
    if (failed) msg += L", " + std::to_wstring(failed) + L" not removed"; if (skipped) msg += L", " + std::to_wstring(skipped) + L" skipped"; if (!firstNote.empty()) msg += L" (" + firstNote + L")";
    msg += L". " + (left.empty() ? wstring(L"No leftovers found.") : std::to_wstring(left.size()) + L" leftover item(s) found - review them in the Cleaner, then press Clean selected to delete them.");
    PostMessageW(g_main, WM_UNINSTALL_DONE, 0, (LPARAM) new UnDone{msg, left}); CoUninitialize(); return 0;
}
static void RunUninstallBatch(bool silent, bool force) {
    if (g_page != P_PROGS || g_unBusy) return; auto sel = Selected(); if (sel.empty()) return; auto* job = new UnJob{{}, silent, force}; std::vector<wstring> names; int noSilent = 0;
    for (int i : sel) { auto& e = g_pg.entries[i]; job->items.push_back(e); auto pl = PlanUninstall(e, silent); if (silent && !force && !pl.available) ++noSilent; names.push_back(e.name + L"  " + e.version + (force ? wstring() : L"  — " + (pl.available ? pl.note : L"not available: " + pl.note))); }
    wstring q;
    if (force) q = L"FORCE REMOVE " + Plural(sel.size(), L"program", L"programs") + L"?\r\nThis skips the official uninstaller. Use it only when the uninstaller is broken or missing. The program's folders, registry keys, startup entries, services and tasks that point to it are listed for you to delete in the Cleaner. Nothing is deleted until you confirm there.";
    else if (silent) q = L"Uninstall " + Plural(sel.size(), L"program", L"programs") + L" silently (no windows)?" + (noSilent ? L" " + std::to_wstring(noSilent) + L" have no documented silent switch and will be skipped - use Uninstall for those." : L"") + L" Leftovers are searched for afterwards.";
    else q = L"Uninstall " + Plural(sel.size(), L"program", L"programs") + L" with the official uninstaller(s)? Each uninstaller opens its own window one after another. Leftovers are searched for afterwards.";
    if (!Confirm(q, names)) { delete job; return; }
    g_unBusy = true; UpdateToolbar(); UpdateStatus(); SetMsg(L"Working…"); HANDLE h = CreateThread(nullptr, 0, UninstallThread, job, 0, nullptr); if (h) CloseHandle(h); else { delete job; g_unBusy = false; }
}
static void ShowProgramDetails() {
    auto* e0 = g_page == P_PROGS && !Selected().empty() ? &g_pg.entries[Selected()[0]] : nullptr; if (!e0) return; ProgramEntry e = *e0; auto std1 = PlanUninstall(e, false), sil = PlanUninstall(e, true);
    wstring t = e.name + L"\r\n\r\nPUBLISHER\r\n" + e.publisher + L"\r\n\r\nVERSION / DATE\r\n" + e.version + L"  ·  installed " + e.installDate + L"\r\n\r\nTYPE\r\n" + e.type + L"  ·  " + e.scope + L"  ·  " + e.arch + L"\r\n\r\nSIZE\r\n" + (e.sizeBytes ? FormatBytes(e.sizeBytes) : wstring(L"not reported")) + L"\r\n\r\nLOCATION\r\n" + (e.installLocation.empty() ? wstring(L"(not recorded)") : e.installLocation) + (e.source.empty() ? L"" : L"  (" + e.source + L")") + L"\r\n\r\nREGISTRY KEY\r\n" + wstring(e.root == HKEY_CURRENT_USER ? L"HKCU" : L"HKLM") + L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + e.keyName +
        L"\r\n\r\nSTANDARD UNINSTALL\r\n" + (std1.available ? std1.file + L" " + std1.args + L"\r\n(" + std1.note + L")" : L"not available: " + std1.note) + L"\r\n\r\nSILENT UNINSTALL\r\n" + (sil.available ? sil.file + L" " + sil.args + L"\r\n(" + sil.note + L")" : L"not available: " + sil.note) + L"\r\n" + (std1.elevate ? L"\r\nWindows will ask for administrator permission when this program is uninstalled.\r\n" : L"");
    ShowReport(g_main, e.name + L" — Details", {{L"Details", t}}, {{L"Open install folder", [e](const wstring&) { wstring d = ExpandEnv(e.installLocation); if (!d.empty()) ShellExecuteW(nullptr, L"open", d.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }}, {L"Copy", [](const wstring& x) { SetClip(x); }}, {L"Close", nullptr}});
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
    } else if (g_page == P_PROGS) {
        AddItem(m, C_DETAILS, L"Details\tEnter", true, true); Sep(m); AddItem(m, C_PG_UNINSTALL, L"Uninstall (standard)\u2026"); AddItem(m, C_PG_SILENT, L"Uninstall silently\u2026"); AddItem(m, C_PG_FORCE, L"Force remove\u2026"); Sep(m); AddItem(m, C_OPENLOC, L"Open install folder", one); AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C");
    } else if (IsFilePage(g_page)) {
        AddItem(m, C_DETAILS, L"Details\tEnter", true, true); Sep(m); AddItem(m, C_OPENLOC, L"Open file location", !sel.empty()); AddItem(m, C_FS_RECYCLE, L"Move to Recycle Bin\tDel", TbOn(C_FS_RECYCLE)); AddItem(m, C_FS_DELETE, L"Delete permanently\u2026\tShift+Del", TbOn(C_FS_DELETE)); Sep(m); AddItem(m, C_COPYNAME, L"Copy path\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C");
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
        HMENU m = CreatePopupMenu(); int n = FilterCount(g_page); const wchar_t** names = g_page == P_STARTUP ? kStartupFilters : g_page == P_SERVICES ? kServiceFilters : g_page == P_CLEAN ? kCleanFilters : g_page == P_LARGE ? kLargeFilters : g_page == P_PROGS ? kProgFilters : kDriverFilters;
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
        if (!g_elevated) AddItem(m, C_ADMIN, L"Restart as administrator"); AddItem(m, C_CHECKUPDATE, L"Check for updates…"); AddItem(m, C_SETTINGS, L"Settings…\tCtrl+,"); AddItem(m, C_ABOUT, L"About Feather System Utilities\tF1"); Sep(m); AddItem(m, C_EXIT, L"Exit");
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
    case C_PG_UNINSTALL: RunUninstallBatch(false, false); break; case C_PG_SILENT: RunUninstallBatch(true, false); break; case C_PG_FORCE: RunUninstallBatch(false, true); break;
    case C_CLEAN: CleanSelected(); break; case C_CLEAN_SELALL: case C_CLEAN_ALL: CleanCheckAll(true); break; case C_CLEAN_NONE: CleanCheckAll(false); break; case C_CLEAN_REC: SetRecommended(); break;
    case C_CLEAN_DEEP: StartDeepScan(true); break; case C_CHECKUPDATE: CheckForUpdates(true); break;
    case C_FS_SCAN: StartFileScan(g_page == P_DUPES ? 1 : g_page == P_EMPTY ? 2 : 0); break; case C_FS_STOP: g_fsCancel = true; SetMsg(L"Stopping\u2026"); break;
    case C_FS_FOLDERS: case C_FS_MIN: case C_FS_AUTO: DropMenu(id); break; case C_FS_RECYCLE: RemoveSelected(false); break; case C_FS_DELETE: RemoveSelected(true); break;
    case C_FS_FOLDER_ADD: AddScanFolder(); break; case C_FS_FOLDER_DEFAULT: Cfg().scanRoots.clear(); SaveSettings(); SetMsg(L"Using the default folders."); break;
    case C_UNDO: Undo(); break; case C_REMOVE_DELAY: RemoveDelays(); break;
    case C_SVC_START: RunSvcOp(SvcOp::Start); break; case C_SVC_STOP: RunSvcOp(SvcOp::Stop); break; case C_SVC_RESTART: RunSvcOp(SvcOp::Restart); break;
    case C_SVC_PAUSE: RunSvcOp(SvcOp::Pause); break; case C_SVC_RESUME: RunSvcOp(SvcOp::Resume); break; case C_SVC_KILL: KillService(); break;
    case C_ST_AUTO: SetStartType(SERVICE_AUTO_START, false, L"Automatic"); break; case C_ST_AUTODELAY: SetStartType(SERVICE_AUTO_START, true, L"Automatic (delayed)"); break;
    case C_ST_MANUAL: SetStartType(SERVICE_DEMAND_START, false, L"Manual"); break; case C_ST_DISABLED: SetStartType(SERVICE_DISABLED, false, L"Disabled"); break;
    case C_PROF_SAVESEL: SaveProfileFlow(false); break; case C_PROF_SNAPSHOT: SaveProfileFlow(true); break; case C_PROF_OPENDIR: ShellExecuteW(nullptr, L"open", SubDir(L"profiles").c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case C_DETAILS: ShowDetails(); break; case C_COPYNAME: CopyNames(); break; case C_COPYDETAILS: CopyDetails(); break; case C_SELALL: SelectAll(); break; case C_EXPORT: ExportList(); break;
    case C_OPENLOC: if (g_page == P_PROGS) { auto s = Selected(); if (!s.empty()) { wstring d = ExpandEnv(g_pg.entries[s[0]].installLocation); if (!d.empty() && GetFileAttributesW(d.c_str()) != INVALID_FILE_ATTRIBUTES) ShellExecuteW(nullptr, L"open", d.c_str(), nullptr, nullptr, SW_SHOWNORMAL); else Warn(L"The install folder is not recorded or no longer exists."); } } else if (IsFilePage(g_page)) { auto s = Selected(); if (!s.empty()) OpenFileLocation(FS(g_page).files[s[0]].path); } else if (g_page == P_STARTUP) { if (auto* e = OneStartup()) OpenFileLocation(e->program); } else if (auto* s = OneService()) OpenFileLocation(s->program); break;
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
    if (p == P_PROGS) { auto& e = g_pg.entries[row]; if (col == 1) return Lower(e.publisher).find(L"microsoft") != wstring::npos ? cText : cLav; if (col == 4) return cGreen; if (col == 3 || col == 6 || col == 8) return cDim; return cText; }
    if (p == P_EMPTY) return col == 2 ? cDim : col == 1 ? cDim : cText;
    if (IsFilePage(p)) { if (col == 1) return cGreen; if (col == 3 && p == P_DUPES) return (g_dupes.files[row].group & 1) ? cAmber : cLav; if (col == 4 && p == P_DUPES) return cDim; return col == 2 ? cDim : cText; }
    if (p == P_CLEAN) { auto& e = g_cl.items[row]; if (col == 0) return g_checked.count(e.id) ? cAccent : cDim; if (col == 2) return IsRegItem(e) ? cAmber : cText; if (col == 3) return e.risk == 2 ? cRed : e.risk == 1 ? cAmber : cGreen; if (col == 4) return e.bytes ? cGreen : cDim; if (col == 5) return e.needsAdmin && !g_elevated ? cAmber : cDim; return col == 7 ? cDim : cText; }
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
    int navW = S(190), stH = S(26), searchW = S(220), searchH = S(26); ComputeLayout();
    MoveWindow(g_nav, 0, S(86), navW, h - S(86) - stH, TRUE);
    { HDC dc = GetDC(g_main); HFONT of = (HFONT)SelectObject(dc, g_font); int x = navW + S(20); auto& pages = Nav()[GroupOf(g_page)].pages;
      for (size_t i = 0; i < g_subBtns.size() && i < pages.size(); ++i) { SIZE sz; const wchar_t* t = SubLabel(pages[i]); GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz); int bw = sz.cx + S(38); MoveWindow(g_subBtns[i], x, g_subTop + S(10), bw, S(32), TRUE); x += bw + S(8); }
      SelectObject(dc, of); ReleaseDC(g_main, dc); }
    if (g_banBtn) { int bx = w - S(32) - S(150); int by = g_banTop + (g_banH - S(42)) / 2; MoveWindow(g_banBtn, bx, by, S(150), S(42), TRUE); MoveWindow(g_banLink, bx - S(16) - S(130), by + S(6), S(130), S(30), TRUE); }
    TbLayout();
    int sx = g_tbLeft - searchW - S(6); if (sx < navW + S(300)) sx = navW + S(300); int ty = g_tbTop + (g_tbH - searchH) / 2; g_searchBox = {sx, ty, sx + searchW, ty + searchH}; MoveWindow(g_search, sx + S(8), ty + S(5), searchW - S(16), searchH - S(10), TRUE);
    int bodyH = h - g_bodyTop - stH; if (bodyH < S(60)) bodyH = S(60); MoveWindow(g_list, navW, g_bodyTop, w - navW, bodyH, TRUE); MoveWindow(g_panel, navW, g_bodyTop, w - navW, bodyH, TRUE);
    MoveWindow(g_status, 0, h - stH, w, stH, TRUE); BringWindowToTop(g_search); InvalidateRect(g_main, nullptr, TRUE);
    if (IsPanelPage(g_page)) RebuildPanel();
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
        for (auto& g : Nav()) SendMessageW(g_nav, LB_ADDSTRING, 0, (LPARAM)g.label);
        g_banBtn = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, h, (HMENU)ID_BANBTN, g_inst, nullptr); g_banLink = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, h, (HMENU)ID_BANLINK, g_inst, nullptr);
        SendMessageW(g_banBtn, WM_SETFONT, (WPARAM)g_font, TRUE); SendMessageW(g_banLink, WM_SETFONT, (WPARAM)g_font, TRUE);
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
        ShowPage(P_DASH); StartScan(); SetTimer(h, 77, 9000, nullptr); return 0; }
    case WM_TIMER: if (w == 77) { KillTimer(h, 77); long long now = (long long)time(nullptr); if (Cfg().autoUpdate && now - Cfg().lastUpdateCheck > 20 * 3600) CheckForUpdates(false); } return 0;
    case WM_SIZE: Layout(LOWORD(l), HIWORD(l)); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = {S(820), S(480)}; return 0;
    case WM_DPICHANGED: { g_dpi = HIWORD(w); MakeFonts(); for (HWND c : {g_nav, g_search, g_list, g_panel, g_status}) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE); for (auto& it : g_items) if (it.h) SendMessageW(it.h, WM_SETFONT, (WPARAM)g_font, TRUE);
        ApplyDensity(); auto* r = (RECT*)l; SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_brBg); RECT sb = {0, 0, S(190), r.bottom}; FillRect((HDC)w, &sb, g_brRail); if (g_tbH) { RECT st = TbStrip(); FillRect((HDC)w, &st, g_brBar); } return 1; }
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_fontTitle); SetTextColor(dc, cText); TextOutW(dc, S(20), S(16), L"Feather", 7); SelectObject(dc, g_font); SetTextColor(dc, cDim); TextOutW(dc, S(20), S(42), L"System Utilities", 16); wstring ver = L"Version " FSU_VERSION; TextOutW(dc, S(20), S(60), ver.c_str(), (int)ver.size());
        if (g_banH) { RECT card = {S(190) + S(16), g_banTop + S(8), cr.right - S(16), g_banTop + g_banH - S(4)}; gfx::RoundRect(dc, card, S(12), cBar, P.dark ? cBg : cBar, cGrid);
            SelectObject(dc, g_fontTitle); SetTextColor(dc, cAmber); RECT hr = {card.left + S(18), card.top + S(10), card.right - S(330), card.top + S(38)}; DrawTextW(dc, g_banHead.c_str(), -1, &hr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            SelectObject(dc, g_font); SetTextColor(dc, cDim); RECT sr = {card.left + S(18), card.top + S(40), card.right - S(330), card.bottom - S(4)}; DrawTextW(dc, g_banSub.c_str(), -1, &sr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS); }
        if (g_tbH) { RECT st = TbStrip(); FillRect(dc, &st, g_brBar); HPEN p = CreatePen(PS_SOLID, 1, cGrid); HGDIOBJ op = SelectObject(dc, p);
            for (auto& it : g_items) if (it.sep) { int x = (it.r.left + it.r.right) / 2; MoveToEx(dc, x, st.top + S(10), nullptr); LineTo(dc, x, st.bottom - S(10)); }
            SelectObject(dc, op); DeleteObject(p);
            if (!IsPanelPage(g_page)) { RECT sbx = g_searchBox; gfx::RoundRect(dc, sbx, S(8), cBg, cBg, cGrid); } }
        EndPaint(h, &ps); return 0; }
    case WM_SETFOCUS: SetFocus(g_nav); return 0;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { HDC dc = (HDC)w; SetTextColor(dc, cText); if ((HWND)l == g_nav) { SetBkColor(dc, cRail); return (LRESULT)g_brRail; } SetBkColor(dc, cBg); return (LRESULT)g_brBg; }
    case WM_MEASUREITEM: { auto* mi = (MEASUREITEMSTRUCT*)l; if (mi->CtlID == ID_NAV) { mi->itemHeight = S(48); return TRUE; } break; }
    case WM_DRAWITEM: { auto* di = (DRAWITEMSTRUCT*)l;
        if (di->CtlID == ID_NAV) { DrawSidebarItem(di); return TRUE; } if (di->CtlID == ID_BANBTN || di->CtlID == ID_BANLINK) { DrawBannerBtn(di); return TRUE; }
        if (di->CtlID >= (UINT)ID_SUB0 && di->CtlID < (UINT)ID_SUB0 + 8) { DrawSubTab(di); return TRUE; } DrawToolbarButton(di); return TRUE; }
    case WM_SCAN_DONE: {
        std::unique_ptr<ScanMsg> r((ScanMsg*)l); if (r->gen != g_gen.load()) return 0;
        if (r->which == 0) g_su = std::move(r->su); else if (r->which == 1) g_sv = std::move(r->sv); else if (r->which == 2) g_dv = std::move(r->dv); else if (r->which == 4) g_pg = std::move(r->pr);
        else { g_cl = std::move(r->cl); MergeLeftovers(); g_scanning = false; if (g_checked.empty() && !g_cl.items.empty()) { for (auto& e : g_cl.items) if (e.risk == 0 && !(e.needsAdmin && !g_elevated)) g_checked.insert(e.id); } }
        BuildCells(); if (g_careRunning) CareOnBaseDone(r->which);
        if (r->which == 3 && g_gotoLeft) { g_gotoLeft = false; g_filter[P_CLEAN] = 3; ShowPage(P_CLEAN); SelectAll(); SetMsg(L"Leftovers found. Review the selected items (Details shows why each is listed), then press Clean selected. Folders are deleted permanently; registry entries are backed up first."); return 0; }
        if (IsActionList(g_page)) ApplyView(); else if (g_page == P_DASH) { RebuildPanel(); UpdateStatus(); } else UpdateStatus();
        return 0; }
    case WM_UNINSTALL_DONE: { std::unique_ptr<UnDone> d((UnDone*)l); g_unBusy = false; for (auto& it : d->left) { bool dup = false; for (auto& o : g_left) if (o.id == it.id) dup = true; if (!dup) g_left.push_back(it); } g_gotoLeft = !d->left.empty(); SetMsg(d->summary); g_scanning = false; StartScan(); return 0; }
    case WM_DEEP_DONE: { std::unique_ptr<DeepDone> d((DeepDone*)l); g_fsBusy = false; g_deep = std::move(d->scan.items); for (auto& e : g_deep) if (e.risk == 0) g_checked.insert(e.id); MergeLeftovers(); BuildCells(); SetMsg(d->scan.coverage);
        if (g_careRunning) { CareOnExtraDone(9); } else { g_filter[P_CLEAN] = 0; if (g_page == P_CLEAN) ApplyView(); else ShowPage(P_CLEAN); RefreshCleanCells(); } UpdateStatus(); UpdateToolbar(); return 0; }
    case WM_UPDATE_INFO: { std::unique_ptr<UpdMsg> m2((UpdMsg*)l); Cfg().lastUpdateCheck = (long long)time(nullptr); SaveSettings(); auto& u = m2->u;
        if (!u.ok) { if (m2->manual) { SetMsg(L"Update check failed."); Warn(L"Could not check for updates: " + u.error); } return 0; }
        if (CompareVersions(u.version, FSU_VERSION) <= 0) { if (m2->manual) { SetMsg(L"You have the latest version."); DarkMsg(g_main, kApp, L"You are up to date. Installed version: " FSU_VERSION L"\r\nLatest version: " + u.version, 0); } return 0; }
        wstring q = L"Version " + u.version + (u.date.empty() ? wstring() : L" (" + u.date + L")") + L" is available. You have " FSU_VERSION L".\r\n\r\n" + u.notes + L"\r\n\r\nUpdate now? The app downloads the installer, checks its SHA-256, installs it (Windows asks for administrator permission) and reopens.";
        if (DarkMsg(g_main, L"Update available", q, 2) != IDYES) return 0; SetMsg(L"Downloading update\u2026"); HANDLE th = CreateThread(nullptr, 0, UpdateDownloadThread, new UpdDl{u}, 0, nullptr); if (th) CloseHandle(th); return 0; }
    case WM_UPDATE_READY: { std::unique_ptr<wstring> info((wstring*)l); if (!w) { SetMsg(L"Update failed."); Warn(L"The update was not installed: " + *info); return 0; }
        wstring err; if (LaunchUpdateInstaller(*info, err)) { SetMsg(L"Installing update\u2026"); SaveSettings(); DestroyWindow(g_main); } else Warn(err); return 0; }
    case WM_FILE_PROGRESS: { std::unique_ptr<wstring> m2((wstring*)l); if (g_fsBusy) SetMsg(*m2); return 0; }
    case WM_FILE_DONE: { std::unique_ptr<FsDone> d((FsDone*)l); g_fsBusy = false; if (d->mode == 1) { g_dupes = std::move(d->scan); g_dupesDone = true; } else if (d->mode == 2) { g_empty = std::move(d->scan); g_emptyDone = true; } else { g_large = std::move(d->scan); g_largeDone = true; }
        auto& sc = d->mode == 1 ? g_dupes : d->mode == 2 ? g_empty : g_large; SetMsg(d->mode == 2 ? std::to_wstring(sc.files.size()) + L" empty folders found. " + sc.coverage : d->mode == 1 ? std::to_wstring(sc.groups) + L" duplicate groups found, " + FormatBytes(sc.reclaimable) + L" reclaimable. " + sc.coverage : std::to_wstring(sc.files.size()) + L" large files found. " + sc.coverage);
        BuildCells(); if (IsActionList(g_page)) ApplyView(); else if (g_page == P_DASH) RebuildPanel(); UpdateStatus(); UpdateToolbar(); if (g_careRunning) CareOnExtraDone(d->mode == 1 ? 7 : d->mode == 2 ? 8 : 6); return 0; }
    case WM_JOB_DONE: { std::unique_ptr<JobDone> j((JobDone*)l); if (j->deep) g_deep.clear(); g_checked.clear(); SetMsg(j->summary); g_scanning = false; StartScan(); return 0; }
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
            case NM_CLICK: { if (g_page == P_CLEAN) { auto* a = (NMITEMACTIVATE*)l; LVHITTESTINFO ht{}; ht.pt = a->ptAction; if (ListView_SubItemHitTest(g_list, &ht) >= 0 && ht.iSubItem == 0) ToggleCleanAtView(ht.iItem); } return 0; }
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
        if (id == ID_NAV && code == LBN_SELCHANGE) { int s = (int)SendMessageW(g_nav, LB_GETCURSEL, 0, 0); if (s >= 0 && s < (int)Nav().size() && s != GroupOf(g_page)) ShowPage(g_lastInGroup[s]); else if (s >= 0) InvalidateRect(g_nav, nullptr, TRUE); return 0; }
        if (id >= ID_SUB0 && id < ID_SUB0 + 8) { auto& pages = Nav()[GroupOf(g_page)].pages; size_t i = id - ID_SUB0; if (i < pages.size() && pages[i] != g_page) ShowPage(pages[i]); return 0; }
        if (id == ID_BANBTN) { if (g_banCmd) DoCommand(g_banCmd); return 0; } if (id == ID_BANLINK) { if (g_banLinkCmd) DoCommand(g_banLinkCmd); return 0; }
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
    if (c.find(L"--broker") != wstring::npos) { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); LoadSettings(); return BrokerMain(GetCommandLineW(), BrokerExecute); }   // elevated administrator helper (no window, no single-instance lock)
    g_mtx = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance");
    Page req = P_DASH; bool haveReq = true; if (c.find(L"--services") != wstring::npos) req = P_SERVICES; else if (c.find(L"--startup") != wstring::npos) req = P_STARTUP; else if (c.find(L"--drivers") != wstring::npos) req = P_DRIVERS; else if (c.find(L"--cleaner") != wstring::npos) req = P_CLEAN; else if (c.find(L"--large") != wstring::npos) req = P_LARGE; else if (c.find(L"--duplicates") != wstring::npos) req = P_DUPES; else haveReq = false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { if (HWND e = FindWindowW(L"FeatherSystemUtilitiesWnd", nullptr)) PostMessageW(e, WM_ACTIVATE_PAGE, req, 0); return 0; }   // F004
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadSettings(); MigrateLegacyData();
    if (c.find(L"--classic") == wstring::npos && RunWebUi(inst, show, haveReq ? (int)req : 0, &g_mtx) == 0) return 0;   // new HTML UI (WebView2); falls back to the classic Win32 UI when the runtime is missing
    P = PalFor(Cfg().theme);
    INITCOMMONCONTROLSEX ic{sizeof ic, ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES}; InitCommonControlsEx(&ic);
    DarkModeInit(Cfg().theme); gfx::Init();
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
            if (ctrl && m.wParam >= '1' && m.wParam <= '7') { size_t gi = m.wParam - '1'; if (gi < Nav().size()) ShowPage(g_lastInGroup[gi]); continue; }
            if (m.wParam == VK_SPACE && onList && g_page == P_CLEAN) { ToggleCleanSelection(); continue; }
            if (m.wParam == VK_F5) { DoCommand(C_REFRESH); continue; } if (m.wParam == VK_F1) { About(); continue; } if (ctrl && m.wParam == VK_OEM_COMMA) { ShowPage(P_SETTINGS); continue; }
            if (ctrl && !shift && m.wParam == 'Z' && !inEdit) { Undo(); continue; }
            if (ctrl && m.wParam == 'E' && listy) { ExportList(); continue; }
            if (!inEdit && IsActionList(g_page)) {
                if (ctrl && m.wParam == 'A') { SelectAll(); continue; }
                if (ctrl && shift && m.wParam == 'C') { CopyDetails(); continue; } if (ctrl && !shift && m.wParam == 'C' && onList) { CopyNames(); continue; }
                if (ctrl && m.wParam == 'N' && g_page == P_STARTUP) { AddEntry(); continue; }
                if (m.wParam == VK_DELETE && g_page == P_STARTUP && onList) { DeleteStartup(); continue; }
                if (m.wParam == VK_DELETE && IsFilePage(g_page) && onList) { RemoveSelected(shift); continue; }
                if (m.wParam == VK_RETURN && onList) { ShowDetails(); continue; }
                if (g_page == P_SERVICES && ctrl && shift) { if (m.wParam == 'S') { RunSvcOp(SvcOp::Start); continue; } if (m.wParam == 'T') { RunSvcOp(SvcOp::Stop); continue; } if (m.wParam == 'R') { RunSvcOp(SvcOp::Restart); continue; } if (m.wParam == 'K') { KillService(); continue; } }
            }
        }
        if (!IsDialogMessageW(w, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    gfx::Shutdown(); if (g_mtx) CloseHandle(g_mtx); return 0;
}
