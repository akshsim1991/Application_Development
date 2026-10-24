#include "pageobjects.hpp"
#include <algorithm>
#include <cmath>

namespace pageobj {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Vec { double x, y; };

// Where a text object (created at the origin) must go so that its centre lands at (cx, cy) measured in
// points from the top-left of the page AS DISPLAYED, turned `angle` radians anticlockwise on screen.
// PDFium converts display positions to page space, so /Rotate and crop boxes are handled for us.
bool place(FPDF_PAGE page, FPDF_PAGEOBJECT obj, double visW, double visH, double cx, double cy, double angle) {
    const auto& api = pdfium::api();
    float l, b, r, t;
    if (!api.FPDFPageObj_GetBounds(obj, &l, &b, &r, &t)) return false;
    const double tcx = (l + r) / 2.0, tcy = (b + t) / 2.0;
    const double S = 100.0;   // device units per point, for sub-pixel accuracy
    const int dw = static_cast<int>(visW * S), dh = static_cast<int>(visH * S);
    auto toPage = [&](double vx, double vy) {
        double px = 0, py = 0;
        api.FPDF_DeviceToPage(page, 0, 0, dw, dh, 0, static_cast<int>(vx * S), static_cast<int>(vy * S), &px, &py);
        return Vec{px, py};
    };
    const Vec p0 = toPage(cx, cy);
    const Vec px = toPage(cx + 1, cy);   // one point to the right on screen
    const Vec py = toPage(cx, cy - 1);   // one point up on screen
    const Vec ex{px.x - p0.x, px.y - p0.y}, ey{py.x - p0.x, py.y - p0.y};
    const double c = std::cos(angle), s = std::sin(angle);
    const double a = ex.x * c + ey.x * s, bb = ex.y * c + ey.y * s;
    const double cc = -ex.x * s + ey.x * c, d = -ex.y * s + ey.y * c;
    const double e = p0.x + ex.x * (-tcx * c + tcy * s) + ey.x * (-tcx * s - tcy * c);
    const double f = p0.y + ex.y * (-tcx * c + tcy * s) + ey.y * (-tcx * s - tcy * c);
    return api.FPDFPageObj_Transform(obj, a, bb, cc, d, e, f), true;
}

FPDF_PAGEOBJECT makeText(Document& d, const char* font, double size, const std::wstring& text, COLORREF col, int alphaPct) {
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT o = api.FPDFPageObj_NewTextObj(d.handle(), font, static_cast<float>(size));
    if (!o) return nullptr;
    std::vector<unsigned short> w(text.begin(), text.end());
    w.push_back(0);
    if (!api.FPDFText_SetText(o, w.data())) { api.FPDFPageObj_Destroy(o); return nullptr; }
    api.FPDFPageObj_SetFillColor(o, GetRValue(col), GetGValue(col), GetBValue(col), static_cast<unsigned>(alphaPct * 255 / 100));
    return o;
}

double textWidth(Document& d, const char* font, const std::wstring& text) {
    FPDF_PAGEOBJECT o = makeText(d, font, 100, text, 0, 100);
    if (!o) return 0;
    float l, b, r, t;
    double w = pdfium::api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t) ? r - l : 0;
    pdfium::api().FPDFPageObj_Destroy(o);
    return w;
}

} // namespace

bool isLatin(const std::wstring& text) {
    for (wchar_t c : text) if (c > 0xFF || (c < 0x20 && c != L'\t')) return false;
    return true;
}

bool addWatermark(Document& d, const std::vector<int>& pages, const std::wstring& text, const WatermarkStyle& st) {
    if (text.empty() || pages.empty()) return false;
    const auto& api = pdfium::api();
    const char* font = "Helvetica-Bold";
    const double w100 = textWidth(d, font, text);
    if (w100 <= 1) return false;
    d.beginChange();
    bool any = false;
    for (int i : pages) {
        FPDF_PAGE pg = d.page(i);
        if (!pg) continue;
        const double vw = d.pageSize(i).w, vh = d.pageSize(i).h;
        const double angle = st.diagonal ? std::atan2(vh, vw) : 0;   // corner to corner
        double target = st.diagonal ? 0.78 * std::hypot(vw, vh) : 0.8 * vw;
        static const double scale[4] = {1.0, 0.35, 0.6, 0.85};
        target *= scale[std::clamp(st.sizeMode, 0, 3)];
        const double size = std::clamp(100.0 * target / w100, 8.0, 600.0);
        FPDF_PAGEOBJECT o = makeText(d, font, size, text, st.color, st.opacityPercent);
        if (!o) continue;
        if (!place(pg, o, vw, vh, vw / 2, vh / 2, angle)) { api.FPDFPageObj_Destroy(o); continue; }
        if (st.behind) api.FPDFPage_InsertObjectAtIndex(pg, o, 0); else api.FPDFPage_InsertObject(pg, o);
        api.FPDFPage_GenerateContent(pg);
        any = true;
    }
    if (!any) { d.cancelChange(); return false; }
    return true;
}

bool addPageNumbers(Document& d, const std::vector<int>& pages, const NumberStyle& st) {
    if (pages.empty()) return false;
    const auto& api = pdfium::api();
    d.beginChange();
    bool any = false;
    int n = st.startAt;
    for (int i : pages) {
        FPDF_PAGE pg = d.page(i);
        if (!pg) continue;
        std::wstring t;
        switch (st.format) {
            case 1: t = L"Page " + std::to_wstring(n); break;
            case 2: t = L"Page " + std::to_wstring(n) + L" of " + std::to_wstring(st.startAt + static_cast<int>(pages.size()) - 1); break;
            case 3: t = std::to_wstring(n) + L" / " + std::to_wstring(st.startAt + static_cast<int>(pages.size()) - 1); break;
            default: t = std::to_wstring(n); break;
        }
        ++n;
        FPDF_PAGEOBJECT o = makeText(d, "Helvetica", st.sizePt, t, st.color, 100);
        if (!o) continue;
        float l, b, r, tp;
        api.FPDFPageObj_GetBounds(o, &l, &b, &r, &tp);
        const double tw = r - l, th = tp - b;
        const double vw = d.pageSize(i).w, vh = d.pageSize(i).h, margin = 28;
        double cx = vw / 2, cy;
        switch (st.position % 3) {
            case 1: cx = margin + tw / 2; break;
            case 2: cx = vw - margin - tw / 2; break;
        }
        cy = st.position < 3 ? vh - margin - th / 2 : margin + th / 2;   // y measured from the top
        if (!place(pg, o, vw, vh, cx, cy, 0)) { api.FPDFPageObj_Destroy(o); continue; }
        api.FPDFPage_InsertObject(pg, o);
        api.FPDFPage_GenerateContent(pg);
        any = true;
    }
    if (!any) { d.cancelChange(); return false; }
    return true;
}

} // namespace pageobj
