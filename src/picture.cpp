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


// The picture as it appears on the page, rendered with `k` pixels per point (BGRA, top row = top of the bounding box).
FPDF_BITMAP renderPicture(Document& d, FPDF_PAGE pg, FPDF_PAGEOBJECT o, double& k, float& l, float& b, float& r, float& t) {
    const auto& api = pdfium::api();
    if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) || r - l < 0.5f || t - b < 0.5f) return nullptr;
    unsigned int W = 0, H = 0;
    api.FPDFImageObj_GetImagePixelSize(o, &W, &H);
    FS_MATRIX m;
    if (!api.FPDFPageObj_GetMatrix(o, &m)) return nullptr;
    const double wPt = std::hypot(m.a, m.b), hPt = std::hypot(m.c, m.d);
    k = 1.0;
    if (wPt > 0 && hPt > 0) k = std::max({1.0, W / wPt, H / hPt});
    while (k > 1.0 && (r - l) * k * (t - b) * k > 40e6) k *= 0.8;   // keep it under 40 million pixels
    FS_MATRIX big{static_cast<float>(m.a * k), static_cast<float>(m.b * k), static_cast<float>(m.c * k), static_cast<float>(m.d * k), static_cast<float>(m.e * k), static_cast<float>(m.f * k)};
    api.FPDFPageObj_SetMatrix(o, &big);
    FPDF_BITMAP bmp = api.FPDFImageObj_GetRenderedBitmap(d.handle(), pg, o);
    api.FPDFPageObj_SetMatrix(o, &m);
    return bmp;
}

// Copies a BGRA bitmap (any stride) into tightly packed rows.
std::vector<unsigned char> pixelsOf(FPDF_BITMAP bmp, int& w, int& h) {
    const auto& api = pdfium::api();
    w = api.FPDFBitmap_GetWidth(bmp); h = api.FPDFBitmap_GetHeight(bmp);
    const int stride = api.FPDFBitmap_GetStride(bmp);
    const unsigned char* src = static_cast<const unsigned char*>(api.FPDFBitmap_GetBuffer(bmp));
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) memcpy(px.data() + static_cast<size_t>(y) * w * 4, src + static_cast<size_t>(y) * stride, static_cast<size_t>(w) * 4);
    return px;
}

// Puts packed BGRA pixels into the picture object and shows them, upright, in the page box. Opaque pictures get no mask.
bool setPixels(FPDF_PAGE pg, FPDF_PAGEOBJECT o, std::vector<unsigned char>& px, int w, int h, double l, double b, double r, double t) {
    const auto& api = pdfium::api();
    bool opaque = true;
    for (size_t i = 3; i < px.size(); i += 4) if (px[i] != 255) { opaque = false; break; }
    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, opaque ? FPDFBitmap_BGRx : FPDFBitmap_BGRA, px.data(), w * 4);
    if (!fb) return false;
    const bool ok = api.FPDFImageObj_SetBitmap(&pg, 1, o, fb) != 0;
    api.FPDFBitmap_Destroy(fb);
    if (!ok) return false;
    FS_MATRIX m{static_cast<float>(r - l), 0, 0, static_cast<float>(t - b), static_cast<float>(l), static_cast<float>(b)};
    return api.FPDFPageObj_SetMatrix(o, &m) != 0;
}

FPDF_PAGEOBJECT pictureObject(Document& d, int page, int obj) {
    const auto& api = pdfium::api();
    FPDF_PAGE pg = d.page(page);
    if (!pg || obj < 0 || obj >= api.FPDFPage_CountObjects(pg)) return nullptr;
    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, obj);
    return o && api.FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_IMAGE ? o : nullptr;
}

struct Gdi {
    ULONG_PTR token = 0;
    bool ok = false;
    Gdi() { Gdiplus::GdiplusStartupInput in; ok = Gdiplus::GdiplusStartup(&token, &in, nullptr) == Gdiplus::Ok; }
    ~Gdi() { if (ok) Gdiplus::GdiplusShutdown(token); }
};

bool pngEncoder(CLSID& out) {
    UINT n = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&n, &size) != Gdiplus::Ok || !size) return false;
    std::vector<unsigned char> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(n, size, codecs) != Gdiplus::Ok) return false;
    for (UINT i = 0; i < n; ++i) if (wcscmp(codecs[i].MimeType, L"image/png") == 0) { out = codecs[i].Clsid; return true; }
    return false;
}

// Bounding box of objects on a page, and the objects themselves.
bool boundsOf(FPDF_PAGE pg, const std::vector<int>& objs, std::vector<FPDF_PAGEOBJECT>& out, double& l, double& b, double& r, double& t) {
    const auto& api = pdfium::api();
    const int n = api.FPDFPage_CountObjects(pg);
    l = b = 1e18; r = t = -1e18;
    for (int i : objs) {
        if (i < 0 || i >= n) return false;
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        float ol, ob, orr, ot;
        if (!o || !api.FPDFPageObj_GetBounds(o, &ol, &ob, &orr, &ot)) return false;
        out.push_back(o);
        l = std::min<double>(l, ol); b = std::min<double>(b, ob); r = std::max<double>(r, orr); t = std::max<double>(t, ot);
    }
    return !out.empty();
}

// Applies the 2x2 matrix (a b c d) about the centre of the objects' bounding box.
bool transformAboutCentre(Document& d, int page, const std::vector<int>& objs, double a, double b, double c, double dd) {
    const auto& api = pdfium::api();
    FPDF_PAGE pg = d.page(page);
    std::vector<FPDF_PAGEOBJECT> os;
    double l, bo, r, t;
    if (!pg || !boundsOf(pg, objs, os, l, bo, r, t)) return false;
    const double cx = (l + r) / 2, cy = (bo + t) / 2;
    d.beginChange();
    pg = d.page(page);
    os.clear();
    if (!boundsOf(pg, objs, os, l, bo, r, t)) { d.cancelChange(); return false; }
    for (FPDF_PAGEOBJECT o : os) api.FPDFPageObj_Transform(o, a, b, c, dd, cx - (a * cx + c * cy), cy - (b * cx + dd * cy));
    api.FPDFPage_GenerateContent(pg);
    d.resetText(page);
    return true;
}

} // namespace

bool addPicture(Document& d, int page, double atX, double atY, const std::wstring& path, std::wstring& error, double forceWidthPt) {
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
            if (forceWidthPt > 0) { wPt = forceWidthPt; hPt = wPt * h / w; }
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


bool rotateGroup(Document& d, int page, const std::vector<int>& objs, int quarterTurnsClockwise) {
    const int q = ((quarterTurnsClockwise % 4) + 4) % 4;
    if (!q) return false;
    static const double m[4][4] = {{1, 0, 0, 1}, {0, -1, 1, 0}, {-1, 0, 0, -1}, {0, 1, -1, 0}};   // clockwise in page space
    return transformAboutCentre(d, page, objs, m[q][0], m[q][1], m[q][2], m[q][3]);
}

bool flipGroup(Document& d, int page, const std::vector<int>& objs, bool horizontal) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    if (pdfium::api().FPDFPage_GetRotation(pg) % 2) horizontal = !horizontal;   // a turned page shows the other axis across
    return horizontal ? transformAboutCentre(d, page, objs, -1, 0, 0, 1) : transformAboutCentre(d, page, objs, 1, 0, 0, -1);
}

bool restackGroup(Document& d, int page, std::vector<int>& objs, bool toFront) {
    const auto& api = pdfium::api();
    FPDF_PAGE pg = d.page(page);
    if (!pg || objs.empty()) return false;
    std::vector<int> sorted = objs;
    std::sort(sorted.begin(), sorted.end());
    const int n = api.FPDFPage_CountObjects(pg);
    if (sorted.front() < 0 || sorted.back() >= n) return false;
    d.beginChange();
    pg = d.page(page);
    std::vector<FPDF_PAGEOBJECT> os;
    for (int i : sorted) os.push_back(api.FPDFPage_GetObject(pg, i));
    for (FPDF_PAGEOBJECT o : os) if (!o || !api.FPDFPage_RemoveObject(pg, o)) { d.undo(); return false; }
    objs.clear();
    for (size_t k = 0; k < os.size(); ++k) {
        if (toFront) { api.FPDFPage_InsertObject(pg, os[k]); objs.push_back(n - static_cast<int>(os.size()) + static_cast<int>(k)); }
        else { api.FPDFPage_InsertObjectAtIndex(pg, os[k], k); objs.push_back(static_cast<int>(k)); }
    }
    api.FPDFPage_GenerateContent(pg);
    d.resetText(page);
    return true;
}

bool isPicture(Document& d, int page, const std::vector<int>& objs) {
    return objs.size() == 1 && pictureObject(d, page, objs[0]) != nullptr;
}

namespace {
// Renders the picture, lets `change` edit the pixels and the kept page box, and puts the result back. One undo step.
template <class F> bool repaintPicture(Document& d, int page, int obj, F change) {
    if (!pictureObject(d, page, obj)) return false;
    d.beginChange();
    FPDF_PAGE pg = d.page(page);
    FPDF_PAGEOBJECT o = pictureObject(d, page, obj);
    double k;
    float l, b, r, t;
    FPDF_BITMAP bmp = o ? renderPicture(d, pg, o, k, l, b, r, t) : nullptr;
    if (!bmp) { d.cancelChange(); return false; }
    int w, h;
    std::vector<unsigned char> px = pixelsOf(bmp, w, h);
    pdfium::api().FPDFBitmap_Destroy(bmp);
    double L = l, B = b, R = l + w / k, T = t;   // the rendered box (it may be a pixel larger than the bounds)
    B = T - h / k;
    if (!change(px, w, h, k, L, B, R, T) || w < 1 || h < 1 || !setPixels(pg, o, px, w, h, L, B, R, T)) { d.undo(); return false; }
    pdfium::api().FPDFPage_GenerateContent(pg);
    return true;
}
} // namespace

bool cropPicture(Document& d, int page, int obj, double left, double bottom, double right, double top) {
    if (left > right) std::swap(left, right);
    if (bottom > top) std::swap(bottom, top);
    return repaintPicture(d, page, obj, [&](std::vector<unsigned char>& px, int& w, int& h, double k, double& L, double& B, double& R, double& T) {
        const int x0 = std::max(0, static_cast<int>(std::floor((left - L) * k))), x1 = std::min(w, static_cast<int>(std::ceil((right - L) * k)));
        const int y0 = std::max(0, static_cast<int>(std::floor((T - top) * k))), y1 = std::min(h, static_cast<int>(std::ceil((T - bottom) * k)));
        if (x1 - x0 < 1 || y1 - y0 < 1) return false;
        std::vector<unsigned char> out(static_cast<size_t>(x1 - x0) * (y1 - y0) * 4);
        for (int y = y0; y < y1; ++y) memcpy(out.data() + static_cast<size_t>(y - y0) * (x1 - x0) * 4, px.data() + (static_cast<size_t>(y) * w + x0) * 4, static_cast<size_t>(x1 - x0) * 4);
        const double nl = L + x0 / k, nr = L + x1 / k, nt = T - y0 / k, nb = T - y1 / k;
        px.swap(out); w = x1 - x0; h = y1 - y0;
        L = nl; R = nr; T = nt; B = nb;
        return true;
    });
}

bool adjustPicture(Document& d, int page, int obj, int brightness, int contrast, bool grey) {
    brightness = std::clamp(brightness, -100, 100); contrast = std::clamp(contrast, -100, 100);
    if (!brightness && !contrast && !grey) return false;
    const double cf = contrast >= 0 ? 1.0 + contrast / 50.0 : 1.0 + contrast / 100.0, add = brightness * 1.28;
    unsigned char lut[256];
    for (int v = 0; v < 256; ++v) lut[v] = static_cast<unsigned char>(std::clamp((v - 127.5) * cf + 127.5 + add, 0.0, 255.0) + 0.5 > 255.0 ? 255 : static_cast<int>(std::clamp((v - 127.5) * cf + 127.5 + add, 0.0, 255.0) + 0.5));
    return repaintPicture(d, page, obj, [&](std::vector<unsigned char>& px, int&, int&, double, double&, double&, double&, double&) {
        for (size_t i = 0; i + 3 < px.size(); i += 4) {
            if (grey) { const unsigned char y = static_cast<unsigned char>((px[i] * 29 + px[i + 1] * 150 + px[i + 2] * 77) >> 8); px[i] = px[i + 1] = px[i + 2] = y; }
            px[i] = lut[px[i]]; px[i + 1] = lut[px[i + 1]]; px[i + 2] = lut[px[i + 2]];
        }
        return true;
    });
}

bool replacePicture(Document& d, int page, int obj, const std::wstring& path, std::wstring& error) {
    FPDF_PAGEOBJECT o = pictureObject(d, page, obj);
    if (!o) { error = L"No picture is selected."; return false; }
    const auto& api = pdfium::api();
    float l, b, r, t;
    if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) { error = L"The picture could not be measured."; return false; }
    Gdi gdi;
    if (!gdi.ok) { error = L"The picture reader could not be started."; return false; }
    Gdiplus::Bitmap bmp(path.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok || bmp.GetWidth() == 0 || bmp.GetHeight() == 0) { error = L"That is not a picture Feather PDF can read (PNG, JPEG, BMP, GIF and TIFF are)."; return false; }
    const int w = static_cast<int>(bmp.GetWidth()), h = static_cast<int>(bmp.GetHeight());
    // fitted inside the old box, centred
    double bw = r - l, bh = t - b, nw = bw, nh = bw * h / w;
    if (nh > bh) { nh = bh; nw = bh * w / h; }
    const double nl = l + (bw - nw) / 2, nb = b + (bh - nh) / 2;
    std::wstring ext = path.substr(path.find_last_of(L'.') == std::wstring::npos ? path.size() : path.find_last_of(L'.'));
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    d.beginChange();
    FPDF_PAGE pg = d.page(page);
    o = pictureObject(d, page, obj);
    bool loaded = false;
    if (o && (ext == L".jpg" || ext == L".jpeg")) {
        MemAccess ma;
        if (readFile(path, ma.data)) {
            ma.m_FileLen = static_cast<unsigned long>(ma.data.size());
            ma.m_GetBlock = readBlock;
            ma.m_Param = &ma;
            loaded = api.FPDFImageObj_LoadJpegFileInline(&pg, 1, o, &ma) != 0;
        }
        FS_MATRIX m{static_cast<float>(nw), 0, 0, static_cast<float>(nh), static_cast<float>(nl), static_cast<float>(nb)};
        if (loaded) api.FPDFPageObj_SetMatrix(o, &m);
    }
    if (o && !loaded) {
        std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
        Gdiplus::BitmapData bd;
        Gdiplus::Rect all(0, 0, w, h);
        if (bmp.LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
            for (int y = 0; y < h; ++y) memcpy(px.data() + static_cast<size_t>(y) * w * 4, static_cast<unsigned char*>(bd.Scan0) + static_cast<size_t>(y) * bd.Stride, static_cast<size_t>(w) * 4);
            bmp.UnlockBits(&bd);
            loaded = setPixels(pg, o, px, w, h, nl, nb, nl + nw, nb + nh);
        }
    }
    if (!loaded) { d.undo(); error = L"The picture could not be put on the page."; return false; }
    api.FPDFPage_GenerateContent(pg);
    return true;
}

bool savePicture(Document& d, int page, int obj, const std::wstring& pngPath) {
    FPDF_PAGEOBJECT o = pictureObject(d, page, obj);
    if (!o) return false;
    double k;
    float l, b, r, t;
    FPDF_BITMAP bmp = renderPicture(d, d.page(page), o, k, l, b, r, t);
    if (!bmp) return false;
    int w, h;
    std::vector<unsigned char> px = pixelsOf(bmp, w, h);
    pdfium::api().FPDFBitmap_Destroy(bmp);
    Gdi gdi;
    CLSID png;
    if (!gdi.ok || !pngEncoder(png)) return false;
    Gdiplus::Bitmap out(w, h, w * 4, PixelFormat32bppARGB, px.data());
    return out.Save(pngPath.c_str(), &png, nullptr) == Gdiplus::Ok;
}

bool clipboardHasPicture() {
    return IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(RegisterClipboardFormatW(L"PNG"));
}

bool pastePicture(Document& d, int page, double atX, double atY, std::wstring& error) {
    if (!OpenClipboard(nullptr)) { error = L"The clipboard could not be opened."; return false; }
    std::vector<unsigned char> file;
    std::wstring ext;
    const UINT pngFmt = RegisterClipboardFormatW(L"PNG");
    if (HANDLE hp = IsClipboardFormatAvailable(pngFmt) ? GetClipboardData(pngFmt) : nullptr) {   // keeps transparency
        if (const void* p = GlobalLock(hp)) { const auto* c = static_cast<const unsigned char*>(p); file.assign(c, c + GlobalSize(hp)); GlobalUnlock(hp); ext = L".png"; }
    }
    if (file.empty())
        if (HANDLE hd = GetClipboardData(CF_DIB)) {
            if (const void* p = GlobalLock(hd)) {
                const SIZE_T size = GlobalSize(hd);
                const auto* bi = static_cast<const BITMAPINFOHEADER*>(p);
                DWORD colours = bi->biClrUsed ? bi->biClrUsed : (bi->biBitCount <= 8 ? 1u << bi->biBitCount : 0);
                DWORD offset = sizeof(BITMAPFILEHEADER) + bi->biSize + colours * 4 + (bi->biCompression == BI_BITFIELDS && bi->biSize == sizeof(BITMAPINFOHEADER) ? 12 : 0);
                BITMAPFILEHEADER fh{};
                fh.bfType = 0x4D42;
                fh.bfSize = static_cast<DWORD>(sizeof(fh) + size);
                fh.bfOffBits = offset;
                file.resize(sizeof(fh));
                memcpy(file.data(), &fh, sizeof(fh));
                file.insert(file.end(), static_cast<const unsigned char*>(p), static_cast<const unsigned char*>(p) + size);
                GlobalUnlock(hd);
                ext = L".bmp";
            }
        }
    CloseClipboard();
    if (file.empty()) { error = L"The clipboard holds no picture."; return false; }
    wchar_t dir[MAX_PATH], tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"fpp", 0, tmp);
    DeleteFileW(tmp);
    const std::wstring path = std::wstring(tmp) + ext;
    if (Document::writeFile(path, file) != SaveResult::Ok) { error = L"The picture could not be read from the clipboard."; return false; }
    const bool ok = addPicture(d, page, atX, atY, path, error);
    DeleteFileW(path.c_str());
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

namespace pageobj {

bool imageSize(const std::wstring& path, int& w, int& h) {
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &in, nullptr) != Gdiplus::Ok) return false;
    bool ok = false;
    {
        Gdiplus::Bitmap bmp(path.c_str());
        if (bmp.GetLastStatus() == Gdiplus::Ok && bmp.GetWidth() > 0 && bmp.GetHeight() > 0) { w = static_cast<int>(bmp.GetWidth()); h = static_cast<int>(bmp.GetHeight()); ok = true; }
    }
    Gdiplus::GdiplusShutdown(token);
    return ok;
}

bool copyAreaToClipboard(Document& d, int page, double l, double b, double r, double t, void* owner, int dpi) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const auto& api = pdfium::api();
    const PageSize& ps = d.pageSize(page);
    const double sc = dpi / 72.0;
    const int fw = std::max(1, static_cast<int>(std::lround(ps.w * sc))), fh = std::max(1, static_cast<int>(std::lround(ps.h * sc)));
    int x1, y1, x2, y2;
    api.FPDF_PageToDevice(pg, 0, 0, fw, fh, 0, l, t, &x1, &y1);
    api.FPDF_PageToDevice(pg, 0, 0, fw, fh, 0, r, b, &x2, &y2);
    const int left = std::min(x1, x2), top = std::min(y1, y2), w = std::abs(x2 - x1), h = std::abs(y2 - y1);
    if (w < 2 || h < 2 || 1LL * w * h > 80000000LL) return false;
    const size_t stride = static_cast<size_t>(w) * 4;
    std::vector<unsigned char> px(stride * h);
    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, px.data(), static_cast<int>(stride));
    if (!fb) return false;
    api.FPDFBitmap_FillRect(fb, 0, 0, w, h, 0xFFFFFFFF);
    d.renderPage(page, fb, -left, -top, fw, fh);
    api.FPDFBitmap_Destroy(fb);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + stride * h);
    if (!g) return false;
    auto* mem = static_cast<unsigned char*>(GlobalLock(g));
    *reinterpret_cast<BITMAPINFOHEADER*>(mem) = {sizeof(BITMAPINFOHEADER), w, h, 1, 32, BI_RGB, static_cast<DWORD>(stride * h), 0, 0, 0, 0};
    unsigned char* dst = mem + sizeof(BITMAPINFOHEADER);
    for (int y = 0; y < h; ++y) memcpy(dst + static_cast<size_t>(y) * stride, px.data() + static_cast<size_t>(h - 1 - y) * stride, stride);
    GlobalUnlock(g);
    if (!OpenClipboard(static_cast<HWND>(owner))) { GlobalFree(g); return false; }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_DIB, g) != nullptr;
    CloseClipboard();
    if (!ok) GlobalFree(g);
    return ok;
}

} // namespace pageobj
