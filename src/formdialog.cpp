#include "formdialog.hpp"
#include <algorithm>

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
            ctl = CreateWindowExW(0, L"STATIC", f.text.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, pad, y, w - 2 * pad, unit * 3, h, nullptr, st->inst, nullptr);
            y += unit * 3 + unit / 2;
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
    HWND ok = CreateWindowExW(0, L"BUTTON", st->okText, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, w - pad * 2 - unit * 12, y, unit * 6, unit * 2, h,
                              reinterpret_cast<HMENU>(IDOK), st->inst, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, w - pad - unit * 6, y, unit * 6, unit * 2, h,
                                  reinterpret_cast<HMENU>(IDCANCEL), st->inst, nullptr);
    SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
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
