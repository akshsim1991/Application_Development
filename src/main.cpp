// Feather PDF: free, lightweight PDF viewer and editor for Windows.
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "document.hpp"
#include "viewer.hpp"

namespace {

constexpr int kTabH = 34, kToolH = 40;
constexpr const wchar_t* kAppName = L"Feather PDF";

struct Button { std::wstring label; RECT rc; int id; bool enabled; };
enum Cmd { C_OPEN = 1, C_PREV, C_NEXT, C_ZOOMOUT, C_ZOOMIN, C_FITW, C_FITP, C_NEWTAB, C_ACTUAL };

struct App {
    HINSTANCE inst = nullptr;
    HWND wnd = nullptr;
    Viewer viewer;
    std::vector<std::unique_ptr<Document>> tabs;   // an empty vector means the "New tab" screen
    int active = -1;
    HFONT font = nullptr, fontBold = nullptr;
    std::vector<Button> buttons;
    std::vector<RECT> tabRects, closeRects;
    RECT plusRect{};
    int hover = -1;
} app;

Document* activeDoc() { return app.active >= 0 ? app.tabs[app.active].get() : nullptr; }

void relayoutChildren() {
    RECT rc; GetClientRect(app.wnd, &rc);
    MoveWindow(app.viewer.hwnd(), 0, kTabH + kToolH, rc.right, std::max<LONG>(0, rc.bottom - kTabH - kToolH), TRUE);
}

void updateTitle() {
    Document* d = activeDoc();
    std::wstring t = d ? d->title() + L" - " + kAppName : kAppName;
    SetWindowTextW(app.wnd, t.c_str());
}

void refresh() {
    updateTitle();
    InvalidateRect(app.wnd, nullptr, FALSE);
}

void switchTab(int i) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    app.active = i;
    app.viewer.setDocument(app.tabs[i].get());
    refresh();
}

void closeTab(int i) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    if (i == app.active) app.viewer.setDocument(nullptr);   // viewer must let go before the document dies
    app.tabs.erase(app.tabs.begin() + i);
    if (app.tabs.empty()) { app.active = -1; app.viewer.setDocument(nullptr); }
    else {
        app.active = std::min(i, static_cast<int>(app.tabs.size()) - 1);
        app.viewer.setDocument(app.tabs[app.active].get());
    }
    refresh();
}

// ---- password prompt: a small modal dialog built in memory (no resource file needed) ----
struct PwState { std::wstring prompt, text; bool ok = false; };

INT_PTR CALLBACK pwProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<PwState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(h, GWLP_USERDATA, l);
            st = reinterpret_cast<PwState*>(l);
            SetDlgItemTextW(h, 100, st->prompt.c_str());
            SetFocus(GetDlgItem(h, 101));
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(w) == IDOK) {
                wchar_t buf[256]{};
                GetDlgItemTextW(h, 101, buf, 256);
                st->text = buf; st->ok = true;
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
void putStr(std::vector<BYTE>& b, const wchar_t* s) {
    for (; *s; ++s) put<WORD>(b, *s);
    put<WORD>(b, 0);
}
void align4(std::vector<BYTE>& b) { while (b.size() % 4) b.push_back(0); }
void item(std::vector<BYTE>& b, WORD cls, DWORD style, short x, short y, short cx, short cy, WORD id, const wchar_t* text) {
    align4(b);
    put<DWORD>(b, style | WS_CHILD | WS_VISIBLE); put<DWORD>(b, 0);
    put<short>(b, x); put<short>(b, y); put<short>(b, cx); put<short>(b, cy);
    put<WORD>(b, id);
    put<WORD>(b, 0xFFFF); put<WORD>(b, cls);   // 0x0080 button, 0x0081 edit, 0x0082 static
    putStr(b, text);
    put<WORD>(b, 0);
}

bool askPassword(const std::wstring& prompt, std::string& out) {
    std::vector<BYTE> b;
    put<DWORD>(b, DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    put<DWORD>(b, 0); put<WORD>(b, 4);
    put<short>(b, 0); put<short>(b, 0); put<short>(b, 200); put<short>(b, 78);
    put<WORD>(b, 0); put<WORD>(b, 0);
    putStr(b, L"Password required");
    put<WORD>(b, 9); putStr(b, L"Segoe UI");
    item(b, 0x0082, SS_LEFT, 8, 8, 184, 22, 100, L"");
    item(b, 0x0081, WS_BORDER | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL, 8, 34, 184, 13, 101, L"");
    item(b, 0x0080, BS_DEFPUSHBUTTON | WS_TABSTOP, 88, 56, 50, 14, IDOK, L"OK");
    item(b, 0x0080, BS_PUSHBUTTON | WS_TABSTOP, 142, 56, 50, 14, IDCANCEL, L"Cancel");
    b[8] = 4;   // cdit = 4 items (offset of the item count in DLGTEMPLATE)
    PwState st{prompt, {}, false};
    DialogBoxIndirectParamW(app.inst, reinterpret_cast<LPCDLGTEMPLATEW>(b.data()), app.wnd, pwProc,
                            reinterpret_cast<LPARAM>(&st));
    if (!st.ok) return false;
    int n = WideCharToMultiByte(CP_UTF8, 0, st.text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    out.assign(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, st.text.c_str(), -1, out.data(), n, nullptr, nullptr);
    return true;
}

// ---- opening files ----
void openFile(const std::wstring& pathIn) {
    wchar_t full[MAX_PATH * 2];
    std::wstring path = GetFullPathNameW(pathIn.c_str(), MAX_PATH * 2, full, nullptr) ? full : pathIn;

    for (size_t i = 0; i < app.tabs.size(); ++i)
        if (_wcsicmp(app.tabs[i]->path().c_str(), path.c_str()) == 0) { switchTab(static_cast<int>(i)); return; }

    auto doc = std::make_unique<Document>();
    std::string pw;
    OpenResult r = doc->open(path);
    while (r == OpenResult::PasswordNeeded || r == OpenResult::WrongPassword) {
        if (!askPassword(Document::describe(r, path), pw)) return;
        r = doc->open(path, pw);
    }
    if (r != OpenResult::Ok) {
        MessageBoxW(app.wnd, Document::describe(r, path).c_str(), kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    app.tabs.push_back(std::move(doc));
    switchTab(static_cast<int>(app.tabs.size()) - 1);
}

void openDialog() {
    wchar_t buf[MAX_PATH * 2] = L"";
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) openFile(buf);
}

void runCommand(int id) {
    Document* d = activeDoc();
    switch (id) {
        case C_OPEN: case C_NEWTAB: openDialog(); break;
        case C_PREV: if (d) app.viewer.goToPage(app.viewer.currentPage() - 1); break;
        case C_NEXT: if (d) app.viewer.goToPage(app.viewer.currentPage() + 1); break;
        case C_ZOOMIN: if (d) app.viewer.setZoom(d->zoom * 1.25); break;
        case C_ZOOMOUT: if (d) app.viewer.setZoom(d->zoom / 1.25); break;
        case C_ACTUAL: if (d) app.viewer.setZoom(1.0); break;
        case C_FITW: app.viewer.fitWidth(); break;
        case C_FITP: app.viewer.fitPage(); break;
    }
}

// ---- header drawing: tabs + toolbar. Text labels only, so nothing depends on icon fonts. ----
void layoutHeader(HDC dc, int width) {
    app.buttons.clear(); app.tabRects.clear(); app.closeRects.clear();
    Document* d = activeDoc();
    SelectObject(dc, app.font);

    int x = 8;
    for (size_t i = 0; i < app.tabs.size(); ++i) {
        SIZE sz; std::wstring t = app.tabs[i]->title();
        GetTextExtentPoint32W(dc, t.c_str(), static_cast<int>(t.size()), &sz);
        int w = std::clamp<int>(sz.cx + 44, 90, 240);
        app.tabRects.push_back({x, 4, x + w, kTabH});
        app.closeRects.push_back({x + w - 26, 10, x + w - 8, 28});
        x += w + 2;
    }
    app.plusRect = {x + 4, 8, x + 28, 30};

    auto add = [&](const wchar_t* label, int id, bool enabled, int minW = 0) {
        SIZE sz; GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &sz);
        int w = std::max<int>(sz.cx + 20, minW);
        app.buttons.push_back({label, {x, kTabH + 6, x + w, kTabH + kToolH - 6}, id, enabled});
        x += w + 4;
    };
    x = 8;
    add(L"Open", C_OPEN, true);
    x += 10;
    add(L"Prev", C_PREV, d != nullptr);
    x += 90;   // room for the "page x / n" label
    add(L"Next", C_NEXT, d != nullptr);
    x += 10;
    add(L"-", C_ZOOMOUT, d != nullptr, 28);
    x += 52;   // zoom label
    add(L"+", C_ZOOMIN, d != nullptr, 28);
    x += 10;
    add(L"Fit width", C_FITW, d != nullptr);
    add(L"Fit page", C_FITP, d != nullptr);
    add(L"100%", C_ACTUAL, d != nullptr);
    (void)width;
}

void paintHeader(HDC dc, const RECT& client) {
    layoutHeader(dc, client.right);
    HBRUSH tabBg = CreateSolidBrush(RGB(0xE9, 0xE9, 0xE9));
    HBRUSH toolBg = CreateSolidBrush(RGB(0xF7, 0xF7, 0xF7));
    RECT tr{0, 0, client.right, kTabH}, br{0, kTabH, client.right, kTabH + kToolH};
    FillRect(dc, &tr, tabBg); FillRect(dc, &br, toolBg);
    DeleteObject(tabBg); DeleteObject(toolBg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, app.font);

    if (app.tabs.empty()) {
        RECT r{8, 4, 128, kTabH};
        HBRUSH w = CreateSolidBrush(RGB(0xFA, 0xFA, 0xFA)); FillRect(dc, &r, w); DeleteObject(w);
        SetTextColor(dc, RGB(0x20, 0x20, 0x20));
        DrawTextW(dc, L"New tab", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (size_t i = 0; i < app.tabRects.size(); ++i) {
        bool act = static_cast<int>(i) == app.active;
        HBRUSH w = CreateSolidBrush(act ? RGB(0xFA, 0xFA, 0xFA) : RGB(0xDD, 0xDD, 0xDD));
        FillRect(dc, &app.tabRects[i], w); DeleteObject(w);
        RECT t = app.tabRects[i]; t.left += 12; t.right -= 30;
        SetTextColor(dc, RGB(0x20, 0x20, 0x20));
        DrawTextW(dc, app.tabs[i]->title().c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, RGB(0x60, 0x60, 0x60));
        DrawTextW(dc, L"x", -1, &app.closeRects[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (!app.tabs.empty()) {
        SetTextColor(dc, RGB(0x40, 0x40, 0x40));
        DrawTextW(dc, L"+", -1, &app.plusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    for (const Button& b : app.buttons) {
        SetTextColor(dc, b.enabled ? RGB(0x20, 0x20, 0x20) : RGB(0xA0, 0xA0, 0xA0));
        DrawTextW(dc, b.label.c_str(), -1, const_cast<RECT*>(&b.rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    Document* d = activeDoc();
    if (d) {
        SetTextColor(dc, RGB(0x30, 0x30, 0x30));
        const Button* prev = nullptr; const Button* zo = nullptr;
        for (const Button& b : app.buttons) { if (b.id == C_PREV) prev = &b; if (b.id == C_ZOOMOUT) zo = &b; }
        wchar_t s[64];
        swprintf(s, 64, L"Page %d / %d", app.viewer.currentPage() + 1, d->pageCount());
        RECT pr{prev->rc.right + 2, prev->rc.top, prev->rc.right + 88, prev->rc.bottom};
        DrawTextW(dc, s, -1, &pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        swprintf(s, 64, L"%d%%", static_cast<int>(d->zoom * 100 + 0.5));
        RECT zr{zo->rc.right + 2, zo->rc.top, zo->rc.right + 50, zo->rc.bottom};
        DrawTextW(dc, s, -1, &zr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0xD0, 0xD0, 0xD0));
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, 0, kTabH + kToolH - 1, nullptr); LineTo(dc, client.right, kTabH + kToolH - 1);
    SelectObject(dc, old); DeleteObject(pen);
}

void onViewerChange(void*) {
    RECT r{0, 0, 4000, kTabH + kToolH};
    InvalidateRect(app.wnd, &r, FALSE);
}

bool hit(const RECT& r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            app.wnd = h;
            LOGFONTW lf{};
            lf.lfHeight = -MulDiv(10, GetDeviceCaps(GetDC(h), LOGPIXELSY), 72);
            wcscpy_s(lf.lfFaceName, L"Segoe UI");
            app.font = CreateFontIndirectW(&lf);
            lf.lfWeight = FW_BOLD;
            app.fontBold = CreateFontIndirectW(&lf);
            app.viewer.create(h, app.inst);
            app.viewer.setOnChange(onViewerChange, nullptr);
            DragAcceptFiles(h, TRUE);
            return 0;
        }
        case WM_SIZE: relayoutChildren(); InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            paintHeader(dc, rc);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            for (size_t i = 0; i < app.closeRects.size(); ++i)
                if (hit(app.closeRects[i], x, y)) { closeTab(static_cast<int>(i)); return 0; }
            for (size_t i = 0; i < app.tabRects.size(); ++i)
                if (hit(app.tabRects[i], x, y)) { switchTab(static_cast<int>(i)); return 0; }
            if (!app.tabs.empty() && hit(app.plusRect, x, y)) { runCommand(C_NEWTAB); return 0; }
            for (const Button& b : app.buttons)
                if (b.enabled && hit(b.rc, x, y)) { runCommand(b.id); return 0; }
            return 0;
        }
        case WM_MBUTTONUP: {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            for (size_t i = 0; i < app.tabRects.size(); ++i)
                if (hit(app.tabRects[i], x, y)) { closeTab(static_cast<int>(i)); return 0; }
            return 0;
        }
        case WM_DROPFILES: {
            HDROP hd = reinterpret_cast<HDROP>(w);
            UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t p[MAX_PATH * 2];
                DragQueryFileW(hd, i, p, MAX_PATH * 2);
                openFile(p);
            }
            DragFinish(hd);
            return 0;
        }
        case WM_COPYDATA: {   // a second launch hands its file to this window
            auto* cds = reinterpret_cast<COPYDATASTRUCT*>(l);
            if (cds->dwData == 1) openFile(std::wstring(static_cast<const wchar_t*>(cds->lpData), cds->cbData / sizeof(wchar_t)));
            return TRUE;
        }
        case WM_KEYDOWN: {
            const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            Document* d = activeDoc();
            if (ctrl) {
                switch (w) {
                    case 'O': runCommand(C_OPEN); return 0;
                    case 'W': closeTab(app.active); return 0;
                    case '0': runCommand(C_FITP); return 0;
                    case '1': runCommand(C_ACTUAL); return 0;
                    case '2': runCommand(C_FITW); return 0;
                    case VK_OEM_PLUS: case VK_ADD: runCommand(C_ZOOMIN); return 0;
                    case VK_OEM_MINUS: case VK_SUBTRACT: runCommand(C_ZOOMOUT); return 0;
                    case VK_TAB:
                        if (!app.tabs.empty()) {
                            int n = static_cast<int>(app.tabs.size());
                            switchTab((app.active + (shift ? n - 1 : 1)) % n);
                        }
                        return 0;
                }
            }
            if (!d) return 0;
            switch (w) {
                case VK_NEXT: app.viewer.goToPage(app.viewer.currentPage() + 1); return 0;
                case VK_PRIOR: app.viewer.goToPage(app.viewer.currentPage() - 1); return 0;
                case VK_HOME: app.viewer.goToPage(0); return 0;
                case VK_END: app.viewer.goToPage(d->pageCount() - 1); return 0;
                case VK_DOWN: app.viewer.scrollBy(60); return 0;
                case VK_UP: app.viewer.scrollBy(-60); return 0;
                case VK_SPACE: app.viewer.scrollBy(shift ? -400 : 400); return 0;
            }
            return 0;
        }
        case WM_SETFOCUS: return 0;
        case WM_DESTROY:
            app.viewer.setDocument(nullptr);
            app.tabs.clear();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    app.inst = inst;
    SetProcessDPIAware();

    std::wstring err;
    if (!pdfium::load(err)) {
        MessageBoxW(nullptr, err.c_str(), L"Feather PDF could not start", MB_OK | MB_ICONERROR);
        return 1;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> files;
    for (int i = 1; i < argc; ++i) files.push_back(argv[i]);
    LocalFree(argv);

    // One window: if Feather PDF is already running, give it the files and quit.
    HWND existing = FindWindowW(L"FeatherPdfMain", nullptr);
    if (existing) {
        for (auto& f : files) {
            COPYDATASTRUCT cds{1, static_cast<DWORD>(f.size() * sizeof(wchar_t)), const_cast<wchar_t*>(f.c_str())};
            SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
        }
        if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
        SetForegroundWindow(existing);
        return 0;
    }

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    Viewer::registerClass(inst);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FeatherPdfMain";
    RegisterClassExW(&wc);

    HWND w = CreateWindowExW(0, wc.lpszClassName, kAppName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1100, 780, nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show);
    UpdateWindow(w);
    relayoutChildren();
    for (auto& f : files) openFile(f);
    SetFocus(w);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
