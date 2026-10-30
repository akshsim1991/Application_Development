#include "themeui.hpp"
#include "theme.hpp"
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <memory>

namespace themeui {
namespace {

HFONT g_font = nullptr, g_bold = nullptr;
int g_dpi = 96;
int px(int v) { return MulDiv(v, g_dpi, 96); }

// brushes of the current palette, made again when it changes
struct Brushes {
    HBRUSH face = nullptr, field = nullptr, menu = nullptr;
    COLORREF faceC = CLR_INVALID, fieldC = CLR_INVALID;
} g_br;
void makeBrushes() {
    if (g_br.faceC == pal().toolbar && g_br.fieldC == pal().field && g_br.face) return;
    for (HBRUSH b : {g_br.face, g_br.field, g_br.menu}) if (b) DeleteObject(b);
    g_br.face = CreateSolidBrush(pal().toolbar);
    g_br.field = CreateSolidBrush(pal().field);
    g_br.menu = CreateSolidBrush(pal().toolbar);
    g_br.faceC = pal().toolbar; g_br.fieldC = pal().field;
}

void fillRect(HDC dc, const RECT& r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}
void roundRect(HDC dc, const RECT& r, int radius, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border == CLR_INVALID ? fill : border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}

// Windows 10 1903+ / 11: menus' frames, scroll bars of system controls and common dialogs in light or dark
// (uxtheme exports these by number only; missing on older systems and in Wine, then nothing happens)
void appDarkMode(bool dark) {
    static HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux) return;
    using SetMode = int(WINAPI*)(int);
    using Flush = void(WINAPI*)();
    auto setMode = reinterpret_cast<SetMode>(reinterpret_cast<void*>(GetProcAddress(ux, MAKEINTRESOURCEA(135))));
    auto flush = reinterpret_cast<Flush>(reinterpret_cast<void*>(GetProcAddress(ux, MAKEINTRESOURCEA(136))));
    if (setMode) setMode(dark ? 2 /* force dark */ : 3 /* force light */);
    if (flush) flush();
}

// a window's title bar in the theme: dark mode on Windows 10, and on Windows 11 the caption's own colours
void titleColours(HWND w) {
    BOOL dark = pal().dark;
    DwmSetWindowAttribute(w, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    COLORREF cap = pal().tabBar, text = pal().text, border = pal().line;
    DwmSetWindowAttribute(w, 35 /* DWMWA_CAPTION_COLOR */, &cap, sizeof(cap));
    DwmSetWindowAttribute(w, 36 /* DWMWA_TEXT_COLOR */, &text, sizeof(text));
    DwmSetWindowAttribute(w, 34 /* DWMWA_BORDER_COLOR */, &border, sizeof(border));
}

void themeControl(HWND c) {
    wchar_t cls[32] = L"";
    GetClassNameW(c, cls, 32);
    const bool dark = pal().dark;
    if (_wcsicmp(cls, L"Button") == 0) {
        const LONG_PTR style = GetWindowLongPtrW(c, GWL_STYLE);
        const LONG_PTR type = style & BS_TYPEMASK;
        if (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) {
            if (type == BS_DEFPUSHBUTTON) SetPropW(c, L"FeatherDefault", reinterpret_cast<HANDLE>(1));
            SetPropW(c, L"FeatherThemed", reinterpret_cast<HANDLE>(1));
            SetWindowLongPtrW(c, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW);
        } else if (type != BS_OWNERDRAW) {
            SetWindowTheme(c, L"", L"");   // the classic look takes the label colour from WM_CTLCOLORSTATIC
        }
    } else if (_wcsicmp(cls, L"ComboBox") == 0) {
        SetWindowTheme(c, dark ? L"DarkMode_CFD" : L"Explorer", nullptr);
    } else if (_wcsicmp(cls, L"Edit") == 0 || _wcsicmp(cls, L"ListBox") == 0 || _wcsicmp(cls, WC_TREEVIEWW) == 0 || _wcsicmp(cls, WC_LISTVIEWW) == 0) {
        SetWindowTheme(c, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        if (_wcsicmp(cls, WC_TREEVIEWW) == 0) {
            TreeView_SetBkColor(c, pal().field);
            TreeView_SetTextColor(c, pal().text);
        } else if (_wcsicmp(cls, WC_LISTVIEWW) == 0) {
            ListView_SetBkColor(c, pal().field);
            ListView_SetTextBkColor(c, pal().field);
            ListView_SetTextColor(c, pal().text);
        }
    }
    InvalidateRect(c, nullptr, TRUE);
}

LRESULT CALLBACK dialogProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (m) {
        case WM_CTLCOLORDLG: case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:
            return ctlColour(m, reinterpret_cast<HDC>(w), reinterpret_cast<HWND>(l));
        case WM_ERASEBKGND: {
            RECT rc; GetClientRect(h, &rc);
            makeBrushes();
            FillRect(reinterpret_cast<HDC>(w), &rc, g_br.face);
            return 1;
        }
        case WM_DRAWITEM: {
            auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (di->CtlType == ODT_BUTTON && GetPropW(di->hwndItem, L"FeatherThemed")) {   // a push button made owner-drawn here
                drawButton(di, GetPropW(di->hwndItem, L"FeatherDefault") != nullptr);
                return TRUE;
            }
            if (di->CtlType == ODT_MENU && drawMenu(di)) return TRUE;
            break;
        }
        case WM_MEASUREITEM:
            if (measureMenu(reinterpret_cast<MEASUREITEMSTRUCT*>(l))) return TRUE;
            break;
        case DM_GETDEFID: {   // the default button keeps working after it is drawn by the theme
            for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT))
                if (GetPropW(c, L"FeatherDefault")) return MAKELRESULT(GetDlgCtrlID(c), DC_HASDEFID);
            break;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(h, dialogProc, 1);
            break;
    }
    return DefSubclassProc(h, m, w, l);
}

// ---- menus ----
struct MenuItem { std::wstring text, accel; bool sep = false, popup = false, radio = false; };
std::vector<std::unique_ptr<MenuItem>> g_items;

void prepare(HMENU m) {
    makeBrushes();
    MENUINFO mi{sizeof(mi)};
    mi.fMask = MIM_BACKGROUND;
    mi.hbrBack = g_br.menu;
    SetMenuInfo(m, &mi);
    const int n = GetMenuItemCount(m);
    for (int i = 0; i < n; ++i) {
        wchar_t buf[512] = L"";
        MENUITEMINFOW ii{sizeof(ii)};
        ii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU | MIIM_STRING | MIIM_DATA;
        ii.dwTypeData = buf; ii.cch = 511;
        if (!GetMenuItemInfoW(m, static_cast<UINT>(i), TRUE, &ii)) continue;
        if (ii.fType & MFT_OWNERDRAW) continue;
        auto item = std::make_unique<MenuItem>();
        item->sep = (ii.fType & MFT_SEPARATOR) != 0;
        item->radio = (ii.fType & MFT_RADIOCHECK) != 0;
        item->popup = ii.hSubMenu != nullptr;
        std::wstring t = buf;
        const size_t tab = t.find(L'\t');
        item->text = tab == std::wstring::npos ? t : t.substr(0, tab);
        if (tab != std::wstring::npos) item->accel = t.substr(tab + 1);
        if (ii.hSubMenu) prepare(ii.hSubMenu);
        MENUITEMINFOW set{sizeof(set)};
        set.fMask = MIIM_FTYPE | MIIM_DATA;
        set.fType = (ii.fType & ~MFT_STRING) | MFT_OWNERDRAW;
        set.dwItemData = reinterpret_cast<ULONG_PTR>(item.get());
        SetMenuItemInfoW(m, static_cast<UINT>(i), TRUE, &set);
        g_items.push_back(std::move(item));
    }
}

bool isOurs(ULONG_PTR data) {
    for (const auto& i : g_items) if (reinterpret_cast<ULONG_PTR>(i.get()) == data) return true;
    return false;
}

// ---- message boxes ----
struct BoxState {
    std::wstring text;
    std::vector<std::wstring> labels;
    std::vector<HWND> buttons;
    UINT icon = 0;
    int chosen = -1, cancel = 0;
    bool done = false;
    RECT textRc{}, iconRc{};
    int footer = 0;
};
const wchar_t* kBoxClass = L"FeatherMessageBox";

void paintIcon(HDC dc, const RECT& r, UINT icon) {
    COLORREF c = pal().accent;
    const wchar_t* glyph = L"i";
    if (icon == MB_ICONWARNING) { c = RGB(0xE8, 0xA3, 0x17); glyph = L"!"; }
    else if (icon == MB_ICONERROR) { c = RGB(0xD9, 0x30, 0x30); glyph = L"\x00D7"; }
    else if (icon == MB_ICONQUESTION) glyph = L"?";
    HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, r.left, r.top, r.right + 1, r.bottom + 1);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b);
    LOGFONTW lf{};
    lf.lfHeight = -(r.bottom - r.top) * 7 / 10;
    lf.lfWeight = FW_BOLD;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    HFONT f = CreateFontIndirectW(&lf);
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, icon == MB_ICONWARNING ? RGB(0x20, 0x18, 0x00) : RGB(0xFF, 0xFF, 0xFF));
    RECT t = r;
    DrawTextW(dc, glyph, -1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, of);
    DeleteObject(f);
}

LRESULT CALLBACK boxProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<BoxState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            fillRect(dc, rc, pal().toolbar);
            RECT foot{0, rc.bottom - st->footer, rc.right, rc.bottom};
            fillRect(dc, foot, pal().panel);
            RECT line{0, foot.top, rc.right, foot.top + 1};
            fillRect(dc, line, pal().line);
            if (st->icon) paintIcon(dc, st->iconRc, st->icon);
            SelectObject(dc, g_font);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, pal().text);
            RECT t = st->textRc;
            DrawTextW(dc, st->text.c_str(), -1, &t, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CTLCOLORBTN: return ctlColour(m, reinterpret_cast<HDC>(w), reinterpret_cast<HWND>(l));
        case WM_DRAWITEM: {
            auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            drawButton(di, GetPropW(di->hwndItem, L"FeatherDefault") != nullptr);
            return TRUE;
        }
        case WM_COMMAND:
            if (HIWORD(w) == BN_CLICKED && LOWORD(w) >= 100 && LOWORD(w) < 100 + st->buttons.size()) { st->chosen = LOWORD(w) - 100; st->done = true; }
            return 0;
        case WM_CLOSE: st->chosen = st->cancel; st->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---- flat scroll bars ----
struct Scroll {
    int dragBar = -1, dragOffset = 0, dragPos = 0;
    int hoverBar = -1;
    int repeatBar = -1, repeatCode = 0;
};
constexpr UINT_PTR kRepeatTimer = 0xFE51;

// the bar's rectangle in window coordinates; false when it is not shown
bool barRect(HWND w, int bar, RECT& r) {
    SCROLLBARINFO sbi{sizeof(sbi)};
    if (!GetScrollBarInfo(w, bar == SB_VERT ? OBJID_VSCROLL : OBJID_HSCROLL, &sbi)) return false;
    if (sbi.rgstate[0] & (STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN)) return false;
    RECT wr; GetWindowRect(w, &wr);
    r = sbi.rcScrollBar;
    OffsetRect(&r, -wr.left, -wr.top);
    return r.right > r.left && r.bottom > r.top;
}

// the thumb's start and length along the bar; false when the whole content fits (no thumb)
bool thumb(HWND w, int bar, const RECT& r, const Scroll& s, int& start, int& len, int& avail, int& maxPos, int& minPos) {
    SCROLLINFO si{sizeof(si), SIF_ALL};
    GetScrollInfo(w, bar, &si);
    const int length = bar == SB_VERT ? r.bottom - r.top : r.right - r.left;
    const int range = si.nMax - si.nMin + 1;
    if (si.nPage == 0 || range <= static_cast<int>(si.nPage)) return false;
    len = std::max(px(28), static_cast<int>(static_cast<long long>(length) * si.nPage / range));
    len = std::min(len, length);
    avail = length - len;
    minPos = si.nMin;
    maxPos = si.nMax - static_cast<int>(si.nPage) + 1;
    const int pos = s.dragBar == bar ? s.dragPos : si.nPos;
    start = (bar == SB_VERT ? r.top : r.left) + (maxPos > minPos ? static_cast<int>(static_cast<long long>(avail) * (pos - minPos) / (maxPos - minPos)) : 0);
    return true;
}

void paintBars(HWND w, Scroll& s) {
    HDC dc = GetWindowDC(w);
    if (!dc) return;
    const COLORREF track = pal().dark ? mixColour(pal().view, pal().toolbar, 0.5) : mixColour(pal().view, pal().toolbar, 0.6);
    RECT rv{}, rh{};
    const bool v = barRect(w, SB_VERT, rv), hz = barRect(w, SB_HORZ, rh);
    for (int bar : {SB_VERT, SB_HORZ}) {
        const RECT& r = bar == SB_VERT ? rv : rh;
        if (!(bar == SB_VERT ? v : hz)) continue;
        const int bw = r.right - r.left, bh = r.bottom - r.top;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, bw, bh);
        HGDIOBJ old = SelectObject(mem, bmp);
        RECT local{0, 0, bw, bh};
        fillRect(mem, local, track);
        int start, len, avail, maxPos, minPos;
        if (thumb(w, bar, r, s, start, len, avail, maxPos, minPos)) {
            const COLORREF c = s.dragBar == bar ? pal().textDim : s.hoverBar == bar ? pal().textOff : mixColour(pal().textOff, track, 0.35);
            const int inset = std::max(2, (bar == SB_VERT ? bw : bh) / 4);
            RECT t = bar == SB_VERT ? RECT{inset, start - r.top + 2, bw - inset, start - r.top + len - 2}
                                    : RECT{start - r.left + 2, inset, start - r.left + len - 2, bh - inset};
            const int radius = bar == SB_VERT ? bw - 2 * inset : bh - 2 * inset;
            roundRect(mem, t, radius, c, CLR_INVALID);
        }
        BitBlt(dc, r.left, r.top, bw, bh, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
    }
    if (v && hz) {   // the corner between the two bars
        RECT c{rv.left, rh.top, rv.right, rh.bottom};
        fillRect(dc, c, track);
    }
    ReleaseDC(w, dc);
}

int barAt(HWND w, POINT screen) {
    RECT wr; GetWindowRect(w, &wr);
    const POINT p{screen.x - wr.left, screen.y - wr.top};
    for (int bar : {SB_VERT, SB_HORZ}) {
        RECT r;
        if (barRect(w, bar, r) && PtInRect(&r, p)) return bar;
    }
    return -1;
}

void sendScroll(HWND w, int bar, int code, int pos = 0) {
    SendMessageW(w, bar == SB_VERT ? WM_VSCROLL : WM_HSCROLL, MAKEWPARAM(code, pos & 0xFFFF), 0);
}

// the page step towards the mouse while the button is held on the track
void pageStep(HWND w, Scroll& s) {
    RECT r;
    if (s.repeatBar < 0 || !barRect(w, s.repeatBar, r)) return;
    POINT p; GetCursorPos(&p);
    RECT wr; GetWindowRect(w, &wr);
    const int at = s.repeatBar == SB_VERT ? p.y - wr.top : p.x - wr.left;
    int start, len, avail, maxPos, minPos;
    if (!thumb(w, s.repeatBar, r, s, start, len, avail, maxPos, minPos)) return;
    if (s.repeatCode == SB_PAGEUP && at < start) sendScroll(w, s.repeatBar, SB_PAGEUP);
    else if (s.repeatCode == SB_PAGEDOWN && at >= start + len) sendScroll(w, s.repeatBar, SB_PAGEDOWN);
}

LRESULT CALLBACK scrollProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto* s = reinterpret_cast<Scroll*>(data);
    switch (m) {
        case WM_NCPAINT: {
            const LONG_PTR style = GetWindowLongPtrW(h, GWL_STYLE), ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
            if ((style & (WS_BORDER | WS_DLGFRAME | WS_THICKFRAME)) || (ex & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE)))
                DefSubclassProc(h, m, w, l);   // a border: drawn by Windows, the bars over it
            paintBars(h, *s);
            return 0;
        }
        case WM_NCLBUTTONDOWN: case WM_NCLBUTTONDBLCLK: {
            if (w != HTVSCROLL && w != HTHSCROLL) break;
            const int bar = w == HTVSCROLL ? SB_VERT : SB_HORZ;
            RECT r;
            if (!barRect(h, bar, r)) return 0;
            RECT wr; GetWindowRect(h, &wr);
            const int at = bar == SB_VERT ? GET_Y_LPARAM(l) - wr.top : GET_X_LPARAM(l) - wr.left;
            int start, len, avail, maxPos, minPos;
            if (!thumb(h, bar, r, *s, start, len, avail, maxPos, minPos)) return 0;
            SetCapture(h);
            if (at >= start && at < start + len) {
                SCROLLINFO si{sizeof(si), SIF_POS};
                GetScrollInfo(h, bar, &si);
                s->dragBar = bar; s->dragOffset = at - start; s->dragPos = si.nPos;
                paintBars(h, *s);
            } else {
                s->repeatBar = bar;
                s->repeatCode = at < start ? SB_PAGEUP : SB_PAGEDOWN;
                sendScroll(h, bar, s->repeatCode);
                SetTimer(h, kRepeatTimer, 350, nullptr);
            }
            return 0;
        }
        case WM_TIMER:
            if (w != kRepeatTimer) break;
            SetTimer(h, kRepeatTimer, 60, nullptr);
            pageStep(h, *s);
            return 0;
        case WM_MOUSEMOVE: {
            if (s->dragBar < 0) { if (s->repeatBar >= 0) return 0; break; }
            RECT r;
            if (!barRect(h, s->dragBar, r)) return 0;
            POINT p; GetCursorPos(&p);
            RECT wr; GetWindowRect(h, &wr);
            const int at = s->dragBar == SB_VERT ? p.y - wr.top : p.x - wr.left;
            int start, len, avail, maxPos, minPos;
            if (!thumb(h, s->dragBar, r, *s, start, len, avail, maxPos, minPos)) return 0;
            const int origin = s->dragBar == SB_VERT ? r.top : r.left;
            const int off = std::clamp(at - s->dragOffset - origin, 0, std::max(0, avail));
            const int pos = avail > 0 ? minPos + static_cast<int>(static_cast<long long>(off) * (maxPos - minPos) / avail) : minPos;
            if (pos != s->dragPos) {
                s->dragPos = pos;
                sendScroll(h, s->dragBar, SB_THUMBTRACK, pos);
                paintBars(h, *s);
            }
            return 0;
        }
        case WM_LBUTTONUP: case WM_CAPTURECHANGED: {
            if (s->dragBar < 0 && s->repeatBar < 0) break;
            const int bar = s->dragBar;
            const int pos = s->dragPos;
            if (s->repeatBar >= 0) { KillTimer(h, kRepeatTimer); sendScroll(h, s->repeatBar, SB_ENDSCROLL); }
            s->repeatBar = -1;
            if (bar >= 0) { sendScroll(h, bar, SB_THUMBPOSITION, pos); s->dragBar = -1; sendScroll(h, bar, SB_ENDSCROLL); }
            if (m == WM_LBUTTONUP && GetCapture() == h) ReleaseCapture();
            paintBars(h, *s);
            return 0;
        }
        case WM_NCMOUSEMOVE: {
            const int bar = w == HTVSCROLL ? SB_VERT : w == HTHSCROLL ? SB_HORZ : -1;
            if (bar != s->hoverBar) { s->hoverBar = bar; paintBars(h, *s); }
            if (bar >= 0) {
                TRACKMOUSEEVENT te{sizeof(te), TME_LEAVE | TME_NONCLIENT, h, 0};
                TrackMouseEvent(&te);
                return 0;   // no hot-tracking drawing by Windows
            }
            break;
        }
        case WM_NCMOUSELEAVE:
            if (s->hoverBar >= 0) { s->hoverBar = -1; paintBars(h, *s); }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(h, scrollProc, 2);
            delete s;
            break;
    }
    return DefSubclassProc(h, m, w, l);
}

Scroll* scrollOf(HWND w) {
    DWORD_PTR data = 0;
    return GetWindowSubclass(w, scrollProc, 2, &data) ? reinterpret_cast<Scroll*>(data) : nullptr;
}

} // namespace

void setFont(HFONT font, HFONT bold) { g_font = font; g_bold = bold; }
void setDpi(int dpi) { g_dpi = dpi > 0 ? dpi : 96; }

void paletteChanged(HWND mainWindow) {
    makeBrushes();
    appDarkMode(pal().dark);
    (void)mainWindow;
}

// ---- menus ----
void prepareMenu(HMENU m) { prepare(m); }
void releaseMenus() { g_items.clear(); }

bool measureMenu(MEASUREITEMSTRUCT* mi) {
    if (mi->CtlType != ODT_MENU || !isOurs(mi->itemData)) return false;
    const auto* it = reinterpret_cast<const MenuItem*>(mi->itemData);
    if (it->sep) { mi->itemHeight = static_cast<UINT>(px(7)); mi->itemWidth = static_cast<UINT>(px(20)); return true; }
    HDC dc = GetDC(nullptr);
    HGDIOBJ of = SelectObject(dc, g_font ? g_font : GetStockObject(DEFAULT_GUI_FONT));
    SIZE a{}, b{};
    GetTextExtentPoint32W(dc, it->text.c_str(), static_cast<int>(it->text.size()), &a);
    if (!it->accel.empty()) GetTextExtentPoint32W(dc, it->accel.c_str(), static_cast<int>(it->accel.size()), &b);
    SelectObject(dc, of);
    ReleaseDC(nullptr, dc);
    mi->itemWidth = static_cast<UINT>(px(34) + a.cx + (b.cx ? px(36) + b.cx : 0) + px(30));
    mi->itemHeight = static_cast<UINT>(std::max<int>(px(22), a.cy + px(5)));   // about Windows' own height, so long menus fit
    return true;
}

bool drawMenu(DRAWITEMSTRUCT* di) {
    if (di->CtlType != ODT_MENU || !isOurs(di->itemData)) return false;
    const auto* it = reinterpret_cast<const MenuItem*>(di->itemData);
    const Palette& P = pal();
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    fillRect(dc, r, P.toolbar);
    if (it->sep) {
        RECT l{r.left + px(10), (r.top + r.bottom) / 2, r.right - px(10), (r.top + r.bottom) / 2 + 1};
        fillRect(dc, l, P.line);
        return true;
    }
    const bool off = (di->itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;
    const bool sel = (di->itemState & ODS_SELECTED) != 0;
    if (sel && !off) roundRect(dc, RECT{r.left + px(4), r.top + 1, r.right - px(4), r.bottom - 1}, px(8), P.hover, CLR_INVALID);
    const COLORREF fg = off ? P.textOff : P.text;
    if (di->itemState & ODS_CHECKED) {   // a tick (or a dot for a choice of several)
        const int cx = r.left + px(18), cy = (r.top + r.bottom) / 2;
        const COLORREF c = off ? P.textOff : P.accent;
        if (it->radio) {
            HBRUSH b = CreateSolidBrush(c);
            HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, cx - px(4), cy - px(4), cx + px(4) + 1, cy + px(4) + 1);
            SelectObject(dc, ob); SelectObject(dc, op);
            DeleteObject(b);
        } else {
            HPEN pen = CreatePen(PS_SOLID, std::max(2, px(2)), c);
            HGDIOBJ op = SelectObject(dc, pen);
            POINT pts[3] = {{cx - px(5), cy}, {cx - px(1), cy + px(4)}, {cx + px(6), cy - px(4)}};
            Polyline(dc, pts, 3);
            SelectObject(dc, op);
            DeleteObject(pen);
        }
    }
    SelectObject(dc, g_font ? g_font : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    RECT t{r.left + px(34), r.top, r.right - px(26), r.bottom};
    DrawTextW(dc, it->text.c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (!it->accel.empty()) {
        SetTextColor(dc, off ? P.textOff : P.textDim);
        DrawTextW(dc, it->accel.c_str(), -1, &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    if (it->popup) {   // the arrow of a submenu; Windows' own (in its colours) is kept out of the item
        const int ax = r.right - px(16), ay = (r.top + r.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, std::max(1, px(1) + px(1) / 2), fg);
        HGDIOBJ op = SelectObject(dc, pen);
        POINT pts[3] = {{ax - px(3), ay - px(4)}, {ax + px(1), ay}, {ax - px(3), ay + px(4)}};
        Polyline(dc, pts, 3);
        SelectObject(dc, op);
        DeleteObject(pen);
        ExcludeClipRect(dc, r.right - px(26), r.top, r.right, r.bottom);
    }
    return true;
}

// ---- dialogs ----
void themeDialog(HWND dlg) {
    makeBrushes();
    SetWindowSubclass(dlg, dialogProc, 1, 0);
    titleColours(dlg);
    for (HWND c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) themeControl(c);
    RedrawWindow(dlg, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

LRESULT ctlColour(UINT msg, HDC dc, HWND ctl) {
    makeBrushes();
    const Palette& P = pal();
    if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX) {
        SetTextColor(dc, P.text);
        SetBkColor(dc, P.field);
        return reinterpret_cast<LRESULT>(g_br.field);
    }
    SetTextColor(dc, ctl && !IsWindowEnabled(ctl) ? P.textOff : P.text);
    SetBkColor(dc, P.toolbar);
    if (msg == WM_CTLCOLORSTATIC) {
        wchar_t cls[16] = L"";
        if (ctl) GetClassNameW(ctl, cls, 16);
        if (_wcsicmp(cls, L"Edit") == 0) {   // a read-only box
            SetBkColor(dc, P.panel);
            static HBRUSH b = nullptr; static COLORREF bc = CLR_INVALID;
            if (bc != P.panel) { if (b) DeleteObject(b); b = CreateSolidBrush(P.panel); bc = P.panel; }
            return reinterpret_cast<LRESULT>(b);
        }
    }
    return reinterpret_cast<LRESULT>(g_br.face);
}

void drawButton(const DRAWITEMSTRUCT* di, bool isDefault) {
    const Palette& P = pal();
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    HWND parent = GetParent(di->hwndItem);
    {   // the parent's background around the rounded corners
        HBRUSH bg = reinterpret_cast<HBRUSH>(SendMessageW(parent, WM_CTLCOLORBTN, reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(di->hwndItem)));
        if (bg) FillRect(dc, &r, bg); else fillRect(dc, r, P.toolbar);
    }
    const bool off = (di->itemState & ODS_DISABLED) != 0, down = (di->itemState & ODS_SELECTED) != 0, focus = (di->itemState & ODS_FOCUS) != 0;
    COLORREF fill = isDefault ? P.accent : P.field, text = isDefault ? (luminance(P.accent) > 0.45 ? RGB(0x10, 0x14, 0x1C) : RGB(0xFF, 0xFF, 0xFF)) : P.text;
    COLORREF border = isDefault ? P.accent : P.line;
    if (down) fill = isDefault ? mixColour(P.accent, RGB(0, 0, 0), 0.2) : P.hover;
    if (off) { fill = P.toolbar; text = P.textOff; border = P.line; }
    if (focus && !off) border = isDefault ? P.text : P.accent;
    roundRect(dc, RECT{r.left, r.top, r.right, r.bottom}, px(8), fill, border);
    wchar_t buf[128] = L"";
    GetWindowTextW(di->hwndItem, buf, 128);
    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(di->hwndItem, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text);
    DrawTextW(dc, buf, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_HIDEPREFIX);
}

int choiceBox(HWND owner, const wchar_t* caption, const std::wstring& text, const std::vector<std::wstring>& labels,
              int defaultIndex, int cancelIndex, UINT icon) {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = boxProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kBoxClass;
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        registered = true;
    }
    if (!owner) owner = GetActiveWindow();
    if (owner) { HWND top = GetAncestor(owner, GA_ROOT); if (top) owner = top; }
    makeBrushes();
    BoxState st;
    st.text = text; st.labels = labels; st.icon = icon; st.cancel = cancelIndex;
    // measure the text: at most about 440 pixels wide
    HDC dc = GetDC(nullptr);
    HGDIOBJ of = SelectObject(dc, g_font ? g_font : GetStockObject(DEFAULT_GUI_FONT));
    RECT tr{0, 0, px(440), 0};
    DrawTextW(dc, text.c_str(), -1, &tr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS | DT_CALCRECT);
    int buttonsW = 0;
    std::vector<int> bw;
    for (const auto& b : labels) {
        SIZE s{};
        GetTextExtentPoint32W(dc, b.c_str(), static_cast<int>(b.size()), &s);
        bw.push_back(std::max(px(88), static_cast<int>(s.cx) + px(28)));
        buttonsW += bw.back() + px(8);
    }
    SelectObject(dc, of);
    ReleaseDC(nullptr, dc);
    const int pad = px(20), iconS = icon ? px(32) : 0;
    const int textX = pad + (icon ? iconS + px(14) : 0);
    const int bodyH = std::max(static_cast<int>(tr.bottom), iconS);
    st.footer = px(56);
    const int cw = std::max(textX + static_cast<int>(tr.right) + pad, buttonsW + 2 * pad - px(8));
    const int ch = pad + bodyH + pad + st.footer;
    st.iconRc = {pad, pad, pad + iconS, pad + iconS};
    st.textRc = {textX, pad + (bodyH - tr.bottom) / 2, textX + tr.right, pad + (bodyH - tr.bottom) / 2 + tr.bottom};
    RECT wr{0, 0, cw, ch};
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    AdjustWindowRectEx(&wr, style, FALSE, WS_EX_DLGMODALFRAME);
    RECT orc{};
    if (owner && IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner, &orc);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &orc, 0);
    const int W = wr.right - wr.left, H = wr.bottom - wr.top;
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, kBoxClass, caption ? caption : L"", style, orc.left + (orc.right - orc.left - W) / 2,
                             orc.top + (orc.bottom - orc.top - H) / 2, W, H, owner, nullptr, inst, &st);
    if (!h) return cancelIndex;
    titleColours(h);
    int x = cw - pad;
    for (size_t i = labels.size(); i-- > 0;) {
        x -= bw[i];
        HWND b = CreateWindowExW(0, L"BUTTON", labels[i].c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, x, ch - st.footer + px(12),
                                 bw[i], px(32), h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100 + i)), inst, nullptr);
        SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), FALSE);
        if (static_cast<int>(i) == defaultIndex) SetPropW(b, L"FeatherDefault", reinterpret_cast<HANDLE>(1));
        x -= px(8);
    }
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) st.buttons.push_back(GetDlgItem(h, 100 + i));
    if (icon) MessageBeep(icon);
    const bool ownerWasEnabled = owner && IsWindowEnabled(owner);
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(h, SW_SHOW);
    if (defaultIndex >= 0 && defaultIndex < static_cast<int>(st.buttons.size())) SetFocus(st.buttons[defaultIndex]);
    MSG msg;
    while (!st.done) {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0) { PostQuitMessage(static_cast<int>(msg.wParam)); break; }
        if (r < 0) break;
        if (msg.message == WM_KEYDOWN && (msg.hwnd == h || IsChild(h, msg.hwnd))) {
            if (msg.wParam == VK_ESCAPE) { st.chosen = cancelIndex; st.done = true; continue; }
            if (msg.wParam == VK_RETURN || msg.wParam == VK_SPACE) {
                HWND f = GetFocus();
                int idx = defaultIndex;
                for (size_t i = 0; i < st.buttons.size(); ++i) if (st.buttons[i] == f) idx = static_cast<int>(i);
                st.chosen = idx; st.done = true; continue;
            }
            if (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT) {
                HWND f = GetFocus();
                for (size_t i = 0; i < st.buttons.size(); ++i)
                    if (st.buttons[i] == f) {
                        const size_t n = st.buttons.size();
                        SetFocus(st.buttons[(i + (msg.wParam == VK_RIGHT ? 1 : n - 1)) % n]);
                        break;
                    }
                continue;
            }
            if (msg.wParam == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) && OpenClipboard(h)) {   // copy the message, like Windows' own boxes
                EmptyClipboard();
                const std::wstring all = std::wstring(caption ? caption : L"") + L"\r\n\r\n" + text;
                if (HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (all.size() + 1) * sizeof(wchar_t))) {
                    memcpy(GlobalLock(g), all.c_str(), (all.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(g);
                    SetClipboardData(CF_UNICODETEXT, g);
                }
                CloseClipboard();
                continue;
            }
        }
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner && ownerWasEnabled) EnableWindow(owner, TRUE);
    DestroyWindow(h);
    if (owner) SetActiveWindow(owner);
    return st.chosen < 0 ? cancelIndex : st.chosen;
}

int messageBox(HWND owner, const wchar_t* text, const wchar_t* caption, UINT flags) {
    std::vector<std::wstring> labels;
    std::vector<int> ids;
    switch (flags & MB_TYPEMASK) {
        case MB_OKCANCEL: labels = {L"OK", L"Cancel"}; ids = {IDOK, IDCANCEL}; break;
        case MB_YESNO: labels = {L"Yes", L"No"}; ids = {IDYES, IDNO}; break;
        case MB_YESNOCANCEL: labels = {L"Yes", L"No", L"Cancel"}; ids = {IDYES, IDNO, IDCANCEL}; break;
        case MB_RETRYCANCEL: labels = {L"Retry", L"Cancel"}; ids = {IDRETRY, IDCANCEL}; break;
        default: labels = {L"OK"}; ids = {IDOK}; break;
    }
    int def = 0;
    if ((flags & MB_DEFMASK) == MB_DEFBUTTON2) def = 1;
    else if ((flags & MB_DEFMASK) == MB_DEFBUTTON3) def = 2;
    def = std::min(def, static_cast<int>(labels.size()) - 1);
    // Esc: Cancel where there is one, else No, else OK
    int cancel = static_cast<int>(labels.size()) - 1;
    const UINT icon = flags & MB_ICONMASK;
    const int i = choiceBox(owner, caption, text ? text : L"", labels, def, cancel, icon);
    return ids[static_cast<size_t>(std::clamp(i, 0, static_cast<int>(ids.size()) - 1))];
}

// ---- scroll bars ----
void flatScroll(HWND w) {
    if (scrollOf(w)) return;
    SetWindowSubclass(w, scrollProc, 2, reinterpret_cast<DWORD_PTR>(new Scroll));
    SetWindowPos(w, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void setScrollInfo(HWND w, int bar, SCROLLINFO& si) {
    SetScrollInfo(w, bar, &si, FALSE);
    if (Scroll* s = scrollOf(w)) paintBars(w, *s);
    else RedrawWindow(w, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_NOCHILDREN);
}

void setScrollPos(HWND w, int bar, int pos) {
    SetScrollPos(w, bar, pos, FALSE);
    if (Scroll* s = scrollOf(w)) paintBars(w, *s);
}

int trackPos(HWND w, int bar) {
    if (Scroll* s = scrollOf(w)) if (s->dragBar == bar) return s->dragPos;
    SCROLLINFO si{sizeof(si), SIF_TRACKPOS};
    GetScrollInfo(w, bar, &si);
    return si.nTrackPos;
}

} // namespace themeui
