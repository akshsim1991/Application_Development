#include "viewer.hpp"
#include "pagecolours.hpp"
#include "theme.hpp"
#include "themeui.hpp"
#include <windowsx.h>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cwctype>

namespace {
constexpr long long kMaxCachedPixels = 6000000;   // larger pages are drawn in slices, uncached
}

bool Viewer::registerClass(HINSTANCE inst) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClass;
    return RegisterClassExW(&wc) != 0;
}

HWND Viewer::create(HWND parent, HINSTANCE inst) {
    hwnd_ = CreateWindowExW(0, kClass, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                            0, 0, 0, 0, parent, nullptr, inst, this);
    themeui::flatScroll(hwnd_);   // scroll bars in the theme's colours
    return hwnd_;
}

LRESULT CALLBACK Viewer::proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Viewer* self;
    if (m == WM_NCCREATE) {
        self = static_cast<Viewer*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        self->hwnd_ = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Viewer*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    return self ? self->handle(m, w, l) : DefWindowProcW(h, m, w, l);
}

int Viewer::pageX(int i) const {
    RECT rc; GetClientRect(hwnd_, &rc);
    const int base = contentW_ <= rc.right ? (rc.right - contentW_) / 2 : -doc_->scrollX;
    return base + xs_[i];
}

void Viewer::clearCache() {
    for (auto& kv : cache_) DeleteObject(kv.second.bmp);
    cache_.clear();
}

void Viewer::notify() { if (onChange_) onChange_(onChangeCtx_); }

void Viewer::setDocument(Document* d) {
    doc_ = d;
    sel_ = Selection{};
    formFocus_ = formCapture_ = false;
    matches_.clear();
    currentMatch_ = -1;
    clearCache();
    relayout();
    if (doc_) themeui::setScrollPos(hwnd_, SB_VERT, doc_->scrollY);
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::setLayout(int layout, bool cover) {
    const int keep = doc_ ? currentPage() : 0;   // with the old layout, before it changes
    layout_ = layout; cover_ = cover;
    if (!doc_) return;
    if (layout_ == 0) doc_->shownPage = keep;
    clearCache();
    relayout();
    goToPage(keep);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::relayout() {
    tops_.clear(); widths_.clear(); heights_.clear(); xs_.clear(); rows_.clear();
    contentW_ = contentH_ = 0;
    if (!doc_) { updateScrollBars(); return; }
    const int n = doc_->pageCount();
    // rows of pages: one, or two side by side
    if (layout_ == 2) {
        int i = 0;
        if (cover_ && n > 0) { rows_.push_back({0, 1}); i = 1; }
        for (; i < n; i += 2) rows_.push_back({i, std::min(2, n - i)});
    } else for (int i = 0; i < n; ++i) rows_.push_back({i, 1});
    RECT rc; GetClientRect(hwnd_, &rc);
    const bool spreads = layout_ == 2 && n > 1;   // a lone page in the two-page layout keeps its place in a spread
    // two pages: like a book, a left and a right column meet at the gutter; each is as wide as its widest page
    auto columns = [&](auto width, double& left, double& right) {
        left = right = 0;
        for (auto& r : rows_) {
            if (r.second == 2) { left = std::max(left, width(r.first)); right = std::max(right, width(r.first + 1)); }
            else if (cover_ && r.first == 0) right = std::max(right, width(r.first));
            else left = std::max(left, width(r.first));
        }
    };
    if (doc_->fitWidth) {
        double maxW = 1;
        if (spreads) {
            double l, r;
            columns([&](int p) { return static_cast<double>(doc_->pageSize(p).w); }, l, r);
            maxW = std::max(1.0, l + r);
        } else for (auto& r : rows_) maxW = std::max(maxW, static_cast<double>(doc_->pageSize(r.first).w));
        double avail = std::max<LONG>(rc.right, 200) - 2.0 * kMargin - (spreads ? kGap + 4 : 2);   // a little slack: rounded page sizes must not need a sideways scroll bar
        doc_->zoom = std::clamp(avail / (maxW * 96.0 / 72.0), kMinZoom, kMaxZoom);
    }
    const double s = pxPerPt();
    tops_.assign(n, 0); widths_.assign(n, 1); heights_.assign(n, 1); xs_.assign(n, 0);
    for (int i = 0; i < n; ++i) {
        widths_[i] = std::max(1, static_cast<int>(std::lround(doc_->pageSize(i).w * s)));
        heights_[i] = std::max(1, static_cast<int>(std::lround(doc_->pageSize(i).h * s)));
    }
    if (n == 0) { updateScrollBars(); return; }
    if (layout_ == 0) {
        // single page: one page on screen at a time; every page sits at the same place, only the shown one is drawn
        doc_->shownPage = std::clamp(doc_->shownPage, 0, n - 1);
        for (int i = 0; i < n; ++i) contentW_ = std::max(contentW_, widths_[i] + 2 * kMargin);
        for (int i = 0; i < n; ++i) { tops_[i] = kMargin; xs_[i] = (contentW_ - widths_[i]) / 2; }
        contentH_ = heights_[doc_->shownPage] + 2 * kMargin;
        updateScrollBars();
        return;
    }
    double colL = 0, colR = 0;
    if (spreads) {
        columns([&](int p) { return static_cast<double>(widths_[p]); }, colL, colR);
        contentW_ = static_cast<int>(colL + kGap + colR) + 2 * kMargin;
    } else for (auto& r : rows_) contentW_ = std::max(contentW_, widths_[r.first] + 2 * kMargin);
    const int gutterL = kMargin + static_cast<int>(colL), gutterR = gutterL + kGap;   // the inner edges of the two columns
    int y = kMargin;
    for (auto& r : rows_) {
        int rowH = 0;
        for (int k = 0; k < r.second; ++k) rowH = std::max(rowH, heights_[r.first + k]);
        for (int k = 0; k < r.second; ++k) {
            const int p = r.first + k;
            tops_[p] = y;
            if (!spreads) xs_[p] = (contentW_ - widths_[p]) / 2;
            else {
                // a left page ends at the gutter, a right page starts there; a lone cover is a right page, a lone last page a left one
                const bool right = r.second == 2 ? k == 1 : (cover_ && r.first == 0);
                xs_[p] = right ? gutterR : gutterL - widths_[p];
            }
        }
        y += rowH + kGap;
    }
    contentH_ = y - kGap + kMargin;
    updateScrollBars();
}

void Viewer::showPage(int page, bool atEnd) {
    if (!doc_ || tops_.empty()) return;
    doc_->shownPage = std::clamp(page, 0, static_cast<int>(tops_.size()) - 1);
    relayout();
    RECT rc; GetClientRect(hwnd_, &rc);
    doc_->scrollY = atEnd ? std::max(0L, contentH_ - rc.bottom) : 0;
    clampScroll();
    updateScrollBars();
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::stepPage(int dir) {
    if (!doc_ || rows_.empty()) return;
    const int cur = currentPage();
    if (layout_ != 2) { if (cur + dir >= 0 && cur + dir < static_cast<int>(tops_.size())) goToPage(cur + dir); return; }
    size_t r = 0;
    while (r + 1 < rows_.size() && rows_[r + 1].first <= cur) ++r;
    const long next = static_cast<long>(r) + dir;
    if (next < 0 || next >= static_cast<long>(rows_.size())) return;
    goToPage(rows_[static_cast<size_t>(next)].first);
}

void Viewer::pageKey(int dir) {
    if (!doc_ || tops_.empty()) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    const int screen = std::max(40, static_cast<int>(rc.bottom) * 9 / 10);   // a little overlap keeps the reader's place
    if (layout_ == 0) { scrollBy(dir * screen); return; }   // single page: scrolling turns the page at its end
    const int y = doc_->scrollY;
    if (dir > 0) {   // the next page top below the current view, if it is within one screen
        for (size_t i = 0; i < tops_.size(); ++i) {
            const int top = tops_[i] - kMargin;
            if (top > y + 1) { if (top <= y + static_cast<int>(rc.bottom)) { goToPage(static_cast<int>(i)); return; } break; }
        }
    } else {
        for (size_t i = tops_.size(); i-- > 0;) {
            const int top = tops_[i] - kMargin;
            if (top < y - 1) { if (top >= y - static_cast<int>(rc.bottom)) { goToPage(static_cast<int>(i)); return; } break; }
        }
    }
    scrollBy(dir * screen);
}

bool Viewer::pageRect(int page, RECT& out) const {
    if (!doc_ || page < 0 || page >= static_cast<int>(tops_.size())) return false;
    out = {pageX(page), tops_[page] - doc_->scrollY, pageX(page) + widths_[page], tops_[page] - doc_->scrollY + heights_[page]};
    return true;
}

void Viewer::updateScrollBars() {
    RECT rc; GetClientRect(hwnd_, &rc);
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0;
    si.nMax = std::max(0, contentH_ - 1);
    si.nPage = rc.bottom;
    si.nPos = doc_ ? doc_->scrollY : 0;
    themeui::setScrollInfo(hwnd_, SB_VERT, si);
    si.nMax = std::max(0, contentW_ - 1);
    si.nPage = rc.right;
    si.nPos = doc_ ? doc_->scrollX : 0;
    themeui::setScrollInfo(hwnd_, SB_HORZ, si);
}

void Viewer::clampScroll() {
    if (!doc_) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    doc_->scrollY = std::clamp(doc_->scrollY, 0, static_cast<int>(std::max<LONG>(0, contentH_ - rc.bottom)));
    doc_->scrollX = std::clamp(doc_->scrollX, 0, static_cast<int>(std::max<LONG>(0, contentW_ - rc.right)));
}

void Viewer::setZoom(double z) {
    RECT rc; GetClientRect(hwnd_, &rc);
    setZoomAt(z, rc.right / 2, rc.bottom / 2);   // buttons and keys: the middle of the window stays put
}

void Viewer::setZoomAt(double z, int cx, int cy) {
    if (!doc_) return;
    z = std::clamp(z, kMinZoom, kMaxZoom);
    RECT rc; GetClientRect(hwnd_, &rc);
    // What is under (cx, cy): a point on a page, or else a spot of the content area. Steps of one wheel gesture (same
    // pointer position, less than 1.5 s apart) keep the point of the first step, so it does not drift when the page gets
    // narrower than the window and is centred again.
    const DWORD nowMs = GetTickCount();
    const bool sameGesture = anchor_.valid && anchor_.doc == doc_ && anchor_.cx == cx && anchor_.cy == cy && nowMs - anchor_.ms < 1500;
    if (!sameGesture) {
        anchor_ = Anchor{};
        anchor_.onPage = pageAt(cx, cy, anchor_.page) && toPage(cx, cy, anchor_.page, anchor_.pt);
        const int originX = contentW_ <= rc.right ? -(static_cast<int>(rc.right) - contentW_) / 2 : doc_->scrollX;
        anchor_.fx = contentW_ > 0 ? (originX + cx) / static_cast<double>(contentW_) : 0.5;
        anchor_.fy = contentH_ > 0 ? (doc_->scrollY + cy) / static_cast<double>(contentH_) : 0;
        anchor_.doc = doc_; anchor_.cx = cx; anchor_.cy = cy; anchor_.valid = true;
    }
    anchor_.ms = nowMs;
    const bool onPage = anchor_.onPage && anchor_.page < static_cast<int>(tops_.size());
    const int page = anchor_.page;
    const annot::PointD pt = anchor_.pt;
    const double fx = anchor_.fx, fy = anchor_.fy;
    doc_->fitWidth = false;
    doc_->zoom = z;
    clearCache();
    relayout();
    if (onPage) {   // the same page point goes back under (cx, cy)
        const RECT now = annotScreenRect(page, {pt.x, pt.y, pt.x, pt.y});
        doc_->scrollY += now.top - cy;
        if (contentW_ > rc.right) doc_->scrollX += now.left - cx;
    } else {
        doc_->scrollY = static_cast<int>(std::lround(fy * contentH_ - cy));
        doc_->scrollX = static_cast<int>(std::lround(fx * contentW_ - cx));
    }
    clampScroll();
    updateScrollBars();
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::fitWidth() {
    if (!doc_) return;
    doc_->fitWidth = true;
    clearCache();
    relayout();
    clampScroll();
    updateScrollBars();
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::fitPage() {
    if (!doc_ || rows_.empty()) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    const int p = currentPage();
    // the page fits; in the two-page layout its whole spread does, at the width of the book's two columns
    double w = doc_->pageSize(p).w, h = doc_->pageSize(p).h;
    bool two = false;
    if (layout_ == 2 && doc_->pageCount() > 1) {
        double l = 0, r = 0;
        for (const auto& row : rows_) {
            if (row.second == 2) { l = std::max(l, static_cast<double>(doc_->pageSize(row.first).w)); r = std::max(r, static_cast<double>(doc_->pageSize(row.first + 1).w)); }
            else if (cover_ && row.first == 0) r = std::max(r, static_cast<double>(doc_->pageSize(row.first).w));
            else l = std::max(l, static_cast<double>(doc_->pageSize(row.first).w));
            if (p >= row.first && p < row.first + row.second) {
                h = 0;
                for (int k = 0; k < row.second; ++k) h = std::max(h, static_cast<double>(doc_->pageSize(row.first + k).h));
            }
        }
        w = l + r;
        two = true;
    }
    const double zw = (rc.right - 2.0 * kMargin - (two ? kGap + 4 : 2)) / (w * 96.0 / 72.0);
    const double zh = (rc.bottom - 2.0 * kMargin) / (h * 96.0 / 72.0);
    setZoom(std::min(zw, zh));
    goToPage(p);
}

int Viewer::currentPage() const {
    if (!doc_ || tops_.empty()) return 0;
    if (layout_ == 0) return std::clamp(doc_->shownPage, 0, static_cast<int>(tops_.size()) - 1);
    RECT rc; GetClientRect(hwnd_, &rc);
    int probe = doc_->scrollY + rc.bottom / 3;
    int idx = static_cast<int>(std::upper_bound(tops_.begin(), tops_.end(), probe) - tops_.begin()) - 1;
    idx = std::clamp(idx, 0, static_cast<int>(tops_.size()) - 1);
    while (idx > 0 && tops_[idx - 1] == tops_[idx]) --idx;   // the left page of a spread
    return idx;
}

void Viewer::goToPage(int index) {
    if (!doc_ || tops_.empty()) return;
    index = std::clamp(index, 0, static_cast<int>(tops_.size()) - 1);
    const int from = currentPage();
    if (onJump && from != index) onJump(from, index);
    if (layout_ == 0) { doc_->shownPage = index; relayout(); }
    doc_->scrollY = tops_[index] - kMargin;
    clampScroll();
    updateScrollBars();
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

// Sideways (a touchpad swiped left or right, a tilting wheel, Shift + wheel): only when the pages are wider than the view.
void Viewer::scrollByX(int dx) {
    if (!doc_ || !dx) return;
    const int before = doc_->scrollX;
    doc_->scrollX += dx;
    clampScroll();
    if (doc_->scrollX == before) return;
    themeui::setScrollPos(hwnd_, SB_HORZ, doc_->scrollX);
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::scrollBy(int dy) {
    if (!doc_) return;
    if (layout_ == 0 && !tops_.empty()) {   // single page: scrolling past the end of the page turns it
        RECT rc; GetClientRect(hwnd_, &rc);
        const int maxY = std::max(0, static_cast<int>(contentH_ - rc.bottom));
        if (dy > 0 && doc_->scrollY >= maxY && doc_->shownPage + 1 < static_cast<int>(tops_.size())) { showPage(doc_->shownPage + 1, false); return; }
        if (dy < 0 && doc_->scrollY <= 0 && doc_->shownPage > 0) { showPage(doc_->shownPage - 1, true); return; }
    }
    doc_->scrollY += dy;
    clampScroll();
    themeui::setScrollPos(hwnd_, SB_VERT, doc_->scrollY);
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::drawPage(HDC dc, int page, int x, int y, int w, int h, const RECT& clip) {
    const auto& api = pdfium::api();
    const long long pixels = 1LL * w * h;
    RECT page_rc{x, y, x + w, y + h};
    RECT vis;
    if (!IntersectRect(&vis, &page_rc, &clip)) return;

    if (pixels <= kMaxCachedPixels) {
        auto it = cache_.find(page);
        if (it == cache_.end() || it->second.w != w || it->second.h != h || it->second.revision != doc_->revision()) {
            if (it != cache_.end()) { DeleteObject(it->second.bmp); cache_.erase(it); }
            if (cache_.size() >= static_cast<size_t>(cacheLimit_)) {   // drop the page farthest from the current one
                int cur = currentPage(), farthest = cache_.begin()->first;
                for (auto& kv : cache_) if (std::abs(kv.first - cur) > std::abs(farthest - cur)) farthest = kv.first;
                DeleteObject(cache_[farthest].bmp); cache_.erase(farthest);
            }
            BITMAPINFO bi{};
            bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
            void* bits = nullptr;
            HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!bmp) return;
            FPDF_PAGE pg = doc_->page(page);
            FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, bits, w * 4);
            api.FPDFBitmap_FillRect(fb, 0, 0, w, h, 0xFFFFFFFF);
            if (pg) doc_->renderPage(page, fb, 0, 0, w, h);
            api.FPDFBitmap_Destroy(fb);
            filterPixels(bits, w, h, page, 0, 0, w, h);
            it = cache_.emplace(page, Cached{bmp, w, h, doc_->zoom, doc_->revision()}).first;
        }
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, it->second.bmp);
        BitBlt(dc, vis.left, vis.top, vis.right - vis.left, vis.bottom - vis.top, mem, vis.left - x, vis.top - y, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
    } else {
        // Very large page: render only the visible slice.
        int sw = vis.right - vis.left, sh = vis.bottom - vis.top;
        BITMAPINFO bi{};
        bi.bmiHeader = {sizeof(BITMAPINFOHEADER), sw, -sh, 1, 32, BI_RGB};
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp) return;
        FPDF_PAGE pg = doc_->page(page);
        FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(sw, sh, FPDFBitmap_BGRx, bits, sw * 4);
        api.FPDFBitmap_FillRect(fb, 0, 0, sw, sh, 0xFFFFFFFF);
        if (pg) doc_->renderPage(page, fb, x - vis.left, y - vis.top, w, h);
        api.FPDFBitmap_Destroy(fb);
        filterPixels(bits, sw, sh, page, x - vis.left, y - vis.top, w, h);
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(dc, vis.left, vis.top, sw, sh, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(bmp);
    }
}

namespace {
// Fills `r` with a colour at the given opacity (0..255) using a 1x1 bitmap stretched by AlphaBlend.
void fillAlpha(HDC dc, const RECT& r, COLORREF col, BYTE alpha) {
    if (r.right <= r.left || r.bottom <= r.top) return;
    HDC mem = CreateCompatibleDC(dc);
    BITMAPINFO bi{};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), 1, -1, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        *static_cast<DWORD*>(bits) = (GetRValue(col) << 16) | (GetGValue(col) << 8) | GetBValue(col);
        HGDIOBJ old = SelectObject(mem, bmp);
        BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, 0};
        AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, mem, 0, 0, 1, 1, bf);
        SelectObject(mem, old);
    }
    if (bmp) DeleteObject(bmp);
    DeleteDC(mem);
}

// The angle at v between the sides to a and b, in degrees (0-180), and a small arc that marks it.
double angleAt(POINT v, POINT a, POINT b) {
    const double ax = a.x - v.x, ay = a.y - v.y, bx = b.x - v.x, by = b.y - v.y;
    const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
    if (la < 1 || lb < 1) return 0;
    return std::acos(std::clamp((ax * bx + ay * by) / (la * lb), -1.0, 1.0)) * 180.0 / 3.14159265358979;
}

void angleArc(HDC dc, POINT v, POINT a, POINT b) {
    const double la = std::hypot(a.x - v.x, a.y - v.y), lb = std::hypot(b.x - v.x, b.y - v.y);
    if (la < 4 || lb < 4) return;
    const int r = static_cast<int>(std::max(10.0, std::min({36.0, la * 0.45, lb * 0.45})));
    // angles as seen on screen (counter-clockwise, y up); the arc runs counter-clockwise through the smaller opening
    const double ta = std::atan2(-(a.y - v.y), a.x - v.x), tb = std::atan2(-(b.y - v.y), b.x - v.x);
    double d = tb - ta;
    while (d < 0) d += 2 * 3.14159265358979;
    const double from = d <= 3.14159265358979 ? ta : tb, to = d <= 3.14159265358979 ? tb : ta;
    const int sx = v.x + static_cast<int>(std::cos(from) * r), sy = v.y - static_cast<int>(std::sin(from) * r);
    const int ex = v.x + static_cast<int>(std::cos(to) * r), ey = v.y - static_cast<int>(std::sin(to) * r);
    const int old = SetArcDirection(dc, AD_COUNTERCLOCKWISE);
    Arc(dc, v.x - r, v.y - r, v.x + r + 1, v.y + r + 1, sx, sy, ex, ey);
    SetArcDirection(dc, old);
}

// A short note next to the mouse (white box, dark text).
void cursorNote(HDC dc, POINT at, const wchar_t* text, COLORREF col) {
    HFONT f = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ of = SelectObject(dc, f);
    SIZE sz;
    GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &sz);
    RECT r{at.x + 16, at.y + 18, at.x + 16 + sz.cx + 12, at.y + 18 + sz.cy + 6};
    fillAlpha(dc, r, RGB(0xFF, 0xFF, 0xFF), 235);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, col);
    DrawTextW(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, of);
    DeleteObject(f);
}
} // namespace

namespace {
// Pen and touch input (Windows 8 and later), looked up when first needed so older systems still start.
using GetPointerTypeFn = BOOL(WINAPI*)(UINT32, POINTER_INPUT_TYPE*);
using GetPointerPenInfoFn = BOOL(WINAPI*)(UINT32, POINTER_PEN_INFO*);
GetPointerTypeFn pointerType() {
    static GetPointerTypeFn f = reinterpret_cast<GetPointerTypeFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetPointerType")));
    return f;
}
GetPointerPenInfoFn penInfo() {
    static GetPointerPenInfoFn f = reinterpret_cast<GetPointerPenInfoFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetPointerPenInfo")));
    return f;
}
// A mouse message Windows made from a touch (not a real mouse or pen): its extra information carries this signature.
bool fromTouch() { return (GetMessageExtraInfo() & 0xFFFFFF80) == 0xFF515780; }
} // namespace

std::vector<RECT> Viewer::rectsFor(int page, int start, int count) const {
    std::vector<RECT> out;
    if (!doc_ || count <= 0 || page < 0 || page >= static_cast<int>(tops_.size())) return out;
    const auto& api = pdfium::api();
    FPDF_PAGE pg = doc_->page(page);
    FPDF_TEXTPAGE tp = doc_->text(page);
    if (!pg || !tp) return out;
    const int w = widths_[page], h = heights_[page];
    const int ox = pageX(page), oy = tops_[page] - doc_->scrollY;
    int n = api.FPDFText_CountRects(tp, start, count);
    for (int i = 0; i < n; ++i) {
        double l, t, r, b;
        if (!api.FPDFText_GetRect(tp, i, &l, &t, &r, &b)) continue;
        int x1, y1, x2, y2;
        api.FPDF_PageToDevice(pg, 0, 0, w, h, 0, l, t, &x1, &y1);
        api.FPDF_PageToDevice(pg, 0, 0, w, h, 0, r, b, &x2, &y2);
        RECT rc{std::min(x1, x2) + ox, std::min(y1, y2) + oy, std::max(x1, x2) + ox + 1, std::max(y1, y2) + oy + 1};
        out.push_back(rc);
    }
    return out;
}

std::vector<RECT> Viewer::hitRects(const TextHit& h) const {
    if (h.annot < 0) return rectsFor(h.page, h.start, h.count);
    if (!doc_ || h.page < 0 || h.page >= static_cast<int>(tops_.size())) return {};
    RECT r = annotScreenRect(h.page, {h.left, h.bottom, h.right, h.top});
    InflateRect(&r, 4, 4);
    return {r};
}

const std::vector<pageobj::EditBlock>& Viewer::blocksOf(int page) {
    if (blockDoc_ != doc_ || blockRev_ != (doc_ ? doc_->revision() : -1)) { blockCache_.clear(); blockDoc_ = doc_; blockRev_ = doc_ ? doc_->revision() : -1; }
    auto it = blockCache_.find(page);
    if (it == blockCache_.end()) it = blockCache_.emplace(page, doc_ ? pageobj::editableBlocks(*doc_, page) : std::vector<pageobj::EditBlock>{}).first;
    return it->second;
}

int Viewer::editBlockAt(int cx, int cy, int& page, pageobj::EditBlock* out) {
    page = -1;
    annot::PointD pt;
    if (!doc_ || !pageAt(cx, cy, page) || !toPage(cx, cy, page, pt)) return -1;
    const auto& bs = blocksOf(page);
    const double m = 3.0 / pxPerPt();   // the outline is drawn a little outside the block: so is its edge
    int best = -1;
    double bestArea = 1e18;
    for (size_t i = 0; i < bs.size(); ++i) {
        const auto& b = bs[i];
        if (pt.x < b.left - m || pt.x > b.right + m || pt.y < b.bottom - m || pt.y > b.top + m) continue;
        // text over a picture behind it wins; otherwise the smallest
        const double area = (b.right - b.left) * (b.top - b.bottom) * (b.kind == pageobj::EditBlock::Picture ? 4 : 1);
        if (area < bestArea) { bestArea = area; best = static_cast<int>(i); }
    }
    if (best >= 0 && out) *out = bs[static_cast<size_t>(best)];
    return best;
}

void Viewer::drawOverlays(HDC dc, int page, int, int, int, int, const RECT& clip) {
    // edit mode: a light frame round every paragraph and picture, the one under the pointer stronger and tinted
    if (tool_ == Tool::EditAll && doc_ && !(editingNow && editingNow())) {
        const auto& bs = blocksOf(page);
        for (size_t i = 0; i < bs.size(); ++i) {
            const auto& b = bs[i];
            RECT r = annotScreenRect(page, {b.left, b.bottom, b.right, b.top});
            InflateRect(&r, 3, 3);
            RECT v;
            if (!IntersectRect(&v, &r, &clip)) continue;
            const bool hot = page == hoverPage_ && static_cast<int>(i) == hoverBlock_;
            const bool pic = b.kind == pageobj::EditBlock::Picture;
            const COLORREF col = hot ? RGB(0x1F, 0x6F, 0xEB) : pic ? RGB(0xE0, 0x8A, 0x1E) : RGB(0x6C, 0xA6, 0xF0);
            if (hot) fillAlpha(dc, v, col, 22);
            LOGBRUSH lb{BS_SOLID, col, 0};
            const DWORD dash[2] = {5, 3};
            HPEN pen = pic && !hot ? ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 1, &lb, 2, dash) : CreatePen(PS_SOLID, hot ? 2 : 1, col);
            HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            SelectObject(dc, ob); SelectObject(dc, op);
            DeleteObject(pen);
        }
    }
    // the tables found on the page, while a table tool is chosen: a dashed frame and a light tint, so it is clear what can
    // be clicked
    if (tableOutlines && (tool_ == Tool::TableEdit || tool_ == Tool::TableDelete)) {
        const COLORREF col = tool_ == Tool::TableDelete ? RGB(0xD0, 0x30, 0x30) : RGB(0x1F, 0x6F, 0xEB);
        for (const annot::RectD& tr : tableOutlines(page)) {
            RECT r = annotScreenRect(page, tr);
            InflateRect(&r, 4, 4);
            RECT v;
            if (IntersectRect(&v, &r, &clip)) fillAlpha(dc, v, col, 28);
            LOGBRUSH lb{BS_SOLID, col, 0};
            const DWORD dash[2] = {6, 4};
            HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT, 2, &lb, 2, dash);
            HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            SelectObject(dc, ob); SelectObject(dc, op);
            DeleteObject(pen);
        }
    }
    auto paintRange = [&](int start, int count, COLORREF col, BYTE alpha) {
        for (const RECT& r : rectsFor(page, start, count)) {
            RECT v;
            if (IntersectRect(&v, &r, &clip)) fillAlpha(dc, v, col, alpha);
        }
    };
    if (marks_) {
        for (const RedactMark& m : *marks_) {
            if (m.page != page) continue;
            RECT r = annotScreenRect(page, {m.left, m.bottom, m.right, m.top});
            RECT v;
            if (!IntersectRect(&v, &r, &clip)) continue;
            fillAlpha(dc, r, RGB(0xD0, 0x10, 0x10), 70);
            HPEN pen = CreatePen(PS_DASH, 1, RGB(0xD0, 0x10, 0x10));
            HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
        }
    }
    // Only search hits on this page are examined; matches_ is sorted by page.
    if (!matches_.empty()) {
        auto lo = std::lower_bound(matches_.begin(), matches_.end(), page,
                                   [](const TextHit& h, int p) { return h.page < p; });
        for (auto it = lo; it != matches_.end() && it->page == page; ++it) {
            bool cur = static_cast<int>(it - matches_.begin()) == currentMatch_;
            if (it->annot < 0) { paintRange(it->start, it->count, cur ? RGB(0xFF, 0x8C, 0x00) : RGB(0xFF, 0xE0, 0x00), cur ? 150 : 110); continue; }
            // found in a comment's text: the comment framed
            for (const RECT& r : hitRects(*it)) {
                RECT v;
                if (IntersectRect(&v, &r, &clip)) fillAlpha(dc, v, cur ? RGB(0xFF, 0x8C, 0x00) : RGB(0xFF, 0xE0, 0x00), cur ? 90 : 60);
                HPEN pen = CreatePen(PS_SOLID, cur ? 3 : 2, cur ? RGB(0xFF, 0x8C, 0x00) : RGB(0xE0, 0xB0, 0x00));
                HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                Rectangle(dc, r.left, r.top, r.right, r.bottom);
                SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
            }
        }
    }
    if (spoken_.page == page) paintRange(spoken_.start, spoken_.count, RGB(0x30, 0xC0, 0x60), 120);   // the word being read aloud
    if (sel_.active) {
        int p1 = sel_.p1, c1 = sel_.c1, p2 = sel_.p2, c2 = sel_.c2;
        if (p1 > p2 || (p1 == p2 && c1 > c2)) { std::swap(p1, p2); std::swap(c1, c2); }
        if (page >= p1 && page <= p2) {
            int start = page == p1 ? c1 : 0;
            int end = page == p2 ? c2 : doc_->charCount(page) - 1;
            if (end >= start) paintRange(start, end - start + 1, RGB(0x30, 0x80, 0xFF), 90);
        }
    }
    drawMeasuring(dc, page);
}

void Viewer::drawMeasuring(HDC dc, int page) {
    auto toScreen = [&](const annot::PointD& p, POINT& out) {
        FPDF_PAGE pg = doc_->page(page);
        int x, y;
        pdfium::api().FPDF_PageToDevice(pg, 0, 0, widths_[page], heights_[page], 0, p.x, p.y, &x, &y);
        out = {x + pageX(page), y + tops_[page] - doc_->scrollY};
    };
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0x00, 0x90, 0x60));
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0x00, 0x60, 0x40));
    HFONT f = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ of = SelectObject(dc, f);
    auto dot = [&](POINT p) { Ellipse(dc, p.x - 3, p.y - 3, p.x + 4, p.y + 4); };
    auto label = [&](POINT p, const std::wstring& t) {
        SIZE sz; GetTextExtentPoint32W(dc, t.c_str(), static_cast<int>(t.size()), &sz);
        RECT r{p.x + 6, p.y - sz.cy - 4, p.x + 6 + sz.cx + 8, p.y - 2};
        fillAlpha(dc, r, RGB(0xFF, 0xFF, 0xFF), 220);
        DrawTextW(dc, t.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    };
    for (const Measure& m : measures_) {
        if (m.page != page) continue;
        POINT a, b, c;
        toScreen(m.a, a); toScreen(m.b, b);
        MoveToEx(dc, a.x, a.y, nullptr); LineTo(dc, b.x, b.y);
        dot(a); dot(b);
        if (m.angle) { toScreen(m.c, c); MoveToEx(dc, a.x, a.y, nullptr); LineTo(dc, c.x, c.y); dot(c); angleArc(dc, a, b, c); label(a, m.text); }
        else label({(a.x + b.x) / 2, (a.y + b.y) / 2}, m.text);
    }
    if (hasPending_ && pendingPage_ == page) {   // protractor: the vertex and the first arm wait for the second arm
        POINT v, a;
        toScreen(pendingVertex_, v); dot(v);
        if (hasArm_) { toScreen(pendingArm_, a); MoveToEx(dc, v.x, v.y, nullptr); LineTo(dc, a.x, a.y); dot(a); }
    }
    if (showRulers_) {
        const double perUnit = units_ == 0 ? 72.0 / 25.4 : units_ == 1 ? 72.0 / 2.54 : units_ == 2 ? 72.0 : 1.0;   // points per unit
        const int step = units_ == 3 ? 50 : 1;
        const double px = pxPerPt();
        const int ox = pageX(page), oy = tops_[page] - doc_->scrollY;
        HPEN tp = CreatePen(PS_SOLID, 1, RGB(0x30, 0x30, 0x30));
        SelectObject(dc, tp);
        HFONT sf = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SelectObject(dc, sf);
        SetTextColor(dc, RGB(0x30, 0x30, 0x30));
        RECT strip{ox, oy, ox + widths_[page], oy + 16}, side{ox, oy, ox + 16, oy + heights_[page]};
        fillAlpha(dc, strip, RGB(0xFF, 0xF3, 0xB0), 210);
        fillAlpha(dc, side, RGB(0xFF, 0xF3, 0xB0), 210);
        const int major = units_ == 3 ? 100 : (units_ == 2 ? 1 : 10);
        for (int u = 0; u * step * perUnit * px < std::max(widths_[page], heights_[page]); ++u) {
            const int tick = u * step;
            const int d = static_cast<int>(tick * perUnit * px);
            const bool big = tick % major == 0;
            const int len = big ? 10 : (tick % (major / 2 ? major / 2 : 1) == 0 ? 7 : 4);
            if (d < widths_[page]) { MoveToEx(dc, ox + d, oy, nullptr); LineTo(dc, ox + d, oy + len); }
            if (d < heights_[page]) { MoveToEx(dc, ox, oy + d, nullptr); LineTo(dc, ox + len, oy + d); }
            if (big && tick > 0) {
                wchar_t t[16]; swprintf(t, 16, L"%d", tick);
                if (d < widths_[page]) TextOutW(dc, ox + d + 2, oy + 3, t, static_cast<int>(wcslen(t)));
                if (d < heights_[page]) TextOutW(dc, ox + 11, oy + d + 1, t, static_cast<int>(wcslen(t)));
            }
        }
        DeleteObject(SelectObject(dc, f)); DeleteObject(tp);
    }
    SelectObject(dc, of); DeleteObject(f);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
}

void Viewer::setMatches(std::vector<TextHit> hits, int current) {
    matches_ = std::move(hits);
    currentMatch_ = matches_.empty() ? -1 : current;
    InvalidateRect(hwnd_, nullptr, FALSE);
    if (currentMatch_ >= 0) setCurrentMatch(currentMatch_);
}

void Viewer::setCurrentMatch(int current) {
    if (matches_.empty()) { currentMatch_ = -1; return; }
    currentMatch_ = ((current % static_cast<int>(matches_.size())) + static_cast<int>(matches_.size())) %
                    static_cast<int>(matches_.size());
    const TextHit& h = matches_[currentMatch_];
    if (layout_ == 0 && h.page != currentPage()) showPage(h.page, false);
    auto rects = hitRects(h);   // client coordinates at the current scroll
    if (!rects.empty()) {
        RECT rc; GetClientRect(hwnd_, &rc);
        const RECT& r = rects.front();
        if (r.top < 60 || r.bottom > rc.bottom - 60) {
            doc_->scrollY += (r.top + r.bottom) / 2 - rc.bottom / 2;
        }
        if (r.left < 20 || r.right > rc.right - 20) {
            doc_->scrollX += (r.left + r.right) / 2 - rc.right / 2;
        }
        clampScroll();
        updateScrollBars();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::setSpoken(int page, int start, int count) {
    spoken_ = {page, start, count};
    if (page >= 0 && doc_ && page < static_cast<int>(tops_.size())) {
        if (layout_ == 0 && page != currentPage()) showPage(page, false);
        auto rects = rectsFor(page, start, count);
        if (!rects.empty()) {
            RECT rc;
            GetClientRect(hwnd_, &rc);
            const RECT& r = rects.front();
            if (r.top < 40 || r.bottom > rc.bottom - 40) {   // the reading goes on below (or above) the screen: follow it
                doc_->scrollY += r.top - rc.bottom / 3;
                clampScroll();
                updateScrollBars();
                notify();
            }
        }
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---- text selection ----

bool Viewer::charAt(int cx, int cy, int& page, int& ch, bool nearest) const {
    if (!doc_ || tops_.empty()) return false;
    const int contentY = cy + doc_->scrollY;
    int idx = static_cast<int>(std::upper_bound(tops_.begin(), tops_.end(), contentY) - tops_.begin()) - 1;
    idx = std::clamp(idx, 0, static_cast<int>(tops_.size()) - 1);
    if (layout_ == 0) idx = currentPage();
    else if (idx > 0 && tops_[idx - 1] == tops_[idx] && cx < pageX(idx)) --idx;   // left page of a spread
    if (!nearest && (contentY > tops_[idx] + heights_[idx] || cx < pageX(idx) || cx > pageX(idx) + widths_[idx]))
        return false;
    const auto& api = pdfium::api();
    FPDF_PAGE pg = doc_->page(idx);
    FPDF_TEXTPAGE tp = doc_->text(idx);
    if (!pg || !tp) return false;
    const int lx = std::clamp(cx - pageX(idx), 0, widths_[idx]);
    const int ly = std::clamp(contentY - tops_[idx], 0, heights_[idx]);
    double px, py;
    api.FPDF_DeviceToPage(pg, 0, 0, widths_[idx], heights_[idx], 0, lx, ly, &px, &py);
    int c = api.FPDFText_GetCharIndexAtPos(tp, px, py, 6, 6);
    if (c < 0 && nearest) c = api.FPDFText_GetCharIndexAtPos(tp, px, py, 60, 60);
    if (c < 0) return false;
    page = idx; ch = c;
    return true;
}

void Viewer::extendSelection(int cx, int cy) {
    int p, c;
    if (!charAt(cx, cy, p, c, true)) return;
    sel_.active = true;
    sel_.p1 = anchorPage_; sel_.c1 = anchorChar_;
    sel_.p2 = p; sel_.c2 = c;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::selectWordAt(int cx, int cy) {
    int p, c;
    if (!charAt(cx, cy, p, c, false)) return;
    std::wstring t = doc_->textRange(p, 0, doc_->charCount(p));
    if (c < 0 || c >= static_cast<int>(t.size())) return;
    auto isWord = [](wchar_t ch) { return std::iswalnum(ch) || ch == L'_' || ch == L'\''; };
    int a = c, b = c;
    if (isWord(t[c])) {
        while (a > 0 && isWord(t[a - 1])) --a;
        while (b + 1 < static_cast<int>(t.size()) && isWord(t[b + 1])) ++b;
    }
    sel_ = {true, p, a, p, b};
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::selectAll() {
    if (!doc_) return;
    int last = doc_->pageCount() - 1;
    while (last > 0 && doc_->charCount(last) == 0) --last;
    int n = doc_->charCount(last);
    if (n == 0) return;
    sel_ = {true, 0, 0, last, n - 1};
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::clearSelection() {
    if (!sel_.active) return;
    sel_ = Selection{};
    InvalidateRect(hwnd_, nullptr, FALSE);
}

std::wstring Viewer::selectedText() {
    if (!sel_.active || !doc_) return {};
    int p1 = sel_.p1, c1 = sel_.c1, p2 = sel_.p2, c2 = sel_.c2;
    if (p1 > p2 || (p1 == p2 && c1 > c2)) { std::swap(p1, p2); std::swap(c1, c2); }
    std::wstring out;
    for (int p = p1; p <= p2; ++p) {
        int start = p == p1 ? c1 : 0;
        int end = p == p2 ? c2 : doc_->charCount(p) - 1;
        if (end < start) continue;
        if (!out.empty()) out += L"\r\n";
        out += doc_->textRange(p, start, end - start + 1);
    }
    return out;
}

bool Viewer::copySelection() {
    std::wstring t = selectedText();
    if (t.empty()) return false;
    if (!OpenClipboard(hwnd_)) return false;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
    if (g) {
        memcpy(GlobalLock(g), t.c_str(), (t.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(g);
        if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
    }
    CloseClipboard();
    return g != nullptr;
}

// ---- annotation tools ----

void Viewer::setMarks(const std::vector<RedactMark>* m) {
    marks_ = m;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::setColours(Colours cl) {
    if (cl == colours_) return;
    colours_ = cl;
    clearCache();
    InvalidateRect(hwnd_, nullptr, TRUE);
}

// Night mode, dimmed and blackboard pages (shared/pagecolours.hpp, the same on the Mac). The bitmaps here are packed
// (stride w * 4); the page is drawn at (ox, oy) with size (pw, ph). Blackboard with original colours leaves the pictures
// of the page as they are.
void Viewer::filterPixels(void* bits, int w, int h, int page, int ox, int oy, int pw, int ph) const {
    using M = pagecolours::Mode;
    const M m = colours_ == Colours::Dark ? M::Dark : colours_ == Colours::Dimmed ? M::Dimmed
              : colours_ == Colours::BlackboardInverted ? M::BlackboardInverted
              : colours_ == Colours::BlackboardOriginal ? M::BlackboardOriginal : M::Normal;
    std::vector<pagecolours::Box> keep;
    if (m == M::BlackboardOriginal && doc_) keep = pagecolours::pictureBoxes(*doc_, page, ox, oy, pw, ph);
    pagecolours::apply(m, bits, w, h, w * 4, keep);
}

void Viewer::setTool(Tool t) {
    tool_ = t;
    hoverBlock_ = hoverPage_ = -1;
    polyPts_.clear(); polyPage_ = -1;
    drawing_ = false;
    if (t != Tool::None) clearSelection();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::invalidatePage(int page) {
    auto it = cache_.find(page);
    if (it != cache_.end()) { DeleteObject(it->second.bmp); cache_.erase(it); }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Viewer::documentChanged() {
    clearCache();
    sel_ = Selection{};
    selAnnotPage_ = selAnnotIndex_ = -1;
    if (doc_) {
        int keepY = doc_->scrollY;
        relayout();
        doc_->scrollY = keepY;
        clampScroll();
        updateScrollBars();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

bool Viewer::pageAt(int cx, int cy, int& page) const {
    if (!doc_ || tops_.empty()) return false;
    const int contentY = cy + doc_->scrollY;
    int idx = static_cast<int>(std::upper_bound(tops_.begin(), tops_.end(), contentY) - tops_.begin()) - 1;
    if (layout_ == 0) idx = currentPage();   // only the shown page is there
    for (int i = idx; i >= 0 && i >= idx - 1; --i) {   // two pages can share a row
        if (layout_ == 0 && i != idx) break;
        if (contentY < tops_[i] || contentY > tops_[i] + heights_[i]) continue;
        if (cx < pageX(i) || cx > pageX(i) + widths_[i]) continue;
        page = i;
        return true;
    }
    return false;
}

bool Viewer::toPage(int cx, int cy, int page, annot::PointD& out) const {
    FPDF_PAGE pg = doc_ ? doc_->page(page) : nullptr;
    if (!pg) return false;
    const int lx = std::clamp(cx - pageX(page), 0, widths_[page]);
    const int ly = std::clamp(cy + doc_->scrollY - tops_[page], 0, heights_[page]);
    double px, py;
    pdfium::api().FPDF_DeviceToPage(pg, 0, 0, widths_[page], heights_[page], 0, lx, ly, &px, &py);
    out = {px, py};
    return true;
}

RECT Viewer::annotScreenRect(int page, const annot::RectD& r) const {
    RECT out{0, 0, 0, 0};
    FPDF_PAGE pg = doc_ ? doc_->page(page) : nullptr;
    if (!pg) return out;
    int x1, y1, x2, y2;
    const auto& api = pdfium::api();
    api.FPDF_PageToDevice(pg, 0, 0, widths_[page], heights_[page], 0, r.left, r.top, &x1, &y1);
    api.FPDF_PageToDevice(pg, 0, 0, widths_[page], heights_[page], 0, r.right, r.bottom, &x2, &y2);
    const int ox = pageX(page), oy = tops_[page] - doc_->scrollY;
    return {std::min(x1, x2) + ox, std::min(y1, y2) + oy, std::max(x1, x2) + ox + 1, std::max(y1, y2) + oy + 1};
}

int Viewer::formFieldAt(int cx, int cy, int& page, annot::PointD& pt) const {
    if (!doc_ || !doc_->hasForm() || !pageAt(cx, cy, page) || !toPage(cx, cy, page, pt)) return -1;
    FPDF_PAGE pg = doc_->page(page);
    return pg ? pdfium::api().FPDFPage_HasFormFieldAtPoint(doc_->form(), pg, pt.x, pt.y) : -1;
}

bool Viewer::selectedAnnot(int& page, int& index) const {
    if (selAnnotPage_ < 0) return false;
    page = selAnnotPage_; index = selAnnotIndex_;
    return true;
}

void Viewer::clearAnnotSelection() {
    if (selAnnotPage_ < 0) return;
    selAnnotPage_ = selAnnotIndex_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

std::vector<Viewer::PageRects> Viewer::selectionRects() {
    std::vector<PageRects> out;
    if (!sel_.active || !doc_) return out;
    int p1 = sel_.p1, c1 = sel_.c1, p2 = sel_.p2, c2 = sel_.c2;
    if (p1 > p2 || (p1 == p2 && c1 > c2)) { std::swap(p1, p2); std::swap(c1, c2); }
    const auto& api = pdfium::api();
    for (int p = p1; p <= p2; ++p) {
        FPDF_TEXTPAGE tp = doc_->text(p);
        if (!tp) continue;
        int start = p == p1 ? c1 : 0;
        int end = p == p2 ? c2 : doc_->charCount(p) - 1;
        if (end < start) continue;
        PageRects pr{p, {}};
        int n = api.FPDFText_CountRects(tp, start, end - start + 1);
        for (int i = 0; i < n; ++i) {
            double l, t, r, b;
            if (api.FPDFText_GetRect(tp, i, &l, &t, &r, &b)) pr.rects.push_back({l, b, r, t});
        }
        if (!pr.rects.empty()) out.push_back(std::move(pr));
    }
    return out;
}

void Viewer::drawPreview(HDC dc) {
    if (tool_ == Tool::Protractor) {
        const COLORREF green = RGB(0x00, 0x90, 0x60);
        static const wchar_t* steps[] = {L"Click the corner of the angle", L"Click a point on the first side", L"Click a point on the second side   (Esc cancels)"};
        if (polyPage_ >= 0 && !polyPts_.empty()) {
            HPEN pen = CreatePen(PS_SOLID, 2, green);
            HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            auto scr = [&](const annot::PointD& p) { RECT r = annotScreenRect(polyPage_, {p.x, p.y, p.x, p.y}); return POINT{r.left, r.top}; };
            const POINT v = scr(polyPts_[0]);
            const POINT a = polyPts_.size() > 1 ? scr(polyPts_[1]) : polyMouse_;
            MoveToEx(dc, v.x, v.y, nullptr); LineTo(dc, a.x, a.y);
            Ellipse(dc, v.x - 3, v.y - 3, v.x + 4, v.y + 4);
            if (polyPts_.size() > 1) {
                Ellipse(dc, a.x - 3, a.y - 3, a.x + 4, a.y + 4);
                MoveToEx(dc, v.x, v.y, nullptr); LineTo(dc, polyMouse_.x, polyMouse_.y);
                angleArc(dc, v, a, polyMouse_);
                wchar_t t[96];
                swprintf(t, 96, L"%.1f\u00B0   - click to keep it (Esc cancels)", angleAt(v, a, polyMouse_));
                cursorNote(dc, polyMouse_, t, RGB(0x00, 0x60, 0x40));
            } else cursorNote(dc, polyMouse_, steps[1], RGB(0x00, 0x60, 0x40));
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
        } else if (polyMouse_.x || polyMouse_.y) cursorNote(dc, polyMouse_, steps[0], RGB(0x00, 0x60, 0x40));
        return;
    }
    if (tool_ == Tool::Polygon && polyPage_ >= 0 && !polyPts_.empty()) {
        HPEN pen = CreatePen(PS_SOLID, std::max(1, static_cast<int>(penWidth_ * pxPerPt())), penColor_);
        HGDIOBJ op = SelectObject(dc, pen);
        bool first = true;
        for (const auto& p : polyPts_) {
            RECT r = annotScreenRect(polyPage_, {p.x, p.y, p.x, p.y});
            if (first) { MoveToEx(dc, r.left, r.top, nullptr); first = false; } else LineTo(dc, r.left, r.top);
        }
        LineTo(dc, polyMouse_.x, polyMouse_.y);
        SelectObject(dc, op); DeleteObject(pen);
    }
    if (!drawing_) return;
    const int w = std::max(1, static_cast<int>(std::lround(penWidth_ * pxPerPt())));
    const bool boxTool = tool_ == Tool::TableEdit || tool_ == Tool::TableDelete || tool_ == Tool::Redact || tool_ == Tool::Link || tool_ == Tool::CopyArea || tool_ == Tool::CropBox || tool_ == Tool::FormText || tool_ == Tool::FormCheck || tool_ == Tool::FormCombo || tool_ == Tool::FormRadio || tool_ == Tool::FormList || tool_ == Tool::FormDate || tool_ == Tool::PicCrop;
    const COLORREF previewCol = tool_ == Tool::Redact ? RGB(0xD0, 0x10, 0x10) : boxTool ? RGB(0x30, 0x80, 0xFF) : (tool_ == Tool::Ruler || tool_ == Tool::Protractor) ? RGB(0x00, 0x90, 0x60) : penColor_;
    HPEN pen = CreatePen(boxTool ? PS_DASH : PS_SOLID, boxTool || tool_ == Tool::Ruler || tool_ == Tool::Protractor ? 1 : w, previewCol);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    switch (tool_) {
        case Tool::TableEdit: case Tool::TableDelete:
        case Tool::Rectangle: case Tool::Redact: case Tool::Link: case Tool::CopyArea: case Tool::CropBox: case Tool::FormText: case Tool::FormCheck: case Tool::FormCombo: case Tool::FormRadio: case Tool::FormList: case Tool::FormDate: case Tool::Cloud: case Tool::PicCrop: Rectangle(dc, drawStart_.x, drawStart_.y, drawNow_.x, drawNow_.y); break;
        case Tool::Ellipse: Ellipse(dc, drawStart_.x, drawStart_.y, drawNow_.x, drawNow_.y); break;
        case Tool::Line: case Tool::Arrow: case Tool::Ruler: case Tool::Protractor: case Tool::Callout:
            MoveToEx(dc, drawStart_.x, drawStart_.y, nullptr); LineTo(dc, drawNow_.x, drawNow_.y); break;
        case Tool::Pen:
            if (!drawPts_.empty()) {
                // Redraw the stroke from its page points, so it stays right while the view scrolls.
                bool first = true;
                for (const auto& p : drawPts_) {
                    FPDF_PAGE pg = doc_->page(drawPage_);
                    int x, y;
                    pdfium::api().FPDF_PageToDevice(pg, 0, 0, widths_[drawPage_], heights_[drawPage_], 0, p.x, p.y, &x, &y);
                    x += pageX(drawPage_); y += tops_[drawPage_] - doc_->scrollY;
                    if (first) { MoveToEx(dc, x, y, nullptr); first = false; } else LineTo(dc, x, y);
                }
            }
            break;
        default: break;
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void Viewer::onPaint() {
    PAINTSTRUCT ps;
    HDC screenDc = BeginPaint(hwnd_, &ps);
    RECT rc; GetClientRect(hwnd_, &rc);
    PaintBuf buf(screenDc, rc, ps.rcPaint);
    HDC dc = buf.dc;

    HBRUSH bg = CreateSolidBrush(blackboard() ? RGB(0, 0, 0) : pal().dark || colours_ == Colours::Dark ? pal().view : RGB(0xE8, 0xE8, 0xE8));
    FillRect(dc, &ps.rcPaint, bg);
    DeleteObject(bg);

    if (!doc_) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, pal().viewText);
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        RECT box{rc.left + 20, 0, rc.right - 20, 0};
        DrawTextW(dc, emptyText_.c_str(), -1, &box, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);   // measure, then centre the block
        const int hgt = box.bottom - box.top;
        box.left = rc.left + 20; box.right = rc.right - 20;   // measuring shrinks the width: restore it so the text is centred
        box.top = (rc.bottom - hgt) / 2; box.bottom = box.top + hgt;
        DrawTextW(dc, emptyText_.c_str(), -1, &box, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    } else {
        for (size_t i = 0; i < tops_.size(); ++i) {
            if (layout_ == 0 && static_cast<int>(i) != currentPage()) continue;   // single page: only the shown one
            int x = pageX(static_cast<int>(i));
            int y = tops_[i] - doc_->scrollY;
            if (y + heights_[i] < ps.rcPaint.top) continue;
            if (y > ps.rcPaint.bottom) break;
            RECT shadow{x + 2, y + 2, x + widths_[i] + 2, y + heights_[i] + 2};
            HBRUSH sb = CreateSolidBrush(blackboard() ? RGB(0x26, 0x26, 0x26) : RGB(0xC8, 0xC8, 0xC8));   // on black: a faint edge
            FillRect(dc, &shadow, sb);
            DeleteObject(sb);
            drawPage(dc, static_cast<int>(i), x, y, widths_[i], heights_[i], ps.rcPaint);
            drawOverlays(dc, static_cast<int>(i), x, y, widths_[i], heights_[i], ps.rcPaint);
        }
        if (selAnnotPage_ >= 0 && selAnnotPage_ < static_cast<int>(tops_.size())) {
            for (const auto& a : annot::list(*doc_, selAnnotPage_)) {
                if (a.index != selAnnotIndex_) continue;
                RECT r = annotScreenRect(selAnnotPage_, a.rect);
                InflateRect(&r, 3, 3);
                HPEN pen = CreatePen(PS_DOT, 1, RGB(0x30, 0x80, 0xFF));
                HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                Rectangle(dc, r.left, r.top, r.right, r.bottom);
                SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
            }
        }
        drawObjectSelection(dc);
        drawPreview(dc);
    }
    buf.done();
    EndPaint(hwnd_, &ps);
}

LRESULT Viewer::handle(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: onPaint(); return 0;
        case WM_SIZE:
            relayout();
            clampScroll();
            updateScrollBars();
            notify();
            return 0;
        case WM_VSCROLL: case WM_HSCROLL: {
            if (!doc_) return 0;
            const bool vert = m == WM_VSCROLL;
            if (vert && layout_ == 0) {
                const int code = LOWORD(w);
                RECT rc; GetClientRect(hwnd_, &rc);
                if (code == SB_LINEDOWN || code == SB_LINEUP) { scrollBy(code == SB_LINEDOWN ? 40 : -40); return 0; }
                if (code == SB_PAGEDOWN || code == SB_PAGEUP) { scrollBy((code == SB_PAGEDOWN ? 1 : -1) * static_cast<int>(rc.bottom) * 9 / 10); return 0; }
            }
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(hwnd_, vert ? SB_VERT : SB_HORZ, &si);
            int pos = si.nPos;
            switch (LOWORD(w)) {
                case SB_LINEUP: pos -= 40; break;
                case SB_LINEDOWN: pos += 40; break;
                case SB_PAGEUP: pos -= static_cast<int>(si.nPage) * 9 / 10; break;
                case SB_PAGEDOWN: pos += static_cast<int>(si.nPage) * 9 / 10; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = themeui::trackPos(hwnd_, vert ? SB_VERT : SB_HORZ); break;
                case SB_TOP: pos = 0; break;
                case SB_BOTTOM: pos = si.nMax; break;
            }
            (vert ? doc_->scrollY : doc_->scrollX) = pos;
            clampScroll();
            updateScrollBars();
            InvalidateRect(hwnd_, nullptr, FALSE);
            notify();
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (!doc_) return 0;
            int delta = GET_WHEEL_DELTA_WPARAM(w);
            if (presenting_ && !(GET_KEYSTATE_WPARAM(w) & MK_CONTROL)) {   // one page per notch, never half a page
                wheelAcc_ += delta;
                while (wheelAcc_ >= WHEEL_DELTA) { wheelAcc_ -= WHEEL_DELTA; stepPage(-1); }
                while (wheelAcc_ <= -WHEEL_DELTA) { wheelAcc_ += WHEEL_DELTA; stepPage(1); }
                return 0;
            }
            if (GET_KEYSTATE_WPARAM(w) & MK_CONTROL) {
                // zoom around the mouse pointer; 10 % per wheel notch, in proportion for finer wheels and touchpads
                POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                ScreenToClient(hwnd_, &p);
                setZoomAt(doc_->zoom * std::pow(1.1, delta / static_cast<double>(WHEEL_DELTA)), p.x, p.y);
            } else if (GET_KEYSTATE_WPARAM(w) & MK_SHIFT) {
                scrollByX(-delta * 3 / 2);   // Shift + wheel: sideways, as in other Windows programs
            } else {
                scrollBy(-delta * 3 / 2);
            }
            return 0;
        }
        case WM_MOUSEHWHEEL: {   // a touchpad swiped left or right, or a wheel tilted: positive is to the right
            if (!doc_ || presenting_) return TRUE;
            scrollByX(GET_WHEEL_DELTA_WPARAM(w) * 3 / 2);
            return TRUE;
        }
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT && doc_) {
                LPCWSTR cur = tool_ == Tool::None ? IDC_IBEAM : IDC_CROSS;
                if (tool_ == Tool::EditAll) {   // edit mode: a text cursor over text, the move cursor over a picture
                    POINT p; GetCursorPos(&p); ScreenToClient(hwnd_, &p);
                    int pg; pageobj::EditBlock b;
                    cur = objHit(p.x, p.y) != ObjDrag::None ? IDC_SIZEALL : editBlockAt(p.x, p.y, pg, &b) < 0 ? IDC_ARROW
                        : b.kind == pageobj::EditBlock::Picture ? IDC_SIZEALL : IDC_IBEAM;
                    SetCursor(LoadCursor(nullptr, cur));
                    return TRUE;
                }
                if (tool_ == Tool::None && doc_->hasForm()) {
                    POINT p; GetCursorPos(&p); ScreenToClient(hwnd_, &p);
                    int pg; annot::PointD pt;
                    int ft = formFieldAt(p.x, p.y, pg, pt);
                    if (ft == FPDF_FORMFIELD_TEXTFIELD) cur = IDC_IBEAM;
                    else if (ft >= 0) cur = IDC_HAND;
                }
                if (tool_ == Tool::None) {
                    POINT p; GetCursorPos(&p); ScreenToClient(hwnd_, &p);
                    int dp; std::wstring u;
                    if (cur == IDC_IBEAM && linkAt(p.x, p.y, dp, u)) cur = IDC_HAND;
                }
                SetCursor(LoadCursor(nullptr, cur));
                return TRUE;
            }
            break;
        case WM_RBUTTONDOWN:
            if (onPress && onPress(m, w, l)) return 0;
            break;
        case WM_LBUTTONDOWN: {
            // palm rejection (systems that send touches as mouse clicks): no drawing with the hand while the pen is in use
            if (fromTouch() && (tool_ == Tool::Pen || GetTickCount() - lastPen_ < 1500)) return 0;
            if (onPress && onPress(m, w, l)) return 0;
            SetFocus(hwnd_);
            if (!doc_) return 0;
            const int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
            int page, ch;
            if (tool_ == Tool::Protractor) {   // three clicks: the corner, a point on each side
                annot::PointD pt;
                if (polyPage_ < 0) { if (pageAt(cx, cy, page) && toPage(cx, cy, page, pt)) { polyPage_ = page; polyPts_ = {pt}; } }
                else if (toPage(cx, cy, polyPage_, pt)) {
                    polyPts_.push_back(pt);
                    if (polyPts_.size() == 3) {
                        Event e; e.kind = Event::Draw; e.tool = Tool::Protractor; e.page = polyPage_; e.pts = polyPts_;
                        polyPts_.clear(); polyPage_ = -1;
                        if (onEvent) onEvent(e);
                    }
                }
                polyMouse_ = {cx, cy};
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (tool_ == Tool::Polygon) {
                annot::PointD pt;
                if (polyPage_ < 0) { if (pageAt(cx, cy, page) && toPage(cx, cy, page, pt)) { polyPage_ = page; polyPts_ = {pt}; } }
                else if (toPage(cx, cy, polyPage_, pt)) {
                    // a click on the first point closes the shape
                    RECT first = annotScreenRect(polyPage_, {polyPts_[0].x, polyPts_[0].y, polyPts_[0].x, polyPts_[0].y});
                    if (polyPts_.size() >= 3 && std::abs(cx - first.left) <= 6 && std::abs(cy - first.top) <= 6) { finishPolygon(); return 0; }
                    polyPts_.push_back(pt);
                }
                polyMouse_ = {cx, cy};
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (tool_ == Tool::Select || tool_ == Tool::EditAll) {   // edit mode picks pictures and moves them as Select does
                ObjDrag m = objHit(cx, cy);
                annot::PointD pt;
                if (m != ObjDrag::None && pageUnclamped(cx, cy, objSelPage_, pt)) {
                    objDrag_ = m; objDragStart_ = pt; objCur_ = objSel_; snappedX_ = snappedY_ = false;
                    SetCapture(hwnd_);
                } else if (pageAt(cx, cy, page) && toPage(cx, cy, page, pt) && onEvent) {
                    Event e; e.kind = Event::Click; e.tool = tool_; e.page = page; e.pts = {pt};
                    onEvent(e);
                }
                return 0;
            }
            if (tool_ != Tool::None) {
                annot::PointD pt;
                if (pageAt(cx, cy, page) && toPage(cx, cy, page, pt)) {
                    drawing_ = true; drawPage_ = page;
                    drawStart_ = drawNow_ = {cx, cy};
                    drawPts_.assign(1, pt);
                    penStroke_ = GetTickCount() - lastPen_ < 200 && penPressure_ >= 0;
                    drawPressure_.assign(1, penStroke_ ? penPressure_ : 0.5);
                    SetCapture(hwnd_);
                }
                return 0;
            }
            {
                int destPage, fpg = 0; std::wstring uri; annot::PointD fpt;
                if (!(doc_->hasForm() && formFieldAt(cx, cy, fpg, fpt) >= 0) && linkAt(cx, cy, destPage, uri)) {
                    if (destPage >= 0) goToPage(destPage);
                    else if (onUri) onUri(uri);
                    return 0;
                }
            }
            if (presenting_) {   // a click turns to the next page, unless it is meant for a form field
                int fpg = 0; annot::PointD fpt;
                if (!(doc_->hasForm() && (formFocus_ || formFieldAt(cx, cy, fpg, fpt) >= 0))) { stepPage(1); return 0; }
            }
            clearSelection();
            clearAnnotSelection();
            if (doc_->hasForm()) {   // a form field under the pointer (or an open drop-down list) takes the click
                annot::PointD fp; int fpage = formPage_;
                const int ft = formFieldAt(cx, cy, fpage, fp);
                if (ft >= 0 || formFocus_) {
                    if (ft < 0) {   // a click inside an open list is outside the field's own box: use the last field's page
                        int pg;
                        if (pageAt(cx, cy, pg)) fpage = pg;
                        toPage(cx, cy, fpage, fp);
                    }
                    doc_->currentPageForForm = fpage;
                    doc_->formBeforeInput();
                    formPage_ = fpage;
                    const bool handled = pdfium::api().FORM_OnLButtonDown(doc_->form(), doc_->page(fpage), 0, fp.x, fp.y) != 0;
                    if (ft >= 0 || handled) {
                        formFocus_ = true;
                        formCapture_ = true;
                        SetCapture(hwnd_);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return 0;
                    }
                    pdfium::api().FORM_ForceToKillFocus(doc_->form());   // an ordinary click elsewhere
                    formFocus_ = false;
                }
            }
            if (pageAt(cx, cy, page)) {
                annot::PointD pt;
                if (toPage(cx, cy, page, pt)) {
                    int ai = annot::hitTest(*doc_, page, pt);
                    if (ai >= 0) {
                        // Shape-like annotations are selected by a click; text markup leaves text selection alone.
                        for (const auto& a : annot::list(*doc_, page)) {
                            if (a.index != ai) continue;
                            if (a.subtype == FPDF_ANNOT_TEXT || a.subtype == FPDF_ANNOT_SQUARE || a.subtype == FPDF_ANNOT_CIRCLE ||
                                a.subtype == FPDF_ANNOT_INK || a.subtype == FPDF_ANNOT_STAMP || a.subtype == FPDF_ANNOT_FREETEXT) {
                                selAnnotPage_ = page; selAnnotIndex_ = ai;
                                InvalidateRect(hwnd_, nullptr, FALSE);
                                return 0;
                            }
                        }
                    }
                }
            }
            // a drag that starts beside the text (in the margin, between lines) starts at the nearest character
            if (charAt(cx, cy, page, ch, false) || (pageAt(cx, cy, page) && charAt(cx, cy, page, ch, true))) {
                anchorPage_ = page; anchorChar_ = ch;
                dragging_ = true;
                SetCapture(hwnd_);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (!doc_) return 0;
            if (tool_ == Tool::Polygon) { finishPolygon(); return 0; }
            const int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
            int page; annot::PointD pt;
            if (tool_ == Tool::None && pageAt(cx, cy, page) && toPage(cx, cy, page, pt)) {
                int ai = annot::hitTest(*doc_, page, pt);
                if (ai >= 0 && onEvent) {
                    Event e; e.kind = Event::DoubleClick; e.page = page; e.annot = ai; e.pts = {pt};
                    onEvent(e);
                    return 0;
                }
            }
            if (tool_ == Tool::None) selectWordAt(cx, cy);
            return 0;
        }
        case WM_RBUTTONUP: {
            if (!onEvent) return 0;
            const int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
            if (!doc_) {   // nothing open: the main window offers the recent files
                Event e; e.kind = Event::ContextMenu; e.page = -1;
                e.screen = {cx, cy};
                ClientToScreen(hwnd_, &e.screen);
                onEvent(e);
                return 0;
            }
            int page; annot::PointD pt;
            Event e; e.kind = Event::ContextMenu; e.tool = tool_;
            if (pageAt(cx, cy, page) && toPage(cx, cy, page, pt)) { e.page = page; e.pts = {pt}; e.annot = annot::hitTest(*doc_, page, pt); }
            else e.page = -1;
            e.screen = {cx, cy};
            ClientToScreen(hwnd_, &e.screen);
            onEvent(e);
            return 0;
        }
        case WM_POINTERDOWN: case WM_POINTERUPDATE: case WM_POINTERUP: {
            const UINT32 id = GET_POINTERID_WPARAM(w);
            POINTER_INPUT_TYPE type = PT_POINTER;
            if (!pointerType() || !pointerType()(id, &type)) break;
            if (type == PT_PEN) {
                lastPen_ = GetTickCount();
                POINTER_PEN_INFO pi{};
                penPressure_ = penInfo() && penInfo()(id, &pi) && (pi.penMask & PEN_MASK_PRESSURE) ? pi.pressure / 1024.0 : -1;
                break;   // Windows turns it into mouse messages, which draw
            }
            // palm rejection: a hand resting on the screen while the pen draws (or the pen tool is on) does nothing
            if (type == PT_TOUCH && (tool_ == Tool::Pen || GetTickCount() - lastPen_ < 1500)) return 0;
            break;
        }
        case WM_MOUSEMOVE:
            if (tool_ == Tool::EditAll && objDrag_ == ObjDrag::None) {   // edit mode: the block under the pointer lights up
                int pg = -1;
                const int b = editBlockAt(GET_X_LPARAM(l), GET_Y_LPARAM(l), pg);
                if (b != hoverBlock_ || (b >= 0 && pg != hoverPage_)) { hoverBlock_ = b; hoverPage_ = b >= 0 ? pg : -1; InvalidateRect(hwnd_, nullptr, FALSE); }
            }
            if ((tool_ == Tool::Polygon && polyPage_ >= 0) || tool_ == Tool::Protractor) { polyMouse_ = {GET_X_LPARAM(l), GET_Y_LPARAM(l)}; InvalidateRect(hwnd_, nullptr, FALSE); }
            if (objDrag_ != ObjDrag::None && doc_) {
                annot::PointD now;
                if (!pageUnclamped(GET_X_LPARAM(l), GET_Y_LPARAM(l), objSelPage_, now)) return 0;
                const double thr = 6.0 / pxPerPt();
                const double dx = now.x - objDragStart_.x, dy = now.y - objDragStart_.y;
                auto snap = [&](const std::vector<double>& g, double v, double& best, bool& ok) {
                    double bd = thr + 1;
                    for (double c : g) if (std::fabs(c - v) < bd) { bd = std::fabs(c - v); best = c; }
                    ok = bd <= thr;
                    return ok;
                };
                annot::RectD r = objSel_;
                snappedX_ = snappedY_ = false;
                if (objDrag_ == ObjDrag::Move) {
                    r = {objSel_.left + dx, objSel_.bottom + dy, objSel_.right + dx, objSel_.top + dy};
                    double bestShift = 1e9, line = 0;
                    for (double e : {r.left, (r.left + r.right) / 2, r.right}) { double g; bool ok; if (snap(guideX_, e, g, ok) && std::fabs(g - e) < std::fabs(bestShift)) { bestShift = g - e; line = g; snappedX_ = true; } }
                    if (snappedX_) { r.left += bestShift; r.right += bestShift; snapX_ = line; }
                    bestShift = 1e9;
                    for (double e : {r.bottom, (r.bottom + r.top) / 2, r.top}) { double g; bool ok; if (snap(guideY_, e, g, ok) && std::fabs(g - e) < std::fabs(bestShift)) { bestShift = g - e; line = g; snappedY_ = true; } }
                    if (snappedY_) { r.bottom += bestShift; r.top += bestShift; snapY_ = line; }
                } else {
                    // a corner moves, the opposite corner stays; the aspect ratio is kept
                    const bool left = objDrag_ == ObjDrag::TL || objDrag_ == ObjDrag::BL, topSide = objDrag_ == ObjDrag::TL || objDrag_ == ObjDrag::TR;
                    const double ax = left ? objSel_.right : objSel_.left, ay = topSide ? objSel_.bottom : objSel_.top;
                    double mx = (left ? objSel_.left : objSel_.right) + dx;
                    double g; bool ok;
                    if (snap(guideX_, mx, g, ok)) { mx = g; snappedX_ = true; snapX_ = g; }
                    const double w0 = objSel_.right - objSel_.left, h0 = objSel_.top - objSel_.bottom;
                    double scale = std::max(0.05, std::fabs(mx - ax) / std::max(1.0, w0));
                    const double nw = w0 * scale, nh = h0 * scale;
                    const double nx = left ? ax - nw : ax + nw, ny = topSide ? ay + nh : ay - nh;
                    r = {std::min(ax, nx), std::min(ay, ny), std::max(ax, nx), std::max(ay, ny)};
                }
                objCur_ = r;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (formCapture_ && doc_) {
                annot::PointD fp;
                if (toPage(GET_X_LPARAM(l), GET_Y_LPARAM(l), formPage_, fp))
                    pdfium::api().FORM_OnMouseMove(doc_->form(), doc_->page(formPage_), 0, fp.x, fp.y);
                return 0;
            }
            if (!formCapture_ && !drawing_ && !dragging_ && doc_ && doc_->hasForm() && tool_ == Tool::None) {
                // Keep PDFium's idea of "which field is the mouse over" current: check boxes and radio buttons depend on it.
                int pg; annot::PointD pt;
                if (pageAt(GET_X_LPARAM(l), GET_Y_LPARAM(l), pg) && toPage(GET_X_LPARAM(l), GET_Y_LPARAM(l), pg, pt))
                    pdfium::api().FORM_OnMouseMove(doc_->form(), doc_->page(pg), 0, pt.x, pt.y);
            }
            if (drawing_) {
                int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
                drawNow_ = {cx, cy};
                annot::PointD pt;
                if (tool_ == Tool::Pen && toPage(cx, cy, drawPage_, pt)) {
                    drawPts_.push_back(pt);
                    const bool pen = GetTickCount() - lastPen_ < 200 && penPressure_ >= 0;
                    penStroke_ = penStroke_ && pen;
                    drawPressure_.push_back(pen ? penPressure_ : 0.5);
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
            } else if (dragging_) {
                int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
                RECT rc; GetClientRect(hwnd_, &rc);
                if (cy < 0) scrollBy(cy / 2 - 8);                       // drag past the edge to scroll
                else if (cy > rc.bottom) scrollBy((cy - rc.bottom) / 2 + 8);
                extendSelection(cx, cy);
            }
            return 0;
        case WM_LBUTTONUP:
            if (objDrag_ != ObjDrag::None) {
                const annot::RectD from = objSel_, to = objCur_;
                objDrag_ = ObjDrag::None;
                ReleaseCapture();
                snappedX_ = snappedY_ = false;
                const bool moved = std::fabs(to.left - from.left) + std::fabs(to.bottom - from.bottom) + std::fabs(to.right - from.right) + std::fabs(to.top - from.top) > 0.5;
                if (moved && onEvent) {
                    Event e; e.kind = Event::Draw; e.tool = Tool::Select; e.page = objSelPage_;
                    e.pts = {{from.left, from.bottom}, {from.right, from.top}, {to.left, to.bottom}, {to.right, to.top}};
                    onEvent(e);
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (formCapture_ && doc_) {
                annot::PointD fp;
                formCapture_ = false;
                ReleaseCapture();
                if (toPage(GET_X_LPARAM(l), GET_Y_LPARAM(l), formPage_, fp))
                    pdfium::api().FORM_OnLButtonUp(doc_->form(), doc_->page(formPage_), 0, fp.x, fp.y);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (drawing_) {
                drawing_ = false;
                ReleaseCapture();
                annot::PointD end;
                const int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
                if (toPage(cx, cy, drawPage_, end) && onEvent) {
                    Event e; e.page = drawPage_; e.tool = tool_;
                    const bool moved = std::abs(cx - drawStart_.x) + std::abs(cy - drawStart_.y) > 3;
                    const bool tableBox = (tool_ == Tool::TableEdit || tool_ == Tool::TableDelete) && moved;   // a box drawn around a table
                    if (!tableBox && (tool_ == Tool::Comment || tool_ == Tool::AddText || tool_ == Tool::EditText || tool_ == Tool::Picture || tool_ == Tool::Stamp || tool_ == Tool::TableDelete || tool_ == Tool::TablePlace || tool_ == Tool::TableEdit || tool_ == Tool::EditPara)) { e.kind = Event::Click; e.pts = {drawPts_.front()}; onEvent(e); }
                    else if (moved) {
                        e.kind = Event::Draw;
                        e.pts = tool_ == Tool::Pen ? drawPts_ : std::vector<annot::PointD>{drawPts_.front(), end};
                        if (tool_ == Tool::Pen && penStroke_ && drawPressure_.size() == drawPts_.size()) e.pressure = drawPressure_;
                        onEvent(e);
                    }
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            if (dragging_) { dragging_ = false; ReleaseCapture(); }
            return 0;
        case WM_CHAR:
            // Backspace and Enter reach fields as characters too (deleting, a new line, closing a list); Ctrl+H and
            // Ctrl+M give the same codes and are shortcuts instead
            if (formFocus_ && doc_ && (w >= 0x20 || ((w == L'\b' || w == L'\r') && GetKeyState(VK_CONTROL) >= 0))) {
                pdfium::api().FORM_OnChar(doc_->form(), doc_->page(formPage_), static_cast<int>(w), 0);
                return 0;
            }
            break;
        case WM_TIMER:
            Document::fireTimer(static_cast<int>(w));
            return 0;
        case WM_KILLFOCUS:
            if (formFocus_ && doc_ && doc_->hasForm() && !formCapture_) { pdfium::api().FORM_ForceToKillFocus(doc_->form()); formFocus_ = false; }
            break;
        case WM_KEYUP:
            if (formFocus_ && doc_) {
                pdfium::api().FORM_OnKeyUp(doc_->form(), doc_->page(formPage_), static_cast<int>(w), 0);
                return 0;
            }
            break;
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
            if (formFocus_ && doc_ && m == WM_KEYDOWN) {
                const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
                const auto& api = pdfium::api();
                FPDF_FORMHANDLE fh = doc_->form();
                FPDF_PAGE pg = doc_->page(formPage_);
                if (w == VK_ESCAPE) { api.FORM_ForceToKillFocus(fh); formFocus_ = false; InvalidateRect(hwnd_, nullptr, FALSE); return 0; }
                if (ctrl) {
                    if (w == 'A') { api.FORM_SelectAllText(fh, pg); return 0; }
                    if (w == 'C' || w == 'X') {
                        unsigned long bytes = api.FORM_GetSelectedText(fh, pg, nullptr, 0);
                        if (bytes >= 4) {
                            std::vector<unsigned short> buf(bytes / 2 + 1);
                            api.FORM_GetSelectedText(fh, pg, buf.data(), bytes);
                            std::wstring t(buf.begin(), buf.begin() + (bytes / 2 - 1));
                            if (OpenClipboard(hwnd_)) {
                                EmptyClipboard();
                                HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
                                if (g) { memcpy(GlobalLock(g), t.c_str(), (t.size() + 1) * sizeof(wchar_t)); GlobalUnlock(g); if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g); }
                                CloseClipboard();
                            }
                            if (w == 'X') api.FORM_ReplaceSelection(fh, pg, reinterpret_cast<FPDF_WIDESTRING>(L""));
                        }
                        return 0;
                    }
                    if (w == 'V') {
                        if (OpenClipboard(hwnd_)) {
                            HANDLE g = GetClipboardData(CF_UNICODETEXT);
                            if (g) { const wchar_t* t = static_cast<const wchar_t*>(GlobalLock(g)); if (t) { std::wstring s(t); GlobalUnlock(g); api.FORM_ReplaceSelection(fh, pg, reinterpret_cast<FPDF_WIDESTRING>(s.c_str())); } }
                            CloseClipboard();
                        }
                        return 0;
                    }
                    return SendMessageW(GetParent(hwnd_), m, w, l);   // other shortcuts (save, find, ...) still work
                }
                int mod = (shift ? 1 : 0);
                api.FORM_OnKeyDown(fh, pg, static_cast<int>(w), mod);
                return 0;   // while a field has the keyboard, its keys do not scroll or trigger shortcuts
            }
            return SendMessageW(GetParent(hwnd_), m, w, l);   // shortcuts live in the main window
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

void Viewer::endFormInput(const Document* d) {
    if (!formFocus_ || !doc_ || (d && doc_ != d) || !doc_->hasForm()) return;
    pdfium::api().FORM_ForceToKillFocus(doc_->form());
    formFocus_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool Viewer::linkAt(int cx, int cy, int& destPage, std::wstring& uri) const {
    destPage = -1; uri.clear();
    int page; annot::PointD pt;
    if (!doc_ || !pageAt(cx, cy, page) || !toPage(cx, cy, page, pt)) return false;
    const auto& api = pdfium::api();
    FPDF_PAGE pg = doc_->page(page);
    FPDF_LINK link = api.FPDFLink_GetLinkAtPoint(pg, pt.x, pt.y);
    if (!link) return false;
    FPDF_ACTION act = api.FPDFLink_GetAction(link);
    if (act) {
        const unsigned long type = api.FPDFAction_GetType(act);
        if (type == PDFACTION_URI) {
            unsigned long n = api.FPDFAction_GetURIPath(doc_->handle(), act, nullptr, 0);
            if (n > 1) {
                std::string buf(n, '\0');
                api.FPDFAction_GetURIPath(doc_->handle(), act, buf.data(), n);
                buf.resize(n - 1);
                uri.assign(buf.begin(), buf.end());
                return true;
            }
            return false;
        }
        if (type == PDFACTION_GOTO) {
            FPDF_DEST dest = api.FPDFAction_GetDest(doc_->handle(), act);
            if (dest) { destPage = api.FPDFDest_GetDestPageIndex(doc_->handle(), dest); return destPage >= 0; }
        }
        return false;
    }
    FPDF_DEST dest = api.FPDFLink_GetDest(doc_->handle(), link);
    if (dest) { destPage = api.FPDFDest_GetDestPageIndex(doc_->handle(), dest); return destPage >= 0; }
    return false;
}

// ---- selected page objects: move and resize with guides ----
void Viewer::setObjectSelection(int page, const annot::RectD* rect, std::vector<double> gx, std::vector<double> gy) {
    objDrag_ = ObjDrag::None;
    if (!rect) { objSelPage_ = -1; }
    else { objSelPage_ = page; objSel_ = objCur_ = *rect; guideX_ = std::move(gx); guideY_ = std::move(gy); }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool Viewer::pageUnclamped(int cx, int cy, int page, annot::PointD& out) const {
    FPDF_PAGE pg = doc_ ? doc_->page(page) : nullptr;
    if (!pg) return false;
    double px, py;
    pdfium::api().FPDF_DeviceToPage(pg, 0, 0, widths_[page], heights_[page], 0, cx - pageX(page), cy + doc_->scrollY - tops_[page], &px, &py);
    out = {px, py};
    return true;
}

Viewer::ObjDrag Viewer::objHit(int cx, int cy) const {
    if (objSelPage_ < 0) return ObjDrag::None;
    RECT r = const_cast<Viewer*>(this)->annotScreenRect(objSelPage_, objSel_);
    auto closeTo = [&](int x, int y) { return std::abs(cx - x) <= 7 && std::abs(cy - y) <= 7; };
    if (closeTo(r.left, r.top)) return ObjDrag::TL;
    if (closeTo(r.right, r.top)) return ObjDrag::TR;
    if (closeTo(r.left, r.bottom)) return ObjDrag::BL;
    if (closeTo(r.right, r.bottom)) return ObjDrag::BR;
    POINT p{cx, cy};
    return PtInRect(&r, p) ? ObjDrag::Move : ObjDrag::None;
}

void Viewer::drawObjectSelection(HDC dc) {
    if (objSelPage_ < 0 || objSelPage_ >= static_cast<int>(tops_.size())) return;
    RECT r = annotScreenRect(objSelPage_, objDrag_ == ObjDrag::None ? objSel_ : objCur_);
    HPEN pen = CreatePen(objDrag_ == ObjDrag::None ? PS_SOLID : PS_DASH, 2, RGB(0x30, 0x80, 0xFF));
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, r.left, r.top, r.right, r.bottom);
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(dc, white);
    for (POINT c : {POINT{r.left, r.top}, POINT{r.right, r.top}, POINT{r.left, r.bottom}, POINT{r.right, r.bottom}}) Rectangle(dc, c.x - 4, c.y - 4, c.x + 5, c.y + 5);
    if (objDrag_ != ObjDrag::None) {   // the guide lines the object snapped to
        HPEN gp = CreatePen(PS_SOLID, 1, RGB(0xE0, 0x30, 0xC0));
        SelectObject(dc, gp);
        RECT pr;
        pageRect(objSelPage_, pr);
        auto lineAt = [&](bool vertical, double v) {
            annot::RectD probe{v, v, v, v};
            RECT s = annotScreenRect(objSelPage_, probe);
            if (vertical) { MoveToEx(dc, s.left, pr.top, nullptr); LineTo(dc, s.left, pr.bottom); }
            else { MoveToEx(dc, pr.left, s.top, nullptr); LineTo(dc, pr.right, s.top); }
        };
        if (snappedX_) lineAt(true, snapX_);
        if (snappedY_) lineAt(false, snapY_);
        SelectObject(dc, pen); DeleteObject(gp);
    }
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(pen); DeleteObject(white);
}

void Viewer::finishPolygon() {
    if (polyPage_ >= 0 && polyPts_.size() >= 3 && onEvent) {
        Event e; e.kind = Event::Draw; e.tool = Tool::Polygon; e.page = polyPage_; e.pts = polyPts_;
        polyPts_.clear(); polyPage_ = -1;
        onEvent(e);
    } else { polyPts_.clear(); polyPage_ = -1; }
    InvalidateRect(hwnd_, nullptr, FALSE);
}
