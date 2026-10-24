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
    void setZoom(double zoom);           // 1.0 == 100 %
    void fitWidth();
    void fitPage();
    void goToPage(int index);            // 0-based
    int currentPage() const;
    void scrollBy(int dy);
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
    enum class Tool { None, Comment, Rectangle, Ellipse, Line, Arrow, Pen, Redact, AddText, EditText, Picture };
    struct Event {
        enum Kind { Draw, Click, DoubleClick, ContextMenu } kind = Draw;
        Tool tool = Tool::None;
        int page = 0;
        std::vector<annot::PointD> pts;   // page coordinates; a drag has its start and end, a pen stroke has many
        int annot = -1;                   // annotation under the pointer, if any
        POINT screen{};                   // for ContextMenu
    };
    std::function<void(const Event&)> onEvent;
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
    std::vector<int> widths_, heights_;
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
    int formFieldAt(int cx, int cy, int& page, annot::PointD& pt) const;   // field type under the point, or -1
    int anchorPage_ = 0, anchorChar_ = 0;
    void (*onChange_)(void*) = nullptr;
    void* onChangeCtx_ = nullptr;
    static constexpr int kGap = 12, kMargin = 12;
};
