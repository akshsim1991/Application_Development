#include "viewer.hpp"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

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

void Viewer::clearCache() {
    for (auto& kv : cache_) DeleteObject(kv.second.bmp);
    cache_.clear();
}

void Viewer::notify() { if (onChange_) onChange_(onChangeCtx_); }

void Viewer::setDocument(Document* d) {
    doc_ = d;
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
        if (it == cache_.end() || it->second.w != w || it->second.h != h) {
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
            FPDF_PAGE pg = api.LoadPage(doc_->handle(), page);
            FPDF_BITMAP fb = api.BitmapCreateEx(w, h, FPDFBitmap_BGRx, bits, w * 4);
            api.BitmapFillRect(fb, 0, 0, w, h, 0xFFFFFFFF);
            if (pg) { api.RenderPageBitmap(fb, pg, 0, 0, w, h, 0, FPDF_ANNOT); api.ClosePage(pg); }
            api.BitmapDestroy(fb);
            it = cache_.emplace(page, Cached{bmp, w, h, doc_->zoom}).first;
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
        FPDF_PAGE pg = api.LoadPage(doc_->handle(), page);
        FPDF_BITMAP fb = api.BitmapCreateEx(sw, sh, FPDFBitmap_BGRx, bits, sw * 4);
        api.BitmapFillRect(fb, 0, 0, sw, sh, 0xFFFFFFFF);
        if (pg) { api.RenderPageBitmap(fb, pg, x - vis.left, y - vis.top, w, h, 0, FPDF_ANNOT); api.ClosePage(pg); }
        api.BitmapDestroy(fb);
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(dc, vis.left, vis.top, sw, sh, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        DeleteObject(bmp);
    }
}

void Viewer::onPaint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd_, &ps);
    RECT rc; GetClientRect(hwnd_, &rc);

    HBRUSH bg = CreateSolidBrush(RGB(0xE8, 0xE8, 0xE8));
    FillRect(dc, &ps.rcPaint, bg);
    DeleteObject(bg);

    if (!doc_) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0x55, 0x55, 0x55));
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(dc, L"Open a PDF with Ctrl+O, or drop a file here.", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        for (size_t i = 0; i < tops_.size(); ++i) {
            int x = contentW_ <= rc.right ? (rc.right - widths_[i]) / 2 : kMargin - doc_->scrollX;
            int y = tops_[i] - doc_->scrollY;
            if (y + heights_[i] < ps.rcPaint.top) continue;
            if (y > ps.rcPaint.bottom) break;
            RECT shadow{x + 2, y + 2, x + widths_[i] + 2, y + heights_[i] + 2};
            HBRUSH sb = CreateSolidBrush(RGB(0xC8, 0xC8, 0xC8));
            FillRect(dc, &shadow, sb);
            DeleteObject(sb);
            drawPage(dc, static_cast<int>(i), x, y, widths_[i], heights_[i], ps.rcPaint);
        }
    }
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
        case WM_LBUTTONDOWN: SetFocus(hwnd_); return 0;
    }
    return DefWindowProcW(hwnd_, m, w, l);
}
