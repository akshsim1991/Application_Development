#include "pageobjects.hpp"
#include "contentgen.hpp"
#include "tables.hpp"
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
#include <array>
#include <cstring>
#include <cstdio>
#include <map>
#include <set>
#include <cwctype>
#include <cctype>
#include <cstdint>

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

// The characters the standard PDF fonts draw (WinAnsi): Latin-1, and the typographic ones of its 0x80-0x9F row
// (dashes, quotes, bullet, ellipsis, euro, trade mark ...).
static bool isWinAnsiExtra(wchar_t c) {
    static const wchar_t extra[] = {0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x017D,
                                    0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x017E, 0x0178};
    for (wchar_t e : extra) if (c == e) return true;
    return false;
}

bool isLatin(const std::wstring& text) {
    for (wchar_t c : text) {
        if (c < 0x20 && c != L'\t' && c != L'\r' && c != L'\n') return false;
        if (c > 0xFF && !isWinAnsiExtra(c)) return false;
    }
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
        contentgen::generate(d, pg);
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
        contentgen::generate(d, pg);
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
    contentgen::generate(d, pg);
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
        if (!contentgen::generate(d, pg)) { if (track) d.rollbackChange(); d.resetText(page); return false; }
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
        if (api.FPDFText_SetText(o, w.data())) {
            if (!contentgen::generate(d, pg)) { if (track) d.rollbackChange(); d.resetText(page); return false; }
            d.resetText(page);
            return true;
        }
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
    if (!contentgen::generate(d, pg)) { if (track) d.rollbackChange(); d.resetText(page); return false; }
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
    contentgen::generate(d, pg);
    return true;
}


// Each match is replaced in its paragraph, which is set again as editing it sets it: in its own fonts, wrapped in its
// column or table cell, what is below moved down when it gets longer. (Replacing inside each text object kept no font for
// letters a subset lacked, ran over its column and across a table's cells, and missed words drawn a letter at a time.)
int replaceAll(Document& d, const std::wstring& find, const std::wstring& with, bool matchCase, std::vector<int>& pages) {
    pages.clear();
    if (find.empty()) return 0;
    const auto& api = pdfium::api();
    auto lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; };
    const std::wstring needle = matchCase ? find : lower(find);
    const size_t depth = d.undoDepth();
    int total = 0;
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_TEXTPAGE tp = d.text(p);
        if (!d.page(p) || !tp) continue;
        // where the matches are: a point on each one's first letter
        struct At { double x, y; };
        std::vector<At> found;
        {
            std::vector<unsigned short> w(find.begin(), find.end());
            w.push_back(0);
            FPDF_SCHHANDLE sh = api.FPDFText_FindStart(tp, w.data(), matchCase ? FPDF_MATCHCASE : 0, 0);
            while (sh && api.FPDFText_FindNext(sh)) {
                const int ci = api.FPDFText_GetSchResultIndex(sh);
                double l, r, b, t;
                for (int k = ci; k < ci + static_cast<int>(find.size()); ++k)
                    if (api.FPDFText_GetCharBox(tp, k, &l, &r, &b, &t) && r > l) { found.push_back({(l + r) / 2, (b + t) / 2}); break; }
            }
            if (sh) api.FPDFText_FindClose(sh);
        }
        // the paragraphs they are in, each once; set again from the bottom of the page up, so the places above stay
        struct Para { At at; double top; std::vector<int> objects; };
        std::vector<Para> paras;
        for (const At& at : found) {
            const ParagraphHit h = paragraphAt(d, p, at.x, at.y);
            if (!h.found) continue;
            std::vector<int> key = h.objects;
            std::sort(key.begin(), key.end());
            bool seen = false;
            for (const Para& q : paras) if (q.objects == key) { seen = true; break; }
            if (!seen) paras.push_back({at, h.top, key});
        }
        std::sort(paras.begin(), paras.end(), [](const Para& a, const Para& b) { return a.top < b.top; });
        bool changed = false;
        for (const Para& q : paras) {
            const ParagraphHit h = paragraphAt(d, p, q.at.x, q.at.y);
            if (!h.found) continue;
            const std::wstring hay = matchCase ? h.text : lower(h.text);
            std::wstring out;
            size_t pos = 0, hitAt;
            int count = 0;
            while ((hitAt = hay.find(needle, pos)) != std::wstring::npos) { out += h.text.substr(pos, hitAt - pos) + with; pos = hitAt + needle.size(); ++count; }
            if (!count) continue;
            out += h.text.substr(pos);
            if (replaceParagraph(d, h, out, h.style, h.align)) { total += count; changed = true; }
        }
        if (changed) pages.push_back(p);
    }
    if (total > 0) d.mergeChangesSince(depth, L"Find and replace");
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
    contentgen::generate(d, pg);
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
    contentgen::generate(d, pg);
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
    std::vector<int> src;      // for each character of `text`, the page object it comes from (-1: a space put between two)
    double base = 0;           // its baseline (page y)
    bool ocr = false;          // recognised text (invisible, over a scan): its sizes are as loose as the recognition
};

// A horizontal line drawn on the page (a table's rule, a separator, a box's top or bottom edge): text above and below it
// is not one paragraph.
struct Rule { double y, l, r; };

static std::vector<Rule> horizontalRules(FPDF_PAGE pg) {
    const auto& api = pdfium::api();
    std::vector<Rule> out;
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        float l, b, r, t;
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_PATH || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) || r - l < 6) continue;
        if (t - b <= 3) out.push_back({(b + t) / 2.0, l, r});
        else { out.push_back({t, l, r}); out.push_back({b, l, r}); }   // a box: its top and bottom edges
    }
    return out;
}

// Whether a rule lies between two lines (one above the other) and runs past their text: a table's row line or a
// separator, not an underline (which sits just under its own words and ends with them).
static bool ruledApart(const std::vector<Rule>& rules, double aboveBase, double belowBase, double size, double l, double r) {
    for (const Rule& u : rules) {
        if (u.y >= aboveBase - 0.05 * size || u.y <= belowBase + 0.5 * size) continue;
        if (u.r < l || u.l > r) continue;
        if (u.l < l - 0.4 * size || u.r > r + 0.4 * size) return true;
        // a fraction's bar: well down in the gap (an underline is just under the letters above) and as wide as the lines
        if (u.y < aboveBase - 0.3 * size && std::min(u.r, r) - std::max(u.l, l) >= 0.6 * (r - l)) return true;
    }
    return false;
}

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
    // (PDFium reads the hyphen that breaks a word at the end of a line, as TeX sets it, as character 2)
    for (wchar_t& c : t) if (c == 0x2010 || c == 0x2011 || c == 0x00AD || c == 0x2012 || c == 0x0002) c = L'-';
    return t;
}

// The plain letter or digit of a character from the Mathematical Alphanumeric Symbols (bold, italic, script ... A-Z a-z,
// Greek, digits), or 0.
wchar_t mathLetter(unsigned cp) {
    if (cp >= 0x1D400 && cp <= 0x1D6A3) { const unsigned i = (cp - 0x1D400) % 52; return static_cast<wchar_t>(i < 26 ? L'A' + i : L'a' + (i - 26)); }
    if (cp >= 0x1D6A8 && cp <= 0x1D7C9) {
        const unsigned i = (cp - 0x1D6A8) % 58;
        if (i < 25) return static_cast<wchar_t>(0x391 + i + (i >= 17 ? 1 : 0));   // capital Alpha .. Omega (no final-sigma slot)
        if (i >= 26 && i < 51) return static_cast<wchar_t>(0x3B1 + (i - 26));     // small alpha .. omega
        return 0;
    }
    if (cp >= 0x1D7CE && cp <= 0x1D7FF) return static_cast<wchar_t>(L'0' + (cp - 0x1D7CE) % 10);
    return 0;
}

std::wstring textOfObject(FPDF_TEXTPAGE tp, FPDF_PAGEOBJECT o) {
    const auto& api = pdfium::api();
    unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
    if (bytes < 2) return {};
    std::vector<unsigned short> buf(bytes / 2 + 1);
    api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
    // Letters from the mathematical alphabets (an equation's italic s, t, G set in Cambria Math, U+1D400 on) are read as
    // the letters they are: outside the basic plane they came apart into two halves that no font draws.
    std::wstring out;
    const size_t n = bytes / 2 - 1;
    for (size_t i = 0; i < n; ++i) {
        const unsigned u = buf[i];
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n && buf[i + 1] >= 0xDC00 && buf[i + 1] <= 0xDFFF) {
            const unsigned cp = 0x10000 + ((u - 0xD800) << 10) + (buf[i + 1] - 0xDC00);
            ++i;
            if (const wchar_t m = mathLetter(cp)) { out += m; continue; }
            if (sizeof(wchar_t) == 2) { out += static_cast<wchar_t>(u); out += static_cast<wchar_t>(buf[i]); }
            else out += static_cast<wchar_t>(cp);
            continue;
        }
        if (u == 0x210E) { out += L'h'; continue; }   // the italic h of the mathematical alphabet (Planck's constant)
        out += static_cast<wchar_t>(u);
    }
    return out;
}

std::string baseFontName(FPDF_FONT font) {
    std::string name;
    size_t len = font ? pdfium::api().FPDFFont_GetBaseFontName(font, nullptr, 0) : 0;
    if (len > 1) { name.assign(len, '\0'); pdfium::api().FPDFFont_GetBaseFontName(font, name.data(), len); name.resize(strlen(name.c_str())); }
    return name;
}

// The name a font gives itself, "Family-Style" ("Calibri-Bold"), from the name table of its embedded TrueType or OpenType
// file; "" when there is none. Microsoft Print to PDF names every font "CIDFont+F1", "CIDFont+F2" ... and marks them all
// serif, with stems that read as bold: the file inside still says "Calibri" and "Regular".
static std::string embeddedFontName(FPDF_FONT font) {
    const auto& api = pdfium::api();
    size_t len = 0;
    if (!font || !api.FPDFFont_GetFontData(font, nullptr, 0, &len) || len < 12 || len > (64u << 20)) return "";
    std::vector<uint8_t> f(len);
    if (!api.FPDFFont_GetFontData(font, f.data(), len, &len) || len != f.size()) return "";
    auto u16 = [&](size_t o) -> unsigned { return o + 2 <= f.size() ? (f[o] << 8) | f[o + 1] : 0; };
    auto u32 = [&](size_t o) -> size_t { return o + 4 <= f.size() ? (size_t(f[o]) << 24) | (f[o + 1] << 16) | (f[o + 2] << 8) | f[o + 3] : 0; };
    const unsigned tables = u16(4);
    size_t at = 0;
    for (unsigned i = 0; i < tables; ++i) {
        const size_t rec = 12 + 16 * size_t(i);
        if (rec + 16 > f.size()) break;
        if (std::memcmp(&f[rec], "name", 4) == 0) { at = u32(rec + 8); break; }
    }
    if (!at || at + 6 > f.size()) return "";
    const unsigned count = u16(at + 2);
    const size_t strings = at + u16(at + 4);
    std::string fam[2], sub[2];   // [0] Windows (Unicode), [1] Mac (Roman)
    for (unsigned i = 0; i < count; ++i) {
        const size_t r = at + 6 + 12 * size_t(i);
        if (r + 12 > f.size()) break;
        const unsigned platform = u16(r), enc = u16(r + 2), lang = u16(r + 4), id = u16(r + 6), n = u16(r + 8), off = u16(r + 10);
        if (id != 1 && id != 2) continue;
        const size_t s0 = strings + off;
        if (s0 + n > f.size()) continue;
        std::string v;
        int which = -1;
        if (platform == 3 && (enc == 1 || enc == 0) && (lang == 0x409 || lang == 0)) {
            which = 0;
            for (size_t k = 0; k + 1 < n; k += 2) { const unsigned c = u16(s0 + k); v += c >= 0x20 && c < 0x7F ? char(c) : '?'; }
        } else if (platform == 1 && enc == 0 && lang == 0) {
            which = 1;
            for (size_t k = 0; k < n; ++k) v += f[s0 + k] >= 0x20 && f[s0 + k] < 0x7F ? char(f[s0 + k]) : '?';
        }
        if (which < 0 || v.empty()) continue;
        (id == 1 ? fam : sub)[which] = v;
    }
    std::string family = !fam[0].empty() ? fam[0] : fam[1], style = !sub[0].empty() ? sub[0] : sub[1];
    if (family.empty() || family.find('?') != std::string::npos) return "";
    family.erase(std::remove(family.begin(), family.end(), ' '), family.end());
    style.erase(std::remove(style.begin(), style.end(), ' '), style.end());
    return style.empty() || style == "Regular" || style == "Normal" || style.find('?') != std::string::npos ? family : family + "-" + style;
}

// A name that says nothing about the font: "CIDFont+F1", "F3", "T1_0", "Unnamed-T3".
static bool placeholderFontName(const std::string& name) {
    std::string n = name;
    if (n.find('+') == 6) n = n.substr(7);
    if (n.empty() || n.rfind("CIDFont", 0) == 0 || n.rfind("Unnamed", 0) == 0) return true;
    size_t i = 0;
    while (i < n.size() && std::isalpha(static_cast<unsigned char>(n[i]))) ++i;
    if (i == 0 || i > 2 || i == n.size()) return false;
    for (; i < n.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(n[i])) && n[i] != '_') return false;
    return true;
}

// Superscripts and subscripts (m³, x², H₂O, eⁿ): kept in the text as the Unicode characters for them, which the editor
// shows and can be typed; set as the plain character, smaller, raised or lowered (so any font draws them).
namespace script {
const wchar_t kSupBase[] = L"0123456789+-=()niabcdefghjklmoprstuvwxyz";
const wchar_t kSup[] = L"⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿⁱ"
                       L"ᵃᵇᶜᵈᵉᶠᵍʰʲᵏˡᵐᵒᵖʳˢᵗᵘᵛʷˣʸᶻ";
const wchar_t kSubBase[] = L"0123456789+-=()aeoxhklmnpst";
const wchar_t kSub[] = L"₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₐₑₒₓₕₖₗₘₙₚₛₜ";
// 1 superscript, -1 subscript, 0 neither; `base` the plain character
inline int of(wchar_t c, wchar_t& base) {
    base = c;
    if (c < 0xB0) return 0;
    if (const wchar_t* p = std::wcschr(kSup, c)) { base = kSupBase[p - kSup]; return 1; }
    if (const wchar_t* p = std::wcschr(kSub, c)) { base = kSubBase[p - kSub]; return -1; }
    return 0;
}
inline wchar_t make(wchar_t c, int kind) {
    const wchar_t* from = kind > 0 ? kSupBase : kSubBase;
    const wchar_t* to = kind > 0 ? kSup : kSub;
    if (const wchar_t* p = std::wcschr(from, c)) return to[p - from];
    return c;
}
inline bool any(const std::wstring& t) { wchar_t b; for (wchar_t c : t) if (of(c, b)) return true; return false; }
inline std::wstring plain(std::wstring t) { for (wchar_t& c : t) { wchar_t b; of(c, b); c = b; } return t; }
} // namespace script

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
    bool mathItalic = false;        // an equation font: letters are its mathematical italic ones (U+1D434 on)

    // The text objects for `t` starting at (e, f): superscripts and subscripts in it are set smaller, raised or lowered.
    std::vector<FPDF_PAGEOBJECT> makeAll(const std::wstring& t, double e, double f) const {
        std::vector<FPDF_PAGEOBJECT> out;
        if (!script::any(t)) { if (FPDF_PAGEOBJECT o = make(t, e, f)) out.push_back(o); return out; }
        const double un = std::hypot(a, b), vn = std::hypot(c, dd);
        const double ux = un > 0 ? a / un : 1, uy = un > 0 ? b / un : 0, vx = vn > 0 ? c / vn : 0, vy = vn > 0 ? dd / vn : 1;
        const double em = fontSize * vn;   // the size as drawn
        double along = 0;
        for (const auto& [seg, kind] : segments(t)) {
            Setter s2 = *this;
            double off = 0;
            if (kind != 0) { s2.fontSize = static_cast<float>(fontSize * 0.62); off = kind > 0 ? 0.36 * em : -0.14 * em; }
            FPDF_PAGEOBJECT o = s2.make(seg, e + along * ux + off * vx, f + along * uy + off * vy);
            if (!o) { for (auto x : out) pdfium::api().FPDFPageObj_Destroy(x); out.clear(); return out; }
            out.push_back(o);
            along += s2.plainWidth(seg) + (kind != 0 ? 0.04 * em : 0);
        }
        return out;
    }
    // `t` in runs of plain text, superscript (1) and subscript (-1), each as its plain characters
    static std::vector<std::pair<std::wstring, int>> segments(const std::wstring& t) {
        std::vector<std::pair<std::wstring, int>> v;
        for (wchar_t ch : t) {
            wchar_t base;
            const int k = script::of(ch, base);
            if (v.empty() || v.back().second != k) v.push_back({std::wstring(), k});
            v.back().first += base;
        }
        return v;
    }
    FPDF_PAGEOBJECT make(const std::wstring& t, double e, double f) const {
        const auto& api = pdfium::api();
        FPDF_PAGEOBJECT o = font ? api.FPDFPageObj_CreateTextObj(d->handle(), font, fontSize) : api.FPDFPageObj_NewTextObj(d->handle(), standard, fontSize);
        if (!o) return nullptr;
        std::vector<unsigned short> w;
        for (wchar_t ch : t) {
            unsigned cp = static_cast<unsigned>(ch);
            if (mathItalic && ch >= L'A' && ch <= L'Z') cp = 0x1D434 + (ch - L'A');
            else if (mathItalic && ch >= L'a' && ch <= L'z') cp = ch == L'h' ? 0x210E : 0x1D44E + (ch - L'a');
            if (cp > 0xFFFF) { cp -= 0x10000; w.push_back(static_cast<unsigned short>(0xD800 + (cp >> 10))); w.push_back(static_cast<unsigned short>(0xDC00 + (cp & 0x3FF))); }
            else w.push_back(static_cast<unsigned short>(cp));
        }
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
        if (!script::any(t)) return plainWidth(t);
        const double em = fontSize * std::hypot(c, dd);
        double along = 0, last = 0;
        for (const auto& [seg, kind] : segments(t)) {
            Setter s2 = *this;
            if (kind != 0) s2.fontSize = static_cast<float>(fontSize * 0.62);
            const double w = s2.plainWidth(seg);
            if (w < 0) return -1;
            last = along + w;
            along = last + (kind != 0 ? 0.04 * em : 0);
        }
        return last;
    }
    double plainWidth(const std::wstring& t) const {
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
    const std::string own = placeholderFontName(fontName) ? embeddedFontName(font) : std::string();
    if (!own.empty()) {
        // the font file's own name is the truth: its flags and weight in the PDF were made up
        fontName = own;
        guessFontStyle(nullptr, own, st.family, st.bold, st.italic);
    } else guessFontStyle(font, fontName, st.family, st.bold, st.italic);
    // an equation's font (Cambria Math, STIX Math ...): its letters are italic serif ones; a standard font that stands in
    // for it is Times Italic
    if (fontName.find("Math") != std::string::npos) { st.family = Family::Serif; st.italic = true; }
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

// The paragraph around line `hitLine`: its lines, top to bottom (neighbouring lines of the same size, spacing and column).
// A line's neighbours are the nearest lines above and below it in its own column (the lines of a page are listed across
// the page, so in two columns the line listed before one is often the other column's).
static std::vector<int> paragraphSpan(const std::vector<Line>& lines, const std::vector<Rule>& rules, int hitLine, bool lineOnly) {
    std::vector<int> span{hitLine};
    if (lineOnly) return span;
    const int n = static_cast<int>(lines.size());
    auto overlap = [](const Line& a, const Line& b) { return std::min(a.r, b.r) - std::max(a.l, b.l); };
    auto centre = [](const Line& a) { return (a.l + a.r) / 2; };
    // the nearest line above (dir -1) or below (+1) that shares some width with line k
    auto neighbour = [&](int k, int dir) {
        for (int j = k + dir; j >= 0 && j < n; j += dir) {
            const Line& o = lines[static_cast<size_t>(j)];
            if (dir < 0 ? o.b <= lines[k].b + 0.3 * lines[k].size : o.b >= lines[k].b - 0.3 * lines[k].size) continue;   // same height: beside it
            if (overlap(o, lines[k]) > 0) return j;
        }
        return -1;
    };
    // how far lines of this size starting at this left edge reach nearby: the column's right edge
    auto columnRight = [&](const Line& ln) {
        double right = ln.r;
        for (const Line& o : lines)
            if (std::fabs(o.l - ln.l) < 3.6 * ln.size && std::fabs(o.size - ln.size) < 0.15 * ln.size && std::fabs(o.b - ln.b) < 30 * ln.size && overlap(o, ln) > 0)
                right = std::max(right, o.r);
        return right;
    };
    // the width of the first word of a line, as drawn
    auto firstWord = [](const Line& ln) {
        const size_t len = ln.text.size();
        size_t w = ln.text.find(L' ');
        if (w == std::wstring::npos) w = len;
        return len ? (ln.r - ln.l) * static_cast<double>(w) / static_cast<double>(len) : 0.0;
    };
    double pageL = 1e9, pageR = -1e9;   // how wide the page's text runs
    for (const Line& o : lines) { pageL = std::min(pageL, o.l); pageR = std::max(pageR, o.r); }
    const double pageSpan = pageR - pageL;
    auto centred = [&](const Line& a, const Line& b) {   // set centred: the same middle, different edges
        return std::fabs(centre(a) - centre(b)) < 0.6 * a.size && std::fabs(a.l - b.l) > 0.6 * a.size;
    };
    auto joins = [&](const Line& above, const Line& below) {
        const double size = above.size;
        if (below.startsItem) return false;
        // a line drawn between them (table rows, a separator) makes them two
        if (ruledApart(rules, above.base, below.base, size, std::min(above.l, below.l), std::max(above.r, below.r))) return false;
        if (std::fabs(above.size - below.size) > (above.ocr && below.ocr ? 0.3 : 0.1) * size) return false;
        if (above.rgb != below.rgb) return false;   // a red line under a black one is not the same paragraph
        const double step = above.b - below.b;
        if (step < 0.8 * size || step > 2.1 * size) return false;
        if (centred(above, below)) return step <= 1.5 * size;   // a heading or title set centred on two lines or more
        const double indent = below.l - above.l;
        if (std::fabs(indent) > 0.6 * size && !(indent > 0 && indent < 3.5 * size) && !(indent < 0 && indent > -3.5 * size)) return false;
        if (!(below.r >= above.l && below.l <= above.r)) return false;
        // A line that ends short of its column, though the next line's first word would have fitted, ends its paragraph:
        // a heading over its text, a line broken on purpose (an address, a poem, a list without bullets).
        const double colR = columnRight(above);
        if (above.r + 0.3 * size + firstWord(below) < colR - 0.5 * size) return false;
        // Short lines that each end a sentence or a label (“Red: stroke=1, fill=1.” over “Green: …”), in a narrow block that
        // does not reach its column's edge: lines set one under another, not wrapped text.
        auto ends = [](const std::wstring& t) { size_t e = t.find_last_not_of(L' '); return e != std::wstring::npos && std::wcschr(L".:;!?", t[e]) != nullptr; };
        auto starts = [](const std::wstring& t) { size_t f = t.find_first_not_of(L' '); return f != std::wstring::npos && (iswupper(t[f]) || iswdigit(t[f])); };
        if (ends(above.text) && starts(below.text) && above.r < colR - 1.0 * size && colR - above.l < pageSpan / 2) return false;
        return true;
    };
    for (int k = hitLine;;) {
        const int j = neighbour(k, -1);
        if (j < 0 || lines[static_cast<size_t>(k)].startsItem || !joins(lines[static_cast<size_t>(j)], lines[static_cast<size_t>(k)])) break;
        if (!centred(lines[static_cast<size_t>(j)], lines[static_cast<size_t>(k)]) && std::fabs(lines[static_cast<size_t>(j)].l - lines[hitLine].l) > 3.6 * lines[hitLine].size) break;
        span.insert(span.begin(), j);
        k = j;
    }
    for (int k = hitLine;;) {
        const int j = neighbour(k, 1);
        if (j < 0 || !joins(lines[static_cast<size_t>(k)], lines[static_cast<size_t>(j)])) break;
        if (!centred(lines[static_cast<size_t>(k)], lines[static_cast<size_t>(j)]) && std::fabs(lines[static_cast<size_t>(j)].l - lines[hitLine].l) > 3.6 * lines[hitLine].size) break;
        span.push_back(j);
        k = j;
    }
    return span;
}

// The text lines of a page, top to bottom (list numbers and bullets left out, the lines they start marked).
static void collectLines(Document& d, FPDF_PAGE pg, FPDF_TEXTPAGE tp, std::vector<Line>& lines) {
    const auto& api = pdfium::api();
    // 1. every text object, with its size taken from the font size and the text matrix
    struct Obj { int idx; double l, b, r, t, size; std::wstring text; FPDF_PAGEOBJECT o; unsigned rgb; double base; bool ocr = false; };
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
        objs.push_back({i, l, b, r, t, sizeIncludesMatrix(d) ? fs : fs * (scale > 0.01 ? scale : 1.0), text, o, (cr << 16) | (cg << 8) | cb, m.f,
                        api.FPDFTextObj_GetTextRenderMode(o) == FPDF_TEXTRENDERMODE_INVISIBLE});
    }
    // Where each object's letters are in the text page: between two objects a space is wherever PDFium's text puts one
    // (it measures with the fonts' real advances). Measuring the gap between the objects' boxes cannot tell a word space
    // from the side of a narrow letter when each letter is an object of its own ("SHAH I D" for "SHAHID").
    std::map<FPDF_PAGEOBJECT, std::pair<int, int>> charsOf;   // first and last character of each object
    const int nChars = api.FPDFText_CountChars(tp);
    for (int c = 0; c < nChars; ++c) {
        FPDF_PAGEOBJECT o = api.FPDFText_GetTextObject(tp, c);
        if (!o) continue;
        auto it = charsOf.find(o);
        if (it == charsOf.end()) charsOf[o] = {c, c};
        else it->second.second = c;
    }
    // -1: no space between `a` and `b` in the text page; 1: a space; 0: cannot tell (the gap decides)
    auto spaceBetween = [&](FPDF_PAGEOBJECT a, FPDF_PAGEOBJECT b) {
        auto ia = charsOf.find(a), ib = charsOf.find(b);
        if (ia == charsOf.end() || ib == charsOf.end() || ib->second.first <= ia->second.second) return 0;
        if (ib->second.first - ia->second.second > 4) return 0;   // not next to each other in the text
        for (int c = ia->second.second + 1; c < ib->second.first; ++c) {
            const unsigned u = api.FPDFText_GetUnicode(tp, c);
            if (u == L' ' || u == L'\t' || u == 0xA0) return 1;
        }
        return -1;
    };
    // 2. lines: objects on one baseline, left to right
    lines.clear();
    std::vector<char> used(objs.size(), 0);
    std::vector<size_t> order(objs.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return objs[a].t != objs[b].t ? objs[a].t > objs[b].t : objs[a].l != objs[b].l ? objs[a].l < objs[b].l : a < b;
    });
    // Exponents and indices (x², H₂O): smaller letters raised or lowered beside a bigger one. They join that line as
    // superscript or subscript characters, and do not start a line of their own while there is such a line for them.
    auto scriptRise = [&](size_t small, size_t big) {   // 1 raised, -1 lowered, 0 neither
        const Obj& a = objs[small]; const Obj& bo = objs[big];
        if (a.ocr || bo.ocr || a.size > 0.85 * bo.size || a.size < 0.3 * bo.size) return 0;
        const double rise = a.base - bo.base;
        if (rise > 0.18 * bo.size && rise < 0.7 * bo.size) return 1;
        if (rise < -0.06 * bo.size && rise > -0.4 * bo.size) return -1;
        return 0;
    };
    std::vector<char> scriptCandidate(objs.size(), 0);
    for (size_t a = 0; a < objs.size(); ++a)
        for (size_t bi = 0; bi < objs.size() && !scriptCandidate[a]; ++bi) {
            if (a == bi || !scriptRise(a, bi)) continue;
            const double gap = std::max(objs[a].l - objs[bi].r, objs[bi].l - objs[a].r);
            if (gap < 0.6 * objs[bi].size && gap > -0.3 * objs[bi].size) scriptCandidate[a] = 1;
        }
    std::vector<int> scriptKind(objs.size(), 0);
    for (int pass = 0; pass < 2; ++pass)
    for (size_t oi : order) {
        if (used[oi] || (pass == 0 && scriptCandidate[oi])) continue;
        // the objects of this line: on the same baseline and of the same size, joined left or right of what is already in
        // it (a bold label before the rest of the line may come second here: its top is lower), not another column
        const double size = objs[oi].size;
        std::vector<size_t> members{oi};
        used[oi] = 1;
        double L = objs[oi].l, R = objs[oi].r;
        for (bool grew = true; grew;) {
            grew = false;
            for (size_t oj : order) {
                if (used[oj]) continue;
                if (const int k = scriptCandidate[oj] ? scriptRise(oj, oi) : 0) {
                    // an exponent or index right beside the line's letters
                    double gap = 1e18;
                    for (size_t m : members) gap = std::min(gap, std::max(objs[oj].l - objs[m].r, objs[m].l - objs[oj].r));
                    if (gap < 0.6 * size && gap > -0.3 * size) {
                        members.push_back(oj);
                        used[oj] = 1;
                        scriptKind[oj] = k;
                        L = std::min(L, objs[oj].l); R = std::max(R, objs[oj].r);
                        grew = true;
                    }
                    continue;
                }
                if (objs[oj].ocr && objs[oi].ocr) {
                    // recognised words: boxes as recognised (taller with descenders or capitals): one line when they share
                    // most of their height
                    const double ov = std::min(objs[oj].t, objs[oi].t) - std::max(objs[oj].b, objs[oi].b);
                    const double hmin = std::min(objs[oj].t - objs[oj].b, objs[oi].t - objs[oi].b);
                    if (ov < 0.6 * hmin || std::max(objs[oj].size, size) > 3.2 * std::min(objs[oj].size, size)) continue;
                } else {
                    // on the line: the same baseline (a hyphen's box is a sliver in the middle of the line's height), or,
                    // when the baselines cannot be compared, about the same bottom
                    const bool sameBase = std::fabs(objs[oj].base - objs[oi].base) < 0.15 * size;
                    if ((!sameBase && std::fabs(objs[oj].b - objs[oi].b) >= 0.4 * size) || std::fabs(objs[oj].size - size) >= 0.15 * size) continue;
                }
                double gap = 1e18;   // to the nearest object of the line (negative: overlapping it)
                size_t closest = members.front();
                for (size_t m : members) {
                    const double g = std::max(objs[oj].l - objs[m].r, objs[m].l - objs[oj].r);
                    if (g < gap) { gap = g; closest = m; }
                }
                if (gap > 2.5 * size || gap < -0.3 * size) continue;   // another column, or drawn over
                // a list number or bullet set apart from its item (wider than a word space) stays a line of its own: it
                // marks the item (2b below) and is not part of its text
                auto trimmed = [](std::wstring t) { while (!t.empty() && t.back() == L' ') t.pop_back(); while (!t.empty() && t.front() == L' ') t.erase(0, 1); return t; };
                if (gap > 0.9 * size && (isListMarker(trimmed(objs[oj].text)) || isListMarker(trimmed(objs[closest].text)))) continue;
                members.push_back(oj);
                used[oj] = 1;
                L = std::min(L, objs[oj].l); R = std::max(R, objs[oj].r);
                grew = true;
            }
        }
        std::sort(members.begin(), members.end(), [&](size_t a, size_t b) { return objs[a].l != objs[b].l ? objs[a].l < objs[b].l : a < b; });
        const size_t m0 = members.front();
        Line ln{objs[m0].l, objs[m0].b, objs[m0].r, objs[m0].t, size, L"", {}, objs[m0].o};
        ln.rgb = objs[oi].rgb;
        ln.base = objs[m0].base;
        ln.ocr = objs[m0].ocr;
        if (ln.ocr) for (size_t k : members) ln.size = std::max(ln.size, objs[k].size);   // the line's tallest recognised word
        for (size_t k = 0; k < members.size(); ++k) {
            const auto& o = objs[members[k]];
            const int sk = scriptKind[members[k]];
            if (k > 0 && sk == 0) {   // (no space before an exponent or index)
                const double gap = o.l - ln.r;
                const int sp = spaceBetween(objs[members[k - 1]].o, o.o);
                const bool space = sp > 0 || (sp == 0 && gap > 0.15 * size);
                if (space && !ln.text.empty() && ln.text.back() != L' ' && !o.text.empty() && o.text.front() != L' ') { ln.text += L' '; ln.src.push_back(-1); }
            }
            std::wstring t = o.text;
            if (sk != 0) {   // as superscript or subscript characters
                while (!t.empty() && t.back() == L' ') t.pop_back();
                while (!t.empty() && t.front() == L' ') t.erase(0, 1);
                for (wchar_t& ch : t) ch = script::make(ch, sk);
            }
            ln.text += t;
            ln.src.insert(ln.src.end(), t.size(), o.idx);
            ln.l = std::min(ln.l, o.l); ln.r = std::max(ln.r, o.r); ln.t = std::max(ln.t, o.t); ln.b = std::min(ln.b, o.b);
            ln.idx.push_back(o.idx);
        }
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

namespace {
// The cells of the page's tables that hold text: a click in one edits the whole cell (its lines are one paragraph, wrapped
// at its lines), not the lines of the page around the point, which may run on into the next cell or row.
struct TextCell { const tables::Table* t; int r, c; double l, b, rr, tp; };
std::vector<TextCell> textCells(FPDF_PAGE pg, const std::vector<tables::Table>& found) {
    const auto& api = pdfium::api();
    std::vector<TextCell> out;
    for (const tables::Table& t : found) {
        if (t.cells.empty() || t.scanned) continue;
        for (int r = 0; r < t.rows(); ++r)
            for (int c = 0; c < t.cols(); ++c) {
                const tables::Cell& cell = t.at(r, c);
                if (cell.covered || cell.objs.empty() || cell.text.find_first_not_of(L" \n") == std::wstring::npos) continue;
                // a text object may also hold the text of the next cell (Word prints a row's cells in one): only its part
                // inside this cell counts
                const double L = t.colX[size_t(c)], R = t.colX[size_t(std::min(t.cols(), c + cell.colSpan))];
                const double T = t.rowY[size_t(r)], B = t.rowY[size_t(std::min(t.rows(), r + cell.rowSpan))];
                TextCell tc{&t, r, c, 1e9, 1e9, -1e9, -1e9};
                for (int i : cell.objs) {
                    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                    float l, b, rr, tp;
                    if (!o || !api.FPDFPageObj_GetBounds(o, &l, &b, &rr, &tp)) continue;
                    const double cl = std::max(double(l), L), cb = std::max(double(b), B), cr = std::min(double(rr), R), ct = std::min(double(tp), T);
                    if (cl >= cr || cb >= ct) continue;
                    tc.l = std::min(tc.l, cl); tc.b = std::min(tc.b, cb); tc.rr = std::max(tc.rr, cr); tc.tp = std::max(tc.tp, ct);
                }
                if (tc.l < tc.rr) out.push_back(tc);
            }
    }
    return out;
}

bool cellParagraph(Document& d, int page, double x, double y, ParagraphHit& hit) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const std::vector<tables::Table> found = tables::detect(d, page);
    const TextCell* best = nullptr;
    const std::vector<TextCell> cells = textCells(pg, found);
    for (const TextCell& tc : cells) {
        const tables::Table& t = *tc.t;
        const tables::Cell& cell = t.at(tc.r, tc.c);
        const double l = t.colX[size_t(tc.c)], r = t.colX[size_t(std::min(t.cols(), tc.c + cell.colSpan))];
        const double top = t.rowY[size_t(tc.r)], bot = t.rowY[size_t(std::min(t.rows(), tc.r + cell.rowSpan))];
        if (x < l || x > r || y > top || y < bot) continue;
        if (!best || (r - l) * (top - bot) < (best->t->colX[size_t(best->c + 1)] - best->t->colX[size_t(best->c)]) * 1e9) best = &tc;
    }
    if (!best) return false;
    const tables::Table& t = *best->t;
    const tables::Cell& cell = t.at(best->r, best->c);
    hit = ParagraphHit{};
    hit.found = true;
    hit.page = page;
    hit.objects = cell.objs;
    std::sort(hit.objects.begin(), hit.objects.end());
    hit.text = cell.text;
    for (auto& ch : hit.text) if (ch == L'\n') ch = L' ';
    hit.left = best->l; hit.top = best->tp; hit.width = best->rr - best->l;
    hit.lines = std::max(1, cell.lines);
    hit.lineHeight = (best->tp - best->b) / hit.lines;
    const double size = cell.size > 0 ? cell.size : 10;
    hit.lineStep = hit.lines > 1 ? (best->tp - best->b - size) / (hit.lines - 1) : size * 1.2;
    if (hit.lineStep < size) hit.lineStep = size * 1.2;
    const double cl = t.colX[size_t(best->c)] + t.pad, cr = t.colX[size_t(std::min(t.cols(), best->c + cell.colSpan))] - t.pad;
    hit.originX = cell.align == 0 || cell.align == 3 ? std::min(best->l, cl + 1) : cl;
    hit.baseline = cell.baseOff > 0 ? t.rowY[size_t(best->r)] - cell.baseOff : best->tp - 0.8 * size;
    hit.wrapWidth = cr - hit.originX;
    hit.align = cell.align == 1 ? Align::Centre : cell.align == 2 ? Align::Right : cell.align == 3 ? Align::Justify : Align::Left;
    if (cell.like >= 0) {
        if (FPDF_PAGEOBJECT o = pdfium::api().FPDFPage_GetObject(pg, cell.like)) styleOfObject(o, size, hit.style, hit.fontName);
    } else { hit.style.family = cell.family; hit.style.bold = cell.bold; hit.style.italic = cell.italic; hit.style.color = cell.color; }
    hit.style.sizePt = std::round(size * 2) / 2;
    hit.sizeExact = size;
    return true;
}
} // namespace

std::vector<EditBlock> editableBlocks(Document& d, int page) {
    std::vector<EditBlock> out;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg) return out;
    const auto& api = pdfium::api();
    // pictures (a group holding pictures counts as one)
    std::function<bool(FPDF_PAGEOBJECT, int)> hasImage = [&](FPDF_PAGEOBJECT o, int depth) {
        const int t = api.FPDFPageObj_GetType(o);
        if (t == FPDF_PAGEOBJ_IMAGE) return !contentgen::drawnAsPicture(o);
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
    const std::vector<Rule> rules = horizontalRules(pg);
    std::vector<char> done(lines.size(), 0);
    // the text in a table's cells: one outline for each cell, as a click there edits the cell
    std::set<int> inCells;
    {
        const std::vector<tables::Table> found = tables::detect(d, page);
        for (const TextCell& tc : textCells(pg, found)) {
            out.push_back({EditBlock::Text, tc.l, tc.b, tc.rr, tc.tp});
            const auto& objs = tc.t->at(tc.r, tc.c).objs;
            inCells.insert(objs.begin(), objs.end());
        }
    }
    for (size_t i = 0; i < lines.size(); ++i)
        for (int ix : lines[i].idx) if (inCells.count(ix)) { done[i] = 1; break; }
    for (size_t i = 0; i < lines.size(); ++i) {
        if (done[i]) continue;
        EditBlock b{EditBlock::Text, 1e9, 1e9, -1e9, -1e9};
        for (int k : paragraphSpan(lines, rules, static_cast<int>(i), false)) {
            done[static_cast<size_t>(k)] = 1;
            b.left = std::min(b.left, lines[k].l); b.bottom = std::min(b.bottom, lines[k].b);
            b.right = std::max(b.right, lines[k].r); b.top = std::max(b.top, lines[k].t);
        }
        out.push_back(b);
    }
    return out;
}

namespace {
bool cellOf(Document& d, const ParagraphHit& hit, std::vector<tables::Table>& found, int& ti, int& row, int& col);

}

namespace {
// A part of the page as drawn (page coordinates l..t), at up to `maxPx` pixels on its longer side: BGRA, rows top down.
struct Area { int w = 0, h = 0; std::vector<uint8_t> px; };
Area drawArea(FPDF_PAGE pg, double l, double b, double r, double t, double maxPx) {
    const auto& api = pdfium::api();
    Area a;
    const double pw = api.FPDF_GetPageWidthF(pg), ph = api.FPDF_GetPageHeightF(pg);
    if (r <= l || t <= b || pw <= 0 || ph <= 0) return a;
    const double scale = std::min(4.0, maxPx / std::max(r - l, t - b));
    const int fw = std::max(1, static_cast<int>(std::lround(pw * scale))), fh = std::max(1, static_cast<int>(std::lround(ph * scale)));
    int x0, y0, x1, y1;
    api.FPDF_PageToDevice(pg, 0, 0, fw, fh, 0, l, t, &x0, &y0);
    api.FPDF_PageToDevice(pg, 0, 0, fw, fh, 0, r, b, &x1, &y1);
    if (x1 < x0) std::swap(x0, x1);
    if (y1 < y0) std::swap(y0, y1);
    a.w = x1 - x0; a.h = y1 - y0;
    if (a.w <= 0 || a.h <= 0 || 1LL * a.w * a.h > 16000000LL) { a.w = a.h = 0; return a; }
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(a.w, a.h, FPDFBitmap_BGRA, nullptr, 0);
    if (!bmp) { a.w = a.h = 0; return a; }
    api.FPDFBitmap_FillRect(bmp, 0, 0, a.w, a.h, 0xFFFFFFFF);
    api.FPDF_RenderPageBitmap(bmp, pg, -x0, -y0, fw, fh, 0, 0);
    const auto* buf = static_cast<const uint8_t*>(api.FPDFBitmap_GetBuffer(bmp));
    const int stride = api.FPDFBitmap_GetStride(bmp);
    a.px.resize(static_cast<size_t>(a.w) * a.h * 4);
    for (int y = 0; y < a.h; ++y) std::memcpy(&a.px[static_cast<size_t>(y) * a.w * 4], buf + static_cast<size_t>(y) * stride, static_cast<size_t>(a.w) * 4);
    api.FPDFBitmap_Destroy(bmp);
    return a;
}
int colourDistance(const uint8_t* p, COLORREF c) {
    return std::abs(p[2] - GetRValue(c)) + std::abs(p[1] - GetGValue(c)) + std::abs(p[0] - GetBValue(c));
}
// The most common colour of the pixels `want` picks (colours grouped in steps of 16), averaged within its group.
template <class Pick> bool commonColour(const Area& a, Pick want, COLORREF& out) {
    std::map<int, std::array<long long, 4>> bins;
    for (int y = 0; y < a.h; ++y)
        for (int x = 0; x < a.w; ++x) {
            if (!want(x, y)) continue;
            const uint8_t* p = &a.px[(static_cast<size_t>(y) * a.w + x) * 4];
            auto& v = bins[(p[2] >> 4) << 8 | (p[1] >> 4) << 4 | (p[0] >> 4)];
            v[0] += p[2]; v[1] += p[1]; v[2] += p[0]; ++v[3];
        }
    long long best = 0;
    for (const auto& [k, v] : bins)
        if (v[3] > best) { best = v[3]; out = RGB(static_cast<int>(v[0] / v[3]), static_cast<int>(v[1] / v[3]), static_cast<int>(v[2] / v[3])); }
    return best > 0;
}

// A paragraph of recognised (invisible) text over a picture: the colour, weight and size of its words in the picture, and
// the patch of background that will hide them when it is set again.
void readScannedLook(Document& d, FPDF_PAGE pg, ParagraphHit& hit, double l, double b, double r, double t, double size) {
    const auto& api = pdfium::api();
    for (int o : hit.objects) {
        FPDF_PAGEOBJECT po = api.FPDFPage_GetObject(pg, o);
        if (!po || api.FPDFPageObj_GetType(po) != FPDF_PAGEOBJ_TEXT || api.FPDFTextObj_GetTextRenderMode(po) != FPDF_TEXTRENDERMODE_INVISIBLE) return;
    }
    bool onPicture = false;
    const double cx = (l + r) / 2, cy = (b + t) / 2;
    for (int i = 0, n = api.FPDFPage_CountObjects(pg); i < n && !onPicture; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        float bl, bb, br, bt;
        onPicture = o && api.FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_IMAGE && api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt) && cx > bl && cx < br && cy > bb && cy < bt;
    }
    if (!onPicture) return;
    // the words' boxes are recognised loosely: the patch reaches a little past them
    const double m = 0.3 * size;
    const double L = l - m, B = b - m, R = r + m, T = t + m;
    const Area a = drawArea(pg, L - m, B - m, R + m, T + m, 1600);
    if (a.w < 8 || a.h < 8) return;
    const double kx = a.w / (R - L + 2 * m), ky = a.h / (T - B + 2 * m);
    const int ix0 = static_cast<int>(2 * m * kx), iy0 = static_cast<int>(2 * m * ky), ix1 = a.w - ix0, iy1 = a.h - iy0;   // the words
    COLORREF bg = RGB(255, 255, 255), ink = RGB(0, 0, 0);
    // the background: the ring around the words; the ink: what is isInk from it, among the words
    if (!commonColour(a, [&](int x, int y) { return x < ix0 / 2 || y < iy0 / 2 || x >= a.w - ix0 / 2 || y >= a.h - iy0 / 2; }, bg)) return;
    auto isInk = [&](int x, int y) { return x >= ix0 && x < ix1 && y >= iy0 && y < iy1 && colourDistance(&a.px[(static_cast<size_t>(y) * a.w + x) * 4], bg) > 150; };
    if (!commonColour(a, isInk, ink)) ink = GetRValue(bg) + GetGValue(bg) + GetBValue(bg) > 384 ? RGB(0, 0, 0) : RGB(255, 255, 255);
    // weight: how thick the strokes are, from the runs of ink along the rows
    long long inkPx = 0, runs = 0;
    for (int y = iy0; y < iy1; ++y) {
        bool in = false;
        for (int x = ix0; x < ix1; ++x) {
            const bool f = isInk(x, y);
            if (f) { ++inkPx; if (!in) ++runs; }
            in = f;
        }
    }
    const double stroke = runs ? static_cast<double>(inkPx) / runs / kx : 0;   // points
    hit.style.bold = stroke > 0.115 * size;
    hit.style.italic = false;
    hit.style.family = Family::Sans;
    hit.style.color = ink;
    // the size that makes the words as wide as they are in the picture, in the standard font they will be set in
    {
        FPDF_PAGEOBJECT o = newText(d, hit.style.bold ? "Helvetica-Bold" : "Helvetica", 10, hit.text, 0, 100);
        float bl, bb, br, bt;
        double total = 0;
        for (int i : hit.objects) {
            FPDF_PAGEOBJECT po = api.FPDFPage_GetObject(pg, i);
            float ol, ob, orr, ot;
            if (po && api.FPDFPageObj_GetBounds(po, &ol, &ob, &orr, &ot)) total += orr - ol;
        }
        if (o && api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt) && br - bl > 1 && total > 0) {
            // the words' own widths (the spaces between them are not in `total`): compare without the spaces
            FPDF_PAGEOBJECT sp = newText(d, hit.style.bold ? "Helvetica-Bold" : "Helvetica", 10, L" ", 0, 100);
            float sl = 0, sb = 0, sr = 0, st = 0;
            double spaceW = 2.78;
            if (sp && api.FPDFPageObj_GetBounds(sp, &sl, &sb, &sr, &st) && sr > sl) spaceW = sr - sl;
            if (sp) api.FPDFPageObj_Destroy(sp);
            const double spaces = static_cast<double>(std::count(hit.text.begin(), hit.text.end(), L' '));
            const double w10 = std::max(1.0, (br - bl) - spaces * spaceW);
            const double fit = 10.0 * total / w10;
            if (fit > 0.5 * size && fit < 2 * size) hit.style.sizePt = std::round(fit * 2) / 2, hit.sizeExact = fit;
        }
        if (o) api.FPDFPageObj_Destroy(o);
    }
    // the size from the height of the first line's letters in the picture (capitals reach about 0.72 of the size, with
    // descenders about 0.93): letter-spaced words (a label in capitals) are wider than the font would set them
    {
        int r0 = -1, r1 = -1;
        for (int y = iy0; y < iy1; ++y) {
            int count = 0;
            for (int x = ix0; x < ix1; ++x) count += isInk(x, y);
            if (count > 0) { if (r0 < 0) r0 = y; r1 = y; }
            else if (r0 >= 0 && y - r1 > std::max(2.0, 0.15 * size * ky)) break;   // the gap under the first line
        }
        if (r0 >= 0 && r1 > r0) {
            std::wstring firstLine = hit.text;
            bool desc = false;
            for (wchar_t ch : firstLine) if (std::wcschr(L"gjpqy(),;", ch)) { desc = true; break; }
            const double h = (r1 - r0 + 1) / ky;
            const double fromHeight = h / (desc ? 0.93 : 0.72);
            if (fromHeight > 0.4 * size && fromHeight < 2.5 * size) { hit.style.sizePt = std::round(fromHeight * 2) / 2; hit.sizeExact = fromHeight; }
        }
    }
    if (hit.lines <= 1) hit.lineStep = 1.2 * hit.sizeExact;
    hit.overScan = true;
    hit.scanBg = bg;
    hit.scanL = L; hit.scanB = B; hit.scanR = R; hit.scanT = T;
}
// A picture of the page's background over l..t, as if what is drawn there were not: each point blended from the
// picture just around the area (left and right on its row, above and below in its column), so a gradient or a tinted
// panel goes on under it. A flat patch in the most common colour where the area cannot be drawn.
FPDF_PAGEOBJECT backgroundPatch(Document& d, FPDF_PAGE pg, double l, double b, double r, double t, COLORREF flat) {
    const auto& api = pdfium::api();
    const double e = std::max(1.0, 0.08 * (t - b));   // the ring read around the area
    const Area a = drawArea(pg, l - e, b - e, r + e, t + e, 1400);
    FPDF_PAGEOBJECT img = nullptr;
    if (a.w > 8 && a.h > 8) {
        const double kx = a.w / (r - l + 2 * e), ky = a.h / (t - b + 2 * e);
        const int x0 = std::max(1, static_cast<int>(e * kx)), y0 = std::max(1, static_cast<int>(e * ky));
        const int x1 = std::max(x0 + 1, a.w - x0), y1 = std::max(y0 + 1, a.h - y0);
        const int w = x1 - x0, h = y1 - y0;
        auto at = [&](int x, int y) { return &a.px[(static_cast<size_t>(std::clamp(y, 0, a.h - 1)) * a.w + std::clamp(x, 0, a.w - 1)) * 4]; };
        // the ring's colour at each row (left, right) and column (top, bottom): the median of a few pixels across it
        auto median = [&](std::vector<std::array<int, 3>>& v) {
            std::array<int, 3> m{};
            for (int k = 0; k < 3; ++k) {
                std::vector<int> c;
                for (auto& p : v) c.push_back(p[k]);
                std::nth_element(c.begin(), c.begin() + c.size() / 2, c.end());
                m[k] = c[c.size() / 2];
            }
            return m;
        };
        // (only pixels near the background colour: the edge of a letter that reaches into the ring would be smeared across)
        const std::array<int, 3> flatC{GetBValue(flat), GetGValue(flat), GetRValue(flat)};
        auto ring = [&](int fx, int fy, int dx, int dy, int depth) {
            std::vector<std::array<int, 3>> v;
            for (int k = 1; k <= depth + 2; ++k) {
                const uint8_t* p = at(fx + dx * k, fy + dy * k);
                if (colourDistance(p, flat) < 90) v.push_back({p[0], p[1], p[2]});
            }
            return v.empty() ? flatC : median(v);
        };
        std::vector<std::array<int, 3>> left(h), right(h), top(w), bottom(w);
        for (int y = 0; y < h; ++y) { left[y] = ring(x0, y0 + y, -1, 0, x0); right[y] = ring(x1 - 1, y0 + y, 1, 0, x0); }
        for (int x = 0; x < w; ++x) { top[x] = ring(x0 + x, y0, 0, -1, y0); bottom[x] = ring(x0 + x, y1 - 1, 0, 1, y0); }
        // smoothed along the edge: compression leaves faint dark fringes by letters, which would show as streaks
        auto smooth = [](std::vector<std::array<int, 3>>& v) {
            const int n = static_cast<int>(v.size()), rad = std::max(3, n / 30);
            std::vector<std::array<int, 3>> o(v.size());
            for (int i = 0; i < n; ++i) {
                long long sum[3] = {0, 0, 0};
                int c = 0;
                for (int j = std::max(0, i - rad); j <= std::min(n - 1, i + rad); ++j) { for (int k = 0; k < 3; ++k) sum[k] += v[j][k]; ++c; }
                for (int k = 0; k < 3; ++k) o[i][k] = static_cast<int>(sum[k] / c);
            }
            v.swap(o);
        };
        smooth(left); smooth(right); smooth(top); smooth(bottom);
        FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, nullptr, 0);
        if (bmp) {
            auto* px = static_cast<uint8_t*>(api.FPDFBitmap_GetBuffer(bmp));
            const int stride = api.FPDFBitmap_GetStride(bmp);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const double u = (x + 0.5) / w, v = (y + 0.5) / h;
                    // nearer edges count more: across a wide, short patch the rows above and below decide
                    const double wh = 1.0 / std::max(1e-3, std::min(u, 1 - u) * w), wv = 1.0 / std::max(1e-3, std::min(v, 1 - v) * h);
                    for (int k = 0; k < 3; ++k) {
                        const double hc = left[y][k] * (1 - u) + right[y][k] * u, vc = top[x][k] * (1 - v) + bottom[x][k] * v;
                        px[y * stride + x * 4 + k] = static_cast<uint8_t>(std::clamp((hc * wh + vc * wv) / (wh + wv), 0.0, 255.0));
                    }
                    px[y * stride + x * 4 + 3] = 255;
                }
            img = api.FPDFPageObj_NewImageObj(d.handle());
            if (img && api.FPDFImageObj_SetBitmap(nullptr, 0, img, bmp)) {
                const FS_MATRIX m{static_cast<float>(r - l), 0, 0, static_cast<float>(t - b), static_cast<float>(l), static_cast<float>(b)};
                api.FPDFPageObj_SetMatrix(img, &m);
            } else if (img) { api.FPDFPageObj_Destroy(img); img = nullptr; }
            api.FPDFBitmap_Destroy(bmp);
        }
    }
    if (img) return img;
    FPDF_PAGEOBJECT rect = api.FPDFPageObj_CreateNewRect(static_cast<float>(l), static_cast<float>(b), static_cast<float>(r - l), static_cast<float>(t - b));
    if (!rect) return nullptr;
    api.FPDFPageObj_SetFillColor(rect, GetRValue(flat), GetGValue(flat), GetBValue(flat), 255);
    api.FPDFPath_SetDrawMode(rect, FPDF_FILLMODE_WINDING, false);
    return rect;
}
} // namespace

ParagraphHit paragraphAt(Document& d, int page, double x, double y, bool lineOnly) {
    ParagraphHit hit;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg || !tp) return hit;
    if (!lineOnly && cellParagraph(d, page, x, y, hit)) return hit;
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
    if (hitLine < 0) {   // between two lines (inside a paragraph's outline): the nearest line across the click
        double nearest = 1e18;
        for (size_t i = 0; i < lines.size(); ++i) {
            const Line& ln = lines[i];
            if (x < ln.l - 2 || x > ln.r + 2) continue;
            const double dy = y > ln.t ? y - ln.t : y < ln.b ? ln.b - y : 0;
            if (dy < 0.9 * ln.size && dy < nearest) { nearest = dy; hitLine = static_cast<int>(i); }
        }
    }
    if (hitLine < 0) return hit;
    // 4. neighbouring lines of the same paragraph
    // (the paragraph's lines are put first, in order: below, lines[first..last] are the paragraph and the rest of the
    // page's lines follow)
    const std::vector<int> span = paragraphSpan(lines, horizontalRules(pg), hitLine, lineOnly);
    {
        std::vector<Line> ordered;
        std::vector<char> taken(lines.size(), 0);
        for (int k : span) { ordered.push_back(lines[static_cast<size_t>(k)]); taken[static_cast<size_t>(k)] = 1; }
        for (size_t k = 0; k < lines.size(); ++k) if (!taken[k]) ordered.push_back(lines[k]);
        lines.swap(ordered);
    }
    const int first = 0, last = static_cast<int>(span.size()) - 1;
    hit.found = true;
    hit.page = page;
    hit.lines = last - first + 1;
    double l = 1e9, r = -1e9;
    std::wstring text;
    std::vector<int> src;   // which object each character comes from
    for (int i = first; i <= last; ++i) {
        const Line& ln = lines[i];
        for (int ix : ln.idx) hit.objects.push_back(ix);
        l = std::min(l, ln.l); r = std::max(r, ln.r);
        std::wstring t = ln.text;
        std::vector<int> ts = ln.src;
        ts.resize(t.size(), -1);
        while (!t.empty() && t.back() == L' ') { t.pop_back(); ts.pop_back(); }
        if (!text.empty()) {
            if (text.size() > 1 && text.back() == L'-' && iswalpha(text[text.size() - 2]) && !t.empty() && iswlower(t[0])) { text.pop_back(); src.pop_back(); }   // a word broken at the line end
            else { text += L' '; src.push_back(-1); }
        }
        text += t;
        src.insert(src.end(), ts.begin(), ts.end());
    }
    hit.text = text;
    hit.charObject = src;
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
        // A line no other starts with (a centred heading) grows into the free space to its right: up to the right edge of
        // the page's text, and not onto anything drawn beside it.
        if (column <= lines[first].r + 0.5) {
            // the right edge of what is on the page (text, rules, pictures; not a background as wide as the page)
            const double pageW = d.pageSize(page).w;
            double contentR = lines[first].r;
            for (const Line& ln : lines) contentR = std::max(contentR, ln.r);
            const int no = api.FPDFPage_CountObjects(pg);
            for (int i = 0; i < no; ++i) {
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                float bl, bb, br, bt;
                if (!o || !api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt)) continue;
                if (br - bl < 0.9 * pageW && br < pageW) contentR = std::max(contentR, static_cast<double>(br));
            }
            // and not onto anything drawn beside the line
            const double bandB = lines[first].b, bandT = lines[first].t;   // the line's letters, descenders included
            for (int i = 0; i < no; ++i) {
                if (std::find(hit.objects.begin(), hit.objects.end(), i) != hit.objects.end()) continue;
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                float bl, bb, br, bt;
                if (!o || !api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt)) continue;
                if (bt < bandB + 1 || bb > bandT - 1) continue;   // not just touching it (a table's lines below)
                if (bl >= lines[first].r - 0.5) contentR = std::min(contentR, static_cast<double>(bl) - 0.5 * size);
            }
            // pages are set with about the same margin on both sides: the text may also reach as far from the right edge
            // as the page's content starts from the left one (a title over a narrow table)
            {
                double contentL = lines[first].l;
                for (const Line& ln : lines) contentL = std::min(contentL, ln.l);
                double mirror = pageW - contentL;
                // not onto anything drawn beside the line
                for (int i = 0; i < no; ++i) {
                    if (std::find(hit.objects.begin(), hit.objects.end(), i) != hit.objects.end()) continue;
                    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                    float bl, bb, br, bt;
                    if (!o || !api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt)) continue;
                    if (bt < bandB + 1 || bb > bandT - 1) continue;
                    if (bl >= lines[first].r - 0.5) mirror = std::min(mirror, static_cast<double>(bl) - 0.5 * size);
                }
                if (mirror > contentR && mirror < pageW) contentR = mirror;
            }
            column = std::max(column, contentR);
        }
        hit.wrapWidth = std::max(widest, column - hit.originX);
    }
    // Centred text: lines sharing their middle and not their left edge; a line on its own is centred when the lines
    // around it (a letterhead's name, department and town) share its middle. Set again around the same middle, as wide as
    // the free space on both sides allows (a logo beside it stops it).
    if (hit.upright && hit.align != Align::Justify) {
        const double size = lines[first].size, mid = (lines[first].l + lines[first].r) / 2;
        bool centred = false;
        if (last > first) {
            bool sameMid = true, sameLeft = true;
            for (int i = first + 1; i <= last; ++i) {
                if (std::fabs((lines[i].l + lines[i].r) / 2 - mid) > 0.6 * size) sameMid = false;
                if (std::fabs(lines[i].l - lines[first].l) > 0.6 * size) sameLeft = false;
            }
            centred = sameMid && !sameLeft;
        } else {
            for (size_t i = static_cast<size_t>(last) + 1; i < lines.size() && !centred; ++i) {
                const Line& o = lines[i];
                if (std::fabs(o.b - lines[first].b) > 8 * size || std::fabs(o.l - lines[first].l) < size) continue;
                if (std::fabs((o.l + o.r) / 2 - mid) < 1.5) centred = true;
            }
        }
        if (centred) {
            hit.align = Align::Centre;
            double l = 1e9, r = -1e9;
            for (int i = first; i <= last; ++i) { l = std::min(l, lines[i].l); r = std::max(r, lines[i].r); }
            double half = (r - l) / 2;
            const double c = (l + r) / 2;
            if (last == first) {
                // the free space: the page's content edges, and not onto anything drawn beside the line
                const double pageW = d.pageSize(page).w;
                double cl = l, cr = r;
                const int no = api.FPDFPage_CountObjects(pg);
                for (int i = 0; i < no; ++i) {
                    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                    float bl, bb, br, bt;
                    if (!o || !api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt) || br - bl >= 0.9 * pageW || bl < 0 || br > pageW) continue;
                    cl = std::min(cl, static_cast<double>(bl)); cr = std::max(cr, static_cast<double>(br));
                }
                for (int i = 0; i < no; ++i) {
                    if (std::find(hit.objects.begin(), hit.objects.end(), i) != hit.objects.end()) continue;
                    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                    float bl, bb, br, bt;
                    if (!o || !api.FPDFPageObj_GetBounds(o, &bl, &bb, &br, &bt)) continue;
                    if (bt < lines[first].b + 1 || bb > lines[first].t - 1) continue;
                    if (bl >= r - 0.5) cr = std::min(cr, static_cast<double>(bl) - 0.5 * size);
                    if (br <= l + 0.5) cl = std::max(cl, static_cast<double>(br) + 0.5 * size);
                }
                half = std::max(half, std::min(c - cl, cr - c));
            }
            hit.originX = c - half;
            hit.wrapWidth = 2 * half;
            hit.firstIndent = hit.restIndent = 0;
        }
    }
    // the style of most of the text (a bold label at the start of a paragraph is not the paragraph's style); each word keeps
    // its own when the paragraph is set again (charObject)
    FPDF_PAGEOBJECT styleObj = lines[first].first;
    {
        std::map<int, size_t> count;
        for (int o : hit.charObject) if (o >= 0) ++count[o];
        size_t best = 0;
        for (const auto& [o, c] : count) if (c > best) { best = c; if (FPDF_PAGEOBJECT po = api.FPDFPage_GetObject(pg, o)) styleObj = po; }
    }
    styleOfObject(styleObj, lines[first].size, hit.style, hit.fontName);
    hit.sizeExact = lines[first].size;
    {   // recognised text on a scanned page: its look in the picture
        double l = 1e9, b = 1e9, r = -1e9, t = -1e9;
        for (int i = first; i <= last; ++i) { l = std::min(l, lines[i].l); b = std::min(b, lines[i].b); r = std::max(r, lines[i].r); t = std::max(t, lines[i].t); }
        if (!lineOnly) readScannedLook(d, pg, hit, l, b, r, t, lines[first].size);
    }
    // in a table's cell: the text wraps at the cell's right line (replaceParagraph sets it as the table does)
    if (!lineOnly) {
        std::vector<tables::Table> found;
        int ti = -1, row = 0, col = 0;
        if (cellOf(d, hit, found, ti, row, col)) {
            const tables::Table& t = found[static_cast<size_t>(ti)];
            const double edge = t.colX[static_cast<size_t>(std::min(t.cols(), col + t.at(row, col).colSpan))] - t.pad;
            if (edge - hit.originX > 10) hit.wrapWidth = edge - hit.originX;
        }
    }
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
            const std::wstring pw = script::plain(w);   // a superscript is drawn as its plain character
            if (limited) { for (wchar_t ch : pw) if (have.find(ch) == std::wstring::npos) { ok = false; break; } }
            else ok = isLatin(pw);
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
        return isLatin(script::plain(w)) ? &fallback : nullptr;
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
    bool mathLetters = false;
    // the standard font: same direction as the paragraph, new size, style and colour
    const double un = std::hypot(own.a, own.b), vn = std::hypot(own.c, own.dd);
    Setter s{&d};
    s.standard = standardFont(st.family, st.bold, st.italic);
    s.fontSize = static_cast<float>(st.sizePt);
    s.a = un > 0 ? own.a / un : 1; s.b = un > 0 ? own.b / un : 0;
    s.c = vn > 0 ? own.c / vn : 0; s.dd = vn > 0 ? own.dd / vn : 1;
    s.color = st.color;
    s.renderMode = own.font && !hit.overScan ? own.renderMode : -1;   // recognised text becomes visible
    f.fallback = s;
    f.main = s;
    f.own = !hit.overScan && own.font && st.family == hit.style.family && st.bold == hit.style.bold && st.italic == hit.style.italic;
    if (f.own) {
        const unsigned long flags = static_cast<unsigned long>(api.FPDFFont_GetFlags(own.font));
        f.limited = api.FPDFFont_GetIsEmbedded(own.font) != 0;
        if (f.limited) {
            // the letters drawn in this font on the page: the text page's characters of its objects, not the spaces PDFium
            // only infers between words (a font WPS embeds may have no space: a space typed was then drawn as a box)
            FPDF_TEXTPAGE tp = d.text(hit.page);
            const int nc = tp ? api.FPDFText_CountChars(tp) : 0;
            for (int c = 0; c < nc; ++c) {
                if (api.FPDFText_IsGenerated(tp, c) == 1) continue;
                FPDF_PAGEOBJECT o = api.FPDFText_GetTextObject(tp, c);
                if (!o || api.FPDFTextObj_GetFont(o) != own.font) continue;
                unsigned u = api.FPDFText_GetUnicode(tp, c);
                if (u >= 0xD800 && u <= 0xDBFF && c + 1 < nc) {   // a letter beyond the basic plane, as its two halves
                    const unsigned lo = api.FPDFText_GetUnicode(tp, c + 1);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); ++c; }
                }
                // an equation font's mathematical italic letters: the font has those, not the plain letters they are read as
                if (u > 0xFFFF || u == 0x210E) { mathLetters = true; continue; }
                if (u == L'\r' || u == L'\n' || u == 0 || u > 0xFFFF) continue;
                if (f.have.find(static_cast<wchar_t>(u)) == std::wstring::npos) f.have += static_cast<wchar_t>(u);
            }
            for (wchar_t ch : f.have) if (ch > L' ') { f.probe = ch; break; }
        } else if (flags & 4) f.own = false;   // a symbol font that is not embedded: its letters are not the ones typed
    }
    if (f.own) {
        f.main = own;
        (void)mathLetters;   // (the font's mathematical italics cannot be set again: PDFium finds no glyph for them; the
                             // letters are set in the standard italic font that stands in for it, the rest in the font)
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
struct Run { std::wstring text; const Setter* set; double width, advance; Fonts* fonts; bool glued = false; };   // glued: the rest of the word before it (no space)
// `wf` gives each word its fonts (a paragraph whose words keep their own fonts, a bold label before regular text ...).
std::vector<Run> runsOf(const std::vector<Fonts*>& wf, const std::vector<std::wstring>& words, bool& ok) {
    std::vector<Run> runs;
    ok = true;
    for (size_t i = 0; i < words.size(); ++i) {
        const auto& w = words[i];
        Fonts& f = *wf[std::min(i, wf.size() - 1)];
        const Setter* s = f.forWord(w);
        // An equation font has the digits and signs of a word, not its plain letters: the word is set in pieces. Other fonts
        // set a word they lack letters of wholly in the stand-in (pieces in two fonts read back as several words).
        const bool mathFont = f.pdfName.find("Math") != std::string::npos;
        bool partly = false;
        if (mathFont && s == &f.fallback && f.own && f.limited) for (wchar_t ch : script::plain(w)) if (ch > L' ' && f.have.find(ch) != std::wstring::npos) { partly = true; break; }
        if ((partly || (!s && mathFont)) && f.own && f.limited) {
            // a word the paragraph's font has only some letters of (an equation's "2s)": the font has the digits and the
            // brackets, not the plain letter): its letters in pieces, each in the font that has it, with no space between
            std::vector<Run> parts;
            for (wchar_t ch : w) {
                wchar_t base;
                script::of(ch, base);
                const Setter* cs = f.have.find(base) != std::wstring::npos ? &f.main : isLatin(std::wstring(1, base)) ? &f.fallback : nullptr;
                if (!cs) { ok = false; return {}; }
                if (!parts.empty() && parts.back().set == cs) parts.back().text += ch;
                else parts.push_back({std::wstring(1, ch), cs, 0, 0, &f, !parts.empty()});
            }
            runs.insert(runs.end(), parts.begin(), parts.end());
            continue;
        }
        if (!s) { ok = false; return {}; }
        // words share a run only when the font draws a space; one WPS embeds may not (the space was drawn as a box), and then
        // each word is its own run, a space's width apart
        const bool spaces = !(s == &f.main && f.limited && f.have.find(L' ') == std::wstring::npos);
        if (!runs.empty() && runs.back().set == s && spaces && !runs.back().glued) runs.back().text += L" " + w;
        else runs.push_back({w, s, 0, 0, &f});
    }
    for (auto& r : runs) {
        r.width = std::max(0.0, r.set->width(r.text));
        r.advance = r.width;
        if (runs.size() > 1) {   // a letter the font has, set after the run, starts at the run's advance
            const std::wstring probe = r.set == &r.fonts->main && r.fonts->limited ? std::wstring(1, r.fonts->probe) : L"x";
            const double a = r.set->width(r.text + probe), b = r.set->width(probe);
            if (a > 0 && b > 0 && a - b > 0) r.advance = a - b;
        }
    }
    return runs;
}

double widthOfRuns(const std::vector<Run>& runs) {
    double w = 0;
    for (size_t i = 0; i < runs.size(); ++i) w += (i + 1 < runs.size() ? runs[i].advance : runs[i].width) + (i && !runs[i].glued ? runs[i - 1].fonts->space : 0);
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
    const auto runs = runsOf({&impl_->fonts}, words, fits);
    return fits ? widthOfRuns(runs) : -1;
}

std::vector<FPDF_PAGEOBJECT> TextSetter::make(const std::wstring& line, double x, double y, double width) {
    const auto words = wordsOf(line);
    if (words.size() < 2) return make(line, x, y);
    std::vector<double> ws;
    double sum = 0;
    for (const auto& w : words) { const double v = this->width(w); if (v < 0) return {}; ws.push_back(v); sum += v; }
    const double gap = (width - sum) / static_cast<double>(words.size() - 1);
    if (gap < impl_->fonts.space * 0.5 || gap > impl_->fonts.space * 8 + 0.5 * width / words.size()) return make(line, x, y);   // not a line to spread
    std::vector<FPDF_PAGEOBJECT> out;
    double at = x;
    for (size_t i = 0; i < words.size(); ++i) {
        std::vector<FPDF_PAGEOBJECT> o = make(words[i], at, y);
        if (o.empty()) { for (FPDF_PAGEOBJECT p : out) pdfium::api().FPDFPageObj_Destroy(p); return {}; }
        out.insert(out.end(), o.begin(), o.end());
        at += ws[i] + gap;
    }
    return out;
}

std::vector<FPDF_PAGEOBJECT> TextSetter::make(const std::wstring& line, double x, double y) {
    std::vector<FPDF_PAGEOBJECT> out;
    const auto words = wordsOf(line);
    bool fits = false;
    const auto runs = runsOf({&impl_->fonts}, words, fits);
    if (!fits) return out;
    double at = x;
    for (size_t i = 0; i < runs.size(); ++i) {
        if (i) at += impl_->fonts.space;
        const std::vector<FPDF_PAGEOBJECT> os = runs[i].set->makeAll(runs[i].text, at, y);
        if (os.empty()) { for (FPDF_PAGEOBJECT p : out) pdfium::api().FPDFPageObj_Destroy(p); return {}; }
        out.insert(out.end(), os.begin(), os.end());
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
    const auto runs = runsOf({&f}, wordsOf(text), ok);
    return ok ? widthOfRuns(runs) : 0;
}

namespace {
std::wstring squeezedText(const std::wstring& t) {
    std::wstring o;
    for (wchar_t c : t) {
        const bool sp = c == L' ' || c == L'\n' || c == L'\r' || c == L'\t';
        if (sp) { if (!o.empty() && o.back() != L' ') o += L' '; } else o += c;
    }
    while (!o.empty() && o.back() == L' ') o.pop_back();
    return o;
}
// The table cell a paragraph is the text of (the same words, its middle inside the cell), if any.
bool cellOf(Document& d, const ParagraphHit& hit, std::vector<tables::Table>& found, int& ti, int& row, int& col) {
    found = tables::detect(d, hit.page);
    const double cx = hit.left + hit.width / 2, cy = hit.top - hit.lineHeight * std::max(1, hit.lines) / 2;
    const std::wstring want = squeezedText(hit.text);
    for (size_t k = 0; k < found.size(); ++k) {
        const tables::Table& t = found[k];
        for (int r = 0; r < t.rows(); ++r)
            for (int c = 0; c < t.cols(); ++c) {
                const tables::Cell& cell = t.at(r, c);
                if (cell.covered) continue;
                const double l = t.colX[static_cast<size_t>(c)], rr = t.colX[static_cast<size_t>(std::min(t.cols(), c + cell.colSpan))];
                const double top = t.rowY[static_cast<size_t>(r)], bot = t.rowY[static_cast<size_t>(std::min(t.rows(), r + cell.rowSpan))];
                if (cx < l || cx > rr || cy > top || cy < bot) continue;
                if (squeezedText(cell.text) != want) return false;   // the cell holds more (or other) text: edit as free text
                ti = static_cast<int>(k); row = r; col = c;
                return true;
            }
    }
    return false;
}
} // namespace

bool replaceParagraph(Document& d, const ParagraphHit& hit, const std::wstring& textIn, const TextStyle& st, Align align, std::wstring* why,
                      bool oneLine) {
    if (!d.page(hit.page) || (!oneLine && hit.wrapWidth < 10)) return false;
    // Text in a table's cell is that cell's text: set again as the table sets it, wrapped inside the cell, its row growing
    // (the rows below and the table's lines move with it). As free text it ran past the cell's right line, onto a second
    // line over the next row.
    if (!oneLine) {
        std::vector<tables::Table> found;
        int ti = -1, row = 0, col = 0;
        if (cellOf(d, hit, found, ti, row, col)) {
            tables::Table& t = found[static_cast<size_t>(ti)];
            tables::Cell& cell = t.at(row, col);
            cell.text = textIn;
            cell.size = st.sizePt; cell.family = st.family; cell.bold = st.bold; cell.italic = st.italic; cell.color = st.color;
            cell.align = align == Align::Centre ? 1 : align == Align::Right ? 2 : align == Align::Justify ? 3 : 0;
            const bool ok = tables::write(d, t, why);
            if (ok) d.resetText(hit.page);
            return ok;
        }
    }
    std::wstring text = textIn;
    if (oneLine) for (auto& ch : text) if (ch == L'\r' || ch == L'\n') ch = L' ';
    const auto& api = pdfium::api();
    Fonts f;
    if (!fontsFor(d, hit, st, f)) return false;
    const double k = std::fabs(st.sizePt - hit.style.sizePt) < 0.05 || hit.sizeExact <= 0 ? 1.0 : st.sizePt / hit.sizeExact;
    // Words keep the fonts they were drawn in (a bold label before regular text stays bold) while the style is the one the
    // paragraph had; a style chosen in the style bar is for the whole paragraph. A new word takes the font of the text
    // before it (at the start, of the text after it).
    std::vector<std::unique_ptr<Fonts>> extra;   // the fonts of the paragraph's other objects
    std::map<int, Fonts*> byObject;
    std::vector<int> wordObject;                 // for each word of `text`, in order, the object whose font it takes
    {
        FPDF_PAGE pg0 = d.page(hit.page);
        std::map<FPDF_FONT, int> fontsSeen;
        for (int o : hit.objects) if (FPDF_PAGEOBJECT po = api.FPDFPage_GetObject(pg0, o)) if (api.FPDFPageObj_GetType(po) == FPDF_PAGEOBJ_TEXT) fontsSeen.emplace(api.FPDFTextObj_GetFont(po), o);
        const bool sameStyle = st.family == hit.style.family && st.bold == hit.style.bold && st.italic == hit.style.italic &&
                               std::fabs(st.sizePt - hit.style.sizePt) < 0.05 && st.color == hit.style.color;
        if (sameStyle && fontsSeen.size() > 1 && hit.charObject.size() == hit.text.size()) {
            const std::wstring& old = hit.text;
            const size_t on = old.size(), nn = text.size();
            size_t pre = 0, suf = 0;
            while (pre < on && pre < nn && old[pre] == text[pre]) ++pre;
            while (suf < on - pre && suf < nn - pre && old[on - 1 - suf] == text[nn - 1 - suf]) ++suf;
            auto sourceOfOld = [&](size_t i, bool back) {   // the nearest character drawn by an object
                for (long j = static_cast<long>(i); j >= 0 && j < static_cast<long>(on); j += back ? -1 : 1) if (hit.charObject[static_cast<size_t>(j)] >= 0) return hit.charObject[static_cast<size_t>(j)];
                return -1;
            };
            auto sourceOfNew = [&](size_t i) {
                if (i < pre) return sourceOfOld(i, true);
                if (i >= nn - suf) return sourceOfOld(on - (nn - i), true);
                // what replaces old text takes that text's style (a word replaced after a bold label stays as it was);
                // what is only typed in takes the style of the text before it
                if (on - suf > pre) { const int r = sourceOfOld(pre, false); if (r >= 0) return r; }
                int s2 = pre > 0 ? sourceOfOld(pre - 1, true) : -1;
                return s2 >= 0 ? s2 : sourceOfOld(pre, false);
            };
            // the words in the order the layout below meets them
            bool inWord = false;
            for (size_t i = 0; i < nn; ++i) {
                const wchar_t c = text[i];
                const bool sp = c == L' ' || c == L'\n' || c == L'\r';
                if (!sp && !inWord) wordObject.push_back(sourceOfNew(i));
                inWord = !sp;
            }
            for (int& o : wordObject) {
                if (o < 0) continue;
                if (byObject.count(o)) continue;
                FPDF_PAGEOBJECT po = api.FPDFPage_GetObject(pg0, o);
                if (!po) { o = -1; continue; }
                ParagraphHit h2 = hit;   // this object's own style, so its font is kept
                h2.objects = {o};
                h2.sizeExact = drawnSize(d, po);
                styleOfObject(po, h2.sizeExact, h2.style, h2.fontName);
                auto fx = std::make_unique<Fonts>();
                if (fontsFor(d, h2, h2.style, *fx)) { byObject[o] = fx.get(); extra.push_back(std::move(fx)); }
            }
        }
    }
    size_t wordAt = 0;   // the next word of `text`
    auto fontsOfWord = [&](size_t gw) -> Fonts* {
        if (gw < wordObject.size()) { auto it = byObject.find(wordObject[gw]); if (it != byObject.end()) return it->second; }
        return &f;
    };
    // The change starts here: setting the text may embed a font. Anything that fails from now on puts the document back.
    d.beginChange(L"Edit a paragraph");
    f.mayLoad = true;
    for (auto& fx : extra) fx->mayLoad = true;
    auto fail = [&](const wchar_t* reason) {
        if (why && reason) *why = reason;
        f.release();
        for (auto& fx : extra) fx->release();
        d.rollbackChange();
        return false;
    };
    const double W = oneLine ? 1e9 : hit.wrapWidth;   // the column keeps its width when the size changes
    const double first = hit.firstIndent;
    double rest = hit.restIndent;
    // A one-line list item that now wraps: its next lines start under its text, not under the bullet or number (as a
    // word processor sets a list). A paragraph of several lines keeps the indent its lines had.
    if (hit.lines == 1 && !oneLine) {
        const std::vector<std::wstring> ws = wordsOf(text);
        auto marker = [](const std::wstring& w) {
            static const std::wstring bullets = L"\u2022\u25E6\u25AA\u25AB\u25CF\u25CB\u2023\u2043\u2013\u2014-*\u00B7\u27A2\u2192";
            if (w.size() == 1 && bullets.find(w[0]) != std::wstring::npos) return true;
            if (w.size() < 2 || w.size() > 5) return false;
            const wchar_t end = w.back();
            if (end != L'.' && end != L')') return false;
            std::wstring core = w.substr(0, w.size() - 1);
            if (!core.empty() && core[0] == L'(' && end == L')') core = core.substr(1);
            if (core.empty()) return false;
            bool digits = true, letter = core.size() == 1 && std::iswalpha(core[0]);
            for (wchar_t c : core) if (!std::iswdigit(c)) digits = false;
            return digits || letter;
        };
        if (ws.size() > 1 && marker(ws[0])) {
            bool ok = true;
            const auto r = runsOf({fontsOfWord(0)}, {ws[0]}, ok);
            if (ok && !r.empty()) rest = first + r[0].advance + r[0].fonts->space;
        }
    }
    // break the text into lines that fit the width; a newline always starts a new line
    struct Out { std::vector<Run> runs; std::vector<std::wstring> words; std::vector<Fonts*> wf; bool last; double indent; };
    std::vector<Out> lines;
    for (const std::wstring& para : splitLines(text)) {
        const std::vector<std::wstring> words = wordsOf(para);
        std::vector<std::wstring> cur;
        std::vector<Fonts*> curF;
        std::vector<Run> curRuns;
        auto indent = [&] { return lines.empty() || lines.back().last ? first : rest; };
        for (const auto& w : words) {
            Fonts* wfont = fontsOfWord(wordAt++);
            std::vector<std::wstring> cand = cur;
            std::vector<Fonts*> candF = curF;
            cand.push_back(w);
            candF.push_back(wfont);
            bool ok = true;
            std::vector<Run> runs = runsOf(candF, cand, ok);
            if (!ok) return fail(L"Paragraphs can use letters, numbers and common symbols of Western languages. Please remove other characters.");
            if (cur.empty() || widthOfRuns(runs) <= W - indent() + 0.25) { cur = std::move(cand); curF = std::move(candF); curRuns = std::move(runs); }
            else {
                const double ind = indent();
                lines.push_back({curRuns, cur, curF, false, ind});
                cur = {w};
                curF = {wfont};
                curRuns = runsOf(curF, cur, ok);
            }
        }
        const double ind = indent();
        lines.push_back({curRuns, cur, curF, true, ind});
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
    std::vector<int> below;           // page objects below the paragraph in its column that have to move down
    std::vector<int> belowAnnots;
    double shift = 0;
    if (upright && !hit.overScan && newLast < oldLast - 0.01) {
        if (newLast - descent < pageBottom) return fail(L"The new text needs more lines than fit on this page. Shorten the text or choose a smaller size.");
        // What is below the paragraph in its column. Only what the longer text would run into moves down, and what that
        // then runs into, and so on, as a word processor moves the text below: a footer or picture far below stays.
        // Everything below is looked at, not only the column: a table wider than the paragraph moves as a whole (its
        // rules and the text in all its cells), never only the part under the paragraph.
        const double colL = hit.originX - 1, colR = hit.originX + W + 1;
        struct Item { bool annot; int index; double l, b, r, t; bool moves; int table; };
        std::vector<Item> items;
        const int n = api.FPDFPage_CountObjects(pg);
        std::vector<int> tableOf(static_cast<size_t>(std::max(n, 0)), -1);
        const std::vector<tables::Table> found = tables::detect(d, hit.page);
        for (size_t k = 0; k < found.size(); ++k)
            for (int i : found[k].objects) if (i >= 0 && i < n) tableOf[static_cast<size_t>(i)] = static_cast<int>(k);
        std::vector<char> tableAbove(found.size(), 0);   // a table with a part above the paragraph's last line
        for (int i = 0; i < n; ++i) {
            if (std::find(hit.objects.begin(), hit.objects.end(), i) != hit.objects.end()) continue;
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            float l, b, r, t;
            if (!o || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
            const int tk = tableOf[static_cast<size_t>(i)];
            if (t > oldLast) { if (tk >= 0) tableAbove[static_cast<size_t>(tk)] = 1; continue; }
            items.push_back({false, i, l, b, r, t, false, tk});
        }
        const int na = api.FPDFPage_GetAnnotCount(pg);
        for (int i = 0; i < na; ++i) {
            FPDF_ANNOTATION a = api.FPDFPage_GetAnnot(pg, i);
            if (!a) continue;
            FS_RECTF r{};
            if (api.FPDFAnnot_GetRect(a, &r) && std::max(r.top, r.bottom) <= oldLast && std::max(r.left, r.right) >= colL && std::min(r.left, r.right) <= colR)
                items.push_back({true, i, std::min(r.left, r.right), std::min(r.top, r.bottom), std::max(r.left, r.right), std::max(r.top, r.bottom), false, -1});
            api.FPDFPage_CloseAnnot(a);
        }
        const double extra = oldLast - newLast;
        // the text right under the paragraph keeps its distance unless there is plenty of room (two lines more than needed)
        bool any = false;
        for (auto& it : items) if (it.r >= colL && it.l <= colR && oldLast - it.t < extra + 2 * step) { it.moves = true; any = true; }
        if (any) {
            shift = extra;
            // what a moved object would come too close to (within 2 points, or onto), or holds (a box around text), moves
            // with it; and a table moves whole
            for (bool grew = true; grew;) {
                grew = false;
                for (auto& y : items) {
                    if (y.moves) continue;
                    for (const auto& x : items) {
                        if (!x.moves) continue;
                        if (y.table >= 0 && y.table == x.table) { y.moves = true; grew = true; break; }
                        // on the same line, a few points apart (a footer's page number): it moves with it
                        const double ov = std::min(x.t, y.t) - std::max(x.b, y.b);
                        if (!x.annot && !y.annot && ov > 0.3 * std::min(x.t - x.b, y.t - y.b) && std::max(y.l - x.r, x.l - y.r) < 12) { y.moves = true; grew = true; break; }
                        if (y.t > x.t || y.r < x.l || y.l > x.r) continue;   // only what is below it and beside it
                        if (x.b - y.t < shift + 2) { y.moves = true; grew = true; break; }
                    }
                }
            }
            for (const auto& it : items)
                if (it.moves && it.table >= 0 && tableAbove[static_cast<size_t>(it.table)])
                    return fail(L"The new text needs more lines, and the table below it starts beside it: it cannot be moved down "
                                L"without tearing it apart. Shorten the text or choose a smaller size.");
            for (const auto& it : items) {
                if (!it.moves) continue;
                if (it.b - shift < pageBottom)
                    return fail(L"The new text needs more lines, and there is no room on this page to move the text below it down. "
                                L"Shorten the text or choose a smaller size.");
                (it.annot ? belowAnnots : below).push_back(it.index);
            }
        }
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
    // Over a scan: a patch of the picture's background hides the old words, under the new text. Where the new text needs
    // more room than the old words had, the picture there must be background too (nothing below can move in a picture).
    if (hit.overScan) {
        double widest = 0;
        for (const Out& ln : lines) widest = std::max(widest, ln.indent + widthOfRuns(ln.runs));
        if (align == Align::Justify) widest = std::max(widest, W);
        const double size = st.sizePt;
        double nl = hit.originX, nr = hit.originX + widest;
        if (align == Align::Centre) { nl = hit.originX + (W - widest) / 2; nr = nl + widest; }
        else if (align == Align::Right) { nl = hit.originX + W - widest; nr = hit.originX + W; }
        const double nt = hit.baseline + 0.95 * size, nb = newLast - 0.3 * size;
        const double L = std::min(hit.scanL, nl - 0.15 * size), R = std::max(hit.scanR, nr + 0.15 * size);
        const double T = std::max(hit.scanT, nt), B = std::min(hit.scanB, nb);
        if (L < hit.scanL - 0.5 || R > hit.scanR + 0.5 || T > hit.scanT + 0.5 || B < hit.scanB - 0.5) {
            const Area a = drawArea(pg, L, B, R, T, 1200);
            if (a.w > 0) {
                const double kx = a.w / (R - L), ky = a.h / (T - B);
                long long out = 0, plain = 0;
                for (int y = 0; y < a.h; ++y)
                    for (int x = 0; x < a.w; ++x) {
                        const double px = L + (x + 0.5) / kx, py = T - (y + 0.5) / ky;
                        if (px >= hit.scanL && px <= hit.scanR && py >= hit.scanB && py <= hit.scanT) continue;   // the old words
                        ++out;
                        if (colourDistance(&a.px[(static_cast<size_t>(y) * a.w + x) * 4], hit.scanBg) < 90) ++plain;
                    }
                if (std::getenv("FEATHER_TRACE_SCAN")) std::fprintf(stderr, "scan: patch %.0f,%.0f-%.0f,%.0f words %.0f,%.0f-%.0f,%.0f plain %lld of %lld\n", L, B, R, T, hit.scanL, hit.scanB, hit.scanR, hit.scanT, plain, out);
                if (out > 0 && plain < 0.92 * out)
                    return fail(L"The new text needs more room than the scanned page has free there (the picture has something beside or below it). Shorten the text or choose a smaller size.");
            }
        }
        FPDF_PAGEOBJECT patch = backgroundPatch(d, pg, L, B, R, T, hit.scanBg);
        if (!patch) return fail(nullptr);
        api.FPDFPage_InsertObject(pg, patch);
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
        for (FPDF_PAGEOBJECT o : s.makeAll(t, ex, ey)) { api.FPDFPage_InsertObject(pg, o); any = true; }
    };
    for (size_t row = 0; row < lines.size(); ++row) {
        const Out& ln = lines[row];
        if (ln.words.empty()) continue;
        const double room = W - ln.indent;
        if (align == Align::Justify && !ln.last && ln.words.size() > 1) {
            // Spread the words over the full width, one object per word, as word spacing does: every gap gets the same
            // extra, after each word's advance, and the last word's last letter ends at the right edge.
            bool ok = true;
            std::vector<std::vector<Run>> ws;   // each word's pieces (one, or several in different fonts)
            std::vector<double> adv, wid;
            for (size_t wi = 0; wi < ln.words.size(); ++wi) {
                const auto& w = ln.words[wi];
                auto r = runsOf({ln.wf[std::min(wi, ln.wf.size() - 1)]}, {w}, ok);
                if (!ok || r.empty()) continue;
                double a = 0;
                for (size_t k = 0; k + 1 < r.size(); ++k) a += r[k].advance;
                const Run& lastPiece = r.back();
                const Setter& s = *lastPiece.set;
                const Fonts& rf = *lastPiece.fonts;
                const std::wstring probe = &s == &rf.main && rf.limited ? std::wstring(1, rf.probe) : L"x";
                const double pa = s.width(lastPiece.text + probe), pb = s.width(probe);
                wid.push_back(a + lastPiece.width);
                adv.push_back(a + (pa > 0 && pb > 0 && pa > pb ? pa - pb : lastPiece.width));
                ws.push_back(r);
            }
            double total = 0;
            for (size_t i = 0; i < ws.size(); ++i) total += i + 1 < ws.size() ? adv[i] : wid[i];
            const double gap = ws.size() > 1 ? (room - total) / (ws.size() - 1) : 0;
            double along = ln.indent;
            for (size_t i = 0; i < ws.size(); ++i) {
                double at = along;
                for (const Run& piece : ws[i]) { put(*piece.set, piece.text, at, static_cast<int>(row)); at += piece.advance; }
                along += adv[i] + gap;
            }
            continue;
        }
        double along = ln.indent;
        const double lw = widthOfRuns(ln.runs);
        if (align == Align::Right) along += room - lw;
        else if (align == Align::Centre) along += (room - lw) / 2;
        for (size_t ri = 0; ri < ln.runs.size(); ++ri) {
            const auto& r = ln.runs[ri];
            put(*r.set, r.text, along, static_cast<int>(row));
            along += r.advance + (ri + 1 < ln.runs.size() && ln.runs[ri + 1].glued ? 0 : r.fonts->space);
        }
    }
    if (!any && !text.empty()) return fail(nullptr);   // nothing could be placed: put the old lines back
    if (!contentgen::generate(d, pg)) return fail(L"This page uses drawing that could not be written back exactly, so it was left as it was.");
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
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE || contentgen::drawnAsPicture(o) || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
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
    if (!contentgen::generate(d, pg)) { d.rollbackChange(); d.resetText(page); return false; }
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
    if (!contentgen::generate(d, pg)) { d.rollbackChange(); d.resetText(page); return false; }
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
        contentgen::generate(d, pg);
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
        if (api.FPDFPage_InsertObjectAtIndex(pg, o, 0)) { contentgen::generate(d, pg); any = true; }
        else api.FPDFPageObj_Destroy(o);
    }
    if (!any) { d.cancelChange(); return false; }
    return true;
}

} // namespace pageobj
