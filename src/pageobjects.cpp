#include "pageobjects.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace pageobj {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Vec { double x, y; };

// Where a text object (created at the origin) must go so that its centre lands at (cx, cy) measured in
// points from the top-left of the page AS DISPLAYED, turned `angle` radians anticlockwise on screen.
// PDFium converts display positions to page space, so /Rotate and crop boxes are handled for us.
// `localCentre`, when given, is the point of the object (in its own coordinates) that lands on (cx, cy); new path objects
// do not know their bounds yet, so shapes drawn around a text pass the text's centre here.
bool place(FPDF_PAGE page, FPDF_PAGEOBJECT obj, double visW, double visH, double cx, double cy, double angle,
           const double* localCentre = nullptr) {
    const auto& api = pdfium::api();
    double tcx = 0, tcy = 0;
    if (localCentre) { tcx = localCentre[0]; tcy = localCentre[1]; }
    else {
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(obj, &l, &b, &r, &t)) return false;
        tcx = (l + r) / 2.0; tcy = (b + t) / 2.0;
    }
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

bool placeScaled(FPDF_PAGE page, FPDF_PAGEOBJECT obj, double visW, double visH, double ox, double oy, double sx) {
    const auto& api = pdfium::api();
    const double S = 100.0;
    const int dw = static_cast<int>(visW * S), dh = static_cast<int>(visH * S);
    auto toPage = [&](double vx, double vy) {
        double px = 0, py = 0;
        api.FPDF_DeviceToPage(page, 0, 0, dw, dh, 0, static_cast<int>(vx * S), static_cast<int>(vy * S), &px, &py);
        return Vec{px, py};
    };
    const Vec p0 = toPage(ox, oy), px = toPage(ox + 1, oy), py = toPage(ox, oy - 1);
    api.FPDFPageObj_Transform(obj, (px.x - p0.x) * sx, (px.y - p0.y) * sx, py.x - p0.x, py.y - p0.y, p0.x, p0.y);
    return true;
}

FPDF_PAGEOBJECT newText(Document& d, const char* font, double size, const std::wstring& text, COLORREF color, int alphaPercent) {
    return makeText(d, font, size, text, color, alphaPercent);
}

bool isLatin(const std::wstring& text) {
    for (wchar_t c : text) if (c > 0xFF || (c < 0x20 && c != L'\t' && c != L'\r' && c != L'\n')) return false;
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


// ---------- text boxes and editing ----------

namespace {

const char* standardFont(Family f, bool bold, bool italic) {
    switch (f) {
        case Family::Serif: return bold ? (italic ? "Times-BoldItalic" : "Times-Bold") : (italic ? "Times-Italic" : "Times-Roman");
        case Family::Mono: return bold ? (italic ? "Courier-BoldOblique" : "Courier-Bold") : (italic ? "Courier-Oblique" : "Courier");
        default: return bold ? (italic ? "Helvetica-BoldOblique" : "Helvetica-Bold") : (italic ? "Helvetica-Oblique" : "Helvetica");
    }
}

std::vector<std::wstring> splitLines(const std::wstring& t) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == L'\r' || t[i] == L'\n') {
            if (t[i] == L'\r' && i + 1 < t.size() && t[i + 1] == L'\n') ++i;
            out.push_back(cur); cur.clear();
        } else cur += t[i];
    }
    out.push_back(cur);
    return out;
}

} // namespace

bool addTextBox(Document& d, int page, double atX, double atY, const std::wstring& text, const TextStyle& st) {
    if (text.empty()) return false;
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const auto& api = pdfium::api();
    const double vw = d.pageSize(page).w, vh = d.pageSize(page).h;
    // Where the click is on the page as displayed (points from the top-left).
    const double S = 100.0;
    int dx = 0, dy = 0;
    api.FPDF_PageToDevice(pg, 0, 0, static_cast<int>(vw * S), static_cast<int>(vh * S), 0, atX, atY, &dx, &dy);
    const double vx = dx / S, vy = dy / S;
    const char* font = standardFont(st.family, st.bold, st.italic);
    d.beginChange();
    bool any = false;
    double lineTop = vy;
    for (const std::wstring& line : splitLines(text)) {
        if (!line.empty()) {
            FPDF_PAGEOBJECT o = makeText(d, font, st.sizePt, line, st.color, 100);
            if (o) {
                float l, b, r, t;
                api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t);
                const double tw = r - l, th = t - b;
                if (place(pg, o, vw, vh, vx + tw / 2, lineTop + th / 2, 0)) { api.FPDFPage_InsertObject(pg, o); any = true; }
                else api.FPDFPageObj_Destroy(o);
            }
        }
        lineTop += st.sizePt * 1.2;
    }
    if (!any) { d.cancelChange(); return false; }
    api.FPDFPage_GenerateContent(pg);
    return true;
}

TextHit textAt(Document& d, int page, double x, double y) {
    TextHit best;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg || !tp) return best;
    const auto& api = pdfium::api();
    double bestArea = 1e18;
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        if (x < l - 1 || x > r + 1 || y < b - 1 || y > t + 1) continue;
        const double area = std::fabs((r - l) * (t - b));
        if (area >= bestArea) continue;
        unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
        std::wstring text;
        if (bytes >= 2) {
            std::vector<unsigned short> buf(bytes / 2 + 1);
            api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
            text.assign(buf.begin(), buf.begin() + (bytes / 2 - 1));
        }
        if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos) continue;   // spaces only
        bestArea = area;
        best.index = i;
        best.text = text;
        float size = 0;
        api.FPDFTextObj_GetFontSize(o, &size);
        best.sizePt = size;
        unsigned R = 0, G = 0, B = 0, A = 255;
        api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
        best.color = RGB(R, G, B);
        FPDF_FONT font = api.FPDFTextObj_GetFont(o);
        if (font) {
            best.embeddedFont = api.FPDFFont_GetIsEmbedded(font) != 0;
            best.flags = static_cast<unsigned long>(api.FPDFFont_GetFlags(font));
            size_t len = api.FPDFFont_GetBaseFontName(font, nullptr, 0);
            if (len > 1) {
                std::string nm(len, '\0');
                api.FPDFFont_GetBaseFontName(font, nm.data(), len);
                nm.resize(strlen(nm.c_str()));
                best.fontName = nm;
            }
        }
    }
    return best;
}

bool replaceText(Document& d, int page, int idx, const std::wstring& text) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || idx < 0 || idx >= pdfium::api().FPDFPage_CountObjects(pg)) return false;
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, idx);
    if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) return false;
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return false;

    d.beginChange();
    o = api.FPDFPage_GetObject(pg, idx);
    if (text.empty()) {
        if (!api.FPDFPage_RemoveObject(pg, o)) { d.cancelChange(); return false; }
        api.FPDFPageObj_Destroy(o);
        api.FPDFPage_GenerateContent(pg);
        d.resetText(page);
        return true;
    }
    // Which characters did the original text use? A subset font can only draw those again.
    unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
    std::wstring old;
    if (bytes >= 2) {
        std::vector<unsigned short> buf(bytes / 2 + 1);
        api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
        old.assign(buf.begin(), buf.begin() + (bytes / 2 - 1));
    }
    FPDF_FONT font = api.FPDFTextObj_GetFont(o);
    const bool embedded = font && api.FPDFFont_GetIsEmbedded(font);
    bool needsFallback = false;
    if (embedded) for (wchar_t c : text) if (c != L' ' && old.find(c) == std::wstring::npos) { needsFallback = true; break; }

    if (!needsFallback) {
        std::vector<unsigned short> w(text.begin(), text.end());
        w.push_back(0);
        if (api.FPDFText_SetText(o, w.data())) { api.FPDFPage_GenerateContent(pg); d.resetText(page); return true; }
        // fall through to the replacement route
    }
    // Replacement in a similar standard font at the same place, size and colour.
    if (!isLatin(text)) { d.cancelChange(); return false; }
    std::string fname;
    unsigned long flags = 0;
    if (font) {
        flags = static_cast<unsigned long>(api.FPDFFont_GetFlags(font));
        size_t len = api.FPDFFont_GetBaseFontName(font, nullptr, 0);
        if (len > 1) { fname.assign(len, '\0'); api.FPDFFont_GetBaseFontName(font, fname.data(), len); fname.resize(strlen(fname.c_str())); }
    }
    auto has = [&](const char* s) { std::string lo = fname; for (auto& ch : lo) ch = static_cast<char>(tolower(ch)); return lo.find(s) != std::string::npos; };
    Family fam = Family::Sans;
    if (has("courier") || has("mono") || has("consolas") || (flags & 1)) fam = Family::Mono;
    else if (has("times") || has("serif") || has("georgia") || has("garamond") || has("cambria") || (flags & 2)) fam = (has("sans") ? Family::Sans : Family::Serif);
    const bool bold = has("bold") || has("black") || (flags & 262144);
    const bool italic = has("italic") || has("oblique") || (flags & 64);
    float size = 12;
    api.FPDFTextObj_GetFontSize(o, &size);
    unsigned R = 0, G = 0, B = 0, A = 255;
    api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
    FS_MATRIX m{};
    api.FPDFPageObj_GetMatrix(o, &m);
    FPDF_PAGEOBJECT n = makeText(d, standardFont(fam, bold, italic), size, text, RGB(R, G, B), static_cast<int>(A * 100 / 255));
    if (!n) { d.cancelChange(); return false; }
    api.FPDFPageObj_SetMatrix(n, &m);
    if (!api.FPDFPage_RemoveObject(pg, o)) { api.FPDFPageObj_Destroy(n); d.cancelChange(); return false; }
    api.FPDFPageObj_Destroy(o);
    api.FPDFPage_InsertObjectAtIndex(pg, n, static_cast<size_t>(idx));
    api.FPDFPage_GenerateContent(pg);
    d.resetText(page);
    return true;
}

bool addStamp(Document& d, int page, double cx, double cy, const std::wstring& text, COLORREF color) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || text.empty()) return false;
    const auto& api = pdfium::api();
    const double vw = d.pageSize(page).w, vh = d.pageSize(page).h, S = 100.0;
    int dx = 0, dy = 0;
    api.FPDF_PageToDevice(pg, 0, 0, static_cast<int>(vw * S), static_cast<int>(vh * S), 0, cx, cy, &dx, &dy);
    const double vx = dx / S, vy = dy / S;
    d.beginChange();
    FPDF_PAGEOBJECT t = makeText(d, "Helvetica-Bold", 24, text, color, 100);
    if (!t) { d.cancelChange(); return false; }
    float l, b, r, tp;
    api.FPDFPageObj_GetBounds(t, &l, &b, &r, &tp);
    // the border is built around the text's own bounds, so both objects share one centre
    FPDF_PAGEOBJECT box = api.FPDFPageObj_CreateNewRect(l - 9, b - 6, (r - l) + 18, (tp - b) + 12);
    bool ok = box != nullptr;
    if (ok) {
        api.FPDFPath_SetDrawMode(box, 0, 1);   // outline only
        api.FPDFPageObj_SetStrokeColor(box, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        api.FPDFPageObj_SetStrokeWidth(box, 2.5f);
        const double centre[2] = {(l + r) / 2.0, (b + tp) / 2.0};   // the text's centre, shared by the border
        ok = place(pg, box, vw, vh, vx, vy, 0, centre) && place(pg, t, vw, vh, vx, vy, 0, centre);
    }
    if (!ok) {
        api.FPDFPageObj_Destroy(t);
        if (box) api.FPDFPageObj_Destroy(box);
        d.cancelChange();
        return false;
    }
    api.FPDFPage_InsertObject(pg, box);
    api.FPDFPage_InsertObject(pg, t);
    api.FPDFPage_GenerateContent(pg);
    return true;
}

} // namespace pageobj
