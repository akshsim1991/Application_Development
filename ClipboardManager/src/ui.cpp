// The main window of Feather Clipboard Manager.
#include "app.h"
#include <cwctype>
#include <memory>

HINSTANCE g_inst;
HWND g_main;
Theme g_th;
HFONT g_font, g_fontBold, g_fontSmall, g_fontIcon;
static int g_dpi = 96;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

static const wchar_t* CLS_MAIN = L"FeatherClipboardManagerMain";
static const wchar_t* MUTEX_NAME = L"Local\\FeatherClipboardManager.SingleInstance";

// ---------------------------------------------------------------- ids
enum {
    IDC_SEARCH = 100, IDC_FILTER, IDC_LIST, IDC_PREVIEW, IDC_PREVTEXT, IDC_STATUS,
    IDC_B_PASTE, IDC_B_COPY, IDC_B_PIN, IDC_B_DELETE, IDC_B_FOLDER, IDC_B_MORE,
};
enum {
    IDM_PASTE = 1000, IDM_PASTE_PLAIN, IDM_COPY, IDM_COPY_NAMES, IDM_OPEN, IDM_SAVEAS, IDM_SHOWFOLDER, IDM_PIN, IDM_DELETE,
    IDM_EDIT, IDM_EDIT_PICTURE, IDM_OCR_COPY, IDM_SNIPPET_FROM, IDM_SNIPPET_NEW, IDM_RESTORE, IDM_DELETE_FOREVER, IDM_EMPTY_TRASH,
    IDM_SELECTALL, IDM_PAUSE, IDM_DELALL, IDM_UNDO, IDM_SETTINGS, IDM_ABOUT, IDM_EXIT, IDM_HIDE, IDM_OPENFOLDER, IDM_AUTOSTART,
    IDM_STATS, IDM_COMPACT, IDM_EXPORT = CMD_EXPORT, IDM_IMPORT = CMD_IMPORT, IDM_UPDATE = CMD_UPDATE, IDM_SHOW, IDM_OPEN_LINK, IDM_OPEN_MAIL, IDM_OPEN_PATH, IDM_ACTUAL,
    IDM_GROUP_NONE = 1100, IDM_GROUP_NEW,
    IDM_XF_BASE = 1200,       // + core::Xf
    IDM_JOIN_BASE = 1230,     // + 0..4
    IDM_FILTER_BASE = 1300,   // + filter id
    IDM_GROUP_BASE = 1400,    // + index (groups, paste-group, filter-group share the vector)
    IDM_FGROUP_BASE = 1500,
    IDM_RECENT_BASE = 1600,
    IDM_PIPE_BASE = 1700,
    IDM_FINDREPL = 1750, IDM_TEMPLATE, IDM_QUEUE_ADD, IDM_QUEUE_CLEAR, IDM_DIFF, IDM_UNICODE, IDM_TABLE, IDM_FORMATS, IDM_SHORTCUTS, IDM_TOPMOST, IDM_INTEGRITY, IDM_LOG, IDM_FAV, IDM_CLEARALL,
};
enum Filter { FL_ALL = 0, FL_TEXT, FL_PICS, FL_FILES, FL_PINNED, FL_TODAY, FL_SNIPPETS, FL_TRASH, FL_LINKS, FL_EMAILS, FL_PHONES, FL_COLOURS, FL_PATHS, FL_JSON, FL_GROUP, FL_COUNT };
enum { TM_CAPTURE = 1, TM_PASTE, TM_CLEAN, TM_SEARCH, TM_STATUS, TM_HOTKEYS, TM_UPDATE };
enum { HK_OPEN = 1, HK_PLAIN = 2, HK_QUEUE = 3, HK_QUICK = 10 };

static HWND hSearch, hFilterBtn, hList, hPrev, hPrevText, hStatus, hTip;
static HWND hBtn[6];
static const int BTN_IDS[6] = {IDC_B_PASTE, IDC_B_COPY, IDC_B_PIN, IDC_B_DELETE, IDC_B_FOLDER, IDC_B_MORE};
static std::vector<int> view;                      // indexes into g_store.items, newest first
static int curFilter = FL_ALL;
static std::wstring curGroup, curQuery;
static HWND g_prevWnd;
static std::wstring g_prevName;
static bool g_paused = false;
static std::vector<uint64_t> g_lastDeleted;
static std::wstring g_status;
static UINT WM_TASKBARCREATED;
static Gdiplus::Bitmap* g_prevBmp;
static uint64_t g_prevBmpId;
static bool g_actualSize;
static std::vector<std::wstring> g_menuGroups;
static std::vector<uint64_t> g_recentIds;
static std::vector<std::wstring> g_hlNeedles;               // words of the current search, highlighted in the list
static std::vector<uint64_t> g_queue;                       // paste queue, oldest first
static std::function<std::wstring(const std::wstring&)> g_customXf;   // used by paste mode 5 (find/replace, pipelines, templates)
static HBRUSH brBg, brPanel, brEdit;
static bool g_splitDrag;
static std::wstring g_startupHotkeyProblem;
static bool g_inTray = false;
static bool g_iconFontOk = false;
static HICON g_icoOn, g_icoOff, g_icoBig;
static UpdateInfo* g_pendingUpdate = nullptr;      // a newer version found while the window was hidden
static void RunUpdateFlow(const UpdateInfo& info);

static void Rebuild(bool keepSelection = true);
static void UpdatePreview();
static void UpdateStatus();
static void Layout();
static void SetStatus(const std::wstring& s, int ms = 4000);
static void SetButtonGlyphs();

// ---------------------------------------------------------------- theme
bool SystemUsesDark() {
    DWORD v = 1, n = sizeof v;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &n);
    return v == 0;
}

typedef int(WINAPI* SetPreferredAppModeFn)(int);
typedef void(WINAPI* FlushMenuThemesFn)();

void ApplyTheme(bool) {
    bool dark = g_set.theme == 2 || (g_set.theme == 0 && SystemUsesDark());
    g_th.dark = dark;
    if (dark) {
        g_th.bg = RGB(32, 32, 32); g_th.panel = RGB(40, 40, 42); g_th.text = RGB(236, 236, 236); g_th.sub = RGB(160, 160, 165);
        g_th.border = RGB(66, 66, 70); g_th.accent = RGB(96, 176, 255); g_th.sel = RGB(0, 95, 184); g_th.selText = RGB(255, 255, 255);
        g_th.btn = RGB(52, 52, 56); g_th.btnHot = RGB(70, 70, 76); g_th.edit = RGB(48, 48, 51);
    } else {
        g_th.bg = RGB(243, 243, 243); g_th.panel = RGB(255, 255, 255); g_th.text = RGB(28, 28, 28); g_th.sub = RGB(100, 100, 104);
        g_th.border = RGB(212, 212, 216); g_th.accent = RGB(0, 103, 192); g_th.sel = RGB(204, 228, 247); g_th.selText = RGB(0, 0, 0);
        g_th.btn = RGB(252, 252, 252); g_th.btnHot = RGB(230, 230, 232); g_th.edit = RGB(255, 255, 255);
    }
    static const COLORREF accL[] = {RGB(0, 103, 192), RGB(16, 137, 62), RGB(136, 23, 152), RGB(202, 80, 16), RGB(196, 43, 28)};
    static const COLORREF accD[] = {RGB(96, 176, 255), RGB(80, 200, 120), RGB(190, 120, 230), RGB(255, 160, 80), RGB(255, 120, 110)};
    g_th.accent = dark ? accD[g_set.accent] : accL[g_set.accent];
    HIGHCONTRASTW hc = {sizeof hc};
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON)) {   // Windows contrast themes
        g_th.bg = g_th.panel = g_th.edit = GetSysColor(COLOR_WINDOW);
        g_th.text = g_th.selText = GetSysColor(COLOR_WINDOWTEXT);
        g_th.sub = GetSysColor(COLOR_GRAYTEXT);
        g_th.border = GetSysColor(COLOR_WINDOWTEXT);
        g_th.accent = GetSysColor(COLOR_HOTLIGHT);
        g_th.sel = GetSysColor(COLOR_HIGHLIGHT);
        g_th.btn = GetSysColor(COLOR_BTNFACE);
        g_th.btnHot = GetSysColor(COLOR_HIGHLIGHT);
    }
    if (brBg) DeleteObject(brBg);
    if (brPanel) DeleteObject(brPanel);
    if (brEdit) DeleteObject(brEdit);
    brBg = CreateSolidBrush(g_th.bg);
    brPanel = CreateSolidBrush(g_th.panel);
    brEdit = CreateSolidBrush(g_th.edit);
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (ux) {
        auto setMode = (SetPreferredAppModeFn)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        auto flush = (FlushMenuThemesFn)GetProcAddress(ux, MAKEINTRESOURCEA(136));
        if (setMode) setMode(g_set.theme == 0 ? 1 : (dark ? 2 : 3));
        if (flush) flush();
    }
    if (g_main) {
        BOOL d = dark;
        DwmSetWindowAttribute(g_main, 20, &d, sizeof d);
        DwmSetWindowAttribute(g_main, 19, &d, sizeof d);
    }
    if (hList) {
        const wchar_t* theme = dark ? L"DarkMode_Explorer" : L"Explorer";
        SetWindowTheme(hList, theme, nullptr);
        SetWindowTheme(hPrevText, theme, nullptr);
        ListView_SetBkColor(hList, g_th.panel);
        ListView_SetTextBkColor(hList, g_th.panel);
        ListView_SetTextColor(hList, g_th.text);
        InvalidateRect(g_main, nullptr, TRUE);
        InvalidateRect(hList, nullptr, TRUE);
    }
}

static bool HasFont(const wchar_t* face) {
    HDC dc = GetDC(nullptr);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, face);
    bool found = false;
    EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int { *(bool*)p = true; return 0; }, (LPARAM)&found, 0);
    ReleaseDC(nullptr, dc);
    return found;
}

static void MakeFonts() {
    if (g_font) DeleteObject(g_font);
    if (g_fontBold) DeleteObject(g_fontBold);
    if (g_fontSmall) DeleteObject(g_fontSmall);
    if (g_fontIcon) DeleteObject(g_fontIcon);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    int pct = g_set.fontSize == 0 ? 90 : (g_set.fontSize == 2 ? 125 : 100);
    lf.lfHeight = -S(13) * pct / 100;
    g_font = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_SEMIBOLD;
    g_fontBold = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_NORMAL;
    lf.lfHeight = -S(11) * pct / 100;
    g_fontSmall = CreateFontIndirectW(&lf);
    const wchar_t* iconFace = HasFont(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : (HasFont(L"Segoe MDL2 Assets") ? L"Segoe MDL2 Assets" : nullptr);
    g_iconFontOk = iconFace != nullptr;
    wcscpy_s(lf.lfFaceName, iconFace ? iconFace : L"Segoe UI");
    lf.lfHeight = -S(16);
    g_fontIcon = CreateFontIndirectW(&lf);
}

// ---------------------------------------------------------------- owner drawn buttons (shared with the dialogs)
LRESULT DrawThemedButton(const DRAWITEMSTRUCT* d, bool glyphFont) {
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    bool pressed = d->itemState & ODS_SELECTED;
    bool disabled = d->itemState & ODS_DISABLED;
    POINT pt;
    GetCursorPos(&pt);
    RECT sr;
    GetWindowRect(d->hwndItem, &sr);
    bool hot = PtInRect(&sr, pt) && !disabled;
    HBRUSH b = CreateSolidBrush(pressed ? g_th.border : (hot ? g_th.btnHot : g_th.btn));
    HPEN pen = CreatePen(PS_SOLID, 1, g_th.border);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, b);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, S(6), S(6));
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(b);
    std::wstring t = TextAt(d->hwndItem);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? g_th.sub : g_th.text);
    SelectObject(dc, glyphFont && g_iconFontOk ? g_fontIcon : g_font);
    RECT tr = r;
    DrawTextW(dc, t.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    if (d->itemState & ODS_FOCUS) { RECT fr = r; InflateRect(&fr, -3, -3); DrawFocusRect(dc, &fr); }
    return TRUE;
}

// ---------------------------------------------------------------- small helpers
HBRUSH ThemeBrush(int which) { return which == 0 ? brBg : (which == 1 ? brPanel : brEdit); }

static bool IsShellWindow(HWND h) {
    wchar_t cls[96];
    GetClassNameW(h, cls, 96);
    static const wchar_t* shell[] = {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"NotifyIconOverflowWindow", L"TopLevelWindowForOverflowXamlIsland",
                                     L"Windows.UI.Core.CoreWindow", L"XamlExplorerHostIslandWindow", L"ForegroundStaging", L"MultitaskingViewFrame",
                                     L"TaskSwitcherWnd", L"#32768", L"Progman", L"WorkerW"};
    for (auto s : shell) if (wcscmp(cls, s) == 0) return true;
    return false;
}

static std::wstring CRLF(const std::wstring& s) {
    std::wstring o;
    o.reserve(s.size() + s.size() / 20);
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r')) o += L"\r\n";
        else if (s[i] == L'\r' && (i + 1 >= s.size() || s[i + 1] != L'\n')) o += L"\r\n";
        else o += s[i];
    }
    return o;
}

static void SetStatus(const std::wstring& s, int ms) {
    g_status = s;
    if (hStatus) InvalidateRect(hStatus, nullptr, FALSE);
    if (g_main && ms > 0) SetTimer(g_main, TM_STATUS, ms, nullptr);
}

void MsgInfo(HWND parent, const std::wstring& text, UINT icon) {
    MessageBoxW(parent, text.c_str(), APP_NAME, MB_OK | icon);
}
bool MsgAsk(HWND parent, const std::wstring& text) {
    return MessageBoxW(parent, text.c_str(), APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

static std::wstring KindLabel(const Item& it) {
    if (it.kind == K_PICTURE) return T(L"PICTURE");
    if (it.kind == K_FILES) return T(L"FILES");
    if (it.kind == K_SNIPPET) return T(L"SNIPPET");
    switch (it.st) {
    case core::ST_LINK: return T(L"LINK");
    case core::ST_EMAIL: return T(L"EMAIL");
    case core::ST_PHONE: return T(L"PHONE");
    case core::ST_COLOUR: return T(L"COLOUR");
    case core::ST_PATH: return T(L"PATH");
    case core::ST_JSON: return T(L"JSON");
    default: return T(L"TEXT");
    }
}

static const wchar_t* FilterName(int f) {
    switch (f) {
    case FL_TEXT: return T(L"Text");
    case FL_PICS: return T(L"Pictures");
    case FL_FILES: return T(L"Files");
    case FL_PINNED: return T(L"Pinned");
    case FL_TODAY: return T(L"Copied today");
    case FL_SNIPPETS: return T(L"Snippets");
    case FL_TRASH: return T(L"Recently deleted");
    case FL_LINKS: return T(L"Links");
    case FL_EMAILS: return T(L"Email addresses");
    case FL_PHONES: return T(L"Phone numbers");
    case FL_COLOURS: return T(L"Colours");
    case FL_PATHS: return T(L"File paths");
    case FL_JSON: return T(L"JSON");
    default: return T(L"Everything");
    }
}

static time_t StartOfToday() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_s(&t, &now);
    t.tm_hour = t.tm_min = t.tm_sec = 0;
    return mktime(&t);
}

static bool PassesFilter(const Item& it) {
    if (curFilter == FL_TRASH) return it.deleted != 0;
    if (it.deleted) return false;
    switch (curFilter) {
    case FL_TEXT: return it.kind == K_TEXT;
    case FL_PICS: return it.kind == K_PICTURE;
    case FL_FILES: return it.kind == K_FILES;
    case FL_PINNED: return it.pinned;
    case FL_TODAY: return it.created >= StartOfToday() && it.kind != K_SNIPPET;
    case FL_SNIPPETS: return it.kind == K_SNIPPET;
    case FL_LINKS: return it.kind == K_TEXT && it.st == core::ST_LINK;
    case FL_EMAILS: return it.kind == K_TEXT && it.st == core::ST_EMAIL;
    case FL_PHONES: return it.kind == K_TEXT && it.st == core::ST_PHONE;
    case FL_COLOURS: return it.kind == K_TEXT && it.st == core::ST_COLOUR;
    case FL_PATHS: return it.kind == K_TEXT && it.st == core::ST_PATH;
    case FL_JSON: return it.kind == K_TEXT && it.st == core::ST_JSON;
    case FL_GROUP: return it.group == curGroup;
    default: return true;
    }
}

static std::vector<uint64_t> SelectedIds() {
    std::vector<uint64_t> ids;
    int i = -1;
    while ((i = ListView_GetNextItem(hList, i, LVNI_SELECTED)) >= 0)
        if (i < (int)view.size()) ids.push_back(g_store.items[view[i]].id);
    return ids;
}

static Item* SelectedOne() {
    auto ids = SelectedIds();
    return ids.size() == 1 ? g_store.Find(ids[0]) : nullptr;
}

static void Rebuild(bool keepSelection) {
    std::set<uint64_t> sel;
    if (keepSelection) for (auto id : SelectedIds()) sel.insert(id);
    int focusTop = ListView_GetTopIndex(hList);
    core::Query q = core::ParseQuery(core::Trim(curQuery), (long long)time(nullptr), (long long)StartOfToday());
    g_hlNeedles = q.words;
    g_hlNeedles.insert(g_hlNeedles.end(), q.phrases.begin(), q.phrases.end());
    view.clear();
    for (size_t i = g_store.items.size(); i-- > 0;) {
        const Item& it = g_store.items[i];
        if (!PassesFilter(it)) continue;
        if (!q.empty() && !g_store.Matches(it, q)) continue;
        view.push_back((int)i);
    }
    SendMessageW(hList, WM_SETREDRAW, FALSE, 0);
    ListView_SetItemCountEx(hList, (int)view.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
    ListView_SetItemState(hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    bool any = false;
    for (size_t v = 0; v < view.size(); v++) {
        if (sel.count(g_store.items[view[v]].id)) { ListView_SetItemState(hList, (int)v, LVIS_SELECTED, LVIS_SELECTED); any = true; }
    }
    if (any && focusTop > 0) ListView_EnsureVisible(hList, std::min<int>(focusTop, (int)view.size() - 1), FALSE);
    SendMessageW(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, nullptr, TRUE);
    std::wstring name = curFilter == FL_GROUP ? curGroup : FilterName(curFilter);
    SetWindowTextW(hFilterBtn, (name + L"  ▾").c_str());
    UpdatePreview();
    UpdateStatus();
}

// ---------------------------------------------------------------- preview
static void LoadPreviewBitmap(const Item* it) {
    if (!it || it->kind != K_PICTURE) {
        if (g_prevBmp) { delete g_prevBmp; g_prevBmp = nullptr; }
        g_prevBmpId = 0;
        return;
    }
    if (g_prevBmp && g_prevBmpId == it->id) return;
    if (g_prevBmp) { delete g_prevBmp; g_prevBmp = nullptr; }
    // load into memory so the file is not locked
    std::string bytes;
    if (ReadFileBytes(g_store.MainPath(*it), bytes)) {
        IStream* st = nullptr;
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
        if (g) {
            memcpy(GlobalLock(g), bytes.data(), bytes.size());
            GlobalUnlock(g);
            if (SUCCEEDED(CreateStreamOnHGlobal(g, TRUE, &st))) {
                Gdiplus::Bitmap tmp(st);
                if (tmp.GetLastStatus() == Gdiplus::Ok) g_prevBmp = tmp.Clone(0, 0, tmp.GetWidth(), tmp.GetHeight(), PixelFormat32bppARGB);
                st->Release();
            } else GlobalFree(g);
        }
    }
    g_prevBmpId = it->id;
}

static std::wstring PreviewHeader1, PreviewHeader2;

static void UpdatePreview() {
    auto ids = SelectedIds();
    PreviewHeader1.clear();
    PreviewHeader2.clear();
    bool showText = false;
    std::wstring body;
    LoadPreviewBitmap(nullptr);
    if (ids.empty()) {
        PreviewHeader1 = g_store.items.empty() ? T(L"Nothing copied yet") : T(L"Select an item to see all of it.");
        if (view.empty() && !g_store.items.empty()) {
            PreviewHeader1 = T(L"Nothing matches.");
            PreviewHeader2 = T(L"Try other words, or choose Everything in the filter.");
        } else if (g_store.items.empty()) {
            PreviewHeader2 = T(L"Copy something — text, a picture or files — and it appears here.");
        }
    } else if (ids.size() > 1) {
        PreviewHeader1 = std::to_wstring(ids.size()) + L" " + T(L"items selected.");
        PreviewHeader2 = T(L"Copy puts their text on the clipboard together, one per line. Pin and Delete work on all of them.");
    } else if (Item* it = g_store.Find(ids[0])) {
        std::wstring when = L"" + std::wstring(T(L"Copied")) + L" " + FormatTime((time_t)it->created);
        if (!it->source.empty()) when += L"  " + std::wstring(T(L"from")) + L" " + FriendlyName(it->source);
        if (it->pinned) when = std::wstring(T(L"Pinned")) + L"  •  " + when;
        if (!it->group.empty()) when += L"  •  " + it->group;
        if (it->deleted) when = std::wstring(T(L"Recently deleted")) + L"  •  " + when;
        PreviewHeader2 = when;
        if (it->kind == K_PICTURE) {
            PreviewHeader1 = std::wstring(T(L"Picture")) + L"  •  " + std::to_wstring(it->w) + L" × " + std::to_wstring(it->h) + L" " + T(L"pixels") + L"  •  " + FormatBytes(it->bytes);
            LoadPreviewBitmap(it);
            if (!it->head.empty()) { showText = false; }
        } else if (it->kind == K_FILES) {
            auto files = g_store.ReadFiles(*it);
            PreviewHeader1 = std::wstring(T(L"Files")) + L"  •  " + std::to_wstring(files.size());
            body = core::Join(files, L"\r\n");
            showText = true;
        } else {
            std::wstring t = g_store.ReadText(*it);
            std::wstring kind = it->kind == K_SNIPPET ? std::wstring(T(L"Snippet")) + L" \"" + it->title + L"\"" : std::wstring(KindLabel(*it));
            PreviewHeader1 = kind + L"  •  " + std::to_wstring(it->lines) + L" " + (it->lines == 1 ? T(L"line") : T(L"lines")) + L", " + Num(it->count) + L" " + (it->count == 1 ? T(L"character") : T(L"characters"));
            if (core::Trim(t).empty()) PreviewHeader1 += L"  •  " + std::wstring(T(L"(spaces or empty lines only)"));
            if (t.size() > 200000) { t.resize(200000); PreviewHeader2 += L"  •  " + std::wstring(T(L"the start is shown")); }
            if (g_set.maskPreviews) t = core::MaskSensitive(t);
            body = CRLF(t);
            showText = true;
        }
    }
    SetWindowTextW(hPrevText, body.c_str());
    ShowWindow(hPrevText, showText ? SW_SHOW : SW_HIDE);
    if (showText && !curQuery.empty()) {              // show where the search words are: select the first match
        core::Query pq = core::ParseQuery(core::Trim(curQuery), 0, 0);
        std::wstring hay = core::Lower(body);
        size_t best = std::wstring::npos, len = 0;
        auto look = [&](const std::wstring& w) { size_t p = hay.find(w); if (p != std::wstring::npos && (best == std::wstring::npos || p < best)) { best = p; len = w.size(); } };
        for (auto& w : pq.words) look(w);
        for (auto& w : pq.phrases) look(w);
        if (best != std::wstring::npos) { SendMessageW(hPrevText, EM_SETSEL, best, best + len); SendMessageW(hPrevText, EM_SCROLLCARET, 0, 0); }
    }
    InvalidateRect(hPrev, nullptr, TRUE);
}

static LRESULT CALLBACK PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDBLCLK:
        if (g_prevBmp) { g_actualSize = !g_actualSize; InvalidateRect(h, nullptr, TRUE); }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        FillRect(mem, &rc, brPanel);
        SetBkMode(mem, TRANSPARENT);
        int pad = S(12);
        RECT t1 = {pad, S(10), rc.right - pad, S(32)};
        SelectObject(mem, g_fontBold);
        SetTextColor(mem, g_th.text);
        DrawTextW(mem, PreviewHeader1.c_str(), -1, &t1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        RECT t2 = {pad, S(32), rc.right - pad, S(54)};
        SelectObject(mem, g_fontSmall);
        SetTextColor(mem, g_th.sub);
        DrawTextW(mem, PreviewHeader2.c_str(), -1, &t2, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        if (g_prevBmp) {
            int top = S(62);
            Gdiplus::Graphics g(mem);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            int aw = rc.right - 2 * pad, ah = rc.bottom - top - pad;
            int iw = (int)g_prevBmp->GetWidth(), ih = (int)g_prevBmp->GetHeight();
            if (aw > 0 && ah > 0 && iw > 0 && ih > 0) {
                if (g_actualSize) {
                    g.SetClip(Gdiplus::Rect(pad, top, aw, ah));
                    g.DrawImage(g_prevBmp, pad, top, iw, ih);
                } else {
                    double sc = std::min(1.0, std::min((double)aw / iw, (double)ah / ih));
                    int dw = std::max(1, (int)(iw * sc)), dh = std::max(1, (int)(ih * sc));
                    int x = pad + (aw - dw) / 2, y = top + (ah - dh) / 2;
                    // checkerboard-free: a thin border shows the picture's edge on transparent images
                    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(g_th.border), GetGValue(g_th.border), GetBValue(g_th.border)));
                    g.DrawImage(g_prevBmp, x, y, dw, dh);
                    g.DrawRectangle(&pen, x, y, dw - 1, dh - 1);
                    if (sc < 1.0) {
                        std::wstring hint = std::wstring(T(L"Shown at")) + L" " + std::to_wstring((int)(sc * 100)) + L"%  —  " + T(L"double-click for actual size");
                        RECT hr = {pad, rc.bottom - S(20), rc.right - pad, rc.bottom - S(2)};
                        SelectObject(mem, g_fontSmall);
                        SetTextColor(mem, g_th.sub);
                        DrawTextW(mem, hint.c_str(), -1, &hr, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                    }
                }
            }
        }
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------------------------------------------------------- status bar
static LRESULT CALLBACK StatusProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: {
        RECT rc;
        GetClientRect(h, &rc);
        if (GET_X_LPARAM(l) > rc.right - S(230)) SendMessageW(g_main, WM_COMMAND, IDM_PAUSE, 0);
        return 0;
    }
    case WM_SETCURSOR: {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(h, &p);
        RECT rc;
        GetClientRect(h, &rc);
        SetCursor(LoadCursor(nullptr, p.x > rc.right - S(230) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        FillRect(dc, &rc, brBg);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_fontSmall);
        int pad = S(12);
        // right: recording state
        std::wstring rec = g_paused ? std::wstring(T(L"Paused")) + L" — " + T(L"click to resume") : std::wstring(T(L"Recording")) + L" (" + T(L"click to pause") + L")";
        RECT rr = {rc.right - S(230), 0, rc.right - pad, rc.bottom};
        SetTextColor(dc, g_paused ? RGB(220, 110, 40) : g_th.sub);
        DrawTextW(dc, (std::wstring(g_paused ? L"❚❚  " : L"●  ") + rec).c_str(), -1, &rr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        // left: message or hints
        RECT lr = {pad, 0, rc.right - S(240), rc.bottom};
        SetTextColor(dc, g_status.empty() ? g_th.sub : g_th.text);
        std::wstring left = g_status;
        if (left.empty()) {
            left = Num((long long)view.size()) + L" " + (view.size() == 1 ? T(L"item") : T(L"items")) + L" " + T(L"shown");
            long today = 0;
            time_t st = StartOfToday();
            for (auto& it : g_store.items) if (!it.deleted && it.created >= st && it.kind != K_SNIPPET) today++;
            left += L"  •  " + std::wstring(T(L"Copied today")) + L": " + Num(today);
            if (!SelectedIds().empty()) {
                left += L"  •  ";
                if (g_set.pasteOnEnter && !g_prevName.empty()) left += std::wstring(T(L"Enter pastes into")) + L" " + g_prevName;
                else left += T(L"Enter copies");
            }
        }
        DrawTextW(dc, left.c_str(), -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

static void UpdateStatus() { if (hStatus) InvalidateRect(hStatus, nullptr, FALSE); }

// ---------------------------------------------------------------- layout
static void Layout() {
    if (!g_main || !hList) return;
    RECT rc;
    GetClientRect(g_main, &rc);
    int W = rc.right, H = rc.bottom;
    int m = S(10), topH = S(38), stH = S(26), btnW = S(40), gap = S(6);
    int x = W - m;
    // toolbar buttons right-to-left
    for (int i = 5; i >= 0; i--) {
        x -= btnW;
        SetWindowPos(hBtn[i], nullptr, x, m, btnW, topH - S(4), SWP_NOZORDER);
        x -= gap;
    }
    int filterW = S(170);
    int fx = x - filterW + gap;
    SetWindowPos(hFilterBtn, nullptr, fx, m, filterW, topH - S(4), SWP_NOZORDER);
    int sx = m, sw = fx - gap - sx;
    SetWindowPos(hSearch, nullptr, sx + S(12), m + S(7), std::max(S(60), sw - S(24)), S(20), SWP_NOZORDER);
    int top = m + topH + gap, bottom = H - stH;
    int cw = W - 2 * m;
    bool compact = g_set.compact;
    int lw = compact ? cw : g_set.listWidth;
    if (lw <= 0) lw = cw * 45 / 100;
    lw = std::clamp(lw, S(240), std::max(S(240), cw - S(240)));
    if (compact) lw = cw;
    SetWindowPos(hList, nullptr, m, top, lw, bottom - top, SWP_NOZORDER);
    int px = m + lw + S(6), pw = cw - lw - S(6);
    if (compact) {
        ShowWindow(hPrev, SW_HIDE);
        ShowWindow(hPrevText, SW_HIDE);
    } else {
        ShowWindow(hPrev, SW_SHOW);
        SetWindowPos(hPrev, nullptr, px, top, pw, bottom - top, SWP_NOZORDER);
        SetWindowPos(hPrevText, nullptr, px + S(10), top + S(62), pw - S(20), bottom - top - S(72), SWP_NOZORDER);
    }
    SetWindowPos(hStatus, nullptr, 0, H - stH, W, stH, SWP_NOZORDER);
    int colW = lw - S(76) - S(128) - GetSystemMetrics(SM_CXVSCROLL) - S(6);
    ListView_SetColumnWidth(hList, 0, S(76));
    ListView_SetColumnWidth(hList, 1, std::max(S(80), colW));
    ListView_SetColumnWidth(hList, 2, S(128));
    InvalidateRect(g_main, nullptr, TRUE);
    if (!compact) UpdatePreview();
}

// ---------------------------------------------------------------- tray
static void TrayAdd() {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = g_main;
    n.uID = 1;
    n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    n.uCallbackMessage = WM_TRAY;
    n.hIcon = g_paused ? g_icoOff : g_icoOn;
    std::wstring tip = std::wstring(APP_NAME) + (g_paused ? L" — " + std::wstring(T(L"paused")) : L"");
    wcsncpy_s(n.szTip, tip.c_str(), _TRUNCATE);
    if (!Shell_NotifyIconW(NIM_MODIFY, &n)) Shell_NotifyIconW(NIM_ADD, &n);
    g_inTray = true;
}

static void TrayRemove() {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = g_main;
    n.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &n);
    g_inTray = false;
}

static void Balloon(const std::wstring& title, const std::wstring& text) {
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof n;
    n.hWnd = g_main;
    n.uID = 1;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcsncpy_s(n.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

// ---------------------------------------------------------------- autostart
static bool SetAutostart(bool on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    LONG r;
    if (on) {
        std::wstring v = L"\"" + ExePath() + L"\" /tray";
        r = RegSetValueExW(k, L"FeatherClipboardManager", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else {
        r = RegDeleteValueW(k, L"FeatherClipboardManager");
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    RegCloseKey(k);
    if (r == ERROR_SUCCESS) g_set.autostart = on;
    return r == ERROR_SUCCESS;
}

// ---------------------------------------------------------------- window show / hide / position
static void SavePlacement() {
    WINDOWPLACEMENT wp = {sizeof wp};
    if (GetWindowPlacement(g_main, &wp)) SetBlobReg(L"WindowPlacement", &wp, sizeof wp);
}

static void HideMain() {
    if (!IsWindowVisible(g_main)) return;
    SavePlacement();
    ShowWindow(g_main, SW_HIDE);
    if (!g_set.toldAboutTray) {
        g_set.toldAboutTray = true;
        g_set.Save();
        Balloon(APP_NAME, T(L"It keeps what you copy from here. Right-click the icon to pause or exit."));
    }
}

static void ForceForeground(HWND h) {
    HWND fg = GetForegroundWindow();
    DWORD ft = fg ? GetWindowThreadProcessId(fg, nullptr) : 0, me = GetCurrentThreadId();
    if (ft && ft != me) AttachThreadInput(me, ft, TRUE);
    BringWindowToTop(h);
    SetForegroundWindow(h);
    if (ft && ft != me) AttachThreadInput(me, ft, FALSE);
}

static void PositionNearCaret(HWND target) {
    POINT pt;
    bool got = false;
    if (target) {
        GUITHREADINFO gi = {sizeof gi};
        if (GetGUIThreadInfo(GetWindowThreadProcessId(target, nullptr), &gi) && gi.hwndCaret) {
            pt = {gi.rcCaret.left, gi.rcCaret.bottom};
            ClientToScreen(gi.hwndCaret, &pt);
            got = true;
        }
    }
    if (!got) GetCursorPos(&pt);
    RECT wr;
    GetWindowRect(g_main, &wr);
    int w = wr.right - wr.left, h = wr.bottom - wr.top;
    MONITORINFO mi = {sizeof mi};
    GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
    int x = pt.x, y = pt.y + S(10);
    if (x + w > mi.rcWork.right) x = mi.rcWork.right - w;
    if (y + h > mi.rcWork.bottom) y = pt.y - h - S(24);
    x = std::max<int>(mi.rcWork.left, x);
    y = std::max<int>(mi.rcWork.top, y);
    SetWindowPos(g_main, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void ShowMain(bool fromHotkey) {
    HWND fg = GetForegroundWindow();
    if (fg && fg != g_main && !IsShellWindow(fg)) {
        g_prevWnd = fg;
        g_prevName = FriendlyName(ProcessNameOf(fg));
    } else if (fg != g_main) {
        g_prevWnd = nullptr;
        g_prevName.clear();
    }
    bool wasVisible = IsWindowVisible(g_main) && !IsIconic(g_main);
    if (IsIconic(g_main)) ShowWindow(g_main, SW_RESTORE);
    if (!wasVisible) {
        Rebuild(false);
        ShowWindow(g_main, SW_SHOW);
        if (fromHotkey && g_set.popupAtCursor) PositionNearCaret(g_prevWnd);
    }
    ForceForeground(g_main);
    SetFocus(hSearch);
    if (view.size() && SelectedIds().empty()) {
        ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    UpdateStatus();
    if (g_pendingUpdate) {
        UpdateInfo* p = g_pendingUpdate;
        g_pendingUpdate = nullptr;
        RunUpdateFlow(*p);
        delete p;
    }
}

// ---------------------------------------------------------------- paste
static void ReleaseModifiersAndSend() {
    std::vector<INPUT> in;
    auto key = [&](WORD vk, bool up) {
        INPUT i = {};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = vk;
        i.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        in.push_back(i);
    };
    WORD mods[] = {VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN, VK_LCONTROL, VK_RCONTROL};
    for (WORD v : mods) if (GetAsyncKeyState(v) & 0x8000) key(v, true);
    key(VK_CONTROL, false);
    key('V', false);
    key('V', true);
    key(VK_CONTROL, true);
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

static const wchar_t* PutMessage(PutResult r) {
    switch (r) {
    case PUT_PICTURE_ONE: return T(L"The clipboard holds one picture at a time: the first one was copied.");
    case PUT_TEXT_ONLY: return T(L"Copied the text. Pictures can not be joined with text, so they were left out.");
    case PUT_FILES_MISSING: return T(L"Some of the copied files no longer exist, so only the files that still exist were put on the clipboard.");
    case PUT_FAILED: return T(L"It could not be put on the clipboard. The file may have been deleted, or another program is using the clipboard.");
    default: return L"";
    }
}

static std::wstring TextOfItem(const Item& it) {
    if (it.kind == K_FILES) return core::Join(g_store.ReadFiles(it), L"\r\n");
    return g_store.ReadText(it);
}

// mode: 0 normal, 1 plain text, 2 transform (xf), 3 join with sep
static PutResult PutSelection(const std::vector<uint64_t>& ids, int mode, core::Xf xf, const std::wstring& sep) {
    std::vector<const Item*> its;
    for (auto id : ids) if (const Item* it = g_store.Find(id)) its.push_back(it);
    if (its.empty()) return PUT_FAILED;
    if (its.size() == 1 && mode <= 1) return ClipPutItem(g_main, *its[0], mode == 1);
    if (its.size() == 1 && its[0]->kind == K_PICTURE) return ClipPutItem(g_main, *its[0], false);
    std::vector<std::wstring> parts;
    bool hadPicture = false;
    for (auto* it : its) {
        if (it->kind == K_PICTURE) { hadPicture = true; continue; }
        std::wstring t = TextOfItem(*it);
        if (mode == 2) t = core::Apply(xf, t);
        else if (mode == 5 && g_customXf) t = g_customXf(t);
        parts.push_back(t);
    }
    if (parts.empty()) return ClipPutItem(g_main, *its[0], false) == PUT_OK ? PUT_PICTURE_ONE : PUT_FAILED;
    std::wstring text = core::Join(parts, mode == 3 ? sep : L"\r\n");
    PutResult r = ClipPutText(g_main, text);
    if (r == PUT_OK && hadPicture) return PUT_TEXT_ONLY;
    return r;
}

static void AfterPut(PutResult r, bool pasteNow) {
    if (r == PUT_FAILED) { SetStatus(PutMessage(r), 7000); return; }
    if (r != PUT_OK) SetStatus(PutMessage(r), 7000);
    bool canPaste = pasteNow && g_set.pasteOnEnter && g_prevWnd && IsWindow(g_prevWnd);
    if (canPaste) {
        HideMain();
        SetForegroundWindow(g_prevWnd);
        SetTimer(g_main, TM_PASTE, 120, nullptr);
    } else if (r == PUT_OK) {
        SetStatus(T(L"Copied. Press Ctrl+V to paste it anywhere."), 3500);
    }
}

static void DoPut(int mode, core::Xf xf, const std::wstring& sep, bool paste) {
    auto ids = SelectedIds();
    if (ids.empty()) return;
    AfterPut(PutSelection(ids, mode, xf, sep), paste);
}

// Global shortcuts: paste the Nth newest / the newest item as plain text into the program in front.
static void GlobalPasteNth(int n, bool plain) {
    int seen = 0;
    for (size_t i = g_store.items.size(); i-- > 0;) {
        const Item& it = g_store.items[i];
        if (it.deleted || it.kind == K_SNIPPET) continue;
        if (++seen == n) {
            HWND fg = GetForegroundWindow();
            if (!fg || IsShellWindow(fg)) return;
            g_prevWnd = fg;
            if (ClipPutItem(g_main, it, plain) == PUT_OK) SetTimer(g_main, TM_PASTE, 150, nullptr);
            return;
        }
    }
}

static void GlobalPasteQueue() {
    while (!g_queue.empty() && !g_store.Find(g_queue.front())) g_queue.erase(g_queue.begin());
    if (g_queue.empty()) { Balloon(APP_NAME, T(L"The paste queue is empty. Right-click items and choose Add to the paste queue.")); return; }
    HWND fg = GetForegroundWindow();
    if (!fg || IsShellWindow(fg)) return;
    g_prevWnd = fg;
    Item* it = g_store.Find(g_queue.front());
    if (it && ClipPutItem(g_main, *it, false) == PUT_OK) {
        g_queue.erase(g_queue.begin());      // advances when you press the shortcut; it can not know whether the other program received it
        SetTimer(g_main, TM_PASTE, 150, nullptr);
        UpdateStatus();
    }
}

static std::vector<std::pair<std::wstring, std::wstring>> PipelineList() {
    std::vector<std::pair<std::wstring, std::wstring>> v;
    for (auto& line : core::SplitLines(g_set.pipelines)) {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring name = core::Trim(line.substr(0, eq)), spec = core::Trim(line.substr(eq + 1));
        if (!name.empty() && !spec.empty()) v.push_back({name, spec});
    }
    return v;
}

// ---------------------------------------------------------------- clipboard capture
static void OnCapture() {
    if (g_paused) return;
    if (GetClipboardSequenceNumber() == g_ignoreSeq) return;
    HWND ownerWnd = GetClipboardOwner();
    std::wstring source = ProcessNameOf(ownerWnd);
    if (ownerWnd == g_main) return;
    if (IsIgnoredProgram(source)) return;
    Captured c;
    bool flagged = false;
    if (!ClipCapture(g_main, c, g_set.recordImages, g_set.recordFiles, g_set.recordRich, flagged)) { LogEvent(L"The clipboard was busy; one copy was skipped."); return; }
    if (flagged || c.kind < 0) return;
    {   // low disk space: stop recording before the disk is full (the limit is in Settings; 0 = off)
        uint64_t need = 0;
        if (c.kind == K_TEXT) need = c.text.size() * 2 + c.html.size() + c.rtf.size();
        else if (c.kind == K_PICTURE) need = FileSizeW(c.tmpPng);
        uint64_t freeB = FreeDiskBytes(g_store.dir);
        if (g_set.minFreeMB > 0 && freeB != ~0ULL && freeB < (uint64_t)g_set.minFreeMB * 1048576ULL + need * 2) {
            if (c.kind == K_PICTURE) DeleteFileW(c.tmpPng.c_str());
            SetStatus(std::wstring(T(L"Not recorded: the disk is almost full")) + L" (" + FormatBytes(freeB) + L" " + T(L"free") + L"). " + T(L"Free some space, or change the limit in Settings > What to keep."), 9000);
            static DWORD lastLog = 0;
            if (GetTickCount() - lastLog > 600000 || !lastLog) { lastLog = GetTickCount(); LogEvent(L"Recording stopped: less than the free-space limit is left on the disk (" + FormatBytes(freeB) + L" free)."); Balloon(APP_NAME, T(L"The disk is almost full, so copies are not being recorded.")); }
            return;
        }
    }
    if (g_set.maxEntryMB > 0) {
        uint64_t limit = (uint64_t)g_set.maxEntryMB * 1048576ULL, size = 0;
        if (c.kind == K_TEXT) size = c.text.size() * 2 + c.html.size() + c.rtf.size();
        else if (c.kind == K_PICTURE) size = FileSizeW(c.tmpPng);
        if (size > limit) {
            if (c.kind == K_PICTURE) DeleteFileW(c.tmpPng.c_str());
            SetStatus(std::wstring(T(L"Not recorded: it is larger than")) + L" " + std::to_wstring(g_set.maxEntryMB) + L" MB (" + FormatBytes(size) + L").", 7000);
            LogEvent(L"A copy larger than the limit was not recorded (" + FormatBytes(size) + L").");
            return;
        }
    }

    bool isNew = true;
    uint64_t id = 0;
    std::wstring sensitiveNote;
    if (c.kind == K_TEXT) {
        int64_t expires = 0;
        std::wstring why;
        if (g_set.sensitive != 0 && core::Sensitive(c.text, &why)) {
            if (g_set.sensitive == 1) { SetStatus(std::wstring(T(L"Not recorded: it looks like")) + L" " + T(why.c_str()) + L".", 5000); return; }
            expires = (int64_t)time(nullptr) + g_set.expireSeconds;
            sensitiveNote = std::wstring(T(L"Looks like")) + L" " + T(why.c_str()) + L" — " + T(L"it deletes itself after") + L" " + std::to_wstring(g_set.expireSeconds) + L" " + T(L"seconds") + L".";
        }
        id = g_store.AddText(c.text, c.html, c.rtf, source, expires, &isNew);
    } else if (c.kind == K_PICTURE) {
        id = g_store.AddPictureFile(c.tmpPng, c.w, c.h, c.hash, source, &isNew);
    } else if (c.kind == K_FILES) {
        id = g_store.AddFiles(c.files, c.dropEffect, source, &isNew);
    }
    if (!id) {
        if (c.kind == K_PICTURE) DeleteFileW(c.tmpPng.c_str());
        SetStatus(std::wstring(T(L"What you copied could not be saved in")) + L" " + g_store.dir + L" " + T(L"(is the disk full?)."), 9000);
        LogEvent(L"A copy could not be saved (disk full or folder not writable).");
        return;
    }
    if (c.kind == K_PICTURE && isNew && g_set.ocr) OcrStart(g_main, id, g_store.MainPath(*g_store.Find(id)));
    if (g_set.sound) MessageBeep(MB_OK);
    if (g_set.toast && g_inTray) {
        Item* it = g_store.Find(id);
        Balloon(T(L"Copied"), it ? ItemTitle(*it).substr(0, 100) : L"");
    }
    if (!sensitiveNote.empty()) SetStatus(sensitiveNote, 8000);
    if (g_store.Cleanup()) {}
    if (IsWindowVisible(g_main)) Rebuild(true);
}

// ---------------------------------------------------------------- hotkeys
static bool RegHK(int id, UINT mods, UINT vk, const wchar_t* name) {
    if (RegisterHotKey(g_main, id, mods | MOD_NOREPEAT, vk)) return true;
    if (!g_startupHotkeyProblem.empty()) g_startupHotkeyProblem += L"\n";
    g_startupHotkeyProblem += std::wstring(name) + L" " + T(L"is already used by another program, so it can not be used by Feather Clipboard Manager.");
    return false;
}

static void RegisterHotkeys() {
    for (int i = 0; i < 20; i++) UnregisterHotKey(g_main, i);
    g_startupHotkeyProblem.clear();
    auto reg = [&](int id, int packed) {
        if (!packed) return;
        std::wstring name = core::HotkeyName(packed);
        RegHK(id, (UINT)core::HotkeyMods(packed), (UINT)core::HotkeyVk(packed), name.c_str());
    };
    reg(HK_OPEN, g_set.keyOpen);
    reg(HK_PLAIN, g_set.keyPlain);
    reg(HK_QUEUE, g_set.keyQueue);
    if (g_set.quickNumbers)
        for (int n = 1; n <= 9; n++) RegHK(HK_QUICK + n - 1, MOD_CONTROL | MOD_ALT, '0' + n, (L"Ctrl+Alt+" + std::to_wstring(n)).c_str());
}

void SettingsChanged() {
    g_set.Save();
    SetLanguage(g_set.language);
    MakeFonts();
    SetFontAll(g_main, g_font);
    SendMessageW(hList, WM_SETFONT, (WPARAM)g_font, TRUE);
    SetButtonGlyphs();
    ApplyTheme();
    SetWindowPos(g_main, g_set.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    RegisterHotkeys();
    if (!g_startupHotkeyProblem.empty() && g_main && IsWindowVisible(g_main))
        MsgInfo(g_main, g_startupHotkeyProblem + L"\n" + T(L"Choose another shortcut in Settings."), MB_ICONWARNING);
    SendMessageW(hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)T(L"Search everything you copied  (Ctrl+F)"));
    const wchar_t* tips[6] = {T(L"Paste into the program you were using (Enter)"), T(L"Put it on the clipboard (Ctrl+C)"),
                              T(L"Pinned items stay at hand and are never deleted automatically (Ctrl+P)"), T(L"Delete the selected items (Del); Ctrl+Z brings them back"),
                              T(L"Open the Clipboard contents folder"), T(L"Save as, pause recording, snippets, backup, settings, About")};
    for (int i = 0; i < 6; i++) {
        TOOLINFOW ti = {sizeof ti};
        ti.hwnd = g_main;
        ti.uId = (UINT_PTR)hBtn[i];
        ti.lpszText = (LPWSTR)tips[i];
        SendMessageW(hTip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
    }
    Layout();
    g_store.Cleanup();
    Rebuild(true);
    TrayAdd();
    for (int i = 0; i < 6; i++) InvalidateRect(hBtn[i], nullptr, TRUE);
    InvalidateRect(g_main, nullptr, TRUE);
}

// ---------------------------------------------------------------- actions
static void SetButtonGlyphs() {
    static const wchar_t* glyph[6] = {L"", L"", L"", L"", L"", L""};
    const wchar_t* label[6] = {T(L"Paste"), T(L"Copy"), T(L"Pin"), T(L"Delete"), T(L"Folder"), T(L"More")};
    for (int i = 0; i < 6; i++) SetWindowTextW(hBtn[i], g_iconFontOk ? glyph[i] : label[i]);
}

static void DeleteSelected() {
    auto ids = SelectedIds();
    if (ids.empty()) return;
    if (curFilter == FL_TRASH) {
        if (MsgAsk(g_main, T(L"Delete these items for good? This can not be undone."))) { g_store.Purge(ids); Rebuild(false); }
        return;
    }
    int pinned = 0;
    for (auto id : ids) if (Item* it = g_store.Find(id)) if (it->pinned || it->kind == K_SNIPPET) pinned++;
    if (pinned) {
        std::wstring q = ids.size() == 1 ? std::wstring(T(L"Delete this pinned item?"))
                                         : std::wstring(T(L"Delete these")) + L" " + std::to_wstring(ids.size()) + L" " + T(L"items?") + L" " + std::to_wstring(pinned) + L" " + (pinned == 1 ? T(L"of them is pinned.") : T(L"of them are pinned."));
        if (!MsgAsk(g_main, q)) return;
    }
    int first = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    g_store.SoftDelete(ids);
    g_lastDeleted = ids;
    Rebuild(false);
    if (!view.empty()) {
        int sel = std::min(first, (int)view.size() - 1);
        ListView_SetItemState(hList, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    SetStatus(std::to_wstring(ids.size()) + L" " + (ids.size() == 1 ? T(L"item") : T(L"items")) + L" " + T(L"deleted. Press Ctrl+Z to bring them back."), 6000);
}

static void UndoDelete() {
    if (g_lastDeleted.empty()) { SetStatus(T(L"Nothing to undo."), 2500); return; }
    g_store.Restore(g_lastDeleted);
    SetStatus(std::to_wstring(g_lastDeleted.size()) + L" " + T(L"restored."), 3000);
    g_lastDeleted.clear();
    Rebuild(true);
}

static void TogglePin() {
    auto ids = SelectedIds();
    if (ids.empty()) return;
    bool allPinned = true;
    for (auto id : ids) if (Item* it = g_store.Find(id)) if (!it->pinned) allPinned = false;
    for (auto id : ids) g_store.SetPinned(id, !allPinned);
    Rebuild(true);
}

static void SaveAs() {
    Item* it = SelectedOne();
    if (!it) return;
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW o = {sizeof o};
    o.hwndOwner = g_main;
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    std::wstring filter;
    if (it->kind == K_PICTURE) {
        filter = std::wstring(T(L"PNG picture (*.png)")) + L'\0' + L"*.png" + L'\0' + L"JPEG picture (*.jpg)" + L'\0' + L"*.jpg" + L'\0' + L"BMP picture (*.bmp)" + L'\0' + L"*.bmp" + L'\0';
        o.lpstrDefExt = L"png";
        wcscpy_s(file, L"picture.png");
    } else if (it->kind == K_FILES) {
        filter = std::wstring(T(L"Text (*.txt)")) + L'\0' + L"*.txt" + L'\0';
        o.lpstrDefExt = L"txt";
        wcscpy_s(file, T(L"File list.txt"));
    } else {
        filter = std::wstring(T(L"Text (*.txt)")) + L'\0' + L"*.txt" + L'\0' + T(L"All files") + L'\0' + L"*.*" + L'\0';
        o.lpstrDefExt = L"txt";
        wcscpy_s(file, L"text.txt");
    }
    o.lpstrFilter = filter.c_str();
    if (!GetSaveFileNameW(&o)) return;
    bool ok = false;
    if (it->kind == K_PICTURE) {
        std::wstring ext = core::Lower(PathFindExtensionW(file));
        if (ext == L".jpg" || ext == L".jpeg") ok = PngToSizedCopy(g_store.MainPath(*it), file, L"image/jpeg", 100);
        else if (ext == L".bmp") ok = PngToSizedCopy(g_store.MainPath(*it), file, L"image/bmp", 100);
        else ok = CopyFileW(g_store.MainPath(*it).c_str(), file, FALSE) != 0;
    } else {
        std::string u = "\xEF\xBB\xBF" + core::ToUtf8(TextOfItem(*it));
        ok = WriteFileAtomic(file, u.data(), u.size());
    }
    if (!ok) MsgInfo(g_main, T(L"The file could not be saved."), MB_ICONWARNING);
}

static void OpenItemFile() {
    Item* it = SelectedOne();
    if (it) OpenShell(g_main, g_store.MainPath(*it));
}

static void NewSnippet(const std::wstring& initialText = L"") {
    std::wstring name, text = initialText;
    if (!EditTextDialog(g_main, T(L"New snippet"), &name, text, true)) return;
    if (core::Trim(name).empty()) name = core::OneLinePreview(text, 30);
    if (g_store.AddSnippet(name, text)) { curFilter = FL_SNIPPETS; Rebuild(false); SetStatus(T(L"Snippet saved. It stays under the Snippets filter and is never deleted automatically."), 5000); }
}

static void EditSelected() {
    Item* it = SelectedOne();
    if (!it) return;
    if (it->kind == K_PICTURE) { ShowImageEditor(g_main, *it); Rebuild(true); return; }
    if (it->kind != K_TEXT && it->kind != K_SNIPPET) return;
    std::wstring name = it->title, text = g_store.ReadText(*it);
    if (!EditTextDialog(g_main, it->kind == K_SNIPPET ? T(L"Edit snippet") : T(L"Edit text"), it->kind == K_SNIPPET ? &name : nullptr, text, true)) return;
    uint64_t id = it->id;
    if (it->kind == K_SNIPPET) { it->title = name; }
    if (!g_store.UpdateText(id, text)) MsgInfo(g_main, T(L"The file could not be saved."), MB_ICONWARNING);
    Rebuild(true);
}

static void CopyOcrText() {
    Item* it = SelectedOne();
    if (!it || it->kind != K_PICTURE) return;
    if (!(it->flags & F_OCR)) {
        if (it->flags & F_OCRTRIED) { SetStatus(T(L"No text was found in this picture."), 4000); return; }
        OcrStart(g_main, it->id, g_store.MainPath(*it));
        SetStatus(T(L"Reading the picture… try again in a moment."), 4000);
        return;
    }
    std::wstring t = g_store.ReadOcr(*it);
    if (ClipPutText(g_main, t) == PUT_OK) SetStatus(T(L"The text of the picture is on the clipboard."), 4000);
}

static void ExportBackup() {
    std::wstring pw;
    if (!PasswordPrompt(g_main, T(L"Backup password (leave empty for no password)"), pw)) return;
    wchar_t file[MAX_PATH] = L"Feather Clipboard Manager backup.cmbackup";
    OPENFILENAMEW o = {sizeof o};
    o.hwndOwner = g_main;
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.lpstrDefExt = L"cmbackup";
    std::wstring filter = std::wstring(T(L"Feather Clipboard Manager backup")) + L" (*.cmbackup)" + L'\0' + L"*.cmbackup" + L'\0';
    o.lpstrFilter = filter.c_str();
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&o)) return;
    std::wstring err;
    SetCursor(LoadCursor(nullptr, IDC_WAIT));
    if (BackupExport(file, pw, err)) SetStatus(T(L"Backup saved."), 5000);
    else MsgInfo(g_main, err, MB_ICONWARNING);
}

static void ImportBackup() {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW o = {sizeof o};
    o.hwndOwner = g_main;
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    std::wstring filter = std::wstring(T(L"Feather Clipboard Manager backup")) + L" (*.cmbackup)" + L'\0' + L"*.cmbackup" + L'\0';
    o.lpstrFilter = filter.c_str();
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&o)) return;
    std::wstring pw, err;
    int added = 0;
    // try without a password first; ask when the file says it needs one
    if (!BackupImport(file, L"", added, err)) {
        if (err.find(L"password") != std::wstring::npos) {
            if (!PasswordPrompt(g_main, T(L"Backup password"), pw)) return;
            if (!BackupImport(file, pw, added, err)) { MsgInfo(g_main, err, MB_ICONWARNING); return; }
        } else {
            MsgInfo(g_main, err, MB_ICONWARNING);
            return;
        }
    }
    Rebuild(false);
    MsgInfo(g_main, std::to_wstring(added) + L" " + T(L"items were added from the backup. Items you already had were skipped."));
}

static void TogglePause() {
    g_paused = !g_paused;
    TrayAdd();
    SetStatus(g_paused ? T(L"Recording is paused: what you copy now is not kept.") : T(L"Recording again."), 4000);
    UpdateStatus();
}

static void ToggleCompact() {
    g_set.compact = !g_set.compact;
    g_set.Save();
    Layout();
}

static void ShowFilterMenu() {
    HMENU m = CreatePopupMenu();
    static const int top[] = {FL_ALL, FL_TEXT, FL_PICS, FL_FILES, FL_PINNED, FL_SNIPPETS, FL_TODAY, FL_TRASH};
    for (int f : top) AppendMenuW(m, MF_STRING | (curFilter == f ? MF_CHECKED : 0), IDM_FILTER_BASE + f, FilterName(f));
    HMENU types = CreatePopupMenu();
    static const int ty[] = {FL_LINKS, FL_EMAILS, FL_PHONES, FL_COLOURS, FL_PATHS, FL_JSON};
    for (int f : ty) AppendMenuW(types, MF_STRING | (curFilter == f ? MF_CHECKED : 0), IDM_FILTER_BASE + f, FilterName(f));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)types, T(L"Kind of text"));
    g_menuGroups = g_store.Groups();
    HMENU groups = CreatePopupMenu();
    if (g_menuGroups.empty()) AppendMenuW(groups, MF_STRING | MF_GRAYED, 0, T(L"No groups yet"));
    for (size_t i = 0; i < g_menuGroups.size() && i < 90; i++)
        AppendMenuW(groups, MF_STRING | (curFilter == FL_GROUP && curGroup == g_menuGroups[i] ? MF_CHECKED : 0), IDM_FGROUP_BASE + (UINT)i, g_menuGroups[i].c_str());
    AppendMenuW(m, MF_POPUP, (UINT_PTR)groups, T(L"Groups"));
    RECT r;
    GetWindowRect(hFilterBtn, &r);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, g_main, nullptr);
    DestroyMenu(m);
}

static HMENU BuildMoreMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_SNIPPET_NEW, (std::wstring(T(L"&New snippet…")) + L"\tCtrl+N").c_str());
    AppendMenuW(m, MF_STRING, IDM_SAVEAS, T(L"&Save as…"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g_paused ? MF_CHECKED : 0), IDM_PAUSE, T(L"&Pause recording"));
    AppendMenuW(m, MF_STRING, IDM_UNDO, (std::wstring(T(L"&Undo delete")) + L"\tCtrl+Z").c_str());
    AppendMenuW(m, MF_STRING, IDM_DELALL, T(L"&Delete all unpinned items"));
    AppendMenuW(m, MF_STRING, IDM_EMPTY_TRASH, T(L"Empty &recently deleted"));
    AppendMenuW(m, MF_STRING, IDM_CLEARALL, T(L"Delete &everything\u2026"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g_set.alwaysOnTop ? MF_CHECKED : 0), IDM_TOPMOST, T(L"Keep on &top"));
    AppendMenuW(m, MF_STRING, IDM_QUEUE_CLEAR, T(L"Clear the paste &queue"));
    AppendMenuW(m, MF_STRING | (g_set.compact ? MF_CHECKED : 0), IDM_COMPACT, T(L"&Compact view (list only)"));
    AppendMenuW(m, MF_STRING, IDM_STATS, T(L"S&tatistics"));
    HMENU tools = CreatePopupMenu();
    AppendMenuW(tools, MF_STRING, IDM_FORMATS, T(L"Clipboard &format inspector"));
    AppendMenuW(tools, MF_STRING, IDM_INTEGRITY, T(L"Check the history &integrity"));
    AppendMenuW(tools, MF_STRING, IDM_LOG, T(L"Diagnostic &log"));
    AppendMenuW(tools, MF_STRING, IDM_SHORTCUTS, T(L"Keyboard &shortcuts"));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)tools, T(L"T&ools"));
    AppendMenuW(m, MF_STRING, IDM_EXPORT, T(L"&Export backup…"));
    AppendMenuW(m, MF_STRING, IDM_IMPORT, T(L"&Import backup…"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_SETTINGS, T(L"S&ettings"));
    AppendMenuW(m, MF_STRING | (g_set.autostart ? MF_CHECKED : 0), IDM_AUTOSTART, T(L"Start with &Windows"));
    AppendMenuW(m, MF_STRING, IDM_HIDE, (std::wstring(T(L"&Hide to the tray")) + L"\tEsc").c_str());
    AppendMenuW(m, MF_STRING, IDM_ABOUT, T(L"A&bout Feather Clipboard Manager"));
    AppendMenuW(m, MF_STRING, IDM_EXIT, T(L"E&xit"));
    return m;
}

static void ShowContextMenu(POINT screen) {
    auto ids = SelectedIds();
    HMENU m = CreatePopupMenu();
    Item* one = ids.size() == 1 ? g_store.Find(ids[0]) : nullptr;
    if (ids.empty()) { AppendMenuW(m, MF_STRING, IDM_SNIPPET_NEW, T(L"&New snippet…")); AppendMenuW(m, MF_STRING, IDM_SELECTALL, T(L"Select &all\tCtrl+A")); }
    else if (curFilter == FL_TRASH) {
        AppendMenuW(m, MF_STRING, IDM_RESTORE, T(L"&Restore"));
        AppendMenuW(m, MF_STRING, IDM_DELETE_FOREVER, T(L"Delete for &good"));
        AppendMenuW(m, MF_STRING, IDM_COPY, T(L"&Copy\tCtrl+C"));
    } else {
        std::wstring pasteLabel = (g_set.pasteOnEnter && !g_prevName.empty()) ? std::wstring(T(L"&Paste into")) + L" " + g_prevName + L"\tEnter" : std::wstring(T(L"&Copy")) + L"\tEnter";
        AppendMenuW(m, MF_STRING, IDM_PASTE, pasteLabel.c_str());
        AppendMenuW(m, MF_STRING, IDM_PASTE_PLAIN, (std::wstring(T(L"Paste as plain &text")) + L"\tShift+Enter").c_str());
        AppendMenuW(m, MF_STRING, IDM_COPY, (std::wstring(T(L"&Copy")) + L"\tCtrl+C").c_str());
        // paste as ...
        bool anyText = false;
        for (auto id : ids) if (Item* it = g_store.Find(id)) if (it->kind != K_PICTURE) anyText = true;
        if (anyText) {
            HMENU xf = CreatePopupMenu();
            static const std::pair<core::Xf, const wchar_t*> xs[] = {
                {core::Xf::Upper, L"UPPER CASE"}, {core::Xf::Lower, L"lower case"}, {core::Xf::Title, L"Title Case"}, {core::Xf::Sentence, L"Sentence case"},
                {core::Xf::Trim, L"Trim spaces and empty lines"}, {core::Xf::BlankLines, L"Remove extra blank lines"}, {core::Xf::OneLine, L"Remove line breaks"}, {core::Xf::SortLines, L"Sort lines A\u2192Z"},
                {core::Xf::UniqueLines, L"Remove duplicate lines"}, {core::Xf::ReverseLines, L"Reverse the lines"}, {core::Xf::NumberLines, L"Number the lines"},
                {core::Xf::JsonFormat, L"Format JSON"}, {core::Xf::JsonMin, L"Minify JSON"}, {core::Xf::XmlFormat, L"Format XML"}, {core::Xf::SqlFormat, L"Format SQL"},
                {core::Xf::Base64Enc, L"Encode as Base64"}, {core::Xf::Base64Dec, L"Decode Base64"}, {core::Xf::UrlEncode, L"URL-encode"}, {core::Xf::UrlDecode, L"URL-decode"},
                {core::Xf::SlashesFwd, L"Path with / slashes"}, {core::Xf::SlashesBack, L"Path with \\ slashes"}, {core::Xf::Sha256, L"SHA-256 hash of the text"}};
            for (auto& x : xs) AppendMenuW(xf, MF_STRING, IDM_XF_BASE + (UINT)x.first, T(x.second));
            AppendMenuW(m, MF_POPUP, (UINT_PTR)xf, T(L"Paste &as"));
            AppendMenuW(m, MF_STRING, IDM_FINDREPL, T(L"Paste with find and &replace\u2026"));
            AppendMenuW(m, MF_STRING, IDM_TEMPLATE, T(L"Paste with today's &date and time ({date} {time})"));
            auto pipes = PipelineList();
            if (!pipes.empty()) {
                HMENU pm = CreatePopupMenu();
                for (size_t i = 0; i < pipes.size() && i < 20; i++) AppendMenuW(pm, MF_STRING, IDM_PIPE_BASE + (UINT)i, pipes[i].first.c_str());
                AppendMenuW(m, MF_POPUP, (UINT_PTR)pm, T(L"Paste &through"));
            }
        }
        if (ids.size() > 1) {
            HMENU jm = CreatePopupMenu();
            AppendMenuW(jm, MF_STRING, IDM_JOIN_BASE + 0, T(L"New line"));
            AppendMenuW(jm, MF_STRING, IDM_JOIN_BASE + 1, T(L"Comma and space"));
            AppendMenuW(jm, MF_STRING, IDM_JOIN_BASE + 2, T(L"Space"));
            AppendMenuW(jm, MF_STRING, IDM_JOIN_BASE + 3, T(L"Tab"));
            AppendMenuW(jm, MF_STRING, IDM_JOIN_BASE + 4, T(L"Nothing"));
            AppendMenuW(jm, MF_STRING, IDM_JOIN_BASE + 5, T(L"Semicolon"));
            AppendMenuW(m, MF_POPUP, (UINT_PTR)jm, T(L"Paste &joined with"));
        }
        if (one && one->kind == K_FILES) AppendMenuW(m, MF_STRING, IDM_COPY_NAMES, T(L"Copy the file &names as text"));
        AppendMenuW(m, MF_STRING, IDM_QUEUE_ADD, (std::wstring(T(L"Add to the paste &queue")) + L"\tCtrl+Q").c_str());
        if (ids.size() == 2) AppendMenuW(m, MF_STRING, IDM_DIFF, T(L"Com&pare the two items"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        if (one) {
            if (one->kind == K_TEXT) {
                if (one->st == core::ST_LINK) AppendMenuW(m, MF_STRING, IDM_OPEN_LINK, T(L"Open the &link"));
                if (one->st == core::ST_EMAIL) AppendMenuW(m, MF_STRING, IDM_OPEN_MAIL, T(L"Write an &email"));
                if (one->st == core::ST_PATH) AppendMenuW(m, MF_STRING, IDM_OPEN_PATH, T(L"Open the &path"));
            }
            if (one->kind == K_PICTURE) {
                AppendMenuW(m, MF_STRING, IDM_EDIT_PICTURE, T(L"Edit the pictu&re (crop, draw, resize)…"));
                AppendMenuW(m, MF_STRING, IDM_OCR_COPY, T(L"Copy the &text in the picture"));
                AppendMenuW(m, MF_STRING, IDM_OPEN, T(L"&Open the picture"));
            } else if (one->kind != K_FILES) {
                AppendMenuW(m, MF_STRING, IDM_EDIT, (std::wstring(T(L"&Edit…")) + L"\tCtrl+E").c_str());
                if (one->kind == K_TEXT) AppendMenuW(m, MF_STRING, IDM_SNIPPET_FROM, T(L"Save as s&nippet…"));
                AppendMenuW(m, MF_STRING, IDM_UNICODE, T(L"Inspect the &characters (Unicode)"));
                AppendMenuW(m, MF_STRING, IDM_TABLE, T(L"Show as a &table (CSV / TSV)"));
                AppendMenuW(m, MF_STRING, IDM_OPEN, T(L"&Open the text file"));
            }
            AppendMenuW(m, MF_STRING, IDM_SAVEAS, T(L"&Save as…"));
            AppendMenuW(m, MF_STRING, IDM_SHOWFOLDER, T(L"Show the &file in its folder"));
        }
        bool allPinned = true;
        for (auto id : ids) if (Item* it = g_store.Find(id)) if (!it->pinned) allPinned = false;
        AppendMenuW(m, MF_STRING, IDM_PIN, (std::wstring(allPinned ? T(L"U&npin") : T(L"P&in")) + L"\tCtrl+P").c_str());
        // groups
        g_menuGroups = g_store.Groups();
        HMENU gm = CreatePopupMenu();
        AppendMenuW(gm, MF_STRING, IDM_GROUP_NEW, T(L"New group…"));
        AppendMenuW(gm, MF_STRING, IDM_GROUP_NONE, T(L"No group"));
        if (!g_menuGroups.empty()) AppendMenuW(gm, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < g_menuGroups.size() && i < 90; i++) AppendMenuW(gm, MF_STRING, IDM_GROUP_BASE + (UINT)i, g_menuGroups[i].c_str());
        AppendMenuW(m, MF_POPUP, (UINT_PTR)gm, T(L"&Group"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, IDM_DELETE, (std::wstring(T(L"&Delete")) + L"\tDel").c_str());
        AppendMenuW(m, MF_STRING, IDM_SELECTALL, (std::wstring(T(L"Select &all")) + L"\tCtrl+A").c_str());
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, IDM_HIDE, T(L"&Hide the window"));
    }
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, screen.x, screen.y, 0, g_main, nullptr);
    DestroyMenu(m);
}

static void SetGroupForSelected(const std::wstring& g) {
    for (auto id : SelectedIds()) g_store.SetGroup(id, g);
    Rebuild(true);
}

static void TrayMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_SHOW, T(L"&Open Feather Clipboard Manager"));
    // recent items
    HMENU rec = CreatePopupMenu();
    g_recentIds.clear();
    int n = 0;
    for (size_t i = g_store.items.size(); i-- > 0 && n < 9;) {
        const Item& it = g_store.items[i];
        if (it.deleted || it.kind == K_SNIPPET) continue;
        std::wstring t = ItemTitle(it);
        if (t.size() > 50) t = t.substr(0, 50) + L"…";
        for (size_t k = 0; k < t.size(); k++) if (t[k] == L'&') { t.insert(k, 1, L'&'); k++; }
        AppendMenuW(rec, MF_STRING, IDM_RECENT_BASE + (UINT)n, t.c_str());
        g_recentIds.push_back(it.id);
        n++;
    }
    if (!n) AppendMenuW(rec, MF_STRING | MF_GRAYED, 0, T(L"Nothing copied yet"));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)rec, T(L"&Copy a recent item"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g_paused ? MF_CHECKED : 0), IDM_PAUSE, T(L"&Pause recording"));
    AppendMenuW(m, MF_STRING, IDM_OPENFOLDER, T(L"Open the Clipboard contents &folder"));
    AppendMenuW(m, MF_STRING, IDM_SETTINGS, T(L"&Settings"));
    AppendMenuW(m, MF_STRING, IDM_ABOUT, T(L"&About Feather Clipboard Manager"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_EXIT, T(L"E&xit"));
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(g_main);
    TrackPopupMenu(m, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, p.x, p.y, 0, g_main, nullptr);
    PostMessageW(g_main, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static void DoCommand(int id) {
    if (id >= IDM_RECENT_BASE && id < IDM_RECENT_BASE + 9) {
        size_t idx = (size_t)(id - IDM_RECENT_BASE);
        if (idx < g_recentIds.size()) {
            if (Item* it = g_store.Find(g_recentIds[idx])) {
                if (ClipPutItem(g_main, *it, false) == PUT_OK) Balloon(APP_NAME, T(L"Copied. Press Ctrl+V to paste it anywhere."));
            }
        }
        return;
    }
    if (id >= IDM_FILTER_BASE && id < IDM_FILTER_BASE + FL_COUNT) { curFilter = id - IDM_FILTER_BASE; g_set.filter = curFilter == FL_GROUP ? 0 : curFilter; g_set.Save(); Rebuild(false); return; }
    if (id >= IDM_FGROUP_BASE && id < IDM_FGROUP_BASE + 100) {
        size_t i = (size_t)(id - IDM_FGROUP_BASE);
        if (i < g_menuGroups.size()) { curFilter = FL_GROUP; curGroup = g_menuGroups[i]; Rebuild(false); }
        return;
    }
    if (id >= IDM_GROUP_BASE && id < IDM_GROUP_BASE + 100) {
        size_t i = (size_t)(id - IDM_GROUP_BASE);
        if (i < g_menuGroups.size()) SetGroupForSelected(g_menuGroups[i]);
        return;
    }
    if (id >= IDM_PIPE_BASE && id < IDM_PIPE_BASE + 20) {
        auto pipes = PipelineList();
        size_t i = (size_t)(id - IDM_PIPE_BASE);
        if (i < pipes.size()) {
            std::wstring spec = pipes[i].second, perr;
            std::wstring probe;
            if (!core::RunPipeline(spec, L"", probe, perr)) { MsgInfo(g_main, perr, MB_ICONWARNING); return; }
            g_customXf = [spec](const std::wstring& t) { std::wstring o, e; return core::RunPipeline(spec, t, o, e) ? o : t; };
            DoPut(5, core::Xf::Plain, L"", true);
        }
        return;
    }
    if (id >= IDM_XF_BASE && id < IDM_XF_BASE + 30) { DoPut(2, (core::Xf)(id - IDM_XF_BASE), L"", true); return; }
    if (id >= IDM_JOIN_BASE && id < IDM_JOIN_BASE + 6) {
        static const wchar_t* seps[] = {L"\r\n", L", ", L" ", L"\t", L"", L"; "};
        DoPut(3, core::Xf::Plain, seps[id - IDM_JOIN_BASE], true);
        return;
    }
    switch (id) {
    case IDM_FINDREPL: {
        std::wstring find, repl;
        SetDialogLabels(T(L"Find"), T(L"Replace with"));
        if (!EditTextDialog(g_main, T(L"Find and replace"), &find, repl, false) || find.empty()) break;
        g_customXf = [find, repl](const std::wstring& t) { return core::FindReplace(t, find, repl, false); };
        DoPut(5, core::Xf::Plain, L"", true);
        break;
    }
    case IDM_TEMPLATE: {
        time_t now = time(nullptr);
        struct tm lt;
        localtime_s(&lt, &now);
        g_customXf = [lt](const std::wstring& t) { return core::ExpandTemplate(t, lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, lt.tm_wday); };
        DoPut(5, core::Xf::Plain, L"", true);
        break;
    }
    case IDM_QUEUE_ADD: {
        auto ids = SelectedIds();
        std::sort(ids.begin(), ids.end());                    // ids grow with time: oldest first
        for (auto i : ids) if (std::find(g_queue.begin(), g_queue.end(), i) == g_queue.end()) g_queue.push_back(i);
        SetStatus(std::to_wstring(g_queue.size()) + L" " + T(L"in the paste queue. Ctrl+Alt+N pastes the next one in the program you are using."), 6000);
        break;
    }
    case IDM_QUEUE_CLEAR: g_queue.clear(); SetStatus(T(L"The paste queue is empty."), 3000); break;
    case IDM_DIFF: {
        auto ids = SelectedIds();
        if (ids.size() == 2) {
            std::sort(ids.begin(), ids.end());
            Item* x = g_store.Find(ids[0]);
            Item* y = g_store.Find(ids[1]);
            if (x && y) TextBoxDialog(g_main, T(L"Compare the two items"), std::wstring(T(L"- only in the older item, + only in the newer item")) + L"\r\n\r\n" + core::LineDiff(TextOfItem(*x), TextOfItem(*y)));
        }
        break;
    }
    case IDM_UNICODE: if (Item* it = SelectedOne()) TextBoxDialog(g_main, T(L"Characters (Unicode)"), core::InspectUnicode(g_store.ReadText(*it))); break;
    case IDM_TABLE:
        if (Item* it = SelectedOne()) {
            std::wstring t = core::FormatTable(g_store.ReadText(*it));
            if (t.empty()) SetStatus(T(L"This text does not look like a table (CSV or tab separated)."), 4000);
            else TextBoxDialog(g_main, T(L"Table"), t);
        }
        break;
    case IDM_FORMATS: ShowClipboardFormats(g_main); break;
    case IDM_SHORTCUTS: ShowShortcuts(g_main); break;
    case IDM_LOG: TextBoxDialog(g_main, T(L"Diagnostic log"), ReadLog()); break;
    case IDM_TOPMOST:
        g_set.alwaysOnTop = !g_set.alwaysOnTop;
        g_set.Save();
        SetWindowPos(g_main, g_set.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        break;
    case IDM_INTEGRITY: {
        std::wstring rep;
        bool problems = g_store.CheckIntegrity(rep, false);
        if (problems) {
            if (MsgAsk(g_main, rep + L"\r\n" + T(L"Repair now? Records without a file are removed from the history; nothing else is deleted."))) { g_store.CheckIntegrity(rep, true); Rebuild(false); MsgInfo(g_main, rep); }
        } else MsgInfo(g_main, rep + L"\r\n" + T(L"No problems found."));
        break;
    }
    case IDM_PASTE: DoPut(0, core::Xf::Plain, L"", true); break;
    case IDM_PASTE_PLAIN: DoPut(1, core::Xf::Plain, L"", true); break;
    case IDM_COPY: DoPut(0, core::Xf::Plain, L"", false); break;
    case IDM_COPY_NAMES: {
        Item* it = SelectedOne();
        if (it) {
            std::vector<std::wstring> names;
            for (auto& f : g_store.ReadFiles(*it)) { size_t s = f.find_last_of(L"\\/"); names.push_back(s == std::wstring::npos ? f : f.substr(s + 1)); }
            AfterPut(ClipPutText(g_main, core::Join(names, L"\r\n")), false);
        }
        break;
    }
    case IDM_OPEN: OpenItemFile(); break;
    case IDM_SAVEAS: SaveAs(); break;
    case IDM_SHOWFOLDER: if (Item* it = SelectedOne()) ShowInExplorer(g_store.MainPath(*it)); break;
    case IDM_PIN: TogglePin(); break;
    case IDM_DELETE: DeleteSelected(); break;
    case IDM_DELETE_FOREVER: DeleteSelected(); break;
    case IDM_RESTORE: g_store.Restore(SelectedIds()); Rebuild(false); break;
    case IDM_EMPTY_TRASH:
        if (MsgAsk(g_main, T(L"Delete everything in Recently deleted for good? This can not be undone."))) { g_store.PurgeAllDeleted(); Rebuild(false); }
        break;
    case IDM_EDIT: case IDM_EDIT_PICTURE: EditSelected(); break;
    case IDM_OCR_COPY: CopyOcrText(); break;
    case IDM_SNIPPET_NEW: NewSnippet(); break;
    case IDM_SNIPPET_FROM: if (Item* it = SelectedOne()) NewSnippet(g_store.ReadText(*it)); break;
    case IDM_SELECTALL: ListView_SetItemState(hList, -1, LVIS_SELECTED, LVIS_SELECTED); UpdatePreview(); break;
    case IDM_PAUSE: TogglePause(); break;
    case IDM_DELALL: {
        int n = g_store.CountUnpinned();
        if (!n) { MsgInfo(g_main, T(L"There are no unpinned items.")); break; }
        if (MsgAsk(g_main, std::wstring(T(L"Delete all")) + L" " + std::to_wstring(n) + L" " + T(L"unpinned items? Pinned items are kept. They can be brought back from Recently deleted for 7 days."))) {
            std::vector<uint64_t> ids;
            for (auto& it : g_store.items) if (!it.pinned && !it.deleted && it.kind != K_SNIPPET) ids.push_back(it.id);
            g_store.SoftDelete(ids);
            g_lastDeleted = ids;
            Rebuild(false);
            SetStatus(T(L"Deleted. Press Ctrl+Z to bring them back."), 6000);
        }
        break;
    }
    case IDM_CLEARALL: {
        int total = (int)g_store.items.size(), pinned = 0, snips = 0;
        for (auto& it : g_store.items) { if (it.pinned) pinned++; if (it.kind == K_SNIPPET) snips++; }
        if (!total) { MsgInfo(g_main, T(L"There is nothing to delete.")); break; }
        std::wstring q = std::wstring(T(L"Delete EVERYTHING?")) + L"\n\n" + std::to_wstring(total) + L" " + T(L"items will be deleted for good,") + L" " + T(L"including") + L" " + std::to_wstring(pinned) + L" " + T(L"pinned items and") + L" " + std::to_wstring(snips) + L" " + T(L"snippets, with their files and the text read from pictures.") + L"\n\n" +
                         T(L"This can not be undone and nothing goes to Recently deleted. Files you copied (the originals) are not touched.") + L"\n\n" +
                         T(L"Note: deleting is not secure erasure. Other programs, Windows' own clipboard history, backups you made and an SSD may still hold copies.");
        if (MessageBoxW(g_main, q.c_str(), APP_NAME, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) break;
        g_store.ClearAll();
        g_queue.clear();
        g_lastDeleted.clear();
        Rebuild(false);
        SetStatus(T(L"Everything was deleted."), 5000);
        LogEvent(L"All history was deleted by the user (" + std::to_wstring(total) + L" items).");
        break;
    }
    case IDM_UNDO: UndoDelete(); break;
    case IDM_SETTINGS: ShowSettingsDialog(g_main); break;
    case IDM_ABOUT: ShowAbout(g_main); break;
    case IDM_STATS: ShowStats(g_main); break;
    case IDM_COMPACT: ToggleCompact(); break;
    case IDM_EXPORT: ExportBackup(); break;
    case IDM_IMPORT: ImportBackup(); break;
    case IDM_UPDATE: SetStatus(T(L"Looking for a new version\u2026"), 8000); UpdateCheckAsync(g_main, true); break;
    case IDM_OPENFOLDER: OpenShell(g_main, g_store.dir); break;
    case IDM_AUTOSTART:
        if (!SetAutostart(!g_set.autostart)) MsgInfo(g_main, T(L"The start-up setting could not be changed."), MB_ICONWARNING);
        break;
    case IDM_HIDE: HideMain(); break;
    case IDM_SHOW: ShowMain(false); break;
    case IDM_EXIT: DestroyWindow(g_main); break;
    case IDM_OPEN_LINK: if (Item* it = SelectedOne()) { std::wstring t = core::Trim(g_store.ReadText(*it)); if (core::StartsWith(core::Lower(t), L"www.")) t = L"https://" + t; OpenShell(g_main, t); } break;
    case IDM_OPEN_MAIL: if (Item* it = SelectedOne()) OpenShell(g_main, L"mailto:" + core::Trim(g_store.ReadText(*it))); break;
    case IDM_OPEN_PATH: if (Item* it = SelectedOne()) { std::wstring p = core::Trim(g_store.ReadText(*it)); if (FileExistsW(p)) OpenShell(g_main, p); else SetStatus(T(L"That path does not exist."), 3000); } break;
    case IDM_GROUP_NONE: SetGroupForSelected(L""); break;
    case IDM_GROUP_NEW: {
        std::wstring g;
        if (EditTextDialog(g_main, T(L"New group"), nullptr, g, false) && !core::Trim(g).empty()) SetGroupForSelected(core::Trim(g));
        break;
    }
    }
}

// ---------------------------------------------------------------- updates (like Feather PDF)
static void RunUpdateFlow(const UpdateInfo& info) {
    std::wstring version = core::FromUtf8(info.rel.version);
    bool skip = false;
    bool now = UpdatePrompt(g_main, info, skip);
    if (skip) { g_set.skipVersion = version; g_set.Save(); }
    if (!now) return;
    std::string setup;
    std::wstring err;
    if (!DownloadWithProgress(g_main, std::wstring(T(L"Downloading")) + L" " + APP_NAME + L" " + version + L"\u2026", info.rel.url, info.via, setup, err)) {
        if (err != L"Stopped.") MsgInfo(g_main, std::wstring(T(L"The new version could not be downloaded.")) + L"\n\n" + err, MB_ICONWARNING);
        return;
    }
    std::string want = core::Lower(core::FromUtf8(info.rel.sha256)).empty() ? std::string() : core::ToUtf8(core::Lower(core::FromUtf8(info.rel.sha256)));
    if (want.empty() || core::Sha256Hex(setup) != want) {
        MsgInfo(g_main, T(L"The downloaded file is not the right one (it may have been damaged on the way), so it was not installed. Try again later."), MB_ICONWARNING);
        return;
    }
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"FeatherClipboardManager-Setup-" + version + L".exe";
    if (!WriteFileAtomic(path, setup.data(), setup.size())) { MsgInfo(g_main, T(L"The new version could not be saved to the temporary folder."), MB_ICONWARNING); return; }
    std::wstring args = L"/S /relaunch /waitfor=" + std::to_wstring(GetCurrentProcessId());
    if ((INT_PTR)ShellExecuteW(g_main, L"open", path.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL) <= 32) {
        MsgInfo(g_main, T(L"The installer could not be started. Nothing was changed."), MB_ICONWARNING);
        return;
    }
    DestroyWindow(g_main);     // the installer waits for this process to end, installs, and starts the new version
}

static void OnUpdateChecked(UpdateInfo* info, bool manual) {
    std::unique_ptr<UpdateInfo> own(info);
    if (!info->problem.empty()) {
        if (manual) MsgInfo(g_main, std::wstring(T(L"Could not find out whether there is a new version.")) + L"\n\n" + info->problem + L"\n\n" + T(L"If the internet works in your browser, a proxy, VPN or security program may be stopping it. The new version can also be downloaded from") + L" github.com/akshsim1991/FeatherClipboardManager-releases", MB_ICONWARNING);
        return;
    }
    bool newer = core::CompareVersions(info->rel.version, core::ToUtf8(APP_VERSION)) > 0 && !info->rel.url.empty();
    std::wstring version = core::FromUtf8(info->rel.version);
    if (!newer) {
        if (manual) MsgInfo(g_main, std::wstring(T(L"You have the newest version of")) + L" " + APP_NAME + L" (" + APP_VERSION + L").");
        return;
    }
    if (!manual && version == g_set.skipVersion) return;
    if (manual || IsWindowVisible(g_main)) { RunUpdateFlow(*info); return; }
    // the window is hidden in the tray: tell quietly, and ask when the window is opened or the notice is clicked
    delete g_pendingUpdate;
    g_pendingUpdate = own.release();
    Balloon(APP_NAME, version + L" " + T(L"is out. Click here to update, or open the window."));
}

// ---------------------------------------------------------------- keyboard
static bool HandleKey(MSG* msg) {
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN) return false;
    if (!g_main || !IsWindowVisible(g_main)) return false;
    HWND fw = GetAncestor(msg->hwnd, GA_ROOT);
    if (fw != g_main) return false;
    bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0, alt = GetKeyState(VK_MENU) < 0;
    HWND f = GetFocus();
    bool inSearch = f == hSearch, inText = f == hPrevText;
    WPARAM k = msg->wParam;
    if (ctrl && !alt && k >= '1' && k <= '9') {
        int n = (int)(k - '0');
        if ((int)view.size() >= n) {
            ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
            ListView_SetItemState(hList, n - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            DoPut(shift ? 1 : 0, core::Xf::Plain, L"", true);
        }
        return true;
    }
    if (k == VK_ESCAPE) {
        if (inSearch && GetWindowTextLengthW(hSearch)) { SetWindowTextW(hSearch, L""); return true; }
        HideMain();
        return true;
    }
    if (ctrl && k == 'F') { SetFocus(hSearch); SendMessageW(hSearch, EM_SETSEL, 0, -1); return true; }
    if (ctrl && k == 'N') { DoCommand(IDM_SNIPPET_NEW); return true; }
    if (ctrl && k == 'Z' && !inSearch) { UndoDelete(); return true; }
    if (ctrl && k == 'P') { TogglePin(); return true; }
    if (ctrl && k == 'E') { EditSelected(); return true; }
    if (ctrl && k == 'Q') { DoCommand(IDM_QUEUE_ADD); return true; }
    if (ctrl && k == VK_OEM_COMMA) { ShowSettingsDialog(g_main); return true; }
    if (!inSearch && !inText) {
        if (ctrl && k == 'A') { DoCommand(IDM_SELECTALL); return true; }
        if (ctrl && k == 'C') { DoPut(0, core::Xf::Plain, L"", false); return true; }
        if (k == VK_DELETE) { DeleteSelected(); return true; }
    }
    if (inText && ctrl && k == 'A') { SendMessageW(hPrevText, EM_SETSEL, 0, -1); return true; }
    if (k == VK_RETURN && !inText) {
        if (view.empty()) return true;
        if (SelectedIds().empty()) ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        DoPut(shift ? 1 : 0, core::Xf::Plain, L"", true);
        return true;
    }
    if (inSearch && (k == VK_DOWN || k == VK_UP || k == VK_NEXT || k == VK_PRIOR)) {
        SendMessageW(hList, WM_KEYDOWN, k, 0);
        return true;
    }
    if (inSearch && k == VK_DELETE) return false;
    return false;
}

// ---------------------------------------------------------------- list rows, with the search words highlighted
// While searching, long texts show the part around the first match (with "..." before it), so the match can be seen.
static std::wstring SearchExcerpt(const Item& it) {
    if (g_hlNeedles.empty() || it.kind != K_TEXT || it.head.empty()) return L"";
    std::wstring low = core::Lower(it.head);
    size_t best = std::wstring::npos;
    for (auto& nd : g_hlNeedles) {
        if (nd.empty()) continue;
        size_t p = low.find(nd);
        if (p != std::wstring::npos && (best == std::wstring::npos || p < best)) best = p;
    }
    if (best == std::wstring::npos || best < 20) return L"";            // already at the start: the normal line shows it
    size_t from = best - 10;
    return L"\u2026" + it.head.substr(from, 220);
}

static std::wstring CellText(const Item& it, int sub) {
    if (sub == 0) return KindLabel(it);
    if (sub == 1) {
        std::wstring ex = SearchExcerpt(it);
        return (it.pinned && it.kind != K_SNIPPET ? L"\u25CF " : L"") + (ex.empty() ? ItemTitle(it) : ex);
    }
    return FormatTime((time_t)it.created);
}

static void FillCell(HDC dc, const RECT& r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

// draws text in a cell, cut with an ellipsis, with every needle found in it on a highlight background
static void DrawCellText(HDC dc, std::wstring text, RECT rc, COLORREF fg, bool selected, bool right, bool highlight) {
    int avail = rc.right - rc.left;
    if (avail < 4 || text.empty()) return;
    SIZE sz;
    GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &sz);
    std::vector<char> mark(text.size(), 0);
    if (highlight) {
        std::wstring low = core::Lower(text);
        for (auto& nd : g_hlNeedles) {
            if (nd.empty()) continue;
            for (size_t p = low.find(nd); p != std::wstring::npos; p = low.find(nd, p + nd.size()))
                for (size_t k = p; k < p + nd.size() && k < mark.size(); k++) mark[k] = 1;
        }
    }
    if (sz.cx > avail) {                                   // too long: keep what fits, then an ellipsis
        SIZE el;
        GetTextExtentPoint32W(dc, L"\u2026", 1, &el);
        int fit = 0;
        GetTextExtentExPointW(dc, text.c_str(), (int)text.size(), std::max<int>(0, avail - (int)el.cx), &fit, nullptr, &sz);
        text = text.substr(0, (size_t)fit) + L"\u2026";
        mark.resize(text.size(), 0);
        GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &sz);
    }
    int x = right ? rc.right - sz.cx : rc.left;
    COLORREF hiBg = g_th.dark ? RGB(122, 98, 0) : RGB(255, 224, 96), hiFg = g_th.dark ? RGB(255, 255, 255) : RGB(0, 0, 0);
    int top = rc.top + (rc.bottom - rc.top - sz.cy) / 2;
    size_t i = 0;
    while (i < text.size()) {
        size_t j = i;
        while (j < text.size() && mark[j] == mark[i]) j++;
        SIZE seg;
        GetTextExtentPoint32W(dc, text.c_str() + i, (int)(j - i), &seg);
        if (mark[i]) {
            RECT hr = {x, top, x + seg.cx, top + sz.cy};
            FillCell(dc, hr, hiBg);
            SetTextColor(dc, hiFg);
        } else SetTextColor(dc, fg);
        TextOutW(dc, x, top, text.c_str() + i, (int)(j - i));
        x += seg.cx;
        i = j;
    }
    (void)selected;
}

static LRESULT DrawListItem(NMLVCUSTOMDRAW* cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        int idx = (int)cd->nmcd.dwItemSpec, sub = cd->iSubItem;
        if (idx < 0 || idx >= (int)view.size()) return CDRF_DODEFAULT;
        RECT row;
        ListView_GetItemRect(hList, idx, &row, LVIR_BOUNDS);
        int x = row.left;
        for (int c = 0; c < sub; c++) x += ListView_GetColumnWidth(hList, c);
        RECT cell = {x, row.top, x + ListView_GetColumnWidth(hList, sub), row.bottom};
        if (sub == 2) cell.right = std::max(cell.right, row.right);
        HDC dc = cd->nmcd.hdc;
        bool sel = (ListView_GetItemState(hList, idx, LVIS_SELECTED) & LVIS_SELECTED) != 0;
        FillCell(dc, cell, sel ? g_th.sel : g_th.panel);
        const Item& it = g_store.items[view[idx]];
        SetBkMode(dc, TRANSPARENT);
        HGDIOBJ oldFont = SelectObject(dc, g_font);
        RECT tr = cell;
        tr.left += S(8);
        tr.right -= S(8);
        COLORREF fg = sel ? g_th.selText : (sub == 1 ? g_th.text : g_th.sub);
        DrawCellText(dc, CellText(it, sub), tr, fg, sel, sub == 2, sub == 1 && !g_hlNeedles.empty());
        SelectObject(dc, oldFont);
        if (sub == 2 && (ListView_GetItemState(hList, idx, LVIS_FOCUSED) & LVIS_FOCUSED) && GetFocus() == hList) {
            RECT fr = row;
            fr.right = cell.right;
            DrawFocusRect(dc, &fr);
        }
        return CDRF_SKIPDEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

// ---------------------------------------------------------------- window procedure
static void CreateChildren() {
    DWORD ed = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    hSearch = CreateWindowExW(0, L"EDIT", L"", ed, 0, 0, 10, 10, g_main, (HMENU)IDC_SEARCH, g_inst, nullptr);
    SendMessageW(hSearch, EM_SETLIMITTEXT, 200, 0);
    hFilterBtn = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, g_main, (HMENU)IDC_FILTER, g_inst, nullptr);
    for (int i = 0; i < 6; i++)
        hBtn[i] = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, g_main, (HMENU)(UINT_PTR)BTN_IDS[i], g_inst, nullptr);
    hList = CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER, 0, 0, 10, 10, g_main, (HMENU)IDC_LIST, g_inst, nullptr);
    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    LVCOLUMNW c = {};
    c.mask = LVCF_WIDTH | LVCF_FMT;
    c.fmt = LVCFMT_LEFT;
    c.cx = 80;
    ListView_InsertColumn(hList, 0, &c);
    ListView_InsertColumn(hList, 1, &c);
    c.fmt = LVCFMT_RIGHT;
    ListView_InsertColumn(hList, 2, &c);
    HIMAGELIST il = ImageList_Create(1, S(30), ILC_COLOR32, 1, 1);   // makes the rows taller
    ListView_SetImageList(hList, il, LVSIL_SMALL);
    hPrev = CreateWindowExW(0, L"CmPreview", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 10, 10, g_main, (HMENU)IDC_PREVIEW, g_inst, nullptr);
    hPrevText = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_VSCROLL, 0, 0, 10, 10, g_main, (HMENU)IDC_PREVTEXT, g_inst, nullptr);
    hStatus = CreateWindowExW(0, L"CmStatus", L"", WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, g_main, (HMENU)IDC_STATUS, g_inst, nullptr);
    hTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, g_main, nullptr, g_inst, nullptr);
    for (int i = 0; i < 6; i++) {
        TOOLINFOW ti = {sizeof ti};
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = g_main;
        ti.uId = (UINT_PTR)hBtn[i];
        ti.lpszText = (LPWSTR)L"";
        SendMessageW(hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }
    SendMessageW(hTip, TTM_SETMAXTIPWIDTH, 0, 360);
    SetFontAll(g_main, g_font);
    SendMessageW(hList, WM_SETFONT, (WPARAM)g_font, TRUE);
    SetWindowTheme(hSearch, L"", L"");
}

static void ApplyDpi(int dpi) {
    g_dpi = dpi;
    MakeFonts();
    SetFontAll(g_main, g_font);
    SetButtonGlyphs();
    HIMAGELIST il = ImageList_Create(1, S(30), ILC_COLOR32, 1, 1);
    HIMAGELIST old = ListView_SetImageList(hList, il, LVSIL_SMALL);
    if (old) ImageList_Destroy(old);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TASKBARCREATED && m != 0) { if (g_inTray) TrayAdd(); return 0; }
    switch (m) {
    case WM_CREATE: {
        g_main = h;
        AddClipboardFormatListener(h);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(h, &rc);
        FillRect((HDC)w, &rc, brBg);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        // rounded box behind the search box
        RECT sr;
        GetWindowRect(hSearch, &sr);
        MapWindowPoints(nullptr, h, (POINT*)&sr, 2);
        InflateRect(&sr, S(10), S(6));
        HBRUSH b = CreateSolidBrush(g_th.edit);
        HPEN p = CreatePen(PS_SOLID, 1, g_th.border);
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
        RoundRect(dc, sr.left, sr.top, sr.right, sr.bottom, S(8), S(8));
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(p);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        bool isSearch = (HWND)l == hSearch;
        SetTextColor(dc, g_th.text);
        SetBkColor(dc, isSearch ? g_th.edit : g_th.panel);
        return (LRESULT)(isSearch ? brEdit : brPanel);
    }
    case WM_CTLCOLORSTATIC: {
        SetTextColor((HDC)w, g_th.text);
        SetBkColor((HDC)w, g_th.panel);
        return (LRESULT)brPanel;
    }
    case WM_DRAWITEM: {
        auto* d = (DRAWITEMSTRUCT*)l;
        if (d->CtlID == IDC_FILTER) return DrawThemedButton(d, false);
        if (d->CtlID >= IDC_B_PASTE && d->CtlID <= IDC_B_MORE) return DrawThemedButton(d, true);
        break;
    }
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) Layout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)l;
        mm->ptMinTrackSize.x = S(560);
        mm->ptMinTrackSize.y = S(340);
        return 0;
    }
    case WM_DPICHANGED: {
        ApplyDpi(HIWORD(w));
        RECT* r = (RECT*)l;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }
    case WM_SETTINGCHANGE:
        if (l && wcscmp((const wchar_t*)l, L"ImmersiveColorSet") == 0 && g_set.theme == 0) { ApplyTheme(); for (int i = 0; i < 6; i++) InvalidateRect(hBtn[i], nullptr, TRUE); InvalidateRect(h, nullptr, TRUE); }
        return 0;
    case WM_LBUTTONDOWN: {
        // the splitter between the list and the preview
        if (!g_set.compact) {
            RECT lr;
            GetWindowRect(hList, &lr);
            MapWindowPoints(nullptr, h, (POINT*)&lr, 2);
            int x = GET_X_LPARAM(l);
            if (x >= lr.right && x <= lr.right + S(6)) { g_splitDrag = true; SetCapture(h); }
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_splitDrag) {
            g_set.listWidth = GET_X_LPARAM(l) - S(10);
            Layout();
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_splitDrag) { g_splitDrag = false; ReleaseCapture(); g_set.Save(); }
        return 0;
    case WM_SETCURSOR:
        if (!g_set.compact && LOWORD(l) == HTCLIENT) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(h, &p);
            RECT lr;
            GetWindowRect(hList, &lr);
            MapWindowPoints(nullptr, h, (POINT*)&lr, 2);
            if (p.x >= lr.right && p.x <= lr.right + S(6) && p.y >= lr.top) { SetCursor(LoadCursor(nullptr, IDC_SIZEWE)); return TRUE; }
        }
        break;
    case WM_CLIPBOARDUPDATE:
        SetTimer(h, TM_CAPTURE, 40, nullptr);
        return 0;
    case WM_TIMER:
        switch (w) {
        case TM_CAPTURE: KillTimer(h, TM_CAPTURE); OnCapture(); break;
        case TM_PASTE: KillTimer(h, TM_PASTE); ReleaseModifiersAndSend(); break;
        case TM_CLEAN:
            if (g_store.Cleanup() && IsWindowVisible(h)) Rebuild(true);
            break;
        case TM_SEARCH: KillTimer(h, TM_SEARCH); curQuery = TextAt(hSearch); Rebuild(false); break;
        case TM_STATUS: KillTimer(h, TM_STATUS); g_status.clear(); UpdateStatus(); break;
        case TM_UPDATE: KillTimer(h, TM_UPDATE); if (g_set.checkUpdates) UpdateCheckAsync(h, false); break;
        }
        return 0;
    case WM_HOTKEY:
        if (w == HK_OPEN) {
            if (IsWindowVisible(h) && GetForegroundWindow() == h) HideMain();
            else ShowMain(true);
        } else if (w == HK_PLAIN) GlobalPasteNth(1, true);
        else if (w == HK_QUEUE) GlobalPasteQueue();
        else if (w >= HK_QUICK && w < HK_QUICK + 9) GlobalPasteNth((int)(w - HK_QUICK + 1), false);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == IDC_SEARCH && code == EN_CHANGE) { SetTimer(h, TM_SEARCH, 180, nullptr); return 0; }
        if (id == IDC_FILTER) { ShowFilterMenu(); return 0; }
        if (id == IDC_B_PASTE) { DoCommand(IDM_PASTE); return 0; }
        if (id == IDC_B_COPY) { DoCommand(IDM_COPY); return 0; }
        if (id == IDC_B_PIN) { TogglePin(); return 0; }
        if (id == IDC_B_DELETE) { DeleteSelected(); return 0; }
        if (id == IDC_B_FOLDER) { OpenShell(h, g_store.dir); return 0; }
        if (id == IDC_B_MORE) {
            HMENU mm = BuildMoreMenu();
            RECT r;
            GetWindowRect(hBtn[5], &r);
            TrackPopupMenu(mm, TPM_RIGHTALIGN | TPM_TOPALIGN, r.right, r.bottom, 0, h, nullptr);
            DestroyMenu(mm);
            return 0;
        }
        if (id >= 1000) { DoCommand(id); return 0; }
        break;
    }
    case WM_NOTIFY: {
        NMHDR* nh = (NMHDR*)l;
        if (nh->hwndFrom == hList) {
            switch (nh->code) {
            case LVN_GETDISPINFOW: {
                auto* di = (NMLVDISPINFOW*)l;
                if (!(di->item.mask & LVIF_TEXT) || di->item.iItem < 0 || di->item.iItem >= (int)view.size()) break;
                const Item& it = g_store.items[view[di->item.iItem]];
                static thread_local std::wstring buf;
                if (di->item.iSubItem == 0) buf = KindLabel(it);
                else if (di->item.iSubItem == 1) buf = (it.pinned && it.kind != K_SNIPPET ? L"● " : L"") + ItemTitle(it);
                else buf = FormatTime((time_t)it.created);
                di->item.pszText = (LPWSTR)buf.c_str();
                return 0;
            }
            case LVN_ITEMCHANGED: {
                auto* nl = (NMLISTVIEW*)l;
                if (nl->uChanged & LVIF_STATE) { UpdatePreview(); UpdateStatus(); }
                return 0;
            }
            case NM_CUSTOMDRAW: return DrawListItem((NMLVCUSTOMDRAW*)l);
            case NM_DBLCLK: DoCommand(IDM_PASTE); return 0;
            case NM_RCLICK: {
                POINT p;
                GetCursorPos(&p);
                LVHITTESTINFO hti = {};
                hti.pt = p;
                ScreenToClient(hList, &hti.pt);
                int idx = ListView_HitTest(hList, &hti);
                if (idx >= 0 && !(ListView_GetItemState(hList, idx, LVIS_SELECTED) & LVIS_SELECTED)) {
                    ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
                    ListView_SetItemState(hList, idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                }
                ShowContextMenu(p);
                return 0;
            }
            }
        }
        break;
    }
    case WM_CONTEXTMENU:
        if ((HWND)w == hPrevText || (HWND)w == hPrev) { POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)}; if (p.x == -1) GetCursorPos(&p); ShowContextMenu(p); return 0; }
        break;
    case WM_TRAY:
        if (l == WM_LBUTTONUP) { if (IsWindowVisible(h)) HideMain(); else ShowMain(false); }
        else if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) TrayMenu();
        else if (l == NIN_BALLOONUSERCLICK && g_pendingUpdate) { ShowMain(false); }
        return 0;
    case WM_OCR_DONE: {
        OcrResult* r = (OcrResult*)l;
        if (r) {
            g_store.SetOcr(r->id, r->text);
            delete r;
            if (IsWindowVisible(h)) { Rebuild(true); }
        }
        return 0;
    }
    case WM_UPDATE_RESULT: OnUpdateChecked((UpdateInfo*)l, w != 0); return 0;
    case WM_SHOW_REQUEST: ShowMain(false); return 0;
    case WM_EXIT_REQUEST: DestroyWindow(h); return 0;
    case WM_ACTIVATE:
        return 0;
    case WM_CLOSE:
        if (g_set.closeToTray) { HideMain(); return 0; }
        DestroyWindow(h);
        return 0;
    case WM_ENDSESSION:
        if (w) { SavePlacement(); g_store.Save(); }
        return 0;
    case WM_DESTROY:
        SavePlacement();
        g_set.Save();
        RemoveClipboardFormatListener(h);
        for (int i = 0; i < 20; i++) UnregisterHotKey(h, i);
        TrayRemove();
        g_store.Save();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------------------------------------------------------- start
static HICON LoadAppIcon(int id, int size) {
    return (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(id), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
}

int RunApp(HINSTANCE inst, const std::wstring& cmdline) {
    g_inst = inst;
    bool trayArg = cmdline.find(L"/tray") != std::wstring::npos || cmdline.find(L"--tray") != std::wstring::npos;
    bool exitArg = cmdline.find(L"--exit") != std::wstring::npos || cmdline.find(L"/exit") != std::wstring::npos;

    HANDLE mutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(CLS_MAIN, nullptr);
        if (other) PostMessageW(other, exitArg ? WM_EXIT_REQUEST : (trayArg ? WM_NULL : WM_SHOW_REQUEST), 0, 0);
        return 0;
    }
    if (exitArg) return 0;

    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);
    OleInitialize(nullptr);
    g_set.Load();
    SetLanguage(g_set.language);
    curFilter = std::clamp(g_set.filter, 0, FL_COUNT - 1);
    if (curFilter == FL_GROUP) curFilter = 0;
    if (!g_store.Init()) {
        MessageBoxW(nullptr, (std::wstring(T(L"The history can not be saved: no folder is writable.")) + L"\n" + g_store.note).c_str(), APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }
    ClipInit();
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    HDC sdc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSY);
    ReleaseDC(nullptr, sdc);
    MakeFonts();

    g_icoOn = LoadAppIcon(1, GetSystemMetrics(SM_CXSMICON));
    g_icoOff = LoadAppIcon(2, GetSystemMetrics(SM_CXSMICON));
    g_icoBig = LoadAppIcon(1, GetSystemMetrics(SM_CXICON));

    WNDCLASSEXW wc = {sizeof wc};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = CLS_MAIN;
    wc.hIcon = g_icoBig;
    wc.hIconSm = g_icoOn;
    wc.style = CS_DBLCLKS;
    RegisterClassExW(&wc);
    WNDCLASSEXW pc = {sizeof pc};
    pc.lpfnWndProc = PreviewProc;
    pc.hInstance = inst;
    pc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    pc.lpszClassName = L"CmPreview";
    pc.style = CS_DBLCLKS;
    RegisterClassExW(&pc);
    WNDCLASSEXW sc = {sizeof sc};
    sc.lpfnWndProc = StatusProc;
    sc.hInstance = inst;
    sc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    sc.lpszClassName = L"CmStatus";
    RegisterClassExW(&sc);

    WINDOWPLACEMENT wp = {};
    bool havePlacement = GetBlobReg(L"WindowPlacement", &wp, sizeof wp) && wp.length == sizeof wp;
    int W = S(940), H = S(600), X = CW_USEDEFAULT, Y = CW_USEDEFAULT;
    if (havePlacement) {
        W = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        H = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        X = wp.rcNormalPosition.left;
        Y = wp.rcNormalPosition.top;
        RECT vr = {X, Y, X + W, Y + H};
        if (!MonitorFromRect(&vr, MONITOR_DEFAULTTONULL)) { X = Y = CW_USEDEFAULT; }
    }
    g_main = CreateWindowExW(0, CLS_MAIN, APP_NAME, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, X, Y, W, H, nullptr, nullptr, inst, nullptr);
    if (!g_main) return 1;
    // per-monitor DPI of the window we got
    typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
    auto gdfw = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    int wdpi = gdfw ? (int)gdfw(g_main) : g_dpi;

    CreateChildren();
    if (wdpi != g_dpi) ApplyDpi(wdpi);
    SetButtonGlyphs();
    SendMessageW(hSearch, EM_SETCUEBANNER, TRUE, (LPARAM)T(L"Search everything you copied  (Ctrl+F)"));
    ApplyTheme();
    SettingsChanged();
    RegisterHotkeys();
    TrayAdd();
    SetTimer(g_main, TM_CLEAN, 5000, nullptr);
    if (g_set.checkUpdates) SetTimer(g_main, TM_UPDATE, 4000, nullptr);
    g_store.Cleanup();

    bool startHidden = trayArg || g_set.startInTray;
    if (!startHidden) {
        ShowWindow(g_main, havePlacement && wp.showCmd == SW_SHOWMAXIMIZED ? SW_SHOWMAXIMIZED : SW_SHOW);
        Rebuild(false);
        SetForegroundWindow(g_main);
        SetFocus(hSearch);
    } else {
        Rebuild(false);
    }
    Layout();

    if (!g_startupHotkeyProblem.empty()) {
        std::wstring msg = g_startupHotkeyProblem + L"\n" + T(L"Choose another shortcut in More ▸ Settings.");
        if (startHidden) Balloon(APP_NAME, msg); else MsgInfo(g_main, msg, MB_ICONWARNING);
    }
    if (!g_set.askedAutostart) {
        g_set.askedAutostart = true;
        g_set.Save();
        if (!g_set.autostart && MsgAsk(g_main, std::wstring(T(L"Start Feather Clipboard Manager with Windows?")) + L"\n\n" + T(L"It can only keep what you copy while it is running. It then waits quietly in the notification area.")))
            SetAutostart(true);
    }

    size_t qp = cmdline.find(L"--query ");                              // for screenshots and checks: start with this search
    if (qp != std::wstring::npos) { std::wstring qv = core::Trim(cmdline.substr(qp + 8)); SetWindowTextW(hSearch, qv.c_str()); curQuery = qv; Rebuild(false); }
    size_t os = cmdline.find(L"--open-settings");
    if (os != std::wstring::npos) {                                      // for screenshots and checks: --open-settings=N opens page N
        extern int g_settingsPage;
        if (cmdline.size() > os + 16 && cmdline[os + 15] == L'=') g_settingsPage = _wtoi(cmdline.substr(os + 16).c_str());
        PostMessageW(g_main, WM_COMMAND, IDM_SETTINGS, 0);
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (HandleKey(&msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_prevBmp) delete g_prevBmp;
    CloseHandle(mutex);
    return (int)msg.wParam;
}
