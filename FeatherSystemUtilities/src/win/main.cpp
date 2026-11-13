// Feather System Utilities - one window: Startup + Services (+ Dashboard, History, Settings).
// Dark native Win32 UI. All Windows changes go through the journal; see app.h / startup.cpp / services.cpp.
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
static const wchar_t* kVer = L"0.2.0";

// ---- palette ----
static const COLORREF cBg = RGB(14, 14, 14), cBar = RGB(22, 22, 22), cText = RGB(228, 228, 228), cDim = RGB(128, 128, 128), cGrid = RGB(40, 40, 40),
    cGreen = RGB(92, 214, 92), cRed = RGB(255, 100, 100), cLav = RGB(160, 160, 255), cAmber = RGB(240, 190, 80), cHot = RGB(44, 44, 44),
    cSel = RGB(32, 56, 92), cSelLine = RGB(70, 130, 220), cDisabledTb = RGB(98, 98, 98), cRail = RGB(18, 18, 18), cRailSel = RGB(40, 44, 60), cAccent = RGB(120, 150, 255);

enum Page { P_DASH, P_STARTUP, P_SERVICES, P_HISTORY, P_SETTINGS, P_COUNT };
static const wchar_t* kPageNames[P_COUNT] = {L"Dashboard", L"Startup", L"Services", L"History", L"Settings"};
enum Cmd { C_REFRESH = 1000, C_ENABLE, C_DISABLE, C_DELETE, C_STARTLATER, C_ADD, C_RESTORE, C_UNDO, C_FILTER, C_MORE, C_SVC_START, C_SVC_STOP, C_SVC_RESTART, C_SVC_PAUSE, C_SVC_RESUME,
    C_STARTTYPE, C_DETAILS, C_COPYNAME, C_COPYDETAILS, C_OPENLOC, C_REGEDIT, C_VT, C_SEARCHONLINE, C_SELALL, C_ADMIN, C_SETTINGS, C_ABOUT, C_EXIT, C_OPENBACKUPS, C_OPENDATA,
    C_ST_AUTO, C_ST_AUTODELAY, C_ST_MANUAL, C_ST_DISABLED, C_FILTER_BASE = 1100, C_RESTORE_BASE = 1200 };
enum { ID_NAV = 100, ID_TB, ID_SEARCH, ID_LIST, ID_STATUS, ID_INFO };
enum { WM_SCAN_DONE = WM_APP + 1, WM_ACTIVATE_PAGE, WM_JOB_DONE, TBM_LEFTEDGE };

// ---- state ----
static HINSTANCE g_inst; static HWND g_main, g_nav, g_tb, g_search, g_list, g_hdr, g_status, g_info;
static HFONT g_font, g_fontSmall; static UINT g_dpi = 96; static HANDLE g_mtx;
static HBRUSH g_brBg, g_brBar, g_brRail;
static Page g_page = P_DASH; static bool g_elevated = false; static wstring g_user;
static StartupScan g_su; static ServiceScan g_sv;
static std::vector<std::vector<wstring>> g_cells[P_COUNT];
static std::vector<int> g_view;                 // view row -> entry index
static int g_filter[P_COUNT]; static wstring g_query[P_COUNT];
static int g_sortCol[P_COUNT] = {0, 1, 0, 0, 0}; static bool g_sortAsc[P_COUNT] = {true, true, true, true, true};
static std::atomic<int> g_gen{0}; static bool g_scanning = false;
static wstring g_stMsg;                        // last result shown in the status bar
static std::set<wstring> g_keepSel;            // selection keys to restore after a rescan
static wstring g_histText;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

struct UndoItem { int kind; StartupEntry se; bool wasEnabled; wstring name; DWORD type; bool delayed; int group; };
static std::vector<UndoItem> g_undo; static int g_groupSeq = 0;

struct ScanMsg { int gen; StartupScan su; ServiceScan sv; bool hasSu, hasSv; wstring hist; };
struct JobDone { wstring summary; };

// ---- columns ----
struct Col { const wchar_t* name; int w; bool right; };
static const Col kStartupCols[] = {{L"Name", 230, false}, {L"Status", 80, false}, {L"Sign-in impact", 100, false}, {L"Warnings", 150, false}, {L"Publisher", 190, false}, {L"Type", 190, false},
    {L"Starts", 120, false}, {L"For", 100, false}, {L"Command", 640, false}, {L"Location", 330, false}};
static const Col kServiceCols[] = {{L"Name", 250, false}, {L"Service name", 170, false}, {L"Status", 80, false}, {L"Start type", 150, false}, {L"Safety", 85, false}, {L"PID", 60, true},
    {L"Log on as", 130, false}, {L"Description", 520, false}, {L"Warnings", 200, false}};
static const Col kHistCols[] = {{L"Time (UTC)", 160, false}, {L"Item", 260, false}, {L"Before", 160, false}, {L"After", 220, false}, {L"Result", 520, false}};
static const Col* ColsFor(Page p, int& n) {
    if (p == P_STARTUP) { n = 10; return kStartupCols; } if (p == P_SERVICES) { n = 9; return kServiceCols; } n = 5; return kHistCols;
}
static const wchar_t* kStartupFilters[] = {L"Everything", L"Enabled", L"Disabled", L"Needs attention (warnings)", L"New in the last 7 days", L"Not from Microsoft", L"Registry (Run keys)", L"Startup folders", L"Scheduled tasks", L"Explorer add-ons"};
static const wchar_t* kServiceFilters[] = {L"All services", L"Running", L"Stopped", L"Automatic but not running", L"Disabled", L"Third-party only", L"Critical to Windows", L"Needs attention (warnings)"};
static const wchar_t* FilterName(Page p) { return p == P_STARTUP ? kStartupFilters[g_filter[p]] : p == P_SERVICES ? kServiceFilters[g_filter[p]] : L""; }

// ================= toolbar (owner-drawn, keyboard accessible) =================
struct TbItem { int id; wstring text; bool enabled = true, dropdown = false, sep = false, right = false; RECT r{}; };
static std::vector<TbItem> g_items; static int g_hot = -1, g_press = -1, g_tbFocus = -1, g_tbLeft = 0; static bool g_tbKbd = false;

static void TbLayout() {
    HDC dc = GetDC(g_tb); HFONT of = (HFONT)SelectObject(dc, g_font); RECT cr; GetClientRect(g_tb, &cr);
    int x = S(6), h = cr.bottom; int rw = 0;
    auto widthOf = [&](TbItem& it) { if (it.sep) return S(10); SIZE sz; wstring t = it.text + (it.dropdown ? L" ▾" : L""); GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz); return (int)sz.cx + S(26); };
    for (auto& it : g_items) if (it.right) rw += widthOf(it);
    int rx = cr.right - rw - S(6); g_tbLeft = rx;
    for (auto& it : g_items) { int w = widthOf(it); int& px = it.right ? rx : x; it.r = {px, 0, px + w, h}; px += w; }
    SelectObject(dc, of); ReleaseDC(g_tb, dc); InvalidateRect(g_tb, nullptr, TRUE);
}
static void TbSet(std::vector<TbItem> items) { g_items = std::move(items); g_hot = g_press = -1; g_tbFocus = -1; TbLayout(); }
static TbItem* TbFind(int id) { for (auto& i : g_items) if (i.id == id) return &i; return nullptr; }
static void TbEnable(int id, bool en) { if (auto* i = TbFind(id)) if (i->enabled != en) { i->enabled = en; InvalidateRect(g_tb, &i->r, FALSE); } }
static void TbText(int id, const wstring& t) { if (auto* i = TbFind(id)) { i->text = t; TbLayout(); } }
static int TbHit(POINT p) { for (size_t i = 0; i < g_items.size(); ++i) if (!g_items[i].sep && PtInRect(&g_items[i].r, p)) return (int)i; return -1; }
static LRESULT CALLBACK TbProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_GETDLGCODE: return DLGC_WANTARROWS;
    case WM_SIZE: TbLayout(); return 0;
    case TBM_LEFTEDGE: return g_tbLeft;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr);
        FillRect(dc, &cr, g_brBar); SelectObject(dc, g_font); SetBkMode(dc, TRANSPARENT);
        bool focused = GetFocus() == h && g_tbKbd;
        for (size_t i = 0; i < g_items.size(); ++i) {
            auto& it = g_items[i];
            if (it.sep) { HPEN p = CreatePen(PS_SOLID, 1, cGrid); SelectObject(dc, p); MoveToEx(dc, (it.r.left + it.r.right) / 2, S(10), nullptr); LineTo(dc, (it.r.left + it.r.right) / 2, cr.bottom - S(10)); DeleteObject(SelectObject(dc, GetStockObject(BLACK_PEN))); continue; }
            if (it.enabled && ((int)i == g_hot || (int)i == g_press)) { RECT rr = it.r; InflateRect(&rr, -S(2), -S(6)); HBRUSH b = CreateSolidBrush((int)i == g_press ? RGB(60, 60, 60) : cHot); FillRect(dc, &rr, b); DeleteObject(b); }
            if (focused && (int)i == g_tbFocus) { RECT rr = it.r; InflateRect(&rr, -S(3), -S(7)); HPEN p = CreatePen(PS_DOT, 1, cAccent); SelectObject(dc, p); SelectObject(dc, GetStockObject(NULL_BRUSH)); Rectangle(dc, rr.left, rr.top, rr.right, rr.bottom); DeleteObject(SelectObject(dc, GetStockObject(BLACK_PEN))); }
            SetTextColor(dc, it.enabled ? cText : cDisabledTb);
            wstring t = it.text + (it.dropdown ? L" ▾" : L""); RECT tr = it.r; DrawTextW(dc, t.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        EndPaint(h, &ps); return 0; }
    case WM_MOUSEMOVE: { POINT p{(short)LOWORD(l), (short)HIWORD(l)}; int hit = TbHit(p); if (hit != g_hot) { g_hot = hit; InvalidateRect(h, nullptr, FALSE); TRACKMOUSEEVENT t{sizeof t, TME_LEAVE, h, 0}; TrackMouseEvent(&t); } return 0; }
    case WM_MOUSELEAVE: g_hot = -1; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: { POINT p{(short)LOWORD(l), (short)HIWORD(l)}; int hit = TbHit(p); if (hit >= 0 && g_items[hit].enabled) { g_press = hit; g_tbFocus = hit; g_tbKbd = false; SetFocus(h); SetCapture(h); InvalidateRect(h, nullptr, FALSE); } return 0; }
    case WM_LBUTTONUP: { if (GetCapture() == h) ReleaseCapture(); POINT p{(short)LOWORD(l), (short)HIWORD(l)}; int hit = TbHit(p); int pr = g_press; g_press = -1; InvalidateRect(h, nullptr, FALSE);
        if (pr >= 0 && hit == pr && g_items[pr].enabled) PostMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(g_items[pr].id, 0), (LPARAM)h); return 0; }
    case WM_SETFOCUS: if (g_tbFocus < 0) for (size_t i = 0; i < g_items.size(); ++i) if (!g_items[i].sep && g_items[i].enabled) { g_tbFocus = (int)i; break; } InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_KILLFOCUS: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_KEYDOWN: { g_tbKbd = true;
        int n = (int)g_items.size(), dir = w == VK_RIGHT ? 1 : w == VK_LEFT ? -1 : 0;
        if (dir) { int i = g_tbFocus; for (int k = 0; k < n; ++k) { i = (i + dir + n) % n; if (!g_items[i].sep && g_items[i].enabled) { g_tbFocus = i; break; } } InvalidateRect(h, nullptr, FALSE); return 0; }
        if ((w == VK_RETURN || w == VK_SPACE) && g_tbFocus >= 0 && g_items[g_tbFocus].enabled) { PostMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(g_items[g_tbFocus].id, 0), (LPARAM)h); return 0; }
        break; }
    }
    return DefWindowProcW(h, m, w, l);
}

// ================= status bar =================
static LRESULT CALLBACK StProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static RECT linkR{}; 
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); FillRect(dc, &cr, g_brBar); SelectObject(dc, g_font); SetBkMode(dc, TRANSPARENT);
        wchar_t left[512]; GetWindowTextW(h, left, 512);
        wstring link = g_elevated ? L"Running as administrator" : L"Restart as administrator";
        SIZE sz; GetTextExtentPoint32W(dc, link.c_str(), (int)link.size(), &sz);
        linkR = {cr.right - S(10) - sz.cx - S(24), 0, cr.right - S(10), cr.bottom};
        SetTextColor(dc, g_elevated ? cDim : cAccent); DrawTextW(dc, link.c_str(), -1, &linkR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, cText); RECT lr{S(8), 0, cr.right / 2, cr.bottom}; DrawTextW(dc, left, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, g_stMsg.empty() ? cDim : cText); RECT mr{cr.right / 2 - S(40), 0, linkR.left - S(12), cr.bottom};
        wstring msg = g_stMsg.empty() ? (g_page == P_STARTUP ? L"Tip: right-click an entry. Every change is journaled; Ctrl+Z undoes it." : g_page == P_SERVICES ? L"Tip: select several services (Ctrl/Shift) to act on them together." : L"") : g_stMsg;
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
static std::vector<int> Selected() {   // entry indices
    std::vector<int> r; int i = -1; while ((i = ListView_GetNextItem(g_list, i, LVNI_SELECTED)) != -1) if (i < (int)g_view.size()) r.push_back(g_view[i]); return r;
}
static wstring Plural(size_t n, const wchar_t* one, const wchar_t* many) { return std::to_wstring(n) + L" " + (n == 1 ? one : many); }
static wstring BulletList(const std::vector<wstring>& v) {
    wstring o; size_t n = 0; for (auto& s : v) { if (n++ == 8) { o += L"\r\n  • …and " + std::to_wstring(v.size() - 8) + L" more"; break; } o += L"\r\n  • " + s; } return o;
}
static bool Confirm(const wstring& q, const std::vector<wstring>& items) {
    wstring t = q + L"\r\n" + BulletList(items);
    return MessageBoxW(g_main, t.c_str(), kApp, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;   // risky confirmations default to No
}
static int Rank(Page p, const wstring& v) {
    if (p == P_STARTUP) return v == L"Enabled" ? 0 : v == L"One-time" ? 1 : v == L"Disabled" ? 2 : 3;
    return v == L"Running" ? 0 : v == L"Paused" ? 1 : v == L"Stopped" ? 3 : 2;
}

// ================= data -> rows =================
static void BuildCells() {
    g_cells[P_STARTUP].clear(); for (auto& e : g_su.entries) g_cells[P_STARTUP].push_back({e.name, e.state, L"Not measured", e.warnings, e.publisher, e.type, e.starts, e.scope, e.command, e.location});
    g_cells[P_SERVICES].clear(); for (auto& e : g_sv.entries) g_cells[P_SERVICES].push_back({e.display, e.name, e.state, e.start, e.safety, e.pid ? std::to_wstring(e.pid) : L"", e.account, e.description, e.warnings});
}
static bool PassFilter(Page p, int idx) {
    int f = g_filter[p]; if (!f) return true;
    if (p == P_STARTUP) { auto& e = g_su.entries[idx];
        switch (f) { case 1: return e.state == L"Enabled"; case 2: return e.state == L"Disabled"; case 3: return e.hasWarning; case 4: return e.recent; case 5: return !e.microsoft;
            case 6: return e.type.rfind(L"Registry", 0) == 0; case 7: return e.type == L"Startup folder"; case 8: return e.type == L"Scheduled task"; case 9: return e.type.rfind(L"Explorer", 0) == 0; } }
    else if (p == P_SERVICES) { auto& e = g_sv.entries[idx];
        switch (f) { case 1: return e.stateCode == SERVICE_RUNNING; case 2: return e.stateCode == SERVICE_STOPPED; case 3: return e.startType == SERVICE_AUTO_START && e.stateCode != SERVICE_RUNNING;
            case 4: return e.startType == SERVICE_DISABLED; case 5: return e.safety == L"Third-party"; case 6: return e.critical; case 7: return !e.warnings.empty(); } }
    return true;
}
static size_t EntryCount(Page p) { return p == P_STARTUP ? g_su.entries.size() : p == P_SERVICES ? g_sv.entries.size() : g_cells[p].size(); }
static wstring KeyOf(Page p, int idx) { return p == P_STARTUP ? g_su.entries[idx].key : p == P_SERVICES ? g_sv.entries[idx].name : L""; }

static void UpdateStatus();
static void UpdateToolbar();
static void ApplyView() {
    Page p = g_page; if (p != P_STARTUP && p != P_SERVICES && p != P_HISTORY) return;
    g_view.clear(); auto& cells = g_cells[p];
    for (size_t i = 0; i < cells.size(); ++i) if (PassFilter(p, (int)i) && MatchesQuery(cells[i], g_query[p])) g_view.push_back((int)i);
    int col = g_sortCol[p]; bool asc = g_sortAsc[p]; bool statusCol = (p == P_STARTUP && col == 1) || (p == P_SERVICES && col == 2); bool numCol = p == P_SERVICES && col == 5;
    std::stable_sort(g_view.begin(), g_view.end(), [&](int a, int b) {
        const wstring &x = cells[a][col], &y = cells[b][col];
        if (x.empty() != y.empty()) return !x.empty();                          // missing values always last
        int c;
        if (statusCol) c = Rank(p, x) - Rank(p, y);
        else if (numCol) c = _wtoi(x.c_str()) - _wtoi(y.c_str());
        else { wstring lx = Lower(x), ly = Lower(y); c = lx < ly ? -1 : lx > ly ? 1 : 0; }
        if (!c && col != 0) { wstring lx = Lower(cells[a][0]), ly = Lower(cells[b][0]); return lx < ly; }
        return asc ? c < 0 : c > 0; });
    ListView_SetItemCountEx(g_list, (int)g_view.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    if (!g_keepSel.empty()) { ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED);
        for (size_t i = 0; i < g_view.size(); ++i) if (g_keepSel.count(KeyOf(p, g_view[i]))) ListView_SetItemState(g_list, (int)i, LVIS_SELECTED, LVIS_SELECTED); g_keepSel.clear(); }
    InvalidateRect(g_list, nullptr, TRUE); InvalidateRect(g_hdr, nullptr, TRUE);
    UpdateStatus(); UpdateToolbar();
}
static void BuildColumns() {
    while (ListView_DeleteColumn(g_list, 0)) {}
    int n; const Col* cs = ColsFor(g_page, n);
    for (int i = 0; i < n; ++i) { LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; c.pszText = (LPWSTR)cs[i].name; c.cx = S(cs[i].w); c.fmt = cs[i].right ? LVCFMT_RIGHT : LVCFMT_LEFT; SendMessageW(g_list, LVM_INSERTCOLUMNW, i, (LPARAM)&c); }
}

static void UpdateStatus() {
    wstring s; Page p = g_page;
    if (p == P_STARTUP) {
        size_t en = 0, di = 0; for (auto& e : g_su.entries) { if (e.state == L"Enabled") ++en; else if (e.state == L"Disabled") ++di; }
        s = std::to_wstring(g_su.entries.size()) + L" entries · " + std::to_wstring(en) + L" enabled · " + std::to_wstring(di) + L" disabled · " + std::to_wstring(g_view.size()) + L" shown";
    } else if (p == P_SERVICES) {
        size_t run = 0; for (auto& e : g_sv.entries) if (e.stateCode == SERVICE_RUNNING) ++run;
        s = std::to_wstring(g_sv.entries.size()) + L" services · " + std::to_wstring(run) + L" running"; if (g_view.size() != g_sv.entries.size()) s += L" · " + std::to_wstring(g_view.size()) + L" shown";
    } else if (p == P_HISTORY) s = std::to_wstring(g_cells[P_HISTORY].size()) + L" journal entries";
    else s = g_scanning ? L"Scanning…" : L"Ready";
    UINT sel = (p == P_STARTUP || p == P_SERVICES) ? ListView_GetSelectedCount(g_list) : 0; if (sel > 1) s += L" · " + std::to_wstring(sel) + L" selected";
    if (g_scanning && (p == P_STARTUP || p == P_SERVICES)) s += L" · scanning…";
    SetWindowTextW(g_status, s.c_str());
}

// ================= toolbar content per page =================
static void BuildToolbar() {
    std::vector<TbItem> it;
    auto add = [&](int id, const wchar_t* t, bool dd = false, bool right = false) { TbItem x; x.id = id; x.text = t; x.dropdown = dd; x.right = right; it.push_back(x); };
    auto sep = [&]() { TbItem x; x.id = 0; x.sep = true; it.push_back(x); };
    if (g_page == P_STARTUP) { add(C_REFRESH, L"Refresh"); sep(); add(C_ENABLE, L"Enable"); add(C_DISABLE, L"Disable"); add(C_DELETE, L"Delete"); add(C_STARTLATER, L"Start later", true); sep(); add(C_ADD, L"Add…"); add(C_RESTORE, L"Restore…"); add(C_UNDO, L"Undo"); }
    else if (g_page == P_SERVICES) { add(C_REFRESH, L"Refresh"); sep(); add(C_SVC_START, L"Start"); add(C_SVC_STOP, L"Stop"); add(C_SVC_RESTART, L"Restart"); add(C_SVC_PAUSE, L"Pause"); add(C_SVC_RESUME, L"Resume"); sep(); add(C_STARTTYPE, L"Start type", true); add(C_UNDO, L"Undo"); }
    else add(C_REFRESH, L"Refresh");
    if (g_page == P_STARTUP || g_page == P_SERVICES) add(C_FILTER, FilterName(g_page), true, true);
    add(C_MORE, L"More", false, true);
    TbSet(std::move(it));
}
static void UpdateToolbar() {
    auto sel = Selected(); bool any = !sel.empty();
    if (g_page == P_STARTUP) {
        bool canEn = false, canDis = false, canDel = false;
        for (int i : sel) { auto& e = g_su.entries[i]; if (e.toggle != Toggle::None) { if (e.state == L"Disabled") canEn = true; if (e.state == L"Enabled") canDis = true; }
            if ((e.type == L"Registry" || e.type == L"Registry (RunOnce)")) canDel = true; }
        TbEnable(C_ENABLE, canEn); TbEnable(C_DISABLE, canDis); TbEnable(C_DELETE, canDel); TbEnable(C_STARTLATER, false); TbEnable(C_ADD, true); TbEnable(C_RESTORE, true);
    } else if (g_page == P_SERVICES) {
        bool st = false, sp = false, rs = false, pa = false, re = false;
        for (int i : sel) { auto& e = g_sv.entries[i]; if (e.stateCode == SERVICE_STOPPED) st = true; if (e.stateCode == SERVICE_RUNNING) { rs = true; if (e.controls & SERVICE_ACCEPT_STOP) sp = true; if (e.controls & SERVICE_ACCEPT_PAUSE_CONTINUE) pa = true; } if (e.stateCode == SERVICE_PAUSED) re = true; }
        TbEnable(C_SVC_START, st); TbEnable(C_SVC_STOP, sp); TbEnable(C_SVC_RESTART, sp && rs); TbEnable(C_SVC_PAUSE, pa); TbEnable(C_SVC_RESUME, re); TbEnable(C_STARTTYPE, any);
    }
    TbEnable(C_UNDO, !g_undo.empty());
}

// ================= scan =================
static wstring JGet(const std::string& line, const char* key) {
    std::string k = std::string("\"") + key + "\":\""; size_t p = line.find(k); if (p == std::string::npos) return L""; p += k.size(); std::string o;
    for (; p < line.size() && line[p] != '"'; ++p) { if (line[p] == '\\' && p + 1 < line.size()) { ++p; o += line[p] == 'n' ? ' ' : line[p]; } else o += line[p]; }
    return FromUtf8(o);
}
static wstring ReadHistory(std::vector<std::vector<wstring>>& rows) {
    rows.clear(); std::ifstream f(std::filesystem::path(DataDir() + L"\\journal.jsonl"), std::ios::binary); std::string line; std::vector<std::string> lines;
    while (std::getline(f, line)) lines.push_back(line);
    for (auto it = lines.rbegin(); it != lines.rend() && rows.size() < 2000; ++it) { auto t = JGet(*it, "time"); if (t.size() >= 19) t = t.substr(0, 10) + L" " + t.substr(11, 8); rows.push_back({t, JGet(*it, "target"), JGet(*it, "before"), JGet(*it, "after"), JGet(*it, "result")}); }
    return L"";
}
static DWORD WINAPI ScanThread(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int gen = g_gen.load();
    auto* a = new ScanMsg{gen, ScanStartup(), {}, true, false, L""}; PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM)a);
    auto* b = new ScanMsg{gen, {}, ScanServices(), false, true, L""}; PostMessageW(g_main, WM_SCAN_DONE, 0, (LPARAM)b);
    CoUninitialize(); return 0;
}
static void StartScan() {
    if (g_scanning) return;
    for (int i : Selected()) g_keepSel.insert(KeyOf(g_page, i));
    g_scanning = true; ++g_gen; UpdateStatus();
    HANDLE h = CreateThread(nullptr, 0, ScanThread, nullptr, 0, nullptr); if (h) CloseHandle(h); else g_scanning = false;
}

// ================= pages =================
static void DashboardText() {
    size_t dis = 0, warn = 0, run = 0, crit = 0, sw = 0, third = 0;
    for (auto& e : g_su.entries) { if (e.state == L"Disabled") ++dis; if (e.hasWarning) ++warn; }
    for (auto& e : g_sv.entries) { if (e.stateCode == SERVICE_RUNNING) ++run; if (e.critical) ++crit; if (!e.warnings.empty()) ++sw; if (e.safety == L"Third-party") ++third; }
    wstring s = wstring(kApp) + L" " + kVer + L"\r\nDeveloped by Akshaya Simha\r\n\r\n";
    s += L"STARTUP\r\n  " + std::to_wstring(g_su.entries.size()) + L" entries, " + std::to_wstring(dis) + L" disabled, " + std::to_wstring(warn) + L" with warnings\r\n  " + g_su.coverage + L"\r\n\r\n";
    s += L"SERVICES\r\n  " + std::to_wstring(g_sv.entries.size()) + L" services, " + std::to_wstring(run) + L" running, " + std::to_wstring(third) + L" third-party, " + std::to_wstring(crit) + L" on the critical list, " + std::to_wstring(sw) + L" with warnings\r\n  " + g_sv.coverage + L"\r\n\r\n";
    s += L"Fully offline: this app makes no network connections and sends no telemetry. \"Check on VirusTotal\" and \"Search online\" only open your browser after you confirm.\r\n"
         L"Every change is backed up or journaled first, verified by reading it back, and can be undone where Windows allows (Ctrl+Z). Undo covers configuration only, not running state.\r\n\r\n"
         L"Keys: Ctrl+F search · Esc clear · F5 refresh · Enter details · Del delete · Ctrl+A select all · Ctrl+C copy name · Ctrl+Shift+C copy details · Ctrl+Z undo · Ctrl+1..5 pages";
    SetWindowTextW(g_info, s.c_str());
}
static void ShowPage(Page p) {
    g_page = p; SendMessageW(g_nav, LB_SETCURSEL, p, 0);
    bool listy = p == P_STARTUP || p == P_SERVICES || p == P_HISTORY;
    ShowWindow(g_list, listy ? SW_SHOW : SW_HIDE); ShowWindow(g_info, listy ? SW_HIDE : SW_SHOW); ShowWindow(g_search, (p == P_STARTUP || p == P_SERVICES) ? SW_SHOW : SW_HIDE);
    wstring title = wstring(kApp) + L"  —  " + kPageNames[p] + (g_elevated ? L" (Administrator)" : L""); SetWindowTextW(g_main, title.c_str());
    BuildToolbar();
    if (listy) { SetWindowTextW(g_search, g_query[p].c_str());
        SendMessageW(g_search, EM_SETCUEBANNER, TRUE, (LPARAM)(p == P_SERVICES ? L"Search services (Ctrl+F)" : L"Search (Ctrl+F)"));
        BuildColumns(); if (p == P_HISTORY) { std::vector<std::vector<wstring>> r; ReadHistory(r); g_cells[P_HISTORY] = r; } ApplyView(); }
    else { if (p == P_DASH) DashboardText(); else SetWindowTextW(g_info, (wstring(L"Settings\r\n\r\nThemes (light/midnight/warm), density and saved column layouts are planned. The app is currently dark only.\r\n\r\nData folder: ") + DataDir() +
        L"\r\n  journal.jsonl – every change made by this app\r\n  backups\\ – deleted startup entries (restorable)\r\n  firstseen.tsv – when each startup entry was first seen\r\n\r\nPrivacy: nothing leaves this PC. No telemetry, no automatic updates, no online advice.").c_str()); UpdateStatus(); UpdateToolbar(); }
    RECT r; GetClientRect(g_main, &r); SendMessageW(g_main, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
}

// ================= actions =================
static const StartupEntry* OneStartup() { auto s = Selected(); return s.empty() ? nullptr : &g_su.entries[s[0]]; }
static const ServiceEntry* OneService() { auto s = Selected(); return s.empty() ? nullptr : &g_sv.entries[s[0]]; }

static void ShowDetails() {
    if (g_page == P_STARTUP) {
        auto* e = OneStartup(); if (!e) return; StartupEntry c = *e; HWND owner = g_main;
        ShowReport(owner, c.name + L" — Details", {{L"Details", StartupDetailsText(c)}}, {
            {L"Open file location", [c](const wstring&) { OpenFileLocation(c.program); }}, {L"Show where it is set", [c](const wstring&) { ShowInRegistryEditor(c.type == L"Scheduled task" ? c.location : c.location); }},
            {L"Check on VirusTotal", [c, owner](const wstring&) { OnlineLookup(owner, true, c.program, c.name); }}, {L"Search online", [c, owner](const wstring&) { OnlineLookup(owner, false, c.program, c.name); }},
            {L"Copy", [](const wstring& t) { SetClip(t); }}, {L"Close", nullptr}});
    } else if (g_page == P_SERVICES) {
        auto* e = OneService(); if (!e) return; ServiceEntry c = *e; HWND owner = g_main;
        ShowReport(owner, c.display + L" — Properties", {{L"General", ServiceTabText(c, 0)}, {L"Configuration", ServiceTabText(c, 1)}, {L"Recovery", ServiceTabText(c, 2)}, {L"Dependencies", ServiceTabText(c, 3)}}, {
            {L"Open file location", [c](const wstring&) { OpenFileLocation(c.program); }}, {L"Check on VirusTotal", [c, owner](const wstring&) { OnlineLookup(owner, true, c.program, c.display); }},
            {L"Search online", [c, owner](const wstring&) { OnlineLookup(owner, false, c.program, c.display); }}, {L"Copy", [](const wstring& t) { SetClip(t); }}, {L"Close", nullptr}});
    }
}
static void CopyNames() { wstring o; for (int i : Selected()) o += (g_page == P_STARTUP ? g_su.entries[i].name : g_sv.entries[i].display) + L"\r\n"; SetClip(o); }
static void CopyDetails() {
    wstring o; for (int i : Selected()) { if (g_page == P_STARTUP) o += StartupCopyText(g_su.entries[i]) + L"\r\n"; else { auto& e = g_sv.entries[i]; o += ServiceTabText(e, 0) + L"\r\n"; } } SetClip(o);
}
static void SelectAll() { ListView_SetItemState(g_list, -1, LVIS_SELECTED, LVIS_SELECTED); UpdateStatus(); UpdateToolbar(); }

static void ToggleStartup(bool enable) {
    auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) { auto& e = g_su.entries[i]; if (e.toggle != Toggle::None && e.state == (enable ? L"Disabled" : L"Enabled")) { todo.push_back(i); names.push_back(e.name + L"  (" + e.type + L")"); } }
    if (todo.empty()) return;
    if (todo.size() > 1 && !Confirm((enable ? L"Enable these " : L"Disable these ") + std::to_wstring(todo.size()) + L" startup entries?", names)) return;
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
    wstring n, c; if (!AskTwo(g_main, L"Add a startup entry (current user)", L"Name:", L"Command (full path, with quotes if it contains spaces):", n, c)) return;
    wstring err; if (AddStartupEntry(n, c, err)) { SetMsg(L"Added \"" + n + L"\" to the current user's Run key."); StartScan(); } else MessageBoxW(g_main, err.c_str(), kApp, MB_OK | MB_ICONWARNING);
}
static void RestoreMenu(POINT pt) {
    auto b = ListBackups(); HMENU m = CreatePopupMenu();
    if (b.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 1, L"No deleted entries to restore"); else for (size_t i = 0; i < b.size(); ++i) AppendMenuW(m, MF_STRING, C_RESTORE_BASE + i, b[i].label.c_str());
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m);
    if (id >= C_RESTORE_BASE && id < C_RESTORE_BASE + (int)b.size()) {
        wstring err, name; if (MessageBoxW(g_main, (L"Restore this entry?\r\n\r\n" + b[id - C_RESTORE_BASE].label).c_str(), kApp, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
        if (RestoreBackup(b[id - C_RESTORE_BASE].path, err, name)) { SetMsg(L"Restored \"" + name + L"\"."); StartScan(); } else MessageBoxW(g_main, err.c_str(), kApp, MB_OK | MB_ICONWARNING);
    }
}

// ---- services ----
struct SvcJob { std::vector<ServiceEntry> items; SvcOp op; };
static DWORD WINAPI SvcJobThread(LPVOID p) {
    std::unique_ptr<SvcJob> j((SvcJob*)p); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // dependency order among the selected services (prerequisites first for Start, dependents first for Stop)
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
    auto sel = Selected(); auto* job = new SvcJob{{}, op}; std::vector<wstring> names; std::vector<wstring> extra;
    for (int i : sel) { auto& e = g_sv.entries[i];
        bool elig = op == SvcOp::Start ? e.stateCode == SERVICE_STOPPED : op == SvcOp::Resume ? e.stateCode == SERVICE_PAUSED : e.stateCode == SERVICE_RUNNING &&
            ((op == SvcOp::Pause) ? (e.controls & SERVICE_ACCEPT_PAUSE_CONTINUE) != 0 : (e.controls & SERVICE_ACCEPT_STOP) != 0);
        if (!elig) continue; job->items.push_back(e); names.push_back(e.display + L" (" + e.name + L")");
        if (op == SvcOp::Stop || op == SvcOp::Restart) for (auto& d : ActiveDependents(e.name)) extra.push_back(L"also stops dependent: " + d); }
    if (job->items.empty()) { delete job; return; }
    static const wchar_t* verbs[] = {L"Start", L"Stop", L"Restart", L"Pause", L"Resume"};
    bool risky = op == SvcOp::Stop || op == SvcOp::Restart;
    if (job->items.size() > 1 || (risky && !extra.empty())) { auto all = names; all.insert(all.end(), extra.begin(), extra.end());
        if (!Confirm(wstring(verbs[(int)op]) + L" these " + std::to_wstring(job->items.size()) + L" services?", all)) { delete job; return; } }
    SetMsg(wstring(verbs[(int)op]) + L"… working");
    HANDLE h = CreateThread(nullptr, 0, SvcJobThread, job, 0, nullptr); if (h) CloseHandle(h); else delete job;
}
static void SetStartType(DWORD type, bool delayed, const wchar_t* label) {
    auto sel = Selected(); std::vector<int> todo; std::vector<wstring> names;
    for (int i : sel) { auto& e = g_sv.entries[i]; todo.push_back(i); names.push_back(e.display + L" (" + e.name + L")"); }
    if (todo.empty()) return;
    if (todo.size() > 1 || type == SERVICE_DISABLED) if (!Confirm(L"Set the start type to " + wstring(label) + L" for " + (todo.size() == 1 ? wstring(L"this service") : L"these " + std::to_wstring(todo.size()) + L" services") + L"?", names)) return;
    int ok = 0, bad = 0, grp = ++g_groupSeq; wstring firstErr, firstName;
    for (int i : todo) { auto e = g_sv.entries[i]; wstring err, before;
        if (SetServiceStartType(e.name, type, delayed, err, before)) { ++ok; g_undo.push_back({2, {}, false, e.name, e.startType, e.delayed, grp}); } else { ++bad; if (firstErr.empty()) { firstErr = err; firstName = e.display; } } }
    wstring msg = ok ? (ok == 1 ? g_sv.entries[todo[0]].display : std::to_wstring(ok) + L" services") + L" set to " + label + L"." : L"";
    if (bad) msg += (msg.empty() ? L"" : L" ") + std::to_wstring(bad) + L" failed: " + firstName + L" — " + firstErr;
    SetMsg(msg); StartScan();
}
static void Undo() {
    if (g_undo.empty()) return; int grp = g_undo.back().group; int ok = 0, bad = 0; wstring firstErr;
    while (!g_undo.empty() && g_undo.back().group == grp) { auto u = g_undo.back(); g_undo.pop_back(); wstring err, before; bool r = u.kind == 1 ? SetStartupEnabled(u.se, u.wasEnabled, err) : SetServiceStartType(u.name, u.type, u.delayed, err, before);
        if (r) ++ok; else { ++bad; if (firstErr.empty()) firstErr = err; } }
    SetMsg(L"Undo: " + std::to_wstring(ok) + L" reverted" + (bad ? L", " + std::to_wstring(bad) + L" failed — " + firstErr : L".") + L" (configuration only; running state is not restored)"); StartScan();
}
static void RestartAsAdmin() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH); wstring arg = g_page == P_SERVICES ? L"--services" : g_page == P_STARTUP ? L"--startup" : L"";
    if (g_mtx) { CloseHandle(g_mtx); g_mtx = nullptr; }                       // let the elevated instance start
    if ((INT_PTR)ShellExecuteW(nullptr, L"runas", exe, arg.c_str(), nullptr, SW_SHOWNORMAL) > 32) DestroyWindow(g_main);
    else g_mtx = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance");   // UAC cancelled: nothing changed
}
static void About() {
    wstring t = wstring(kApp) + L" " + kVer + L"\r\nDeveloped by Akshaya Simha\r\n\r\nBuilt " + L"" __DATE__ L" " __TIME__ L"\r\nWindows 10/11 x64 · native Win32 · offline · no telemetry\r\nUnsigned preview build.";
    MessageBoxW(g_main, t.c_str(), L"About", MB_OK | MB_ICONINFORMATION);
}

// ================= menus =================
static void AddItem(HMENU m, UINT id, const wchar_t* text, bool enabled = true, bool def = false) {
    AppendMenuW(m, MF_STRING | (enabled ? 0 : MF_GRAYED), id, text); if (def) SetMenuDefaultItem(m, id, FALSE);
}
static bool AnyStartupToggle(bool enable) { for (int i : Selected()) { auto& e = g_su.entries[i]; if (e.toggle != Toggle::None && e.state == (enable ? L"Disabled" : L"Enabled")) return true; } return false; }
static bool TbOn(int id) { auto* i = TbFind(id); return i && i->enabled; }
static HMENU StartTypeMenu() { HMENU m = CreatePopupMenu(); AddItem(m, C_ST_AUTO, L"Automatic"); AddItem(m, C_ST_AUTODELAY, L"Automatic (delayed)"); AddItem(m, C_ST_MANUAL, L"Manual"); AddItem(m, C_ST_DISABLED, L"Disabled"); return m; }

static void ContextMenu(POINT pt) {
    auto sel = Selected(); if (sel.empty()) return; HMENU m = CreatePopupMenu();
    if (g_page == P_STARTUP) {
        AddItem(m, C_DETAILS, L"Details\tEnter", true, true); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AddItem(m, C_ENABLE, L"Enable", TbOn(C_ENABLE)); AddItem(m, C_DISABLE, L"Disable", TbOn(C_DISABLE));
        HMENU sl = CreatePopupMenu(); AddItem(sl, 0, L"Not available yet", false); AppendMenuW(m, MF_POPUP | MF_GRAYED, (UINT_PTR)sl, L"Start later");
        AddItem(m, C_DELETE, L"Delete…\tDel", TbOn(C_DELETE)); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        bool one = sel.size() == 1; AddItem(m, C_OPENLOC, L"Open file location", one); AddItem(m, C_REGEDIT, L"Show where it is set", one);
        AddItem(m, C_VT, L"Check on VirusTotal…", one); AddItem(m, C_SEARCHONLINE, L"Search online…", one); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C");
    } else {
        AddItem(m, C_DETAILS, L"Properties\tEnter", true, true); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AddItem(m, C_SVC_START, L"Start\tCtrl+Shift+S", TbOn(C_SVC_START)); AddItem(m, C_SVC_STOP, L"Stop\tCtrl+Shift+T", TbOn(C_SVC_STOP)); AddItem(m, C_SVC_RESTART, L"Restart\tCtrl+Shift+R", TbOn(C_SVC_RESTART));
        AddItem(m, C_SVC_PAUSE, L"Pause", TbOn(C_SVC_PAUSE)); AddItem(m, C_SVC_RESUME, L"Resume", TbOn(C_SVC_RESUME)); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_POPUP, (UINT_PTR)StartTypeMenu(), L"Start type"); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        bool one = sel.size() == 1; AddItem(m, C_COPYNAME, L"Copy name\tCtrl+C"); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C"); AddItem(m, C_OPENLOC, L"Open file location", one);
        AddItem(m, C_VT, L"Check on VirusTotal…", one); AddItem(m, C_SEARCHONLINE, L"Search online…", one);
    }
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m);
    if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
}
static void DropMenu(int cmd) {
    auto* it = TbFind(cmd); if (!it) return; POINT pt{it->r.left, it->r.bottom}; ClientToScreen(g_tb, &pt);
    if (cmd == C_FILTER) {
        HMENU m = CreatePopupMenu(); int n = g_page == P_STARTUP ? 10 : 8; const wchar_t** names = g_page == P_STARTUP ? kStartupFilters : kServiceFilters;
        for (int i = 0; i < n; ++i) { if ((g_page == P_STARTUP && (i == 3 || i == 6)) || (g_page == P_SERVICES && (i == 3 || i == 5 || i == 7))) AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(m, MF_STRING | (i == g_filter[g_page] ? MF_CHECKED : 0), C_FILTER_BASE + i, names[i]); }
        TPMPARAMS tp{sizeof tp, it->r}; MapWindowPoints(g_tb, nullptr, (LPPOINT)&tp.rcExclude, 2);
        int id = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, it->r.right > 0 ? pt.x + (it->r.right - it->r.left) : pt.x, pt.y, g_main, &tp); DestroyMenu(m);
        if (id >= C_FILTER_BASE && id < C_FILTER_BASE + n) { g_filter[g_page] = id - C_FILTER_BASE; TbText(C_FILTER, FilterName(g_page)); ApplyView(); }
    } else if (cmd == C_MORE) {
        HMENU m = CreatePopupMenu(); auto sel = Selected();
        AddItem(m, C_DETAILS, g_page == P_SERVICES ? L"Properties\tEnter" : L"Details\tEnter", sel.size() >= 1); AddItem(m, C_UNDO, L"Undo\tCtrl+Z", !g_undo.empty());
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        if (g_page == P_STARTUP) { AddItem(m, C_ADD, L"Add an entry…\tCtrl+N"); AddItem(m, C_RESTORE, L"Restore deleted entries…"); AddItem(m, C_OPENBACKUPS, L"Open the backup folder"); AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
        AddItem(m, C_REFRESH, L"Refresh\tF5"); AddItem(m, C_SELALL, L"Select all\tCtrl+A", g_page == P_STARTUP || g_page == P_SERVICES); AddItem(m, C_COPYDETAILS, L"Copy details\tCtrl+Shift+C", !sel.empty());
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AddItem(m, C_OPENDATA, L"Open the data folder");
        if (!g_elevated) AddItem(m, C_ADMIN, L"Restart as administrator"); AddItem(m, C_SETTINGS, L"Settings…\tCtrl+,"); AddItem(m, C_ABOUT, L"About Feather System Utilities\tF1");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AddItem(m, C_EXIT, L"Exit");
        int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, pt.x + (it->r.right - it->r.left), pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    } else if (cmd == C_STARTTYPE) {
        HMENU m = StartTypeMenu(); int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, g_main, nullptr); DestroyMenu(m); if (id) PostMessageW(g_main, WM_COMMAND, id, 0);
    } else if (cmd == C_RESTORE) RestoreMenu(pt);
}

static void DoCommand(int id) {
    switch (id) {
    case C_REFRESH: g_stMsg.clear(); StartScan(); if (g_page == P_HISTORY) ShowPage(P_HISTORY); break;
    case C_ENABLE: ToggleStartup(true); break; case C_DISABLE: ToggleStartup(false); break; case C_DELETE: DeleteStartup(); break; case C_ADD: AddEntry(); break;
    case C_RESTORE: DropMenu(C_RESTORE); break; case C_UNDO: Undo(); break;
    case C_FILTER: case C_MORE: case C_STARTTYPE: DropMenu(id); break;
    case C_SVC_START: RunSvcOp(SvcOp::Start); break; case C_SVC_STOP: RunSvcOp(SvcOp::Stop); break; case C_SVC_RESTART: RunSvcOp(SvcOp::Restart); break;
    case C_SVC_PAUSE: RunSvcOp(SvcOp::Pause); break; case C_SVC_RESUME: RunSvcOp(SvcOp::Resume); break;
    case C_ST_AUTO: SetStartType(SERVICE_AUTO_START, false, L"Automatic"); break; case C_ST_AUTODELAY: SetStartType(SERVICE_AUTO_START, true, L"Automatic (delayed)"); break;
    case C_ST_MANUAL: SetStartType(SERVICE_DEMAND_START, false, L"Manual"); break; case C_ST_DISABLED: SetStartType(SERVICE_DISABLED, false, L"Disabled"); break;
    case C_DETAILS: ShowDetails(); break; case C_COPYNAME: CopyNames(); break; case C_COPYDETAILS: CopyDetails(); break; case C_SELALL: SelectAll(); break;
    case C_OPENLOC: if (g_page == P_STARTUP) { if (auto* e = OneStartup()) OpenFileLocation(e->program); } else if (auto* s = OneService()) OpenFileLocation(s->program); break;
    case C_REGEDIT: if (auto* e = OneStartup()) ShowInRegistryEditor(e->location); break;
    case C_VT: case C_SEARCHONLINE: { wstring prog, name; if (g_page == P_STARTUP) { if (auto* e = OneStartup()) { prog = e->program; name = e->name; } } else if (auto* s = OneService()) { prog = s->program; name = s->display; }
        if (!prog.empty() || !name.empty()) OnlineLookup(g_main, id == C_VT, prog, name); break; }
    case C_ADMIN: RestartAsAdmin(); break; case C_SETTINGS: ShowPage(P_SETTINGS); break; case C_ABOUT: About(); break;
    case C_OPENBACKUPS: ShellExecuteW(nullptr, L"open", (DataDir() + L"\\backups").c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case C_OPENDATA: ShellExecuteW(nullptr, L"open", DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case C_EXIT: DestroyWindow(g_main); break;
    }
}

// ================= list drawing =================
static COLORREF CellColor(Page p, int row, int col, bool& dimmed) {
    dimmed = false;
    if (p == P_STARTUP) { auto& e = g_su.entries[row]; if (e.state == L"Disabled") dimmed = true;
        switch (col) { case 1: return e.state == L"Enabled" ? cGreen : e.state == L"Disabled" ? cDim : e.state == L"One-time" ? cAmber : cAmber; case 2: return cDim; case 3: return cRed; case 4: return e.microsoft ? cText : cLav; }
        return dimmed ? cDim : cText; }
    if (p == P_SERVICES) { auto& e = g_sv.entries[row];
        switch (col) { case 2: return e.stateCode == SERVICE_RUNNING ? cGreen : e.stateCode == SERVICE_STOPPED ? cDim : cAmber; case 4: return e.safety == L"Critical" ? cRed : e.safety == L"Third-party" ? cLav : cDim; case 8: return cRed; }
        return (e.stateCode == SERVICE_STOPPED && col != 0) ? cDim : cText; }
    return cText;
}
static LRESULT ListCustomDraw(LPNMLVCUSTOMDRAW cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    case CDDS_ITEMPREPAINT: cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS); return CDRF_NOTIFYSUBITEMDRAW;   // we paint selection ourselves (consistent dark colours)
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        int vi = (int)cd->nmcd.dwItemSpec, col = cd->iSubItem; if (vi >= (int)g_view.size()) return CDRF_DODEFAULT;
        cd->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
        bool dim; cd->clrText = CellColor(g_page, g_view[vi], col, dim);
        bool sel = ListView_GetItemState(g_list, vi, LVIS_SELECTED) != 0; cd->clrTextBk = sel ? cSel : cBg; return CDRF_NEWFONT; }
    case CDDS_POSTPAINT: {   // vertical column lines
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
    if (i == g_sortCol[g_page]) { SetTextColor(dc, cDim); RECT ar = r; ar.right -= S(6); DrawTextW(dc, g_sortAsc[g_page] ? L"\u25B2" : L"\u25BC", -1, &ar, DT_RIGHT | DT_VCENTER | DT_SINGLELINE); }
}
static LRESULT CALLBACK HeaderProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {   // fully owner-painted so it is dark regardless of visual-style support
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT cr; GetClientRect(h, &cr); FillRect(dc, &cr, g_brBar);
        for (int i = 0; i < Header_GetItemCount(h); ++i) PaintHeaderItem(dc, i);
        EndPaint(h, &ps); return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

// ================= window =================
static void MakeFonts() {
    if (g_font) DeleteObject(g_font); if (g_fontSmall) DeleteObject(g_fontSmall);
    g_font = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSmall = CreateFontW(-MulDiv(8, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}
static RECT g_searchBox;
static void Layout(int w, int h) {
    int railW = S(120), tbH = S(40), stH = S(26), searchW = S(220), searchH = S(26);
    MoveWindow(g_nav, 0, 0, railW, h - stH, TRUE);
    MoveWindow(g_tb, railW, 0, w - railW, tbH, TRUE); TbLayout();
    int edge = (int)SendMessageW(g_tb, TBM_LEFTEDGE, 0, 0); int sx = railW + edge - searchW - S(6); if (sx < railW + S(300)) sx = railW + S(300);
    g_searchBox = {sx, (tbH - searchH) / 2, sx + searchW, (tbH - searchH) / 2 + searchH};
    MoveWindow(g_search, sx + S(8), g_searchBox.top + S(5), searchW - S(16), searchH - S(10), TRUE);
    MoveWindow(g_list, railW, tbH, w - railW, h - tbH - stH, TRUE); MoveWindow(g_info, railW + S(10), tbH + S(6), w - railW - S(20), h - tbH - stH - S(12), TRUE);
    MoveWindow(g_status, 0, h - stH, w, stH, TRUE); InvalidateRect(g_main, nullptr, TRUE); BringWindowToTop(g_search);
}
static LRESULT CALLBACK SearchProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { SetWindowTextW(h, L""); return 0; }
    if (m == WM_KEYDOWN && (w == VK_DOWN || w == VK_RETURN)) { SetFocus(g_list); return 0; }
    if (m == WM_CHAR && (w == VK_ESCAPE || w == VK_RETURN)) return 0;
    return DefSubclassProc(h, m, w, l);
}
static void DarkMode() {
    HMODULE ux = LoadLibraryW(L"uxtheme.dll"); if (!ux) return;
    auto setMode = (int(WINAPI*)(int))GetProcAddress(ux, MAKEINTRESOURCEA(135)); auto flush = (void(WINAPI*)())GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (setMode) setMode(2 /*ForceDark: native dark popup menus*/); if (flush) flush();
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_main = h; g_dpi = GetDpiForWindow(h); MakeFonts(); SetDarkTitleBar(h, true);
        g_nav = CreateWindowExW(0, L"LISTBOX", L"Navigation", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_NAV, g_inst, nullptr);
        for (auto n : kPageNames) SendMessageW(g_nav, LB_ADDSTRING, 0, (LPARAM)n);
        g_tb = CreateWindowExW(0, L"FsuToolbar", L"Toolbar", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_TB, g_inst, nullptr);
        g_search = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_SEARCH, g_inst, nullptr); SetWindowSubclass(g_search, SearchProc, 1, 0);
        g_list = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_LIST, g_inst, nullptr);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP);
        ListView_SetBkColor(g_list, cBg); ListView_SetTextBkColor(g_list, cBg); ListView_SetTextColor(g_list, cText); SetWindowTheme(g_list, L"DarkMode_Explorer", nullptr);
        g_hdr = ListView_GetHeader(g_list); SetWindowTheme(g_hdr, L"DarkMode_ItemsView", nullptr); SetWindowSubclass(g_hdr, HeaderProc, 2, 0);
        g_info = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, 0, 0, 10, 10, h, (HMENU)ID_INFO, g_inst, nullptr);
        g_status = CreateWindowExW(0, L"FsuStatus", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, h, (HMENU)ID_STATUS, g_inst, nullptr);
        for (HWND c : {g_nav, g_tb, g_search, g_list, g_info, g_status}) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_search, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(2, 2));
        wchar_t un[256]; DWORD n = 256; GetUserNameW(un, &n); g_user = un; g_elevated = IsElevated();
        ShowPage(P_DASH); StartScan(); return 0; }
    case WM_SIZE: Layout(LOWORD(l), HIWORD(l)); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO*)l)->ptMinTrackSize = {S(800), S(480)}; return 0;
    case WM_DPICHANGED: { g_dpi = HIWORD(w); MakeFonts(); for (HWND c : {g_nav, g_tb, g_search, g_list, g_info, g_status}) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
        auto* r = (RECT*)l; SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_brBg); return 1; }
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        if (g_page == P_STARTUP || g_page == P_SERVICES) { HPEN p = CreatePen(PS_SOLID, 1, RGB(70, 70, 70)); HPEN op = (HPEN)SelectObject(dc, p); HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            RoundRect(dc, g_searchBox.left, g_searchBox.top, g_searchBox.right, g_searchBox.bottom, S(6), S(6)); SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(p); }
        EndPaint(h, &ps); return 0; }
    case WM_SETFOCUS: SetFocus(g_nav); return 0;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: case WM_CTLCOLORLISTBOX: { HDC dc = (HDC)w; SetTextColor(dc, cText); SetBkColor(dc, (HWND)l == g_search ? cBg : cBg); return (LRESULT)g_brBg; }
    case WM_MEASUREITEM: { auto* mi = (MEASUREITEMSTRUCT*)l; if (mi->CtlID == ID_NAV) { mi->itemHeight = S(34); return TRUE; } break; }
    case WM_DRAWITEM: { auto* di = (DRAWITEMSTRUCT*)l; if (di->CtlID != ID_NAV || di->itemID == (UINT)-1) break;
        bool sel = (int)di->itemID == (int)g_page; HBRUSH b = CreateSolidBrush(sel ? cRailSel : cRail); FillRect(di->hDC, &di->rcItem, b); DeleteObject(b);
        if (sel) { RECT bar = di->rcItem; bar.right = bar.left + S(3); HBRUSH a = CreateSolidBrush(cAccent); FillRect(di->hDC, &bar, a); DeleteObject(a); }
        SetBkMode(di->hDC, TRANSPARENT); SetTextColor(di->hDC, sel ? RGB(255, 255, 255) : cText); SelectObject(di->hDC, g_font); RECT tr = di->rcItem; tr.left += S(16);
        DrawTextW(di->hDC, kPageNames[di->itemID], -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE); if (di->itemState & ODS_FOCUS) { RECT f = di->rcItem; InflateRect(&f, -2, -2); DrawFocusRect(di->hDC, &f); } return TRUE; }
    case WM_SCAN_DONE: {
        std::unique_ptr<ScanMsg> r((ScanMsg*)l); if (r->gen != g_gen.load()) return 0;           // stale generation: discard
        if (r->hasSu) { g_su = std::move(r->su); } if (r->hasSv) { g_sv = std::move(r->sv); g_scanning = false; }
        BuildCells();
        if (g_page == P_STARTUP || g_page == P_SERVICES) ApplyView(); else { if (g_page == P_DASH) DashboardText(); UpdateStatus(); }
        return 0; }
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
    case WM_CONTEXTMENU: if ((HWND)w == g_list && (g_page == P_STARTUP || g_page == P_SERVICES)) {
            POINT pt{(short)LOWORD(l), (short)HIWORD(l)};
            if (l == -1) { int f = ListView_GetNextItem(g_list, -1, LVNI_FOCUSED | LVNI_SELECTED); RECT r{}; if (f >= 0) ListView_GetItemRect(g_list, f, &r, LVIR_BOUNDS); pt = {r.left + S(20), r.bottom}; ClientToScreen(g_list, &pt); }
            ContextMenu(pt); return 0; } break;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == ID_NAV && code == LBN_SELCHANGE) { int s = (int)SendMessageW(g_nav, LB_GETCURSEL, 0, 0); if (s >= 0 && s != g_page) ShowPage((Page)s); return 0; }
        if (id == ID_SEARCH && code == EN_CHANGE) { wchar_t b[512]; GetWindowTextW(g_search, b, 512); g_query[g_page] = b; ApplyView(); return 0; }
        if (id >= C_REFRESH && id < C_FILTER_BASE) DoCommand(id); return 0; }
    case WM_DESTROY: ++g_gen; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
    g_inst = inst;
    g_mtx = CreateMutexW(nullptr, TRUE, L"Local\\FeatherSystemUtilities.Instance");
    wstring c = cmd ? cmd : L""; Page req = P_DASH; if (c.find(L"--services") != wstring::npos) req = P_SERVICES; else if (c.find(L"--startup") != wstring::npos) req = P_STARTUP;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { if (HWND e = FindWindowW(L"FeatherSystemUtilitiesWnd", nullptr)) PostMessageW(e, WM_ACTIVATE_PAGE, req, 0); return 0; }   // F004
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX ic{sizeof ic, ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES}; InitCommonControlsEx(&ic);
    DarkMode(); g_brBg = CreateSolidBrush(cBg); g_brBar = CreateSolidBrush(cBar); g_brRail = CreateSolidBrush(cRail);
    WNDCLASSW wc{}; wc.hInstance = inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpfnWndProc = TbProc; wc.lpszClassName = L"FsuToolbar"; RegisterClassW(&wc);
    wc.lpfnWndProc = StProc; wc.lpszClassName = L"FsuStatus"; RegisterClassW(&wc);
    wc.lpfnWndProc = WndProc; wc.lpszClassName = L"FeatherSystemUtilitiesWnd"; wc.hbrBackground = g_brBg; RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, kApp, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1360, 780, nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show); UpdateWindow(w); if (req != P_DASH) PostMessageW(w, WM_ACTIVATE_PAGE, req, 0);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_KEYDOWN && GetForegroundWindow() == w) {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0; HWND f = GetFocus(); bool onList = f == g_list; bool listy = g_page == P_STARTUP || g_page == P_SERVICES;
            if (f == g_tb && (m.wParam == VK_LEFT || m.wParam == VK_RIGHT || m.wParam == VK_RETURN || m.wParam == VK_SPACE)) { SendMessageW(g_tb, WM_KEYDOWN, m.wParam, m.lParam); continue; }
            if (ctrl && m.wParam == 'F' && listy) { SetFocus(g_search); SendMessageW(g_search, EM_SETSEL, 0, -1); continue; }
            if (ctrl && m.wParam >= '1' && m.wParam <= '5') { ShowPage((Page)(m.wParam - '1')); continue; }
            if (m.wParam == VK_F5) { DoCommand(C_REFRESH); continue; } if (m.wParam == VK_F1) { About(); continue; } if (ctrl && m.wParam == VK_OEM_COMMA) { ShowPage(P_SETTINGS); continue; }
            if (ctrl && !shift && m.wParam == 'Z' && f != g_search) { Undo(); continue; }
            if (f != g_search && listy) {
                if (ctrl && m.wParam == 'A') { SelectAll(); continue; }
                if (ctrl && shift && m.wParam == 'C') { CopyDetails(); continue; } if (ctrl && !shift && m.wParam == 'C' && onList) { CopyNames(); continue; }
                if (ctrl && m.wParam == 'N' && g_page == P_STARTUP) { AddEntry(); continue; }
                if (m.wParam == VK_DELETE && g_page == P_STARTUP && onList) { DeleteStartup(); continue; }
                if (m.wParam == VK_RETURN && onList) { ShowDetails(); continue; }
                if (g_page == P_SERVICES && ctrl && shift) { if (m.wParam == 'S') { RunSvcOp(SvcOp::Start); continue; } if (m.wParam == 'T') { RunSvcOp(SvcOp::Stop); continue; } if (m.wParam == 'R') { RunSvcOp(SvcOp::Restart); continue; } }
            }
        }
        if (!IsDialogMessageW(w, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    if (g_mtx) CloseHandle(g_mtx); return 0;
}
