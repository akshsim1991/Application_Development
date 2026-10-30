#include "startpage.hpp"
#include "icons.hpp"
#include "theme.hpp"
#include "themeui.hpp"
#include <windowsx.h>
#include <algorithm>

namespace startpage {
namespace {

enum Hit { H_NONE = -1, H_ZONE = 0, H_BUTTON = 1, H_CLEAR = 2, H_ROW = 10 };

struct State {
    Actions act;
    std::vector<std::wstring> recent;
    std::vector<bool> missing;                        // the file was not found when the list was given
    int dpi = 96;
    HFONT title = nullptr, sub = nullptr, body = nullptr, small = nullptr, heading = nullptr, strong = nullptr;
    RECT zone{}, button{}, clear{};                   // laid out when painted
    std::vector<RECT> rows;
    int hover = H_NONE, pressed = H_NONE;
};

State* stateOf(HWND w) { return reinterpret_cast<State*>(GetWindowLongPtrW(w, GWLP_USERDATA)); }
int px(const State& s, int v) { return MulDiv(v, s.dpi, 96); }

HFONT makeFont(int dpi, int tenthsOfPoint, int weight) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(tenthsOfPoint, dpi, 720);
    lf.lfWeight = weight;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&lf);
}

void freeFonts(State& s) {
    for (HFONT* f : {&s.title, &s.sub, &s.body, &s.small, &s.heading, &s.strong})
        if (*f) { DeleteObject(*f); *f = nullptr; }
}

void makeFonts(State& s) {
    freeFonts(s);
    s.title = makeFont(s.dpi, 240, FW_SEMIBOLD);
    s.sub = makeFont(s.dpi, 115, FW_NORMAL);
    s.body = makeFont(s.dpi, 110, FW_NORMAL);
    s.small = makeFont(s.dpi, 95, FW_NORMAL);
    s.heading = makeFont(s.dpi, 115, FW_SEMIBOLD);
    s.strong = makeFont(s.dpi, 110, FW_SEMIBOLD);
}

int lineHeight(HDC dc, HFONT f) {
    HGDIOBJ old = SelectObject(dc, f);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    return tm.tmHeight;
}

COLORREF mix(COLORREF a, COLORREF b, int t) {   // t of 255 parts of b
    return RGB((GetRValue(a) * (255 - t) + GetRValue(b) * t) / 255, (GetGValue(a) * (255 - t) + GetGValue(b) * t) / 255,
               (GetBValue(a) * (255 - t) + GetBValue(b) * t) / 255);
}

void roundBox(HDC dc, const RECT& r, int radius, COLORREF fill, HPEN pen) {
    HBRUSH b = CreateSolidBrush(fill);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen ? static_cast<HGDIOBJ>(pen) : GetStockObject(NULL_PEN));
    RoundRect(dc, r.left, r.top, r.right + (pen ? 0 : 1), r.bottom + (pen ? 0 : 1), radius, radius);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b);
}

void text(HDC dc, HFONT f, COLORREF c, const std::wstring& t, RECT r, UINT flags) {
    HGDIOBJ old = SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, t.c_str(), -1, &r, flags | DT_NOPREFIX | DT_SINGLELINE);
    SelectObject(dc, old);
}

std::wstring nameOf(const std::wstring& p) { const size_t c = p.find_last_of(L"\\/"); return c == std::wstring::npos ? p : p.substr(c + 1); }
std::wstring folderOf(const std::wstring& p) { const size_t c = p.find_last_of(L"\\/"); return c == std::wstring::npos ? L"" : p.substr(0, c); }

void paint(HWND w, State& s) {
    PAINTSTRUCT ps;
    HDC screen = BeginPaint(w, &ps);
    RECT rc; GetClientRect(w, &rc);
    PaintBuf buf(screen, rc, ps.rcPaint);
    HDC dc = buf.dc;
    const Palette& P = pal();
    HBRUSH bg = CreateSolidBrush(P.view);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    const int W = rc.right, H = rc.bottom;
    const int cw = std::max(px(s, 200), std::min(px(s, 640), W - px(s, 48)));
    const int x0 = (W - cw) / 2;
    const int titleH = lineHeight(dc, s.title), subH = lineHeight(dc, s.sub), bodyH = lineHeight(dc, s.body);
    const int smallH = lineHeight(dc, s.small), headH = lineHeight(dc, s.heading);
    // On a short window the drop area and the gaps give way first, so a few recent files stay in view; on a very short
    // one the subtitle and the hint under the drop area go too.
    const int rowH = px(s, 48);
    const int wantRows = std::min(static_cast<int>(s.recent.size()), 3);
    const int listMin = s.recent.empty() ? bodyH : wantRows * rowH;
    bool lines = true;
    auto fixedFor = [&](int zone, int gap, bool withLines) {
        return titleH + (withLines ? px(s, 6) + subH : 0) + gap + zone + (withLines ? px(s, 10) + smallH : 0) + gap + headH + px(s, 10);
    };
    int zoneH = px(s, 190), gap = px(s, 30);
    const int avail = H - px(s, 32);
    if (fixedFor(zoneH, gap, true) + listMin > avail) {
        gap = px(s, 16);
        zoneH = std::max(px(s, 120), avail - listMin - fixedFor(0, gap, true));
        if (fixedFor(zoneH, gap, true) + listMin > avail) lines = false;
    }
    const int fixed = fixedFor(zoneH, gap, lines);
    int rows = static_cast<int>(s.recent.size());
    rows = std::max(0, std::min({rows, 8, (avail - fixed) / rowH}));
    const int listH = s.recent.empty() ? bodyH : rows * rowH;
    int y = std::max(px(s, 16), (H - fixed - listH) / 2);

    text(dc, s.title, P.text, L"Feather PDF", {x0, y, x0 + cw, y + titleH}, DT_CENTER | DT_VCENTER);
    y += titleH;
    if (lines) {
        y += px(s, 6);
        text(dc, s.sub, P.textDim, L"Read, edit, sign and print PDF documents \x2014 free, on your own PC.", {x0, y, x0 + cw, y + subH}, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
        y += subH;
    }
    y += gap;

    // the drop area, with the Open PDF button in it
    s.zone = {x0, y, x0 + cw, y + zoneH};
    {
        const bool hot = s.hover == H_ZONE || s.hover == H_BUTTON;
        roundBox(dc, s.zone, px(s, 14), hot ? mix(P.view, P.accent, 18) : mix(P.view, P.text, 8), nullptr);
        LOGBRUSH lb{BS_SOLID, hot ? P.accent : mix(P.view, P.text, 90), 0};
        const DWORD dash[2] = {static_cast<DWORD>(px(s, 6)), static_cast<DWORD>(px(s, 4))};
        HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, std::max(1, px(s, 2) - 1 + (s.dpi >= 144)), &lb, 2, dash);
        HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, s.zone.left, s.zone.top, s.zone.right, s.zone.bottom, px(s, 14), px(s, 14));
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(pen);
        const int ic = px(s, 40);
        int zy = s.zone.top + std::max(px(s, 8), (zoneH - (ic + px(s, 8) + bodyH + px(s, 14) + px(s, 38))) / 2);   // the contents in the middle
        icons::draw(dc, RECT{x0, zy, x0 + cw, zy + ic}, icons::Drop, hot ? P.accent : P.textDim, ic);
        zy += ic + px(s, 8);
        text(dc, s.body, P.text, L"Drag and drop PDF files here", {x0, zy, x0 + cw, zy + bodyH}, DT_CENTER | DT_VCENTER);
        zy += bodyH + px(s, 14);
        const int bw = px(s, 176), bh = px(s, 38);
        s.button = {(W - bw) / 2, zy, (W + bw) / 2, zy + bh};
        const COLORREF accent = s.hover == H_BUTTON ? mix(P.accent, RGB(255, 255, 255), s.pressed == H_BUTTON ? 0 : 40) : P.accent;
        roundBox(dc, s.button, px(s, 8), accent, nullptr);
        const int bi = px(s, 20);
        SIZE ts; HGDIOBJ of = SelectObject(dc, s.strong);
        GetTextExtentPoint32W(dc, L"Open PDF\x2026", 9, &ts);
        SelectObject(dc, of);
        const int gx = (W - (bi + px(s, 8) + ts.cx)) / 2;
        icons::draw(dc, RECT{gx, s.button.top, gx + bi, s.button.bottom}, icons::Open, RGB(255, 255, 255), bi);
        text(dc, s.strong, RGB(255, 255, 255), L"Open PDF\x2026", {gx + bi + px(s, 8), s.button.top, s.button.right, s.button.bottom}, DT_LEFT | DT_VCENTER);
    }
    y += zoneH;
    if (lines) {
        y += px(s, 10);
        text(dc, s.small, P.textDim, L"Ctrl+O opens a file  \x00B7  text (.txt) and Markdown (.md) files become new PDFs",
             {x0, y, x0 + cw, y + smallH}, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);
        y += smallH;
    }
    y += gap;

    // recent files
    text(dc, s.heading, P.text, L"Recent files", {x0, y, x0 + cw, y + headH}, DT_LEFT | DT_VCENTER);
    s.clear = RECT{};
    if (!s.recent.empty()) {
        SIZE cs; HGDIOBJ of = SelectObject(dc, s.small);
        GetTextExtentPoint32W(dc, L"Clear list", 10, &cs);
        SelectObject(dc, of);
        s.clear = {x0 + cw - cs.cx - px(s, 8), y, x0 + cw, y + headH};
        text(dc, s.small, s.hover == H_CLEAR ? P.accent : P.textDim, L"Clear list", s.clear, DT_RIGHT | DT_VCENTER);
    }
    y += headH + px(s, 10);
    HPEN sep = CreatePen(PS_SOLID, 1, P.line);
    HGDIOBJ oldPen = SelectObject(dc, sep);
    MoveToEx(dc, x0, y - px(s, 5), nullptr); LineTo(dc, x0 + cw, y - px(s, 5));
    SelectObject(dc, oldPen); DeleteObject(sep);
    s.rows.clear();
    if (s.recent.empty())
        text(dc, s.body, P.textDim, L"The files you open will be listed here.", {x0, y + px(s, 6), x0 + cw, y + px(s, 6) + bodyH}, DT_LEFT | DT_VCENTER);
    for (int i = 0; i < rows; ++i) {
        RECT r{x0, y, x0 + cw, y + rowH - px(s, 2)};
        s.rows.push_back(r);
        if (s.hover == H_ROW + i) roundBox(dc, r, px(s, 8), P.hover, nullptr);
        const bool gone = static_cast<size_t>(i) < s.missing.size() && s.missing[i];
        const int ic = px(s, 24);
        icons::draw(dc, RECT{r.left + px(s, 10), r.top, r.left + px(s, 10) + ic, r.bottom}, icons::File, gone ? P.textOff : P.accent, ic);
        const int tx = r.left + px(s, 10) + ic + px(s, 12), mid = (r.top + r.bottom) / 2;
        text(dc, s.body, gone ? P.textOff : P.text, nameOf(s.recent[i]) + (gone ? L"  (not found)" : L""), {tx, mid - bodyH, r.right - px(s, 10), mid + px(s, 1)},
             DT_LEFT | DT_BOTTOM | DT_END_ELLIPSIS);
        text(dc, s.small, P.textDim, folderOf(s.recent[i]), {tx, mid + px(s, 1), r.right - px(s, 10), mid + px(s, 1) + smallH}, DT_LEFT | DT_TOP | DT_PATH_ELLIPSIS);
        y += rowH;
    }
    buf.done();
    EndPaint(w, &ps);
}

int hitTest(const State& s, int x, int y) {
    auto in = [&](const RECT& r) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; };
    for (size_t i = 0; i < s.rows.size(); ++i) if (in(s.rows[i])) return H_ROW + static_cast<int>(i);
    if (in(s.button)) return H_BUTTON;
    if (in(s.zone)) return H_ZONE;
    if (!IsRectEmpty(&s.clear) && in(s.clear)) return H_CLEAR;
    return H_NONE;
}

void setHover(HWND w, State& s, int h) {
    if (h == s.hover) return;
    s.hover = h;
    InvalidateRect(w, nullptr, FALSE);
    TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, w, 0};
    TrackMouseEvent(&tme);
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    State* s = stateOf(w);
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
            break;
        case WM_NCDESTROY:
            if (s) { freeFonts(*s); delete s; SetWindowLongPtrW(w, GWLP_USERDATA, 0); }
            break;
        case WM_ERASEBKGND: return 1;
        case WM_MEASUREITEM: if (themeui::measureMenu(reinterpret_cast<MEASUREITEMSTRUCT*>(lp))) return TRUE; break;
        case WM_DRAWITEM: if (themeui::drawMenu(reinterpret_cast<DRAWITEMSTRUCT*>(lp))) return TRUE; break;
        case WM_PAINT: if (s) { paint(w, *s); return 0; } break;
        case WM_SIZE: InvalidateRect(w, nullptr, FALSE); return 0;
        case WM_MOUSEMOVE: if (s) setHover(w, *s, hitTest(*s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp))); return 0;
        case WM_MOUSELEAVE: if (s) setHover(w, *s, H_NONE); return 0;
        case WM_SETCURSOR:
            if (s && LOWORD(lp) == HTCLIENT && s->hover != H_NONE) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
            break;
        case WM_LBUTTONDOWN:
            if (!s) return 0;
            SetFocus(w);
            s->pressed = hitTest(*s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (s->pressed != H_NONE) { SetCapture(w); InvalidateRect(w, nullptr, FALSE); }
            return 0;
        case WM_LBUTTONUP: {
            if (!s) return 0;
            const int pressed = s->pressed;
            s->pressed = H_NONE;
            if (GetCapture() == w) ReleaseCapture();
            const int h = hitTest(*s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            InvalidateRect(w, nullptr, FALSE);
            if (pressed == H_NONE || h != pressed) return 0;   // released somewhere else: nothing happens
            const Actions act = s->act;                        // an action may open a document and hide this page
            if (h == H_ZONE || h == H_BUTTON) { if (act.open) act.open(); }
            else if (h == H_CLEAR) { if (act.clearAll) act.clearAll(); }
            else if (h >= H_ROW) { if (act.openRecent) act.openRecent(static_cast<size_t>(h - H_ROW)); }
            return 0;
        }
        case WM_RBUTTONUP: {
            if (!s) return 0;
            const int h = hitTest(*s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (h < H_ROW) return 0;
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"Open");
            AppendMenuW(menu, MF_STRING, 2, L"Remove from the list");
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(w, &pt);
            themeui::prepareMenu(menu);
            const int cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, w, nullptr);
            themeui::releaseMenus();
            DestroyMenu(menu);
            const Actions act = s->act;
            const size_t i = static_cast<size_t>(h - H_ROW);
            if (cmd == 1 && act.openRecent) act.openRecent(i);
            else if (cmd == 2 && act.forget) act.forget(i);
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (s && s->pressed != H_NONE && reinterpret_cast<HWND>(lp) != w) { s->pressed = H_NONE; InvalidateRect(w, nullptr, FALSE); }
            break;
        case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_CHAR:
            return SendMessageW(GetParent(w), m, wp, lp);   // shortcuts (Ctrl+O, Ctrl+K, F1...) live in the main window
    }
    return DefWindowProcW(w, m, wp, lp);
}

} // namespace

void registerClass(HINSTANCE inst) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FeatherStart";
    RegisterClassExW(&wc);
}

HWND create(HWND parent, HINSTANCE inst, Actions actions) {
    auto* s = new State;
    s->act = std::move(actions);
    makeFonts(*s);
    HWND w = CreateWindowExW(0, L"FeatherStart", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 0, 0, parent, nullptr, inst, s);
    if (!w) { freeFonts(*s); delete s; }
    return w;
}

void setRecent(HWND w, const std::vector<std::wstring>& paths) {
    State* s = stateOf(w);
    if (!s) return;
    s->recent = paths;
    s->missing.clear();
    for (const auto& p : paths) {
        // a disconnected network drive can take seconds to answer: such files are not checked here (opening one says so)
        const bool network = p.rfind(L"\\\\", 0) == 0 || (p.size() > 2 && p[1] == L':' && GetDriveTypeW((p.substr(0, 2) + L"\\").c_str()) == DRIVE_REMOTE);
        s->missing.push_back(!network && GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES);
    }
    s->hover = H_NONE;
    InvalidateRect(w, nullptr, FALSE);
}

void setDpi(HWND w, int dpi) {
    State* s = stateOf(w);
    if (!s || dpi <= 0 || dpi == s->dpi) return;
    s->dpi = dpi;
    makeFonts(*s);
    InvalidateRect(w, nullptr, FALSE);
}

void themeChanged(HWND w) { InvalidateRect(w, nullptr, FALSE); }

} // namespace startpage
