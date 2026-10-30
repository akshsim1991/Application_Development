#include "tableeditor.hpp"
#include "theme.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tableeditor {
namespace {

enum Id {
    ID_ROW_ABOVE = 101, ID_ROW_BELOW, ID_ROW_DEL, ID_COL_LEFT, ID_COL_RIGHT, ID_COL_DEL, ID_MERGE, ID_SPLIT,
    ID_ALIGN, ID_VALIGN, ID_BOLD, ID_SIZE, ID_FILL, ID_NOFILL, ID_LINES, ID_WIDTH,
    ID_LEFT, ID_TOP, ID_TW, ID_TH, ID_DELETE, ID_APPLY, ID_CANCEL, ID_GRID, ID_EDIT
};

constexpr double kMm = 25.4 / 72.0;   // points to millimetres

struct State {
    tables::Table t;
    double pageW = 0, pageH = 0;
    int dpi = 96;
    HWND wnd = nullptr, grid = nullptr, edit = nullptr;
    HWND align = nullptr, valign = nullptr, bold = nullptr, size = nullptr, lines = nullptr, width = nullptr;
    HWND left = nullptr, top = nullptr, tw = nullptr, th = nullptr, hint = nullptr;
    HFONT font = nullptr;
    int r0 = 0, c0 = 0, r1 = 0, c1 = 0;   // the selection, corners as clicked
    int editR = -1, editC = -1;
    bool selecting = false;
    int dragCol = -1, dragRow = -1;          // a column or row edge being dragged
    bool syncing = false;                    // the fields are being filled in: their change messages are not edits
    bool done = false;
    Result result = Result::Cancel;
    // the grid's scale and origin (pixels per point, where the table's top-left corner is drawn)
    double k = 1;
    int ox = 0, oy = 0;
};

int px(const State& s, int v) { return MulDiv(v, s.dpi, 96); }

// ---- selection ----

void selBox(const State& s, int& ra, int& ca, int& rb, int& cb) {
    ra = std::min(s.r0, s.r1); rb = std::max(s.r0, s.r1);
    ca = std::min(s.c0, s.c1); cb = std::max(s.c0, s.c1);
    // grow until it holds every merged block it touches whole
    for (bool grew = true; grew;) {
        grew = false;
        for (int r = ra; r <= rb; ++r)
            for (int c = ca; c <= cb; ++c) {
                int ar, ac;
                tables::anchorOf(s.t, r, c, ar, ac);
                const tables::Cell& a = s.t.at(ar, ac);
                const int er = ar + a.rowSpan - 1, ec = ac + a.colSpan - 1;
                if (ar < ra) { ra = ar; grew = true; }
                if (ac < ca) { ca = ac; grew = true; }
                if (er > rb) { rb = er; grew = true; }
                if (ec > cb) { cb = ec; grew = true; }
            }
    }
}

tables::Cell& anchorCell(State& s) {
    int ar, ac;
    tables::anchorOf(s.t, std::min(s.r0, s.t.rows() - 1), std::min(s.c0, s.t.cols() - 1), ar, ac);
    return s.t.at(ar, ac);
}

template <typename Fn>
void forSelected(State& s, Fn fn) {
    int ra, ca, rb, cb;
    selBox(s, ra, ca, rb, cb);
    for (int r = ra; r <= rb; ++r)
        for (int c = ca; c <= cb; ++c)
            if (!s.t.at(r, c).covered) fn(s.t.at(r, c));
}

void clampSel(State& s) {
    s.r0 = std::clamp(s.r0, 0, s.t.rows() - 1); s.r1 = std::clamp(s.r1, 0, s.t.rows() - 1);
    s.c0 = std::clamp(s.c0, 0, s.t.cols() - 1); s.c1 = std::clamp(s.c1, 0, s.t.cols() - 1);
}

// ---- the fields follow the table and the selection ----

std::wstring num(double v) {
    wchar_t b[32];
    swprintf(b, 32, L"%.1f", v);
    return b;
}

void syncFields(State& s) {
    s.syncing = true;
    const tables::Cell& a = anchorCell(s);
    SendMessageW(s.align, CB_SETCURSEL, a.align, 0);
    SendMessageW(s.valign, CB_SETCURSEL, a.valign, 0);
    SendMessageW(s.bold, BM_SETCHECK, a.bold ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(s.size, num(a.size).c_str());
    SendMessageW(s.lines, CB_SETCURSEL, static_cast<int>(tables::bordersOf(s.t)), 0);
    SetWindowTextW(s.width, num(s.t.lineWidth).c_str());
    SetWindowTextW(s.left, num(s.t.left() * kMm).c_str());
    SetWindowTextW(s.top, num((s.pageH - s.t.top()) * kMm).c_str());
    SetWindowTextW(s.tw, num((s.t.right() - s.t.left()) * kMm).c_str());
    SetWindowTextW(s.th, num((s.t.top() - s.t.bottom()) * kMm).c_str());
    s.syncing = false;
}

double fieldValue(HWND h, double fallback) {
    wchar_t b[32] = L"";
    GetWindowTextW(h, b, 32);
    for (wchar_t& c : b) if (c == L',') c = L'.';
    wchar_t* end = nullptr;
    const double v = wcstod(b, &end);
    return end != b ? v : fallback;
}

// ---- the grid ----

void layoutGrid(State& s) {
    RECT rc; GetClientRect(s.grid, &rc);
    const double W = s.t.right() - s.t.left(), H = s.t.top() - s.t.bottom();
    const int m = px(s, 16);
    s.k = std::max(0.2, std::min({(rc.right - 2.0 * m) / std::max(1.0, W), (rc.bottom - 2.0 * m) / std::max(1.0, H), 2.5 * s.dpi / 96.0}));
    s.ox = static_cast<int>((rc.right - W * s.k) / 2);
    s.oy = static_cast<int>((rc.bottom - H * s.k) / 2);
}
int gx(const State& s, double x) { return s.ox + static_cast<int>(std::lround((x - s.t.left()) * s.k)); }
int gy(const State& s, double y) { return s.oy + static_cast<int>(std::lround((s.t.top() - y) * s.k)); }

RECT cellRect(const State& s, int r, int c, int rs = 1, int cs = 1) {
    return {gx(s, s.t.colX[c]), gy(s, s.t.rowY[r]), gx(s, s.t.colX[c + cs]), gy(s, s.t.rowY[r + rs])};
}

bool cellAt(const State& s, int x, int y, int& r, int& c) {
    r = c = -1;
    for (int i = 0; i < s.t.cols(); ++i) if (x >= gx(s, s.t.colX[i]) && x < gx(s, s.t.colX[i + 1])) c = i;
    for (int i = 0; i < s.t.rows(); ++i) if (y >= gy(s, s.t.rowY[i]) && y < gy(s, s.t.rowY[i + 1])) r = i;
    return r >= 0 && c >= 0;
}

// an inner column or row edge near the point (to drag), else -1
int colEdgeAt(const State& s, int x, int y) {
    if (y < gy(s, s.t.top()) - 4 || y > gy(s, s.t.bottom()) + 4) return -1;
    for (int i = 1; i <= s.t.cols(); ++i) if (std::abs(x - gx(s, s.t.colX[i])) <= px(s, 4)) return i;
    return -1;
}
int rowEdgeAt(const State& s, int x, int y) {
    if (x < gx(s, s.t.left()) - 4 || x > gx(s, s.t.right()) + 4) return -1;
    for (int i = 1; i <= s.t.rows(); ++i) if (std::abs(y - gy(s, s.t.rowY[i])) <= px(s, 4)) return i;
    return -1;
}

void paintGrid(State& s) {
    PAINTSTRUCT ps;
    HDC screen = BeginPaint(s.grid, &ps);
    RECT rc; GetClientRect(s.grid, &rc);
    PaintBuf buf(screen, rc, ps.rcPaint);
    HDC dc = buf.dc;
    HBRUSH bg = CreateSolidBrush(pal().view);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    layoutGrid(s);
    const tables::Table& t = s.t;
    RECT page{gx(s, t.left()) - px(s, 10), gy(s, t.top()) - px(s, 10), gx(s, t.right()) + px(s, 10), gy(s, t.bottom()) + px(s, 10)};
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &page, white);
    DeleteObject(white);
    SetBkMode(dc, TRANSPARENT);
    int ra, ca, rb, cb;
    selBox(s, ra, ca, rb, cb);
    for (int r = 0; r < t.rows(); ++r)
        for (int c = 0; c < t.cols(); ++c) {
            const tables::Cell& cell = t.at(r, c);
            if (cell.covered) continue;
            RECT cr = cellRect(s, r, c, cell.rowSpan, cell.colSpan);
            if (cell.fill != tables::kNoFill) { HBRUSH f = CreateSolidBrush(cell.fill); FillRect(dc, &cr, f); DeleteObject(f); }
            if (r >= ra && r <= rb && c >= ca && c <= cb) {   // selected: a blue tint
                BLENDFUNCTION bf{AC_SRC_OVER, 0, 60, 0};
                HDC mem = CreateCompatibleDC(dc);
                HBITMAP one = CreateCompatibleBitmap(dc, 1, 1);
                HGDIOBJ old = SelectObject(mem, one);
                SetPixel(mem, 0, 0, RGB(0x1F, 0x6F, 0xEB));
                AlphaBlend(dc, cr.left, cr.top, cr.right - cr.left, cr.bottom - cr.top, mem, 0, 0, 1, 1, bf);
                SelectObject(mem, old); DeleteObject(one); DeleteDC(mem);
            }
            if (r == s.editR && c == s.editC) continue;   // the edit box is over it
            LOGFONTW lf{};
            lf.lfHeight = -std::max(8, static_cast<int>(std::lround(cell.size * s.k)));
            lf.lfWeight = cell.bold ? FW_BOLD : FW_NORMAL;
            lf.lfItalic = cell.italic;
            wcscpy_s(lf.lfFaceName, cell.family == pageobj::Family::Serif ? L"Times New Roman" : cell.family == pageobj::Family::Mono ? L"Courier New" : L"Arial");
            HFONT f = CreateFontIndirectW(&lf);
            HGDIOBJ of = SelectObject(dc, f);
            SetTextColor(dc, cell.color);
            RECT tr = cr;
            InflateRect(&tr, -std::max(2, static_cast<int>(t.pad * s.k)), -std::max(1, static_cast<int>(1.5 * s.k)));
            UINT flags = DT_WORDBREAK | DT_NOPREFIX | (cell.align == 1 ? DT_CENTER : cell.align == 2 ? DT_RIGHT : DT_LEFT);
            RECT meas = tr;
            DrawTextW(dc, cell.text.c_str(), -1, &meas, flags | DT_CALCRECT);
            const int h = meas.bottom - meas.top, room = tr.bottom - tr.top;
            if (cell.valign == 1) tr.top += std::max(0, (room - h) / 2);
            else if (cell.valign == 2) tr.top += std::max(0, room - h);
            DrawTextW(dc, cell.text.c_str(), -1, &tr, flags);
            SelectObject(dc, of);
            DeleteObject(f);
        }
    // lines: the ones that will be drawn dark, the cells' other edges as faint guides
    auto edge = [&](int x0, int y0, int x1, int y1, bool drawn) {
        HPEN pen = drawn ? CreatePen(PS_SOLID, std::max(1, static_cast<int>(std::lround(t.lineWidth * s.k))), t.lineColor)
                         : CreatePen(PS_DOT, 1, RGB(0xB8, 0xBE, 0xC8));
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, x0, y0, nullptr); LineTo(dc, x1, y1);
        SelectObject(dc, op); DeleteObject(pen);
    };
    auto same = [&](int r0, int c0, int r1, int c1) {
        int a, b, e, f;
        tables::anchorOf(t, r0, c0, a, b); tables::anchorOf(t, r1, c1, e, f);
        return a == e && b == f;
    };
    for (int r = 0; r <= t.rows(); ++r)
        for (int c = 0; c < t.cols(); ++c) {
            if (r > 0 && r < t.rows() && same(r - 1, c, r, c)) continue;
            edge(gx(s, t.colX[c]), gy(s, t.rowY[r]), gx(s, t.colX[c + 1]), gy(s, t.rowY[r]), t.hRule[r] != 0);
        }
    for (int c = 0; c <= t.cols(); ++c)
        for (int r = 0; r < t.rows(); ++r) {
            if (c > 0 && c < t.cols() && same(r, c - 1, r, c)) continue;
            edge(gx(s, t.colX[c]), gy(s, t.rowY[r]), gx(s, t.colX[c]), gy(s, t.rowY[r + 1]), t.vRule[c] != 0);
        }
    // the selection's frame
    RECT sel{gx(s, t.colX[ca]), gy(s, t.rowY[ra]), gx(s, t.colX[cb + 1]), gy(s, t.rowY[rb + 1])};
    HPEN sp = CreatePen(PS_SOLID, 2, RGB(0x1F, 0x6F, 0xEB));
    HGDIOBJ op = SelectObject(dc, sp), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, sel.left, sel.top, sel.right + 1, sel.bottom + 1);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(sp);
    buf.done();
    EndPaint(s.grid, &ps);
}

void changed(State& s) {
    clampSel(s);
    syncFields(s);
    InvalidateRect(s.grid, nullptr, FALSE);
}

// ---- typing into a cell ----

void endEdit(State& s, bool keep) {
    if (!s.edit) return;
    HWND e = s.edit;
    s.edit = nullptr;
    if (keep && s.editR >= 0) {
        const int n = GetWindowTextLengthW(e);
        std::wstring raw(static_cast<size_t>(n) + 1, L'\0');
        GetWindowTextW(e, raw.data(), n + 1);
        raw.resize(static_cast<size_t>(n));
        std::wstring text;
        for (wchar_t c : raw) if (c != L'\r') text += c;
        s.t.at(s.editR, s.editC).text = text;
    }
    s.editR = s.editC = -1;
    DestroyWindow(e);
    SetFocus(s.grid);
    InvalidateRect(s.grid, nullptr, FALSE);
}

LRESULT CALLBACK editProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    State& s = *reinterpret_cast<State*>(data);
    if (m == WM_KEYDOWN) {
        const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
        if (w == VK_ESCAPE) { endEdit(s, false); return 0; }
        if (w == VK_RETURN && !ctrl && !shift) { const int r = s.editR; endEdit(s, true); s.r0 = s.r1 = std::min(r + 1, s.t.rows() - 1); changed(s); return 0; }
        if (w == VK_TAB) { const int c = s.editC; endEdit(s, true); s.c0 = s.c1 = std::min(c + (shift ? -1 : 1), s.t.cols() - 1); if (s.c0 < 0) s.c0 = s.c1 = 0; changed(s); return 0; }
    }
    if (m == WM_CHAR && (w == L'\t' || (w == L'\r' && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_SHIFT) >= 0) || w == 27)) return 0;
    if (m == WM_KILLFOCUS && s.edit == h) { DefSubclassProc(h, m, w, l); PostMessageW(s.wnd, WM_APP + 7, 0, 0); return 0; }
    return DefSubclassProc(h, m, w, l);
}

void beginEdit(State& s, const wchar_t* first) {
    endEdit(s, true);
    int ar, ac;
    tables::anchorOf(s.t, s.r0, s.c0, ar, ac);
    const tables::Cell& cell = s.t.at(ar, ac);
    s.r0 = s.r1 = ar; s.c0 = s.c1 = ac;
    s.editR = ar; s.editC = ac;
    RECT cr = cellRect(s, ar, ac, cell.rowSpan, cell.colSpan);
    const int minW = px(s, 160), minH = px(s, 48);
    if (cr.right - cr.left < minW) cr.right = cr.left + minW;
    if (cr.bottom - cr.top < minH) cr.bottom = cr.top + minH;
    s.edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                             cr.left, cr.top, cr.right - cr.left, cr.bottom - cr.top, s.grid, reinterpret_cast<HMENU>(ID_EDIT), nullptr, nullptr);
    SendMessageW(s.edit, WM_SETFONT, reinterpret_cast<WPARAM>(s.font), TRUE);
    std::wstring t;
    for (wchar_t c : (first ? std::wstring(first) : cell.text)) { if (c == L'\n') t += L'\r'; t += c; }
    SetWindowTextW(s.edit, t.c_str());
    SetWindowSubclass(s.edit, editProc, 1, reinterpret_cast<DWORD_PTR>(&s));
    SetFocus(s.edit);
    SendMessageW(s.edit, EM_SETSEL, first ? t.size() : 0, first ? t.size() : -1);
    InvalidateRect(s.grid, nullptr, FALSE);
}

LRESULT CALLBACK gridProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    State* sp = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) { SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); return TRUE; }
    if (!sp) return DefWindowProcW(h, m, w, l);
    State& s = *sp;
    switch (m) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: paintGrid(s); return 0;
        case WM_SIZE: InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_SETCURSOR: {
            POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
            if (colEdgeAt(s, p.x, p.y) > 0) { SetCursor(LoadCursor(nullptr, IDC_SIZEWE)); return TRUE; }
            if (rowEdgeAt(s, p.x, p.y) > 0) { SetCursor(LoadCursor(nullptr, IDC_SIZENS)); return TRUE; }
            break;
        }
        case WM_LBUTTONDOWN: {
            endEdit(s, true);
            SetFocus(h);
            const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            s.dragCol = colEdgeAt(s, x, y);
            s.dragRow = s.dragCol > 0 ? -1 : rowEdgeAt(s, x, y);
            if (s.dragCol > 0 || s.dragRow > 0) { SetCapture(h); return 0; }
            int r, c;
            if (!cellAt(s, x, y, r, c)) return 0;
            if (GetKeyState(VK_SHIFT) < 0) { s.r1 = r; s.c1 = c; }
            else { s.r0 = s.r1 = r; s.c0 = s.c1 = c; }
            s.selecting = true;
            SetCapture(h);
            changed(s);
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            if (s.dragCol > 0) {   // the edge follows the pointer; the columns on either side share the change
                const int c = s.dragCol;
                const double pos = s.t.left() + (x - s.ox) / s.k;
                const double lo = s.t.colX[c - 1] + 6, hi = c < s.t.cols() ? s.t.colX[c + 1] - 6 : 1e9;
                if (c == s.t.cols()) s.t.colX[c] = std::max(lo, pos);
                else s.t.colX[c] = std::clamp(pos, lo, hi);
                syncFields(s);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if (s.dragRow > 0) {
                const int r = s.dragRow;
                const double pos = s.t.top() - (y - s.oy) / s.k;
                const double hi = s.t.rowY[r - 1] - 6, lo = r < s.t.rows() ? s.t.rowY[r + 1] + 6 : -1e9;
                if (r == s.t.rows()) s.t.rowY[r] = std::min(hi, pos);
                else s.t.rowY[r] = std::clamp(pos, lo, hi);
                syncFields(s);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if (!s.selecting) return 0;
            int r, c;
            if (cellAt(s, x, y, r, c) && (r != s.r1 || c != s.c1)) { s.r1 = r; s.c1 = c; changed(s); }
            return 0;
        }
        case WM_LBUTTONUP:
            s.selecting = false;
            s.dragCol = s.dragRow = -1;
            if (GetCapture() == h) ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK: beginEdit(s, nullptr); return 0;
        case WM_KEYDOWN: {
            const bool shift = GetKeyState(VK_SHIFT) < 0;
            int dr = 0, dc = 0;
            switch (w) {
                case VK_UP: dr = -1; break;
                case VK_DOWN: dr = 1; break;
                case VK_LEFT: dc = -1; break;
                case VK_RIGHT: case VK_TAB: dc = 1; break;
                case VK_F2: case VK_RETURN: beginEdit(s, nullptr); return 0;
                case VK_DELETE: forSelected(s, [](tables::Cell& c) { c.text.clear(); }); changed(s); return 0;
                default: return 0;
            }
            if (shift) { s.r1 += dr; s.c1 += dc; } else { s.r0 = s.r1 = s.r0 + dr; s.c0 = s.c1 = s.c0 + dc; }
            changed(s);
            return 0;
        }
        case WM_CHAR:
            if (w >= 32) { const wchar_t first[2] = {static_cast<wchar_t>(w), 0}; beginEdit(s, first); }
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---- the window ----

HWND make(State& s, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, px(s, x), px(s, y), px(s, w), px(s, h), s.wnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(s.font), TRUE);
    return c;
}

void build(State& s) {
    int x = 12;
    auto button = [&](const wchar_t* t, int id, int w) { make(s, L"BUTTON", t, WS_TABSTOP | BS_PUSHBUTTON, x, 10, w, 28, id); x += w + 4; };
    button(L"Row above", ID_ROW_ABOVE, 84); button(L"Row below", ID_ROW_BELOW, 84); button(L"Delete row", ID_ROW_DEL, 84); x += 10;
    button(L"Column left", ID_COL_LEFT, 92); button(L"Column right", ID_COL_RIGHT, 96); button(L"Delete column", ID_COL_DEL, 104); x += 10;
    button(L"Merge cells", ID_MERGE, 92); button(L"Split cell", ID_SPLIT, 80);
    // the second row: the selected cells' look, and the lines
    x = 12;
    auto label = [&](const wchar_t* t, int w) { make(s, L"STATIC", t, SS_LEFT | SS_CENTERIMAGE, x, 46, w, 26, 0); x += w; };
    auto combo = [&](int id, int w, std::initializer_list<const wchar_t*> items) {
        HWND c = make(s, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, x, 46, w, 200, id);
        for (const wchar_t* i : items) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(i));
        x += w + 10;
        return c;
    };
    label(L"Align", 40); s.align = combo(ID_ALIGN, 80, {L"Left", L"Centre", L"Right"});
    label(L"Vertical", 52); s.valign = combo(ID_VALIGN, 80, {L"Top", L"Middle", L"Bottom"});
    s.bold = make(s, L"BUTTON", L"Bold", WS_TABSTOP | BS_AUTOCHECKBOX, x, 46, 54, 26, ID_BOLD); x += 60;
    label(L"Size", 32); s.size = make(s, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, x, 48, 44, 22, ID_SIZE, WS_EX_CLIENTEDGE); x += 54;
    make(s, L"BUTTON", L"Cell colour\x2026", WS_TABSTOP | BS_PUSHBUTTON, x, 46, 96, 26, ID_FILL); x += 100;
    make(s, L"BUTTON", L"No colour", WS_TABSTOP | BS_PUSHBUTTON, x, 46, 80, 26, ID_NOFILL); x += 94;
    label(L"Lines", 38); s.lines = combo(ID_LINES, 120, {L"All lines", L"Outside only", L"Rows only", L"None"});
    label(L"Width", 40); s.width = make(s, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, x, 48, 44, 22, ID_WIDTH, WS_EX_CLIENTEDGE);
    s.hint = make(s, L"STATIC", L"Double-click a cell or type to change its text (Enter: done, Ctrl+Enter: new line). Drag across cells to "
                                L"select several; drag a grid line to resize a column or row.", SS_LEFT, 12, 80, 900, 20, 0);
}

void layout(State& s) {
    RECT rc; GetClientRect(s.wnd, &rc);
    const int bottom = rc.bottom - px(s, 44);
    MoveWindow(s.hint, px(s, 12), px(s, 80), rc.right - px(s, 24), px(s, 20), TRUE);
    MoveWindow(s.grid, px(s, 12), px(s, 104), rc.right - px(s, 24), bottom - px(s, 112), TRUE);
    // the bottom row: position and size, delete, cancel, apply
    int x = px(s, 12);
    const int y = bottom + px(s, 8);
    const struct { int label, lw; HWND field; } row[] = {{1000, 70, s.left}, {1001, 34, s.top}, {1002, 46, s.tw}, {1003, 50, s.th}};
    for (const auto& it : row) {
        MoveWindow(GetDlgItem(s.wnd, it.label), x, y, px(s, it.lw), px(s, 26), TRUE);
        x += px(s, it.lw);
        MoveWindow(it.field, x, y + px(s, 2), px(s, 52), px(s, 22), TRUE);
        x += px(s, 64);
    }
    MoveWindow(GetDlgItem(s.wnd, ID_DELETE), x + px(s, 16), y, px(s, 100), px(s, 28), TRUE);
    MoveWindow(GetDlgItem(s.wnd, ID_CANCEL), rc.right - px(s, 196), y, px(s, 88), px(s, 28), TRUE);
    MoveWindow(GetDlgItem(s.wnd, ID_APPLY), rc.right - px(s, 100), y, px(s, 88), px(s, 28), TRUE);
}

void command(State& s, int id, int code) {
    if (s.syncing) return;
    int ra, ca, rb, cb;
    selBox(s, ra, ca, rb, cb);
    switch (id) {
        case ID_ROW_ABOVE: endEdit(s, true); tables::insertRow(s.t, ra); s.r0 = s.r1 = ra; break;
        case ID_ROW_BELOW: endEdit(s, true); tables::insertRow(s.t, rb + 1); s.r0 = s.r1 = rb + 1; break;
        case ID_ROW_DEL: endEdit(s, true); if (s.t.rows() - (rb - ra + 1) < 1) { MessageBeep(MB_ICONWARNING); return; } for (int r = rb; r >= ra; --r) tables::deleteRow(s.t, r); break;
        case ID_COL_LEFT: endEdit(s, true); tables::insertColumn(s.t, ca); s.c0 = s.c1 = ca; break;
        case ID_COL_RIGHT: endEdit(s, true); tables::insertColumn(s.t, cb + 1); s.c0 = s.c1 = cb + 1; break;
        case ID_COL_DEL: endEdit(s, true); if (s.t.cols() - (cb - ca + 1) < 1) { MessageBeep(MB_ICONWARNING); return; } for (int c = cb; c >= ca; --c) tables::deleteColumn(s.t, c); break;
        case ID_MERGE:
            endEdit(s, true);
            if (!tables::merge(s.t, ra, ca, rb, cb)) { MessageBeep(MB_ICONWARNING); return; }
            s.r0 = ra; s.c0 = ca; s.r1 = rb; s.c1 = cb;
            break;
        case ID_SPLIT: endEdit(s, true); for (int r = ra; r <= rb; ++r) for (int c = ca; c <= cb; ++c) if (!s.t.at(r, c).covered) tables::split(s.t, r, c); break;
        case ID_ALIGN: if (code != CBN_SELCHANGE) return; { const int v = static_cast<int>(SendMessageW(s.align, CB_GETCURSEL, 0, 0)); forSelected(s, [&](tables::Cell& c) { c.align = v; }); } break;
        case ID_VALIGN: if (code != CBN_SELCHANGE) return; { const int v = static_cast<int>(SendMessageW(s.valign, CB_GETCURSEL, 0, 0)); forSelected(s, [&](tables::Cell& c) { c.valign = v; }); } break;
        case ID_BOLD: { const bool b = SendMessageW(s.bold, BM_GETCHECK, 0, 0) == BST_CHECKED; forSelected(s, [&](tables::Cell& c) { c.bold = b; }); } break;
        case ID_SIZE: if (code != EN_CHANGE) return; { const double v = fieldValue(s.size, 0); if (v >= 4 && v <= 96) forSelected(s, [&](tables::Cell& c) { c.size = v; }); InvalidateRect(s.grid, nullptr, FALSE); } return;
        case ID_FILL: {
            static COLORREF custom[16] = {RGB(0xE6, 0xE6, 0xE6), RGB(0xDD, 0xEB, 0xF7), RGB(0xE2, 0xEF, 0xDA), RGB(0xFF, 0xF2, 0xCC), RGB(0xFC, 0xE4, 0xD6), RGB(0x1F, 0x38, 0x64)};
            CHOOSECOLORW cc{sizeof(cc)};
            cc.hwndOwner = s.wnd;
            cc.lpCustColors = custom;
            cc.rgbResult = anchorCell(s).fill == tables::kNoFill ? RGB(0xE6, 0xE6, 0xE6) : anchorCell(s).fill;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (!ChooseColorW(&cc)) return;
            forSelected(s, [&](tables::Cell& c) { c.fill = cc.rgbResult; });
            break;
        }
        case ID_NOFILL: forSelected(s, [](tables::Cell& c) { c.fill = tables::kNoFill; }); break;
        case ID_LINES: if (code != CBN_SELCHANGE) return; tables::setBorders(s.t, static_cast<tables::Borders>(SendMessageW(s.lines, CB_GETCURSEL, 0, 0))); break;
        case ID_WIDTH: if (code != EN_CHANGE) return; { const double v = fieldValue(s.width, 0); if (v >= 0.1 && v <= 6) s.t.lineWidth = v; InvalidateRect(s.grid, nullptr, FALSE); } return;
        case ID_LEFT: case ID_TOP: case ID_TW: case ID_TH: {
            if (code != EN_KILLFOCUS) return;   // applied when the field is left, so typing is not disturbed
            const double l = fieldValue(s.left, s.t.left() * kMm) / kMm, tp = s.pageH - fieldValue(s.top, (s.pageH - s.t.top()) * kMm) / kMm;
            const double w = fieldValue(s.tw, (s.t.right() - s.t.left()) * kMm) / kMm, h = fieldValue(s.th, (s.t.top() - s.t.bottom()) * kMm) / kMm;
            tables::resize(s.t, std::clamp(w, 10.0, s.pageW), std::clamp(h, 6.0, s.pageH));
            tables::moveTo(s.t, std::clamp(l, 0.0, s.pageW - 10), std::clamp(tp, 10.0, s.pageH));
            break;
        }
        case ID_DELETE:
            if (MessageBoxW(s.wnd, L"Delete this table from the page?", L"Edit table", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
            s.result = Result::Delete; s.done = true; return;
        case ID_APPLY: endEdit(s, true); s.result = Result::Apply; s.done = true; return;
        case ID_CANCEL: s.done = true; return;
        default: return;
    }
    changed(s);
    // after a button, the keyboard goes back to the grid, so typing (or Space / Enter) edits cells instead of pressing the button again
    if (id != ID_ALIGN && id != ID_VALIGN && id != ID_LINES && id != ID_LEFT && id != ID_TOP && id != ID_TW && id != ID_TH) SetFocus(s.grid);
}

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    State* sp = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) { SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); return DefWindowProcW(h, m, w, l); }
    if (!sp) return DefWindowProcW(h, m, w, l);
    State& s = *sp;
    switch (m) {
        case WM_COMMAND: command(s, LOWORD(w), HIWORD(w)); return 0;
        case WM_APP + 7: if (s.edit && GetFocus() != s.edit) endEdit(s, true); return 0;   // the cell being typed in lost the keyboard
        case WM_SIZE: if (s.grid) layout(s); return 0;
        case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize = {px(s, 960), px(s, 460)}; return 0;
        case WM_CTLCOLORSTATIC: SetBkMode(reinterpret_cast<HDC>(w), TRANSPARENT); return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        case WM_CLOSE: s.done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

Result run(HWND parent, HINSTANCE inst, tables::Table& t, double pageW, double pageH, int dpi) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(parent, GCLP_HICON));
        wc.lpszClassName = L"FeatherTableEditor";
        RegisterClassExW(&wc);
        WNDCLASSEXW gc{sizeof(gc)};
        gc.style = CS_DBLCLKS;
        gc.lpfnWndProc = gridProc;
        gc.hInstance = inst;
        gc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        gc.lpszClassName = L"FeatherTableGrid";
        RegisterClassExW(&gc);
        registered = true;
    }
    State s;
    s.t = t;
    s.pageW = pageW; s.pageH = pageH;
    s.dpi = dpi > 0 ? dpi : 96;
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(9, s.dpi, 72);
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    s.font = CreateFontIndirectW(&lf);
    RECT pr; GetWindowRect(parent, &pr);
    const int W = px(s, 980), H = px(s, 640);
    s.wnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FeatherTableEditor", L"Edit table", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN,
                            (pr.left + pr.right - W) / 2, std::max<int>(pr.top, (pr.top + pr.bottom - H) / 2), W, H, parent, nullptr, inst, &s);
    if (!s.wnd) { DeleteObject(s.font); return Result::Cancel; }
    build(s);
    s.grid = CreateWindowExW(WS_EX_CLIENTEDGE, L"FeatherTableGrid", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10, s.wnd,
                             reinterpret_cast<HMENU>(ID_GRID), inst, &s);
    // the bottom row
    make(s, L"STATIC", L"Left (mm)", SS_LEFT | SS_CENTERIMAGE, 0, 0, 10, 10, 1000);
    s.left = make(s, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, ID_LEFT, WS_EX_CLIENTEDGE);
    make(s, L"STATIC", L"Top", SS_LEFT | SS_CENTERIMAGE, 0, 0, 10, 10, 1001);
    s.top = make(s, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, ID_TOP, WS_EX_CLIENTEDGE);
    make(s, L"STATIC", L"Width", SS_LEFT | SS_CENTERIMAGE, 0, 0, 10, 10, 1002);
    s.tw = make(s, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, ID_TW, WS_EX_CLIENTEDGE);
    make(s, L"STATIC", L"Height", SS_LEFT | SS_CENTERIMAGE, 0, 0, 10, 10, 1003);
    s.th = make(s, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, ID_TH, WS_EX_CLIENTEDGE);
    make(s, L"BUTTON", L"Delete table", WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, ID_DELETE);
    make(s, L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, ID_CANCEL);
    make(s, L"BUTTON", L"Apply", WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 10, 10, ID_APPLY);
    layout(s);
    syncFields(s);
    EnableWindow(parent, FALSE);
    ShowWindow(s.wnd, SW_SHOW);
    SetFocus(s.grid);
    MSG msg;
    while (!s.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && !s.edit) { s.done = true; continue; }
        if (s.edit && msg.hwnd == s.edit) { TranslateMessage(&msg); DispatchMessageW(&msg); continue; }   // the cell's own keys
        if (msg.hwnd == s.grid || IsChild(s.grid, msg.hwnd)) { TranslateMessage(&msg); DispatchMessageW(&msg); continue; }
        if (!IsDialogMessageW(s.wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(s.wnd);
    DeleteObject(s.font);
    SetForegroundWindow(parent);
    if (s.result == Result::Apply) t = s.t;
    return s.result;
}

} // namespace tableeditor
