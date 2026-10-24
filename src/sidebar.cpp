#include "sidebar.hpp"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

bool Sidebar::registerClasses(HINSTANCE inst) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    bool ok = RegisterClassExW(&wc) != 0;
    wc.lpfnWndProc = thumbProc;
    wc.lpszClassName = kThumbClass;
    return RegisterClassExW(&wc) != 0 && ok;
}

HWND Sidebar::create(HWND parent, HINSTANCE inst) {
    hwnd_ = CreateWindowExW(0, kClass, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 0, 0, parent,
                            nullptr, inst, this);
    tree_ = CreateWindowExW(0, WC_TREEVIEWW, L"", WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES |
                            TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT, 0, 0, 0, 0, hwnd_,
                            reinterpret_cast<HMENU>(1), inst, nullptr);
    thumbs_ = CreateWindowExW(0, kThumbClass, L"", WS_CHILD | WS_VSCROLL, 0, 0, 0, 0, hwnd_, nullptr, inst, this);
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(9, GetDeviceCaps(GetDC(hwnd_), LOGPIXELSY), 72);
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    font_ = CreateFontIndirectW(&lf);
    SendMessageW(tree_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return hwnd_;
}

LRESULT CALLBACK Sidebar::proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Sidebar* self;
    if (m == WM_NCCREATE) {
        self = static_cast<Sidebar*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Sidebar*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self && !self->hwnd_) self->hwnd_ = h;
    return self ? self->handle(m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT CALLBACK Sidebar::thumbProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Sidebar* self;
    if (m == WM_NCCREATE) {
        self = static_cast<Sidebar*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Sidebar*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self && !self->thumbs_) self->thumbs_ = h;   // the handle is needed before CreateWindowEx returns
    return self ? self->handleThumbs(m, w, l) : DefWindowProcW(h, m, w, l);
}

void Sidebar::layout() {
    RECT rc; GetClientRect(hwnd_, &rc);
    MoveWindow(tree_, 0, kHeaderH, rc.right, std::max<LONG>(0, rc.bottom - kHeaderH), TRUE);
    MoveWindow(thumbs_, 0, kHeaderH, rc.right, std::max<LONG>(0, rc.bottom - kHeaderH), TRUE);
    thumbLayout();
}

void Sidebar::setMode(Mode m) {
    mode_ = m;
    ShowWindow(tree_, m == Mode::Bookmarks ? SW_SHOW : SW_HIDE);
    ShowWindow(thumbs_, m == Mode::Pages ? SW_SHOW : SW_HIDE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Sidebar::addTreeItems(HTREEITEM parent, const std::vector<Bookmark>& list) {
    for (const Bookmark& b : list) {
        TVINSERTSTRUCTW is{};
        is.hParent = parent;
        is.hInsertAfter = TVI_LAST;
        is.item.mask = TVIF_TEXT | TVIF_PARAM;
        is.item.pszText = const_cast<wchar_t*>(b.title.empty() ? L"(untitled)" : b.title.c_str());
        is.item.lParam = b.page;
        HTREEITEM it = reinterpret_cast<HTREEITEM>(SendMessageW(tree_, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&is)));
        if (it && !b.children.empty()) addTreeItems(it, b.children);
    }
}

void Sidebar::fillTree() {
    SendMessageW(tree_, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(tree_);
    hasBookmarks_ = 0;
    if (doc_) {
        auto bms = doc_->bookmarks();
        hasBookmarks_ = static_cast<int>(bms.size());
        addTreeItems(TVI_ROOT, bms);
        // Expand the first level so the structure is visible at once.
        for (HTREEITEM it = TreeView_GetRoot(tree_); it; it = TreeView_GetNextSibling(tree_, it))
            TreeView_Expand(tree_, it, TVE_EXPAND);
        TreeView_EnsureVisible(tree_, TreeView_GetRoot(tree_));
    }
    SendMessageW(tree_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(tree_, nullptr, TRUE);
}

void Sidebar::clearThumbs() {
    for (auto& kv : thumbCache_) DeleteObject(kv.second);
    thumbCache_.clear();
}

void Sidebar::setDocument(Document* d) {
    doc_ = d;
    clearThumbs();
    scroll_ = 0;
    current_ = 0;
    fillTree();
    thumbLayout();
    InvalidateRect(thumbs_, nullptr, TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Sidebar::refresh() {
    clearThumbs();
    fillTree();
    thumbLayout();
    InvalidateRect(thumbs_, nullptr, TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Sidebar::setCurrentPage(int page) {
    if (page == current_) return;
    current_ = page;
    if (doc_ && page >= 0 && page < static_cast<int>(tops_.size())) {
        RECT rc; GetClientRect(thumbs_, &rc);
        if (tops_[page] < scroll_ || tops_[page] + heights_[page] > scroll_ + rc.bottom) {
            scroll_ = std::clamp(tops_[page] - kPad, 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
            SetScrollPos(thumbs_, SB_VERT, scroll_, TRUE);
        }
    }
    InvalidateRect(thumbs_, nullptr, FALSE);
}

LRESULT Sidebar::handle(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_SIZE: layout(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd_, &ps);
            RECT rc; GetClientRect(hwnd_, &rc);
            RECT hr{0, 0, rc.right, kHeaderH};
            HBRUSH bg = CreateSolidBrush(RGB(0xF2, 0xF2, 0xF2));
            FillRect(dc, &hr, bg);
            DeleteObject(bg);
            SetBkMode(dc, TRANSPARENT);
            SelectObject(dc, font_);
            const wchar_t* names[2] = {L"Bookmarks", L"Pages"};
            for (int i = 0; i < 2; ++i) {
                RECT r{i * rc.right / 2, 0, (i + 1) * rc.right / 2, kHeaderH};
                bool act = (i == 0) == (mode_ == Mode::Bookmarks);
                SetTextColor(dc, act ? RGB(0x10, 0x10, 0x10) : RGB(0x70, 0x70, 0x70));
                DrawTextW(dc, names[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (act) {
                    RECT u{r.left + 12, kHeaderH - 3, r.right - 12, kHeaderH};
                    HBRUSH ub = CreateSolidBrush(RGB(0x30, 0x80, 0xFF));
                    FillRect(dc, &u, ub);
                    DeleteObject(ub);
                }
            }
            if (mode_ == Mode::Bookmarks && doc_ && hasBookmarks_ == 0) {
                RECT r{10, kHeaderH + 20, rc.right - 10, kHeaderH + 80};
                SetTextColor(dc, RGB(0x70, 0x70, 0x70));
                DrawTextW(dc, L"This document has no bookmarks.", -1, &r, DT_CENTER | DT_WORDBREAK);
            }
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (GET_Y_LPARAM(l) < kHeaderH) {
                RECT rc; GetClientRect(hwnd_, &rc);
                setMode(GET_X_LPARAM(l) < rc.right / 2 ? Mode::Bookmarks : Mode::Pages);
            }
            return 0;
        }
        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<NMHDR*>(l);
            if (nm->hwndFrom == tree_ && nm->code == TVN_SELCHANGEDW) {
                auto* tv = reinterpret_cast<NMTREEVIEWW*>(l);
                if (tv->action != TVC_UNKNOWN) navigate(static_cast<int>(tv->itemNew.lParam));
            }
            return 0;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN: return SendMessageW(GetParent(hwnd_), m, w, l);
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

// ---- thumbnails ----

void Sidebar::thumbLayout() {
    tops_.clear(); heights_.clear();
    totalH_ = 0;
    if (!doc_) return;
    int y = kPad;
    for (int i = 0; i < doc_->pageCount(); ++i) {
        const PageSize& ps = doc_->pageSize(i);
        int h = std::clamp(static_cast<int>(std::lround(kThumbW * ps.h / ps.w)), 40, 260);
        tops_.push_back(y); heights_.push_back(h);
        y += h + kLabelH + kPad;
    }
    totalH_ = y;
    RECT rc; GetClientRect(thumbs_, &rc);
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0; si.nMax = std::max(0, totalH_ - 1); si.nPage = rc.bottom;
    scroll_ = std::clamp(scroll_, 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
    si.nPos = scroll_;
    SetScrollInfo(thumbs_, SB_VERT, &si, TRUE);
}

void Sidebar::thumbPaint() {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(thumbs_, &ps);
    RECT rc; GetClientRect(thumbs_, &rc);
    HBRUSH bg = CreateSolidBrush(RGB(0xE8, 0xE8, 0xE8));
    FillRect(dc, &ps.rcPaint, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, font_);
    if (doc_) {
        const auto& api = pdfium::api();
        const int x = (rc.right - kThumbW) / 2;
        for (size_t i = 0; i < tops_.size(); ++i) {
            int y = tops_[i] - scroll_;
            if (y + heights_[i] + kLabelH < ps.rcPaint.top) continue;
            if (y > ps.rcPaint.bottom) break;
            const int h = heights_[i];
            auto it = thumbCache_.find(static_cast<int>(i));
            if (it == thumbCache_.end()) {
                if (thumbCache_.size() > 60) {   // keep memory bounded: drop the farthest from view
                    int farthest = thumbCache_.begin()->first;
                    for (auto& kv : thumbCache_) if (std::abs(kv.first - static_cast<int>(i)) > std::abs(farthest - static_cast<int>(i))) farthest = kv.first;
                    DeleteObject(thumbCache_[farthest]); thumbCache_.erase(farthest);
                }
                BITMAPINFO bi{};
                bi.bmiHeader = {sizeof(BITMAPINFOHEADER), kThumbW, -h, 1, 32, BI_RGB};
                void* bits = nullptr;
                HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (bmp) {
                    FPDF_PAGE pg = doc_->page(static_cast<int>(i));
                    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(kThumbW, h, FPDFBitmap_BGRx, bits, kThumbW * 4);
                    api.FPDFBitmap_FillRect(fb, 0, 0, kThumbW, h, 0xFFFFFFFF);
                    if (pg) api.FPDF_RenderPageBitmap(fb, pg, 0, 0, kThumbW, h, 0, FPDF_ANNOT);
                    api.FPDFBitmap_Destroy(fb);
                    it = thumbCache_.emplace(static_cast<int>(i), bmp).first;
                }
            }
            if (static_cast<int>(i) == current_) {
                RECT sel{x - 5, y - 5, x + kThumbW + 5, y + h + kLabelH + 2};
                HBRUSH sb = CreateSolidBrush(RGB(0x30, 0x80, 0xFF));
                FillRect(dc, &sel, sb);
                DeleteObject(sb);
            }
            if (it != thumbCache_.end()) {
                HDC mem = CreateCompatibleDC(dc);
                HGDIOBJ old = SelectObject(mem, it->second);
                BitBlt(dc, x, y, kThumbW, h, mem, 0, 0, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
            }
            wchar_t num[16];
            swprintf(num, 16, L"%d", static_cast<int>(i) + 1);
            RECT lr{x, y + h + 1, x + kThumbW, y + h + kLabelH};
            SetTextColor(dc, static_cast<int>(i) == current_ ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x30, 0x30, 0x30));
            DrawTextW(dc, num, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    EndPaint(thumbs_, &ps);
}

LRESULT Sidebar::handleThumbs(UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: thumbPaint(); return 0;
        case WM_SIZE: thumbLayout(); return 0;
        case WM_VSCROLL: {
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(thumbs_, SB_VERT, &si);
            int pos = si.nPos;
            switch (LOWORD(w)) {
                case SB_LINEUP: pos -= 40; break;
                case SB_LINEDOWN: pos += 40; break;
                case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
                case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
            }
            RECT rc; GetClientRect(thumbs_, &rc);
            scroll_ = std::clamp(pos, 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
            SetScrollPos(thumbs_, SB_VERT, scroll_, TRUE);
            InvalidateRect(thumbs_, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            RECT rc; GetClientRect(thumbs_, &rc);
            scroll_ = std::clamp(scroll_ - GET_WHEEL_DELTA_WPARAM(w), 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
            SetScrollPos(thumbs_, SB_VERT, scroll_, TRUE);
            InvalidateRect(thumbs_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(thumbs_);
            int y = GET_Y_LPARAM(l) + scroll_;
            for (size_t i = 0; i < tops_.size(); ++i)
                if (y >= tops_[i] - 4 && y <= tops_[i] + heights_[i] + kLabelH) { navigate(static_cast<int>(i)); break; }
            return 0;
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN: return SendMessageW(GetParent(GetParent(thumbs_)), m, w, l);
    }
    return DefWindowProcW(thumbs_, m, w, l);
}
