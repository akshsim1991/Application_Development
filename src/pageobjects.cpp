#include "pageobjects.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <map>
#include <cwctype>

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

bool replaceText(Document& d, int page, int idx, const std::wstring& text, bool track) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || idx < 0 || idx >= pdfium::api().FPDFPage_CountObjects(pg)) return false;
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, idx);
    if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) return false;
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return false;

    if (track) d.beginChange();
    o = api.FPDFPage_GetObject(pg, idx);
    if (text.empty()) {
        if (!api.FPDFPage_RemoveObject(pg, o)) { if (track) d.cancelChange(); return false; }
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
    if (!isLatin(text)) { if (track) d.cancelChange(); return false; }
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
    if (!n) { if (track) d.cancelChange(); return false; }
    api.FPDFPageObj_SetMatrix(n, &m);
    if (!api.FPDFPage_RemoveObject(pg, o)) { api.FPDFPageObj_Destroy(n); if (track) d.cancelChange(); return false; }
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


int replaceAll(Document& d, const std::wstring& find, const std::wstring& with, bool matchCase, std::vector<int>& pages) {
    pages.clear();
    if (find.empty()) return 0;
    const auto& api = pdfium::api();
    auto lower = [](std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; };
    const std::wstring needle = matchCase ? find : lower(find);
    d.beginChange();
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
std::string toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
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
    snprintf(idbuf, sizeof(idbuf), "t%u", static_cast<unsigned>(GetTickCount()));
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
    if (track) d.beginChange();
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
    api.FPDFPage_GenerateContent(pg);
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
    if (track) d.beginChange();
    bool any = false;
    for (int i = api.FPDFPage_CountObjects(pg) - 1; i >= 0; --i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || tableIdOf(o) != id) continue;
        if (api.FPDFPage_RemoveObject(pg, o)) { api.FPDFPageObj_Destroy(o); any = true; }
    }
    if (!any) { if (track) d.cancelChange(); return false; }
    api.FPDFPage_GenerateContent(pg);
    d.resetText(page);
    return true;
}


// ---- paragraphs ----
namespace {
struct Line { double l, b, r, t, size; std::wstring text; std::vector<int> idx; FPDF_PAGEOBJECT first; };

std::wstring textOfObject(FPDF_TEXTPAGE tp, FPDF_PAGEOBJECT o) {
    const auto& api = pdfium::api();
    unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
    if (bytes < 2) return {};
    std::vector<unsigned short> buf(bytes / 2 + 1);
    api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
    return std::wstring(buf.begin(), buf.begin() + (bytes / 2 - 1));
}
} // namespace

ParagraphHit paragraphAt(Document& d, int page, double x, double y) {
    ParagraphHit hit;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg || !tp) return hit;
    const auto& api = pdfium::api();
    // 1. every text object, with its size taken from the font size and the text matrix
    struct Obj { int idx; double l, b, r, t, size; std::wstring text; FPDF_PAGEOBJECT o; };
    std::vector<Obj> objs;
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        std::wstring text = textOfObject(tp, o);
        if (text.find_first_not_of(L" \t\r\n") == std::wstring::npos) continue;
        float fs = 0;
        api.FPDFTextObj_GetFontSize(o, &fs);
        FS_MATRIX m{1, 0, 0, 1, 0, 0};
        api.FPDFPageObj_GetMatrix(o, &m);
        const double scale = std::sqrt(static_cast<double>(m.b) * m.b + static_cast<double>(m.d) * m.d);
        objs.push_back({i, l, b, r, t, fs * (scale > 0.01 ? scale : 1.0), text, o});
    }
    // 2. lines: objects on one baseline, left to right
    std::vector<Line> lines;
    std::vector<char> used(objs.size(), 0);
    std::vector<size_t> order(objs.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return objs[a].t != objs[b].t ? objs[a].t > objs[b].t : objs[a].l < objs[b].l; });
    for (size_t oi : order) {
        if (used[oi]) continue;
        Line ln{objs[oi].l, objs[oi].b, objs[oi].r, objs[oi].t, objs[oi].size, objs[oi].text, {objs[oi].idx}, objs[oi].o};
        used[oi] = 1;
        std::vector<size_t> same;
        for (size_t oj : order) if (!used[oj] && std::fabs(objs[oj].b - objs[oi].b) < 0.4 * objs[oi].size && std::fabs(objs[oj].size - objs[oi].size) < 0.15 * objs[oi].size) same.push_back(oj);
        std::sort(same.begin(), same.end(), [&](size_t a, size_t b) { return objs[a].l < objs[b].l; });
        for (size_t oj : same) {
            const double gap = objs[oj].l - ln.r;
            if (gap > 2.5 * objs[oi].size || gap < -0.3 * objs[oi].size) continue;   // another column or overlapping
            ln.text += (gap > 0.15 * objs[oi].size && !ln.text.empty() && ln.text.back() != L' ' ? L" " : L"") + objs[oj].text;
            ln.r = std::max(ln.r, objs[oj].r); ln.t = std::max(ln.t, objs[oj].t); ln.b = std::min(ln.b, objs[oj].b);
            ln.idx.push_back(objs[oj].idx);
            used[oj] = 1;
        }
        lines.push_back(ln);
    }
    std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.t > b.t; });
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
    auto joins = [&](const Line& above, const Line& below) {
        const double size = above.size;
        if (std::fabs(above.size - below.size) > 0.1 * size) return false;
        const double step = above.b - below.b;
        if (step < 0.8 * size || step > 2.1 * size) return false;
        const double indent = below.l - above.l;
        if (std::fabs(indent) > 0.6 * size && !(indent > 0 && indent < 3.5 * size) && !(indent < 0 && indent > -3.5 * size)) return false;
        return below.r >= above.l && below.l <= above.r;
    };
    int first = hitLine, last = hitLine;
    while (first > 0 && joins(lines[first - 1], lines[first]) && std::fabs(lines[first - 1].l - lines[hitLine].l) <= 3.6 * lines[hitLine].size) --first;
    while (last + 1 < static_cast<int>(lines.size()) && joins(lines[last], lines[last + 1]) && std::fabs(lines[last + 1].l - lines[hitLine].l) <= 3.6 * lines[hitLine].size) ++last;
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
    // style from the first line's first object
    FPDF_PAGEOBJECT o = lines[first].first;
    FPDF_FONT font = api.FPDFTextObj_GetFont(o);
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
    else if (has("times") || has("serif") || has("georgia") || has("garamond") || has("cambria") || has("minion") || has("palatino") || (flags & 2)) fam = has("sans") ? Family::Sans : Family::Serif;
    hit.style.family = fam;
    hit.style.bold = has("bold") || has("black") || has("heavy") || (flags & 262144);
    hit.style.italic = has("italic") || has("oblique") || (flags & 64);
    hit.style.sizePt = std::round(lines[first].size * 2) / 2;
    unsigned R = 0, G = 0, B = 0, A = 255;
    api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
    hit.style.color = RGB(R, G, B);
    return hit;
}

bool replaceParagraph(Document& d, const ParagraphHit& hit, const std::wstring& text, const TextStyle& st, Align align) {
    FPDF_PAGE pg = d.page(hit.page);
    if (!pg || !isLatin(text) || hit.width < 10) return false;
    const auto& api = pdfium::api();
    const char* font = standardFont(st.family, st.bold, st.italic);
    const double unit = textWidth(d, font, L"M") / 100.0;   // width of an "M" per point of size, to cut long words if ever needed
    (void)unit;
    auto widthOf = [&](const std::wstring& s) { return textWidth(d, font, s) * st.sizePt / 100.0; };
    // break the text into lines that fit the width; a newline always starts a new line
    std::vector<std::pair<std::wstring, bool>> lines;   // text, ends the paragraph
    for (const std::wstring& para : splitLines(text)) {
        std::wstring cur;
        size_t pos = 0;
        bool anyWord = false;
        while (pos <= para.size()) {
            size_t sp = para.find(L' ', pos);
            std::wstring word = para.substr(pos, sp == std::wstring::npos ? std::wstring::npos : sp - pos);
            if (!word.empty()) {
                anyWord = true;
                const std::wstring cand = cur.empty() ? word : cur + L" " + word;
                if (cur.empty() || widthOf(cand) <= hit.width) cur = cand;
                else { lines.push_back({cur, false}); cur = word; }
            }
            if (sp == std::wstring::npos) break;
            pos = sp + 1;
        }
        if (anyWord || para.empty()) lines.push_back({cur, true});
    }
    const double lh = st.sizePt * (hit.lineHeight > 0 ? std::clamp(hit.lineHeight / std::max(1.0, st.sizePt), 1.0, 2.0) : 1.2);
    // remove the old lines (last object first so the indices stay valid), then set the new ones
    d.beginChange();
    pg = d.page(hit.page);
    std::vector<int> idx = hit.objects;
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int i : idx) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (o && api.FPDFPage_RemoveObject(pg, o)) api.FPDFPageObj_Destroy(o);
    }
    const double vw = d.pageSize(hit.page).w, vh = d.pageSize(hit.page).h;
    const double S = 100.0;
    int dx = 0, dy = 0;
    api.FPDF_PageToDevice(pg, 0, 0, static_cast<int>(vw * S), static_cast<int>(vh * S), 0, hit.left, hit.top, &dx, &dy);
    const double vx = dx / S;
    double lineTop = dy / S;
    bool any = false;
    for (auto& ln : lines) {
        if (!ln.first.empty()) {
            std::wstring t = ln.first;
            FPDF_PAGEOBJECT o = makeText(d, font, st.sizePt, t, st.color, 100);
            if (o) {
                float l, b, r, tp;
                api.FPDFPageObj_GetBounds(o, &l, &b, &r, &tp);
                const double tw = r - l, th = tp - b;
                double x0 = vx;
                if (align == Align::Right) x0 = vx + hit.width - tw;
                else if (align == Align::Centre) x0 = vx + (hit.width - tw) / 2;
                else if (align == Align::Justify && !ln.second && t.find(L' ') != std::wstring::npos) {
                    // spread the words: stretch the line to the full width with extra word spacing (done with several objects)
                    api.FPDFPageObj_Destroy(o);
                    std::vector<std::wstring> words;
                    size_t p0 = 0;
                    while (p0 <= t.size()) { size_t sp = t.find(L' ', p0); words.push_back(t.substr(p0, sp == std::wstring::npos ? std::wstring::npos : sp - p0)); if (sp == std::wstring::npos) break; p0 = sp + 1; }
                    double total = 0;
                    for (auto& w : words) total += widthOf(w);
                    const double gap = words.size() > 1 ? (hit.width - total) / (words.size() - 1) : 0;
                    double cx = vx;
                    for (auto& w : words) {
                        FPDF_PAGEOBJECT wo = makeText(d, font, st.sizePt, w, st.color, 100);
                        if (!wo) continue;
                        float wl, wb, wr, wt;
                        api.FPDFPageObj_GetBounds(wo, &wl, &wb, &wr, &wt);
                        if (place(pg, wo, vw, vh, cx + (wr - wl) / 2, lineTop + (wt - wb) / 2, 0)) { api.FPDFPage_InsertObject(pg, wo); any = true; }
                        else api.FPDFPageObj_Destroy(wo);
                        cx += widthOf(w) + gap;
                    }
                    lineTop += lh;
                    continue;
                }
                if (place(pg, o, vw, vh, x0 + tw / 2, lineTop + th / 2, 0)) { api.FPDFPage_InsertObject(pg, o); any = true; }
                else api.FPDFPageObj_Destroy(o);
            }
        }
        lineTop += lh;
    }
    if (!any && !text.empty()) { d.undo(); return false; }   // nothing could be placed: put the old lines back
    api.FPDFPage_GenerateContent(pg);
    d.resetText(hit.page);
    return true;
}

} // namespace pageobj
