#include "headfoot.hpp"
#include "contentgen.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <map>
#include <set>

namespace headfoot {
namespace {

using pdfium::api;
constexpr double kBand = 0.16;   // the top and bottom strips: 16 % of the page's height each

// ---- where objects are on the page as it is shown (page rotation and boxes included)
struct View {
    FPDF_PAGE pg = nullptr;
    double w = 0, h = 0;
    Box map(double l, double b, double r, double t) const {
        constexpr int S = 16;   // FPDF_PageToDevice works in whole pixels: 1/16 point is fine enough
        const int W = static_cast<int>(std::lround(w * S)), H = static_cast<int>(std::lround(h * S));
        int x[4], y[4];
        const double px[4] = {l, r, l, r}, py[4] = {b, b, t, t};
        for (int i = 0; i < 4; ++i) api().FPDF_PageToDevice(pg, 0, 0, W, H, 0, px[i], py[i], &x[i], &y[i]);
        Box o;
        o.l = *std::min_element(x, x + 4) / static_cast<double>(S); o.r = *std::max_element(x, x + 4) / static_cast<double>(S);
        o.t = *std::min_element(y, y + 4) / static_cast<double>(S); o.b = *std::max_element(y, y + 4) / static_cast<double>(S);
        return o;
    }
    // from the shown page back to page space
    void unmap(double dx, double dy, double& x, double& y) const {
        constexpr int S = 16;
        api().FPDF_DeviceToPage(pg, 0, 0, static_cast<int>(std::lround(w * S)), static_cast<int>(std::lround(h * S)), 0,
                                static_cast<int>(std::lround(dx * S)), static_cast<int>(std::lround(dy * S)), &x, &y);
    }
};

View viewOf(Document& d, int page) {
    View v;
    v.pg = d.page(page);
    if (v.pg) { v.w = api().FPDF_GetPageWidthF(v.pg); v.h = api().FPDF_GetPageHeightF(v.pg); }
    return v;
}

std::wstring objText(FPDF_TEXTPAGE tp, FPDF_PAGEOBJECT o) {
    const unsigned long bytes = api().FPDFTextObj_GetText(o, tp, nullptr, 0);
    if (bytes < 2) return {};
    std::vector<unsigned short> buf(bytes / 2 + 1);
    api().FPDFTextObj_GetText(o, tp, buf.data(), bytes);
    return std::wstring(buf.begin(), buf.begin() + (bytes / 2 - 1));
}

// bounds of an object inside a form, on the page: the form's own matrix applied to the bounds in the form's space
bool childBounds(FPDF_PAGEOBJECT form, FPDF_PAGEOBJECT child, float& l, float& b, float& r, float& t) {
    if (!api().FPDFPageObj_GetBounds(child, &l, &b, &r, &t)) return false;
    FS_MATRIX m;
    if (!api().FPDFPageObj_GetMatrix(form, &m)) return true;
    const double xs[4] = {l, r, l, r}, ys[4] = {b, b, t, t};
    double nl = 1e9, nr = -1e9, nb = 1e9, nt = -1e9;
    for (int i = 0; i < 4; ++i) {
        const double x = m.a * xs[i] + m.c * ys[i] + m.e, y = m.b * xs[i] + m.d * ys[i] + m.f;
        nl = std::min(nl, x); nr = std::max(nr, x); nb = std::min(nb, y); nt = std::max(nt, y);
    }
    l = static_cast<float>(nl); r = static_cast<float>(nr); b = static_cast<float>(nb); t = static_cast<float>(nt);
    return true;
}

// marked as a header, footer or page number in a tagged PDF (/Artifact with /Subtype /Header, /Footer, /Pagination)
int artifactBand(FPDF_PAGEOBJECT o) {   // 0 none, 1 header, 2 footer, 3 pagination (band from its place)
    const int n = api().FPDFPageObj_CountMarks(o);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECTMARK m = api().FPDFPageObj_GetMark(o, static_cast<unsigned long>(i));
        unsigned short name[64] = {};
        unsigned long len = 0;
        if (!m || !api().FPDFPageObjMark_GetName(m, name, sizeof(name), &len)) continue;
        if (std::wstring(name, name + std::min<unsigned long>(len / 2, 63)).find(L"Artifact") != 0) continue;
        for (const char* key : {"Subtype", "Type"}) {
            unsigned short v[64] = {};
            unsigned long vl = 0;
            if (!api().FPDFPageObjMark_GetParamStringValue(m, key, v, sizeof(v), &vl)) continue;
            const std::wstring s(v, v + std::min<unsigned long>(vl / 2, 63));
            if (s.find(L"Header") == 0) return 1;
            if (s.find(L"Footer") == 0) return 2;
            if (s.find(L"Pagination") == 0) return 3;
        }
    }
    return 0;
}

// A unit: one object, or a line of text objects next to each other, inside a strip of one page.
struct Unit {
    int page = 0;
    Band band = Band::Header;
    std::vector<std::vector<int>> paths;
    Box box;
    std::wstring text;          // text units
    double size = 0;            // font size
    int type = 0;               // FPDF_PAGEOBJ_*
    bool scanned = false, artifact = false, inForm = false;
};

std::wstring normalise(const std::wstring& s) {   // digits as '#', runs of spaces as one
    std::wstring o;
    for (wchar_t c : s) {
        if (std::iswdigit(c)) { if (o.empty() || o.back() != L'#') o += L'#'; }
        else if (std::iswspace(c)) { if (!o.empty() && o.back() != L' ') o += L' '; }
        else o += static_cast<wchar_t>(std::towlower(c));
    }
    while (!o.empty() && o.back() == L' ') o.pop_back();
    return o;
}

// the same words, whatever the spaces and capitals
bool sameWords(const std::wstring& a, const std::wstring& b) {
    auto clean = [](const std::wstring& s) {
        std::wstring o;
        for (wchar_t c : s) {
            if (std::iswspace(c)) { if (!o.empty() && o.back() != L' ') o += L' '; }
            else o += static_cast<wchar_t>(std::towlower(c));
        }
        while (!o.empty() && o.back() == L' ') o.pop_back();
        return o;
    };
    return clean(a) == clean(b);
}

std::vector<long> numbersIn(const std::wstring& s) {
    std::vector<long> v;
    for (size_t i = 0; i < s.size();) {
        if (std::iswdigit(s[i])) { long x = 0; while (i < s.size() && std::iswdigit(s[i]) && x < 100000000) x = x * 10 + (s[i++] - L'0'); v.push_back(x); }
        else ++i;
    }
    return v;
}

long romanValue(const std::wstring& w) {
    long total = 0, prev = 0;
    for (auto it = w.rbegin(); it != w.rend(); ++it) {
        const wchar_t c = static_cast<wchar_t>(std::towlower(*it));
        const long v = c == L'i' ? 1 : c == L'v' ? 5 : c == L'x' ? 10 : c == L'l' ? 50 : c == L'c' ? 100 : 0;
        if (!v) return -1;
        total += v < prev ? -v : v;
        prev = std::max(prev, v);
    }
    return total;
}

bool looksLikeDate(const std::wstring& s) {
    static const wchar_t* months[] = {L"jan", L"feb", L"mar", L"apr", L"may", L"jun", L"jul", L"aug", L"sep", L"oct", L"nov", L"dec"};
    std::wstring low;
    for (wchar_t c : s) low += static_cast<wchar_t>(std::towlower(c));
    const auto nums = numbersIn(s);
    bool month = false;   // a month's name as a word of its own ("march", "mar."), not inside another word ("remark")
    for (const wchar_t* m : months)
        for (size_t at = low.find(m); at != std::wstring::npos; at = low.find(m, at + 1))
            if (at == 0 || !std::iswalpha(low[at - 1])) {   // the word starts there: is the whole word a month?
                size_t e = at;
                while (e < low.size() && std::iswalpha(low[e])) ++e;
                static const wchar_t* full[] = {L"january", L"february", L"march", L"april", L"may", L"june", L"july", L"august", L"september", L"october", L"november", L"december", L"sept"};
                const std::wstring word = low.substr(at, e - at);
                bool ok = word.size() == 3;
                for (const wchar_t* f : full) ok = ok || word == f;
                if (ok) month = true;
            }
    bool year = false;
    for (long n : nums) if (n >= 1900 && n <= 2199) year = true;
    if (month && (year || nums.size() >= 1)) return true;
    // 12/05/2026, 2026-05-12, 12.05.26
    int seps = 0;
    for (wchar_t c : s) if (c == L'/' || c == L'-' || c == L'.') ++seps;
    return nums.size() == 3 && seps >= 2 && s.size() <= 12;
}

bool inBand(const Box& b, const View& v, Band& band) {
    if (b.b <= v.h * kBand + 0.5) { band = Band::Header; return true; }
    if (b.t >= v.h * (1 - kBand) - 0.5) { band = Band::Footer; return true; }
    return false;
}

// the units of one page: objects (and objects inside forms that reach out of the strips) wholly inside a strip
void unitsOf(Document& d, int page, std::vector<Unit>& out, bool& scanned, bool& scannedNoText) {
    const View v = viewOf(d, page);
    scanned = scannedNoText = false;
    if (!v.pg) return;
    FPDF_TEXTPAGE tp = d.text(page);
    const int n = api().FPDFPage_CountObjects(v.pg);
    // a scan: one picture over most of the page; its text, if any, is the recognised (invisible) text
    int visibleText = 0, invisibleText = 0;
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(v.pg, i);
        const int type = api().FPDFPageObj_GetType(o);
        float l, b, r, t;
        if (type == FPDF_PAGEOBJ_IMAGE && api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t) && (r - l) * (t - b) > 0.6 * v.w * v.h) scanned = true;
        if (type == FPDF_PAGEOBJ_TEXT) (api().FPDFTextObj_GetTextRenderMode(o) == FPDF_TEXTRENDERMODE_INVISIBLE ? invisibleText : visibleText)++;
    }
    if (scanned && visibleText > 0) scanned = false;   // a picture under real text: not a scan
    if (scanned && invisibleText == 0) scannedNoText = true;

    std::vector<Unit> raw;
    auto consider = [&](FPDF_PAGEOBJECT o, std::vector<int> path, float l, float b, float r, float t, bool inForm) {
        Unit u;
        u.page = page;
        u.box = v.map(l, b, r, t);
        u.type = api().FPDFPageObj_GetType(o);
        u.inForm = inForm;
        const int art = artifactBand(o);
        u.artifact = art != 0;
        if (!inBand(u.box, v, u.band)) {
            if (!art) return false;
            u.band = art == 2 ? Band::Footer : art == 1 ? Band::Header : ((u.box.t + u.box.b) / 2 > v.h / 2 ? Band::Footer : Band::Header);
        }
        if (u.type == FPDF_PAGEOBJ_TEXT) {
            u.text = objText(tp, o);
            float fs = 0;
            api().FPDFTextObj_GetFontSize(o, &fs);
            FS_MATRIX m;
            if (api().FPDFPageObj_GetMatrix(o, &m)) fs = static_cast<float>(fs * std::sqrt(std::fabs(m.a * m.d - m.b * m.c)));
            u.size = fs;
            if (u.text.find_first_not_of(L" \t\r\n") == std::wstring::npos) return true;   // spaces only: nothing to find
            if (scanned) u.scanned = true;
        } else if (scanned) return true;   // on a scan only the recognised text counts (the picture is the page)
        u.paths.push_back(std::move(path));
        raw.push_back(std::move(u));
        return true;
    };
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(v.pg, i);
        float l, b, r, t;
        if (!api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        if (consider(o, {i}, l, b, r, t, false)) continue;
        // a form that is not wholly in a strip (a page template): its own pieces may be
        if (api().FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_FORM) {
            const int k = api().FPDFFormObj_CountObjects(o);
            for (int j = 0; j < k; ++j) {
                FPDF_PAGEOBJECT c = api().FPDFFormObj_GetObject(o, static_cast<unsigned long>(j));
                float cl, cb, cr, ct;
                if (c && childBounds(o, c, cl, cb, cr, ct)) consider(c, {i, j}, cl, cb, cr, ct, true);
            }
        }
    }
    // every line of text on the page (to tell a header line from the first lines of the page's body)
    std::vector<Box> lines;
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(v.pg, i);
        if (api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
        float l, b, r, t;
        if (api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) lines.push_back(v.map(l, b, r, t));
    }
    // pieces of text on one line next to each other make one unit ("Page", "3", "of", "6")
    std::vector<char> used(raw.size(), 0);
    for (size_t a = 0; a < raw.size(); ++a) {
        if (used[a]) continue;
        Unit u = raw[a];
        used[a] = 1;
        if (u.type == FPDF_PAGEOBJ_TEXT) {
            bool grew = true;
            while (grew) {
                grew = false;
                for (size_t c = 0; c < raw.size(); ++c) {
                    if (used[c] || raw[c].type != FPDF_PAGEOBJ_TEXT || raw[c].band != u.band || raw[c].inForm != u.inForm) continue;
                    const Box& x = raw[c].box;
                    const double hgt = std::max(4.0, std::max(u.box.b - u.box.t, x.b - x.t));
                    const bool sameLine = std::fabs((x.t + x.b) / 2 - (u.box.t + u.box.b) / 2) < 0.4 * hgt;
                    const double gap = std::max(x.l - u.box.r, u.box.l - x.r);
                    if (!sameLine || gap > 1.2 * std::max(u.size, raw[c].size)) continue;
                    // in reading order
                    if (x.l < u.box.l) u.text = raw[c].text + (gap > 0.15 * u.size ? L" " : L"") + u.text;
                    else u.text += (gap > 0.15 * u.size ? L" " : L"") + raw[c].text;
                    u.box.l = std::min(u.box.l, x.l); u.box.r = std::max(u.box.r, x.r); u.box.t = std::min(u.box.t, x.t); u.box.b = std::max(u.box.b, x.b);
                    u.paths.insert(u.paths.end(), raw[c].paths.begin(), raw[c].paths.end());
                    u.artifact = u.artifact || raw[c].artifact;
                    used[c] = 1;
                    grew = true;
                }
            }
        }
        // text that runs on as a paragraph out of the strip (lines close below a header line, or above a footer line,
        // reaching the body) is the page's body, even where its first lines repeat from page to page
        if (u.type == FPDF_PAGEOBJ_TEXT && !u.artifact) {
            Box cur = u.box;
            bool body = false;
            for (int step = 0; step < 60 && !body; ++step) {
                const double hgt = std::max(4.0, cur.b - cur.t);
                bool next = false;
                Box best{};
                double bestGap = 1e9;
                for (const Box& x : lines) {
                    if (x.r < cur.l - 2 || x.l > cur.r + 2) continue;   // not under (or over) it
                    const double gap = u.band == Band::Header ? x.t - cur.b : cur.t - x.b;
                    if (gap < -0.3 * hgt || gap > 0.9 * hgt) continue;   // the next line of a paragraph: closer than a line
                    if ((u.band == Band::Header ? x.t <= cur.t + 0.5 : x.b >= cur.b - 0.5)) continue;
                    if (gap < bestGap) { bestGap = gap; best = x; next = true; }
                }
                if (!next) break;
                Band bb;
                if (!inBand(best, v, bb) || bb != u.band) body = true;
                cur = best;
            }
            if (body) continue;
        }
        out.push_back(std::move(u));
    }
}

// Do two units on different pages hold the same thing in the same place?
bool samePlace(const Unit& a, const Unit& b) {
    if (a.band != b.band || a.type != b.type || a.scanned != b.scanned) return false;
    constexpr double tol = 6;
    const bool yOk = std::fabs(a.box.t - b.box.t) < tol || std::fabs(a.box.b - b.box.b) < tol;
    const bool xOk = std::fabs(a.box.l - b.box.l) < tol || std::fabs(a.box.r - b.box.r) < tol || std::fabs((a.box.l + a.box.r) - (b.box.l + b.box.r)) / 2 < tol;
    if (!yOk || !xOk) return false;
    // the same text, numbers included (text whose number changes from page to page counts only as a page number)
    if (a.type == FPDF_PAGEOBJ_TEXT) return sameWords(a.text, b.text);
    // pictures, lines, shapes, forms: the same size
    return std::fabs((a.box.r - a.box.l) - (b.box.r - b.box.l)) < tol && std::fabs((a.box.b - a.box.t) - (b.box.b - b.box.t)) < tol;
}

// A page number is a short label: "3", "- 3 -", "Page 3", "Page 3 of 6", "3 / 6", "p. 3", "iv" (a few words at most, and
// mostly the number), not a sentence that happens to contain the page's number.
bool shortLabel(const std::wstring& text) {
    std::wstring t;
    for (wchar_t c : text) if (!std::iswspace(c) || (!t.empty() && t.back() != L' ')) t += std::iswspace(c) ? L' ' : c;
    while (!t.empty() && t.back() == L' ') t.pop_back();
    if (t.empty() || t.size() > 24) return false;
    int words = 1, letters = 0;
    for (wchar_t c : t) { if (c == L' ') ++words; else if (std::iswalpha(c)) ++letters; }
    return words <= 5 && letters <= 12;
}

// the page number in a unit's text, when it follows the pages (the same offset from the page on most of them)
bool isPageNumber(const std::vector<const Unit*>& us, double bodySize) {
    if (us.size() < 2) return false;
    for (const Unit* u : us) if (!shortLabel(u->text)) return false;
    // a page number is not set larger than the text of the pages ("Section 2" as a heading on page 2 is not one)
    if (bodySize > 0) for (const Unit* u : us) if (u->size > bodySize * 1.15 + 0.5) return false;
    // each number position in the text is tried
    size_t most = 0;
    for (const Unit* u : us) most = std::max(most, numbersIn(u->text).size());
    for (size_t k = 0; k < most; ++k) {
        std::map<long, int> offsets;
        for (const Unit* u : us) {
            const auto nums = numbersIn(u->text);
            if (k < nums.size()) offsets[nums[k] - u->page]++;
        }
        for (const auto& o : offsets) if (o.second >= 2 && o.second * 10 >= static_cast<int>(us.size()) * 8) return true;
    }
    // roman numerals (front matter)
    std::map<long, int> offsets;
    for (const Unit* u : us) {
        std::wstring w;
        for (wchar_t c : u->text) if (std::iswalpha(c)) w += c; else if (!w.empty()) break;
        const long v = romanValue(w);
        if (v > 0) offsets[v - u->page]++;
    }
    for (const auto& o : offsets) if (o.second >= 2 && o.second * 10 >= static_cast<int>(us.size()) * 8) return true;
    return false;
}

std::wstring shortText(const std::wstring& raw) {
    std::wstring s = raw;
    while (!s.empty() && std::iswspace(s.back())) s.pop_back();
    while (!s.empty() && std::iswspace(s.front())) s.erase(s.begin());
    std::wstring one;   // runs of spaces as one
    for (wchar_t c : s) if (!std::iswspace(c) || (!one.empty() && one.back() != L' ')) one += std::iswspace(c) ? L' ' : c;
    s = one;
    std::wstring t = s.size() > 40 ? s.substr(0, 38) + L"\x2026" : s;
    return L"\x201C" + t + L"\x201D";
}

// Long dark horizontal lines in the strips of a scanned page (rules under a header, over a footer), from a render at one
// pixel per point. Only lines longer than a third of the page count.
std::vector<Box> scanRules(Document& d, int page, const View& v) {
    std::vector<Box> out;
    const int W = static_cast<int>(v.w), H = static_cast<int>(v.h);
    if (W < 50 || H < 50) return out;
    FPDF_BITMAP bmp = api().FPDFBitmap_Create(W, H, 0);
    if (!bmp) return out;
    api().FPDFBitmap_FillRect(bmp, 0, 0, W, H, 0xFFFFFFFF);
    d.renderPage(page, bmp, 0, 0, W, H, false);
    const auto* px = static_cast<const unsigned char*>(api().FPDFBitmap_GetBuffer(bmp));
    const int stride = api().FPDFBitmap_GetStride(bmp);
    const int band = static_cast<int>(H * kBand);
    for (int y = 0; y < H; ++y) {
        if (y > band && y < H - band) continue;
        int run = 0, best = 0, bestEnd = 0;
        for (int x = 0; x < W; ++x) {
            const unsigned char* q = px + y * stride + x * 4;
            const bool dark = (q[0] + q[1] + q[2]) < 3 * 110;
            run = dark ? run + 1 : 0;
            if (run > best) { best = run; bestEnd = x; }
        }
        if (best < W / 3) continue;
        const Box b{static_cast<double>(bestEnd - best + 1), static_cast<double>(y), static_cast<double>(bestEnd + 1), static_cast<double>(y + 1)};
        if (!out.empty() && std::fabs(out.back().b - b.t) < 0.5 && std::fabs(out.back().l - b.l) < 4) { out.back().b = b.b; continue; }   // a thick line
        out.push_back(b);
    }
    api().FPDFBitmap_Destroy(bmp);
    return out;
}

} // namespace

int Element::pageCount() const {
    std::set<int> p;
    for (const auto& i : items) p.insert(i.page);
    return static_cast<int>(p.size());
}

std::wstring kindName(Kind k) {
    switch (k) {
        case Kind::PageNumber: return L"Page number";
        case Kind::Date: return L"Date";
        case Kind::Text: return L"Text";
        case Kind::Line: return L"Line";
        case Kind::Picture: return L"Picture or logo";
        case Kind::Shape: return L"Shape";
        case Kind::Group: return L"Block";
        case Kind::Area: return L"Area you drew";
    }
    return L"";
}

Detection detect(Document& d) {
    Detection det;
    det.pages = d.pageCount();
    det.scannedPage.assign(static_cast<size_t>(det.pages), 0);
    det.scannedNoText.assign(static_cast<size_t>(det.pages), 0);
    std::vector<Unit> all;
    for (int p = 0; p < det.pages; ++p) {
        bool sc = false, noText = false;
        unitsOf(d, p, all, sc, noText);
        det.scannedPage[static_cast<size_t>(p)] = sc;
        det.scannedNoText[static_cast<size_t>(p)] = noText;
        if (sc) {   // rules drawn in the scan: no object of their own, only an area of the picture
            const View v = viewOf(d, p);
            for (const Box& b : scanRules(d, p, v)) {
                Unit u;
                u.page = p;
                u.box = b;
                u.type = FPDF_PAGEOBJ_PATH;
                u.scanned = true;
                if (!inBand(u.box, v, u.band)) continue;
                u.paths.push_back({});   // nothing to remove: the picture is whitened there
                all.push_back(u);
            }
        }
    }
    // groups of units that are the same thing in the same place on different pages
    std::vector<std::vector<int>> groups;
    std::vector<int> groupOf(all.size(), -1);
    for (size_t i = 0; i < all.size(); ++i) {
        for (size_t g = 0; g < groups.size() && groupOf[i] < 0; ++g) {
            const Unit& first = all[static_cast<size_t>(groups[g][0])];
            bool pageTaken = false;
            for (int m : groups[g]) if (all[static_cast<size_t>(m)].page == all[i].page) pageTaken = true;
            if (!pageTaken && samePlace(first, all[i])) { groups[g].push_back(static_cast<int>(i)); groupOf[i] = static_cast<int>(g); }
        }
        if (groupOf[i] < 0) { groupOf[i] = static_cast<int>(groups.size()); groups.push_back({static_cast<int>(i)}); }
    }
    // the size of most of the document's text (weighted by how much text there is in each size)
    double bodySize = 0;
    {
        std::map<long, long> bySize;
        for (int p = 0; p < det.pages; ++p) {
            FPDF_PAGE pg = d.page(p);
            if (!pg) continue;
            const int n = api().FPDFPage_CountObjects(pg);
            for (int i = 0; i < n; ++i) {
                FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
                if (api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
                float fs = 0;
                api().FPDFTextObj_GetFontSize(o, &fs);
                FS_MATRIX m;
                if (api().FPDFPageObj_GetMatrix(o, &m)) fs = static_cast<float>(fs * std::sqrt(std::fabs(m.a * m.d - m.b * m.c)));
                float l, b, r, t;
                if (fs > 0 && api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) bySize[std::lround(fs * 2)] += std::lround(r - l);
            }
        }
        long best = 0;
        for (const auto& kv : bySize) if (kv.second > best) { best = kv.second; bodySize = kv.first / 2.0; }
    }
    // page numbers differ from page to page: units of the same pattern in the same place
    std::vector<std::vector<int>> numberGroups;
    {
        std::vector<char> done(all.size(), 0);
        for (size_t i = 0; i < all.size(); ++i) {
            if (done[i] || all[i].type != FPDF_PAGEOBJ_TEXT || numbersIn(all[i].text).empty() && romanValue(all[i].text) <= 0) continue;
            std::vector<int> g{static_cast<int>(i)};
            done[i] = 1;
            for (size_t j = i + 1; j < all.size(); ++j) {
                if (done[j] || all[j].type != FPDF_PAGEOBJ_TEXT || all[j].band != all[i].band || all[j].scanned != all[i].scanned) continue;
                bool pageTaken = false;
                for (int m : g) if (all[static_cast<size_t>(m)].page == all[j].page) pageTaken = true;
                if (pageTaken) continue;
                const Unit& a = all[i]; const Unit& b = all[j];
                const bool yOk = std::fabs(a.box.t - b.box.t) < 6 || std::fabs(a.box.b - b.box.b) < 6;
                std::wstring pa = normalise(a.text), pb = normalise(b.text);
                if (romanValue(a.text) > 0 && romanValue(b.text) > 0) pa = pb = L"roman";
                if (yOk && pa == pb) { g.push_back(static_cast<int>(j)); done[j] = 1; }
            }
            std::vector<const Unit*> us;
            for (int m : g) us.push_back(&all[static_cast<size_t>(m)]);
            if (isPageNumber(us, bodySize)) numberGroups.push_back(g);
        }
    }
    const int pages = det.pages;
    int odd = 0, even = 0;
    for (int p = 0; p < pages; ++p) (p % 2 == 0 ? odd : even)++;   // page index 0 is page 1: odd
    auto repeats = [&](const std::vector<int>& g) {
        std::set<int> ps;
        for (int m : g) ps.insert(all[static_cast<size_t>(m)].page);
        // on three pages at least (two-page documents: both), and on half of them or more; or on nearly every odd
        // (or every even) page and none of the others - twice is a coincidence (a page repeated in the document), not a header
        const int n = static_cast<int>(ps.size());
        if (n < std::min(3, pages) || n < 2) return false;
        if (n * 2 >= pages) return true;
        int o = 0, e = 0;
        for (int p : ps) (p % 2 == 0 ? o : e)++;
        return (e == 0 && o * 10 >= odd * 8 && o >= std::min(3, odd)) || (o == 0 && e * 10 >= even * 8 && e >= std::min(3, even));
    };
    auto addElement = [&](const std::vector<int>& g, Kind kind) {
        Element e;
        e.kind = kind;
        e.band = all[static_cast<size_t>(g[0])].band;
        for (int m : g) {
            const Unit& u = all[static_cast<size_t>(m)];
            for (const auto& path : u.paths) e.items.push_back({u.page, path, u.box, u.scanned, u.text});
        }
        std::set<int> ps;
        for (int m : g) ps.insert(all[static_cast<size_t>(m)].page);
        int o = 0, ev = 0;
        for (int p : ps) (p % 2 == 0 ? o : ev)++;
        const std::wstring where = e.band == Band::Header ? L"top" : L"bottom";
        const Unit& u0 = all[static_cast<size_t>(g[0])];
        switch (kind) {
            case Kind::PageNumber: e.label = L"Page number (" + where + L"), e.g. " + shortText(u0.text); break;
            case Kind::Date: e.label = L"Date (" + where + L") " + shortText(u0.text); break;
            case Kind::Text: e.label = L"Text (" + where + L") " + shortText(u0.text); break;
            case Kind::Line: e.label = L"Line (" + where + L")"; break;
            case Kind::Picture: e.label = L"Picture or logo (" + where + L")"; break;
            case Kind::Shape: e.label = L"Shape (" + where + L")"; break;
            case Kind::Group: e.label = L"Block (" + where + L")"; break;
            case Kind::Area: break;
        }
        // a piece of a page template (a form that also draws the body): PDFium cannot write such a form again without it,
        // so it is shown, but kept
        for (const Item& it : e.items) if (it.path.size() == 2) e.locked = true;
        if (e.locked) { e.selected = false; e.label += L" \x2013 part of a page template, kept"; }
        if (ev == 0 && o > 0 && pages > 2) e.label += L", odd pages";
        else if (o == 0 && ev > 0 && pages > 2) e.label += L", even pages";
        if (u0.scanned) e.label += L", scanned";
        det.elements.push_back(std::move(e));
    };
    std::vector<char> inNumbers(all.size(), 0);
    for (const auto& g : numberGroups) {
        if (!repeats(g)) continue;   // page numbers too must be on enough pages
        addElement(g, Kind::PageNumber);
        for (int m : g) inNumbers[static_cast<size_t>(m)] = 1;
    }
    for (const auto& g : groups) {
        bool numbered = false;
        for (int m : g) if (inNumbers[static_cast<size_t>(m)]) numbered = true;
        if (numbered) continue;
        bool artifact = true;
        for (int m : g) artifact = artifact && all[static_cast<size_t>(m)].artifact;
        if (!repeats(g) && !artifact) continue;   // not repeated: never taken for a header or footer
        const Unit& u = all[static_cast<size_t>(g[0])];
        Kind k = Kind::Shape;
        if (u.type == FPDF_PAGEOBJ_TEXT) k = looksLikeDate(u.text) ? Kind::Date : Kind::Text;
        else if (u.type == FPDF_PAGEOBJ_IMAGE) k = Kind::Picture;
        else if (u.type == FPDF_PAGEOBJ_FORM) k = Kind::Group;
        else if (u.type == FPDF_PAGEOBJ_PATH && std::min(u.box.r - u.box.l, u.box.b - u.box.t) < 4) k = Kind::Line;
        addElement(g, k);
    }
    // headers first, then footers; page numbers first in each
    std::stable_sort(det.elements.begin(), det.elements.end(), [](const Element& a, const Element& b) {
        if (a.band != b.band) return a.band == Band::Header;
        return (a.kind == Kind::PageNumber) > (b.kind == Kind::PageNumber);
    });
    return det;
}

void selectFor(Detection& det, int what) {
    if (what == 4) return;
    for (Element& e : det.elements) {
        if (e.kind == Kind::Area) continue;
        if (e.locked) { e.selected = false; continue; }
        e.selected = what == 2 || (what == 0 && e.band == Band::Header) || (what == 1 && e.band == Band::Footer) || (what == 3 && e.kind == Kind::PageNumber);
    }
}

std::vector<int> choosePages(const Document& d, int which, int current, const std::wstring& spec, std::wstring* why) {
    std::vector<int> out;
    const int n = d.pageCount();
    switch (which) {
        case 0: for (int i = 0; i < n; ++i) out.push_back(i); break;
        case 1: if (current >= 0 && current < n) out.push_back(current); break;
        case 2: for (int i = 0; i < n; i += 2) out.push_back(i); break;
        case 3: for (int i = 1; i < n; i += 2) out.push_back(i); break;
        default:
            out = d.parseRange(spec);
            if (out.empty() && why) *why = L"Type pages like 2,5,8 or 3-15 (or both: 1-3, 7, 9-12), between 1 and " + std::to_wstring(n) + L".";
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

Plan plan(Document& d, const Detection& det, const std::vector<int>& pages) {
    Plan pl;
    for (int page : pages) {
        PagePlan pp;
        pp.page = page;
        const View v = viewOf(d, page);
        for (const Element& e : det.elements) {
            if (!e.selected) continue;
            if (e.kind == Kind::Area) {
                Box a = e.area;
                if (e.fromBottom) { a.t += v.h - e.refHeight; a.b += v.h - e.refHeight; }   // as far from the foot as where it was drawn
                if (det.scannedPage[static_cast<size_t>(page)]) pp.whiten.push_back(a);
                // the objects wholly inside the area go; those across its edge stay (and are counted for the warning)
                if (!v.pg) continue;
                const int n = api().FPDFPage_CountObjects(v.pg);
                for (int i = 0; i < n; ++i) {
                    FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(v.pg, i);
                    float l, b, r, t;
                    if (!api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
                    const Box x = v.map(l, b, r, t);
                    const bool scanPicture = det.scannedPage[static_cast<size_t>(page)] && api().FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_IMAGE;
                    if (scanPicture) continue;   // the scan itself: whitened, not removed
                    const bool inside = x.l >= a.l - 0.5 && x.r <= a.r + 0.5 && x.t >= a.t - 0.5 && x.b <= a.b + 0.5;
                    const bool touches = x.r > a.l && x.l < a.r && x.b > a.t && x.t < a.b;
                    if (inside) {
                        pp.objects.push_back({i});
                        // not part of a header or footer that was found: the area takes in the page's own content
                        bool known = false;
                        for (const Element& o : det.elements)
                            for (const Item& it : o.items) if (it.page == page && it.path.size() == 1 && it.path[0] == i) known = true;
                        if (!known && !det.scannedPage[static_cast<size_t>(page)]) {
                            ++pp.bodyTaken;
                            if (pp.bodySample.empty() && api().FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_TEXT) pp.bodySample = objText(d.text(page), o);
                        }
                    } else if (touches) {
                        // recognised words on a scan under the whitened area go with it when their middle is inside
                        const bool word = det.scannedPage[static_cast<size_t>(page)] && api().FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_TEXT;
                        const double cx = (x.l + x.r) / 2, cy = (x.t + x.b) / 2;
                        if (word && cx > a.l && cx < a.r && cy > a.t && cy < a.b) pp.objects.push_back({i});
                        else ++pp.crossing;
                    }
                }
                continue;
            }
            if (e.locked) continue;
            for (const Item& it : e.items) {
                if (it.page != page) continue;
                if (it.scanned) pp.whiten.push_back({it.box.l - 2, it.box.t - 2, it.box.r + 2, it.box.b + 2});
                if (it.path.empty()) continue;   // a rule drawn in a scan: only whitened
                if (it.path.size() != 1) { ++pp.sharedKept; continue; }   // never inside a form
                pp.objects.push_back(std::vector<int>{it.path[0]});
            }
        }
        std::sort(pp.objects.begin(), pp.objects.end());
        pp.objects.erase(std::unique(pp.objects.begin(), pp.objects.end()), pp.objects.end());
        if (pp.objects.empty() && pp.whiten.empty() && !pp.crossing && !pp.sharedKept && !pp.bodyTaken) continue;
        pl.objects += static_cast<int>(pp.objects.size());
        pl.areas += static_cast<int>(pp.whiten.size());
        pl.crossing += pp.crossing;
        pl.sharedKept += pp.sharedKept;
        pl.bodyTaken += pp.bodyTaken;
        if (pl.bodySample.empty()) pl.bodySample = pp.bodySample;
        pl.pages.push_back(std::move(pp));
    }
    return pl;
}

bool applyPage(Document& d, int index, const PagePlan& pp) {
    FPDF_PAGE pg = d.page(index);
    if (!pg) return false;
    const View v = viewOf(d, index);
    // 1. the scan's picture is made white under the areas (in the picture's own pixels)
    if (!pp.whiten.empty()) {
        const int n = api().FPDFPage_CountObjects(pg);
        for (int i = 0; i < n; ++i) {
            FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
            if (api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE) continue;
            float l, b, r, t;
            if (!api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t) || (r - l) * (t - b) < 0.6 * v.w * v.h) continue;
            FS_MATRIX m;
            if (!api().FPDFPageObj_GetMatrix(o, &m)) continue;
            FPDF_BITMAP src = api().FPDFImageObj_GetBitmap(o);
            if (!src) continue;
            const int bw = api().FPDFBitmap_GetWidth(src), bh = api().FPDFBitmap_GetHeight(src);
            // a copy in 32-bit colour: every bitmap format comes out the same
            FPDF_BITMAP bmp = api().FPDFBitmap_Create(bw, bh, 0);
            api().FPDFBitmap_FillRect(bmp, 0, 0, bw, bh, 0xFFFFFFFF);
            const int fmt = api().FPDFBitmap_GetFormat(src);
            const int sstride = api().FPDFBitmap_GetStride(src), dstride = api().FPDFBitmap_GetStride(bmp);
            const auto* sp = static_cast<const unsigned char*>(api().FPDFBitmap_GetBuffer(src));
            auto* dp = static_cast<unsigned char*>(api().FPDFBitmap_GetBuffer(bmp));
            for (int y = 0; y < bh; ++y)
                for (int x = 0; x < bw; ++x) {
                    unsigned char* q = dp + y * dstride + x * 4;
                    const unsigned char* s = sp + y * sstride;
                    if (fmt == FPDFBitmap_Gray) { q[0] = q[1] = q[2] = s[x]; }
                    else if (fmt == FPDFBitmap_BGR) { q[0] = s[x * 3]; q[1] = s[x * 3 + 1]; q[2] = s[x * 3 + 2]; }
                    else { q[0] = s[x * 4]; q[1] = s[x * 4 + 1]; q[2] = s[x * 4 + 2]; }
                    q[3] = 255;
                }
            api().FPDFBitmap_Destroy(src);
            // page point -> picture pixel: the picture's unit square is mapped by its matrix; pixel rows run from the top
            const double det = m.a * m.d - m.b * m.c;
            if (std::fabs(det) < 1e-9) { api().FPDFBitmap_Destroy(bmp); continue; }
            for (const Box& a : pp.whiten) {
                double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
                const double dx[4] = {a.l, a.r, a.l, a.r}, dy[4] = {a.t, a.t, a.b, a.b};
                for (int k = 0; k < 4; ++k) {
                    double px, py;
                    v.unmap(dx[k], dy[k], px, py);
                    const double x = px - m.e, y = py - m.f;
                    const double u = (m.d * x - m.c * y) / det, w = (-m.b * x + m.a * y) / det;   // in the unit square
                    minX = std::min(minX, u * bw); maxX = std::max(maxX, u * bw);
                    minY = std::min(minY, (1 - w) * bh); maxY = std::max(maxY, (1 - w) * bh);
                }
                const int x0 = std::max(0, static_cast<int>(std::floor(minX))), x1 = std::min(bw, static_cast<int>(std::ceil(maxX)));
                const int y0 = std::max(0, static_cast<int>(std::floor(minY))), y1 = std::min(bh, static_cast<int>(std::ceil(maxY)));
                if (x1 > x0 && y1 > y0) api().FPDFBitmap_FillRect(bmp, x0, y0, x1 - x0, y1 - y0, 0xFFFFFFFF);
            }
            FPDF_PAGE pages[1] = {pg};
            api().FPDFImageObj_SetBitmap(pages, 1, o, bmp);
            api().FPDFBitmap_Destroy(bmp);
        }
    }
    // 2. the objects go (last first, so the other indices hold; pieces inside forms before their form's index changes)
    std::vector<std::vector<int>> objs = pp.objects;
    std::sort(objs.begin(), objs.end(), [](const std::vector<int>& a, const std::vector<int>& b) { return a > b; });
    for (const auto& path : objs) {
        FPDF_PAGEOBJECT o = path.empty() ? nullptr : api().FPDFPage_GetObject(pg, path[0]);
        if (!o) continue;
        if (path.size() == 1) {
            if (api().FPDFPage_RemoveObject(pg, o)) api().FPDFPageObj_Destroy(o);
        } else {
            FPDF_PAGEOBJECT c = api().FPDFFormObj_GetObject(o, static_cast<unsigned long>(path[1]));
            if (c && api().FPDFFormObj_RemoveObject(o, c)) api().FPDFPageObj_Destroy(c);
        }
    }
    contentgen::generate(d.handle(), pg);
    d.resetText(index);
    return true;
}

bool apply(Document& d, const Plan& p, std::wstring* why) {
    if (p.pages.empty()) { if (why) *why = L"Nothing is chosen to delete on these pages."; return false; }
    d.beginChange();
    for (const PagePlan& pp : p.pages) {
        if (!applyPage(d, pp.page, pp)) {
            d.rollbackChange();
            if (why) *why = L"Page " + std::to_wstring(pp.page + 1) + L" could not be changed.";
            return false;
        }
    }
    return true;
}

} // namespace headfoot
