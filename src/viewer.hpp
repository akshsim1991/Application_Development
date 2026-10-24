#pragma once
#include <windows.h>
#include <map>
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

    struct Cached { HBITMAP bmp; int w, h; double zoom; };
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
    int anchorPage_ = 0, anchorChar_ = 0;
    void (*onChange_)(void*) = nullptr;
    void* onChangeCtx_ = nullptr;
    static constexpr int kGap = 12, kMargin = 12;
};
