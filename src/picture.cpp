#include "picture.hpp"
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>
#include <cmath>
#include <cwctype>

namespace pageobj {
namespace {

struct MemAccess : FPDF_FILEACCESS {
    std::vector<unsigned char> data;
};

int readBlock(void* param, unsigned long pos, unsigned char* buf, unsigned long size) {
    auto* m = static_cast<MemAccess*>(param);
    if (pos + size > m->data.size()) return 0;
    memcpy(buf, m->data.data() + pos, size);
    return 1;
}

bool readFile(const std::wstring& path, std::vector<unsigned char>& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    if (sz.QuadPart > 200LL * 1024 * 1024) { CloseHandle(f); return false; }
    out.resize(static_cast<size_t>(sz.QuadPart));
    size_t got = 0;
    while (got < out.size()) {
        DWORD n = 0;
        if (!ReadFile(f, out.data() + got, static_cast<DWORD>(out.size() - got), &n, nullptr) || !n) break;
        got += n;
    }
    CloseHandle(f);
    return got == out.size();
}

} // namespace

bool addPicture(Document& d, int page, double atX, double atY, const std::wstring& path, std::wstring& error) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) { error = L"The page could not be found."; return false; }
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &in, nullptr) != Gdiplus::Ok) { error = L"The picture reader could not be started."; return false; }
    bool ok = false;
    {
        Gdiplus::Bitmap bmp(path.c_str());
        if (bmp.GetLastStatus() != Gdiplus::Ok || bmp.GetWidth() == 0 || bmp.GetHeight() == 0) {
            error = L"That is not a picture Feather PDF can read (PNG, JPEG, BMP, GIF and TIFF are).";
        } else {
            const int w = static_cast<int>(bmp.GetWidth()), h = static_cast<int>(bmp.GetHeight());
            const auto& api = pdfium::api();
            const double vw = d.pageSize(page).w, vh = d.pageSize(page).h;
            double wPt = std::min(w * 0.75, vw * 0.5), hPt = wPt * h / w;
            if (hPt > vh * 0.8) { hPt = vh * 0.8; wPt = hPt * w / h; }
            std::wstring ext = path.substr(path.find_last_of(L'.') == std::wstring::npos ? path.size() : path.find_last_of(L'.'));
            for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
            const bool jpeg = ext == L".jpg" || ext == L".jpeg";

            d.beginChange();
            pg = d.page(page);
            FPDF_PAGEOBJECT img = api.FPDFPageObj_NewImageObj(d.handle());
            bool loaded = false;
            std::vector<unsigned char> keep;   // must stay alive until the page content is generated
            if (img && jpeg) {
                MemAccess ma;
                if (readFile(path, ma.data)) {
                    ma.m_FileLen = static_cast<unsigned long>(ma.data.size());
                    ma.m_GetBlock = readBlock;
                    ma.m_Param = &ma;
                    loaded = api.FPDFImageObj_LoadJpegFileInline(&pg, 1, img, &ma) != 0;
                }
            }
            if (img && !loaded) {   // everything else (and JPEGs PDFium could not take) goes through pixels
                keep.resize(static_cast<size_t>(w) * h * 4);
                Gdiplus::BitmapData bd;
                Gdiplus::Rect all(0, 0, w, h);
                if (bmp.LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
                    for (int y = 0; y < h; ++y) memcpy(keep.data() + static_cast<size_t>(y) * w * 4, static_cast<unsigned char*>(bd.Scan0) + static_cast<size_t>(y) * bd.Stride, static_cast<size_t>(w) * 4);
                    bmp.UnlockBits(&bd);
                    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, keep.data(), w * 4);
                    if (fb) { loaded = api.FPDFImageObj_SetBitmap(&pg, 1, img, fb) != 0; api.FPDFBitmap_Destroy(fb); }
                }
            }
            if (!img || !loaded) {
                if (img) api.FPDFPageObj_Destroy(img);
                d.cancelChange();
                error = L"The picture could not be put on the page.";
            } else {
                // The picture's unit square goes to a wPt x hPt box whose top-left is the click, as the page is displayed.
                const double S = 100.0;
                int dx = 0, dy = 0;
                api.FPDF_PageToDevice(pg, 0, 0, static_cast<int>(vw * S), static_cast<int>(vh * S), 0, atX, atY, &dx, &dy);
                const double vx = dx / S, vy = dy / S;
                auto toPage = [&](double x, double y) {
                    double px = 0, py = 0;
                    api.FPDF_DeviceToPage(pg, 0, 0, static_cast<int>(vw * S), static_cast<int>(vh * S), 0, static_cast<int>(x * S), static_cast<int>(y * S), &px, &py);
                    return std::pair<double, double>(px, py);
                };
                auto p0 = toPage(vx, vy + hPt);      // bottom-left on screen
                auto pX = toPage(vx + 1, vy + hPt);  // one point right
                auto pY = toPage(vx, vy + hPt - 1);  // one point up
                api.FPDFImageObj_SetMatrix(img, (pX.first - p0.first) * wPt, (pX.second - p0.second) * wPt,
                                           (pY.first - p0.first) * hPt, (pY.second - p0.second) * hPt, p0.first, p0.second);
                api.FPDFPage_InsertObject(pg, img);
                api.FPDFPage_GenerateContent(pg);
                ok = true;
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
    return ok;
}

bool copyPageToClipboard(Document& d, int page, void* owner, int dpi) {
    const PageSize& ps = d.pageSize(page);
    const int w = std::max(1, static_cast<int>(std::lround(ps.w * dpi / 72.0)));
    const int h = std::max(1, static_cast<int>(std::lround(ps.h * dpi / 72.0)));
    if (1LL * w * h > 80000000LL) return false;   // too large for the clipboard
    const size_t stride = static_cast<size_t>(w) * 4;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + stride * h);
    if (!g) return false;
    auto* mem = static_cast<unsigned char*>(GlobalLock(g));
    BITMAPINFOHEADER* bi = reinterpret_cast<BITMAPINFOHEADER*>(mem);
    *bi = {sizeof(BITMAPINFOHEADER), w, h, 1, 32, BI_RGB, static_cast<DWORD>(stride * h), 0, 0, 0, 0};
    std::vector<unsigned char> px(stride * h);
    const auto& api = pdfium::api();
    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, px.data(), static_cast<int>(stride));
    if (!fb) { GlobalUnlock(g); GlobalFree(g); return false; }
    api.FPDFBitmap_FillRect(fb, 0, 0, w, h, 0xFFFFFFFF);
    d.renderPage(page, fb, 0, 0, w, h);
    api.FPDFBitmap_Destroy(fb);
    unsigned char* dst = mem + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < h; ++y) memcpy(dst + static_cast<size_t>(y) * stride, px.data() + static_cast<size_t>(h - 1 - y) * stride, stride);   // bottom-up
    GlobalUnlock(g);
    if (!OpenClipboard(static_cast<HWND>(owner))) { GlobalFree(g); return false; }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_DIB, g) != nullptr;
    if (!ok) GlobalFree(g);
    CloseClipboard();
    return ok;
}

} // namespace pageobj
