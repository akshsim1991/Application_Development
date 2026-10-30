#include "visualdiff.hpp"
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>
#include <cmath>
#include <vector>

namespace visualdiff {
namespace {

bool render(Document& d, int page, int w, int h, std::vector<unsigned char>& px) {
    px.assign(static_cast<size_t>(w) * h * 4, 255);
    if (page < 0 || page >= d.pageCount()) return true;   // a missing page is blank
    const auto& api = pdfium::api();
    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, px.data(), w * 4);
    if (!fb) return false;
    api.FPDFBitmap_FillRect(fb, 0, 0, w, h, 0xFFFFFFFF);
    d.renderOutput(page, fb, 0, 0, w, h);
    api.FPDFBitmap_Destroy(fb);
    return true;
}

bool encoder(CLSID& out) {
    UINT n = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&n, &size);
    if (!size) return false;
    std::vector<unsigned char> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(n, size, codecs);
    for (UINT i = 0; i < n; ++i) if (wcscmp(codecs[i].MimeType, L"image/png") == 0) { out = codecs[i].Clsid; return true; }
    return false;
}

} // namespace

PageResult overlay(Document& a, int pa, Document& b, int pb, int dpi, const std::wstring& pngPath) {
    PageResult res;
    const PageSize sa = pa < a.pageCount() ? a.pageSize(pa) : (pb < b.pageCount() ? b.pageSize(pb) : PageSize{595, 842});
    const int w = std::max(8, static_cast<int>(std::lround(sa.w * dpi / 72.0))), h = std::max(8, static_cast<int>(std::lround(sa.h * dpi / 72.0)));
    if (1LL * w * h > 40000000LL) return res;
    std::vector<unsigned char> A, B;
    if (!render(a, pa, w, h, A) || !render(b, pb, w, h, B)) return res;
    auto ink = [&](const std::vector<unsigned char>& p, int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return false;
        const unsigned char* q = &p[(static_cast<size_t>(y) * w + x) * 4];
        return (q[0] + q[1] * 2 + q[2]) / 4 < 190;   // dark enough to be content
    };
    // a pixel counts as different only when the other page has no ink within one pixel (antialiasing and rounding)
    auto nearInk = [&](const std::vector<unsigned char>& p, int x, int y) {
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (ink(p, x + dx, y + dy)) return true;
        return false;
    };
    std::vector<unsigned char> out(static_cast<size_t>(w) * h * 4, 255);
    long diff = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char* o = &out[(static_cast<size_t>(y) * w + x) * 4];   // BGRA
            const bool ia = ink(A, x, y), ib = ink(B, x, y);
            if (ia && !nearInk(B, x, y)) { o[0] = 0x30; o[1] = 0x30; o[2] = 0xE0; ++diff; }          // only in the first: red
            else if (ib && !nearInk(A, x, y)) { o[0] = 0x30; o[1] = 0xB0; o[2] = 0x30; ++diff; }     // only in the second: green
            else if (ia || ib) { o[0] = o[1] = o[2] = 0x90; }                                       // the same: grey
        }
    // make the differences easy to see: a halo of a few pixels in the same colour
    {
        std::vector<unsigned char> halo = out;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const unsigned char* o = &out[(static_cast<size_t>(y) * w + x) * 4];
                const bool red = o[2] == 0xE0 && o[0] == 0x30, green = o[1] == 0xB0 && o[0] == 0x30;
                if (!red && !green) continue;
                for (int dy = -3; dy <= 3; ++dy)
                    for (int dx = -3; dx <= 3; ++dx) {
                        const int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                        unsigned char* q = &halo[(static_cast<size_t>(ny) * w + nx) * 4];
                        if (q[0] == 0x30 && (q[2] == 0xE0 || q[1] == 0xB0)) continue;   // already a difference
                        if (red) { q[0] = 0xB0; q[1] = 0xB0; q[2] = 0xFF; } else { q[0] = 0xB0; q[1] = 0xFF; q[2] = 0xB0; }
                    }
            }
        for (size_t i = 0; i < out.size(); i += 4) {   // the strong pixels go back on top of the halo
            const unsigned char* o = &out[i];
            if (o[0] == 0x30 && (o[2] == 0xE0 || o[1] == 0xB0)) { halo[i] = o[0]; halo[i + 1] = o[1]; halo[i + 2] = o[2]; }
        }
        out.swap(halo);
    }
    res.differingPixels = diff;
    res.percent = 100.0 * diff / (static_cast<double>(w) * h);
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &in, nullptr) != Gdiplus::Ok) return res;
    {
        Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppRGB, out.data());
        CLSID png;
        res.ok = encoder(png) && bmp.Save(pngPath.c_str(), &png, nullptr) == Gdiplus::Ok;
    }
    Gdiplus::GdiplusShutdown(token);
    return res;
}

} // namespace visualdiff
