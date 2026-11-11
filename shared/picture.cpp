#include "picture.hpp"
#include "contentgen.hpp"
#include "image.hpp"
#include "platform.hpp"
#ifdef _WIN32
#include <windows.h>   // the clipboard
#endif
#include <algorithm>
#include <cstring>
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

bool readFile(const std::wstring& path, std::vector<unsigned char>& out) { return platform::readFile(path, out); }

// A JPEG by its contents (FF D8 FF), whatever the file is called: a PNG or WebP named .jpg is not one.
bool isJpeg(const std::vector<unsigned char>& b) { return b.size() > 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF; }

// The orientation a camera wrote into a JPEG (EXIF tag 0x0112): 1 as stored, 6 turn a quarter clockwise, 8 a quarter
// anticlockwise, 3 a half turn; 2, 4, 5, 7 mirrored. 1 when there is none.
int jpegOrientation(const std::vector<unsigned char>& b) {
    size_t i = 2;
    while (i + 4 <= b.size() && b[i] == 0xFF) {
        const unsigned marker = b[i + 1];
        const size_t len = (static_cast<size_t>(b[i + 2]) << 8) | b[i + 3];
        if (marker == 0xDA || len < 2) break;   // the picture data: no more headers
        if (marker == 0xE1 && len > 14 && i + 2 + len <= b.size() && std::memcmp(&b[i + 4], "Exif\0\0", 6) == 0) {
            const size_t t = i + 10;   // the TIFF header
            const bool le = b[t] == 'I';
            auto u16 = [&](size_t o) -> unsigned { return o + 2 > b.size() ? 0 : le ? b[o] | (b[o + 1] << 8) : (b[o] << 8) | b[o + 1]; };
            auto u32 = [&](size_t o) -> size_t { return o + 4 > b.size() ? 0 : le ? (u16(o) | (static_cast<size_t>(u16(o + 2)) << 16)) : ((static_cast<size_t>(u16(o)) << 16) | u16(o + 2)); };
            const size_t ifd = t + u32(t + 4);
            const unsigned n = u16(ifd);
            for (unsigned k = 0; k < n && k < 512; ++k) {
                const size_t e = ifd + 2 + k * 12;
                if (e + 12 > i + 2 + len) break;
                if (u16(e) == 0x0112) { const unsigned v = u16(e + 8); return v >= 1 && v <= 8 ? static_cast<int>(v) : 1; }
            }
            return 1;
        }
        i += 2 + len;
    }
    return 1;
}

// The pixels turned and mirrored as `orientation` says, so the picture stands as the camera saw it.
image::Picture oriented(const image::Picture& p, int orientation) {
    if (orientation <= 1 || orientation > 8) return p;
    const bool swap = orientation >= 5;
    image::Picture o;
    o.w = swap ? p.h : p.w; o.h = swap ? p.w : p.h;
    o.px.resize(static_cast<size_t>(o.w) * o.h * 4);
    for (int y = 0; y < o.h; ++y)
        for (int x = 0; x < o.w; ++x) {
            int sx = x, sy = y;
            switch (orientation) {
                case 2: sx = p.w - 1 - x; break;
                case 3: sx = p.w - 1 - x; sy = p.h - 1 - y; break;
                case 4: sy = p.h - 1 - y; break;
                case 5: sx = y; sy = x; break;
                case 6: sx = y; sy = p.h - 1 - x; break;
                case 7: sx = p.w - 1 - y; sy = p.h - 1 - x; break;
                case 8: sx = p.w - 1 - y; sy = x; break;
            }
            std::memcpy(&o.px[(static_cast<size_t>(y) * o.w + x) * 4], &p.px[(static_cast<size_t>(sy) * p.w + sx) * 4], 4);
        }
    return o;
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
    d.beginChange(L"Rotate or flip a picture");
    pg = d.page(page);
    os.clear();
    if (!boundsOf(pg, objs, os, l, bo, r, t)) { d.cancelChange(); return false; }
    for (FPDF_PAGEOBJECT o : os) api.FPDFPageObj_Transform(o, a, b, c, dd, cx - (a * cx + c * cy), cy - (b * cx + dd * cy));
    contentgen::generate(d.handle(), pg);
    d.resetText(page);
    return true;
}

} // namespace

bool addPicture(Document& d, int page, double atX, double atY, const std::wstring& path, std::wstring& error, double forceWidthPt) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) { error = L"The page could not be found."; return false; }
    bool ok = false;
    {
        image::Picture bmp;
        std::vector<unsigned char> raw;
        const bool haveRaw = readFile(path, raw);
        const bool jpeg = haveRaw && isJpeg(raw);   // by what the file holds, not its name
        const int orientation = jpeg ? jpegOrientation(raw) : 1;
        if (!image::load(path, bmp) || bmp.w == 0 || bmp.h == 0) {
            error = L"That is not a picture Feather PDF can read (PNG, JPEG, BMP, GIF and TIFF are).";
        } else {
            if (orientation != 1) bmp = oriented(bmp, orientation);
            const int w = bmp.w, h = bmp.h;
            const auto& api = pdfium::api();
            const double vw = d.pageSize(page).w, vh = d.pageSize(page).h;
            double wPt = std::min(w * 0.75, vw * 0.5), hPt = wPt * h / w;
            if (hPt > vh * 0.8) { hPt = vh * 0.8; wPt = hPt * w / h; }
            if (forceWidthPt > 0) { wPt = forceWidthPt; hPt = wPt * h / w; }

            d.beginChange(L"Add a picture");
            pg = d.page(page);
            FPDF_PAGEOBJECT img = api.FPDFPageObj_NewImageObj(d.handle());
            bool loaded = false;
            std::vector<unsigned char> keep;   // must stay alive until the page content is generated
            if (img && jpeg) {
                // As stored; a turned photo is turned in pixels and saved as a JPEG again (high quality), so it stays small.
                MemAccess ma;
                if (orientation == 1 ? (ma.data = raw, true) : image::encode(bmp.px.data(), w, h, w * 4, image::Format::Jpeg, 92, false, ma.data)) {
                    ma.m_FileLen = static_cast<unsigned long>(ma.data.size());
                    ma.m_GetBlock = readBlock;
                    ma.m_Param = &ma;
                    loaded = api.FPDFImageObj_LoadJpegFileInline(&pg, 1, img, &ma) != 0;
                }
            }
            if (img && !loaded) {   // everything else (and JPEGs PDFium could not take) goes through pixels
                keep = bmp.px;
                FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, keep.data(), w * 4);
                if (fb) { loaded = api.FPDFImageObj_SetBitmap(&pg, 1, img, fb) != 0; api.FPDFBitmap_Destroy(fb); }
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
                contentgen::generate(d.handle(), pg);
                ok = true;
            }
        }
    }
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
    d.beginChange(L"Bring to front or send to back");
    pg = d.page(page);
    std::vector<FPDF_PAGEOBJECT> os;
    for (int i : sorted) os.push_back(api.FPDFPage_GetObject(pg, i));
    for (FPDF_PAGEOBJECT o : os) if (!o || !api.FPDFPage_RemoveObject(pg, o)) { d.undo(); return false; }
    objs.clear();
    for (size_t k = 0; k < os.size(); ++k) {
        if (toFront) { api.FPDFPage_InsertObject(pg, os[k]); objs.push_back(n - static_cast<int>(os.size()) + static_cast<int>(k)); }
        else { api.FPDFPage_InsertObjectAtIndex(pg, os[k], k); objs.push_back(static_cast<int>(k)); }
    }
    contentgen::generate(d.handle(), pg);
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
    d.beginChange(L"Adjust a picture");
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
    contentgen::generate(d.handle(), pg);
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
    image::Picture bmp;
    if (!image::load(path, bmp) || bmp.w == 0 || bmp.h == 0) { error = L"That is not a picture Feather PDF can read (PNG, JPEG, BMP, GIF and TIFF are)."; return false; }
    const int w = bmp.w, h = bmp.h;
    // fitted inside the old box, centred
    double bw = r - l, bh = t - b, nw = bw, nh = bw * h / w;
    if (nh > bh) { nh = bh; nw = bh * w / h; }
    const double nl = l + (bw - nw) / 2, nb = b + (bh - nh) / 2;
    std::wstring ext = path.substr(path.find_last_of(L'.') == std::wstring::npos ? path.size() : path.find_last_of(L'.'));
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    d.beginChange(L"Replace a picture");
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
        loaded = setPixels(pg, o, bmp.px, w, h, nl, nb, nl + nw, nb + nh);
    }
    if (!loaded) { d.undo(); error = L"The picture could not be put on the page."; return false; }
    contentgen::generate(d.handle(), pg);
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
    return image::save(px.data(), w, h, w * 4, image::Format::Png, 100, true, pngPath);
}

#ifdef _WIN32   // the Windows clipboard (the Mac app uses its own pasteboard code)
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
    d.renderOutput(page, fb, 0, 0, w, h);
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
#endif

} // namespace pageobj

namespace pageobj {

bool imageSize(const std::wstring& path, int& w, int& h) {
    if (!image::sizeOf(path, w, h)) return false;
    std::vector<unsigned char> raw;   // a photo turned by its camera: the size as it stands
    if (readFile(path, raw) && isJpeg(raw) && jpegOrientation(raw) >= 5) std::swap(w, h);
    return true;
}

#ifdef _WIN32
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
    d.renderOutput(page, fb, -left, -top, fw, fh);
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

#endif

} // namespace pageobj
