#include "eqeditor.hpp"
#include "equation.hpp"
#include "theme.hpp"
#include "themeui.hpp"
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace eqeditor {
namespace {

enum { ID_EDIT = 100, ID_SIZE, ID_PREVIEW, ID_ERROR, ID_HINT, ID_APPLY, ID_CANCEL, ID_REMOVE, ID_BUTTON0 = 1000 };
const wchar_t* kClass = L"FeatherEquationEditor";
const wchar_t* kPreviewClass = L"FeatherEquationPreview";
const UINT_PTR kTimer = 7;
const wchar_t kSlot = L'\x0001';   // in a template: where the selection (or the caret) goes

struct Button { const wchar_t* label; const wchar_t* insert; const wchar_t* tip; };
// Structures: what they insert, the selected text (or the caret) going where kSlot is
const Button kStructures[] = {
    {L"x\x00B2", L"^{\x0001}", L"Power: s^8, e^{-st}  (Ctrl+Shift+=)"},
    {L"x\x2082", L"_{\x0001}", L"Index: x_1, a_{n+1}  (Ctrl+=)"},
    {L"x\x2082\x00B2", L"_{\x0001}^{}", L"Index and power"},
    {L"a\x2044" L"b", L"\\frac{\x0001}{}", L"Fraction: \\frac{top}{bottom}"},
    {L"\x221A", L"\\sqrt{\x0001}", L"Square root"},
    {L"\x207F\x221A", L"\\sqrt[\x0001]{}", L"Root: \\sqrt[3]{x}"},
    {L"( )", L"\\left(\x0001\\right)", L"Brackets that grow with what they hold"},
    {L"[ ]", L"\\left[\x0001\\right]", L"Square brackets"},
    {L"|x|", L"\\left|\x0001\\right|", L"Absolute value"},
    {L"\x2211", L"\\sum_{\x0001}^{}", L"Sum with limits"},
    {L"\x220F", L"\\prod_{\x0001}^{}", L"Product with limits"},
    {L"\x222B", L"\\int_{\x0001}^{}", L"Integral with limits"},
    {L"lim", L"\\lim_{\x0001}", L"Limit: \\lim_{x \\to 0}"},
    {L"sin", L"\\sin \x0001", L"Functions (sin, cos, tan, log, ln, exp) are set upright"},
    {L"abc", L"\\text{\x0001}", L"Ordinary upright text inside the equation"},
};
const wchar_t kSigns[] = L"+\x2212\x00B1\x00D7\x00B7\x00F7=\x2260\x2248\x2261\x2264\x2265<>\x221E\x2192\x21D2\x2202\x2207\x2032\x00B0\x2208\x2200\x2203\x2026";
const wchar_t kGreek[] = L"\x03B1\x03B2\x03B3\x03B4\x03B5\x03B6\x03B7\x03B8\x03BB\x03BC\x03BD\x03BE\x03C0\x03C1\x03C3\x03C4\x03C6\x03C7\x03C8\x03C9\x0393\x0394\x0398\x039B\x03A0\x03A3\x03A6\x03A8\x03A9";

struct State {
    HWND wnd = nullptr, edit = nullptr, size = nullptr, preview = nullptr, error = nullptr, hint = nullptr, tip = nullptr;
    std::vector<HWND> buttons;
    std::vector<std::wstring> inserts;
    HFONT font = nullptr, big = nullptr, small = nullptr, sym = nullptr;
    int dpi = 96;
    bool done = false, ok = false, remove = false, existing = false, tex = false;
    std::vector<unsigned char> pixels;
    int pw = 0, ph = 0;
    std::wstring err;
    Result result;
    int px(int v) const { return MulDiv(v, dpi, 96); }
};

std::wstring textOf(HWND w) {
    const int n = GetWindowTextLengthW(w);
    std::wstring t(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(w, t.data(), n + 1);
    t.resize(static_cast<size_t>(n));
    return t;
}

double sizeOf(const State& s) {
    const double v = wcstod(textOf(s.size).c_str(), nullptr);
    return v >= 4 && v <= 144 ? v : 12;
}

void render(State& s) {
    const std::wstring src = textOf(s.edit);
    s.err.clear();
    s.pixels.clear();
    s.pw = s.ph = 0;
    if (src.find_first_not_of(L" \t") != std::wstring::npos) {
        // drawn larger than on the page, to be read easily: a 12-point equation about twice its printed size
        const double scale = 2.0 * s.dpi / 72.0;
        if (!equation::preview(src, sizeOf(s), scale, s.pixels, s.pw, s.ph, &s.err, s.tex)) { s.pixels.clear(); s.pw = s.ph = 0; }
    }
    SetWindowTextW(s.error, s.err.c_str());
    EnableWindow(GetDlgItem(s.wnd, ID_APPLY), s.err.empty());
    InvalidateRect(s.preview, nullptr, FALSE);
}

// Puts a button's text in at the caret: a template wraps the selected text (the caret goes inside it otherwise).
void insert(State& s, const std::wstring& what) {
    DWORD a = 0, b = 0;
    SendMessageW(s.edit, EM_GETSEL, reinterpret_cast<WPARAM>(&a), reinterpret_cast<LPARAM>(&b));
    const std::wstring all = textOf(s.edit);
    const std::wstring sel = b > a ? all.substr(a, b - a) : std::wstring();
    const size_t slot = what.find(kSlot);
    std::wstring put = what;
    size_t caret = what.size();
    if (slot != std::wstring::npos) {
        put = what.substr(0, slot) + sel + what.substr(slot + 1);
        // with a selection, the caret goes to the next empty place (a fraction's bottom), else inside the first
        if (!sel.empty()) {
            const size_t next = put.find(L"{}", slot + sel.size());
            caret = next != std::wstring::npos ? next + 1 : put.size();
        } else caret = slot;
    }
    SendMessageW(s.edit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(put.c_str()));
    const DWORD at = a + static_cast<DWORD>(caret);
    SendMessageW(s.edit, EM_SETSEL, at, at);
    SetFocus(s.edit);
}

void layout(State& s) {
    RECT rc;
    GetClientRect(s.wnd, &rc);
    const int m = s.px(14), W = rc.right - m * 2;
    int y = m;
    auto place = [&](int id, int x, int yy, int w, int h) { MoveWindow(GetDlgItem(s.wnd, id), x, yy, w, h, FALSE); };
    place(ID_HINT + 100, m, y, W, s.px(20));
    y += s.px(22);
    MoveWindow(s.edit, m, y, W - s.px(110), s.px(34), FALSE);
    place(ID_SIZE + 100, rc.right - m - s.px(100), y + s.px(7), s.px(34), s.px(20));
    MoveWindow(s.size, rc.right - m - s.px(62), y + s.px(3), s.px(62), s.px(300), FALSE);
    y += s.px(40);
    MoveWindow(s.hint, m, y, W, s.px(34), FALSE);
    y += s.px(38);
    // the buttons, in rows that wrap
    const int bh = s.px(30);
    int x = m;
    size_t k = 0;
    const size_t groups[] = {std::size(kStructures), std::size(kSigns) - 1, std::size(kGreek) - 1};
    for (size_t g = 0; g < 3; ++g) {
        for (size_t j = 0; j < groups[g]; ++j, ++k) {
            HWND b = s.buttons[k];
            wchar_t t[16] = L"";
            GetWindowTextW(b, t, 16);
            const int bw = std::max(s.px(34), s.px(14) + static_cast<int>(wcslen(t)) * s.px(9));
            if (x + bw > m + W) { x = m; y += bh + s.px(4); }
            MoveWindow(b, x, y, bw, bh, FALSE);
            x += bw + s.px(4);
        }
        x = m;
        y += bh + s.px(10);
    }
    const int bottomH = s.px(32);
    const int prevTop = y + s.px(4);
    const int errH = s.px(36);
    const int prevH = std::max<int>(s.px(80), rc.bottom - m - bottomH - s.px(10) - errH - prevTop);
    place(ID_PREVIEW + 100, m, y - s.px(18) + s.px(4), W, s.px(18));
    MoveWindow(s.preview, m, prevTop, W, prevH, FALSE);
    MoveWindow(s.error, m, prevTop + prevH + s.px(4), W, errH, FALSE);
    const int by = static_cast<int>(rc.bottom) - m - bottomH;
    place(ID_REMOVE, m, by, s.px(150), bottomH);
    place(ID_CANCEL, rc.right - m - s.px(100), by, s.px(100), bottomH);
    place(ID_APPLY, rc.right - m - s.px(208), by, s.px(100), bottomH);
    InvalidateRect(s.wnd, nullptr, TRUE);
}

LRESULT CALLBACK previewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) { SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); return TRUE; }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT && s) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        // the equation on white, as on a page; scaled down when it is wider than the box
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        HBRUSH white = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
        FillRect(mem, &rc, white);
        DeleteObject(white);
        if (s->pw > 0 && s->ph > 0) {
            const double k = std::min({1.0, (rc.right - 16.0) / s->pw, (rc.bottom - 16.0) / s->ph});
            const int w2 = std::max(1, static_cast<int>(s->pw * k)), h2 = std::max(1, static_cast<int>(s->ph * k));
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = s->pw;
            bi.bmiHeader.biHeight = -s->ph;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(mem, HALFTONE);
            SetBrushOrgEx(mem, 0, 0, nullptr);
            StretchDIBits(mem, (rc.right - w2) / 2, (rc.bottom - h2) / 2, w2, h2, 0, 0, s->pw, s->ph, s->pixels.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
        } else {
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, RGB(0x80, 0x80, 0x80));
            SelectObject(mem, s->font);
            const wchar_t* t = s->err.empty() ? L"The equation appears here as you type." : L"";
            DrawTextW(mem, t, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        HBRUSH line = CreateSolidBrush(pal().line);
        FrameRect(mem, &rc, line);
        DeleteObject(line);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

LRESULT CALLBACK editProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto* s = reinterpret_cast<State*>(data);
    const bool ctrl = GetKeyState(VK_CONTROL) < 0;
    if (m == WM_KEYDOWN && ctrl && (w == VK_OEM_PLUS || w == VK_ADD)) { insert(*s, GetKeyState(VK_SHIFT) < 0 ? L"^{\x0001}" : L"_{\x0001}"); return 0; }
    if (m == WM_KEYDOWN && ctrl && w == 'A') { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
    if (m == WM_CHAR && ctrl && (w == L'=' || w == L'+' || w == 1 || w == 0x1D)) return 0;
    return DefSubclassProc(h, m, w, l);
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); break;
        case WM_SIZE: if (s && s->preview) layout(*s); return 0;
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(l);
            mm->ptMinTrackSize = {MulDiv(620, s ? s->dpi : 96, 96), MulDiv(520, s ? s->dpi : 96, 96)};
            return 0;
        }
        case WM_TIMER:
            if (s && w == kTimer) { KillTimer(h, kTimer); render(*s); }
            return 0;
        case WM_CTLCOLORSTATIC:
            if (s && reinterpret_cast<HWND>(l) == s->error) {
                const LRESULT r = themeui::ctlColour(m, reinterpret_cast<HDC>(w), reinterpret_cast<HWND>(l));
                SetTextColor(reinterpret_cast<HDC>(w), pal().dark ? RGB(0xFF, 0x8A, 0x80) : RGB(0xC6, 0x28, 0x28));
                return r;
            }
            break;
        case WM_COMMAND: {
            if (!s) break;
            const int id = LOWORD(w), code = HIWORD(w);
            if (id == ID_EDIT && code == EN_CHANGE) { SetTimer(h, kTimer, 150, nullptr); return 0; }
            if (id == ID_SIZE && (code == CBN_EDITCHANGE || code == CBN_SELCHANGE)) {
                if (code == CBN_SELCHANGE) {
                    wchar_t t[32] = L"";
                    SendMessageW(s->size, CB_GETLBTEXT, SendMessageW(s->size, CB_GETCURSEL, 0, 0), reinterpret_cast<LPARAM>(t));
                    SetWindowTextW(s->size, t);
                }
                SetTimer(h, kTimer, 150, nullptr);
                return 0;
            }
            if (id >= ID_BUTTON0 && id < ID_BUTTON0 + static_cast<int>(s->inserts.size())) { insert(*s, s->inserts[static_cast<size_t>(id - ID_BUTTON0)]); return 0; }
            if (id == ID_APPLY || id == IDOK) {
                KillTimer(h, kTimer);
                render(*s);
                if (!s->err.empty()) { SetFocus(s->edit); MessageBeep(MB_ICONWARNING); return 0; }
                s->result.source = textOf(s->edit);
                s->result.size = sizeOf(*s);
                s->ok = true;
                s->done = true;
                return 0;
            }
            if (id == ID_REMOVE) { s->result.remove = true; s->ok = true; s->done = true; return 0; }
            if (id == ID_CANCEL || id == IDCANCEL) { s->done = true; return 0; }
            break;
        }
        case WM_CLOSE: if (s) s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

bool run(HWND parent, HINSTANCE inst, int dpi, HFONT font, const wchar_t* title, const std::wstring& source, double size, bool existing, Result& out,
         bool tex) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = proc; wc.hInstance = inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = kClass;
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        WNDCLASSEXW pc{sizeof(pc)};
        pc.lpfnWndProc = previewProc; pc.hInstance = inst; pc.hCursor = LoadCursorW(nullptr, IDC_ARROW); pc.lpszClassName = kPreviewClass;
        RegisterClassExW(&pc);
        registered = true;
    }
    State s;
    s.dpi = dpi;
    s.font = font;
    s.existing = existing;
    s.tex = tex;
    s.big = CreateFontW(-MulDiv(19, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    s.sym = CreateFontW(-MulDiv(17, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    LOGFONTW lf{};
    GetObjectW(font, sizeof(lf), &lf);
    lf.lfHeight = lf.lfHeight * 9 / 10;
    s.small = CreateFontIndirectW(&lf);
    RECT pr;
    GetWindowRect(parent, &pr);
    const int W = s.px(780), H = s.px(600);
    s.wnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kClass, title,
                            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN,
                            pr.left + std::max<int>(0, (pr.right - pr.left - W) / 2), pr.top + std::max<int>(0, (pr.bottom - pr.top - H) / 2),
                            W, H, parent, nullptr, inst, &s);
    if (!s.wnd) { DeleteObject(s.big); DeleteObject(s.sym); DeleteObject(s.small); return false; }
    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0, HFONT f = nullptr) {
        HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(f ? f : font), FALSE);
        return c;
    };
    mk(L"STATIC", L"Type the equation, or build it with the buttons:", SS_LEFT, ID_HINT + 100);
    s.edit = mk(L"EDIT", source.c_str(), WS_TABSTOP | ES_AUTOHSCROLL, ID_EDIT, WS_EX_CLIENTEDGE, s.big);
    SetWindowSubclass(s.edit, editProc, 1, reinterpret_cast<DWORD_PTR>(&s));
    mk(L"STATIC", L"Size", SS_LEFT, ID_SIZE + 100);
    s.size = mk(L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN, ID_SIZE);
    for (const wchar_t* t : {L"8", L"9", L"10", L"11", L"12", L"14", L"16", L"18", L"20", L"24", L"28", L"36"}) SendMessageW(s.size, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
    wchar_t sz[16];
    swprintf(sz, 16, L"%g", std::round(size * 10) / 10);
    SetWindowTextW(s.size, sz);
    s.hint = mk(L"STATIC",
                L"s^8 is a power, x_1 an index (use { } for more: e^{-st}, x_{n+1}). \\frac{top}{bottom} is a fraction, \\sqrt{x} a root. "
                L"Spaces only separate; signs get their own space. Select text and press a button to wrap it.",
                SS_LEFT, ID_HINT);
    SendMessageW(s.hint, WM_SETFONT, reinterpret_cast<WPARAM>(s.small), FALSE);
    s.tip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, s.wnd, nullptr, inst, nullptr);
    auto addTip = [&](HWND b, const wchar_t* text) {
        TOOLINFOW ti{sizeof(ti)};
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = s.wnd;
        ti.uId = reinterpret_cast<UINT_PTR>(b);
        ti.lpszText = const_cast<wchar_t*>(text);
        SendMessageW(s.tip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    };
    int id = ID_BUTTON0;
    for (const Button& b : kStructures) {
        HWND c = mk(L"BUTTON", b.label, WS_TABSTOP | BS_PUSHBUTTON, id++, 0, s.sym);
        addTip(c, b.tip);
        s.buttons.push_back(c);
        s.inserts.push_back(b.insert);
    }
    for (const wchar_t* list : {kSigns, kGreek})
        for (const wchar_t* p = list; *p; ++p) {
            const wchar_t one[2] = {*p, 0};
            HWND c = mk(L"BUTTON", one, WS_TABSTOP | BS_PUSHBUTTON, id++, 0, s.sym);
            s.buttons.push_back(c);
            s.inserts.push_back(std::wstring(one));
        }
    mk(L"STATIC", L"How it will look:", SS_LEFT, ID_PREVIEW + 100);
    s.preview = CreateWindowExW(0, kPreviewClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PREVIEW)), inst, &s);
    s.error = mk(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, ID_ERROR);
    HWND rem = mk(L"BUTTON", L"Remove equation", WS_TABSTOP | BS_PUSHBUTTON, ID_REMOVE);
    if (!existing) ShowWindow(rem, SW_HIDE);
    mk(L"BUTTON", existing ? L"Apply" : L"Insert", WS_TABSTOP | BS_DEFPUSHBUTTON, ID_APPLY);
    mk(L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON, ID_CANCEL);
    layout(s);
    themeui::themeDialog(s.wnd);
    render(s);
    EnableWindow(parent, FALSE);
    ShowWindow(s.wnd, SW_SHOW);
    SetFocus(s.edit);
    SendMessageW(s.edit, EM_SETSEL, static_cast<WPARAM>(-1), -1);
    MSG msg;
    while (!s.done) {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0) { PostQuitMessage(static_cast<int>(msg.wParam)); break; }
        if (r < 0) break;
        if (msg.message == WM_KEYDOWN && (msg.hwnd == s.wnd || IsChild(s.wnd, msg.hwnd))) {
            wchar_t cls[16] = L"";
            GetClassNameW(msg.hwnd, cls, 16);
            const bool dropped = _wcsicmp(cls, L"ComboBox") == 0 && SendMessageW(msg.hwnd, CB_GETDROPPEDSTATE, 0, 0);
            if (msg.wParam == VK_ESCAPE && !dropped) { s.done = true; continue; }
            if (msg.wParam == VK_RETURN && !dropped) { SendMessageW(s.wnd, WM_COMMAND, MAKEWPARAM(ID_APPLY, BN_CLICKED), 0); continue; }
        }
        if (!IsDialogMessageW(s.wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(s.wnd);
    SetActiveWindow(parent);
    DeleteObject(s.big);
    DeleteObject(s.sym);
    DeleteObject(s.small);
    if (s.ok) out = s.result;
    return s.ok;
}

} // namespace eqeditor
