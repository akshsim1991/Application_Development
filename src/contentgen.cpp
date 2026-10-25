#include "contentgen.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace contentgen {
namespace {

struct Letter { wchar_t ch; double x, y; };

struct Style {   // one text object's way of drawing letters
    FPDF_FONT font;
    float size;
    FS_MATRIX m;
    bool operator<(const Style& o) const {
        return std::tie(font, size, m.a, m.b, m.c, m.d) < std::tie(o.font, o.size, o.m.a, o.m.b, o.m.c, o.m.d);
    }
};

FPDF_PAGEOBJECT make(FPDF_DOCUMENT doc, const Style& s, const std::wstring& text, double e, double f) {
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT o = api.FPDFPageObj_CreateTextObj(doc, s.font, s.size);
    if (!o) return nullptr;
    std::vector<unsigned short> w(text.begin(), text.end());
    w.push_back(0);
    if (!api.FPDFText_SetText(o, w.data())) { api.FPDFPageObj_Destroy(o); return nullptr; }
    const FS_MATRIX m{s.m.a, s.m.b, s.m.c, s.m.d, static_cast<float>(e), static_cast<float>(f)};
    api.FPDFPageObj_SetMatrix(o, &m);
    return o;
}

// How far `text` reaches along the line from its origin (to the right edge of the last letter's box); -1 when unknown.
double reach(FPDF_DOCUMENT doc, const Style& s, const std::wstring& text) {
    FPDF_PAGEOBJECT o = make(doc, s, text, 0, 0);
    if (!o) return -1;
    float l, b, r, t;
    double w = -1;
    if (pdfium::api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) {
        const double n = std::hypot(s.m.a, s.m.b), ux = n > 0 ? s.m.a / n : 1, uy = n > 0 ? s.m.b / n : 0;
        w = std::max({l * ux + b * uy, r * ux + b * uy, l * ux + t * uy, r * ux + t * uy});
    }
    pdfium::api().FPDFPageObj_Destroy(o);
    return w;
}

// Splits one text object if it needs it. Returns false when the page is unchanged.
bool split(FPDF_DOCUMENT doc, FPDF_PAGE page, FPDF_PAGEOBJECT obj, const std::vector<Letter>& letters,
           std::map<std::pair<Style, std::wstring>, double>& advances, bool plainText) {
    const auto& api = pdfium::api();
    Style s{api.FPDFTextObj_GetFont(obj), 0, {1, 0, 0, 1, 0, 0}};
    if (!s.font || !api.FPDFTextObj_GetFontSize(obj, &s.size) || !api.FPDFPageObj_GetMatrix(obj, &s.m)) return false;
    if (FPDF_CLIPPATH cp = api.FPDFPageObj_GetClipPath(obj); cp && api.FPDFClipPath_CountPaths(cp) > 0) return false;
    const double un = std::hypot(s.m.a, s.m.b), vn = std::hypot(s.m.c, s.m.d);
    if (un < 1e-6 || vn < 1e-6) return false;
    const double ux = s.m.a / un, uy = s.m.b / un, vx = s.m.c / vn, vy = s.m.d / vn;
    wchar_t probe = 0;
    for (const auto& l : letters) if (l.ch > L' ') { probe = l.ch; break; }
    if (!probe) return false;
    auto advance = [&](wchar_t ch) {   // where the next letter starts, from this letter's origin
        auto key = std::make_pair(s, std::wstring(1, ch));
        auto it = advances.find(key);
        if (it != advances.end()) return it->second;
        const std::wstring p(1, probe);
        const double a = reach(doc, s, ch == L' ' ? p + L" " + p : std::wstring(2, ch));
        const double b = reach(doc, s, ch == L' ' ? p + p : std::wstring(1, ch));
        const double adv = a >= 0 && b >= 0 ? a - b : -1;
        advances[key] = adv;
        return adv;
    };
    const double tol = 0.05;   // points
    // How far each letter is from where its predecessor's advance puts it. Kerning (numbers in a TJ array) is written
    // again by PDFium and needs nothing; word spacing (after spaces) and letter spacing (after every letter) are lost.
    std::vector<double> dev(letters.size(), 0);
    std::vector<double> afterSpace, afterLetter;
    for (size_t k = 1; k < letters.size(); ++k) {
        const double adv = advance(letters[k - 1].ch);
        if (adv < 0) return false;
        const double dx = letters[k].x - letters[k - 1].x, dy = letters[k].y - letters[k - 1].y;
        const double along = dx * ux + dy * uy, across = dx * vx + dy * vy;
        if (std::fabs(along) < 1e-3 && letters[k - 1].ch > L' ') return false;   // two letters drawn by one glyph (a ligature)
        dev[k] = std::fabs(across) > tol ? 1e9 : along - adv;
        (letters[k - 1].ch == L' ' ? afterSpace : afterLetter).push_back(dev[k]);
    }
    auto median = [](std::vector<double> v) { if (v.empty()) return 0.0; std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end()); return v[v.size() / 2]; };
    const double letterSpacing = median(afterLetter), wordSpacing = median(afterSpace) - letterSpacing;
    // small spacing adds up along a line: what matters is how far the last letter would move
    const double drift = std::fabs(letterSpacing) * static_cast<double>(afterLetter.size() + afterSpace.size()) +
                         std::fabs(wordSpacing) * static_cast<double>(afterSpace.size());
    bool kerned = false;
    for (double d : dev) if (std::fabs(d) > 0.003) kerned = true;
    if (plainText ? !kerned : drift <= tol) return false;   // nothing that would be lost
    // a new piece wherever a letter is not where a plain string would put it (also at kerning, as pieces have none);
    // letter positions are exact to far better than this
    const double eps = 0.003;
    std::vector<size_t> starts{0};
    for (size_t k = 1; k < letters.size(); ++k) if (std::fabs(dev[k]) > eps) starts.push_back(k);
    // a piece of spaces only draws nothing (and reads as empty text): it stays with the piece before it
    for (size_t si = starts.size(); si-- > 1;) {
        const size_t a = starts[si], b = si + 1 < starts.size() ? starts[si + 1] : letters.size();
        bool blank = true;
        for (size_t k = a; k < b && blank; ++k) blank = letters[k].ch == L' ';
        if (blank) starts.erase(starts.begin() + static_cast<std::ptrdiff_t>(si));
    }
    // the object's place in the page, so the pieces are drawn at the same depth
    const int n = api.FPDFPage_CountObjects(page);
    int index = -1;
    for (int i = 0; i < n; ++i) if (api.FPDFPage_GetObject(page, i) == obj) { index = i; break; }
    if (index < 0) return false;   // inside a form object: not written again, nothing to do
    unsigned fr = 0, fg = 0, fb = 0, fa = 255, sr = 0, sg = 0, sb = 0, sa = 255;
    api.FPDFPageObj_GetFillColor(obj, &fr, &fg, &fb, &fa);
    const bool hasStroke = api.FPDFPageObj_GetStrokeColor(obj, &sr, &sg, &sb, &sa) != 0;
    float sw = 0;
    const bool hasWidth = api.FPDFPageObj_GetStrokeWidth(obj, &sw) != 0;
    const int mode = static_cast<int>(api.FPDFTextObj_GetTextRenderMode(obj));
    std::vector<FPDF_PAGEOBJECT> pieces;
    for (size_t si = 0; si < starts.size(); ++si) {
        const size_t a = starts[si], b = si + 1 < starts.size() ? starts[si + 1] : letters.size();
        std::wstring text;
        for (size_t k = a; k < b; ++k) text += letters[k].ch;
        FPDF_PAGEOBJECT o = make(doc, s, text, letters[a].x, letters[a].y);
        if (!o) { for (auto p : pieces) api.FPDFPageObj_Destroy(p); return false; }
        api.FPDFPageObj_SetFillColor(o, fr, fg, fb, fa);
        if (hasStroke) api.FPDFPageObj_SetStrokeColor(o, sr, sg, sb, sa);
        if (hasWidth) api.FPDFPageObj_SetStrokeWidth(o, sw);
        if (mode >= 0) api.FPDFTextObj_SetTextRenderMode(o, static_cast<FPDF_TEXT_RENDERMODE>(mode));
        pieces.push_back(o);
    }
    for (size_t i = 0; i < pieces.size(); ++i) api.FPDFPage_InsertObjectAtIndex(page, pieces[i], static_cast<size_t>(index) + i);
    if (api.FPDFPage_RemoveObject(page, obj)) api.FPDFPageObj_Destroy(obj);
    return true;
}

} // namespace

bool prepare(FPDF_DOCUMENT doc, FPDF_PAGE page, bool plainText) {
    const auto& api = pdfium::api();
    bool changed = false;
    if (FPDF_TEXTPAGE tp = doc && page ? api.FPDFText_LoadPage(page) : nullptr) {
        // the letters of each text object, in order, where they are drawn now
        std::vector<FPDF_PAGEOBJECT> order;
        std::map<FPDF_PAGEOBJECT, std::vector<Letter>> letters;
        std::map<FPDF_PAGEOBJECT, bool> unsafe;
        const int n = api.FPDFText_CountChars(tp);
        for (int i = 0; i < n; ++i) {
            if (api.FPDFText_IsGenerated(tp, i) == 1) continue;   // a space or line break PDFium adds between objects
            FPDF_PAGEOBJECT o = api.FPDFText_GetTextObject(tp, i);
            if (!o) continue;
            const unsigned ch = api.FPDFText_GetUnicode(tp, i);
            double x = 0, y = 0;
            if (!letters.count(o)) order.push_back(o);
            auto& v = letters[o];
            if (ch == 0 || ch > 0xFFFF || (ch >= 0xD800 && ch <= 0xDFFF) || api.FPDFText_HasUnicodeMapError(tp, i) == 1 ||
                !api.FPDFText_GetCharOrigin(tp, i, &x, &y)) { unsafe[o] = true; continue; }
            if (ch == L'\r' || ch == L'\n') continue;
            v.push_back({static_cast<wchar_t>(ch), x, y});
        }
        std::map<std::pair<Style, std::wstring>, double> advances;
        for (FPDF_PAGEOBJECT o : order) {
            const auto& v = letters[o];
            if (unsafe[o] || v.empty()) continue;
            // every letter of the object must be there, in order: the pieces are made from these letters (spaces draw
            // nothing, and PDFium reads spaces into kerning gaps and object ends that are not in the object)
            const unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
            if (bytes < 2) continue;
            std::vector<unsigned short> buf(bytes / 2 + 1);
            api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
            std::wstring own, got;
            for (size_t i = 0; i + 1 < bytes / 2; ++i) if (buf[i] > L' ') own += static_cast<wchar_t>(buf[i]);
            for (const auto& l : v) if (l.ch > L' ') got += l.ch;
            if (own != got) continue;
            changed |= split(doc, page, o, v, advances, plainText);
        }
        api.FPDFText_ClosePage(tp);
    }
    return changed;
}

bool generate(FPDF_DOCUMENT doc, FPDF_PAGE page) {
    if (!page) return false;
    prepare(doc, page, false);
    return pdfium::api().FPDFPage_GenerateContent(page) != 0;
}

} // namespace contentgen
