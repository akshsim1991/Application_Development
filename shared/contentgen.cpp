#include "contentgen.hpp"
#include "document.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
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

namespace {

// ---- drawing a page to compare what is held in memory with what was written
struct Shot {
    int w = 0, h = 0;
    std::vector<uint8_t> px;   // BGRA
};

constexpr double kCheckPixels = 1100;   // the longer side of the drawings compared

double checkScale(FPDF_PAGE pg) {
    const auto& api = pdfium::api();
    const double w = api.FPDF_GetPageWidthF(pg), h = api.FPDF_GetPageHeightF(pg);
    const double m = std::max(w, h);
    return m > 0 ? kCheckPixels / m : 1;
}

Shot draw(FPDF_PAGE pg, double scale) {
    const auto& api = pdfium::api();
    Shot s;
    s.w = std::max(1, static_cast<int>(std::lround(api.FPDF_GetPageWidthF(pg) * scale)));
    s.h = std::max(1, static_cast<int>(std::lround(api.FPDF_GetPageHeightF(pg) * scale)));
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(s.w, s.h, FPDFBitmap_BGRA, nullptr, 0);
    if (!bmp) { s.w = s.h = 0; return s; }
    api.FPDFBitmap_FillRect(bmp, 0, 0, s.w, s.h, 0xFFFFFFFF);
    api.FPDF_RenderPageBitmap(bmp, pg, 0, 0, s.w, s.h, 0, 0);
    const auto* buf = static_cast<const uint8_t*>(api.FPDFBitmap_GetBuffer(bmp));
    const int stride = api.FPDFBitmap_GetStride(bmp);
    s.px.resize(static_cast<size_t>(s.w) * s.h * 4);
    for (int y = 0; y < s.h; ++y) std::memcpy(&s.px[static_cast<size_t>(y) * s.w * 4], buf + static_cast<size_t>(y) * stride, static_cast<size_t>(s.w) * 4);
    api.FPDFBitmap_Destroy(bmp);
    return s;
}

// Each pixel the average of the 3 x 3 around it: the same drawing smoothed differently (a picture of an object shown
// smaller, letters a fraction of a pixel apart) then reads the same; a missing fill, letter or hairline does not.
Shot softened(const Shot& s) {
    Shot o = s;
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x) {
            int sum[3] = {0, 0, 0}, n = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= s.w || yy >= s.h) continue;
                    const uint8_t* c = &s.px[(static_cast<size_t>(yy) * s.w + xx) * 4];
                    for (int k = 0; k < 3; ++k) sum[k] += c[k];
                    ++n;
                }
            uint8_t* d = &o.px[(static_cast<size_t>(y) * s.w + x) * 4];
            for (int k = 0; k < 3; ++k) d[k] = static_cast<uint8_t>(sum[k] / n);
        }
    return o;
}

// The pixels that differ clearly: in the softened drawings, a pixel counts when no pixel next to it (or itself) in the
// other drawing is close to it.
std::vector<uint8_t> differences(const Shot& a0, const Shot& b0, int& count) {
    count = 0;
    std::vector<uint8_t> m;
    if (a0.w != b0.w || a0.h != b0.h || a0.px.empty()) { count = -1; return m; }
    if (a0.px == b0.px) { m.assign(static_cast<size_t>(a0.w) * a0.h, 0); return m; }
    const Shot a = softened(a0), b = softened(b0);
    const int w = a.w, h = a.h;
    m.assign(static_cast<size_t>(w) * h, 0);
    auto unmatched = [&](const Shot& p, const Shot& q, int x, int y) {   // p's pixel has nothing close to it around q's
        const uint8_t* c = &p.px[(static_cast<size_t>(y) * w + x) * 4];
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int xx = x + dx, yy = y + dy;
                if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                const uint8_t* e = &q.px[(static_cast<size_t>(yy) * w + xx) * 4];
                if (std::abs(c[0] - e[0]) <= 28 && std::abs(c[1] - e[1]) <= 28 && std::abs(c[2] - e[2]) <= 28) return false;
            }
        return true;
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (std::memcmp(&a.px[i * 4], &b.px[i * 4], 3) == 0) continue;
            if (unmatched(a, b, x, y) || unmatched(b, a, x, y)) { m[i] = 1; ++count; }
        }
    return m;
}

// The page as it was written: a copy of it, read again from the document's objects, drawn.
Shot drawWritten(Document& d, int index, double scale) {
    const auto& api = pdfium::api();
    Shot s;
    FPDF_DOCUMENT tmp = api.FPDF_CreateNewDocument();
    if (!tmp) return s;
    if (api.FPDF_ImportPagesByIndex(tmp, d.handle(), &index, 1, 0)) {
        if (FPDF_PAGE tp = api.FPDF_LoadPage(tmp, 0)) { s = draw(tp, scale); api.FPDF_ClosePage(tp); }
    }
    api.FPDF_CloseDocument(tmp);
    return s;
}

struct DevBox { int l, t, r, b; };

DevBox deviceBox(FPDF_PAGE pg, const Shot& s, double l, double b, double r, double t) {
    const auto& api = pdfium::api();
    int xs[4], ys[4];
    const double px[4] = {l, r, l, r}, py[4] = {b, b, t, t};
    for (int i = 0; i < 4; ++i) api.FPDF_PageToDevice(pg, 0, 0, s.w, s.h, 0, px[i], py[i], &xs[i], &ys[i]);
    return {*std::min_element(xs, xs + 4), *std::min_element(ys, ys + 4), *std::max_element(xs, xs + 4), *std::max_element(ys, ys + 4)};
}

bool touches(const std::vector<uint8_t>& mask, const Shot& s, DevBox bx) {
    bx.l = std::max(0, bx.l - 2); bx.t = std::max(0, bx.t - 2);
    bx.r = std::min(s.w - 1, bx.r + 2); bx.b = std::min(s.h - 1, bx.b + 2);
    for (int y = bx.t; y <= bx.b; ++y)
        for (int x = bx.l; x <= bx.r; ++x) if (mask[static_cast<size_t>(y) * s.w + x]) return true;
    return false;
}

// Replaces the object at `index` by a picture of it alone, at about 300 dots per inch, in the same place and depth.
bool asPicture(FPDF_DOCUMENT doc, FPDF_PAGE pg, int index) {
    const auto& api = pdfium::api();
    FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, index);
    float l, b, r, t;
    if (!o || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) return false;
    // what is drawn is only within the page's shown area
    const double pw = api.FPDF_GetPageWidthF(pg), ph = api.FPDF_GetPageHeightF(pg);
    double scale = 300.0 / 72.0;
    // the page drawn whole at `scale`; only the object's box of it is made
    Shot full;
    full.w = static_cast<int>(std::lround(pw * scale)); full.h = static_cast<int>(std::lround(ph * scale));
    DevBox bx = deviceBox(pg, full, l - 1, b - 1, r + 1, t + 1);
    bx.l = std::max(0, bx.l); bx.t = std::max(0, bx.t); bx.r = std::min(full.w, bx.r); bx.b = std::min(full.h, bx.b);
    double area = static_cast<double>(bx.r - bx.l) * (bx.b - bx.t);
    if (area <= 0) return false;
    if (area > 24e6) {   // a very large object: fewer dots
        const double k = std::sqrt(24e6 / area);
        scale *= k;
        full.w = static_cast<int>(std::lround(pw * scale)); full.h = static_cast<int>(std::lround(ph * scale));
        bx = deviceBox(pg, full, l - 1, b - 1, r + 1, t + 1);
        bx.l = std::max(0, bx.l); bx.t = std::max(0, bx.t); bx.r = std::min(full.w, bx.r); bx.b = std::min(full.h, bx.b);
    }
    const int w = bx.r - bx.l, h = bx.b - bx.t;
    if (w <= 0 || h <= 0) return false;
    // only this object is drawn
    const int n = api.FPDFPage_CountObjects(pg);
    std::vector<FPDF_PAGEOBJECT> others;
    for (int i = 0; i < n; ++i) if (i != index) if (FPDF_PAGEOBJECT x = api.FPDFPage_GetObject(pg, i)) { api.FPDFPageObj_SetIsActive(x, false); others.push_back(x); }
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, nullptr, 0);
    if (bmp) {
        api.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0x00000000);
        api.FPDF_RenderPageBitmap(bmp, pg, -bx.l, -bx.t, full.w, full.h, 0, 0);
    }
    for (auto x : others) api.FPDFPageObj_SetIsActive(x, true);
    if (!bmp) return false;
    // Pixels left fully transparent are black; when the picture is scaled to be shown, that black would darken its edges.
    // They take the colour of the drawn pixels next to them (a few pixels deep), staying transparent.
    {
        auto* px = static_cast<uint8_t*>(api.FPDFBitmap_GetBuffer(bmp));
        const int stride = api.FPDFBitmap_GetStride(bmp);
        std::vector<uint8_t> done(static_cast<size_t>(w) * h, 0);
        std::vector<int> frontier;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) if (px[y * stride + x * 4 + 3]) done[static_cast<size_t>(y) * w + x] = 1;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                if (done[static_cast<size_t>(y) * w + x]) continue;
                bool edge = false;
                for (int k = 0; k < 4 && !edge; ++k) {
                    const int xx = x + (k == 0) - (k == 1), yy = y + (k == 2) - (k == 3);
                    edge = xx >= 0 && yy >= 0 && xx < w && yy < h && done[static_cast<size_t>(yy) * w + xx] == 1;
                }
                if (edge) frontier.push_back(y * w + x);
            }
        for (int pass = 0; pass < 6 && !frontier.empty(); ++pass) {
            std::vector<int> next;
            for (int id : frontier) {
                const int x = id % w, y = id / w;
                if (done[static_cast<size_t>(id)]) continue;
                int sum[3] = {0, 0, 0}, n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h || !done[static_cast<size_t>(yy) * w + xx] || done[static_cast<size_t>(yy) * w + xx] == pass + 2) continue;
                        for (int c = 0; c < 3; ++c) sum[c] += px[yy * stride + xx * 4 + c];
                        ++n;
                    }
                if (!n) continue;
                for (int c = 0; c < 3; ++c) px[y * stride + x * 4 + c] = static_cast<uint8_t>(sum[c] / n);
                done[static_cast<size_t>(id)] = static_cast<uint8_t>(pass + 2);   // filled in this pass
                for (int k = 0; k < 4; ++k) {
                    const int xx = x + (k == 0) - (k == 1), yy = y + (k == 2) - (k == 3);
                    if (xx >= 0 && yy >= 0 && xx < w && yy < h && !done[static_cast<size_t>(yy) * w + xx]) next.push_back(yy * w + xx);
                }
            }
            frontier.swap(next);
        }
    }
    FPDF_PAGEOBJECT img = api.FPDFPageObj_NewImageObj(doc);
    bool ok = img && api.FPDFImageObj_SetBitmap(nullptr, 0, img, bmp);
    api.FPDFBitmap_Destroy(bmp);
    if (!ok) { if (img) api.FPDFPageObj_Destroy(img); return false; }
    // the picture's corners on the page
    double x0, y0, x1, y1;
    api.FPDF_DeviceToPage(pg, 0, 0, full.w, full.h, 0, bx.l, bx.b, &x0, &y0);   // bottom left
    api.FPDF_DeviceToPage(pg, 0, 0, full.w, full.h, 0, bx.r, bx.t, &x1, &y1);   // top right
    const FS_MATRIX m{static_cast<float>(x1 - x0), 0, 0, static_cast<float>(y1 - y0), static_cast<float>(x0), static_cast<float>(y0)};
    api.FPDFPageObj_AddMark(img, "FeatherDrawing");
    if (!api.FPDFPageObj_SetMatrix(img, &m) || !api.FPDFPage_InsertObjectAtIndex(pg, img, static_cast<size_t>(index))) {
        api.FPDFPageObj_Destroy(img);
        return false;
    }
    if (api.FPDFPage_RemoveObject(pg, o)) api.FPDFPageObj_Destroy(o);
    return true;
}

// Only the objects in `on` are drawn and written (nullptr: all of them).
void activate(const std::vector<FPDF_PAGEOBJECT>& all, const std::vector<FPDF_PAGEOBJECT>* on) {
    const auto& api = pdfium::api();
    for (auto o : all) api.FPDFPageObj_SetIsActive(o, on == nullptr);
    if (on) for (auto o : *on) api.FPDFPageObj_SetIsActive(o, true);
}

// Whether the objects of `group`, written alone, read back as they are drawn now.
bool writtenFaithfully(Document& d, FPDF_PAGE pg, int index, double scale, const std::vector<FPDF_PAGEOBJECT>& all,
                       const std::vector<FPDF_PAGEOBJECT>& group) {
    activate(all, &group);
    const Shot want = draw(pg, scale);
    bool same = false;
    if (pdfium::api().FPDFPage_GenerateContent(pg)) {
        int count = 0;
        differences(want, drawWritten(d, index, scale), count);
        same = count >= 0 && count <= 4;
    }
    activate(all, nullptr);
    return same;
}

// The objects of `group` that PDFium cannot write: halves are tried until each is found alone.
void findUnwritable(Document& d, FPDF_PAGE pg, int index, double scale, const std::vector<FPDF_PAGEOBJECT>& all,
                    const std::vector<FPDF_PAGEOBJECT>& group, std::vector<FPDF_PAGEOBJECT>& out, int& budget) {
    if (group.empty() || budget <= 0) return;
    --budget;
    if (writtenFaithfully(d, pg, index, scale, all, group)) return;
    if (group.size() == 1) { out.push_back(group[0]); return; }
    const auto mid = group.begin() + static_cast<std::ptrdiff_t>(group.size() / 2);
    findUnwritable(d, pg, index, scale, all, std::vector<FPDF_PAGEOBJECT>(group.begin(), mid), out, budget);
    findUnwritable(d, pg, index, scale, all, std::vector<FPDF_PAGEOBJECT>(mid, group.end()), out, budget);
}

} // namespace

bool drawnAsPicture(FPDF_PAGEOBJECT o) {
    const auto& api = pdfium::api();
    if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE) return false;
    const int marks = api.FPDFPageObj_CountMarks(o);
    for (int i = 0; i < marks; ++i) {
        FPDF_PAGEOBJECTMARK m = api.FPDFPageObj_GetMark(o, i);
        unsigned long n = 0;
        if (!m || !api.FPDFPageObjMark_GetName(m, nullptr, 0, &n) || n < 2) continue;
        std::vector<unsigned short> buf(n / 2 + 1);
        api.FPDFPageObjMark_GetName(m, buf.data(), n, &n);
        if (std::wstring(buf.begin(), buf.begin() + (n / 2 - 1)) == L"FeatherDrawing") return true;
    }
    return false;
}

bool generate(Document& d, FPDF_PAGE page) {
    if (!page) return false;
    const auto& api = pdfium::api();
    prepare(d.handle(), page, false);
    const int index = d.indexOfPage(page);
    if (index < 0) return api.FPDFPage_GenerateContent(page) != 0;   // not one of the document's pages: unchecked
    const double scale = checkScale(page);
    const Shot want = draw(page, scale);   // the page as it should be: its objects as held in memory
    for (int round = 0; round < 2; ++round) {
        if (!api.FPDFPage_GenerateContent(page)) return false;
        int count = 0;
        const Shot got = drawWritten(d, index, scale);
        const std::vector<uint8_t> mask = differences(want, got, count);
        if (const char* f = std::getenv("FEATHER_TRACE_DIFF")) {
            for (int k = 0; k < 2; ++k) {
                const Shot& sh = k ? got : want;
                const std::string path = std::string(f) + std::to_string(round) + (k ? "got" : "want") + ".ppm";
                if (FILE* fp = std::fopen(path.c_str(), "wb")) {
                    std::fprintf(fp, "P6 %d %d 255\n", sh.w, sh.h);
                    for (size_t i = 0; i + 3 < sh.px.size(); i += 4) { std::fputc(sh.px[i + 2], fp); std::fputc(sh.px[i + 1], fp); std::fputc(sh.px[i], fp); }
                    std::fclose(fp);
                }
            }
        }
        if (count < 0) return true;   // could not be checked (the copy failed): written as before
        if (std::getenv("FEATHER_TRACE_WRITE")) {
            int x0 = want.w, y0 = want.h, x1 = -1, y1 = -1;
            for (int y = 0; y < want.h && count > 0; ++y)
                for (int x = 0; x < want.w; ++x) if (mask[static_cast<size_t>(y) * want.w + x]) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
            std::fprintf(stderr, "contentgen: page %d round %d differs %d (pixels %d,%d-%d,%d of %dx%d)\n", index, round, count, x0, y0, x1, y1, want.w, want.h);
            if (const char* f = std::getenv("FEATHER_TRACE_DIFF")) {
                const std::string path = std::string(f) + std::to_string(round) + ".pgm";
                if (FILE* fp = std::fopen(path.c_str(), "wb")) {
                    std::fprintf(fp, "P5 %d %d 255\n", want.w, want.h);
                    for (size_t i = 0; i < mask.size(); ++i) std::fputc(mask[i] ? 0 : 255, fp);
                    std::fclose(fp);
                }
            }
        }
        if (count <= 4) return true;
        if (round == 1) return false;
        // the objects drawn where the page differs; those of them PDFium cannot write become pictures of themselves
        std::vector<FPDF_PAGEOBJECT> all, there;
        for (int i = 0; i < api.FPDFPage_CountObjects(page); ++i) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(page, i);
            if (!o) continue;
            all.push_back(o);
            float l, b, r, t;
            if (api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) && touches(mask, want, deviceBox(page, want, l, b, r, t))) there.push_back(o);
        }
        std::vector<FPDF_PAGEOBJECT> bad;
        int budget = 400;   // tries; plenty for a few unwritable objects among many
        findUnwritable(d, page, index, scale, all, there, bad, budget);
        if (std::getenv("FEATHER_TRACE_WRITE")) std::fprintf(stderr, "contentgen: page %d: %zu of %zu objects there become pictures\n", index, bad.size(), there.size());
        if (bad.empty()) return false;
        for (int i = api.FPDFPage_CountObjects(page) - 1; i >= 0; --i)   // topmost first: the indices below stay valid
            if (std::find(bad.begin(), bad.end(), api.FPDFPage_GetObject(page, i)) != bad.end()) asPicture(d.handle(), page, i);
    }
    return false;
}

} // namespace contentgen
