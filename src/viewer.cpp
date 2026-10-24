#include "viewer.hpp"
#include "theme.hpp"
#include <windowsx.h>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cwctype>

namespace {
constexpr int kCacheLimit = 6;
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
    hwnd_ = CreateWindowExW(0, kClass, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS,
                            0, 0, 0, 0, parent, nullptr, inst, this);
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
    return contentW_ <= rc.right ? (rc.right - widths_[i]) / 2 : kMargin - doc_->scrollX;
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
    if (doc_) SetScrollPos(hwnd_, SB_VERT, doc_->scrollY, FALSE);
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::relayout() {
    tops_.clear(); widths_.clear(); heights_.clear();
    contentW_ = contentH_ = 0;
    if (!doc_) { updateScrollBars(); return; }
    if (doc_->fitWidth) {
        RECT rc; GetClientRect(hwnd_, &rc);
        double maxW = 1;
        for (int i = 0; i < doc_->pageCount(); ++i) maxW = std::max<double>(maxW, doc_->pageSize(i).w);
        double avail = std::max<LONG>(rc.right, 200) - 2.0 * kMargin;
        doc_->zoom = std::clamp(avail / (maxW * 96.0 / 72.0), kMinZoom, kMaxZoom);
    }
    const double s = pxPerPt();
    int y = kMargin;
    for (int i = 0; i < doc_->pageCount(); ++i) {
        int w = std::max(1, static_cast<int>(std::lround(doc_->pageSize(i).w * s)));
        int h = std::max(1, static_cast<int>(std::lround(doc_->pageSize(i).h * s)));
        tops_.push_back(y); widths_.push_back(w); heights_.push_back(h);
        contentW_ = std::max(contentW_, w + 2 * kMargin);
        y += h + kGap;
    }
    contentH_ = y - kGap + kMargin;
    updateScrollBars();
}

void Viewer::updateScrollBars() {
    RECT rc; GetClientRect(hwnd_, &rc);
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0;
    si.nMax = std::max(0, contentH_ - 1);
    si.nPage = rc.bottom;
    si.nPos = doc_ ? doc_->scrollY : 0;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
    si.nMax = std::max(0, contentW_ - 1);
    si.nPage = rc.right;
    si.nPos = doc_ ? doc_->scrollX : 0;
    SetScrollInfo(hwnd_, SB_HORZ, &si, TRUE);
}

void Viewer::clampScroll() {
    if (!doc_) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    doc_->scrollY = std::clamp(doc_->scrollY, 0, static_cast<int>(std::max<LONG>(0, contentH_ - rc.bottom)));
    doc_->scrollX = std::clamp(doc_->scrollX, 0, static_cast<int>(std::max<LONG>(0, contentW_ - rc.right)));
}

void Viewer::setZoom(double z) {
    if (!doc_) return;
    z = std::clamp(z, kMinZoom, kMaxZoom);
    RECT rc; GetClientRect(hwnd_, &rc);
    // Keep the middle of the window on the same spot of the document.
    double rel = contentH_ > 0 ? (doc_->scrollY + rc.bottom / 2.0) / contentH_ : 0;
    doc_->fitWidth = false;
    doc_->zoom = z;
    clearCache();
    relayout();
    doc_->scrollY = static_cast<int>(rel * contentH_ - rc.bottom / 2.0);
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
    if (!doc_) return;
    RECT rc; GetClientRect(hwnd_, &rc);
    int p = currentPage();
    const PageSize& ps = doc_->pageSize(p);
    double zw = (rc.right - 2.0 * kMargin) / (ps.w * 96.0 / 72.0);
    double zh = (rc.bottom - 2.0 * kMargin) / (ps.h * 96.0 / 72.0);
    setZoom(std::min(zw, zh));
    goToPage(p);
}

int Viewer::currentPage() const {
    if (!doc_ || tops_.empty()) return 0;
    RECT rc; GetClientRect(hwnd_, &rc);
    int probe = doc_->scrollY + rc.bottom / 3;
    int idx = static_cast<int>(std::upper_bound(tops_.begin(), tops_.end(), probe) - tops_.begin()) - 1;
    return std::clamp(idx, 0, static_cast<int>(tops_.size()) - 1);
}

void Viewer::goToPage(int index) {
    if (!doc_ || tops_.empty()) return;
    index = std::clamp(index, 0, static_cast<int>(tops_.size()) - 1);
    doc_->scrollY = tops_[index] - kMargin;
    clampScroll();
    updateScrollBars();
    InvalidateRect(hwnd_, nullptr, FALSE);
    notify();
}

void Viewer::scrollBy(int dy) {
    if (!doc_) return;
    doc_->scrollY += dy;
    clampScroll();
    SetScrollPos(hwnd_, SB_VERT, doc_->scrollY, TRUE);
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
            if (cache_.size() >= kCacheLimit) {   // drop the page farthest from the current one
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
            filterPixels(bits, w, h);
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
        filterPixels(bits, sw, sh);
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

void Viewer::drawOverlays(HDC dc, int page, int, int, int, int, const RECT& clip) {
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
            paintRange(it->start, it->count, cur ? RGB(0xFF, 0x8C, 0x00) : RGB(0xFF, 0xE0, 0x00), cur ? 150 : 110);
        }
    }
    if (sel_.active) {
        int p1 = sel_.p1, c1 = sel_.c1, p2 = sel_.p2, c2 = sel_.c2;
        if (p1 > p2 || (p1 == p2 && c1 > c2)) { std::swap(p1, p2); std::swap(c1, c2); }
        if (page >= p1 && page <= p2) {
            int start = page == p1 ? c1 : 0;
            int end = page == p2 ? c2 : doc_->charCount(page) - 1;
            if (end >= start) paintRange(start, end - start + 1, RGB(0x30, 0x80, 0xFF), 90);
        }
    }
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
    auto rects = rectsFor(h.page, h.start, h.count);   // client coordinates at the current scroll
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

// ---- text selection ----

bool Viewer::charAt(int cx, int cy, int& page, int& ch, bool nearest) const {
    if (!doc_ || tops_.empty()) return false;
    const int contentY = cy + doc_->scrollY;
    int idx = static_cast<int>(std::upper_bound(tops_.begin(), tops_.end(), contentY) - tops_.begin()) - 1;
    idx = std::clamp(idx, 0, static_cast<int>(tops_.size()) - 1);
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

// Night mode inverts the page into soft light-on-dark; dimmed mode turns the white into a warm light grey.
void Viewer::filterPixels(void* bits, int w, int h) const {
    if (colours_ == Colours::Normal) return;
    auto* p = static_cast<unsigned char*>(bits);
    const size_t n = static_cast<size_t>(w) * h;
    if (colours_ == Colours::Dark) {
        for (size_t i = 0; i < n; ++i, p += 4) {
            for (int k = 0; k < 3; ++k) p[k] = static_cast<unsigned char>(0x1C + (255 - p[k]) * (0xE4 - 0x1C) / 255);
        }
    } else {
        for (size_t i = 0; i < n; ++i, p += 4) {
            p[0] = static_cast<unsigned char>(p[0] * 205 / 255);   // blue
            p[1] = static_cast<unsigned char>(p[1] * 232 / 255);   // green
            p[2] = static_cast<unsigned char>(p[2] * 240 / 255);   // red
        }
    }
}

void Viewer::setTool(Tool t) {
    tool_ = t;
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
    if (idx < 0 || contentY > tops_[idx] + heights_[idx]) return false;
    if (cx < pageX(idx) || cx > pageX(idx) + widths_[idx]) return false;
    page = idx;
    return true;
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
    if (!drawing_) return;
    const int w = std::max(1, static_cast<int>(std::lround(penWidth_ * pxPerPt())));
    HPEN pen = CreatePen(tool_ == Tool::Redact ? PS_DASH : PS_SOLID, tool_ == Tool::Redact ? 1 : w, tool_ == Tool::Redact ? RGB(0xD0, 0x10, 0x10) : penColor_);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    switch (tool_) {
        case Tool::Rectangle: case Tool::Redact: Rectangle(dc, drawStart_.x, drawStart_.y, drawNow_.x, drawNow_.y); break;
        case Tool::Ellipse: Ellipse(dc, drawStart_.x, drawStart_.y, drawNow_.x, drawNow_.y); break;
        case Tool::Line: case Tool::Arrow:
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

    HBRUSH bg = CreateSolidBrush(pal().dark || colours_ == Colours::Dark ? pal().view : RGB(0xE8, 0xE8, 0xE8));
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
            int x = pageX(static_cast<int>(i));
            int y = tops_[i] - doc_->scrollY;
            if (y + heights_[i] < ps.rcPaint.top) continue;
            if (y > ps.rcPaint.bottom) break;
            RECT shadow{x + 2, y + 2, x + widths_[i] + 2, y + heights_[i] + 2};
            HBRUSH sb = CreateSolidBrush(RGB(0xC8, 0xC8, 0xC8));
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
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(hwnd_, vert ? SB_VERT : SB_HORZ, &si);
            int pos = si.nPos;
            switch (LOWORD(w)) {
                case SB_LINEUP: pos -= 40; break;
                case SB_LINEDOWN: pos += 40; break;
                case SB_PAGEUP: pos -= static_cast<int>(si.nPage) * 9 / 10; break;
                case SB_PAGEDOWN: pos += static_cast<int>(si.nPage) * 9 / 10; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
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
            if (GET_KEYSTATE_WPARAM(w) & MK_CONTROL) {
                setZoom(doc_->zoom * (delta > 0 ? 1.1 : 1 / 1.1));
            } else {
                scrollBy(-delta * 3 / 2);
            }
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT && doc_) {
                LPCWSTR cur = tool_ == Tool::None ? IDC_IBEAM : IDC_CROSS;
                if (tool_ == Tool::None && doc_->hasForm()) {
                    POINT p; GetCursorPos(&p); ScreenToClient(hwnd_, &p);
                    int pg; annot::PointD pt;
                    int ft = formFieldAt(p.x, p.y, pg, pt);
                    if (ft == FPDF_FORMFIELD_TEXTFIELD) cur = IDC_IBEAM;
                    else if (ft >= 0) cur = IDC_HAND;
                }
                SetCursor(LoadCursor(nullptr, cur));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd_);
            if (!doc_) return 0;
            const int cx = GET_X_LPARAM(l), cy = GET_Y_LPARAM(l);
            int page, ch;
            if (tool_ != Tool::None) {
                annot::PointD pt;
                if (pageAt(cx, cy, page) && toPage(cx, cy, page, pt)) {
                    drawing_ = true; drawPage_ = page;
                    drawStart_ = drawNow_ = {cx, cy};
                    drawPts_.assign(1, pt);
                    SetCapture(hwnd_);
                }
                return 0;
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
            if (charAt(cx, cy, page, ch, false)) {
                anchorPage_ = page; anchorChar_ = ch;
                dragging_ = true;
                SetCapture(hwnd_);
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (!doc_) return 0;
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
        case WM_MOUSEMOVE:
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
                if (tool_ == Tool::Pen && toPage(cx, cy, drawPage_, pt)) drawPts_.push_back(pt);
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
                    if (tool_ == Tool::Comment || tool_ == Tool::AddText || tool_ == Tool::EditText || tool_ == Tool::Picture || tool_ == Tool::Stamp) { e.kind = Event::Click; e.pts = {drawPts_.front()}; onEvent(e); }
                    else if (moved) {
                        e.kind = Event::Draw;
                        e.pts = tool_ == Tool::Pen ? drawPts_ : std::vector<annot::PointD>{drawPts_.front(), end};
                        onEvent(e);
                    }
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            if (dragging_) { dragging_ = false; ReleaseCapture(); }
            return 0;
        case WM_CHAR:
            if (formFocus_ && doc_ && w >= 0x20) {
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
