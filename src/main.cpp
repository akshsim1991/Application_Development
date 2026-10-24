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
#include "theme.hpp"
#include <dwmapi.h>
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
enum { ID_FIND_EDIT = 201, ID_FIND_PREV, ID_FIND_NEXT, ID_FIND_CASE, ID_FIND_COUNT, ID_FIND_CLOSE, TIMER_FIND = 1 };
constexpr const wchar_t* kAppName = L"Feather PDF";

struct Button { std::wstring label; RECT rc; int id; bool enabled; };
enum Cmd { C_SAVE = 100, C_UNDO, C_REDO, C_ANNOT, C_PAGES, C_TOOLS, C_OPEN = 1, C_PREV, C_NEXT, C_ZOOMOUT, C_ZOOMIN, C_FITW, C_FITP, C_NEWTAB, C_ACTUAL, C_FIND, C_PANEL, C_GOTO, C_MENU };

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
    HBRUSH ctlBrush = nullptr;
} app;

Document* activeDoc() { return app.active >= 0 ? app.tabs[app.active].get() : nullptr; }

std::wstring tabLabel(const Document& d) { return (d.dirty() ? L"\u2022 " : L"") + d.title(); }

void applyTheme();

void relayoutChildren() {
    RECT rc; GetClientRect(app.wnd, &rc);
    if (app.fullscreen) {   // only the pages, edge to edge
        MoveWindow(app.viewer.hwnd(), 0, 0, rc.right, rc.bottom, TRUE);
        MoveWindow(app.sidebar.hwnd(), 0, 0, 0, 0, FALSE);
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
    MoveWindow(app.viewer.hwnd(), left, top, std::max<LONG>(0, rc.right - left), std::max<LONG>(0, rc.bottom - top), TRUE);
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

void closeTab(int i) {
    if (i < 0 || i >= static_cast<int>(app.tabs.size())) return;
    if (!confirmClose(*app.tabs[i])) return;
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
              P_DELETE = 400, P_BLANK, P_ROT_R, P_ROT_L, P_ROT_ALL, P_MOVE, P_EXTRACT, P_INSERT, P_MERGE, P_CROP, M_HL_COLOUR = 320, M_DRAW_COLOUR = 340, M_WIDTH = 360 };
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
    app.textStyle.family = static_cast<pageobj::Family>(std::clamp(num(L"textFont", 0), 0, 2));
    app.textStyle.bold = num(L"textBold", 0) != 0;
    app.textStyle.italic = num(L"textItalic", 0) != 0;
    app.textStyle.sizePt = std::clamp(num(L"textSize", 14), 6, 144);
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
    put(L"view", L"textFont", static_cast<int>(app.textStyle.family));
    put(L"view", L"textBold", app.textStyle.bold);
    put(L"view", L"textItalic", app.textStyle.italic);
    put(L"view", L"textSize", static_cast<int>(app.textStyle.sizePt));
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

bool saveDocument(Document& d, const std::wstring& path) {
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
                if (pageobj::addPicture(*d, e.page, e.pts.front().x, e.pts.front().y, app.pendingPicture, err)) afterChange(e.page);
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
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_COPY, L"Copy\tCtrl+C");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_HIGHLIGHT, L"Highlight\tCtrl+H");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_UNDERLINE, L"Underline\tCtrl+U");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_STRIKE, L"Strikethrough\tCtrl+Shift+K");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_COMMENT, L"Comment on selected text...\tCtrl+M");
            if (e.page >= 0) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, M_COPY_PAGE, L"Copy this page as a picture"); }
            int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, e.screen.x, e.screen.y, 0, app.wnd, nullptr);
            DestroyMenu(m);
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

void showAnnotateMenu(const RECT& anchor) {
    HMENU m = CreatePopupMenu();
    auto item = [&](UINT id, const wchar_t* text, bool checked = false) {
        AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0), id, text);
    };
    item(M_HIGHLIGHT, L"Highlight selected text\tCtrl+H");
    item(M_UNDERLINE, L"Underline selected text\tCtrl+U");
    item(M_STRIKE, L"Strikethrough selected text\tCtrl+Shift+K");
    item(M_SQUIGGLY, L"Squiggly underline selected text");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    item(M_COMMENT, L"Add comment\tCtrl+M", app.tool == Viewer::Tool::Comment);
    item(M_RECT, L"Rectangle", app.tool == Viewer::Tool::Rectangle);
    item(M_ELLIPSE, L"Ellipse", app.tool == Viewer::Tool::Ellipse);
    item(M_LINE, L"Line", app.tool == Viewer::Tool::Line);
    item(M_ARROW, L"Arrow", app.tool == Viewer::Tool::Arrow);
    item(M_PEN, L"Pen (freehand)", app.tool == Viewer::Tool::Pen);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU hl = CreatePopupMenu(), dc = CreatePopupMenu(), wd = CreatePopupMenu();
    for (int i = 0; i < 7; ++i) {
        AppendMenuW(hl, MF_STRING | (app.hlColour == i ? MF_CHECKED : 0), M_HL_COLOUR + i, kPaletteNames[i]);
        AppendMenuW(dc, MF_STRING | (app.drawColour == i ? MF_CHECKED : 0), M_DRAW_COLOUR + i, kPaletteNames[i]);
    }
    for (int i = 0; i < 3; ++i) AppendMenuW(wd, MF_STRING | (app.widthIdx == i ? MF_CHECKED : 0), M_WIDTH + i, kWidthNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(hl), L"Highlight and note colour");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(dc), L"Drawing colour");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(wd), L"Line width");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    int a, b;
    AppendMenuW(m, app.viewer.selectedAnnot(a, b) ? MF_STRING : MF_GRAYED, M_DELETE, L"Delete selected annotation\tDel");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);   // also destroys the submenus
    if (cmd >= M_HL_COLOUR && cmd < M_HL_COLOUR + 7) { app.hlColour = cmd - M_HL_COLOUR; return; }
    if (cmd >= M_DRAW_COLOUR && cmd < M_DRAW_COLOUR + 7) { app.drawColour = cmd - M_DRAW_COLOUR; applyStyle(); return; }
    if (cmd >= M_WIDTH && cmd < M_WIDTH + 3) { app.widthIdx = cmd - M_WIDTH; applyStyle(); return; }
    switch (cmd) {
        case M_HIGHLIGHT: markupSelection(annot::Markup::Highlight, false); break;
        case M_UNDERLINE: markupSelection(annot::Markup::Underline, false); break;
        case M_STRIKE: markupSelection(annot::Markup::Strikethrough, false); break;
        case M_SQUIGGLY: markupSelection(annot::Markup::Squiggly, false); break;
        case M_COMMENT: if (app.viewer.hasSelection()) markupSelection(annot::Markup::Highlight, true); else setTool(Viewer::Tool::Comment); break;
        case M_RECT: setTool(Viewer::Tool::Rectangle); break;
        case M_ELLIPSE: setTool(Viewer::Tool::Ellipse); break;
        case M_LINE: setTool(Viewer::Tool::Line); break;
        case M_ARROW: setTool(Viewer::Tool::Arrow); break;
        case M_PEN: setTool(Viewer::Tool::Pen); break;
        case M_DELETE: deleteSelectedAnnotation(); break;
    }
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
    PRINTDLGW pd{sizeof(pd)};
    pd.hwndOwner = app.wnd;
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOSELECTION | PD_ALLPAGES;
    pd.nMinPage = 1; pd.nMaxPage = static_cast<WORD>(std::min(d->pageCount(), 65535));
    pd.nFromPage = 1; pd.nToPage = pd.nMaxPage;
    pd.nCopies = 1;
    if (!PrintDlgW(&pd)) {
        if (CommDlgExtendedError() == PDERR_NODEFAULTPRN || CommDlgExtendedError() == PDERR_PRINTERNOTFOUND)
            MessageBoxW(app.wnd, L"No printer is installed. Add one in Windows Settings > Bluetooth & devices > Printers & scanners, then try again.", kAppName, MB_OK | MB_ICONINFORMATION);
        return;
    }
    HDC dc = pd.hDC;
    int from = 1, to = d->pageCount();
    if (pd.Flags & PD_PAGENUMS) { from = std::max<int>(1, pd.nFromPage); to = std::min<int>(d->pageCount(), pd.nToPage); }
    DOCINFOW di{sizeof(di)};
    std::wstring name = d->title();
    di.lpszDocName = name.c_str();
    bool ok = StartDocW(dc, &di) > 0;
    const int areaW = GetDeviceCaps(dc, HORZRES), areaH = GetDeviceCaps(dc, VERTRES);
    for (int p = from - 1; ok && p < to; ++p) {
        if (StartPage(dc) <= 0) { ok = false; break; }
        FPDF_PAGE pg = d->page(p);
        if (pg) {
            const PageSize& ps = d->pageSize(p);
            const bool rotate = (ps.w > ps.h) != (areaW > areaH);       // turn landscape pages on portrait paper
            const double pw = rotate ? ps.h : ps.w, ph = rotate ? ps.w : ps.h;
            const double sc = std::min(areaW / pw, areaH / ph);
            const int w = static_cast<int>(pw * sc), h = static_cast<int>(ph * sc);
            pdfium::api().FPDF_RenderPage(dc, pg, (areaW - w) / 2, (areaH - h) / 2, w, h, rotate ? 1 : 0, FPDF_ANNOT | FPDF_PRINTING);
        }
        if (EndPage(dc) <= 0) ok = false;
    }
    if (ok) EndDoc(dc); else AbortDoc(dc);
    DeleteDC(dc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
    if (!ok) MessageBoxW(app.wnd, L"The document could not be printed. Check that the printer is on and connected, then try again.", kAppName, MB_OK | MB_ICONWARNING);
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
            int n = redact::apply(d);
            SetCursor(old);
            if (n) afterPagesChanged();
            else MessageBoxW(app.wnd, L"The redactions could not be applied.", kAppName, MB_OK | MB_ICONWARNING);
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
    std::vector<FormField> f(4);
    f[0] = {FormField::Note, L"", {}, L"Signs the document with a certificate. The signed copy is saved as a new file, and any change made to it afterwards shows up as an invalid signature."};
    f[1] = {FormField::Choice, L"Certificate:", {L"One of my Windows certificates", L"A certificate file (.pfx / .p12)"}, L"", 0};
    f[2] = {FormField::Text, L"Reason (optional):", {}, L""};
    f[3] = {FormField::Text, L"Place (optional):", {}, L""};
    if (!showForm(app.wnd, app.inst, L"Sign with a certificate", f, L"Choose certificate...")) return;

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
    bool ok = d.serialize(plain, FPDF_REMOVE_SECURITY) && sign::signPdf(plain, cert, opt, signedBytes, err);
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

void showToolsMenu(const RECT& anchor) {
    Document* d = activeDoc();
    if (!d) return;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (app.tool == Viewer::Tool::EditText ? MF_CHECKED : 0), T_EDIT_TEXT, L"Edit text (click on a line)\tCtrl+E");
    AppendMenuW(m, MF_STRING | (app.tool == Viewer::Tool::AddText ? MF_CHECKED : 0), T_ADD_TEXT, L"Add a text box (click on the page)");
    AppendMenuW(m, MF_STRING, T_TEXT_STYLE, L"Style of new text...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_PICTURE, L"Insert a picture...");
    {
        HMENU st = CreatePopupMenu();
        static const struct { const wchar_t* name; COLORREF col; } stamps[] = {
            {L"APPROVED", RGB(0x2E, 0x9E, 0x4A)}, {L"FINAL", RGB(0x25, 0x6F, 0xD1)}, {L"DRAFT", RGB(0xE0, 0x8A, 0x00)}, {L"CONFIDENTIAL", RGB(0xD0, 0x2B, 0x2B)},
            {L"NOT APPROVED", RGB(0xD0, 0x2B, 0x2B)}, {L"FOR COMMENT", RGB(0x25, 0x6F, 0xD1)}, {L"REVIEWED", RGB(0x2E, 0x9E, 0x4A)}};
        for (int i = 0; i < 7; ++i) AppendMenuW(st, MF_STRING, T_STAMP + i, stamps[i].name);
        AppendMenuW(st, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(st, MF_STRING, T_STAMP_CUSTOM, L"Custom stamp...");
        AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(st), L"Stamp");
    }
    AppendMenuW(m, MF_STRING, T_NOTES, L"Notes summary (highlights and comments)...");
    AppendMenuW(m, MF_STRING, T_WATERMARK, L"Watermark...");
    AppendMenuW(m, MF_STRING, T_NUMBERS, L"Page numbers...");
    AppendMenuW(m, MF_STRING, T_OCR, L"Recognise text in scanned pages (OCR)...");
    AppendMenuW(m, MF_STRING, T_COMPRESS, L"Compress (make the file smaller)...");
    AppendMenuW(m, MF_STRING, T_PROTECT, L"Password protection...");
    {
        HMENU sg = CreatePopupMenu();
        AppendMenuW(sg, MF_STRING, T_SIGN, L"Sign with a certificate...");
        AppendMenuW(sg, MF_STRING, T_OWNCERT, L"Make my own certificate...");
        std::wstring label = d->signatures().empty() ? L"Show signatures (none in this document)" : L"Show the " + std::to_wstring(d->signatures().size()) + L" signature(s) of this document";
        AppendMenuW(sg, d->signatures().empty() ? MF_GRAYED : MF_STRING, T_SHOWSIGS, label.c_str());
        AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(sg), L"Digital signatures");
    }
    if (d->isProtected()) AppendMenuW(m, MF_STRING, T_UNPROTECT, L"Remove password protection");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_EXPORT, L"Export as pictures, text, Word or Excel...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU red = CreatePopupMenu();
    AppendMenuW(red, MF_STRING | (app.tool == Viewer::Tool::Redact ? MF_CHECKED : 0), T_RED_AREA, L"Mark areas to redact");
    AppendMenuW(red, MF_STRING, T_RED_SEL, L"Mark selected text");
    AppendMenuW(red, MF_STRING, T_RED_FIND, L"Find and mark...");
    AppendMenuW(red, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(red, d->redactions.empty() ? MF_GRAYED : MF_STRING, T_RED_APPLY, L"Apply redactions...");
    AppendMenuW(red, d->redactions.empty() ? MF_GRAYED : MF_STRING, T_RED_CLEAR, L"Clear marks");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(red), L"Redact (remove sensitive content)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_READ, speech::speaking() ? L"Stop reading aloud\tCtrl+Shift+U" : L"Read aloud\tCtrl+Shift+U");
    HMENU rate = CreatePopupMenu();
    static const wchar_t* rateNames[] = {L"Slow", L"Normal", L"Fast", L"Very fast"};
    static const int rates[] = {-4, 0, 3, 6};
    for (int i = 0; i < 4; ++i) AppendMenuW(rate, MF_STRING | (app.readRate == rates[i] ? MF_CHECKED : 0), T_RATE + i, rateNames[i]);
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(rate), L"Reading speed");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU col = CreatePopupMenu();
    const Viewer::Colours cur = app.viewer.colours();
    AppendMenuW(col, MF_STRING | (cur == Viewer::Colours::Normal ? MF_CHECKED : 0), T_COLOURS, L"Normal");
    AppendMenuW(col, MF_STRING | (cur == Viewer::Colours::Dark ? MF_CHECKED : 0), T_COLOURS + 1, L"Dark (night mode)");
    AppendMenuW(col, MF_STRING | (cur == Viewer::Colours::Dimmed ? MF_CHECKED : 0), T_COLOURS + 2, L"Dimmed (softer white)");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(col), L"Page colours");
    AppendMenuW(m, MF_STRING, T_PRINT, L"Print...\tCtrl+P");
    AppendMenuW(m, MF_STRING, T_PROPS, L"Document properties...\tCtrl+D");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_FULLSCREEN, L"Full screen\tF11");
    AppendMenuW(m, MF_STRING, T_PRESENT, L"Presentation\tF5");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, T_DEFAULT_VIEWER, L"Set as the default PDF viewer...");
    POINT pt{anchor.left, anchor.bottom};
    ClientToScreen(app.wnd, &pt);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, app.wnd, nullptr);
    DestroyMenu(m);
    switch (cmd) {
        case T_WATERMARK: doWatermark(*d); break;
        case T_NUMBERS: doPageNumbers(*d); break;
        case T_EXPORT: doExport(*d); break;
        case T_COMPRESS: doCompress(*d); break;
        case T_OCR: doOcr(*d); break;
        case T_NOTES: notesSummary(*d); break;
        case T_FULLSCREEN: setFullscreen(true, false); break;
        case T_PRESENT: setFullscreen(true, true); break;
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
        case T_ADD_TEXT: setTool(Viewer::Tool::AddText); break;
        case T_TEXT_STYLE: textStyleDialog(); break;
        case T_READ: readAloud(); break;
        case T_PRINT: printDocument(); break;
        case T_PROPS: showProperties(); break;
        case T_PICTURE: startPicture(); break;
        case T_DEFAULT_VIEWER: registerAsViewer(); break;
        default:
            if (cmd >= T_STAMP && cmd < T_STAMP + 7) {
                static const wchar_t* names[] = {L"APPROVED", L"FINAL", L"DRAFT", L"CONFIDENTIAL", L"NOT APPROVED", L"FOR COMMENT", L"REVIEWED"};
                static const COLORREF cols[] = {RGB(0x2E, 0x9E, 0x4A), RGB(0x25, 0x6F, 0xD1), RGB(0xE0, 0x8A, 0x00), RGB(0xD0, 0x2B, 0x2B), RGB(0xD0, 0x2B, 0x2B), RGB(0x25, 0x6F, 0xD1), RGB(0x2E, 0x9E, 0x4A)};
                startStamp(names[cmd - T_STAMP], cols[cmd - T_STAMP]);
                break;
            }
            if (cmd >= T_RED_AREA && cmd <= T_RED_APPLY) { redactionMenuAction(*d, cmd); break; }
            if (cmd >= T_RATE && cmd < T_RATE + 4) { app.readRate = rates[cmd - T_RATE]; if (speech::speaking()) { stopReading(); readAloud(); } }
            else if (cmd >= T_COLOURS && cmd < T_COLOURS + 3) app.viewer.setColours(static_cast<Viewer::Colours>(cmd - T_COLOURS));
            break;
    }
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
    f[0] = {FormField::Note, L"", {}, L"Feather PDF\nLightweight PDF viewer and editor\nVersion 2.13.0\n\nCreated by: Akshaya Simha\n"
            L"Built with care for a faster, simpler document experience.\n\nFree to use and share, always. No subscription, no trial, no licence key.\n\n"
            L"PDF rendering by PDFium; text recognition by Tesseract. See the licenses folder."};
    showForm(app.wnd, app.inst, L"About Feather PDF", f, L"Close");
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
           MM_SETTINGS = MM_THEME + 3, MM_DEFAULT, MM_ABOUT, MM_EXIT, MM_RECENT = 100 };
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* t, bool on = true, bool check = false) {
        AppendMenuW(m, MF_STRING | (on ? 0 : MF_GRAYED) | (check ? MF_CHECKED : 0), id, t); };
    auto sep = [&] { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    const bool has = d != nullptr;
    add(MM_OPEN, L"Open...\tCtrl+O");
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
    add(MM_CLOSE, L"Close tab\tCtrl+W", has);
    sep();
    add(MM_UNDO, L"Undo\tCtrl+Z", has && d->canUndo());
    add(MM_REDO, L"Redo\tCtrl+Y", has && d->canRedo());
    sep();
    add(MM_PRINT, L"Print...\tCtrl+P", has);
    add(MM_PROPS, L"Document properties...\tCtrl+D", has);
    add(MM_READ, L"Read aloud\tCtrl+Shift+U", has);
    add(MM_SELALL, L"Select all\tCtrl+A", has);
    add(MM_COPYPAGE, L"Copy page as image", has);
    sep();
    add(MM_FIRST, L"First page\tHome", has);
    add(MM_LAST, L"Last page\tEnd", has);
    add(MM_GOTO, L"Go to page...\tCtrl+G", has);
    sep();
    add(MM_BM, L"Bookmarks panel\tCtrl+B", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::Bookmarks);
    add(MM_THUMBS, L"Thumbnails panel\tCtrl+Shift+B", has, app.sidebarVisible && app.sidebar.mode() == Sidebar::Mode::Pages);
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
        case MM_SETTINGS: showSettings(); break;
        case MM_DEFAULT: registerAsViewer(); break;
        case MM_ABOUT: showAbout(); break;
        case MM_EXIT: PostMessageW(app.wnd, WM_CLOSE, 0, 0); break;
        default:
            if (cmd >= MM_COL && cmd < MM_COL + 3) app.viewer.setColours(static_cast<Viewer::Colours>(cmd - MM_COL));
            else if (cmd >= MM_THEME && cmd < MM_THEME + 3) { app.theme = cmd - MM_THEME; applyTheme(); saveSettings(); }
            else if (cmd >= MM_RECENT && cmd < MM_RECENT + 100 && static_cast<size_t>(cmd - MM_RECENT) < app.recent.size()) openFile(app.recent[cmd - MM_RECENT]);
    }
}

// ---- header drawing: tabs + toolbar. Text labels only, so nothing depends on icon fonts. ----
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

    auto add = [&](const wchar_t* label, int id, bool enabled, int minW = 0) {
        SIZE sz; GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &sz);
        int w = std::max<int>(sz.cx + 20, minW);
        app.buttons.push_back({label, {x, kTabH + 6, x + w, kTabH + kToolH - 6}, id, enabled});
        x += w + 4;
    };
    x = 8;
    add(L"Open", C_OPEN, true);
    add(L"Save", C_SAVE, d && d->dirty());
    x += 6;
    add(L"Undo", C_UNDO, d && d->canUndo());
    add(L"Redo", C_REDO, d && d->canRedo());
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
    x += 10;
    add(L"Find", C_FIND, d != nullptr);
    add(L"Panel", C_PANEL, d != nullptr);
    x += 10;
    add(L"Annotate", C_ANNOT, d != nullptr);
    add(L"Pages", C_PAGES, d != nullptr);
    add(L"Tools", C_TOOLS, d != nullptr);
    {
        SIZE sz; GetTextExtentPoint32W(dc, L"Menu", 4, &sz);
        int w = sz.cx + 24;
        app.buttons.push_back({L"Menu", {width - w - 8, kTabH + 6, width - 8, kTabH + kToolH - 6}, C_MENU, true});
    }
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
        SetTextColor(dc, b.enabled ? P.text : P.textOff);
        DrawTextW(dc, b.label.c_str(), -1, const_cast<RECT*>(&b.rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    Document* d = activeDoc();
    if (d) {
        SetTextColor(dc, P.text);
        const Button* prev = nullptr; const Button* zo = nullptr;
        for (const Button& b : app.buttons) { if (b.id == C_PREV) prev = &b; if (b.id == C_ZOOMOUT) zo = &b; }
        wchar_t s[64];
        swprintf(s, 64, L"Page %d / %d", app.viewer.currentPage() + 1, d->pageCount());
        RECT pr{prev->rc.right + 2, prev->rc.top, prev->rc.right + 88, prev->rc.bottom};
        app.pageRect = pr;
        DrawTextW(dc, s, -1, &pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        swprintf(s, 64, L"%d%%", static_cast<int>(d->zoom * 100 + 0.5));
        RECT zr{zo->rc.right + 2, zo->rc.top, zo->rc.right + 50, zo->rc.bottom};
        DrawTextW(dc, s, -1, &zr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, P.line);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, 0, kTabH + kToolH - 1, nullptr); LineTo(dc, client.right, kTabH + kToolH - 1);
    SelectObject(dc, old); DeleteObject(pen);
}

void onViewerChange(void*) {
    RECT r{0, 0, 4000, kTabH + kToolH};
    InvalidateRect(app.wnd, &r, FALSE);
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
            app.viewer.onEvent = onViewerEvent;
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
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            if (!app.fullscreen) paintHeader(dc, rc);
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
                    else if (b.id == C_PAGES) showPagesMenu(b.rc);
                    else if (b.id == C_TOOLS) showToolsMenu(b.rc);
                    else if (b.id == C_MENU) showMainMenu(b.rc);
                    else runCommand(b.id);
                    return 0;
                }
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
                    case 'E': if (d) setTool(Viewer::Tool::EditText); return 0;
                    case 'Q': PostMessageW(app.wnd, WM_CLOSE, 0, 0); return 0;
                    case 'P': printDocument(); return 0;
                    case 'D': showProperties(); return 0;
                    case 'U': if (shift) { readAloud(); return 0; } markupSelection(annot::Markup::Underline, false); return 0;
                    case 'S': if (shift) { if (d) saveAsDialog(*d); } else saveActive(); return 0;
                    case 'Z': undoRedo(shift); return 0;
                    case 'Y': undoRedo(true); return 0;
                    case 'H': markupSelection(annot::Markup::Highlight, false); return 0;
                    case 'K': if (shift) markupSelection(annot::Markup::Strikethrough, false); return 0;
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
                case VK_F3: stepMatch(shift ? -1 : 1); return 0;
                case VK_DELETE:
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
            if (GetKeyState(VK_MENU) < 0 && (w == VK_LEFT || w == VK_RIGHT)) return 0;
            break;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize = {1130, 420};   // the toolbar needs this much room
            return 0;
        case WM_CLOSE:
            for (auto& t : app.tabs) if (!confirmClose(*t)) return 0;
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            saveSettings();
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
    for (auto& f : files) openFile(f);
    SetFocus(w);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
