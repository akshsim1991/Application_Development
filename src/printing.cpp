#include "printing.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <commctrl.h>
#include <winspool.h>
#include <objbase.h>
using std::min; using std::max;
#include <gdiplus.h>
#include "pdfium_api.hpp"

namespace printing {
namespace {

struct Cell { int page; int x, y, w, h; bool rotate; };

// Pages side by side on a landscape sheet, one above the other on a portrait one.
void gridOf(int perSheet, int& cols, int& rows, bool landscape = true) {
    switch (perSheet) {
        case 2: cols = 2; rows = 1; break;
        case 4: cols = 2; rows = 2; break;
        case 6: cols = 3; rows = 2; break;
        case 9: cols = 3; rows = 3; break;
        default: cols = 1; rows = 1; break;
    }
    if (!landscape) std::swap(cols, rows);
}

// Where each page of the sheet goes inside the printable area (device pixels).
std::vector<Cell> cellsOf(Document& d, const Layout& l, int sheet, int areaW, int areaH, double ppp) {
    std::vector<Cell> out;
    int gc, gr;
    gridOf(l.perSheet, gc, gr, areaW > areaH);
    const int nup = gc * gr;
    const int cw = areaW / gc, ch = areaH / gr, pad = nup > 1 ? std::max(1, static_cast<int>(12 * ppp / (300 / 72.0))) : 0;
    for (int s = 0; s < nup; ++s) {
        const size_t k = static_cast<size_t>(sheet) * nup + s;
        if (k >= l.pages.size()) break;
        const PageSize& ps = d.pageSize(l.pages[k]);
        const int cx = (s % gc) * cw + pad, cy = (s / gc) * ch + pad, aw = cw - 2 * pad, ah = ch - 2 * pad;
        bool rotate = false;
        double pw = ps.w, ph = ps.h;
        if (nup == 1 && l.autoRotate && (ps.w > ps.h) != (aw > ah)) { rotate = true; std::swap(pw, ph); }
        const bool actual = l.actualSize && nup == 1;
        const double sc = actual ? ppp : std::min(aw / pw, ah / ph);
        const int w = static_cast<int>(pw * sc), h = static_cast<int>(ph * sc);
        int x = cx + (aw - w) / 2, y = cy + (ah - h) / 2;
        if (actual) { x = std::max(0, x); y = std::max(0, y); }
        out.push_back({l.pages[k], x, y, w, h, rotate});
    }
    return out;
}

// Draws the cells into a BGRx bitmap whose (0,0) is device point (originX, originY), scaled by `scale`.
void drawCells(FPDF_BITMAP bmp, Document& d, const Layout& l, const std::vector<Cell>& cells, double originX, double originY, double scale) {
    const auto& api = pdfium::api();
    const int bw = api.FPDFBitmap_GetWidth(bmp), bh = api.FPDFBitmap_GetHeight(bmp);
    for (const Cell& c : cells) {
        FPDF_PAGE pg = d.page(c.page);
        if (!pg) continue;
        const int x = static_cast<int>(std::lround((originX + c.x) * scale)), y = static_cast<int>(std::lround((originY + c.y) * scale));
        const int w = std::max(1, static_cast<int>(std::lround(c.w * scale))), h = std::max(1, static_cast<int>(std::lround(c.h * scale)));
        api.FPDF_RenderPageBitmap(bmp, pg, x, y, w, h, c.rotate ? 1 : 0, FPDF_ANNOT | FPDF_PRINTING);
        if (l.perSheet > 1) {   // thin frame so the pages are easy to tell apart
            auto box = [&](int l0, int t0, int r0, int b0) {
                l0 = std::max(0, l0); t0 = std::max(0, t0); r0 = std::min(bw, r0); b0 = std::min(bh, b0);
                if (r0 > l0 && b0 > t0) api.FPDFBitmap_FillRect(bmp, l0, t0, r0 - l0, b0 - t0, 0xFF000000);
            };
            box(x, y, x + w, y + 1); box(x, y + h - 1, x + w, y + h); box(x, y, x + 1, y + h); box(x + w - 1, y, x + w, y + h);
        }
    }
    if (l.grey) {
        unsigned char* buf = static_cast<unsigned char*>(api.FPDFBitmap_GetBuffer(bmp));
        const int stride = api.FPDFBitmap_GetStride(bmp);
        for (int y = 0; y < bh; ++y) {
            unsigned char* p = buf + static_cast<size_t>(y) * stride;
            for (int x = 0; x < bw; ++x, p += 4) p[0] = p[1] = p[2] = static_cast<unsigned char>((p[0] * 29 + p[1] * 150 + p[2] * 77) >> 8);
        }
    }
}

std::vector<unsigned char> packed(FPDF_BITMAP bmp) {
    const auto& api = pdfium::api();
    const int w = api.FPDFBitmap_GetWidth(bmp), h = api.FPDFBitmap_GetHeight(bmp), stride = api.FPDFBitmap_GetStride(bmp);
    const unsigned char* src = static_cast<const unsigned char*>(api.FPDFBitmap_GetBuffer(bmp));
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) memcpy(px.data() + static_cast<size_t>(y) * w * 4, src + static_cast<size_t>(y) * stride, static_cast<size_t>(w) * 4);
    return px;
}

void blit(HDC dc, int x, int y, int w, int h, const std::vector<unsigned char>& px, int pw, int ph) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = pw;
    bi.bmiHeader.biHeight = -ph;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, HALFTONE);
    StretchDIBits(dc, x, y, w, h, 0, 0, pw, ph, px.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
}

} // namespace

Paper paperOf(HDC dc) {
    Paper p;
    p.areaW = GetDeviceCaps(dc, HORZRES);
    p.areaH = GetDeviceCaps(dc, VERTRES);
    p.physW = GetDeviceCaps(dc, PHYSICALWIDTH);
    p.physH = GetDeviceCaps(dc, PHYSICALHEIGHT);
    p.offX = GetDeviceCaps(dc, PHYSICALOFFSETX);
    p.offY = GetDeviceCaps(dc, PHYSICALOFFSETY);
    if (p.physW < p.areaW || p.physH < p.areaH) { p.physW = p.areaW; p.physH = p.areaH; p.offX = p.offY = 0; }   // displays and odd drivers
    p.ppp = GetDeviceCaps(dc, LOGPIXELSX) / 72.0;
    return p;
}

int sheetCount(const Layout& l) {
    int gc, gr;
    gridOf(l.perSheet, gc, gr);
    const int n = gc * gr;
    return static_cast<int>((l.pages.size() + n - 1) / n);
}

void printSheet(HDC dc, Document& d, const Layout& l, int sheet, const Paper& p) {
    const auto& api = pdfium::api();
    const std::vector<Cell> cells = cellsOf(d, l, sheet, p.areaW, p.areaH, p.ppp);
    if (!l.grey) {   // colour pages go to the printer as they are (sharp text and lines)
        for (const Cell& c : cells) {
            FPDF_PAGE pg = d.page(c.page);
            if (!pg) continue;
            api.FPDF_RenderPage(dc, pg, c.x, c.y, c.w, c.h, c.rotate ? 1 : 0, FPDF_ANNOT | FPDF_PRINTING);
            if (l.perSheet > 1) {
                HGDIOBJ op = SelectObject(dc, GetStockObject(BLACK_PEN)), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                Rectangle(dc, c.x, c.y, c.x + c.w, c.y + c.h);
                SelectObject(dc, ob); SelectObject(dc, op);
            }
        }
        return;
    }
    // greyscale: the sheet becomes a grey picture at up to 300 dpi, drawn in strips so large sheets stay small in memory
    const double scale = std::min(1.0, (300.0 / 72.0) / p.ppp);
    const int w = std::max(1, static_cast<int>(p.areaW * scale)), h = std::max(1, static_cast<int>(p.areaH * scale));
    const int strip = std::max(1, std::min(h, static_cast<int>(16'000'000 / (static_cast<long long>(w) * 4))));
    for (int top = 0; top < h; top += strip) {
        const int sh = std::min(strip, h - top);
        FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, sh, FPDFBitmap_BGRx, nullptr, 0);
        if (!bmp) return;
        api.FPDFBitmap_FillRect(bmp, 0, 0, w, sh, 0xFFFFFFFF);
        drawCells(bmp, d, l, cells, 0, -top / scale, scale);
        const std::vector<unsigned char> px = packed(bmp);
        api.FPDFBitmap_Destroy(bmp);
        const int y0 = static_cast<int>(top / scale), y1 = static_cast<int>((top + sh) / scale);
        blit(dc, 0, y0, p.areaW, y1 - y0, px, w, sh);
    }
}

bool renderSheet(Document& d, const Layout& l, int sheet, const Paper& p, double scale, std::vector<unsigned char>& px, int& w, int& h) {
    const auto& api = pdfium::api();
    w = std::max(1, static_cast<int>(p.physW * scale));
    h = std::max(1, static_cast<int>(p.physH * scale));
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, nullptr, 0);
    if (!bmp) return false;
    api.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
    drawCells(bmp, d, l, cellsOf(d, l, sheet, p.areaW, p.areaH, p.ppp), p.offX, p.offY, scale);
    px = packed(bmp);
    api.FPDFBitmap_Destroy(bmp);
    return true;
}

bool savePng(const std::vector<unsigned char>& px, int w, int h, const std::wstring& path) {
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR tok = 0;
    if (Gdiplus::GdiplusStartup(&tok, &in, nullptr) != Gdiplus::Ok) return false;
    bool ok = false;
    {
        Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppRGB, const_cast<unsigned char*>(px.data()));
        UINT n = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&n, &size);
        std::vector<unsigned char> buf(size ? size : 1);
        auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
        if (size && Gdiplus::GetImageEncoders(n, size, codecs) == Gdiplus::Ok)
            for (UINT i = 0; i < n; ++i) if (wcscmp(codecs[i].MimeType, L"image/png") == 0) ok = bmp.Save(path.c_str(), &codecs[i].Clsid, nullptr) == Gdiplus::Ok;
    }
    Gdiplus::GdiplusShutdown(tok);
    return ok;
}

// ---- the Print window: settings on the left, every sheet as it will print on the right ----
namespace {

enum { ID_PRINTER = 201, ID_PROPS, ID_ALL, ID_CURRENT, ID_RANGE, ID_RANGETEXT, ID_COPIES, ID_NUP, ID_SIZE, ID_ORIENT, ID_COLOUR,
       ID_SIDES, ID_STATUS, ID_PRINT, ID_CANCEL, ID_SHEETS };
enum { TIMER_REFRESH = 1 };
constexpr int kPanel = 320;     // width of the settings panel
constexpr int kGap = 28;        // between sheets in the preview

struct State {
    Document* doc = nullptr;
    int current = 0;
    std::vector<std::wstring> printers;
    std::vector<std::vector<BYTE>> devmodes;   // per printer, as Properties left it (empty: not read yet)
    int printer = -1;                          // -1: no printer installed
    Layout layout;
    Paper paper;
    bool valid = false, print = false, done = false;
    std::wstring status;
    // preview
    HWND wnd = nullptr, sheets = nullptr;
    int scroll = 0, sheetW = 0, sheetH = 0;    // scroll position and one sheet's size on screen (pixels)
    struct Rendered { std::vector<unsigned char> px; int w = 0, h = 0; };
    std::map<int, Rendered> cache;             // rendered sheets at the current size
    std::vector<int> cacheOrder;
    HFONT font = nullptr, bold = nullptr;
};

std::vector<BYTE> defaultDevmode(const std::wstring& printer) {
    std::vector<BYTE> dm;
    HANDLE hp = nullptr;
    if (!OpenPrinterW(const_cast<wchar_t*>(printer.c_str()), &hp, nullptr)) return dm;
    const LONG sz = DocumentPropertiesW(nullptr, hp, const_cast<wchar_t*>(printer.c_str()), nullptr, nullptr, 0);
    if (sz > 0) {
        dm.resize(static_cast<size_t>(sz));
        if (DocumentPropertiesW(nullptr, hp, const_cast<wchar_t*>(printer.c_str()), reinterpret_cast<DEVMODEW*>(dm.data()), nullptr, DM_OUT_BUFFER) != IDOK) dm.clear();
    }
    ClosePrinter(hp);
    return dm;
}

int choice(HWND w, int id) { return static_cast<int>(SendDlgItemMessageW(w, id, CB_GETCURSEL, 0, 0)); }
std::wstring textOf(HWND w, int id) {
    wchar_t t[512] = L"";
    GetDlgItemTextW(w, id, t, 512);
    return t;
}

// The printer's settings for this job: its own settings with orientation, colour and sides as chosen here.
std::vector<BYTE> jobDevmode(State* st, bool& landscape) {
    HWND w = st->wnd;
    std::vector<BYTE> dm;
    if (st->printer >= 0) {
        if (st->devmodes[static_cast<size_t>(st->printer)].empty()) st->devmodes[static_cast<size_t>(st->printer)] = defaultDevmode(st->printers[static_cast<size_t>(st->printer)]);
        dm = st->devmodes[static_cast<size_t>(st->printer)];
    }
    // orientation: chosen, or from the first page (two pages per sheet sit side by side on the other orientation)
    const int o = choice(w, ID_ORIENT);
    landscape = o == 2;
    if (o == 0 && !st->layout.pages.empty()) {
        const PageSize& ps = st->doc->pageSize(st->layout.pages.front());
        landscape = ps.w > ps.h;
        if (st->layout.perSheet == 2) landscape = !landscape;
    }
    if (dm.size() >= sizeof(DEVMODEW)) {
        auto* dev = reinterpret_cast<DEVMODEW*>(dm.data());
        dev->dmFields |= DM_ORIENTATION | DM_COLOR | DM_DUPLEX;
        dev->dmOrientation = static_cast<short>(landscape ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT);
        dev->dmColor = static_cast<short>(st->layout.grey ? DMCOLOR_MONOCHROME : DMCOLOR_COLOR);
        const int sides = choice(w, ID_SIDES);   // long-edge flipping is "vertical" on portrait sheets, "horizontal" on landscape
        dev->dmDuplex = static_cast<short>(!sides ? DMDUP_SIMPLEX : ((sides == 1) != landscape ? DMDUP_VERTICAL : DMDUP_HORIZONTAL));
    }
    return dm;
}

void layoutSheets(State* st);

// Reads the settings, works out the sheets and the paper, and shows them.
void refresh(State* st) {
    HWND w = st->wnd;
    Document& d = *st->doc;
    st->valid = false;
    st->layout = Layout{};
    std::wstring problem;
    const bool range = SendDlgItemMessageW(w, ID_RANGE, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(GetDlgItem(w, ID_RANGETEXT), range);
    if (SendDlgItemMessageW(w, ID_CURRENT, BM_GETCHECK, 0, 0) == BST_CHECKED) st->layout.pages = {st->current};
    else if (range) {
        st->layout.pages = d.parseRange(textOf(w, ID_RANGETEXT));
        if (st->layout.pages.empty()) problem = L"Type the pages to print, such as 1-3, 5, 8-.";
    } else for (int i = 0; i < d.pageCount(); ++i) st->layout.pages.push_back(i);
    static const int perSheet[] = {1, 2, 4, 6, 9};
    st->layout.perSheet = perSheet[std::clamp(choice(w, ID_NUP), 0, 4)];
    st->layout.actualSize = choice(w, ID_SIZE) == 1;
    st->layout.autoRotate = choice(w, ID_ORIENT) == 0;
    st->layout.grey = choice(w, ID_COLOUR) == 1;
    EnableWindow(GetDlgItem(w, ID_SIZE), st->layout.perSheet == 1);
    const std::wstring copiesText = textOf(w, ID_COPIES);
    wchar_t* end = nullptr;
    const long copies = wcstol(copiesText.c_str(), &end, 10);
    if (problem.empty() && (end == copiesText.c_str() || *end || copies < 1 || copies > 999)) problem = L"Copies must be a number from 1 to 999.";
    // the paper of the chosen printer, turned as the job will be
    bool landscape = false;
    std::vector<BYTE> dm = jobDevmode(st, landscape);
    st->paper = Paper{};
    if (st->printer >= 0) {
        if (HDC ic = CreateICW(L"WINSPOOL", st->printers[static_cast<size_t>(st->printer)].c_str(), nullptr, dm.empty() ? nullptr : reinterpret_cast<DEVMODEW*>(dm.data()))) {
            st->paper = paperOf(ic);
            DeleteDC(ic);
        }
    }
    const bool noPrinter = st->paper.physW <= 0 || st->paper.physH <= 0;
    if (noPrinter) {   // A4 at 300 dpi with 5 mm margins
        st->paper.physW = 2480; st->paper.physH = 3508; st->paper.offX = st->paper.offY = 59;
        st->paper.areaW = 2480 - 118; st->paper.areaH = 3508 - 118; st->paper.ppp = 300 / 72.0;
        if (landscape) { std::swap(st->paper.physW, st->paper.physH); std::swap(st->paper.areaW, st->paper.areaH); }
    }
    const int n = problem.empty() ? sheetCount(st->layout) : 0;
    st->valid = problem.empty() && n > 0 && st->printer >= 0 && !noPrinter;
    if (!problem.empty()) st->status = problem;
    else {
        const double inW = st->paper.physW / (st->paper.ppp * 72), inH = st->paper.physH / (st->paper.ppp * 72);
        wchar_t t[200];
        swprintf(t, 200, L"%d %ls on %.1f \x00D7 %.1f cm paper, %d %ls", n, n == 1 ? L"sheet" : L"sheets", inW * 2.54, inH * 2.54,
                 static_cast<int>(st->layout.pages.size()), st->layout.pages.size() == 1 ? L"page" : L"pages");
        st->status = t;
        if (st->printer < 0) st->status += L"\nNo printer is installed: add one in Windows Settings > Printers & scanners.";
        else if (noPrinter) st->status += L"\nThe printer could not be reached. Check that it is on and connected.";
    }
    SetDlgItemTextW(w, ID_STATUS, st->status.c_str());
    EnableWindow(GetDlgItem(w, ID_PRINT), st->valid);
    st->cache.clear();
    st->cacheOrder.clear();
    layoutSheets(st);
}

// The preview's sheet size and scroll range for its window size.
void layoutSheets(State* st) {
    RECT rc;
    GetClientRect(st->sheets, &rc);
    const int availW = rc.right - 2 * kGap, availH = rc.bottom - 2 * kGap - 20;
    const Paper& p = st->paper;
    if (availW < 20 || availH < 20 || p.physW <= 0 || p.physH <= 0) { st->sheetW = st->sheetH = 0; return; }
    const double scale = std::min(static_cast<double>(availW) / p.physW, static_cast<double>(availH) / p.physH);   // a whole sheet in view
    const int w = std::max(1, static_cast<int>(p.physW * scale)), h = std::max(1, static_cast<int>(p.physH * scale));
    if (w != st->sheetW || h != st->sheetH) { st->cache.clear(); st->cacheOrder.clear(); }
    st->sheetW = w; st->sheetH = h;
    const int n = st->valid || st->status.find(L"sheet") != std::wstring::npos ? sheetCount(st->layout) : 0;
    const int total = n * (h + kGap + 20) + kGap;
    st->scroll = std::clamp(st->scroll, 0, std::max(0, total - static_cast<int>(rc.bottom)));
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, std::max(0, total - 1), static_cast<UINT>(rc.bottom), st->scroll, 0};
    SetScrollInfo(st->sheets, SB_VERT, &si, TRUE);
    InvalidateRect(st->sheets, nullptr, FALSE);
}

void scrollTo(State* st, int y) {
    RECT rc;
    GetClientRect(st->sheets, &rc);
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE};
    GetScrollInfo(st->sheets, SB_VERT, &si);
    y = std::clamp(y, 0, std::max(0, si.nMax + 1 - static_cast<int>(si.nPage)));
    if (y == st->scroll) return;
    st->scroll = y;
    si.fMask = SIF_POS; si.nPos = y;
    SetScrollInfo(st->sheets, SB_VERT, &si, TRUE);
    InvalidateRect(st->sheets, nullptr, FALSE);
}

LRESULT CALLBACK sheetsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(GetParent(h), GWLP_USERDATA));
    if (!st) return DefWindowProcW(h, msg, wp, lp);
    switch (msg) {
        case WM_SIZE: layoutSheets(st); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_VSCROLL: {
            SCROLLINFO si{sizeof(si), SIF_ALL};
            GetScrollInfo(h, SB_VERT, &si);
            const int step = st->sheetH + kGap + 20;
            int y = st->scroll;
            switch (LOWORD(wp)) {
                case SB_LINEUP: y -= 60; break;
                case SB_LINEDOWN: y += 60; break;
                case SB_PAGEUP: y -= step; break;
                case SB_PAGEDOWN: y += step; break;
                case SB_TOP: y = 0; break;
                case SB_BOTTOM: y = si.nMax; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: y = si.nTrackPos; break;
            }
            scrollTo(st, y);
            return 0;
        }
        case WM_MOUSEWHEEL: scrollTo(st, st->scroll - GET_WHEEL_DELTA_WPARAM(wp) * (st->sheetH + kGap + 20) / (2 * WHEEL_DELTA)); return 0;
        case WM_LBUTTONDOWN: SetFocus(h); return 0;
        case WM_GETDLGCODE: return DLGC_WANTARROWS;
        case WM_KEYDOWN: {
            const int step = st->sheetH + kGap + 20;
            switch (wp) {
                case VK_DOWN: scrollTo(st, st->scroll + 60); return 0;
                case VK_UP: scrollTo(st, st->scroll - 60); return 0;
                case VK_NEXT: scrollTo(st, st->scroll + step); return 0;
                case VK_PRIOR: scrollTo(st, st->scroll - step); return 0;
                case VK_HOME: scrollTo(st, 0); return 0;
                case VK_END: scrollTo(st, 1 << 28); return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bm = CreateCompatibleBitmap(dc, std::max(1L, rc.right), std::max(1L, rc.bottom));
            HGDIOBJ old = SelectObject(mem, bm);
            HBRUSH back = CreateSolidBrush(RGB(0x5A, 0x5E, 0x66));
            FillRect(mem, &rc, back);
            DeleteObject(back);
            HGDIOBJ of = SelectObject(mem, st->font);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, RGB(0xF0, 0xF0, 0xF0));
            const int n = st->sheetW > 0 && (st->valid || st->status.find(L"sheet") != std::wstring::npos) ? sheetCount(st->layout) : 0;
            if (!n) {
                RECT tr = rc;
                InflateRect(&tr, -24, 0);
                DrawTextW(mem, st->status.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }
            const int step = st->sheetH + kGap + 20, x = (rc.right - st->sheetW) / 2;
            const int first = std::max(0, (st->scroll - kGap) / std::max(1, step)), last = std::min<int>(n - 1, (st->scroll + static_cast<int>(rc.bottom)) / std::max(1, step));
            for (int k = first; k <= last && n; ++k) {
                const int y = kGap + k * step - st->scroll;
                auto it = st->cache.find(k);
                if (it == st->cache.end()) {   // rendered when first seen; a few are kept
                    State::Rendered r;
                    const double scale = static_cast<double>(st->sheetW) / st->paper.physW;
                    if (!renderSheet(*st->doc, st->layout, k, st->paper, scale, r.px, r.w, r.h)) continue;
                    it = st->cache.emplace(k, std::move(r)).first;
                    st->cacheOrder.push_back(k);
                    while (st->cacheOrder.size() > 24) { st->cache.erase(st->cacheOrder.front()); st->cacheOrder.erase(st->cacheOrder.begin()); }
                }
                RECT shadow{x + 4, y + 4, x + st->sheetW + 4, y + st->sheetH + 4};
                HBRUSH sb = CreateSolidBrush(RGB(0x3A, 0x3D, 0x42));
                FillRect(mem, &shadow, sb);
                DeleteObject(sb);
                if (!it->second.px.empty()) blit(mem, x, y, st->sheetW, st->sheetH, it->second.px, it->second.w, it->second.h);
                // what is on the sheet, under it
                int gc, gr;
                gridOf(st->layout.perSheet, gc, gr);
                const size_t per = static_cast<size_t>(gc * gr), a = static_cast<size_t>(k) * per;
                const size_t b = std::min(st->layout.pages.size(), a + per);
                std::wstring t = L"Sheet " + std::to_wstring(k + 1) + L" of " + std::to_wstring(n) + L"   \x2022   ";
                if (b - a == 1) t += L"page " + std::to_wstring(st->layout.pages[a] + 1);
                else {
                    bool run = true;
                    for (size_t i = a + 1; i < b; ++i) if (st->layout.pages[i] != st->layout.pages[i - 1] + 1) run = false;
                    t += L"pages ";
                    if (run) t += std::to_wstring(st->layout.pages[a] + 1) + L"-" + std::to_wstring(st->layout.pages[b - 1] + 1);
                    else for (size_t i = a; i < b; ++i) t += (i > a ? L", " : L"") + std::to_wstring(st->layout.pages[i] + 1);
                }
                RECT tr{0, y + st->sheetH + 6, rc.right, y + st->sheetH + 26};
                DrawTextW(mem, t.c_str(), -1, &tr, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SelectObject(mem, of);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bm);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void layoutControls(State* st) {
    HWND w = st->wnd;
    RECT rc;
    GetClientRect(w, &rc);
    struct Row { int id; int x, w, h; bool newRow; int gapBefore; };
    int y = 16;
    auto place = [&](int id, int x, int width, int height) { MoveWindow(GetDlgItem(w, id), x, y, width, height, TRUE); };
    auto label = [&](int id) { MoveWindow(GetDlgItem(w, id), 16, y + 4, 110, 20, TRUE); };
    const int cx = 128, cw = kPanel - cx - 16;
    label(ID_PRINTER + 1000); place(ID_PRINTER, cx, cw, 300); y += 32;
    place(ID_PROPS, cx, cw, 28); y += 44;
    label(ID_ALL + 1000); place(ID_ALL, cx, cw, 22); y += 26;
    place(ID_CURRENT, cx, cw, 22); y += 26;
    place(ID_RANGE, cx, 72, 22); MoveWindow(GetDlgItem(w, ID_RANGETEXT), cx + 74, y - 1, cw - 74, 24, TRUE); y += 40;
    label(ID_COPIES + 1000); place(ID_COPIES, cx, 64, 24); y += 36;
    label(ID_NUP + 1000); place(ID_NUP, cx, cw, 300); y += 32;
    label(ID_SIZE + 1000); place(ID_SIZE, cx, cw, 300); y += 32;
    label(ID_ORIENT + 1000); place(ID_ORIENT, cx, cw, 300); y += 32;
    label(ID_COLOUR + 1000); place(ID_COLOUR, cx, cw, 300); y += 32;
    label(ID_SIDES + 1000); place(ID_SIDES, cx, cw, 300); y += 44;
    MoveWindow(GetDlgItem(w, ID_STATUS), 16, y, kPanel - 32, 64, TRUE);
    const int by = rc.bottom - 16 - 32;
    MoveWindow(GetDlgItem(w, ID_PRINT), kPanel - 16 - 96 - 8 - 96, by, 96, 32, TRUE);
    MoveWindow(GetDlgItem(w, ID_CANCEL), kPanel - 16 - 96, by, 96, 32, TRUE);
    MoveWindow(st->sheets, kPanel, 0, std::max(10L, rc.right - kPanel), rc.bottom, TRUE);
}

LRESULT CALLBACK printProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (msg == WM_CREATE) {
        st = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->wnd = w;
        return 0;
    }
    if (!st) return DefWindowProcW(w, msg, wp, lp);
    switch (msg) {
        case WM_SIZE: layoutControls(st); return 0;
        case WM_GETMINMAXINFO: reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {kPanel + 360, 560}; return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            SetTextColor(dc, reinterpret_cast<HWND>(lp) == GetDlgItem(w, ID_STATUS) && !st->valid && st->printer >= 0 ? RGB(0xB0, 0x20, 0x20) : GetSysColor(COLOR_BTNTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        case WM_MOUSEWHEEL: SendMessageW(st->sheets, msg, wp, lp); return 0;
        case WM_TIMER:
            if (wp == TIMER_REFRESH) { KillTimer(w, TIMER_REFRESH); refresh(st); }
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            switch (id) {
                case ID_PRINT: if (st->valid) { st->print = true; st->done = true; } return 0;
                case ID_CANCEL: case IDCANCEL: st->done = true; return 0;
                case ID_PROPS: {
                    if (st->printer < 0) return 0;
                    bool land = false;
                    std::vector<BYTE> dm = jobDevmode(st, land);
                    HANDLE hp = nullptr;
                    const std::wstring& name = st->printers[static_cast<size_t>(st->printer)];
                    if (dm.size() >= sizeof(DEVMODEW) && OpenPrinterW(const_cast<wchar_t*>(name.c_str()), &hp, nullptr)) {
                        auto* dev = reinterpret_cast<DEVMODEW*>(dm.data());
                        if (DocumentPropertiesW(w, hp, const_cast<wchar_t*>(name.c_str()), dev, dev, DM_IN_BUFFER | DM_OUT_BUFFER | DM_IN_PROMPT) == IDOK) {
                            st->devmodes[static_cast<size_t>(st->printer)] = dm;
                            // what was chosen there shows here too
                            if (dev->dmFields & DM_ORIENTATION) SendDlgItemMessageW(w, ID_ORIENT, CB_SETCURSEL, dev->dmOrientation == DMORIENT_LANDSCAPE ? 2 : 1, 0);
                            if (dev->dmFields & DM_COLOR) SendDlgItemMessageW(w, ID_COLOUR, CB_SETCURSEL, dev->dmColor == DMCOLOR_MONOCHROME ? 1 : 0, 0);
                            refresh(st);
                        }
                        ClosePrinter(hp);
                    }
                    return 0;
                }
                case ID_PRINTER:
                    if (code == CBN_SELCHANGE) { st->printer = choice(w, ID_PRINTER); refresh(st); }
                    return 0;
                case ID_NUP: case ID_SIZE: case ID_ORIENT: case ID_COLOUR: case ID_SIDES:
                    if (code == CBN_SELCHANGE) refresh(st);
                    return 0;
                case ID_ALL: case ID_CURRENT: case ID_RANGE:
                    if (code == BN_CLICKED) {
                        refresh(st);
                        if (id == ID_RANGE) { SetFocus(GetDlgItem(w, ID_RANGETEXT)); SendDlgItemMessageW(w, ID_RANGETEXT, EM_SETSEL, 0, -1); }
                    }
                    return 0;
                case ID_RANGETEXT: case ID_COPIES:   // typing: the preview follows a moment after the last key
                    if (code == EN_CHANGE) SetTimer(w, TIMER_REFRESH, 250, nullptr);
                    return 0;
            }
            break;
        }
        case WM_CLOSE: st->done = true; return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

} // namespace

bool run(HWND parent, HINSTANCE inst, Document& d, int currentPage) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = printProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(parent, GCLP_HICON));
        wc.lpszClassName = L"FeatherPrint";
        WNDCLASSEXW sc{sizeof(sc)};
        sc.lpfnWndProc = sheetsProc;
        sc.hInstance = inst;
        sc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        sc.lpszClassName = L"FeatherPrintSheets";
        registered = RegisterClassExW(&wc) && RegisterClassExW(&sc);
        if (!registered) { MessageBoxW(parent, L"The Print window could not be shown.", L"Print", MB_OK | MB_ICONWARNING); return false; }
    }
    State st;
    st.doc = &d;
    st.current = std::clamp(currentPage, 0, std::max(0, d.pageCount() - 1));
    // printers installed on this PC, the default one first chosen
    {
        DWORD need = 0, n = 0;
        EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, nullptr, 0, &need, &n);
        std::vector<BYTE> buf(need ? need : 1);
        if (need && EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, buf.data(), need, &need, &n))
            for (DWORD i = 0; i < n; ++i) st.printers.push_back(reinterpret_cast<PRINTER_INFO_4W*>(buf.data())[i].pPrinterName);
    }
    st.devmodes.resize(st.printers.size());
    wchar_t def[256] = L"";
    DWORD defLen = 256;
    GetDefaultPrinterW(def, &defLen);
    if (!st.printers.empty()) st.printer = 0;
    for (size_t i = 0; i < st.printers.size(); ++i) if (st.printers[i] == def) st.printer = static_cast<int>(i);

    HMONITOR mon = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    const int sw = mi.rcWork.right - mi.rcWork.left, sh = mi.rcWork.bottom - mi.rcWork.top;
    const int h = static_cast<int>(sh * 0.92), wdt = std::min(sw - 40, std::max(kPanel + 520, static_cast<int>(h * 0.95)));
    HWND w = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"FeatherPrint", (L"Print \x2013 " + d.title()).c_str(),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
                             mi.rcWork.left + (sw - wdt) / 2, mi.rcWork.top + (sh - h) / 2, wdt, h, parent, nullptr, inst, &st);
    if (!w) { MessageBoxW(parent, L"The Print window could not be shown.", L"Print", MB_OK | MB_ICONWARNING); return false; }
    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    st.font = CreateFontIndirectW(&ncm.lfMessageFont);
    auto mk = [&](DWORD ex, const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
        HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st.font), TRUE);
        return c;
    };
    auto combo = [&](int id, const wchar_t* lbl, std::initializer_list<const wchar_t*> items, int sel) {
        mk(0, L"STATIC", lbl, SS_LEFT, id + 1000);
        HWND c = mk(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, id);
        for (const wchar_t* t : items) SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
        SendMessageW(c, CB_SETCURSEL, sel, 0);
        return c;
    };
    static int lastColour = 0, lastSides = 0;   // remembered; the rest starts as for an ordinary print every time
    mk(0, L"STATIC", L"Printer:", SS_LEFT, ID_PRINTER + 1000);
    HWND pc = mk(0, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_PRINTER);
    for (const auto& p : st.printers) SendMessageW(pc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.c_str()));
    if (st.printers.empty()) SendMessageW(pc, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(no printer installed)"));
    SendMessageW(pc, CB_SETCURSEL, std::max(0, st.printer), 0);
    EnableWindow(pc, !st.printers.empty());
    mk(0, L"BUTTON", L"Printer settings...", BS_PUSHBUTTON | WS_TABSTOP, ID_PROPS);
    EnableWindow(GetDlgItem(w, ID_PROPS), !st.printers.empty());
    mk(0, L"STATIC", L"Pages:", SS_LEFT, ID_ALL + 1000);
    mk(0, L"BUTTON", L"All pages", BS_AUTORADIOBUTTON | WS_TABSTOP | WS_GROUP, ID_ALL);
    mk(0, L"BUTTON", (L"Current page (" + std::to_wstring(st.current + 1) + L")").c_str(), BS_AUTORADIOBUTTON, ID_CURRENT);
    mk(0, L"BUTTON", L"Pages:", BS_AUTORADIOBUTTON, ID_RANGE);
    mk(WS_EX_CLIENTEDGE, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_GROUP, ID_RANGETEXT);
    SendDlgItemMessageW(w, ID_RANGETEXT, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"such as 1-3, 5, 8-"));
    SendDlgItemMessageW(w, ID_ALL, BM_SETCHECK, BST_CHECKED, 0);
    mk(0, L"STATIC", L"Copies:", SS_LEFT, ID_COPIES + 1000);
    mk(WS_EX_CLIENTEDGE, L"EDIT", L"1", ES_NUMBER | WS_TABSTOP, ID_COPIES);
    combo(ID_NUP, L"Pages per sheet:", {L"1", L"2", L"4", L"6", L"9"}, 0);
    combo(ID_SIZE, L"Size:", {L"Fit to the paper", L"Actual size"}, 0);
    combo(ID_ORIENT, L"Orientation:", {L"Automatic (as the pages)", L"Portrait", L"Landscape"}, 0);
    combo(ID_COLOUR, L"Colour:", {L"Colour", L"Black and white (shades of grey)"}, lastColour);
    combo(ID_SIDES, L"Sides:", {L"One-sided", L"Two-sided, flip on the long edge", L"Two-sided, flip on the short edge"}, lastSides);
    mk(0, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, ID_STATUS);
    mk(0, L"BUTTON", L"Print", BS_DEFPUSHBUTTON | WS_TABSTOP, ID_PRINT);
    mk(0, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, ID_CANCEL);
    st.sheets = CreateWindowExW(0, L"FeatherPrintSheets", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP, 0, 0, 10, 10, w,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SHEETS)), inst, nullptr);
    layoutControls(&st);
    refresh(&st);
    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
    SetForegroundWindow(w);
    SetFocus(GetDlgItem(w, ID_PRINT));
    EnableWindow(parent, FALSE);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && (msg.hwnd == w || IsChild(w, msg.hwnd))) {
            // Esc closes a dropped-down list first
            HWND f = GetFocus();
            if (!(f && SendMessageW(f, CB_GETDROPPEDSTATE, 0, 0))) { st.done = true; continue; }
        }
        if (!IsDialogMessageW(w, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    KillTimer(w, TIMER_REFRESH);
    if (st.print) refresh(&st);   // a range typed just now
    lastColour = choice(w, ID_COLOUR); lastSides = choice(w, ID_SIDES);
    const int sides = choice(w, ID_SIDES);
    const int copies = static_cast<int>(wcstol(textOf(w, ID_COPIES).c_str(), nullptr, 10));
    bool landscape = false;
    std::vector<BYTE> dm = st.print && st.valid ? jobDevmode(&st, landscape) : std::vector<BYTE>{};
    EnableWindow(parent, TRUE);
    DestroyWindow(w);
    SetActiveWindow(parent);
    if (st.font) DeleteObject(st.font);
    if (!st.print || !st.valid) return false;

    // print exactly what the preview showed: same printer settings, same sheets
    const std::wstring& pname = st.printers[static_cast<size_t>(st.printer)];
    if (sides && DeviceCapabilitiesW(pname.c_str(), nullptr, DC_DUPLEX, nullptr, nullptr) != 1)
        MessageBoxW(parent, L"This printer does not say it can print on both sides. Feather PDF asks for two-sided printing anyway; if the pages come out one-sided, choose two-sided in Printer settings.", L"Print", MB_OK | MB_ICONINFORMATION);
    HDC dc = CreateDCW(L"WINSPOOL", pname.c_str(), nullptr, dm.size() >= sizeof(DEVMODEW) ? reinterpret_cast<DEVMODEW*>(dm.data()) : nullptr);
    if (!dc) { MessageBoxW(parent, L"The printer could not be opened. Check that it is on and connected.", L"Print", MB_OK | MB_ICONWARNING); return false; }
    const Paper paper = paperOf(dc);
    DOCINFOW di{sizeof(di)};
    const std::wstring name = d.title();
    di.lpszDocName = name.c_str();
    bool good = StartDocW(dc, &di) > 0;
    const int sheets = sheetCount(st.layout);
    // a small window shows how far it is, with Cancel; the program stays responsive between sheets
    bool cancelled = false;
    WNDCLASSEXW wcp{sizeof(wcp)};
    if (!GetClassInfoExW(inst, L"FeatherPrintProgress", &wcp)) {
        wcp = WNDCLASSEXW{sizeof(wcp)};
        wcp.lpfnWndProc = DefWindowProcW;
        wcp.hInstance = inst;
        wcp.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcp.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wcp.lpszClassName = L"FeatherPrintProgress";
        RegisterClassExW(&wcp);
    }
    HWND prog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"FeatherPrintProgress", L"", WS_POPUP | WS_CAPTION | WS_VISIBLE,
                                0, 0, 360, 120, parent, nullptr, inst, nullptr);
    HWND progText = nullptr, progCancel = nullptr;
    if (prog) {
        SetWindowTextW(prog, L"Printing");
        RECT pr;
        GetWindowRect(parent, &pr);
        SetWindowPos(prog, nullptr, (pr.left + pr.right - 360) / 2, (pr.top + pr.bottom - 120) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        progText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 14, 330, 20, prog, nullptr, inst, nullptr);
        progCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 46, 96, 28, prog, nullptr, inst, nullptr);
        NONCLIENTMETRICSW pm{sizeof(pm)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(pm), &pm, 0);
        HFONT pf = CreateFontIndirectW(&pm.lfMessageFont);
        SendMessageW(progText, WM_SETFONT, reinterpret_cast<WPARAM>(pf), TRUE);
        SendMessageW(progCancel, WM_SETFONT, reinterpret_cast<WPARAM>(pf), TRUE);
        SetPropW(prog, L"font", pf);
        EnableWindow(parent, FALSE);
    }
    auto pump = [&](int done, int total) {
        if (!prog) return;
        wchar_t t[96];
        swprintf(t, 96, L"Sheet %d of %d", done, total);
        SetWindowTextW(progText, t);
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if ((m.message == WM_LBUTTONUP && m.hwnd == progCancel) || (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE)) cancelled = true;
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    };
    const int total = sheets * std::max(1, copies);
    int done = 0;
    for (int c = 0; good && !cancelled && c < copies; ++c) {
        for (int k = 0; good && !cancelled && k < sheets; ++k) {
            pump(++done, total);
            if (cancelled) break;
            if (StartPage(dc) <= 0) { good = false; break; }
            printSheet(dc, d, st.layout, k, paper);
            if (EndPage(dc) <= 0) good = false;
        }
        if (good && !cancelled && sides && sheets % 2 && c + 1 < copies) {   // two-sided: the next copy starts on a fresh sheet
            if (StartPage(dc) <= 0 || EndPage(dc) <= 0) good = false;
        }
    }
    if (prog) {
        EnableWindow(parent, TRUE);
        if (HFONT pf = static_cast<HFONT>(RemovePropW(prog, L"font"))) { DestroyWindow(prog); DeleteObject(pf); } else DestroyWindow(prog);
        SetActiveWindow(parent);
    }
    if (good && !cancelled) EndDoc(dc); else AbortDoc(dc);
    DeleteDC(dc);
    if (!good) MessageBoxW(parent, L"The document could not be printed. Check that the printer is on and connected, then try again.", L"Print", MB_OK | MB_ICONWARNING);
    return good && !cancelled;
}

} // namespace printing
