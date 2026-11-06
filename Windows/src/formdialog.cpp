#include "formdialog.hpp"
#include <algorithm>
#include "themeui.hpp"
// message boxes in the window's theme
#define MessageBoxW themeui::messageBox

namespace {

struct State {
    std::vector<FormField>* fields;
    std::vector<HWND> controls;
    const wchar_t* okText;
    std::wstring (*validate)(const std::vector<FormField>&);
    HINSTANCE inst;
};

enum { ID_FIRST = 500 };

void layout(HWND h, State* st) {
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
    HDC dc = GetDC(h);
    HGDIOBJ old = SelectObject(dc, font);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    ReleaseDC(h, dc);
    const int unit = std::max<int>(tm.tmHeight, 15);
    const int pad = unit, labelW = unit * 11, fieldW = unit * 17, rowH = unit * 2, w = pad * 3 + labelW + fieldW;
    int y = pad;
    for (size_t i = 0; i < st->fields->size(); ++i) {
        FormField& f = (*st->fields)[i];
        HWND ctl = nullptr;
        if (f.kind == FormField::Note) {
            // as tall as the text is when wrapped at the dialog's width (long lines wrap, not only the line breaks count)
            RECT tr{0, 0, w - 2 * pad, 0};
            HDC mdc = GetDC(h);
            HGDIOBJ mo = SelectObject(mdc, font);
            DrawTextW(mdc, f.text.c_str(), -1, &tr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS);
            SelectObject(mdc, mo);
            ReleaseDC(h, mdc);
            int nh = static_cast<int>(tr.bottom) + unit / 2;
            if (nh > unit * 26) {   // a long text (About, reports): a box of its own that scrolls, and its text can be copied
                nh = unit * 26;
                std::wstring crlf;
                for (wchar_t c : f.text) { if (c == L'\n') crlf += L'\r'; crlf += c; }
                ctl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", crlf.c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                      pad, y, w - 2 * pad, nh, h, nullptr, st->inst, nullptr);
            } else
            ctl = CreateWindowExW(0, L"STATIC", f.text.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, pad, y, w - 2 * pad, nh, h, nullptr, st->inst, nullptr);
            y += nh + unit / 2;
        } else if (f.kind == FormField::Check) {
            ctl = CreateWindowExW(0, L"BUTTON", f.label.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, pad, y, w - 2 * pad, unit + 6, h,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIRST + i)), st->inst, nullptr);
            SendMessageW(ctl, BM_SETCHECK, f.checked ? BST_CHECKED : BST_UNCHECKED, 0);
            y += rowH;
        } else {
            HWND lab = CreateWindowExW(0, L"STATIC", f.label.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, pad, y + 4, labelW, unit + 4, h, nullptr, st->inst, nullptr);
            SendMessageW(lab, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (f.kind == FormField::Choice) {
                ctl = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 2 * pad + labelW, y, fieldW, unit * 12, h,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIRST + i)), st->inst, nullptr);
                for (auto& c : f.choices) SendMessageW(ctl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c.c_str()));
                SendMessageW(ctl, CB_SETCURSEL, std::clamp(f.choice, 0, std::max(0, static_cast<int>(f.choices.size()) - 1)), 0);
            } else {
                ctl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", f.text.c_str(),
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (f.kind == FormField::Password ? ES_PASSWORD : 0),
                                      2 * pad + labelW, y, fieldW, unit + 8, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FIRST + i)), st->inst, nullptr);
            }
            y += rowH + 2;
        }
        SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        st->controls.push_back(ctl);
    }
    y += unit / 2;
    // an OK text that starts with "!" makes a dialog with only that one button (for information)
    const bool single = st->okText[0] == L'!';
    const wchar_t* okLabel = single ? st->okText + 1 : st->okText;
    const int okW = std::max<int>(unit * 6, static_cast<int>(wcslen(okLabel)) * unit / 2 + unit * 2);
    HWND ok = CreateWindowExW(0, L"BUTTON", okLabel, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                              single ? w - pad - okW : w - pad * 2 - unit * 6 - okW, y, okW, unit * 2, h, reinterpret_cast<HMENU>(IDOK), st->inst, nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (!single) {
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, w - pad - unit * 6, y, unit * 6, unit * 2, h,
                                      reinterpret_cast<HMENU>(IDCANCEL), st->inst, nullptr);
        SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    y += unit * 2 + pad;

    RECT rc{0, 0, w, y};
    DWORD style = static_cast<DWORD>(GetWindowLongW(h, GWL_STYLE));
    AdjustWindowRectEx(&rc, style, FALSE, static_cast<DWORD>(GetWindowLongW(h, GWL_EXSTYLE)));
    RECT pr{};
    GetWindowRect(GetParent(h) ? GetParent(h) : GetDesktopWindow(), &pr);
    const int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    SetWindowPos(h, nullptr, pr.left + (pr.right - pr.left - ww) / 2, pr.top + std::max<LONG>(20, (pr.bottom - pr.top - wh) / 3), ww, wh, SWP_NOZORDER);
}

INT_PTR CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_INITDIALOG: {
            st = reinterpret_cast<State*>(l);
            SetWindowLongPtrW(h, GWLP_USERDATA, l);
            layout(h, st);
            themeui::themeDialog(h);
            for (size_t i = 0; i < st->controls.size(); ++i) {
                if ((*st->fields)[i].kind == FormField::Note) continue;
                SetFocus(st->controls[i]);
                if ((*st->fields)[i].kind == FormField::Text || (*st->fields)[i].kind == FormField::Password)
                    SendMessageW(st->controls[i], EM_SETSEL, 0, -1);
                break;
            }
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(w) == IDOK) {
                // Read the answers back into the fields.
                for (size_t i = 0; i < st->controls.size(); ++i) {
                    FormField& f = (*st->fields)[i];
                    HWND c = st->controls[i];
                    if (f.kind == FormField::Text || f.kind == FormField::Password) {
                        int len = GetWindowTextLengthW(c);
                        std::wstring s(static_cast<size_t>(len) + 1, L'\0');
                        GetWindowTextW(c, s.data(), len + 1);
                        s.resize(static_cast<size_t>(len));
                        f.text = s;
                    } else if (f.kind == FormField::Choice) {
                        f.choice = static_cast<int>(SendMessageW(c, CB_GETCURSEL, 0, 0));
                    } else if (f.kind == FormField::Check) {
                        f.checked = SendMessageW(c, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    }
                }
                if (st->validate) {
                    std::wstring err = st->validate(*st->fields);
                    if (!err.empty()) { MessageBoxW(h, err.c_str(), L"Feather PDF", MB_OK | MB_ICONINFORMATION); return TRUE; }
                }
                EndDialog(h, IDOK);
            } else if (LOWORD(w) == IDCANCEL) {
                EndDialog(h, IDCANCEL);
            }
            return TRUE;
    }
    return FALSE;
}

template <typename T> void put(std::vector<BYTE>& b, T v) {
    const BYTE* p = reinterpret_cast<const BYTE*>(&v);
    b.insert(b.end(), p, p + sizeof(T));
}

} // namespace

bool showForm(HWND parent, HINSTANCE inst, const wchar_t* title, std::vector<FormField>& fields, const wchar_t* okText,
              std::wstring (*validate)(const std::vector<FormField>&)) {
    // An empty dialog template (title and font only); the controls are created in WM_INITDIALOG.
    std::vector<BYTE> b;
    put<DWORD>(b, DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    put<DWORD>(b, 0); put<WORD>(b, 0);
    put<short>(b, 0); put<short>(b, 0); put<short>(b, 100); put<short>(b, 50);
    put<WORD>(b, 0); put<WORD>(b, 0);
    for (const wchar_t* p = title; *p; ++p) put<WORD>(b, *p);
    put<WORD>(b, 0);
    put<WORD>(b, 9);
    for (const wchar_t* p = L"Segoe UI"; *p; ++p) put<WORD>(b, *p);
    put<WORD>(b, 0);
    State st{&fields, {}, okText, validate, inst};
    return DialogBoxIndirectParamW(inst, reinterpret_cast<LPCDLGTEMPLATEW>(b.data()), parent, proc, reinterpret_cast<LPARAM>(&st)) == IDOK;
}
