#include "inlineedit.hpp"
#include "fontlist.hpp"
#include "equation.hpp"
#include <algorithm>
#include <cmath>
#include <cwchar>

namespace inlineedit {
namespace {

enum { ID_EDIT = 301, ID_FAM, ID_SIZE, ID_BOLD, ID_ITAL, ID_COL, ID_ALIGN, ID_OK, ID_NO, ID_HINT, ID_SUP, ID_SUB, ID_EQ };
const COLORREF kAccent = RGB(0x2D, 0x7F, 0xF9);
const wchar_t* kFaces[] = {L"Arial", L"Times New Roman", L"Courier New"};
Editor* g_current = nullptr;
wchar_t featherMake(wchar_t c, int kind) {
    const std::wstring s = equation::toScript(std::wstring(1, c), kind);
    return s.empty() ? c : s[0];
}   // the edit box's subclass procedure finds its editor here (one editor at a time)

} // namespace

void Editor::create(HWND viewer, HINSTANCE inst) {
    viewer_ = viewer;
    inst_ = inst;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FeatherInlineEdit";
    RegisterClassExW(&wc);
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    uiFont_ = CreateFontIndirectW(&ncm.lfMessageFont);
    editBrush_ = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
}

void Editor::start(const Spec& spec, Locate locate, Commit commit) {
    if (wnd_) cancel();
    spec_ = spec;
    style_ = spec.style;
    locate_ = std::move(locate);
    commit_ = std::move(commit);
    closing_ = false;
    if (!locate_(box_, ppp_)) return;
    wnd_ = CreateWindowExW(0, L"FeatherInlineEdit", L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 10, 10, viewer_, nullptr, inst_, this);
    if (!wnd_) return;
    g_current = this;
    InvalidateRect(viewer_, nullptr, FALSE);   // what the page shows for edit mode (the frames) steps aside while typing
    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, int w) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, w, 24, wnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst_, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        return c;
    };
    barH_ = 32;
    famBox_ = sizeBox_ = boldBtn_ = italBtn_ = colBox_ = alignBox_ = hint_ = supBtn_ = subBtn_ = eqBtn_ = nullptr;
    scriptMode_ = 0;
    if (spec_.paragraph) {
        famBox_ = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_FAM, 120);
        for (const wchar_t* f : {L"Sans-serif", L"Serif", L"Monospaced"}) SendMessageW(famBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(f));
        SendMessageW(famBox_, CB_SETCURSEL, std::clamp(style_.family, 0, 2), 0);
        sizeBox_ = mk(L"COMBOBOX", L"", CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, ID_SIZE, 62);
        for (const wchar_t* s : {L"8", L"9", L"10", L"11", L"12", L"14", L"16", L"18", L"20", L"24", L"28", L"32", L"36", L"48", L"72"}) SendMessageW(sizeBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        wchar_t sz[16];
        swprintf(sz, 16, L"%g", std::round(style_.sizePt * 10) / 10);
        SetWindowTextW(sizeBox_, sz);
        boldBtn_ = mk(L"BUTTON", L"B", BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, ID_BOLD, 28);
        italBtn_ = mk(L"BUTTON", L"I", BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, ID_ITAL, 28);
        SendMessageW(boldBtn_, BM_SETCHECK, style_.bold ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(italBtn_, BM_SETCHECK, style_.italic ? BST_CHECKED : BST_UNCHECKED, 0);
        supBtn_ = mk(L"BUTTON", L"x\x00B2", BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, ID_SUP, 32);
        subBtn_ = mk(L"BUTTON", L"x\x2082", BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, ID_SUB, 32);
        if (spec_.onEquation) eqBtn_ = mk(L"BUTTON", L"\x2211 Equation", BS_PUSHBUTTON | WS_TABSTOP, ID_EQ, 84);
        colBox_ = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_COL, 112);
        SendMessageW(colBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Colour as now"));
        for (const auto& n : spec_.paletteNames) SendMessageW(colBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(n.c_str()));
        SendMessageW(colBox_, CB_SETCURSEL, 0, 0);
        alignBox_ = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_ALIGN, 92);
        for (const wchar_t* a : {L"Left", L"Centre", L"Right", L"Justified"}) SendMessageW(alignBox_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(a));
        SendMessageW(alignBox_, CB_SETCURSEL, std::clamp(style_.align, 0, 3), 0);
        okBtn_ = mk(L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP, ID_OK, 64);
        noBtn_ = mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, ID_NO, 64);
        hint_ = mk(L"STATIC", L"Ctrl+Enter applies", SS_LEFT | SS_CENTERIMAGE, ID_HINT, 120);
    } else {
        supBtn_ = mk(L"BUTTON", L"x\x00B2", BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, ID_SUP, 32);
        subBtn_ = mk(L"BUTTON", L"x\x2082", BS_AUTOCHECKBOX | BS_PUSHLIKE | WS_TABSTOP, ID_SUB, 32);
        if (spec_.onEquation) eqBtn_ = mk(L"BUTTON", L"\x2211 Equation", BS_PUSHBUTTON | WS_TABSTOP, ID_EQ, 84);
        hint_ = mk(L"STATIC", L"Enter applies \x00B7 Esc cancels", SS_LEFT | SS_CENTERIMAGE, ID_HINT, 170);
        okBtn_ = mk(L"BUTTON", L"Apply", BS_PUSHBUTTON | WS_TABSTOP, ID_OK, 64);
        noBtn_ = mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, ID_NO, 64);
    }
    // the bar's controls, left to right
    int x = 4;
    for (HWND c : {famBox_, sizeBox_, boldBtn_, italBtn_, supBtn_, subBtn_, eqBtn_, colBox_, alignBox_, okBtn_, noBtn_, hint_}) {
        if (!c) continue;
        RECT r;
        GetWindowRect(c, &r);
        const int w = r.right - r.left;
        MoveWindow(c, x, 4, w, c == famBox_ || c == sizeBox_ || c == colBox_ || c == alignBox_ ? 300 : 24, FALSE);
        x += w + 4;
    }
    barW_ = x;
    applyFont();
    makeEdit();
    layout();
    ShowWindow(wnd_, SW_SHOW);
    SetFocus(edit_);
    POINT at = spec_.click;
    if (at.x >= 0) {   // the caret goes where the text was clicked
        MapWindowPoints(viewer_, edit_, &at, 1);
        const LRESULT r = SendMessageW(edit_, EM_CHARFROMPOS, 0, MAKELPARAM(std::max(0L, at.x), std::max(0L, at.y)));
        const int ch = LOWORD(r);
        SendMessageW(edit_, EM_SETSEL, ch, ch);
    } else SendMessageW(edit_, EM_SETSEL, static_cast<WPARAM>(-1), -1);
}

void Editor::applyFont() {
    if (textFont_) DeleteObject(textFont_);
    // the document's font when it is installed and the style is still the original one, else a common face of the family
    const bool original = style_.family == spec_.style.family && style_.bold == spec_.style.bold && style_.italic == spec_.style.italic;
    std::wstring face = original ? fontlist::installedFace(spec_.pdfFont) : L"";
    if (face.empty()) face = kFaces[std::clamp(style_.family, 0, 2)];
    const int px = std::max(6, static_cast<int>(std::lround(style_.sizePt * ppp_)));
    auto make = [&](int width) {
        return CreateFontW(-px, width, 0, 0, style_.bold ? FW_BOLD : FW_NORMAL, style_.italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face.c_str());
    };
    textFont_ = make(0);
    // Match the widths to the page: the same text must be as wide in the box as it will be on the page, or the box breaks
    // lines in other places. The font is narrowed or widened, and what rounding leaves over is made up in the box width.
    fit_ = 1;
    std::wstring sample;
    for (wchar_t c : spec_.text) if (c != L'\r') sample += c == L'\n' ? L' ' : c;
    const double want = spec_.measure && !sample.empty() ? spec_.measure(sample, style_) * ppp_ : 0;
    if (want > 4) {
        HDC dc = GetDC(viewer_);
        auto widthOf = [&](HFONT f, TEXTMETRICW* tm) {
            HGDIOBJ old = SelectObject(dc, f);
            SIZE ext{};
            GetTextExtentPoint32W(dc, sample.c_str(), static_cast<int>(sample.size()), &ext);
            if (tm) GetTextMetricsW(dc, tm);
            SelectObject(dc, old);
            return static_cast<double>(ext.cx);
        };
        TEXTMETRICW tm{};
        double have = widthOf(textFont_, &tm);
        if (have > 0 && std::fabs(want / have - 1) > 0.01 && tm.tmAveCharWidth > 0) {
            HFONT f = make(std::max(1, static_cast<int>(std::lround(tm.tmAveCharWidth * want / have))));
            const double have2 = f ? widthOf(f, nullptr) : 0;
            if (have2 > 0 && std::fabs(have2 / want - 1) < std::fabs(have / want - 1)) { DeleteObject(textFont_); textFont_ = f; have = have2; }
            else if (f) DeleteObject(f);
        }
        if (have > 0) fit_ = have / want;
        ReleaseDC(viewer_, dc);
    }
    if (edit_) { SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(textFont_), TRUE); InvalidateRect(edit_, nullptr, TRUE); }
}

void Editor::makeEdit() {
    std::wstring text = spec_.text;
    DWORD sel0 = 0, sel1 = 0;
    if (edit_) {
        const int n = GetWindowTextLengthW(edit_);
        text.assign(static_cast<size_t>(n) + 1, L'\0');
        GetWindowTextW(edit_, text.data(), n + 1);
        text.resize(static_cast<size_t>(n));
        SendMessageW(edit_, EM_GETSEL, reinterpret_cast<WPARAM>(&sel0), reinterpret_cast<LPARAM>(&sel1));
        DestroyWindow(edit_);
    } else if (spec_.paragraph) {   // the edit box wants CR LF line ends
        std::wstring t;
        for (wchar_t c : text) { if (c == L'\n') t += L"\r\n"; else if (c != L'\r') t += c; }
        text = t;
    }
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (spec_.paragraph) style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | (style_.align == 1 ? ES_CENTER : style_.align == 2 ? ES_RIGHT : ES_LEFT);
    else style |= ES_AUTOHSCROLL;
    edit_ = CreateWindowExW(0, L"EDIT", text.c_str(), style, 0, 0, 10, 10, wnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT)), inst_, nullptr);
    SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(textFont_), FALSE);
    SendMessageW(edit_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));   // the text starts where it does on the page
    oldEdit_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editProc)));
    if (sel0 || sel1) SendMessageW(edit_, EM_SETSEL, sel0, sel1);
}

void Editor::layout() {
    if (!wnd_ || !edit_) return;
    HDC dc = GetDC(edit_);
    HGDIOBJ old = SelectObject(dc, textFont_);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int n = GetWindowTextLengthW(edit_);
    std::wstring text(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(edit_, text.data(), n + 1);
    text.resize(static_cast<size_t>(n));
    // The edit box lies exactly over the text: the first line on the first baseline, as wide as the page's line width
    // (paragraphs) or as the text (a line). It grows with the text.
    const int textX = box_.left, textY = spec_.baselineBox ? box_.top - tm.tmAscent : box_.top;
    int w = 0, h = 0, wrap = 0;
    if (spec_.paragraph) {
        // Lines break at the page's line width, plus a little: the screen font's letters are rounded to whole pixels, and a
        // line that exactly fills the width on the page must not break early in the box. The box itself also covers the
        // whole paragraph as it is on the page, so none of the old text shows beside or below it.
        wrap = spec_.wrapPt > 0 ? static_cast<int>(std::lround(spec_.wrapPt * ppp_ * fit_ * 1.006)) + 3 : box_.right - box_.left + 6;
        wrap = std::max(wrap, static_cast<int>(tm.tmAveCharWidth * 12));
        w = std::max(wrap, static_cast<int>(box_.right - box_.left) + 3);
        RECT er;
        GetClientRect(edit_, &er);
        if (er.right != w) SetWindowPos(edit_, nullptr, 0, 0, w, std::max<int>(er.bottom, tm.tmHeight), SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW);
        RECT fr{0, 0, wrap, 30000};
        SendMessageW(edit_, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&fr));   // lines are counted at this width
        const int lines = std::max(1, static_cast<int>(SendMessageW(edit_, EM_GETLINECOUNT, 0, 0)));
        h = lines * tm.tmHeight + 2;
        if (spec_.baselineBox) h = std::max(h, static_cast<int>(box_.bottom - box_.top) + static_cast<int>(tm.tmHeight) + 2);
    } else {
        SIZE ext{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &ext);
        w = std::max(static_cast<int>(box_.right - box_.left) + 2, static_cast<int>(ext.cx + tm.tmAveCharWidth * 2));
        h = tm.tmHeight + 2;
    }
    SelectObject(dc, old);
    ReleaseDC(edit_, dc);
    // the bar goes above the text, or below it when there is no room above; a 3-pixel frame goes round the edit box
    const int pad = 3;
    const bool barBelow = textY - pad - barH_ < 0;
    const int left = textX - pad, top = barBelow ? textY - pad : textY - pad - barH_;
    const int frameY = barBelow ? 0 : barH_, barY = barBelow ? h + 2 * pad : 0;
    MoveWindow(wnd_, left, top, std::max(w + 2 * pad, barW_), h + 2 * pad + barH_, TRUE);
    MoveWindow(edit_, pad, frameY + pad, w, h, TRUE);
    if (wrap > 0) { RECT fr{0, 0, wrap, h}; SendMessageW(edit_, EM_SETRECT, 0, reinterpret_cast<LPARAM>(&fr)); }   // resizing reset it
    for (HWND c : {famBox_, sizeBox_, boldBtn_, italBtn_, supBtn_, subBtn_, eqBtn_, colBox_, alignBox_, okBtn_, noBtn_, hint_}) {
        if (!c) continue;
        RECT r;
        GetWindowRect(c, &r);
        MapWindowPoints(nullptr, wnd_, reinterpret_cast<POINT*>(&r), 2);
        SetWindowPos(c, nullptr, r.left, barY + 4, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    // only the bar and the framed edit box are part of the window, so the page stays visible around them
    HRGN rgn = CreateRectRgn(0, frameY, w + 2 * pad, frameY + h + 2 * pad);
    HRGN bar = CreateRectRgn(0, barY, barW_, barY + barH_);
    CombineRgn(rgn, rgn, bar, RGN_OR);
    DeleteObject(bar);
    SetWindowRgn(wnd_, rgn, TRUE);
}

void Editor::reposition() {
    if (!wnd_ || closing_) return;
    const double before = ppp_;
    if (!locate_(box_, ppp_)) { ShowWindow(wnd_, SW_HIDE); return; }
    if (std::fabs(before - ppp_) > 1e-6) applyFont();
    layout();
    ShowWindow(wnd_, SW_SHOW);
}

void Editor::readBar() {
    if (!spec_.paragraph) return;
    style_.family = static_cast<int>(SendMessageW(famBox_, CB_GETCURSEL, 0, 0));
    wchar_t sz[32] = L"";
    const int sel = static_cast<int>(SendMessageW(sizeBox_, CB_GETCURSEL, 0, 0));
    if (sel >= 0 && GetWindowTextLengthW(sizeBox_) == 0) SendMessageW(sizeBox_, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(sz));
    else GetWindowTextW(sizeBox_, sz, 32);
    const double v = wcstod(sz, nullptr);
    if (v >= 4 && v <= 200) style_.sizePt = v;
    style_.bold = SendMessageW(boldBtn_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    style_.italic = SendMessageW(italBtn_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const int c = static_cast<int>(SendMessageW(colBox_, CB_GETCURSEL, 0, 0));
    style_.color = c > 0 && c - 1 < static_cast<int>(spec_.palette.size()) ? spec_.palette[static_cast<size_t>(c - 1)] : spec_.style.color;
    style_.align = static_cast<int>(SendMessageW(alignBox_, CB_GETCURSEL, 0, 0));
}

// x² and x₂: the selected text made superscript (subscript), or back to plain when it already is; with nothing selected,
// what is typed next is (pressed again: plain again). Letters and signs that have no such character stay as they are.
void Editor::script(int kind) {
    if (!edit_) return;
    DWORD a = 0, b = 0;
    SendMessageW(edit_, EM_GETSEL, reinterpret_cast<WPARAM>(&a), reinterpret_cast<LPARAM>(&b));
    if (b > a) {
        const int n = GetWindowTextLengthW(edit_);
        std::wstring all(static_cast<size_t>(n) + 1, L'\0');
        GetWindowTextW(edit_, all.data(), n + 1);
        const std::wstring sel = all.substr(a, b - a);
        bool every = true;
        const std::wstring made = equation::scriptKind(sel) == kind ? equation::fromScript(sel) : equation::toScript(sel, kind, &every);
        SendMessageW(edit_, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(made.c_str()));
        SendMessageW(edit_, EM_SETSEL, a, a + made.size());
        scriptMode_ = 0;
        if (hint_) SetWindowTextW(hint_, every ? L"" : L"Some letters have no small raised form: use \x2211 Equation");
    } else {
        scriptMode_ = scriptMode_ == kind ? 0 : kind;
    }
    if (supBtn_) SendMessageW(supBtn_, BM_SETCHECK, scriptMode_ > 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    if (subBtn_) SendMessageW(subBtn_, BM_SETCHECK, scriptMode_ < 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    layout();
    SetFocus(edit_);
}

bool Editor::commit() {
    if (!wnd_ || closing_) return true;
    readBar();
    const int n = GetWindowTextLengthW(edit_);
    std::wstring raw(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(edit_, raw.data(), n + 1);
    raw.resize(static_cast<size_t>(n));
    std::wstring text;
    for (wchar_t c : raw) if (c != L'\r') text += c;
    closing_ = true;   // the commit may show a message; nothing should move the editor meanwhile
    const bool ok = commit_ ? commit_(text, style_) : true;
    closing_ = false;
    if (ok) close();
    else if (edit_) SetFocus(edit_);
    return ok;
}

void Editor::cancel() { close(); }

void Editor::close() {
    if (!wnd_) return;
    HWND w = wnd_;
    wnd_ = nullptr;
    edit_ = nullptr;
    g_current = nullptr;
    DestroyWindow(w);
    InvalidateRect(viewer_, nullptr, FALSE);   // and comes back
    if (textFont_) { DeleteObject(textFont_); textFont_ = nullptr; }
    SetFocus(viewer_);
}

LRESULT CALLBACK Editor::proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
    auto* e = reinterpret_cast<Editor*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!e || m == WM_NCCREATE) return DefWindowProcW(h, m, w, l);
    return e->handle(h, m, w, l);
}

LRESULT Editor::handle(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            FillRect(dc, &rc, GetSysColorBrush(COLOR_BTNFACE));
            if (edit_ && h == wnd_) {   // an accent frame around the text being edited
                RECT er;
                GetWindowRect(edit_, &er);
                MapWindowPoints(nullptr, h, reinterpret_cast<POINT*>(&er), 2);
                InflateRect(&er, 2, 2);
                HBRUSH b = CreateSolidBrush(kAccent);
                FrameRect(dc, &er, b);
                InflateRect(&er, -1, -1);
                FrameRect(dc, &er, b);
                DeleteObject(b);
            }
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CTLCOLOREDIT:
            if (reinterpret_cast<HWND>(l) == edit_) {
                HDC dc = reinterpret_cast<HDC>(w);
                SetTextColor(dc, style_.color);
                SetBkColor(dc, RGB(0xFF, 0xFF, 0xFF));
                return reinterpret_cast<LRESULT>(editBrush_);
            }
            break;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(w);
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        case WM_COMMAND: {
            const int id = LOWORD(w), code = HIWORD(w);
            if (id == ID_EDIT && code == EN_CHANGE) { layout(); return 0; }
            if (id == ID_OK) { commit(); return 0; }
            if (id == ID_NO) { cancel(); return 0; }
            if ((id == ID_FAM || id == ID_COL) && code == CBN_SELCHANGE) { readBar(); applyFont(); layout(); SetFocus(edit_); return 0; }
            if (id == ID_ALIGN && code == CBN_SELCHANGE) { readBar(); makeEdit(); layout(); SetFocus(edit_); return 0; }
            if (id == ID_SIZE && (code == CBN_SELCHANGE || code == CBN_EDITCHANGE)) {
                if (code == CBN_SELCHANGE) {   // the box shows the new choice only after this message
                    wchar_t t[32] = L"";
                    SendMessageW(sizeBox_, CB_GETLBTEXT, SendMessageW(sizeBox_, CB_GETCURSEL, 0, 0), reinterpret_cast<LPARAM>(t));
                    SetWindowTextW(sizeBox_, t);
                }
                readBar(); applyFont(); layout();
                return 0;
            }
            if ((id == ID_BOLD || id == ID_ITAL) && code == BN_CLICKED) { readBar(); applyFont(); layout(); SetFocus(edit_); return 0; }
            if ((id == ID_SUP || id == ID_SUB) && code == BN_CLICKED) { script(id == ID_SUP ? 1 : -1); return 0; }
            if (id == ID_EQ && code == BN_CLICKED && spec_.onEquation) {
                auto open = spec_.onEquation;   // the editor goes first: what it held is read again as an equation
                cancel();
                open();
                return 0;
            }
            break;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

LRESULT CALLBACK Editor::editProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Editor* e = g_current;
    if (!e) return DefWindowProcW(h, m, w, l);
    WNDPROC old = e->oldEdit_;
    const bool ctrl = GetKeyState(VK_CONTROL) < 0;
    switch (m) {
        case WM_GETDLGCODE: return DLGC_WANTALLKEYS | CallWindowProcW(old, h, m, w, l);
        case WM_KEYDOWN:
            if (w == VK_ESCAPE) { e->cancel(); return 0; }
            // applied once this key is handled, like the Apply button: a message the commit shows then gets the keyboard
            if (w == VK_RETURN && (!e->spec_.paragraph || ctrl)) { PostMessageW(e->wnd_, WM_COMMAND, MAKEWPARAM(ID_OK, BN_CLICKED), 0); return 0; }
            if (w == 'A' && ctrl) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
            // Ctrl+Shift+= superscript, Ctrl+= subscript (as in word processors)
            if (ctrl && (w == VK_OEM_PLUS || w == VK_ADD)) { e->script(GetKeyState(VK_SHIFT) < 0 ? 1 : -1); return 0; }
            break;
        case WM_CHAR:
            if (w == 0x1B || w == 1) return 0;                                       // Esc, Ctrl+A
            if (ctrl && (w == L'=' || w == L'+' || w == 0x1D)) return 0;              // the shortcuts above
            if (e->scriptMode_ != 0 && w >= 0x20 && w != 0x7F) {                     // typing superscripts or subscripts
                const wchar_t c = featherMake(static_cast<wchar_t>(w), e->scriptMode_);
                return CallWindowProcW(old, h, m, c, l);
            }
            if ((w == L'\r' || w == L'\n') && (!e->spec_.paragraph || ctrl)) return 0;   // handled on key down
            break;
    }
    return CallWindowProcW(old, h, m, w, l);
}

} // namespace inlineedit
