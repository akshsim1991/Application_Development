// Feather PDF: free, lightweight PDF viewer and editor for Windows.
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <cderr.h>
#include <shellapi.h>
#include <shlobj.h>
#include <cryptuiapi.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "document.hpp"
#include "viewer.hpp"
#include "sidebar.hpp"
#include "outline.hpp"
#include "foldersearch.hpp"
#include "batch.hpp"
#include "formcreate.hpp"
#include "visualdiff.hpp"
#include "formdata.hpp"
#include "exporter.hpp"
#include <commctrl.h>
#include <shlobj.h>
#include <sstream>
#include <map>
#include <set>
#include "sigpad.hpp"
#include "theme.hpp"
#include <dwmapi.h>
#include <winspool.h>
#include <uxtheme.h>
#include "formdialog.hpp"
#include "pageobjects.hpp"
#include "exporter.hpp"
#include "speech.hpp"
#include "redact.hpp"
#include "compress.hpp"
#include "ocr.hpp"
#include "sign.hpp"
#include "picture.hpp"

namespace {

constexpr int kTabH = 34, kToolH = 40, kFindH = 38, kSideW = 230;
enum { ID_FIND_EDIT = 201, ID_FIND_PREV, ID_FIND_NEXT, ID_FIND_CASE, ID_FIND_COUNT, ID_FIND_CLOSE, TIMER_FIND = 1, TIMER_AUTOSAVE = 2 };
constexpr const wchar_t* kAppName = L"Feather PDF";

struct Button { std::wstring label; RECT rc; int id; bool enabled; int icon = 0; };
enum Cmd { C_EDITPDF = 120, C_COMMENT, C_GEOM, C_SBS, C_ZOOMMENU, C_SAVE = 100, C_UNDO, C_REDO, C_ANNOT, C_PAGES, C_TOOLS, C_OPEN = 1, C_PREV, C_NEXT, C_ZOOMOUT, C_ZOOMIN, C_FITW, C_FITP, C_NEWTAB, C_ACTUAL, C_FIND, C_PANEL, C_GOTO, C_MENU };

struct App {
    HINSTANCE inst = nullptr;
    HWND wnd = nullptr;
    Viewer viewer;
    Sidebar sidebar;
    bool sidebarVisible = false, findVisible = false;
    HWND findEdit = nullptr, findPrev = nullptr, findNext = nullptr, findCase = nullptr, findCount = nullptr, findClose = nullptr;
    int curMatch = -1, matchTotal = 0;
    WNDPROC editProc = nullptr;
    int hlColour = 0, drawColour = 5, widthIdx = 1, readRate = 0;
    Viewer::Tool tool = Viewer::Tool::None;
    std::vector<std::wstring> recent;
    std::wstring pendingPicture, pendingStamp;
    COLORREF pendingStampColour = RGB(0xE5, 0x39, 0x35);
    bool fullscreen = false;
    WINDOWPLACEMENT savedPlace{sizeof(WINDOWPLACEMENT)};
    LONG savedStyle = 0;
    bool pictureHintShown = false;
    pageobj::TextStyle textStyle{pageobj::Family::Sans, false, false, 14, RGB(0x20, 0x20, 0x20)};
    std::vector<std::unique_ptr<Document>> tabs;   // an empty vector means the "New tab" screen
    int active = -1;
    HFONT font = nullptr, fontBold = nullptr;
    std::vector<Button> buttons;
    std::vector<RECT> tabRects, closeRects;
    RECT plusRect{}, pageRect{};
    int hover = -1;
    int theme = 2;            // 0 light, 1 dark, 2 like Windows
    int units = 0, scaleIdx = 0;   // geometry tools: mm/cm/in/pt, drawing scale
    bool sbs = false, syncing = false;
    Viewer viewer2;                                   // the second view of side by side
    Document* sbsDoc = nullptr;
    std::unique_ptr<Document> sbsOwned;
    std::vector<int> navBack, navFwd;
    bool navBusy = false;
    int layout = 1; bool cover = false;
    pageobj::TableSpec pendingTable;
    struct { int page = -1; std::vector<int> objects; } sel;   // the selected picture, paragraph or table
    annot::PointD protVertex{}, protArm{}; int protStage = 0, protPage = 0;
    double pendingPictureWidth = 0;
    struct Pos { std::wstring path; double zoom; int scrollY; bool fit; };
    std::vector<Pos> positions;                       // where each file was last read, most recent first

    HBRUSH ctlBrush = nullptr;
} app;

Document* activeDoc() { return app.active >= 0 ? app.tabs[app.active].get() : nullptr; }


// features implemented further down
void lookUpKind(int kind); void readAloud(); void syncSideBySide(bool fromFirst); void linkOnSelection(); void openUri(const std::wstring& uri);
void formExport(); void formFillFromCsv(); void startFormField(Viewer::Tool t); bool formFieldDraw(Document& d, const Viewer::Event& e); void formAutoDetect();
void searchFolder(); void batchDialog(); void writeSession(); void clearSession(); bool restoreSession();
void refreshLinks(Document& d); std::wstring signaturePathFwd();
void doFindReplace(); void addDocumentTab(std::unique_ptr<Document> doc); void tableInsert(); void tableEdit(); void tableDelete(); void addBookmarkHere(); void doGreyscale();
void startLink(); void signatureAction(int cmd); void startCopyArea(); void setPageLayout(int layout); void rotateView(int turns);
void navBack(); void navForward(); void reloadFromDisk(); void geometryAction(int cmd); void compareDocs(); void sideBySideAction(int cmd);
void commandPalette(); void newPdfFromPictures(); void splitPdf(); void cropByBox(); void lookUpSelection(bool web); void readFromThisPage();
bool featureDraw(Document& d, const Viewer::Event& e);
bool featureClick(Document& d, const Viewer::Event& e);
void pagesMenuAction(int cmd, const std::vector<int>& sel);
void toolsDispatch(int cmd);
void showMainMenu(const RECT& anchor);

std::wstring tabLabel(const Document& d) { return (d.dirty() ? L"\u2022 " : L"") + d.title(); }

void applyTheme();

void relayoutChildren() {
    RECT rc; GetClientRect(app.wnd, &rc);
    if (app.fullscreen) {   // only the pages, edge to edge
        MoveWindow(app.viewer.hwnd(), 0, 0, rc.right, rc.bottom, TRUE);
        MoveWindow(app.sidebar.hwnd(), 0, 0, 0, 0, FALSE);
        if (app.viewer2.hwnd()) ShowWindow(app.viewer2.hwnd(), SW_HIDE);
        return;
    }
    int top = kTabH + kToolH;
    if (app.findVisible) {
        int x = 8;
        MoveWindow(app.findEdit, x, top + 6, 260, 26, TRUE); x += 268;
        MoveWindow(app.findPrev, x, top + 6, 56, 26, TRUE); x += 60;
        MoveWindow(app.findNext, x, top + 6, 56, 26, TRUE); x += 64;
        MoveWindow(app.findCase, x, top + 6, 100, 26, TRUE); x += 108;
        MoveWindow(app.findCount, x, top + 11, 160, 20, TRUE); x += 164;
        MoveWindow(app.findClose, std::max(x, static_cast<int>(rc.right) - 64), top + 6, 56, 26, TRUE);
        top += kFindH;
    }
    int left = 0;
    if (app.sidebarVisible) {
        MoveWindow(app.sidebar.hwnd(), 0, top, kSideW, std::max<LONG>(0, rc.bottom - top), TRUE);
        left = kSideW;
    }
    const int total = std::max<LONG>(0, rc.right - left), height = std::max<LONG>(0, rc.bottom - top);
    if (app.sbs && app.viewer2.hwnd()) {
        const int half = total / 2;
        MoveWindow(app.viewer.hwnd(), left, top, half - 2, height, TRUE);
        MoveWindow(app.viewer2.hwnd(), left + half + 2, top, total - half - 2, height, TRUE);
        ShowWindow(app.viewer2.hwnd(), SW_SHOW);
    } else {
        MoveWindow(app.viewer.hwnd(), left, top, total, height, TRUE);
        if (app.viewer2.hwnd()) ShowWindow(app.viewer2.hwnd(), SW_HIDE);
    }
}

void showControls() {
    const int f = app.findVisible ? SW_SHOW : SW_HIDE;
    for (HWND h : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findCount, app.findClose}) ShowWindow(h, f);
    ShowWindow(app.sidebar.hwnd(), app.sidebarVisible ? SW_SHOW : SW_HIDE);
    relayoutChildren();
    InvalidateRect(app.wnd, nullptr, FALSE);
}

void updateTitle() {
    Document* d = activeDoc();
    std::wstring t = d ? tabLabel(*d) + L" - " + kAppName : kAppName;
    SetWindowTextW(app.wnd, t.c_str());
}

void refresh() {
    updateTitle();
    InvalidateRect(app.wnd, nullptr, FALSE);
}

void runSearch(bool keepPosition);

void setActiveDocument(Document* d) {
    app.viewer.setDocument(d);
    app.viewer.setMarks(d ? &d->redactions : nullptr);
    app.sidebar.setDocument(d);
    app.curMatch = -1; app.matchTotal = 0;
    SetWindowTextW(app.findCount, L"");
    if (d && app.findVisible && GetWindowTextLengthW(app.findEdit) > 0) runSearch(false);
}

void switchTab(int i) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    app.active = i;
    setActiveDocument(app.tabs[i].get());
    refresh();
}

bool confirmClose(Document& d);

void rememberPosition(const Document& d) {
    if (d.path().empty()) return;
    app.positions.erase(std::remove_if(app.positions.begin(), app.positions.end(), [&](const App::Pos& p) { return _wcsicmp(p.path.c_str(), d.path().c_str()) == 0; }), app.positions.end());
    app.positions.insert(app.positions.begin(), {d.path(), d.zoom, d.scrollY, d.fitWidth});
    if (app.positions.size() > 100) app.positions.resize(100);
}

void closeTab(int i) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    if (!confirmClose(*app.tabs[i])) return;
    rememberPosition(*app.tabs[i]);
    if (i == app.active) setActiveDocument(nullptr);   // viewer and sidebar must let go before the document dies
    app.tabs.erase(app.tabs.begin() + i);
    if (app.tabs.empty()) { app.active = -1; setActiveDocument(nullptr); }
    else {
        app.active = std::min(i, static_cast<int>(app.tabs.size()) - 1);
        setActiveDocument(app.tabs[app.active].get());
    }
    refresh();
}

// ---- password prompt: a small modal dialog built in memory (no resource file needed) ----
struct PwState { std::wstring prompt, text; bool ok = false; };

// Annotate menu command ids (kept apart from toolbar command ids).
enum MenuId { M_HIGHLIGHT = 300, M_UNDERLINE, M_STRIKE, M_SQUIGGLY, M_COMMENT, M_RECT, M_ELLIPSE, M_LINE, M_ARROW, M_PEN,
              M_DELETE, M_EDIT_COMMENT, M_COPY,
              T_WATERMARK = 500, T_NUMBERS, T_EXPORT, T_READ, T_RATE = 510, T_COLOURS = 520, T_PRINT = 530, T_PROPS, T_RED_AREA = 540, T_RED_SEL, T_RED_FIND, T_RED_CLEAR, T_RED_APPLY, M_RED_REMOVE, T_COMPRESS, T_EDIT_TEXT, T_ADD_TEXT, T_TEXT_STYLE, T_PROTECT, T_UNPROTECT, T_OCR, T_SIGN, T_OWNCERT, T_SHOWSIGS, T_NOTES, T_FULLSCREEN, T_PRESENT, T_STAMP = 700, T_STAMP_CUSTOM = 720, T_PICTURE, T_DEFAULT_VIEWER, T_RECENT = 600, T_RECENT_CLEAR = 620, M_COPY_PAGE,
              P_DELETE = 400, P_BLANK, P_ROT_R, P_ROT_L, P_ROT_ALL, P_MOVE, P_EXTRACT, P_INSERT, P_MERGE, P_CROP, P_SPLIT, P_CROPBOX, P_NEWPDF, P_NUP, M_HL_COLOUR = 320, M_DRAW_COLOUR = 340, M_WIDTH = 360 };
// extra menu ids
enum ExtraId { M_SELALL = 990, M_ADDCOMMENT_HERE, M_EDITTEXT_HERE, N_SELECTOBJ = 1012, N_EDITPARA = 1010, M_EDITPARA_HERE, N_RED_CHECK = 999, N_BATCH = 998, F_EXPORT = 1000, F_FILL, F_TEXT, F_CHECK, F_COMBO, F_AUTO, N_FINDREPLACE = 800, N_TABLE_INS, N_TABLE_EDIT, N_TABLE_DEL, N_BOOKMARK, N_BW, N_LINK, N_SIG_TYPE, N_SIG_DRAW, N_SIG_PIC, N_SIG_CLEAR,
               N_TEXTSIZE = 820, N_LAYOUT = 840, N_ROTL = 850, N_ROTR, N_BACK, N_FWD, N_RELOAD, N_COPYAREA, N_PALETTE, N_COMPARE, N_SBS_OPEN, N_SBS_TAB, N_SBS_OFF,
               G_RULER = 870, G_PROT, G_RULERS, G_KEEP, G_CLEAR, G_UNITS = 880, G_SCALE = 890, Z_BASE = 900, N_OPENFILEBOX = 950, N_SEARCH_WEB, N_LOOKUP = 960, N_READ_FROM = 970, N_SELECT_NOTES };
constexpr double kZoomPresets[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0};
constexpr double kTextSizes[] = {8, 10, 12, 14, 18, 24, 36, 48, 72};
constexpr COLORREF kPalette[] = {RGB(0xFF, 0xE0, 0x00), RGB(0x66, 0xDD, 0x66), RGB(0x55, 0xB8, 0xFF), RGB(0xFF, 0x8A, 0xC8),
                                 RGB(0xFF, 0x8C, 0x00), RGB(0xE5, 0x39, 0x35), RGB(0x20, 0x20, 0x20)};
constexpr const wchar_t* kPaletteNames[] = {L"Yellow", L"Green", L"Blue", L"Pink", L"Orange", L"Red", L"Black"};
constexpr double kWidths[] = {1, 2, 4};
constexpr const wchar_t* kWidthNames[] = {L"Thin (1 pt)", L"Medium (2 pt)", L"Thick (4 pt)"};

INT_PTR CALLBACK pwProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<PwState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(h, GWLP_USERDATA, l);
            st = reinterpret_cast<PwState*>(l);
            SetDlgItemTextW(h, 100, st->prompt.c_str());
            SetFocus(GetDlgItem(h, 101));
            SendDlgItemMessageW(h, 101, EM_SETSEL, 0, -1);   // typing replaces the suggested text
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(w) == IDOK) {
                int len = GetWindowTextLengthW(GetDlgItem(h, 101));
                std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
                GetDlgItemTextW(h, 101, buf.data(), len + 1);
                buf.resize(static_cast<size_t>(len));
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

bool askText(const wchar_t* title, const std::wstring& prompt, bool password, std::wstring& out, const wchar_t* initial = L"", bool multiline = false) {
    std::vector<BYTE> b;
    put<DWORD>(b, DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    put<DWORD>(b, 0); put<WORD>(b, 4);
    const short boxH = multiline ? 70 : 13;
    put<short>(b, 0); put<short>(b, 0); put<short>(b, 200); put<short>(b, static_cast<short>(65 + boxH));
    put<WORD>(b, 0); put<WORD>(b, 0);
    putStr(b, title);
    put<WORD>(b, 9); putStr(b, L"Segoe UI");
    item(b, 0x0082, SS_LEFT, 8, 8, 184, 22, 100, L"");
    item(b, 0x0081, WS_BORDER | WS_TABSTOP | (password ? ES_PASSWORD : 0) |
                    (multiline ? (ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL) : ES_AUTOHSCROLL),
         8, 34, 184, boxH, 101, initial);
    item(b, 0x0080, (multiline ? BS_PUSHBUTTON : BS_DEFPUSHBUTTON) | WS_TABSTOP, 88, static_cast<short>(34 + boxH + 9), 50, 14, IDOK, L"OK");
    item(b, 0x0080, BS_PUSHBUTTON | WS_TABSTOP, 142, static_cast<short>(34 + boxH + 9), 50, 14, IDCANCEL, L"Cancel");
    b[8] = 4;   // cdit = 4 items (offset of the item count in DLGTEMPLATE)
    PwState st{prompt, {}, false};
    DialogBoxIndirectParamW(app.inst, reinterpret_cast<LPCDLGTEMPLATEW>(b.data()), app.wnd, pwProc,
                            reinterpret_cast<LPARAM>(&st));
    if (!st.ok) return false;
    out = st.text;
    return true;
}

bool askPassword(const std::wstring& prompt, std::string& out) {
    std::wstring w;
    if (!askText(L"Password required", prompt, true, w)) return false;
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    out.assign(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
    return true;
}


// ---- settings: recent files and preferences, kept in %APPDATA%\FeatherPDF\settings.ini ----

std::wstring settingsPath() {
    wchar_t base[MAX_PATH];
    if (GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH) == 0) return L"";
    std::wstring dir = std::wstring(base) + L"\\FeatherPDF";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\settings.ini";
}

void updateEmptyText() {
    std::wstring t = L"Open a PDF with Ctrl+O, or drop a file here.";
    if (!app.recent.empty()) t += L"\n\nRight-click here to open one of your recent files.";
    app.viewer.setEmptyText(t);
}

void loadSettings() {
    const std::wstring p = settingsPath();
    if (p.empty()) return;
    auto num = [&](const wchar_t* key, int def) { return static_cast<int>(GetPrivateProfileIntW(L"view", key, def, p.c_str())); };
    app.hlColour = std::clamp(num(L"highlightColour", app.hlColour), 0, 6);
    app.drawColour = std::clamp(num(L"drawingColour", app.drawColour), 0, 6);
    app.widthIdx = std::clamp(num(L"lineWidth", app.widthIdx), 0, 2);
    app.readRate = std::clamp(num(L"readingSpeed", app.readRate), -10, 10);
    app.viewer.setColours(static_cast<Viewer::Colours>(std::clamp(num(L"pageColours", 0), 0, 2)));
    app.theme = std::clamp(num(L"theme", 2), 0, 2);
    app.layout = std::clamp(num(L"layout", 1), 0, 2);
    app.cover = num(L"cover", 0) != 0;
    app.units = std::clamp(num(L"units", 0), 0, 3);
    app.scaleIdx = std::clamp(num(L"scale", 0), 0, 9);
    app.textStyle.family = static_cast<pageobj::Family>(std::clamp(num(L"textFont", 0), 0, 2));
    app.textStyle.bold = num(L"textBold", 0) != 0;
    app.textStyle.italic = num(L"textItalic", 0) != 0;
    app.textStyle.sizePt = std::clamp(num(L"textSize", 14), 6, 144);
    app.positions.clear();
    for (int i = 0; i < 100; ++i) {
        wchar_t key[16], buf[MAX_PATH * 2 + 64];
        swprintf(key, 16, L"p%d", i);
        GetPrivateProfileStringW(L"positions", key, L"", buf, MAX_PATH * 2 + 64, p.c_str());
        if (!buf[0]) break;
        std::wstring v = buf;
        size_t a = v.rfind(L'|'), b = a == std::wstring::npos ? a : v.rfind(L'|', a - 1), c = b == std::wstring::npos ? b : v.rfind(L'|', b - 1);
        if (c == std::wstring::npos) continue;
        app.positions.push_back({v.substr(0, c), wcstod(v.substr(c + 1, b - c - 1).c_str(), nullptr), static_cast<int>(wcstol(v.substr(b + 1, a - b - 1).c_str(), nullptr, 10)), v.substr(a + 1) == L"1"});
    }
    app.recent.clear();
    for (int i = 0; i < 10; ++i) {
        wchar_t key[16], buf[MAX_PATH * 2];
        swprintf(key, 16, L"file%d", i);
        GetPrivateProfileStringW(L"recent", key, L"", buf, MAX_PATH * 2, p.c_str());
        if (buf[0]) app.recent.push_back(buf);
    }
}

void saveSettings() {
    const std::wstring p = settingsPath();
    if (p.empty()) return;
    auto put = [&](const wchar_t* sec, const wchar_t* key, int v) { WritePrivateProfileStringW(sec, key, std::to_wstring(v).c_str(), p.c_str()); };
    put(L"view", L"highlightColour", app.hlColour);
    put(L"view", L"drawingColour", app.drawColour);
    put(L"view", L"lineWidth", app.widthIdx);
    put(L"view", L"readingSpeed", app.readRate);
    put(L"view", L"pageColours", static_cast<int>(app.viewer.colours()));
    put(L"view", L"theme", app.theme);
    put(L"view", L"layout", app.layout);
    put(L"view", L"cover", app.cover);
    put(L"view", L"units", app.units);
    put(L"view", L"scale", app.scaleIdx);
    put(L"view", L"textFont", static_cast<int>(app.textStyle.family));
    put(L"view", L"textBold", app.textStyle.bold);
    put(L"view", L"textItalic", app.textStyle.italic);
    put(L"view", L"textSize", static_cast<int>(app.textStyle.sizePt));
    WritePrivateProfileStringW(L"positions", nullptr, nullptr, p.c_str());
    for (size_t i = 0; i < app.positions.size() && i < 100; ++i) {
        wchar_t key[16], val[MAX_PATH * 2 + 64];
        swprintf(key, 16, L"p%d", static_cast<int>(i));
        swprintf(val, MAX_PATH * 2 + 64, L"%ls|%.4f|%d|%d", app.positions[i].path.c_str(), app.positions[i].zoom, app.positions[i].scrollY, app.positions[i].fit ? 1 : 0);
        WritePrivateProfileStringW(L"positions", key, val, p.c_str());
    }
    WritePrivateProfileStringW(L"recent", nullptr, nullptr, p.c_str());   // rewrite the whole section
    for (size_t i = 0; i < app.recent.size() && i < 10; ++i) {
        wchar_t key[16];
        swprintf(key, 16, L"file%d", static_cast<int>(i));
        WritePrivateProfileStringW(L"recent", key, app.recent[i].c_str(), p.c_str());
    }
}

void addRecent(const std::wstring& path) {
    app.recent.erase(std::remove_if(app.recent.begin(), app.recent.end(), [&](const std::wstring& r) { return _wcsicmp(r.c_str(), path.c_str()) == 0; }), app.recent.end());
    app.recent.insert(app.recent.begin(), path);
    if (app.recent.size() > 10) app.recent.resize(10);
    updateEmptyText();
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
    {   // form fields repaint the page and mark the document as modified
        Document* raw = doc.get();
        doc->onFormInvalidate = [raw](int page) { if (activeDoc() == raw) app.viewer.invalidatePage(page); };
        doc->onFormChanged = [raw]() { if (activeDoc() == raw) { refresh(); app.sidebar.refresh(); } else InvalidateRect(app.wnd, nullptr, FALSE); };
    }
    addRecent(path);
    const bool badSig = std::any_of(doc->signatures().begin(), doc->signatures().end(),
                                    [](const sign::Status& s) { return s.kind == sign::Status::Changed || s.kind == sign::Status::Unreadable; });
    for (const auto& ps : app.positions)
        if (_wcsicmp(ps.path.c_str(), path.c_str()) == 0) { doc->zoom = ps.zoom; doc->scrollY = ps.scrollY; doc->fitWidth = ps.fit; break; }
    app.tabs.push_back(std::move(doc));
    switchTab(static_cast<int>(app.tabs.size()) - 1);
    if (badSig) MessageBoxW(app.wnd, L"A digital signature in this document does not match: the document was changed after it was signed (or the signature is damaged). "
                                     L"Do not trust its content. Tools > Digital signatures shows the details.", kAppName, MB_OK | MB_ICONWARNING);
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


// ---- saving, closing, undo ----

void afterChange(int page) {   // page < 0: everything may have changed
    if (page >= 0) app.viewer.invalidatePage(page); else app.viewer.documentChanged();
    app.sidebar.refresh();
    refresh();
}

void afterDocReplaced() {
    if (Document* cd = activeDoc()) cd->redactions.clear();
    app.viewer.documentChanged();
    app.sidebar.refresh();
    app.viewer.setMatches({}, -1);
    app.curMatch = -1; app.matchTotal = 0;
    if (app.findVisible && GetWindowTextLengthW(app.findEdit) > 0) runSearch(false);
    refresh();
}

bool saveAsDialog(Document& d);

bool saveDocument(Document& d, const std::wstring& path) {
    if (path.empty()) return saveAsDialog(d);   // a new document has no file yet
    SaveResult r = d.saveAs(path);
    std::wstring name = path.substr(path.find_last_of(L"\\/") + 1);
    const wchar_t* msg = nullptr;
    switch (r) {
        case SaveResult::Ok: refresh(); return true;
        case SaveResult::CannotWrite:
            msg = L" could not be saved. The folder may be read-only, you may not have permission, or the file may be open in another program.";
            break;
        case SaveResult::DiskFull: msg = L" could not be saved because the disk is full. Free some space and try again."; break;
        default: msg = L" could not be saved: the document could not be prepared for writing."; break;
    }
    MessageBoxW(app.wnd, (L"\"" + name + L"\"" + msg).c_str(), kAppName, MB_OK | MB_ICONWARNING);
    return false;
}

bool saveAsDialog(Document& d) {
    wchar_t buf[MAX_PATH * 2];
    wcsncpy_s(buf, d.path().c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrDefExt = L"pdf";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return false;
    for (const auto& t : app.tabs)
        if (t.get() != &d && _wcsicmp(t->path().c_str(), buf) == 0) {
            MessageBoxW(app.wnd, L"That file is open in another tab. Close it first, or choose another name.", kAppName, MB_OK | MB_ICONWARNING);
            return false;
        }
    return saveDocument(d, buf);
}

bool saveActive() {
    Document* d = activeDoc();
    if (!d) return false;
    if (!d->dirty()) return true;
    return saveDocument(*d, d->path());
}

// Asks about unsaved changes. Returns false when the user cancels.
bool confirmClose(Document& d) {
    if (!d.dirty()) return true;
    std::wstring q = L"Do you want to save the changes to \"" + d.title() + L"\" before closing?\n\nChoose No to throw the changes away.";
    switch (MessageBoxW(app.wnd, q.c_str(), kAppName, MB_YESNOCANCEL | MB_ICONQUESTION)) {
        case IDYES: return saveDocument(d, d.path());
        case IDNO: return true;
        default: return false;
    }
}

void undoRedo(bool redo) {
    Document* d = activeDoc();
    if (!d) return;
    if (redo ? !d->redo() : !d->undo()) return;
    afterDocReplaced();
}

// ---- annotations ----

COLORREF hlColor() { return kPalette[app.hlColour]; }
COLORREF drawColor() { return kPalette[app.drawColour]; }
double lineWidth() { return kWidths[app.widthIdx]; }

void applyStyle() { app.viewer.setStyle(drawColor(), lineWidth()); }

void setTool(Viewer::Tool t) {
    app.tool = (app.tool == t) ? Viewer::Tool::None : t;   // choosing the active tool again switches it off
    app.viewer.setTool(app.tool);
    if (app.tool != Viewer::Tool::Select) { app.sel.page = -1; app.sel.objects.clear(); app.viewer.setObjectSelection(-1, nullptr); }
    applyStyle();
}

std::wstring toEdit(const std::wstring& s) {   // PDF comments use bare CR or LF; edit boxes want CRLF
    std::wstring o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r' && i + 1 < s.size() && s[i + 1] == L'\n') { o += L"\r\n"; ++i; }
        else if (s[i] == L'\r' || s[i] == L'\n') o += L"\r\n";
        else o += s[i];
    }
    return o;
}

std::wstring fromEdit(const std::wstring& s) {
    std::wstring o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r' && i + 1 < s.size() && s[i + 1] == L'\n') ++i;
        o += (s[i] == L'\n') ? L'\r' : s[i];
    }
    return o;
}

void markupSelection(annot::Markup kind, bool withComment) {
    Document* d = activeDoc();
    if (!d) return;
    auto sel = app.viewer.selectionRects();
    if (sel.empty()) {
        MessageBoxW(app.wnd, L"Select some text first (drag over it), then choose this again.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring comment;
    if (withComment) {
        if (!askText(L"Comment", L"Type your comment on the selected text.", false, comment, L"", true)) return;
        comment = fromEdit(comment);
        if (comment.empty()) return;
    }
    const COLORREF col = kind == annot::Markup::Highlight ? hlColor() : drawColor();
    for (const auto& pr : sel) {
        if (annot::addMarkup(*d, pr.page, kind, pr.rects, col, comment)) afterChange(pr.page);
    }
    app.viewer.clearSelection();
    refresh();
}

void addArrowOrLine(Document& d, const Viewer::Event& e, bool arrow) {
    std::vector<std::vector<annot::PointD>> strokes;
    const annot::PointD a = e.pts.front(), b = e.pts.back();
    strokes.push_back({a, b});
    if (arrow) {
        const double dx = b.x - a.x, dy = b.y - a.y, len = std::hypot(dx, dy);
        if (len > 1) {
            const double head = std::max(8.0, lineWidth() * 5), ang = 0.45;   // ~26 degrees
            const double base = std::atan2(dy, dx) + 3.14159265358979;
            for (double s : {-1.0, 1.0})
                strokes.push_back({b, {b.x + head * std::cos(base + s * ang), b.y + head * std::sin(base + s * ang)}});
        }
    }
    if (annot::addInk(d, e.page, strokes, drawColor(), lineWidth())) afterChange(e.page);
}

void editComment(Document& d, int page, int index) {
    std::wstring cur;
    for (const auto& a : annot::list(d, page)) if (a.index == index) cur = a.contents;
    std::wstring text;
    if (!askText(L"Comment", L"Edit the comment.", false, text, toEdit(cur).c_str(), true)) return;
    text = fromEdit(text);
    if (text == cur) return;
    if (annot::setContents(d, page, index, text)) afterChange(page);
}

void deleteAnnotation(Document& d, int page, int index) {
    if (annot::remove(d, page, index)) { app.viewer.clearAnnotSelection(); afterChange(page); }
}

void openRecentMenu(POINT screenPt);

void onViewerEvent(const Viewer::Event& e) {
    Document* d = activeDoc();
    if (!d) {
        if (e.kind == Viewer::Event::ContextMenu) openRecentMenu(e.screen);
        return;
    }
    switch (e.kind) {
        case Viewer::Event::Draw:
            if (featureDraw(*d, e)) break;
            switch (e.tool) {
                case Viewer::Tool::Rectangle: case Viewer::Tool::Ellipse: {
                    annot::RectD r{std::min(e.pts[0].x, e.pts[1].x), std::min(e.pts[0].y, e.pts[1].y),
                                   std::max(e.pts[0].x, e.pts[1].x), std::max(e.pts[0].y, e.pts[1].y)};
                    if (annot::addShape(*d, e.page, e.tool == Viewer::Tool::Rectangle ? annot::Shape::Rectangle : annot::Shape::Ellipse,
                                        r, drawColor(), lineWidth())) afterChange(e.page);
                    break;
                }
                case Viewer::Tool::Redact: {
                    d->redactions.push_back({e.page, std::min(e.pts[0].x, e.pts[1].x), std::min(e.pts[0].y, e.pts[1].y),
                                             std::max(e.pts[0].x, e.pts[1].x), std::max(e.pts[0].y, e.pts[1].y)});
                    app.viewer.setMarks(&d->redactions);
                    break;
                }
                case Viewer::Tool::Line: addArrowOrLine(*d, e, false); break;
                case Viewer::Tool::Arrow: addArrowOrLine(*d, e, true); break;
                case Viewer::Tool::Pen:
                    if (annot::addInk(*d, e.page, {e.pts}, drawColor(), lineWidth())) afterChange(e.page);
                    break;
                default: break;
            }
            break;
        case Viewer::Event::Click: {
            if (featureClick(*d, e)) break;
            if (e.tool == Viewer::Tool::AddText) {
                std::wstring text;
                if (askText(L"Add text", L"Type the text to put on the page. Press Enter for a new line.", false, text, L"", true)) {
                    text = fromEdit(text);
                    if (!text.empty()) {
                        if (!pageobj::isLatin(text)) MessageBoxW(app.wnd, L"New text can use letters, numbers and common symbols of Western languages. Please remove other characters.", kAppName, MB_OK | MB_ICONINFORMATION);
                        else if (pageobj::addTextBox(*d, e.page, e.pts.front().x, e.pts.front().y, text, app.textStyle)) { d->resetText(e.page); afterChange(e.page); }
                        else MessageBoxW(app.wnd, L"The text could not be added.", kAppName, MB_OK | MB_ICONWARNING);
                    }
                }
                setTool(Viewer::Tool::AddText);   // one text box at a time
                break;
            }
            if (e.tool == Viewer::Tool::Stamp) {
                if (pageobj::addStamp(*d, e.page, e.pts.front().x, e.pts.front().y, app.pendingStamp, app.pendingStampColour)) afterChange(e.page);
                else MessageBoxW(app.wnd, L"The stamp could not be added.", kAppName, MB_OK | MB_ICONWARNING);
                setTool(Viewer::Tool::Stamp);
                break;
            }
            if (e.tool == Viewer::Tool::Picture) {
                std::wstring err;
                if (pageobj::addPicture(*d, e.page, e.pts.front().x, e.pts.front().y, app.pendingPicture, err, app.pendingPictureWidth)) afterChange(e.page);
                else MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONWARNING);
                setTool(Viewer::Tool::Picture);   // one picture at a time
                break;
            }
            if (e.tool == Viewer::Tool::EditText) {
                pageobj::TextHit hit = pageobj::textAt(*d, e.page, e.pts.front().x, e.pts.front().y);
                if (hit.index < 0) {
                    MessageBoxW(app.wnd, L"No editable text was found there. Click directly on a line of text. Scanned pages are pictures and have no editable text.", kAppName, MB_OK | MB_ICONINFORMATION);
                    break;
                }
                std::wstring text;
                if (askText(L"Edit text", L"Change this text. Leave it empty to delete it.", false, text, toEdit(hit.text).c_str(), true)) {
                    text = fromEdit(text);
                    if (text != hit.text) {
                        // an edited line stays one object: line breaks are not kept
                        std::wstring flat;
                        for (wchar_t ch : text) flat += (ch == L'\r' || ch == L'\n') ? L' ' : ch;
                        if (pageobj::replaceText(*d, e.page, hit.index, flat)) afterChange(e.page);
                        else MessageBoxW(app.wnd, L"The text could not be changed. It may use a font that cannot show those characters. Try letters and numbers of Western languages.", kAppName, MB_OK | MB_ICONWARNING);
                    }
                }
                break;
            }
            std::wstring text;
            if (askText(L"Comment", L"Type your comment.", false, text, L"", true)) {
                text = fromEdit(text);
                if (!text.empty() && annot::addNote(*d, e.page, e.pts.front(), text, hlColor())) afterChange(e.page);
            }
            setTool(Viewer::Tool::Comment);   // one note at a time: switch the tool off again
            break;
        }
        case Viewer::Event::DoubleClick:
            for (const auto& a : annot::list(*d, e.page))
                if (a.index == e.annot && annot::hasComment(a.subtype)) { editComment(*d, e.page, e.annot); break; }
            break;
        case Viewer::Event::ContextMenu: {
            HMENU m = CreatePopupMenu();
            int subtype = 0;
            if (e.annot >= 0)
                for (const auto& a : annot::list(*d, e.page)) if (a.index == e.annot) subtype = a.subtype;
            if (subtype) {
                if (annot::hasComment(subtype)) AppendMenuW(m, MF_STRING, M_EDIT_COMMENT, subtype == FPDF_ANNOT_TEXT ? L"Edit comment..." : L"Add or edit comment...");
                AppendMenuW(m, MF_STRING, M_DELETE, (std::wstring(L"Delete this ") + annot::typeName(subtype)).c_str());
                AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
            }
            int markIdx = -1;
            if (e.page >= 0 && !e.pts.empty())
                for (size_t i = 0; i < d->redactions.size(); ++i) {
                    const RedactMark& mk = d->redactions[i];
                    if (mk.page == e.page && e.pts[0].x >= mk.left && e.pts[0].x <= mk.right && e.pts[0].y >= mk.bottom && e.pts[0].y <= mk.top) markIdx = static_cast<int>(i);
                }
            if (markIdx >= 0) { AppendMenuW(m, MF_STRING, M_RED_REMOVE, L"Remove this redaction mark"); AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
            const bool hasSel = app.viewer.hasSelection();
            auto add = [&](UINT id, const wchar_t* t, bool on = true) { AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED), id, t); };
            auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
            add(M_COPY, L"Copy\tCtrl+C", hasSel);
            add(M_SELALL, L"Select all\tCtrl+A");
            sep();
            add(N_SEARCH_WEB, L"Search online (Google)", hasSel);
            HMENU lk = CreatePopupMenu();
            AppendMenuW(lk, hasSel ? MF_STRING : MF_GRAYED, N_LOOKUP + 1, L"Dictionary");
            AppendMenuW(lk, hasSel ? MF_STRING : MF_GRAYED, N_LOOKUP + 2, L"Wikipedia");
            AppendMenuW(lk, hasSel ? MF_STRING : MF_GRAYED, N_LOOKUP + 3, L"Translate");
            AppendMenuW(m, MF_POPUP | (hasSel ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(lk), L"Look up");
            sep();
            add(M_HIGHLIGHT, L"Highlight\tCtrl+H", hasSel);
            add(M_UNDERLINE, L"Underline\tCtrl+U", hasSel);
            add(M_STRIKE, L"Strikethrough\tCtrl+Shift+K", hasSel);
            add(N_LINK, L"Add a link...", hasSel);
            sep();
            if (e.page >= 0) add(M_ADDCOMMENT_HERE, L"Add comment here...");
            add(M_COMMENT, L"Comment on selected text...\tCtrl+Shift+M", hasSel);
            if (e.page >= 0) add(M_EDITTEXT_HERE, L"Edit text (line)\tCtrl+E");
            if (e.page >= 0) add(M_EDITPARA_HERE, L"Edit paragraph\tF2");
            if (e.page >= 0) { sep(); add(M_COPY_PAGE, L"Copy page as image"); add(N_COPYAREA, L"Copy area as image"); }
            sep();
            add(N_READ_FROM, hasSel ? L"Read the selected text aloud\tCtrl+Shift+U" : L"Read aloud from this page\tCtrl+Shift+U");
            int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, e.screen.x, e.screen.y, 0, app.wnd, nullptr);
            DestroyMenu(m);
            switch (cmd) {
                case M_SELALL: app.viewer.selectAll(); break;
                case N_SEARCH_WEB: lookUpKind(0); break;
                case N_LOOKUP + 1: case N_LOOKUP + 2: case N_LOOKUP + 3: lookUpKind(cmd - N_LOOKUP); break;
                case N_LINK: linkOnSelection(); break;
                case N_COPYAREA: startCopyArea(); break;
                case N_READ_FROM: readAloud(); break;
                case M_ADDCOMMENT_HERE: {
                    std::wstring text;
                    if (e.page >= 0 && !e.pts.empty() && askText(L"Comment", L"Type your comment.", false, text, L"", true)) {
                        text = fromEdit(text);
                        if (!text.empty() && annot::addNote(*d, e.page, e.pts.front(), text, hlColor())) afterChange(e.page);
                    }
                    break;
                }
                case M_EDITPARA_HERE: {
                    Viewer::Event ee = e; ee.kind = Viewer::Event::Click; ee.tool = Viewer::Tool::EditPara;
                    featureClick(*d, ee);
                    break;
                }
                case M_EDITTEXT_HERE: {
                    Viewer::Event ee = e; ee.kind = Viewer::Event::Click; ee.tool = Viewer::Tool::EditText;
                    onViewerEvent(ee);
                    break;
                }
            }
            if (cmd >= M_EDIT_COMMENT || cmd == 0) {}
            switch (cmd) {
                case M_EDIT_COMMENT: editComment(*d, e.page, e.annot); break;
                case M_DELETE: deleteAnnotation(*d, e.page, e.annot); break;
                case M_COPY: app.viewer.copySelection(); break;
                case M_RED_REMOVE: d->redactions.erase(d->redactions.begin() + markIdx); app.viewer.setMarks(&d->redactions); break;
                case M_HIGHLIGHT: markupSelection(annot::Markup::Highlight, false); break;
                case M_UNDERLINE: markupSelection(annot::Markup::Underline, false); break;
                case M_STRIKE: markupSelection(annot::Markup::Strikethrough, false); break;
                case M_COMMENT: markupSelection(annot::Markup::Highlight, true); break;
                case M_COPY_PAGE:
                    if (!pageobj::copyPageToClipboard(*d, e.page, app.wnd)) MessageBoxW(app.wnd, L"The page could not be copied as a picture (it may be too large).", kAppName, MB_OK | MB_ICONWARNING);
                    break;
            }
            break;
        }
    }
}

void deleteSelectedAnnotation() {
    Document* d = activeDoc();
    int page, index;
    if (d && app.viewer.selectedAnnot(page, index)) deleteAnnotation(*d, page, index);
}

void showStampMenu(HMENU parent);

void showAnnotateMenu(const RECT& anchor) {
    HMENU m = CreatePopupMenu();
    const bool sel = app.viewer.hasSelection();
    auto item = [&](UINT id, const wchar_t* text, bool checked = false, bool on = true) {
        AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0) | (on ? 0 : MF_GRAYED), id, text);
    };
    item(M_HIGHLIGHT, L"Highlight\tCtrl+H", false, sel);
    item(M_UNDERLINE, L"Underline\tCtrl+U", false, sel);
    item(M_STRIKE, L"Strikethrough\tCtrl+Shift+K", false, sel);
    item(M_SQUIGGLY, L"Squiggly underline", false, sel);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    item(T_ADD_TEXT, L"Add text", app.tool == Viewer::Tool::AddText);
    item(M_RECT, L"Rectangle", app.tool == Viewer::Tool::Rectangle);
    item(M_ELLIPSE, L"Ellipse", app.tool == Viewer::Tool::Ellipse);
    item(M_LINE, L"Line", app.tool == Viewer::Tool::Line);
    item(M_ARROW, L"Arrow", app.tool == Viewer::Tool::Arrow);
    item(M_PEN, L"Pen (freehand)", app.tool == Viewer::Tool::Pen);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    showStampMenu(m);
    HMENU sg = CreatePopupMenu();
    AppendMenuW(sg, MF_STRING, N_SIG_TYPE, L"Type my name...");
    AppendMenuW(sg, MF_STRING, N_SIG_DRAW, L"Draw my signature...");
    AppendMenuW(sg, MF_STRING, N_SIG_PIC, L"Use a picture of my signature...");
    {
        std::wstring saved = signaturePathFwd();
        AppendMenuW(sg, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(sg, MF_STRING | (GetFileAttributesW(saved.c_str()) != INVALID_FILE_ATTRIBUTES ? 0 : MF_GRAYED), N_SIG_CLEAR, L"Use my saved signature");
    }
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(sg), L"Signature");
    item(T_PICTURE, L"Insert picture...");
    item(N_LINK, L"Link (drag a box)", app.tool == Viewer::Tool::Link);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU hl = CreatePopupMenu(), dc = CreatePopupMenu(), wd = CreatePopupMenu(), ts = CreatePopupMenu();
    for (int i = 0; i < 7; ++i) {
        AppendMenuW(hl, MF_STRING | (app.hlColour == i ? MF_CHECKED : 0), M_HL_COLOUR + i, kPaletteNames[i]);
        AppendMenuW(dc, MF_STRING | (app.drawColour == i ? MF_CHECKED : 0), M_DRAW_COLOUR + i, kPaletteNames[i]);
    }
    for (int i = 0; i < 3; ++i) AppendMenuW(wd, MF_STRING | (app.widthIdx == i ? MF_CHECKED : 0), M_WIDTH + i, kWidthNames[i]);
    for (int i = 0; i < 9; ++i) { wchar_t t[16]; swprintf(t, 16, L"%d pt", static_cast<int>(kTextSizes[i])); AppendMenuW(ts, MF_STRING | (app.textStyle.sizePt == kTextSizes[i] ? MF_CHECKED : 0), N_TEXTSIZE + i, t); }
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(dc), L"Colour");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(wd), L"Line width");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(ts), L"Text size");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(hl), L"Highlight colour");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    int a, b;
    item(M_DELETE, L"Delete selected annotation\tDel", false, app.viewer.selectedAnnot(a, b));
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);   // also destroys the submenus
    if (cmd >= M_HL_COLOUR && cmd < M_HL_COLOUR + 7) { app.hlColour = cmd - M_HL_COLOUR; return; }
    if (cmd >= M_DRAW_COLOUR && cmd < M_DRAW_COLOUR + 7) { app.drawColour = cmd - M_DRAW_COLOUR; applyStyle(); return; }
    if (cmd >= M_WIDTH && cmd < M_WIDTH + 3) { app.widthIdx = cmd - M_WIDTH; applyStyle(); return; }
    if (cmd >= N_TEXTSIZE && cmd < N_TEXTSIZE + 9) { app.textStyle.sizePt = kTextSizes[cmd - N_TEXTSIZE]; saveSettings(); return; }
    if (cmd >= T_STAMP && cmd <= T_STAMP_CUSTOM) { toolsDispatch(cmd); return; }
    switch (cmd) {
        case M_HIGHLIGHT: markupSelection(annot::Markup::Highlight, false); break;
        case M_UNDERLINE: markupSelection(annot::Markup::Underline, false); break;
        case M_STRIKE: markupSelection(annot::Markup::Strikethrough, false); break;
        case M_SQUIGGLY: markupSelection(annot::Markup::Squiggly, false); break;
        case T_ADD_TEXT: setTool(Viewer::Tool::AddText); break;
        case M_RECT: setTool(Viewer::Tool::Rectangle); break;
        case M_ELLIPSE: setTool(Viewer::Tool::Ellipse); break;
        case M_LINE: setTool(Viewer::Tool::Line); break;
        case M_ARROW: setTool(Viewer::Tool::Arrow); break;
        case M_PEN: setTool(Viewer::Tool::Pen); break;
        case T_PICTURE: toolsDispatch(cmd); break;
        case N_LINK: startLink(); break;
        case N_SIG_TYPE: case N_SIG_DRAW: case N_SIG_PIC: case N_SIG_CLEAR: signatureAction(cmd); break;
        case M_DELETE: deleteSelectedAnnotation(); break;
    }
}

void showCommentMenu(const RECT& anchor) {
    HMENU m = CreatePopupMenu();
    int a, b;
    AppendMenuW(m, MF_STRING | (app.tool == Viewer::Tool::Comment ? MF_CHECKED : 0), M_COMMENT, L"Add comment\tCtrl+M");
    AppendMenuW(m, MF_STRING | (app.viewer.hasSelection() ? 0 : MF_GRAYED), M_SQUIGGLY + 100, L"Comment on selected text\tCtrl+Shift+M");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, N_SELECT_NOTES, L"Notes summary (highlights and comments)...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (app.viewer.selectedAnnot(a, b) ? 0 : MF_GRAYED), M_DELETE, L"Delete selected comment\tDel");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
        case M_COMMENT: setTool(Viewer::Tool::Comment); break;
        case M_SQUIGGLY + 100: markupSelection(annot::Markup::Highlight, true); break;
        case N_SELECT_NOTES: toolsDispatch(T_NOTES); break;
        case M_DELETE: deleteSelectedAnnotation(); break;
    }
}

void showGeometryMenu(const RECT& anchor) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (app.tool == Viewer::Tool::Ruler ? MF_CHECKED : 0), G_RULER, L"Ruler (measure a distance)");
    AppendMenuW(m, MF_STRING | (app.tool == Viewer::Tool::Protractor ? MF_CHECKED : 0), G_PROT, L"Protractor (measure an angle)");
    AppendMenuW(m, MF_STRING | (app.viewer.showRulers() ? MF_CHECKED : 0), G_RULERS, L"Show rulers on the page");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU un = CreatePopupMenu(), sc = CreatePopupMenu();
    static const wchar_t* unitNames[] = {L"Millimetres (mm)", L"Centimetres (cm)", L"Inches (in)", L"Points (pt)"};
    for (int i = 0; i < 4; ++i) AppendMenuW(un, MF_STRING | (app.units == i ? MF_CHECKED : 0), G_UNITS + i, unitNames[i]);
    static const wchar_t* scaleNames[] = {L"1 : 1 (actual size)", L"1 : 2", L"1 : 5", L"1 : 10", L"1 : 20", L"1 : 50", L"1 : 100", L"2 : 1", L"5 : 1", L"10 : 1"};
    for (int i = 0; i < 10; ++i) AppendMenuW(sc, MF_STRING | (app.scaleIdx == i ? MF_CHECKED : 0), G_SCALE + i, scaleNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(un), L"Units");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(sc), L"Drawing scale");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (app.viewer.hasMeasurements() ? 0 : MF_GRAYED), G_KEEP, L"Keep the measurements on the page");
    AppendMenuW(m, MF_STRING | (app.viewer.hasMeasurements() ? 0 : MF_GRAYED), G_CLEAR, L"Clear the measurements");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd) geometryAction(cmd);
}

void showSideBySideMenu(const RECT& anchor) {
    HMENU m = CreatePopupMenu();
    const bool two = app.tabs.size() >= 2 && activeDoc();
    AppendMenuW(m, MF_STRING | (two ? 0 : MF_GRAYED), N_SBS_TAB, L"Show another open tab next to this one");
    AppendMenuW(m, MF_STRING | (activeDoc() ? 0 : MF_GRAYED), N_SBS_OPEN, L"Open a file next to this one...");
    AppendMenuW(m, MF_STRING | (app.sbs ? 0 : MF_GRAYED), N_SBS_OFF, L"Back to one document");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (two ? 0 : MF_GRAYED), N_COMPARE, L"Compare with another open tab...");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd == N_COMPARE) compareDocs(); else if (cmd) sideBySideAction(cmd);
}

void showZoomMenu(const RECT& anchor) {
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < 8; ++i) { wchar_t t[16]; swprintf(t, 16, L"%d%%", static_cast<int>(kZoomPresets[i] * 100 + 0.5)); AppendMenuW(m, MF_STRING, Z_BASE + i, t); }
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, Z_BASE + 20, L"Fit width\tCtrl+2");
    AppendMenuW(m, MF_STRING, Z_BASE + 21, L"Fit page\tCtrl+0");
    AppendMenuW(m, MF_STRING, Z_BASE + 22, L"Actual size\tCtrl+1");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    Document* d = activeDoc();
    if (!d || !cmd) return;
    if (cmd < Z_BASE + 8) app.viewer.setZoom(kZoomPresets[cmd - Z_BASE]);
    else if (cmd == Z_BASE + 20) app.viewer.fitWidth();
    else if (cmd == Z_BASE + 21) app.viewer.fitPage();
    else if (cmd == Z_BASE + 22) app.viewer.setZoom(1.0);
}


// ---- page operations ----

std::unique_ptr<Document> openForImport(const std::wstring& path) {
    auto doc = std::make_unique<Document>();
    std::string pw;
    OpenResult r = doc->open(path);
    while (r == OpenResult::PasswordNeeded || r == OpenResult::WrongPassword) {
        if (!askPassword(Document::describe(r, path), pw)) return nullptr;
        r = doc->open(path, pw);
    }
    if (r != OpenResult::Ok) {
        MessageBoxW(app.wnd, Document::describe(r, path).c_str(), kAppName, MB_OK | MB_ICONWARNING);
        return nullptr;
    }
    return doc;
}

// Asks for pages like "1-3, 5". `initial` is shown first. Returns empty when cancelled or invalid (after telling the user).
std::vector<int> askPages(Document& d, const wchar_t* title, const wchar_t* what, const std::wstring& initial) {
    std::wstring in;
    wchar_t prompt[160];
    swprintf(prompt, 160, L"%ls Type page numbers or ranges between 1 and %d, such as 1-3, 5, 8-.", what, d.pageCount());
    while (askText(title, prompt, false, in, initial.c_str())) {
        auto pages = d.parseRange(in);
        if (!pages.empty()) return pages;
        MessageBoxW(app.wnd, L"That is not a valid page list. Use numbers and ranges such as 1-3, 5, 8-.", kAppName, MB_OK | MB_ICONINFORMATION);
    }
    return {};
}

void afterPagesChanged() {
    if (Document* cd = activeDoc()) cd->redactions.clear();
    app.viewer.documentChanged();
    app.sidebar.refresh();
    app.viewer.setMatches({}, -1);
    app.curMatch = -1; app.matchTotal = 0;
    if (app.findVisible && GetWindowTextLengthW(app.findEdit) > 0) runSearch(false);
    refresh();
}

std::wstring pickPdf(bool multi, std::vector<std::wstring>& out, const wchar_t* title) {
    std::vector<wchar_t> buf(MAX_PATH * 16, 0);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0";
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | (multi ? (OFN_ALLOWMULTISELECT | OFN_EXPLORER) : 0);
    out.clear();
    if (!GetOpenFileNameW(&ofn)) return L"";
    const wchar_t* p = buf.data();
    std::wstring first = p;
    p += first.size() + 1;
    if (!*p) out.push_back(first);                       // a single file
    else for (; *p; p += wcslen(p) + 1) out.push_back(first + L"\\" + p);   // folder, then names
    return first;
}

void pagesMenuAction(int cmd, const std::vector<int>& sel);

void showPagesMenu(const RECT& anchor) {
    Document* d = activeDoc();
    if (!d) return;
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* t, bool on = true) { AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED), id, t); };
    add(P_DELETE, L"Delete pages...", d->pageCount() > 1);
    add(P_BLANK, L"Insert a blank page after this one");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(P_ROT_R, L"Rotate this page right");
    add(P_ROT_L, L"Rotate this page left");
    add(P_ROT_ALL, L"Rotate all pages right");
    add(P_MOVE, L"Move pages...", d->pageCount() > 1);
    add(P_CROP, L"Crop pages...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(P_INSERT, L"Insert pages from a file...");
    add(P_MERGE, L"Merge PDFs into the end of this document...");
    add(P_EXTRACT, L"Extract pages into a new file...");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    pagesMenuAction(cmd, {});
}

void pagesMenuAction(int cmd, const std::vector<int>& sel) {
    Document* d = activeDoc();
    if (!d) return;
    const int cur = app.viewer.currentPage();
    std::wstring curText = std::to_wstring(cur + 1);
    if (!sel.empty()) {
        curText.clear();
        for (int p : sel) curText += (curText.empty() ? L"" : L",") + std::to_wstring(p + 1);
    }
    switch (cmd) {
        case P_DELETE: {
            auto pages = askPages(*d, L"Delete pages", L"Which pages should be deleted?", curText);
            if (pages.empty()) break;
            if (static_cast<int>(pages.size()) >= d->pageCount()) {
                MessageBoxW(app.wnd, L"A document needs at least one page, so not every page can be deleted.", kAppName, MB_OK | MB_ICONINFORMATION);
                break;
            }
            if (d->deletePages(pages)) afterPagesChanged();
            break;
        }
        case P_BLANK: if (d->insertBlankPage(cur, cur)) { afterPagesChanged(); app.viewer.goToPage(cur + 1); } break;
        case P_ROT_R: if (d->rotatePages({cur}, 1)) afterPagesChanged(); break;
        case P_ROT_L: if (d->rotatePages({cur}, -1)) afterPagesChanged(); break;
        case P_ROT_ALL: {
            std::vector<int> all(d->pageCount());
            for (int i = 0; i < d->pageCount(); ++i) all[i] = i;
            if (d->rotatePages(all, 1)) afterPagesChanged();
            break;
        }
        case P_MOVE: {
            auto pages = askPages(*d, L"Move pages", L"Which pages should be moved?", curText);
            if (pages.empty()) break;
            std::wstring in;
            wchar_t prompt[128];
            swprintf(prompt, 128, L"Move them before which page? Type 1 to %d, or %d to move them to the end.", d->pageCount(), d->pageCount() + 1);
            while (askText(L"Move pages", prompt, false, in, L"1")) {
                wchar_t* end = nullptr;
                long n = wcstol(in.c_str(), &end, 10);
                if (end != in.c_str() && *end == 0 && n >= 1 && n <= d->pageCount() + 1) {
                    if (d->movePages(pages, static_cast<int>(n) - 1)) afterPagesChanged();
                    break;
                }
                MessageBoxW(app.wnd, prompt, kAppName, MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        case P_CROP: {
            std::wstring in;
            while (askText(L"Crop pages", L"Cut this much off every edge of every page (in millimetres, 0 to 100).", false, in, L"10")) {
                wchar_t* end = nullptr;
                double mm = wcstod(in.c_str(), &end);
                if (end != in.c_str() && *end == 0 && mm > 0 && mm <= 100) {
                    std::vector<int> all(d->pageCount());
                    for (int i = 0; i < d->pageCount(); ++i) all[i] = i;
                    const double pt = mm * 72.0 / 25.4;
                    if (d->cropPages(all, pt, pt, pt, pt)) afterPagesChanged();
                    else MessageBoxW(app.wnd, L"That would leave the pages too small. Choose a smaller amount.", kAppName, MB_OK | MB_ICONINFORMATION);
                    break;
                }
                MessageBoxW(app.wnd, L"Type a number of millimetres greater than 0 and up to 100.", kAppName, MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        case P_INSERT: {
            std::vector<std::wstring> files;
            pickPdf(false, files, L"Insert pages from");
            if (files.empty()) break;
            auto src = openForImport(files[0]);
            if (!src) break;
            std::wstring in;
            wchar_t prompt[160];
            swprintf(prompt, 160, L"Which pages of \"%ls\" (it has %d)? Leave as it is for all of them.", src->title().c_str(), src->pageCount());
            std::wstring all = L"1-" + std::to_wstring(src->pageCount());
            std::vector<int> pages;
            while (askText(L"Insert pages", prompt, false, in, all.c_str())) {
                pages = src->parseRange(in);
                if (!pages.empty()) break;
                MessageBoxW(app.wnd, L"That is not a valid page list.", kAppName, MB_OK | MB_ICONINFORMATION);
            }
            if (pages.empty()) break;
            swprintf(prompt, 160, L"Insert them after which page? Type 0 for the start, or 1 to %d.", d->pageCount());
            std::wstring where;
            while (askText(L"Insert pages", prompt, false, where, curText.c_str())) {
                wchar_t* end = nullptr;
                long n = wcstol(where.c_str(), &end, 10);
                if (end != where.c_str() && *end == 0 && n >= 0 && n <= d->pageCount()) {
                    if (d->importPages(*src, pages, static_cast<int>(n))) { afterPagesChanged(); app.viewer.goToPage(static_cast<int>(n)); }
                    else MessageBoxW(app.wnd, L"The pages could not be copied.", kAppName, MB_OK | MB_ICONWARNING);
                    break;
                }
                MessageBoxW(app.wnd, prompt, kAppName, MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        case P_MERGE: {
            std::vector<std::wstring> files;
            pickPdf(true, files, L"Choose the PDFs to add (they are added in the order shown)");
            if (files.empty()) break;
            std::sort(files.begin(), files.end());
            int added = 0;
            for (const auto& f : files) {
                auto src = openForImport(f);
                if (!src) continue;
                std::vector<int> all(src->pageCount());
                for (int i = 0; i < src->pageCount(); ++i) all[i] = i;
                if (d->importPages(*src, all, d->pageCount())) ++added;
                else MessageBoxW(app.wnd, (L"The pages of \"" + src->title() + L"\" could not be added.").c_str(), kAppName, MB_OK | MB_ICONWARNING);
            }
            if (added) { afterPagesChanged(); app.viewer.goToPage(d->pageCount() - 1); }
            break;
        }
        case P_EXTRACT: {
            auto pages = askPages(*d, L"Extract pages", L"Which pages should go into the new file?", curText);
            if (pages.empty()) break;
            wchar_t buf[MAX_PATH * 2] = L"";
            std::wstring base = d->title();
            size_t dot = base.find_last_of(L'.');
            if (dot != std::wstring::npos) base.resize(dot);
            wcsncpy_s(buf, (base + L" (extract).pdf").c_str(), _TRUNCATE);
            OPENFILENAMEW ofn{sizeof(ofn)};
            ofn.hwndOwner = app.wnd;
            ofn.lpstrTitle = L"Save the extracted pages as";
            ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = MAX_PATH * 2;
            ofn.lpstrDefExt = L"pdf";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetSaveFileNameW(&ofn)) break;
            bool openTab = false;
            for (const auto& t : app.tabs) if (_wcsicmp(t->path().c_str(), buf) == 0) openTab = true;
            if (openTab) { MessageBoxW(app.wnd, L"That file is open in a tab. Close it first, or choose another name.", kAppName, MB_OK | MB_ICONWARNING); break; }
            SaveResult r = d->extractPages(pages, buf);
            if (r != SaveResult::Ok)
                MessageBoxW(app.wnd, r == SaveResult::DiskFull ? L"The file could not be saved because the disk is full."
                                                               : L"The pages could not be saved. The folder may be read-only, or the file may be open in another program.",
                            kAppName, MB_OK | MB_ICONWARNING);
            else if (MessageBoxW(app.wnd, L"The pages were saved. Open the new file in a tab?", kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES) openFile(buf);
            break;
        }
    }
}


// ---- tools: watermark, page numbers ----

Document* g_formDoc = nullptr;   // the document the open form works on (for validating page lists)

std::vector<int> pagesFromText(Document& d, const std::wstring& t) {
    bool blank = true;
    for (wchar_t c : t) if (c != L' ') blank = false;
    if (blank) {
        std::vector<int> all(d.pageCount());
        for (int i = 0; i < d.pageCount(); ++i) all[i] = i;
        return all;
    }
    return d.parseRange(t);
}

std::wstring badPages(const std::wstring& t) {
    if (!g_formDoc || !pagesFromText(*g_formDoc, t).empty()) return L"";
    return L"The pages are not valid. Leave the box empty for all pages, or type page numbers and ranges such as 1-3, 5, 8-.";
}

std::wstring validateWatermark(const std::vector<FormField>& f) {
    if (f[0].text.empty()) return L"Type the text of the watermark.";
    if (!pageobj::isLatin(f[0].text)) return L"The watermark can use letters, numbers and common symbols of Western languages. Please remove other characters.";
    return badPages(f[6].text);
}

std::wstring validateNumbers(const std::vector<FormField>& f) {
    wchar_t* end = nullptr;
    long n = wcstol(f[2].text.c_str(), &end, 10);
    if (end == f[2].text.c_str() || *end || n < 0 || n > 1000000) return L"Enter a whole number from 0 to 1000000 for the first page number.";
    return badPages(f[4].text);
}

void doWatermark(Document& d) {
    std::vector<FormField> f(7);
    f[0] = {FormField::Text, L"Text:", {}, L"CONFIDENTIAL"};
    f[1] = {FormField::Choice, L"Colour:", {kPaletteNames[0], kPaletteNames[1], kPaletteNames[2], kPaletteNames[3], kPaletteNames[4], kPaletteNames[5], kPaletteNames[6]}, L"", 5};
    f[2] = {FormField::Choice, L"Opacity:", {L"10%", L"20%", L"30%", L"40%", L"50%", L"70%", L"100%"}, L"", 2};
    f[3] = {FormField::Choice, L"Size:", {L"Fit the page", L"Small", L"Medium", L"Large"}, L"", 0};
    f[4] = {FormField::Choice, L"Direction:", {L"Diagonal", L"Horizontal"}, L"", 0};
    f[5] = {FormField::Check, L"Put it behind the page content (hidden by pictures and scans)"};
    f[6] = {FormField::Text, L"Pages (empty = all):", {}, L""};
    g_formDoc = &d;
    bool ok = showForm(app.wnd, app.inst, L"Watermark", f, L"Add", validateWatermark);
    g_formDoc = nullptr;
    if (!ok) return;
    static const int pct[] = {10, 20, 30, 40, 50, 70, 100};
    pageobj::WatermarkStyle st;
    st.color = kPalette[f[1].choice];
    st.opacityPercent = pct[std::clamp(f[2].choice, 0, 6)];
    st.sizeMode = f[3].choice;
    st.diagonal = f[4].choice == 0;
    st.behind = f[5].checked;
    if (pageobj::addWatermark(d, pagesFromText(d, f[6].text), f[0].text, st)) afterPagesChanged();
    else MessageBoxW(app.wnd, L"The watermark could not be added.", kAppName, MB_OK | MB_ICONWARNING);
}

void doPageNumbers(Document& d) {
    std::vector<FormField> f(5);
    f[0] = {FormField::Choice, L"Format:", {L"1", L"Page 1", L"Page 1 of N", L"1 / N"}, L"", 1};
    f[1] = {FormField::Choice, L"Position:", {L"Bottom centre", L"Bottom left", L"Bottom right", L"Top centre", L"Top left", L"Top right"}, L"", 0};
    f[2] = {FormField::Text, L"Start at:", {}, L"1"};
    f[3] = {FormField::Choice, L"Size:", {L"8 pt", L"10 pt", L"12 pt", L"14 pt"}, L"", 1};
    f[4] = {FormField::Text, L"Pages (empty = all):", {}, L""};
    g_formDoc = &d;
    bool ok = showForm(app.wnd, app.inst, L"Page numbers", f, L"Add", validateNumbers);
    g_formDoc = nullptr;
    if (!ok) return;
    pageobj::NumberStyle st;
    st.format = f[0].choice;
    st.position = f[1].choice;
    st.startAt = static_cast<int>(wcstol(f[2].text.c_str(), nullptr, 10));
    static const double sizes[] = {8, 10, 12, 14};
    st.sizePt = sizes[std::clamp(f[3].choice, 0, 3)];
    if (pageobj::addPageNumbers(d, pagesFromText(d, f[4].text), st)) afterPagesChanged();
    else MessageBoxW(app.wnd, L"The page numbers could not be added.", kAppName, MB_OK | MB_ICONWARNING);
}


void doExport(Document& d) {
    std::vector<FormField> f(3);
    f[0] = {FormField::Choice, L"Export as:", {L"PNG pictures (best for text and drawings)", L"JPEG pictures (smaller, for photos)", L"Plain text (.txt)",
                                               L"Markdown (.md)", L"Word document (.docx)", L"Excel workbook, tables only (.xlsx)", L"CSV, tables only (.csv)"}, L"", 4};
    f[1] = {FormField::Choice, L"Picture resolution:", {L"72 dpi (screen)", L"150 dpi (good quality)", L"220 dpi (high quality)", L"300 dpi (print)"}, L"", 1};
    f[2] = {FormField::Text, L"Pages (empty = all):", {}, L""};
    g_formDoc = &d;
    bool ok = showForm(app.wnd, app.inst, L"Export", f, L"Export...", [](const std::vector<FormField>& v) { return badPages(v[2].text); });
    g_formDoc = nullptr;
    if (!ok) return;
    static const exporter::Format fmts[] = {exporter::Format::Png, exporter::Format::Jpeg, exporter::Format::Text, exporter::Format::Markdown,
                                            exporter::Format::Word, exporter::Format::ExcelXlsx, exporter::Format::Csv};
    static const int dpis[] = {72, 150, 220, 300};
    const exporter::Format fmt = fmts[std::clamp(f[0].choice, 0, 6)];
    const bool pics = fmt == exporter::Format::Png || fmt == exporter::Format::Jpeg;
    std::vector<int> pages = pagesFromText(d, f[2].text);

    static const wchar_t* filters[] = {L"PNG pictures (*.png)\0*.png\0", L"JPEG pictures (*.jpg)\0*.jpg\0", L"Text files (*.txt)\0*.txt\0",
                                       L"Markdown files (*.md)\0*.md\0", L"Word documents (*.docx)\0*.docx\0", L"Excel workbooks (*.xlsx)\0*.xlsx\0",
                                       L"CSV files (*.csv)\0*.csv\0"};
    wchar_t buf[MAX_PATH * 2] = L"";
    std::wstring base = d.title();
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    wcsncpy_s(buf, (base + L"." + exporter::extension(fmt)).c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = pics ? L"Export as (each page gets its number added)" : L"Export as";
    ofn.lpstrFilter = filters[std::clamp(f[0].choice, 0, 6)];
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrDefExt = exporter::extension(fmt);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return;
    for (const auto& t : app.tabs)
        if (_wcsicmp(t->path().c_str(), buf) == 0) {
            MessageBoxW(app.wnd, L"That file is open in a tab. Choose another name.", kAppName, MB_OK | MB_ICONWARNING);
            return;
        }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    exporter::Result r = exporter::run(d, pages, fmt, buf, dpis[std::clamp(f[1].choice, 0, 3)]);
    SetCursor(old);
    if (!r.ok) {
        MessageBoxW(app.wnd, (r.error.empty() ? L"The export could not be completed." : r.error).c_str(), kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t msg[600];
    if (pics) swprintf(msg, 600, L"%d picture%ls saved next to \"%ls\". Show them in the folder?", r.items, r.items == 1 ? L"" : L"s", r.firstFile.substr(r.firstFile.find_last_of(L"\\/") + 1).c_str());
    else swprintf(msg, 600, L"Saved \"%ls\". Open it?", r.firstFile.substr(r.firstFile.find_last_of(L"\\/") + 1).c_str());
    if (MessageBoxW(app.wnd, msg, kAppName, MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        if (pics) { std::wstring a = L"/select,\"" + r.firstFile + L"\""; ShellExecuteW(app.wnd, nullptr, L"explorer.exe", a.c_str(), nullptr, SW_SHOWNORMAL); }
        else ShellExecuteW(app.wnd, L"open", r.firstFile.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}


// ---- read aloud, print, properties ----

void stopReading() { speech::stop(); }

void readAloud() {
    Document* d = activeDoc();
    if (!d) return;
    if (speech::speaking()) { stopReading(); return; }
    std::wstring text = app.viewer.selectedText();
    if (text.empty()) {   // from the page being read to the end of the document
        for (int p = app.viewer.currentPage(); p < d->pageCount() && text.size() < 200000; ++p) {
            std::wstring t = d->textRange(p, 0, d->charCount(p));
            if (!t.empty()) text += t + L"\r\n";
        }
    }
    if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        MessageBoxW(app.wnd, L"There is no text to read here. Scanned pages are pictures; they need text recognition (OCR) first.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring err;
    if (!speech::start(text, app.readRate, err)) MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}

void printDocument() {
    Document* d = activeDoc();
    if (!d) return;
    // printers installed on this PC
    std::vector<std::wstring> printers;
    {
        DWORD need = 0, n = 0;
        EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, nullptr, 0, &need, &n);
        std::vector<BYTE> buf(need ? need : 1);
        if (need && EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, buf.data(), need, &need, &n))
            for (DWORD i = 0; i < n; ++i) printers.push_back(reinterpret_cast<PRINTER_INFO_4W*>(buf.data())[i].pPrinterName);
    }
    if (printers.empty()) {
        MessageBoxW(app.wnd, L"No printer is installed. Add one in Windows Settings > Bluetooth & devices > Printers & scanners, then try again.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t def[256] = L"";
    DWORD defLen = 256;
    GetDefaultPrinterW(def, &defLen);
    int defIdx = 0;
    for (size_t i = 0; i < printers.size(); ++i) if (printers[i] == def) defIdx = static_cast<int>(i);

    std::vector<FormField> f(7);
    f[0] = {FormField::Choice, L"Printer:", printers, L"", defIdx};
    f[1] = {FormField::Choice, L"Pages:", {L"All pages", L"Current page", L"Page range"}, L"", 0};
    f[2] = {FormField::Text, L"Range (such as 1-3, 5, 8-):", {}, L""};
    f[3] = {FormField::Text, L"Copies:", {}, L"1"};
    f[4] = {FormField::Choice, L"Layout:", {L"1 page per sheet", L"2 pages per sheet", L"4 pages per sheet", L"6 pages per sheet", L"9 pages per sheet"}, L"", 0};
    f[5] = {FormField::Choice, L"Size:", {L"Fit to the paper", L"Actual size"}, L"", 0};
    f[6] = {FormField::Choice, L"Orientation:", {L"Automatic (as the pages)", L"Portrait", L"Landscape"}, L"", 0};
    g_formDoc = d;
    bool ok = showForm(app.wnd, app.inst, L"Print", f, L"Print", [](const std::vector<FormField>& v) -> std::wstring {
        wchar_t* end = nullptr;
        long c = wcstol(v[3].text.c_str(), &end, 10);
        if (end == v[3].text.c_str() || *end || c < 1 || c > 999) return L"Copies must be a number from 1 to 999.";
        if (v[1].choice == 2 && (!g_formDoc || g_formDoc->parseRange(v[2].text).empty())) return L"Type the pages to print, such as 1-3, 5, 8-.";
        return L"";
    });
    g_formDoc = nullptr;
    if (!ok) return;

    std::vector<int> pages;
    if (f[1].choice == 0) { for (int i = 0; i < d->pageCount(); ++i) pages.push_back(i); }
    else if (f[1].choice == 1) pages.push_back(app.viewer.currentPage());
    else pages = d->parseRange(f[2].text);
    const int copies = static_cast<int>(wcstol(f[3].text.c_str(), nullptr, 10));
    static const int perSheet[] = {1, 2, 4, 6, 9}, cols[] = {1, 2, 2, 2, 3}, rowsN[] = {1, 1, 2, 3, 3};
    const int nup = perSheet[f[4].choice], gc = cols[f[4].choice], gr = rowsN[f[4].choice];
    const bool actual = f[5].choice == 1;
    const std::wstring& pname = printers[f[0].choice];

    // orientation: choose from the first page when automatic
    int orient = f[6].choice == 1 ? DMORIENT_PORTRAIT : f[6].choice == 2 ? DMORIENT_LANDSCAPE : 0;
    if (!orient) {
        const PageSize& ps0 = d->pageSize(pages.front());
        bool land = ps0.w > ps0.h;
        if (nup == 2) land = !land;   // two portrait pages sit side by side on a landscape sheet
        orient = land ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
    }
    HDC dc = nullptr;
    HANDLE hp = nullptr;
    std::vector<BYTE> dm;
    if (OpenPrinterW(const_cast<wchar_t*>(pname.c_str()), &hp, nullptr)) {
        LONG sz = DocumentPropertiesW(app.wnd, hp, const_cast<wchar_t*>(pname.c_str()), nullptr, nullptr, 0);
        if (sz > 0) {
            dm.resize(sz);
            auto* dev = reinterpret_cast<DEVMODEW*>(dm.data());
            if (DocumentPropertiesW(app.wnd, hp, const_cast<wchar_t*>(pname.c_str()), dev, nullptr, DM_OUT_BUFFER) == IDOK) {
                dev->dmFields |= DM_ORIENTATION;
                dev->dmOrientation = static_cast<short>(orient);
                DocumentPropertiesW(app.wnd, hp, const_cast<wchar_t*>(pname.c_str()), dev, dev, DM_IN_BUFFER | DM_OUT_BUFFER);
                dc = CreateDCW(L"WINSPOOL", pname.c_str(), nullptr, dev);
            }
        }
        ClosePrinter(hp);
    }
    if (!dc) dc = CreateDCW(L"WINSPOOL", pname.c_str(), nullptr, nullptr);
    if (!dc) { MessageBoxW(app.wnd, L"The printer could not be opened. Check that it is on and connected.", kAppName, MB_OK | MB_ICONWARNING); return; }

    DOCINFOW di{sizeof(di)};
    std::wstring name = d->title();
    di.lpszDocName = name.c_str();
    bool good = StartDocW(dc, &di) > 0;
    const int areaW = GetDeviceCaps(dc, HORZRES), areaH = GetDeviceCaps(dc, VERTRES);
    const double ppp = GetDeviceCaps(dc, LOGPIXELSX) / 72.0;   // device pixels per point
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    for (int c = 0; good && c < copies; ++c) {
        for (size_t k = 0; good && k < pages.size(); k += nup) {
            if (StartPage(dc) <= 0) { good = false; break; }
            const int cw = areaW / gc, ch = areaH / gr, pad = nup > 1 ? 12 : 0;
            for (int s = 0; s < nup && k + s < pages.size(); ++s) {
                FPDF_PAGE pg = d->page(pages[k + s]);
                if (!pg) continue;
                const PageSize& ps = d->pageSize(pages[k + s]);
                const int cx = (s % gc) * cw + pad, cy = (s / gc) * ch + pad, aw = cw - 2 * pad, ah = ch - 2 * pad;
                bool rotate = false;
                double pw = ps.w, ph = ps.h;
                if (nup == 1 && f[6].choice == 0 && (ps.w > ps.h) != (aw > ah)) { rotate = true; std::swap(pw, ph); }
                double sc = actual && nup == 1 ? ppp : std::min(aw / pw, ah / ph);
                int w = static_cast<int>(pw * sc), h = static_cast<int>(ph * sc);
                int x = cx + (aw - w) / 2, y = cy + (ah - h) / 2;
                if (actual && nup == 1) { x = std::max(0, x); y = std::max(0, y); }
                pdfium::api().FPDF_RenderPage(dc, pg, x, y, w, h, rotate ? 1 : 0, FPDF_ANNOT | FPDF_PRINTING);
                if (nup > 1) {   // thin frame so the sheets are easy to tell apart
                    HGDIOBJ op = SelectObject(dc, GetStockObject(BLACK_PEN)), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                    Rectangle(dc, x, y, x + w, y + h);
                    SelectObject(dc, ob); SelectObject(dc, op);
                }
            }
            if (EndPage(dc) <= 0) good = false;
        }
    }
    SetCursor(old);
    if (good) EndDoc(dc); else AbortDoc(dc);
    DeleteDC(dc);
    if (!good) MessageBoxW(app.wnd, L"The document could not be printed. Check that the printer is on and connected, then try again.", kAppName, MB_OK | MB_ICONWARNING);
}

std::wstring metaText(FPDF_DOCUMENT doc, const char* tag) {
    unsigned long bytes = pdfium::api().FPDF_GetMetaText(doc, tag, nullptr, 0);
    if (bytes < 4) return L"";
    std::vector<unsigned short> buf(bytes / 2 + 1);
    pdfium::api().FPDF_GetMetaText(doc, tag, buf.data(), bytes);
    return std::wstring(buf.begin(), buf.begin() + (bytes / 2 - 1));
}

std::wstring prettyDate(const std::wstring& s) {   // "D:20260102150405+01'00'" -> "2026-01-02 15:04"
    if (s.size() < 10 || s.compare(0, 2, L"D:") != 0) return s;
    std::wstring o = s.substr(2, 4) + L"-" + s.substr(6, 2) + L"-" + s.substr(8, 2);
    if (s.size() >= 14) o += L" " + s.substr(10, 2) + L":" + s.substr(12, 2);
    return o;
}

void showProperties() {
    Document* d = activeDoc();
    if (!d) return;
    auto line = [&](const wchar_t* label, const std::wstring& v) { return std::wstring(label) + L"  " + (v.empty() ? L"-" : v) + L"\n"; };
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    std::wstring size;
    if (GetFileAttributesExW(d->path().c_str(), GetFileExInfoStandard, &fa)) {
        unsigned long long b = (static_cast<unsigned long long>(fa.nFileSizeHigh) << 32) | fa.nFileSizeLow;
        wchar_t t[64];
        if (b >= (1ull << 20)) swprintf(t, 64, L"%.1f MB (%llu bytes)", b / 1048576.0, b); else swprintf(t, 64, L"%.1f KB (%llu bytes)", b / 1024.0, b);
        size = t;
    }
    int ver = 0;
    pdfium::api().FPDF_GetFileVersion(d->handle(), &ver);
    wchar_t pg[96];
    swprintf(pg, 96, L"%d (first page %.0f x %.0f mm)", d->pageCount(), d->pageSize(0).w * 25.4 / 72, d->pageSize(0).h * 25.4 / 72);
    std::wstring text;
    text += line(L"File:", d->path());
    text += line(L"File size:", size);
    text += line(L"Pages:", pg);
    text += line(L"PDF version:", ver ? std::to_wstring(ver / 10) + L"." + std::to_wstring(ver % 10) : L"");
    text += line(L"Protected by a password:", d->isProtected() ? L"yes" : L"no");
    text += L"\n";
    text += line(L"Title:", metaText(d->handle(), "Title"));
    text += line(L"Author:", metaText(d->handle(), "Author"));
    text += line(L"Subject:", metaText(d->handle(), "Subject"));
    text += line(L"Keywords:", metaText(d->handle(), "Keywords"));
    text += line(L"Created with:", metaText(d->handle(), "Creator"));
    text += line(L"PDF producer:", metaText(d->handle(), "Producer"));
    text += line(L"Created:", prettyDate(metaText(d->handle(), "CreationDate")));
    text += line(L"Modified:", prettyDate(metaText(d->handle(), "ModDate")));
    std::vector<FormField> f(1);
    f[0].kind = FormField::Note; f[0].text = text;
    showForm(app.wnd, app.inst, L"Document properties", f, L"Close");
}


// ---- redaction ----

void showRedactionReport(Document& d, const std::vector<std::wstring>& phrases) {
    if (phrases.empty()) {
        MessageBoxW(app.wnd, L"The marked areas were removed. They held no text, so there was nothing to look for. Pictures under the black boxes are gone: those pages are now pictures.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    redact::Report r = redact::verify(d, phrases);
    SetCursor(old);
    std::wstring msg;
    if (r.found.empty()) {
        msg = L"Checked: " + std::to_wstring(r.phrases) + L" redacted phrase(s) were looked for in the text of every page, comments, bookmarks, the document information and the file data. None of them is still in the file.";
    } else {
        msg = L"WARNING: some redacted text is still in the file:\n\n";
        for (size_t i = 0; i < r.found.size() && i < 12; ++i) msg += L"  \"" + r.found[i].phrase.substr(0, 40) + L"\" in " + r.found[i].where + L"\n";
        msg += L"\nDo not share the file yet. Remove those items (for example the comments or bookmarks) and check again with Edit PDF > Redact > Check that the text is gone.";
    }
    MessageBoxW(app.wnd, msg.c_str(), L"Redaction check", MB_OK | (r.found.empty() ? MB_ICONINFORMATION : MB_ICONWARNING));
}

void checkRedactionText(Document& d) {
    std::wstring t;
    if (!askText(L"Check that the text is gone", L"Type the words or phrases that must not be in this document, one per line.", false, t, app.viewer.hasSelection() ? toEdit(app.viewer.selectedText()).c_str() : L"", true)) return;
    t = fromEdit(t);
    std::vector<std::wstring> list;
    size_t pos = 0;
    while (pos <= t.size()) {
        size_t e = t.find(L'\r', pos);
        std::wstring line = t.substr(pos, e == std::wstring::npos ? std::wstring::npos : e - pos);
        size_t a = line.find_first_not_of(L' '), z = line.find_last_not_of(L' ');
        if (a != std::wstring::npos && z - a + 1 >= 3) list.push_back(line.substr(a, z - a + 1));
        if (e == std::wstring::npos) break;
        pos = e + 1;
    }
    if (list.empty()) { MessageBoxW(app.wnd, L"Type at least one word of three or more letters.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    showRedactionReport(d, list);
}

void redactionMenuAction(Document& d, int cmd) {
    switch (cmd) {
        case T_RED_AREA:
            setTool(Viewer::Tool::Redact);
            break;
        case T_RED_SEL: {
            auto sel = app.viewer.selectionRects();
            if (sel.empty()) { MessageBoxW(app.wnd, L"Select some text first (drag over it), then choose this again.", kAppName, MB_OK | MB_ICONINFORMATION); break; }
            for (const auto& pr : sel) for (const auto& r : pr.rects) d.redactions.push_back({pr.page, r.left, r.bottom, r.right, r.top});
            app.viewer.clearSelection();
            app.viewer.setMarks(&d.redactions);
            break;
        }
        case T_RED_FIND: {
            std::vector<FormField> f(2);
            f[0] = {FormField::Text, L"Text to find:", {}, L""};
            f[1] = {FormField::Check, L"Match case"};
            if (!showForm(app.wnd, app.inst, L"Find and mark for redaction", f, L"Mark all", [](const std::vector<FormField>& v) -> std::wstring {
                    return v[0].text.empty() ? L"Type the text to find. Every occurrence in the document will be marked." : L""; })) break;
            HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            int n = redact::markText(d, f[0].text, f[1].checked);
            SetCursor(old);
            app.viewer.setMarks(&d.redactions);
            if (!n) MessageBoxW(app.wnd, L"Nothing was found to mark.", kAppName, MB_OK | MB_ICONINFORMATION);
            break;
        }
        case N_RED_CHECK: checkRedactionText(d); break;
        case T_RED_CLEAR:
            d.redactions.clear();
            app.viewer.setMarks(&d.redactions);
            break;
        case T_RED_APPLY: {
            if (d.redactions.empty()) {
                MessageBoxW(app.wnd, L"Mark what to redact first: use Mark areas, Mark selected text, or Find and mark.", kAppName, MB_OK | MB_ICONINFORMATION);
                break;
            }
            std::set<int> pages;
            for (auto& m : d.redactions) pages.insert(m.page);
            wchar_t q[600];
            swprintf(q, 600, L"Remove everything under the %d marked area%ls (on %d page%ls) for good?\n\n"
                             L"Those pages become pictures: their text can no longer be selected or searched, and their comments are removed. "
                             L"You can undo this (Ctrl+Z) until you save the file.",
                     static_cast<int>(d.redactions.size()), d.redactions.size() == 1 ? L"" : L"s", static_cast<int>(pages.size()), pages.size() == 1 ? L"" : L"s");
            if (MessageBoxW(app.wnd, q, L"Apply redactions", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) break;
            HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            const std::vector<std::wstring> phrases = redact::collect(d);
            int n = redact::apply(d);
            SetCursor(old);
            if (!n) { MessageBoxW(app.wnd, L"The redactions could not be applied.", kAppName, MB_OK | MB_ICONWARNING); break; }
            afterPagesChanged();
            showRedactionReport(d, phrases);
            break;
        }
    }
}


void doCompress(Document& d) {
    std::vector<FormField> f(4);
    f[0] = {FormField::Note, L"", {}, L"Makes the file smaller by storing large pictures at a lower resolution as JPEG. Text and drawings stay sharp."};
    f[1] = {FormField::Choice, L"Quality:", {L"Balanced (150 dpi) - recommended", L"High quality (220 dpi)", L"Smallest file (96 dpi)", L"Make it smaller than..."}, L"", 0};
    f[2] = {FormField::Text, L"Target size (MB):", {}, L"2"};
    f[3] = {FormField::Check, L"Black and white / greyscale (smallest for scans)"};
    if (!showForm(app.wnd, app.inst, L"Compress", f, L"Compress", [](const std::vector<FormField>& v) -> std::wstring {
            if (v[1].choice != 3) return L"";
            wchar_t* end = nullptr;
            double mb = wcstod(v[2].text.c_str(), &end);
            return (end == v[2].text.c_str() || *end || mb < 0.05 || mb > 10000) ? L"Type the size in megabytes, for example 2 or 0.5." : L""; })) return;
    compress::Options o;
    static const int dpis[] = {150, 220, 96};
    static const int qual[] = {80, 88, 70};
    if (f[1].choice == 3) o.targetMB = wcstod(f[2].text.c_str(), nullptr);
    else { o.dpi = dpis[f[1].choice]; o.jpegQuality = qual[f[1].choice]; }
    o.greyscale = f[3].checked;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    compress::Result r = compress::run(d, o);
    SetCursor(old);
    auto mb = [](size_t b) { wchar_t t[32]; if (b >= (1u << 20)) swprintf(t, 32, L"%.1f MB", b / 1048576.0); else swprintf(t, 32, L"%.0f KB", b / 1024.0); return std::wstring(t); };
    if (!r.changed) {
        MessageBoxW(app.wnd, L"No pictures could be made smaller: they are already at or below this resolution, have transparent parts, or the file has no pictures. The document is unchanged.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    afterPagesChanged();
    std::wstring msg = L"The file is now about " + mb(r.after) + L" (it was " + mb(r.before) + L"). Pictures made smaller: " + std::to_wstring(r.pictures) + L".\n\nUndo (Ctrl+Z) takes the change back; Save keeps it.";
    if (o.targetMB > 0 && !r.reachedTarget) msg = L"The file could not get under " + mb(static_cast<size_t>(o.targetMB * 1048576.0)) + L"; the smallest result is " + mb(r.after) + L" (it was " + mb(r.before) + L").\n\nUndo (Ctrl+Z) takes the change back; Save keeps it.";
    MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}


void textStyleDialog() {
    std::vector<FormField> f(5);
    f[0] = {FormField::Choice, L"Font:", {L"Sans-serif (Helvetica)", L"Serif (Times)", L"Monospaced (Courier)"}, L"", static_cast<int>(app.textStyle.family)};
    int si = 3;
    static const double sizes[] = {8, 10, 12, 14, 18, 24, 36, 48, 72};
    for (int i = 0; i < 9; ++i) if (sizes[i] == app.textStyle.sizePt) si = i;
    f[1] = {FormField::Choice, L"Size:", {L"8 pt", L"10 pt", L"12 pt", L"14 pt", L"18 pt", L"24 pt", L"36 pt", L"48 pt", L"72 pt"}, L"", si};
    int ci = 6;
    for (int i = 0; i < 7; ++i) if (kPalette[i] == app.textStyle.color) ci = i;
    f[2] = {FormField::Choice, L"Colour:", {kPaletteNames[0], kPaletteNames[1], kPaletteNames[2], kPaletteNames[3], kPaletteNames[4], kPaletteNames[5], kPaletteNames[6]}, L"", ci};
    f[3] = {FormField::Check, L"Bold"}; f[3].checked = app.textStyle.bold;
    f[4] = {FormField::Check, L"Italic"}; f[4].checked = app.textStyle.italic;
    if (!showForm(app.wnd, app.inst, L"Style of new text", f, L"OK")) return;
    app.textStyle.family = static_cast<pageobj::Family>(f[0].choice);
    app.textStyle.sizePt = sizes[std::clamp(f[1].choice, 0, 8)];
    app.textStyle.color = kPalette[std::clamp(f[2].choice, 0, 6)];
    app.textStyle.bold = f[3].checked;
    app.textStyle.italic = f[4].checked;
}


void doProtect(Document& d) {
    std::vector<FormField> f(8);
    f[0] = {FormField::Note, L"", {}, L"Protect the document with a password (AES-256). Keep the passwords safe: they cannot be recovered.\nThe protection is applied when you save the document."};
    f[1] = {FormField::Password, L"Password to open it:", {}, L""};
    f[2] = {FormField::Password, L"Type it again:", {}, L""};
    f[3] = {FormField::Check, L"Limit what others can do with it"};
    f[4] = {FormField::Password, L"Owner password:", {}, L""};
    f[5] = {FormField::Password, L"Type it again:", {}, L""};
    f[6] = {FormField::Check, L"Allow printing"}; f[6].checked = true;
    f[7] = {FormField::Check, L"Allow copying text and pictures"}; f[7].checked = true;
    std::vector<FormField> g = f;
    g.push_back({FormField::Check, L"Allow changes (editing, comments, filling in forms)"});
    g.back().checked = true;
    if (!showForm(app.wnd, app.inst, L"Password protection", g, L"Protect", [](const std::vector<FormField>& v) -> std::wstring {
            if (v[1].text != v[2].text) return L"The two passwords to open the document are not the same. Type them again.";
            const bool limit = v[3].checked;
            if (limit) {
                if (v[4].text.empty()) return L"Type an owner password: it is needed to lift the limits later.";
                if (v[4].text != v[5].text) return L"The two owner passwords are not the same. Type them again.";
                if (v[4].text == v[1].text) return L"The owner password must be different from the password to open the document.";
            } else if (v[1].text.empty()) return L"Type a password to open the document, choose limits, or both.";
            return L""; })) return;
    protect::Options o;
    o.userPassword = g[1].text;
    o.ownerPassword = g[3].checked ? g[4].text : L"";
    o.allowPrint = !g[3].checked || g[6].checked;
    o.allowCopy = !g[3].checked || g[7].checked;
    o.allowModify = !g[3].checked || g[8].checked;
    if (d.setProtection(o)) {
        refresh();
        MessageBoxW(app.wnd, L"New protection is set. It is applied when you save the document (Ctrl+S).", kAppName, MB_OK | MB_ICONINFORMATION);
    }
}

void doUnprotect(Document& d) {
    if (MessageBoxW(app.wnd, L"Remove the password protection? The saved file will open without a password and without limits.", kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if (d.removeProtection()) {
        refresh();
        MessageBoxW(app.wnd, L"The protection will be removed when you save the document (Ctrl+S).", kAppName, MB_OK | MB_ICONINFORMATION);
    }
}


// A small always-on-top box that shows progress of a long task and lets Esc stop it.
struct Progress {
    HWND box = nullptr;
    explicit Progress(const wchar_t* text) {
        RECT pr; GetWindowRect(app.wnd, &pr);
        const int w = 420, h = 80;
        box = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"STATIC", text, WS_POPUP | WS_BORDER | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
                              pr.left + (pr.right - pr.left - w) / 2, pr.top + (pr.bottom - pr.top - h) / 3, w, h, app.wnd, nullptr, app.inst, nullptr);
        SendMessageW(box, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
        EnableWindow(app.wnd, FALSE);
        pump();
    }
    ~Progress() { EnableWindow(app.wnd, TRUE); if (box) DestroyWindow(box); SetForegroundWindow(app.wnd); }
    void set(const std::wstring& t) { SetWindowTextW(box, t.c_str()); pump(); }
    bool stopRequested() { pump(); return (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0; }
    static void pump() {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
};

void doOcr(Document& d) {
    std::wstring why;
    if (!ocr::available(why)) { MessageBoxW(app.wnd, why.c_str(), kAppName, MB_OK | MB_ICONINFORMATION); return; }
    auto langs = ocr::languages();
    std::vector<FormField> f(4);
    f[0] = {FormField::Note, L"", {}, L"Finds the words on scanned pages and adds them invisibly, so the pages can be searched, selected and copied. The pages look exactly the same."};
    f[1] = {FormField::Choice, L"Language:", {}, L"", 0};
    for (auto& l : langs) f[1].choices.push_back(l);
    f[2] = {FormField::Text, L"Pages (empty = all):", {}, L""};
    f[3] = {FormField::Check, L"Skip pages that already have text"}; f[3].checked = true;
    g_formDoc = &d;
    bool ok = showForm(app.wnd, app.inst, L"Recognise text (OCR)", f, L"Recognise", [](const std::vector<FormField>& v) { return badPages(v[2].text); });
    g_formDoc = nullptr;
    if (!ok) return;
    std::vector<int> pages = pagesFromText(d, f[2].text);
    ocr::Result r;
    {
        Progress progress(L"Starting text recognition...");
        r = ocr::run(d, pages, langs[std::clamp(f[1].choice, 0, static_cast<int>(langs.size()) - 1)], f[3].checked,
                     [&](int done, int total) {
                         progress.set(L"Recognising text: page " + std::to_wstring(done + 1) + L" of " + std::to_wstring(total) + L"\nPress Esc to stop.");
                         return !progress.stopRequested();
                     });
    }
    if (r.words > 0) afterPagesChanged();
    std::wstring msg;
    if (!r.error.empty()) msg = r.error + L"\n\n";
    if (r.words > 0) msg += L"Text was recognised on " + std::to_wstring(r.pagesDone) + L" page" + (r.pagesDone == 1 ? L"" : L"s") + L" (" + std::to_wstring(r.words) + L" words). Save the document to keep the text; Undo (Ctrl+Z) takes it back.";
    else if (r.pagesSkipped == static_cast<int>(pages.size())) msg += L"The pages already have text, so there was nothing to recognise.";
    else if (r.cancelled) msg += L"Text recognition was stopped.";
    else msg += L"No text was found on those pages.";
    if (r.pagesSkipped && r.words > 0) msg += L"\n" + std::to_wstring(r.pagesSkipped) + L" page(s) already had text and were skipped.";
    MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_OK | (r.error.empty() ? MB_ICONINFORMATION : MB_ICONWARNING));
}


void openRecentMenu(POINT screenPt) {
    HMENU m = CreatePopupMenu();
    if (app.recent.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"No recent files");
    for (size_t i = 0; i < app.recent.size(); ++i) {
        std::wstring name = app.recent[i].substr(app.recent[i].find_last_of(L"\\/") + 1);
        std::wstring label = name + L"    " + app.recent[i].substr(0, app.recent[i].size() - name.size());
        if (label.size() > 90) label = name;
        AppendMenuW(m, MF_STRING, T_RECENT + i, label.c_str());
    }
    if (!app.recent.empty()) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, T_RECENT_CLEAR, L"Clear the list"); }
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, screenPt.x, screenPt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd == T_RECENT_CLEAR) { app.recent.clear(); updateEmptyText(); saveSettings(); }
    else if (cmd >= T_RECENT && cmd < T_RECENT + 10 && cmd - T_RECENT < static_cast<int>(app.recent.size())) {
        std::wstring p = app.recent[static_cast<size_t>(cmd - T_RECENT)];
        if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(app.wnd, (L"\"" + p + L"\" no longer exists, so it was removed from the list.").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
            app.recent.erase(app.recent.begin() + (cmd - T_RECENT));
            updateEmptyText();
        } else openFile(p);
    }
}

void registerAsViewer() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto set = [](const std::wstring& key, const wchar_t* name, const std::wstring& value) {
        HKEY k;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
        bool ok = RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        RegCloseKey(k);
        return ok;
    };
    bool ok = set(L"Software\\Classes\\FeatherPDF.Document", nullptr, L"PDF document");
    ok = set(L"Software\\Classes\\FeatherPDF.Document\\DefaultIcon", nullptr, std::wstring(exe) + L",0") && ok;
    ok = set(L"Software\\Classes\\FeatherPDF.Document\\shell\\open\\command", nullptr, L"\"" + std::wstring(exe) + L"\" \"%1\"") && ok;
    ok = set(L"Software\\Classes\\.pdf\\OpenWithProgids", L"FeatherPDF.Document", L"") && ok;
    if (!ok) { MessageBoxW(app.wnd, L"The file association could not be registered.", kAppName, MB_OK | MB_ICONWARNING); return; }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    if (MessageBoxW(app.wnd, L"Feather PDF now appears in the \"Open with\" list for PDF files.\n\nWindows only lets you choose the default app yourself. "
                             L"Open the Windows settings to choose Feather PDF for .pdf files?", kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES)
        ShellExecuteW(app.wnd, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
}

void startPicture() {
    app.pendingPictureWidth = 0;
    std::vector<wchar_t> buf(MAX_PATH * 2, 0);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Choose a picture to put on the page";
    ofn.lpstrFilter = L"Pictures (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return;
    app.pendingPicture = buf.data();
    if (app.tool != Viewer::Tool::Picture) setTool(Viewer::Tool::Picture);
    if (!app.pictureHintShown) {
        app.pictureHintShown = true;
        MessageBoxW(app.wnd, L"Now click on the page where the top-left corner of the picture should go. Esc cancels.", kAppName, MB_OK | MB_ICONINFORMATION);
    }
}


// ---- digital signatures ----

std::wstring sigSummary(const std::vector<sign::Status>& v) {
    std::wstring t;
    int n = 0;
    for (const auto& st : v) {
        ++n;
        const wchar_t* head = st.kind == sign::Status::Valid ? L"Valid" : st.kind == sign::Status::ValidUntrusted ? L"Signed, but not confirmed" :
                              st.kind == sign::Status::Changed ? L"NOT VALID: changed after signing" : L"Could not be checked";
        t += L"Signature " + std::to_wstring(n) + L": " + head + L"\n";
        t += L"  Signed by: " + (st.signer.empty() ? L"(unknown)" : st.signer) + L"\n";
        if (!st.time.empty()) t += L"  Time: " + st.time + L"\n";
        if (!st.reason.empty()) t += L"  Reason: " + st.reason + L"\n";
        t += L"  " + st.note + L"\n\n";
    }
    return t;
}

void showSignatures(Document& d) {
    std::vector<FormField> f(1);
    f[0].kind = FormField::Note;
    f[0].text = d.signatures().empty() ? L"This document has no digital signatures." : sigSummary(d.signatures());
    showForm(app.wnd, app.inst, L"Digital signatures", f, L"Close");
}

void makeOwnCertificate() {
    wchar_t user[256] = L""; DWORD n = 256;
    GetUserNameW(user, &n);
    std::vector<FormField> f(3);
    f[0] = {FormField::Note, L"", {}, L"Makes a certificate for signing, kept in your personal Windows certificates.\\nOther people see your signature as valid, but they must choose to trust it,\\nbecause no authority vouches for it."};
    f[1] = {FormField::Text, L"Your name:", {}, user};
    f[2] = {FormField::Text, L"E-mail (optional):", {}, L""};
    if (!showForm(app.wnd, app.inst, L"Make my own certificate", f, L"Make", [](const std::vector<FormField>& v) -> std::wstring {
            return v[1].text.empty() ? L"Type your name as it should appear in your signatures." : L""; })) return;
    std::wstring err;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    PCCERT_CONTEXT c = sign::makeOwnCertificate(f[1].text, f[2].text, err);
    SetCursor(old);
    if (!c) { MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    CertFreeCertificateContext(c);
    MessageBoxW(app.wnd, (err.empty() ? L"The certificate was made and kept in your personal certificates. You can choose it when you sign." : err.c_str()), kAppName, MB_OK | MB_ICONINFORMATION);
}

void signDocument(Document& d) {
    if (d.isProtected()) {
        MessageBoxW(app.wnd, L"Remove the password protection first (Tools > Password protection), then sign. A signed file cannot be changed afterwards.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::vector<FormField> f(7);
    f[0] = {FormField::Note, L"", {}, L"Signs the document with a certificate. The signed copy is saved as a new file, and any change made to it afterwards shows up as an invalid signature."};
    f[1] = {FormField::Choice, L"Certificate:", {L"One of my Windows certificates", L"A certificate file (.pfx / .p12)"}, L"", 0};
    f[2] = {FormField::Text, L"Reason (optional):", {}, L""};
    f[3] = {FormField::Text, L"Place (optional):", {}, L""};
    f[4] = {FormField::Choice, L"Signature on the page:", {L"Invisible", L"Visible stamp, bottom right", L"Visible stamp, bottom left", L"Visible stamp, top right", L"Visible stamp, top left"}, L"", 0};
    f[5] = {FormField::Choice, L"Time stamp (proves when you signed):", {L"None", L"DigiCert", L"Sectigo", L"GlobalSign", L"Another server (type the address below)"}, L"", 0};
    f[6] = {FormField::Text, L"Time stamp server address:", {}, L"http://"};
    if (!showForm(app.wnd, app.inst, L"Sign with a certificate", f, L"Choose certificate...", [](const std::vector<FormField>& v) -> std::wstring {
            if (v[5].choice == 4 && v[6].text.rfind(L"http", 0) != 0) return L"Type the address of the time stamp server, starting with http:// or https://.";
            return L""; })) return;

    PCCERT_CONTEXT cert = nullptr;
    HCERTSTORE fileStore = nullptr, myStore = nullptr;
    std::wstring err;
    if (f[1].choice == 0) {
        myStore = CertOpenSystemStoreW(0, L"MY");
        bool any = false;
        for (PCCERT_CONTEXT c = nullptr; myStore && (c = CertEnumCertificatesInStore(myStore, c)) != nullptr;) {
            DWORD sz = 0;
            if (CertGetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &sz)) { any = true; CertFreeCertificateContext(c); break; }
        }
        if (!any) {
            if (myStore) CertCloseStore(myStore, 0);
            MessageBoxW(app.wnd, L"You have no certificates with a private key yet. Use \"Make my own certificate\" first, or sign with a certificate file.", kAppName, MB_OK | MB_ICONINFORMATION);
            return;
        }
        cert = CryptUIDlgSelectCertificateFromStore(myStore, app.wnd, L"Sign with a certificate", L"Choose the certificate to sign with.", 0, 0, nullptr);
        if (!cert) { CertCloseStore(myStore, 0); return; }
    } else {
        wchar_t buf[MAX_PATH * 2] = L"";
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = app.wnd;
        ofn.lpstrTitle = L"Certificate with key";
        ofn.lpstrFilter = L"Certificate with key (*.pfx;*.p12)\0*.pfx;*.p12\0";
        ofn.lpstrFile = buf;
        ofn.nMaxFile = MAX_PATH * 2;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&ofn)) return;
        std::wstring pw;
        if (!askText(L"Certificate password", L"Type the password of the certificate file.", true, pw)) return;
        cert = sign::certFromPfx(buf, pw, fileStore, err);
        if (!cert) { MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    }
    auto cleanup = [&] {
        if (cert) CertFreeCertificateContext(cert);
        if (fileStore) CertCloseStore(fileStore, 0);
        if (myStore) CertCloseStore(myStore, 0);
    };

    wchar_t out[MAX_PATH * 2] = L"";
    std::wstring base = d.title();
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    wcsncpy_s(out, (base + L" (signed).pdf").c_str(), _TRUNCATE);
    OPENFILENAMEW so{sizeof(so)};
    so.hwndOwner = app.wnd;
    so.lpstrTitle = L"Save the signed document as";
    so.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0";
    so.lpstrFile = out;
    so.nMaxFile = MAX_PATH * 2;
    so.lpstrDefExt = L"pdf";
    so.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&so)) { cleanup(); return; }
    for (const auto& t : app.tabs)
        if (_wcsicmp(t->path().c_str(), out) == 0) {
            cleanup();
            MessageBoxW(app.wnd, L"That file is open in a tab. Close it first, or choose another name.", kAppName, MB_OK | MB_ICONWARNING);
            return;
        }
    std::vector<unsigned char> plain, signedBytes;
    sign::Options opt;
    opt.reason = f[2].text;
    opt.location = f[3].text;
    if (f[4].choice > 0) {
        const int page = app.viewer.currentPage();
        double cl, cb, cr, ct;
        if (d.cropBoxOf(page, cl, cb, cr, ct)) {
            const double w = 200, h = 60, m = 36;
            const bool right = f[4].choice == 1 || f[4].choice == 3, bottom = f[4].choice == 1 || f[4].choice == 2;
            opt.visible = true; opt.page = page;
            opt.left = right ? cr - m - w : cl + m; opt.right = opt.left + w;
            opt.bottom = bottom ? cb + m : ct - m - h; opt.top = opt.bottom + h;
        }
    }
    {
        static const wchar_t* servers[] = {L"", L"http://timestamp.digicert.com", L"http://timestamp.sectigo.com", L"http://timestamp.globalsign.com/tsa/r6advanced1"};
        std::wstring url = f[5].choice == 4 ? f[6].text : servers[f[5].choice];
        opt.timestampUrl.assign(url.begin(), url.end());
    }
    HCURSOR waitCursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    bool ok = d.serialize(plain, FPDF_REMOVE_SECURITY) && sign::signPdf(plain, cert, opt, signedBytes, err);
    SetCursor(waitCursor);
    cleanup();
    if (!ok) { MessageBoxW(app.wnd, (err.empty() ? L"The document could not be signed." : err).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    SaveResult r = Document::writeFile(out, signedBytes);
    if (r != SaveResult::Ok) {
        MessageBoxW(app.wnd, r == SaveResult::DiskFull ? L"The signed document could not be saved because the disk is full."
                                                       : L"The signed document could not be saved. The folder may be read-only, or the file may be open in another program.", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    if (MessageBoxW(app.wnd, L"The document was signed and saved. Open the signed copy in a new tab?", kAppName, MB_YESNO | MB_ICONINFORMATION) == IDYES) openFile(out);
}


void setFullscreen(bool on, bool presentation) {
    if (on == app.fullscreen) { if (on && presentation) app.viewer.fitPage(); return; }
    if (on) {
        GetWindowPlacement(app.wnd, &app.savedPlace);
        app.savedStyle = GetWindowLong(app.wnd, GWL_STYLE);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(app.wnd, MONITOR_DEFAULTTONEAREST), &mi);
        app.fullscreen = true;
        SetWindowLong(app.wnd, GWL_STYLE, app.savedStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(app.wnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        ShowWindow(app.sidebar.hwnd(), SW_HIDE);
        for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findCount, app.findClose}) ShowWindow(c, SW_HIDE);
    } else {
        app.fullscreen = false;
        SetWindowLong(app.wnd, GWL_STYLE, app.savedStyle);
        SetWindowPlacement(app.wnd, &app.savedPlace);
        SetWindowPos(app.wnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        showControls();
    }
    relayoutChildren();
    InvalidateRect(app.wnd, nullptr, TRUE);
    if (on) { if (presentation) app.viewer.fitPage(); else app.viewer.fitWidth(); }
    else if (activeDoc()) app.viewer.fitWidth();
}

void startStamp(const std::wstring& text, COLORREF col) {
    app.pendingStamp = text;
    app.pendingStampColour = col;
    if (app.tool != Viewer::Tool::Stamp) setTool(Viewer::Tool::Stamp);
    MessageBoxW(app.wnd, L"Now click on the page where the stamp should go (its centre lands on the click). Esc cancels.", kAppName, MB_OK | MB_ICONINFORMATION);
}

void notesSummary(Document& d) {
    std::wstring md = annot::notesSummary(d);
    if (md.empty()) { MessageBoxW(app.wnd, L"This document has no highlights or comments yet.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    wchar_t buf[MAX_PATH * 2] = L"";
    std::wstring base = d.title();
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    wcsncpy_s(buf, (base + L" (notes).md").c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Save the notes as";
    ofn.lpstrFilter = L"Markdown files (*.md)\0*.md\0Text files (*.txt)\0*.txt\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, md.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, md.c_str(), -1, u.data(), n, nullptr, nullptr);
    std::vector<unsigned char> bytes{0xEF, 0xBB, 0xBF};
    bytes.insert(bytes.end(), u.begin(), u.end());
    if (Document::writeFile(buf, bytes) != SaveResult::Ok) {
        MessageBoxW(app.wnd, L"The notes could not be saved. The folder may be read-only, or the disk may be full.", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    if (MessageBoxW(app.wnd, L"The notes were saved. Open the file?", kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES) ShellExecuteW(app.wnd, L"open", buf, nullptr, nullptr, SW_SHOWNORMAL);
}

static const wchar_t* kRateNames[] = {L"Slow", L"Normal", L"Fast", L"Very fast"};
static const int kRates[] = {-4, 0, 3, 6};
static const wchar_t* kStampNames[] = {L"APPROVED", L"FINAL", L"DRAFT", L"CONFIDENTIAL", L"NOT APPROVED", L"FOR COMMENT", L"REVIEWED"};
static const COLORREF kStampCols[] = {RGB(0x2E, 0x9E, 0x4A), RGB(0x25, 0x6F, 0xD1), RGB(0xE0, 0x8A, 0x00), RGB(0xD0, 0x2B, 0x2B), RGB(0xD0, 0x2B, 0x2B), RGB(0x25, 0x6F, 0xD1), RGB(0x2E, 0x9E, 0x4A)};

void showStampMenu(HMENU parent) {
    HMENU st = CreatePopupMenu();
    for (int i = 0; i < 7; ++i) AppendMenuW(st, MF_STRING, T_STAMP + i, kStampNames[i]);
    AppendMenuW(st, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(st, MF_STRING, T_STAMP_CUSTOM, L"Custom stamp...");
    AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(st), L"Stamp");
}

void toolsDispatch(int cmd) {
    Document* d = activeDoc();
    if (!d) return;
    switch (cmd) {
        case T_WATERMARK: doWatermark(*d); break;
        case T_NUMBERS: doPageNumbers(*d); break;
        case T_EXPORT: doExport(*d); break;
        case T_COMPRESS: doCompress(*d); break;
        case T_OCR: doOcr(*d); break;
        case T_NOTES: notesSummary(*d); break;
        case T_STAMP_CUSTOM: {
            std::wstring t;
            if (askText(L"Custom stamp", L"The word or words on the stamp (for example PAID).", false, t, L"") && !t.empty()) {
                if (!pageobj::isLatin(t)) MessageBoxW(app.wnd, L"A stamp can use letters, numbers and common symbols of Western languages.", kAppName, MB_OK | MB_ICONINFORMATION);
                else startStamp(t, RGB(0x25, 0x6F, 0xD1));
            }
            break;
        }
        case T_SIGN: signDocument(*d); break;
        case T_OWNCERT: makeOwnCertificate(); break;
        case T_SHOWSIGS: showSignatures(*d); break;
        case T_PROTECT: doProtect(*d); break;
        case T_UNPROTECT: doUnprotect(*d); break;
        case T_EDIT_TEXT: setTool(Viewer::Tool::EditText); break;
        case N_EDITPARA: setTool(Viewer::Tool::EditPara); break;
        case N_SELECTOBJ: setTool(Viewer::Tool::Select); break;
        case T_ADD_TEXT: setTool(Viewer::Tool::AddText); break;
        case T_TEXT_STYLE: textStyleDialog(); break;
        case T_READ: readAloud(); break;
        case T_PICTURE: startPicture(); break;
        case N_FINDREPLACE: doFindReplace(); break;
        case N_TABLE_INS: tableInsert(); break;
        case N_TABLE_EDIT: tableEdit(); break;
        case N_TABLE_DEL: tableDelete(); break;
        case N_BOOKMARK: addBookmarkHere(); break;
        case N_BW: doGreyscale(); break;
        case N_BATCH: batchDialog(); break;
        case F_EXPORT: formExport(); break;
        case F_FILL: formFillFromCsv(); break;
        case F_TEXT: startFormField(Viewer::Tool::FormText); break;
        case F_CHECK: startFormField(Viewer::Tool::FormCheck); break;
        case F_COMBO: startFormField(Viewer::Tool::FormCombo); break;
        case F_AUTO: formAutoDetect(); break;
        default:
            if (cmd >= T_STAMP && cmd < T_STAMP + 7) { startStamp(kStampNames[cmd - T_STAMP], kStampCols[cmd - T_STAMP]); break; }
            if ((cmd >= T_RED_AREA && cmd <= T_RED_APPLY) || cmd == N_RED_CHECK) { redactionMenuAction(*d, cmd); break; }
            if (cmd >= P_DELETE && cmd <= P_NUP) { pagesMenuAction(cmd, {}); break; }
            break;
    }
}

void showToolsMenu(const RECT& anchor) {
    Document* d = activeDoc();
    if (!d) return;
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* t, bool on = true, bool check = false) { AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED) | (check ? MF_CHECKED : 0), id, t); };
    auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    add(T_EDIT_TEXT, L"Edit text (one line)\tCtrl+E", true, app.tool == Viewer::Tool::EditText);
    add(N_EDITPARA, L"Edit paragraph (font, size, bold, italic)\tF2", true, app.tool == Viewer::Tool::EditPara);
    add(N_SELECTOBJ, L"Move or resize pictures, paragraphs and tables", true, app.tool == Viewer::Tool::Select);
    add(N_FINDREPLACE, L"Find and replace text...\tCtrl+Shift+H");
    add(N_TABLE_INS, L"Insert table...");
    add(N_TABLE_EDIT, L"Edit a table (text, rows, width)...");
    add(N_TABLE_DEL, L"Delete table", true, app.tool == Viewer::Tool::TableDelete);
    sep();
    add(T_ADD_TEXT, L"Add a text box", true, app.tool == Viewer::Tool::AddText);
    add(T_TEXT_STYLE, L"Style of new text...");
    add(T_NOTES, L"Notes summary (highlights and comments)...");
    add(N_BOOKMARK, L"Add a bookmark for this page...");
    sep();
    add(T_OCR, L"Recognise text (OCR)...");
    add(T_WATERMARK, L"Watermark...");
    add(T_NUMBERS, L"Page numbers...");
    sep();
    HMENU frm = CreatePopupMenu();
    AppendMenuW(frm, MF_STRING, F_EXPORT, L"Export the answers to Excel or CSV...");
    AppendMenuW(frm, MF_STRING, F_FILL, L"Fill from a spreadsheet (CSV)...");
    AppendMenuW(frm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormText ? MF_CHECKED : 0), F_TEXT, L"Add a text field");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormCheck ? MF_CHECKED : 0), F_CHECK, L"Add a check box");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormCombo ? MF_CHECKED : 0), F_COMBO, L"Add a drop-down list");
    AppendMenuW(frm, MF_STRING, F_AUTO, L"Find fields in a flat form...");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(frm), L"Fillable form");
    HMENU red = CreatePopupMenu();
    AppendMenuW(red, MF_STRING | (app.tool == Viewer::Tool::Redact ? MF_CHECKED : 0), T_RED_AREA, L"Mark areas to redact");
    AppendMenuW(red, MF_STRING, T_RED_SEL, L"Mark selected text");
    AppendMenuW(red, MF_STRING, T_RED_FIND, L"Find and mark...");
    AppendMenuW(red, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(red, d->redactions.empty() ? MF_GRAYED : MF_STRING, T_RED_APPLY, L"Apply redactions...");
    AppendMenuW(red, d->redactions.empty() ? MF_GRAYED : MF_STRING, T_RED_CLEAR, L"Clear marks");
    AppendMenuW(red, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(red, MF_STRING, N_RED_CHECK, L"Check that the text is gone...");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(red), L"Redact");
    add(N_BATCH, L"Batch: process a whole folder...");
    add(T_COMPRESS, L"Compress (make the file smaller)...");
    add(N_BW, L"Black and white / greyscale pictures...");
    add(T_PROTECT, L"Password protection...");
    if (d->isProtected()) add(T_UNPROTECT, L"Remove password protection");
    {
        HMENU sg = CreatePopupMenu();
        AppendMenuW(sg, MF_STRING, T_SIGN, L"Sign with a certificate...");
        AppendMenuW(sg, MF_STRING, T_OWNCERT, L"Make my own certificate...");
        std::wstring label = d->signatures().empty() ? L"Show signatures (none in this document)" : L"Show the " + std::to_wstring(d->signatures().size()) + L" signature(s) of this document";
        AppendMenuW(sg, d->signatures().empty() ? MF_GRAYED : MF_STRING, T_SHOWSIGS, label.c_str());
        AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(sg), L"Digital signatures");
    }
    sep();
    HMENU pg = CreatePopupMenu();
    auto padd = [&](UINT id, const wchar_t* t, bool on = true) { AppendMenuW(pg, MF_STRING | (on ? 0 : MF_GRAYED), id, t); };
    padd(P_DELETE, L"Delete pages...", d->pageCount() > 1);
    padd(P_BLANK, L"Insert a blank page after this one");
    padd(P_INSERT, L"Insert pages from a file...");
    padd(P_MERGE, L"Merge PDFs into the end of this document...");
    padd(P_NEWPDF, L"New PDF from pictures...");
    AppendMenuW(pg, MF_SEPARATOR, 0, nullptr);
    padd(P_ROT_R, L"Rotate this page right");
    padd(P_ROT_L, L"Rotate this page left");
    padd(P_ROT_ALL, L"Rotate all pages right");
    padd(P_MOVE, L"Move pages...", d->pageCount() > 1);
    AppendMenuW(pg, MF_SEPARATOR, 0, nullptr);
    padd(P_CROP, L"Crop pages...");
    padd(P_CROPBOX, L"Crop by drawing a box");
    AppendMenuW(pg, MF_SEPARATOR, 0, nullptr);
    padd(P_EXTRACT, L"Extract pages into a new file...");
    padd(P_SPLIT, L"Split into several files...");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(pg), L"Edit pages");
    sep();
    add(T_EXPORT, L"Export as pictures, text, Word or Excel...");
    HMENU rate = CreatePopupMenu();
    add(T_READ, speech::speaking() ? L"Stop reading aloud\tCtrl+Shift+U" : L"Read aloud\tCtrl+Shift+U");
    for (int i = 0; i < 4; ++i) AppendMenuW(rate, MF_STRING | (app.readRate == kRates[i] ? MF_CHECKED : 0), T_RATE + i, kRateNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(rate), L"Reading speed");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd >= T_RATE && cmd < T_RATE + 4) { app.readRate = kRates[cmd - T_RATE]; if (speech::speaking()) { stopReading(); readAloud(); } return; }
    if (cmd) toolsDispatch(cmd);
}

void updateFindCount() {
    wchar_t t[64] = L"";
    if (GetWindowTextLengthW(app.findEdit) > 0) {
        if (app.matchTotal == 0) wcscpy_s(t, L"No results");
        else swprintf(t, 64, L"%d of %d", app.curMatch + 1, app.matchTotal);
    }
    SetWindowTextW(app.findCount, t);
}

void runSearch(bool keepPosition) {
    Document* d = activeDoc();
    wchar_t q[512];
    GetWindowTextW(app.findEdit, q, 512);
    if (!d || !q[0]) {
        app.viewer.setMatches({}, -1);
        app.curMatch = -1; app.matchTotal = 0;
        updateFindCount();
        return;
    }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::vector<TextHit> hits = d->find(q, SendMessageW(app.findCase, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetCursor(old);
    app.matchTotal = static_cast<int>(hits.size());
    int first = 0;   // first match on or after the page being read
    int page = app.viewer.currentPage();
    for (size_t i = 0; i < hits.size(); ++i) if (hits[i].page >= page) { first = static_cast<int>(i); break; }
    if (keepPosition && app.curMatch >= 0 && app.curMatch < app.matchTotal) first = app.curMatch;
    app.curMatch = hits.empty() ? -1 : first;
    app.viewer.setMatches(std::move(hits), app.curMatch);
    updateFindCount();
}

void stepMatch(int dir) {
    if (app.matchTotal == 0) {
        if (!app.findVisible) return;
        runSearch(false);
        return;
    }
    app.curMatch = (app.curMatch + dir + app.matchTotal) % app.matchTotal;
    app.viewer.setCurrentMatch(app.curMatch);
    updateFindCount();
}

void toggleFind(bool show) {
    app.findVisible = show;
    showControls();
    if (show) {
        SetFocus(app.findEdit);
        SendMessageW(app.findEdit, EM_SETSEL, 0, -1);
        if (GetWindowTextLengthW(app.findEdit) > 0) runSearch(true);
    } else {
        app.viewer.setMatches({}, -1);
        app.curMatch = -1; app.matchTotal = 0;
        SetFocus(app.viewer.hwnd());
    }
}

void showSidebar(Sidebar::Mode m) {
    if (app.sidebarVisible && app.sidebar.mode() == m) app.sidebarVisible = false;
    else { app.sidebarVisible = true; app.sidebar.setMode(m); }
    showControls();
    app.viewer.fitWidth();   // keep fit-width tabs filling the narrower view
    if (activeDoc()) app.sidebar.setCurrentPage(app.viewer.currentPage());
}

void goToPageDialog() {
    Document* d = activeDoc();
    if (!d) return;
    wchar_t prompt[96];
    swprintf(prompt, 96, L"Type a page number from 1 to %d.", d->pageCount());
    wchar_t cur[16];
    swprintf(cur, 16, L"%d", app.viewer.currentPage() + 1);
    std::wstring in;
    while (askText(L"Go to page", prompt, false, in, cur)) {
        wchar_t* end = nullptr;
        long n = wcstol(in.c_str(), &end, 10);
        if (end != in.c_str() && *end == 0 && n >= 1 && n <= d->pageCount()) { app.viewer.goToPage(static_cast<int>(n) - 1); return; }
        MessageBoxW(app.wnd, prompt, kAppName, MB_OK | MB_ICONINFORMATION);
    }
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
        case C_FIND: if (d) toggleFind(!app.findVisible); break;
        case C_PANEL: showSidebar(Sidebar::Mode::Bookmarks); break;
        case C_GOTO: goToPageDialog(); break;
        case C_SAVE: saveActive(); break;
        case C_UNDO: undoRedo(false); break;
        case C_REDO: undoRedo(true); break;
    }
}

// ---- theme ----
bool systemUsesDark() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v == 0;
}

void applyTheme() {
    const bool dark = app.theme == 1 || (app.theme == 2 && systemUsesDark());
    pal() = dark ? darkPalette() : lightPalette();
    if (app.ctlBrush) DeleteObject(app.ctlBrush);
    app.ctlBrush = CreateSolidBrush(pal().toolbar);
    BOOL on = dark;
    DwmSetWindowAttribute(app.wnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &on, sizeof(on));
    const wchar_t* cls = dark ? L"DarkMode_Explorer" : L"Explorer";
    for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findClose, app.findCase}) if (c) SetWindowTheme(c, cls, nullptr);
    app.sidebar.applyTheme();
    RedrawWindow(app.wnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
}

// ---- About and Settings ----
void showAbout() {
    std::vector<FormField> f(1);
    f[0] = {FormField::Note, L"", {}, L"Feather PDF\nVersion 2.13.0\n\nA fast, lightweight PDF reader and editor for Windows. Read, search, print and present documents in tabs; change and replace text, fill in forms, sign, add comments, highlights, drawings, stamps, pictures and tables; add watermarks, page numbers, bookmarks and links; reorganise, merge, split and extract pages; compress, redact, password-protect, recognise text (OCR), compare and measure; export pages as pictures, text, Word or Excel. Everything happens on your PC, with no account, cloud service or tracking.\n\nCreated by: Akshaya Simha\n\nFree to use and share, always. No subscription, no trial, no licence key.\nPDF rendering by PDFium; text recognition by Tesseract (see the licenses folder).\n\nBuilt with care for a faster, simpler document experience."};
    showForm(app.wnd, app.inst, L"About Feather PDF", f, L"!Close");
}

void showSettings() {
    std::vector<FormField> f(6);
    f[0] = {FormField::Choice, L"Window theme:", {L"Light", L"Dark", L"Like Windows"}, L"", app.theme};
    f[1] = {FormField::Choice, L"Page colours:", {L"Normal", L"Dark (night mode)", L"Dimmed (softer white)"}, L"", static_cast<int>(app.viewer.colours())};
    f[2] = {FormField::Choice, L"Highlight colour:", {L"Yellow", L"Green", L"Blue", L"Pink", L"Orange", L"Red", L"Purple"}, L"", app.hlColour};
    f[3] = {FormField::Choice, L"Drawing colour:", {L"Yellow", L"Green", L"Blue", L"Pink", L"Orange", L"Red", L"Purple"}, L"", app.drawColour};
    f[4] = {FormField::Choice, L"Line width:", {L"Thin", L"Medium", L"Thick"}, L"", app.widthIdx};
    f[5] = {FormField::Choice, L"Reading speed:", {L"Slow", L"Normal", L"Fast", L"Very fast"}, L"", app.readRate <= -4 ? 0 : app.readRate == 0 ? 1 : app.readRate <= 3 ? 2 : 3};
    if (!showForm(app.wnd, app.inst, L"Settings", f, L"Save")) return;
    static const int rates[] = {-4, 0, 3, 6};
    app.theme = f[0].choice;
    app.viewer.setColours(static_cast<Viewer::Colours>(f[1].choice));
    app.hlColour = f[2].choice; app.drawColour = f[3].choice; app.widthIdx = f[4].choice; app.readRate = rates[f[5].choice];
    applyTheme();
    saveSettings();
}

// ---- page clipboard and the thumbnail menu ----
std::vector<unsigned char> g_pageClip;

void copyPagesToClip(Document& d, const std::vector<int>& pages) {
    std::vector<unsigned char> b;
    if (d.pagesToBytes(pages, b)) g_pageClip = std::move(b);
    else MessageBoxW(app.wnd, L"The pages could not be copied.", kAppName, MB_OK | MB_ICONWARNING);
}

void pastePages(Document& d, int at) {
    if (g_pageClip.empty()) return;
    Document src;
    std::vector<unsigned char> copy = g_pageClip;
    if (src.load(std::move(copy), {}) != OpenResult::Ok) return;
    std::vector<int> all(src.pageCount());
    for (int i = 0; i < src.pageCount(); ++i) all[i] = i;
    if (d.importPages(src, all, at)) { afterPagesChanged(); app.viewer.goToPage(at); }
    else MessageBoxW(app.wnd, L"The pages could not be pasted.", kAppName, MB_OK | MB_ICONWARNING);
}

void deleteThumbPages(Document& d, const std::vector<int>& pages) {
    if (pages.empty()) return;
    if (static_cast<int>(pages.size()) >= d.pageCount()) {
        MessageBoxW(app.wnd, L"A document needs at least one page, so not every page can be deleted.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (d.deletePages(pages)) afterPagesChanged();
}

void showThumbMenu(int clicked, POINT pt) {
    Document* d = activeDoc();
    if (!d) return;
    std::vector<int> pages = app.sidebar.selectedPages();
    if (pages.empty()) pages = {clicked};
    const bool many = pages.size() > 1;
    const bool clip = !g_pageClip.empty();
    enum { M_DEL = 1, M_ROTR, M_ROTL, M_COPY, M_CUT, M_PBEFORE, M_PAFTER, M_PSTART, M_PEND, M_BLANK, M_BLANKB, M_INSERT, M_MERGE, M_CROP, M_EXTRACT, M_ALL };
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* t, bool on = true) { AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED), id, t); };
    auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    add(M_DEL, many ? L"Delete pages\tDel" : L"Delete page\tDel", static_cast<int>(pages.size()) < d->pageCount());
    add(M_ROTR, L"Rotate clockwise");
    add(M_ROTL, L"Rotate counterclockwise");
    sep();
    add(M_COPY, many ? L"Copy pages\tCtrl+C" : L"Copy page\tCtrl+C");
    add(M_CUT, many ? L"Cut pages\tCtrl+X" : L"Cut page\tCtrl+X", static_cast<int>(pages.size()) < d->pageCount());
    add(M_PAFTER, L"Paste pages after\tCtrl+V", clip);
    add(M_PBEFORE, L"Paste pages before", clip);
    add(M_PSTART, L"Paste pages at the start", clip);
    add(M_PEND, L"Paste pages at the end", clip);
    sep();
    add(M_BLANK, L"Insert blank page after");
    add(M_BLANKB, L"Insert blank page before");
    add(M_INSERT, L"Insert pages from file...");
    add(M_MERGE, L"Merge PDFs into this document...");
    sep();
    add(M_CROP, L"Crop pages...");
    sep();
    add(M_EXTRACT, L"Extract pages...");
    add(M_ALL, L"Select all pages\tCtrl+A");
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    const int first = pages.front(), last = pages.back();
    switch (cmd) {
        case M_DEL: deleteThumbPages(*d, pages); break;
        case M_ROTR: if (d->rotatePages(pages, 1)) afterPagesChanged(); break;
        case M_ROTL: if (d->rotatePages(pages, -1)) afterPagesChanged(); break;
        case M_COPY: copyPagesToClip(*d, pages); break;
        case M_CUT: copyPagesToClip(*d, pages); deleteThumbPages(*d, pages); break;
        case M_PAFTER: pastePages(*d, last + 1); break;
        case M_PBEFORE: pastePages(*d, first); break;
        case M_PSTART: pastePages(*d, 0); break;
        case M_PEND: pastePages(*d, d->pageCount()); break;
        case M_BLANK: if (d->insertBlankPage(last, last)) { afterPagesChanged(); app.viewer.goToPage(last + 1); } break;
        case M_BLANKB: if (d->insertBlankPage(first - 1, first)) { afterPagesChanged(); app.viewer.goToPage(first); } break;
        case M_INSERT: case M_MERGE: case M_CROP: case M_EXTRACT: {
            // reuse the dialogs of the Pages menu with the selection as the default
            app.viewer.goToPage(first);
            pagesMenuAction(cmd == M_INSERT ? P_INSERT : cmd == M_MERGE ? P_MERGE : cmd == M_CROP ? P_CROP : P_EXTRACT, pages);
            break;
        }
        case M_ALL: app.sidebar.selectAll(); break;
    }
}

void onThumbReorder(const std::vector<int>& pages, int before) {
    Document* d = activeDoc();
    if (d && d->movePages(pages, before)) afterPagesChanged();
}

// ---- the main ("...") menu ----
void showMainMenu(const RECT& anchor) {
    Document* d = activeDoc();
    enum { MM_OPEN = 1, MM_SAVE, MM_SAVEAS, MM_EXPORT, MM_CLOSE, MM_UNDO, MM_REDO, MM_PRINT, MM_PROPS, MM_READ, MM_COPYPAGE, MM_SELALL,
           MM_FIRST, MM_LAST, MM_GOTO, MM_BM, MM_THUMBS, MM_FITW, MM_FITP, MM_ACTUAL, MM_FULL, MM_PRESENT, MM_COL, MM_THEME = MM_COL + 3,
           MM_SETTINGS = MM_THEME + 3, MM_DEFAULT, MM_ABOUT, MM_EXIT, MM_NEWPDF, MM_RELOAD, MM_SIGN, MM_COPYAREA, MM_BACK, MM_FWD, MM_ROTL, MM_ROTR, MM_PALETTE, MM_COMPARE, MM_SEARCHDIR, MM_BATCH, MM_SBSTAB, MM_SBSOPEN, MM_SBSOFF, MM_LAYOUT, MM_RECENT = 100 };
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* t, bool on = true, bool check = false) {
        AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED) | (check ? MF_CHECKED : 0), id, t); };
    auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    const bool has = d != nullptr;
    add(MM_OPEN, L"Open...\tCtrl+O");
    add(MM_NEWPDF, L"New PDF from pictures...");
    add(MM_SEARCHDIR, L"Search all my PDFs in a folder...");
    add(MM_BATCH, L"Batch: process a whole folder...");
    HMENU rec = CreatePopupMenu();
    for (size_t i = 0; i < app.recent.size(); ++i) {
        size_t s = app.recent[i].find_last_of(L"\\/");
        AppendMenuW(rec, MF_STRING, MM_RECENT + i, (s == std::wstring::npos ? app.recent[i] : app.recent[i].substr(s + 1)).c_str());
    }
    if (app.recent.empty()) AppendMenuW(rec, MF_STRING | MF_GRAYED, 0, L"(none)");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(rec), L"Open recent");
    add(MM_SAVE, L"Save\tCtrl+S", has && d->dirty());
    add(MM_SAVEAS, L"Save as...\tCtrl+Shift+S", has);
    add(MM_EXPORT, L"Export as pictures, text, Word or Excel...", has);
    {
        HMENU cm = CreatePopupMenu();
        AppendMenuW(cm, MF_STRING | (has && app.tabs.size() >= 2 ? 0 : MF_GRAYED), MM_COMPARE, L"Another open tab (compare the text)...");
        AppendMenuW(m, MF_POPUP | (has && app.tabs.size() >= 2 ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(cm), L"Compare with");
        HMENU sb = CreatePopupMenu();
        AppendMenuW(sb, MF_STRING | (has && app.tabs.size() >= 2 ? 0 : MF_GRAYED), MM_SBSTAB, L"Another open tab");
        AppendMenuW(sb, MF_STRING | (has ? 0 : MF_GRAYED), MM_SBSOPEN, L"Open a file next to this one...");
        AppendMenuW(sb, MF_STRING | (app.sbs ? 0 : MF_GRAYED), MM_SBSOFF, L"Back to one document");
        AppendMenuW(m, MF_POPUP | (has ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(sb), L"Side by side (two in one tab)");
    }
    add(MM_CLOSE, L"Close tab\tCtrl+W", has);
    sep();
    add(MM_UNDO, L"Undo\tCtrl+Z", has && d->canUndo());
    add(MM_REDO, L"Redo\tCtrl+Y", has && d->canRedo());
    sep();
    add(MM_PRINT, L"Print...\tCtrl+P", has);
    add(MM_PROPS, L"Document properties...\tCtrl+D", has);
    add(MM_RELOAD, L"Reload from disk", has && !d->path().empty());
    add(MM_READ, L"Read aloud\tCtrl+Shift+U", has);
    add(MM_SIGN, L"Sign with a certificate...", has);
    sep();
    add(MM_SELALL, L"Select all\tCtrl+A", has);
    add(MM_COPYPAGE, L"Copy page as image", has);
    add(MM_COPYAREA, L"Copy area as image", has);
    sep();
    add(MM_FIRST, L"First page\tHome", has);
    add(MM_LAST, L"Last page\tEnd", has);
    add(MM_GOTO, L"Go to page...\tCtrl+G", has);
    add(MM_BACK, L"Back\tAlt+Left", has && !app.navBack.empty());
    add(MM_FWD, L"Forward\tAlt+Right", has && !app.navFwd.empty());
    sep();
    add(MM_BM, L"Bookmarks panel\tCtrl+B", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::Bookmarks);
    add(MM_THUMBS, L"Thumbnails panel\tCtrl+Shift+B", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::Pages);
    {
        HMENU lay = CreatePopupMenu();
        AppendMenuW(lay, MF_STRING | (app.layout == 0 ? MF_CHECKED : 0), MM_LAYOUT + 0, L"Single page");
        AppendMenuW(lay, MF_STRING | (app.layout == 1 ? MF_CHECKED : 0), MM_LAYOUT + 1, L"Continuous");
        AppendMenuW(lay, MF_STRING | (app.layout == 2 ? MF_CHECKED : 0), MM_LAYOUT + 2, L"Two pages");
        AppendMenuW(lay, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(lay, MF_STRING | (app.layout == 2 ? 0 : MF_GRAYED) | (app.cover ? MF_CHECKED : 0), MM_LAYOUT + 3, L"Show cover page separately");
        AppendMenuW(m, MF_POPUP | (has ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(lay), L"Page layout");
    }
    add(MM_ROTL, L"Rotate left\tCtrl+L", has);
    add(MM_ROTR, L"Rotate right\tCtrl+R", has);
    sep();
    add(MM_FITW, L"Fit width\tCtrl+2", has);
    add(MM_FITP, L"Fit page\tCtrl+0", has);
    add(MM_ACTUAL, L"Actual size\tCtrl+1", has);
    add(MM_FULL, L"Full screen\tF11", has);
    add(MM_PRESENT, L"Presentation\tF5", has);
    HMENU col = CreatePopupMenu();
    static const wchar_t* colNames[] = {L"Normal", L"Dark (night mode)", L"Dimmed (softer white)"};
    for (int i = 0; i < 3; ++i) AppendMenuW(col, MF_STRING | (static_cast<int>(app.viewer.colours()) == i ? MF_CHECKED : 0), MM_COL + i, colNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(col), L"Page colours");
    HMENU th = CreatePopupMenu();
    static const wchar_t* thNames[] = {L"Light", L"Dark", L"Like Windows"};
    for (int i = 0; i < 3; ++i) AppendMenuW(th, MF_STRING | (app.theme == i ? MF_CHECKED : 0), MM_THEME + i, thNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(th), L"Theme");
    sep();
    add(MM_PALETTE, L"Command palette...\tCtrl+K");
    add(MM_SETTINGS, L"Settings...");
    add(MM_DEFAULT, L"Set as default PDF viewer...");
    add(MM_ABOUT, L"About Feather PDF");
    add(MM_EXIT, L"Exit\tCtrl+Q");
    POINT pt{anchor.right, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
        case MM_OPEN: runCommand(C_OPEN); break;
        case MM_SAVE: saveActive(); break;
        case MM_SAVEAS: if (d) saveAsDialog(*d); break;
        case MM_EXPORT: if (d) doExport(*d); break;
        case MM_CLOSE: closeTab(app.active); break;
        case MM_UNDO: undoRedo(false); break;
        case MM_REDO: undoRedo(true); break;
        case MM_PRINT: printDocument(); break;
        case MM_PROPS: showProperties(); break;
        case MM_READ: readAloud(); break;
        case MM_SELALL: app.viewer.selectAll(); break;
        case MM_COPYPAGE:
            if (d && !pageobj::copyPageToClipboard(*d, app.viewer.currentPage(), app.wnd))
                MessageBoxW(app.wnd, L"The page could not be copied as a picture (it may be too large).", kAppName, MB_OK | MB_ICONWARNING);
            break;
        case MM_FIRST: app.viewer.goToPage(0); break;
        case MM_LAST: if (d) app.viewer.goToPage(d->pageCount() - 1); break;
        case MM_GOTO: runCommand(C_GOTO); break;
        case MM_BM: showSidebar(Sidebar::Mode::Bookmarks); break;
        case MM_THUMBS: showSidebar(Sidebar::Mode::Pages); break;
        case MM_FITW: runCommand(C_FITW); break;
        case MM_FITP: runCommand(C_FITP); break;
        case MM_ACTUAL: runCommand(C_ACTUAL); break;
        case MM_FULL: setFullscreen(true, false); break;
        case MM_PRESENT: setFullscreen(true, true); break;
        case MM_NEWPDF: newPdfFromPictures(); break;
        case MM_SEARCHDIR: searchFolder(); break;
        case MM_BATCH: batchDialog(); break;
        case MM_RELOAD: reloadFromDisk(); break;
        case MM_SIGN: toolsDispatch(T_SIGN); break;
        case MM_COPYAREA: startCopyArea(); break;
        case MM_BACK: navBack(); break;
        case MM_FWD: navForward(); break;
        case MM_ROTL: rotateView(-1); break;
        case MM_ROTR: rotateView(1); break;
        case MM_PALETTE: commandPalette(); break;
        case MM_COMPARE: compareDocs(); break;
        case MM_SBSTAB: sideBySideAction(N_SBS_TAB); break;
        case MM_SBSOPEN: sideBySideAction(N_SBS_OPEN); break;
        case MM_SBSOFF: sideBySideAction(N_SBS_OFF); break;
        case MM_SETTINGS: showSettings(); break;
        case MM_DEFAULT: registerAsViewer(); break;
        case MM_ABOUT: showAbout(); break;
        case MM_EXIT: PostMessageW(app.wnd, WM_CLOSE, 0, 0); break;
        default:
            if (cmd >= MM_LAYOUT && cmd < MM_LAYOUT + 4) setPageLayout(cmd - MM_LAYOUT);
            else if (cmd >= MM_COL && cmd < MM_COL + 3) app.viewer.setColours(static_cast<Viewer::Colours>(cmd - MM_COL));
            else if (cmd >= MM_THEME && cmd < MM_THEME + 3) { app.theme = cmd - MM_THEME; applyTheme(); saveSettings(); }
            else if (cmd >= MM_RECENT && cmd < MM_RECENT + 100 && static_cast<size_t>(cmd - MM_RECENT) < app.recent.size()) openFile(app.recent[cmd - MM_RECENT]);
    }
}

// ---- opening documents that did not come from a file ----
void addDocumentTab(std::unique_ptr<Document> doc) {
    Document* raw = doc.get();
    doc->onFormInvalidate = [raw](int page) { if (activeDoc() == raw) app.viewer.invalidatePage(page); };
    doc->onFormChanged = [raw]() { if (activeDoc() == raw) { refresh(); app.sidebar.refresh(); } else InvalidateRect(app.wnd, nullptr, FALSE); };
    app.tabs.push_back(std::move(doc));
    switchTab(static_cast<int>(app.tabs.size()) - 1);
}


// ---- session: the open tabs and their unsaved changes are kept in %APPDATA%\FeatherPDF\session while the app runs ----
std::wstring sessionDir() {
    std::wstring s = settingsPath();
    size_t p = s.find_last_of(L'\\');
    return (p == std::wstring::npos ? L"" : s.substr(0, p)) + L"\\session";
}

void clearSession() {
    const std::wstring dir = sessionDir();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileW((dir + L"\\" + fd.cFileName).c_str()); } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

void writeSession() {
    if (app.tabs.empty()) { clearSession(); return; }
    const std::wstring dir = sessionDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    static std::map<Document*, std::pair<int, int>> written;   // document -> revision on disk, recovery file number
    static int nextId = 1;
    std::set<std::wstring> keep;
    std::wstring ini;
    int n = 0;
    for (auto& t : app.tabs) {
        Document& d = *t;
        std::wstring rec;
        if (d.dirty() && !d.isProtected()) {
            auto it = written.find(&d);
            if (it == written.end()) it = written.emplace(&d, std::make_pair(-1, nextId++)).first;
            rec = L"rec_" + std::to_wstring(it->second.second) + L".pdf";
            if (it->second.first != d.revision()) {
                std::vector<unsigned char> bytes;
                if (d.serialize(bytes) && Document::writeFile(dir + L"\\" + rec, bytes) == SaveResult::Ok) it->second.first = d.revision();
                else rec.clear();
            }
            if (!rec.empty()) keep.insert(rec);
        }
        wchar_t sec[32], v[64];
        swprintf(sec, 32, L"tab%d", n++);
        const std::wstring ps = (dir + L"\\session.ini");
        WritePrivateProfileStringW(sec, L"path", d.path().c_str(), ps.c_str());
        WritePrivateProfileStringW(sec, L"name", d.title().c_str(), ps.c_str());
        WritePrivateProfileStringW(sec, L"recovery", rec.c_str(), ps.c_str());
        swprintf(v, 64, L"%.4f", d.zoom); WritePrivateProfileStringW(sec, L"zoom", v, ps.c_str());
        WritePrivateProfileStringW(sec, L"scrollY", std::to_wstring(d.scrollY).c_str(), ps.c_str());
        WritePrivateProfileStringW(sec, L"fit", d.fitWidth ? L"1" : L"0", ps.c_str());
    }
    WritePrivateProfileStringW(L"meta", L"count", std::to_wstring(n).c_str(), (dir + L"\\session.ini").c_str());
    // remove tab sections and recovery files that no longer apply
    for (int k = n; k < n + 20; ++k) { wchar_t sec[32]; swprintf(sec, 32, L"tab%d", k); WritePrivateProfileStringW(sec, nullptr, nullptr, (dir + L"\\session.ini").c_str()); }
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\rec_*.pdf").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!keep.count(fd.cFileName)) DeleteFileW((dir + L"\\" + fd.cFileName).c_str()); } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    for (auto it = written.begin(); it != written.end();) it = std::any_of(app.tabs.begin(), app.tabs.end(), [&](auto& t) { return t.get() == it->first; }) ? std::next(it) : written.erase(it);
}

bool restoreSession() {
    const std::wstring dir = sessionDir(), ini = dir + L"\\session.ini";
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    const int count = static_cast<int>(GetPrivateProfileIntW(L"meta", L"count", 0, ini.c_str()));
    struct Entry { std::wstring path, name, rec; double zoom; int scrollY; bool fit; };
    std::vector<Entry> list;
    int withChanges = 0;
    for (int i = 0; i < count; ++i) {
        wchar_t sec[32], b1[MAX_PATH * 2], b2[MAX_PATH * 2], b3[64], b4[64];
        swprintf(sec, 32, L"tab%d", i);
        GetPrivateProfileStringW(sec, L"path", L"", b1, MAX_PATH * 2, ini.c_str());
        GetPrivateProfileStringW(sec, L"name", L"", b2, MAX_PATH * 2, ini.c_str());
        Entry e{b1, b2, L"", 1.0, 0, true};
        GetPrivateProfileStringW(sec, L"recovery", L"", b3, 64, ini.c_str());
        if (b3[0] && GetFileAttributesW((dir + L"\\" + b3).c_str()) != INVALID_FILE_ATTRIBUTES) { e.rec = dir + L"\\" + b3; ++withChanges; }
        GetPrivateProfileStringW(sec, L"zoom", L"1", b4, 64, ini.c_str());
        e.zoom = wcstod(b4, nullptr);
        e.scrollY = static_cast<int>(GetPrivateProfileIntW(sec, L"scrollY", 0, ini.c_str()));
        e.fit = GetPrivateProfileIntW(sec, L"fit", 1, ini.c_str()) != 0;
        if (e.rec.empty() && (e.path.empty() || GetFileAttributesW(e.path.c_str()) == INVALID_FILE_ATTRIBUTES)) continue;
        list.push_back(e);
    }
    if (list.empty()) { clearSession(); return false; }
    std::wstring msg = L"Feather PDF did not close normally last time.\n\nReopen " + std::to_wstring(list.size()) + L" document(s)";
    if (withChanges) msg += L", including " + std::to_wstring(withChanges) + L" with changes that were not saved";
    msg += L"?";
    if (MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) { clearSession(); return false; }
    for (const Entry& e : list) {
        if (!e.rec.empty()) {
            std::vector<unsigned char> bytes;
            HANDLE f = CreateFileW(e.rec.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz{};
                GetFileSizeEx(f, &sz);
                bytes.resize(static_cast<size_t>(sz.QuadPart));
                DWORD got = 0;
                if (!ReadFile(f, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr) || got != bytes.size()) bytes.clear();
                CloseHandle(f);
            }
            auto doc = std::make_unique<Document>();
            if (!bytes.empty() && doc->load(std::move(bytes), {}) == OpenResult::Ok) {
                doc->setPath(e.path);
                if (e.path.empty()) doc->setName(e.name);
                doc->markModified();
                doc->zoom = e.zoom; doc->scrollY = e.scrollY; doc->fitWidth = e.fit;
                addDocumentTab(std::move(doc));
                continue;
            }
        }
        if (!e.path.empty()) openFile(e.path);
    }
    clearSession();
    writeSession();
    return true;
}

// ---- back / forward, layout, rotate view, reload ----
void onJump(int from, int) {
    if (app.navBusy) return;
    app.navBack.push_back(from);
    app.navFwd.clear();
    if (app.navBack.size() > 100) app.navBack.erase(app.navBack.begin());
}

void navBack() {
    if (app.navBack.empty() || !activeDoc()) return;
    app.navFwd.push_back(app.viewer.currentPage());
    int p = app.navBack.back(); app.navBack.pop_back();
    app.navBusy = true; app.viewer.goToPage(p); app.navBusy = false;
}

void navForward() {
    if (app.navFwd.empty() || !activeDoc()) return;
    app.navBack.push_back(app.viewer.currentPage());
    int p = app.navFwd.back(); app.navFwd.pop_back();
    app.navBusy = true; app.viewer.goToPage(p); app.navBusy = false;
}

void setPageLayout(int layout) {
    if (layout == 3) app.cover = !app.cover; else app.layout = layout;
    app.viewer.setLayout(app.layout, app.cover);
    if (app.sbs) app.viewer2.setLayout(app.layout, app.cover);
    saveSettings();
}

void rotateView(int turns) {
    Document* d = activeDoc();
    if (!d) return;
    std::vector<int> all(d->pageCount());
    for (int i = 0; i < d->pageCount(); ++i) all[i] = i;
    if (d->rotatePages(all, turns)) afterPagesChanged();
}

void reloadFromDisk() {
    Document* d = activeDoc();
    if (!d || d->path().empty()) return;
    if (d->dirty() && MessageBoxW(app.wnd, L"This document has changes that are not saved. Reloading from disk throws them away. Reload anyway?", kAppName, MB_YESNO | MB_ICONWARNING) != IDYES) return;
    const double zoom = d->zoom; const int sy = d->scrollY; const bool fw = d->fitWidth;
    std::string pw;
    OpenResult r = d->open(d->path());
    while (r == OpenResult::PasswordNeeded || r == OpenResult::WrongPassword) {
        if (!askPassword(Document::describe(r, d->path()), pw)) return;
        r = d->open(d->path(), pw);
    }
    if (r != OpenResult::Ok) { MessageBoxW(app.wnd, Document::describe(r, d->path()).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    d->zoom = zoom; d->scrollY = sy; d->fitWidth = fw;
    setActiveDocument(d);
    app.viewer.documentChanged();
    app.sidebar.refresh();
    refresh();
}

// ---- find and replace ----
void doFindReplace() {
    Document* d = activeDoc();
    if (!d) return;
    std::vector<FormField> f(3);
    f[0] = {FormField::Text, L"Find:", {}, app.viewer.hasSelection() ? app.viewer.selectedText() : L""};
    f[1] = {FormField::Text, L"Replace with:", {}, L""};
    f[2] = {FormField::Check, L"Match case"};
    if (!showForm(app.wnd, app.inst, L"Find and replace text", f, L"Replace all", [](const std::vector<FormField>& v) -> std::wstring {
            if (v[0].text.empty()) return L"Type the text to find.";
            if (!pageobj::isLatin(v[1].text)) return L"The new text can use letters, numbers and common symbols of Western languages.";
            return L""; })) return;
    std::vector<int> pages;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    int n = pageobj::replaceAll(*d, f[0].text, f[1].text, f[2].checked, pages);
    SetCursor(old);
    if (n == 0) {
        MessageBoxW(app.wnd, L"Nothing was replaced. The text may not be there, may be split across lines or styles, or may be on scanned pages (pictures).", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    afterPagesChanged();
    wchar_t msg[160];
    swprintf(msg, 160, L"Replaced %d occurrence(s) on %d page(s). Undo (Ctrl+Z) takes it back.", n, static_cast<int>(pages.size()));
    MessageBoxW(app.wnd, msg, kAppName, MB_OK | MB_ICONINFORMATION);
}

// ---- tables ----
bool tableDialog(pageobj::TableSpec& t, const wchar_t* title, const wchar_t* ok) {
    std::vector<FormField> f(6);
    wchar_t b[32];
    f[0] = {FormField::Text, L"Rows:", {}, std::to_wstring(t.rows)};
    f[1] = {FormField::Text, L"Columns:", {}, std::to_wstring(t.cols)};
    swprintf(b, 32, L"%.0f", t.colWidth * 25.4 / 72.0); f[2] = {FormField::Text, L"Column width (mm):", {}, b};
    swprintf(b, 32, L"%.0f", t.rowHeight * 25.4 / 72.0); f[3] = {FormField::Text, L"Row height (mm):", {}, b};
    int si = 3;
    static const double sizes[] = {8, 9, 10, 11, 12, 14};
    for (int i = 0; i < 6; ++i) if (sizes[i] == t.fontSize) si = i;
    f[4] = {FormField::Choice, L"Text size:", {L"8 pt", L"9 pt", L"10 pt", L"11 pt", L"12 pt", L"14 pt"}, L"", si};
    f[5] = {FormField::Check, L"First row is a heading", {}, L"", 0, t.header};
    if (!showForm(app.wnd, app.inst, title, f, ok, [](const std::vector<FormField>& v) -> std::wstring {
            auto num = [](const std::wstring& s, double lo, double hi) { wchar_t* e = nullptr; double x = wcstod(s.c_str(), &e); return e != s.c_str() && !*e && x >= lo && x <= hi; };
            if (!num(v[0].text, 1, 50) || !num(v[1].text, 1, 20)) return L"Rows can be 1 to 50 and columns 1 to 20.";
            if (!num(v[2].text, 5, 200) || !num(v[3].text, 4, 100)) return L"Column width can be 5 to 200 mm and row height 4 to 100 mm.";
            return L""; })) return false;
    const int oldRows = t.rows, oldCols = t.cols;
    std::vector<std::wstring> old = t.cells;
    t.rows = static_cast<int>(wcstol(f[0].text.c_str(), nullptr, 10));
    t.cols = static_cast<int>(wcstol(f[1].text.c_str(), nullptr, 10));
    t.colWidth = wcstod(f[2].text.c_str(), nullptr) * 72.0 / 25.4;
    t.rowHeight = wcstod(f[3].text.c_str(), nullptr) * 72.0 / 25.4;
    t.fontSize = sizes[f[4].choice];
    t.header = f[5].checked;
    // cell texts: one line per row, cells separated by |
    std::wstring initial;
    for (int r = 0; r < t.rows; ++r) {
        for (int c = 0; c < t.cols; ++c) {
            std::wstring cell = (r < oldRows && c < oldCols && static_cast<size_t>(r) * oldCols + c < old.size()) ? old[static_cast<size_t>(r) * oldCols + c] : L"";
            initial += (c ? L" | " : L"") + cell;
        }
        initial += L"\r\n";
    }
    std::wstring text;
    while (askText(title, L"Type the text of the cells. One line per row; separate the cells of a row with the | sign.", false, text, initial.c_str(), true)) {
        text = fromEdit(text);
        if (!pageobj::isLatin(text)) { MessageBoxW(app.wnd, L"Cells can use letters, numbers and common symbols of Western languages.", kAppName, MB_OK | MB_ICONINFORMATION); initial = toEdit(text); continue; }
        t.cells.assign(static_cast<size_t>(t.rows) * t.cols, L"");
        int r = 0;
        size_t pos = 0;
        while (r < t.rows) {
            size_t e = text.find(L'\r', pos);
            std::wstring line = text.substr(pos, e == std::wstring::npos ? std::wstring::npos : e - pos);
            size_t cp = 0;
            for (int c = 0; c < t.cols; ++c) {
                size_t bar = line.find(L'|', cp);
                std::wstring cell = line.substr(cp, bar == std::wstring::npos ? std::wstring::npos : bar - cp);
                size_t a = cell.find_first_not_of(L' '), z = cell.find_last_not_of(L' ');
                t.cells[static_cast<size_t>(r) * t.cols + c] = a == std::wstring::npos ? L"" : cell.substr(a, z - a + 1);
                if (bar == std::wstring::npos) break;
                cp = bar + 1;
            }
            ++r;
            if (e == std::wstring::npos) break;
            pos = e + 1;
        }
        return true;
    }
    return false;
}

void tableInsert() {
    if (!activeDoc()) return;
    app.pendingTable = pageobj::TableSpec{};
    if (!tableDialog(app.pendingTable, L"Insert table", L"Next")) return;
    if (app.tool != Viewer::Tool::TablePlace) setTool(Viewer::Tool::TablePlace);
    MessageBoxW(app.wnd, L"Now click on the page where the top-left corner of the table should go. Esc cancels.", kAppName, MB_OK | MB_ICONINFORMATION);
}

void tableEdit() {
    if (!activeDoc()) return;
    if (app.tool != Viewer::Tool::TableEdit) setTool(Viewer::Tool::TableEdit);
    MessageBoxW(app.wnd, L"Click a table that was inserted with Feather PDF. Esc cancels.", kAppName, MB_OK | MB_ICONINFORMATION);
}

void tableDelete() { if (activeDoc() && app.tool != Viewer::Tool::TableDelete) setTool(Viewer::Tool::TableDelete); }

// ---- links, copy area, crop by box ----
void startLink() { if (activeDoc()) setTool(Viewer::Tool::Link); }
void startCopyArea() { if (activeDoc()) setTool(Viewer::Tool::CopyArea); }
void cropByBox() {
    if (!activeDoc()) return;
    setTool(Viewer::Tool::CropBox);
}

void lookUpSelection(bool web) {
    std::wstring t = app.viewer.selectedText();
    if (t.size() > 200) t.resize(200);
    for (auto& c : t) if (c == L'\r' || c == L'\n') c = L' ';
    if (t.find_first_not_of(L' ') == std::wstring::npos) return;
    std::string u8(WideCharToMultiByte(CP_UTF8, 0, t.c_str(), -1, nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, t.c_str(), -1, u8.data(), static_cast<int>(u8.size()), nullptr, nullptr);
    u8.resize(strlen(u8.c_str()));
    std::wstring enc;
    for (unsigned char ch : u8) {
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.') enc += static_cast<wchar_t>(ch);
        else { wchar_t b[8]; swprintf(b, 8, L"%%%02X", ch); enc += b; }
    }
    const wchar_t* base = web ? L"https://www.google.com/search?q=" : L"https://www.google.com/search?q=define+";
    ShellExecuteW(app.wnd, L"open", (std::wstring(base) + enc).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void lookUpKind(int kind) {   // 0 search, 1 dictionary, 2 Wikipedia, 3 translate
    std::wstring t = app.viewer.selectedText();
    if (t.size() > 200) t.resize(200);
    for (auto& c : t) if (c == L'\r' || c == L'\n') c = L' ';
    if (t.find_first_not_of(L' ') == std::wstring::npos) return;
    std::string u8(WideCharToMultiByte(CP_UTF8, 0, t.c_str(), -1, nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, t.c_str(), -1, u8.data(), static_cast<int>(u8.size()), nullptr, nullptr);
    u8.resize(strlen(u8.c_str()));
    std::wstring enc;
    for (unsigned char ch : u8) {
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.') enc += static_cast<wchar_t>(ch);
        else { wchar_t b[8]; swprintf(b, 8, L"%%%02X", ch); enc += b; }
    }
    static const wchar_t* bases[] = {L"https://www.google.com/search?q=", L"https://www.google.com/search?q=define+", L"https://en.wikipedia.org/w/index.php?search=", L"https://translate.google.com/?sl=auto&op=translate&text="};
    ShellExecuteW(app.wnd, L"open", (std::wstring(bases[kind]) + enc).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void readFromThisPage() { if (speech::speaking()) stopReading(); app.viewer.clearSelection(); readAloud(); }

// ---- black and white ----
void doGreyscale() {
    Document* d = activeDoc();
    if (!d) return;
    compress::Options o;
    o.dpi = 300; o.jpegQuality = 88; o.greyscale = true;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    compress::Result r = compress::run(*d, o);
    SetCursor(old);
    if (!r.changed) { MessageBoxW(app.wnd, L"There are no pictures in colour to convert. Text and drawings are not changed by this command.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    afterPagesChanged();
    MessageBoxW(app.wnd, (L"Pictures converted to greyscale: " + std::to_wstring(r.pictures) + L". Text and drawings keep their colours.\n\nUndo (Ctrl+Z) takes the change back.").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}

// ---- handwritten signature ----
std::wstring signaturePath() {
    std::wstring s = settingsPath();
    size_t p = s.find_last_of(L'\\');
    return (p == std::wstring::npos ? L"" : s.substr(0, p + 1)) + L"signature.png";
}

std::wstring signaturePathFwd() { return signaturePath(); }

void signatureAction(int cmd) {
    Document* d = activeDoc();
    if (!d) return;
    const std::wstring png = signaturePath();
    bool ok = false;
    if (cmd == N_SIG_CLEAR) ok = GetFileAttributesW(png.c_str()) != INVALID_FILE_ATTRIBUTES;   // the one made last time
    else if (cmd == N_SIG_TYPE) {
        std::wstring name;
        if (!askText(L"Signature", L"Type your name as you sign it.", false, name, L"") || name.empty()) return;
        if (name.size() > 60) name.resize(60);
        ok = sigpad::fromText(name, png);
    } else if (cmd == N_SIG_DRAW) {
        ok = sigpad::draw(app.wnd, app.inst, png);
        if (!ok) return;
    } else if (cmd == N_SIG_PIC) {
        std::vector<wchar_t> buf(MAX_PATH * 2, 0);
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = app.wnd;
        ofn.lpstrTitle = L"Choose a picture of your signature";
        ofn.lpstrFilter = L"Pictures (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0";
        ofn.lpstrFile = buf.data(); ofn.nMaxFile = static_cast<DWORD>(buf.size());
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&ofn)) return;
        ok = CopyFileW(buf.data(), (png + L".src").c_str(), FALSE) != 0;
        if (ok) { app.pendingPicture = buf.data(); setTool(Viewer::Tool::Picture); MessageBoxW(app.wnd, L"Now click on the page where the top-left corner of the signature should go. Esc cancels.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    }
    if (!ok) { MessageBoxW(app.wnd, L"The signature could not be made.", kAppName, MB_OK | MB_ICONWARNING); return; }
    app.pendingPicture = png;
    app.pendingPictureWidth = 150;
    if (app.tool != Viewer::Tool::Picture) setTool(Viewer::Tool::Picture);
    MessageBoxW(app.wnd, L"Now click on the page where the top-left corner of the signature should go. Esc cancels.\n\nThis is a picture of your handwriting. For a signature that proves who signed, use Edit PDF > Digital signatures.", kAppName, MB_OK | MB_ICONINFORMATION);
}

// ---- geometry ----
double unitsPerPoint() { static const double v[] = {25.4 / 72.0, 2.54 / 72.0, 1.0 / 72.0, 1.0}; return v[app.units]; }
const wchar_t* unitName() { static const wchar_t* v[] = {L"mm", L"cm", L"in", L"pt"}; return v[app.units]; }
double scaleFactor() { static const double v[] = {1, 2, 5, 10, 20, 50, 100, 0.5, 0.2, 0.1}; return v[app.scaleIdx]; }

void geometryAction(int cmd) {
    if (!activeDoc()) return;
    if (cmd == G_RULER) { setTool(Viewer::Tool::Ruler); return; }
    if (cmd == G_PROT) { app.protStage = 0; app.viewer.setPending(nullptr, 0, nullptr); setTool(Viewer::Tool::Protractor); return; }
    if (cmd == G_RULERS) { app.viewer.setShowRulers(!app.viewer.showRulers(), app.units); return; }
    if (cmd == G_CLEAR) { app.viewer.clearMeasures(); return; }
    if (cmd == G_KEEP) {
        Document* d = activeDoc();
        int n = 0;
        for (const auto& m : app.viewer.measures()) {
            std::vector<std::vector<annot::PointD>> strokes{{m.a, m.b}};
            if (m.angle) strokes.push_back({m.a, m.c});
            if (annot::addInk(*d, m.page, strokes, RGB(0x00, 0x90, 0x60), 1.5)) {
                annot::addNote(*d, m.page, {(m.a.x + m.b.x) / 2, (m.a.y + m.b.y) / 2}, m.text, RGB(0x66, 0xDD, 0x66));
                ++n;
            }
        }
        app.viewer.clearMeasures();
        app.viewer.documentChanged();
        refresh();
        (void)n;
        return;
    }
    if (cmd >= G_UNITS && cmd < G_UNITS + 4) { app.units = cmd - G_UNITS; app.viewer.setShowRulers(app.viewer.showRulers(), app.units); saveSettings(); return; }
    if (cmd >= G_SCALE && cmd < G_SCALE + 10) { app.scaleIdx = cmd - G_SCALE; saveSettings(); return; }
}

void measureDraw(const Viewer::Event& e) {
    wchar_t t[64];
    if (e.tool == Viewer::Tool::Ruler) {
        const double dx = e.pts[1].x - e.pts[0].x, dy = e.pts[1].y - e.pts[0].y;
        const double v = std::hypot(dx, dy) * unitsPerPoint() * scaleFactor();
        swprintf(t, 64, app.units == 1 || app.units == 2 ? L"%.2f %ls" : L"%.1f %ls", v, unitName());
        app.viewer.addMeasure({e.page, e.pts[0], e.pts[1], {}, false, t});
        return;
    }
    if (app.protStage == 0) {
        app.protVertex = e.pts[0]; app.protArm = e.pts[1]; app.protPage = e.page; app.protStage = 1;
        app.viewer.setPending(&app.protVertex, e.page, &app.protArm);
        return;
    }
    if (e.page != app.protPage) { app.protStage = 0; app.viewer.setPending(nullptr, 0, nullptr); return; }
    const double ax = app.protArm.x - app.protVertex.x, ay = app.protArm.y - app.protVertex.y;
    const double bx = e.pts[1].x - app.protVertex.x, by = e.pts[1].y - app.protVertex.y;
    const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
    if (la < 1 || lb < 1) return;
    double c = std::clamp((ax * bx + ay * by) / (la * lb), -1.0, 1.0);
    swprintf(t, 64, L"%.1f°", std::acos(c) * 180.0 / 3.14159265358979);
    app.viewer.addMeasure({e.page, app.protVertex, app.protArm, e.pts[1], true, t});
    app.protStage = 0;
    app.viewer.setPending(nullptr, 0, nullptr);
}

// ---- events of the new tools ----
bool featureDraw(Document& d, const Viewer::Event& e) {
    switch (e.tool) {
        case Viewer::Tool::Select: {
            if (app.sel.page != e.page || e.pts.size() < 4) return true;
            if (pageobj::transformGroup(d, e.page, app.sel.objects, e.pts[0].x, e.pts[0].y, e.pts[1].x, e.pts[1].y, e.pts[2].x, e.pts[2].y, e.pts[3].x, e.pts[3].y)) {
                app.viewer.documentChanged();
                app.sidebar.refresh();
                refresh();
                std::vector<double> gx, gy;
                pageobj::snapGuides(d, e.page, app.sel.objects, gx, gy);
                annot::RectD r{e.pts[2].x, e.pts[2].y, e.pts[3].x, e.pts[3].y};
                app.viewer.setObjectSelection(e.page, &r, gx, gy);
            }
            return true;
        }
        case Viewer::Tool::Ruler: case Viewer::Tool::Protractor: measureDraw(e); return true;
        case Viewer::Tool::FormText: case Viewer::Tool::FormCheck: case Viewer::Tool::FormCombo: return formFieldDraw(d, e);
        case Viewer::Tool::Link: {
            annot::RectD r{std::min(e.pts[0].x, e.pts[1].x), std::min(e.pts[0].y, e.pts[1].y), std::max(e.pts[0].x, e.pts[1].x), std::max(e.pts[0].y, e.pts[1].y)};
            std::wstring url;
            if (askText(L"Link", L"The web address this box should open, for example https://example.com", false, url, L"https://")) {
                std::string u8(WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, nullptr, 0, nullptr, nullptr), '\0');
                WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, u8.data(), static_cast<int>(u8.size()), nullptr, nullptr);
                u8.resize(strlen(u8.c_str()));
                if (u8.rfind("http://", 0) != 0 && u8.rfind("https://", 0) != 0 && u8.rfind("mailto:", 0) != 0) u8 = "https://" + u8;
                if (annot::addLink(d, e.page, r, u8)) { refreshLinks(d); app.viewer.documentChanged(); refresh(); }
                else MessageBoxW(app.wnd, L"The link could not be added.", kAppName, MB_OK | MB_ICONWARNING);
            }
            setTool(Viewer::Tool::Link);
            return true;
        }
        case Viewer::Tool::CopyArea: {
            if (!pageobj::copyAreaToClipboard(d, e.page, std::min(e.pts[0].x, e.pts[1].x), std::min(e.pts[0].y, e.pts[1].y), std::max(e.pts[0].x, e.pts[1].x), std::max(e.pts[0].y, e.pts[1].y), app.wnd))
                MessageBoxW(app.wnd, L"The area could not be copied as a picture.", kAppName, MB_OK | MB_ICONWARNING);
            setTool(Viewer::Tool::CopyArea);
            return true;
        }
        case Viewer::Tool::CropBox: {
            double cl, cb, cr, ct;
            if (!d.cropBoxOf(e.page, cl, cb, cr, ct)) return true;
            const double l = std::min(e.pts[0].x, e.pts[1].x), r = std::max(e.pts[0].x, e.pts[1].x), b = std::min(e.pts[0].y, e.pts[1].y), t = std::max(e.pts[0].y, e.pts[1].y);
            int ans = MessageBoxW(app.wnd, L"Crop to this box.\n\nYes = crop every page by the same amounts\nNo = crop only this page\nCancel = do nothing", kAppName, MB_YESNOCANCEL | MB_ICONQUESTION);
            setTool(Viewer::Tool::CropBox);
            if (ans == IDCANCEL) return true;
            std::vector<int> pages;
            if (ans == IDYES) for (int i = 0; i < d.pageCount(); ++i) pages.push_back(i); else pages.push_back(e.page);
            if (d.cropPages(pages, l - cl, b - cb, cr - r, ct - t)) afterPagesChanged();
            else MessageBoxW(app.wnd, L"That would leave the page too small.", kAppName, MB_OK | MB_ICONINFORMATION);
            return true;
        }
        default: return false;
    }
}

bool featureClick(Document& d, const Viewer::Event& e) {
    switch (e.tool) {
        case Viewer::Tool::TablePlace: {
            if (pageobj::addTable(d, e.page, e.pts.front().x, e.pts.front().y, app.pendingTable)) { app.viewer.documentChanged(); refresh(); app.sidebar.refresh(); }
            else MessageBoxW(app.wnd, L"The table could not be added.", kAppName, MB_OK | MB_ICONWARNING);
            setTool(Viewer::Tool::TablePlace);   // one table at a time
            return true;
        }
        case Viewer::Tool::Select: {
            pageobj::GroupHit g = pageobj::groupAt(d, e.page, e.pts.front().x, e.pts.front().y);
            if (!g.found) { app.sel.page = -1; app.sel.objects.clear(); app.viewer.setObjectSelection(-1, nullptr); return true; }
            app.sel.page = e.page; app.sel.objects = g.objects;
            std::vector<double> gx, gy;
            pageobj::snapGuides(d, e.page, g.objects, gx, gy);
            annot::RectD r{g.left, g.bottom, g.right, g.top};
            app.viewer.setObjectSelection(e.page, &r, gx, gy);
            return true;
        }
        case Viewer::Tool::EditPara: {
            pageobj::ParagraphHit hit = pageobj::paragraphAt(d, e.page, e.pts.front().x, e.pts.front().y);
            if (!hit.found) {
                MessageBoxW(app.wnd, L"No paragraph of text was found there. Click directly on the text. Scanned pages are pictures and have no editable text.", kAppName, MB_OK | MB_ICONINFORMATION);
                return true;
            }
            std::wstring text;
            wchar_t head[200];
            swprintf(head, 200, L"Change the paragraph (%d line%ls). It is set again to fit the same width; a new line starts where you press Enter.", hit.lines, hit.lines == 1 ? L"" : L"s");
            if (askText(L"Edit paragraph", head, false, text, toEdit(hit.text).c_str(), true)) {
                text = fromEdit(text);
                std::vector<FormField> f(6);
                f[0] = {FormField::Choice, L"Font:", {L"Sans-serif (Helvetica)", L"Serif (Times)", L"Monospaced (Courier)"}, L"", static_cast<int>(hit.style.family)};
                wchar_t sz[16];
                swprintf(sz, 16, L"%.1f", hit.style.sizePt);
                f[1] = {FormField::Text, L"Size (pt):", {}, sz};
                f[2] = {FormField::Check, L"Bold", {}, L"", 0, hit.style.bold};
                f[3] = {FormField::Check, L"Italic", {}, L"", 0, hit.style.italic};
                std::vector<std::wstring> colours{L"Keep the colour"};
                for (const wchar_t* n : kPaletteNames) colours.push_back(n);
                f[4] = {FormField::Choice, L"Colour:", colours, L"", 0};
                f[5] = {FormField::Choice, L"Alignment:", {L"Left", L"Centre", L"Right", L"Justified"}, L"", 0};
                if (showForm(app.wnd, app.inst, L"Paragraph style", f, L"Apply", [](const std::vector<FormField>& v) -> std::wstring {
                        wchar_t* end = nullptr; double x = wcstod(v[1].text.c_str(), &end);
                        return (end == v[1].text.c_str() || *end || x < 4 || x > 200) ? L"Type a size between 4 and 200 points." : L""; })) {
                    pageobj::TextStyle st = hit.style;
                    st.family = static_cast<pageobj::Family>(f[0].choice);
                    st.sizePt = wcstod(f[1].text.c_str(), nullptr);
                    st.bold = f[2].checked; st.italic = f[3].checked;
                    if (f[4].choice > 0) st.color = kPalette[f[4].choice - 1];
                    if (!pageobj::isLatin(text)) MessageBoxW(app.wnd, L"Paragraphs can use letters, numbers and common symbols of Western languages. Please remove other characters.", kAppName, MB_OK | MB_ICONINFORMATION);
                    else if (pageobj::replaceParagraph(d, hit, text, st, static_cast<pageobj::Align>(f[5].choice))) { app.viewer.documentChanged(); app.sidebar.refresh(); refresh(); }
                    else MessageBoxW(app.wnd, L"The paragraph could not be changed.", kAppName, MB_OK | MB_ICONWARNING);
                }
            }
            setTool(Viewer::Tool::EditPara);
            return true;
        }
        case Viewer::Tool::TableEdit: {
            pageobj::TableHit hit = pageobj::tableAt(d, e.page, e.pts.front().x, e.pts.front().y);
            if (!hit.found) { MessageBoxW(app.wnd, L"No table made by Feather PDF was found there.", kAppName, MB_OK | MB_ICONINFORMATION); return true; }
            pageobj::TableSpec t = hit.spec;
            if (tableDialog(t, L"Edit table", L"Apply")) {
                d.beginChange();
                pageobj::removeTable(d, e.page, hit.id, false);
                if (pageobj::addTable(d, e.page, hit.left, hit.top, t, false)) { d.resetText(e.page); app.viewer.documentChanged(); refresh(); app.sidebar.refresh(); }
                else { d.cancelChange(); }
            }
            setTool(Viewer::Tool::TableEdit);
            return true;
        }
        case Viewer::Tool::TableDelete: {
            pageobj::TableHit hit = pageobj::tableAt(d, e.page, e.pts.front().x, e.pts.front().y);
            if (!hit.found) { MessageBoxW(app.wnd, L"No table made by Feather PDF was found there.", kAppName, MB_OK | MB_ICONINFORMATION); return true; }
            if (pageobj::removeTable(d, e.page, hit.id)) { app.viewer.documentChanged(); refresh(); app.sidebar.refresh(); }
            setTool(Viewer::Tool::TableDelete);
            return true;
        }
        default: return false;
    }
}


void addBookmarkHere() {
    Document* d = activeDoc();
    if (!d) return;
    const int page = app.viewer.currentPage();
    std::wstring title = L"Page " + std::to_wstring(page + 1);
    if (app.viewer.hasSelection()) {
        std::wstring sel = app.viewer.selectedText();
        for (auto& c : sel) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
        if (sel.size() > 80) sel.resize(80);
        if (sel.find_first_not_of(L' ') != std::wstring::npos) title = sel;
    }
    std::wstring t;
    if (!askText(L"Add bookmark", L"Name of the bookmark for this page.", false, t, title.c_str()) || t.empty()) return;
    std::vector<unsigned char> bytes, out;
    if (!d->serialize(bytes) || !outline::addBookmark(bytes, page, t, out)) {
        MessageBoxW(app.wnd, L"The bookmark could not be added to this document (its structure is not supported).", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    d->beginChange();
    if (d->reload(std::move(out)) != OpenResult::Ok) {
        d->cancelChange();
        MessageBoxW(app.wnd, L"The bookmark could not be added to this document.", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    afterPagesChanged();
    showSidebar(Sidebar::Mode::Bookmarks);
}

// PDFium keeps its list of links from the first time it is asked; reopening the saved bytes makes new links clickable.
void refreshLinks(Document& d) {
    std::vector<unsigned char> b;
    if (d.serialize(b)) d.reload(std::move(b));
}

void linkOnSelection() {
    Document* d = activeDoc();
    if (!d || !app.viewer.hasSelection()) return;
    std::wstring url;
    if (!askText(L"Link", L"The web address the selected text should open, for example https://example.com", false, url, L"https://")) return;
    std::string u8(WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, nullptr, 0, nullptr, nullptr), '\0');
    WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, u8.data(), static_cast<int>(u8.size()), nullptr, nullptr);
    u8.resize(strlen(u8.c_str()));
    if (u8.rfind("http://", 0) != 0 && u8.rfind("https://", 0) != 0 && u8.rfind("mailto:", 0) != 0) u8 = "https://" + u8;
    bool any = false;
    for (const auto& pr : app.viewer.selectionRects())
        for (const auto& r : pr.rects) any = annot::addLink(*d, pr.page, r, u8) || any;
    app.viewer.clearSelection();
    if (any) { refreshLinks(*d); app.viewer.documentChanged(); refresh(); }
    else MessageBoxW(app.wnd, L"The link could not be added.", kAppName, MB_OK | MB_ICONWARNING);
}

// ---- new PDF from pictures, split ----
void newPdfFromPictures() {
    std::vector<wchar_t> buf(MAX_PATH * 64, 0);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Choose the pictures (they become pages in the order of their names)";
    ofn.lpstrFilter = L"Pictures (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0";
    ofn.lpstrFile = buf.data(); ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) return;
    std::vector<std::wstring> files;
    const wchar_t* p = buf.data();
    std::wstring first = p;
    p += first.size() + 1;
    if (!*p) files.push_back(first);
    else for (; *p; p += wcslen(p) + 1) files.push_back(first + L"\\" + p);
    std::sort(files.begin(), files.end());
    std::vector<std::pair<double, double>> sizes;
    std::vector<std::wstring> ok;
    for (const auto& f : files) {
        int w, h;
        if (!pageobj::imageSize(f, w, h)) continue;
        double pw = w * 0.75, ph = h * 0.75;
        const double big = std::max(pw, ph);
        if (big > 842) { pw *= 842 / big; ph *= 842 / big; }   // no larger than A4's long side
        sizes.push_back({pw, ph});
        ok.push_back(f);
    }
    if (ok.empty()) { MessageBoxW(app.wnd, L"None of those files is a picture Feather PDF can read.", kAppName, MB_OK | MB_ICONWARNING); return; }
    std::vector<unsigned char> bytes;
    auto doc = std::make_unique<Document>();
    if (!Document::blankBytes(sizes, bytes) || doc->load(std::move(bytes), {}) != OpenResult::Ok) { MessageBoxW(app.wnd, L"The new document could not be made.", kAppName, MB_OK | MB_ICONWARNING); return; }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    int bad = 0;
    for (size_t i = 0; i < ok.size(); ++i) {
        std::wstring err;
        const double vh = sizes[i].second;
        if (!pageobj::addPicture(*doc, static_cast<int>(i), 0, vh, ok[i], err, sizes[i].first)) ++bad;
    }
    SetCursor(old);
    doc->setName(L"New PDF.pdf");
    addDocumentTab(std::move(doc));
    if (bad) MessageBoxW(app.wnd, L"Some pictures could not be placed. Their pages are empty.", kAppName, MB_OK | MB_ICONWARNING);
}

void splitPdf() {
    Document* d = activeDoc();
    if (!d || d->pageCount() < 2) return;
    std::vector<FormField> f(3);
    f[0] = {FormField::Choice, L"Split:", {L"Into single pages", L"Every N pages", L"By page ranges"}, L"", 1};
    f[1] = {FormField::Text, L"N pages per file:", {}, L"10"};
    f[2] = {FormField::Text, L"Ranges (use ; between):", {}, L"1-5; 6-"};
    g_formDoc = d;
    bool ok = showForm(app.wnd, app.inst, L"Split into several files", f, L"Name...", [](const std::vector<FormField>& v) -> std::wstring {
        if (v[0].choice == 1) { wchar_t* e = nullptr; long n = wcstol(v[1].text.c_str(), &e, 10); if (e == v[1].text.c_str() || *e || n < 1) return L"Type how many pages each file should have."; }
        if (v[0].choice == 2) {
            size_t pos = 0;
            const std::wstring& t = v[2].text;
            bool any = false;
            while (pos <= t.size()) {
                size_t e = t.find(L';', pos);
                std::wstring part = t.substr(pos, e == std::wstring::npos ? std::wstring::npos : e - pos);
                if (part.find_first_not_of(L' ') != std::wstring::npos) { any = true; if (!g_formDoc || g_formDoc->parseRange(part).empty()) return L"\"" + part + L"\" is not a valid page range."; }
                if (e == std::wstring::npos) break;
                pos = e + 1;
            }
            if (!any) return L"Type at least one range.";
        }
        return L""; });
    g_formDoc = nullptr;
    if (!ok) return;
    std::vector<std::vector<int>> groups;
    const int n = d->pageCount();
    if (f[0].choice == 0) for (int i = 0; i < n; ++i) groups.push_back({i});
    else if (f[0].choice == 1) {
        int per = static_cast<int>(wcstol(f[1].text.c_str(), nullptr, 10));
        for (int i = 0; i < n; i += per) { std::vector<int> g; for (int k = i; k < std::min(n, i + per); ++k) g.push_back(k); groups.push_back(g); }
    } else {
        size_t pos = 0;
        const std::wstring& t = f[2].text;
        while (pos <= t.size()) {
            size_t e = t.find(L';', pos);
            std::wstring part = t.substr(pos, e == std::wstring::npos ? std::wstring::npos : e - pos);
            if (part.find_first_not_of(L' ') != std::wstring::npos) groups.push_back(d->parseRange(part));
            if (e == std::wstring::npos) break;
            pos = e + 1;
        }
    }
    wchar_t buf[MAX_PATH * 2];
    std::wstring base = d->title();
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    wcsncpy_s(buf, (base + L" (part).pdf").c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Name for the files (a number is added)";
    ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0";
    ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2; ofn.lpstrDefExt = L"pdf";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return;
    std::wstring stem = buf;
    if (stem.size() > 4 && _wcsicmp(stem.c_str() + stem.size() - 4, L".pdf") == 0) stem.resize(stem.size() - 4);
    int saved = 0;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i].empty()) continue;
        if (d->extractPages(groups[i], stem + L"-" + std::to_wstring(i + 1) + L".pdf") == SaveResult::Ok) ++saved;
    }
    wchar_t msg[160];
    swprintf(msg, 160, L"%d file(s) were saved next to \"%ls-1.pdf\".", saved, stem.substr(stem.find_last_of(L'\\') + 1).c_str());
    MessageBoxW(app.wnd, msg, kAppName, saved ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONWARNING);
}

// ---- a small progress window (used by folder search and batch mode) ----
struct ProgressWin {
    HWND wnd = nullptr, label = nullptr, bar = nullptr;
    bool cancelled = false;
};

LRESULT CALLBACK progressProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* p = reinterpret_cast<ProgressWin*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); break;
        case WM_COMMAND: if (LOWORD(w) == IDCANCEL && p) p->cancelled = true; return 0;
        case WM_CLOSE: if (p) p->cancelled = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void progressOpen(ProgressWin& p, const wchar_t* title) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = progressProc; wc.hInstance = app.inst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1); wc.lpszClassName = L"FeatherProgress";
        RegisterClassExW(&wc);
        reg = true;
    }
    RECT pr; GetWindowRect(app.wnd, &pr);
    const int W = 460, H = 130;
    p.wnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FeatherProgress", title, WS_POPUP | WS_CAPTION, (pr.left + pr.right - W) / 2, (pr.top + pr.bottom - H) / 2, W, H, app.wnd, nullptr, app.inst, &p);
    HFONT f = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    p.label = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_PATHELLIPSIS, 14, 12, W - 40, 18, p.wnd, nullptr, app.inst, nullptr);
    p.bar = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE, 14, 38, W - 40, 16, p.wnd, nullptr, app.inst, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, W - 110, 66, 80, 26, p.wnd, reinterpret_cast<HMENU>(IDCANCEL), app.inst, nullptr);
    SendMessageW(p.label, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
    SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
    EnableWindow(app.wnd, FALSE);
    ShowWindow(p.wnd, SW_SHOW);
}

// updates the window and handles clicks; returns false once Cancel was pressed
bool progressStep(ProgressWin& p, int done, int total, const std::wstring& text) {
    SetWindowTextW(p.label, text.c_str());
    SendMessageW(p.bar, PBM_SETRANGE32, 0, std::max(total, 1));
    SendMessageW(p.bar, PBM_SETPOS, done, 0);
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return !p.cancelled;
}

void progressClose(ProgressWin& p) {
    EnableWindow(app.wnd, TRUE);
    DestroyWindow(p.wnd);
    SetForegroundWindow(app.wnd);
}

// ---- compare two documents (by text) ----
struct Word { int start, count; std::wstring text; };
std::vector<Word> wordsOf(Document& d, int page) {
    std::vector<Word> out;
    const int n = d.charCount(page);
    if (n <= 0) return out;
    std::wstring t = d.textRange(page, 0, n);
    size_t i = 0;
    while (i < t.size()) {
        while (i < t.size() && iswspace(t[i])) ++i;
        size_t s = i;
        while (i < t.size() && !iswspace(t[i])) ++i;
        if (i > s) out.push_back({static_cast<int>(s), static_cast<int>(i - s), t.substr(s, i - s)});
    }
    return out;
}

void compareTextDocs(Document* a, Document* b) {
    std::vector<unsigned char> bytes;
    if (!a->serialize(bytes)) { MessageBoxW(app.wnd, L"The document could not be prepared for comparing.", kAppName, MB_OK | MB_ICONWARNING); return; }
    auto copy = std::make_unique<Document>();
    if (copy->load(std::move(bytes), {}) != OpenResult::Ok) { MessageBoxW(app.wnd, L"The comparison copy could not be opened. (Password-protected documents cannot be compared.)", kAppName, MB_OK | MB_ICONWARNING); return; }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    int pagesDiff = 0, onlyA = 0, onlyB = 0, skipped = 0;
    const int pages = std::max(a->pageCount(), b->pageCount());
    for (int p = 0; p < pages; ++p) {
        std::vector<Word> wa = p < a->pageCount() ? wordsOf(*a, p) : std::vector<Word>{};
        std::vector<Word> wb = p < b->pageCount() ? wordsOf(*b, p) : std::vector<Word>{};
        const size_t n = wa.size(), m = wb.size();
        if (n * m > 9000000) { ++skipped; continue; }
        std::vector<char> keepA(n, 0);
        {   // longest common subsequence of the words
            std::vector<std::vector<unsigned short>> L(n + 1, std::vector<unsigned short>(m + 1, 0));
            for (size_t i = n; i-- > 0;)
                for (size_t j = m; j-- > 0;) L[i][j] = wa[i].text == wb[j].text ? static_cast<unsigned short>(L[i + 1][j + 1] + 1) : std::max(L[i + 1][j], L[i][j + 1]);
            size_t i = 0, j = 0;
            while (i < n && j < m) {
                if (wa[i].text == wb[j].text) { keepA[i] = 1; ++i; ++j; }
                else if (L[i + 1][j] >= L[i][j + 1]) ++i; else ++j;
            }
            onlyB += static_cast<int>(m - L[0][0]);
        }
        std::vector<annot::RectD> rects;
        FPDF_TEXTPAGE tp = p < copy->pageCount() ? copy->text(p) : nullptr;
        for (size_t i = 0; i < n; ++i) {
            if (keepA[i]) continue;
            ++onlyA;
            if (!tp) continue;
            const int cnt = pdfium::api().FPDFText_CountRects(tp, wa[i].start, wa[i].count);
            for (int k = 0; k < cnt; ++k) {
                double l, t, r, bt;
                if (pdfium::api().FPDFText_GetRect(tp, k, &l, &t, &r, &bt)) rects.push_back({l, bt, r, t});
            }
        }
        const bool differs = !rects.empty() || wa.size() != wb.size() || std::any_of(keepA.begin(), keepA.end(), [](char c) { return !c; });
        if (differs) ++pagesDiff;
        if (!rects.empty() && p < copy->pageCount()) annot::addMarkup(*copy, p, annot::Markup::Highlight, rects, RGB(0xFF, 0x8C, 0x00));
    }
    SetCursor(old);
    wchar_t msg[400];
    swprintf(msg, 400, L"Pages with differences: %d of %d.\nWords only in \"%ls\" (highlighted in the new tab): %d.\nWords only in \"%ls\": %d.%ls\n\nThis compares the text, not how the pages look.",
             pagesDiff, pages, a->title().c_str(), onlyA, b->title().c_str(), onlyB, skipped ? L"\nSome very large pages were skipped." : L"");
    copy->setName(L"Comparison - " + a->title());
    addDocumentTab(std::move(copy));
    MessageBoxW(app.wnd, msg, kAppName, MB_OK | MB_ICONINFORMATION);
}


void compareVisual(Document* a, Document* b) {
    const int pages = std::max(a->pageCount(), b->pageCount());
    std::vector<std::pair<double, double>> sizes;
    for (int p = 0; p < pages; ++p) {
        const PageSize ps = p < a->pageCount() ? a->pageSize(p) : b->pageSize(p);
        sizes.push_back({ps.w, ps.h});
    }
    std::vector<unsigned char> bytes;
    auto doc = std::make_unique<Document>();
    if (!Document::blankBytes(sizes, bytes) || doc->load(std::move(bytes), {}) != OpenResult::Ok) { MessageBoxW(app.wnd, L"The comparison could not be made.", kAppName, MB_OK | MB_ICONWARNING); return; }
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    ProgressWin pw;
    progressOpen(pw, L"Comparing the pages");
    int differing = 0, failed = 0;
    for (int p = 0; p < pages; ++p) {
        if (!progressStep(pw, p, pages, L"Page " + std::to_wstring(p + 1) + L" of " + std::to_wstring(pages))) break;
        const std::wstring png = std::wstring(tmp) + L"feather_cmp_" + std::to_wstring(p) + L".png";
        visualdiff::PageResult r = visualdiff::overlay(*a, p, *b, p, 100, png);
        std::wstring err;
        if (!r.ok || !pageobj::addPicture(*doc, p, 0, sizes[p].second, png, err, sizes[p].first)) ++failed;
        else if (r.percent > 0.02) ++differing;
        DeleteFileW(png.c_str());
    }
    progressClose(pw);
    doc->setName(L"Visual comparison - " + a->title());
    addDocumentTab(std::move(doc));
    wchar_t msg[400];
    swprintf(msg, 400, L"Pages that look different: %d of %d.\n\nRed: only in \"%ls\". Green: only in \"%ls\". Grey: the same.%ls",
             differing, pages, a->title().c_str(), b->title().c_str(), failed ? L"\n\nSome pages could not be compared." : L"");
    MessageBoxW(app.wnd, msg, kAppName, MB_OK | MB_ICONINFORMATION);
}

void compareDocs() {
    Document* a = activeDoc();
    if (!a || app.tabs.size() < 2) return;
    std::vector<Document*> others;
    std::vector<std::wstring> names;
    for (auto& t : app.tabs) if (t.get() != a) { others.push_back(t.get()); names.push_back(t->title()); }
    std::vector<FormField> f(3);
    f[0] = {FormField::Note, L"", {}, L"Compare \"" + a->title() + L"\" with another open document."};
    f[1] = {FormField::Choice, L"Compare with:", names, L"", 0};
    f[2] = {FormField::Choice, L"How:", {L"The words (text differences)", L"How the pages look (overlay)", L"Both"}, L"", 0};
    if (!showForm(app.wnd, app.inst, L"Compare documents", f, L"Compare")) return;
    Document* b = others[f[1].choice];
    if (f[2].choice != 1) compareTextDocs(a, b);
    if (f[2].choice != 0) compareVisual(a, b);
}

// ---- side by side ----
void sideBySideAction(int cmd) {
    if (cmd == N_SBS_OFF) {
        app.sbs = false; app.sbsDoc = nullptr; app.sbsOwned.reset();
        app.viewer2.setDocument(nullptr);
        ShowWindow(app.viewer2.hwnd(), SW_HIDE);
        relayoutChildren();
        return;
    }
    Document* other = nullptr;
    if (cmd == N_SBS_TAB) {
        std::vector<Document*> others;
        std::vector<std::wstring> names;
        for (auto& t : app.tabs) if (t.get() != activeDoc()) { others.push_back(t.get()); names.push_back(t->title()); }
        if (others.empty()) return;
        std::vector<FormField> f(1);
        f[0] = {FormField::Choice, L"Show next to this one:", names, L"", 0};
        if (!showForm(app.wnd, app.inst, L"Side by side", f, L"Show")) return;
        other = others[f[0].choice];
        app.sbsOwned.reset();
    } else if (cmd == N_SBS_OPEN) {
        std::vector<std::wstring> files;
        pickPdf(false, files, L"Open next to this document");
        if (files.empty()) return;
        auto doc = openForImport(files[0]);
        if (!doc) return;
        other = doc.get();
        app.sbsOwned = std::move(doc);
    }
    if (!other) return;
    app.sbs = true; app.sbsDoc = other;
    app.viewer2.setLayout(app.layout, app.cover);
    app.viewer2.setDocument(other);
    ShowWindow(app.viewer2.hwnd(), SW_SHOW);
    relayoutChildren();
    if (activeDoc()) { other->fitWidth = true; app.viewer2.fitWidth(); app.viewer.fitWidth(); }
}

// keeps the second view at the same zoom and position as the first
void syncSideBySide(bool fromFirst) {
    if (!app.sbs || app.syncing) return;
    Document* a = activeDoc();
    if (!a || !app.sbsDoc || a == app.sbsDoc) return;
    app.syncing = true;
    Viewer& dst = fromFirst ? app.viewer2 : app.viewer;
    Document* src = fromFirst ? a : app.sbsDoc;
    Document* tgt = fromFirst ? app.sbsDoc : a;
    if (std::fabs(tgt->zoom - src->zoom) > 1e-6) dst.setZoom(src->zoom);
    dst.scrollTo(src->scrollY);
    app.syncing = false;
}

// ---- a list of results to choose from (modal); returns the index or -1 ----
struct ListState { std::vector<std::wstring> items; HWND list = nullptr; bool done = false; int chosen = -1; };

LRESULT CALLBACK resultsProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<ListState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); break;
        case WM_SIZE: if (st && st->list) MoveWindow(st->list, 8, 8, LOWORD(l) - 16, HIWORD(l) - 52, TRUE); return 0;
        case WM_COMMAND:
            if (HIWORD(w) == LBN_DBLCLK) { int s = static_cast<int>(SendMessageW(st->list, LB_GETCURSEL, 0, 0)); if (s >= 0) { st->chosen = s; st->done = true; } }
            else if (LOWORD(w) == IDOK) { int s = static_cast<int>(SendMessageW(st->list, LB_GETCURSEL, 0, 0)); if (s >= 0) { st->chosen = s; st->done = true; } }
            else if (LOWORD(w) == IDCANCEL) st->done = true;
            return 0;
        case WM_CLOSE: st->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int pickFromList(const wchar_t* title, const std::vector<std::wstring>& items, const wchar_t* openText) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = resultsProc; wc.hInstance = app.inst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1); wc.lpszClassName = L"FeatherResults";
        RegisterClassExW(&wc);
        reg = true;
    }
    ListState st; st.items = items;
    RECT pr; GetWindowRect(app.wnd, &pr);
    const int W = 900, H = 520;
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FeatherResults", title, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, (pr.left + pr.right - W) / 2, (pr.top + pr.bottom - H) / 2, W, H, app.wnd, nullptr, app.inst, &st);
    HFONT f = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    st.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 8, 8, W - 30, H - 100, h, nullptr, app.inst, nullptr);
    HWND open = CreateWindowExW(0, L"BUTTON", openText, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 8, 0, 110, 28, h, reinterpret_cast<HMENU>(IDOK), app.inst, nullptr);
    HWND close = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 126, 0, 90, 28, h, reinterpret_cast<HMENU>(IDCANCEL), app.inst, nullptr);
    SendMessageW(st.list, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
    SendMessageW(open, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
    SendMessageW(close, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
    SendMessageW(st.list, LB_SETHORIZONTALEXTENT, 2400, 0);
    for (const auto& it : items) SendMessageW(st.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(it.c_str()));
    SendMessageW(st.list, LB_SETCURSEL, 0, 0);
    RECT cr; GetClientRect(h, &cr);
    MoveWindow(st.list, 8, 8, cr.right - 16, cr.bottom - 52, TRUE);
    MoveWindow(open, 8, cr.bottom - 36, 110, 28, TRUE);
    MoveWindow(close, 126, cr.bottom - 36, 90, 28, TRUE);
    EnableWindow(app.wnd, FALSE);
    ShowWindow(h, SW_SHOW);
    SetFocus(st.list);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(app.wnd, TRUE);
    DestroyWindow(h);
    SetForegroundWindow(app.wnd);
    return st.chosen;
}

// ---- search all my PDFs ----
void searchFolder() {
    wchar_t folder[MAX_PATH] = L"";
    BROWSEINFOW bi{};
    bi.hwndOwner = app.wnd;
    bi.lpszTitle = L"Choose the folder to search (its sub-folders are searched too)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    SHGetPathFromIDListW(pidl, folder);
    CoTaskMemFree(pidl);
    std::vector<FormField> f(2);
    f[0] = {FormField::Text, L"Find:", {}, app.viewer.hasSelection() ? app.viewer.selectedText() : L""};
    f[1] = {FormField::Check, L"Match case"};
    if (!showForm(app.wnd, app.inst, L"Search all my PDFs", f, L"Search", [](const std::vector<FormField>& v) -> std::wstring { return v[0].text.empty() ? L"Type the text to find." : L""; })) return;
    std::wstring dir = settingsPath();
    dir = dir.substr(0, dir.find_last_of(L'\\')) + L"\\index";
    ProgressWin pw;
    progressOpen(pw, L"Searching");
    foldersearch::Stats stats;
    auto hits = foldersearch::search(folder, f[0].text, f[1].checked, dir, [&](int d, int t, const std::wstring& file) {
        return progressStep(pw, d, t, file);
    }, stats);
    progressClose(pw);
    wchar_t summary[256];
    swprintf(summary, 256, L"%d result(s) in %d file(s) searched (%d read now, %d from the saved index, %d skipped).", static_cast<int>(hits.size()), stats.files, stats.scanned, stats.cached, stats.skipped);
    if (hits.empty()) { MessageBoxW(app.wnd, (std::wstring(L"Nothing was found.\n\n") + summary).c_str(), kAppName, MB_OK | MB_ICONINFORMATION); return; }
    std::vector<std::wstring> items;
    const size_t base = std::wstring(folder).size() + 1;
    for (const auto& h : hits) items.push_back(h.file.substr(std::min(base, h.file.size())) + L"   -   page " + std::to_wstring(h.page + 1) + L"   -   " + h.snippet);
    const std::wstring title = L"Search results: " + f[0].text + L"  (" + summary + L")";
    int pick = pickFromList(title.c_str(), items, L"Open");
    if (pick < 0) return;
    openFile(hits[pick].file);
    if (Document* d = activeDoc(); d && d->path() == hits[pick].file) {
        app.viewer.goToPage(hits[pick].page);
        SetWindowTextW(app.findEdit, f[0].text.c_str());
        SendMessageW(app.findCase, BM_SETCHECK, f[1].checked ? BST_CHECKED : BST_UNCHECKED, 0);
        toggleFind(true);
    }
}

// ---- batch mode ----
std::wstring presetsDir() {
    std::wstring s = settingsPath();
    return s.substr(0, s.find_last_of(L'\\')) + L"\\presets";
}

std::vector<std::wstring> presetNames() {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((presetsDir() + L"\\*.ini").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { std::wstring n = fd.cFileName; out.push_back(n.substr(0, n.size() - 4)); } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool browseFolder(const wchar_t* title, std::wstring& out) {
    wchar_t folder[MAX_PATH] = L"";
    BROWSEINFOW bi{};
    bi.hwndOwner = app.wnd; bi.lpszTitle = title; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return false;
    SHGetPathFromIDListW(pidl, folder);
    CoTaskMemFree(pidl);
    out = folder;
    return !out.empty();
}

void batchDialog() {
    batch::Job job;
    std::vector<std::wstring> names = presetNames();
    if (!names.empty()) {
        std::vector<FormField> f(2);
        std::vector<std::wstring> choices{L"New settings"};
        choices.insert(choices.end(), names.begin(), names.end());
        f[0] = {FormField::Choice, L"Start from:", choices, L"", 0};
        f[1] = {FormField::Check, L"Delete the chosen preset instead"};
        if (!showForm(app.wnd, app.inst, L"Batch: process a folder", f, L"Next")) return;
        if (f[0].choice > 0) {
            const std::wstring file = presetsDir() + L"\\" + names[f[0].choice - 1] + L".ini";
            if (f[1].checked) { DeleteFileW(file.c_str()); MessageBoxW(app.wnd, L"The preset was deleted.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
            batch::load(file, job);
        }
    }
    if (!browseFolder(L"Choose the folder with the PDF files to process", job.inputDir)) return;
    int where = MessageBoxW(app.wnd, L"Where should the results go?\n\nYes = next to the originals, named \"... (batch).pdf\"\nNo = choose another folder (the sub-folder structure is kept)\nCancel = stop", L"Batch", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (where == IDCANCEL) return;
    job.outputDir.clear();
    if (where == IDNO && !browseFolder(L"Choose the folder for the results", job.outputDir)) return;

    std::vector<FormField> f(11);
    f[0] = {FormField::Check, L"Recognise text (OCR)", {}, L"", 0, job.ocr};
    f[1] = {FormField::Text, L"OCR language:", {}, job.ocrLanguage};
    f[2] = {FormField::Text, L"Watermark text (empty = none):", {}, job.watermark};
    f[3] = {FormField::Choice, L"Compress:", {L"No", L"Balanced", L"High quality", L"Smallest file"}, L"", job.compress};
    f[4] = {FormField::Check, L"Greyscale pictures", {}, L"", 0, job.greyscale};
    f[5] = {FormField::Password, L"Password to open (empty = none):", {}, job.userPassword};
    f[6] = {FormField::Password, L"Owner password (empty = same):", {}, job.ownerPassword};
    f[7] = {FormField::Text, L"Sign with certificate file (.pfx):", {}, job.certificate};
    f[8] = {FormField::Password, L"Certificate password:", {}, job.certificatePassword};
    f[9] = {FormField::Check, L"Include sub-folders", {}, L"", 0, job.subfolders};
    f[10] = {FormField::Text, L"Save these settings as preset:", {}, L""};
    if (!showForm(app.wnd, app.inst, L"Batch: what to do with every file", f, L"Start", [](const std::vector<FormField>& v) -> std::wstring {
            if (!v[2].text.empty() && !pageobj::isLatin(v[2].text)) return L"The watermark can use letters, numbers and common symbols of Western languages.";
            if (!v[7].text.empty() && GetFileAttributesW(v[7].text.c_str()) == INVALID_FILE_ATTRIBUTES) return L"The certificate file was not found.";
            const bool any = v[0].checked || !v[2].text.empty() || v[3].choice > 0 || v[4].checked || !v[5].text.empty() || !v[6].text.empty() || !v[7].text.empty();
            return any ? L"" : L"Choose at least one thing to do."; })) return;
    job.ocr = f[0].checked; job.ocrLanguage = f[1].text.empty() ? L"eng" : f[1].text; job.watermark = f[2].text; job.compress = f[3].choice;
    job.greyscale = f[4].checked; job.userPassword = f[5].text; job.ownerPassword = f[6].text; job.certificate = f[7].text;
    job.certificatePassword = f[8].text; job.subfolders = f[9].checked;
    std::wstring pname = f[10].text;
    for (auto& c : pname) if (wcschr(L"\\/:*?\"<>|.", c)) c = L'_';
    if (!pname.empty()) {
        CreateDirectoryW(presetsDir().c_str(), nullptr);
        batch::save(presetsDir() + L"\\" + pname + L".ini", job);
    }
    ProgressWin pw;
    progressOpen(pw, L"Batch");
    batch::Result r = batch::run(job, [&](int d, int t, const std::wstring& file) { return progressStep(pw, d, t, file); });
    progressClose(pw);
    std::wstring msg = std::to_wstring(r.done) + L" of " + std::to_wstring(r.total) + L" file(s) processed";
    if (r.failed) msg += L", " + std::to_wstring(r.failed) + L" with problems";
    if (r.cancelled) msg += L" (stopped)";
    msg += L".";
    for (size_t i = 0; i < r.problems.size() && i < 10; ++i) msg += L"\n  - " + r.problems[i];
    if (r.problems.size() > 10) msg += L"\n  ... and " + std::to_wstring(r.problems.size() - 10) + L" more";
    if (!r.total) msg = L"There are no PDF files in that folder.";
    MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_OK | (r.failed ? MB_ICONWARNING : MB_ICONINFORMATION));
}

// ---- forms: export, fill from a spreadsheet, create fields, find blanks ----
std::wstring readTextFile(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    std::string raw(static_cast<size_t>(sz.QuadPart), '\0');
    DWORD got = 0;
    ReadFile(f, raw.data(), static_cast<DWORD>(raw.size()), &got, nullptr);
    CloseHandle(f);
    raw.resize(got);
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE)   // UTF-16 little endian
        return std::wstring(reinterpret_cast<const wchar_t*>(raw.data() + 2), (raw.size() - 2) / 2);
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
    UINT cp = CP_UTF8;
    if (n <= 0) { cp = CP_ACP; n = MultiByteToWideChar(CP_ACP, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0); }
    std::wstring w(n, L'\0');
    MultiByteToWideChar(cp, 0, raw.data(), static_cast<int>(raw.size()), w.data(), n);
    return w;
}

void formExport() {
    Document* d = activeDoc();
    if (!d) return;
    auto rows = formdata::answers(*d);
    if (rows.empty()) { MessageBoxW(app.wnd, L"This document has no form fields to read.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    wchar_t buf[MAX_PATH * 2];
    std::wstring base = d->title();
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    wcsncpy_s(buf, (base + L" - answers.xlsx").c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Save the answers as";
    ofn.lpstrFilter = L"Excel workbook (*.xlsx)\0*.xlsx\0CSV, opens in Excel (*.csv)\0*.csv\0";
    ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2; ofn.lpstrDefExt = L"xlsx";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return;
    const std::wstring path = buf;
    const bool xlsx = path.size() > 5 && _wcsicmp(path.c_str() + path.size() - 5, L".xlsx") == 0;
    std::wstring err;
    if (!exporter::writeRows(rows, xlsx, path, err)) { MessageBoxW(app.wnd, err.empty() ? L"The file could not be saved." : err.c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    MessageBoxW(app.wnd, (std::to_wstring(rows[0].size()) + L" answers were saved.").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}

void formFillFromCsv() {
    Document* d = activeDoc();
    if (!d) return;
    const auto fields = formdata::list(*d);
    if (fields.empty()) { MessageBoxW(app.wnd, L"This document has no form fields to fill in. Use Fillable form > Find fields in a flat form to make some.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    std::vector<wchar_t> buf(MAX_PATH * 2, 0);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Choose the spreadsheet (a CSV file; the first row holds the field names)";
    ofn.lpstrFilter = L"CSV files (*.csv;*.txt)\0*.csv;*.txt\0";
    ofn.lpstrFile = buf.data(); ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return;
    auto rows = formdata::parseCsv(readTextFile(buf.data()));
    if (rows.size() < 2) { MessageBoxW(app.wnd, L"The file needs a first row with the field names and at least one row of answers. Save the sheet from Excel as CSV.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    std::set<std::wstring> names;
    for (const auto& f : fields) names.insert(f.name);
    std::wstring unmatched;
    int matched = 0;
    for (const auto& h : rows[0]) { if (names.count(h)) ++matched; else if (!h.empty()) unmatched += (unmatched.empty() ? L"" : L", ") + h; }
    if (!matched) {
        std::wstring have;
        for (const auto& n : names) have += (have.empty() ? L"" : L", ") + n;
        MessageBoxW(app.wnd, (L"None of the column names matches a field of this document.\n\nThe fields are called: " + have).c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring folder;
    if (!browseFolder(L"Choose the folder for the filled-in copies (one file per row)", folder)) return;
    std::vector<unsigned char> original;
    if (!d->serialize(original)) { MessageBoxW(app.wnd, L"The document could not be prepared.", kAppName, MB_OK | MB_ICONWARNING); return; }
    std::wstring base = d->title();
    size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    ProgressWin pw;
    progressOpen(pw, L"Filling in the form");
    int done = 0, failed = 0;
    for (size_t r = 1; r < rows.size(); ++r) {
        if (!progressStep(pw, static_cast<int>(r - 1), static_cast<int>(rows.size() - 1), L"Row " + std::to_wstring(r))) break;
        std::map<std::wstring, std::wstring> values;
        for (size_t c = 0; c < rows[0].size() && c < rows[r].size(); ++c) values[rows[0][c]] = rows[r][c];
        Document copy;
        std::vector<unsigned char> bytes = original;
        if (copy.load(std::move(bytes), {}) != OpenResult::Ok) { ++failed; continue; }
        formdata::setValues(copy, values);
        std::vector<unsigned char> out;
        std::wstring label = rows[r].empty() ? L"" : rows[r][0];
        for (auto& ch : label) if (wcschr(L"\\/:*?\"<>|", ch)) ch = L'_';
        if (label.size() > 40) label.resize(40);
        std::wstring file = folder + L"\\" + base + L" - " + std::to_wstring(r) + (label.empty() ? L"" : L" " + label) + L".pdf";
        if (copy.serialize(out) && Document::writeFile(file, out) == SaveResult::Ok) ++done; else ++failed;
    }
    progressClose(pw);
    std::wstring msg = std::to_wstring(done) + L" filled-in copies were saved in the chosen folder";
    if (failed) msg += L"; " + std::to_wstring(failed) + L" could not be made";
    msg += L".";
    if (!unmatched.empty()) msg += L"\n\nColumns that matched no field and were ignored: " + unmatched;
    MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}

bool addFieldSpecs(Document& d, const std::vector<formcreate::Spec>& specs) {
    std::vector<unsigned char> bytes, out;
    std::wstring err;
    if (!d.serialize(bytes) || !formcreate::add(bytes, specs, out, err)) {
        MessageBoxW(app.wnd, (L"The fields could not be added. " + err).c_str(), kAppName, MB_OK | MB_ICONWARNING);
        return false;
    }
    d.beginChange();
    if (d.reload(std::move(out)) != OpenResult::Ok) {
        d.cancelChange();
        MessageBoxW(app.wnd, L"The fields could not be added to this document.", kAppName, MB_OK | MB_ICONWARNING);
        return false;
    }
    app.viewer.documentChanged();
    app.sidebar.refresh();
    refresh();
    return true;
}

void startFormField(Viewer::Tool t) {
    if (!activeDoc()) return;
    if (app.tool != t) setTool(t);
}

bool formFieldDraw(Document& d, const Viewer::Event& e) {
    formcreate::Spec sp;
    sp.page = e.page;
    sp.left = std::min(e.pts[0].x, e.pts[1].x); sp.right = std::max(e.pts[0].x, e.pts[1].x);
    sp.bottom = std::min(e.pts[0].y, e.pts[1].y); sp.top = std::max(e.pts[0].y, e.pts[1].y);
    if (sp.right - sp.left < 6 || sp.top - sp.bottom < 6) return true;
    std::set<std::wstring> used;
    for (const auto& f : formdata::list(d)) used.insert(f.name);
    const wchar_t* what = e.tool == Viewer::Tool::FormText ? L"text field" : e.tool == Viewer::Tool::FormCheck ? L"check box" : L"drop-down list";
    std::wstring suggestion = e.tool == Viewer::Tool::FormText ? L"Text" : e.tool == Viewer::Tool::FormCheck ? L"Check" : L"Choice";
    for (int k = 1; used.count(suggestion + std::to_wstring(k)); ++k) {}
    int k = 1;
    while (used.count(suggestion + std::to_wstring(k))) ++k;
    std::wstring name = suggestion + std::to_wstring(k);
    std::wstring in;
    while (askText(L"Form field", std::wstring(L"Name of the ") + what + L" (it must be different from the other fields).", false, in, name.c_str())) {
        if (in.empty() || used.count(in)) { MessageBoxW(app.wnd, L"Type a name that no other field has.", kAppName, MB_OK | MB_ICONINFORMATION); name = in; continue; }
        sp.name = in;
        sp.kind = e.tool == Viewer::Tool::FormText ? formcreate::Spec::Text : e.tool == Viewer::Tool::FormCheck ? formcreate::Spec::Check : formcreate::Spec::Combo;
        if (sp.kind == formcreate::Spec::Combo) {
            std::wstring opts;
            if (!askText(L"Drop-down list", L"The choices, one per line.", false, opts, L"Yes\r\nNo", true)) break;
            opts = fromEdit(opts);
            size_t pos = 0;
            while (pos <= opts.size()) {
                size_t z = opts.find(L'\r', pos);
                std::wstring line = opts.substr(pos, z == std::wstring::npos ? std::wstring::npos : z - pos);
                if (!line.empty()) sp.options.push_back(line);
                if (z == std::wstring::npos) break;
                pos = z + 1;
            }
            if (sp.options.empty()) break;
        } else if (sp.kind == formcreate::Spec::Text && sp.top - sp.bottom > 40) sp.multiline = true;
        addFieldSpecs(d, {sp});
        break;
    }
    setTool(e.tool);
    return true;
}

void formAutoDetect() {
    Document* d = activeDoc();
    if (!d) return;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    auto specs = formdata::detect(*d);
    SetCursor(old);
    if (specs.empty()) { MessageBoxW(app.wnd, L"No blanks were found. Feather PDF looks for lines of underscores, a rule after a label that ends with a colon, and small squares with text after them.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    int text = 0, check = 0;
    std::wstring sample;
    for (const auto& s : specs) { (s.kind == formcreate::Spec::Text ? text : check)++; if (sample.size() < 160) sample += L"\n  " + s.name; }
    std::wstring msg = L"Found " + std::to_wstring(text) + L" text field(s) and " + std::to_wstring(check) + L" check box(es):" + sample + (specs.size() > 6 ? L"\n  ..." : L"") + L"\n\nAdd them as fillable fields?";
    if (MessageBoxW(app.wnd, msg.c_str(), L"Find fields", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    addFieldSpecs(*d, specs);
}

// ---- command palette (Ctrl+K) ----
struct PaletteCmd { std::wstring name; std::function<void()> run; };
struct PaletteState { std::vector<PaletteCmd> all; std::vector<int> shown; HWND edit = nullptr, list = nullptr; bool done = false; int chosen = -1; };

LRESULT CALLBACK paletteEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<PaletteState*>(GetWindowLongPtrW(GetParent(h), GWLP_USERDATA));
    if (m == WM_KEYDOWN) {
        if (w == VK_ESCAPE) { st->done = true; return 0; }
        if (w == VK_RETURN) {
            int sel = static_cast<int>(SendMessageW(st->list, LB_GETCURSEL, 0, 0));
            if (sel >= 0 && sel < static_cast<int>(st->shown.size())) st->chosen = st->shown[sel];
            st->done = true;
            return 0;
        }
        if (w == VK_DOWN || w == VK_UP) {
            int n = static_cast<int>(SendMessageW(st->list, LB_GETCOUNT, 0, 0));
            int sel = static_cast<int>(SendMessageW(st->list, LB_GETCURSEL, 0, 0)) + (w == VK_DOWN ? 1 : -1);
            if (n > 0) SendMessageW(st->list, LB_SETCURSEL, std::clamp(sel, 0, n - 1), 0);
            return 0;
        }
    }
    return CallWindowProcW(reinterpret_cast<WNDPROC>(GetPropW(h, L"orig")), h, m, w, l);
}

LRESULT CALLBACK paletteProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* st = reinterpret_cast<PaletteState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); break;
        case WM_COMMAND:
            if (HIWORD(w) == EN_CHANGE && reinterpret_cast<HWND>(l) == st->edit) {
                wchar_t q[128];
                GetWindowTextW(st->edit, q, 128);
                std::wstring lq = q;
                for (auto& c : lq) c = static_cast<wchar_t>(towlower(c));
                SendMessageW(st->list, LB_RESETCONTENT, 0, 0);
                st->shown.clear();
                for (size_t i = 0; i < st->all.size(); ++i) {
                    std::wstring ln = st->all[i].name;
                    for (auto& c : ln) c = static_cast<wchar_t>(towlower(c));
                    bool ok = true;
                    size_t pos = 0;
                    std::wstringstream ss(lq);
                    std::wstring word;
                    while (ss >> word) { if (ln.find(word) == std::wstring::npos) { ok = false; break; } (void)pos; }
                    if (ok) { st->shown.push_back(static_cast<int>(i)); SendMessageW(st->list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(st->all[i].name.c_str())); }
                }
                if (!st->shown.empty()) SendMessageW(st->list, LB_SETCURSEL, 0, 0);
            } else if (HIWORD(w) == LBN_DBLCLK && reinterpret_cast<HWND>(l) == st->list) {
                int sel = static_cast<int>(SendMessageW(st->list, LB_GETCURSEL, 0, 0));
                if (sel >= 0) { st->chosen = st->shown[sel]; st->done = true; }
            }
            return 0;
        case WM_ERASEBKGND: {
            RECT r; GetClientRect(h, &r);
            HBRUSH b = CreateSolidBrush(pal().panel);
            FillRect(reinterpret_cast<HDC>(w), &r, b);
            DeleteObject(b);
            return 1;
        }
        case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE) st->done = true; return 0;
        case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(w);
            SetTextColor(dc, pal().text);
            SetBkColor(dc, pal().panel);
            static HBRUSH br = nullptr;
            if (br) DeleteObject(br);
            br = CreateSolidBrush(pal().panel);
            return reinterpret_cast<LRESULT>(br);
        }
    }
    return DefWindowProcW(h, m, w, l);
}

void commandPalette() {
    PaletteState st;
    auto add = [&](const wchar_t* n, std::function<void()> f) { st.all.push_back({n, std::move(f)}); };
    Document* d = activeDoc();
    add(L"Open a file", [] { runCommand(C_OPEN); });
    add(L"Search all my PDFs in a folder", [] { searchFolder(); });
    add(L"Batch: process a whole folder", [] { batchDialog(); });
    if (d) {
        add(L"Form: export the answers to Excel or CSV", [] { formExport(); });
        add(L"Form: fill from a spreadsheet", [] { formFillFromCsv(); });
        add(L"Form: add a text field", [] { startFormField(Viewer::Tool::FormText); });
        add(L"Form: add a check box", [] { startFormField(Viewer::Tool::FormCheck); });
        add(L"Form: add a drop-down list", [] { startFormField(Viewer::Tool::FormCombo); });
        add(L"Form: find fields in a flat form", [] { formAutoDetect(); });
    }
    add(L"New PDF from pictures", [] { newPdfFromPictures(); });
    if (d) {
        add(L"Save", [] { saveActive(); });
        add(L"Save as", [] { if (activeDoc()) saveAsDialog(*activeDoc()); });
        add(L"Close tab", [] { closeTab(app.active); });
        add(L"Print", [] { printDocument(); });
        add(L"Document properties", [] { showProperties(); });
        add(L"Find", [] { runCommand(C_FIND); });
        add(L"Find and replace text", [] { doFindReplace(); });
        add(L"Go to page", [] { runCommand(C_GOTO); });
        add(L"First page", [] { app.viewer.goToPage(0); });
        add(L"Last page", [] { if (activeDoc()) app.viewer.goToPage(activeDoc()->pageCount() - 1); });
        add(L"Back", [] { navBack(); });
        add(L"Forward", [] { navForward(); });
        add(L"Reload from disk", [] { reloadFromDisk(); });
        add(L"Fit width", [] { runCommand(C_FITW); });
        add(L"Fit page", [] { runCommand(C_FITP); });
        add(L"Actual size", [] { runCommand(C_ACTUAL); });
        add(L"Zoom in", [] { runCommand(C_ZOOMIN); });
        add(L"Zoom out", [] { runCommand(C_ZOOMOUT); });
        add(L"Page layout: single page", [] { setPageLayout(0); });
        add(L"Page layout: continuous", [] { setPageLayout(1); });
        add(L"Page layout: two pages", [] { setPageLayout(2); });
        add(L"Rotate view left", [] { rotateView(-1); });
        add(L"Rotate view right", [] { rotateView(1); });
        add(L"Full screen", [] { setFullscreen(true, false); });
        add(L"Presentation", [] { setFullscreen(true, true); });
        add(L"Bookmarks panel", [] { showSidebar(Sidebar::Mode::Bookmarks); });
        add(L"Thumbnails panel", [] { showSidebar(Sidebar::Mode::Pages); });
        add(L"Edit text (one line)", [] { toolsDispatch(T_EDIT_TEXT); });
        add(L"Edit paragraph", [] { toolsDispatch(N_EDITPARA); });
        add(L"Move or resize pictures, paragraphs and tables", [] { toolsDispatch(N_SELECTOBJ); });
        add(L"Add text box", [] { toolsDispatch(T_ADD_TEXT); });
        add(L"Insert table", [] { tableInsert(); });
        add(L"Edit a table", [] { tableEdit(); });
        add(L"Delete table", [] { tableDelete(); });
        add(L"Add a bookmark for this page", [] { addBookmarkHere(); });
        add(L"Highlight", [] { markupSelection(annot::Markup::Highlight, false); });
        add(L"Underline", [] { markupSelection(annot::Markup::Underline, false); });
        add(L"Strikethrough", [] { markupSelection(annot::Markup::Strikethrough, false); });
        add(L"Add comment", [] { setTool(Viewer::Tool::Comment); });
        add(L"Rectangle", [] { setTool(Viewer::Tool::Rectangle); });
        add(L"Ellipse", [] { setTool(Viewer::Tool::Ellipse); });
        add(L"Line", [] { setTool(Viewer::Tool::Line); });
        add(L"Arrow", [] { setTool(Viewer::Tool::Arrow); });
        add(L"Pen", [] { setTool(Viewer::Tool::Pen); });
        add(L"Insert picture", [] { startPicture(); });
        add(L"Link (drag a box)", [] { startLink(); });
        add(L"Signature: draw", [] { signatureAction(N_SIG_DRAW); });
        add(L"Signature: type my name", [] { signatureAction(N_SIG_TYPE); });
        add(L"Ruler (measure a distance)", [] { geometryAction(G_RULER); });
        add(L"Protractor (measure an angle)", [] { geometryAction(G_PROT); });
        add(L"Copy page as image", [] { if (activeDoc()) pageobj::copyPageToClipboard(*activeDoc(), app.viewer.currentPage(), app.wnd); });
        add(L"Copy area as image", [] { startCopyArea(); });
        add(L"Notes summary", [] { toolsDispatch(T_NOTES); });
        add(L"Recognise text (OCR)", [] { toolsDispatch(T_OCR); });
        add(L"Watermark", [] { toolsDispatch(T_WATERMARK); });
        add(L"Page numbers", [] { toolsDispatch(T_NUMBERS); });
        add(L"Compress", [] { toolsDispatch(T_COMPRESS); });
        add(L"Black and white / greyscale pictures", [] { doGreyscale(); });
        add(L"Password protection", [] { toolsDispatch(T_PROTECT); });
        add(L"Sign with a certificate", [] { toolsDispatch(T_SIGN); });
        add(L"Export", [] { toolsDispatch(T_EXPORT); });
        add(L"Read aloud", [] { readAloud(); });
        add(L"Compare with another open tab", [] { compareDocs(); });
        add(L"Split into several files", [] { splitPdf(); });
        add(L"Crop by drawing a box", [] { cropByBox(); });
        add(L"Check that redacted text is gone", [] { toolsDispatch(N_RED_CHECK); });
        add(L"Delete pages", [] { pagesMenuAction(P_DELETE, {}); });
        add(L"Insert blank page", [] { pagesMenuAction(P_BLANK, {}); });
    }
    add(L"Settings", [] { showSettings(); });
    add(L"Theme: light", [] { app.theme = 0; applyTheme(); saveSettings(); });
    add(L"Theme: dark", [] { app.theme = 1; applyTheme(); saveSettings(); });
    add(L"Theme: like Windows", [] { app.theme = 2; applyTheme(); saveSettings(); });
    add(L"About Feather PDF", [] { showAbout(); });

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = paletteProc; wc.hInstance = app.inst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; wc.lpszClassName = L"FeatherPalette";
        RegisterClassExW(&wc);
        reg = true;
    }
    RECT pr; GetWindowRect(app.wnd, &pr);
    const int W = 520, H = 360;
    HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, L"FeatherPalette", L"", WS_POPUP | WS_BORDER, (pr.left + pr.right - W) / 2, pr.top + 90, W, H, app.wnd, nullptr, app.inst, &st);
    if (!h) return;
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    st.edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, 10, W - 22, 26, h, nullptr, app.inst, nullptr);
    st.list = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 10, 44, W - 22, H - 56, h, nullptr, app.inst, nullptr);
    SendMessageW(st.edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(st.list, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(st.edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Type a command..."));
    SetPropW(st.edit, L"orig", reinterpret_cast<HANDLE>(SetWindowLongPtrW(st.edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(paletteEditProc))));
    for (size_t i = 0; i < st.all.size(); ++i) { st.shown.push_back(static_cast<int>(i)); SendMessageW(st.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(st.all[i].name.c_str())); }
    SendMessageW(st.list, LB_SETCURSEL, 0, 0);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetFocus(st.edit);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    DestroyWindow(h);
    SetForegroundWindow(app.wnd);
    SetFocus(app.wnd);
    if (st.chosen >= 0) st.all[st.chosen].run();
}

// ---- header drawing: tabs + toolbar. Icons are drawn with GDI lines, so nothing depends on icon fonts. ----
enum Icon { I_NONE, I_OPEN, I_SAVE, I_UNDO, I_REDO, I_EDIT, I_ANNOT, I_COMMENT, I_GEOM, I_SBS, I_PREV, I_NEXT, I_ZOUT, I_ZIN, I_SEARCH, I_MORE };

void drawIcon(HDC dc, RECT r, int icon, COLORREF col) {
    const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, 2, col), thin = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HBRUSH solid = CreateSolidBrush(col);
    auto line = [&](int x1, int y1, int x2, int y2) { MoveToEx(dc, cx + x1, cy + y1, nullptr); LineTo(dc, cx + x2, cy + y2); };
    auto box = [&](int x1, int y1, int x2, int y2) { Rectangle(dc, cx + x1, cy + y1, cx + x2, cy + y2); };
    switch (icon) {
        case I_OPEN:   // folder
            box(-9, -4, 9, 8); line(-9, -4, -9, -7); line(-9, -7, -3, -7); line(-3, -7, 0, -4); break;
        case I_SAVE:   // floppy
            box(-8, -8, 8, 8); box(-4, -8, 4, -2); box(-5, 2, 5, 8); break;
        case I_UNDO: case I_REDO: {
            const int d = icon == I_UNDO ? 1 : -1;
            Arc(dc, cx - 8, cy - 6, cx + 8, cy + 10, cx + 8 * d, cy + 2, cx - 6 * d, cy - 6);
            line(-7 * d, -5, -7 * d, 1); line(-7 * d, -5, -1 * d, -5); break;
        }
        case I_EDIT:   // page with a pencil
            box(-8, -9, 4, 9); line(-5, -4, 1, -4); line(-5, 0, 1, 0); line(-5, 4, -1, 4);
            line(9, -7, 3, 5); line(3, 5, 1, 7); line(9, -7, 11, -5); break;
        case I_ANNOT:  // highlighter
            line(-8, 8, 8, 8); line(-6, 2, 4, -8); line(-2, 6, 8, -4); line(-6, 2, -2, 6); line(4, -8, 8, -4); break;
        case I_COMMENT: // speech bubble
            box(-9, -8, 9, 4); line(-4, 4, -6, 9); line(-6, 9, 1, 4); line(-5, -3, 5, -3); line(-5, 0, 2, 0); break;
        case I_GEOM:   // triangle with ticks
            line(-8, 8, 8, 8); line(-8, 8, -8, -8); line(-8, -8, 8, 8); line(-8, 2, -4, 2); line(-8, -3, -5, -3); break;
        case I_SBS:    // two panes
            box(-9, -7, -1, 7); box(1, -7, 9, 7); break;
        case I_PREV: line(3, -7, -3, 0); line(-3, 0, 3, 7); break;
        case I_NEXT: line(-3, -7, 3, 0); line(3, 0, -3, 7); break;
        case I_ZOUT: case I_ZIN: case I_SEARCH:
            Ellipse(dc, cx - 8, cy - 8, cx + 4, cy + 4); line(3, 3, 9, 9);
            if (icon != I_SEARCH) { line(-5, -2, 1, -2); if (icon == I_ZIN) line(-2, -5, -2, 1); }
            break;
        case I_MORE:
            for (int i = -1; i <= 1; ++i) { RECT d{cx + i * 6 - 2, cy - 2, cx + i * 6 + 2, cy + 2}; FillRect(dc, &d, solid); }
            break;
    }
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(pen); DeleteObject(thin); DeleteObject(solid);
}

void layoutHeader(HDC dc, int width) {
    app.buttons.clear(); app.tabRects.clear(); app.closeRects.clear();
    Document* d = activeDoc();
    SelectObject(dc, app.font);

    int x = 8;
    for (size_t i = 0; i < app.tabs.size(); ++i) {
        SIZE sz; std::wstring t = tabLabel(*app.tabs[i]);
        GetTextExtentPoint32W(dc, t.c_str(), static_cast<int>(t.size()), &sz);
        int w = std::clamp<int>(sz.cx + 44, 90, 240);
        app.tabRects.push_back({x, 4, x + w, kTabH});
        app.closeRects.push_back({x + w - 26, 10, x + w - 8, 28});
        x += w + 2;
    }
    app.plusRect = {x + 4, 8, x + 28, 30};

    const int top = kTabH + 4, bottom = kTabH + kToolH - 4;
    auto icon = [&](int id, int ic, bool enabled) {
        app.buttons.push_back({L"", {x, top, x + 34, bottom}, id, enabled, ic});
        x += 36;
    };
    auto group = [&](const wchar_t* label, int id, int ic, bool enabled) {
        SIZE sz; GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &sz);
        int w = 12 + 22 + 6 + sz.cx + 12;
        app.buttons.push_back({label, {x, top, x + w, bottom}, id, enabled, ic});
        x += w + 2;
    };
    x = 8;
    icon(C_OPEN, I_OPEN, true);
    icon(C_SAVE, I_SAVE, d && d->dirty());
    icon(C_UNDO, I_UNDO, d && d->canUndo());
    icon(C_REDO, I_REDO, d && d->canRedo());
    x += 10;
    group(L"Edit PDF", C_EDITPDF, I_EDIT, d != nullptr);
    group(L"Annotate", C_ANNOT, I_ANNOT, d != nullptr);
    group(L"Comment", C_COMMENT, I_COMMENT, d != nullptr);
    group(L"Geometry", C_GEOM, I_GEOM, d != nullptr);
    group(L"Side by side", C_SBS, I_SBS, d != nullptr);
    x += 12;
    icon(C_PREV, I_PREV, d != nullptr);
    icon(C_NEXT, I_NEXT, d != nullptr);
    app.pageRect = {x, top + 2, x + 56, bottom - 2};
    x += 62 + 52;   // page box + "/ n"
    icon(C_ZOOMOUT, I_ZOUT, d != nullptr);
    {
        wchar_t z[16];
        swprintf(z, 16, L"%d%%", d ? static_cast<int>(d->zoom * 100 + 0.5) : 100);
        app.buttons.push_back({z, {x, top, x + 62, bottom}, C_ZOOMMENU, d != nullptr, 0});
        x += 64;
    }
    icon(C_ZOOMIN, I_ZIN, d != nullptr);
    int rx = width - 8;
    rx -= 34; app.buttons.push_back({L"", {rx, top, rx + 34, bottom}, C_MENU, true, I_MORE});
    rx -= 36; app.buttons.push_back({L"", {rx, top, rx + 34, bottom}, C_FIND, d != nullptr, I_SEARCH});
}

void paintHeader(HDC dc, const RECT& client) {
    layoutHeader(dc, client.right);
    const Palette& P = pal();
    HBRUSH tabBg = CreateSolidBrush(P.tabBar);
    HBRUSH toolBg = CreateSolidBrush(P.toolbar);
    RECT tr{0, 0, client.right, kTabH}, br{0, kTabH, client.right, kTabH + kToolH};
    FillRect(dc, &tr, tabBg); FillRect(dc, &br, toolBg);
    if (app.findVisible) {   // the find bar sits between the toolbar and the page area
        RECT fr{0, kTabH + kToolH, client.right, kTabH + kToolH + kFindH};
        FillRect(dc, &fr, toolBg);
    }
    DeleteObject(tabBg); DeleteObject(toolBg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, app.font);

    if (app.tabs.empty()) {
        RECT r{8, 4, 128, kTabH};
        HBRUSH w = CreateSolidBrush(P.tabActive); FillRect(dc, &r, w); DeleteObject(w);
        SetTextColor(dc, P.text);
        DrawTextW(dc, L"New tab", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (size_t i = 0; i < app.tabRects.size(); ++i) {
        bool act = static_cast<int>(i) == app.active;
        HBRUSH w = CreateSolidBrush(act ? P.tabActive : P.tabIdle);
        FillRect(dc, &app.tabRects[i], w); DeleteObject(w);
        RECT t = app.tabRects[i]; t.left += 12; t.right -= 30;
        SetTextColor(dc, P.text);
        DrawTextW(dc, tabLabel(*app.tabs[i]).c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, P.textDim);
        DrawTextW(dc, L"x", -1, &app.closeRects[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (!app.tabs.empty()) {
        SetTextColor(dc, P.textDim);
        DrawTextW(dc, L"+", -1, &app.plusRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    for (const Button& b : app.buttons) {
        const COLORREF col = b.enabled ? P.text : P.textOff;
        if (b.enabled && app.hover == b.id) {
            HBRUSH hb = CreateSolidBrush(P.hover);
            HGDIOBJ opn = SelectObject(dc, GetStockObject(NULL_PEN)), obr = SelectObject(dc, hb);
            RoundRect(dc, b.rc.left, b.rc.top, b.rc.right, b.rc.bottom, 8, 8);
            SelectObject(dc, obr); SelectObject(dc, opn); DeleteObject(hb);
        }
        SetTextColor(dc, col);
        if (b.icon && b.label.empty()) drawIcon(dc, b.rc, b.icon, col);
        else if (b.icon) {
            RECT ir{b.rc.left + 8, b.rc.top, b.rc.left + 34, b.rc.bottom};
            drawIcon(dc, ir, b.icon, col);
            RECT tr2{b.rc.left + 36, b.rc.top, b.rc.right, b.rc.bottom};
            DrawTextW(dc, b.label.c_str(), -1, &tr2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else DrawTextW(dc, b.label.c_str(), -1, const_cast<RECT*>(&b.rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    Document* d = activeDoc();
    if (d) {
        // page number box and "/ n"
        HBRUSH pb = CreateSolidBrush(P.panelDark);
        HPEN bp = CreatePen(PS_SOLID, 1, P.line);
        HGDIOBJ opn = SelectObject(dc, bp), obr = SelectObject(dc, pb);
        RoundRect(dc, app.pageRect.left, app.pageRect.top, app.pageRect.right, app.pageRect.bottom, 6, 6);
        SelectObject(dc, obr); SelectObject(dc, opn); DeleteObject(pb); DeleteObject(bp);
        wchar_t s[64];
        SetTextColor(dc, P.text);
        swprintf(s, 64, L"%d", app.viewer.currentPage() + 1);
        DrawTextW(dc, s, -1, &app.pageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        swprintf(s, 64, L"/ %d", d->pageCount());
        RECT pr{app.pageRect.right + 6, app.pageRect.top, app.pageRect.right + 56, app.pageRect.bottom};
        SetTextColor(dc, P.textDim);
        DrawTextW(dc, s, -1, &pr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, P.line);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, 0, kTabH + kToolH - 1, nullptr); LineTo(dc, client.right, kTabH + kToolH - 1);
    SelectObject(dc, old); DeleteObject(pen);
}

void openUri(const std::wstring& uri) {
    // only web and mail addresses are opened; a PDF must not be able to start programs
    auto starts = [&](const wchar_t* p) { return _wcsnicmp(uri.c_str(), p, wcslen(p)) == 0; };
    if (!(starts(L"http://") || starts(L"https://") || starts(L"mailto:"))) {
        MessageBoxW(app.wnd, L"This link does not point to a web page, so it was not opened.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (MessageBoxW(app.wnd, (L"Open this link in your browser?\n\n" + uri).c_str(), kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES)
        ShellExecuteW(app.wnd, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void onViewer2Change(void*) { syncSideBySide(false); }

void onViewerChange(void*) {
    RECT r{0, 0, 4000, kTabH + kToolH};
    InvalidateRect(app.wnd, &r, FALSE);
    syncSideBySide(true);
    if (activeDoc() && app.sidebarVisible) app.sidebar.setCurrentPage(app.viewer.currentPage());
}

void onSidebarNavigate(void*, int page) { app.viewer.goToPage(page); }

// The search box: Enter = next match, Shift+Enter = previous, Esc = close the bar.
LRESULT CALLBACK findEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) { stepMatch(GetKeyState(VK_SHIFT) < 0 ? -1 : 1); return 0; }
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { toggleFind(false); return 0; }
    if (m == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE)) return 0;
    return CallWindowProcW(app.editProc, h, m, w, l);
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
            Document::setTimerWindow(app.viewer.hwnd());
            app.viewer.setOnChange(onViewerChange, nullptr);
            SetTimer(h, TIMER_AUTOSAVE, 20000, nullptr);
            app.viewer.onEvent = onViewerEvent;
            app.viewer.onJump = onJump;
            app.viewer.onUri = openUri;
            app.viewer2.create(h, app.inst);
            ShowWindow(app.viewer2.hwnd(), SW_HIDE);
            app.viewer2.setOnChange(onViewer2Change, nullptr);
            app.viewer.setLayout(app.layout, app.cover);
            app.viewer.setStyle(drawColor(), lineWidth());
            app.sidebar.create(h, app.inst);
            app.sidebar.setOnNavigate(onSidebarNavigate, nullptr);
            app.sidebar.onContext = showThumbMenu;
            app.sidebar.onReorder = onThumbReorder;
            app.findEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_EDIT), app.inst, nullptr);
            app.findPrev = CreateWindowExW(0, L"BUTTON", L"Prev", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_PREV), app.inst, nullptr);
            app.findNext = CreateWindowExW(0, L"BUTTON", L"Next", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_NEXT), app.inst, nullptr);
            app.findCase = CreateWindowExW(0, L"BUTTON", L"Match case", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_CASE), app.inst, nullptr);
            app.findCount = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_COUNT), app.inst, nullptr);
            app.findClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_CLOSE), app.inst, nullptr);
            for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findCount, app.findClose})
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
            app.editProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(app.findEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(findEditProc)));
            DragAcceptFiles(h, TRUE);
            return 0;
        }
        case WM_SIZE: relayoutChildren(); InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case ID_FIND_EDIT:
                    if (HIWORD(w) == EN_CHANGE) SetTimer(h, TIMER_FIND, 300, nullptr);   // search once typing pauses
                    break;
                case ID_FIND_PREV: stepMatch(-1); break;
                case ID_FIND_NEXT: stepMatch(1); break;
                case ID_FIND_CASE: runSearch(false); break;
                case ID_FIND_CLOSE: toggleFind(false); break;
            }
            return 0;
        case WM_TIMER:
            if (w == TIMER_FIND) { KillTimer(h, TIMER_FIND); runSearch(false); }
            else if (w == TIMER_AUTOSAVE) writeSession();
            return 0;
        case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
            HDC cdc = reinterpret_cast<HDC>(w);
            SetTextColor(cdc, pal().text);
            SetBkColor(cdc, m == WM_CTLCOLOREDIT ? (pal().dark ? RGB(0x2B, 0x2B, 0x2B) : RGB(0xFF, 0xFF, 0xFF)) : pal().toolbar);
            if (m == WM_CTLCOLOREDIT) { static HBRUSH eb = nullptr; if (eb) DeleteObject(eb); eb = CreateSolidBrush(pal().dark ? RGB(0x2B, 0x2B, 0x2B) : RGB(0xFF, 0xFF, 0xFF)); return reinterpret_cast<LRESULT>(eb); }
            return reinterpret_cast<LRESULT>(app.ctlBrush);
        }
        case WM_SETTINGCHANGE:
            if (app.theme == 2 && l && wcscmp(reinterpret_cast<const wchar_t*>(l), L"ImmersiveColorSet") == 0) applyTheme();
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC screenDc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            PaintBuf buf(screenDc, rc, ps.rcPaint);
            if (!app.fullscreen) paintHeader(buf.dc, rc);
            buf.done();
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
            if (activeDoc() && hit(app.pageRect, x, y)) { runCommand(C_GOTO); return 0; }
            for (const Button& b : app.buttons)
                if (b.enabled && hit(b.rc, x, y)) {
                    if (b.id == C_ANNOT) showAnnotateMenu(b.rc);
                    else if (b.id == C_EDITPDF) showToolsMenu(b.rc);
                    else if (b.id == C_COMMENT) showCommentMenu(b.rc);
                    else if (b.id == C_GEOM) showGeometryMenu(b.rc);
                    else if (b.id == C_SBS) showSideBySideMenu(b.rc);
                    else if (b.id == C_ZOOMMENU) showZoomMenu(b.rc);
                    else if (b.id == C_MENU) showMainMenu(b.rc);
                    else runCommand(b.id);
                    return 0;
                }
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l), hv = -1;
            for (const Button& b : app.buttons) if (b.enabled && hit(b.rc, x, y)) hv = b.id;
            if (hv != app.hover) {
                app.hover = hv;
                RECT r{0, kTabH, 4000, kTabH + kToolH};
                InvalidateRect(h, &r, FALSE);
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, h, 0};
                TrackMouseEvent(&tme);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            if (app.hover != -1) { app.hover = -1; RECT r{0, kTabH, 4000, kTabH + kToolH}; InvalidateRect(h, &r, FALSE); }
            return 0;
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
                    case 'L': rotateView(-1); return 0;
                    case 'R': rotateView(1); return 0;
                    case 'E': if (d) setTool(Viewer::Tool::EditText); return 0;
                    case 'Q': PostMessageW(app.wnd, WM_CLOSE, 0, 0); return 0;
                    case 'P': printDocument(); return 0;
                    case 'D': showProperties(); return 0;
                    case 'U': if (shift) { readAloud(); return 0; } markupSelection(annot::Markup::Underline, false); return 0;
                    case 'S': if (shift) { if (d) saveAsDialog(*d); } else saveActive(); return 0;
                    case 'Z': undoRedo(shift); return 0;
                    case 'Y': undoRedo(true); return 0;
                    case 'H': if (shift) doFindReplace(); else markupSelection(annot::Markup::Highlight, false); return 0;
                    case 'K': if (shift) markupSelection(annot::Markup::Strikethrough, false); else commandPalette(); return 0;
                    case 'M':
                        if (app.viewer.hasSelection()) markupSelection(annot::Markup::Highlight, true);
                        else if (d) setTool(Viewer::Tool::Comment);
                        return 0;
                    case 'F': runCommand(C_FIND); return 0;
                    case 'G': runCommand(C_GOTO); return 0;
                    case 'C': if (d && app.sidebar.thumbsFocused() && !app.sidebar.selection().empty()) copyPagesToClip(*d, app.sidebar.selectedPages()); else app.viewer.copySelection(); return 0;
                    case 'X': if (d && app.sidebar.thumbsFocused() && !app.sidebar.selection().empty()) { auto p = app.sidebar.selectedPages(); copyPagesToClip(*d, p); deleteThumbPages(*d, p); } return 0;
                    case 'V': if (d && app.sidebar.thumbsFocused() && !app.sidebar.selection().empty()) pastePages(*d, app.sidebar.selectedPages().back() + 1); return 0;
                    case 'A': if (app.sidebar.thumbsFocused()) app.sidebar.selectAll(); else app.viewer.selectAll(); return 0;
                    case 'B': showSidebar(shift ? Sidebar::Mode::Pages : Sidebar::Mode::Bookmarks); return 0;
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
                case VK_F11: setFullscreen(!app.fullscreen, false); return 0;
                case VK_F5: setFullscreen(true, true); return 0;
                case VK_F2: setTool(Viewer::Tool::EditPara); return 0;
                case VK_F3: stepMatch(shift ? -1 : 1); return 0;
                case VK_DELETE:
                    if (app.tool == Viewer::Tool::Select && app.sel.page >= 0) {
                        if (pageobj::deleteGroup(*d, app.sel.page, app.sel.objects)) { app.sel.page = -1; app.sel.objects.clear(); app.viewer.setObjectSelection(-1, nullptr); app.viewer.documentChanged(); app.sidebar.refresh(); refresh(); }
                        return 0;
                    }
                    if (app.sidebar.thumbsFocused() && !app.sidebar.selection().empty()) deleteThumbPages(*d, app.sidebar.selectedPages());
                    else deleteSelectedAnnotation();
                    return 0;
                case VK_ESCAPE:
                    if (app.fullscreen) setFullscreen(false, false);
                    else if (app.findVisible) toggleFind(false);
                    else if (app.tool != Viewer::Tool::None) setTool(app.tool);
                    else { app.viewer.clearSelection(); app.viewer.clearAnnotSelection(); }
                    return 0;
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
        case WM_SYSKEYDOWN:
            if (GetKeyState(VK_MENU) < 0 && w == VK_LEFT) { navBack(); return 0; }
            if (GetKeyState(VK_MENU) < 0 && w == VK_RIGHT) { navForward(); return 0; }
            break;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize = {1130, 420};   // the toolbar needs this much room
            return 0;
        case WM_CLOSE:
            for (auto& t : app.tabs) if (!confirmClose(*t)) return 0;
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            KillTimer(h, TIMER_AUTOSAVE);
            for (auto& t : app.tabs) rememberPosition(*t);
            saveSettings();
            clearSession();   // a normal exit leaves nothing to recover
            speech::stop();
            setActiveDocument(nullptr);
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

    // Command line export, without a window: FeatherPDF.exe --export png|jpg|txt|md|docx|xlsx|csv <pdf> <output> [dpi]
    // The exit code is 0 on success, 2 for bad arguments, 3 when the PDF cannot be opened, 4 when the export fails.
    if (!files.empty() && files[0] == L"--export") {
        if (files.size() < 4) return 2;
        static const struct { const wchar_t* name; exporter::Format f; } names[] = {
            {L"png", exporter::Format::Png}, {L"jpg", exporter::Format::Jpeg}, {L"txt", exporter::Format::Text}, {L"md", exporter::Format::Markdown},
            {L"docx", exporter::Format::Word}, {L"xlsx", exporter::Format::ExcelXlsx}, {L"csv", exporter::Format::Csv}};
        int fi = -1;
        for (int i = 0; i < 7; ++i) if (files[1] == names[i].name) fi = i;
        if (fi < 0) return 2;
        Document doc;
        if (doc.open(files[2]) != OpenResult::Ok) return 3;
        std::vector<int> all(doc.pageCount());
        for (int i = 0; i < doc.pageCount(); ++i) all[i] = i;
        exporter::Result r = exporter::run(doc, all, names[fi].f, files[3], files.size() > 4 ? _wtoi(files[4].c_str()) : 150);
        return r.ok ? 0 : 4;
    }

    // Command line compression: FeatherPDF.exe --compress <in.pdf> <out.pdf> [dpi]   (exit code 0 ok, 5 nothing to reduce)
    if (!files.empty() && files[0] == L"--compress") {
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        compress::Options o;
        if (files.size() > 3) o.dpi = std::max(30, _wtoi(files[3].c_str()));
        if (!compress::run(doc, o).changed) return 5;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }

    // Command line protection: FeatherPDF.exe --protect <in.pdf> <out.pdf> <open-password> [owner-password]
    // With an owner password the file is also limited: no printing, copying or changes. Exit code 0 ok.
    if (!files.empty() && files[0] == L"--protect") {
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        protect::Options o;
        o.userPassword = files[3];
        if (files.size() > 4) { o.ownerPassword = files[4]; o.allowPrint = o.allowCopy = o.allowModify = false; }
        if (!doc.setProtection(o)) return 4;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }

    // Command line text recognition: FeatherPDF.exe --ocr <in.pdf> <out.pdf> [language]   (exit code 0 ok, 5 not available, 6 no text found)
    if (!files.empty() && files[0] == L"--ocr") {
        if (files.size() < 3) return 2;
        std::wstring why;
        if (!ocr::available(why)) return 5;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<int> all(doc.pageCount());
        for (int i = 0; i < doc.pageCount(); ++i) all[i] = i;
        ocr::Result r = ocr::run(doc, all, files.size() > 3 ? files[3] : L"eng", true, nullptr);
        if (!r.error.empty()) return 4;
        if (r.words == 0) return 6;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }

    // Command line signing: FeatherPDF.exe --sign <in.pdf> <out.pdf> <certificate.pfx> <password>
    // and checking:         FeatherPDF.exe --checksig <file.pdf> <report.txt>   (exit code 0 ok; the report lists each signature)
    if (!files.empty() && files[0] == L"--sign") {
        if (files.size() < 5) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<unsigned char> plain;
        if (!doc.serialize(plain, FPDF_REMOVE_SECURITY)) return 4;
        HCERTSTORE store = nullptr;
        std::wstring err;
        PCCERT_CONTEXT cert = sign::certFromPfx(files[3], files[4], store, err);
        if (!cert) return 5;
        std::vector<unsigned char> out;
        sign::Options so;
        so.reason = L"Approved";
        if (files.size() > 5 && files[5] != L"-") so.timestampUrl.assign(files[5].begin(), files[5].end());   // time stamp server
        if (files.size() > 6 && files[6] == L"visible") {   // bottom right of the first page
            double cl, cb, cr, ct;
            if (doc.cropBoxOf(0, cl, cb, cr, ct)) { so.visible = true; so.page = 0; so.right = cr - 36; so.left = so.right - 200; so.bottom = cb + 36; so.top = so.bottom + 60; }
        }
        bool ok = sign::signPdf(plain, cert, so, out, err);
        CertFreeCertificateContext(cert);
        if (store) CertCloseStore(store, 0);
        if (!ok) {   // leave the reason next to the output file so scripts can show it
            int n = WideCharToMultiByte(CP_UTF8, 0, err.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
            if (n > 1) WideCharToMultiByte(CP_UTF8, 0, err.c_str(), -1, u.data(), n, nullptr, nullptr);
            std::vector<unsigned char> eb(u.begin(), u.end());
            Document::writeFile(files[2] + L".error.txt", eb);
            return 6;
        }
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--search") {   // --search folder text report.txt
        if (files.size() < 4) return 2;
        std::wstring dir = settingsPath();
        dir = dir.substr(0, dir.find_last_of(L'\\')) + L"\\index";
        foldersearch::Stats st;
        auto hits = foldersearch::search(files[1], files[2], false, dir, nullptr, st);
        std::wstring report = L"files=" + std::to_wstring(st.files) + L" scanned=" + std::to_wstring(st.scanned) + L" cached=" + std::to_wstring(st.cached) + L"\n";
        for (const auto& h : hits) report += h.file + L"|" + std::to_wstring(h.page + 1) + L"|" + h.snippet + L"\n";
        int n = WideCharToMultiByte(CP_UTF8, 0, report.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, report.c_str(), -1, u.data(), n, nullptr, nullptr);
        std::vector<unsigned char> rb(u.begin(), u.end());
        return Document::writeFile(files[3], rb) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--redact") {   // --redact in.pdf out.pdf phrase : marks, applies, saves and checks
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        int marked = redact::markText(doc, files[3], false);
        std::vector<std::wstring> phrases = redact::collect(doc);
        int pages = marked ? redact::apply(doc) : 0;
        redact::Report r = redact::verify(doc, phrases);
        std::vector<unsigned char> out;
        if (!pages || !doc.serialize(out) || Document::writeFile(files[2], out) != SaveResult::Ok) return 4;
        std::string rep = "marked=" + std::to_string(marked) + " pages=" + std::to_string(pages) + " phrases=" + std::to_string(r.phrases) + " found=" + std::to_string(r.found.size()) + "\n";
        std::vector<unsigned char> rb(rep.begin(), rep.end());
        Document::writeFile(files[2] + L".txt", rb);
        return r.found.empty() ? 0 : 5;
    }
    if (!files.empty() && files[0] == L"--batch") {   // --batch preset.ini : runs the saved preset on its folders, writes preset.ini.log
        if (files.size() < 2) return 2;
        batch::Job job;
        if (!batch::load(files[1], job) || job.inputDir.empty()) return 3;
        batch::Result r = batch::run(job, nullptr);
        std::wstring log = L"total=" + std::to_wstring(r.total) + L" done=" + std::to_wstring(r.done) + L" failed=" + std::to_wstring(r.failed) + L"\r\n";
        for (const auto& p : r.problems) log += p + L"\r\n";
        int n = WideCharToMultiByte(CP_UTF8, 0, log.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, log.c_str(), -1, u.data(), n, nullptr, nullptr);
        Document::writeFile(files[1] + L".log", std::vector<unsigned char>(u.begin(), u.end()));
        return r.failed ? 5 : 0;
    }
    if (!files.empty() && (files[0] == L"--formdetect" || files[0] == L"--formexport" || files[0] == L"--formfill")) {
        // --formdetect in.pdf out.pdf        adds fillable fields where a flat form has blanks
        // --formexport in.pdf out.csv|xlsx   writes the answers
        // --formfill in.pdf data.csv outdir  one filled-in copy per row of the CSV file
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        if (files[0] == L"--formdetect") {
            auto specs = formdata::detect(doc);
            std::vector<unsigned char> bytes, out;
            std::wstring err;
            std::wstring dump;
            for (const auto& sp : specs) dump += (sp.kind == formcreate::Spec::Text ? L"text|" : L"check|") + sp.name + L"|" + std::to_wstring(sp.page + 1) + L"\n";
            { int n = WideCharToMultiByte(CP_UTF8, 0, dump.c_str(), -1, nullptr, 0, nullptr, nullptr); std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0'); if (n > 1) WideCharToMultiByte(CP_UTF8, 0, dump.c_str(), -1, u.data(), n, nullptr, nullptr); Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end())); }
            if (specs.empty() || !doc.serialize(bytes) || !formcreate::add(bytes, specs, out, err)) return 5;
            return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
        }
        if (files[0] == L"--formexport") {
            auto rows = formdata::answers(doc);
            if (rows.empty()) return 5;
            std::wstring err;
            const std::wstring& o = files[2];
            return exporter::writeRows(rows, o.size() > 5 && _wcsicmp(o.c_str() + o.size() - 5, L".xlsx") == 0, o, err) ? 0 : 4;
        }
        if (files.size() < 4) return 2;
        auto rows = formdata::parseCsv(readTextFile(files[2]));
        std::vector<unsigned char> original;
        if (rows.size() < 2 || !doc.serialize(original)) return 5;
        CreateDirectoryW(files[3].c_str(), nullptr);
        for (size_t r = 1; r < rows.size(); ++r) {
            std::map<std::wstring, std::wstring> values;
            for (size_t c = 0; c < rows[0].size() && c < rows[r].size(); ++c) values[rows[0][c]] = rows[r][c];
            Document copy;
            std::vector<unsigned char> bytes = original, out;
            if (copy.load(std::move(bytes), {}) != OpenResult::Ok) return 6;
            formdata::setValues(copy, values);
            if (!copy.serialize(out) || Document::writeFile(files[3] + L"\\row" + std::to_wstring(r) + L".pdf", out) != SaveResult::Ok) return 4;
        }
        return 0;
    }
    if (!files.empty() && files[0] == L"--visualdiff") {   // --visualdiff a.pdf b.pdf out.png [page] : overlay of one page pair
        if (files.size() < 4) return 2;
        Document da, db;
        if (da.open(files[1]) != OpenResult::Ok || db.open(files[2]) != OpenResult::Ok) return 3;
        const int page = files.size() > 4 ? _wtoi(files[4].c_str()) - 1 : 0;
        visualdiff::PageResult r = visualdiff::overlay(da, page, db, page, 100, files[3]);
        std::string rep = "ok=" + std::to_string(r.ok) + " pixels=" + std::to_string(r.differingPixels) + "\n";
        Document::writeFile(files[3] + L".txt", std::vector<unsigned char>(rep.begin(), rep.end()));
        return r.ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--checksig") {
        if (files.size() < 3) return 2;
        Document doc;
        std::vector<unsigned char> bytes;
        HANDLE h = CreateFileW(files[1].c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return 3;
        LARGE_INTEGER sz{}; GetFileSizeEx(h, &sz);
        bytes.resize(static_cast<size_t>(sz.QuadPart));
        DWORD got = 0; ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr); CloseHandle(h);
        std::string report;
        for (const auto& st : sign::check(bytes)) {
            static const char* kinds[] = {"VALID", "VALID-UNTRUSTED", "CHANGED", "UNREADABLE"};
            std::wstring line = L"; signer=" + st.signer + L"; time=" + st.time + L"; reason=" + st.reason;
            int n = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
            if (n > 1) WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, u.data(), n, nullptr, nullptr);
            report += std::string(kinds[st.kind]) + u + "\n";
        }
        std::vector<unsigned char> rb(report.begin(), report.end());
        return Document::writeFile(files[2], rb) == SaveResult::Ok ? 0 : 4;
    }

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

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    Viewer::registerClass(inst);
    Sidebar::registerClasses(inst);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    wc.hIconSm = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wc.lpszClassName = L"FeatherPdfMain";
    RegisterClassExW(&wc);

    HWND w = CreateWindowExW(0, wc.lpszClassName, kAppName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1200, 780, nullptr, nullptr, inst, nullptr);
    loadSettings();
    applyTheme();
    updateEmptyText();
    ShowWindow(w, show);
    UpdateWindow(w);
    relayoutChildren();
    restoreSession();
    for (auto& f : files) openFile(f);
    SetFocus(w);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
