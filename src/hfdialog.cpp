#include "hfdialog.hpp"
#include "headfoot.hpp"
#include "pdfium_api.hpp"
#include "theme.hpp"
#include "themeui.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
// message boxes in the window's theme
#define MessageBoxW themeui::messageBox

namespace hfdialog {
namespace {

using pdfium::api;
using headfoot::Box;
using headfoot::Element;

enum { ID_WHAT = 100, ID_PAGES, ID_LIST_PAGES, ID_LIST, ID_PREV, ID_NEXT, ID_BEFORE, ID_AFTER, ID_DRAW, ID_DELETE, ID_CANCEL, ID_PREVIEW, ID_REMOVE_AREA };
const wchar_t* kClass = L"FeatherHfWindow";
const wchar_t* kPreviewClass = L"FeatherHfPreview";

struct State {
    Document* d = nullptr;
    headfoot::Detection det;
    HWND wnd = nullptr, preview = nullptr, list = nullptr, what = nullptr, pages = nullptr, pageList = nullptr, notes = nullptr, summary = nullptr;
    int dpi = 96;
    HFONT font = nullptr, bold = nullptr, small = nullptr;
    int page = 0;              // the page shown
    int current = 0;           // the page that was on screen (for "Current page")
    bool after = false;        // show the page as it will be
    bool drawing = false;      // the next drag draws an area
    int activeArea = -1;       // the drawn area with handles (index into det.elements)
    bool done = false, applied = false;
    headfoot::Plan plan;
    std::vector<int> chosen;
    std::wstring pageError;
    // the page as drawn in the preview: its bitmap and where it sits
    HBITMAP shot = nullptr;
    int shotPage = -1;
    bool shotAfter = false;
    bool afterFailed = false;
    double scale = 1;          // preview pixels per point
    int ox = 0, oy = 0;        // where the page starts in the preview
    double pw = 0, ph = 0;     // the page in points (as shown)
    // dragging in the preview
    enum Drag { None, New, Move, Handle } drag = None;
    int handle = -1;
    POINT down{};
    Box startBox;
    int px(int v) const { return MulDiv(v, dpi, 96); }
};

// ---- the plan and the texts that describe it
void update(State& s);

std::wstring pagesText(const State& s) {
    const int n = s.d->pageCount();
    const int which = static_cast<int>(SendMessageW(s.pages, CB_GETCURSEL, 0, 0));
    if (which == 0) return n == 1 ? L"the page" : L"all " + std::to_wstring(n) + L" pages";
    if (s.chosen.size() == 1) return L"page " + std::to_wstring(s.chosen[0] + 1);
    return std::to_wstring(s.chosen.size()) + L" pages";
}

void update(State& s) {
    wchar_t spec[256] = L"";
    GetWindowTextW(s.pageList, spec, 256);
    const int which = static_cast<int>(SendMessageW(s.pages, CB_GETCURSEL, 0, 0));
    EnableWindow(s.pageList, which == 4);
    s.pageError.clear();
    s.chosen = headfoot::choosePages(*s.d, which, s.current, spec, &s.pageError);
    s.plan = headfoot::plan(*s.d, s.det, s.chosen);
    // the notes: what could not be found, what is kept, what the drawn areas take in
    std::wstring notes;
    int noText = 0;
    for (char c : s.det.scannedNoText) noText += c ? 1 : 0;
    if (s.det.elements.empty())
        notes += s.d->pageCount() < 2 ? L"With one page nothing can be seen to repeat, so nothing was found. Draw an area round what should go.\r\n"
                                      : L"Nothing that repeats at the top or bottom of the pages was found. Draw an area round what should go.\r\n";
    if (noText) notes += std::to_wstring(noText) + L" scanned page" + (noText > 1 ? L"s have" : L" has") + L" no recognised text: only lines could be found there. "
                         L"Recognise the text first (Edit PDF > Recognise text), or draw an area.\r\n";
    if (s.plan.bodyTaken)
        notes += L"Attention: the area you drew also takes in " + std::to_wstring(s.plan.bodyTaken) + L" piece" + (s.plan.bodyTaken > 1 ? L"s" : L"") +
                 L" of the pages' own content" + (s.plan.bodySample.empty() ? L"" : L", for example \x201C" + s.plan.bodySample.substr(0, 40) + L"\x201D") + L". Make it smaller if that should stay.\r\n";
    if (s.plan.crossing)
        notes += L"Attention: " + std::to_wstring(s.plan.crossing) + L" piece" + (s.plan.crossing > 1 ? L"s run" : L" runs") + L" across the edge of the area you drew; they are kept.\r\n";
    bool locked = false;
    for (const Element& e : s.det.elements) locked = locked || e.locked;
    if (locked) notes += L"Items marked \x201Cpart of a page template\x201D are drawn together with the page's own content and are kept.\r\n";
    if (!s.pageError.empty()) notes += s.pageError + L"\r\n";
    SetWindowTextW(s.notes, notes.c_str());
    // the summary
    int kinds = 0;
    for (const Element& e : s.det.elements) if (e.selected && !e.locked) ++kinds;
    std::wstring sum;
    if (s.chosen.empty()) sum = L"No pages chosen.";
    else if (s.plan.pages.empty() || (s.plan.objects == 0 && s.plan.areas == 0)) sum = L"Nothing to delete on " + pagesText(s) + L".";
    else {
        sum = L"Deletes " + std::to_wstring(kinds) + L" item" + (kinds == 1 ? L"" : L"s") + L" on " + pagesText(s) + L": " +
              std::to_wstring(s.plan.objects) + L" object" + (s.plan.objects == 1 ? L"" : L"s");
        if (s.plan.areas) sum += L" and " + std::to_wstring(s.plan.areas) + L" area" + (s.plan.areas == 1 ? L"" : L"s") + L" of scanned pictures";
        sum += L". Bookmarks, links and comments stay.";
    }
    SetWindowTextW(s.summary, sum.c_str());
    EnableWindow(GetDlgItem(s.wnd, ID_DELETE), !s.plan.pages.empty() && (s.plan.objects > 0 || s.plan.areas > 0));
    EnableWindow(GetDlgItem(s.wnd, ID_REMOVE_AREA), s.activeArea >= 0);
    InvalidateRect(s.list, nullptr, FALSE);
    s.shotPage = -1;   // the preview is made again
    InvalidateRect(s.preview, nullptr, FALSE);
    wchar_t t[64];
    swprintf(t, 64, L"Page %d of %d", s.page + 1, s.d->pageCount());
    SetWindowTextW(GetDlgItem(s.wnd, ID_PREV + 100), t);
}

void fillList(State& s) {
    SendMessageW(s.list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < s.det.elements.size(); ++i) SendMessageW(s.list, LB_ADDSTRING, 0, static_cast<LPARAM>(i));
}

// ---- the preview
Box areaOnPage(const State& s, const Element& e) {
    Box a = e.area;
    if (e.fromBottom) { a.t += s.ph - e.refHeight; a.b += s.ph - e.refHeight; }
    return a;
}

RECT toScreen(const State& s, const Box& b) {
    return {s.ox + static_cast<int>(std::lround(b.l * s.scale)), s.oy + static_cast<int>(std::lround(b.t * s.scale)),
            s.ox + static_cast<int>(std::lround(b.r * s.scale)), s.oy + static_cast<int>(std::lround(b.b * s.scale))};
}

// the page drawn into a bitmap of the preview's size: as it is, or (after) a copy of it with the plan carried out
void makeShot(State& s, const RECT& rc) {
    if (s.shot) { DeleteObject(s.shot); s.shot = nullptr; }
    s.shotPage = s.page; s.shotAfter = s.after; s.afterFailed = false;
    FPDF_PAGE pg = s.d->page(s.page);
    if (!pg) return;
    s.pw = api().FPDF_GetPageWidthF(pg); s.ph = api().FPDF_GetPageHeightF(pg);
    const int pad = s.px(14);
    s.scale = std::min((rc.right - 2.0 * pad) / s.pw, (rc.bottom - 2.0 * pad) / s.ph);
    const int w = std::max(1, static_cast<int>(s.pw * s.scale)), h = std::max(1, static_cast<int>(s.ph * s.scale));
    s.ox = (rc.right - w) / 2; s.oy = (rc.bottom - h) / 2;
    std::unique_ptr<Document> copy;
    Document* src = s.d;
    int index = s.page;
    if (s.after) {   // a copy of this one page, with this page's plan done to it
        std::vector<unsigned char> bytes;
        copy = std::make_unique<Document>();
        bool ok = s.d->pagesToBytes({s.page}, bytes) && copy->load(std::move(bytes), {}) == OpenResult::Ok;
        if (ok && api().FPDFPage_CountObjects(copy->page(0)) != api().FPDFPage_CountObjects(pg)) ok = false;   // the same objects, in the same order
        if (ok) {
            for (const auto& pp : s.plan.pages) if (pp.page == s.page) { headfoot::PagePlan c = pp; c.page = 0; headfoot::applyPage(*copy, 0, c); }
            src = copy.get(); index = 0;
        } else s.afterFailed = true;
    }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    s.shot = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!s.shot) return;
    FPDF_BITMAP bmp = api().FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, bits, w * 4);
    api().FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
    src->renderPage(index, bmp, 0, 0, w, h, true);
    api().FPDFBitmap_Destroy(bmp);
}

void dashed(HDC dc, const RECT& r, COLORREF c, int width) {
    LOGBRUSH lb{BS_SOLID, c, 0};
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_DASH | PS_ENDCAP_FLAT, static_cast<DWORD>(width), &lb, 0, nullptr);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, r.left, r.top, r.right, r.bottom);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
}

void tint(HDC dc, const RECT& r, COLORREF c, BYTE alpha) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP b = CreateCompatibleBitmap(dc, 1, 1);
    HGDIOBJ old = SelectObject(mem, b);
    SetPixel(mem, 0, 0, c);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, 0};
    AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, mem, 0, 0, 1, 1, bf);
    SelectObject(mem, old); DeleteObject(b); DeleteDC(mem);
}

std::vector<RECT> handles(const State& s, const RECT& r) {
    const int k = s.px(4);
    const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    const POINT p[8] = {{r.left, r.top}, {cx, r.top}, {r.right, r.top}, {r.right, cy}, {r.right, r.bottom}, {cx, r.bottom}, {r.left, r.bottom}, {r.left, cy}};
    std::vector<RECT> out;
    for (const POINT& q : p) out.push_back({q.x - k, q.y - k, q.x + k + 1, q.y + k + 1});
    return out;
}

void paintPreview(State& s, HDC dc, const RECT& rc) {
    const Palette& P = pal();
    HBRUSH bg = CreateSolidBrush(P.view);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    if (s.shotPage != s.page || s.shotAfter != s.after || !s.shot) makeShot(s, rc);
    if (!s.shot) return;
    BITMAP bm; GetObjectW(s.shot, sizeof(bm), &bm);
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, s.shot);
    BitBlt(dc, s.ox, s.oy, bm.bmWidth, bm.bmHeight, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteDC(mem);
    RECT frame{s.ox - 1, s.oy - 1, s.ox + bm.bmWidth + 1, s.oy + bm.bmHeight + 1};
    HBRUSH fb = CreateSolidBrush(P.line); FrameRect(dc, &frame, fb); DeleteObject(fb);
    const bool onPage = std::binary_search(s.chosen.begin(), s.chosen.end(), s.page);
    // what was found on this page: red (and tinted) when it will go, grey dashes when it is left out
    for (size_t i = 0; i < s.det.elements.size(); ++i) {
        const Element& e = s.det.elements[i];
        const bool goes = e.selected && !e.locked && onPage;
        if (e.kind == headfoot::Kind::Area) {
            const RECT r = toScreen(s, areaOnPage(s, e));
            if (!s.after && goes) tint(dc, r, RGB(0xE5, 0x48, 0x4D), 40);
            dashed(dc, r, goes ? RGB(0xD0, 0x2A, 0x2A) : RGB(0x80, 0x80, 0x80), std::max(1, s.px(2)));
            if (static_cast<int>(i) == s.activeArea && !s.after)
                for (const RECT& h : handles(s, r)) {
                    HBRUSH wb = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF)), rb = CreateSolidBrush(RGB(0xD0, 0x2A, 0x2A));
                    FillRect(dc, &h, wb); FrameRect(dc, &h, rb);
                    DeleteObject(wb); DeleteObject(rb);
                }
            continue;
        }
        for (const auto& it : e.items) {
            if (it.page != s.page) continue;
            RECT r = toScreen(s, it.box);
            InflateRect(&r, s.px(2), s.px(2));
            if (!s.after && goes) tint(dc, r, RGB(0xE5, 0x48, 0x4D), 45);
            if (!s.after || !goes) dashed(dc, r, goes ? RGB(0xD0, 0x2A, 0x2A) : RGB(0x90, 0x90, 0x90), 1);
        }
    }
    // the rubber band of an area being drawn
    if (s.drag == State::New) {
        POINT p; GetCursorPos(&p); ScreenToClient(s.preview, &p);
        RECT r{std::min(s.down.x, p.x), std::min(s.down.y, p.y), std::max(s.down.x, p.x), std::max(s.down.y, p.y)};
        dashed(dc, r, RGB(0xD0, 0x2A, 0x2A), std::max(1, s.px(2)));
    }
    // what is shown
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, s.small);
    SetTextColor(dc, P.viewText);
    RECT t{rc.left + s.px(10), rc.top + s.px(2), rc.right - s.px(10), rc.top + s.px(14)};
    std::wstring label = s.after ? (s.afterFailed ? L"After: the preview of this page could not be made" : L"After: as the page will be")
                                 : L"Before: red is deleted, grey dashes are kept";
    if (!onPage) label = L"This page is not among the pages chosen: nothing changes here";
    DrawTextW(dc, label.c_str(), -1, &t, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
}

Box fromScreen(const State& s, RECT r) {
    Box b{(r.left - s.ox) / s.scale, (r.top - s.oy) / s.scale, (r.right - s.ox) / s.scale, (r.bottom - s.oy) / s.scale};
    b.l = std::clamp(b.l, 0.0, s.pw); b.r = std::clamp(b.r, 0.0, s.pw); b.t = std::clamp(b.t, 0.0, s.ph); b.b = std::clamp(b.b, 0.0, s.ph);
    return b;
}

void setArea(State& s, Element& e, const Box& onPage) {
    e.area = onPage;
    e.fromBottom = (onPage.t + onPage.b) / 2 > s.ph / 2;
    e.refHeight = s.ph;
    e.band = e.fromBottom ? headfoot::Band::Footer : headfoot::Band::Header;
}

LRESULT CALLBACK previewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            if (s) paintPreview(*s, mem, rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SIZE: if (s) { s->shotPage = -1; InvalidateRect(h, nullptr, FALSE); } return 0;
        case WM_SETCURSOR:
            if (s && LOWORD(l) == HTCLIENT) {
                POINT p; GetCursorPos(&p); ScreenToClient(h, &p);
                LPCWSTR cur = s->drawing ? IDC_CROSS : IDC_ARROW;
                if (s->activeArea >= 0 && !s->after) {
                    const RECT r = toScreen(*s, areaOnPage(*s, s->det.elements[static_cast<size_t>(s->activeArea)]));
                    const auto hs = handles(*s, r);
                    static const LPCWSTR sizes[8] = {IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE, IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE};
                    for (int i = 0; i < 8; ++i) if (PtInRect(&hs[static_cast<size_t>(i)], p)) cur = sizes[i];
                    if (cur == IDC_ARROW && PtInRect(&r, p)) cur = IDC_SIZEALL;
                }
                SetCursor(LoadCursorW(nullptr, cur));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN: {
            if (!s || s->after) return 0;
            SetFocus(h);
            const POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            s->down = p;
            // a handle or the inside of the drawn area with handles
            if (s->activeArea >= 0) {
                Element& e = s->det.elements[static_cast<size_t>(s->activeArea)];
                const RECT r = toScreen(*s, areaOnPage(*s, e));
                const auto hs = handles(*s, r);
                for (int i = 0; i < 8; ++i) if (PtInRect(&hs[static_cast<size_t>(i)], p)) { s->drag = State::Handle; s->handle = i; }
                if (s->drag == State::None && PtInRect(&r, p)) s->drag = State::Move;
                if (s->drag != State::None) { s->startBox = areaOnPage(*s, e); SetCapture(h); return 0; }
            }
            if (s->drawing) { s->drag = State::New; SetCapture(h); return 0; }
            // a click on something found leaves it out (or puts it back): for every page
            const Box at = fromScreen(*s, RECT{p.x, p.y, p.x, p.y});
            for (size_t i = 0; i < s->det.elements.size(); ++i) {
                Element& e = s->det.elements[i];
                bool hitIt = false;
                if (e.kind == headfoot::Kind::Area) {
                    const Box a = areaOnPage(*s, e);
                    hitIt = at.l >= a.l && at.l <= a.r && at.t >= a.t && at.t <= a.b;
                    if (hitIt) { s->activeArea = static_cast<int>(i); InvalidateRect(h, nullptr, FALSE); return 0; }
                }
                for (const auto& it : e.items)
                    if (it.page == s->page && at.l >= it.box.l - 3 && at.l <= it.box.r + 3 && at.t >= it.box.t - 3 && at.t <= it.box.b + 3) hitIt = true;
                if (hitIt && !e.locked) {
                    e.selected = !e.selected;
                    SendMessageW(s->what, CB_SETCURSEL, 4, 0);   // now "selected elements"
                    update(*s);
                    return 0;
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!s || s->drag == State::None) return 0;
            const POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            if (s->drag == State::New) { InvalidateRect(h, nullptr, FALSE); return 0; }
            Element& e = s->det.elements[static_cast<size_t>(s->activeArea)];
            const double dx = (p.x - s->down.x) / s->scale, dy = (p.y - s->down.y) / s->scale;
            Box b = s->startBox;
            if (s->drag == State::Move) { b.l += dx; b.r += dx; b.t += dy; b.b += dy; }
            else {
                const int k = s->handle;
                if (k == 0 || k == 6 || k == 7) b.l += dx;
                if (k == 2 || k == 3 || k == 4) b.r += dx;
                if (k == 0 || k == 1 || k == 2) b.t += dy;
                if (k == 4 || k == 5 || k == 6) b.b += dy;
                if (b.r < b.l + 4) b.r = b.l + 4;
                if (b.b < b.t + 4) b.b = b.t + 4;
            }
            b.l = std::clamp(b.l, 0.0, s->pw); b.r = std::clamp(b.r, 0.0, s->pw); b.t = std::clamp(b.t, 0.0, s->ph); b.b = std::clamp(b.b, 0.0, s->ph);
            setArea(*s, e, b);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (!s || s->drag == State::None) return 0;
            const State::Drag was = s->drag;
            s->drag = State::None;
            ReleaseCapture();
            if (was == State::New) {
                const POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                RECT r{std::min(s->down.x, p.x), std::min(s->down.y, p.y), std::max(s->down.x, p.x), std::max(s->down.y, p.y)};
                if (r.right - r.left > s->px(6) && r.bottom - r.top > s->px(6)) {
                    Element e;
                    e.kind = headfoot::Kind::Area;
                    setArea(*s, e, fromScreen(*s, r));
                    int n = 1;
                    for (const Element& o : s->det.elements) if (o.kind == headfoot::Kind::Area) ++n;
                    e.label = L"Area you drew " + std::to_wstring(n) + (e.fromBottom ? L" (bottom, kept at the same distance from the foot of each page)" : L" (top)");
                    s->det.elements.push_back(e);
                    s->activeArea = static_cast<int>(s->det.elements.size()) - 1;
                    fillList(*s);
                }
                s->drawing = false;
                InvalidateRect(GetDlgItem(s->wnd, ID_DRAW), nullptr, FALSE);
            }
            update(*s);
            return 0;
        }
        case WM_KEYDOWN:
            if (s && w == VK_DELETE && s->activeArea >= 0) { SendMessageW(s->wnd, WM_COMMAND, ID_REMOVE_AREA, 0); return 0; }
            break;
        case WM_GETDLGCODE: return DLGC_WANTARROWS;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---- the list: a tick box, the name, on how many pages
void drawItem(State& s, const DRAWITEMSTRUCT* di) {
    if (di->itemID == static_cast<UINT>(-1)) return;
    const Palette& P = pal();
    const Element& e = s.det.elements[di->itemID];
    RECT r = di->rcItem;
    const bool sel = (di->itemState & ODS_SELECTED) != 0;
    HBRUSH bg = CreateSolidBrush(sel ? P.hover : P.field);
    FillRect(di->hDC, &r, bg);
    DeleteObject(bg);
    const int box = s.px(16);
    RECT b{r.left + s.px(8), (r.top + r.bottom - box) / 2, r.left + s.px(8) + box, (r.top + r.bottom + box) / 2};
    const bool on = e.selected && !e.locked;
    HBRUSH fill = CreateSolidBrush(on ? P.accent : P.field);
    HPEN pen = CreatePen(PS_SOLID, 1, e.locked ? P.textOff : on ? P.accent : P.textDim);
    HGDIOBJ ob = SelectObject(di->hDC, fill), op = SelectObject(di->hDC, pen);
    RoundRect(di->hDC, b.left, b.top, b.right, b.bottom, s.px(5), s.px(5));
    SelectObject(di->hDC, ob); SelectObject(di->hDC, op);
    DeleteObject(fill); DeleteObject(pen);
    if (on) {
        HPEN tick = CreatePen(PS_SOLID, std::max(2, s.px(2)), luminance(P.accent) > 0.45 ? RGB(0x10, 0x14, 0x1C) : RGB(0xFF, 0xFF, 0xFF));
        HGDIOBJ o2 = SelectObject(di->hDC, tick);
        POINT pts[3] = {{b.left + s.px(4), b.top + s.px(8)}, {b.left + s.px(7), b.top + s.px(11)}, {b.left + s.px(12), b.top + s.px(5)}};
        Polyline(di->hDC, pts, 3);
        SelectObject(di->hDC, o2);
        DeleteObject(tick);
    }
    SetBkMode(di->hDC, TRANSPARENT);
    SelectObject(di->hDC, s.font);
    SetTextColor(di->hDC, e.locked ? P.textOff : P.text);
    RECT t{b.right + s.px(10), r.top + s.px(3), r.right - s.px(8), r.top + s.px(22)};
    DrawTextW(di->hDC, e.label.c_str(), -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(di->hDC, s.small);
    SetTextColor(di->hDC, P.textDim);
    RECT t2{t.left, t.bottom, t.right, r.bottom - s.px(2)};
    std::wstring sub;
    if (e.kind == headfoot::Kind::Area) sub = L"On every chosen page. Drag its edges to change it; Delete key removes it.";
    else sub = std::wstring(e.band == headfoot::Band::Header ? L"Header" : L"Footer") + L", found on " + std::to_wstring(e.pageCount()) + L" page" + (e.pageCount() == 1 ? L"" : L"s");
    DrawTextW(di->hDC, sub.c_str(), -1, &t2, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void toggleItem(State& s, int i) {
    if (i < 0 || i >= static_cast<int>(s.det.elements.size())) return;
    Element& e = s.det.elements[static_cast<size_t>(i)];
    if (e.locked) { MessageBeep(MB_ICONWARNING); return; }
    e.selected = !e.selected;
    SendMessageW(s.what, CB_SETCURSEL, 4, 0);
    // show a page that has it
    if (e.kind != headfoot::Kind::Area && !e.items.empty()) {
        bool here = false;
        for (const auto& it : e.items) here = here || it.page == s.page;
        if (!here) s.page = e.items.front().page;
    } else if (e.kind == headfoot::Kind::Area) s.activeArea = i;
    update(s);
}

LRESULT CALLBACK listProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    auto* s = reinterpret_cast<State*>(data);
    if (m == WM_LBUTTONDOWN) {   // a click on the tick box (or anywhere on the line) ticks or unticks it
        const LRESULT r = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        if (!HIWORD(r)) { DefSubclassProc(h, m, w, l); toggleItem(*s, LOWORD(r)); return 0; }
    }
    if (m == WM_KEYDOWN && w == VK_SPACE) { toggleItem(*s, static_cast<int>(SendMessageW(h, LB_GETCURSEL, 0, 0))); return 0; }
    if (m == WM_CHAR && w == L' ') return 0;
    if (m == WM_NCDESTROY) RemoveWindowSubclass(h, listProc, 1);
    return DefSubclassProc(h, m, w, l);
}

// ---- the window
void layout(State& s) {
    RECT rc; GetClientRect(s.wnd, &rc);
    const int pad = s.px(14), side = s.px(400);
    const int right = rc.right - pad - side;
    MoveWindow(s.preview, pad, pad + s.px(40), right - pad - s.px(12), rc.bottom - 2 * pad - s.px(40), TRUE);
    // above the preview: page navigation, before / after, draw an area
    int x = pad;
    auto at = [&](int id, int w) { MoveWindow(GetDlgItem(s.wnd, id), x, pad, w, s.px(30), TRUE); x += w + s.px(6); };
    at(ID_PREV, s.px(34)); at(ID_PREV + 100, s.px(110)); at(ID_NEXT, s.px(34)); x += s.px(10);
    at(ID_BEFORE, s.px(76)); at(ID_AFTER, s.px(70)); x += s.px(10);
    at(ID_DRAW, s.px(130));
    // the side
    const int sx = right, sw = side;
    int y = pad;
    auto row = [&](int id, int h, int dy) { MoveWindow(GetDlgItem(s.wnd, id), sx, y, sw, h, TRUE); y += h + dy; };
    row(1001, s.px(20), s.px(2));                          // "What to delete"
    MoveWindow(s.what, sx, y, sw, s.px(300), TRUE);   // a list: its height includes the part that drops down
    y += s.px(40);
    row(1002, s.px(20), s.px(2));                          // "Pages"
    MoveWindow(s.pages, sx, y, s.px(150), s.px(300), TRUE);
    MoveWindow(s.pageList, sx + s.px(158), y, sw - s.px(158), s.px(24), TRUE);
    y += s.px(30);
    row(1003, s.px(18), s.px(8));                          // the example under it
    row(1004, s.px(20), s.px(2));                          // "Found"
    const int listH = std::max<int>(s.px(120), rc.bottom - y - s.px(260));
    row(ID_LIST, listH, s.px(4));
    MoveWindow(GetDlgItem(s.wnd, ID_REMOVE_AREA), sx, y, s.px(170), s.px(28), TRUE);
    y += s.px(36);
    row(1005, s.px(110), s.px(6));                         // notes
    row(1006, s.px(40), s.px(8));                          // summary
    MoveWindow(GetDlgItem(s.wnd, ID_CANCEL), rc.right - pad - s.px(100), rc.bottom - pad - s.px(34), s.px(100), s.px(34), TRUE);
    MoveWindow(GetDlgItem(s.wnd, ID_DELETE), rc.right - pad - s.px(216), rc.bottom - pad - s.px(34), s.px(110), s.px(34), TRUE);
}

void showPage(State& s, int p) {
    s.page = std::clamp(p, 0, s.d->pageCount() - 1);
    update(s);
}

bool confirmAndApply(State& s) {
    update(s);
    if (s.plan.pages.empty()) return false;
    std::wstring text = L"Delete the chosen header and footer items on " + pagesText(s) + L"?\n\n";
    wchar_t sum[512] = L"";
    GetWindowTextW(s.summary, sum, 512);
    text += sum;
    if (s.plan.bodyTaken) text += L"\n\nAttention: an area you drew also takes in " + std::to_wstring(s.plan.bodyTaken) + L" piece(s) of the pages' own content.";
    text += L"\n\nThey are removed from the pages (not covered): their text can no longer be found or copied. Undo (Ctrl+Z) brings them back until the document is closed.";
    if (MessageBoxW(s.wnd, text.c_str(), L"Delete header and footer", MB_YESNO | MB_ICONQUESTION | (s.plan.bodyTaken ? MB_DEFBUTTON2 : 0)) != IDYES) return false;
    std::wstring why;
    if (!headfoot::apply(*s.d, s.plan, &why)) { MessageBoxW(s.wnd, (L"Nothing was changed. " + why).c_str(), L"Delete header and footer", MB_OK | MB_ICONWARNING); return false; }
    return true;
}

LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams)); break;
        case WM_SIZE: if (s && s->preview) layout(*s); return 0;
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(l);
            mm->ptMinTrackSize = {MulDiv(900, s ? s->dpi : 96, 96), MulDiv(620, s ? s->dpi : 96, 96)};
            return 0;
        }
        case WM_MEASUREITEM: {
            auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (mi->CtlType == ODT_LISTBOX) { mi->itemHeight = static_cast<UINT>(MulDiv(44, s ? s->dpi : 96, 96)); return TRUE; }
            break;
        }
        case WM_DRAWITEM: {
            auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (s && di->CtlType == ODT_LISTBOX) { drawItem(*s, di); return TRUE; }
            if (s && di->CtlType == ODT_BUTTON && (di->CtlID == ID_BEFORE || di->CtlID == ID_AFTER || di->CtlID == ID_DRAW)) {   // toggles
                const bool on = di->CtlID == ID_BEFORE ? !s->after : di->CtlID == ID_AFTER ? s->after : s->drawing;
                const Palette& P = pal();
                HBRUSH bb = CreateSolidBrush(P.toolbar); FillRect(di->hDC, &di->rcItem, bb); DeleteObject(bb);
                HBRUSH f = CreateSolidBrush(on ? P.accentSoft : P.field);
                HPEN pn = CreatePen(PS_SOLID, 1, on ? P.accent : P.line);
                HGDIOBJ ob = SelectObject(di->hDC, f), op = SelectObject(di->hDC, pn);
                RoundRect(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, s->px(8), s->px(8));
                SelectObject(di->hDC, ob); SelectObject(di->hDC, op); DeleteObject(f); DeleteObject(pn);
                wchar_t t[64] = L""; GetWindowTextW(di->hwndItem, t, 64);
                SetBkMode(di->hDC, TRANSPARENT); SelectObject(di->hDC, s->font);
                SetTextColor(di->hDC, on ? P.accentText : P.text);
                RECT r = di->rcItem;
                DrawTextW(di->hDC, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                if (di->itemState & ODS_FOCUS) { HBRUSH fb = CreateSolidBrush(P.accent); InflateRect(&r, -s->px(3), -s->px(3)); FrameRect(di->hDC, &r, fb); DeleteObject(fb); }
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            if (!s) break;
            const int id = LOWORD(w), code = HIWORD(w);
            switch (id) {
                case ID_WHAT:
                    if (code == CBN_SELCHANGE) { headfoot::selectFor(s->det, static_cast<int>(SendMessageW(s->what, CB_GETCURSEL, 0, 0))); update(*s); }
                    return 0;
                case ID_PAGES: if (code == CBN_SELCHANGE) { update(*s); if (SendMessageW(s->pages, CB_GETCURSEL, 0, 0) == 4) SetFocus(s->pageList); } return 0;
                case ID_LIST_PAGES: if (code == EN_CHANGE) update(*s); return 0;
                case ID_PREV: showPage(*s, s->page - 1); return 0;
                case ID_NEXT: showPage(*s, s->page + 1); return 0;
                case ID_BEFORE: s->after = false; InvalidateRect(GetDlgItem(h, ID_AFTER), nullptr, FALSE); InvalidateRect(GetDlgItem(h, ID_BEFORE), nullptr, FALSE); InvalidateRect(s->preview, nullptr, FALSE); return 0;
                case ID_AFTER: s->after = true; InvalidateRect(GetDlgItem(h, ID_AFTER), nullptr, FALSE); InvalidateRect(GetDlgItem(h, ID_BEFORE), nullptr, FALSE); InvalidateRect(s->preview, nullptr, FALSE); return 0;
                case ID_DRAW:
                    s->drawing = !s->drawing;
                    if (s->drawing && s->after) { s->after = false; InvalidateRect(GetDlgItem(h, ID_AFTER), nullptr, FALSE); InvalidateRect(GetDlgItem(h, ID_BEFORE), nullptr, FALSE); }
                    InvalidateRect(GetDlgItem(h, ID_DRAW), nullptr, FALSE);
                    InvalidateRect(s->preview, nullptr, FALSE);
                    return 0;
                case ID_REMOVE_AREA:
                    if (s->activeArea >= 0 && s->activeArea < static_cast<int>(s->det.elements.size())) {
                        s->det.elements.erase(s->det.elements.begin() + s->activeArea);
                        s->activeArea = -1;
                        fillList(*s);
                        update(*s);
                    }
                    return 0;
                case ID_DELETE: if (confirmAndApply(*s)) { s->applied = true; s->done = true; } return 0;
                case ID_CANCEL: s->done = true; return 0;
            }
            break;
        }
        case WM_CLOSE: if (s) s->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

bool run(HWND parent, HINSTANCE inst, Document& d, int page, int dpi, HFONT font, HFONT bold) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = proc; wc.hInstance = inst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.lpszClassName = kClass;
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
        WNDCLASSEXW pc{sizeof(pc)};
        pc.lpfnWndProc = previewProc; pc.hInstance = inst; pc.hCursor = LoadCursorW(nullptr, IDC_ARROW); pc.lpszClassName = kPreviewClass;
        RegisterClassExW(&pc);
        registered = true;
    }
    State s;
    s.d = &d;
    s.dpi = dpi;
    s.font = font; s.bold = bold;
    LOGFONTW lf{}; GetObjectW(font, sizeof(lf), &lf);
    lf.lfHeight = lf.lfHeight * 9 / 10;
    s.small = CreateFontIndirectW(&lf);
    s.page = s.current = std::clamp(page, 0, std::max(0, d.pageCount() - 1));
    {   // finding takes a moment on long documents
        HCURSOR old = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        s.det = headfoot::detect(d);
        SetCursor(old);
    }
    RECT pr; GetWindowRect(parent, &pr);
    const int W = s.px(1080), H = s.px(720);
    s.wnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, kClass, L"Delete header and footer",
                            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
                            pr.left + std::max<int>(0, (pr.right - pr.left - W) / 2), pr.top + std::max<int>(0, (pr.bottom - pr.top - H) / 2),
                            W, H, parent, nullptr, inst, &s);
    if (!s.wnd) { DeleteObject(s.small); return false; }
    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
        HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, s.wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        return c;
    };
    mk(L"BUTTON", L"\x2039", WS_TABSTOP | BS_PUSHBUTTON, ID_PREV);
    mk(L"STATIC", L"", SS_CENTER | SS_CENTERIMAGE, ID_PREV + 100);
    mk(L"BUTTON", L"\x203A", WS_TABSTOP | BS_PUSHBUTTON, ID_NEXT);
    mk(L"BUTTON", L"Before", WS_TABSTOP | BS_OWNERDRAW, ID_BEFORE);
    mk(L"BUTTON", L"After", WS_TABSTOP | BS_OWNERDRAW, ID_AFTER);
    mk(L"BUTTON", L"Draw an area", WS_TABSTOP | BS_OWNERDRAW, ID_DRAW);
    s.preview = CreateWindowExW(0, kPreviewClass, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 10, 10, s.wnd, reinterpret_cast<HMENU>(ID_PREVIEW), inst, &s);
    mk(L"STATIC", L"What to delete", SS_LEFT, 1001);
    s.what = mk(L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, ID_WHAT);
    for (const wchar_t* t : {L"Header only", L"Footer only", L"Header and footer", L"Page numbers only", L"The items ticked below"})
        SendMessageW(s.what, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
    SendMessageW(s.what, CB_SETCURSEL, 2, 0);
    mk(L"STATIC", L"Pages", SS_LEFT, 1002);
    s.pages = mk(L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, ID_PAGES);
    for (const wchar_t* t : {L"All pages", L"Current page", L"Odd pages", L"Even pages", L"These pages:"}) SendMessageW(s.pages, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
    SendMessageW(s.pages, CB_SETCURSEL, 0, 0);
    s.pageList = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_LIST_PAGES, WS_EX_CLIENTEDGE);
    SendMessageW(s.pageList, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"2,5,8 or 3-15"));
    HWND ex = mk(L"STATIC", L"For example 2,5,8 \x00B7 3-15 \x00B7 1-3, 7, 9-12", SS_LEFT, 1003);
    SendMessageW(ex, WM_SETFONT, reinterpret_cast<WPARAM>(s.small), FALSE);
    mk(L"STATIC", L"Found (untick what should stay, or click it on the page)", SS_LEFT, 1004);
    s.list = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, ID_LIST, WS_EX_CLIENTEDGE);
    SetWindowSubclass(s.list, listProc, 1, reinterpret_cast<DWORD_PTR>(&s));
    mk(L"BUTTON", L"Remove drawn area", WS_TABSTOP | BS_PUSHBUTTON, ID_REMOVE_AREA);
    s.notes = mk(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 1005);
    s.summary = mk(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 1006);
    SendMessageW(s.summary, WM_SETFONT, reinterpret_cast<WPARAM>(bold), FALSE);
    mk(L"BUTTON", L"Delete\x2026", WS_TABSTOP | BS_DEFPUSHBUTTON, ID_DELETE);
    mk(L"BUTTON", L"Cancel", WS_TABSTOP | BS_PUSHBUTTON, ID_CANCEL);
    fillList(s);
    layout(s);
    themeui::themeDialog(s.wnd);
    update(s);
    EnableWindow(parent, FALSE);
    ShowWindow(s.wnd, SW_SHOW);
    SetFocus(s.list);
    MSG msg;
    while (!s.done) {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0) { PostQuitMessage(static_cast<int>(msg.wParam)); break; }
        if (r < 0) break;
        if (msg.message == WM_KEYDOWN && (msg.hwnd == s.wnd || IsChild(s.wnd, msg.hwnd))) {
            wchar_t cls[16] = L"";
            GetClassNameW(msg.hwnd, cls, 16);
            const bool dropped = _wcsicmp(cls, L"ComboBox") == 0 && SendMessageW(msg.hwnd, CB_GETDROPPEDSTATE, 0, 0);
            if (msg.wParam == VK_ESCAPE && !dropped) {
                if (s.drawing) { s.drawing = false; InvalidateRect(GetDlgItem(s.wnd, ID_DRAW), nullptr, FALSE); continue; }
                s.done = true;
                continue;
            }
            if (msg.wParam == VK_NEXT || msg.wParam == VK_PRIOR) { showPage(s, s.page + (msg.wParam == VK_NEXT ? 1 : -1)); continue; }
        }
        if (!IsDialogMessageW(s.wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(s.wnd);
    SetActiveWindow(parent);
    if (s.shot) DeleteObject(s.shot);
    DeleteObject(s.small);
    return s.applied;
}

} // namespace hfdialog
