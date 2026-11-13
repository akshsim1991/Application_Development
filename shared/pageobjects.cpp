#include "pageobjects.hpp"
#include "contentgen.hpp"
#include "fontlist.hpp"
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include "platform.hpp"
#endif
#include <algorithm>
#include <functional>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <cwctype>

namespace pageobj {

// The closest standard family, weight and slant of a font, from its PDF name (the subset prefix "ABCDEF+" left out), its
// flags and the weight and slant it gives. Knows the names TeX, Office and the usual type foundries give their fonts:
// "NimbusRomNo9L-Medi" (LaTeX's Times, bold), "CMBX12", "LMRoman12-Bold", "TimesNewRomanPS-BoldMT", "Arial-ItalicMT" ...
static void guessFontStyle(FPDF_FONT font, const std::string& name, Family& fam, bool& bold, bool& italic) {
    const auto& api = pdfium::api();
    const unsigned long flags = font ? static_cast<unsigned long>(api.FPDFFont_GetFlags(font)) : 0;
    std::string lo = name;
    const size_t plus = lo.find('+');
    if (plus == 6) lo = lo.substr(7);
    for (auto& ch : lo) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    auto has = [&](const char* s) { return lo.find(s) != std::string::npos; };
    auto any = [&](std::initializer_list<const char*> l) { for (const char* s : l) if (has(s)) return true; return false; };
    auto starts = [&](std::initializer_list<const char*> l) { for (const char* s : l) if (lo.rfind(s, 0) == 0) return true; return false; };
    const bool mono = any({"courier", "mono", "consolas", "inconsolata", "menlo", "lucidaconsole", "nimbusmon"}) || starts({"cmtt", "sftt"}) || (flags & 1);
    const bool sans = any({"sans", "helvet", "arial", "nimbussan", "calibri", "verdana", "tahoma", "segoe", "gill", "futura", "frutiger",
                           "myriad", "roboto", "lato", "candara", "trebuchet", "franklin", "univers", "avenir", "corbel", "lucida grande"}) || starts({"cmss", "sfss"});
    const bool serif = any({"times", "serif", "georgia", "garamond", "cambria", "minion", "palatino", "roman", "nimbusrom", "termes", "pagella",
                            "bookman", "antiqua", "century", "schoolbook", "baskerville", "caslon", "didot", "bodoni", "charter", "utopia",
                            "libertine", "constantia", "sabon", "mincho", "sylfaen", "book"}) || starts({"cmr", "cmbx", "cmti", "cmsl", "cmb", "sfrm", "sfbx", "sfti"}) || (flags & 2);
    fam = mono ? Family::Mono : sans ? Family::Sans : serif ? Family::Serif : Family::Sans;
    const int weight = font ? api.FPDFFont_GetWeight(font) : -1;
    bold = any({"bold", "black", "heavy", "semibold", "demi", "-medi", "-bd", "extrabold"}) || starts({"cmbx", "cmb10", "sfbx"}) || (flags & 262144) || weight >= 600;
    int angle = 0;
    if (font) api.FPDFFont_GetItalicAngle(font, &angle);
    italic = any({"italic", "oblique", "ital", "slanted"}) || starts({"cmti", "cmsl", "sfti", "sfsl"}) || (flags & 64) || angle != 0;
}

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

// Whether FPDFTextObj_GetFontSize gives the size as drawn (font size times the text matrix's scale) rather than the size
// in the content stream. Versions of PDFium differ; asked once with a test object.
bool sizeIncludesMatrix(Document& d) {
    static int known = -1;
    if (known < 0) {
        const auto& api = pdfium::api();
        known = 0;
        if (FPDF_PAGEOBJECT o = api.FPDFPageObj_NewTextObj(d.handle(), "Helvetica", 10)) {
            const FS_MATRIX m{2, 0, 0, 2, 0, 0};
            api.FPDFPageObj_SetMatrix(o, &m);
            float fs = 0;
            api.FPDFTextObj_GetFontSize(o, &fs);
            known = std::fabs(fs - 20) < 0.01 ? 1 : 0;
            api.FPDFPageObj_Destroy(o);
        }
    }
    return known == 1;
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
    d.beginChange(L"Add a watermark");
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
        contentgen::generate(d.handle(), pg);
        any = true;
    }
    if (!any) { d.cancelChange(); return false; }
    return true;
}

bool addPageNumbers(Document& d, const std::vector<int>& pages, const NumberStyle& st) {
    if (pages.empty()) return false;
    const auto& api = pdfium::api();
    d.beginChange(L"Add page numbers");
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
        contentgen::generate(d.handle(), pg);
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
    d.beginChange(L"Add text");
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
    contentgen::generate(d.handle(), pg);
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
        best.left = l; best.bottom = b; best.right = r; best.top = t;
        FS_MATRIX m{1, 0, 0, 1, l, b};
        best.shownSizePt = api.FPDFPageObj_GetMatrix(o, &m) && !sizeIncludesMatrix(d) ? size * std::hypot(m.c, m.d) : size;
        best.originX = m.e;
        best.baseline = m.f;
        if (best.shownSizePt <= 0) best.shownSizePt = t - b;
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

bool replaceText(Document& d, int page, int idx, const std::wstring& text, bool track) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || idx < 0 || idx >= pdfium::api().FPDFPage_CountObjects(pg)) return false;
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, idx);
    if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) return false;
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return false;

    if (track) d.beginChange(L"Edit text");
    o = api.FPDFPage_GetObject(pg, idx);
    if (text.empty()) {
        if (!api.FPDFPage_RemoveObject(pg, o)) { if (track) d.cancelChange(); return false; }
        api.FPDFPageObj_Destroy(o);
        contentgen::generate(d.handle(), pg);
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
        if (api.FPDFText_SetText(o, w.data())) { contentgen::generate(d.handle(), pg); d.resetText(page); return true; }
        // fall through to the replacement route
    }
    // Replacement in a similar standard font at the same place, size and colour.
    if (!isLatin(text)) { if (track) d.cancelChange(); return false; }
    std::string fname;
    if (font) {
        size_t len = api.FPDFFont_GetBaseFontName(font, nullptr, 0);
        if (len > 1) { fname.assign(len, '\0'); api.FPDFFont_GetBaseFontName(font, fname.data(), len); fname.resize(strlen(fname.c_str())); }
    }
    Family fam = Family::Sans;
    bool bold = false, italic = false;
    guessFontStyle(font, fname, fam, bold, italic);
    float size = 12;
    api.FPDFTextObj_GetFontSize(o, &size);
    unsigned R = 0, G = 0, B = 0, A = 255;
    api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
    FS_MATRIX m{};
    api.FPDFPageObj_GetMatrix(o, &m);
    FPDF_PAGEOBJECT n = makeText(d, standardFont(fam, bold, italic), size, text, RGB(R, G, B), static_cast<int>(A * 100 / 255));
    if (!n) { if (track) d.cancelChange(); return false; }
    api.FPDFPageObj_SetMatrix(n, &m);
    if (!api.FPDFPage_RemoveObject(pg, o)) { api.FPDFPageObj_Destroy(n); if (track) d.cancelChange(); return false; }
    api.FPDFPageObj_Destroy(o);
    api.FPDFPage_InsertObjectAtIndex(pg, n, static_cast<size_t>(idx));
    contentgen::generate(d.handle(), pg);
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
    d.beginChange(L"Add a stamp");
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
    contentgen::generate(d.handle(), pg);
    return true;
}


int replaceAll(Document& d, const std::wstring& find, const std::wstring& with, bool matchCase, std::vector<int>& pages) {
    pages.clear();
    if (find.empty()) return 0;
    const auto& api = pdfium::api();
    auto lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; };
    const std::wstring needle = matchCase ? find : lower(find);
    d.beginChange(L"Find and replace");
    int total = 0;
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        FPDF_TEXTPAGE tp = d.text(p);
        if (!pg || !tp) continue;
        struct Job { int idx; std::wstring text; };
        std::vector<Job> jobs;
        const int n = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < n; ++i) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
            unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
            if (bytes < 2) continue;
            std::vector<unsigned short> buf(bytes / 2 + 1);
            api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
            std::wstring text(buf.begin(), buf.begin() + (bytes / 2 - 1));
            const std::wstring hay = matchCase ? text : lower(text);
            if (hay.find(needle) == std::wstring::npos) continue;
            std::wstring out;
            size_t pos = 0, hit;
            int count = 0;
            while ((hit = hay.find(needle, pos)) != std::wstring::npos) { out += text.substr(pos, hit - pos) + with; pos = hit + needle.size(); ++count; }
            out += text.substr(pos);
            total += count;
            jobs.push_back({i, out});
        }
        bool changed = false;
        for (auto it = jobs.rbegin(); it != jobs.rend(); ++it)   // last object first: deleting one must not shift the others
            if (replaceText(d, p, it->idx, it->text, false)) changed = true; else --total;
        if (changed) pages.push_back(p);
        d.resetText(p);
    }
    if (total <= 0) { d.cancelChange(); return 0; }
    return total;
}

// ---- tables ----
namespace {
std::string toUtf8(const std::wstring& w) { return platform::utf8(w); }
std::wstring fromUtf8(const std::string& s) { return platform::wide(s); }
std::string serialise(const TableSpec& t) {
    char head[128];
    snprintf(head, sizeof(head), "%d;%d;%.2f;%.2f;%.2f;%d;", t.rows, t.cols, t.colWidth, t.rowHeight, t.fontSize, t.header ? 1 : 0);
    std::string s = head;
    for (size_t i = 0; i < t.cells.size(); ++i) { if (i) s += '\x1f'; s += toUtf8(t.cells[i]); }
    return s;
}
bool parse(const std::string& s, TableSpec& t) {
    int header = 1;
    int used = 0;
    if (sscanf(s.c_str(), "%d;%d;%lf;%lf;%lf;%d;%n", &t.rows, &t.cols, &t.colWidth, &t.rowHeight, &t.fontSize, &header, &used) < 6 || used <= 0) return false;
    t.header = header != 0;
    t.cells.clear();
    std::string rest = s.substr(used), cur;
    for (char c : rest) { if (c == '\x1f') { t.cells.push_back(fromUtf8(cur)); cur.clear(); } else cur += c; }
    t.cells.push_back(fromUtf8(cur));
    t.cells.resize(static_cast<size_t>(t.rows) * t.cols);
    return t.rows > 0 && t.cols > 0;
}
std::wstring markString(FPDF_PAGEOBJECTMARK m, const char* key) {
    const auto& api = pdfium::api();
    unsigned long n = 0;
    if (!api.FPDFPageObjMark_GetParamStringValue(m, key, nullptr, 0, &n) || n < 2) return {};
    std::vector<unsigned short> buf(n / 2 + 1);
    api.FPDFPageObjMark_GetParamStringValue(m, key, buf.data(), n, &n);
    return std::wstring(buf.begin(), buf.begin() + (n / 2 - 1));
}
// The id of the table an object belongs to (empty when it is not part of one).
std::string tableIdOf(FPDF_PAGEOBJECT o, std::string* spec = nullptr) {
    const auto& api = pdfium::api();
    const int marks = api.FPDFPageObj_CountMarks(o);
    for (int i = 0; i < marks; ++i) {
        FPDF_PAGEOBJECTMARK m = api.FPDFPageObj_GetMark(o, i);
        unsigned long n = 0;
        if (!m || !api.FPDFPageObjMark_GetName(m, nullptr, 0, &n) || n < 2) continue;
        std::vector<unsigned short> buf(n / 2 + 1);
        api.FPDFPageObjMark_GetName(m, buf.data(), n, &n);
        if (std::wstring(buf.begin(), buf.begin() + (n / 2 - 1)) != L"FeatherTable") continue;
        if (spec) *spec = toUtf8(markString(m, "spec"));
        return toUtf8(markString(m, "id"));
    }
    return {};
}
} // namespace

bool addTable(Document& d, int page, double atX, double atY, const TableSpec& spec, bool track) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || spec.rows < 1 || spec.cols < 1) return false;
    const auto& api = pdfium::api();
    const double vw = d.pageSize(page).w, vh = d.pageSize(page).h;
    const double S = 100.0;
    const int dw = static_cast<int>(vw * S), dh = static_cast<int>(vh * S);
    int dx = 0, dy = 0;
    api.FPDF_PageToDevice(pg, 0, 0, dw, dh, 0, atX, atY, &dx, &dy);
    const double vx = dx / S, vy = dy / S;   // top-left corner, visual points from the page's top-left
    auto toPageXY = [&](double x, double y, double& px, double& py) { api.FPDF_DeviceToPage(pg, 0, 0, dw, dh, 0, static_cast<int>(x * S), static_cast<int>(y * S), &px, &py); };
    const double W = spec.cols * spec.colWidth, H = spec.rows * spec.rowHeight;
    char idbuf[32];
    snprintf(idbuf, sizeof(idbuf), "t%u", static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()));
    const std::string id = idbuf;
    const std::string specText = serialise(spec);
    bool firstObj = true;
    auto tag = [&](FPDF_PAGEOBJECT o) {
        FPDF_PAGEOBJECTMARK m = api.FPDFPageObj_AddMark(o, "FeatherTable");
        if (!m) return;
        api.FPDFPageObjMark_SetStringParam(d.handle(), o, m, "id", id.c_str());
        if (firstObj) { api.FPDFPageObjMark_SetStringParam(d.handle(), o, m, "spec", specText.c_str()); firstObj = false; }
    };
    auto rect = [&](double x0, double y0, double x1, double y1, unsigned r, unsigned g, unsigned b) {   // visual coordinates
        double ax, ay, bx, by;
        toPageXY(x0, y0, ax, ay); toPageXY(x1, y1, bx, by);
        FPDF_PAGEOBJECT o = api.FPDFPageObj_CreateNewRect(static_cast<float>(std::min(ax, bx)), static_cast<float>(std::min(ay, by)),
                                                           static_cast<float>(std::fabs(bx - ax)), static_cast<float>(std::fabs(by - ay)));
        if (!o) return;
        api.FPDFPageObj_SetFillColor(o, r, g, b, 255);
        api.FPDFPath_SetDrawMode(o, FPDF_FILLMODE_ALTERNATE, 0);
        tag(o);
        api.FPDFPage_InsertObject(pg, o);
    };
    if (track) d.beginChange(L"Insert a table");
    if (spec.header) rect(vx, vy, vx + W, vy + spec.rowHeight, 0xE6, 0xE6, 0xE6);
    const double line = 0.75;
    for (int r = 0; r <= spec.rows; ++r) rect(vx, vy + r * spec.rowHeight - line / 2, vx + W, vy + r * spec.rowHeight + line / 2, 0x30, 0x30, 0x30);
    for (int c = 0; c <= spec.cols; ++c) rect(vx + c * spec.colWidth - line / 2, vy, vx + c * spec.colWidth + line / 2, vy + H, 0x30, 0x30, 0x30);
    for (int r = 0; r < spec.rows; ++r)
        for (int c = 0; c < spec.cols; ++c) {
            const std::wstring& text = spec.cells[static_cast<size_t>(r) * spec.cols + c];
            if (text.empty()) continue;
            FPDF_PAGEOBJECT o = makeText(d, spec.header && r == 0 ? "Helvetica-Bold" : "Helvetica", spec.fontSize, text, RGB(0x20, 0x20, 0x20), 100);
            if (!o) continue;
            float l, b, rr, t;
            api.FPDFPageObj_GetBounds(o, &l, &b, &rr, &t);
            const double tw = rr - l, th = t - b;
            if (place(pg, o, vw, vh, vx + c * spec.colWidth + 4 + tw / 2, vy + r * spec.rowHeight + spec.rowHeight / 2, 0)) { tag(o); api.FPDFPage_InsertObject(pg, o); }
            else api.FPDFPageObj_Destroy(o);
            (void)th;
        }
    contentgen::generate(d.handle(), pg);
    d.resetText(page);
    return true;
}

TableHit tableAt(Document& d, int page, double x, double y) {
    TableHit hit;
    FPDF_PAGE pg = d.page(page);
    if (!pg) return hit;
    const auto& api = pdfium::api();
    struct Box { float l = 1e9f, b = 1e9f, r = -1e9f, t = -1e9f; std::string spec; };
    std::map<std::string, Box> boxes;
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o) continue;
        std::string sp;
        const std::string id = tableIdOf(o, &sp);
        if (id.empty()) continue;
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        Box& bx = boxes[id];
        bx.l = std::min(bx.l, l); bx.b = std::min(bx.b, b); bx.r = std::max(bx.r, r); bx.t = std::max(bx.t, t);
        if (!sp.empty()) bx.spec = sp;
    }
    double bestArea = 1e18;
    for (auto& kv : boxes) {
        const Box& bx = kv.second;
        if (x < bx.l - 1 || x > bx.r + 1 || y < bx.b - 1 || y > bx.t + 1) continue;
        const double area = static_cast<double>(bx.r - bx.l) * (bx.t - bx.b);
        TableSpec spec;
        if (area >= bestArea || !parse(bx.spec, spec)) continue;
        bestArea = area;
        hit.found = true; hit.page = page; hit.id = kv.first; hit.spec = spec; hit.left = bx.l; hit.top = bx.t;
    }
    return hit;
}

bool removeTable(Document& d, int page, const std::string& id, bool track) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const auto& api = pdfium::api();
    if (track) d.beginChange(L"Delete a table");
    bool any = false;
    for (int i = api.FPDFPage_CountObjects(pg) - 1; i >= 0; --i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || tableIdOf(o) != id) continue;
        if (api.FPDFPage_RemoveObject(pg, o)) { api.FPDFPageObj_Destroy(o); any = true; }
    }
    if (!any) { if (track) d.cancelChange(); return false; }
    contentgen::generate(d.handle(), pg);
    d.resetText(page);
    return true;
}


// ---- paragraphs ----
namespace {
struct Line {
    double l, b, r, t, size; std::wstring text; std::vector<int> idx; FPDF_PAGEOBJECT first;
    bool startsItem = false;   // the first line of a list item (a number or bullet before it, or at its start)
    bool marker = false;       // only a list number or bullet ("3.", "a)", "iv.", "•")
    unsigned rgb = 0;          // the colour of its text: a line in another colour is another paragraph
};

// A list number or bullet on its own: "3.", "12)", "(a)", "b.", "iv.", "•", "–", "▪".
bool isListMarker(const std::wstring& raw) {
    std::wstring t = raw;
    while (!t.empty() && (t.back() == L' ' || t.back() == L'\t')) t.pop_back();
    while (!t.empty() && (t.front() == L' ' || t.front() == L'\t')) t.erase(0, 1);
    if (t.empty() || t.size() > 6) return false;
    static const std::wstring bullets = L"\u2022\u2023\u25E6\u25AA\u25AB\u25CF\u25CB\u25A0\u25A1\u2013\u2014\u2043\u2219\u00B7-*\uF0B7\uF0A7\uF076\uF0D8";
    if (t.size() == 1 && bullets.find(t[0]) != std::wstring::npos) return true;
    size_t i = 0;
    if (t[i] == L'(') ++i;
    const size_t start = i;
    while (i < t.size() && (iswdigit(t[i]) || iswalpha(t[i]))) ++i;
    const size_t len = i - start;
    if (len == 0 || i + 1 != t.size() || (t[i] != L'.' && t[i] != L')')) return false;
    const std::wstring body = t.substr(start, len);
    bool digits = true, roman = true;
    for (wchar_t c : body) { digits = digits && iswdigit(c); roman = roman && std::wstring(L"ivxlcIVXLC").find(c) != std::wstring::npos; }
    return digits ? len <= 3 : (len == 1 || roman);   // 1. 12) a. (b) iv.
}

// The same text with the hyphen look-alikes made plain hyphens (a hyphen that must not break the line, a soft hyphen in
// the middle of a word): the editor shows them, and the paragraph's own font, or a standard one, can draw them.
std::wstring plainHyphens(std::wstring t) {
    for (wchar_t& c : t) if (c == 0x2010 || c == 0x2011 || c == 0x00AD || c == 0x2012) c = L'-';
    return t;
}

std::wstring textOfObject(FPDF_TEXTPAGE tp, FPDF_PAGEOBJECT o) {
    const auto& api = pdfium::api();
    unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
    if (bytes < 2) return {};
    std::vector<unsigned short> buf(bytes / 2 + 1);
    api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
    return std::wstring(buf.begin(), buf.begin() + (bytes / 2 - 1));
}

std::string baseFontName(FPDF_FONT font) {
    std::string name;
    size_t len = font ? pdfium::api().FPDFFont_GetBaseFontName(font, nullptr, 0) : 0;
    if (len > 1) { name.assign(len, '\0'); pdfium::api().FPDFFont_GetBaseFontName(font, name.data(), len); name.resize(strlen(name.c_str())); }
    return name;
}

// Makes text objects in one font, size and direction: either a font of the document (the paragraph's own) or a standard font.
struct Setter {
    Document* d;
    FPDF_FONT font = nullptr;       // the document's font, or null for `standard`
    const char* standard = nullptr;
    float fontSize = 12;
    double a = 1, b = 0, c = 0, dd = 1;   // the text matrix without its position
    COLORREF color = 0;
    unsigned alpha = 255;
    int renderMode = -1;            // -1: as new text objects are made

    FPDF_PAGEOBJECT make(const std::wstring& t, double e, double f) const {
        const auto& api = pdfium::api();
        FPDF_PAGEOBJECT o = font ? api.FPDFPageObj_CreateTextObj(d->handle(), font, fontSize) : api.FPDFPageObj_NewTextObj(d->handle(), standard, fontSize);
        if (!o) return nullptr;
        std::vector<unsigned short> w(t.begin(), t.end());
        w.push_back(0);
        if (!api.FPDFText_SetText(o, w.data())) { api.FPDFPageObj_Destroy(o); return nullptr; }
        api.FPDFPageObj_SetFillColor(o, GetRValue(color), GetGValue(color), GetBValue(color), alpha);
        if (renderMode >= 0) api.FPDFTextObj_SetTextRenderMode(o, static_cast<FPDF_TEXT_RENDERMODE>(renderMode));
        const FS_MATRIX m{static_cast<float>(a), static_cast<float>(b), static_cast<float>(c), static_cast<float>(dd), static_cast<float>(e), static_cast<float>(f)};
        api.FPDFPageObj_SetMatrix(o, &m);
        return o;
    }
    // From the start of the line (the origin) to the right edge of the last letter, along the line. -1 when the text cannot be set.
    double width(const std::wstring& t) const {
        if (t.empty()) return 0;
        FPDF_PAGEOBJECT o = make(t, 0, 0);
        if (!o) return -1;
        float l, bo, r, tp;
        double w = -1;
        if (pdfium::api().FPDFPageObj_GetBounds(o, &l, &bo, &r, &tp)) {
            const double n = std::hypot(a, b), ux = n > 0 ? a / n : 1, uy = n > 0 ? b / n : 0;
            w = std::max({l * ux + bo * uy, r * ux + bo * uy, l * ux + tp * uy, r * ux + tp * uy});
        }
        pdfium::api().FPDFPageObj_Destroy(o);
        return w;
    }
};

// The closest standard family, weight, slant and the colour of a text object drawn at `size` points, and its font's PDF name.
void styleOfObject(FPDF_PAGEOBJECT o, double size, TextStyle& st, std::string& fontName) {
    const auto& api = pdfium::api();
    FPDF_FONT font = api.FPDFTextObj_GetFont(o);
    fontName = baseFontName(font);
    guessFontStyle(font, fontName, st.family, st.bold, st.italic);
    st.sizePt = std::round(size * 2) / 2;
    unsigned R = 0, G = 0, B = 0, A = 255;
    api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
    st.color = RGB(R, G, B);
}

// The size a text object is drawn at, in points (its font size scaled by its matrix, unless PDFium includes that already).
double drawnSize(Document& d, FPDF_PAGEOBJECT o) {
    const auto& api = pdfium::api();
    float fs = 0;
    api.FPDFTextObj_GetFontSize(o, &fs);
    FS_MATRIX m{1, 0, 0, 1, 0, 0};
    api.FPDFPageObj_GetMatrix(o, &m);
    const double scale = std::sqrt(static_cast<double>(m.b) * m.b + static_cast<double>(m.d) * m.d);
    return sizeIncludesMatrix(d) ? fs : fs * (scale > 0.01 ? scale : 1.0);
}

// The paragraph's own font, set up like its first text object (same size and slant), or nullptr font when it is not usable.
Setter ownFont(Document& d, FPDF_PAGEOBJECT first) {
    const auto& api = pdfium::api();
    Setter s{&d};
    s.font = api.FPDFTextObj_GetFont(first);
    if (!s.font) return s;
    float fs = 0;
    api.FPDFTextObj_GetFontSize(first, &fs);
    FS_MATRIX m{1, 0, 0, 1, 0, 0};
    api.FPDFPageObj_GetMatrix(first, &m);
    if (sizeIncludesMatrix(d) && std::hypot(m.c, m.d) > 1e-6) fs = static_cast<float>(fs / std::hypot(m.c, m.d));
    s.fontSize = fs > 0 ? fs : 1;
    s.a = m.a; s.b = m.b; s.c = m.c; s.dd = m.d;
    unsigned R = 0, G = 0, B = 0, A = 255;
    api.FPDFPageObj_GetFillColor(first, &R, &G, &B, &A);
    s.color = RGB(R, G, B);
    s.alpha = A;
    s.renderMode = static_cast<int>(api.FPDFTextObj_GetTextRenderMode(first));
    if (s.renderMode < 0) s.renderMode = -1;
    return s;
}
} // namespace

// The paragraph around line `hitLine`: its first and last line (neighbouring lines of the same size, spacing and column).
static void paragraphSpan(const std::vector<Line>& lines, int hitLine, bool lineOnly, int& first, int& last) {
    auto joins = [&](const Line& above, const Line& below) {
        const double size = above.size;
        if (std::fabs(above.size - below.size) > 0.1 * size) return false;
        if (above.rgb != below.rgb) return false;   // a red line under a black one is not the same paragraph
        const double step = above.b - below.b;
        if (step < 0.8 * size || step > 2.1 * size) return false;
        const double indent = below.l - above.l;
        if (std::fabs(indent) > 0.6 * size && !(indent > 0 && indent < 3.5 * size) && !(indent < 0 && indent > -3.5 * size)) return false;
        return below.r >= above.l && below.l <= above.r;
    };
    first = hitLine; last = hitLine;
    while (!lineOnly && first > 0 && !lines[first].startsItem && joins(lines[first - 1], lines[first]) && std::fabs(lines[first - 1].l - lines[hitLine].l) <= 3.6 * lines[hitLine].size) --first;
    while (!lineOnly && last + 1 < static_cast<int>(lines.size()) && !lines[last + 1].startsItem && joins(lines[last], lines[last + 1]) && std::fabs(lines[last + 1].l - lines[hitLine].l) <= 3.6 * lines[hitLine].size) ++last;
}

// The text lines of a page, top to bottom (list numbers and bullets left out, the lines they start marked).
static void collectLines(Document& d, FPDF_PAGE pg, FPDF_TEXTPAGE tp, std::vector<Line>& lines) {
    const auto& api = pdfium::api();
    // 1. every text object, with its size taken from the font size and the text matrix
    struct Obj { int idx; double l, b, r, t, size; std::wstring text; FPDF_PAGEOBJECT o; unsigned rgb; };
    std::vector<Obj> objs;
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        std::wstring text = textOfObject(tp, o);
        if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos && r - l > 0.5f) {
            // nothing read from the object itself (some PDFs give a hyphen this way): the text page's letters in its box
            const int nb = api.FPDFText_GetBoundedText(tp, l + 0.1, t - 0.1, r - 0.1, b + 0.1, nullptr, 0);
            if (nb > 0) {
                std::vector<unsigned short> buf(static_cast<size_t>(nb) + 1);
                api.FPDFText_GetBoundedText(tp, l + 0.1, t - 0.1, r - 0.1, b + 0.1, buf.data(), nb);
                text.assign(buf.begin(), buf.begin() + nb);
            }
        }
        text = plainHyphens(text);
        if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos) continue;
        float fs = 0;
        api.FPDFTextObj_GetFontSize(o, &fs);
        FS_MATRIX m{1, 0, 0, 1, 0, 0};
        api.FPDFPageObj_GetMatrix(o, &m);
        const double scale = std::sqrt(static_cast<double>(m.b) * m.b + static_cast<double>(m.d) * m.d);
        unsigned cr = 0, cg = 0, cb = 0, ca = 255;
        api.FPDFPageObj_GetFillColor(o, &cr, &cg, &cb, &ca);
        objs.push_back({i, l, b, r, t, sizeIncludesMatrix(d) ? fs : fs * (scale > 0.01 ? scale : 1.0), text, o, (cr << 16) | (cg << 8) | cb});
    }
    // 2. lines: objects on one baseline, left to right
    lines.clear();
    std::vector<char> used(objs.size(), 0);
    std::vector<size_t> order(objs.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return objs[a].t != objs[b].t ? objs[a].t > objs[b].t : objs[a].l != objs[b].l ? objs[a].l < objs[b].l : a < b;
    });
    for (size_t oi : order) {
        if (used[oi]) continue;
        Line ln{objs[oi].l, objs[oi].b, objs[oi].r, objs[oi].t, objs[oi].size, objs[oi].text, {objs[oi].idx}, objs[oi].o};
        ln.rgb = objs[oi].rgb;
        used[oi] = 1;
        std::vector<size_t> same;
        for (size_t oj : order) if (!used[oj] && std::fabs(objs[oj].b - objs[oi].b) < 0.4 * objs[oi].size && std::fabs(objs[oj].size - objs[oi].size) < 0.15 * objs[oi].size) same.push_back(oj);
        std::sort(same.begin(), same.end(), [&](size_t a, size_t b) { return objs[a].l != objs[b].l ? objs[a].l < objs[b].l : a < b; });
        for (size_t oj : same) {
            const double gap = objs[oj].l - ln.r;
            if (gap > 2.5 * objs[oi].size || gap < -0.3 * objs[oi].size) continue;   // another column or overlapping
            ln.text += (gap > 0.15 * objs[oi].size && !ln.text.empty() && ln.text.back() != L' ' ? L" " : L"") + objs[oj].text;
            ln.r = std::max(ln.r, objs[oj].r); ln.t = std::max(ln.t, objs[oj].t); ln.b = std::min(ln.b, objs[oj].b);
            ln.idx.push_back(objs[oj].idx);
            used[oj] = 1;
        }
        // the line's first object is the leftmost one
        if (objs[oi].l > ln.l + 0.01) for (size_t oj = 0; oj < objs.size(); ++oj) if (objs[oj].o != objs[oi].o && std::find(ln.idx.begin(), ln.idx.end(), objs[oj].idx) != ln.idx.end() && objs[oj].l <= ln.l + 0.01) { ln.first = objs[oj].o; break; }
        lines.push_back(ln);
    }
    std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.t > b.t; });
    // 2b. numbered and bulleted lists: a number on its own marks the line beside it as the start of an item, and is not
    // part of the text (it stays as it is). Without this, the number was a line of its own between the item's lines, so
    // an item's paragraph stopped after its first line and took in the end of the item before.
    for (Line& m : lines) {
        if (!isListMarker(m.text)) continue;
        for (Line& ln : lines) {
            if (&ln == &m || ln.marker || std::fabs(ln.b - m.b) > 0.4 * std::max(ln.size, m.size)) continue;
            if (ln.l > m.r - 0.5 && ln.l - m.r < 6 * std::max(ln.size, m.size)) { ln.startsItem = true; m.marker = true; break; }
        }
    }
    for (Line& ln : lines) {   // a number at the start of the line itself ("3. Explain ...")
        const size_t sp = ln.text.find(L' ');
        if (!ln.marker && sp != std::wstring::npos && sp > 0 && isListMarker(ln.text.substr(0, sp))) ln.startsItem = true;
    }
    lines.erase(std::remove_if(lines.begin(), lines.end(), [](const Line& ln) { return ln.marker; }), lines.end());
}

std::vector<EditBlock> editableBlocks(Document& d, int page) {
    std::vector<EditBlock> out;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg) return out;
    const auto& api = pdfium::api();
    // pictures (a group holding pictures counts as one)
    std::function<bool(FPDF_PAGEOBJECT, int)> hasImage = [&](FPDF_PAGEOBJECT o, int depth) {
        const int t = api.FPDFPageObj_GetType(o);
        if (t == FPDF_PAGEOBJ_IMAGE) return true;
        if (t != FPDF_PAGEOBJ_FORM || depth > 6) return false;
        const int n = api.FPDFFormObj_CountObjects(o);
        for (int i = 0; i < n; ++i) if (hasImage(api.FPDFFormObj_GetObject(o, static_cast<unsigned long>(i)), depth + 1)) return true;
        return false;
    };
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        float l, b, r, t;
        if (o && hasImage(o, 0) && api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) && r - l > 2 && t - b > 2) out.push_back({EditBlock::Picture, l, b, r, t});
    }
    if (!tp) return out;
    // paragraphs, grouped as paragraphAt groups them, so an outline is what a click there edits
    std::vector<Line> lines;
    collectLines(d, pg, tp, lines);
    std::vector<char> done(lines.size(), 0);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (done[i]) continue;
        int first = static_cast<int>(i), last = first;
        paragraphSpan(lines, first, false, first, last);
        EditBlock b{EditBlock::Text, 1e9, 1e9, -1e9, -1e9};
        for (int k = first; k <= last; ++k) {
            done[static_cast<size_t>(k)] = 1;
            b.left = std::min(b.left, lines[k].l); b.bottom = std::min(b.bottom, lines[k].b);
            b.right = std::max(b.right, lines[k].r); b.top = std::max(b.top, lines[k].t);
        }
        out.push_back(b);
    }
    return out;
}

ParagraphHit paragraphAt(Document& d, int page, double x, double y, bool lineOnly) {
    ParagraphHit hit;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg || !tp) return hit;
    const auto& api = pdfium::api();
    std::vector<Line> lines;
    collectLines(d, pg, tp, lines);
    // 3. the line under the click
    int hitLine = -1;
    double best = 1e18;
    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& ln = lines[i];
        if (x < ln.l - 2 || x > ln.r + 2 || y < ln.b - 2 || y > ln.t + 2) continue;
        const double area = (ln.r - ln.l) * (ln.t - ln.b);
        if (area < best) { best = area; hitLine = static_cast<int>(i); }
    }
    if (hitLine < 0) return hit;
    // 4. neighbouring lines of the same paragraph
    int first = hitLine, last = hitLine;
    paragraphSpan(lines, hitLine, lineOnly, first, last);
    hit.found = true;
    hit.page = page;
    hit.lines = last - first + 1;
    double l = 1e9, r = -1e9;
    std::wstring text;
    for (int i = first; i <= last; ++i) {
        const Line& ln = lines[i];
        for (int ix : ln.idx) hit.objects.push_back(ix);
        l = std::min(l, ln.l); r = std::max(r, ln.r);
        std::wstring t = ln.text;
        while (!t.empty() && t.back() == L' ') t.pop_back();
        if (!text.empty()) {
            if (text.size() > 1 && text.back() == L'-' && iswalpha(text[text.size() - 2]) && !t.empty() && iswlower(t[0])) text.pop_back();   // a word broken at the line end
            else text += L' ';
        }
        text += t;
    }
    hit.text = text;
    hit.left = l; hit.top = lines[first].t; hit.width = r - l;
    hit.lineHeight = last > first ? (lines[first].b - lines[last].b) / (last - first) : lines[first].size * 1.2;
    // 5. exact geometry from the text matrices: the lines' starting points on their baselines, along and across the line
    auto origin = [&](FPDF_PAGEOBJECT o) { FS_MATRIX m{1, 0, 0, 1, 0, 0}; api.FPDFPageObj_GetMatrix(o, &m); return Vec{m.e, m.f}; };
    FS_MATRIX m0{1, 0, 0, 1, 0, 0};
    api.FPDFPageObj_GetMatrix(lines[first].first, &m0);
    double ux = m0.a, uy = m0.b, vx = m0.c, vy = m0.d;
    const double un = std::hypot(ux, uy), vn = std::hypot(vx, vy);
    if (un > 1e-6) { ux /= un; uy /= un; } else { ux = 1; uy = 0; }
    if (vn > 1e-6) { vx /= vn; vy /= vn; } else { vx = -uy; vy = ux; }
    hit.upright = ux > 0.999 && std::fabs(vy - 1) < 0.001;
    const Vec o0 = origin(lines[first].first);
    double s0 = 0;
    for (int i = first; i <= last; ++i) { const Vec o = origin(lines[i].first); s0 = std::min(s0, (o.x - o0.x) * ux + (o.y - o0.y) * uy); }
    hit.originX = o0.x + s0 * ux;   // the paragraph's left edge, on the first line's baseline
    hit.baseline = o0.y + s0 * uy;
    hit.firstIndent = -s0;
    hit.restIndent = hit.firstIndent;
    for (int i = first + 1; i <= last; ++i) {
        const Vec o = origin(lines[i].first);
        const double s = (o.x - hit.originX) * ux + (o.y - hit.baseline) * uy;
        hit.restIndent = i == first + 1 ? s : std::min(hit.restIndent, s);
    }
    if (last > first) {
        const Vec ol = origin(lines[last].first);
        hit.lineStep = ((o0.x - ol.x) * vx + (o0.y - ol.y) * vy) / (last - first);
    }
    if (hit.lineStep <= 0.1) hit.lineStep = lines[first].size * 1.2;
    double widest = 0;   // the right edge of the widest line, from the left edge
    std::vector<double> lineEnds;
    for (int i = first; i <= last; ++i) {
        double e = 0;
        for (int ix : lines[i].idx) {
            float bl, bb, br, bt;
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, ix);
            if (!o || !api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt)) continue;
            for (double cxp : {static_cast<double>(bl), static_cast<double>(br)})
                for (double cyp : {static_cast<double>(bb), static_cast<double>(bt)}) e = std::max(e, (cxp - hit.originX) * ux + (cyp - hit.baseline) * uy);
        }
        lineEnds.push_back(e);
        widest = std::max(widest, e);
    }
    hit.wrapWidth = widest;
    // 6. alignment, read from the line edges: justified when every line but the last ends at the same place (lines that are
    // only broken between words end up to a word apart)
    if (last - first >= 2) {
        double lo = 1e9, hi = -1e9;
        for (size_t i = 0; i + 1 < lineEnds.size(); ++i) { lo = std::min(lo, lineEnds[i]); hi = std::max(hi, lineEnds[i]); }
        const bool leftEdge = std::fabs(hit.restIndent) < 0.5 && hit.firstIndent > -0.5 && hit.firstIndent < 3.5 * lines[first].size;
        if (hi - lo <= 0.6 + 0.002 * widest && leftEdge) hit.align = Align::Justify;
    } else if (last - first == 1) {
        // two lines: justified when the first one reaches the right edge of its column (the farthest line end of the
        // text that starts near it on this page) and the last one is clearly shorter
        double column = -1e9;
        for (const Line& ln : lines)
            if (std::fabs(ln.l - lines[last].l) < 3.6 * lines[first].size && ln.r > lines[last].l) column = std::max(column, ln.r);
        const double size = lines[first].size;
        const bool leftEdge = std::fabs(hit.restIndent) < 0.5 && hit.firstIndent > -0.5 && hit.firstIndent < 3.5 * size;
        if (leftEdge && column - lines[first].r < 0.6 + 0.002 * widest && lines[first].r - lines[last].r > 1.5 * size) {
            // and the column edge is shared by other lines (not just this one being the longest)
            int reach = 0;
            for (const Line& ln : lines) if (std::fabs(ln.r - column) < 0.6 + 0.002 * widest && std::fabs(ln.l - lines[last].l) < 3.6 * size) ++reach;
            if (reach >= 2) hit.align = Align::Justify;
        }
    }
    // A line on its own (a heading, a short item) grows to the right edge of its column before it wraps, as in other PDF
    // editors: the farthest any line that starts at the same left edge reaches on this page (so it stays in its column).
    if (last == first && hit.align == Align::Left && hit.upright) {
        const double size = lines[first].size;
        double column = lines[first].r;
        for (const Line& ln : lines)
            if (std::fabs(ln.l - lines[first].l) < 3.6 * size) column = std::max(column, ln.r);
        hit.wrapWidth = std::max(widest, column - hit.originX);
    }
    // style from the first line's first object
    styleOfObject(lines[first].first, lines[first].size, hit.style, hit.fontName);
    hit.sizeExact = lines[first].size;
    return hit;
}

namespace {
// The installed font a PDF font name stands for, as a font of the document that can draw any letter: one an earlier edit
// embedded on this page, or else the installed font file, embedded now (`loaded` is then true and the caller closes it).
// nullptr when it is not installed in that style, its licence does not allow embedding, or it is a collection or a CFF font.
FPDF_FONT installedFont(Document& d, FPDF_PAGE pg, const std::string& pdfName, bool bold, bool italic, bool& loaded) {
    loaded = false;
    const std::wstring face = fontlist::installedFace(pdfName);
    if (face.empty()) return nullptr;
#if defined(__APPLE__)
    // The Mac: the installed font of that family and style (CoreText), read from its file.
    const auto& api = pdfium::api();
    const std::string faceUtf8 = platform::utf8(face);
    CFStringRef family = CFStringCreateWithCString(nullptr, faceUtf8.c_str(), kCFStringEncodingUTF8);
    if (!family) return nullptr;
    const uint32_t want = (bold ? kCTFontBoldTrait : 0) | (italic ? kCTFontItalicTrait : 0);
    CFNumberRef sym = CFNumberCreate(nullptr, kCFNumberSInt32Type, &want);
    const void* tk[] = {kCTFontSymbolicTrait};
    const void* tv[] = {sym};
    CFDictionaryRef traits = CFDictionaryCreate(nullptr, tk, tv, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void* ak[] = {kCTFontFamilyNameAttribute, kCTFontTraitsAttribute};
    const void* av[] = {family, traits};
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr, ak, av, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
    CTFontRef font = desc ? CTFontCreateWithFontDescriptor(desc, 200, nullptr) : nullptr;
    FPDF_FONT result = nullptr;
    std::vector<uint8_t> data;
    if (font) {
        const uint32_t has = CTFontGetSymbolicTraits(font);
        CFStringRef gotFamily = CTFontCopyFamilyName(font);
        const bool sameFamily = gotFamily && CFStringCompare(gotFamily, family, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
        if (gotFamily) CFRelease(gotFamily);
        bool mayEmbed = true;   // OS/2 fsType bit 1: the licence does not allow embedding
        if (CFDataRef os2 = CTFontCopyTable(font, kCTFontTableOS2, kCTFontTableOptionNoOptions)) {
            if (CFDataGetLength(os2) >= 10) mayEmbed = !(CFDataGetBytePtr(os2)[9] & 0x02);
            CFRelease(os2);
        }
        if (sameFamily && mayEmbed && ((has & kCTFontBoldTrait) != 0) == bold && ((has & kCTFontItalicTrait) != 0) == italic) {
            if (CFURLRef url = static_cast<CFURLRef>(CTFontCopyAttribute(font, kCTFontURLAttribute))) {
                char path[2048];
                if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(path), sizeof path)) {
                    std::vector<unsigned char> bytes;
                    if (platform::readFile(platform::wide(path), bytes) && bytes.size() > 12 && bytes.size() < 64u * 1024 * 1024) {
                        if (std::memcmp(bytes.data(), "ttcf", 4) == 0) {
                            // a collection (.ttc: Helvetica, Times, Avenir...): this one font, rebuilt from its tables
                            std::map<uint32_t, std::vector<unsigned char>> tables;
                            if (CFArrayRef tags = CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions)) {
                                for (CFIndex t = 0, nt = CFArrayGetCount(tags); t < nt; ++t) {
                                    const auto tag = static_cast<CTFontTableTag>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(tags, t)));
                                    if (CFDataRef td = CTFontCopyTable(font, tag, kCTFontTableOptionNoOptions)) {
                                        tables[tag] = std::vector<unsigned char>(CFDataGetBytePtr(td), CFDataGetBytePtr(td) + CFDataGetLength(td));
                                        CFRelease(td);
                                    }
                                }
                                CFRelease(tags);
                            }
                            const std::vector<unsigned char> one = fontlist::buildSfnt(tables);   // empty for CFF outlines
                            data.assign(one.begin(), one.end());
                        } else if (std::memcmp(bytes.data(), "OTTO", 4) != 0) data.assign(bytes.begin(), bytes.end());   // CFF: see below
                    }
                }
                CFRelease(url);
            }
        }
    }
    if (!data.empty()) {
        // An earlier edit may have embedded it on this page already (the same name, the whole file as its program).
        const std::wstring wantName = fontlist::squeeze(face);
        const int n = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < n && !result; ++i) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
            FPDF_FONT pf = api.FPDFTextObj_GetFont(o);
            if (!pf || !api.FPDFFont_GetIsEmbedded(pf)) continue;
            const std::string nm = baseFontName(pf);
            const std::wstring have = fontlist::squeeze(std::wstring(nm.begin(), nm.end()));
            size_t len = 0;
            if (have.rfind(wantName, 0) == 0 && api.FPDFFont_GetFontData(pf, nullptr, 0, &len) && len == data.size()) result = pf;
        }
        if (!result) {
            result = api.FPDFText_LoadFont(d.handle(), data.data(), static_cast<uint32_t>(data.size()), FPDF_FONT_TRUETYPE, 1);
            loaded = result != nullptr;
        }
    }
    if (font) CFRelease(font);
    if (desc) CFRelease(desc);
    CFRelease(attrs);
    CFRelease(traits);
    CFRelease(sym);
    CFRelease(family);
    return result;
#elif !defined(_WIN32)
    (void)d; (void)pg; (void)bold; (void)italic;
    return nullptr;   // the Linux engine check: standard PDF fonts
#else
    const auto& api = pdfium::api();
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT f = CreateFontW(-200, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face.c_str());
    HGDIOBJ old = SelectObject(dc, f);
    FPDF_FONT result = nullptr;
    const UINT size = GetOutlineTextMetricsW(dc, 0, nullptr);
    if (size > sizeof(OUTLINETEXTMETRICW)) {
        std::vector<char> buf(size);
        auto* otm = reinterpret_cast<OUTLINETEXTMETRICW*>(buf.data());
        GetOutlineTextMetricsW(dc, size, otm);
        auto str = [&](PSTR off) { return std::wstring(reinterpret_cast<const wchar_t*>(buf.data() + reinterpret_cast<size_t>(off))); };
        std::wstring style = str(otm->otmpStyleName);
        for (auto& c : style) c = static_cast<wchar_t>(towlower(c));
        const bool isBold = style.find(L"bold") != std::wstring::npos || style.find(L"black") != std::wstring::npos || style.find(L"heavy") != std::wstring::npos;
        const bool isItalic = style.find(L"italic") != std::wstring::npos || style.find(L"oblique") != std::wstring::npos;
        const bool mayEmbed = !(otm->otmfsType & 0x0002);   // bit 1: the licence does not allow embedding
        const DWORD ttcf = 0x66637474, cff = 0x20464643;     // the table tags "ttcf" and "CFF "
        if (isBold == bold && isItalic == italic && mayEmbed && GetFontData(dc, ttcf, 0, nullptr, 0) == GDI_ERROR && GetFontData(dc, cff, 0, nullptr, 0) == GDI_ERROR) {
            // An earlier edit may have embedded it on this page already: a font with the same name whose font program is the
            // whole installed file (PDFium may report a subset font's name without its "ABCDEF+" prefix, so the size tells).
            const std::wstring want = fontlist::squeeze(str(otm->otmpFaceName));
            const DWORD fileSize = GetFontData(dc, 0, 0, nullptr, 0);
            const int n = api.FPDFPage_CountObjects(pg);
            for (int i = 0; i < n && !result && fileSize != GDI_ERROR; ++i) {
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
                FPDF_FONT pf = api.FPDFTextObj_GetFont(o);
                if (!pf || !api.FPDFFont_GetIsEmbedded(pf)) continue;
                const std::string nm = baseFontName(pf);
                const std::wstring have = fontlist::squeeze(std::wstring(nm.begin(), nm.end()));
                size_t len = 0;
                if ((have == want || have == want + L"identityh") && api.FPDFFont_GetFontData(pf, nullptr, 0, &len) && len == fileSize) result = pf;
            }
            const DWORD bytes = result ? 0 : GetFontData(dc, 0, 0, nullptr, 0);
            if (!result && bytes != GDI_ERROR && bytes > 0 && bytes < 64u * 1024 * 1024) {
                std::vector<uint8_t> data(bytes);
                if (GetFontData(dc, 0, 0, data.data(), bytes) == bytes) {
                    result = api.FPDFText_LoadFont(d.handle(), data.data(), bytes, FPDF_FONT_TRUETYPE, 1);
                    loaded = result != nullptr;
                }
            }
        }
    }
    SelectObject(dc, old);
    DeleteObject(f);
    DeleteDC(dc);
    return result;
#endif
}

// The fonts replaceParagraph sets text in. `main` is the paragraph's own font when the family, weight and slant were not
// changed, else the closest standard font. An embedded font may hold only the letters the document uses, so it is used only
// for words whose letters are drawn with it on this page. Other words are set in the installed font of the same name when
// there is one (`full`, embedded on first use), else in `fallback`, a standard font of the same style and size.
struct Fonts {
    Document* d = nullptr;
    FPDF_PAGE pg = nullptr;
    Setter main{nullptr}, fallback{nullptr}, full{nullptr};
    bool own = false;               // main is the paragraph's own font
    bool limited = false;           // ... and it can draw only the letters in `have`
    bool mayLoad = false;           // `full` may be embedded (only while a change is being made)
    int fullState = 0;              // 0 not looked for yet, 1 found, -1 none
    FPDF_FONT loaded = nullptr;     // `full`'s font when it was loaded here
    std::string pdfName;
    bool bold = false, italic = false;
    std::wstring have;
    wchar_t probe = L'x';           // a letter `main` can draw
    double space = 0;               // the width of a space in `main`

    Fonts() = default;
    Fonts(const Fonts&) = delete;
    Fonts& operator=(const Fonts&) = delete;
    ~Fonts() { release(); }
    void release() { if (loaded) pdfium::api().FPDFFont_Close(loaded); loaded = nullptr; }   // before the document is reloaded

    // The font for a word, or nullptr when no font can draw it.
    const Setter* forWord(const std::wstring& w) {
        if (own) {
            bool ok = true;
            if (limited) { for (wchar_t ch : w) if (have.find(ch) == std::wstring::npos) { ok = false; break; } }
            else ok = isLatin(w);
            if (ok) return &main;
            if (limited && mayLoad && fullState == 0) {
                bool fresh = false;
                FPDF_FONT ff = installedFont(*d, pg, pdfName, bold, italic, fresh);
                fullState = ff ? 1 : -1;
                if (fresh) loaded = ff;
                if (ff) {
                    full = main;
                    full.font = ff;
                    // the same size as drawn: a loaded font has no slant or scale of its own, so take the main font's matrix
                    // with its size moved into the font size
                    const double sx = std::hypot(main.c, main.dd);
                    full.fontSize = static_cast<float>(main.fontSize * sx);
                    if (sx > 0) { full.a /= sx; full.b /= sx; full.c /= sx; full.dd /= sx; }
                }
            }
            if (fullState == 1) return &full;
        }
        return isLatin(w) ? &fallback : nullptr;
    }
};

bool fontsFor(Document& d, const ParagraphHit& hit, const TextStyle& st, Fonts& f) {
    FPDF_PAGE pg = d.page(hit.page);
    if (!pg || hit.objects.empty()) return false;
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT firstObj = api.FPDFPage_GetObject(pg, hit.objects.front());
    if (!firstObj || api.FPDFPageObj_GetType(firstObj) != FPDF_PAGEOBJ_TEXT) return false;
    f.d = &d;
    f.pg = pg;
    f.pdfName = hit.fontName;
    f.bold = st.bold;
    f.italic = st.italic;
    const double k = std::fabs(st.sizePt - hit.style.sizePt) < 0.05 || hit.sizeExact <= 0 ? 1.0 : st.sizePt / hit.sizeExact;   // the change of size
    const Setter own = ownFont(d, firstObj);
    // the standard font: same direction as the paragraph, new size, style and colour
    const double un = std::hypot(own.a, own.b), vn = std::hypot(own.c, own.dd);
    Setter s{&d};
    s.standard = standardFont(st.family, st.bold, st.italic);
    s.fontSize = static_cast<float>(st.sizePt);
    s.a = un > 0 ? own.a / un : 1; s.b = un > 0 ? own.b / un : 0;
    s.c = vn > 0 ? own.c / vn : 0; s.dd = vn > 0 ? own.dd / vn : 1;
    s.color = st.color;
    s.renderMode = own.font ? own.renderMode : -1;
    f.fallback = s;
    f.main = s;
    f.own = own.font && st.family == hit.style.family && st.bold == hit.style.bold && st.italic == hit.style.italic;
    if (f.own) {
        const unsigned long flags = static_cast<unsigned long>(api.FPDFFont_GetFlags(own.font));
        f.limited = api.FPDFFont_GetIsEmbedded(own.font) != 0;
        if (f.limited) {
            FPDF_TEXTPAGE tp = d.text(hit.page);
            const int n = api.FPDFPage_CountObjects(pg);
            for (int i = 0; tp && i < n; ++i) {
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                if (o && api.FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_TEXT && api.FPDFTextObj_GetFont(o) == own.font) f.have += textOfObject(tp, o);
            }
            for (wchar_t ch : f.have) if (ch > L' ') { f.probe = ch; break; }
        } else if (flags & 4) f.own = false;   // a symbol font that is not embedded: its letters are not the ones typed
    }
    if (f.own) {
        f.main = own;
        f.main.a *= k; f.main.b *= k; f.main.c *= k; f.main.dd *= k;
        if (st.color != hit.style.color) { f.main.color = st.color; f.main.alpha = 255; }
    }
    // a space: "x x" less "xx" is one space whatever the letter's side bearings
    const std::wstring p(1, f.probe);
    const double w1 = f.main.width(p + L" " + p), w2 = f.main.width(p + p);
    f.space = w1 > 0 && w2 > 0 && w1 > w2 ? w1 - w2 : 0.25 * st.sizePt;
    if (f.own && f.limited && f.have.find(L' ') == std::wstring::npos) f.space = 0.25 * st.sizePt * k;
    return true;
}

std::vector<std::wstring> wordsOf(const std::wstring& s) {
    std::vector<std::wstring> words;
    size_t p = 0;
    while (p <= s.size()) {
        const size_t sp = s.find(L' ', p);
        std::wstring w = s.substr(p, sp == std::wstring::npos ? std::wstring::npos : sp - p);
        if (!w.empty()) words.push_back(w);
        if (sp == std::wstring::npos) break;
        p = sp + 1;
    }
    return words;
}

// Words of a line grouped into runs that share a font; each run becomes one text object. `width` reaches the right edge of
// the run's last letter; `advance` is where the next letter would start.
struct Run { std::wstring text; const Setter* set; double width, advance; };
std::vector<Run> runsOf(Fonts& f, const std::vector<std::wstring>& words, bool& ok) {
    std::vector<Run> runs;
    ok = true;
    for (const auto& w : words) {
        const Setter* s = f.forWord(w);
        if (!s) { ok = false; return {}; }
        if (!runs.empty() && runs.back().set == s) runs.back().text += L" " + w;
        else runs.push_back({w, s, 0, 0});
    }
    for (auto& r : runs) {
        r.width = std::max(0.0, r.set->width(r.text));
        r.advance = r.width;
        if (runs.size() > 1) {   // a letter the font has, set after the run, starts at the run's advance
            const std::wstring probe = r.set == &f.main && f.limited ? std::wstring(1, f.probe) : L"x";
            const double a = r.set->width(r.text + probe), b = r.set->width(probe);
            if (a > 0 && b > 0 && a - b > 0) r.advance = a - b;
        }
    }
    return runs;
}

double widthOfRuns(const Fonts& f, const std::vector<Run>& runs) {
    double w = 0;
    for (size_t i = 0; i < runs.size(); ++i) w += (i + 1 < runs.size() ? runs[i].advance : runs[i].width) + (i ? f.space : 0);
    return w;
}
} // namespace

bool textObjectStyle(Document& d, int page, int object, TextStyle& style, std::string& fontName, double& size) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    FPDF_PAGEOBJECT o = pdfium::api().FPDFPage_GetObject(pg, object);
    if (!o || pdfium::api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) return false;
    size = drawnSize(d, o);
    styleOfObject(o, size, style, fontName);
    return true;
}

struct TextSetter::Impl { Fonts fonts; bool ok = false; };

TextSetter::TextSetter(Document& d, int page, int like, const TextStyle& style) : impl_(new Impl) {
    Fonts& f = impl_->fonts;
    ParagraphHit hit;
    double size = 0;
    if (like >= 0 && textObjectStyle(d, page, like, hit.style, hit.fontName, size)) {
        // the object's own look, as if it were a one-line paragraph; tables are upright, so a turned object gives only its font
        hit.found = true;
        hit.page = page;
        hit.objects = {like};
        hit.sizeExact = size;
        impl_->ok = fontsFor(d, hit, style, f);
        f.main.renderMode = f.fallback.renderMode = -1;   // new text is always drawn, even when its model was invisible
        if (impl_->ok && (std::fabs(f.main.b) > 1e-6 || std::fabs(f.main.c) > 1e-6 || f.main.a <= 0 || f.main.dd <= 0)) impl_->ok = false;
        if (impl_->ok) { f.mayLoad = true; return; }
    }
    // a standard font of the style
    f.d = &d;
    f.pg = d.page(page);
    Setter s{&d};
    s.standard = standardFont(style.family, style.bold, style.italic);
    s.fontSize = static_cast<float>(style.sizePt);
    s.color = style.color;
    f.main = s;
    f.fallback = s;
    f.own = false;
    const double w1 = s.width(L"x x"), w2 = s.width(L"xx");
    f.space = w1 > 0 && w2 > 0 && w1 > w2 ? w1 - w2 : 0.25 * style.sizePt;
    impl_->ok = f.pg != nullptr;
}

TextSetter::~TextSetter() = default;

bool TextSetter::ok() const { return impl_->ok; }

double TextSetter::width(const std::wstring& line) {
    const auto words = wordsOf(line);
    if (words.empty()) return 0;
    bool fits = false;
    const auto runs = runsOf(impl_->fonts, words, fits);
    return fits ? widthOfRuns(impl_->fonts, runs) : -1;
}

std::vector<FPDF_PAGEOBJECT> TextSetter::make(const std::wstring& line, double x, double y) {
    std::vector<FPDF_PAGEOBJECT> out;
    const auto words = wordsOf(line);
    bool fits = false;
    const auto runs = runsOf(impl_->fonts, words, fits);
    if (!fits) return out;
    double at = x;
    for (size_t i = 0; i < runs.size(); ++i) {
        if (i) at += impl_->fonts.space;
        FPDF_PAGEOBJECT o = runs[i].set->make(runs[i].text, at, y);
        if (!o) { for (FPDF_PAGEOBJECT p : out) pdfium::api().FPDFPageObj_Destroy(p); return {}; }
        out.push_back(o);
        at += runs[i].advance;
    }
    return out;
}

double objectTextWidth(Document& d, int page, int objectIndex, const std::wstring& text) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || objectIndex < 0 || objectIndex >= pdfium::api().FPDFPage_CountObjects(pg)) return 0;
    FPDF_PAGEOBJECT o = pdfium::api().FPDFPage_GetObject(pg, objectIndex);
    if (!o || pdfium::api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) return 0;
    const double w = ownFont(d, o).width(text);
    return w > 0 ? w : 0;
}

double paragraphTextWidth(Document& d, const ParagraphHit& hit, const std::wstring& text, const TextStyle& st) {
    Fonts f;
    if (!fontsFor(d, hit, st, f)) return 0;
    bool ok = true;
    const auto runs = runsOf(f, wordsOf(text), ok);
    return ok ? widthOfRuns(f, runs) : 0;
}

bool replaceParagraph(Document& d, const ParagraphHit& hit, const std::wstring& textIn, const TextStyle& st, Align align, std::wstring* why,
                      bool oneLine) {
    if (!d.page(hit.page) || (!oneLine && hit.wrapWidth < 10)) return false;
    std::wstring text = textIn;
    if (oneLine) for (auto& ch : text) if (ch == L'\r' || ch == L'\n') ch = L' ';
    const auto& api = pdfium::api();
    Fonts f;
    if (!fontsFor(d, hit, st, f)) return false;
    const double k = std::fabs(st.sizePt - hit.style.sizePt) < 0.05 || hit.sizeExact <= 0 ? 1.0 : st.sizePt / hit.sizeExact;
    // The change starts here: setting the text may embed a font. Anything that fails from now on puts the document back.
    d.beginChange(L"Edit a paragraph");
    f.mayLoad = true;
    auto fail = [&](const wchar_t* reason) {
        if (why && reason) *why = reason;
        f.release();
        d.rollbackChange();
        return false;
    };
    const double W = oneLine ? 1e9 : hit.wrapWidth;   // the column keeps its width when the size changes
    const double first = hit.firstIndent, rest = hit.restIndent;
    // break the text into lines that fit the width; a newline always starts a new line
    struct Out { std::vector<Run> runs; std::vector<std::wstring> words; bool last; double indent; };
    std::vector<Out> lines;
    for (const std::wstring& para : splitLines(text)) {
        const std::vector<std::wstring> words = wordsOf(para);
        std::vector<std::wstring> cur;
        std::vector<Run> curRuns;
        auto indent = [&] { return lines.empty() || lines.back().last ? first : rest; };
        for (const auto& w : words) {
            std::vector<std::wstring> cand = cur;
            cand.push_back(w);
            bool ok = true;
            std::vector<Run> runs = runsOf(f, cand, ok);
            if (!ok) return fail(L"Paragraphs can use letters, numbers and common symbols of Western languages. Please remove other characters.");
            if (cur.empty() || widthOfRuns(f, runs) <= W - indent() + 0.25) { cur = std::move(cand); curRuns = std::move(runs); }
            else {
                const double ind = indent();
                lines.push_back({curRuns, cur, false, ind});
                cur = {w};
                curRuns = runsOf(f, cur, ok);
            }
        }
        const double ind = indent();
        lines.push_back({curRuns, cur, true, ind});
    }
    const double step = hit.lineStep * k;
    const Setter& m = f.main;
    double ux = m.a, uy = m.b, vx = m.c, vy = m.dd;
    const double un = std::hypot(ux, uy), vn = std::hypot(vx, vy);
    ux /= un > 0 ? un : 1; uy /= un > 0 ? un : 1; vx /= vn > 0 ? vn : 1; vy /= vn > 0 ? vn : 1;

    // A longer paragraph moves what is below it in the same column down, so nothing is drawn over. Only for upright text.
    FPDF_PAGE pg = d.page(hit.page);
    const double oldLast = hit.baseline - (hit.lines - 1) * hit.lineStep;   // the old last baseline
    const double newLast = hit.baseline - (static_cast<double>(lines.size()) - 1) * step;
    const double descent = 0.3 * st.sizePt;
    float pl, pb, pr, pt;
    if (!api.FPDFPage_GetCropBox(pg, &pl, &pb, &pr, &pt) && !api.FPDFPage_GetMediaBox(pg, &pl, &pb, &pr, &pt)) { pl = 0; pb = 0; pr = 612; pt = 792; }
    const double pageBottom = std::min(pb, pt);
    const bool upright = std::fabs(ux - 1) < 0.01 && std::fabs(vy - 1) < 0.01;
    std::vector<int> below;           // page objects below the paragraph in its column
    std::vector<int> belowAnnots;
    double shift = 0;
    if (upright && newLast < oldLast - 0.01) {
        const double colL = hit.originX - 1, colR = hit.originX + W + 1;
        double nextTop = -1e9, lowest = 1e9;
        const int n = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < n; ++i) {
            if (std::find(hit.objects.begin(), hit.objects.end(), i) != hit.objects.end()) continue;
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            float l, b, r, t;
            if (!o || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
            if (t > oldLast || r < colL || l > colR) continue;
            below.push_back(i);
            nextTop = std::max(nextTop, static_cast<double>(t));
            lowest = std::min(lowest, static_cast<double>(b));
        }
        const int na = api.FPDFPage_GetAnnotCount(pg);
        for (int i = 0; i < na; ++i) {
            FPDF_ANNOTATION a = api.FPDFPage_GetAnnot(pg, i);
            if (!a) continue;
            FS_RECTF r{};
            if (api.FPDFAnnot_GetRect(a, &r) && std::max(r.top, r.bottom) <= oldLast && std::max(r.left, r.right) >= colL && std::min(r.left, r.right) <= colR) {
                belowAnnots.push_back(i);
                lowest = std::min(lowest, static_cast<double>(std::min(r.top, r.bottom)));
            }
            api.FPDFPage_CloseAnnot(a);
        }
        const double extra = oldLast - newLast;
        if (!below.empty() || !belowAnnots.empty()) {
            const double gap = oldLast - nextTop;          // free space under the old last line
            shift = gap >= extra + 2 * step ? 0 : extra;   // plenty of room: nothing needs to move
            if (shift > 0 && lowest - shift < pageBottom)
                return fail(L"The new text needs more lines, and there is no room on this page to move the text below it down. "
                            L"Shorten the text or choose a smaller size.");
        }
        if (newLast - descent < pageBottom) return fail(L"The new text needs more lines than fit on this page. Shorten the text or choose a smaller size.");
    }

    if (shift > 0) {
        for (int i : below) if (FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i)) api.FPDFPageObj_Transform(o, 1, 0, 0, 1, 0, -shift);
        for (int i : belowAnnots) {
            FPDF_ANNOTATION a = api.FPDFPage_GetAnnot(pg, i);
            if (!a) continue;
            FS_RECTF r{};
            if (api.FPDFAnnot_GetRect(a, &r)) {
                r.top = static_cast<float>(r.top - shift); r.bottom = static_cast<float>(r.bottom - shift);
                api.FPDFAnnot_SetRect(a, &r);
            }
            const size_t nq = api.FPDFAnnot_CountAttachmentPoints(a);
            for (size_t q = 0; q < nq; ++q) {
                FS_QUADPOINTSF p{};
                if (!api.FPDFAnnot_GetAttachmentPoints(a, q, &p)) continue;
                p.y1 -= static_cast<float>(shift); p.y2 -= static_cast<float>(shift); p.y3 -= static_cast<float>(shift); p.y4 -= static_cast<float>(shift);
                api.FPDFAnnot_SetAttachmentPoints(a, q, &p);
            }
            api.FPDFPage_CloseAnnot(a);
        }
    }
    // remove the old lines (last object first so the indices stay valid), then set the new ones
    std::vector<int> idx = hit.objects;
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int i : idx) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (o && api.FPDFPage_RemoveObject(pg, o)) api.FPDFPageObj_Destroy(o);
    }
    bool any = false;
    auto put = [&](const Setter& s, const std::wstring& t, double along, int row) {
        const double ex = hit.originX + along * ux - row * step * vx, ey = hit.baseline + along * uy - row * step * vy;
        FPDF_PAGEOBJECT o = s.make(t, ex, ey);
        if (!o) return;
        api.FPDFPage_InsertObject(pg, o);
        any = true;
    };
    for (size_t row = 0; row < lines.size(); ++row) {
        const Out& ln = lines[row];
        if (ln.words.empty()) continue;
        const double room = W - ln.indent;
        if (align == Align::Justify && !ln.last && ln.words.size() > 1) {
            // Spread the words over the full width, one object per word, as word spacing does: every gap gets the same
            // extra, after each word's advance, and the last word's last letter ends at the right edge.
            bool ok = true;
            std::vector<Run> ws;
            std::vector<double> adv;
            for (const auto& w : ln.words) {
                auto r = runsOf(f, {w}, ok);
                if (!ok || r.empty()) continue;
                const Setter& s = *r[0].set;
                const std::wstring probe = &s == &f.main && f.limited ? std::wstring(1, f.probe) : L"x";
                const double a = s.width(w + probe), b = s.width(probe);
                ws.push_back(r[0]);
                adv.push_back(a > 0 && b > 0 && a > b ? a - b : r[0].width);
            }
            double total = 0;
            for (size_t i = 0; i < ws.size(); ++i) total += i + 1 < ws.size() ? adv[i] : ws[i].width;
            const double gap = ws.size() > 1 ? (room - total) / (ws.size() - 1) : 0;
            double along = ln.indent;
            for (size_t i = 0; i < ws.size(); ++i) { put(*ws[i].set, ws[i].text, along, static_cast<int>(row)); along += adv[i] + gap; }
            continue;
        }
        double along = ln.indent;
        const double lw = widthOfRuns(f, ln.runs);
        if (align == Align::Right) along += room - lw;
        else if (align == Align::Centre) along += (room - lw) / 2;
        for (const auto& r : ln.runs) { put(*r.set, r.text, along, static_cast<int>(row)); along += r.advance + f.space; }
    }
    if (!any && !text.empty()) return fail(nullptr);   // nothing could be placed: put the old lines back
    contentgen::generate(d.handle(), pg);
    d.resetText(hit.page);
    return true;
}


// ---- picking up and moving page objects ----
GroupHit groupAt(Document& d, int page, double x, double y) {
    GroupHit g;
    FPDF_PAGE pg = d.page(page);
    if (!pg) return g;
    // the picked objects are remembered by number while they are moved: split the text that writing the page again would
    // split now, so the numbers stay right
    if (contentgen::prepare(d.handle(), pg)) { d.resetText(page); d.pinPage(page); }   // kept loaded: the split is not written yet
    const auto& api = pdfium::api();
    auto bounds = [&](const std::vector<int>& idx) {
        float L = 1e9f, B = 1e9f, R = -1e9f, T = -1e9f;
        for (int i : idx) {
            float l, b, r, t;
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            if (o && api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) { L = std::min(L, l); B = std::min(B, b); R = std::max(R, r); T = std::max(T, t); }
        }
        g.left = L; g.bottom = B; g.right = R; g.top = T;
    };
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = n - 1; i >= 0; --i) {   // the topmost picture
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        float l, b, r, t;
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        if (x < l || x > r || y < b || y > t) continue;
        g.found = true; g.kind = GroupHit::Picture; g.objects = {i};
        bounds(g.objects);
        return g;
    }
    TableHit th = tableAt(d, page, x, y);
    ParagraphHit ph = paragraphAt(d, page, x, y);
    if (ph.found && !th.found) {
        g.found = true; g.kind = GroupHit::Paragraph; g.objects = ph.objects;
        bounds(g.objects);
        return g;
    }
    if (th.found) {
        for (int i = 0; i < n; ++i) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            if (o && tableIdOf(o) == th.id) g.objects.push_back(i);
        }
        g.found = !g.objects.empty(); g.kind = GroupHit::Table;
        bounds(g.objects);
    }
    return g;
}

bool transformGroup(Document& d, int page, const std::vector<int>& objects, double fl, double fb, double fr, double ft, double tl, double tb, double tr, double tt) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || objects.empty() || fr - fl < 1 || ft - fb < 1) return false;
    const double sx = (tr - tl) / (fr - fl), sy = (tt - tb) / (ft - fb);
    if (sx < 0.02 || sy < 0.02) return false;
    const auto& api = pdfium::api();
    d.beginChange(L"Move or resize");
    pg = d.page(page);
    const double e = tl - fl * sx, f = tb - fb * sy;
    for (int i : objects) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (o) api.FPDFPageObj_Transform(o, sx, 0, 0, sy, e, f);
    }
    contentgen::generate(d.handle(), pg);
    d.resetText(page);
    return true;
}

bool deleteGroup(Document& d, int page, std::vector<int> objects) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || objects.empty()) return false;
    const auto& api = pdfium::api();
    d.beginChange(L"Delete");
    pg = d.page(page);
    std::sort(objects.begin(), objects.end(), std::greater<int>());
    bool any = false;
    for (int i : objects) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (o && api.FPDFPage_RemoveObject(pg, o)) { api.FPDFPageObj_Destroy(o); any = true; }
    }
    if (!any) { d.cancelChange(); return false; }
    contentgen::generate(d.handle(), pg);
    d.resetText(page);
    return true;
}

void snapGuides(Document& d, int page, const std::vector<int>& exclude, std::vector<double>& gx, std::vector<double>& gy) {
    gx.clear(); gy.clear();
    FPDF_PAGE pg = d.page(page);
    if (!pg) return;
    const auto& api = pdfium::api();
    float l, b, r, t;
    // the page: its crop box
    double cl, cb, cr, ct;
    if (d.cropBoxOf(page, cl, cb, cr, ct)) { gx.push_back(cl); gx.push_back((cl + cr) / 2); gx.push_back(cr); gy.push_back(cb); gy.push_back((cb + ct) / 2); gy.push_back(ct); }
    const int n = api.FPDFPage_CountObjects(pg);
    std::set<int> skip(exclude.begin(), exclude.end());
    for (int i = 0; i < n && gx.size() < 600; ++i) {
        if (skip.count(i)) continue;
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        gx.push_back(l); gx.push_back((l + r) / 2); gx.push_back(r);
        gy.push_back(b); gy.push_back((b + t) / 2); gy.push_back(t);
    }
}


// ---- more page tools ----
namespace {
// the page drawn small in grey levels (0 black .. 255 white)
std::vector<unsigned char> greyThumb(Document& d, int page, int& w, int& h, double dpi) {
    const PageSize& ps = d.pageSize(page);
    w = std::max(4, static_cast<int>(ps.w * dpi / 72.0)); h = std::max(4, static_cast<int>(ps.h * dpi / 72.0));
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4, 255), g(static_cast<size_t>(w) * h);
    const auto& api = pdfium::api();
    FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, px.data(), w * 4);
    if (fb) { api.FPDFBitmap_FillRect(fb, 0, 0, w, h, 0xFFFFFFFF); d.renderOutput(page, fb, 0, 0, w, h); api.FPDFBitmap_Destroy(fb); }
    for (size_t i = 0; i < g.size(); ++i) g[i] = static_cast<unsigned char>((px[i * 4] + px[i * 4 + 1] * 2 + px[i * 4 + 2]) / 4);
    return g;
}
}

std::vector<int> blankPages(Document& d) {
    std::vector<int> out;
    for (int p = 0; p < d.pageCount(); ++p) {
        if (d.charCount(p) > 0) {
            std::wstring t = d.textRange(p, 0, d.charCount(p));
            if (t.find_first_not_of(L" \t\r\n") != std::wstring::npos) continue;
        }
        int w, h;
        auto g = greyThumb(d, p, w, h, 40);
        size_t dark = 0;
        for (unsigned char v : g) if (v < 200) ++dark;
        if (dark <= g.size() / 500) out.push_back(p);   // at most 0.2 % ink: specks of a scanned blank sheet
    }
    return out;
}

std::vector<std::vector<int>> duplicatePages(Document& d) {
    std::map<std::string, std::vector<int>> groups;
    for (int p = 0; p < d.pageCount(); ++p) {
        int w, h;
        auto g = greyThumb(d, p, w, h, 24);
        std::wstring t = d.charCount(p) > 0 ? d.textRange(p, 0, d.charCount(p)) : L"";
        unsigned long long hsh = 1469598103934665603ULL;
        auto mix = [&](unsigned long long v) { hsh ^= v; hsh *= 1099511628211ULL; };
        mix(static_cast<unsigned long long>(w)); mix(static_cast<unsigned long long>(h));
        for (unsigned char v : g) mix(v / 16);   // small rendering differences do not count
        for (wchar_t c : t) mix(c);
        groups[std::to_string(hsh)].push_back(p);
    }
    std::vector<std::vector<int>> out;
    for (auto& kv : groups) if (kv.second.size() > 1) out.push_back(kv.second);
    std::sort(out.begin(), out.end());
    return out;
}

bool addHeaderFooter(Document& d, const std::vector<int>& pages, const HeaderFooter& hf) {
    if (pages.empty()) return false;
    const auto& api = pdfium::api();
    wchar_t date[64] = L"";
#ifdef _WIN32
    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, nullptr, nullptr, date, 64);
#else
    const time_t now = time(nullptr);
    tm t{}; localtime_r(&now, &t);
    wcsftime(date, 64, L"%x", &t);   // the short date as the user's region writes it
#endif
    const std::wstring file = d.title();
    std::wstring title = file;
    {
        const unsigned long n = api.FPDF_GetMetaText(d.handle(), "Title", nullptr, 0);
        if (n > 2) { std::vector<unsigned short> buf(n / 2 + 1); api.FPDF_GetMetaText(d.handle(), "Title", buf.data(), n); title.assign(buf.begin(), buf.begin() + (n / 2 - 1)); }
    }
    d.beginChange(L"Add header and footer");
    bool any = false;
    int num = hf.firstNumber;
    const int total = hf.firstNumber + static_cast<int>(pages.size()) - 1;
    for (int i : pages) {
        FPDF_PAGE pg = d.page(i);
        if (!pg) { ++num; continue; }
        const double vw = d.pageSize(i).w, vh = d.pageSize(i).h;
        for (int slot = 0; slot < 6; ++slot) {
            std::wstring t = hf.text[slot];
            if (t.find_first_not_of(L' ') == std::wstring::npos) continue;
            auto replaceAll = [&](const std::wstring& a, const std::wstring& b) { size_t p = 0; while ((p = t.find(a, p)) != std::wstring::npos) { t.replace(p, a.size(), b); p += b.size(); } };
            replaceAll(L"{page}", std::to_wstring(num));
            replaceAll(L"{pages}", std::to_wstring(total));
            replaceAll(L"{date}", date);
            replaceAll(L"{file}", file);
            replaceAll(L"{title}", title);
            if (!isLatin(t)) continue;
            FPDF_PAGEOBJECT o = makeText(d, "Helvetica", hf.sizePt, t, hf.color, 100);
            if (!o) continue;
            float l, b, r, tp;
            api.FPDFPageObj_GetBounds(o, &l, &b, &r, &tp);
            const double tw = r - l, th = tp - b;
            const int col = slot % 3;
            const double cx = col == 0 ? hf.marginPt + tw / 2 : col == 1 ? vw / 2 : vw - hf.marginPt - tw / 2;
            if (place(pg, o, vw, vh, cx, slot < 3 ? hf.marginPt : vh - hf.marginPt, 0)) { api.FPDFPage_InsertObject(pg, o); any = true; }
            else api.FPDFPageObj_Destroy(o);
            (void)th;
        }
        contentgen::generate(d.handle(), pg);
        ++num;
    }
    if (!any) { d.cancelChange(); return false; }
    return true;
}

bool addBackground(Document& d, const std::vector<int>& pages, COLORREF color) {
    if (pages.empty()) return false;
    const auto& api = pdfium::api();
    d.beginChange(L"Background colour");
    bool any = false;
    for (int i : pages) {
        FPDF_PAGE pg = d.page(i);
        double l, b, r, t;
        if (!pg || !d.cropBoxOf(i, l, b, r, t)) continue;
        FPDF_PAGEOBJECT o = api.FPDFPageObj_CreateNewRect(static_cast<float>(l), static_cast<float>(b), static_cast<float>(r - l), static_cast<float>(t - b));
        if (!o) continue;
        api.FPDFPageObj_SetFillColor(o, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        api.FPDFPath_SetDrawMode(o, FPDF_FILLMODE_ALTERNATE, 0);
        if (api.FPDFPage_InsertObjectAtIndex(pg, o, 0)) { contentgen::generate(d.handle(), pg); any = true; }
        else api.FPDFPageObj_Destroy(o);
    }
    if (!any) { d.cancelChange(); return false; }
    return true;
}

} // namespace pageobj
