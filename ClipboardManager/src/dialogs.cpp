// Dialogs: settings, text editor, password prompt, About, statistics and the picture editor.
#include "app.h"
#include <atomic>
#include <thread>

static int g_dpiD() { HDC dc = GetDC(nullptr); int d = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(nullptr, dc); return d; }
static int S(int v) { return MulDiv(v, g_dpiD(), 96); }

// ================================================================= tiny modal dialog framework
struct Dlg {
    HWND h = nullptr;
    bool done = false;
    int result = 0;
    std::function<void(int id, int code)> onCommand;
    std::map<int, std::vector<std::wstring>> choices;   // selector buttons: id -> items
    std::map<int, int> chosen;
};

static const wchar_t* DLG_CLASS = L"CmDialog";

static LRESULT CALLBACK DlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Dlg* d = (Dlg*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_ERASEBKGND: { RECT rc; GetClientRect(h, &rc); FillRect((HDC)w, &rc, ThemeBrush(0)); return 1; }
    case WM_CTLCOLOREDIT: SetTextColor((HDC)w, g_th.text); SetBkColor((HDC)w, g_th.edit); return (LRESULT)ThemeBrush(2);
    case WM_CTLCOLORSTATIC: SetTextColor((HDC)w, g_th.text); SetBkColor((HDC)w, g_th.bg); return (LRESULT)ThemeBrush(0);
    case WM_DRAWITEM: {
        auto* di = (DRAWITEMSTRUCT*)l;
        if (GetPropW(di->hwndItem, L"cb")) {
            HDC dc = di->hDC;
            RECT r = di->rcItem;
            FillRect(dc, &r, ThemeBrush(0));
            bool on = GetPropW(di->hwndItem, L"on") != nullptr;
            int bs = S(16);
            RECT box = {r.left + 2, (r.top + r.bottom - bs) / 2, r.left + 2 + bs, (r.top + r.bottom + bs) / 2};
            HBRUSH b = CreateSolidBrush(on ? g_th.accent : g_th.edit);
            HPEN p = CreatePen(PS_SOLID, 1, on ? g_th.accent : g_th.sub);
            HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
            RoundRect(dc, box.left, box.top, box.right, box.bottom, S(4), S(4));
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(b);
            DeleteObject(p);
            if (on) {
                HPEN w2 = CreatePen(PS_SOLID, S(2), RGB(255, 255, 255));
                HGDIOBJ o2 = SelectObject(dc, w2);
                MoveToEx(dc, box.left + S(3), box.top + bs / 2, nullptr);
                LineTo(dc, box.left + bs / 2 - 1, box.bottom - S(4));
                LineTo(dc, box.right - S(3), box.top + S(4));
                SelectObject(dc, o2);
                DeleteObject(w2);
            }
            std::wstring t = TextAt(di->hwndItem);
            RECT tr = {box.right + S(8), r.top, r.right, r.bottom};
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, (di->itemState & ODS_DISABLED) ? g_th.sub : g_th.text);
            SelectObject(dc, g_font);
            DrawTextW(dc, t.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            if (di->itemState & ODS_FOCUS) { RECT fr = tr; fr.left -= 2; DrawFocusRect(dc, &fr); }
            return TRUE;
        }
        if (GetPropW(di->hwndItem, L"nav")) {
            HDC dc = di->hDC;
            RECT r = di->rcItem;
            bool on = GetPropW(di->hwndItem, L"on") != nullptr;
            HBRUSH b = CreateSolidBrush(on ? g_th.btnHot : g_th.bg);
            FillRect(dc, &r, b);
            DeleteObject(b);
            if (on) { RECT bar = {r.left, r.top + S(6), r.left + S(3), r.bottom - S(6)}; HBRUSH ab = CreateSolidBrush(g_th.accent); FillRect(dc, &bar, ab); DeleteObject(ab); }
            std::wstring t = TextAt(di->hwndItem);
            RECT tr = {r.left + S(14), r.top, r.right - S(6), r.bottom};
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, g_th.text);
            SelectObject(dc, on ? g_fontBold : g_font);
            DrawTextW(dc, t.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            return TRUE;
        }
        return DrawThemedButton(di, false);
    }
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        HWND c = (HWND)l;
        if (d && c && code == BN_CLICKED) {
            if (GetPropW(c, L"cb")) {
                if (GetPropW(c, L"on")) RemovePropW(c, L"on"); else SetPropW(c, L"on", (HANDLE)1);
                InvalidateRect(c, nullptr, TRUE);
            } else if (d->choices.count(id)) {
                HMENU pm = CreatePopupMenu();
                auto& items = d->choices[id];
                for (size_t i = 0; i < items.size(); i++) AppendMenuW(pm, MF_STRING | ((int)i == d->chosen[id] ? MF_CHECKED : 0), 1 + i, items[i].c_str());
                RECT r;
                GetWindowRect(c, &r);
                int pick = TrackPopupMenu(pm, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, h, nullptr);
                DestroyMenu(pm);
                if (pick > 0) {
                    d->chosen[id] = pick - 1;
                    SetWindowTextW(c, (items[pick - 1] + L"  ▾").c_str());
                    InvalidateRect(c, nullptr, TRUE);
                }
                return 0;
            }
        }
        if (d && d->onCommand) d->onCommand(id, code);
        return 0;
    }
    case WM_APP + 9: if (d) { d->done = true; d->result = 1; } return 0;
    case WM_CLOSE:
        if (d) { d->done = true; d->result = 0; }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void RegisterDlgClass() {
    static bool done = false;
    if (done) return;
    done = true;
    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = DLG_CLASS;
    wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);
}

static HWND Make(HWND p, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, p, (HMENU)(UINT_PTR)id, g_inst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

static Dlg* NewDialog(Dlg& d, HWND parent, const wchar_t* title, int w, int h) {
    RegisterDlgClass();
    RECT pr = {0, 0, 0, 0};
    if (parent && IsWindowVisible(parent)) GetWindowRect(parent, &pr);
    else { pr.right = GetSystemMetrics(SM_CXSCREEN); pr.bottom = GetSystemMetrics(SM_CYSCREEN); }
    int cw = S(w), ch = S(h);
    RECT wr = {0, 0, cw, ch};
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    int x = pr.left + ((pr.right - pr.left) - ww) / 2, y = pr.top + ((pr.bottom - pr.top) - wh) / 2;
    d.h = CreateWindowExW(WS_EX_DLGMODALFRAME, DLG_CLASS, title, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, x, y, ww, wh, parent, nullptr, g_inst, &d);
    BOOL dark = g_th.dark;
    DwmSetWindowAttribute(d.h, 20, &dark, sizeof dark);
    return &d;
}

static void RunModal(Dlg& d, HWND parent, HWND firstFocus = nullptr) {
    if (parent) EnableWindow(parent, FALSE);
    ShowWindow(d.h, SW_SHOW);
    SetForegroundWindow(d.h);
    if (firstFocus) SetFocus(firstFocus);
    MSG msg;
    while (!d.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && GetAncestor(msg.hwnd, GA_ROOT) == d.h) { d.done = true; d.result = 0; break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && GetAncestor(msg.hwnd, GA_ROOT) == d.h) {
            // simple tab navigation
            HWND next = GetNextDlgTabItem(d.h, GetFocus(), GetKeyState(VK_SHIFT) < 0);
            if (next) SetFocus(next);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (parent) EnableWindow(parent, TRUE);
    DestroyWindow(d.h);
    if (parent) SetForegroundWindow(parent);
}

static void SetCheck(HWND c, bool on) { if (on) SetPropW(c, L"on", (HANDLE)1); else RemovePropW(c, L"on"); InvalidateRect(c, nullptr, TRUE); }
static bool GetCheck(HWND c) { return GetPropW(c, L"on") != nullptr; }

static HWND MakeCheck(HWND p, const wchar_t* text, int x, int y, int w, bool on, int id = 0) {
    HWND c = Make(p, L"BUTTON", text, BS_OWNERDRAW | WS_TABSTOP, x, y, w, S(24), id);
    SetPropW(c, L"cb", (HANDLE)1);
    SetCheck(c, on);
    return c;
}

static HWND MakeSel(Dlg& d, HWND p, int id, int x, int y, int w, const std::vector<std::wstring>& items, int cur) {
    HWND c = Make(p, L"BUTTON", (items[cur] + L"  ▾").c_str(), BS_OWNERDRAW | WS_TABSTOP, x, y, w, S(28), id);
    d.choices[id] = items;
    d.chosen[id] = cur;
    return c;
}

// ================================================================= text / password dialogs
static const wchar_t* g_lbl1 = nullptr;
static const wchar_t* g_lbl2 = nullptr;
void SetDialogLabels(const wchar_t* l1, const wchar_t* l2) { g_lbl1 = l1; g_lbl2 = l2; }

bool EditTextDialog(HWND parent, const wchar_t* title, std::wstring* name, std::wstring& text, bool multiline) {
    // Because the window is destroyed after RunModal, read the values through a second pass.
    Dlg d;
    int W = multiline ? 600 : 460, H = multiline ? (name ? 430 : 400) : (name ? 190 : 150);
    NewDialog(d, parent, title, W, H);
    int y = 14;
    HWND hName = nullptr;
    if (name) {
        Make(d.h, L"STATIC", g_lbl1 ? g_lbl1 : T(L"Name"), 0, S(16), S(y), S(W - 32), S(18));
        y += 20;
        hName = Make(d.h, L"EDIT", name->c_str(), ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, S(16), S(y), S(W - 32), S(26));
        y += 36;
        Make(d.h, L"STATIC", g_lbl2 ? g_lbl2 : (multiline ? T(L"Text") : T(L"Value")), 0, S(16), S(y), S(W - 32), S(18));
        y += 20;
    }
    DWORD st = WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL;
    int eh = 26;
    if (multiline) { st = WS_TABSTOP | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL; eh = H - y - 62; }
    else if (!name) y += 6;
    std::wstring shown;
    for (size_t i = 0; i < text.size(); i++) {
        if (multiline && text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r')) shown += L"\r\n";
        else if (multiline && text[i] == L'\r' && (i + 1 >= text.size() || text[i + 1] != L'\n')) shown += L"\r\n";
        else shown += text[i];
    }
    HWND hText = Make(d.h, L"EDIT", shown.c_str(), st, S(16), S(y), S(W - 32), S(eh));
    if (multiline) SendMessageW(hText, EM_SETLIMITTEXT, 0, 0);
    Make(d.h, L"BUTTON", T(L"OK"), BS_OWNERDRAW | WS_TABSTOP, S(W - 190), S(H - 46), S(80), S(30), IDOK);
    Make(d.h, L"BUTTON", T(L"Cancel"), BS_OWNERDRAW | WS_TABSTOP, S(W - 100), S(H - 46), S(84), S(30), IDCANCEL);
    std::wstring outName, outText;
    d.onCommand = [&](int id, int) {
        if (id == IDOK) {
            outText = TextAt(hText);
            if (hName) outName = TextAt(hName);
            d.result = 1;
            d.done = true;
        } else if (id == IDCANCEL) { d.result = 0; d.done = true; }
    };
    g_lbl1 = g_lbl2 = nullptr;
    RunModal(d, parent, hName ? hName : hText);
    if (d.result != 1) return false;
    text = outText;
    if (name) *name = outName;
    return true;
}

bool PasswordPrompt(HWND parent, const wchar_t* title, std::wstring& pw) {
    Dlg d;
    NewDialog(d, parent, title, 440, 140);
    HWND e = Make(d.h, L"EDIT", L"", ES_AUTOHSCROLL | ES_PASSWORD | WS_TABSTOP | WS_BORDER, S(16), S(22), S(408), S(26));
    Make(d.h, L"BUTTON", T(L"OK"), BS_OWNERDRAW | WS_TABSTOP, S(440 - 190), S(140 - 46), S(80), S(30), IDOK);
    Make(d.h, L"BUTTON", T(L"Cancel"), BS_OWNERDRAW | WS_TABSTOP, S(440 - 100), S(140 - 46), S(84), S(30), IDCANCEL);
    std::wstring out;
    d.onCommand = [&](int id, int) {
        if (id == IDOK) { out = TextAt(e); d.result = 1; d.done = true; }
        else if (id == IDCANCEL) { d.result = 0; d.done = true; }
    };
    RunModal(d, parent, e);
    if (d.result != 1) return false;
    pw = out;
    return true;
}

void TextBoxDialog(HWND parent, const wchar_t* title, const std::wstring& text) {
    Dlg d;
    NewDialog(d, parent, title, 520, 400);
    std::wstring crlf;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r')) crlf += L"\r\n";
        else crlf += text[i];
    }
    HWND e = Make(d.h, L"EDIT", crlf.c_str(), ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, S(16), S(16), S(488), S(318));
    (void)e;
    Make(d.h, L"BUTTON", T(L"OK"), BS_OWNERDRAW | WS_TABSTOP, S(520 - 100), S(400 - 46), S(84), S(30), IDOK);
    d.onCommand = [&](int id, int) { if (id == IDOK || id == IDCANCEL) { d.done = true; } };
    RunModal(d, parent);
}

void ShowAbout(HWND parent) {
    std::wstring t = std::wstring(APP_NAME) + L"  " + APP_VERSION_SHORT + L"  (" + APP_VERSION + L")\n© " + APP_YEAR + L" " + APP_AUTHOR + L".\n" +
                     T(L"Developed for faster experience.") + L"\n\n" +
                     T(L"Keeps everything you copy — text, pictures and files — as ordinary files in the Clipboard contents folder. Search, pin and paste any of it again from any program.") + L"\n\n" +
                     T(L"Your history stays on this PC:") + L"\n" + g_store.dir + L"\n\n" +
                     T(L"Nothing you copy is ever sent anywhere. There is no account, no cloud and no telemetry. The only network use is looking for a new version: it reads one small version file when the program starts (you can switch this off in Settings) and never sends anything about you or your history.");
    TextBoxDialog(parent, T(L"About Feather Clipboard Manager"), t);
}

void ShowStats(HWND parent) {
    int text = 0, pics = 0, files = 0, snips = 0, trash = 0, today = 0, pinned = 0;
    uint64_t bytes = 0;
    std::map<std::wstring, int> bySource;
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    lt.tm_hour = lt.tm_min = lt.tm_sec = 0;
    time_t st = mktime(&lt);
    int week = 0;
    for (auto& it : g_store.items) {
        bytes += it.bytes;
        if (it.deleted) { trash++; continue; }
        if (it.kind == K_TEXT) text++;
        else if (it.kind == K_PICTURE) pics++;
        else if (it.kind == K_FILES) files++;
        else snips++;
        if (it.pinned) pinned++;
        if (it.kind != K_SNIPPET) {
            if (it.created >= st) today++;
            if (it.created >= now - 7 * 86400) week++;
            if (!it.source.empty()) bySource[FriendlyName(it.source)]++;
        }
    }
    std::vector<std::pair<int, std::wstring>> top;
    for (auto& s : bySource) top.push_back({s.second, s.first});
    std::sort(top.rbegin(), top.rend());
    std::wstring t;
    t += std::wstring(T(L"Copied today")) + L": " + Num(today) + L"\n";
    t += std::wstring(T(L"Copied in the last 7 days")) + L": " + Num(week) + L"\n\n";
    t += std::wstring(T(L"Text")) + L": " + Num(text) + L"\n" + T(L"Pictures") + L": " + Num(pics) + L"\n" + T(L"Files") + L": " + Num(files) + L"\n" + T(L"Snippets") + L": " + Num(snips) + L"\n";
    t += std::wstring(T(L"Pinned")) + L": " + Num(pinned) + L"\n" + T(L"Recently deleted") + L": " + Num(trash) + L"\n\n";
    t += std::wstring(T(L"Space used")) + L": " + FormatBytes(bytes) + L"\n" + T(L"Folder") + L": " + g_store.dir + L"\n\n";
    t += std::wstring(T(L"Most copied from")) + L":\n";
    for (size_t i = 0; i < top.size() && i < 8; i++) t += L"  " + top[i].second + L"  —  " + Num(top[i].first) + L"\n";
    if (top.empty()) t += L"  —\n";
    TextBoxDialog(parent, T(L"Statistics"), t);
}

// ================================================================= settings
int g_settingsPage = 0;   // page to open first (set by --open-settings=N for screenshots)

void ShowSettingsDialog(HWND parent) {
    Dlg d;
    const int W = 760, H = 560, NAVW = 170;
    NewDialog(d, parent, T(L"Settings"), W, H);
    const wchar_t* pageNames[] = {T(L"General"), T(L"What to keep"), T(L"Privacy"), T(L"Shortcuts"), T(L"Appearance"), T(L"Text tools"), T(L"Backup")};
    const int NP = 7;
    std::vector<HWND> pages[NP];
    HWND navBtn[NP];
    int cur = 0;
    for (int i = 0; i < NP; i++) {
        navBtn[i] = Make(d.h, L"BUTTON", pageNames[i], BS_OWNERDRAW | WS_TABSTOP, 0, S(12 + i * 38), S(NAVW), S(34), 500 + i);
        SetPropW(navBtn[i], L"nav", (HANDLE)1);
    }
    int X = NAVW + 24, CW = W - NAVW - 48;
    auto add = [&](int page, HWND c) { pages[page].push_back(c); return c; };
    auto label = [&](int page, const std::wstring& s, int y, int w = 0, bool bold = false) {
        HWND c = Make(d.h, L"STATIC", s.c_str(), 0, S(X), S(y), S(w ? w : CW), S(40));
        if (bold) SendMessageW(c, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
        return add(page, c);
    };
    auto oneLine = [&](int page, const std::wstring& s, int y, int w) { HWND c = Make(d.h, L"STATIC", s.c_str(), SS_LEFTNOWORDWRAP, S(X), S(y + 4), S(w), S(20)); return add(page, c); };
    auto numEdit = [&](int page, int val, int x, int y, int w) {
        HWND c = Make(d.h, L"EDIT", std::to_wstring(val).c_str(), ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, S(x), S(y), S(w), S(26));
        SendMessageW(c, EM_SETLIMITTEXT, 8, 0);
        return add(page, c);
    };

    // ---- page 0: General
    int y = 20;
    auto chk = [&](int page, const wchar_t* t, bool on) { HWND c = MakeCheck(d.h, t, S(X), S(y), S(CW), on); y += 34; return add(page, c); };
    HWND cAutostart = chk(0, T(L"Start with Windows"), g_set.autostart);
    HWND cTray = chk(0, T(L"...in the tray, without the window"), g_set.startInTray);
    HWND cClose = chk(0, T(L"Closing the window keeps it recording in the tray"), g_set.closeToTray);
    HWND cEnter = chk(0, T(L"Enter or a double-click pastes into the program you were using"), g_set.pasteOnEnter);
    HWND cPopup = chk(0, T(L"Open the window next to the cursor or typing position"), g_set.popupAtCursor);
    HWND cSound = chk(0, T(L"Play a sound when something is copied"), g_set.sound);
    HWND cToast = chk(0, T(L"Show a small notification when something is copied"), g_set.toast);
    HWND cUpd = chk(0, T(L"Look for new versions when it starts (reads only a small version file)"), g_set.checkUpdates);
    y += 6;
    oneLine(0, T(L"Language:"), y, 120);
    std::vector<std::wstring> langs;
    for (int i = 0; i < LANG_COUNT; i++) langs.push_back(LanguageName(i));
    HWND selLang = add(0, MakeSel(d, d.h, 600, S(X) + S(130), S(y), S(200), langs, g_set.language));
    y += 46;
    add(0, Make(d.h, L"BUTTON", T(L"Update to new version\u2026"), BS_OWNERDRAW | WS_TABSTOP, S(X), S(y), S(220), S(30), 703));
    y += 44;
    label(0, T(L"Your history is kept in this folder (it is not removed when you uninstall):"), y);
    y += 22;
    label(0, g_store.dir, y);

    // ---- page 1: What to keep
    y = 20;
    label(1, T(L"Text is always kept. Also keep:"), y, 0, true);
    y += 30;
    HWND cPics = chk(1, T(L"Pictures (screenshots, images copied from programs and web pages)"), g_set.recordImages);
    HWND cFiles = chk(1, T(L"Copied files (the list of their names and folders)"), g_set.recordFiles);
    HWND cRich = chk(1, T(L"Formatting of copied text (bold, colours, tables from Word and web pages)"), g_set.recordRich);
    HWND cOcr = chk(1, T(L"Read the text in pictures, so pictures can be found by their words"), g_set.ocr);
    y += 10;
    oneLine(1, T(L"Delete unpinned items after"), y, 230);
    HWND eDays = numEdit(1, g_set.deleteAfterDays, X + 235, y, 70);
    oneLine(1, T(L"days   (0: keep everything forever)"), y, 0);
    SetWindowPos(pages[1].back(), nullptr, S(X + 315), S(y + 4), S(CW - 315), S(20), SWP_NOZORDER);
    y += 40;
    oneLine(1, T(L"Keep at most"), y, 230);
    HWND eMax = numEdit(1, g_set.maxItems, X + 235, y, 70);
    oneLine(1, T(L"items   (0: no limit)"), y, 0);
    SetWindowPos(pages[1].back(), nullptr, S(X + 315), S(y + 4), S(CW - 315), S(20), SWP_NOZORDER);
    y += 40;
    oneLine(1, T(L"Use at most"), y, 230);
    HWND eMB = numEdit(1, g_set.maxMB, X + 235, y, 70);
    oneLine(1, T(L"MB of disk space   (0: no limit)"), y, 0);
    SetWindowPos(pages[1].back(), nullptr, S(X + 315), S(y + 4), S(CW - 315), S(20), SWP_NOZORDER);
    y += 40;
    oneLine(1, T(L"Do not keep a single copy larger than"), y, 280);
    HWND eEntry = numEdit(1, g_set.maxEntryMB, X + 285, y, 70);
    oneLine(1, T(L"MB  (0: no limit)"), y, 0);
    SetWindowPos(pages[1].back(), nullptr, S(X + 365), S(y + 4), S(CW - 365), S(20), SWP_NOZORDER);
    y += 46;
    label(1, T(L"Pinned items and snippets are always kept. Deleted items stay in Recently deleted for 7 days."), y);
    y += 44;
    HWND bFolder = add(1, Make(d.h, L"BUTTON", T(L"Open folder"), BS_OWNERDRAW | WS_TABSTOP, S(X), S(y), S(140), S(30), 700));
    (void)bFolder;

    // ---- page 2: Privacy
    y = 20;
    label(2, T(L"When a copy looks like a card number, password, API key or private key:"), y, 0, true);
    y += 32;
    std::vector<std::wstring> sens = {T(L"Keep it like anything else"), T(L"Do not record it"), T(L"Record it, then delete it automatically")};
    HWND selSens = add(2, MakeSel(d, d.h, 601, S(X), S(y), S(380), sens, g_set.sensitive));
    y += 40;
    oneLine(2, T(L"Delete it after"), y, 130);
    HWND eExp = numEdit(2, g_set.expireSeconds, X + 135, y, 70);
    oneLine(2, T(L"seconds"), y, 100);
    SetWindowPos(pages[2].back(), nullptr, S(X + 215), S(y + 4), S(120), S(20), SWP_NOZORDER);
    y += 46;
    label(2, T(L"Never record what is copied from these programs (one file name per line, for example KeePass.exe):"), y, 0, true);
    y += 44;
    std::wstring ign;
    for (size_t i = 0; i < g_set.ignoreList.size(); i++) {
        if (g_set.ignoreList[i] == L'\n' && (i == 0 || g_set.ignoreList[i - 1] != L'\r')) ign += L"\r\n"; else ign += g_set.ignoreList[i];
    }
    HWND eIgnore = add(2, Make(d.h, L"EDIT", ign.c_str(), ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP | WS_BORDER, S(X), S(y), S(CW), S(150)));
    y += 158;
    label(2, T(L"Programs that mark a copy as private (most password managers do) are always skipped, whatever this list says."), y);
    y += 46;
    HWND cMask = chk(2, T(L"Mask emails, phone numbers and tokens in the list and the preview"), g_set.maskPreviews);
    HWND cLog = chk(2, T(L"Keep a local diagnostic log (errors only, never what you copied)"), g_set.logging);

    // ---- page 3: Shortcuts
    y = 20;
    oneLine(3, T(L"Open the history with:"), y, 250);
    std::vector<std::wstring> hk = {L"Ctrl+Shift+V", L"Ctrl+Alt+V", L"Win+Shift+V", T(L"None")};
    HWND selHK = add(3, MakeSel(d, d.h, 602, S(X) + S(260), S(y), S(190), hk, g_set.hotkey));
    y += 44;
    oneLine(3, T(L"Paste the newest item as plain text:"), y, 250);
    std::vector<std::wstring> hk2 = {L"Ctrl+Alt+P", L"Ctrl+Alt+Shift+V", L"Win+Alt+V", T(L"None")};
    HWND selHK2 = add(3, MakeSel(d, d.h, 603, S(X) + S(260), S(y), S(190), hk2, g_set.hotkeyPlain));
    y += 44;
    HWND cQuick = chk(3, T(L"Ctrl+Alt+1 to Ctrl+Alt+9 paste the 1st to 9th newest item from any program"), g_set.quickNumbers);
    y += 10;
    label(3, T(L"In the window: Enter pastes, Shift+Enter pastes as plain text, Ctrl+1 to Ctrl+9 pastes the Nth item, Ctrl+C copies, Ctrl+P pins, Ctrl+E edits, Ctrl+N makes a snippet, Ctrl+Z undoes a delete, Ctrl+F searches, Del deletes, Esc hides."), y);
    SetWindowPos(pages[3].back(), nullptr, S(X), S(y), S(CW), S(90), SWP_NOZORDER);

    // ---- page 4: Appearance
    y = 20;
    oneLine(4, T(L"Theme:"), y, 120);
    std::vector<std::wstring> th = {T(L"Same as Windows"), T(L"Light"), T(L"Dark")};
    HWND selTheme = add(4, MakeSel(d, d.h, 604, S(X) + S(130), S(y), S(200), th, g_set.theme));
    y += 50;
    oneLine(4, T(L"Highlight colour:"), y, 160);
    std::vector<std::wstring> acc = {T(L"Blue"), T(L"Green"), T(L"Purple"), T(L"Orange"), T(L"Red")};
    HWND selAcc = add(4, MakeSel(d, d.h, 605, S(X) + S(170), S(y), S(160), acc, g_set.accent));
    y += 46;
    oneLine(4, T(L"Text size:"), y, 160);
    std::vector<std::wstring> fsz = {T(L"Small"), T(L"Normal"), T(L"Large")};
    HWND selFont = add(4, MakeSel(d, d.h, 606, S(X) + S(170), S(y), S(160), fsz, g_set.fontSize));
    y += 46;
    oneLine(4, T(L"Length of each line in the list:"), y, 250);
    std::vector<std::wstring> pl = {T(L"Short"), T(L"Medium"), T(L"Long")};
    HWND selPrev = add(4, MakeSel(d, d.h, 607, S(X) + S(260), S(y), S(160), pl, g_set.previewLen));
    y += 50;
    HWND cCompact = chk(4, T(L"Compact view: only the list, without the preview"), g_set.compact);
    HWND cTop = chk(4, T(L"Keep the window on top of other windows"), g_set.alwaysOnTop);

    // ---- page 5: Text tools
    y = 20;
    label(5, T(L"Pipelines: a saved chain of steps you can paste through (right-click \u25B8 Paste through). One per line:  Name = step | step | step"), y, 0, true);
    y += 52;
    std::wstring pipes;
    for (size_t i = 0; i < g_set.pipelines.size(); i++) {
        if (g_set.pipelines[i] == L'\n' && (i == 0 || g_set.pipelines[i - 1] != L'\r')) pipes += L"\r\n"; else pipes += g_set.pipelines[i];
    }
    HWND ePipes = add(5, Make(d.h, L"EDIT", pipes.c_str(), ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP | WS_BORDER, S(X), S(y), S(CW), S(150)));
    y += 160;
    label(5, T(L"Steps: upper lower title sentence trim blanklines oneline sort unique reverse number b64 unb64 json jsonmin xml sql slashes backslashes sha256 urlencode urldecode, and  find:old=>new"), y);
    SetWindowPos(pages[5].back(), nullptr, S(X), S(y), S(CW), S(70), SWP_NOZORDER);

    // ---- page 6: Backup
    y = 20;
    label(6, T(L"Backup the whole history into one file, optionally protected by a password, and bring it back on another PC."), y);
    y += 56;
    add(6, Make(d.h, L"BUTTON", T(L"Export backup…"), BS_OWNERDRAW | WS_TABSTOP, S(X), S(y), S(170), S(32), 701));
    add(6, Make(d.h, L"BUTTON", T(L"Import backup…"), BS_OWNERDRAW | WS_TABSTOP, S(X) + S(184), S(y), S(170), S(32), 702));

    HWND bOk = Make(d.h, L"BUTTON", T(L"OK"), BS_OWNERDRAW | WS_TABSTOP, S(W - 190), S(H - 46), S(84), S(30), IDOK);
    HWND bCancel = Make(d.h, L"BUTTON", T(L"Cancel"), BS_OWNERDRAW | WS_TABSTOP, S(W - 98), S(H - 46), S(84), S(30), IDCANCEL);
    (void)bOk; (void)bCancel;

    auto showPage = [&](int p) {
        cur = p;
        for (int i = 0; i < NP; i++) {
            for (HWND c : pages[i]) ShowWindow(c, i == p ? SW_SHOW : SW_HIDE);
            if (i == p) SetPropW(navBtn[i], L"on", (HANDLE)1); else RemovePropW(navBtn[i], L"on");
            InvalidateRect(navBtn[i], nullptr, TRUE);
        }
    };
    showPage(std::clamp(g_settingsPage, 0, NP - 1));
    g_settingsPage = 0;

    auto num = [&](HWND e) { int v = _wtoi(TextAt(e).c_str()); return v < 0 ? 0 : v; };
    d.onCommand = [&](int id, int) {
        if (id >= 500 && id < 500 + NP) { showPage(id - 500); return; }
        if (id == 700) OpenShell(d.h, g_store.dir);
        else if (id == 701) { EnableWindow(d.h, FALSE); SendMessageW(g_main, WM_COMMAND, CMD_EXPORT, 0); EnableWindow(d.h, TRUE); }
        else if (id == 702) { EnableWindow(d.h, FALSE); SendMessageW(g_main, WM_COMMAND, CMD_IMPORT, 0); EnableWindow(d.h, TRUE); }
        else if (id == 703) { EnableWindow(d.h, FALSE); SendMessageW(g_main, WM_COMMAND, CMD_UPDATE, 0); EnableWindow(d.h, TRUE); }
        else if (id == IDCANCEL) { d.result = 0; d.done = true; }
        else if (id == IDOK) {
            Settings n = g_set;
            n.startInTray = GetCheck(cTray);
            n.closeToTray = GetCheck(cClose);
            n.pasteOnEnter = GetCheck(cEnter);
            n.popupAtCursor = GetCheck(cPopup);
            n.sound = GetCheck(cSound);
            n.toast = GetCheck(cToast);
            n.checkUpdates = GetCheck(cUpd);
            n.language = d.chosen[600];
            n.recordImages = GetCheck(cPics);
            n.recordFiles = GetCheck(cFiles);
            n.recordRich = GetCheck(cRich);
            n.ocr = GetCheck(cOcr);
            n.deleteAfterDays = std::min(num(eDays), 36500);
            n.maxItems = std::min(num(eMax), 10000000);
            n.maxMB = std::min(num(eMB), 10000000);
            n.sensitive = d.chosen[601];
            n.expireSeconds = std::clamp(num(eExp), 5, 86400);
            n.ignoreList = TextAt(eIgnore);
            n.hotkey = d.chosen[602];
            n.hotkeyPlain = d.chosen[603];
            n.quickNumbers = GetCheck(cQuick);
            n.theme = d.chosen[604];
            n.compact = GetCheck(cCompact);
            n.alwaysOnTop = GetCheck(cTop);
            n.accent = d.chosen[605];
            n.fontSize = d.chosen[606];
            n.previewLen = d.chosen[607];
            n.maxEntryMB = std::min(num(eEntry), 100000);
            n.maskPreviews = GetCheck(cMask);
            n.logging = GetCheck(cLog);
            n.pipelines = TextAt(ePipes);
            // confirm destructive limits
            if ((n.deleteAfterDays > 0 && (g_set.deleteAfterDays == 0 || n.deleteAfterDays < g_set.deleteAfterDays)) ||
                (n.maxItems > 0 && (g_set.maxItems == 0 || n.maxItems < g_set.maxItems)) || (n.maxMB > 0 && (g_set.maxMB == 0 || n.maxMB < g_set.maxMB))) {
                if (!MsgAsk(d.h, T(L"Older unpinned items beyond these limits will be deleted, now and from then on. Pinned items are always kept. Continue?"))) return;
            }
            bool wantAuto = GetCheck(cAutostart);
            g_set = n;
            if (wantAuto != g_set.autostart) {
                HKEY k;
                if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
                    if (wantAuto) { std::wstring v = L"\"" + ExePath() + L"\" /tray"; RegSetValueExW(k, L"FeatherClipboardManager", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * 2)); }
                    else RegDeleteValueW(k, L"FeatherClipboardManager");
                    RegCloseKey(k);
                    g_set.autostart = wantAuto;
                }
            }
            d.result = 1;
            d.done = true;
        }
    };
    (void)selLang; (void)selSens; (void)selHK; (void)selHK2; (void)selTheme; (void)selAcc; (void)selFont; (void)selPrev;
    RunModal(d, parent);
    if (d.result == 1) SettingsChanged();
}

// ================================================================= updates
bool UpdatePrompt(HWND parent, const UpdateInfo& u, bool& skipThis) {
    Dlg d;
    NewDialog(d, parent, (std::wstring(L"A new version of ") + APP_NAME).c_str(), 560, 330);
    wchar_t mb[32];
    _snwprintf(mb, 32, L"%.1f MB", u.rel.size / 1048576.0);
    std::wstring ver = core::FromUtf8(u.rel.version);
    std::wstring t = std::wstring(APP_NAME) + L" " + ver + L" " + T(L"is out") + (u.rel.date.empty() ? L"" : L" (" + core::FromUtf8(u.rel.date) + L")") + L". " + T(L"You have") + L" " + APP_VERSION + L".\r\n\r\n";
    if (!u.rel.notes.empty()) {
        std::wstring n;
        for (size_t i = 0; i < u.rel.notes.size(); i++) { if (u.rel.notes[i] == L'\n' && (i == 0 || u.rel.notes[i - 1] != L'\r')) n += L"\r\n"; else n += u.rel.notes[i]; }
        t += std::wstring(T(L"What is new:")) + L"\r\n" + n + L"\r\n\r\n";
    }
    t += std::wstring(T(L"Update now downloads it")) + L" (" + mb + L"), " + T(L"closes the program, installs the new version in the same place and starts it again. Your settings and your history stay. No administrator permission is needed.") + L"\r\n\r\n" + T(L"Later: update another time.");
    Make(d.h, L"EDIT", t.c_str(), ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_TABSTOP, S(16), S(16), S(528), S(214));
    HWND skip = MakeCheck(d.h, T(L"Do not remind me about this version"), S(16), S(238), S(528), false);
    Make(d.h, L"BUTTON", T(L"Update now"), BS_OWNERDRAW | WS_TABSTOP, S(560 - 230), S(330 - 46), S(120), S(30), IDOK);
    Make(d.h, L"BUTTON", T(L"Later"), BS_OWNERDRAW | WS_TABSTOP, S(560 - 100), S(330 - 46), S(84), S(30), IDCANCEL);
    d.onCommand = [&](int id, int) {
        if (id == IDOK) { skipThis = GetCheck(skip); d.result = 1; d.done = true; }
        else if (id == IDCANCEL) { skipThis = GetCheck(skip); d.result = 0; d.done = true; }
    };
    RunModal(d, parent);
    return d.result == 1;
}

namespace {
struct ProgressState { std::atomic<long long> done{0}, total{0}; std::atomic<bool> stop{false}; HWND label = nullptr, bar = nullptr; std::wstring title; };
ProgressState* g_prog = nullptr;
VOID CALLBACK ProgressTimer(HWND, UINT, UINT_PTR, DWORD) {
    if (!g_prog) return;
    long long dn = g_prog->done, tt = g_prog->total;
    wchar_t buf[200];
    if (tt > 0) _snwprintf(buf, 200, L"%ls  %.1f of %.1f MB", g_prog->title.c_str(), dn / 1048576.0, tt / 1048576.0);
    else _snwprintf(buf, 200, L"%ls  %.1f MB", g_prog->title.c_str(), dn / 1048576.0);
    SetWindowTextW(g_prog->label, buf);
    SendMessageW(g_prog->bar, PBM_SETPOS, tt > 0 ? (WPARAM)(dn * 1000 / tt) : 0, 0);
}
}  // namespace

bool DownloadWithProgress(HWND parent, const std::wstring& title, const std::string& url, const std::string& via, std::string& out, std::wstring& err) {
    Dlg d;
    NewDialog(d, parent, APP_NAME, 480, 150);
    ProgressState ps;
    ps.title = title;
    ps.label = Make(d.h, L"STATIC", title.c_str(), 0, S(16), S(16), S(448), S(20));
    ps.bar = Make(d.h, PROGRESS_CLASSW, L"", PBS_SMOOTH, S(16), S(46), S(448), S(18));
    SendMessageW(ps.bar, PBM_SETRANGE32, 0, 1000);
    Make(d.h, L"BUTTON", T(L"Cancel"), BS_OWNERDRAW | WS_TABSTOP, S(480 - 100), S(150 - 46), S(84), S(30), IDCANCEL);
    bool ok = false;
    g_prog = &ps;
    std::thread th([&] {
        ok = UpdateDownload(url, out, err, via, [&](long long dn, long long tt) { ps.done = dn; ps.total = tt; return !ps.stop.load(); });
        PostMessageW(d.h, WM_APP + 9, 0, 0);
    });
    d.onCommand = [&](int id, int) { if (id == IDCANCEL) ps.stop = true; };
    SetTimer(d.h, 77, 120, ProgressTimer);
    RunModal(d, parent);
    ps.stop = true;
    th.join();
    g_prog = nullptr;
    return ok;
}

// ================================================================= tools that only show information
static std::wstring FormatName(UINT f) {
    switch (f) {
    case CF_TEXT: return L"CF_TEXT"; case CF_BITMAP: return L"CF_BITMAP"; case CF_METAFILEPICT: return L"CF_METAFILEPICT"; case CF_SYLK: return L"CF_SYLK";
    case CF_DIF: return L"CF_DIF"; case CF_TIFF: return L"CF_TIFF"; case CF_OEMTEXT: return L"CF_OEMTEXT"; case CF_DIB: return L"CF_DIB";
    case CF_PALETTE: return L"CF_PALETTE"; case CF_UNICODETEXT: return L"CF_UNICODETEXT"; case CF_ENHMETAFILE: return L"CF_ENHMETAFILE";
    case CF_HDROP: return L"CF_HDROP"; case CF_LOCALE: return L"CF_LOCALE"; case CF_DIBV5: return L"CF_DIBV5";
    }
    wchar_t n[128] = L"";
    if (GetClipboardFormatNameW(f, n, 128)) return n;
    return L"(format " + std::to_wstring(f) + L")";
}

void ShowClipboardFormats(HWND parent) {
    std::wstring t = std::wstring(T(L"What is on the clipboard right now (for troubleshooting). Only the format names and sizes are read; nothing is interpreted.")) + L"\r\n\r\n";
    if (!OpenClipboard(g_main)) { t += T(L"The clipboard is in use by another program. Try again."); TextBoxDialog(parent, T(L"Clipboard format inspector"), t); return; }
    HWND owner = GetClipboardOwner();
    t += std::wstring(T(L"Owner")) + L": " + (owner ? ProcessNameOf(owner) : std::wstring(L"?")) + L"\r\n";
    t += std::wstring(T(L"Sequence number")) + L": " + std::to_wstring(GetClipboardSequenceNumber()) + L"\r\n\r\n";
    UINT f = 0;
    int n = 0;
    while ((f = EnumClipboardFormats(f)) != 0 && n < 100) {
        n++;
        std::wstring line = FormatName(f);
        bool handleType = f == CF_BITMAP || f == CF_PALETTE || f == CF_METAFILEPICT || f == CF_ENHMETAFILE;
        if (!handleType) {
            HANDLE h = GetClipboardData(f);
            if (h) line += L"   " + FormatBytes((uint64_t)GlobalSize(h));
            else line += L"   (rendered on demand)";
        }
        t += line + L"\r\n";
    }
    CloseClipboard();
    if (!n) t += T(L"The clipboard is empty.");
    TextBoxDialog(parent, T(L"Clipboard format inspector"), t);
}

void ShowShortcuts(HWND parent) {
    std::wstring t = std::wstring(T(L"From any program")) + L"\r\n"
        L"  Ctrl+Shift+V            " + T(L"open or hide the history (can be changed)") + L"\r\n"
        L"  Ctrl+Alt+P              " + T(L"paste the newest item as plain text") + L"\r\n"
        L"  Ctrl+Alt+1 \u2026 9        " + T(L"paste the 1st \u2026 9th newest item") + L"\r\n"
        L"  Ctrl+Alt+N              " + T(L"paste the next item of the paste queue") + L"\r\n\r\n" +
        T(L"In the window") + L"\r\n"
        L"  \u2191 \u2193 PgUp PgDn        " + T(L"move through the history") + L"\r\n"
        L"  Enter                   " + T(L"paste (or copy, see Settings)") + L"\r\n"
        L"  Shift+Enter             " + T(L"paste as plain text") + L"\r\n"
        L"  Ctrl+1 \u2026 9            " + T(L"paste the Nth item in the list") + L"\r\n"
        L"  Ctrl+C                  " + T(L"copy without pasting") + L"\r\n"
        L"  Ctrl+F                  " + T(L"search") + L"\r\n"
        L"  Ctrl+P                  " + T(L"pin or unpin") + L"\r\n"
        L"  Ctrl+E                  " + T(L"edit the text or the picture") + L"\r\n"
        L"  Ctrl+N                  " + T(L"new snippet") + L"\r\n"
        L"  Ctrl+Q                  " + T(L"add to the paste queue") + L"\r\n"
        L"  Ctrl+Z                  " + T(L"undo a delete") + L"\r\n"
        L"  Ctrl+A                  " + T(L"select all") + L"\r\n"
        L"  Del                     " + T(L"delete") + L"\r\n"
        L"  Ctrl+,                  " + T(L"settings") + L"\r\n"
        L"  Esc                     " + T(L"clear the search, then hide the window") + L"\r\n\r\n" +
        T(L"Searching") + L"\r\n"
        L"  words                   " + T(L"every word must be found") + L"\r\n"
        L"  \"exact phrase\"        " + T(L"the words in this order") + L"\r\n"
        L"  ~colour                 " + T(L"tolerates a typo") + L"\r\n"
        L"  re:^inv\\d+            " + T(L"regular expression") + L"\r\n"
        L"  app:chrome              " + T(L"copied from this program") + L"\r\n"
        L"  type:link               " + T(L"text picture files snippet link email phone colour path json") + L"\r\n"
        L"  tag:work                " + T(L"in this group") + L"\r\n"
        L"  date:today  date:7d  date:2026-11-13   " + T(L"when it was copied") + L"\r\n";
    TextBoxDialog(parent, T(L"Keyboard shortcuts"), t);
}
