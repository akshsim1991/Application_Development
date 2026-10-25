#include "printing.hpp"
#include <algorithm>
#include <cmath>
#include <objbase.h>
using std::min; using std::max;
#include <gdiplus.h>
#include "pdfium_api.hpp"

namespace printing {
namespace {

struct Cell { int page; int x, y, w, h; bool rotate; };

void gridOf(int perSheet, int& cols, int& rows) {
    switch (perSheet) {
        case 2: cols = 2; rows = 1; break;
        case 4: cols = 2; rows = 2; break;
        case 6: cols = 2; rows = 3; break;
        case 9: cols = 3; rows = 3; break;
        default: cols = 1; rows = 1; break;
    }
}

// Where each page of the sheet goes inside the printable area (device pixels).
std::vector<Cell> cellsOf(Document& d, const Layout& l, int sheet, int areaW, int areaH, double ppp) {
    std::vector<Cell> out;
    int gc, gr;
    gridOf(l.perSheet, gc, gr);
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

// ---- the preview window ----
namespace {

enum { ID_PREV = 101, ID_NEXT, ID_PRINT, ID_CLOSE, ID_LABEL };
constexpr int kBar = 52;

struct Preview {
    Document* doc = nullptr;
    const Layout* layout = nullptr;
    const Paper* paper = nullptr;
    std::wstring printer;
    int copies = 1, sheet = 0, count = 0;
    bool print = false, done = false;
    // the last rendered sheet
    int cachedSheet = -1, cw = 0, ch = 0;
    std::vector<unsigned char> px;
    HFONT font = nullptr;
};

void updateLabel(HWND w, Preview* pv) {
    wchar_t t[300];
    swprintf(t, 300, L"Sheet %d of %d   \x2022   %ls   \x2022   %d cop%ls%ls", pv->sheet + 1, pv->count, pv->printer.c_str(), pv->copies, pv->copies == 1 ? L"y" : L"ies", pv->layout->grey ? L"   \x2022   greyscale" : L"");
    SetDlgItemTextW(w, ID_LABEL, t);
    EnableWindow(GetDlgItem(w, ID_PREV), pv->sheet > 0);
    EnableWindow(GetDlgItem(w, ID_NEXT), pv->sheet + 1 < pv->count);
}

void go(HWND w, Preview* pv, int sheet) {
    sheet = std::clamp(sheet, 0, std::max(0, pv->count - 1));
    if (sheet == pv->sheet) return;
    pv->sheet = sheet;
    updateLabel(w, pv);
    InvalidateRect(w, nullptr, FALSE);
}

void layoutControls(HWND w) {
    RECT rc;
    GetClientRect(w, &rc);
    const int y = rc.bottom - kBar + 12, bh = 28;
    MoveWindow(GetDlgItem(w, ID_PREV), 12, y, 90, bh, TRUE);
    MoveWindow(GetDlgItem(w, ID_NEXT), 108, y, 90, bh, TRUE);
    MoveWindow(GetDlgItem(w, ID_LABEL), 212, y + 6, std::max(10L, rc.right - 212 - 220), bh - 6, TRUE);
    MoveWindow(GetDlgItem(w, ID_PRINT), rc.right - 204, y, 96, bh, TRUE);
    MoveWindow(GetDlgItem(w, ID_CLOSE), rc.right - 102, y, 90, bh, TRUE);
}

LRESULT CALLBACK previewProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto* pv = reinterpret_cast<Preview*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            pv = static_cast<Preview*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pv));
            HINSTANCE inst = reinterpret_cast<CREATESTRUCTW*>(lp)->hInstance;
            NONCLIENTMETRICSW ncm{sizeof(ncm)};
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            pv->font = CreateFontIndirectW(&ncm.lfMessageFont);
            auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
                HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(pv->font), TRUE);
            };
            mk(L"BUTTON", L"\x2039  Previous", WS_TABSTOP | BS_PUSHBUTTON, ID_PREV);
            mk(L"BUTTON", L"Next  \x203A", WS_TABSTOP | BS_PUSHBUTTON, ID_NEXT);
            mk(L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, ID_LABEL);
            mk(L"BUTTON", L"Print", WS_TABSTOP | BS_DEFPUSHBUTTON, ID_PRINT);
            mk(L"BUTTON", L"Close", WS_TABSTOP | BS_PUSHBUTTON, ID_CLOSE);
            layoutControls(w);
            updateLabel(w, pv);
            return 0;
        }
        case WM_SIZE: layoutControls(w); InvalidateRect(w, nullptr, FALSE); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(w, &ps);
            RECT rc;
            GetClientRect(w, &rc);
            // double-buffered: the sheet area, then the bar
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bm = CreateCompatibleBitmap(dc, std::max(1L, rc.right), std::max(1L, rc.bottom));
            HGDIOBJ old = SelectObject(mem, bm);
            RECT area{0, 0, rc.right, rc.bottom - kBar};
            HBRUSH back = CreateSolidBrush(RGB(0x5A, 0x5E, 0x66));
            FillRect(mem, &area, back);
            DeleteObject(back);
            RECT bar{0, rc.bottom - kBar, rc.right, rc.bottom};
            FillRect(mem, &bar, GetSysColorBrush(COLOR_BTNFACE));
            const Paper& p = *pv->paper;
            const int availW = area.right - 40, availH = area.bottom - 40;
            if (availW > 20 && availH > 20 && p.physW > 0 && p.physH > 0) {
                const double scale = std::min(static_cast<double>(availW) / p.physW, static_cast<double>(availH) / p.physH);
                const int sw = static_cast<int>(p.physW * scale), sh = static_cast<int>(p.physH * scale);
                const int x = (area.right - sw) / 2, y = (area.bottom - sh) / 2;
                if (pv->cachedSheet != pv->sheet || std::abs(pv->cw - sw) > 1 || std::abs(pv->ch - sh) > 1) {
                    HCURSOR oc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
                    if (renderSheet(*pv->doc, *pv->layout, pv->sheet, p, scale, pv->px, pv->cw, pv->ch)) pv->cachedSheet = pv->sheet;
                    SetCursor(oc);
                }
                RECT shadow{x + 4, y + 4, x + sw + 4, y + sh + 4};
                HBRUSH sb = CreateSolidBrush(RGB(0x3A, 0x3D, 0x42));
                FillRect(mem, &shadow, sb);
                DeleteObject(sb);
                if (pv->cachedSheet == pv->sheet) blit(mem, x, y, sw, sh, pv->px, pv->cw, pv->ch);
            }
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bm);
            DeleteDC(mem);
            EndPaint(w, &ps);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_PREV: go(w, pv, pv->sheet - 1); return 0;
                case ID_NEXT: go(w, pv, pv->sheet + 1); return 0;
                case ID_PRINT: pv->print = true; pv->done = true; return 0;
                case ID_CLOSE: case IDCANCEL: pv->done = true; return 0;
            }
            break;
        case WM_MOUSEWHEEL: go(w, pv, pv->sheet + (GET_WHEEL_DELTA_WPARAM(wp) < 0 ? 1 : -1)); return 0;
        case WM_CLOSE: pv->done = true; return 0;
        case WM_DESTROY: if (pv && pv->font) DeleteObject(pv->font); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

} // namespace

bool preview(HWND parent, HINSTANCE inst, Document& d, const Layout& l, const Paper& p, const std::wstring& printerName, int copies) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = previewProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = reinterpret_cast<HICON>(SendMessageW(parent, WM_GETICON, ICON_BIG, 0));
        wc.lpszClassName = L"FeatherPrintPreview";
        registered = RegisterClassExW(&wc) != 0;
    }
    Preview pv;
    pv.doc = &d; pv.layout = &l; pv.paper = &p; pv.printer = printerName; pv.copies = copies; pv.count = sheetCount(l);
    if (!pv.count) return false;
    HMONITOR mon = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    // as tall as the screen allows, and as wide as the sheet needs at that height
    const int h = static_cast<int>((mi.rcWork.bottom - mi.rcWork.top) * 0.9);
    const int sheetW = p.physH > 0 ? static_cast<int>((h - kBar - 80) * static_cast<double>(p.physW) / p.physH) : h;
    const int wdt = std::min(static_cast<int>(mi.rcWork.right - mi.rcWork.left) - 40, std::max(640, sheetW + 60));
    const int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - wdt) / 2, y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
    HWND w = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FeatherPrintPreview", L"Print preview", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_VISIBLE,
                             x, y, wdt, h, parent, nullptr, inst, &pv);
    if (!w) return false;
    EnableWindow(parent, FALSE);
    MSG msg;
    while (!pv.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.hwnd == w || IsChild(w, msg.hwnd))) {
            switch (msg.wParam) {
                case VK_ESCAPE: pv.done = true; continue;
                case VK_RIGHT: case VK_NEXT: case VK_DOWN: go(w, &pv, pv.sheet + 1); continue;
                case VK_LEFT: case VK_PRIOR: case VK_UP: go(w, &pv, pv.sheet - 1); continue;
                case VK_HOME: go(w, &pv, 0); continue;
                case VK_END: go(w, &pv, pv.count - 1); continue;
                case VK_RETURN: pv.print = true; pv.done = true; continue;
            }
        }
        if (!IsDialogMessageW(w, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(w);
    SetActiveWindow(parent);
    return pv.print;
}

} // namespace printing
