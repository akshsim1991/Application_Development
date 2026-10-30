#include "settingsdlg.hpp"
#include "theme.hpp"
#include "themeui.hpp"
#include <windowsx.h>
#include <algorithm>

namespace settingsdlg {
namespace {

enum { ID_NAV = 100, ID_ITEM = 200, ID_CLOSE = IDCANCEL };
const wchar_t* kClass = L"FeatherSettings";

struct State {
    const std::vector<Category>* cats = nullptr;
    int cat = 0;
    int dpi = 96;
    HFONT font = nullptr, bold = nullptr, title = nullptr, small = nullptr;
    HWND wnd = nullptr;
    std::vector<HWND> nav, ctls;   // category buttons; one control per item of the category shown
    std::vector<int> rowTop;       // where each item's row starts (rows are as tall as their description needs)
    bool done = false;
    int px(int v) const { return MulDiv(v, dpi, 96); }
    int navW() const { return px(210); }
    int rowH() const { return px(72); }
    int footH() const { return px(60); }
    int top() const { return px(76); }
};

void fill(HDC dc, const RECT& r, COLORREF c) { HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
void round(HDC dc, const RECT& r, int rad, COLORREF c, COLORREF border = CLR_INVALID) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN p = CreatePen(PS_SOLID, 1, border == CLR_INVALID ? c : border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}

// the controls of the category shown: made again when another category is chosen
void buildItems(State& s) {
    for (HWND c : s.ctls) DestroyWindow(c);
    s.ctls.clear();
    RECT rc; GetClientRect(s.wnd, &rc);
    const auto& items = (*s.cats)[static_cast<size_t>(s.cat)].items;
    const int right = rc.right - s.px(28);
    s.rowTop.clear();
    {
        HDC dc = GetDC(s.wnd);
        HGDIOBJ of = SelectObject(dc, s.small);
        int y = s.top();
        for (const Item& it : items) {
            s.rowTop.push_back(y);
            const int ctlW = it.kind == Item::Toggle ? s.px(70) : s.px(240);
            RECT hr{0, 0, right - ctlW - (s.navW() + s.px(28)), 0};
            DrawTextW(dc, it.help.c_str(), -1, &hr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
            y += std::max(s.rowH(), s.px(44) + static_cast<int>(hr.bottom));
        }
        s.rowTop.push_back(y);
        SelectObject(dc, of);
        ReleaseDC(s.wnd, dc);
    }
    HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(s.wnd, GWLP_HINSTANCE));
    for (size_t i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        const int y = s.rowTop[i];
        const int id = ID_ITEM + static_cast<int>(i);
        HWND c = nullptr;
        if (it.kind == Item::Toggle) {
            c = CreateWindowExW(0, L"BUTTON", it.label.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                right - s.px(52), y + s.px(14), s.px(52), s.px(28), s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        } else if (it.kind == Item::Choice) {
            c = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                                right - s.px(220), y + s.px(12), s.px(220), s.px(300), s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(s.font), FALSE);
            for (const auto& ch : it.choices) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ch.c_str()));
            SendMessageW(c, CB_SETCURSEL, static_cast<WPARAM>(it.get ? it.get() : 0), 0);
        } else {
            c = CreateWindowExW(0, L"BUTTON", it.choices.empty() ? it.label.c_str() : it.choices[0].c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                right - s.px(220), y + s.px(12), s.px(220), s.px(34), s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(s.font), FALSE);
        }
        s.ctls.push_back(c);
    }
    themeui::themeDialog(s.wnd);
    InvalidateRect(s.wnd, nullptr, TRUE);
}

void choose(State& s, int cat) {
    if (cat < 0 || cat >= static_cast<int>(s.cats->size())) return;
    s.cat = cat;
    buildItems(s);
    for (HWND n : s.nav) InvalidateRect(n, nullptr, FALSE);
}

void paint(State& s) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(s.wnd, &ps);
    RECT rc; GetClientRect(s.wnd, &rc);
    const Palette& P = pal();
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(mem, bmp);
    fill(mem, rc, P.toolbar);
    RECT nav{0, 0, s.navW(), rc.bottom};
    fill(mem, nav, P.panel);
    RECT nl{s.navW() - 1, 0, s.navW(), rc.bottom};
    fill(mem, nl, P.line);
    RECT foot{s.navW(), rc.bottom - s.footH(), rc.right, rc.bottom};
    RECT fl{s.navW(), foot.top, rc.right, foot.top + 1};
    fill(mem, fl, P.line);
    SetBkMode(mem, TRANSPARENT);
    const Category& c = (*s.cats)[static_cast<size_t>(s.cat)];
    SelectObject(mem, s.title);
    SetTextColor(mem, P.text);
    RECT t{s.navW() + s.px(28), s.px(18), rc.right - s.px(28), s.px(60)};
    DrawTextW(mem, c.name.c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    for (size_t i = 0; i < c.items.size(); ++i) {
        const Item& it = c.items[i];
        const int y = s.rowTop[i], rowEnd = s.rowTop[i + 1];
        const int ctlW = it.kind == Item::Toggle ? s.px(70) : s.px(240);
        RECT lr{s.navW() + s.px(28), y + s.px(10), rc.right - s.px(28) - ctlW, y + s.px(32)};
        SelectObject(mem, s.font);
        SetTextColor(mem, P.text);
        DrawTextW(mem, it.label.c_str(), -1, &lr, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        RECT hr{lr.left, y + s.px(33), lr.right, rowEnd - s.px(4)};
        SelectObject(mem, s.small);
        SetTextColor(mem, P.textDim);
        DrawTextW(mem, it.help.c_str(), -1, &hr, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (i + 1 < c.items.size()) {
            RECT sep{s.navW() + s.px(28), rowEnd - 1, rc.right - s.px(28), rowEnd};
            fill(mem, sep, mixColour(P.line, P.toolbar, 0.4));
        }
    }
    SelectObject(mem, s.small);
    SetTextColor(mem, P.textDim);
    RECT fn{s.navW() + s.px(28), foot.top, rc.right - s.px(140), foot.bottom};
    DrawTextW(mem, L"Changes apply at once and are kept.", -1, &fn, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(s.wnd, &ps);
}

// a category in the list on the left
void drawNav(State& s, const DRAWITEMSTRUCT* di) {
    const Palette& P = pal();
    const int i = static_cast<int>(di->CtlID) - ID_NAV;
    RECT r = di->rcItem;
    fill(di->hDC, r, P.panel);
    const bool sel = i == s.cat, focus = (di->itemState & ODS_FOCUS) != 0;
    RECT b{r.left + s.px(10), r.top + s.px(2), r.right - s.px(10), r.bottom - s.px(2)};
    if (sel) {
        round(di->hDC, b, s.px(10), P.accentSoft);
        RECT bar{b.left + s.px(4), b.top + s.px(9), b.left + s.px(7), b.bottom - s.px(9)};
        round(di->hDC, bar, s.px(3), P.accent);
    } else if (di->itemState & ODS_SELECTED) round(di->hDC, b, s.px(10), P.hover);
    if (focus) {   // keyboard focus: a thin ring
        HBRUSH hb = CreateSolidBrush(P.accent);
        FrameRect(di->hDC, &b, hb);
        DeleteObject(hb);
    }
    SetBkMode(di->hDC, TRANSPARENT);
    SelectObject(di->hDC, sel ? s.bold : s.font);
    SetTextColor(di->hDC, sel ? P.accentText : P.text);
    RECT t{b.left + s.px(18), b.top, b.right - s.px(8), b.bottom};
    DrawTextW(di->hDC, (*s.cats)[static_cast<size_t>(i)].name.c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// an on / off switch
void drawToggle(State& s, const DRAWITEMSTRUCT* di) {
    const Palette& P = pal();
    const Item& it = (*s.cats)[static_cast<size_t>(s.cat)].items[di->CtlID - ID_ITEM];
    const bool on = it.get && it.get() != 0, focus = (di->itemState & ODS_FOCUS) != 0;
    RECT r = di->rcItem;
    fill(di->hDC, r, P.toolbar);
    const int h = s.px(22), w = s.px(44);
    RECT tr{r.right - w - s.px(2), (r.top + r.bottom - h) / 2, r.right - s.px(2), (r.top + r.bottom + h) / 2};
    if (focus) { RECT fr{tr.left - s.px(3), tr.top - s.px(3), tr.right + s.px(3), tr.bottom + s.px(3)}; round(di->hDC, fr, h + s.px(6), P.toolbar, P.accent); }
    round(di->hDC, tr, h, on ? P.accent : mixColour(P.textOff, P.toolbar, 0.45), on ? P.accent : P.textOff);
    const int d = h - s.px(6);
    const int x = on ? tr.right - s.px(3) - d : tr.left + s.px(3);
    RECT knob{x, tr.top + s.px(3), x + d, tr.top + s.px(3) + d};
    HBRUSH kb = CreateSolidBrush(on ? (luminance(P.accent) > 0.45 ? RGB(0x10, 0x14, 0x1C) : RGB(0xFF, 0xFF, 0xFF)) : (P.dark ? P.text : RGB(0xFF, 0xFF, 0xFF)));
    HGDIOBJ ob = SelectObject(di->hDC, kb), op = SelectObject(di->hDC, GetStockObject(NULL_PEN));
    Ellipse(di->hDC, knob.left, knob.top, knob.right + 1, knob.bottom + 1);
    SelectObject(di->hDC, ob); SelectObject(di->hDC, op);
    DeleteObject(kb);
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: if (s) { paint(*s); return 0; } break;
        case WM_MEASUREITEM: {   // the lists' lines
            auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (mi->CtlType != ODT_COMBOBOX) break;
            const int dpi = s ? s->dpi : 96;
            mi->itemHeight = static_cast<UINT>(MulDiv(mi->itemID == static_cast<UINT>(-1) ? 24 : 28, dpi, 96));
            return TRUE;
        }
        case WM_DRAWITEM: {
            auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (s && di->CtlType == ODT_COMBOBOX) {   // a list in the theme's colours, the closed box and the opened list alike
                const Palette& P = pal();
                const bool sel = (di->itemState & ODS_SELECTED) && !(di->itemState & ODS_COMBOBOXEDIT);
                fill(di->hDC, di->rcItem, sel ? P.accentSoft : P.field);
                if (di->itemID != static_cast<UINT>(-1)) {
                    wchar_t text[128] = L"";
                    SendMessageW(di->hwndItem, CB_GETLBTEXT, di->itemID, reinterpret_cast<LPARAM>(text));
                    SetBkMode(di->hDC, TRANSPARENT);
                    SelectObject(di->hDC, s->font);
                    SetTextColor(di->hDC, sel ? P.accentText : P.text);
                    RECT t = di->rcItem; t.left += s->px(8);
                    DrawTextW(di->hDC, text, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
                }
                if ((di->itemState & ODS_FOCUS) && (di->itemState & ODS_COMBOBOXEDIT)) { HBRUSH fb = CreateSolidBrush(P.accent); FrameRect(di->hDC, &di->rcItem, fb); DeleteObject(fb); }
                return TRUE;
            }
            if (!s || di->CtlType != ODT_BUTTON) break;
            if (di->CtlID >= ID_NAV && di->CtlID < ID_NAV + s->cats->size()) { drawNav(*s, di); return TRUE; }
            if (di->CtlID >= ID_ITEM) { drawToggle(*s, di); return TRUE; }
            break;
        }
        case WM_COMMAND: {
            if (!s) break;
            const int id = LOWORD(w), code = HIWORD(w);
            if (id == ID_CLOSE) { s->done = true; return 0; }
            if (id >= ID_NAV && id < ID_NAV + static_cast<int>(s->cats->size())) { if (code == BN_CLICKED) choose(*s, id - ID_NAV); return 0; }
            const auto& items = (*s->cats)[static_cast<size_t>(s->cat)].items;
            if (id >= ID_ITEM && id < ID_ITEM + static_cast<int>(items.size())) {
                const Item& it = items[static_cast<size_t>(id - ID_ITEM)];
                const int cat = s->cat;
                if (it.kind == Item::Toggle && code == BN_CLICKED) { if (it.set) it.set(it.get && it.get() ? 0 : 1); InvalidateRect(reinterpret_cast<HWND>(l), nullptr, FALSE); }
                else if (it.kind == Item::Choice && code == CBN_SELCHANGE) { if (it.set) it.set(static_cast<int>(SendMessageW(reinterpret_cast<HWND>(l), CB_GETCURSEL, 0, 0))); }
                else if (it.kind == Item::Button && code == BN_CLICKED) { if (it.press) it.press(); }
                else return 0;
                // the theme may have changed: everything in the window takes the new colours
                if (cat == s->cat) {
                    themeui::themeDialog(h);
                    RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
                }
            }
            return 0;
        }
        case WM_CLOSE: if (s) s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

void run(HWND parent, HINSTANCE inst, const std::vector<Category>& cats, int first, int dpi, HFONT font, HFONT bold) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        registered = RegisterClassExW(&wc) != 0;
    }
    State s;
    s.cats = &cats;
    s.cat = std::clamp(first, 0, static_cast<int>(cats.size()) - 1);
    s.dpi = dpi;
    s.font = font; s.bold = bold;
    LOGFONTW lf{};
    GetObjectW(font, sizeof(lf), &lf);
    LOGFONTW tl = lf; tl.lfHeight = lf.lfHeight * 3 / 2; tl.lfWeight = FW_SEMIBOLD;
    s.title = CreateFontIndirectW(&tl);
    LOGFONTW sl = lf; sl.lfHeight = lf.lfHeight * 9 / 10;
    s.small = CreateFontIndirectW(&sl);
    size_t most = 0;
    for (const auto& c : cats) most = std::max(most, c.items.size());
    const int cw = s.px(820), ch = std::max(s.top() + static_cast<int>(most) * (s.rowH() + s.px(16)) + s.footH() + s.px(12), s.px(78) + static_cast<int>(cats.size()) * s.px(44) + s.footH());
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    RECT wr{0, 0, cw, ch};
    AdjustWindowRectEx(&wr, style, FALSE, WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT);
    RECT pr; GetWindowRect(parent, &pr);
    const int W = wr.right - wr.left, H = wr.bottom - wr.top;
    s.wnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kClass, L"Settings", style, pr.left + (pr.right - pr.left - W) / 2,
                            std::max<int>(pr.top, pr.top + (pr.bottom - pr.top - H) / 2), W, H, parent, nullptr, inst, &s);
    if (!s.wnd) { DeleteObject(s.title); DeleteObject(s.small); return; }
    for (size_t i = 0; i < cats.size(); ++i) {
        HWND n = CreateWindowExW(0, L"BUTTON", cats[i].name.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, s.px(18) + static_cast<int>(i) * s.px(44),
                                 s.navW() - 1, s.px(42), s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_NAV + i)), inst, nullptr);
        s.nav.push_back(n);
    }
    HWND close = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, cw - s.px(28) - s.px(104),
                                 ch - s.footH() + s.px(13), s.px(104), s.px(34), s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CLOSE)), inst, nullptr);
    SendMessageW(close, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    buildItems(s);
    EnableWindow(parent, FALSE);
    ShowWindow(s.wnd, SW_SHOW);
    SetFocus(s.nav[static_cast<size_t>(s.cat)]);
    MSG msg;
    while (!s.done) {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0) { PostQuitMessage(static_cast<int>(msg.wParam)); break; }
        if (r < 0) break;
        if (msg.message == WM_KEYDOWN && (msg.hwnd == s.wnd || IsChild(s.wnd, msg.hwnd))) {
            if (msg.wParam == VK_ESCAPE) {
                wchar_t cls[16] = L"";
                GetClassNameW(msg.hwnd, cls, 16);
                if (!(_wcsicmp(cls, L"ComboBox") == 0 && SendMessageW(msg.hwnd, CB_GETDROPPEDSTATE, 0, 0))) { s.done = true; continue; }
            }
            // Ctrl+Tab / Ctrl+Page Down: the next category; Up / Down in the list of categories
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const int n = static_cast<int>(cats.size());
            int step = 0;
            if (ctrl && (msg.wParam == VK_TAB || msg.wParam == VK_NEXT)) step = (msg.wParam == VK_TAB && (GetKeyState(VK_SHIFT) & 0x8000)) ? -1 : 1;
            else if (ctrl && msg.wParam == VK_PRIOR) step = -1;
            else if (std::find(s.nav.begin(), s.nav.end(), msg.hwnd) != s.nav.end() && (msg.wParam == VK_DOWN || msg.wParam == VK_UP)) step = msg.wParam == VK_DOWN ? 1 : -1;
            if (step) {
                choose(s, (s.cat + step + n) % n);
                SetFocus(s.nav[static_cast<size_t>(s.cat)]);
                continue;
            }
        }
        if (!IsDialogMessageW(s.wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(s.wnd);
    SetActiveWindow(parent);
    DeleteObject(s.title);
    DeleteObject(s.small);
}

} // namespace settingsdlg
