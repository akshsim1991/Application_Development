#include "compress.hpp"
#include "contentgen.hpp"
#include "image.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace compress {
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

// Encodes BGR/BGRA/gray pixels as a JPEG of w2 x h2 pixels (scaled down).
bool encode(const unsigned char* src, int w, int h, int stride, int channels, int w2, int h2, int quality, bool grey,
            std::vector<unsigned char>& out) {
    // Build a 32-bit BGRA copy (GDI+ needs a known layout), expanding gray.
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const unsigned char* s = src + static_cast<size_t>(y) * stride;
        unsigned char* d = px.data() + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x, d += 4) {
            if (channels == 1) { d[0] = d[1] = d[2] = s[x]; }
            else { d[0] = s[x * channels]; d[1] = s[x * channels + 1]; d[2] = s[x * channels + 2]; }
            d[3] = 255;
        }
    }
    image::Picture scaled;
    if (!image::scale(px.data(), w, h, w * 4, w2, h2, scaled)) return false;
    if (grey)
        for (size_t i = 0; i < scaled.px.size(); i += 4) {
            unsigned char* p = &scaled.px[i];
            const unsigned char g8 = static_cast<unsigned char>((p[0] * 29 + p[1] * 150 + p[2] * 77) >> 8);
            p[0] = p[1] = p[2] = g8;
        }
    return image::encode(scaled.px.data(), w2, h2, w2 * 4, image::Format::Jpeg, quality, false, out);
}

// One pass over all pictures with the given settings. Returns the number replaced.
int pass(Document& d, int dpi, int quality, bool grey) {
    const auto& api = pdfium::api();
    int replaced = 0;
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        bool pageChanged = false;
        const int n = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < n; ++i) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE) continue;
            float l, b, r, t;
            if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
            const double wpt = std::fabs(r - l), hpt = std::fabs(t - b);
            if (wpt < 1 || hpt < 1) continue;
            FPDF_BITMAP bmp = api.FPDFImageObj_GetBitmap(o);
            if (!bmp) continue;
            const int w = api.FPDFBitmap_GetWidth(bmp), h = api.FPDFBitmap_GetHeight(bmp), stride = api.FPDFBitmap_GetStride(bmp);
            const int fmt = api.FPDFBitmap_GetFormat(bmp);
            const unsigned char* buf = static_cast<const unsigned char*>(api.FPDFBitmap_GetBuffer(bmp));
            int ch = fmt == FPDFBitmap_Gray ? 1 : fmt == FPDFBitmap_BGR ? 3 : (fmt == FPDFBitmap_BGRx || fmt == FPDFBitmap_BGRA) ? 4 : 0;
            bool skip = ch == 0 || !buf || w < 16 || h < 16;
            if (!skip && fmt == FPDFBitmap_BGRA) {   // transparent parts cannot be kept in a JPEG
                for (int y = 0; y < h && !skip; ++y)
                    for (int x = 0; x < w; ++x) if (buf[static_cast<size_t>(y) * stride + x * 4 + 3] != 255) { skip = true; break; }
            }
            const double eff = std::max(w / (wpt / 72.0), h / (hpt / 72.0));   // pixels per inch as placed on the page
            const double ratio = eff > dpi ? dpi / eff : 1.0;
            if (!skip && (ratio < 0.92 || grey)) {
                const int w2 = std::max(8, static_cast<int>(std::lround(w * ratio))), h2 = std::max(8, static_cast<int>(std::lround(h * ratio)));
                MemAccess ma;
                if (encode(buf, w, h, stride, ch, w2, h2, quality, grey, ma.data)) {
                    // Only worth it when the new data is clearly smaller than the raw picture would be at this size.
                    ma.m_FileLen = static_cast<unsigned long>(ma.data.size());
                    ma.m_GetBlock = readBlock;
                    ma.m_Param = &ma;
                    if (api.FPDFImageObj_LoadJpegFileInline(&pg, 1, o, &ma)) { ++replaced; pageChanged = true; }
                }
            }
            api.FPDFBitmap_Destroy(bmp);
        }
        if (pageChanged) contentgen::generate(d.handle(), pg);
    }
    return replaced;
}

size_t sizeNow(const Document& d) {
    std::vector<unsigned char> v;
    return d.serialize(v) ? v.size() : 0;
}

} // namespace

Result run(Document& d, const Options& o) {
    Result res;

    std::vector<unsigned char> original;
    if (!d.serialize(original)) return res;
    res.before = original.size();
    d.beginChange();   // the undo step for everything below

    struct Step { int dpi, q; };
    std::vector<Step> steps;
    if (o.targetMB > 0) steps = {{220, 82}, {150, 78}, {120, 72}, {96, 68}, {72, 60}, {60, 50}};
    else steps = {{o.dpi, o.jpegQuality}};

    size_t bestSize = 0;
    int bestIdx = -1, bestCount = 0;
    for (size_t k = 0; k < steps.size(); ++k) {
        if (k > 0) {   // go back to the original before trying stronger settings
            std::vector<unsigned char> copy = original;
            if (d.reload(std::move(copy)) != OpenResult::Ok) break;   // see note in main: encrypted files keep their password inside Document
        }
        int n = pass(d, steps[k].dpi, steps[k].q, o.greyscale);
        size_t sz = n ? sizeNow(d) : res.before;
        if (n && (bestIdx < 0 || sz < bestSize)) { bestSize = sz; bestIdx = static_cast<int>(k); bestCount = n; }
        if (o.targetMB > 0 && sz <= o.targetMB * 1048576.0) { bestSize = sz; bestIdx = static_cast<int>(k); bestCount = n; res.reachedTarget = true; break; }
        if (o.targetMB > 0) res.reachedTarget = false;
        if (o.targetMB <= 0) break;
    }
    // Make the document hold the best result found (the last attempt is not always the smallest).
    if (bestIdx < 0 || bestSize >= res.before) {
        std::vector<unsigned char> copy = original;
        d.reload(std::move(copy));
        d.cancelChange();
        res.changed = false;
        res.after = res.before;
        return res;
    }
    if (static_cast<size_t>(bestIdx) != steps.size() - 1 || o.targetMB > 0) {
        std::vector<unsigned char> copy = original;
        d.reload(std::move(copy));
        bestCount = pass(d, steps[bestIdx].dpi, steps[bestIdx].q, o.greyscale);
        bestSize = sizeNow(d);
    }
    d.refreshPages();
    res.changed = true;
    res.pictures = bestCount;
    res.after = bestSize;
    return res;
}

} // namespace compress
