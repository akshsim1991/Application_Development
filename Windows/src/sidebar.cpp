#include "sidebar.hpp"
#include "theme.hpp"
#include "themeui.hpp"
#include <windowsx.h>
#include <uxtheme.h>
#include <algorithm>
#include <cmath>
#include <ctime>

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
    history_ = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                               0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(2), inst, nullptr);
    themeui::flatScroll(thumbs_);
    LOGFONTW lf{};
    {   // the screen's dots per inch (the DC is given back)
                HDC sdc = GetDC(hwnd_);
                lf.lfHeight = -MulDiv(9, GetDeviceCaps(sdc, LOGPIXELSY), 72);
                ReleaseDC(hwnd_, sdc);
            }
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    font_ = CreateFontIndirectW(&lf);
    SendMessageW(tree_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SendMessageW(history_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SendMessageW(history_, LB_SETITEMHEIGHT, 0, -lf.lfHeight * 2 + 16);
    SetTimer(hwnd_, 7, 700, nullptr);   // the history follows the document's changes
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
    MoveWindow(history_, 0, kHeaderH, rc.right, std::max<LONG>(0, rc.bottom - kHeaderH), TRUE);
    thumbLayout();
}

void Sidebar::setMode(Mode m) {
    mode_ = m;
    ShowWindow(tree_, m == Mode::Bookmarks && hasBookmarks_ > 0 ? SW_SHOW : SW_HIDE);   // else the hint behind it shows
    ShowWindow(thumbs_, m == Mode::Pages ? SW_SHOW : SW_HIDE);
    ShowWindow(history_, m == Mode::History ? SW_SHOW : SW_HIDE);
    if (m == Mode::History) { historyRev_ = -1; fillHistory(); }
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
    ShowWindow(tree_, mode_ == Mode::Bookmarks && hasBookmarks_ > 0 ? SW_SHOW : SW_HIDE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Sidebar::clearThumbs() {
    for (auto& kv : thumbCache_) DeleteObject(kv.second);
    thumbCache_.clear();
}

void Sidebar::setDocument(Document* d) {
    doc_ = d;
    historyRev_ = -1;
    if (mode_ == Mode::History) fillHistory();
    clearThumbs();
    scroll_ = 0;
    current_ = 0;
    fillTree();
    thumbLayout();
    InvalidateRect(thumbs_, nullptr, TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Sidebar::refresh() {
    historyRev_ = -1;
    if (mode_ == Mode::History) fillHistory();
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
            themeui::setScrollPos(thumbs_, SB_VERT, scroll_);
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
            HDC screenDc = BeginPaint(hwnd_, &ps);
            RECT rc; GetClientRect(hwnd_, &rc);
            PaintBuf buf(screenDc, rc, ps.rcPaint);
            HDC dc = buf.dc;
            RECT hr = rc;
            HBRUSH bg = CreateSolidBrush(pal().panel);
            FillRect(dc, &hr, bg);
            DeleteObject(bg);
            SetBkMode(dc, TRANSPARENT);
            SelectObject(dc, font_);
            const wchar_t* names[3] = {L"Bookmarks", L"Pages", L"History"};
            for (int i = 0; i < 3; ++i) {
                RECT r{i * rc.right / 3, 0, (i + 1) * rc.right / 3, kHeaderH};
                bool act = i == static_cast<int>(mode_);
                SetTextColor(dc, act ? pal().text : pal().textDim);
                DrawTextW(dc, names[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (act) {
                    RECT u{r.left + 12, kHeaderH - 3, r.right - 12, kHeaderH};
                    HBRUSH ub = CreateSolidBrush(RGB(0x30, 0x80, 0xFF));
                    FillRect(dc, &u, ub);
                    DeleteObject(ub);
                }
            }
            if (mode_ == Mode::Bookmarks && doc_ && hasBookmarks_ == 0) {
                RECT r{14, kHeaderH + 20, rc.right - 14, kHeaderH + 160};
                SetTextColor(dc, pal().textDim);
                DrawTextW(dc, L"This document has no bookmarks.\n\nTo add one, go to the page and choose Edit PDF > Add a bookmark for this page.",
                          -1, &r, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
            }
            buf.done();
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (GET_Y_LPARAM(l) < kHeaderH) {
                RECT rc; GetClientRect(hwnd_, &rc);
                const int third = std::clamp(static_cast<int>(GET_X_LPARAM(l) * 3 / std::max<LONG>(1, rc.right)), 0, 2);
                setMode(static_cast<Mode>(third));
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
        case WM_TIMER:
            if (w == 7 && mode_ == Mode::History && IsWindowVisible(hwnd_)) fillHistory();
            return 0;
        case WM_COMMAND:
            if (reinterpret_cast<HWND>(l) == history_ && HIWORD(w) == LBN_SELCHANGE) {
                const int k = static_cast<int>(SendMessageW(history_, LB_GETCURSEL, 0, 0));
                if (k >= 0 && k != historyNow_ && onHistory) onHistory(historyNow_ - k);
            }
            return 0;
        case WM_MEASUREITEM: return TRUE;
        case WM_DRAWITEM: {
            auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (di->hwndItem != history_ || di->itemID == static_cast<UINT>(-1)) return TRUE;
            wchar_t text[300] = L"";
            SendMessageW(history_, LB_GETTEXT, di->itemID, reinterpret_cast<LPARAM>(text));
            const int k = static_cast<int>(di->itemID);
            const bool now = k == historyNow_, undone = k > historyNow_;
            HBRUSH b = CreateSolidBrush(now ? (pal().dark ? RGB(0x24, 0x40, 0x70) : RGB(0xD6, 0xE6, 0xFF)) : pal().panel);
            FillRect(di->hDC, &di->rcItem, b);
            DeleteObject(b);
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, undone ? pal().textDim : pal().text);
            RECT r = di->rcItem;
            r.left += 12; r.right -= 6;
            std::wstring t = text;
            const size_t tab = t.find(L'\t');
            const std::wstring when = tab == std::wstring::npos ? L"" : t.substr(0, tab), what = tab == std::wstring::npos ? t : t.substr(tab + 1);
            RECT top = r; top.bottom = (r.top + r.bottom) / 2 + 2;
            RECT bottom = r; bottom.top = (r.top + r.bottom) / 2 - 1;
            DrawTextW(di->hDC, what.c_str(), -1, &top, DT_SINGLELINE | DT_BOTTOM | DT_END_ELLIPSIS | DT_NOPREFIX);
            SetTextColor(di->hDC, pal().textDim);
            const std::wstring sep = when.empty() ? L"" : L"  \u00B7 ";
            DrawTextW(di->hDC, (when + (undone ? sep + L"undone: click to redo" : now ? sep + L"now" : L"")).c_str(), -1, &bottom, DT_SINGLELINE | DT_TOP | DT_END_ELLIPSIS | DT_NOPREFIX);
            return TRUE;
        }
        case WM_CTLCOLORLISTBOX: {
            if (panelBrush_) DeleteObject(panelBrush_);
            panelBrush_ = CreateSolidBrush(pal().panel);
            return reinterpret_cast<LRESULT>(panelBrush_);
        }
        case WM_KEYDOWN: case WM_SYSKEYDOWN: return SendMessageW(GetParent(hwnd_), m, w, l);
    }
    return DefWindowProcW(hwnd_, m, w, l);
}

// ---- the undo history: the start, each change (oldest first), then the changes that were undone ----

void Sidebar::fillHistory() {
    if (!history_) return;
    const int rev = doc_ ? doc_->revision() : -2;
    if (rev == historyRev_) return;
    historyRev_ = rev;
    SendMessageW(history_, WM_SETREDRAW, FALSE, 0);
    SendMessageW(history_, LB_RESETCONTENT, 0, 0);
    historyNow_ = 0;
    if (doc_) {
        auto clock = [](long long t) {
            if (t <= 0) return std::wstring();
            const time_t tt = static_cast<time_t>(t);
            tm lt{};
            localtime_s(&lt, &tt);
            wchar_t b[16];
            swprintf(b, 16, L"%02d:%02d:%02d", lt.tm_hour, lt.tm_min, lt.tm_sec);
            return std::wstring(b);
        };
        const auto done = doc_->undoSteps(), undone = doc_->redoSteps();
        SendMessageW(history_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(done.size() >= 30 ? L"\tEarlier changes (no longer kept)" : L"\tAs opened"));
        for (const auto& st : done) SendMessageW(history_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((clock(st.when) + L"\t" + st.what).c_str()));
        historyNow_ = static_cast<int>(done.size());
        for (const auto& st : undone) SendMessageW(history_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((clock(st.when) + L"\t" + st.what).c_str()));
        SendMessageW(history_, LB_SETCURSEL, static_cast<WPARAM>(historyNow_), 0);
    }
    SendMessageW(history_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(history_, nullptr, TRUE);
}

// ---- thumbnails ----

int Sidebar::thumbAt(int y) const {
    const int cy = y + scroll_;
    for (size_t i = 0; i < tops_.size(); ++i)
        if (cy >= tops_[i] - 4 && cy <= tops_[i] + heights_[i] + kLabelH) return static_cast<int>(i);
    return -1;
}

// The gap between thumbnails nearest to y: the page index the dragged pages would go before.
int Sidebar::dropIndexAt(int y) const {
    const int cy = y + scroll_;
    for (size_t i = 0; i < tops_.size(); ++i)
        if (cy < tops_[i] + heights_[i] / 2) return static_cast<int>(i);
    return static_cast<int>(tops_.size());
}

void Sidebar::selectAll() {
    sel_.clear();
    for (size_t i = 0; i < tops_.size(); ++i) sel_.insert(static_cast<int>(i));
    InvalidateRect(thumbs_, nullptr, FALSE);
}

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
    for (auto it = sel_.begin(); it != sel_.end();) it = (*it >= static_cast<int>(tops_.size())) ? sel_.erase(it) : std::next(it);
    RECT rc; GetClientRect(thumbs_, &rc);
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0; si.nMax = std::max(0, totalH_ - 1); si.nPage = rc.bottom;
    scroll_ = std::clamp(scroll_, 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
    si.nPos = scroll_;
    themeui::setScrollInfo(thumbs_, SB_VERT, si);
}

void Sidebar::thumbPaint() {
    PAINTSTRUCT ps;
    HDC screenDc = BeginPaint(thumbs_, &ps);
    RECT rc; GetClientRect(thumbs_, &rc);
    PaintBuf buf(screenDc, rc, ps.rcPaint);
    HDC dc = buf.dc;
    HBRUSH bg = CreateSolidBrush(pal().panelDark);
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
                    if (pg) doc_->renderPage(static_cast<int>(i), fb, 0, 0, kThumbW, h);
                    api.FPDFBitmap_Destroy(fb);
                    it = thumbCache_.emplace(static_cast<int>(i), bmp).first;
                }
            }
            const bool selected = sel_.count(static_cast<int>(i)) != 0;
            if (selected || static_cast<int>(i) == current_) {
                RECT sel{x - 5, y - 5, x + kThumbW + 5, y + h + kLabelH + 2};
                HBRUSH sb = CreateSolidBrush(selected ? pal().accent : pal().accentSoft);
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
            SetTextColor(dc, selected ? RGB(0xFF, 0xFF, 0xFF) : pal().thumbText);
            DrawTextW(dc, num, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    if (dragging_ && dropBefore_ >= 0 && !tops_.empty()) {   // where the dragged pages would land
        int y = dropBefore_ < static_cast<int>(tops_.size()) ? tops_[dropBefore_] - kPad / 2 - scroll_ : totalH_ - kPad / 2 - scroll_;
        RECT line{6, y - 1, rc.right - 6, y + 2};
        HBRUSH lb = CreateSolidBrush(RGB(0x30, 0x80, 0xFF));
        FillRect(dc, &line, lb);
        DeleteObject(lb);
    }
    buf.done();
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
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = themeui::trackPos(thumbs_, SB_VERT); break;
            }
            RECT rc; GetClientRect(thumbs_, &rc);
            scroll_ = std::clamp(pos, 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
            themeui::setScrollPos(thumbs_, SB_VERT, scroll_);
            InvalidateRect(thumbs_, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            RECT rc; GetClientRect(thumbs_, &rc);
            scroll_ = std::clamp(scroll_ - GET_WHEEL_DELTA_WPARAM(w), 0, std::max(0, totalH_ - static_cast<int>(rc.bottom)));
            themeui::setScrollPos(thumbs_, SB_VERT, scroll_);
            InvalidateRect(thumbs_, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(thumbs_);
            const int idx = thumbAt(GET_Y_LPARAM(l));
            if (idx < 0) { sel_.clear(); InvalidateRect(thumbs_, nullptr, FALSE); return 0; }
            const bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            pressed_ = true; dragging_ = false; pressIdx_ = idx;
            pressPt_ = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            SetCapture(thumbs_);
            if (ctrl) { if (!sel_.erase(idx)) sel_.insert(idx); anchor_ = idx; pressed_ = false; ReleaseCapture(); }
            else if (shift) {
                sel_.clear();
                for (int i = std::min(anchor_, idx); i <= std::max(anchor_, idx); ++i) sel_.insert(i);
                pressed_ = false; ReleaseCapture();
            } else if (!sel_.count(idx)) { sel_ = {idx}; anchor_ = idx; navigate(idx); }
            // pressing on an already selected page keeps the selection, so the group can be dragged
            InvalidateRect(thumbs_, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (pressed_ && (w & MK_LBUTTON)) {
                const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                if (!dragging_ && (std::abs(x - pressPt_.x) > 6 || std::abs(y - pressPt_.y) > 6)) dragging_ = true;
                if (dragging_) {
                    RECT rc; GetClientRect(thumbs_, &rc);
                    if (y < 20) scroll_ = std::max(0, scroll_ - 20); else if (y > rc.bottom - 20) scroll_ = std::min(std::max(0, totalH_ - static_cast<int>(rc.bottom)), scroll_ + 20);
                    themeui::setScrollPos(thumbs_, SB_VERT, scroll_);
                    dropBefore_ = dropIndexAt(y);
                    InvalidateRect(thumbs_, nullptr, FALSE);
                }
            }
            return 0;
        case WM_LBUTTONUP:
            if (pressed_) {
                pressed_ = false;
                ReleaseCapture();
                if (dragging_) {
                    dragging_ = false;
                    const int before = dropBefore_;
                    dropBefore_ = -1;
                    InvalidateRect(thumbs_, nullptr, FALSE);
                    if (before >= 0 && onReorder && !sel_.empty()) onReorder(selectedPages(), before);
                } else if (sel_.size() > 1 && sel_.count(pressIdx_)) {   // a plain click inside a group selects just that page
                    sel_ = {pressIdx_}; anchor_ = pressIdx_;
                    navigate(pressIdx_);
                    InvalidateRect(thumbs_, nullptr, FALSE);
                }
            }
            return 0;
        case WM_RBUTTONUP: {
            const int idx = thumbAt(GET_Y_LPARAM(l));
            if (idx >= 0 && !sel_.count(idx)) { sel_ = {idx}; anchor_ = idx; InvalidateRect(thumbs_, nullptr, FALSE); }
            POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ClientToScreen(thumbs_, &p);
            if (onContext) onContext(idx, p);
            return 0;
        }
        case WM_CHAR:
            if (w == 1) { selectAll(); return 0; }   // Ctrl+A
            break;
        case WM_KEYDOWN: case WM_SYSKEYDOWN: return SendMessageW(GetParent(GetParent(thumbs_)), m, w, l);
    }
    return DefWindowProcW(thumbs_, m, w, l);
}

void Sidebar::applyTheme() {
    if (tree_) {
        TreeView_SetBkColor(tree_, pal().panel);
        TreeView_SetTextColor(tree_, pal().text);
        TreeView_SetLineColor(tree_, pal().line);
        SetWindowTheme(tree_, pal().dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
    if (thumbs_) InvalidateRect(thumbs_, nullptr, TRUE);
}
