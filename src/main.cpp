// Feather PDF: free, lightweight PDF viewer and editor for Windows.
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "document.hpp"
#include "viewer.hpp"
#include "sidebar.hpp"

namespace {

constexpr int kTabH = 34, kToolH = 40, kFindH = 38, kSideW = 230;
enum { ID_FIND_EDIT = 201, ID_FIND_PREV, ID_FIND_NEXT, ID_FIND_CASE, ID_FIND_COUNT, ID_FIND_CLOSE, TIMER_FIND = 1 };
constexpr const wchar_t* kAppName = L"Feather PDF";

struct Button { std::wstring label; RECT rc; int id; bool enabled; };
enum Cmd { C_SAVE = 100, C_UNDO, C_REDO, C_ANNOT, C_OPEN = 1, C_PREV, C_NEXT, C_ZOOMOUT, C_ZOOMIN, C_FITW, C_FITP, C_NEWTAB, C_ACTUAL, C_FIND, C_PANEL, C_GOTO };

struct App {
    HINSTANCE inst = nullptr;
    HWND wnd = nullptr;
    Viewer viewer;
    Sidebar sidebar;
    bool sidebarVisible = false, findVisible = false;
    HWND findEdit = nullptr, findPrev = nullptr, findNext = nullptr, findCase = nullptr, findCount = nullptr, findClose = nullptr;
    int curMatch = -1, matchTotal = 0;
    WNDPROC editProc = nullptr;
    int hlColour = 0, drawColour = 5, widthIdx = 1;
    Viewer::Tool tool = Viewer::Tool::None;
    std::vector<std::unique_ptr<Document>> tabs;   // an empty vector means the "New tab" screen
    int active = -1;
    HFONT font = nullptr, fontBold = nullptr;
    std::vector<Button> buttons;
    std::vector<RECT> tabRects, closeRects;
    RECT plusRect{}, pageRect{};
    int hover = -1;
} app;

Document* activeDoc() { return app.active >= 0 ? app.tabs[app.active].get() : nullptr; }

std::wstring tabLabel(const Document& d) { return (d.dirty() ? L"\u2022 " : L"") + d.title(); }

void relayoutChildren() {
    RECT rc; GetClientRect(app.wnd, &rc);
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
              M_DELETE, M_EDIT_COMMENT, M_COPY, M_HL_COLOUR = 320, M_DRAW_COLOUR = 340, M_WIDTH = 360 };
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


// ---- saving, closing, undo ----

void afterChange(int page) {   // page < 0: everything may have changed
    if (page >= 0) app.viewer.invalidatePage(page); else app.viewer.documentChanged();
    app.sidebar.refresh();
    refresh();
}

void afterDocReplaced() {
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

void onViewerEvent(const Viewer::Event& e) {
    Document* d = activeDoc();
    if (!d) return;
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
                case Viewer::Tool::Line: addArrowOrLine(*d, e, false); break;
                case Viewer::Tool::Arrow: addArrowOrLine(*d, e, true); break;
                case Viewer::Tool::Pen:
                    if (annot::addInk(*d, e.page, {e.pts}, drawColor(), lineWidth())) afterChange(e.page);
                    break;
                default: break;
            }
            break;
        case Viewer::Event::Click: {   // comment tool: place a note
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
            const bool hasSel = app.viewer.hasSelection();
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_COPY, L"Copy\tCtrl+C");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_HIGHLIGHT, L"Highlight\tCtrl+H");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_UNDERLINE, L"Underline\tCtrl+U");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_STRIKE, L"Strikethrough\tCtrl+Shift+K");
            AppendMenuW(m, hasSel ? MF_STRING : MF_GRAYED, M_COMMENT, L"Comment on selected text...\tCtrl+M");
            int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, e.screen.x, e.screen.y, 0, app.wnd, nullptr);
            DestroyMenu(m);
            switch (cmd) {
                case M_EDIT_COMMENT: editComment(*d, e.page, e.annot); break;
                case M_DELETE: deleteAnnotation(*d, e.page, e.annot); break;
                case M_COPY: app.viewer.copySelection(); break;
                case M_HIGHLIGHT: markupSelection(annot::Markup::Highlight, false); break;
                case M_UNDERLINE: markupSelection(annot::Markup::Underline, false); break;
                case M_STRIKE: markupSelection(annot::Markup::Strikethrough, false); break;
                case M_COMMENT: markupSelection(annot::Markup::Highlight, true); break;
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
        DrawTextW(dc, tabLabel(*app.tabs[i]).c_str(), -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
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
        app.pageRect = pr;
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
            app.viewer.setOnChange(onViewerChange, nullptr);
            app.viewer.onEvent = onViewerEvent;
            app.viewer.setStyle(drawColor(), lineWidth());
            app.sidebar.create(h, app.inst);
            app.sidebar.setOnNavigate(onSidebarNavigate, nullptr);
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
            if (activeDoc() && hit(app.pageRect, x, y)) { runCommand(C_GOTO); return 0; }
            for (const Button& b : app.buttons)
                if (b.enabled && hit(b.rc, x, y)) {
                    if (b.id == C_ANNOT) showAnnotateMenu(b.rc); else runCommand(b.id);
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
                    case 'S': if (shift) { if (d) saveAsDialog(*d); } else saveActive(); return 0;
                    case 'Z': undoRedo(shift); return 0;
                    case 'Y': undoRedo(true); return 0;
                    case 'H': markupSelection(annot::Markup::Highlight, false); return 0;
                    case 'U': markupSelection(annot::Markup::Underline, false); return 0;
                    case 'K': if (shift) markupSelection(annot::Markup::Strikethrough, false); return 0;
                    case 'M':
                        if (app.viewer.hasSelection()) markupSelection(annot::Markup::Highlight, true);
                        else if (d) setTool(Viewer::Tool::Comment);
                        return 0;
                    case 'F': runCommand(C_FIND); return 0;
                    case 'G': runCommand(C_GOTO); return 0;
                    case 'C': app.viewer.copySelection(); return 0;
                    case 'A': app.viewer.selectAll(); return 0;
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
                case VK_F3: stepMatch(shift ? -1 : 1); return 0;
                case VK_DELETE: deleteSelectedAnnotation(); return 0;
                case VK_ESCAPE:
                    if (app.findVisible) toggleFind(false);
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
            reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize = {980, 420};   // the toolbar needs this much room
            return 0;
        case WM_CLOSE:
            for (auto& t : app.tabs) if (!confirmClose(*t)) return 0;
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
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
