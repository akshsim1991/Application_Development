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
#include "textpdf.hpp"
#include "sanitize.hpp"
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
#include "sensitive.hpp"
#include "search.hpp"
#include "splitmerge.hpp"
#include "textdiff.hpp"
#include "attach.hpp"
#include "webview.hpp"
#include "access.hpp"
#include "review.hpp"
#include "readaloud.hpp"
#include "compress.hpp"
#include "ocr.hpp"
#include "sign.hpp"
#include "picture.hpp"
#include "printing.hpp"
#include "pdfa.hpp"
#include "fontlist.hpp"
#include "buildinfo.hpp"
#include "inlineedit.hpp"
#include "startpage.hpp"
#include "tables.hpp"
#include "cli.hpp"
#include "update.hpp"
#include "updater.hpp"
#include "platform.hpp"
#include "headfoot.hpp"
#include "hfdialog.hpp"
#include "tableeditor.hpp"
#include "about.hpp"
#include "icons.hpp"
#include "themeui.hpp"
#include "settingsdlg.hpp"
// message boxes in the window's theme
#define MessageBoxW themeui::messageBox

namespace {

enum { ID_FIND_EDIT = 201, ID_FIND_PREV, ID_FIND_NEXT, ID_FIND_CASE, ID_FIND_COUNT, ID_FIND_CLOSE, ID_FIND_WORD, ID_FIND_WILD, ID_FIND_NOTES, ID_READ_LABEL = 230, ID_READ_BACK, ID_READ_PAUSE, ID_READ_FWD, ID_READ_SLOWER, ID_READ_FASTER, ID_READ_STOP, TIMER_FIND = 1, TIMER_AUTOSAVE = 2 };
constexpr const wchar_t* kAppName = L"Feather PDF";
constexpr UINT kMsgOpenHandedOver = WM_APP + 1;   // opens the files a second launch handed over
constexpr UINT kMsgUpdateChecked = WM_APP + 7;    // the version file was read (updater::checkInBackground)
constexpr UINT kMsgSpeech = WM_APP + 8;           // the voice reading aloud has news (a word started, or it finished)

// kind: a toolbar button, or one of the window buttons (minimise, maximise, close) drawn in the tab row
struct Button { std::wstring label; RECT rc; int id; bool enabled; int icon = 0; bool caption = false; };
enum Cmd { C_EDITPDF = 120, C_COMMENT, C_GEOM, C_SBS, C_ZOOMMENU, C_SAVE = 100, C_UNDO, C_REDO, C_ANNOT, C_OPEN = 1, C_PREV, C_NEXT, C_ZOOMOUT, C_ZOOMIN, C_FITW, C_FITP, C_NEWTAB, C_ACTUAL, C_FIND, C_PANEL, C_GOTO, C_MENU,
           C_CAPMIN = 140, C_CAPMAX, C_CAPCLOSE, C_TAB = 2000 /* + tab index: hover and tips */, C_TABCLOSE = 3000 /* + tab index */ };
enum { ID_PAGE_BOX = 220, ID_ZOOM_BOX };

struct App {
    HINSTANCE inst = nullptr;
    HWND wnd = nullptr;
    Viewer viewer;
    inlineedit::Editor inlineEd;                      // text being edited in place on the page
    Sidebar sidebar;
    bool sidebarVisible = false, findVisible = false, readVisible = false;
    HWND readLabel = nullptr, readBack = nullptr, readPause = nullptr, readFwd = nullptr, readSlower = nullptr, readFaster = nullptr, readStop = nullptr;
    HWND findEdit = nullptr, findPrev = nullptr, findNext = nullptr, findCase = nullptr, findCount = nullptr, findClose = nullptr, findWord = nullptr, findWild = nullptr, findNotes = nullptr;
    int curMatch = -1, matchTotal = 0;
    WNDPROC editProc = nullptr;
    int hlColour = 0, drawColour = 5, widthIdx = 1, readRate = 0;
    Viewer::Tool tool = Viewer::Tool::None;
    std::vector<std::wstring> recent;
    bool testMode = false;                            // --menucheck: leave the user's settings, recent files and session alone
    std::wstring pendingPicture, pendingStamp;
    std::vector<std::wstring> handedOver;             // files from a second launch, opened once its message has returned
    COLORREF pendingStampColour = RGB(0xE5, 0x39, 0x35);
    bool fullscreen = false, presenting = false;      // presenting: full screen, one page at a time (F5)
    WINDOWPLACEMENT savedPlace{sizeof(WINDOWPLACEMENT)};
    LONG savedStyle = 0;
    bool pictureHintShown = false;
    pageobj::TextStyle textStyle{pageobj::Family::Sans, false, false, 14, RGB(0x20, 0x20, 0x20)};
    std::vector<std::unique_ptr<Document>> tabs;   // an empty vector means the "New tab" screen
    int active = -1;
    HFONT font = nullptr, fontBold = nullptr, fontSmall = nullptr;
    int dpi = 96;                                     // the window's screen: every size below is scaled from 96
    std::vector<Button> buttons;
    std::vector<RECT> tabRects, closeRects;
    RECT plusRect{}, pageRect{}, zoomRect{}, ofRect{}, iconRect{};   // the page and zoom boxes, "of n", the program icon
    HWND pageBox = nullptr, zoomBox = nullptr;        // editable page number and zoom
    WNDPROC boxProc = nullptr;
    HWND startPage = nullptr;                         // shown instead of the pages while no document is open
    HWND tips = nullptr;                              // tooltips of the toolbar and tab row
    std::wstring tipKey;                              // the tools registered with it, to redo them only on change
    bool windowActive = true;
    bool compact = false;                             // the toolbar shows icons without labels (narrow window)
    int hover = -1;
    int pressed = -1;          // the toolbar button (or -2 - tab close button) pressed and not yet released
    int theme = 2;            // ThemeId: 0 light, 1 dark, 2 like Windows, 3 AMOLED dark, 4 modern light, 5 midnight blue, 6 graphite
    bool colourIcons = true;  // toolbar icons each in their own colour (else in the text colour)
    bool tabColours = false;  // each tab in a colour of its own
    bool tabNumbers = false;  // "1. Document.pdf"
    int dupMode = 0;          // a PDF opened again that is already open: 0 ask, 1 go to its tab
    bool checkUpdates = true; // look for a new version when Feather PDF starts
    bool ownWindows = false;  // files opened from outside (Explorer, a second launch) each get a window of their own
    std::wstring skipVersion; // a new version the user chose not to be reminded of
    bool restorePositions = true;   // documents open where they were last read
    int cacheIdx = 1;         // pages kept ready in memory: 3, 6, 12
    int autosaveIdx = 0;      // recovery copy of unsaved changes: every 20 s, 1 min, 5 min, never
    int tabDrag = -1;         // the tab held down by the mouse (dragged to another place once it moves)
    int tabDragX = 0;
    bool tabDragging = false;
    int units = 0, scaleIdx = 0;   // geometry tools: mm/cm/in/pt, drawing scale
    bool sbs = false, syncing = false;
    bool sbsLinked = true;     // both documents scroll and zoom together
    bool sbsStacked = false;   // top and bottom instead of left and right
    bool sbsSwapped = false;   // the active document (app.viewer) is in the second place (right or bottom)
    Viewer viewer2;                                   // the second view of side by side
    Document* sbsDoc = nullptr;
    std::unique_ptr<Document> sbsOwned;
    std::vector<int> navBack, navFwd;
    bool navBusy = false;
    int layout = 1; bool cover = false;
    pageobj::TableSpec pendingTable;
    struct { int page = -1; std::vector<int> objects; } sel;   // the selected picture, paragraph or table
    struct { int page = -1, obj = -1; } cropPic;                // the picture the PicCrop tool trims
    annot::PointD protVertex{}, protArm{}; int protStage = 0, protPage = 0;
    double pendingPictureWidth = 0;
    struct Pos { std::wstring path; double zoom; int scrollY; bool fit; };
    std::vector<Pos> positions;                       // where each file was last read, most recent first

    HBRUSH ctlBrush = nullptr;
} app;

Document* activeDoc() { return app.active >= 0 ? app.tabs[app.active].get() : nullptr; }

// Sizes are designed at 96 dots per inch and scaled to the window's screen.
int px(int v) { return MulDiv(v, app.dpi, 96); }
int tabH() { return px(40); }          // the tab row, which is also the title bar
int toolH() { return px(48); }
int findH() { return px(42); }
int statusH() { return px(26); }
int sideW() { return px(230); }
int headerH() { return tabH() + toolH(); }


// features implemented further down
void lookUpKind(int kind); void readAloud(); void syncSideBySide(bool fromFirst); void linkOnSelection(); void openUri(const std::wstring& uri);
void formReset(); void flattenDocument(); void formExport(); void formFillFromCsv(); void startFormField(Viewer::Tool t); bool formFieldDraw(Document& d, const Viewer::Event& e); void formAutoDetect();
void sanitizeDialog(); void pdfaDialog(); void showFonts(Document& d); void searchFolder(); void batchDialog(); void writeSession(); void clearSession(); bool restoreSession();
void startStamp(const std::wstring& text, COLORREF col); void pictureAction(int cmd); void addPictureItems(HMENU m);
void refreshLinks(Document& d); std::wstring signaturePathFwd();
void doFindReplace(); void addDocumentTab(std::unique_ptr<Document> doc); void tableInsert(); void tableEdit(); void tableDelete(); void addBookmarkHere(); void doGreyscale();
void startLink(); void signatureAction(int cmd); void startCopyArea(); void setPageLayout(int layout); void rotateView(int turns);
std::wstring pickPdf(bool multi, std::vector<std::wstring>& out, const wchar_t* title); void accessibilityCheck(Document& d); void reviewDialog(Document& d); void afterPagesChanged(); void repairPdf(); void saveForWeb(Document& d); void attachmentsDialog(); void layersDialog(); void navBack(); void navForward(); void reloadFromDisk(); void geometryAction(int cmd); void compareDocs(); void sideBySideAction(int cmd); void endSideBySide();
void commandPalette(); void newPdfFromPictures(); void splitPdf(); void cropByBox(); void lookUpSelection(bool web); void readFromThisPage();
bool featureDraw(Document& d, const Viewer::Event& e);
bool featureClick(Document& d, const Viewer::Event& e);
std::vector<annot::RectD> tableOutlines(int page);
void pagesMenuAction(int cmd, const std::vector<int>& sel);
void toolsDispatch(int cmd);
void notice(const std::wstring& text);
void showMainMenu(const RECT& anchor);

// A tab's text: unsaved mark, its number when tab numbers are on, the document's name.
std::wstring tabLabel(const Document& d) {
    std::wstring n;
    if (app.tabNumbers)
        for (size_t i = 0; i < app.tabs.size(); ++i) if (app.tabs[i].get() == &d) n = std::to_wstring(i + 1) + L". ";
    return (d.dirty() ? L"\u2022 " : L"") + n + d.title();
}

void applyTheme();

// ---- every popup menu goes through trackMenu: in the --menucheck test it is inspected instead of shown ----
bool g_menuOpen = false;   // a popup menu is open: no tooltips (they would sit over it)

struct MenuCheck {
    bool on = false;
    std::wstring report;   // problems found
    std::wstring listing;  // every item, for testing them one by one
    int menus = 0, items = 0;
};
MenuCheck g_menuCheck;
RECT g_menuExclude{};   // screen rectangle a menu must not cover (the button that opened it); empty: none

void collectMenu(HMENU m, const std::wstring& path, std::map<UINT, std::wstring>& seen, const std::wstring& keys = L"") {
    const int n = GetMenuItemCount(m);
    int pos = 0;   // position as the arrow keys count it (separators are skipped)
    for (int i = 0; i < n; ++i) {
        wchar_t text[256] = L"";
        MENUITEMINFOW mi{sizeof(mi)};
        mi.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE | MIIM_STRING | MIIM_STATE;
        mi.dwTypeData = text;
        mi.cch = 255;
        if (!GetMenuItemInfoW(m, static_cast<UINT>(i), TRUE, &mi)) continue;
        std::wstring label = text;
        if (label.find(L'\t') != std::wstring::npos) label.resize(label.find(L'\t'));
        if (mi.fType & MFT_SEPARATOR) continue;
        const std::wstring here = keys + (keys.empty() ? L"" : L",") + std::to_wstring(pos++);
        if (mi.hSubMenu) { collectMenu(mi.hSubMenu, path + L" > " + label, seen, here); continue; }
        ++g_menuCheck.items;
        g_menuCheck.listing += path + L"\t" + here + L"\t" + label + ((mi.fState & MFS_DISABLED) ? L"\t(greyed)" : L"") + L"\n";
        auto it = seen.find(mi.wID);
        if (mi.wID == 0 && (mi.fState & MFS_DISABLED)) continue;   // a greyed placeholder such as "(none)": nothing to run
        if (mi.wID == 0) g_menuCheck.report += L"no command number: " + path + L" > " + label + L"\n";
        else if (it != seen.end()) g_menuCheck.report += L"same command number " + std::to_wstring(mi.wID) + L": \"" + it->second + L"\" and \"" + path + L" > " + label + L"\"\n";
        else seen[mi.wID] = path + L" > " + label;
    }
}

int trackMenu(HMENU m, UINT flags, int x, int y, int reserved, HWND owner, const RECT* rc) {
    if (!g_menuCheck.on) {
        (void)reserved; (void)rc;
        TPMPARAMS tp{sizeof(tp), g_menuExclude};
        const bool exclude = !IsRectEmpty(&g_menuExclude);
        g_menuExclude = RECT{};
        // TPM_VERTICAL: when the menu does not fit below the button it goes above it, never over it
        if (app.tips) { SendMessageW(app.tips, TTM_ACTIVATE, FALSE, 0); SendMessageW(app.tips, TTM_POP, 0, 0); }   // no tip over the open menu
        themeui::prepareMenu(m);   // drawn in the theme's colours (items measured and painted through the owner)
        g_menuOpen = true;
        const int r = TrackPopupMenuEx(m, flags | (exclude ? TPM_VERTICAL : 0), x, y, owner, exclude ? &tp : nullptr);
        g_menuOpen = false;
        themeui::releaseMenus();
        if (app.tips) SendMessageW(app.tips, TTM_ACTIVATE, TRUE, 0);
        return r;
    }
    std::map<UINT, std::wstring> seen;
    ++g_menuCheck.menus;
    collectMenu(m, L"menu " + std::to_wstring(g_menuCheck.menus), seen);
    return 0;   // as if the menu was closed without a choice
}

void relayoutChildren() {
    RECT rc; GetClientRect(app.wnd, &rc);
    const bool none = app.tabs.empty();
    if (app.startPage) ShowWindow(app.startPage, none && !app.fullscreen ? SW_SHOWNA : SW_HIDE);
    ShowWindow(app.viewer.hwnd(), none ? SW_HIDE : SW_SHOWNA);
    for (HWND b : {app.pageBox, app.zoomBox}) if (b) ShowWindow(b, none || app.fullscreen ? SW_HIDE : SW_SHOWNA);
    if (app.fullscreen) {   // only the pages, edge to edge
        MoveWindow(app.viewer.hwnd(), 0, 0, rc.right, rc.bottom, TRUE);
        MoveWindow(app.sidebar.hwnd(), 0, 0, 0, 0, FALSE);
        if (app.viewer2.hwnd()) ShowWindow(app.viewer2.hwnd(), SW_HIDE);
        return;
    }
    int top = headerH();
    if (app.findVisible) {
        const int y = top + px(7), hgt = px(28);
        int x = px(10);
        MoveWindow(app.findEdit, x, y, px(260), hgt, TRUE); x += px(268);
        MoveWindow(app.findPrev, x, y, px(60), hgt, TRUE); x += px(64);
        MoveWindow(app.findNext, x, y, px(60), hgt, TRUE); x += px(68);
        MoveWindow(app.findCase, x, y, px(96), hgt, TRUE); x += px(100);
        MoveWindow(app.findWord, x, y, px(100), hgt, TRUE); x += px(104);
        MoveWindow(app.findWild, x, y, px(126), hgt, TRUE); x += px(130);
        MoveWindow(app.findNotes, x, y, px(104), hgt, TRUE); x += px(112);
        MoveWindow(app.findCount, x, y + px(5), px(170), px(20), TRUE); x += px(174);
        MoveWindow(app.findClose, std::max(x, static_cast<int>(rc.right) - px(70)), y, px(60), hgt, TRUE);
        top += findH();
    }
    int bottom = std::max<int>(top, rc.bottom - statusH());   // the status bar
    if (app.readVisible) {   // the reading controls, above the status bar
        bottom = std::max(top, bottom - findH());
        const int y = bottom + px(7), hgt = px(28);
        int x = px(10);
        MoveWindow(app.readLabel, x, y + px(5), px(250), px(20), TRUE); x += px(256);
        for (HWND b : {app.readBack, app.readPause, app.readFwd}) { MoveWindow(b, x, y, px(96), hgt, TRUE); x += px(100); }
        x += px(10);
        for (HWND b : {app.readSlower, app.readFaster}) { MoveWindow(b, x, y, px(70), hgt, TRUE); x += px(74); }
        MoveWindow(app.readStop, std::max(x + px(10), static_cast<int>(rc.right) - px(90)), y, px(80), hgt, TRUE);
    }
    int left = 0;
    if (app.sidebarVisible) {
        MoveWindow(app.sidebar.hwnd(), 0, top, sideW(), bottom - top, TRUE);
        left = sideW();
    }
    if (app.startPage) MoveWindow(app.startPage, left, top, std::max<LONG>(0, rc.right - left), bottom - top, TRUE);
    const int total = std::max<LONG>(0, rc.right - left), height = bottom - top;
    if (app.sbs && app.viewer2.hwnd()) {
        // two places, left / right or top / bottom; the active document may sit in either
        RECT a, b;
        if (app.sbsStacked) {
            const int half = height / 2;
            a = {left, top, left + total, top + half - 2};
            b = {left, top + half + 2, left + total, top + height};
        } else {
            const int half = total / 2;
            a = {left, top, left + half - 2, top + height};
            b = {left + half + 2, top, left + total, top + height};
        }
        const RECT& mine = app.sbsSwapped ? b : a;
        const RECT& other = app.sbsSwapped ? a : b;
        MoveWindow(app.viewer.hwnd(), mine.left, mine.top, mine.right - mine.left, mine.bottom - mine.top, TRUE);
        MoveWindow(app.viewer2.hwnd(), other.left, other.top, other.right - other.left, other.bottom - other.top, TRUE);
        ShowWindow(app.viewer2.hwnd(), SW_SHOW);
        InvalidateRect(app.wnd, nullptr, FALSE);   // the marker of the active side
    } else {
        MoveWindow(app.viewer.hwnd(), left, top, total, height, TRUE);
        if (app.viewer2.hwnd()) ShowWindow(app.viewer2.hwnd(), SW_HIDE);
    }
}

void showControls() {
    const int f = app.findVisible ? SW_SHOW : SW_HIDE;
    for (HWND h : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findWord, app.findWild, app.findNotes, app.findCount, app.findClose}) ShowWindow(h, f);
    for (HWND h : {app.readLabel, app.readBack, app.readPause, app.readFwd, app.readSlower, app.readFaster, app.readStop}) ShowWindow(h, app.readVisible && !app.fullscreen ? SW_SHOW : SW_HIDE);
    ShowWindow(app.sidebar.hwnd(), app.sidebarVisible ? SW_SHOW : SW_HIDE);
    relayoutChildren();
    InvalidateRect(app.wnd, nullptr, FALSE);
}

void updateTitle() {
    Document* d = activeDoc();
    std::wstring t = d ? (d->dirty() ? L"\u2022 " : L"") + d->title() + L" - " + kAppName : kAppName;
    SetWindowTextW(app.wnd, t.c_str());
}

void refresh() {
    updateTitle();
    InvalidateRect(app.wnd, nullptr, FALSE);
}

void runSearch(bool keepPosition);

// Hands over text still being typed (an in-place edit, a form field) before another action takes over: until then it
// is not part of the document. False when the in-place edit could not be applied (the reason was shown, the editor stays).
bool finishInput() {
    if (app.inlineEd.active() && !app.inlineEd.commit()) return false;
    app.viewer.endFormInput();
    app.viewer2.endFormInput();
    return true;
}

void setActiveDocument(Document* d) {
    if (app.inlineEd.active()) app.inlineEd.commit();
    const bool startShown = app.startPage && IsWindowVisible(app.startPage);
    app.viewer.setDocument(d);
    if (startShown == !app.tabs.empty() && app.wnd) relayoutChildren();   // the start page goes or comes back
    app.viewer.setMarks(d ? &d->redactions : nullptr);
    app.sidebar.setDocument(d);
    app.curMatch = -1; app.matchTotal = 0;
    SetWindowTextW(app.findCount, L"");
    if (d && app.findVisible && GetWindowTextLengthW(app.findEdit) > 0) runSearch(false);
}

void switchTab(int i) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    if (app.sbs && app.tabs[i].get() == app.sbsDoc && activeDoc() && activeDoc() != app.sbsDoc) {
        // the document on the other side becomes the active one; both stay where they are
        if (app.inlineEd.active()) app.inlineEd.commit();
        Document* was = activeDoc();
        app.sbsDoc = was;
        app.sbsSwapped = !app.sbsSwapped;
        app.syncing = true;   // no zoom or scroll copying while the views trade documents
        app.viewer2.setDocument(was);
        app.active = i;
        setActiveDocument(app.tabs[i].get());
        app.syncing = false;
        relayoutChildren();
        refresh();
        return;
    }
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

// Moves the tab at `from` to `to`; the document on screen stays the same (its number changes with its place).
void moveTab(int from, int to) {
    const int n = static_cast<int>(app.tabs.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    Document* activeWas = activeDoc();
    auto doc = std::move(app.tabs[static_cast<size_t>(from)]);
    app.tabs.erase(app.tabs.begin() + from);
    app.tabs.insert(app.tabs.begin() + to, std::move(doc));
    for (int i = 0; i < n; ++i) if (app.tabs[static_cast<size_t>(i)].get() == activeWas) app.active = i;
    if (app.tabDrag == from) app.tabDrag = to;
    refresh();
}

std::wstring settingsPath();
void closeTab(int i, bool ask = true);
void openInNewWindow(int i, const POINT* at = nullptr);
bool launchWindow(const std::vector<std::wstring>& args);

// Right-click on a tab: open the folder with its file (the file selected there), copy its path, close it or the others.
void showTabMenu(int i, POINT pt) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    const std::wstring path = app.tabs[i]->path();
    const bool onDisk = !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    enum { T_FOLDER = 1, T_COPYPATH, T_CLOSE, T_OTHERS, T_NEWWINDOW };
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (onDisk ? 0 : MF_GRAYED), T_FOLDER, L"Open directory having this file");
    AppendMenuW(m, MF_STRING | (path.empty() ? MF_GRAYED : 0), T_COPYPATH, L"Copy the file's path");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_NEWWINDOW, L"Open in new window");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_CLOSE, L"Close tab");
    AppendMenuW(m, MF_STRING | (app.tabs.size() > 1 ? 0 : MF_GRAYED), T_OTHERS, L"Close other tabs");
    const int cmd = trackMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd == T_FOLDER) {
        // Explorer opens the folder with the file selected (an unsaved new document has no folder: the item is greyed)
        if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(path.c_str())) {
            if (FAILED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0))) {
                const std::wstring a = L"/select,\"" + path + L"\"";
                ShellExecuteW(app.wnd, nullptr, L"explorer.exe", a.c_str(), nullptr, SW_SHOWNORMAL);
            }
            ILFree(pidl);
        }
    } else if (cmd == T_COPYPATH && OpenClipboard(app.wnd)) {
        EmptyClipboard();
        if (HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (path.size() + 1) * sizeof(wchar_t))) {
            memcpy(GlobalLock(g), path.c_str(), (path.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(g);
            if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
        }
        CloseClipboard();
    } else if (cmd == T_NEWWINDOW) openInNewWindow(i);
    else if (cmd == T_CLOSE) closeTab(i);
    else if (cmd == T_OTHERS) {
        Document* keep = app.tabs[i].get();
        for (int k = static_cast<int>(app.tabs.size()) - 1; k >= 0; --k)
            if (k < static_cast<int>(app.tabs.size()) && app.tabs[k].get() != keep) closeTab(k);   // each asks about unsaved changes
    }
}

// ---- more windows: each window is a copy of Feather PDF of its own (its tabs, toolbar, side by side, recovery copies) ----

// Starts another window of Feather PDF with these arguments ("--newwindow" first: it does not hand them to this one).
bool launchWindow(const std::vector<std::wstring>& args) {
    wchar_t exe[MAX_PATH * 2];
    if (!GetModuleFileNameW(nullptr, exe, MAX_PATH * 2)) return false;
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --newwindow";
    for (const std::wstring& a : args) {   // quoted; a backslash before a closing quote doubled, as the command line rules want
        std::wstring q = a;
        size_t trailing = 0;
        while (trailing < q.size() && q[q.size() - 1 - trailing] == L'\\') ++trailing;
        q.append(trailing, L'\\');
        std::wstring esc;
        for (wchar_t c : q) { if (c == L'"') esc += L'\\'; esc += c; }
        cmd += L" \"" + esc + L"\"";
    }
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    AllowSetForegroundWindow(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

std::wstring handoverDir() {
    std::wstring s = settingsPath();
    return s.substr(0, s.find_last_of(L'\\')) + L"\\handover";
}

// The tab moves to a window of its own (where the mouse let go of it, when dragged off the tab row). Unsaved changes go
// with it: the document as it is now is handed over in a file the new window reads and deletes.
void openInNewWindow(int i, const POINT* at) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size()) || !finishInput()) return;
    Document& d = *app.tabs[i];
    std::vector<std::wstring> args;
    if (at) args.insert(args.end(), {L"--at", std::to_wstring(at->x) + L"," + std::to_wstring(at->y)});
    if (d.dirty() || d.path().empty()) {
        std::vector<unsigned char> bytes;
        if (d.isProtected() && d.dirty()) {
            MessageBoxW(app.wnd, L"Save this password-protected document first, then open it in a new window.", kAppName, MB_OK | MB_ICONINFORMATION);
            return;
        }
        const std::wstring dir = handoverDir();
        CreateDirectoryW(dir.c_str(), nullptr);
        const std::wstring file = dir + L"\\" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()) + L".pdf";
        if (!d.serialize(bytes) || Document::writeFile(file, bytes) != SaveResult::Ok) {
            MessageBoxW(app.wnd, L"The document could not be handed to a new window. Save it, then try again.", kAppName, MB_OK | MB_ICONWARNING);
            return;
        }
        args.insert(args.end(), {L"--handover", file, d.path(), d.title()});
    } else args.push_back(d.path());
    if (!launchWindow(args)) { MessageBoxW(app.wnd, L"A new window could not be opened.", kAppName, MB_OK | MB_ICONWARNING); return; }
    rememberPosition(d);
    closeTab(i, false);   // it lives on in the new window
    writeSession();
}

void closeTab(int i, bool ask) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    if (ask && !confirmClose(*app.tabs[i])) return;
    const bool endsSbs = app.sbs && (app.tabs[i].get() == app.sbsDoc || i == app.active);
    Document* survivor = endsSbs ? (app.tabs[i].get() == app.sbsDoc ? activeDoc() : app.sbsDoc) : nullptr;
    if (endsSbs) endSideBySide();   // back to one document
    rememberPosition(*app.tabs[i]);
    const bool wasActive = i == app.active;
    if (wasActive) setActiveDocument(nullptr);   // viewer and sidebar must let go before the document dies
    app.tabs.erase(app.tabs.begin() + i);
    if (app.tabs.empty()) { app.active = -1; setActiveDocument(nullptr); }
    else if (wasActive) {
        app.active = std::min(i, static_cast<int>(app.tabs.size()) - 1);
        setActiveDocument(app.tabs[app.active].get());
    } else if (i < app.active) --app.active;   // another tab closed: the document on screen stays
    // the other side had the zoom of half the window: it fills the whole view again
    if (survivor && activeDoc() == survivor) { survivor->fitWidth = true; app.viewer.fitWidth(); }
    refresh();
}

// ---- password prompt: a small modal dialog built in memory (no resource file needed) ----
struct PwState { std::wstring prompt, text; bool ok = false; };

// Annotate menu command ids (kept apart from toolbar command ids).
enum MenuId { M_HIGHLIGHT = 300, M_UNDERLINE, M_STRIKE, M_SQUIGGLY, M_COMMENT, M_RECT, M_ELLIPSE, M_LINE, M_ARROW, M_PEN,
              M_DELETE, M_EDIT_COMMENT, M_COPY, M_EDIT_CALLOUT,
              T_WATERMARK = 500, T_NUMBERS, T_EXPORT, T_READ, T_RATE = 510, T_COLOURS = 520, T_PRINT = 530, T_PROPS, T_RED_AREA = 540, T_RED_SEL, T_RED_FIND, T_RED_CLEAR, T_RED_APPLY, M_RED_REMOVE, T_COMPRESS, T_EDIT_TEXT, T_ADD_TEXT, T_TEXT_STYLE, T_PROTECT, T_UNPROTECT, T_OCR, T_SIGN, T_OWNCERT, T_SHOWSIGS, T_NOTES, T_FULLSCREEN, T_PRESENT, T_STAMP = 700, T_STAMP_CUSTOM = 720, T_PICTURE, T_DEFAULT_VIEWER, T_RECENT = 600, T_RECENT_CLEAR = 620, M_COPY_PAGE,
              P_DELETE = 400, P_BLANK, P_ROT_R, P_ROT_L, P_ROT_ALL, P_MOVE, P_EXTRACT, P_INSERT, P_MERGE, P_CROP, P_SPLIT, P_CROPBOX, P_NEWPDF, P_NUP, P_REVERSE, P_BLANKS, P_DUPES, P_HEADER, P_BGCOLOR, P_HFDELETE, P_INTERLEAVE, M_HL_COLOUR = 320, M_DRAW_COLOUR = 340, M_WIDTH = 360 };
// extra menu ids
enum ExtraId { M_SELALL = 990, M_ADDCOMMENT_HERE, M_EDITTEXT_HERE, N_SELECTOBJ = 1012, N_EDITPARA = 1010, M_EDITPARA_HERE, N_RED_CHECK = 999, N_BATCH = 998, N_RED_SENSITIVE = 997, N_SANITIZE = 1020, N_PDFA, N_FONTS, N_ACCESS, N_REVIEW, F_RADIO = 1030, F_LIST, F_DATE, F_RESET, F_FLATTEN, A_POLYGON = 1040, A_CLOUD, A_CALLOUT, A_DATESTAMP = 1050, M_TABLE_HERE = 1080, M_TABLEDEL_HERE, PX_ROT_R = 1060, PX_ROT_L, PX_FLIP_H, PX_FLIP_V, PX_CROP, PX_ADJUST, PX_GREY, PX_REPLACE, PX_SAVE, PX_FRONT, PX_BACK, PX_PASTE, F_EXPORT = 1000, F_FILL, F_TEXT, F_CHECK, F_COMBO, F_AUTO, N_FINDREPLACE = 800, N_TABLE_INS, N_TABLE_EDIT, N_TABLE_DEL, N_BOOKMARK, N_BW, N_LINK, N_SIG_TYPE, N_SIG_DRAW, N_SIG_PIC, N_SIG_SAVED,
               N_TEXTSIZE = 820, N_LAYOUT = 840, N_ROTL = 850, N_ROTR, N_BACK, N_FWD, N_RELOAD, N_COPYAREA, N_PALETTE, N_COMPARE, N_SBS_OPEN, N_SBS_TAB, N_SBS_OFF, N_SBS_LINK, N_SBS_HORZ, N_SBS_VERT, N_SBS_SWAP,
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

void updateEmptyText() {   // the start page lists the recent files
    if (app.startPage) startpage::setRecent(app.startPage, app.recent);
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
    app.viewer2.setColours(app.viewer.colours());   // the second view of side by side shows pages the same way
    app.theme = std::clamp(num(L"theme", 2), 0, ThemeCount - 1);
    app.colourIcons = num(L"colourIcons", 1) != 0;
    app.tabColours = num(L"tabColours", 0) != 0;
    app.tabNumbers = num(L"tabNumbers", 0) != 0;
    app.dupMode = std::clamp(num(L"duplicateOpen", 0), 0, 1);
    app.checkUpdates = num(L"checkUpdates", 1) != 0;
    app.ownWindows = num(L"ownWindows", 0) != 0;
    {
        wchar_t skip[64] = L"";
        GetPrivateProfileStringW(L"view", L"skipVersion", L"", skip, 64, p.c_str());
        app.skipVersion = skip;
    }
    app.restorePositions = num(L"restorePositions", 1) != 0;
    app.cacheIdx = std::clamp(num(L"pageCache", 1), 0, 2);
    app.autosaveIdx = std::clamp(num(L"recoveryCopy", 0), 0, 3);
    app.viewer.setCacheLimit(app.cacheIdx == 0 ? 3 : app.cacheIdx == 1 ? 6 : 12);
    app.viewer2.setCacheLimit(app.cacheIdx == 0 ? 3 : app.cacheIdx == 1 ? 6 : 12);
    app.layout = std::clamp(num(L"layout", 1), 0, 2);
    app.cover = num(L"cover", 0) != 0;
    app.units = std::clamp(num(L"units", 0), 0, 3);
    app.sbsLinked = num(L"sideBySideLinked", 1) != 0;
    app.sbsStacked = num(L"sideBySideStacked", 0) != 0;
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
    if (app.testMode) return;
    const std::wstring p = settingsPath();
    if (p.empty()) return;
    auto put = [&](const wchar_t* sec, const wchar_t* key, int v) { WritePrivateProfileStringW(sec, key, std::to_wstring(v).c_str(), p.c_str()); };
    put(L"view", L"highlightColour", app.hlColour);
    put(L"view", L"drawingColour", app.drawColour);
    put(L"view", L"lineWidth", app.widthIdx);
    put(L"view", L"readingSpeed", app.readRate);
    put(L"view", L"pageColours", static_cast<int>(app.viewer.colours()));
    put(L"view", L"theme", app.theme);
    put(L"view", L"colourIcons", app.colourIcons);
    put(L"view", L"tabColours", app.tabColours);
    put(L"view", L"tabNumbers", app.tabNumbers);
    put(L"view", L"duplicateOpen", app.dupMode);
    put(L"view", L"checkUpdates", app.checkUpdates);
    put(L"view", L"ownWindows", app.ownWindows);
    WritePrivateProfileStringW(L"view", L"skipVersion", app.skipVersion.c_str(), p.c_str());
    put(L"view", L"restorePositions", app.restorePositions);
    put(L"view", L"pageCache", app.cacheIdx);
    put(L"view", L"recoveryCopy", app.autosaveIdx);
    put(L"view", L"layout", app.layout);
    put(L"view", L"cover", app.cover);
    put(L"view", L"units", app.units);
    put(L"view", L"sideBySideLinked", app.sbsLinked);
    put(L"view", L"sideBySideStacked", app.sbsStacked);
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
    if (app.testMode) return;
    app.recent.erase(std::remove_if(app.recent.begin(), app.recent.end(), [&](const std::wstring& r) { return _wcsicmp(r.c_str(), path.c_str()) == 0; }), app.recent.end());
    app.recent.insert(app.recent.begin(), path);
    if (app.recent.size() > 10) app.recent.resize(10);
    updateEmptyText();
}

// ---- opening files ----
bool openTextAsPdf(const std::wstring& path);

// A file's identity on its disk (volume and file number): the same file reached by another path (a mapped drive, a
// network path, a short 8.3 name, a link, other capitals) has the same identity.
struct FileId { DWORD volume = 0, high = 0, low = 0; bool known = false; };
FileId fileIdOf(const std::wstring& path) {
    FileId id;
    HANDLE f = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (f == INVALID_HANDLE_VALUE) return id;
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileInformationByHandle(f, &info)) { id.volume = info.dwVolumeSerialNumber; id.high = info.nFileIndexHigh; id.low = info.nFileIndexLow; id.known = true; }
    CloseHandle(f);
    return id;
}

// the full, long form of a path (when the file cannot be asked for its identity)
std::wstring canonicalPath(const std::wstring& p) {
    wchar_t full[MAX_PATH * 2], longer[MAX_PATH * 2];
    std::wstring s = GetFullPathNameW(p.c_str(), MAX_PATH * 2, full, nullptr) ? full : p;
    if (GetLongPathNameW(s.c_str(), longer, MAX_PATH * 2)) s = longer;
    return s;
}

// The tab that already shows this file, or -1.
int tabShowing(const std::wstring& path) {
    const FileId id = fileIdOf(path);
    const std::wstring canon = canonicalPath(path);
    for (size_t i = 0; i < app.tabs.size(); ++i) {
        const std::wstring& other = app.tabs[i]->path();
        if (other.empty()) continue;
        if (_wcsicmp(other.c_str(), path.c_str()) == 0) return static_cast<int>(i);
        const FileId o = id.known ? fileIdOf(other) : FileId{};
        if (id.known && o.known) { if (o.volume == id.volume && o.high == id.high && o.low == id.low) return static_cast<int>(i); continue; }
        if (_wcsicmp(canonicalPath(other).c_str(), canon.c_str()) == 0) return static_cast<int>(i);
    }
    return -1;
}

// A PDF that is already open: says so, and goes to its tab if asked (or straight away, as set in Settings > Tabs).
// The open tab is never reloaded, so its unsaved changes stay as they are.
void offerOpenTab(int i) {
    if (app.dupMode == 1) { switchTab(i); return; }
    const std::wstring n = std::to_wstring(i + 1);
    std::wstring text = L"This PDF is already open in Tab " + n + L".\n\n" + app.tabs[static_cast<size_t>(i)]->title();
    if (app.tabs[static_cast<size_t>(i)]->dirty()) text += L"\n\nThat tab has unsaved changes; they stay as they are.";
    if (themeui::choiceBox(app.wnd, kAppName, text, {L"Go to Tab " + n, L"Cancel"}, 0, 1, MB_ICONINFORMATION) == 0) switchTab(i);
}

bool g_restoring = false;   // tabs coming back after a crash stay in this window

void openFile(const std::wstring& pathIn) {
    wchar_t full[MAX_PATH * 2];
    std::wstring path = GetFullPathNameW(pathIn.c_str(), MAX_PATH * 2, full, nullptr) ? full : pathIn;
    const DWORD attr = GetFileAttributesW(path.c_str());
    // each file in its own window (Settings > Tabs): a window with a document already opens the next file in a new one
    if (app.ownWindows && !app.tabs.empty() && !g_restoring && !app.testMode && attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        bool here = false;   // unless it is open in this window already (its tab is shown)
        for (const auto& t : app.tabs) if (_wcsicmp(t->path().c_str(), path.c_str()) == 0) here = true;
        if (!here && launchWindow({path})) return;
    }
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring name = path;
        while (name.size() > 3 && (name.back() == L'\\' || name.back() == L'/')) name.pop_back();
        if (name.size() > 3) name = name.substr(name.find_last_of(L"\\/") + 1);
        MessageBoxW(app.wnd, (L"\"" + name + L"\" is a folder. Open a PDF document inside it.").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    {   // a text or Markdown file becomes a new PDF
        const size_t dot = path.find_last_of(L'.');
        std::wstring ext = dot == std::wstring::npos ? L"" : path.substr(dot);
        for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
        if (ext == L".txt" || ext == L".md" || ext == L".markdown") { openTextAsPdf(path); return; }
    }

    if (const int open = tabShowing(path); open >= 0) { offerOpenTab(open); return; }

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
    if (app.restorePositions)
        for (const auto& ps : app.positions)
            if (_wcsicmp(ps.path.c_str(), path.c_str()) == 0) { doc->zoom = ps.zoom; doc->scrollY = ps.scrollY; doc->fitWidth = ps.fit; break; }
    app.tabs.push_back(std::move(doc));
    switchTab(static_cast<int>(app.tabs.size()) - 1);
    if (badSig) MessageBoxW(app.wnd, L"A digital signature in this document does not match: the document was changed after it was signed (or the signature is damaged). "
                                     L"Do not trust its content. Edit PDF > Digital signatures > Show the signatures gives the details.", kAppName, MB_OK | MB_ICONWARNING);
}

void openDialog() {
    wchar_t buf[MAX_PATH * 2] = L"";
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0Text and Markdown (made into a PDF) (*.txt;*.md)\0*.txt;*.md;*.markdown\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) openFile(buf);
}


// ---- text and Markdown files as PDF ----
std::wstring readTextFile(const std::wstring& path);
void addDocumentTab(std::unique_ptr<Document> doc);

bool openTextAsPdf(const std::wstring& path) {
    std::wstring ext = path.substr(path.find_last_of(L'.'));
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    std::vector<unsigned char> bytes;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    textpdf::Result r = textpdf::build(readTextFile(path), ext != L".txt", bytes);
    SetCursor(old);
    auto doc = std::make_unique<Document>();
    if (!r.ok || doc->load(std::move(bytes), {}) != OpenResult::Ok) { MessageBoxW(app.wnd, L"The file could not be made into a PDF.", kAppName, MB_OK | MB_ICONWARNING); return false; }
    std::wstring name = path.substr(path.find_last_of(L"\\/") + 1);
    name = name.substr(0, name.find_last_of(L'.')) + L".pdf";
    doc->setName(name);
    doc->markModified();
    addDocumentTab(std::move(doc));
    if (r.replaced) MessageBoxW(app.wnd, (std::to_wstring(r.replaced) + L" character(s) are not in the standard PDF fonts (for example Indian or Asian scripts) and were written as \"?\".").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
    return true;
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
    if (!finishInput()) return false;   // text still being typed is part of what is saved
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
    if (r == SaveResult::CannotWrite || r == SaveResult::DiskFull) {   // the changes are still here: offer another place
        const std::wstring q = L"\"" + name + L"\"" + msg + L"\n\nSave it under another name or in another folder instead?";
        if (MessageBoxW(app.wnd, q.c_str(), kAppName, MB_YESNO | MB_ICONWARNING) == IDYES) return saveAsDialog(d);
        return false;
    }
    MessageBoxW(app.wnd, (L"\"" + name + L"\"" + msg).c_str(), kAppName, MB_OK | MB_ICONWARNING);
    return false;
}

// ---- comment threads and review ----

std::wstring reviewerName() {
    static std::wstring name;
    if (name.empty()) {
        wchar_t b[256];
        DWORD n = 256;
        name = GetUserNameW(b, &n) ? std::wstring(b) : L"Reviewer";
    }
    return name;
}

void reviewDialog(Document& d) {
    const auto all = review::threads(d);
    if (all.empty()) { MessageBoxW(app.wnd, L"This document has no comments yet. Add one with Comment, or highlight some text.", L"Comments and review", MB_OK | MB_ICONINFORMATION); return; }
    std::wstring list;
    std::vector<std::wstring> labels;
    for (size_t i = 0; i < all.size(); ++i) {
        const auto& t = all[i];
        std::wstring line = std::to_wstring(i + 1) + L". Page " + std::to_wstring(t.page + 1) + L" \u00B7 " + t.kind + (t.author.empty() ? L"" : L" by " + t.author) +
                            L" \u00B7 " + (t.state == review::State::None ? L"Open" : review::stateName(t.state));
        std::wstring body = !t.text.empty() ? t.text : t.quoted;
        list += line + L"\n    " + (body.size() > 120 ? body.substr(0, 120) + L"\u2026" : body) + L"\n";
        for (const auto& r : t.replies) list += L"      reply by " + r.author + L": " + (r.text.size() > 100 ? r.text.substr(0, 100) + L"\u2026" : r.text) + L"\n";
        list += L"\n";
        labels.push_back(std::to_wstring(i + 1) + L". Page " + std::to_wstring(t.page + 1) + L": " + (body.size() > 40 ? body.substr(0, 40) + L"\u2026" : body));
    }
    std::vector<FormField> f(5);
    f[0] = {FormField::Note, L"", {}, list};
    f[1] = {FormField::Choice, L"Comment:", labels, L"", 0};
    f[2] = {FormField::Choice, L"Do:", {L"Reply", L"Mark it Accepted", L"Mark it Rejected", L"Mark it Completed", L"Mark it Cancelled", L"Clear its status",
                                       L"Open the review summary (all comments)", L"Save the review summary as a file..."}, L"", 0};
    f[3] = {FormField::Text, L"Reply:", {}, L""};
    f[4] = {FormField::Text, L"Your name:", {}, reviewerName()};
    if (!showForm(app.wnd, app.inst, L"Comments and review", f, L"OK", [](const std::vector<FormField>& v) -> std::wstring {
            return v[2].choice == 0 && v[3].text.empty() ? L"Type the reply." : L""; })) return;
    const review::Thread& t = all[static_cast<size_t>(std::clamp(f[1].choice, 0, static_cast<int>(all.size()) - 1))];
    const std::wstring who = f[4].text.empty() ? reviewerName() : f[4].text;
    const int what = f[2].choice;
    if (what >= 6) {
        const std::wstring md = review::summary(d);
        if (what == 6) {
            std::vector<unsigned char> pdf;
            auto doc = std::make_unique<Document>();
            if (textpdf::build(md, true, pdf).ok && doc->load(std::move(pdf), {}) == OpenResult::Ok) {
                doc->setName(L"Review summary - " + d.title());
                addDocumentTab(std::move(doc));
            }
            return;
        }
        wchar_t buf[MAX_PATH * 2];
        std::wstring base = d.title();
        if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
        wcsncpy_s(buf, (base + L" (review).md").c_str(), _TRUNCATE);
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = app.wnd;
        ofn.lpstrTitle = L"Save the review summary as";
        ofn.lpstrFilter = L"Markdown files (*.md)\0*.md\0Text files (*.txt)\0*.txt\0";
        ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2; ofn.lpstrDefExt = L"md";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetSaveFileNameW(&ofn)) return;
        const std::string u = platform::utf8(md);
        std::vector<unsigned char> bytes{0xEF, 0xBB, 0xBF};
        bytes.insert(bytes.end(), u.begin(), u.end());
        if (Document::writeFile(buf, bytes) != SaveResult::Ok) MessageBoxW(app.wnd, L"The summary could not be saved there.", kAppName, MB_OK | MB_ICONWARNING);
        else notice(L"Review summary saved.");
        return;
    }
    static const review::State states[] = {review::State::None, review::State::Accepted, review::State::Rejected, review::State::Completed, review::State::Cancelled, review::State::None};
    const bool ok = what == 0 ? review::reply(d, t.object, who, f[3].text) : review::setState(d, t.object, states[what], who);
    if (!ok) { MessageBoxW(app.wnd, L"That could not be saved in this document.", kAppName, MB_OK | MB_ICONWARNING); return; }
    afterDocReplaced();
    app.viewer.goToPage(t.page);
    notice(what == 0 ? L"Reply added. Save the document to keep it." : std::wstring(L"Marked ") + review::stateName(states[what]) + L". Save the document to keep it.");
}

// ---- accessibility: check, and fix what needs no person ----

void accessibilityCheck(Document& d) {
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    const auto items = a11y::check(d);
    SetCursor(old);
    int failed = 0, person = 0, fixable = 0;
    std::wstring summary, title;
    for (const auto& i : items) {
        if (i.id == "title" && i.result == a11y::Result::Passed && i.detail.size() > 2) title = i.detail.substr(1, i.detail.size() - 2);
        if (i.result == a11y::Result::Passed) continue;
        (i.result == a11y::Result::Failed ? failed : person)++;
        fixable += i.fixable;
        summary += (i.result == a11y::Result::Failed ? L"NOT MET:  " : L"CHECK:  ") + i.rule + L"\n      " + i.detail + (i.fixable ? L"  (Fix puts this right.)" : L"") + L"\n\n";
    }
    summary = std::to_wstring(items.size() - static_cast<size_t>(failed + person)) + L" of " + std::to_wstring(items.size()) + L" checks passed; " +
              std::to_wstring(failed) + L" not met, " + std::to_wstring(person) + L" for a person to check.\n\n" + summary;
    if (title.empty()) title = a11y::suggestedTitle(d);
    static const std::vector<std::pair<std::wstring, std::wstring>> langs = {
        {L"English (United States)", L"en-US"}, {L"English (United Kingdom)", L"en-GB"}, {L"English (India)", L"en-IN"}, {L"Hindi", L"hi-IN"},
        {L"Kannada", L"kn-IN"}, {L"Tamil", L"ta-IN"}, {L"Telugu", L"te-IN"}, {L"Malayalam", L"ml-IN"}, {L"Marathi", L"mr-IN"}, {L"Bengali", L"bn-IN"},
        {L"Gujarati", L"gu-IN"}, {L"French", L"fr-FR"}, {L"German", L"de-DE"}, {L"Spanish", L"es-ES"}, {L"Portuguese", L"pt-BR"}, {L"Chinese", L"zh-CN"},
        {L"Japanese", L"ja-JP"}, {L"Arabic", L"ar"}};
    std::vector<std::wstring> langNames;
    int langChoice = 0;
    wchar_t loc[LOCALE_NAME_MAX_LENGTH] = L"";
    GetUserDefaultLocaleName(loc, LOCALE_NAME_MAX_LENGTH);
    for (size_t i = 0; i < langs.size(); ++i) {
        langNames.push_back(langs[i].first);
        if (_wcsicmp(langs[i].second.c_str(), loc) == 0) langChoice = static_cast<int>(i);
    }
    std::vector<FormField> f(5);
    f[0] = {FormField::Note, L"", {}, summary};
    f[1] = {FormField::Text, L"Title:", {}, title};
    f[2] = {FormField::Choice, L"Language of the text:", langNames, L"", langChoice};
    f[3] = {FormField::Check, L"Fix what can be fixed now (title, language, tab order, links, field tooltips)"};
    f[3].checked = fixable > 0;
    f[4] = {FormField::Check, L"Open the full report in a new tab"};
    if (!showForm(app.wnd, app.inst, L"Accessibility check", f, L"OK")) return;
    std::wstring msg;
    if (f[3].checked) {
        a11y::FixOptions o;
        o.titleText = f[1].text;
        o.language = langs[static_cast<size_t>(std::clamp(f[2].choice, 0, static_cast<int>(langs.size()) - 1))].second;
        const auto done = a11y::fix(d, o);
        if (!done.empty()) {
            afterDocReplaced();
            for (const auto& line : done) msg += L"\u2022 " + line + L"\n";
        }
    }
    if (f[4].checked) {
        std::vector<unsigned char> pdf;
        auto rep = std::make_unique<Document>();
        if (textpdf::build(a11y::report(a11y::check(d), d.title()), true, pdf).ok && rep->load(std::move(pdf), {}) == OpenResult::Ok) {
            rep->setName(L"Accessibility report - " + d.title());
            addDocumentTab(std::move(rep));
        }
    }
    if (!msg.empty()) MessageBoxW(app.wnd, (L"Fixed:\n" + msg + L"\nSave the document to keep the changes; Undo (Ctrl+Z) takes them back.").c_str(), L"Accessibility", MB_OK | MB_ICONINFORMATION);
}

// ---- repair, and Fast Web View ----

void repairPdf() {
    std::vector<std::wstring> files;
    pickPdf(false, files, L"Choose the damaged PDF");
    if (files.empty()) return;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::vector<unsigned char> bytes;
    const repair::Report r = repair::run(files[0], bytes);
    SetCursor(old);
    std::wstring msg;
    for (const auto& p : r.problems) msg += L"\u2022 " + p + L"\n";
    std::wstring name = files[0].substr(files[0].find_last_of(L"\\/") + 1);
    if (bytes.empty()) {
        MessageBoxW(app.wnd, (L"\"" + name + L"\" could not be repaired.\n\n" + msg).c_str(), L"Repair", MB_OK | MB_ICONWARNING);
        return;
    }
    auto doc = std::make_unique<Document>();
    if (doc->load(std::move(bytes), {}) != OpenResult::Ok) { MessageBoxW(app.wnd, L"The repaired copy could not be opened.", L"Repair", MB_OK | MB_ICONWARNING); return; }
    const size_t dot = name.find_last_of(L'.');
    doc->setName((dot == std::wstring::npos ? name : name.substr(0, dot)) + L" (repaired).pdf");
    doc->markModified();
    addDocumentTab(std::move(doc));
    if (!r.fixed)
        MessageBoxW(app.wnd, (L"Nothing was wrong with \"" + name + L"\" (" + std::to_wstring(r.pages) + L" pages). A fresh copy is open in a new tab.").c_str(), L"Repair", MB_OK | MB_ICONINFORMATION);
    else
        MessageBoxW(app.wnd, (L"\"" + name + L"\" was repaired: " + std::to_wstring(r.pages) + L" page(s) could be read.\n\nWhat was wrong:\n" + msg +
                              L"\nThe repaired copy is open in a new tab. Look it over, then save it (Ctrl+S) under a new name.").c_str(),
                    L"Repair", MB_OK | MB_ICONINFORMATION);
}

void saveForWeb(Document& d) {
    wchar_t buf[MAX_PATH * 2];
    std::wstring base = d.title();
    const size_t dot = base.find_last_of(L'.');
    if (dot != std::wstring::npos) base.resize(dot);
    wcsncpy_s(buf, (base + L" (web).pdf").c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Save a copy for the web (Fast Web View)";
    ofn.lpstrFilter = L"PDF documents (*.pdf)\0*.pdf\0";
    ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2; ofn.lpstrDefExt = L"pdf";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::vector<unsigned char> out;
    std::wstring why;
    const bool ok = !d.isProtected() && webview::linearize(d, out, &why);
    SetCursor(old);
    if (!ok) {
        if (d.isProtected()) why = L"A password-protected document cannot be made fast for the web. Remove the password first.";
        MessageBoxW(app.wnd, (L"The copy could not be made. " + why).c_str(), kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    if (Document::writeFile(buf, out) != SaveResult::Ok) { MessageBoxW(app.wnd, L"The copy could not be written there.", kAppName, MB_OK | MB_ICONWARNING); return; }
    notice(L"Saved for the web: the first page shows while the rest is still downloading.");
}

// ---- files attached to the document, and its layers ----

void attachmentsDialog() {
    Document* d = activeDoc();
    if (!d) return;
    const auto files = attach::list(*d);
    std::vector<std::wstring> names;
    std::wstring listText;
    for (const auto& f : files) {
        names.push_back(f.name);
        listText += L"\n  " + f.name + L"  (" + (f.size >= 10240 ? std::to_wstring(f.size / 1024) + L" KB" : std::to_wstring(f.size) + L" bytes") + L")";
    }
    std::vector<FormField> f;
    f.push_back({FormField::Note, L"", {}, files.empty() ? L"No files are attached to this document." : std::to_wstring(files.size()) + L" file(s) attached:" + listText});
    if (!files.empty()) {
        f.push_back({FormField::Choice, L"File:", names, L"", 0});
        f.push_back({FormField::Choice, L"Do:", {L"Open it", L"Save a copy...", L"Remove it from the document", L"Attach another file..."}, L"", 0});
    }
    if (!showForm(app.wnd, app.inst, L"Attached files", f, files.empty() ? L"Attach a file..." : L"OK")) return;
    const int which = files.empty() ? 0 : f[1].choice;
    const int what = files.empty() ? 3 : f[2].choice;
    if (what == 3) {   // attach a file
        wchar_t buf[MAX_PATH * 2] = L"";
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = app.wnd;
        ofn.lpstrTitle = L"Choose the file to attach";
        ofn.lpstrFilter = L"All files (*.*)\0*.*\0";
        ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&ofn)) return;
        std::vector<unsigned char> data;
        const std::wstring path = buf;
        if (!platform::readFile(path, data)) { MessageBoxW(app.wnd, L"The file could not be read.", kAppName, MB_OK | MB_ICONWARNING); return; }
        if (!attach::add(*d, path.substr(path.find_last_of(L"\\/") + 1), data)) { MessageBoxW(app.wnd, L"The file could not be attached.", kAppName, MB_OK | MB_ICONWARNING); return; }
        afterPagesChanged();
        notice(L"Attached. Save the document to keep it.");
        return;
    }
    if (what == 2) {
        if (MessageBoxW(app.wnd, (L"Remove \"" + names[static_cast<size_t>(which)] + L"\" from the document? Undo (Ctrl+Z) brings it back until you save.").c_str(),
                        L"Attached files", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        if (attach::remove(*d, which)) { afterPagesChanged(); notice(L"Removed."); }
        return;
    }
    std::vector<unsigned char> data;
    if (!attach::get(*d, which, data)) { MessageBoxW(app.wnd, L"The attached file could not be read.", kAppName, MB_OK | MB_ICONWARNING); return; }
    std::wstring name = names[static_cast<size_t>(which)];
    for (auto& c : name) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_';
    std::wstring target;
    if (what == 1) {
        wchar_t buf[MAX_PATH * 2];
        wcsncpy_s(buf, name.c_str(), _TRUNCATE);
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = app.wnd;
        ofn.lpstrTitle = L"Save the attached file as";
        ofn.lpstrFilter = L"All files (*.*)\0*.*\0";
        ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetSaveFileNameW(&ofn)) return;
        target = buf;
    } else {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        target = std::wstring(tmp) + L"Feather PDF attachments";
        CreateDirectoryW(target.c_str(), nullptr);
        target += L"\\" + name;
    }
    if (Document::writeFile(target, data) != SaveResult::Ok) { MessageBoxW(app.wnd, L"The file could not be written.", kAppName, MB_OK | MB_ICONWARNING); return; }
    if (what == 1) { notice(L"Saved: " + target); return; }
    // opening a file that can run something is the user's choice, as in other PDF readers
    const std::wstring ext = name.find_last_of(L'.') == std::wstring::npos ? L"" : name.substr(name.find_last_of(L'.'));
    static const wchar_t* risky[] = {L".exe", L".bat", L".cmd", L".com", L".js", L".vbs", L".ps1", L".msi", L".scr", L".lnk", L".hta", L".jar"};
    for (const wchar_t* r : risky)
        if (_wcsicmp(ext.c_str(), r) == 0 &&
            MessageBoxW(app.wnd, (L"\"" + name + L"\" is a program or script. Open it only if you trust where the document came from. Open it?").c_str(),
                        L"Attached files", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    ShellExecuteW(app.wnd, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void layersDialog() {
    Document* d = activeDoc();
    if (!d) return;
    const auto all = layers::list(*d);
    if (all.empty()) { MessageBoxW(app.wnd, L"This document has no layers.", L"Layers", MB_OK | MB_ICONINFORMATION); return; }
    std::vector<FormField> f;
    f.push_back({FormField::Note, L"", {}, L"Ticked layers are shown and printed; the others are hidden. Saving keeps the choice."});
    for (const auto& l : all) {
        FormField c{FormField::Check, l.name};
        c.checked = l.visible;
        f.push_back(c);
    }
    if (!showForm(app.wnd, app.inst, L"Layers", f, L"Apply")) return;
    std::vector<std::pair<int, bool>> changes;
    for (size_t i = 0; i < all.size(); ++i) if (f[i + 1].checked != all[i].visible) changes.push_back({all[i].object, f[i + 1].checked});
    if (changes.empty()) return;
    if (!layers::setVisible(*d, changes)) { MessageBoxW(app.wnd, L"The layers could not be changed in this document.", L"Layers", MB_OK | MB_ICONWARNING); return; }
    afterPagesChanged();
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
    if (!d || !finishInput()) return false;   // typing in a field or on the page counts as a change once handed over
    if (!d->dirty()) return true;
    return saveDocument(*d, d->path());
}

// Asks about unsaved changes. Returns false when the user cancels.
bool confirmClose(Document& d) {
    // text still being typed (on the page, in a form field) is applied first, so it counts as a change
    // (if it cannot be applied, the reason was shown and the editor stays open: nothing closes)
    if (!finishInput()) return false;
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
    if (app.inlineEd.active()) app.inlineEd.cancel();
    if (redo ? !d->redo() : !d->undo()) return;
    app.sel.page = -1; app.sel.objects.clear(); app.viewer.setObjectSelection(-1, nullptr);   // object indices may differ now
    afterDocReplaced();
}

// ---- annotations ----

COLORREF hlColor() { return kPalette[app.hlColour]; }
COLORREF drawColor() { return kPalette[app.drawColour]; }
double lineWidth() { return kWidths[app.widthIdx]; }

void applyStyle() { app.viewer.setStyle(drawColor(), lineWidth()); }

// What to do with a tool, shown when it is switched on (tools placed by a command say it themselves).
const wchar_t* toolHint(Viewer::Tool t) {
    using T = Viewer::Tool;
    switch (t) {
        case T::Comment: return L"Click where the comment should go. Esc stops.";
        case T::Rectangle: case T::Ellipse: case T::Line: case T::Arrow: return L"Drag on the page to draw. Esc stops.";
        case T::Pen: return L"Hold the mouse button down and draw. Esc stops.";
        case T::Redact: return L"Drag a box over each area to remove, then Edit PDF > Redact > Apply redactions. Esc stops.";
        case T::AddText: return L"Click where the text should start. Esc stops.";
        case T::EditText: return L"Click a line of text to change it. Esc stops.";
        case T::EditPara: return L"Click a paragraph to change it. Esc stops.";
        case T::Link: return L"Drag a box over the text or area that should become a link. Esc stops.";
        case T::CopyArea: return L"Drag a box around the area to copy as a picture. Esc stops.";
        case T::CropBox: return L"Drag a box around the part of the page to keep. Esc stops.";
        case T::TableDelete: return L"The tables found are outlined: click one to delete it, or drag a box around a table that is not outlined. Esc stops.";
        case T::TableEdit: return L"The tables found are outlined: click one to edit it, or drag a box around a table that is not outlined. Esc stops.";
        case T::TablePlace: return L"Click where the top-left corner of the table should go. Esc cancels.";
        case T::Select: return L"Click a picture, paragraph or table to pick it, then drag it or its corners. Esc stops.";
        case T::Polygon: return L"Click each corner; double-click, or click the first corner again, to finish. Esc stops.";
        case T::Cloud: return L"Drag a box where the cloud should go. Esc stops.";
        case T::Callout: return L"Drag from the point to where the text goes. Esc stops.";
        case T::FormText: case T::FormCheck: case T::FormCombo: case T::FormRadio: case T::FormList: case T::FormDate:
            return L"Drag a box where the field should go. Esc stops.";
        case T::Ruler: return L"Drag from one point to another to measure. Esc stops.";
        case T::Protractor: return L"Click the corner of the angle, then a point on each side. Esc stops.";
        default: return nullptr;   // Picture, Stamp, PicCrop: the command that starts them explains
    }
}

void setTool(Viewer::Tool t) {
    if (app.inlineEd.active()) app.inlineEd.commit();
    app.tool = (app.tool == t) ? Viewer::Tool::None : t;   // choosing the active tool again switches it off
    app.viewer.setTool(app.tool);
    InvalidateRect(app.wnd, nullptr, FALSE);   // the toolbar group of the tool and the status bar show it
    if (const wchar_t* hint = toolHint(app.tool)) notice(hint);
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
        notice(L"Select some text first (drag over it), then choose this again.");
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

void deleteAnnotation(Document& d, int page, int index);

void editCallout(Document& d, int page, int index) {
    std::wstring cur;
    for (const auto& a : annot::list(d, page)) if (a.index == index) cur = a.contents;
    std::wstring text;
    if (!askText(L"Callout", L"The text in the box (Enter for a new line).", false, text, toEdit(cur).c_str(), true)) return;
    text = fromEdit(text);
    if (text == cur) return;
    if (text.find_first_not_of(L" \r\n\t") == std::wstring::npos) { deleteAnnotation(d, page, index); return; }   // emptied: the callout goes
    if (!pageobj::isLatin(text)) { MessageBoxW(app.wnd, L"Callouts can use letters, numbers and common symbols of Western languages.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    if (annot::setCalloutText(d, page, index, text)) { app.viewer.clearAnnotSelection(); afterChange(page); }
    else MessageBoxW(app.wnd, L"The callout could not be changed.", kAppName, MB_OK | MB_ICONWARNING);
}

void deleteAnnotation(Document& d, int page, int index) {
    if (annot::remove(d, page, index)) { app.viewer.clearAnnotSelection(); afterChange(page); }
}

void openRecentMenu(POINT screenPt);

// ---- short notices at the bottom of the window, for small things that need no answer ----
LRESULT CALLBACK noticeProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HBRUSH b = CreateSolidBrush(RGB(0x30, 0x33, 0x38));
            FillRect(dc, &rc, b);
            DeleteObject(b);
            wchar_t t[512] = L"";
            GetWindowTextW(h, t, 512);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(0xF2, 0xF2, 0xF2));
            HGDIOBJ of = SelectObject(dc, app.font);
            InflateRect(&rc, -14, 0);
            DrawTextW(dc, t, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(dc, of);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_TIMER: KillTimer(h, 1); ShowWindow(h, SW_HIDE); return 0;
        case WM_LBUTTONDOWN: ShowWindow(h, SW_HIDE); return 0;   // a click dismisses it
    }
    return DefWindowProcW(h, m, w, l);
}

// Shows `text` for a few seconds at the bottom of the page view.
void notice(const std::wstring& text) {
    static HWND n = nullptr;
    if (!n) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = noticeProc;
        wc.hInstance = app.inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"FeatherNotice";
        RegisterClassExW(&wc);
        n = CreateWindowExW(0, L"FeatherNotice", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0, 10, 10, app.wnd, nullptr, app.inst, nullptr);
        if (!n) { MessageBoxW(app.wnd, text.c_str(), kAppName, MB_OK | MB_ICONINFORMATION); return; }
    }
    SetWindowTextW(n, text.c_str());
    HDC dc = GetDC(n);
    HGDIOBJ of = SelectObject(dc, app.font);
    SIZE ext{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &ext);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, of);
    ReleaseDC(n, dc);
    RECT vr;
    GetWindowRect(app.viewer.hwnd(), &vr);
    MapWindowPoints(nullptr, app.wnd, reinterpret_cast<POINT*>(&vr), 2);
    const int w = std::min<int>(ext.cx + 32, vr.right - vr.left - 40), h = tm.tmHeight + 16;
    SetWindowPos(n, HWND_TOP, (vr.left + vr.right - w) / 2, vr.bottom - h - 24, w, h, SWP_SHOWWINDOW);
    InvalidateRect(n, nullptr, TRUE);
    SetTimer(n, 1, 4500, nullptr);
}

// ---- editing text where it is on the page ----
POINT clickPoint(int page, const annot::PointD& p) {
    const RECT r = app.viewer.screenRect(page, {p.x, p.y, p.x, p.y});
    return {r.left, r.top};
}

// One text object, where it is: for turned text, which is not set again as a line.
void editObjectTextInPlace(Document& d, int page, const pageobj::TextHit& hit, POINT click) {
    inlineedit::Spec sp;
    sp.text = hit.text;
    sp.click = click;
    std::string nm = hit.fontName;
    for (auto& c : nm) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    sp.style.family = (hit.flags & 1) || nm.find("courier") != std::string::npos || nm.find("mono") != std::string::npos ? 2
                    : (hit.flags & 2) || nm.find("times") != std::string::npos || nm.find("serif") != std::string::npos ? 1 : 0;
    sp.style.bold = nm.find("bold") != std::string::npos || nm.find("black") != std::string::npos || (hit.flags & (1u << 18));
    sp.style.italic = nm.find("italic") != std::string::npos || nm.find("oblique") != std::string::npos || (hit.flags & 64);
    sp.style.sizePt = hit.shownSizePt > 0 ? hit.shownSizePt : hit.sizePt;
    sp.style.color = hit.color;
    sp.pdfFont = hit.fontName;
    sp.baselineBox = true;
    Document* doc = &d;
    const int index = hit.index;
    sp.measure = [doc, page, index](const std::wstring& t, const inlineedit::Style&) { return activeDoc() == doc ? pageobj::objectTextWidth(*doc, page, index, t) : 0.0; };
    const annot::RectD box{hit.originX, hit.bottom, std::max(hit.right, hit.originX + 1), hit.baseline};
    const std::wstring original = hit.text;
    app.inlineEd.start(sp,
        [doc, page, box](RECT& r, double& ppp) {
            if (activeDoc() != doc) return false;
            r = app.viewer.screenRect(page, box);
            ppp = app.viewer.pixelsPerPoint();
            return true;
        },
        [doc, page, index, original](const std::wstring& text, const inlineedit::Style&) {
            if (activeDoc() != doc || text == original) return true;
            std::wstring flat;   // an edited line stays one object: line breaks are not kept
            for (wchar_t ch : text) flat += (ch == L'\r' || ch == L'\n') ? L' ' : ch;
            if (pageobj::replaceText(*doc, page, index, flat)) { afterChange(page); return true; }
            MessageBoxW(app.wnd, L"The text could not be changed. It may use a font that cannot show those characters. Try letters and numbers of Western languages.", kAppName, MB_OK | MB_ICONWARNING);
            return false;
        });
}

// A line of text, where it is: every object on the clicked baseline that belongs to the line, set again exactly like a
// paragraph (same font where it can draw the letters, same baseline), but always on one line.
void editLineInPlace(Document& d, const pageobj::ParagraphHit& hit, POINT click) {
    inlineedit::Spec sp;
    sp.text = hit.text;
    sp.click = click;
    sp.style.family = static_cast<int>(hit.style.family);
    sp.style.bold = hit.style.bold;
    sp.style.italic = hit.style.italic;
    sp.style.sizePt = hit.style.sizePt;
    sp.style.color = hit.style.color;
    sp.pdfFont = hit.fontName;
    sp.baselineBox = true;
    Document* doc = &d;
    const pageobj::ParagraphHit h = hit;
    sp.measure = [doc, h](const std::wstring& t, const inlineedit::Style&) { return activeDoc() == doc ? pageobj::paragraphTextWidth(*doc, h, t, h.style) : 0.0; };
    const annot::RectD box{hit.originX, hit.baseline, hit.originX + std::max(1.0, hit.wrapWidth), hit.baseline};
    app.inlineEd.start(sp,
        [doc, box, page = hit.page](RECT& r, double& ppp) {
            if (activeDoc() != doc) return false;
            r = app.viewer.screenRect(page, box);
            ppp = app.viewer.pixelsPerPoint();
            return true;
        },
        [doc, h](const std::wstring& text, const inlineedit::Style&) {
            if (activeDoc() != doc) return true;
            std::wstring flat;   // a line stays one line: line breaks are not kept
            for (wchar_t ch : text) flat += (ch == L'\r' || ch == L'\n') ? L' ' : ch;
            if (flat == h.text) return true;
            std::wstring why;
            if (pageobj::replaceParagraph(*doc, h, flat, h.style, pageobj::Align::Left, &why, true)) { afterChange(h.page); return true; }
            app.viewer.documentChanged();   // a failed change puts the document back by reloading it
            refresh();
            MessageBoxW(app.wnd, why.empty() ? L"The text could not be changed." : why.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
            return false;
        });
}

void editParagraphInPlace(Document& d, const pageobj::ParagraphHit& hit, POINT click) {
    inlineedit::Spec sp;
    sp.paragraph = true;
    sp.click = click;
    sp.text = hit.text;
    sp.style.family = static_cast<int>(hit.style.family);
    sp.style.bold = hit.style.bold;
    sp.style.italic = hit.style.italic;
    sp.style.sizePt = hit.style.sizePt;
    sp.style.color = hit.style.color;
    sp.style.align = static_cast<int>(hit.align);
    for (size_t i = 0; i < std::size(kPalette); ++i) { sp.palette.push_back(kPalette[i]); sp.paletteNames.push_back(kPaletteNames[i]); }
    sp.pdfFont = hit.fontName;
    sp.baselineBox = true;
    sp.wrapPt = hit.wrapWidth;
    // the box starts at the paragraph's left edge, its top on the first baseline
    const annot::RectD box{hit.originX, hit.baseline - hit.lineStep * std::max(0, hit.lines - 1), hit.originX + hit.wrapWidth, hit.baseline};
    Document* doc = &d;
    const pageobj::ParagraphHit h = hit;
    sp.measure = [doc, h](const std::wstring& t, const inlineedit::Style& st) {
        if (activeDoc() != doc) return 0.0;
        pageobj::TextStyle ts = h.style;
        ts.family = static_cast<pageobj::Family>(st.family);
        ts.sizePt = st.sizePt;
        ts.bold = st.bold; ts.italic = st.italic;
        return pageobj::paragraphTextWidth(*doc, h, t, ts);
    };
    app.inlineEd.start(sp,
        [doc, box, page = hit.page](RECT& r, double& ppp) {
            if (activeDoc() != doc) return false;
            r = app.viewer.screenRect(page, box);
            ppp = app.viewer.pixelsPerPoint();
            return true;
        },
        [doc, h](const std::wstring& text, const inlineedit::Style& st) {
            if (activeDoc() != doc) return true;
            pageobj::TextStyle ts = h.style;
            ts.family = static_cast<pageobj::Family>(st.family);
            ts.sizePt = st.sizePt;
            ts.bold = st.bold; ts.italic = st.italic;
            ts.color = st.color;
            const bool same = text == h.text && ts.family == h.style.family && std::fabs(ts.sizePt - h.style.sizePt) < 0.05 && ts.bold == h.style.bold &&
                              ts.italic == h.style.italic && ts.color == h.style.color && st.align == static_cast<int>(h.align);
            if (same) return true;
            std::wstring why;
            if (pageobj::replaceParagraph(*doc, h, text, ts, static_cast<pageobj::Align>(st.align), &why)) { app.viewer.documentChanged(); app.sidebar.refresh(); refresh(); return true; }
            app.viewer.documentChanged();   // a failed change puts the document back by reloading it
            refresh();
            MessageBoxW(app.wnd, why.empty() ? L"The paragraph could not be changed." : why.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
            return false;
        });
}

void onViewerEvent(const Viewer::Event& e) {
    // a click anywhere else applies the text being edited (and may start editing the next line)
    if (app.inlineEd.active() && (e.kind == Viewer::Event::Click || e.kind == Viewer::Event::ContextMenu || e.kind == Viewer::Event::Draw) && !app.inlineEd.commit()) return;
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
                const pageobj::ParagraphHit line = pageobj::paragraphAt(*d, e.page, e.pts.front().x, e.pts.front().y, true);
                if (line.found && line.upright) { editLineInPlace(*d, line, clickPoint(e.page, e.pts.front())); break; }
                pageobj::TextHit hit = pageobj::textAt(*d, e.page, e.pts.front().x, e.pts.front().y);
                if (hit.index < 0) {
                    notice(L"No text there. Click directly on a line of text (scanned pages are pictures: use text recognition first).");
                    break;
                }
                editObjectTextInPlace(*d, e.page, hit, clickPoint(e.page, e.pts.front()));
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
            for (const auto& a : annot::list(*d, e.page)) {
                if (a.index != e.annot) continue;
                if (a.callout) editCallout(*d, e.page, e.annot);
                else if (annot::hasComment(a.subtype)) editComment(*d, e.page, e.annot);
                break;
            }
            break;
        case Viewer::Event::ContextMenu: {
            HMENU m = CreatePopupMenu();
            int subtype = 0;
            bool callout = false;
            if (e.annot >= 0)
                for (const auto& a : annot::list(*d, e.page)) if (a.index == e.annot) { subtype = a.subtype; callout = a.callout; }
            if (subtype) {
                if (callout) AppendMenuW(m, MF_STRING, M_EDIT_CALLOUT, L"Edit callout text...");
                if (annot::hasComment(subtype)) AppendMenuW(m, MF_STRING, M_EDIT_COMMENT, subtype == FPDF_ANNOT_TEXT ? L"Edit comment..." : L"Add or edit comment...");
                AppendMenuW(m, MF_STRING, M_DELETE, (std::wstring(L"Delete this ") + (callout ? L"callout" : annot::typeName(subtype))).c_str());
                AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
            }
            int markIdx = -1;
            if (e.page >= 0 && !e.pts.empty())
                for (size_t i = 0; i < d->redactions.size(); ++i) {
                    const RedactMark& mk = d->redactions[i];
                    if (mk.page == e.page && e.pts[0].x >= mk.left && e.pts[0].x <= mk.right && e.pts[0].y >= mk.bottom && e.pts[0].y <= mk.top) markIdx = static_cast<int>(i);
                }
            if (markIdx >= 0) { AppendMenuW(m, MF_STRING, M_RED_REMOVE, L"Remove this redaction mark"); AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
            if (app.tool == Viewer::Tool::Select && app.sel.page >= 0) { addPictureItems(m); AppendMenuW(m, MF_SEPARATOR, 0, nullptr); }
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
            bool onTable = false;
            if (e.page >= 0 && !e.pts.empty())
                for (const annot::RectD& r : tableOutlines(e.page))
                    if (e.pts[0].x >= r.left && e.pts[0].x <= r.right && e.pts[0].y >= r.bottom && e.pts[0].y <= r.top) onTable = true;
            if (onTable) { add(M_TABLE_HERE, L"Edit this table..."); add(M_TABLEDEL_HERE, L"Delete this table"); }
            if (e.page >= 0) { sep(); add(M_COPY_PAGE, L"Copy page as image"); add(N_COPYAREA, L"Copy area as image"); }
            sep();
            add(N_READ_FROM, hasSel ? L"Read the selected text aloud\tCtrl+Shift+U" : L"Read aloud from this page\tCtrl+Shift+U");
            int cmd = trackMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, e.screen.x, e.screen.y, 0, app.wnd, nullptr);
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
                case M_TABLE_HERE: case M_TABLEDEL_HERE: {
                    Viewer::Event ee = e; ee.kind = Viewer::Event::Click; ee.tool = cmd == M_TABLE_HERE ? Viewer::Tool::TableEdit : Viewer::Tool::TableDelete;
                    featureClick(*d, ee);
                    break;
                }
                case M_EDITTEXT_HERE: {
                    Viewer::Event ee = e; ee.kind = Viewer::Event::Click; ee.tool = Viewer::Tool::EditText;
                    onViewerEvent(ee);
                    break;
                }
            }
            switch (cmd) {
                case M_EDIT_COMMENT: editComment(*d, e.page, e.annot); break;
                case M_EDIT_CALLOUT: editCallout(*d, e.page, e.annot); break;
                case M_DELETE: deleteAnnotation(*d, e.page, e.annot); break;
                case M_COPY: app.viewer.copySelection(); break;
                case M_RED_REMOVE: d->redactions.erase(d->redactions.begin() + markIdx); app.viewer.setMarks(&d->redactions); break;
                case M_HIGHLIGHT: markupSelection(annot::Markup::Highlight, false); break;
                case M_UNDERLINE: markupSelection(annot::Markup::Underline, false); break;
                case M_STRIKE: markupSelection(annot::Markup::Strikethrough, false); break;
                case M_COMMENT: markupSelection(annot::Markup::Highlight, true); break;
                case PX_ROT_R: case PX_ROT_L: case PX_FLIP_H: case PX_FLIP_V: case PX_CROP: case PX_ADJUST: case PX_GREY:
                case PX_REPLACE: case PX_SAVE: case PX_FRONT: case PX_BACK: case PX_PASTE: pictureAction(cmd); break;
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
    item(A_POLYGON, L"Polygon (click the corners, double-click to finish)", app.tool == Viewer::Tool::Polygon);
    item(A_CLOUD, L"Cloud (drag a box)", app.tool == Viewer::Tool::Cloud);
    item(A_CALLOUT, L"Callout (drag from the point to where the text goes)", app.tool == Viewer::Tool::Callout);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    showStampMenu(m);
    HMENU sg = CreatePopupMenu();
    AppendMenuW(sg, MF_STRING, N_SIG_TYPE, L"Type my name...");
    AppendMenuW(sg, MF_STRING, N_SIG_DRAW, L"Draw my signature...");
    AppendMenuW(sg, MF_STRING, N_SIG_PIC, L"Use a picture of my signature...");
    {
        std::wstring saved = signaturePathFwd();
        AppendMenuW(sg, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(sg, MF_STRING | (GetFileAttributesW(saved.c_str()) != INVALID_FILE_ATTRIBUTES ? 0 : MF_GRAYED), N_SIG_SAVED, L"Use my saved signature");
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
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);   // also destroys the submenus
    if (cmd >= M_HL_COLOUR && cmd < M_HL_COLOUR + 7) { app.hlColour = cmd - M_HL_COLOUR; return; }
    if (cmd >= M_DRAW_COLOUR && cmd < M_DRAW_COLOUR + 7) { app.drawColour = cmd - M_DRAW_COLOUR; applyStyle(); return; }
    if (cmd >= M_WIDTH && cmd < M_WIDTH + 3) { app.widthIdx = cmd - M_WIDTH; applyStyle(); return; }
    if (cmd >= N_TEXTSIZE && cmd < N_TEXTSIZE + 9) { app.textStyle.sizePt = kTextSizes[cmd - N_TEXTSIZE]; saveSettings(); return; }
    if (cmd >= T_STAMP && cmd <= T_STAMP_CUSTOM) { toolsDispatch(cmd); return; }
    if (cmd >= A_DATESTAMP && cmd < A_DATESTAMP + 4) {
        SYSTEMTIME now; GetLocalTime(&now);
        wchar_t date[64] = L"", time[32] = L"";
        GetDateFormatW(LOCALE_USER_DEFAULT, 0, &now, L"dd MMM yyyy", date, 64);
        GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &now, L"HH:mm", time, 32);
        static const wchar_t* words[] = {L"", L"APPROVED ", L"RECEIVED ", L"PAID "};
        static const COLORREF cols[] = {RGB(0x25, 0x6F, 0xD1), RGB(0x2E, 0x9E, 0x4A), RGB(0x25, 0x6F, 0xD1), RGB(0xD0, 0x2B, 0x2B)};
        std::wstring t = std::wstring(words[cmd - A_DATESTAMP]) + date + (cmd == A_DATESTAMP ? std::wstring(L" ") + time : L"");
        bool ok = true;
        for (auto& c : t) if (c > 0xFF) ok = false;
        if (!ok) { wchar_t iso[32]; swprintf(iso, 32, L"%04d-%02d-%02d %02d:%02d", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute); t = std::wstring(words[cmd - A_DATESTAMP]) + iso; }
        startStamp(t, cols[cmd - A_DATESTAMP]);
        return;
    }
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
        case A_POLYGON: setTool(Viewer::Tool::Polygon); break;
        case A_CLOUD: setTool(Viewer::Tool::Cloud); break;
        case A_CALLOUT: setTool(Viewer::Tool::Callout); break;
        case T_PICTURE: toolsDispatch(cmd); break;
        case N_LINK: startLink(); break;
        case N_SIG_TYPE: case N_SIG_DRAW: case N_SIG_PIC: case N_SIG_SAVED: signatureAction(cmd); break;
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
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, F_FLATTEN, L"Flatten: make comments and fields part of the page...");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
        case M_COMMENT: setTool(Viewer::Tool::Comment); break;
        case M_SQUIGGLY + 100: markupSelection(annot::Markup::Highlight, true); break;
        case N_SELECT_NOTES: toolsDispatch(T_NOTES); break;
        case F_FLATTEN: flattenDocument(); break;
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
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
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
    AppendMenuW(m, MF_STRING | (app.sbsLinked ? MF_CHECKED : 0), N_SBS_LINK, L"Link scrolling (both scroll and zoom together)");
    AppendMenuW(m, MF_STRING | (!app.sbsStacked ? MF_CHECKED : 0), N_SBS_HORZ, L"Left and right");
    AppendMenuW(m, MF_STRING | (app.sbsStacked ? MF_CHECKED : 0), N_SBS_VERT, L"Top and bottom");
    AppendMenuW(m, MF_STRING | (app.sbs ? 0 : MF_GRAYED), N_SBS_SWAP, L"Swap the two sides");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (two ? 0 : MF_GRAYED), N_COMPARE, L"Compare with another open tab...");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
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
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
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

void splitPdf();
void newPdfFromPictures();
void cropByBox();

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
        case P_SPLIT: splitPdf(); break;
        case P_NEWPDF: newPdfFromPictures(); break;
        case P_CROPBOX: cropByBox(); break;
        case P_REVERSE: if (d->reversePages()) afterPagesChanged(); break;
        case P_INTERLEAVE: {
            // a stack scanned on one side, turned over and scanned again: fronts 1, 2, 3 then backs 3, 2, 1
            std::vector<FormField> f(3);
            f[0] = {FormField::Note, L"", {}, L"For a two-sided paper scanned on a one-sided scanner: first all the fronts, then all the backs. The pages are put in order: front 1, back 1, front 2, back 2 ..."};
            f[1] = {FormField::Choice, L"The back pages are:", {L"In the second half of this document", L"In another PDF file (choose it next)"}, L"", 0};
            f[2] = {FormField::Check, L"The backs are in reverse order (the stack was turned over)"};
            f[2].checked = true;
            if (!showForm(app.wnd, app.inst, L"Two-sided scan", f, L"Put in order")) break;
            std::unique_ptr<Document> backs;
            if (f[1].choice == 1) {
                std::vector<std::wstring> files;
                pickPdf(false, files, L"Choose the PDF with the back pages");
                if (files.empty()) break;
                backs = openForImport(files[0]);
                if (!backs) break;
            } else if (d->pageCount() < 2) break;
            std::vector<unsigned char> bytes;
            if (!splitmerge::interleave(*d, backs.get(), f[2].checked, bytes)) {
                MessageBoxW(app.wnd, L"The pages could not be put in order.", kAppName, MB_OK | MB_ICONWARNING);
                break;
            }
            d->beginChange(L"Two-sided scan: pages in order");
            if (d->reload(std::move(bytes)) != OpenResult::Ok) { d->rollbackChange(); break; }
            afterPagesChanged();
            notice(L"The pages are in order: front 1, back 1, front 2, back 2 ... Undo (Ctrl+Z) puts them back.");
            break;
        }
        case P_BLANKS: {
            HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            std::vector<int> blank = pageobj::blankPages(*d);
            SetCursor(old);
            if (blank.empty()) { MessageBoxW(app.wnd, L"No blank pages were found.", kAppName, MB_OK | MB_ICONINFORMATION); break; }
            if (static_cast<int>(blank.size()) >= d->pageCount()) { MessageBoxW(app.wnd, L"Every page looks blank, so none was removed.", kAppName, MB_OK | MB_ICONINFORMATION); break; }
            std::wstring list;
            for (size_t i = 0; i < blank.size() && i < 30; ++i) list += (i ? L", " : L"") + std::to_wstring(blank[i] + 1);
            if (blank.size() > 30) list += L", ...";
            if (MessageBoxW(app.wnd, (std::to_wstring(blank.size()) + L" blank page(s) found: " + list + L".\n\nRemove them?").c_str(), L"Remove blank pages", MB_YESNO | MB_ICONQUESTION) == IDYES && d->deletePages(blank)) afterPagesChanged();
            break;
        }
        case P_DUPES: {
            HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            auto groups = pageobj::duplicatePages(*d);
            SetCursor(old);
            if (groups.empty()) { MessageBoxW(app.wnd, L"No duplicate pages were found.", kAppName, MB_OK | MB_ICONINFORMATION); break; }
            std::wstring msg = L"These pages look the same:\n";
            std::vector<int> extra;
            for (size_t g = 0; g < groups.size() && g < 15; ++g) {
                msg += L"  pages";
                for (size_t k = 0; k < groups[g].size(); ++k) msg += (k ? L", " : L" ") + std::to_wstring(groups[g][k] + 1);
                msg += L"\n";
            }
            for (auto& g : groups) for (size_t k = 1; k < g.size(); ++k) extra.push_back(g[k]);
            msg += L"\nDelete the later copies (" + std::to_wstring(extra.size()) + L" page(s)) and keep the first of each?";
            if (MessageBoxW(app.wnd, msg.c_str(), L"Duplicate pages", MB_YESNO | MB_ICONQUESTION) == IDYES && d->deletePages(extra)) afterPagesChanged();
            break;
        }
        case P_HEADER: {
            std::vector<FormField> f(10);
            f[0] = {FormField::Note, L"", {}, L"Leave a box empty for nothing there. You can use {page} {pages} {date} {file} {title}."};
            f[1] = {FormField::Text, L"Header, left:", {}, L""};
            f[2] = {FormField::Text, L"Header, centre:", {}, L"{title}"};
            f[3] = {FormField::Text, L"Header, right:", {}, L""};
            f[4] = {FormField::Text, L"Footer, left:", {}, L"{date}"};
            f[5] = {FormField::Text, L"Footer, centre:", {}, L""};
            f[6] = {FormField::Text, L"Footer, right:", {}, L"Page {page} of {pages}"};
            f[7] = {FormField::Choice, L"Size:", {L"8 pt", L"9 pt", L"10 pt", L"12 pt"}, L"", 1};
            f[8] = {FormField::Text, L"First page number:", {}, L"1"};
            f[9] = {FormField::Text, L"Pages (empty = all):", {}, L""};
            g_formDoc = d;
            bool ok = showForm(app.wnd, app.inst, L"Headers and footers", f, L"Add", [](const std::vector<FormField>& v) -> std::wstring {
                for (int i = 1; i <= 6; ++i) if (!pageobj::isLatin(v[i].text)) return L"Use letters, numbers and common symbols of Western languages.";
                wchar_t* e = nullptr; wcstol(v[8].text.c_str(), &e, 10);
                if (e == v[8].text.c_str() || *e) return L"Type a number for the first page number.";
                return badPages(v[9].text); });
            g_formDoc = nullptr;
            if (!ok) break;
            pageobj::HeaderFooter hf;
            for (int i = 0; i < 6; ++i) hf.text[i] = f[i + 1].text;
            static const double sizes[] = {8, 9, 10, 12};
            hf.sizePt = sizes[f[7].choice];
            hf.firstNumber = static_cast<int>(wcstol(f[8].text.c_str(), nullptr, 10));
            if (pageobj::addHeaderFooter(*d, pagesFromText(*d, f[9].text), hf)) afterPagesChanged();
            else MessageBoxW(app.wnd, L"Nothing was added (all boxes are empty?).", kAppName, MB_OK | MB_ICONINFORMATION);
            break;
        }
        case P_BGCOLOR: {
            std::vector<FormField> f(2);
            static const wchar_t* names[] = {L"Light yellow (easy reading)", L"Light green", L"Light blue", L"Light grey", L"Cream", L"White"};
            static const COLORREF cols[] = {RGB(0xFF, 0xF8, 0xD0), RGB(0xE4, 0xF4, 0xE0), RGB(0xE2, 0xEE, 0xFA), RGB(0xEE, 0xEE, 0xEE), RGB(0xFB, 0xF4, 0xE4), RGB(0xFF, 0xFF, 0xFF)};
            f[0] = {FormField::Choice, L"Colour:", {names[0], names[1], names[2], names[3], names[4], names[5]}, L"", 0};
            f[1] = {FormField::Text, L"Pages (empty = all):", {}, L""};
            g_formDoc = d;
            bool ok = showForm(app.wnd, app.inst, L"Background colour", f, L"Add", [](const std::vector<FormField>& v) -> std::wstring { return badPages(v[1].text); });
            g_formDoc = nullptr;
            if (!ok) break;
            if (pageobj::addBackground(*d, pagesFromText(*d, f[1].text), cols[f[0].choice])) afterPagesChanged();
            break;
        }
        case P_HFDELETE: {
            if (hfdialog::run(app.wnd, app.inst, *d, cur, app.dpi, app.font, app.fontBold)) {
                afterPagesChanged();
                notice(L"The header and footer items were deleted. Undo (Ctrl+Z) brings them back.");
            }
            break;
        }
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
            std::vector<FormField> bm(1);
            bm[0] = {FormField::Choice, L"Bookmarks:", {L"Keep the bookmarks of every file", L"A bookmark for each file, its own bookmarks inside", L"Add the pages only"}, L"", 0};
            if (!showForm(app.wnd, app.inst, L"Merge PDFs", bm, L"Merge")) break;
            if (bm[0].choice < 2) {   // one new file with every bookmark: one undo step
                std::vector<std::unique_ptr<Document>> docs;
                std::vector<splitmerge::Source> src{{d, d->title()}};
                for (const auto& f : files) {
                    auto one = openForImport(f);
                    if (!one) continue;
                    src.push_back({one.get(), one->title()});
                    docs.push_back(std::move(one));
                }
                for (auto& s2 : src) { size_t dot = s2.name.find_last_of(L'.'); if (dot != std::wstring::npos) s2.name.resize(dot); }
                std::vector<unsigned char> bytes;
                HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
                const bool ok = src.size() > 1 && splitmerge::merge(src, bm[0].choice == 0 ? splitmerge::Marks::Keep : splitmerge::Marks::UnderFileNames, bytes);
                SetCursor(old);
                if (!ok) { if (src.size() > 1) MessageBoxW(app.wnd, L"The files could not be merged.", kAppName, MB_OK | MB_ICONWARNING); break; }
                const int before = d->pageCount();
                d->beginChange(L"Merge PDFs");
                if (d->reload(std::move(bytes)) != OpenResult::Ok) { d->rollbackChange(); break; }
                afterPagesChanged();
                app.viewer.goToPage(before);
                break;
            }
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

// Read aloud: the text from the selection or the page shown on; the word being spoken is tinted on the page, and the
// controls below the pages go back or forward a sentence, pause, change the speed, or stop.
struct Reading {
    readaloud::Script script;
    size_t base = 0, at = 0;   // where the voice started in the script; the word it is on
    bool paused = false;
} g_reading;

void updateReadingBar() {
    SetWindowTextW(app.readPause, g_reading.paused ? L"Resume" : L"Pause");
    int page = -1, start = 0, count = 0;
    readaloud::span(g_reading.script, g_reading.at, 1, page, start, count);
    static const wchar_t* speeds[] = {L"slow", L"normal", L"fast", L"very fast"};
    const int sp = app.readRate <= -4 ? 0 : app.readRate == 0 ? 1 : app.readRate <= 3 ? 2 : 3;
    SetWindowTextW(app.readLabel, ((g_reading.paused ? L"Paused on page " : L"Reading page ") + std::to_wstring(page + 1) + L"  \u00B7  " + speeds[sp]).c_str());
}

void endReading() {
    speech::stop();
    g_reading = Reading();
    app.viewer.setSpoken(-1, 0, 0);
    if (app.readVisible) { app.readVisible = false; showControls(); }
}

void stopReading() { endReading(); }

bool readFrom(size_t offset) {
    offset = std::min(offset, g_reading.script.text.size());
    if (offset >= g_reading.script.text.size()) { endReading(); return false; }
    g_reading.base = g_reading.at = offset;
    g_reading.paused = false;
    std::wstring err;
    if (!speech::start(g_reading.script.text.substr(offset), app.readRate, err, app.wnd, kMsgSpeech)) {
        endReading();
        MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
        return false;
    }
    if (!app.readVisible) { app.readVisible = true; showControls(); }
    updateReadingBar();
    return true;
}

void readingNews() {
    speech::events([](int kind, size_t pos, size_t len) {
        if (kind == 1) { endReading(); return; }
        g_reading.at = g_reading.base + pos;
        int page, start = 0, count = 0;
        if (readaloud::span(g_reading.script, g_reading.at, len ? len : readaloud::wordLength(g_reading.script, g_reading.at), page, start, count))
            app.viewer.setSpoken(page, start, count);
        updateReadingBar();
    });
}

void readingControl(int id) {
    if (!app.readVisible) return;
    switch (id) {
        case ID_READ_BACK: readFrom(readaloud::sentence(g_reading.script, g_reading.at, -1)); break;
        case ID_READ_FWD: readFrom(readaloud::sentence(g_reading.script, g_reading.at, 1)); break;
        case ID_READ_PAUSE: speech::pauseResume(); g_reading.paused = speech::paused(); updateReadingBar(); break;
        case ID_READ_SLOWER: case ID_READ_FASTER: {
            static const int rates[] = {-4, 0, 3, 6};
            int i = app.readRate <= -4 ? 0 : app.readRate == 0 ? 1 : app.readRate <= 3 ? 2 : 3;
            i = std::clamp(i + (id == ID_READ_FASTER ? 1 : -1), 0, 3);
            app.readRate = rates[i];
            readFrom(readaloud::sentence(g_reading.script, g_reading.at, 0));   // the sentence again, at the new speed
            break;
        }
        case ID_READ_STOP: endReading(); break;
    }
}

void readAloud() {
    Document* d = activeDoc();
    if (!d) return;
    if (app.readVisible || speech::speaking()) { endReading(); return; }
    int fromPage = app.viewer.currentPage(), fromChar = 0;
    size_t limit = 200000;
    if (app.viewer.hasSelection()) {   // the selection only
        const std::wstring sel = app.viewer.selectedText();
        const auto rects = app.viewer.selectionRects();
        if (!rects.empty()) fromPage = rects.front().page;
        int sp = -1, sc = 0;
        if (app.viewer.selectionStart(sp, sc)) { fromPage = sp; fromChar = sc; }
        limit = sel.size() + 1;
    }
    g_reading = Reading();
    g_reading.script = readaloud::build(*d, fromPage, fromChar, limit);
    if (g_reading.script.text.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        MessageBoxW(app.wnd, L"There is no text to read here. Scanned pages are pictures; they need text recognition (OCR) first.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    readFrom(0);
}

void printDocument();

// The user guide is installed next to the program; it opens in a tab like any PDF.
void openUserGuide() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    dir.resize(dir.find_last_of(L"\\/") + 1);
    const std::wstring guide = dir + L"Feather PDF User Guide.pdf";
    if (GetFileAttributesW(guide.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(app.wnd, L"The user guide (Feather PDF User Guide.pdf) was not found next to the program. Reinstalling Feather PDF puts it back.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (size_t i = 0; i < app.tabs.size(); ++i)
        if (_wcsicmp(app.tabs[i]->path().c_str(), guide.c_str()) == 0) { switchTab(static_cast<int>(i)); return; }
    openFile(guide);
}

// The Print window: settings and a live preview of every sheet in one place.
void printDocument() {
    Document* d = activeDoc();
    if (!d) return;
    if (!finishInput()) return;   // text still being typed prints as shown
    printing::run(app.wnd, app.inst, *d, app.viewer.currentPage());
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
    showForm(app.wnd, app.inst, L"Document properties", f, L"!Close");
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

// Finds e-mail addresses, phone numbers, Aadhaar numbers, PAN and bank details, lets the user choose which to hide,
// and marks those for redaction (Apply redactions then removes them for good, as for any other marks).
void findSensitive(Document& d) {
    const auto& kinds = sensitive::kinds();
    std::vector<FormField> f(kinds.size() + 1);
    f[0] = {FormField::Note, L"", {}, L"Looks through the text of every page for:"};
    for (size_t i = 0; i < kinds.size(); ++i) {
        f[i + 1] = {FormField::Check, sensitive::name(kinds[i])};
        f[i + 1].checked = true;
    }
    if (!showForm(app.wnd, app.inst, L"Find sensitive information", f, L"Find", [](const std::vector<FormField>& v) -> std::wstring {
            for (size_t i = 1; i < v.size(); ++i) if (v[i].checked) return L"";
            return L"Choose at least one kind of information to look for."; })) return;
    unsigned want = 0;
    for (size_t i = 0; i < kinds.size(); ++i) if (f[i + 1].checked) want |= kinds[i];
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    const std::vector<sensitive::Hit> hits = sensitive::scan(d, want);
    SetCursor(old);
    if (hits.empty()) {
        MessageBoxW(app.wnd, L"None was found in the text of this document.\n\nScanned pages have no text to look through: recognise their text first (OCR), then look again.", L"Find sensitive information", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // one check box per item when there are few; one per kind when there are many
    const bool each = hits.size() <= 14;
    std::vector<FormField> g;
    std::wstring summary = L"Found " + std::to_wstring(hits.size()) + (hits.size() == 1 ? L" item" : L" items") + L":";
    for (sensitive::Kind k : kinds) {
        size_t n = 0;
        for (const auto& h : hits) n += h.kind == k;
        if (n) summary += L"  " + std::wstring(sensitive::name(k)) + L" " + std::to_wstring(n) + L";";
    }
    summary.back() = L'.';
    summary += L"\nThe ticked ones are marked for redaction. Look over the black boxes, then choose Edit PDF > Redact > Apply redactions.";
    g.push_back({FormField::Note, L"", {}, summary});
    std::vector<sensitive::Kind> groupKinds;
    if (each) {
        for (const auto& h : hits) {
            FormField c{FormField::Check, L"Page " + std::to_wstring(h.page + 1) + L"  \u00b7  " + sensitive::name(h.kind) + L":  " + h.text.substr(0, 48)};
            c.checked = true;
            g.push_back(c);
        }
    } else {
        for (sensitive::Kind k : kinds) {
            size_t n = 0;
            for (const auto& h : hits) n += h.kind == k;
            if (!n) continue;
            FormField c{FormField::Check, std::wstring(sensitive::name(k)) + L" (" + std::to_wstring(n) + L")"};
            c.checked = true;
            g.push_back(c);
            groupKinds.push_back(k);
        }
    }
    if (!showForm(app.wnd, app.inst, L"Sensitive information found", g, L"Mark for redaction")) return;
    std::vector<sensitive::Hit> chosen;
    for (size_t i = 0; i < hits.size(); ++i) {
        bool on = false;
        if (each) on = g[i + 1].checked;
        else for (size_t k = 0; k < groupKinds.size(); ++k) on = on || (groupKinds[k] == hits[i].kind && g[k + 1].checked);
        if (on) chosen.push_back(hits[i]);
    }
    if (chosen.empty()) return;
    const int n = sensitive::mark(d, chosen);
    app.viewer.setMarks(&d.redactions);
    app.viewer.goToPage(chosen.front().page);
    notice(std::to_wstring(chosen.size()) + L" item(s) marked (" + std::to_wstring(n) + L" box(es)). Check them, then choose Edit PDF > Redact > Apply redactions.");
}

void redactionMenuAction(Document& d, int cmd) {
    switch (cmd) {
        case T_RED_AREA:
            setTool(Viewer::Tool::Redact);
            break;
        case T_RED_SEL: {
            auto sel = app.viewer.selectionRects();
            if (sel.empty()) { notice(L"Select some text first (drag over it), then choose this again."); break; }
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
        case N_RED_SENSITIVE: findSensitive(d); break;
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
    if (!ocr::available(why)) {
        if (MessageBoxW(app.wnd, (why + L"\n\nShow the fonts used in this PDF instead? (Pages without any fonts are scans.)").c_str(), kAppName, MB_YESNO | MB_ICONINFORMATION) == IDYES) showFonts(d);
        return;
    }
    auto langs = ocr::languages();
    std::vector<FormField> f(5);
    f[0] = {FormField::Note, L"", {}, L"Finds the words on scanned pages and adds them invisibly, so the pages can be searched, selected and copied. The pages look exactly the same."};
    f[1] = {FormField::Choice, L"Language:", {}, L"", 0};
    for (auto& l : langs) f[1].choices.push_back(l);
    f[2] = {FormField::Text, L"Pages (empty = all):", {}, L""};
    f[3] = {FormField::Check, L"Skip pages that already have text"}; f[3].checked = true;
    f[4] = {FormField::Check, L"First show the fonts used in this PDF (and which pages are scans)"};
    g_formDoc = &d;
    bool ok = showForm(app.wnd, app.inst, L"Recognise text (OCR)", f, L"Recognise", [](const std::vector<FormField>& v) { return badPages(v[2].text); });
    g_formDoc = nullptr;
    if (!ok) return;
    std::vector<int> pages = pagesFromText(d, f[2].text);
    if (f[4].checked) {
        showFonts(d);
        if (MessageBoxW(app.wnd, L"Recognise the text now?", L"Recognise text (OCR)", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
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


// A recent file in a menu: its name, with its folder when another recent file has the same name. "&" is doubled, or the
// menu would show it as an underlined access key.
std::wstring recentLabel(size_t i, bool withFolder) {
    const std::wstring& p = app.recent[i];
    const size_t cut = p.find_last_of(L"\\/");
    const std::wstring name = cut == std::wstring::npos ? p : p.substr(cut + 1);
    bool clash = false;
    for (size_t j = 0; j < app.recent.size() && !clash; ++j) {
        if (j == i) continue;
        const size_t c = app.recent[j].find_last_of(L"\\/");
        clash = _wcsicmp((c == std::wstring::npos ? app.recent[j] : app.recent[j].substr(c + 1)).c_str(), name.c_str()) == 0;
    }
    std::wstring label = name;
    if ((withFolder || clash) && cut != std::wstring::npos) {
        std::wstring folder = p.substr(0, cut);
        if (folder.size() > 60) folder = folder.substr(0, 24) + L"\x2026" + folder.substr(folder.size() - 33);   // the drive and the last folders
        label += L"    " + folder;
    }
    std::wstring out;
    for (wchar_t c : label) { out += c; if (c == L'&') out += L'&'; }
    return out;
}

// Opens recent file `i`; one that no longer exists is taken off the list.
void openRecent(size_t i) {
    if (i >= app.recent.size()) return;
    const std::wstring p = app.recent[i];
    if (GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(app.wnd, (L"\"" + p + L"\" no longer exists, so it was removed from the list.").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
        app.recent.erase(app.recent.begin() + static_cast<std::ptrdiff_t>(i));
        updateEmptyText();
        saveSettings();
        return;
    }
    openFile(p);
}

void openRecentMenu(POINT screenPt) {
    HMENU m = CreatePopupMenu();
    if (app.recent.empty()) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"No recent files");
    for (size_t i = 0; i < app.recent.size(); ++i) AppendMenuW(m, MF_STRING, T_RECENT + i, recentLabel(i, true).c_str());
    if (!app.recent.empty()) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, T_RECENT_CLEAR, L"Clear the list"); }
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, screenPt.x, screenPt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd == T_RECENT_CLEAR) { app.recent.clear(); updateEmptyText(); saveSettings(); }
    else if (cmd >= T_RECENT && cmd < T_RECENT + 10) openRecent(static_cast<size_t>(cmd - T_RECENT));
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
    // listed on Windows' Default apps page under its own name (the installer does the same for every user)
    set(L"Software\\FeatherPDF\\Capabilities", L"ApplicationName", kAppName);
    set(L"Software\\FeatherPDF\\Capabilities", L"ApplicationDescription", L"A fast, free PDF reader and editor.");
    set(L"Software\\FeatherPDF\\Capabilities\\FileAssociations", L".pdf", L"FeatherPDF.Document");
    set(L"Software\\RegisteredApplications", kAppName, L"Software\\FeatherPDF\\Capabilities");
    if (!ok) { MessageBoxW(app.wnd, L"The file association could not be registered.", kAppName, MB_OK | MB_ICONWARNING); return; }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    if (MessageBoxW(app.wnd, L"Feather PDF now appears in the \"Open with\" list for PDF files.\n\nWindows only lets you choose the default app yourself. "
                             L"Open the Windows settings to choose Feather PDF for .pdf files?", kAppName, MB_YESNO | MB_ICONQUESTION) == IDYES)
        // Windows 11 opens Feather PDF's own page there; older versions ignore the address's end and show the list
        if (reinterpret_cast<INT_PTR>(ShellExecuteW(app.wnd, L"open", L"ms-settings:defaultapps?registeredAppUser=Feather%20PDF", nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
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
        notice(L"Click on the page where the top-left corner of the picture should go. Esc cancels.");
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
    showForm(app.wnd, app.inst, L"Digital signatures", f, L"!Close");
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
        MessageBoxW(app.wnd, L"Remove the password protection first (Edit PDF > Remove password protection), then sign. A signed file cannot be changed afterwards.", kAppName, MB_OK | MB_ICONINFORMATION);
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
    app.presenting = on && presentation;
    app.viewer.setPresenting(app.presenting);
    const int keep = activeDoc() ? app.viewer.currentPage() : -1;   // the page being read stays on screen
    if (on == app.fullscreen) {
        if (on && presentation) { app.viewer.setLayout(0, app.cover); app.viewer.fitPage(); if (keep >= 0) app.viewer.goToPage(keep); }
        return;
    }
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
        for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findWord, app.findWild, app.findNotes, app.findCount, app.findClose}) ShowWindow(c, SW_HIDE);
    } else {
        app.fullscreen = false;
        SetWindowLong(app.wnd, GWL_STYLE, app.savedStyle);
        SetWindowPlacement(app.wnd, &app.savedPlace);
        SetWindowPos(app.wnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        showControls();
    }
    relayoutChildren();
    InvalidateRect(app.wnd, nullptr, TRUE);
    // a presentation shows one page per screen; the chosen layout comes back afterwards
    if (on && presentation) app.viewer.setLayout(0, app.cover);
    else if (!on) app.viewer.setLayout(app.layout, app.cover);
    if (on) { if (presentation) app.viewer.fitPage(); else app.viewer.fitWidth(); }
    else if (activeDoc()) app.viewer.fitWidth();
    if (keep >= 0) app.viewer.goToPage(keep);
}

void startStamp(const std::wstring& text, COLORREF col) {
    app.pendingStamp = text;
    app.pendingStampColour = col;
    if (app.tool != Viewer::Tool::Stamp) setTool(Viewer::Tool::Stamp);
    notice(L"Click on the page where the stamp should go (its centre lands on the click). Esc cancels.");
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
    AppendMenuW(st, MF_STRING, A_DATESTAMP, L"Date and time");
    AppendMenuW(st, MF_STRING, A_DATESTAMP + 1, L"APPROVED with date");
    AppendMenuW(st, MF_STRING, A_DATESTAMP + 2, L"RECEIVED with date");
    AppendMenuW(st, MF_STRING, A_DATESTAMP + 3, L"PAID with date");
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
        case N_SANITIZE: sanitizeDialog(); break;
        case N_PDFA: pdfaDialog(); break;
        case N_FONTS: showFonts(*d); break;
        case N_ACCESS: accessibilityCheck(*d); break;
        case N_REVIEW: reviewDialog(*d); break;
        case F_EXPORT: formExport(); break;
        case F_FILL: formFillFromCsv(); break;
        case F_TEXT: startFormField(Viewer::Tool::FormText); break;
        case F_CHECK: startFormField(Viewer::Tool::FormCheck); break;
        case F_COMBO: startFormField(Viewer::Tool::FormCombo); break;
        case F_AUTO: formAutoDetect(); break;
        case F_RADIO: startFormField(Viewer::Tool::FormRadio); break;
        case F_LIST: startFormField(Viewer::Tool::FormList); break;
        case F_DATE: startFormField(Viewer::Tool::FormDate); break;
        case F_RESET: formReset(); break;
        case F_FLATTEN: flattenDocument(); break;
        default:
            if (cmd >= PX_ROT_R && cmd <= PX_PASTE) { pictureAction(cmd); break; }
            if (cmd >= T_STAMP && cmd < T_STAMP + 7) { startStamp(kStampNames[cmd - T_STAMP], kStampCols[cmd - T_STAMP]); break; }
            if ((cmd >= T_RED_AREA && cmd <= T_RED_APPLY) || cmd == N_RED_CHECK || cmd == N_RED_SENSITIVE) { redactionMenuAction(*d, cmd); break; }
            if (cmd >= P_DELETE && cmd <= P_INTERLEAVE) { pagesMenuAction(cmd, {}); break; }
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
    { HMENU pm = CreatePopupMenu(); addPictureItems(pm); AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(pm), L"Picture (turn, crop, brightness...)"); }
    add(N_FINDREPLACE, L"Find and replace text...\tCtrl+Shift+H");
    add(N_TABLE_INS, L"Insert table...");
    add(N_TABLE_EDIT, L"Edit a table (any table: found automatically)...");
    add(N_TABLE_DEL, L"Delete table", true, app.tool == Viewer::Tool::TableDelete);
    sep();
    add(T_ADD_TEXT, L"Add a text box", true, app.tool == Viewer::Tool::AddText);
    add(T_TEXT_STYLE, L"Style of new text...");
    add(T_NOTES, L"Notes summary (highlights and comments)...");
    add(N_REVIEW, L"Comments and review: reply, accept, reject, summary...");
    add(N_BOOKMARK, L"Add a bookmark for this page...");
    sep();
    add(T_OCR, L"Recognise text (OCR)...");
    add(N_FONTS, L"Fonts used in this PDF...");
    add(N_ACCESS, L"Accessibility: check and fix...");
    add(T_WATERMARK, L"Watermark...");
    add(T_NUMBERS, L"Page numbers...");
    add(P_HEADER, L"Add headers and footers...");
    add(P_HFDELETE, L"Delete header and footer...");
    sep();
    HMENU frm = CreatePopupMenu();
    AppendMenuW(frm, MF_STRING, F_EXPORT, L"Export the answers to Excel or CSV...");
    AppendMenuW(frm, MF_STRING, F_FILL, L"Fill from a spreadsheet (CSV)...");
    AppendMenuW(frm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormText ? MF_CHECKED : 0), F_TEXT, L"Add a text field");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormCheck ? MF_CHECKED : 0), F_CHECK, L"Add a check box");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormCombo ? MF_CHECKED : 0), F_COMBO, L"Add a drop-down list");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormRadio ? MF_CHECKED : 0), F_RADIO, L"Add radio buttons (one choice of several)");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormList ? MF_CHECKED : 0), F_LIST, L"Add a list box");
    AppendMenuW(frm, MF_STRING | (app.tool == Viewer::Tool::FormDate ? MF_CHECKED : 0), F_DATE, L"Add a date field");
    AppendMenuW(frm, MF_STRING, F_AUTO, L"Find fields in a flat form...");
    AppendMenuW(frm, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(frm, MF_STRING, F_RESET, L"Clear all answers (reset the form)");
    AppendMenuW(frm, MF_STRING, F_FLATTEN, L"Flatten: make fields and comments part of the page...");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(frm), L"Fillable form");
    HMENU red = CreatePopupMenu();
    AppendMenuW(red, MF_STRING | (app.tool == Viewer::Tool::Redact ? MF_CHECKED : 0), T_RED_AREA, L"Mark areas to redact");
    AppendMenuW(red, MF_STRING, T_RED_SEL, L"Mark selected text");
    AppendMenuW(red, MF_STRING, T_RED_FIND, L"Find and mark...");
    AppendMenuW(red, MF_STRING, N_RED_SENSITIVE, L"Find sensitive information (e-mail, phone, Aadhaar, PAN, bank)...");
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
    add(N_SANITIZE, L"Remove hidden data (metadata, attachments, scripts)...");
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
    padd(P_REVERSE, L"Reverse the page order", d->pageCount() > 1);
    padd(P_INTERLEAVE, L"Two-sided scan: put fronts and backs in order...");
    padd(P_BLANKS, L"Remove blank pages...", d->pageCount() > 1);
    padd(P_DUPES, L"Find duplicate pages...", d->pageCount() > 1);
    AppendMenuW(pg, MF_SEPARATOR, 0, nullptr);
    padd(P_BGCOLOR, L"Background colour...");
    AppendMenuW(pg, MF_SEPARATOR, 0, nullptr);
    padd(P_CROP, L"Crop pages...");
    padd(P_CROPBOX, L"Crop by drawing a box");
    AppendMenuW(pg, MF_SEPARATOR, 0, nullptr);
    padd(P_EXTRACT, L"Extract pages into a new file...");
    padd(P_SPLIT, L"Split into several files...");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(pg), L"Edit pages");
    sep();
    add(T_EXPORT, L"Export as pictures, text, Word or Excel...");
    add(N_PDFA, L"Save a PDF/A copy (for archiving)...");
    HMENU rate = CreatePopupMenu();
    add(T_READ, speech::speaking() ? L"Stop reading aloud\tCtrl+Shift+U" : L"Read aloud\tCtrl+Shift+U");
    for (int i = 0; i < 4; ++i) AppendMenuW(rate, MF_STRING | (app.readRate == kRates[i] ? MF_CHECKED : 0), T_RATE + i, kRateNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(rate), L"Reading speed");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    if (cmd >= T_RATE && cmd < T_RATE + 4) { app.readRate = kRates[cmd - T_RATE]; if (app.readVisible) readFrom(readaloud::sentence(g_reading.script, g_reading.at, 0)); return; }
    if (cmd) toolsDispatch(cmd);
}

void updateFindCount() {
    wchar_t t[64] = L"";
    if (GetWindowTextLengthW(app.findEdit) > 0) {
        if (app.matchTotal == 0) wcscpy_s(t, L"No results");
        else {
            swprintf(t, 64, L"%d of %d", app.curMatch + 1, app.matchTotal);
            if (app.viewer.currentMatchInComment()) wcscat_s(t, L"  (in a comment)");
        }
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
    auto on = [](HWND b) { return SendMessageW(b, BM_GETCHECK, 0, 0) == BST_CHECKED; };
    search::Options o;
    o.matchCase = on(app.findCase);
    o.wholeWord = on(app.findWord);
    o.wildcards = on(app.findWild);
    o.comments = on(app.findNotes);
    std::vector<TextHit> hits = search::find(*d, q, o);
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
        case C_PREV: if (d) app.viewer.stepPage(-1); break;
        case C_NEXT: if (d) app.viewer.stepPage(1); break;
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
    pal() = paletteFor(app.theme, systemUsesDark());
    const bool dark = pal().dark;
    themeui::paletteChanged(app.wnd);
    if (app.ctlBrush) DeleteObject(app.ctlBrush);
    app.ctlBrush = CreateSolidBrush(pal().toolbar);
    BOOL on = dark;
    DwmSetWindowAttribute(app.wnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &on, sizeof(on));
    const wchar_t* cls = dark ? L"DarkMode_Explorer" : L"Explorer";
    for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findClose, app.findCase, app.findWord, app.findWild, app.findNotes}) if (c) SetWindowTheme(c, cls, nullptr);
    app.sidebar.applyTheme();
    if (app.startPage) startpage::themeChanged(app.startPage);
    RedrawWindow(app.wnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
}

// ---- About and Settings ----
// ---- updates (shared/update.hpp, updater.cpp) ----

void onUpdateChecked(const update::Release* r, bool manual) {
    if (!r) {
        if (manual) MessageBoxW(app.wnd, L"Feather PDF could not find out whether there is a new version. Check the internet connection and try again.", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    const std::wstring version = platform::wide(r->version), current = platform::wide(FEATHER_VERSION);
    const bool newer = update::compareVersions(r->version, FEATHER_VERSION) > 0 && !r->windows.url.empty();
    if (!newer) {
        if (manual) MessageBoxW(app.wnd, (L"You have the newest version of Feather PDF (" + current + L").").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!manual && version == app.skipVersion) return;
    wchar_t mb[32];
    swprintf(mb, 32, L"%.1f MB", r->windows.size / 1048576.0);
    std::vector<FormField> f(2);
    f[0] = {FormField::Note, L"", {}, L"Feather PDF " + version + L" is out" + (r->date.empty() ? L"" : L" (" + platform::wide(r->date) + L")") +
                                     L". You have " + current + L".\n\n" + (r->notes.empty() ? L"" : L"What is new:\n" + r->notes + L"\n\n") +
                                     L"Update now downloads it (" + mb + L"), closes Feather PDF, installs the new version in the same place and starts it again. "
                                     L"Your settings stay. Windows asks for permission to install.\n\nCancel: update later."};
    f[1] = {FormField::Check, L"Do not remind me about this version"};
    const bool now = showForm(app.wnd, app.inst, L"A new version of Feather PDF", f, L"Update now");
    if (f[1].checked) { app.skipVersion = version; saveSettings(); }
    if (!now) return;

    std::vector<unsigned char> setup;
    std::wstring err;
    {
        Progress pr((L"Downloading Feather PDF " + version + L"...  (Esc stops)").c_str());
        const bool ok = updater::download(r->windows.url, setup, err, [&](long long done, long long total) {
            wchar_t t[160];
            if (total > 0) swprintf(t, 160, L"Downloading Feather PDF %ls...  %.1f of %.1f MB  (Esc stops)", version.c_str(), done / 1048576.0, total / 1048576.0);
            else swprintf(t, 160, L"Downloading Feather PDF %ls...  %.1f MB  (Esc stops)", version.c_str(), done / 1048576.0);
            pr.set(t);
            return !pr.stopRequested();
        });
        if (!ok) { MessageBoxW(app.wnd, (L"The new version could not be downloaded. " + err).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    }
    if (!update::verify(setup, r->windows.sha256)) {
        MessageBoxW(app.wnd, L"The downloaded file is not the right one (it may have been damaged on the way), so it was not installed. Try again later.", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring path = std::wstring(tmp) + L"FeatherPDF-Setup-" + version + L".exe";
    if (Document::writeFile(path, setup) != SaveResult::Ok) { MessageBoxW(app.wnd, L"The new version could not be saved to the temporary folder.", kAppName, MB_OK | MB_ICONWARNING); return; }
    const std::wstring args = L"/S /relaunch /waitfor=" + std::to_wstring(GetCurrentProcessId());
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(app.wnd, L"open", path.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL)) <= 32) {
        MessageBoxW(app.wnd, L"The installer could not be started (was the permission refused?). Nothing was changed.", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    PostMessageW(app.wnd, WM_CLOSE, 0, 0);   // asks about unsaved changes as usual; the installer waits for Feather PDF to close
}

void showAbout() {
    std::vector<FormField> f(2);
    f[0] = {FormField::Note, L"", {}, std::wstring(L"Feather PDF\nVersion " L"" FEATHER_VERSION L" \u201C" L"" FEATHER_CODENAME L"\u201D, built ") + buildDate() +
                                         L"\n\n" + about::body(L"Windows", L"your PC") +
                                         L"\nPDF rendering by PDFium; text recognition by Tesseract (see the licenses folder).\n\n"
                                         L"Built with care for a faster, simpler document experience."};
    f[1] = {FormField::Check, L"Write to the developer now (opens your e-mail program)"};
    if (showForm(app.wnd, app.inst, L"About Feather PDF", f, L"Close") && f[1].checked)
        ShellExecuteW(app.wnd, L"open", (std::wstring(L"mailto:") + about::kFeedbackAddress + L"?subject=Feather%20PDF%20" FEATHER_VERSION).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// how often unsaved changes are copied for recovery after a crash (never: no copy)
void restartAutosave() {
    static const UINT ms[] = {20000, 60000, 300000, 0};
    KillTimer(app.wnd, TIMER_AUTOSAVE);
    if (ms[app.autosaveIdx]) SetTimer(app.wnd, TIMER_AUTOSAVE, ms[app.autosaveIdx], nullptr);
}

// Settings: every preference lives here (and only here), in six groups; a change applies at once and is saved.
enum SettingsPage { SP_APPEARANCE, SP_TABS, SP_VIEWING, SP_EDITING, SP_PERFORMANCE, SP_ADVANCED };
void showSettings(int first) {
    using settingsdlg::Item;
    auto toggle = [](const wchar_t* label, const wchar_t* help, bool& v, std::function<void()> after = nullptr) {
        Item it; it.kind = Item::Toggle; it.label = label; it.help = help;
        bool* p = &v;
        it.get = [p] { return *p ? 1 : 0; };
        it.set = [p, after](int x) { *p = x != 0; if (after) after(); saveSettings(); };
        return it;
    };
    auto choice = [](const wchar_t* label, const wchar_t* help, std::vector<std::wstring> choices, std::function<int()> get, std::function<void(int)> set) {
        Item it; it.kind = Item::Choice; it.label = label; it.help = help; it.choices = std::move(choices);
        it.get = std::move(get);
        it.set = [set](int x) { set(x); saveSettings(); };
        return it;
    };
    auto button = [](const wchar_t* label, const wchar_t* help, const wchar_t* text, std::function<void()> press) {
        Item it; it.kind = Item::Button; it.label = label; it.help = help; it.choices = {text}; it.press = std::move(press);
        return it;
    };
    auto repaint = [] { InvalidateRect(app.wnd, nullptr, FALSE); refresh(); };
    static const int themeOrder[] = {ThemeLight, ThemeModernLight, ThemeDark, ThemeAmoled, ThemeMidnight, ThemeGraphite, ThemeSystem};
    static const wchar_t* colourNames[] = {L"Yellow", L"Green", L"Blue", L"Pink", L"Orange", L"Red", L"Purple"};
    const std::vector<std::wstring> colours(std::begin(colourNames), std::end(colourNames));
    std::vector<settingsdlg::Category> cats(6);
    cats[SP_APPEARANCE] = {L"Appearance", {
        choice(L"Theme", L"Colours of the window, menus, dialogs and scroll bars. Like Windows follows Windows' light or dark mode.",
               {L"Light", L"Modern Light", L"Dark", L"AMOLED Dark (true black)", L"Midnight Blue", L"Graphite", L"Like Windows"},
               [] { for (int i = 0; i < 7; ++i) if (themeOrder[i] == app.theme) return i; return 0; },
               [](int i) { app.theme = themeOrder[std::clamp(i, 0, 6)]; applyTheme(); }),
        toggle(L"Colourful toolbar icons", L"Each toolbar icon in its own colour. Off: icons in the text colour.", app.colourIcons, repaint),
        toggle(L"Random tab colours", L"Each new tab gets a colour of its own and keeps it until it is closed. Off: tabs follow the theme.", app.tabColours, repaint)}};
    cats[SP_TABS] = {L"Tabs", {
        toggle(L"Show tab numbers", L"Number the tabs: \"1. Report.pdf\". The numbers follow when tabs are opened, closed or dragged to another place.", app.tabNumbers, repaint),
        choice(L"Opening a PDF that is already open", L"The same file opened again (also through another path or a shortcut).",
               {L"Ask first", L"Go to its tab"}, [] { return app.dupMode; }, [](int i) { app.dupMode = i; }),
        choice(L"Opening files", L"Each file in a window of its own (to see two PDFs at once, or one on each screen), or as a tab of the window already open. For File Explorer, Open and Open recent alike.",
               {L"As tabs of one window", L"Each in its own window"}, [] { return app.ownWindows ? 1 : 0; }, [](int i) { app.ownWindows = i == 1; })}};
    cats[SP_VIEWING] = {L"PDF Viewing", {
        choice(L"Page colours", L"Dark turns pages dark for reading at night; Dimmed softens bright white.", {L"Normal", L"Dark (night mode)", L"Dimmed (softer white)"},
               [] { return static_cast<int>(app.viewer.colours()); },
               [](int i) { app.viewer.setColours(static_cast<Viewer::Colours>(i)); app.viewer2.setColours(static_cast<Viewer::Colours>(i)); }),
        toggle(L"Open documents where they were last read", L"The page and zoom of the last 100 documents are remembered.", app.restorePositions),
        choice(L"Reading speed", L"How fast Read aloud speaks.", {L"Slow", L"Normal", L"Fast", L"Very fast"},
               [] { return app.readRate <= -4 ? 0 : app.readRate == 0 ? 1 : app.readRate <= 3 ? 2 : 3; },
               [](int i) { static const int rates[] = {-4, 0, 3, 6}; app.readRate = rates[std::clamp(i, 0, 3)]; })}};
    cats[SP_EDITING] = {L"Editing", {
        choice(L"Highlight colour", L"For highlights and for comments on selected text.", colours, [] { return app.hlColour; }, [](int i) { app.hlColour = i; }),
        choice(L"Drawing colour", L"For shapes, lines, arrows, the pen and new text boxes.", colours, [] { return app.drawColour; }, [](int i) { app.drawColour = i; }),
        choice(L"Line width", L"For shapes, lines, arrows and the pen.", {L"Thin", L"Medium", L"Thick"}, [] { return app.widthIdx; }, [](int i) { app.widthIdx = i; })}};
    cats[SP_PERFORMANCE] = {L"Performance", {
        choice(L"Pages kept ready in memory", L"More pages make paging back and forth faster and use more memory.", {L"Few (3)", L"Normal (6)", L"Many (12)"},
               [] { return app.cacheIdx; },
               [](int i) { app.cacheIdx = i; const int n = i == 0 ? 3 : i == 1 ? 6 : 12; app.viewer.setCacheLimit(n); app.viewer2.setCacheLimit(n); }),
        choice(L"Recovery copy of unsaved changes", L"Unsaved changes are copied so they come back after a crash. Never: no copy is kept.",
               {L"Every 20 seconds", L"Every minute", L"Every 5 minutes", L"Never"}, [] { return app.autosaveIdx; },
               [](int i) { app.autosaveIdx = i; restartAutosave(); })}};
    cats[SP_ADVANCED] = {L"Advanced", {
        toggle(L"Look for new versions", L"When Feather PDF starts, it checks whether a new version is out and asks whether to update now or later.", app.checkUpdates),
        button(L"Update", L"Checks now whether a new version of Feather PDF is out.", L"Update to new version\x2026", [] {
            updater::checkInBackground(app.wnd, kMsgUpdateChecked, true); }),
        button(L"Default PDF application", L"Opens the Windows settings where you choose the app that opens PDF files.", L"Set as default\x2026", [] { registerAsViewer(); }),
        button(L"Recent files", L"The list on the start page and in Open recent.", L"Clear the list", [] {
            if (MessageBoxW(GetActiveWindow(), L"Clear the list of recent files?", L"Settings", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
            app.recent.clear(); updateEmptyText(); saveSettings(); }),
        button(L"Settings folder", L"Where Feather PDF keeps its settings and recovery copies.", L"Open the folder", [] {
            std::wstring dir = settingsPath();
            dir = dir.substr(0, dir.find_last_of(L'\\'));
            ShellExecuteW(app.wnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }),
        button(L"Reset settings", L"Puts every setting on this page and the others back as it was when Feather PDF was installed.", L"Reset all\x2026", [] {
            if (MessageBoxW(GetActiveWindow(), L"Put every setting back to its default? Recent files and reading positions are kept.", L"Settings", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
            app.theme = ThemeSystem; app.colourIcons = true; app.tabColours = false; app.tabNumbers = false; app.dupMode = 0; app.ownWindows = false; app.restorePositions = true;
            app.viewer.setColours(Viewer::Colours::Normal); app.viewer2.setColours(Viewer::Colours::Normal);
            app.readRate = 0; app.hlColour = 0; app.drawColour = 5; app.widthIdx = 1; app.cacheIdx = 1; app.autosaveIdx = 0;
            app.viewer.setCacheLimit(6); app.viewer2.setCacheLimit(6);
            restartAutosave(); applyTheme(); saveSettings(); refresh(); })}};
    settingsdlg::run(app.wnd, app.inst, cats, first, app.dpi, app.font, app.fontBold);
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
    if (!d || !finishInput()) return;
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
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, app.wnd, nullptr);
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
    if (d && finishInput() && d->movePages(pages, before)) afterPagesChanged();
}

// ---- saved workspaces: a named group of open documents (tab order, page, zoom), reopened together ----
std::wstring workspaceDir() { std::wstring s = settingsPath(); return s.substr(0, s.find_last_of(L'\\')) + L"\\workspaces"; }

std::vector<std::wstring> workspaceNames() {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((workspaceDir() + L"\\*.ini").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do { std::wstring n = fd.cFileName; out.push_back(n.substr(0, n.size() - 4)); } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    return out;
}

void saveWorkspace() {
    if (app.tabs.empty() || !finishInput()) return;
    int unsaved = 0;
    for (const auto& t : app.tabs) if (t->path().empty()) ++unsaved;
    std::wstring name;
    if (!askText(L"Save as a workspace", L"Name of the workspace (for example: Semester 3 Papers):", false, name) || name.empty()) return;
    for (auto& c : name) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_';
    const std::wstring dir = workspaceDir(), file = dir + L"\\" + name + L".ini";
    CreateDirectoryW(dir.c_str(), nullptr);
    if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES &&
        MessageBoxW(app.wnd, (L"A workspace named \"" + name + L"\" exists. Replace it?").c_str(), kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    DeleteFileW(file.c_str());
    int n = 0;
    for (size_t i = 0; i < app.tabs.size(); ++i) {
        const Document& d = *app.tabs[i];
        if (d.path().empty()) continue;   // a new document that was never saved has no file to reopen
        wchar_t sec[32], v[64];
        swprintf(sec, 32, L"tab%d", n);
        WritePrivateProfileStringW(sec, L"path", d.path().c_str(), file.c_str());
        swprintf(v, 64, L"%.4f", d.zoom); WritePrivateProfileStringW(sec, L"zoom", v, file.c_str());
        WritePrivateProfileStringW(sec, L"scrollY", std::to_wstring(d.scrollY).c_str(), file.c_str());
        WritePrivateProfileStringW(sec, L"fit", d.fitWidth ? L"1" : L"0", file.c_str());
        if (static_cast<int>(i) == app.active) WritePrivateProfileStringW(L"meta", L"active", std::to_wstring(n).c_str(), file.c_str());
        ++n;
    }
    WritePrivateProfileStringW(L"meta", L"count", std::to_wstring(n).c_str(), file.c_str());
    std::wstring msg = L"The workspace \"" + name + L"\" was saved with " + std::to_wstring(n) + L" document(s). Open it from the \x2026 menu > Workspaces.";
    if (unsaved) msg += L"\n\n" + std::to_wstring(unsaved) + L" new document(s) that were never saved are not in it: save them first to include them.";
    MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
}

extern bool g_restoring;
void openWorkspace(const std::wstring& name) {
    const std::wstring file = workspaceDir() + L"\\" + name + L".ini";
    const int count = static_cast<int>(GetPrivateProfileIntW(L"meta", L"count", 0, file.c_str()));
    const int active = static_cast<int>(GetPrivateProfileIntW(L"meta", L"active", 0, file.c_str()));
    std::vector<std::wstring> missing;
    int activeTab = -1;
    g_restoring = true;   // all as tabs of this window, whatever "Opening files" says
    for (int i = 0; i < count; ++i) {
        wchar_t sec[32], path[MAX_PATH * 2], z[64];
        swprintf(sec, 32, L"tab%d", i);
        GetPrivateProfileStringW(sec, L"path", L"", path, MAX_PATH * 2, file.c_str());
        if (!path[0]) continue;
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) { missing.push_back(path); continue; }
        openFile(path);
        Document* d = nullptr;
        int at = -1;
        for (size_t k = 0; k < app.tabs.size(); ++k) if (_wcsicmp(app.tabs[k]->path().c_str(), path) == 0) { d = app.tabs[k].get(); at = static_cast<int>(k); }
        if (!d) continue;
        GetPrivateProfileStringW(sec, L"zoom", L"1", z, 64, file.c_str());
        d->zoom = wcstod(z, nullptr);
        d->scrollY = static_cast<int>(GetPrivateProfileIntW(sec, L"scrollY", 0, file.c_str()));
        d->fitWidth = GetPrivateProfileIntW(sec, L"fit", 1, file.c_str()) != 0;
        if (i == active) activeTab = at;
    }
    g_restoring = false;
    if (activeTab < 0 && !app.tabs.empty()) activeTab = app.active;
    if (activeTab >= 0) { setActiveDocument(nullptr); app.active = -1; switchTab(activeTab); }   // shown at its saved place
    writeSession();
    if (!missing.empty()) {
        std::wstring msg = L"These files of the workspace were not found (moved, renamed or deleted):";
        for (const auto& m : missing) msg += L"\n  " + m;
        MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_OK | MB_ICONWARNING);
    }
}

void deleteWorkspace() {
    const auto names = workspaceNames();
    if (names.empty()) return;
    std::vector<FormField> f(1);
    f[0] = {FormField::Choice, L"Workspace:", names, L"", 0};
    if (!showForm(app.wnd, app.inst, L"Delete a workspace", f, L"Delete")) return;
    const std::wstring& n = names[static_cast<size_t>(std::clamp(f[0].choice, 0, static_cast<int>(names.size()) - 1))];
    DeleteFileW((workspaceDir() + L"\\" + n + L".ini").c_str());   // only the list: the documents themselves stay
}

// ---- the main ("...") menu ----
void showMainMenu(const RECT& anchor) {
    Document* d = activeDoc();
    enum { MM_OPEN = 1, MM_SAVE, MM_SAVEAS, MM_EXPORT, MM_CLOSE, MM_UNDO, MM_REDO, MM_PRINT, MM_PROPS, MM_READ, MM_COPYPAGE, MM_SELALL,
           MM_FIRST, MM_LAST, MM_GOTO, MM_BM, MM_THUMBS, MM_FITW, MM_FITP, MM_ACTUAL, MM_FULL, MM_PRESENT, MM_COL, MM_THEME = MM_COL + 3,
           MM_SETTINGS = MM_THEME + 3, MM_DEFAULT, MM_ABOUT, MM_EXIT, MM_NEWPDF, MM_RELOAD, MM_SIGN, MM_COPYAREA, MM_BACK, MM_FWD, MM_ROTL, MM_ROTR, MM_PALETTE, MM_COMPARE, MM_SEARCHDIR, MM_BATCH, MM_NEWTEXT, MM_SBSTAB, MM_SBSOPEN, MM_SBSOFF, MM_LAYOUT, MM_PREVIEW = MM_LAYOUT + 4 /* four layout items */, MM_GUIDE, MM_UPDATE, MM_NEWWINDOW, MM_WS_SAVE, MM_WS_DELETE, MM_ATTACH, MM_LAYERS, MM_REPAIR, MM_WEBVIEW, MM_HISTORY, MM_RECENT = 100, MM_WORKSPACE = 400 };
    static_assert(MM_GUIDE < MM_RECENT, "main menu ids run into the recent files");
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* t, bool on = true, bool check = false) {
        AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED) | (check ? MF_CHECKED : 0), id, t); };
    auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    const bool has = d != nullptr;
    // Only what the toolbar, the zoom box, the tool menus and the right-click menu do not already offer.
    add(MM_OPEN, L"Open...\tCtrl+O");
    HMENU rec = CreatePopupMenu();
    for (size_t i = 0; i < app.recent.size(); ++i) AppendMenuW(rec, MF_STRING, MM_RECENT + i, recentLabel(i, false).c_str());
    if (app.recent.empty()) AppendMenuW(rec, MF_STRING | MF_GRAYED, 0, L"(none)");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(rec), L"Open recent");
    {
        HMENU np = CreatePopupMenu();
        AppendMenuW(np, MF_STRING, MM_NEWPDF, L"From pictures...");
        AppendMenuW(np, MF_STRING, MM_NEWTEXT, L"From a text or Markdown file...");
        AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(np), L"New PDF");
    }
    add(MM_NEWWINDOW, L"New window\tCtrl+N");
    const std::vector<std::wstring> spaces = workspaceNames();
    {
        HMENU ws = CreatePopupMenu();
        AppendMenuW(ws, MF_STRING | (has ? 0 : MF_GRAYED), MM_WS_SAVE, L"Save open tabs as a workspace...");
        AppendMenuW(ws, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < spaces.size() && i < 50; ++i) AppendMenuW(ws, MF_STRING, MM_WORKSPACE + i, spaces[i].c_str());
        if (spaces.empty()) AppendMenuW(ws, MF_STRING | MF_GRAYED, 0, L"(no workspaces yet)");
        AppendMenuW(ws, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(ws, MF_STRING | (spaces.empty() ? MF_GRAYED : 0), MM_WS_DELETE, L"Delete a workspace...");
        AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(ws), L"Workspaces");
    }
    add(MM_SAVEAS, L"Save as...\tCtrl+Shift+S", has);
    add(MM_EXPORT, L"Export as pictures, text, Word or Excel...", has);
    add(MM_WEBVIEW, L"Save a copy for the web (Fast Web View)...", has);
    add(MM_REPAIR, L"Repair a damaged PDF...");
    add(MM_CLOSE, L"Close tab\tCtrl+W", has);
    sep();
    add(MM_PRINT, L"Print...\tCtrl+P", has);
    add(MM_PROPS, L"Document properties...\tCtrl+D", has);
    add(MM_ATTACH, L"Attached files...", has);
    add(MM_LAYERS, L"Layers (show or hide)...", has);
    add(MM_RELOAD, L"Reload from disk", has && !d->path().empty());
    sep();
    add(MM_BM, L"Bookmarks panel\tCtrl+B", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::Bookmarks);
    add(MM_THUMBS, L"Thumbnails panel\tCtrl+Shift+B", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::Pages);
    add(MM_HISTORY, L"Undo history panel", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::History);
    {
        HMENU lay = CreatePopupMenu();
        AppendMenuW(lay, MF_STRING | (app.layout == 0 ? MF_CHECKED : 0), MM_LAYOUT + 0, L"Single page");
        AppendMenuW(lay, MF_STRING | (app.layout == 1 ? MF_CHECKED : 0), MM_LAYOUT + 1, L"Continuous");
        AppendMenuW(lay, MF_STRING | (app.layout == 2 ? MF_CHECKED : 0), MM_LAYOUT + 2, L"Two pages");
        AppendMenuW(lay, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(lay, MF_STRING | (app.layout == 2 ? 0 : MF_GRAYED) | (app.cover ? MF_CHECKED : 0), MM_LAYOUT + 3, L"Show cover page separately");
        AppendMenuW(m, MF_POPUP | (has ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(lay), L"Page layout");
    }
    {
        HMENU go = CreatePopupMenu();
        AppendMenuW(go, MF_STRING | (has ? 0 : MF_GRAYED), MM_FIRST, L"First page\tHome");
        AppendMenuW(go, MF_STRING | (has ? 0 : MF_GRAYED), MM_LAST, L"Last page\tEnd");
        AppendMenuW(go, MF_STRING | (has && !app.navBack.empty() ? 0 : MF_GRAYED), MM_BACK, L"Back\tAlt+Left");
        AppendMenuW(go, MF_STRING | (has && !app.navFwd.empty() ? 0 : MF_GRAYED), MM_FWD, L"Forward\tAlt+Right");
        AppendMenuW(m, MF_POPUP | (has ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(go), L"Go to");
    }
    add(MM_FULL, L"Full screen\tF11", has);
    add(MM_PRESENT, L"Presentation\tF5", has);
    sep();
    add(MM_SEARCHDIR, L"Search all my PDFs in a folder...");
    add(MM_BATCH, L"Batch: process a whole folder...");
    add(MM_PALETTE, L"Command palette...\tCtrl+K");
    sep();
    add(MM_SETTINGS, L"Settings...\tCtrl+,");
    add(MM_GUIDE, L"User guide\tF1");
    add(MM_ABOUT, L"About Feather PDF");
    add(MM_UPDATE, L"Update to new version...");
    add(MM_EXIT, L"Exit\tCtrl+Q");
    POINT pt{anchor.right, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = trackMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
        case MM_OPEN: runCommand(C_OPEN); break;
        case MM_SAVEAS: if (d) saveAsDialog(*d); break;
        case MM_EXPORT: if (d) doExport(*d); break;
        case MM_WEBVIEW: if (d) saveForWeb(*d); break;
        case MM_REPAIR: repairPdf(); break;
        case MM_CLOSE: closeTab(app.active); break;
        case MM_PRINT: printDocument(); break;
        case MM_GUIDE: openUserGuide(); break;
        case MM_PROPS: showProperties(); break;
        case MM_ATTACH: attachmentsDialog(); break;
        case MM_LAYERS: layersDialog(); break;
        case MM_FIRST: app.viewer.goToPage(0); break;
        case MM_LAST: if (d) app.viewer.goToPage(d->pageCount() - 1); break;
        case MM_BM: showSidebar(Sidebar::Mode::Bookmarks); break;
        case MM_THUMBS: showSidebar(Sidebar::Mode::Pages); break;
        case MM_HISTORY: showSidebar(Sidebar::Mode::History); break;
        case MM_FULL: setFullscreen(true, false); break;
        case MM_PRESENT: setFullscreen(true, true); break;
        case MM_NEWPDF: newPdfFromPictures(); break;
        case MM_NEWWINDOW: launchWindow({}); break;
        case MM_WS_SAVE: saveWorkspace(); break;
        case MM_WS_DELETE: deleteWorkspace(); break;
        case MM_NEWTEXT: {
            wchar_t buf[MAX_PATH * 2] = L"";
            OPENFILENAMEW ofn{sizeof(ofn)};
            ofn.hwndOwner = app.wnd;
            ofn.lpstrTitle = L"Choose a text or Markdown file";
            ofn.lpstrFilter = L"Text and Markdown (*.txt;*.md)\0*.txt;*.md;*.markdown\0";
            ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH * 2;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) openTextAsPdf(buf);
            break;
        }
        case MM_SEARCHDIR: searchFolder(); break;
        case MM_BATCH: batchDialog(); break;
        case MM_RELOAD: reloadFromDisk(); break;
        case MM_BACK: navBack(); break;
        case MM_FWD: navForward(); break;
        case MM_PALETTE: commandPalette(); break;
        case MM_SETTINGS: showSettings(SP_APPEARANCE); break;
        case MM_ABOUT: showAbout(); break;
        case MM_UPDATE: updater::checkInBackground(app.wnd, kMsgUpdateChecked, true); break;
        case MM_EXIT: PostMessageW(app.wnd, WM_CLOSE, 0, 0); break;
        default:
            if (cmd >= MM_LAYOUT && cmd < MM_LAYOUT + 4) setPageLayout(cmd - MM_LAYOUT);
            else if (cmd >= MM_RECENT && cmd < MM_RECENT + 100) openRecent(static_cast<size_t>(cmd - MM_RECENT));
            else if (cmd >= MM_WORKSPACE && cmd < MM_WORKSPACE + 50) { if (static_cast<size_t>(cmd - MM_WORKSPACE) < spaces.size()) openWorkspace(spaces[static_cast<size_t>(cmd - MM_WORKSPACE)]); }
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
// Each window keeps its own: session-<process number>. A window that crashed leaves its folder behind for the next start.
std::wstring sessionBase() {
    std::wstring s = settingsPath();
    size_t p = s.find_last_of(L'\\');
    return p == std::wstring::npos ? L"" : s.substr(0, p);
}
std::wstring sessionDir() { return sessionBase() + L"\\session-" + std::to_wstring(GetCurrentProcessId()); }

void clearSessionDir(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileW((dir + L"\\" + fd.cFileName).c_str()); } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

// Is process `pid` a Feather PDF window still running? (Its session folder is then not left over from a crash.)
bool featherRunning(DWORD pid) {
    if (pid == GetCurrentProcessId()) return true;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    wchar_t name[MAX_PATH * 2];
    DWORD len = MAX_PATH * 2;
    bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE && QueryFullProcessImageNameW(h, 0, name, &len);
    if (alive) { std::wstring n(name); for (auto& c : n) c = static_cast<wchar_t>(towlower(c)); alive = n.find(L"featherpdf") != std::wstring::npos; }
    CloseHandle(h);
    return alive;
}

// The session folders left by windows that are no longer running (and the one older versions used).
std::vector<std::wstring> leftoverSessions() {
    std::vector<std::wstring> out;
    const std::wstring base = sessionBase();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((base + L"\\session*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const std::wstring name = fd.cFileName;
        if (name == L"session") out.push_back(base + L"\\" + name);
        else if (name.rfind(L"session-", 0) == 0 && !featherRunning(static_cast<DWORD>(wcstoul(name.c_str() + 8, nullptr, 10)))) out.push_back(base + L"\\" + name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

void clearSession() {
    if (app.testMode) return;
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
    if (app.testMode) return;
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
    // every window that did not close normally (each kept its own folder): their tabs come back in this window
    const std::vector<std::wstring> dirs = leftoverSessions();
    auto clearLeft = [&] { for (const auto& dir : dirs) clearSessionDir(dir); };
    struct Entry { std::wstring path, name, rec; double zoom; int scrollY; bool fit; };
    std::vector<Entry> list;
    int withChanges = 0;
    for (const std::wstring& dir : dirs) {
    const std::wstring ini = dir + L"\\session.ini";
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
    const int count = static_cast<int>(GetPrivateProfileIntW(L"meta", L"count", 0, ini.c_str()));
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
    }
    if (list.empty()) { clearLeft(); return false; }
    std::wstring msg = L"Feather PDF did not close normally last time.\n\nReopen " + std::to_wstring(list.size()) + L" document(s)";
    if (withChanges) msg += L", including " + std::to_wstring(withChanges) + L" with changes that were not saved";
    msg += L"?";
    if (MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_YESNO | MB_ICONQUESTION) != IDYES) { clearLeft(); return false; }
    g_restoring = true;
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
    g_restoring = false;
    clearLeft();
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
    if (app.inlineEd.active()) app.inlineEd.cancel();   // its text goes with the other unsaved changes
    const double zoom = d->zoom; const int sy = d->scrollY; const bool fw = d->fitWidth;
    std::string pw;
    OpenResult r = d->open(d->path());
    while (r == OpenResult::PasswordNeeded || r == OpenResult::WrongPassword) {
        if (!askPassword(Document::describe(r, d->path()), pw)) return;
        r = d->open(d->path(), pw);
    }
    if (r != OpenResult::Ok) { MessageBoxW(app.wnd, Document::describe(r, d->path()).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    d->zoom = zoom; d->scrollY = sy; d->fitWidth = fw;
    // the file may hold other pages now: selected objects and redaction marks belonged to the old ones
    app.sel.page = -1; app.sel.objects.clear(); app.viewer.setObjectSelection(-1, nullptr);
    setActiveDocument(d);
    afterDocReplaced();
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
    
}

void tableEdit() {
    if (!activeDoc()) return;
    if (app.tool != Viewer::Tool::TableEdit) setTool(Viewer::Tool::TableEdit);
    
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
    if (cmd == N_SIG_SAVED) ok = GetFileAttributesW(png.c_str()) != INVALID_FILE_ATTRIBUTES;   // the one made last time
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
        if (ok) { app.pendingPicture = buf.data(); setTool(Viewer::Tool::Picture); notice(L"Click on the page where the top-left corner of the signature should go. Esc cancels."); return; }
    }
    if (!ok) { MessageBoxW(app.wnd, L"The signature could not be made.", kAppName, MB_OK | MB_ICONWARNING); return; }
    app.pendingPicture = png;
    app.pendingPictureWidth = 150;
    if (app.tool != Viewer::Tool::Picture) setTool(Viewer::Tool::Picture);
    notice(L"Click where the top-left corner of the signature should go (a picture of your handwriting; for proof of who signed, use Digital signatures).");
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
    // protractor: the corner and a point on each side, clicked one after the other
    if (e.pts.size() < 3) return;
    const double ax = e.pts[1].x - e.pts[0].x, ay = e.pts[1].y - e.pts[0].y;
    const double bx = e.pts[2].x - e.pts[0].x, by = e.pts[2].y - e.pts[0].y;
    const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
    if (la < 0.5 || lb < 0.5) return;
    const double c = std::clamp((ax * bx + ay * by) / (la * lb), -1.0, 1.0);
    swprintf(t, 64, L"%.1f\u00B0", std::acos(c) * 180.0 / 3.14159265358979);
    app.viewer.addMeasure({e.page, e.pts[0], e.pts[1], e.pts[2], true, t});
}


// ---- picture tools (on the object picked with the Move tool) ----
void addPictureItems(HMENU m) {
    Document* d = activeDoc();
    const bool any = d && app.tool == Viewer::Tool::Select && app.sel.page >= 0 && !app.sel.objects.empty();
    const bool pic = any && pageobj::isPicture(*d, app.sel.page, app.sel.objects);
    auto add = [&](UINT id, const wchar_t* t, bool on) { AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED), id, t); };
    add(PX_ROT_R, L"Turn right (clockwise)", any);
    add(PX_ROT_L, L"Turn left", any);
    add(PX_FLIP_H, L"Flip left to right", any);
    add(PX_FLIP_V, L"Flip upside down", any);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(PX_CROP, L"Crop picture (drag a box)", pic);
    add(PX_ADJUST, L"Brightness and contrast...", pic);
    add(PX_GREY, L"Make the picture black and white", pic);
    add(PX_REPLACE, L"Replace picture...", pic);
    add(PX_SAVE, L"Save picture as PNG...", pic);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(PX_FRONT, L"Bring to front", any);
    add(PX_BACK, L"Send to back", any);
    add(PX_PASTE, L"Paste picture\tCtrl+V", d && pageobj::clipboardHasPicture());
}

void pictureAction(int cmd) {
    Document* d = activeDoc();
    if (!d) return;
    const auto& api = pdfium::api();
    auto showSelection = [&] {
        FPDF_PAGE pg = d->page(app.sel.page);
        double l = 1e18, b = 1e18, r = -1e18, t = -1e18;
        for (int i : app.sel.objects) {
            float ol, ob, orr, ot;
            FPDF_PAGEOBJECT o = pg ? api.FPDFPage_GetObject(pg, i) : nullptr;
            if (o && api.FPDFPageObj_GetBounds(o, &ol, &ob, &orr, &ot)) { l = std::min<double>(l, ol); b = std::min<double>(b, ob); r = std::max<double>(r, orr); t = std::max<double>(t, ot); }
        }
        app.viewer.documentChanged();
        app.sidebar.refresh();
        refresh();
        if (l > r) { app.viewer.setObjectSelection(-1, nullptr); return; }
        std::vector<double> gx, gy;
        pageobj::snapGuides(*d, app.sel.page, app.sel.objects, gx, gy);
        annot::RectD rc{l, b, r, t};
        app.viewer.setObjectSelection(app.sel.page, &rc, gx, gy);
    };
    if (cmd == 0) { showSelection(); return; }
    if (cmd == PX_PASTE) {
        const int page = std::max(0, app.viewer.currentPage());
        const auto sz = d->pageSize(page);
        std::wstring err;
        if (!pageobj::pastePicture(*d, page, sz.w * 0.2, sz.h * 0.8, err)) { MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
        if (app.tool != Viewer::Tool::Select) setTool(Viewer::Tool::Select);
        app.sel.page = page;
        app.sel.objects = {api.FPDFPage_CountObjects(d->page(page)) - 1};
        showSelection();
        return;
    }
    if (app.tool != Viewer::Tool::Select || app.sel.page < 0 || app.sel.objects.empty()) {
        if (app.tool != Viewer::Tool::Select) setTool(Viewer::Tool::Select);
        MessageBoxW(app.wnd, L"First click the picture (or paragraph or table) to pick it. The Move tool is now on.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    const int page = app.sel.page;
    const bool pic = pageobj::isPicture(*d, page, app.sel.objects);
    if (!pic && (cmd == PX_CROP || cmd == PX_ADJUST || cmd == PX_GREY || cmd == PX_REPLACE || cmd == PX_SAVE)) {
        MessageBoxW(app.wnd, L"This works on pictures only. Click a picture to pick it first.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    const int obj = app.sel.objects.front();
    bool ok = true;
    switch (cmd) {
        case PX_ROT_R: ok = pageobj::rotateGroup(*d, page, app.sel.objects, 1); break;
        case PX_ROT_L: ok = pageobj::rotateGroup(*d, page, app.sel.objects, 3); break;
        case PX_FLIP_H: ok = pageobj::flipGroup(*d, page, app.sel.objects, true); break;
        case PX_FLIP_V: ok = pageobj::flipGroup(*d, page, app.sel.objects, false); break;
        case PX_FRONT: ok = pageobj::restackGroup(*d, page, app.sel.objects, true); break;
        case PX_BACK: ok = pageobj::restackGroup(*d, page, app.sel.objects, false); break;
        case PX_GREY: ok = pageobj::adjustPicture(*d, page, obj, 0, 0, true); break;
        case PX_CROP:
            app.cropPic.page = page; app.cropPic.obj = obj;
            setTool(Viewer::Tool::PicCrop);
            notice(L"Drag a box around the part of the picture to keep. Esc cancels.");
            return;
        case PX_ADJUST: {
            std::vector<FormField> f(3);
            f[0] = {FormField::Text, L"Brightness (-100 to 100):", {}, L"20"};
            f[1] = {FormField::Text, L"Contrast (-100 to 100):", {}, L"0"};
            f[2] = {FormField::Check, L"Black and white", {}, L"", 0, false};
            if (!showForm(app.wnd, app.inst, L"Brightness and contrast", f, L"Apply", [](const std::vector<FormField>& v) -> std::wstring {
                    for (int i = 0; i < 2; ++i) { const int n = _wtoi(v[i].text.c_str()); if (n < -100 || n > 100) return L"Use numbers from -100 to 100."; }
                    return L"";
                })) return;
            ok = pageobj::adjustPicture(*d, page, obj, _wtoi(f[0].text.c_str()), _wtoi(f[1].text.c_str()), f[2].checked);
            if (!ok && !_wtoi(f[0].text.c_str()) && !_wtoi(f[1].text.c_str()) && !f[2].checked) return;   // nothing to change
            break;
        }
        case PX_REPLACE: {
            std::vector<wchar_t> buf(MAX_PATH * 2, 0);
            OPENFILENAMEW ofn{sizeof(ofn)};
            ofn.hwndOwner = app.wnd;
            ofn.lpstrTitle = L"Choose the new picture";
            ofn.lpstrFilter = L"Pictures (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0All files (*.*)\0*.*\0";
            ofn.lpstrFile = buf.data();
            ofn.nMaxFile = static_cast<DWORD>(buf.size());
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetOpenFileNameW(&ofn)) return;
            std::wstring err;
            if (!pageobj::replacePicture(*d, page, obj, buf.data(), err)) { MessageBoxW(app.wnd, err.c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
            break;
        }
        case PX_SAVE: {
            std::vector<wchar_t> buf(MAX_PATH * 2, 0);
            std::wstring base = d->title();
            if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
            wcsncpy_s(buf.data(), buf.size(), (base + L" picture.png").c_str(), _TRUNCATE);
            OPENFILENAMEW ofn{sizeof(ofn)};
            ofn.hwndOwner = app.wnd;
            ofn.lpstrTitle = L"Save the picture as";
            ofn.lpstrFilter = L"PNG pictures (*.png)\0*.png\0";
            ofn.lpstrDefExt = L"png";
            ofn.lpstrFile = buf.data();
            ofn.nMaxFile = static_cast<DWORD>(buf.size());
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (!GetSaveFileNameW(&ofn)) return;
            if (!pageobj::savePicture(*d, page, obj, buf.data())) MessageBoxW(app.wnd, L"The picture could not be saved.", kAppName, MB_OK | MB_ICONWARNING);
            return;
        }
        default: return;
    }
    if (!ok) { MessageBoxW(app.wnd, L"That did not work on this object.", kAppName, MB_OK | MB_ICONWARNING); return; }
    showSelection();
}

// ---- events of the new tools ----
// ---- tables on any page ----
struct { const Document* doc = nullptr; int revision = -1; std::map<int, std::vector<annot::RectD>> pages; } g_tableCache;

// The tables found on a page, remembered until the document changes (outlined while a table tool is chosen).
std::vector<annot::RectD> tableOutlines(int page) {
    Document* d = activeDoc();
    if (!d) return {};
    if (g_tableCache.doc != d || g_tableCache.revision != d->revision()) { g_tableCache.doc = d; g_tableCache.revision = d->revision(); g_tableCache.pages.clear(); }
    auto it = g_tableCache.pages.find(page);
    if (it != g_tableCache.pages.end()) return it->second;
    std::vector<annot::RectD> rects;
    for (const tables::Table& t : tables::detect(*d, page)) rects.push_back({t.left(), t.bottom(), t.right(), t.top()});
    g_tableCache.pages[page] = rects;
    return rects;
}

void afterTableChange(Document& d, int page) {
    d.resetText(page);
    app.viewer.documentChanged();
    app.sidebar.refresh();
    refresh();
}

void editTable(Document& d, tables::Table t) {
    const PageSize& ps = d.pageSize(t.page);
    const tableeditor::Result r = tableeditor::run(app.wnd, app.inst, t, ps.w, ps.h, app.dpi);
    if (r == tableeditor::Result::Delete) { if (tables::remove(d, t)) afterTableChange(d, t.page); return; }
    if (r != tableeditor::Result::Apply) return;
    std::wstring why;
    double grew = 0;
    bool shrunk = false;
    if (!tables::write(d, t, &why, &grew, &shrunk)) { MessageBoxW(app.wnd, (L"The table could not be changed. " + why).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    afterTableChange(d, t.page);
    if (shrunk) notice(L"There was no room on the page for the table to grow, so the text you changed was set a little smaller to fit. Undo (Ctrl+Z) takes it back.");
    else if (grew > 0.5) notice(L"The table got longer; what was below it moved down with it.");
}

bool featureDraw(Document& d, const Viewer::Event& e) {
    switch (e.tool) {
        case Viewer::Tool::TableEdit: case Viewer::Tool::TableDelete: {   // a box drawn around a table
            if (e.pts.size() < 2) return true;
            tables::Table t;
            std::wstring why;
            if (!tables::tableIn(d, e.page, e.pts[0].x, e.pts[0].y, e.pts[1].x, e.pts[1].y, t, &why)) { notice(why); return true; }
            if (e.tool == Viewer::Tool::TableEdit) editTable(d, t);
            else if (MessageBoxW(app.wnd, L"Delete this table from the page?", kAppName, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES && tables::remove(d, t)) afterTableChange(d, t.page);
            return true;
        }
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
        case Viewer::Tool::PicCrop: {
            const int page = app.cropPic.page, obj = app.cropPic.obj;
            if (page != e.page || e.pts.size() < 2) { MessageBoxW(app.wnd, L"Drag the box over the picture you chose.", kAppName, MB_OK | MB_ICONINFORMATION); return true; }
            if (!pageobj::cropPicture(d, page, obj, e.pts[0].x, e.pts[0].y, e.pts[1].x, e.pts[1].y)) { MessageBoxW(app.wnd, L"The box does not cover any of the picture.", kAppName, MB_OK | MB_ICONINFORMATION); return true; }
            setTool(Viewer::Tool::Select);
            app.sel.page = page; app.sel.objects = {obj};
            pictureAction(0);   // shows the new selection
            return true;
        }
        case Viewer::Tool::Ruler: case Viewer::Tool::Protractor: measureDraw(e); return true;
        case Viewer::Tool::FormText: case Viewer::Tool::FormCheck: case Viewer::Tool::FormCombo:
        case Viewer::Tool::FormRadio: case Viewer::Tool::FormList: case Viewer::Tool::FormDate: return formFieldDraw(d, e);
        case Viewer::Tool::Polygon:
            if (annot::addPolygon(d, e.page, e.pts, drawColor(), lineWidth())) { app.viewer.documentChanged(); refresh(); }
            return true;
        case Viewer::Tool::Cloud: {
            annot::RectD r{std::min(e.pts[0].x, e.pts[1].x), std::min(e.pts[0].y, e.pts[1].y), std::max(e.pts[0].x, e.pts[1].x), std::max(e.pts[0].y, e.pts[1].y)};
            if (annot::addCloud(d, e.page, r, drawColor(), lineWidth())) { app.viewer.documentChanged(); refresh(); }
            return true;
        }
        case Viewer::Tool::Callout: {
            std::wstring text;
            if (askText(L"Callout", L"The text in the box (Enter for a new line).", false, text, L"", true)) {
                text = fromEdit(text);
                if (!text.empty() && !pageobj::isLatin(text)) MessageBoxW(app.wnd, L"Callouts can use letters, numbers and common symbols of Western languages.", kAppName, MB_OK | MB_ICONINFORMATION);
                else if (!text.empty() && annot::addCallout(d, e.page, e.pts[0], e.pts[1], text, drawColor())) { app.viewer.documentChanged(); refresh(); }
            }
            setTool(Viewer::Tool::Callout);
            return true;
        }
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
                notice(L"No paragraph there. Click directly on the text (scanned pages are pictures: use text recognition first).");
                return true;
            }
            editParagraphInPlace(d, hit, clickPoint(e.page, e.pts.front()));
            return true;
        }
        case Viewer::Tool::TableEdit: case Viewer::Tool::TableDelete: {
            tables::Table t;
            std::wstring why;
            if (!tables::tableAt(d, e.page, e.pts.front().x, e.pts.front().y, t, &why)) { notice(why + L" Or drag a box around the table."); return true; }
            if (e.tool == Viewer::Tool::TableEdit) editTable(d, t);
            else if (MessageBoxW(app.wnd, L"Delete this table from the page?", kAppName, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES && tables::remove(d, t)) afterTableChange(d, t.page);
            return true;   // the tool stays on: the next table can be clicked
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
    d->beginChange(L"Add a bookmark");
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
    std::vector<FormField> f(4);
    f[0] = {FormField::Choice, L"Split:", {L"Into single pages", L"Every N pages", L"By page ranges", L"At the bookmarks (chapters)", L"Into files no larger than"}, L"", 1};
    f[1] = {FormField::Text, L"N pages per file:", {}, L"10"};
    f[2] = {FormField::Text, L"Ranges (use ; between):", {}, L"1-5; 6-"};
    f[3] = {FormField::Text, L"Largest file (MB):", {}, L"10"};
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
        if (v[0].choice == 3 && g_formDoc && g_formDoc->bookmarks().empty()) return L"This document has no bookmarks. Choose another way to split it.";
        if (v[0].choice == 4) { wchar_t* e = nullptr; double mb = wcstod(v[3].text.c_str(), &e); if (e == v[3].text.c_str() || *e || mb <= 0.01) return L"Type the largest size in megabytes, for example 10 or 2.5."; }
        return L""; });
    g_formDoc = nullptr;
    if (!ok) return;
    std::vector<std::vector<int>> groups;
    std::vector<std::wstring> names;   // bookmark titles, for the file names
    const int n = d->pageCount();
    if (f[0].choice >= 3) {
        HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
        const auto parts = f[0].choice == 3 ? splitmerge::byBookmarks(*d, 1) : splitmerge::bySize(*d, wcstod(f[3].text.c_str(), nullptr) * 1024 * 1024);
        SetCursor(old);
        for (const auto& p : parts) { groups.push_back(p.pages); names.push_back(splitmerge::fileSafe(p.name)); }
    } else if (f[0].choice == 0) for (int i = 0; i < n; ++i) groups.push_back({i});
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
        std::wstring file = stem + L"-" + std::to_wstring(i + 1);
        if (i < names.size() && !names[i].empty()) file += L" " + names[i];
        std::vector<unsigned char> bytes;   // with the bookmarks that point into the part
        if (splitmerge::partBytes(*d, groups[i], bytes) && Document::writeFile(file + L".pdf", bytes) == SaveResult::Ok) ++saved;
    }
    wchar_t msg[160];
    swprintf(msg, 160, L"%d file(s) were saved, numbered from \"%ls-1\".", saved, stem.substr(stem.find_last_of(L'\\') + 1).c_str());
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
    themeui::themeDialog(p.wnd);
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
// Word by word across the whole documents (shared/textdiff): a copy of the first with each change marked (the comment
// says what it became), and a report listing every change with its pages, each in a tab of its own.
void compareTextDocs(Document* a, Document* b, const textdiff::Options& o) {
    std::vector<unsigned char> bytes;
    auto copy = std::make_unique<Document>();
    if (!a->serialize(bytes) || copy->load(std::move(bytes), {}) != OpenResult::Ok) {
        MessageBoxW(app.wnd, L"The comparison copy could not be opened. (Password-protected documents cannot be compared.)", kAppName, MB_OK | MB_ICONWARNING);
        return;
    }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    const textdiff::Result r = textdiff::compare(*a, *b, o);
    textdiff::mark(*copy, r);
    const std::wstring md = textdiff::report(r, a->title(), b->title());
    std::vector<unsigned char> pdf;
    auto rep = std::make_unique<Document>();
    const bool haveReport = textpdf::build(md, true, pdf).ok && rep->load(std::move(pdf), {}) == OpenResult::Ok;
    SetCursor(old);
    int replaced = 0, inserted = 0, removed = 0;
    for (const auto& c : r.changes) (c.kind == textdiff::Change::Replaced ? replaced : c.kind == textdiff::Change::Inserted ? inserted : removed)++;
    if (!r.changes.empty()) { copy->setName(L"Changes marked - " + a->title()); addDocumentTab(std::move(copy)); }
    if (haveReport) { rep->setName(L"Comparison report - " + a->title()); addDocumentTab(std::move(rep)); }
    wchar_t msg[600];
    if (r.changes.empty())
        swprintf(msg, 600, L"The words of \"%ls\" and \"%ls\" are the same (%d words).\n\nThis compares the text, not how the pages look.", a->title().c_str(), b->title().c_str(), r.same);
    else
        swprintf(msg, 600, L"%d change(s): %d replaced, %d inserted, %d removed. %d words are unchanged.%ls\n\n"
                           L"Two new tabs: the first document with each change marked (point at a mark to see what it became), and the report.",
                 static_cast<int>(r.changes.size()), replaced, inserted, removed, r.same, r.rough ? L"\nThe documents differ a lot, so they were compared page by page." : L"");
    MessageBoxW(app.wnd, msg, L"Compare the text", MB_OK | MB_ICONINFORMATION);
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
    f[2] = {FormField::Choice, L"How:", {L"The words (a report and a marked copy)", L"How the pages look (overlay)", L"Both"}, L"", 0};
    f.push_back({FormField::Check, L"Words: ignore capital letters"});
    f.push_back({FormField::Check, L"Words: ignore punctuation"});
    if (!showForm(app.wnd, app.inst, L"Compare documents", f, L"Compare")) return;
    Document* b = others[f[1].choice];
    textdiff::Options o;
    o.ignoreCase = f[3].checked;
    o.ignorePunctuation = f[4].checked;
    if (f[2].choice != 1) compareTextDocs(a, b, o);
    if (f[2].choice != 0) compareVisual(a, b);
}

// ---- side by side ----
void sideBySideAction(int cmd) {
    if (cmd == N_SBS_OFF) {
        const int page = activeDoc() ? app.viewer.currentPage() : 0;
        app.sbs = false; app.sbsDoc = nullptr; app.sbsOwned.reset(); app.sbsSwapped = false;
        app.viewer2.setDocument(nullptr);
        ShowWindow(app.viewer2.hwnd(), SW_HIDE);
        relayoutChildren();
        if (Document* d = activeDoc()) { d->fitWidth = true; app.viewer.fitWidth(); app.viewer.goToPage(page); }   // the usual one-document view, same page
        InvalidateRect(app.wnd, nullptr, FALSE);
        return;
    }
    if (cmd == N_SBS_LINK) {
        app.sbsLinked = !app.sbsLinked;
        saveSettings();
        if (app.sbsLinked) syncSideBySide(true);
        return;
    }
    if (cmd == N_SBS_HORZ || cmd == N_SBS_VERT) {
        app.sbsStacked = cmd == N_SBS_VERT;
        saveSettings();
        const int p1 = app.viewer.currentPage(), p2 = app.sbs ? app.viewer2.currentPage() : 0;
        relayoutChildren();
        if (app.sbs && activeDoc() && app.sbsDoc) {
            app.syncing = true;
            activeDoc()->fitWidth = true; app.sbsDoc->fitWidth = true; app.viewer.fitWidth(); app.viewer2.fitWidth();
            app.viewer.goToPage(p1); app.viewer2.goToPage(p2);
            app.syncing = false;
        }
        return;
    }
    if (cmd == N_SBS_SWAP) {
        if (!app.sbs) return;
        app.sbsSwapped = !app.sbsSwapped;
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
        const int keep = app.active;
        addDocumentTab(std::move(doc));   // a tab of its own, so it can be the active side too
        switchTab(keep);
    }
    if (!other) return;
    app.sbs = true; app.sbsDoc = other; app.sbsSwapped = false;
    app.viewer2.setLayout(app.layout, app.cover);
    app.viewer2.setDocument(other);
    ShowWindow(app.viewer2.hwnd(), SW_SHOW);
    relayoutChildren();
    if (activeDoc()) { other->fitWidth = true; app.viewer2.fitWidth(); app.viewer.fitWidth(); }
}

void endSideBySide() { sideBySideAction(N_SBS_OFF); }

// keeps the second view at the same zoom and position as the first
void syncSideBySide(bool fromFirst) {
    if (!app.sbs || app.syncing || !app.sbsLinked) return;
    Document* a = activeDoc();
    if (!a || !app.sbsDoc || a == app.sbsDoc) return;
    app.syncing = true;
    Viewer& dst = fromFirst ? app.viewer2 : app.viewer;
    Document* src = fromFirst ? a : app.sbsDoc;
    Document* tgt = fromFirst ? app.sbsDoc : a;
    if (std::fabs(tgt->zoom - src->zoom) > 1e-6) dst.setZoom(src->zoom);
    if (app.layout == 0) {   // single page: the same page number on both sides
        const int p = (fromFirst ? app.viewer : app.viewer2).currentPage();
        if (dst.currentPage() != p) dst.goToPage(p);
    }
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
    themeui::themeDialog(h);
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
std::wstring readTextFile(const std::wstring& path) { return platform::readTextFile(path); }

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
    if (fields.empty()) { MessageBoxW(app.wnd, L"This document has no form fields to fill in. Use Edit PDF > Fillable form > Find fields in a flat form to make some.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
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
    d.beginChange(L"Add form fields");
    if (d.reload(std::move(out)) != OpenResult::Ok) {
        d.cancelChange();
        MessageBoxW(app.wnd, L"The fields could not be added to this document.", kAppName, MB_OK | MB_ICONWARNING);
        return false;
    }
    formdata::drawRadioLabels(d, specs);
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
    const Viewer::Tool tl = e.tool;
    const wchar_t* what = tl == Viewer::Tool::FormText ? L"text field" : tl == Viewer::Tool::FormCheck ? L"check box" : tl == Viewer::Tool::FormRadio ? L"radio button group" :
                          tl == Viewer::Tool::FormList ? L"list box" : tl == Viewer::Tool::FormDate ? L"date field" : L"drop-down list";
    std::wstring suggestion = tl == Viewer::Tool::FormText ? L"Text" : tl == Viewer::Tool::FormCheck ? L"Check" : tl == Viewer::Tool::FormRadio ? L"Group" :
                              tl == Viewer::Tool::FormList ? L"List" : tl == Viewer::Tool::FormDate ? L"Date" : L"Choice";
    for (int k = 1; used.count(suggestion + std::to_wstring(k)); ++k) {}
    int k = 1;
    while (used.count(suggestion + std::to_wstring(k))) ++k;
    std::wstring name = suggestion + std::to_wstring(k);
    std::wstring in;
    while (askText(L"Form field", std::wstring(L"Name of the ") + what + L" (it must be different from the other fields).", false, in, name.c_str())) {
        if (in.empty() || used.count(in)) { MessageBoxW(app.wnd, L"Type a name that no other field has.", kAppName, MB_OK | MB_ICONINFORMATION); name = in; continue; }
        sp.name = in;
        sp.kind = tl == Viewer::Tool::FormText ? formcreate::Spec::Text : tl == Viewer::Tool::FormCheck ? formcreate::Spec::Check : tl == Viewer::Tool::FormRadio ? formcreate::Spec::Radio :
                  tl == Viewer::Tool::FormList ? formcreate::Spec::List : tl == Viewer::Tool::FormDate ? formcreate::Spec::Date : formcreate::Spec::Combo;
        if (sp.kind == formcreate::Spec::Combo || sp.kind == formcreate::Spec::List || sp.kind == formcreate::Spec::Radio) {
            std::wstring opts;
            if (!askText(sp.kind == formcreate::Spec::Radio ? L"Radio buttons" : sp.kind == formcreate::Spec::List ? L"List box" : L"Drop-down list",
                         sp.kind == formcreate::Spec::Radio ? L"The choices, one per line. They are stacked in the box you drew, one button each." : L"The choices, one per line.", false, opts, L"Yes\r\nNo", true)) break;
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

void formReset() {
    Document* d = activeDoc();
    if (!d) return;
    if (formdata::list(*d).empty()) { MessageBoxW(app.wnd, L"This document has no form fields.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    if (MessageBoxW(app.wnd, L"Clear every answer in this form?", L"Reset the form", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    d->beginChange(L"Reset the form");
    formdata::resetAll(*d);
    std::vector<unsigned char> bytes, out;
    if (d->serialize(bytes) && formcreate::clearRadios(bytes, out)) d->reload(std::move(out));
    app.viewer.documentChanged();
    app.sidebar.refresh();
    refresh();
}

void flattenDocument() {
    Document* d = activeDoc();
    if (!d) return;
    if (MessageBoxW(app.wnd, L"Make every comment, drawing, stamp and form field a fixed part of the page?\n\nThey can no longer be edited or filled in. Links keep working. Undo (Ctrl+Z) takes it back until you save.",
                    L"Flatten", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    const int n = d->flattenAll();
    SetCursor(old);
    if (!n) { MessageBoxW(app.wnd, L"There was nothing to flatten.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    afterPagesChanged();
    MessageBoxW(app.wnd, (L"Flattened " + std::to_wstring(n) + L" page(s).").c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
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

// ---- privacy: remove hidden data ----
void showFonts(Document& d) {
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    const fontlist::Report r = fontlist::build(d);
    SetCursor(old);
    fontlist::show(app.wnd, app.inst, r, L"Fonts used in " + d.title());
}

void pdfaDialog() {
    Document* d = activeDoc();
    if (!d) return;
    std::vector<FormField> f(2);
    f[0] = {FormField::Note, L"", {}, L"PDF/A-2b, for archiving: fonts are embedded; scripts, attachments and passwords are left out. Comments and fields become part of the pages."};
    f[1] = {FormField::Choice, L"Pages:", {L"Keep text and drawings (recommended)", L"Pictures with searchable text (larger)"}, L"", 0};
    if (!showForm(app.wnd, app.inst, L"Save a PDF/A copy", f, L"Choose file...")) return;
    std::vector<wchar_t> buf(MAX_PATH * 2, 0);
    std::wstring base = d->title();
    if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
    wcsncpy_s(buf.data(), buf.size(), (base + L" (PDF-A).pdf").c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = app.wnd;
    ofn.lpstrTitle = L"Save the PDF/A copy as";
    ofn.lpstrFilter = L"PDF files (*.pdf)\0*.pdf\0";
    ofn.lpstrDefExt = L"pdf";
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = static_cast<DWORD>(buf.size());
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) return;
    if (_wcsicmp(buf.data(), d->path().c_str()) == 0) { MessageBoxW(app.wnd, L"Choose a different name: the PDF/A copy cannot replace the open file.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::vector<unsigned char> out;
    pdfa::Result r;
    std::wstring err;
    const bool ok = pdfa::convert(*d, f[1].choice == 1, out, r, err) && Document::writeFile(buf.data(), out) == SaveResult::Ok;
    SetCursor(old);
    if (!ok) { MessageBoxW(app.wnd, (L"The PDF/A copy could not be made. " + err).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    std::wstring msg = L"Saved the PDF/A-2b copy (" + std::to_wstring(r.pages) + L" page(s)).";
    for (const auto& n : r.notes) msg += L"\n\n" + n;
    msg += L"\n\nOpen it?";
    if (MessageBoxW(app.wnd, msg.c_str(), kAppName, MB_YESNO | MB_ICONINFORMATION) == IDYES) openFile(buf.data());
}

void sanitizeDialog() {
    Document* d = activeDoc();
    if (!d) return;
    if (d->isProtected()) { MessageBoxW(app.wnd, L"Remove the password protection first (Edit PDF > Remove password protection), clean the document, then protect it again.", kAppName, MB_OK | MB_ICONINFORMATION); return; }
    std::vector<FormField> f(7);
    f[0] = {FormField::Note, L"", {}, L"Removes data people do not see but that travels with the file. The pages look the same."};
    f[1] = {FormField::Check, L"Document information and metadata (author, title, software, dates)", {}, L"", 0, true};
    f[2] = {FormField::Check, L"Attached files", {}, L"", 0, true};
    f[3] = {FormField::Check, L"Scripts and automatic actions", {}, L"", 0, true};
    f[4] = {FormField::Check, L"Hidden annotations, page thumbnails, application data", {}, L"", 0, true};
    f[5] = {FormField::Check, L"All comments, highlights and drawings", {}, L"", 0, false};
    f[6] = {FormField::Check, L"Bookmarks", {}, L"", 0, false};
    if (!showForm(app.wnd, app.inst, L"Remove hidden data", f, L"Remove")) return;
    sanitize::Options o;
    o.metadata = f[1].checked; o.attachments = f[2].checked; o.scripts = f[3].checked; o.hidden = f[4].checked; o.comments = f[5].checked; o.bookmarks = f[6].checked;
    std::vector<unsigned char> bytes, out;
    sanitize::Report rep;
    std::wstring err;
    HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    bool ok = d->serialize(bytes) && sanitize::run(bytes, o, out, rep, err);
    SetCursor(old);
    if (!ok) { MessageBoxW(app.wnd, (L"The document could not be cleaned. " + err).c_str(), kAppName, MB_OK | MB_ICONWARNING); return; }
    d->beginChange(L"Remove hidden data");
    if (d->reload(std::move(out)) != OpenResult::Ok) { d->undo(); MessageBoxW(app.wnd, L"The cleaned document could not be opened, so nothing was changed.", kAppName, MB_OK | MB_ICONWARNING); return; }
    afterPagesChanged();
    std::wstring msg = rep.removed.empty() ? L"Nothing of the chosen kinds was found. The document was already clean." : L"Removed:\n";
    for (const auto& r : rep.removed) msg += L"  - " + r + L"\n";
    wchar_t sz[160];
    swprintf(sz, 160, L"\nSize: %.0f KB before, %.0f KB after.\n\nSave the document to keep the change; Undo (Ctrl+Z) takes it back.", rep.before / 1024.0, rep.after / 1024.0);
    if (!rep.removed.empty()) msg += sz;
    MessageBoxW(app.wnd, msg.c_str(), L"Privacy report", MB_OK | MB_ICONINFORMATION);
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
        add(L"Form: add radio buttons", [] { startFormField(Viewer::Tool::FormRadio); });
        add(L"Form: add a list box", [] { startFormField(Viewer::Tool::FormList); });
        add(L"Form: add a date field", [] { startFormField(Viewer::Tool::FormDate); });
        add(L"Form: clear all answers", [] { formReset(); });
        add(L"Flatten comments and form fields", [] { flattenDocument(); });
        add(L"Polygon", [] { setTool(Viewer::Tool::Polygon); });
        add(L"Cloud", [] { setTool(Viewer::Tool::Cloud); });
        add(L"Callout", [] { setTool(Viewer::Tool::Callout); });
        add(L"Reverse the page order", [] { pagesMenuAction(P_REVERSE, {}); });
        add(L"Two-sided scan: interleave fronts and backs (odd and even pages)", [] { pagesMenuAction(P_INTERLEAVE, {}); });
        add(L"Remove blank pages", [] { pagesMenuAction(P_BLANKS, {}); });
        add(L"Find duplicate pages", [] { pagesMenuAction(P_DUPES, {}); });
        add(L"Add headers and footers", [] { pagesMenuAction(P_HEADER, {}); });
        add(L"Delete header and footer (page numbers, running titles, logos)", [] { pagesMenuAction(P_HFDELETE, {}); });
        add(L"Background colour", [] { pagesMenuAction(P_BGCOLOR, {}); });
    }
    add(L"New PDF from pictures", [] { newPdfFromPictures(); });
    if (d) {
        add(L"Save", [] { saveActive(); });
        add(L"Save as", [] { if (activeDoc()) saveAsDialog(*activeDoc()); });
        add(L"Close tab", [] { closeTab(app.active); });
        add(L"Print (with preview)", [] { printDocument(); });
        add(L"User guide (how to use Feather PDF)", [] { openUserGuide(); });
        add(L"Document properties", [] { showProperties(); });
        add(L"Attached files: open, save, add or remove", [] { attachmentsDialog(); });
        add(L"Repair a damaged PDF", [] { repairPdf(); });
        add(L"Accessibility: check for screen readers and fix (title, language, tab order...)", [] { if (Document* d = activeDoc()) accessibilityCheck(*d); });
        add(L"Save a copy for the web (Fast Web View, linearized)", [] { if (Document* d = activeDoc()) saveForWeb(*d); });
        add(L"Layers: show or hide", [] { layersDialog(); });
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
        add(L"Page layout: show the cover page on its own (two pages)", [] { setPageLayout(3); });
        add(L"Rotate view left", [] { rotateView(-1); });
        add(L"Rotate view right", [] { rotateView(1); });
        add(L"Full screen", [] { setFullscreen(true, false); });
        add(L"Presentation", [] { setFullscreen(true, true); });
        add(L"Bookmarks panel", [] { showSidebar(Sidebar::Mode::Bookmarks); });
        add(L"Thumbnails panel", [] { showSidebar(Sidebar::Mode::Pages); });
        add(L"Undo history panel: go back to any earlier step", [] { showSidebar(Sidebar::Mode::History); });
        add(L"Edit text (one line)", [] { toolsDispatch(T_EDIT_TEXT); });
        add(L"Edit paragraph", [] { toolsDispatch(N_EDITPARA); });
        add(L"Move or resize pictures, paragraphs and tables", [] { toolsDispatch(N_SELECTOBJ); });
        add(L"Picture: turn right", [] { pictureAction(PX_ROT_R); });
        add(L"Picture: turn left", [] { pictureAction(PX_ROT_L); });
        add(L"Picture: flip left to right", [] { pictureAction(PX_FLIP_H); });
        add(L"Picture: flip upside down", [] { pictureAction(PX_FLIP_V); });
        add(L"Picture: crop", [] { pictureAction(PX_CROP); });
        add(L"Picture: brightness and contrast", [] { pictureAction(PX_ADJUST); });
        add(L"Picture: black and white", [] { pictureAction(PX_GREY); });
        add(L"Picture: replace", [] { pictureAction(PX_REPLACE); });
        add(L"Picture: save as PNG (extract)", [] { pictureAction(PX_SAVE); });
        add(L"Picture: bring to front", [] { pictureAction(PX_FRONT); });
        add(L"Picture: send to back", [] { pictureAction(PX_BACK); });
        add(L"Paste picture from the clipboard", [] { pictureAction(PX_PASTE); });
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
        add(L"Comments and review: reply to a comment, accept or reject it, review summary", [] { if (Document* d = activeDoc()) reviewDialog(*d); });
        add(L"Recognise text (OCR)", [] { toolsDispatch(T_OCR); });
        add(L"Watermark", [] { toolsDispatch(T_WATERMARK); });
        add(L"Page numbers", [] { toolsDispatch(T_NUMBERS); });
        add(L"Compress", [] { toolsDispatch(T_COMPRESS); });
        add(L"Black and white / greyscale pictures", [] { doGreyscale(); });
        add(L"Password protection", [] { toolsDispatch(T_PROTECT); });
        add(L"Remove hidden data (metadata, attachments, scripts)", [] { sanitizeDialog(); });
        add(L"Save a PDF/A copy for archiving (also text and pictures)", [] { pdfaDialog(); });
        add(L"Fonts used in this PDF (and pages that need OCR)", [] { if (Document* d = activeDoc()) showFonts(*d); });
        add(L"Side by side: link scrolling on / off", [] { sideBySideAction(N_SBS_LINK); });
        add(L"Side by side: left and right", [] { sideBySideAction(N_SBS_HORZ); });
        add(L"Side by side: top and bottom", [] { sideBySideAction(N_SBS_VERT); });
        add(L"Side by side: swap the two sides", [] { sideBySideAction(N_SBS_SWAP); });
        add(L"Sign with a certificate", [] { toolsDispatch(T_SIGN); });
        add(L"Export", [] { toolsDispatch(T_EXPORT); });
        add(L"Read aloud", [] { readAloud(); });
        add(L"Compare with another open tab", [] { compareDocs(); });
        add(L"Split into several files (every N pages, at bookmarks, by size)", [] { splitPdf(); });
        add(L"Merge PDFs into this document (keeping their bookmarks)", [] { pagesMenuAction(P_MERGE, {}); });
        add(L"Crop by drawing a box", [] { cropByBox(); });
        add(L"Check that redacted text is gone", [] { toolsDispatch(N_RED_CHECK); });
        add(L"Find sensitive information to redact (e-mail, phone, Aadhaar, PAN, bank)", [] { toolsDispatch(N_RED_SENSITIVE); });
        add(L"Delete pages", [] { pagesMenuAction(P_DELETE, {}); });
        add(L"Insert blank page", [] { pagesMenuAction(P_BLANK, {}); });
    }
    add(L"Settings", [] { showSettings(SP_APPEARANCE); });
    add(L"Settings: theme, icons and tab colours", [] { showSettings(SP_APPEARANCE); });
    add(L"Settings: tab numbers", [] { showSettings(SP_TABS); });
    add(L"Settings: page colours and reading speed", [] { showSettings(SP_VIEWING); });
    add(L"Settings: highlight and drawing colours", [] { showSettings(SP_EDITING); });
    add(L"Set as default PDF application", [] { registerAsViewer(); });
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
    themeui::themeDialog(h);
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

// ---- the header: the tab row (which is also the title bar) and the toolbar; the status bar at the bottom ----
enum Icon { I_NONE, I_OPEN, I_SAVE, I_UNDO, I_REDO, I_EDIT, I_ANNOT, I_COMMENT, I_GEOM, I_SBS, I_PREV, I_NEXT, I_ZOUT, I_ZIN, I_SEARCH, I_MORE,
            I_FILE, I_DROP, I_PLUS, I_CHEVRON };   // same numbering as icons::

std::vector<int> g_separators;   // x of the thin lines between toolbar groups

// The toolbar group whose tool is in use: highlighted, so it is clear where the clicks on the page go.
int activeGroup() {
    using T = Viewer::Tool;
    switch (app.tool) {
        case T::None: case T::CopyArea: return 0;
        case T::Comment: return C_COMMENT;
        case T::Ruler: case T::Protractor: return C_GEOM;
        case T::AddText: case T::Rectangle: case T::Ellipse: case T::Line: case T::Arrow: case T::Pen: case T::Polygon:
        case T::Cloud: case T::Callout: case T::Stamp: case T::Picture: case T::Link: return C_ANNOT;
        default: return C_EDITPDF;
    }
}
bool highlighted(int id) { return (id != 0 && id == activeGroup()) || (id == C_SBS && app.sbs); }

const wchar_t* toolName(Viewer::Tool t) {
    using T = Viewer::Tool;
    switch (t) {
        case T::Comment: return L"Comment"; case T::Rectangle: return L"Rectangle"; case T::Ellipse: return L"Ellipse";
        case T::Line: return L"Line"; case T::Arrow: return L"Arrow"; case T::Pen: return L"Pen"; case T::Redact: return L"Mark areas to redact";
        case T::AddText: return L"Add text"; case T::EditText: return L"Edit text"; case T::Picture: return L"Insert picture";
        case T::Stamp: return L"Stamp"; case T::Link: return L"Link"; case T::CopyArea: return L"Copy area"; case T::CropBox: return L"Crop";
        case T::TableDelete: return L"Delete table"; case T::TablePlace: return L"Insert table"; case T::TableEdit: return L"Edit table";
        case T::EditPara: return L"Edit paragraph"; case T::Select: return L"Move or resize"; case T::Polygon: return L"Polygon";
        case T::Cloud: return L"Cloud"; case T::Callout: return L"Callout"; case T::FormRadio: return L"Radio button";
        case T::FormList: return L"List field"; case T::FormDate: return L"Date field"; case T::FormText: return L"Text field";
        case T::FormCheck: return L"Check box"; case T::FormCombo: return L"Drop-down list"; case T::Ruler: return L"Ruler";
        case T::Protractor: return L"Protractor"; case T::PicCrop: return L"Crop picture"; case T::None: break;
    }
    return L"";
}

int textWidth(HDC dc, HFONT f, const std::wstring& t) {
    HGDIOBJ old = SelectObject(dc, f);
    SIZE sz{};
    GetTextExtentPoint32W(dc, t.c_str(), static_cast<int>(t.size()), &sz);
    SelectObject(dc, old);
    return sz.cx;
}

std::wstring tipFor(int id) {
    switch (id) {
        case C_OPEN: return L"Open a PDF (Ctrl+O)";
        case C_SAVE: return L"Save (Ctrl+S)";
        case C_UNDO: return L"Undo (Ctrl+Z)";
        case C_REDO: return L"Redo (Ctrl+Y)";
        case C_EDITPDF: return L"Edit text, pictures, pages and forms";
        case C_ANNOT: return L"Highlight, draw, add text, stamps and signatures";
        case C_COMMENT: return L"Comments and notes";
        case C_GEOM: return L"Measure distances and angles";
        case C_SBS: return L"Show two documents side by side";
        case C_PREV: return L"Previous page (Page Up)";
        case C_NEXT: return L"Next page (Page Down)";
        case C_ZOOMOUT: return L"Zoom out";
        case C_ZOOMIN: return L"Zoom in";
        case C_ZOOMMENU: return L"Zoom presets, fit width, fit page";
        case C_FIND: return L"Find in the document (Ctrl+F)";
        case C_MENU: return L"More: files, view, settings and help";
        case C_NEWTAB: return L"Open a PDF in a new tab (Ctrl+O)";
        case C_CAPMIN: return L"Minimise";
        case C_CAPMAX: return IsZoomed(app.wnd) ? L"Restore down" : L"Maximise";
        case C_CAPCLOSE: return L"Close";
        case ID_PAGE_BOX: return L"Page number: type a page and press Enter (Ctrl+G)";
        case ID_ZOOM_BOX: return L"Zoom: type a percentage and press Enter";
    }
    if (id >= C_TABCLOSE && id < C_TABCLOSE + 1000) return L"Close this tab (Ctrl+W)";
    if (id >= C_TAB && id < C_TAB + 1000 && static_cast<size_t>(id - C_TAB) < app.tabs.size()) {
        const Document& d = *app.tabs[static_cast<size_t>(id - C_TAB)];
        return d.path().empty() ? d.title() + L" (not saved yet)" : d.path();
    }
    return L"";
}

// The tooltips follow the buttons; they are registered again only when the buttons moved.
void syncTips() {
    if (!app.tips) return;
    std::wstring key;
    auto add = [&](int id, const RECT& r) { key += std::to_wstring(id) + L":" + std::to_wstring(r.left) + L"," + std::to_wstring(r.top) + L"," + std::to_wstring(r.right) + L";"; };
    for (const Button& b : app.buttons) add(b.id, b.rc);
    for (size_t i = 0; i < app.tabRects.size(); ++i) { add(C_TAB + static_cast<int>(i), app.tabRects[i]); add(C_TABCLOSE + static_cast<int>(i), app.closeRects[i]); }
    add(C_NEWTAB, app.plusRect);
    if (key == app.tipKey) return;
    app.tipKey = key;
    while (SendMessageW(app.tips, TTM_GETTOOLCOUNT, 0, 0) > 0) {
        TTTOOLINFOW ti{sizeof(ti)};
        if (!SendMessageW(app.tips, TTM_ENUMTOOLSW, 0, reinterpret_cast<LPARAM>(&ti))) break;
        SendMessageW(app.tips, TTM_DELTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
    auto reg = [&](UINT id, const RECT& r) {
        TTTOOLINFOW ti{sizeof(ti)};
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = app.wnd;
        ti.uId = id;
        ti.rect = r;
        ti.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(app.tips, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    };
    // the close buttons first: a tab's own tip must not cover its close button's
    for (size_t i = 0; i < app.tabRects.size(); ++i) reg(C_TABCLOSE + static_cast<UINT>(i), app.closeRects[i]);
    for (size_t i = 0; i < app.tabRects.size(); ++i) reg(C_TAB + static_cast<UINT>(i), app.tabRects[i]);
    reg(C_NEWTAB, app.plusRect);
    for (const Button& b : app.buttons) reg(static_cast<UINT>(b.id), b.rc);
    for (HWND box : {app.pageBox, app.zoomBox}) {
        TTTOOLINFOW ti{sizeof(ti)};
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = app.wnd;
        ti.uId = reinterpret_cast<UINT_PTR>(box);
        ti.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(app.tips, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
}

void placeBox(HWND box, const RECT& frame, int rightInset) {
    if (!box) return;
    TEXTMETRICW tm{};
    HDC dc = GetDC(box);
    HGDIOBJ of = SelectObject(dc, app.font);
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, of);
    ReleaseDC(box, dc);
    const int hgt = tm.tmHeight + 2;
    RECT want{frame.left + px(4), (frame.top + frame.bottom - hgt) / 2, frame.right - px(4) - rightInset, (frame.top + frame.bottom - hgt) / 2 + hgt};
    RECT now; GetWindowRect(box, &now);
    MapWindowPoints(nullptr, app.wnd, reinterpret_cast<POINT*>(&now), 2);
    if (!EqualRect(&now, &want)) SetWindowPos(box, nullptr, want.left, want.top, want.right - want.left, want.bottom - want.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

// The page and zoom boxes show the view, except while they are being typed in.
void syncBoxes(bool force) {
    Document* d = activeDoc();
    for (HWND box : {app.pageBox, app.zoomBox}) {
        if (!box || (!force && GetFocus() == box)) continue;
        std::wstring want;
        if (d) want = box == app.pageBox ? std::to_wstring(app.viewer.currentPage() + 1) : std::to_wstring(static_cast<int>(d->zoom * 100 + 0.5)) + L"%";
        wchar_t now[32] = L"";
        GetWindowTextW(box, now, 32);
        if (want != now) SetWindowTextW(box, want.c_str());
    }
}

void layoutHeader(HDC dc, int width) {
    app.buttons.clear(); app.tabRects.clear(); app.closeRects.clear(); g_separators.clear();
    Document* d = activeDoc();
    const int th = tabH();

    // the tab row: the program icon, the tabs, the + button, and the window buttons at the right
    const int capW = px(46);
    const int capLeft = width - 3 * capW;
    app.buttons.push_back({L"", {capLeft, 0, capLeft + capW, th}, C_CAPMIN, true, 0, true});
    app.buttons.push_back({L"", {capLeft + capW, 0, capLeft + 2 * capW, th}, C_CAPMAX, true, 0, true});
    app.buttons.push_back({L"", {capLeft + 2 * capW, 0, width, th}, C_CAPCLOSE, true, 0, true});
    const int is = px(18);
    app.iconRect = {px(12), (th - is) / 2 + px(2), px(12) + is, (th - is) / 2 + px(2) + is};
    int x = app.iconRect.right + px(10);
    const int tabTop = px(8), mid = (tabTop + th) / 2, cs = px(16);
    if (app.tabs.empty()) x += textWidth(dc, app.font, kAppName) + px(10);
    const int plusW = px(30);
    const int room = capLeft - px(16) - plusW - x;
    std::vector<int> ws;
    int sum = 0;
    for (const auto& t : app.tabs) {
        const int w = std::clamp(textWidth(dc, app.fontBold, tabLabel(*t)) + px(14) + px(8) + cs + px(10), px(120), px(240));   // bold when active
        ws.push_back(w);
        sum += w + px(2);
    }
    if (!ws.empty() && sum > room) {   // many tabs: all get narrower
        const int each = std::max(px(36), room / static_cast<int>(ws.size()) - px(2));
        for (int& w : ws) w = std::min(w, each);
    }
    const int limit = capLeft - px(8) - plusW;   // tabs and + never run under the window buttons
    for (int w : ws) {
        if (x + w > limit) break;   // more tabs than fit even at the narrowest: Ctrl+Tab reaches the rest
        app.tabRects.push_back({x, tabTop, x + w, th});
        const int ci = w < px(64) ? (w - cs) / 2 : w - px(10) - cs;   // the narrowest tabs: the button in the middle
        app.closeRects.push_back({x + ci, mid - cs / 2, x + ci + cs, mid - cs / 2 + cs});
        x += w + px(2);
    }
    x = std::min(x, limit);
    app.plusRect = {x + px(4), mid - px(14), x + px(4) + px(28), mid + px(14)};

    // the toolbar: file | undo | tool groups ........ page | zoom | find, more
    const int top = th + (toolH() - px(36)) / 2, bottom = top + px(36);
    const int sep = px(13), iconW = px(36);
    static const struct { const wchar_t* label; int id, icon; } groups[] = {
        {L"Edit PDF", C_EDITPDF, I_EDIT}, {L"Annotate", C_ANNOT, I_ANNOT}, {L"Comment", C_COMMENT, I_COMMENT},
        {L"Geometry", C_GEOM, I_GEOM}, {L"Side by side", C_SBS, I_SBS}};
    auto groupW = [&](const wchar_t* label, bool compact) { return compact ? px(40) : px(10) + px(20) + px(6) + textWidth(dc, app.font, label) + px(12); };
    const std::wstring ofText = d ? L"of " + std::to_wstring(d->pageCount()) : L"";
    const int ofW = std::max(textWidth(dc, app.font, L"of 999"), textWidth(dc, app.font, ofText)) + px(10);
    const int rightW = px(32) + px(50) + ofW + px(32) + sep + px(32) + px(84) + px(32) + sep + iconW + px(42) + px(10);
    int leftW = px(8) + 4 * iconW + 2 * sep;
    for (const auto& g : groups) leftW += groupW(g.label, false) + px(2);
    app.compact = leftW + rightW + px(16) > width;
    x = px(8);
    auto icon = [&](int id, int ic, bool enabled) { app.buttons.push_back({L"", {x, top, x + iconW, bottom}, id, enabled, ic}); x += iconW; };
    auto separator = [&] { g_separators.push_back(x + sep / 2); x += sep; };
    icon(C_OPEN, I_OPEN, true);
    icon(C_SAVE, I_SAVE, d && d->dirty());
    separator();
    icon(C_UNDO, I_UNDO, d && d->canUndo());
    icon(C_REDO, I_REDO, d && d->canRedo());
    separator();
    for (const auto& g : groups) {
        const int w = groupW(g.label, app.compact);
        app.buttons.push_back({app.compact ? L"" : g.label, {x, top, x + w, bottom}, g.id, d != nullptr, g.icon});
        x += w + px(2);
    }
    // right-hand cluster, laid out from the right edge
    int rx = width - px(10);
    rx -= px(42); app.buttons.push_back({L"", {rx, top, rx + px(42), bottom}, C_MENU, true, I_MORE});
    rx -= iconW; app.buttons.push_back({L"", {rx, top, rx + iconW, bottom}, C_FIND, d != nullptr, I_SEARCH});
    rx -= sep; g_separators.push_back(rx + sep / 2);
    rx -= px(32); app.buttons.push_back({L"", {rx, top, rx + px(32), bottom}, C_ZOOMIN, d != nullptr, I_ZIN});
    rx -= px(84);
    app.zoomRect = {rx, top + px(3), rx + px(84), bottom - px(3)};
    app.buttons.push_back({L"", {app.zoomRect.right - px(24), app.zoomRect.top, app.zoomRect.right, app.zoomRect.bottom}, C_ZOOMMENU, d != nullptr, I_CHEVRON});
    rx -= px(32); app.buttons.push_back({L"", {rx, top, rx + px(32), bottom}, C_ZOOMOUT, d != nullptr, I_ZOUT});
    rx -= sep; g_separators.push_back(rx + sep / 2);
    rx -= px(32); app.buttons.push_back({L"", {rx, top, rx + px(32), bottom}, C_NEXT, d != nullptr, I_NEXT});
    rx -= ofW; app.ofRect = {rx, top, rx + ofW, bottom};
    rx -= px(50); app.pageRect = {rx, top + px(3), rx + px(50), bottom - px(3)};
    rx -= px(32); app.buttons.push_back({L"", {rx, top, rx + px(32), bottom}, C_PREV, d != nullptr, I_PREV});
    placeBox(app.pageBox, app.pageRect, 0);
    placeBox(app.zoomBox, app.zoomRect, px(24));
    syncTips();
}

// Narrow tabs show their close button only when active or pointed at, leaving the room to the name.
bool closeShown(size_t i) {
    if (i >= app.tabRects.size()) return false;
    const int t = static_cast<int>(i);
    return app.tabRects[i].right - app.tabRects[i].left >= px(110) || t == app.active || app.hover == C_TAB + t || app.hover == C_TABCLOSE + t;
}

// ---- tab colours and icon colours ----
// The tab colours, vivid; each theme shows them lighter or deeper so the names stay readable.
const COLORREF kTabHues[] = {RGB(0xE5, 0x48, 0x4D), RGB(0xF7, 0x6B, 0x15), RGB(0xE8, 0xA3, 0x17), RGB(0x46, 0xA7, 0x58), RGB(0x12, 0xA5, 0x94),
                             RGB(0x05, 0xA2, 0xC2), RGB(0x00, 0x90, 0xFF), RGB(0x3E, 0x63, 0xDD), RGB(0x8E, 0x4E, 0xC6), RGB(0xD6, 0x40, 0x9F),
                             RGB(0xAD, 0x7F, 0x58), RGB(0x6E, 0x8B, 0x3D)};
constexpr int kTabHueCount = static_cast<int>(sizeof(kTabHues) / sizeof(kTabHues[0]));

// A colour for a new tab: one no open tab has (else one the fewest have), never the one given last, picked at random.
int pickTabHue() {
    static int last = -1;
    static bool seeded = false;
    if (!seeded) { std::srand(static_cast<unsigned>(GetTickCount())); seeded = true; }
    int uses[kTabHueCount] = {};
    for (const auto& t : app.tabs) if (t->tabHue >= 0 && t->tabHue < kTabHueCount) ++uses[t->tabHue];
    int least = 1 << 30;
    for (int h = 0; h < kTabHueCount; ++h) if (h != last) least = std::min(least, uses[h]);
    std::vector<int> pick;
    for (int h = 0; h < kTabHueCount; ++h) if (h != last && uses[h] == least) pick.push_back(h);
    last = pick[static_cast<size_t>(std::rand()) % pick.size()];
    return last;
}

// the darker or the lighter of two text colours, whichever reads better on `fill`
COLORREF readableOn(COLORREF fill) {
    const COLORREF dark = RGB(0x14, 0x16, 0x1A), light = RGB(0xFF, 0xFF, 0xFF);
    return contrast(dark, fill) >= contrast(light, fill) ? dark : light;
}

struct TabLook { COLORREF fill, text, strip; };
TabLook tabLook(Document& d, bool active, bool hover) {
    if (d.tabHue < 0 || d.tabHue >= kTabHueCount) d.tabHue = pickTabHue();
    const COLORREF base = kTabHues[d.tabHue];
    const Palette& P = pal();
    COLORREF fill;
    if (P.dark) fill = active ? mixColour(base, RGB(0, 0, 0), 0.35) : mixColour(base, P.tabBar, 0.62);
    else fill = active ? mixColour(base, RGB(0xFF, 0xFF, 0xFF), 0.55) : mixColour(base, P.tabBar, 0.72);
    if (hover && !active) fill = mixColour(fill, P.text, 0.08);
    return {fill, readableOn(fill), P.dark ? mixColour(base, RGB(0xFF, 0xFF, 0xFF), 0.25) : base};
}

// A toolbar icon's own colour (Open amber, Save blue, Undo teal...), made to stand out from `bg` (3:1 at least).
COLORREF iconColour(int icon, COLORREF bg) {
    COLORREF c;
    switch (icon) {
        case icons::Open: c = RGB(0xE0, 0x9B, 0x12); break;
        case icons::Save: c = RGB(0x1F, 0x6F, 0xEB); break;
        case icons::Undo: case icons::Redo: c = RGB(0x0F, 0x9D, 0x8A); break;
        case icons::Edit: c = RGB(0x6D, 0x4A, 0xFF); break;
        case icons::Annotate: c = RGB(0xE5, 0x53, 0x2D); break;
        case icons::Comment: c = RGB(0x1E, 0x9E, 0x50); break;
        case icons::Geometry: c = RGB(0xC2, 0x36, 0x7E); break;
        case icons::SideBySide: c = RGB(0x0E, 0x8F, 0xB3); break;
        case icons::ZoomIn: case icons::ZoomOut: c = RGB(0x54, 0x63, 0xD6); break;
        case icons::Search: c = RGB(0x1F, 0x6F, 0xEB); break;
        default: return pal().text;   // arrows, More: in the text colour
    }
    if (pal().dark) c = mixColour(c, RGB(0xFF, 0xFF, 0xFF), 0.3);   // lighter on dark toolbars
    for (int i = 0; i < 6 && contrast(c, bg) < 3.0; ++i) c = mixColour(c, pal().text, 0.25);
    return c;
}

// a box with rounded top corners, open at the bottom (a tab joined to the toolbar under it)
void tabShape(HDC dc, const RECT& r, COLORREF fill) {
    const int rad = px(9);
    HRGN clip = CreateRectRgn(r.left, r.top, r.right, r.bottom);
    SaveDC(dc);
    ExtSelectClipRgn(dc, clip, RGN_AND);
    HBRUSH b = CreateSolidBrush(fill);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + rad * 2, rad * 2, rad * 2);
    SelectObject(dc, ob); SelectObject(dc, op);
    RestoreDC(dc, -1);
    DeleteObject(b); DeleteObject(clip);
}

void roundFill(HDC dc, const RECT& r, int radius, COLORREF fill, COLORREF border, bool withBorder) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN pen = withBorder ? CreatePen(PS_SOLID, 1, border) : nullptr;
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen ? static_cast<HGDIOBJ>(pen) : GetStockObject(NULL_PEN));
    RoundRect(dc, r.left, r.top, r.right + (pen ? 0 : 1), r.bottom + (pen ? 0 : 1), radius, radius);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b);
    if (pen) DeleteObject(pen);
}

void hline(HDC dc, int x1, int x2, int y, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ op = SelectObject(dc, pen);
    MoveToEx(dc, x1, y, nullptr); LineTo(dc, x2, y);
    SelectObject(dc, op); DeleteObject(pen);
}

void vline(HDC dc, int x, int y1, int y2, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ op = SelectObject(dc, pen);
    MoveToEx(dc, x, y1, nullptr); LineTo(dc, x, y2);
    SelectObject(dc, op); DeleteObject(pen);
}

// minimise, maximise / restore and close, drawn as thin crisp lines like Windows' own
void captionGlyph(HDC dc, const RECT& r, int id, COLORREF c) {
    const int g = px(10), cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, x0 = cx - g / 2, y0 = cy - g / 2;
    HPEN pen = CreatePen(PS_SOLID, std::max(1, px(1)), c);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    if (id == C_CAPMIN) { MoveToEx(dc, x0, cy, nullptr); LineTo(dc, x0 + g, cy); }
    else if (id == C_CAPMAX) {
        if (IsZoomed(app.wnd)) {   // restore: two overlapping windows
            const int o = px(2);
            Rectangle(dc, x0, y0 + o, x0 + g - o + 1, y0 + g + 1);
            MoveToEx(dc, x0 + o, y0 + o, nullptr); LineTo(dc, x0 + o, y0); LineTo(dc, x0 + g, y0); LineTo(dc, x0 + g, y0 + g - o); LineTo(dc, x0 + g - o, y0 + g - o);
        } else Rectangle(dc, x0, y0, x0 + g + 1, y0 + g + 1);
    } else {
        MoveToEx(dc, x0, y0, nullptr); LineTo(dc, x0 + g + 1, y0 + g + 1);
        MoveToEx(dc, x0 + g, y0, nullptr); LineTo(dc, x0 - 1, y0 + g + 1);
    }
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(pen);
}

void paintHeader(HDC dc, const RECT& client) {
    layoutHeader(dc, client.right);
    syncBoxes(false);
    const Palette& P = pal();
    const int th = tabH();
    RECT tr{0, 0, client.right, th}, br{0, th, client.right, headerH()};
    HBRUSH tabBg = CreateSolidBrush(P.tabBar), toolBg = CreateSolidBrush(P.toolbar);
    FillRect(dc, &tr, tabBg); FillRect(dc, &br, toolBg);
    if (app.findVisible) {   // the find bar sits between the toolbar and the pages
        RECT fr{0, headerH(), client.right, headerH() + findH()};
        FillRect(dc, &fr, toolBg);
        hline(dc, 0, client.right, headerH() + findH() - 1, P.line);
    }
    if (app.readVisible && !app.fullscreen) {   // the reading controls sit above the status bar
        const int b = client.bottom - statusH();
        RECT rr{0, b - findH(), client.right, b};
        FillRect(dc, &rr, toolBg);
        hline(dc, 0, client.right, b - findH(), P.line);
    }
    DeleteObject(tabBg); DeleteObject(toolBg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, app.font);

    // the tab row
    if (HICON ic = reinterpret_cast<HICON>(GetClassLongPtrW(app.wnd, GCLP_HICONSM)))
        DrawIconEx(dc, app.iconRect.left, app.iconRect.top, ic, app.iconRect.right - app.iconRect.left, app.iconRect.bottom - app.iconRect.top, 0, nullptr, DI_NORMAL);
    const COLORREF capText = app.windowActive ? P.text : P.textDim;
    if (app.tabs.empty()) {
        SetTextColor(dc, capText);
        RECT r{app.iconRect.right + px(10), 0, app.plusRect.left, th};
        DrawTextW(dc, kAppName, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    hline(dc, 0, client.right, th - 1, P.line);   // under the tab row; the active tab opens into the toolbar
    for (size_t i = 0; i < app.tabRects.size(); ++i) {
        const RECT& r = app.tabRects[i];
        const bool act = static_cast<int>(i) == app.active;
        const bool hov = app.hover == C_TAB + static_cast<int>(i) || app.hover == C_TABCLOSE + static_cast<int>(i);
        COLORREF textCol = act ? P.text : P.textDim;
        if (app.tabColours) {   // each tab in its own colour; the active one deeper, with a strip along its foot
            const TabLook look = tabLook(*app.tabs[i], act, hov);
            tabShape(dc, RECT{r.left, r.top, r.right - px(1), act ? r.bottom : r.bottom - 1}, look.fill);
            if (act) { RECT st{r.left + px(8), r.bottom - px(3), r.right - px(9), r.bottom}; HBRUSH sb = CreateSolidBrush(look.strip); FillRect(dc, &st, sb); DeleteObject(sb); }
            textCol = look.text;
        } else if (act) {
            tabShape(dc, r, P.toolbar);
            vline(dc, r.left, r.top + px(9), r.bottom, P.line);
            vline(dc, r.right - 1, r.top + px(9), r.bottom, P.line);
        } else if (hov) tabShape(dc, RECT{r.left, r.top, r.right, r.bottom - 1}, P.tabIdle);
        else if (i + 1 < app.tabRects.size() && static_cast<int>(i + 1) != app.active)
            vline(dc, r.right + px(1), r.top + px(8), r.bottom - px(8), P.line);   // between two idle tabs
        const int w = r.right - r.left;
        const bool showClose = closeShown(i);
        RECT t{r.left + px(w < px(64) ? 6 : 14), r.top, showClose ? app.closeRects[i].left - px(6) : r.right - px(8), r.bottom};
        SelectObject(dc, act ? app.fontBold : app.font);
        SetTextColor(dc, textCol);
        if (!(showClose && w < px(64)))   // a tab only wide enough for its button shows the button alone
            DrawTextW(dc, tabLabel(*app.tabs[i]).c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        const bool closeHot = app.hover == C_TABCLOSE + static_cast<int>(i);
        if (showClose) icons::closeBadge(dc, app.closeRects[i], closeHot ? RGB(0xF0, 0x3E, 0x3E) : RGB(0xD9, 0x30, 0x30));
    }
    SelectObject(dc, app.font);
    {
        const bool hov = app.hover == C_NEWTAB;
        if (hov) roundFill(dc, app.plusRect, px(8), P.tabIdle, 0, false);
        icons::draw(dc, app.plusRect, I_PLUS, hov ? P.text : P.textDim, px(18));
    }

    // buttons: the window buttons in the tab row, the rest in the toolbar
    for (const Button& b : app.buttons) {
        const bool hov = b.enabled && app.hover == b.id, down = app.pressed == b.id;
        if (b.caption) {
            if (hov || down) {
                RECT r = b.rc;
                HBRUSH hb = CreateSolidBrush(b.id == C_CAPCLOSE ? (down ? RGB(0x94, 0x1F, 0x14) : RGB(0xC4, 0x2B, 0x1C)) : (down ? P.line : P.tabIdle));
                FillRect(dc, &r, hb); DeleteObject(hb);
            }
            captionGlyph(dc, b.rc, b.id, b.id == C_CAPCLOSE && (hov || down) ? RGB(0xFF, 0xFF, 0xFF) : capText);
            continue;
        }
        const bool hl = b.enabled && highlighted(b.id);
        if (b.id == C_ZOOMMENU) {   // the drop-down part of the zoom box: drawn with the box below
            continue;
        }
        if (down) roundFill(dc, b.rc, px(8), P.line, 0, false);
        else if (hl) roundFill(dc, b.rc, px(8), P.accentSoft, P.accent, false);
        else if (hov) roundFill(dc, b.rc, px(8), P.hover, 0, false);
        const COLORREF col = !b.enabled ? P.textOff : hl ? P.accentText : P.text;
        // colourful icons: each its own colour, made to stand out from what is behind it (pressed, active, hover);
        // disabled ones stay grey so it is clear they cannot be used
        const COLORREF behind = down ? P.line : hl ? P.accentSoft : hov ? P.hover : P.toolbar;
        const COLORREF iconCol = app.colourIcons && b.enabled ? iconColour(b.icon, behind) : col;
        const int ic = px(20);
        if (b.label.empty()) icons::draw(dc, b.rc, b.icon, iconCol, ic);
        else {
            icons::draw(dc, RECT{b.rc.left + px(12), b.rc.top, b.rc.left + px(12) + ic, b.rc.bottom}, b.icon, iconCol, ic);
            RECT t{b.rc.left + px(12) + ic + px(7), b.rc.top, b.rc.right, b.rc.bottom};
            SetTextColor(dc, col);
            DrawTextW(dc, b.label.c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
    for (int sx : g_separators) vline(dc, sx, th + px(14), headerH() - px(14), P.line);

    // the page and zoom boxes (their numbers are edit controls on top)
    Document* d = activeDoc();
    if (d) {
        roundFill(dc, app.pageRect, px(8), P.field, P.line, true);
        roundFill(dc, app.zoomRect, px(8), P.field, P.line, true);
        for (const Button& b : app.buttons)
            if (b.id == C_ZOOMMENU) {
                const bool hov = app.hover == b.id || app.pressed == b.id;
                RECT r{b.rc.left + 1, b.rc.top + 1, b.rc.right - 1, b.rc.bottom - 1};
                if (hov) roundFill(dc, r, px(6), P.hover, 0, false);
                icons::draw(dc, b.rc, I_CHEVRON, P.text, px(16));
            }
        SetTextColor(dc, P.textDim);
        RECT o = app.ofRect; o.left += px(7);
        DrawTextW(dc, (L"of " + std::to_wstring(d->pageCount())).c_str(), -1, &o, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {   // nothing open: the boxes are there, greyed like the buttons beside them
        for (const RECT* r : {&app.pageRect, &app.zoomRect}) {
            roundFill(dc, *r, px(8), P.toolbar, P.line, true);
            SetTextColor(dc, P.textOff);
            DrawTextW(dc, L"\x2013", -1, const_cast<RECT*>(r), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    hline(dc, 0, client.right, headerH() - 1, P.line);   // between the toolbar and the pages
}

// a page size in millimetres, with its name when it is a standard one
std::wstring pageSizeText(const PageSize& ps) {
    const double w = ps.w * 25.4 / 72, h = ps.h * 25.4 / 72;
    static const struct { const wchar_t* name; double w, h; } known[] = {
        {L"A3", 297, 420}, {L"A4", 210, 297}, {L"A5", 148, 210}, {L"Letter", 215.9, 279.4}, {L"Legal", 215.9, 355.6}, {L"Tabloid", 279.4, 431.8}};
    wchar_t t[96];
    swprintf(t, 96, L"%.0f \x00D7 %.0f mm", w, h);
    std::wstring out = t;
    for (const auto& k : known)
        if ((std::fabs(w - k.w) < 1.5 && std::fabs(h - k.h) < 1.5) || (std::fabs(w - k.h) < 1.5 && std::fabs(h - k.w) < 1.5)) {
            out += std::wstring(L" (") + k.name + (w > h ? L", landscape)" : L")");
            break;
        }
    return out;
}

void paintStatus(HDC dc, const RECT& client) {
    const Palette& P = pal();
    RECT r{0, client.bottom - statusH(), client.right, client.bottom};
    HBRUSH bg = CreateSolidBrush(P.toolbar);
    FillRect(dc, &r, bg);
    DeleteObject(bg);
    hline(dc, 0, client.right, r.top, P.line);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, app.fontSmall);
    RECT t{r.left + px(12), r.top + 1, r.right - px(12), r.bottom};
    Document* d = activeDoc();
    if (!d) {
        SetTextColor(dc, P.textDim);
        DrawTextW(dc, L"No document open", -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(dc, L"Feather PDF " FEATHER_VERSION L" " FEATHER_CODENAME, -1, &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    const int page = app.viewer.currentPage();
    std::wstring left = L"Page " + std::to_wstring(page + 1) + L" of " + std::to_wstring(d->pageCount());
    if (page >= 0 && page < d->pageCount()) left += L"   \x00B7   " + pageSizeText(d->pageSize(page));
    if (app.tool != Viewer::Tool::None) left += std::wstring(L"   \x00B7   Tool: ") + toolName(app.tool) + L" (Esc stops)";
    if (app.sbs) left += L"   \x00B7   Side by side";
    SetTextColor(dc, P.textDim);
    DrawTextW(dc, left.c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    const wchar_t* state = d->dirty() ? L"Unsaved changes" : d->path().empty() ? L"Not saved yet" : L"All changes saved";
    SetTextColor(dc, d->dirty() ? P.text : P.textDim);
    DrawTextW(dc, state, -1, &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    if (d->dirty()) {   // a small dot before the words
        const int tw = textWidth(dc, app.fontSmall, state), cy = (r.top + r.bottom) / 2, dot = px(7);
        RECT c{t.right - tw - px(8) - dot, cy - dot / 2, t.right - tw - px(8), cy - dot / 2 + dot};
        HBRUSH ab = CreateSolidBrush(P.accent);
        HGDIOBJ ob = SelectObject(dc, ab), op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, c.left, c.top, c.right + 1, c.bottom + 1);
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(ab);
    }
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

// A press on the other side makes its document the active one, then the press goes on to it (same place on screen).
bool onViewer2Press(UINT msg, WPARAM w, LPARAM l) {
    if (!app.sbs || !app.sbsDoc) return false;
    for (size_t i = 0; i < app.tabs.size(); ++i)
        if (app.tabs[i].get() == app.sbsDoc) {
            switchTab(static_cast<int>(i));
            if (msg == WM_LBUTTONDOWN) PostMessageW(app.viewer.hwnd(), msg, w, l);
            else if (msg == WM_RBUTTONDOWN) SetFocus(app.viewer.hwnd());   // the release opens the menu of the active side
            return true;
        }
    return false;
}

void onViewerChange(void*) {
    app.inlineEd.reposition();
    RECT r{0, 0, 30000, headerH()};
    InvalidateRect(app.wnd, &r, FALSE);
    RECT c; GetClientRect(app.wnd, &c);
    RECT st{0, c.bottom - statusH(), c.right, c.bottom};   // the page number in the status bar
    InvalidateRect(app.wnd, &st, FALSE);
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

// The dots per inch of the screen the window is on (Windows 10 and later report it per monitor).
int windowDpi(HWND h) {
    using Fn = UINT(WINAPI*)(HWND);
    static Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")));
    UINT dpi = fn && h ? fn(h) : 0;
    if (!dpi) { HDC dc = GetDC(nullptr); dpi = static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSY)); ReleaseDC(nullptr, dc); }
    return dpi >= 48 ? static_cast<int>(dpi) : 96;
}

// The window's fonts at its current dots per inch: 10.5 pt text, bold for the active tab, 9 pt in the status bar.
void makeUiFonts() {
    for (HFONT* f : {&app.font, &app.fontBold, &app.fontSmall}) if (*f) { DeleteObject(*f); *f = nullptr; }
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(105, app.dpi, 720);
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    app.font = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_SEMIBOLD;
    app.fontBold = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_NORMAL;
    lf.lfHeight = -MulDiv(90, app.dpi, 720);
    app.fontSmall = CreateFontIndirectW(&lf);
    themeui::setFont(app.font, app.fontBold);
    themeui::setDpi(app.dpi);
}

// The page and zoom boxes: Enter goes there, Esc or leaving the box puts back what the view shows.
void applyBox(HWND box) {
    Document* d = activeDoc();
    wchar_t t[32] = L"";
    GetWindowTextW(box, t, 32);
    if (d && box == app.pageBox) {
        const int n = _wtoi(t);
        if (n >= 1 && n <= d->pageCount()) app.viewer.goToPage(n - 1);
        else notice(L"There is no page " + std::wstring(t) + L": this document has " + std::to_wstring(d->pageCount()) + (d->pageCount() == 1 ? L" page." : L" pages."));
    } else if (d && box == app.zoomBox) {
        const double z = _wtof(t);   // "150", "150%" and "62.5" all work
        if (z > 0) app.viewer.setZoom(std::clamp(z / 100.0, Viewer::kMinZoom, Viewer::kMaxZoom));
        else notice(L"Type the zoom as a percentage, for example 150.");
    }
    SetFocus(app.viewer.hwnd());   // leaving the box shows the page and zoom as they now are
}

LRESULT CALLBACK boxEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_KEYDOWN:
            if (w == VK_RETURN) { applyBox(h); return 0; }
            if (w == VK_ESCAPE) { SetFocus(app.viewer.hwnd()); return 0; }
            break;
        case WM_CHAR:
            if (w == VK_RETURN || w == VK_ESCAPE || w == L'\t') return 0;   // no beep
            break;
        case WM_SETFOCUS: {
            const LRESULT r = CallWindowProcW(app.boxProc, h, m, w, l);
            PostMessageW(h, EM_SETSEL, 0, -1);   // typing replaces the number
            return r;
        }
        case WM_KILLFOCUS: {
            const LRESULT r = CallWindowProcW(app.boxProc, h, m, w, l);
            syncBoxes(true);
            return r;
        }
    }
    return CallWindowProcW(app.boxProc, h, m, w, l);
}

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_CREATE: {
            app.wnd = h;
            app.dpi = windowDpi(h);
            makeUiFonts();
            app.viewer.create(h, app.inst);
            app.inlineEd.create(app.viewer.hwnd(), app.inst);
            Document::setTimerWindow(app.viewer.hwnd());
            app.viewer.setOnChange(onViewerChange, nullptr);
            restartAutosave();
            app.viewer.onEvent = onViewerEvent;
            app.viewer.tableOutlines = tableOutlines;
            app.viewer.onJump = onJump;
            app.viewer.onUri = openUri;
            app.viewer2.create(h, app.inst);
            ShowWindow(app.viewer2.hwnd(), SW_HIDE);
            app.viewer2.setOnChange(onViewer2Change, nullptr);
            app.viewer2.onPress = onViewer2Press;
            app.viewer.setLayout(app.layout, app.cover);
            app.viewer.setStyle(drawColor(), lineWidth());
            app.sidebar.create(h, app.inst);
            app.sidebar.setOnNavigate(onSidebarNavigate, nullptr);
            app.sidebar.onContext = showThumbMenu;
            app.sidebar.onReorder = onThumbReorder;
            app.sidebar.onHistory = [](int n) {   // back (or forward) to the clicked step of the undo history
                Document* d = activeDoc();
                if (!d || n == 0) return;
                HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
                if (app.inlineEd.active()) app.inlineEd.cancel();
                const bool ok = d->stepBack(n);
                SetCursor(old);
                app.sel.page = -1; app.sel.objects.clear(); app.viewer.setObjectSelection(-1, nullptr);
                afterDocReplaced();
                if (!ok) MessageBoxW(app.wnd, L"Some of the steps could not be taken back.", kAppName, MB_OK | MB_ICONWARNING);
            };
            app.findEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_EDIT), app.inst, nullptr);
            app.findPrev = CreateWindowExW(0, L"BUTTON", L"Prev", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_PREV), app.inst, nullptr);
            app.findNext = CreateWindowExW(0, L"BUTTON", L"Next", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_NEXT), app.inst, nullptr);
            app.findCase = CreateWindowExW(0, L"BUTTON", L"Match case", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_CASE), app.inst, nullptr);
            app.findWord = CreateWindowExW(0, L"BUTTON", L"Whole word", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_WORD), app.inst, nullptr);
            app.findWild = CreateWindowExW(0, L"BUTTON", L"Wildcards * ?", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_WILD), app.inst, nullptr);
            app.findNotes = CreateWindowExW(0, L"BUTTON", L"In comments", WS_CHILD | BS_AUTOCHECKBOX, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_NOTES), app.inst, nullptr);
            app.readLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT | SS_ENDELLIPSIS, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_READ_LABEL), app.inst, nullptr);
            {
                const std::pair<HWND*, std::pair<int, const wchar_t*>> rb[] = {{&app.readBack, {ID_READ_BACK, L"<< Back"}}, {&app.readPause, {ID_READ_PAUSE, L"Pause"}},
                    {&app.readFwd, {ID_READ_FWD, L"Forward >>"}}, {&app.readSlower, {ID_READ_SLOWER, L"Slower"}}, {&app.readFaster, {ID_READ_FASTER, L"Faster"}},
                    {&app.readStop, {ID_READ_STOP, L"Stop"}}};
                for (const auto& b : rb) {
                    *b.first = CreateWindowExW(0, L"BUTTON", b.second.second, WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(b.second.first)), app.inst, nullptr);
                    SendMessageW(*b.first, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
                }
                SendMessageW(app.readLabel, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
            }
            app.findCount = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_COUNT), app.inst, nullptr);
            app.findClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_FIND_CLOSE), app.inst, nullptr);
            for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findWord, app.findWild, app.findNotes, app.findCount, app.findClose})
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
            app.editProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(app.findEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(findEditProc)));
            // the page number and zoom in the toolbar can be typed into
            app.pageBox = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_CENTER | ES_AUTOHSCROLL | ES_NUMBER, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_PAGE_BOX), app.inst, nullptr);
            app.zoomBox = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_CENTER | ES_AUTOHSCROLL, 0, 0, 0, 0, h, reinterpret_cast<HMENU>(ID_ZOOM_BOX), app.inst, nullptr);
            for (HWND b : {app.pageBox, app.zoomBox}) {
                SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
                SendMessageW(b, EM_SETLIMITTEXT, 6, 0);
                app.boxProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(b, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(boxEditProc)));
            }
            app.startPage = startpage::create(h, app.inst, {
                [] { openDialog(); },
                [](size_t i) { openRecent(i); },
                [](size_t i) { if (i < app.recent.size()) { app.recent.erase(app.recent.begin() + static_cast<std::ptrdiff_t>(i)); updateEmptyText(); saveSettings(); } },
                [] { app.recent.clear(); updateEmptyText(); saveSettings(); }});
            startpage::setDpi(app.startPage, app.dpi);
            app.tips = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT, h, nullptr, app.inst, nullptr);
            if (app.tips) SendMessageW(app.tips, TTM_SETMAXTIPWIDTH, 0, px(420));
            DragAcceptFiles(h, TRUE);
            SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);   // no caption: the tab row is the title bar
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
                case ID_FIND_CASE: case ID_FIND_WORD: case ID_FIND_WILD: case ID_FIND_NOTES: runSearch(false); break;
                case ID_READ_BACK: case ID_READ_PAUSE: case ID_READ_FWD: case ID_READ_SLOWER: case ID_READ_FASTER: case ID_READ_STOP:
                    readingControl(LOWORD(w)); SetFocus(app.viewer.hwnd()); break;
                case ID_FIND_CLOSE: toggleFind(false); break;
            }
            return 0;
        case kMsgSpeech: readingNews(); return 0;
        case WM_TIMER:
            if (w == TIMER_FIND) { KillTimer(h, TIMER_FIND); runSearch(false); }
            else if (w == TIMER_AUTOSAVE) writeSession();
            return 0;
        case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
            HDC cdc = reinterpret_cast<HDC>(w);
            SetTextColor(cdc, pal().text);
            if (reinterpret_cast<HWND>(l) == app.pageBox || reinterpret_cast<HWND>(l) == app.zoomBox) {   // inside the toolbar's boxes
                static HBRUSH fb = nullptr; static COLORREF fc = CLR_INVALID;
                if (fc != pal().field) { if (fb) DeleteObject(fb); fb = CreateSolidBrush(pal().field); fc = pal().field; }
                SetBkColor(cdc, pal().field);
                return reinterpret_cast<LRESULT>(fb);
            }
            return themeui::ctlColour(m, cdc, reinterpret_cast<HWND>(l));   // the find box and its check box
        }
        case WM_MEASUREITEM:
            if (themeui::measureMenu(reinterpret_cast<MEASUREITEMSTRUCT*>(l))) return TRUE;
            break;
        case WM_DRAWITEM:
            if (themeui::drawMenu(reinterpret_cast<DRAWITEMSTRUCT*>(l))) return TRUE;
            break;
        case WM_SETTINGCHANGE:
            if (app.theme == 2 && l && wcscmp(reinterpret_cast<const wchar_t*>(l), L"ImmersiveColorSet") == 0) applyTheme();
            break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC screenDc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            PaintBuf buf(screenDc, rc, ps.rcPaint);
            if (!app.fullscreen) { paintHeader(buf.dc, rc); paintStatus(buf.dc, rc); }
            if (app.sbs && !app.fullscreen && app.viewer2.hwnd()) {   // an accent line along the active side, in the gap
                RECT a, o;
                GetWindowRect(app.viewer.hwnd(), &a);
                GetWindowRect(app.viewer2.hwnd(), &o);
                MapWindowPoints(nullptr, h, reinterpret_cast<POINT*>(&a), 2);
                MapWindowPoints(nullptr, h, reinterpret_cast<POINT*>(&o), 2);
                HBRUSH gap = CreateSolidBrush(RGB(0x30, 0x30, 0x34)), acc = CreateSolidBrush(RGB(0x2D, 0x7F, 0xF9));
                RECT g = app.sbsStacked ? RECT{a.left, std::min(a.bottom, o.bottom), a.right, std::max(a.top, o.top)} : RECT{std::min(a.right, o.right), a.top, std::max(a.left, o.left), a.bottom};
                FillRect(buf.dc, &g, gap);
                RECT mark = g;
                if (app.sbsStacked) { if (a.top < o.top) mark.bottom = mark.top + 2; else mark.top = mark.bottom - 2; }
                else { if (a.left < o.left) mark.right = mark.left + 2; else mark.left = mark.right - 2; }
                FillRect(buf.dc, &mark, acc);
                DeleteObject(gap); DeleteObject(acc);
            }
            buf.done();
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            // Buttons act when the mouse is released over them (as Windows buttons do). Acting on the press left the
            // release to land on whatever menu item had opened under the pointer.
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            app.pressed = -1;
            if (app.tips) SendMessageW(app.tips, TTM_POP, 0, 0);   // a tip must not sit over the menu that opens
            // Save, a tab, a menu...: text still being typed (on the page, in a form field) is applied first
            if (!finishInput()) return 0;
            UpdateWindow(h);   // the buttons are laid out when painted: Save and Undo are live for this click

            for (size_t i = 0; i < app.closeRects.size(); ++i)
                if (closeShown(i) && hit(app.closeRects[i], x, y)) { app.pressed = -2 - static_cast<int>(i); SetCapture(h); return 0; }
            for (size_t i = 0; i < app.tabRects.size(); ++i)
                if (hit(app.tabRects[i], x, y)) {   // the tab is shown; held down, it can be dragged to another place
                    switchTab(static_cast<int>(i));
                    app.tabDrag = static_cast<int>(i); app.tabDragX = x; app.tabDragging = false;
                    SetCapture(h);
                    return 0;
                }
            if (hit(app.plusRect, x, y)) { app.pressed = C_NEWTAB; SetCapture(h); InvalidateRect(h, nullptr, FALSE); return 0; }
            for (const Button& b : app.buttons)
                if (b.enabled && hit(b.rc, x, y)) { app.pressed = b.id; SetCapture(h); InvalidateRect(h, &b.rc, FALSE); return 0; }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (app.tabDrag >= 0) {
                const bool moved = app.tabDragging;
                const int dragged = app.tabDrag;
                app.tabDrag = -1; app.tabDragging = false;
                if (GetCapture() == h) ReleaseCapture();
                // let go well below the tab row (or outside the window): the tab moves to a window of its own there
                const int ux = GET_X_LPARAM(l), uy = GET_Y_LPARAM(l);
                RECT rc;
                GetClientRect(h, &rc);
                if (moved && app.tabs.size() >= 1 && (uy > headerH() + px(60) || uy < -px(40) || ux < -px(40) || ux > rc.right + px(40))) {
                    POINT at{ux, uy};
                    ClientToScreen(h, &at);
                    openInNewWindow(dragged, &at);
                    return 0;
                }
                if (moved) { InvalidateRect(h, nullptr, FALSE); writeSession(); }
                return 0;
            }
            const int pressed = app.pressed;
            app.pressed = -1;
            if (GetCapture() == h) ReleaseCapture();
            if (pressed == -1) return 0;
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            if (pressed <= -2) {   // a tab's close button: only when released on the same button
                const size_t i = static_cast<size_t>(-2 - pressed);
                if (i < app.closeRects.size() && hit(app.closeRects[i], x, y)) closeTab(static_cast<int>(i));
                return 0;
            }
            if (pressed == C_NEWTAB) {
                InvalidateRect(h, nullptr, FALSE);
                if (hit(app.plusRect, x, y)) runCommand(C_NEWTAB);
                return 0;
            }
            for (const Button& b : app.buttons) {
                if (b.id != pressed) continue;
                InvalidateRect(h, &b.rc, FALSE);
                if (!b.enabled || !hit(b.rc, x, y)) return 0;   // dragged off the button: nothing happens
                RECT scr = b.rc;
                MapWindowPoints(h, nullptr, reinterpret_cast<POINT*>(&scr), 2);
                g_menuExclude = scr;   // its menu must not open over it
                if (b.id == C_CAPMIN) { g_menuExclude = RECT{}; ShowWindow(h, SW_MINIMIZE); return 0; }
                if (b.id == C_CAPMAX) { g_menuExclude = RECT{}; ShowWindow(h, IsZoomed(h) ? SW_RESTORE : SW_MAXIMIZE); return 0; }
                if (b.id == C_CAPCLOSE) { g_menuExclude = RECT{}; PostMessageW(h, WM_CLOSE, 0, 0); return 0; }
                if (b.id == C_ANNOT) showAnnotateMenu(b.rc);
                else if (b.id == C_EDITPDF) showToolsMenu(b.rc);
                else if (b.id == C_COMMENT) showCommentMenu(b.rc);
                else if (b.id == C_GEOM) showGeometryMenu(b.rc);
                else if (b.id == C_SBS) showSideBySideMenu(b.rc);
                else if (b.id == C_ZOOMMENU) showZoomMenu(b.rc);
                else if (b.id == C_MENU) showMainMenu(b.rc);
                else { g_menuExclude = RECT{}; runCommand(b.id); }
                g_menuExclude = RECT{};
                return 0;
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (app.pressed != -1 && reinterpret_cast<HWND>(l) != h) { app.pressed = -1; InvalidateRect(h, nullptr, FALSE); }
            if (app.tabDrag >= 0 && reinterpret_cast<HWND>(l) != h) { app.tabDrag = -1; app.tabDragging = false; }
            break;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l), hv = -1;
            if (app.tabDrag >= 0 && (w & MK_LBUTTON)) {   // a tab dragged along the row: it changes places with the tabs it passes
                if (!app.tabDragging && std::abs(x - app.tabDragX) < px(8)) return 0;
                app.tabDragging = true;
                int to = app.tabDrag;
                for (size_t i = 0; i < app.tabRects.size(); ++i) {
                    const RECT& r = app.tabRects[i];
                    const int mid = (r.left + r.right) / 2;
                    if (static_cast<int>(i) < app.tabDrag && x < mid) { to = static_cast<int>(i); break; }
                    if (static_cast<int>(i) > app.tabDrag && x > mid) to = static_cast<int>(i);
                }
                if (to != app.tabDrag) moveTab(app.tabDrag, to);
                return 0;
            }
            for (const Button& b : app.buttons) if (b.enabled && hit(b.rc, x, y)) hv = b.id;
            for (size_t i = 0; i < app.tabRects.size() && hv == -1; ++i)
                if (hit(app.tabRects[i], x, y)) hv = hit(app.closeRects[i], x, y) ? C_TABCLOSE + static_cast<int>(i) : C_TAB + static_cast<int>(i);
            if (hv == -1 && hit(app.plusRect, x, y)) hv = C_NEWTAB;
            if (hv != app.hover) {
                app.hover = hv;
                RECT r{0, 0, 30000, headerH()};
                InvalidateRect(h, &r, FALSE);
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, h, 0};
                TrackMouseEvent(&tme);
            }
            return 0;
        }
        case WM_MOUSELEAVE: case WM_NCMOUSEMOVE:
            if (app.hover != -1) { app.hover = -1; RECT r{0, 0, 30000, headerH()}; InvalidateRect(h, &r, FALSE); }
            break;
        case WM_NCCALCSIZE:
            if (w && !app.fullscreen) {
                // the usual frame at the sides and the bottom (Windows' resize borders and shadow), but no caption:
                // the tab row is the title bar
                auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(l);
                const RECT before = p->rgrc[0];
                DefWindowProcW(h, m, w, l);
                p->rgrc[0].top = before.top;
                if (IsZoomed(h)) p->rgrc[0].top += p->rgrc[0].left - before.left;   // maximised, the window overhangs the screen by its frame
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            const LRESULT r = DefWindowProcW(h, m, w, l);
            if (r != HTCLIENT || app.fullscreen) return r;
            POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ScreenToClient(h, &p);
            RECT rc; GetClientRect(h, &rc);
            const int edge = px(6);
            if (!IsZoomed(h) && p.y < edge) return p.x < edge * 3 ? HTTOPLEFT : p.x >= rc.right - edge * 3 ? HTTOPRIGHT : HTTOP;
            if (p.y >= tabH()) return HTCLIENT;
            if (hit(app.iconRect, p.x, p.y)) return HTSYSMENU;
            for (const RECT& t : app.tabRects) if (hit(t, p.x, p.y)) return HTCLIENT;
            if (hit(app.plusRect, p.x, p.y)) return HTCLIENT;
            for (const Button& b : app.buttons) if (b.caption && hit(b.rc, p.x, p.y)) return HTCLIENT;
            return HTCAPTION;   // the empty part of the tab row moves the window; a double click maximises it
        }
        case WM_ACTIVATE: {
            app.windowActive = LOWORD(w) != WA_INACTIVE;
            RECT r{0, 0, 30000, tabH()};
            InvalidateRect(h, &r, FALSE);
            break;
        }
        case WM_DPICHANGED: {   // moved to a screen with other scaling: fonts and sizes follow
            app.dpi = LOWORD(w) >= 48 ? LOWORD(w) : 96;
            makeUiFonts();
            for (HWND c : {app.findEdit, app.findPrev, app.findNext, app.findCase, app.findWord, app.findWild, app.findNotes, app.findCount, app.findClose, app.pageBox, app.zoomBox})
                if (c) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(app.font), TRUE);
            startpage::setDpi(app.startPage, app.dpi);
            app.tipKey.clear();
            const RECT* r = reinterpret_cast<const RECT*>(l);
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            relayoutChildren();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<NMHDR*>(l);
            if (app.tips && nm->hwndFrom == app.tips && nm->code == TTN_GETDISPINFOW) {
                auto* di = reinterpret_cast<NMTTDISPINFOW*>(l);
                static std::wstring tip;
                const UINT_PTR id = nm->idFrom;
                if (g_menuOpen) tip.clear();   // a tip asked for while a menu is open is not shown
                else tip = (di->uFlags & TTF_IDISHWND) ? tipFor(reinterpret_cast<HWND>(id) == app.pageBox ? ID_PAGE_BOX : ID_ZOOM_BOX) : tipFor(static_cast<int>(id));
                di->lpszText = const_cast<wchar_t*>(tip.c_str());
                return 0;
            }
            break;
        }
        case WM_RBUTTONUP: {   // a tab's own menu
            const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
            for (size_t i = 0; i < app.tabRects.size() && i < app.tabs.size(); ++i)
                if (hit(app.tabRects[i], x, y)) {
                    POINT pt{x, y};
                    ClientToScreen(h, &pt);
                    showTabMenu(static_cast<int>(i), pt);
                    return 0;
                }
            break;
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
            if (cds->dwData != 1) return FALSE;
            // opened after returning: a password prompt or a message would otherwise keep the launching program waiting
            app.handedOver.emplace_back(static_cast<const wchar_t*>(cds->lpData), cds->cbData / sizeof(wchar_t));
            if (app.handedOver.size() == 1) PostMessageW(h, kMsgOpenHandedOver, 0, 0);
            return TRUE;
        }
        case kMsgUpdateChecked: {
            std::unique_ptr<update::Release> r(reinterpret_cast<update::Release*>(l));
            onUpdateChecked(r.get(), w != 0);
            return 0;
        }
        case kMsgOpenHandedOver: {
            std::vector<std::wstring> files;
            files.swap(app.handedOver);
            for (const auto& f : files) openFile(f);
            return 0;
        }
        case WM_KEYDOWN: {
            const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            Document* d = activeDoc();
            if (ctrl) {
                switch (w) {
                    case 'O': runCommand(C_OPEN); return 0;
                    case 'N': launchWindow({}); return 0;   // a new window of its own
                    case 'L': rotateView(-1); return 0;
                    case 'R': rotateView(1); return 0;
                    case 'E': if (d) setTool(Viewer::Tool::EditText); return 0;
                    case 'Q': PostMessageW(app.wnd, WM_CLOSE, 0, 0); return 0;
                    case 'P': printDocument(); return 0;   // Ctrl+Shift+P (the old print preview) opens the same window
                    case 'D': showProperties(); return 0;
                    case VK_OEM_COMMA: showSettings(SP_APPEARANCE); return 0;
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
                    case 'V':
                        if (d && app.sidebar.thumbsFocused() && !app.sidebar.selection().empty()) pastePages(*d, app.sidebar.selectedPages().back() + 1);
                        else if (d && pageobj::clipboardHasPicture()) pictureAction(PX_PASTE);
                        return 0;
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
            if (w == VK_F1) { openUserGuide(); return 0; }
            if (!d) return 0;
            if (app.presenting && !ctrl) {   // slides: forward and back by whole pages
                switch (w) {
                    case VK_RIGHT: case VK_DOWN: case VK_NEXT: case VK_RETURN: app.viewer.stepPage(1); return 0;
                    case VK_SPACE: app.viewer.stepPage(shift ? -1 : 1); return 0;
                    case VK_LEFT: case VK_UP: case VK_PRIOR: case VK_BACK: app.viewer.stepPage(-1); return 0;
                }
            }
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
                case VK_RIGHT: case VK_LEFT: app.viewer.stepPage(w == VK_RIGHT ? 1 : -1); return 0;
                case VK_NEXT: app.viewer.pageKey(1); return 0;
                case VK_PRIOR: app.viewer.pageKey(-1); return 0;
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
        case WM_GETMINMAXINFO: {   // the toolbar fits (its groups shown as icons), but never more than the screen holds
            MONITORINFO mi{sizeof(mi)};
            GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
            reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize = {std::min<LONG>(px(900), mi.rcWork.right - mi.rcWork.left),
                                                                std::min<LONG>(px(460), mi.rcWork.bottom - mi.rcWork.top)};
            return 0;
        }
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

    // The command line (shared/cli.cpp): every feature that runs without a window, the same as on the Mac.
    if (!files.empty()) {
        const int rc = cli::run(files);
        if (rc >= 0) return rc;
    }
    if (!files.empty() && files[0] == L"--printsheet") {   // --printsheet in.pdf out.png perSheet [grey] : sheet 1 on A4 at 100 dpi, as the preview shows it
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        printing::Layout lay;
        for (int i = 0; i < doc.pageCount(); ++i) lay.pages.push_back(i);
        lay.perSheet = _wtoi(files[3].c_str());
        lay.grey = files.size() > 4 && files[4] == L"grey";
        printing::Paper p;   // A4 portrait at 600 dpi with 1/4 inch margins (landscape for 2 per sheet)
        p.physW = 4961; p.physH = 7016; p.offX = p.offY = 150; p.ppp = 600 / 72.0;
        if (lay.perSheet == 2) { std::swap(p.physW, p.physH); }
        p.areaW = p.physW - 300; p.areaH = p.physH - 300;
        std::vector<unsigned char> px;
        int w, h;
        if (!printing::renderSheet(doc, lay, 0, p, 100.0 / 600, px, w, h)) return 4;
        std::string rep = "sheets=" + std::to_string(printing::sheetCount(lay)) + " size=" + std::to_string(w) + "x" + std::to_string(h) + "\n";
        Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(rep.begin(), rep.end()));
        const bool ok = printing::savePng(px, w, h, files[2]);
        return ok ? 0 : 4;
    }
    // One window: if Feather PDF is already running, give it the files and quit.
    // --menucheck report.txt file.pdf : builds every menu with the file open and reports command numbers used twice (exit 1)
    std::wstring menuCheckOut;
    if (!files.empty() && files[0] == L"--menucheck") {
        if (files.size() < 3) return 2;
        menuCheckOut = files[1];
        app.testMode = true;
        files = {files[2]};
    }
    // "--newwindow [--at x,y] [--handover file path name] files": a window of its own (New window, Open in new window, a tab
    // dragged off the tab row), never handed to the window already open
    bool newWindow = false;
    bool hasAt = false;
    POINT at{};
    std::wstring handFile, handPath, handName;
    if (!files.empty() && files[0] == L"--newwindow") {
        newWindow = true;
        files.erase(files.begin());
        while (!files.empty()) {
            if (files[0] == L"--at" && files.size() >= 2) {
                hasAt = swscanf(files[1].c_str(), L"%ld,%ld", &at.x, &at.y) == 2;
                files.erase(files.begin(), files.begin() + 2);
            } else if (files[0] == L"--handover" && files.size() >= 4) {
                handFile = files[1]; handPath = files[2]; handName = files[3];
                files.erase(files.begin(), files.begin() + 4);
            } else break;
        }
    }
    // the setting "each file in its own window" (read before the window and the other settings exist)
    const bool ownWindows = GetPrivateProfileIntW(L"view", L"ownWindows", 0, settingsPath().c_str()) != 0;
    HWND existing = menuCheckOut.empty() && !newWindow && !(ownWindows && !files.empty()) ? FindWindowW(L"FeatherPdfMain", nullptr) : nullptr;
    if (existing) {
        for (auto& f : files) {
            wchar_t full[MAX_PATH * 2];   // a relative name means this launch's folder, not the running copy's
            if (GetFullPathNameW(f.c_str(), MAX_PATH * 2, full, nullptr)) f = full;
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
    startpage::registerClass(inst);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0));
    wc.hIconSm = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wc.lpszClassName = L"FeatherPdfMain";
    RegisterClassExW(&wc);

    const int sysDpi = windowDpi(nullptr);   // the first size follows the screen's scaling
    HWND w = CreateWindowExW(0, wc.lpszClassName, kAppName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(1240, sysDpi, 96), MulDiv(800, sysDpi, 96), nullptr, nullptr, inst, nullptr);
    loadSettings();
    // the viewers were made before the settings were read: give them the saved page layout now
    app.viewer.setLayout(app.layout, app.cover);
    app.viewer2.setLayout(app.layout, app.cover);
    applyTheme();
    updateEmptyText();
    if (hasAt) {   // a dragged-off tab: the new window where the mouse let go, its tab row under the pointer
        RECT wr;
        GetWindowRect(w, &wr);
        HMONITOR mon = MonitorFromPoint(at, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(mon, &mi);
        const int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
        const int x = std::clamp(static_cast<int>(at.x) - ww / 4, static_cast<int>(mi.rcWork.left), static_cast<int>(std::max(mi.rcWork.left, mi.rcWork.right - ww)));
        const int y = std::clamp(static_cast<int>(at.y) - 16, static_cast<int>(mi.rcWork.top), static_cast<int>(std::max(mi.rcWork.top, mi.rcWork.bottom - wh)));
        SetWindowPos(w, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    } else if (newWindow) {   // a little down and to the right of the window it came from, so both can be seen
        HWND other = nullptr;   // another Feather PDF window (not this new one)
        do other = FindWindowExW(nullptr, other, L"FeatherPdfMain", nullptr); while (other == w);
        if (other) {
            RECT orc;
            GetWindowRect(other, &orc);
            SetWindowPos(w, nullptr, orc.left + 40, orc.top + 40, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    ShowWindow(w, show);
    UpdateWindow(w);
    relayoutChildren();
    if (menuCheckOut.empty() && !newWindow) restoreSession();
    if (!handFile.empty()) {   // a tab handed over by another window, with its unsaved changes
        std::vector<unsigned char> bytes;
        auto doc = std::make_unique<Document>();
        if (platform::readFile(handFile, bytes) && doc->load(std::move(bytes), {}) == OpenResult::Ok) {
            doc->setPath(handPath);
            if (handPath.empty()) doc->setName(handName);
            doc->markModified();
            addDocumentTab(std::move(doc));
        }
        DeleteFileW(handFile.c_str());
    }
    // each file in its own window: the first here, every other one in another new window
    if (ownWindows && files.size() > 1 && menuCheckOut.empty()) {
        for (size_t k = 1; k < files.size(); ++k) {
            wchar_t full[MAX_PATH * 2];
            launchWindow({GetFullPathNameW(files[k].c_str(), MAX_PATH * 2, full, nullptr) ? std::wstring(full) : files[k]});
        }
        files.resize(1);
    }
    for (auto& f : files) openFile(f);
    SetFocus(w);
    if (app.checkUpdates && menuCheckOut.empty()) updater::checkInBackground(w, kMsgUpdateChecked, false);   // quietly, unless there is one
    if (!menuCheckOut.empty()) {
        g_menuCheck.on = true;
        RECT r{0, 0, 10, 10};
        const bool doc = activeDoc() != nullptr;
        showMainMenu(r); showToolsMenu(r); showAnnotateMenu(r); showCommentMenu(r); showGeometryMenu(r);
        showSideBySideMenu(r); showZoomMenu(r); openRecentMenu(POINT{0, 0});
        if (doc) {
            showThumbMenu(0, POINT{0, 0});
            Viewer::Event e;
            e.kind = Viewer::Event::ContextMenu; e.page = 0; e.pts = {{100, 100}}; e.annot = -1; e.screen = {0, 0};
            onViewerEvent(e);                                   // the page's right-click menu
            app.tool = Viewer::Tool::Select; app.sel.page = 0; app.sel.objects = {0};
            onViewerEvent(e);                                   // with an object picked by the Move tool
            app.tool = Viewer::Tool::None; app.sel.page = -1; app.sel.objects.clear();
        }
        g_menuCheck.on = false;
        std::wstring rep = L"menus=" + std::to_wstring(g_menuCheck.menus) + L" items=" + std::to_wstring(g_menuCheck.items) + L" document=" + (doc ? L"yes" : L"no") + L"\n" + g_menuCheck.report +
                           L"--- every item: menu, keys (arrow-key positions, a comma enters a submenu), label\n" + g_menuCheck.listing;
        const int n = WideCharToMultiByte(CP_UTF8, 0, rep.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string u(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, rep.c_str(), -1, u.data(), n, nullptr, nullptr);
        Document::writeFile(menuCheckOut, std::vector<unsigned char>(u.begin(), u.end()));
        DestroyWindow(w);
        return g_menuCheck.report.empty() && doc ? 0 : 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
