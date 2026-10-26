#pragma once
#include <windows.h>
#include <functional>
#include <map>
#include "annot.hpp"
#include "document.hpp"

// Child window that shows the pages of one Document in a continuous vertical layout,
// with text selection and highlighting of search matches.
class Viewer {
public:
    static constexpr const wchar_t* kClass = L"FeatherPdfViewer";
    static bool registerClass(HINSTANCE inst);
    HWND create(HWND parent, HINSTANCE inst);
    HWND hwnd() const { return hwnd_; }

    void setDocument(Document* d);       // not owned; nullptr shows the empty state
    void setZoom(double zoom);           // 1.0 == 100 %; the middle of the window stays on the same spot
    void setZoomAt(double zoom, int cx, int cy);   // the document point at client (cx, cy) stays there
    void fitWidth();
    void fitPage();
    void goToPage(int index);            // 0-based
    int currentPage() const;
    void scrollBy(int dy);
    void scrollTo(int y) { if (!doc_) return; doc_->scrollY = y; clampScroll(); updateScrollBars(); InvalidateRect(hwnd_, nullptr, FALSE); }
    // Called after any change of page, zoom, or document, so the toolbar can update.
    void setOnChange(void (*cb)(void*), void* ctx) { onChange_ = cb; onChangeCtx_ = ctx; }

    // Search highlights: all matches in yellow, the current one in orange.
    void setMatches(std::vector<TextHit> hits, int current);
    void setCurrentMatch(int current);   // scrolls it into view

    // Text selection.
    bool hasSelection() const { return sel_.active; }
    void selectAll();
    void clearSelection();
    std::wstring selectedText();
    bool copySelection();                // puts the selected text on the clipboard

    // ---- annotation tools ----
    enum class Tool { None, Comment, Rectangle, Ellipse, Line, Arrow, Pen, Redact, AddText, EditText, Picture, Stamp, Link, CopyArea, CropBox, TableDelete, TablePlace, TableEdit, EditPara, Select, Polygon, Cloud, Callout, FormRadio, FormList, FormDate, FormText, FormCheck, FormCombo, Ruler, Protractor, PicCrop };
    struct Event {
        enum Kind { Draw, Click, DoubleClick, ContextMenu } kind = Draw;
        Tool tool = Tool::None;
        int page = 0;
        std::vector<annot::PointD> pts;   // page coordinates; a drag has its start and end, a pen stroke has many
        int annot = -1;                   // annotation under the pointer, if any
        POINT screen{};                   // for ContextMenu
    };
    std::function<void(const Event&)> onEvent;
    // Called before a mouse press is handled; returning true swallows it (side by side: the press makes this pane the active one).
    std::function<bool(UINT msg, WPARAM w, LPARAM l)> onPress;
    void setTool(Tool t);
    Tool tool() const { return tool_; }
    void setStyle(COLORREF c, double widthPt) { penColor_ = c; penWidth_ = widthPt; }

    enum class Colours { Normal, Dark, Dimmed };
    void setMarks(const std::vector<RedactMark>* marks);   // pending redaction marks to show (not owned)
    void setEmptyText(const std::wstring& t) { emptyText_ = t; InvalidateRect(hwnd_, nullptr, TRUE); }   // shown when no document is open
    void setColours(Colours c);          // page colour filter: normal, night mode, or softened
    Colours colours() const { return colours_; }

    void invalidatePage(int page);       // a page's content changed: redraw it
    void documentChanged();              // the document was replaced (undo/redo): keep position, redraw all
    // The selected text as rectangles per page, in page coordinates (for markup annotations).
    struct PageRects { int page; std::vector<annot::RectD> rects; };
    std::vector<PageRects> selectionRects();
    bool selectedAnnot(int& page, int& index) const;
    bool formFocused() const { return formFocus_; }
    void clearAnnotSelection();
    // A form field being typed in keeps its text to itself until it loses the keyboard: this hands the text to the
    // document (before saving). Null: whatever document is shown.
    void endFormInput(const Document* d = nullptr);

    // Page layout: 0 one page at a time, 1 continuous, 2 two pages side by side (cover = first page alone).
    void setLayout(int layout, bool cover);
    // The next / previous page, or spread in the two-page layout (dir = +1 or -1).
    void stepPage(int dir);
    void pageKey(int dir);           // Page Down / Page Up: one screen, or to the next page when it starts within that screen
    void setPresenting(bool on) { presenting_ = on; wheelAcc_ = 0; }   // a click or a wheel notch turns the page
    int layout() const { return layout_; }
    bool cover() const { return cover_; }
    std::function<void(int from, int to)> onJump;                 // goToPage moved to another page
    std::function<void(const std::wstring&)> onUri;               // a link to a web page was clicked

    // Measuring tools: results stay on screen until cleared.
    struct Measure { int page; annot::PointD a, b, c; bool angle; std::wstring text; };
    void addMeasure(const Measure& m) { measures_.push_back(m); InvalidateRect(hwnd_, nullptr, FALSE); }
    void clearMeasures() { measures_.clear(); InvalidateRect(hwnd_, nullptr, FALSE); }
    const std::vector<Measure>& measures() const { return measures_; }
    bool hasMeasurements() const { return !measures_.empty(); }
    void setShowRulers(bool on, int units) { showRulers_ = on; units_ = units; InvalidateRect(hwnd_, nullptr, FALSE); }
    bool showRulers() const { return showRulers_; }
    void setPending(const annot::PointD* vertex, int page, const annot::PointD* arm) { pendingVertex_ = vertex ? *vertex : annot::PointD{}; hasPending_ = vertex != nullptr; pendingPage_ = page; pendingArm_ = arm ? *arm : annot::PointD{}; hasArm_ = arm != nullptr; InvalidateRect(hwnd_, nullptr, FALSE); }
    bool pageRect(int page, RECT& out) const;
    // A page rectangle (page coordinates) on screen, in this window's client coordinates; and pixels per point at this zoom.
    RECT screenRect(int page, const annot::RectD& r) const { return annotScreenRect(page, r); }
    double pixelsPerPoint() const { return pxPerPt(); }

    // A selected group of page objects (a picture, a paragraph, a table). The viewer lets the user drag it or its corners
    // (aspect ratio kept), snaps to the guide lines (page coordinates) and reports the new rectangle as a Draw event
    // with the tool Select. Pass nullptr to clear the selection.
    void setObjectSelection(int page, const annot::RectD* rect, std::vector<double> guidesX = {}, std::vector<double> guidesY = {});
    bool hasObjectSelection() const { return objSelPage_ >= 0; }                     // a page's screen rectangle

    static constexpr double kMinZoom = 0.1, kMaxZoom = 8.0;

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT, WPARAM, LPARAM);
    void onPaint();
    void relayout();
    void updateScrollBars();
    void clampScroll();
    void notify();
    void clearCache();
    void drawPage(HDC dc, int page, int x, int y, int w, int h, const RECT& clip);
    void drawOverlays(HDC dc, int page, int x, int y, int w, int h, const RECT& clip);
    int pageX(int i) const;
    double pxPerPt() const { return doc_ ? doc_->zoom * 96.0 / 72.0 : 1.0; }
    // Screen rectangles (client coordinates) covering `count` characters from `start` on a page.
    std::vector<RECT> rectsFor(int page, int start, int count) const;
    bool charAt(int cx, int cy, int& page, int& ch, bool nearest) const;
    void extendSelection(int cx, int cy);
    void selectWordAt(int cx, int cy);

    bool pageAt(int cx, int cy, int& page) const;
    bool toPage(int cx, int cy, int page, annot::PointD& out) const;
    RECT annotScreenRect(int page, const annot::RectD& r) const;
    void drawPreview(HDC dc);

    struct Cached { HBITMAP bmp; int w, h; double zoom; int revision; };
    struct Selection { bool active = false; int p1 = 0, c1 = 0, p2 = 0, c2 = 0; };   // anchor .. focus

    HWND hwnd_ = nullptr;
    Document* doc_ = nullptr;
    std::vector<int> tops_;      // page top offsets in pixels (content coordinates)
    std::vector<int> widths_, heights_, xs_;   // xs_: left edge of each page inside the content
    int layout_ = 1; bool cover_ = false;
    struct Anchor { bool valid = false, onPage = false; const Document* doc = nullptr; int cx = 0, cy = 0, page = -1; annot::PointD pt{}; double fx = 0, fy = 0; DWORD ms = 0; };
    Anchor anchor_;                           // the point a wheel zoom keeps under the mouse
    std::vector<std::pair<int, int>> rows_;   // first page and page count of each row (two pages side by side in the two-page layout)
    void showPage(int page, bool atEnd);      // single-page layout: put this page on screen, at its top or its end
    std::vector<Measure> measures_;
    bool showRulers_ = false; int units_ = 0;
    annot::PointD pendingVertex_{}, pendingArm_{}; bool hasPending_ = false, hasArm_ = false; int pendingPage_ = 0;
    void drawMeasuring(HDC dc, int page);
    int objSelPage_ = -1;
    annot::RectD objSel_{}, objCur_{};
    std::vector<double> guideX_, guideY_;
    enum class ObjDrag { None, Move, TL, TR, BL, BR } objDrag_ = ObjDrag::None;
    annot::PointD objDragStart_{};
    double snapX_ = 0, snapY_ = 0; bool snappedX_ = false, snappedY_ = false;
    bool pageUnclamped(int cx, int cy, int page, annot::PointD& out) const;
    void drawObjectSelection(HDC dc);
    ObjDrag objHit(int cx, int cy) const;
    std::vector<annot::PointD> polyPts_; int polyPage_ = -1; POINT polyMouse_{};
    void finishPolygon();
    bool linkAt(int cx, int cy, int& destPage, std::wstring& uri) const;
    int contentW_ = 0, contentH_ = 0;
    std::map<int, Cached> cache_;
    std::vector<TextHit> matches_;
    int currentMatch_ = -1;
    Selection sel_;
    bool dragging_ = false;
    Tool tool_ = Tool::None;
    Colours colours_ = Colours::Normal;
    std::wstring emptyText_ = L"Open a PDF with Ctrl+O, or drop a file here.";
    const std::vector<RedactMark>* marks_ = nullptr;
    void filterPixels(void* bits, int w, int h) const;
    COLORREF penColor_ = RGB(0xE5, 0x39, 0x35);
    double penWidth_ = 2;
    bool drawing_ = false;
    int drawPage_ = 0;
    std::vector<annot::PointD> drawPts_;
    POINT drawStart_{}, drawNow_{};
    int selAnnotPage_ = -1, selAnnotIndex_ = -1;
    bool formFocus_ = false, formCapture_ = false;   // a form field has the keyboard / is being pressed
    int formPage_ = 0;
    bool presenting_ = false;
    int wheelAcc_ = 0;                                // presentation: wheel movement not yet a whole notch
    int formFieldAt(int cx, int cy, int& page, annot::PointD& pt) const;   // field type under the point, or -1
    int anchorPage_ = 0, anchorChar_ = 0;
    void (*onChange_)(void*) = nullptr;
    void* onChangeCtx_ = nullptr;
    static constexpr int kGap = 12, kMargin = 12;
};
