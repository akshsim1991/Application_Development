#include "redact.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace redact {

int apply(Document& d, int dpi) {
    if (d.redactions.empty()) return 0;
    std::set<int> pages;
    for (const auto& m : d.redactions) pages.insert(m.page);
    const auto& api = pdfium::api();
    d.beginChange();
    int done = 0;
    for (int p : pages) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        const double vw = d.pageSize(p).w, vh = d.pageSize(p).h;
        const int w = std::max(1, static_cast<int>(std::lround(vw * dpi / 72.0)));
        const int h = std::max(1, static_cast<int>(std::lround(vh * dpi / 72.0)));
        if (1LL * w * h > 100000000LL) continue;
        std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
        FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, px.data(), w * 4);
        if (!bmp) continue;
        api.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
        api.FPDF_RenderPageBitmap(bmp, pg, 0, 0, w, h, 0, FPDF_ANNOT);
        // Black out the marked areas (converted from page space to this bitmap's pixels).
        for (const auto& m : d.redactions) {
            if (m.page != p) continue;
            int x1, y1, x2, y2;
            api.FPDF_PageToDevice(pg, 0, 0, w, h, 0, m.left, m.top, &x1, &y1);
            api.FPDF_PageToDevice(pg, 0, 0, w, h, 0, m.right, m.bottom, &x2, &y2);
            const int pad = 1;
            int l = std::max(0, std::min(x1, x2) - pad), t = std::max(0, std::min(y1, y2) - pad);
            int r = std::min(w, std::max(x1, x2) + pad), b = std::min(h, std::max(y1, y2) + pad);
            if (r > l && b > t) api.FPDFBitmap_FillRect(bmp, l, t, r - l, b - t, 0xFF000000);
        }
        // Remove all original content and annotations from the page.
        while (api.FPDFPage_GetAnnotCount(pg) > 0) {
            if (!api.FPDFPage_RemoveAnnot(pg, api.FPDFPage_GetAnnotCount(pg) - 1)) break;
        }
        while (api.FPDFPage_CountObjects(pg) > 0) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, api.FPDFPage_CountObjects(pg) - 1);
            if (!o || !api.FPDFPage_RemoveObject(pg, o)) break;
            api.FPDFPageObj_Destroy(o);
        }
        // The page becomes an upright, unrotated picture of exactly the size it was shown at.
        api.FPDFPage_SetRotation(pg, 0);
        api.FPDFPage_SetMediaBox(pg, 0, 0, static_cast<float>(vw), static_cast<float>(vh));
        api.FPDFPage_SetCropBox(pg, 0, 0, static_cast<float>(vw), static_cast<float>(vh));
        FPDF_PAGEOBJECT img = api.FPDFPageObj_NewImageObj(d.handle());
        bool ok = img && api.FPDFImageObj_SetBitmap(nullptr, 0, img, bmp) &&
                  api.FPDFImageObj_SetMatrix(img, vw, 0, 0, vh, 0, 0);
        api.FPDFBitmap_Destroy(bmp);
        if (!ok) { if (img) api.FPDFPageObj_Destroy(img); continue; }
        api.FPDFPage_InsertObject(pg, img);
        api.FPDFPage_GenerateContent(pg);
        ++done;
    }
    if (!done) { d.cancelChange(); return 0; }
    d.redactions.clear();
    d.refreshPages();   // cached text of the redacted pages is stale
    return done;
}

int markText(Document& d, const std::wstring& query, bool matchCase) {
    int added = 0;
    const auto& api = pdfium::api();
    for (const TextHit& h : d.find(query, matchCase)) {
        FPDF_TEXTPAGE tp = d.text(h.page);
        if (!tp) continue;
        int n = api.FPDFText_CountRects(tp, h.start, h.count);
        for (int i = 0; i < n; ++i) {
            double l, t, r, b;
            if (api.FPDFText_GetRect(tp, i, &l, &t, &r, &b)) { d.redactions.push_back({h.page, l, b, r, t}); ++added; }
        }
    }
    return added;
}

} // namespace redact
