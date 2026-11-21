#include "tables.hpp"
#include "contentgen.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>
#include <set>

namespace tables {
namespace {

const pdfium::Api& api() { return pdfium::api(); }

// ---------------------------------------------------------------- what is drawn

// A straight line along x (horizontal: from a to b at y = at) or along y (vertical: from a to b at x = at).
struct Seg {
    double a, b, at, width;
    COLORREF color;
    std::vector<int> objs;            // the page objects it comes from (-1: a line of a scanned picture)
};
struct Box { double l, b, r, t; COLORREF fill; int obj; };

void apply(const FS_MATRIX& m, double x, double y, double& ox, double& oy) {
    ox = m.a * x + m.c * y + m.e;
    oy = m.b * x + m.d * y + m.f;
}

// Lines and filled boxes of the page's path objects. A filled box thinner than 2.5 points is a line.
void collectPaths(FPDF_PAGE pg, std::vector<Seg>& hs, std::vector<Seg>& vs, std::vector<Box>& fills) {
    const int n = api().FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
        if (!o || api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_PATH) continue;
        int fillMode = 0;
        FPDF_BOOL stroke = 0;
        if (!api().FPDFPath_GetDrawMode(o, &fillMode, &stroke)) continue;
        FS_MATRIX m{1, 0, 0, 1, 0, 0};
        api().FPDFPageObj_GetMatrix(o, &m);
        if (std::fabs(m.b) > 1e-3 || std::fabs(m.c) > 1e-3) continue;   // turned: not a table of this page
        float sw = 1;
        api().FPDFPageObj_GetStrokeWidth(o, &sw);
        const double width = std::max(0.1, sw * std::sqrt(std::fabs(static_cast<double>(m.a) * m.d)));
        unsigned R = 0, G = 0, B = 0, A = 255;
        api().FPDFPageObj_GetStrokeColor(o, &R, &G, &B, &A);
        const COLORREF strokeColor = RGB(R, G, B);
        api().FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
        const COLORREF fillColor = A < 30 ? kNoFill : RGB(R, G, B);
        // the subpaths, as polylines; curves make a subpath unusable (rounded boxes are not table cells)
        struct Sub { std::vector<std::pair<double, double>> pts; bool closed = false, curved = false; };
        std::vector<Sub> subs;
        const int segs = api().FPDFPath_CountSegments(o);
        for (int k = 0; k < segs; ++k) {
            FPDF_PATHSEGMENT s = api().FPDFPath_GetPathSegment(o, k);
            if (!s) continue;
            float x = 0, y = 0;
            api().FPDFPathSegment_GetPoint(s, &x, &y);
            double px, py;
            apply(m, x, y, px, py);
            const int type = api().FPDFPathSegment_GetType(s);
            if (type == FPDF_SEGMENT_MOVETO || subs.empty()) subs.push_back({});
            if (type == FPDF_SEGMENT_BEZIERTO) subs.back().curved = true;
            subs.back().pts.push_back({px, py});
            if (api().FPDFPathSegment_GetClose(s)) subs.back().closed = true;
        }
        for (Sub& sub : subs) {
            if (sub.curved || sub.pts.size() < 2) continue;
            if (sub.closed && (sub.pts.front().first != sub.pts.back().first || sub.pts.front().second != sub.pts.back().second)) sub.pts.push_back(sub.pts.front());
            // an axis-aligned box?
            double l = 1e9, b = 1e9, r = -1e9, t = -1e9;
            bool axis = true;
            for (size_t k = 0; k < sub.pts.size(); ++k) {
                l = std::min(l, sub.pts[k].first); r = std::max(r, sub.pts[k].first);
                b = std::min(b, sub.pts[k].second); t = std::max(t, sub.pts[k].second);
                if (k && std::fabs(sub.pts[k].first - sub.pts[k - 1].first) > 0.3 && std::fabs(sub.pts[k].second - sub.pts[k - 1].second) > 0.3) axis = false;
            }
            const bool box = axis && sub.pts.size() >= 4 && sub.pts.size() <= 6;
            if (fillMode != 0 && box && fillColor != kNoFill) {
                const double w = r - l, h = t - b;
                if (h <= 2.5 && w > 3) { hs.push_back({l, r, (b + t) / 2, std::max(0.1, h), fillColor, {i}}); continue; }
                if (w <= 2.5 && h > 3) { vs.push_back({b, t, (l + r) / 2, std::max(0.1, w), fillColor, {i}}); continue; }
                if (w > 3 && h > 3) {
                    fills.push_back({l, b, r, t, fillColor, i});
                    if (!stroke) continue;   // a filled box without a frame: a cell colour, its edges are not lines
                }
            }
            if (!stroke) continue;
            for (size_t k = 1; k < sub.pts.size(); ++k) {
                const double x0 = sub.pts[k - 1].first, y0 = sub.pts[k - 1].second, x1 = sub.pts[k].first, y1 = sub.pts[k].second;
                if (std::fabs(y1 - y0) < 0.6 && std::fabs(x1 - x0) > 1.5) hs.push_back({std::min(x0, x1), std::max(x0, x1), (y0 + y1) / 2, width, strokeColor, {i}});
                else if (std::fabs(x1 - x0) < 0.6 && std::fabs(y1 - y0) > 1.5) vs.push_back({std::min(y0, y1), std::max(y0, y1), (x0 + x1) / 2, width, strokeColor, {i}});
            }
        }
    }
}

// Lines of a scanned page: long runs of dark pixels in the page drawn small (about 100 dpi).
void collectPictureLines(Document& d, int page, std::vector<Seg>& hs, std::vector<Seg>& vs) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return;
    const PageSize& ps = d.pageSize(page);
    const double k = 100.0 / 72.0;
    const int W = std::max(1, static_cast<int>(ps.w * k)), H = std::max(1, static_cast<int>(ps.h * k));
    if (1LL * W * H > 40'000'000) return;
    std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4);
    FPDF_BITMAP bmp = api().FPDFBitmap_CreateEx(W, H, FPDFBitmap_BGRx, px.data(), W * 4);
    if (!bmp) return;
    api().FPDFBitmap_FillRect(bmp, 0, 0, W, H, 0xFFFFFFFF);
    api().FPDF_RenderPageBitmap(bmp, pg, 0, 0, W, H, 0, 0);
    api().FPDFBitmap_Destroy(bmp);
    auto dark = [&](int x, int y) {
        const unsigned char* p = &px[(static_cast<size_t>(y) * W + x) * 4];
        return p[0] * 29 + p[1] * 150 + p[2] * 77 < 140 * 256;
    };
    auto toPage = [&](double x, double y, double& ox, double& oy) {
        int dx = static_cast<int>(x), dy = static_cast<int>(y);
        api().FPDF_DeviceToPage(pg, 0, 0, W, H, 0, dx, dy, &ox, &oy);
    };
    const int minRun = static_cast<int>(40 * k);   // 40 points: shorter dark runs are letters
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W;) {
            if (!dark(x, y)) { ++x; continue; }
            int e = x;
            while (e < W && (dark(e, y) || (e + 1 < W && dark(e + 1, y)))) ++e;   // one-pixel gaps are noise
            if (e - x >= minRun) {
                double ax, ay, bx, by;
                toPage(x, y + 0.5, ax, ay); toPage(e, y + 0.5, bx, by);
                hs.push_back({std::min(ax, bx), std::max(ax, bx), ay, 0.75, RGB(0, 0, 0), {-1}});
            }
            x = e + 1;
        }
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < H;) {
            if (!dark(x, y)) { ++y; continue; }
            int e = y;
            while (e < H && (dark(x, e) || (e + 1 < H && dark(x, e + 1)))) ++e;
            if (e - y >= minRun) {
                double ax, ay, bx, by;
                toPage(x + 0.5, y, ax, ay); toPage(x + 0.5, e, bx, by);
                vs.push_back({std::min(ay, by), std::max(ay, by), ax, 0.75, RGB(0, 0, 0), {-1}});
            }
            y = e + 1;
        }
}

// Lines on one level joined: pieces that touch or nearly touch become one line.
std::vector<Seg> joinLines(std::vector<Seg> s, double levelTol) {
    std::sort(s.begin(), s.end(), [](const Seg& p, const Seg& q) { return p.at != q.at ? p.at < q.at : p.a < q.a; });
    std::vector<Seg> out;
    for (const Seg& x : s) {
        bool joined = false;
        for (size_t j = out.size(); j-- > 0 && x.at - out[j].at <= levelTol;) {
            Seg& o = out[j];
            if (std::fabs(o.at - x.at) <= levelTol && x.a <= o.b + 2.0 && x.b >= o.a - 2.0) {
                o.a = std::min(o.a, x.a); o.b = std::max(o.b, x.b);
                o.objs.insert(o.objs.end(), x.objs.begin(), x.objs.end());
                o.width = std::max(o.width, x.width);
                joined = true;
                break;
            }
        }
        if (!joined) out.push_back(x);
    }
    return out;
}

// Positions closer than `tol` become one (their average).
std::vector<double> cluster(std::vector<double> v, double tol) {
    std::sort(v.begin(), v.end());
    std::vector<double> out;
    std::vector<int> count;
    for (double x : v) {
        if (!out.empty() && x - out.back() <= tol) { out.back() = (out.back() * count.back() + x) / (count.back() + 1); ++count.back(); }
        else { out.push_back(x); count.push_back(1); }
    }
    return out;
}

// How much of [a, b] the lines at level `at` (within tol) cover, as a share of its length.
double coverage(const std::vector<Seg>& lines, double at, double a, double b, double tol) {
    if (b <= a) return 1;
    std::vector<std::pair<double, double>> iv;
    for (const Seg& s : lines)
        if (std::fabs(s.at - at) <= tol && s.b > a && s.a < b) iv.push_back({std::max(a, s.a), std::min(b, s.b)});
    std::sort(iv.begin(), iv.end());
    double covered = 0, end = a;
    for (auto& p : iv) {
        if (p.second <= end) continue;
        covered += p.second - std::max(p.first, end);
        end = p.second;
    }
    return covered / (b - a);
}

// ---------------------------------------------------------------- text

struct Char {
    wchar_t ch;
    double l, r, b, t, ox, oy, size;   // box, origin, font size
    double al, ar;                     // where it is set: from its origin to its advance (the box is its ink)
    int obj;                           // page object index
    bool generated;
    bool spaceAfter;                   // PDFium puts a space after it (one in the text, or one it inferred from the gap)
};

struct PageText {
    std::vector<Char> chars;
    std::vector<std::array<float, 4>> objBounds;   // l, b, r, t of every page object (text objects only filled)
};

PageText readText(Document& d, int page) {
    PageText pt;
    FPDF_PAGE pg = d.page(page);
    FPDF_TEXTPAGE tp = d.text(page);
    if (!pg || !tp) return pt;
    const int n = api().FPDFPage_CountObjects(pg);
    std::map<FPDF_PAGEOBJECT, int> index;
    pt.objBounds.assign(static_cast<size_t>(std::max(n, 0)), {0, 0, 0, 0});
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
        if (!o || api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
        index[o] = i;
        float l, b, r, t;
        if (api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) pt.objBounds[static_cast<size_t>(i)] = {l, b, r, t};
    }
    const int count = api().FPDFText_CountChars(tp);
    for (int i = 0; i < count; ++i) {
        Char c{};
        c.ch = static_cast<wchar_t>(api().FPDFText_GetUnicode(tp, i));
        c.generated = api().FPDFText_IsGenerated(tp, i) == 1;
        // word spaces are PDFium's: a space it reads or infers marks the letter before it; line ends too
        if (c.ch == L' ' || c.ch == L'\r' || c.ch == L'\n' || c.generated) { if (!pt.chars.empty()) pt.chars.back().spaceAfter = true; continue; }
        if (!api().FPDFText_GetCharBox(tp, i, &c.l, &c.r, &c.b, &c.t)) continue;
        api().FPDFText_GetCharOrigin(tp, i, &c.ox, &c.oy);
        c.al = c.l; c.ar = c.r;
        FS_RECTF lb{};
        if (api().FPDFText_GetLooseCharBox(tp, i, &lb) && lb.right > lb.left) { c.al = lb.left; c.ar = lb.right; }
        c.size = api().FPDFText_GetFontSize(tp, i);
        FPDF_PAGEOBJECT o = api().FPDFText_GetTextObject(tp, i);
        auto it = index.find(o);
        c.obj = it == index.end() ? -1 : it->second;
        // The size as drawn: some programs (WPS) set a large font size and scale the text down (319 x 0.05); the text
        // page gives the font size unscaled. Of it and it scaled by the object's matrix, the one nearer the height the
        // object takes is the size: with 319, every line of a column read as one line, its letters shuffled.
        if (o && c.obj >= 0) {
            FS_MATRIX m{1, 0, 0, 1, 0, 0};
            const auto& ob = pt.objBounds[static_cast<size_t>(c.obj)];
            const double h = ob[3] - ob[1], sc = api().FPDFPageObj_GetMatrix(o, &m) ? std::hypot(m.c, m.d) : 1.0;
            if (h > 0 && sc > 0 && std::fabs(sc - 1) > 0.01 && std::fabs(c.size * sc - h) < std::fabs(c.size - h)) c.size *= sc;
        }
        if (c.size <= 0) c.size = std::max(1.0, c.t - c.b);
        pt.chars.push_back(c);
    }
    return pt;
}

// A line of text: characters on one baseline, left to right, in pieces separated by wide gaps.
struct TLine {
    double l, r, b, t, base, size;
    std::vector<int> chars;                                  // indices into PageText::chars
    std::vector<std::pair<double, double>> chunks;           // horizontal extents of the pieces
    double sl = 0, sr = 0;                                   // where its letters are set (origins to advances), not their ink
};

std::vector<TLine> linesOf(const PageText& pt, const std::vector<int>& idx) {
    std::vector<int> order = idx;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const Char& p = pt.chars[a]; const Char& q = pt.chars[b];
        if (std::fabs(p.oy - q.oy) > 0.3 * std::max(p.size, q.size)) return p.oy > q.oy;
        return p.ox != q.ox ? p.ox < q.ox : a < b;   // same place (a ligature such as "ti"): the PDF's own order
    });
    std::vector<TLine> lines;
    for (int i : order) {
        const Char& c = pt.chars[i];
        if (!lines.empty() && std::fabs(lines.back().base - c.oy) <= 0.4 * std::max(c.size, lines.back().size)) {
            TLine& L = lines.back();
            L.chars.push_back(i);
            L.l = std::min(L.l, c.l); L.r = std::max(L.r, c.r); L.b = std::min(L.b, c.b); L.t = std::max(L.t, c.t);
            L.size = std::max(L.size, c.size);
        } else lines.push_back({c.l, c.r, c.b, c.t, c.oy, c.size, {i}, {}});
    }
    // A superscript or subscript raised or lowered far enough to make a line of its own (m³, H₂O) belongs to the line
    // whose letters it sits beside: smaller letters, within that line's height and its width.
    for (size_t k = 0; k < lines.size(); ++k) {
        TLine& sm = lines[k];
        int into = -1;
        for (size_t j : {k - 1, k + 1}) {
            if (j >= lines.size() || j == k) continue;
            const TLine& big = lines[j];
            if (sm.size >= 0.88 * big.size || sm.chars.size() > 4) continue;
            if (std::fabs(sm.base - big.base) < 0.25 * big.size || sm.base < big.base - 0.5 * big.size || sm.base > big.base + 0.75 * big.size) continue;
            if (sm.r < big.l - 0.2 * big.size || sm.l > big.r + 0.5 * big.size) continue;
            into = static_cast<int>(j);
            break;
        }
        if (into < 0) continue;
        TLine& L = lines[static_cast<size_t>(into)];
        L.chars.insert(L.chars.end(), sm.chars.begin(), sm.chars.end());
        L.l = std::min(L.l, sm.l); L.r = std::max(L.r, sm.r); L.b = std::min(L.b, sm.b); L.t = std::max(L.t, sm.t);
        lines.erase(lines.begin() + static_cast<long>(k));
        --k;
    }
    for (TLine& L : lines) {
        // Left to right; letters drawn at the same place (the two letters of a ligature such as "ti") keep the PDF's
        // own order, which a sort by place alone left to chance ("representaiton").
        std::sort(L.chars.begin(), L.chars.end(), [&](int a, int b) {
            return pt.chars[a].ox != pt.chars[b].ox ? pt.chars[a].ox < pt.chars[b].ox : a < b;
        });
        // pieces: a gap of more than one em (and three spaces) starts a new piece
        double s = 0, e = 0;
        bool open = false;
        for (int i : L.chars) {
            const Char& c = pt.chars[i];
            if (open && c.l - e > std::max(1.0 * L.size, 3.0)) { L.chunks.push_back({s, e}); open = false; }
            if (!open) { s = c.l; open = true; }
            e = std::max(e, c.r);
        }
        if (open) L.chunks.push_back({s, e});
        L.sl = 1e9; L.sr = -1e9;
        for (int i : L.chars) { L.sl = std::min(L.sl, pt.chars[i].al); L.sr = std::max(L.sr, pt.chars[i].ar); }
    }
    return lines;
}

// ---------------------------------------------------------------- building a table

void initRules(Table& t, bool on) {
    t.hRule.assign(static_cast<size_t>(t.rows() + 1), on);
    t.vRule.assign(static_cast<size_t>(t.cols() + 1), on);
}

// The cells' text, style, alignment and colour from the characters inside them; the table's text objects. False with a
// reason when the table shares text objects with the rest of the page.
bool fillCells(Document& d, Table& t, const PageText& pt, const std::vector<Box>& fills, std::wstring* why) {
    const int R = t.rows(), C = t.cols();
    if (t.cells.size() != static_cast<size_t>(R) * C) t.cells.assign(static_cast<size_t>(R) * C, Cell{});   // keeps merged cells
    std::vector<std::vector<int>> inCell(static_cast<size_t>(R) * C);
    std::vector<char> objIn(pt.objBounds.size(), 0);
    auto rowOf = [&](double y) { for (int r = 0; r < R; ++r) if (y <= t.rowY[r] + 0.5 && y >= t.rowY[r + 1] - 0.5) return r; return -1; };
    auto colOf = [&](double x) { for (int c = 0; c < C; ++c) if (x >= t.colX[c] - 0.5 && x <= t.colX[c + 1] + 0.5) return c; return -1; };
    for (size_t i = 0; i < pt.chars.size(); ++i) {
        const Char& ch = pt.chars[i];
        const double cx = (ch.l + ch.r) / 2, cy = (ch.b + ch.t) / 2;
        const int r = rowOf(cy), c = colOf(cx);
        if (r < 0 || c < 0) continue;
        int ar = r, ac = c;
        anchorOf(t, r, c, ar, ac);
        inCell[static_cast<size_t>(ar) * C + ac].push_back(static_cast<int>(i));
        if (ch.obj >= 0) objIn[static_cast<size_t>(ch.obj)] = 1;
    }
    // the text objects of each cell (an object holding letters of two cells ties them together)
    {
        std::map<int, std::set<int>> cellsOfObj;
        for (size_t k = 0; k < inCell.size(); ++k)
            for (int i : inCell[k]) if (pt.chars[i].obj >= 0) cellsOfObj[pt.chars[i].obj].insert(static_cast<int>(k));
        for (const auto& kv : cellsOfObj)
            for (int k : kv.second) {
                Cell& cell = t.cells[static_cast<size_t>(k)];
                cell.objs.push_back(kv.first);
                if (kv.second.size() > 1) cell.sharedObjs = true;
            }
    }
    // every text object with a letter inside must lie inside: else removing it would take text outside the table with it
    for (size_t o = 0; o < objIn.size(); ++o) {
        if (!objIn[o]) continue;
        const auto& b = pt.objBounds[o];
        if (b[0] < t.left() - 2 || b[2] > t.right() + 2 || b[1] < t.bottom() - 2 || b[3] > t.top() + 2) {
            if (why) *why = L"This table shares its text with the rest of the page (a line of text runs out of it), so it cannot be changed safely.";
            return false;
        }
        t.objects.push_back(static_cast<int>(o));
    }
    double padSum = 0;
    int padN = 0;
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            Cell& cell = t.at(r, c);
            if (cell.covered) continue;
            const double L = t.colX[c], Rr = t.colX[c + cell.colSpan], T = t.rowY[r], B = t.rowY[r + cell.rowSpan];
            // the colour behind the cell
            for (const Box& bx : fills) {
                const double ow = std::min(Rr, bx.r) - std::max(L, bx.l), oh = std::min(T, bx.t) - std::max(B, bx.b);
                if (ow <= 0 || oh <= 0) continue;
                if (ow * oh >= 0.8 * (Rr - L) * (T - B) && (bx.r - bx.l) * (bx.t - bx.b) <= 1.2 * (t.right() - t.left()) * (t.top() - t.bottom())) {
                    const bool white = GetRValue(bx.fill) > 248 && GetGValue(bx.fill) > 248 && GetBValue(bx.fill) > 248;
                    cell.fill = white ? kNoFill : bx.fill;
                }
            }
            const auto& idx = inCell[static_cast<size_t>(r) * C + c];
            if (idx.empty()) continue;
            std::vector<TLine> lines = linesOf(pt, idx);
            if (lines.empty()) continue;
            // the text: words joined by spaces; a line that ends well before the cell's edge ended on purpose
            std::wstring text;
            double minL = 1e9, maxR = -1e9, maxT = -1e9, minB = 1e9;
            for (size_t li = 0; li < lines.size(); ++li) {
                const TLine& ln = lines[li];
                std::wstring s;
                // the line's own baseline: where most of its full-size letters sit (not a superscript's)
                double lineBase = ln.base;
                {
                    std::map<long, int> votes;
                    for (int ci : ln.chars) if (pt.chars[ci].size >= 0.88 * ln.size) ++votes[std::lround(pt.chars[ci].oy * 4)];
                    int most = 0;
                    for (const auto& [k, v] : votes) if (v > most) { most = v; lineBase = k / 4.0; }
                }
                for (size_t k = 0; k < ln.chars.size(); ++k) {
                    const Char& ch = pt.chars[ln.chars[k]];
                    // a raised, smaller letter (m³, 29°): as the superscript letter every font has, joined to its word; the
                    // gap PDFium sees around it is not a space
                    const bool raised = ch.size < 0.88 * ln.size && ch.oy > lineBase + 0.25 * ln.size;
                    wchar_t c = ch.ch;
                    if (raised) {
                        if (c == L'1') c = 0xB9; else if (c == L'2') c = 0xB2; else if (c == L'3') c = 0xB3;
                        else if (c == L'o' || c == L'0' || c == 0xBA) c = 0xB0;
                        if (!s.empty() && s.back() == L' ' && k > 0 && !pt.chars[ln.chars[k - 1]].spaceAfter) s.pop_back();
                        if (!s.empty() && s.back() == L' ' && k > 0 && pt.chars[ln.chars[k - 1]].r > ch.l - 0.5 * ln.size) s.pop_back();
                    }
                    s += c;
                    const bool nextRaised = k + 1 < ln.chars.size() && pt.chars[ln.chars[k + 1]].size < 0.88 * ln.size &&
                                            pt.chars[ln.chars[k + 1]].oy > lineBase + 0.25 * ln.size;
                    const bool tight = k + 1 < ln.chars.size() && pt.chars[ln.chars[k + 1]].l - ch.r < 0.15 * ln.size;
                    if (ch.spaceAfter && k + 1 < ln.chars.size() && !((raised || nextRaised) && tight)) s += L' ';
                }
                while (!s.empty() && s.back() == L' ') s.pop_back();
                if (li) {
                    const TLine& prev = lines[li - 1];
                    // the first word of this line would have fitted on the previous one: a line break
                    double firstWord = 0;
                    for (int ci : ln.chars) { firstWord = pt.chars[ci].r - ln.l; if (pt.chars[ci].spaceAfter) break; }
                    text += prev.r + 0.3 * ln.size + firstWord < Rr - 1 ? L'\n' : L' ';
                }
                text += s;
                minL = std::min(minL, ln.l); maxR = std::max(maxR, ln.r); maxT = std::max(maxT, ln.t); minB = std::min(minB, ln.b);
            }
            cell.text = text;
            cell.textW = maxR - minL;
            cell.baseOff = T - lines.front().base;
            cell.lines = static_cast<int>(lines.size());
            // style: the first letter's object
            const Char& first = pt.chars[lines.front().chars.front()];
            if (first.obj >= 0) {
                pageobj::TextStyle st;
                std::string fname;
                double size = first.size;
                if (pageobj::textObjectStyle(d, t.page, first.obj, st, fname, size)) {
                    cell.like = first.obj;
                    cell.family = st.family; cell.bold = st.bold; cell.italic = st.italic; cell.color = st.color;
                    cell.size = size > 0 ? size : first.size;
                } else cell.size = first.size;
            } else cell.size = first.size;
            // alignment from the gaps around the text
            const double lg = minL - L, rg = Rr - maxR, tg = T - maxT, bg = minB - B;
            if (std::fabs(lg - rg) < std::max(2.0, 0.12 * (lg + rg)) && lg > 4) cell.align = 1;
            else cell.align = rg + 2 < lg ? 2 : 0;
            // justified: two lines or more, starting at one edge, and all but the last ending at one edge (a word's width
            // short of the cell's right line at most); the last one shorter
            {
                // the text's own lines: not a superscript's or subscript's (smaller letters raised off the line)
                double big = 0;
                for (const TLine& ln : lines) big = std::max(big, ln.size);
                double left = 1e9;
                for (const TLine& ln : lines) left = std::min(left, ln.l);
                std::vector<const TLine*> body;
                for (const TLine& ln : lines)
                    if (ln.size >= 0.8 * big && (ln.l - left < 2 * big || ln.r - ln.l > 4 * big)) body.push_back(&ln);
                if (body.size() >= 2) {
                    const double sz = big, tol = std::max(1.0, 0.12 * sz);   // lines measured where their letters are set, not by their ink, which differs by font (Apple's Arial)
                    bool sameLeft = true, sameRight = true;
                    for (size_t li = 0; li < body.size(); ++li) {
                        if (std::fabs(body[li]->sl - body.front()->sl) > tol) sameLeft = false;
                        if (li + 1 < body.size() && std::fabs(body[li]->sr - body.front()->sr) > tol) sameRight = false;
                    }
                    // (and they end as far from the cell's right line as they start from its left one: a line only wrapped ends a
                    // word's width short of it, or less)
                    if (sameLeft && sameRight && body.back()->sr < body.front()->sr - sz &&
                        std::fabs((Rr - body.front()->sr) - (body.front()->sl - L)) < std::max(2 * tol, 0.25 * (body.front()->sl - L))) cell.align = 3;
                    // lines that start at one edge and end at different places are set from the left, however even the
                    // gaps around the longest one are
                    else if (sameLeft && cell.align == 1) {
                        double lo = 1e9, hi = -1e9;
                        for (const TLine* ln : body) { lo = std::min(lo, ln->sr); hi = std::max(hi, ln->sr); }
                        if (hi - lo > sz) cell.align = 0;
                    }
                }
            }
            if (std::fabs(tg - bg) < std::max(2.0, 0.2 * (tg + bg))) cell.valign = 1;
            else cell.valign = tg < bg ? 0 : 2;
            if ((cell.align == 0 || cell.align == 3) && lg > 0) { padSum += lg; ++padN; }
        }
    if (padN) t.pad = std::clamp(padSum / padN, 1.5, 8.0);
    // Without lines between the columns, their edges are only the middles of the gaps: a column's alignment is the edge
    // its cells share (left, right, or else the middle).
    bool innerLines = false;
    for (int c = 1; c < C; ++c) innerLines = innerLines || t.vRule[c];
    if (!innerLines)
        for (int c = 0; c < C; ++c) {
            std::vector<double> ls, rs;
            for (int r = 0; r < R; ++r) {
                const auto& idx = inCell[static_cast<size_t>(r) * C + c];
                if (idx.empty() || t.at(r, c).covered || t.at(r, c).colSpan > 1) continue;
                double l = 1e9, rr = -1e9;
                for (int i : idx) { l = std::min(l, pt.chars[i].l); rr = std::max(rr, pt.chars[i].r); }
                ls.push_back(l); rs.push_back(rr);
            }
            if (ls.size() < 2) continue;
            std::vector<double> ms;
            for (size_t k = 0; k < ls.size(); ++k) ms.push_back((ls[k] + rs[k]) / 2);
            // how many cells share an edge (within the letters' side bearings, about a point and a half), and how wide
            // that edge's spread is otherwise
            auto shared = [](const std::vector<double>& v) {
                size_t best = 0;
                for (double a : v) { size_t n = 0; for (double b : v) n += std::fabs(a - b) <= 1.5; best = std::max(best, n); }
                return best;
            };
            auto spread = [](const std::vector<double>& v) { return *std::max_element(v.begin(), v.end()) - *std::min_element(v.begin(), v.end()); };
            const size_t nl = shared(ls), nr = shared(rs), nm = shared(ms);
            int align;
            if (nl != nr || nl != nm) align = nl >= nr && nl >= nm ? 0 : nr >= nm ? 2 : 1;   // the edge most cells share
            else {
                const double sl = spread(ls), sr = spread(rs), sm = spread(ms);
                align = sl <= std::min(sr, sm) + 0.5 ? 0 : sr <= sm + 0.5 ? 2 : 1;
            }
            const double colLeft = *std::min_element(ls.begin(), ls.end());
            for (int r = 0; r < R; ++r) {
                Cell& cell = t.at(r, c);
                if (cell.covered || cell.colSpan != 1 || cell.align == 3) continue;
                cell.align = align;
                // a cell that starts where the column starts while the others line up elsewhere (a heading over
                // figures) stays on the left
                const auto& idx = inCell[static_cast<size_t>(r) * C + c];
                if (align != 0 && !idx.empty()) {
                    double l = 1e9, rr = -1e9;
                    for (int i : idx) { l = std::min(l, pt.chars[i].l); rr = std::max(rr, pt.chars[i].r); }
                    const std::vector<double>& own = align == 2 ? rs : ms;
                    const double edge = align == 2 ? rr : (l + rr) / 2;
                    size_t alike = 0;
                    for (double v : own) alike += std::fabs(v - edge) <= 1.5;
                    if (alike < 2 && std::fabs(l - colLeft) <= 1.5) cell.align = 0;
                }
            }
        }
    return true;
}

// The objects drawing the table's lines and colours must lie inside it, else removing them changes the page outside.
bool takeGraphics(Table& t, const std::vector<Seg>& hs, const std::vector<Seg>& vs, const std::vector<Box>& fills, FPDF_PAGE pg, std::wstring* why) {
    std::vector<int> objs;
    auto inside = [&](double l, double b, double r, double tp) { return l >= t.left() - 3 && r <= t.right() + 3 && b >= t.bottom() - 3 && tp <= t.top() + 3; };
    for (const auto* list : {&hs, &vs})
        for (const Seg& s : *list) {
            const bool horiz = list == &hs;
            const double l = horiz ? s.a : s.at, r = horiz ? s.b : s.at, b = horiz ? s.at : s.a, tp = horiz ? s.at : s.b;
            if (!inside(l, b, r, tp)) continue;
            for (int o : s.objs) if (o >= 0) objs.push_back(o);
        }
    for (const Box& bx : fills) if (inside(bx.l, bx.b, bx.r, bx.t)) objs.push_back(bx.obj);
    std::sort(objs.begin(), objs.end());
    objs.erase(std::unique(objs.begin(), objs.end()), objs.end());
    for (int o : objs) {
        float l, b, r, tp;
        FPDF_PAGEOBJECT po = api().FPDFPage_GetObject(pg, o);
        if (!po || !api().FPDFPageObj_GetBounds(po, &l, &b, &r, &tp)) continue;
        // a drawing that also draws outside the table (Word and LibreOffice draw many borders as one): only its part
        // inside the table is taken; the rest is drawn again as it was
        if (!inside(l, b, r, tp)) {
            if (api().FPDFPageObj_GetType(po) != FPDF_PAGEOBJ_PATH) {
                if (why) *why = L"This table's lines are drawn together with other parts of the page, so it cannot be changed safely.";
                return false;
            }
            t.sharedPaths.push_back(o);
        }
        t.objects.push_back(o);
    }
    return true;
}

// Columns from text that lines up: the gaps that are free of text on every line become column edges.
std::vector<double> columnsFromText(const std::vector<TLine>& lines, double l, double r) {
    std::vector<std::pair<double, double>> free{{l, r}};
    for (const TLine& ln : lines)
        for (const auto& ch : ln.chunks) {
            std::vector<std::pair<double, double>> next;
            for (const auto& f : free) {
                if (ch.second <= f.first || ch.first >= f.second) { next.push_back(f); continue; }
                if (ch.first > f.first) next.push_back({f.first, ch.first});
                if (ch.second < f.second) next.push_back({ch.second, f.second});
            }
            free.swap(next);
        }
    std::vector<double> edges{l};
    const double size = lines.empty() ? 10 : lines.front().size;
    for (const auto& f : free)
        if (f.first > l + 1 && f.second < r - 1 && f.second - f.first >= 0.8 * size) edges.push_back((f.first + f.second) / 2);
    edges.push_back(r);
    // two neighbouring bands that no line has text in both of are one column whose pieces are aligned differently
    // (a heading on the left over numbers on the right, a centred heading over right-aligned figures)
    for (size_t k = 1; k + 1 < edges.size();) {
        bool both = false;
        for (const TLine& ln : lines) {
            bool left = false, right = false;
            for (const auto& ch : ln.chunks) {
                const double mid = (ch.first + ch.second) / 2;
                if (mid > edges[k - 1] && mid < edges[k]) left = true;
                if (mid > edges[k] && mid < edges[k + 1]) right = true;
            }
            if (left && right) { both = true; break; }
        }
        if (both) ++k;
        else edges.erase(edges.begin() + static_cast<std::ptrdiff_t>(k));
    }
    return edges;
}

// Row edges between text lines (each line a row), from `top` down to `bottom`.
std::vector<double> rowsFromText(const std::vector<TLine>& lines, double top, double bottom) {
    std::vector<double> edges{top};
    for (size_t i = 1; i < lines.size(); ++i) edges.push_back((lines[i - 1].b + lines[i].t) / 2);
    edges.push_back(bottom);
    return edges;
}

// `colsFromLines` / `rowsFromLines`: whether the column / row edges are drawn lines (else they came from the text, and a
// missing line there says nothing about merged cells)
void markMerges(Table& t, const std::vector<Seg>& hs, const std::vector<Seg>& vs, bool colsFromLines, bool rowsFromLines) {
    const int R = t.rows(), C = t.cols();
    // cells joined where the line between them is missing
    std::vector<int> parent(static_cast<size_t>(R) * C);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> find = [&](int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); };
    for (int r = 0; r < R && colsFromLines; ++r)
        for (int c = 0; c + 1 < C; ++c)
            if (coverage(vs, t.colX[c + 1], t.rowY[r + 1], t.rowY[r], 1.6) < 0.5) parent[find(r * C + c)] = find(r * C + c + 1);
    for (int r = 0; r + 1 < R && rowsFromLines; ++r)
        for (int c = 0; c < C; ++c)
            if (coverage(hs, t.rowY[r + 1], t.colX[c], t.colX[c + 1], 1.6) < 0.5) parent[find(r * C + c)] = find((r + 1) * C + c);
    std::map<int, std::vector<int>> groups;
    for (int i = 0; i < R * C; ++i) groups[find(i)].push_back(i);
    for (auto& g : groups) {
        if (g.second.size() < 2) continue;
        int r0 = R, c0 = C, r1 = -1, c1 = -1;
        for (int i : g.second) { r0 = std::min(r0, i / C); r1 = std::max(r1, i / C); c0 = std::min(c0, i % C); c1 = std::max(c1, i % C); }
        if ((r1 - r0 + 1) * (c1 - c0 + 1) != static_cast<int>(g.second.size())) continue;   // not a rectangle: leave the cells apart
        Cell& a = t.at(r0, c0);
        a.rowSpan = r1 - r0 + 1; a.colSpan = c1 - c0 + 1;
        for (int r = r0; r <= r1; ++r) for (int c = c0; c <= c1; ++c) if (r != r0 || c != c0) t.at(r, c).covered = true;
    }
}

void lineStyle(Table& t, const std::vector<Seg>& hs, const std::vector<Seg>& vs) {
    std::map<std::pair<long, COLORREF>, double> votes;
    for (const auto* list : {&hs, &vs})
        for (const Seg& s : *list) {
            const bool horiz = list == &hs;
            const double mid = horiz ? s.at : (s.a + s.b) / 2, across = horiz ? (s.a + s.b) / 2 : s.at;
            if ((horiz && (mid < t.bottom() - 2 || mid > t.top() + 2 || across < t.left() || across > t.right())) ||
                (!horiz && (across < t.left() - 2 || across > t.right() + 2 || mid < t.bottom() || mid > t.top()))) continue;
            votes[{std::lround(s.width * 100), s.color}] += s.b - s.a;
        }
    double best = 0;
    for (auto& v : votes) if (v.second > best) { best = v.second; t.lineWidth = std::clamp(v.first.first / 100.0, 0.25, 6.0); t.lineColor = v.first.second; }
}

void cleanEdges(std::vector<double>& e, double minGap) {   // edges too close (double lines) become one
    std::vector<double> out;
    for (double x : e) if (out.empty() || std::fabs(x - out.back()) >= minGap) out.push_back(x);
    e.swap(out);
}

} // namespace

// ---------------------------------------------------------------- detection

// Tables drawn one right on top of the other (a box of boxes over a framed note over a form, as in an exam paper made in
// Word) touch, so their lines make one grid. Between two bands of rows, a change in the outer edges (or column edges
// that do not fit each other) means a different table: the grid is cut there, each part a table of its own.
std::vector<std::pair<std::vector<Seg>, std::vector<Seg>>> splitStacked(const std::vector<Seg>& ch, const std::vector<Seg>& cv) {
    std::vector<double> ys;
    for (const Seg& sg : ch) ys.push_back(sg.at);
    std::vector<double> rowY = cluster(ys, 1.6);
    std::sort(rowY.rbegin(), rowY.rend());   // top first
    auto xsAt = [&](double y) {   // the vertical lines across height y
        std::vector<double> xs;
        for (const Seg& sg : cv) if (sg.a <= y && sg.b >= y) xs.push_back(sg.at);
        std::sort(xs.begin(), xs.end());
        return cluster(xs, 1.6);
    };
    auto subset = [](const std::vector<double>& a, const std::vector<double>& b) {   // every edge of a is in b
        for (double x : a) { bool in = false; for (double y : b) if (std::fabs(x - y) < 2) in = true; if (!in) return false; }
        return true;
    };
    for (size_t k = 1; k + 1 < rowY.size(); ++k) {
        const std::vector<double> up = xsAt((rowY[k - 1] + rowY[k]) / 2), down = xsAt((rowY[k] + rowY[k + 1]) / 2);
        if (up.size() < 2 || down.size() < 2) continue;
        const bool edges = std::fabs(up.front() - down.front()) > 3 || std::fabs(up.back() - down.back()) > 3;
        if (!edges && (subset(up, down) || subset(down, up))) continue;
        // cut at rowY[k]: the line there belongs to both parts, each vertical line to the part it is in
        const double y = rowY[k];
        std::vector<Seg> h1, v1, h2, v2;
        for (Seg sg : ch) {
            const bool atCut = std::fabs(sg.at - y) <= 1.6;   // only the line along the cut is shared: each part takes its own width of it
            if (sg.at >= y - 1.6) { Seg a = sg; if (atCut) { a.a = std::max(a.a, up.front() - 2); a.b = std::min(a.b, up.back() + 2); } if (a.b - a.a > 1) h1.push_back(a); }
            if (sg.at <= y + 1.6) { Seg b = sg; if (atCut) { b.a = std::max(b.a, down.front() - 2); b.b = std::min(b.b, down.back() + 2); } if (b.b - b.a > 1) h2.push_back(b); }
        }
        for (Seg sg : cv) {
            if (sg.b > y + 1.6) { Seg a = sg; a.a = std::max(a.a, y); v1.push_back(a); }
            if (sg.a < y - 1.6) { Seg b = sg; b.b = std::min(b.b, y); v2.push_back(b); }
        }
        auto out = splitStacked(h1, v1);
        auto more = splitStacked(h2, v2);
        out.insert(out.end(), more.begin(), more.end());
        return out;
    }
    return {{ch, cv}};
}

std::vector<Table> detect(Document& d, int page) {
    std::vector<Table> found;
    FPDF_PAGE pg = d.page(page);
    if (!pg) return found;
    std::vector<Seg> hs0, vs0;
    std::vector<Box> fills;
    collectPaths(pg, hs0, vs0, fills);
    // a scanned page: one picture over most of the page and hardly any drawing; its lines come from the picture
    bool scanned = false;
    {
        const PageSize& ps = d.pageSize(page);
        const int n = api().FPDFPage_CountObjects(pg);
        for (int i = 0; i < n && !scanned; ++i) {
            FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
            if (!o || api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE || contentgen::drawnAsPicture(o)) continue;
            float l, b, r, t;
            if (api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t) && (r - l) * (t - b) > 0.5 * ps.w * ps.h) scanned = hs0.size() + vs0.size() < 4;
        }
    }
    if (scanned) collectPictureLines(d, page, hs0, vs0);
    const std::vector<Seg> hs = joinLines(hs0, 1.0), vs = joinLines(vs0, 1.0);
    const PageText pt = readText(d, page);

    // 1. ruled tables: horizontal and vertical lines that cross form a grid
    const size_t H = hs.size(), V = vs.size();
    std::vector<int> parent(H + V);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> find = [&](int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); };
    for (size_t i = 0; i < H; ++i)
        for (size_t j = 0; j < V; ++j)
            if (vs[j].at >= hs[i].a - 2.5 && vs[j].at <= hs[i].b + 2.5 && hs[i].at >= vs[j].a - 2.5 && hs[i].at <= vs[j].b + 2.5)
                parent[find(static_cast<int>(i))] = find(static_cast<int>(H + j));
    std::map<int, std::pair<std::vector<int>, std::vector<int>>> comps;
    for (size_t i = 0; i < H; ++i) comps[find(static_cast<int>(i))].first.push_back(static_cast<int>(i));
    for (size_t j = 0; j < V; ++j) comps[find(static_cast<int>(H + j))].second.push_back(static_cast<int>(j));
    std::vector<char> used(pt.chars.size(), 0);
    // one grid of lines: a table (lines that touch but belong to tables stacked one on the other are cut apart first)
    auto buildGrid = [&](const std::vector<Seg>& ch, const std::vector<Seg>& cv) {
        if (ch.size() < 2 || cv.size() < 2) return;
        std::vector<double> xs, ys;
        for (const Seg& s : cv) xs.push_back(s.at);
        for (const Seg& s : ch) ys.push_back(s.at);
        std::vector<double> colX = cluster(xs, 1.6), rowY = cluster(ys, 1.6);
        std::reverse(rowY.begin(), rowY.end());
        cleanEdges(colX, 3.0);
        cleanEdges(rowY, 3.0);
        if (colX.size() < 2 || rowY.size() < 2) return;
        Table t;
        t.page = page;
        t.scanned = scanned;
        t.colX = colX; t.rowY = rowY;
        if (t.right() - t.left() < 20 || t.top() - t.bottom() < 8) return;
        // only the frame: the columns or rows come from the text inside
        std::vector<int> inside;
        for (size_t i = 0; i < pt.chars.size(); ++i) {
            const Char& c = pt.chars[i];
            const double cx = (c.l + c.r) / 2, cy = (c.b + c.t) / 2;
            if (cx > t.left() && cx < t.right() && cy < t.top() && cy > t.bottom()) inside.push_back(static_cast<int>(i));
        }
        const std::vector<TLine> lines = linesOf(pt, inside);
        const bool colsFromLines = t.colX.size() > 2, rowsFromLines = t.rowY.size() > 2;
        if (!colsFromLines && lines.size() >= 2) t.colX = columnsFromText(lines, t.left(), t.right());
        if (!rowsFromLines && lines.size() >= 2) t.rowY = rowsFromText(lines, t.top(), t.bottom());
        if (t.rows() * t.cols() < 2) return;   // one box: a frame, not a table
        // A box around a paragraph (one column of text lines, no lines between them) is not a table; one column with
        // ruled rows is (a list drawn as a table: names, items, one per row).
        if (!colsFromLines && t.cols() < 2 && !rowsFromLines) return;
        t.cells.assign(static_cast<size_t>(t.rows()) * t.cols(), Cell{});
        markMerges(t, ch, cv, colsFromLines, rowsFromLines);
        initRules(t, true);
        // the lines that are drawn
        for (int r = 0; r <= t.rows(); ++r) t.hRule[r] = coverage(ch, t.rowY[r], t.left(), t.right(), 1.6) > 0.2;
        for (int c = 0; c <= t.cols(); ++c) t.vRule[c] = coverage(cv, t.colX[c], t.bottom(), t.top(), 1.6) > 0.2;
        lineStyle(t, ch, cv);
        for (int i : inside) used[i] = 1;
        found.push_back(std::move(t));
    };
    for (auto& kv : comps) {
        const auto& hi = kv.second.first;
        const auto& vi = kv.second.second;
        if (hi.size() < 2 || vi.size() < 2) continue;
        std::vector<Seg> ch, cv;
        for (int i : hi) ch.push_back(hs[i]);
        for (int j : vi) cv.push_back(vs[j]);
        for (auto& part : splitStacked(ch, cv)) buildGrid(part.first, part.second);
    }

    // 2. tables without lines (or with horizontal rules only): text lines whose pieces line up in columns
    {
        std::vector<int> rest;
        for (size_t i = 0; i < pt.chars.size(); ++i) if (!used[i]) rest.push_back(static_cast<int>(i));
        std::vector<TLine> lines = linesOf(pt, rest);
        std::sort(lines.begin(), lines.end(), [](const TLine& a, const TLine& b) { return a.base > b.base; });
        size_t i = 0;
        while (i < lines.size()) {
            // a run of lines with two or more pieces, close together
            size_t j = i;
            while (j < lines.size() && lines[j].chunks.size() >= 2 && (j == i || lines[j - 1].base - lines[j].base <= 2.6 * lines[j].size)) ++j;
            if (j - i < 3) { i = std::max(j, i + 1); continue; }
            std::vector<TLine> block(lines.begin() + static_cast<std::ptrdiff_t>(i), lines.begin() + static_cast<std::ptrdiff_t>(j));
            double l = 1e9, r = -1e9;
            for (const TLine& ln : block) { l = std::min(l, ln.l); r = std::max(r, ln.r); }
            std::vector<double> colX = columnsFromText(block, l - 2, r + 2);
            const int cols = static_cast<int>(colX.size()) - 1;
            // two columns of long pieces are a two-column text layout, not a table
            double longest = 0;
            for (const TLine& ln : block) for (const auto& ch : ln.chunks) longest = std::max(longest, ch.second - ch.first);
            if (cols < 2 || (cols == 2 && longest > 0.45 * (r - l))) { i = j; continue; }
            Table t;
            t.page = page;
            t.colX = colX;
            const double top = block.front().t + 0.35 * block.front().size, bottom = block.back().b - 0.35 * block.back().size;
            t.rowY = rowsFromText(block, top, bottom);
            t.cells.assign(static_cast<size_t>(t.rows()) * t.cols(), Cell{});
            initRules(t, false);
            // horizontal rules that run along the block (above, between or below its lines) belong to it
            for (const Seg& s : hs) {
                if (s.a > l + 0.2 * (r - l) || s.b < r - 0.2 * (r - l)) continue;
                if (s.at > top + 1.2 * block.front().size || s.at < bottom - 1.2 * block.back().size) continue;
                int best = 0;
                for (int k = 1; k <= t.rows(); ++k) if (std::fabs(t.rowY[k] - s.at) < std::fabs(t.rowY[best] - s.at)) best = k;
                t.hRule[best] = 1;
                if (best == 0) t.rowY[0] = std::max(t.rowY[0], s.at);
                if (best == t.rows()) t.rowY[best] = std::min(t.rowY[best], s.at);
                // the rules reach past the text (the cells' padding): the table is as wide as its rules
                t.colX.front() = std::min(t.colX.front(), s.a);
                t.colX.back() = std::max(t.colX.back(), s.b);
                t.lineWidth = s.width; t.lineColor = s.color;
            }
            found.push_back(std::move(t));
            i = j;
        }
    }

    // text, colours and objects of each table; tables that cannot be changed safely are kept, marked by an empty cell list
    std::vector<Table> out;
    for (Table& t : found) {
        t.pad = 3;
        t.foundL = t.left(); t.foundR = t.right(); t.foundT = t.top(); t.foundB = t.bottom();
        if (!fillCells(d, t, pt, fills, nullptr)) { t.cells.clear(); out.push_back(std::move(t)); continue; }
        if (!takeGraphics(t, hs, vs, fills, pg, nullptr)) { t.cells.clear(); out.push_back(std::move(t)); continue; }
        if (t.scanned) {   // recognised words are sized by their own box (capitals and descenders make it larger): one size for all
            double size = 0, rowMin = 1e9;
            for (const Cell& c : t.cells) if (!c.covered && !c.text.empty()) size = std::max(size, c.size);
            for (int r = 0; r < t.rows(); ++r) rowMin = std::min(rowMin, t.rowY[r] - t.rowY[r + 1]);
            size = std::min(size, 0.6 * rowMin);
            for (Cell& c : t.cells) { c.size = size; c.baseOff = -1; c.valign = 1; }   // the scan's baselines are guesses: centre the text
        }
        // a row's cells sit alike: the vertical alignment most of them have
        for (int r = 0; r < t.rows(); ++r) {
            int votes[3] = {0, 0, 0};
            for (int c = 0; c < t.cols(); ++c) { const Cell& x = t.at(r, c); if (!x.covered && !x.text.empty() && x.rowSpan == 1) ++votes[x.valign]; }
            const int v = votes[1] >= votes[0] && votes[1] >= votes[2] ? 1 : votes[0] >= votes[2] ? 0 : 2;
            for (int c = 0; c < t.cols(); ++c) { Cell& x = t.at(r, c); if (!x.covered && x.rowSpan == 1) x.valign = v; }
        }
        // an empty cell writes like its neighbours: the nearest text in its row (else in its column) gives font and size
        for (int r = 0; r < t.rows(); ++r)
            for (int c = 0; c < t.cols(); ++c) {
                Cell& x = t.at(r, c);
                if (x.covered || !x.text.empty()) continue;
                const Cell* from = nullptr;
                for (int dc = 1; dc < t.cols() && !from; ++dc)
                    for (int cc : {c - dc, c + dc})
                        if (!from && cc >= 0 && cc < t.cols()) { const Cell& y = t.at(r, cc); if (!y.covered && !y.text.empty()) from = &y; }
                for (int dr = 1; dr < t.rows() && !from; ++dr)
                    for (int rr : {r - dr, r + dr})
                        if (!from && rr >= 0 && rr < t.rows()) { const Cell& y = t.at(rr, c); if (!y.covered && !y.text.empty()) from = &y; }
                if (!from) continue;
                x.size = from->size; x.family = from->family; x.color = from->color; x.like = from->like; x.italic = from->italic;
            }
        // how each cell was found: what is not changed later is left as it is
        for (int r = 0; r < t.rows(); ++r)
            for (int c = 0; c < t.cols(); ++c) {
                Cell& x = t.at(r, c);
                if (x.covered) continue;
                x.found = true;
                x.origText = x.text; x.origSize = x.size; x.origFamily = x.family; x.origBold = x.bold; x.origItalic = x.italic;
                x.origColor = x.color; x.origAlign = x.align; x.origValign = x.valign;
                x.fL = t.colX[c]; x.fR = t.colX[c + x.colSpan]; x.fT = t.rowY[r]; x.fB = t.rowY[r + x.rowSpan];
                x.fRowSpan = x.rowSpan; x.fColSpan = x.colSpan; x.fFill = x.fill;
            }
        t.foundColX = t.colX; t.foundRowY = t.rowY; t.foundH = t.hRule; t.foundV = t.vRule;
        t.hOrig.resize(static_cast<size_t>(t.rows() + 1)); std::iota(t.hOrig.begin(), t.hOrig.end(), 0);
        t.vOrig.resize(static_cast<size_t>(t.cols() + 1)); std::iota(t.vOrig.begin(), t.vOrig.end(), 0);
        for (const auto* list : {&hs0, &vs0})
            for (const Seg& sg : *list) {
                const bool h = list == &hs0;
                const double x0 = h ? sg.a : sg.at, x1 = h ? sg.b : sg.at, y0 = h ? sg.at : sg.a, y1 = h ? sg.at : sg.b;
                if (x1 < t.left() - 3 || x0 > t.right() + 3 || y1 < t.bottom() - 3 || y0 > t.top() + 3) continue;
                t.pieces.push_back({h, sg.at, sg.a, sg.b, sg.width, sg.color});
            }
        // scanned: the recognised words are invisible text; they go with the table
        std::sort(t.objects.begin(), t.objects.end());
        t.objects.erase(std::unique(t.objects.begin(), t.objects.end()), t.objects.end());
        out.push_back(std::move(t));
    }
    std::sort(out.begin(), out.end(), [](const Table& a, const Table& b) {
        return (a.right() - a.left()) * (a.top() - a.bottom()) > (b.right() - b.left()) * (b.top() - b.bottom());
    });
    return out;
}

namespace {

bool formHasText(FPDF_PAGEOBJECT form, int depth = 0) {
    const auto& a = pdfium::api();
    const int n = a.FPDFFormObj_CountObjects(form);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = a.FPDFFormObj_GetObject(form, static_cast<unsigned long>(i));
        const int type = o ? a.FPDFPageObj_GetType(o) : 0;
        if (type == FPDF_PAGEOBJ_TEXT) return true;
        if (type == FPDF_PAGEOBJ_FORM && depth < 8 && formHasText(o, depth + 1)) return true;
    }
    return false;
}

} // namespace

// Text inside form objects (PDFs saved from Word on a Mac put the whole page in one) cannot be taken out by the editor,
// which would then draw the changed table over the old one. Such forms are opened up: their objects go onto the page
// itself, in the same place and order, so the page looks the same and every text object is the page's own. Not a step
// of its own for Undo: the page looks as it did, and the edit that follows is the step.
bool unwrapTextForms(Document& d, int page) {
    const auto& a = pdfium::api();
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    bool changed = false;
    for (int pass = 0; pass < 8; ++pass) {   // forms inside forms come up one level each pass
        bool again = false;
        for (int i = a.FPDFPage_CountObjects(pg) - 1; i >= 0; --i) {
            FPDF_PAGEOBJECT fo = a.FPDFPage_GetObject(pg, i);
            if (!fo || a.FPDFPageObj_GetType(fo) != FPDF_PAGEOBJ_FORM || !formHasText(fo)) continue;
            FS_MATRIX m;
            if (!a.FPDFPageObj_GetMatrix(fo, &m)) continue;
            std::vector<FPDF_PAGEOBJECT> kids;
            for (int k = 0, n = a.FPDFFormObj_CountObjects(fo); k < n; ++k) kids.push_back(a.FPDFFormObj_GetObject(fo, static_cast<unsigned long>(k)));
            std::vector<FPDF_PAGEOBJECT> moved;
            for (FPDF_PAGEOBJECT c : kids) {
                if (!c || !a.FPDFFormObj_RemoveObject(fo, c)) break;
                moved.push_back(c);
            }
            if (moved.empty()) continue;
            size_t at = static_cast<size_t>(i);   // in the form's place (whatever could not be taken out stays in it, after)
            for (FPDF_PAGEOBJECT c : moved) {
                a.FPDFPageObj_Transform(c, m.a, m.b, m.c, m.d, m.e, m.f);
                a.FPDFPage_InsertObjectAtIndex(pg, c, at++);
            }
            if (moved.size() == kids.size() && a.FPDFPage_RemoveObject(pg, fo)) a.FPDFPageObj_Destroy(fo);
            else again = false;   // a form that keeps some of its objects: not tried again
            changed = true;
            if (moved.size() == kids.size()) again = true;
        }
        if (!again) break;
    }
    if (changed) {
        contentgen::generate(d, pg);
        d.resetText(page);
    }
    return changed;
}

bool tableAt(Document& d, int page, double x, double y, Table& out, std::wstring* why) {
    unwrapTextForms(d, page);
    std::vector<Table> all = detect(d, page);
    const Table* best = nullptr;
    for (const Table& t : all)
        if (x >= t.left() - 2 && x <= t.right() + 2 && y <= t.top() + 2 && y >= t.bottom() - 2)
            if (!best || (t.right() - t.left()) * (t.top() - t.bottom()) < (best->right() - best->left()) * (best->top() - best->bottom())) best = &t;
    if (!best) {
        if (why) *why = L"No table was found there. Click inside a table: one drawn with lines, or text set in columns.";
        return false;
    }
    if (best->cells.empty()) {
        // detected, but sharing objects with the page: say why
        Table t = *best;
        PageText pt = readText(d, page);
        std::vector<Seg> hs, vs; std::vector<Box> fills;
        collectPaths(d.page(page), hs, vs, fills);
        t.objects.clear();
        std::wstring reason;
        if (fillCells(d, t, pt, fills, &reason)) takeGraphics(t, joinLines(hs, 1.0), joinLines(vs, 1.0), fills, d.page(page), &reason);
        if (why) *why = reason.empty() ? L"This table cannot be changed safely." : reason;
        return false;
    }
    if (best->scanned) {   // a scanned table without recognised text would be drawn again empty
        bool any = false;
        for (const Cell& c : best->cells) if (!c.text.empty()) { any = true; break; }
        if (!any) {
            if (why) *why = L"This table is part of a scanned page. Recognise its text first (Edit PDF > Recognise text (OCR)), then edit it.";
            return false;
        }
    }
    out = *best;
    return true;
}

bool tableIn(Document& d, int page, double l, double b, double r, double t, Table& out, std::wstring* why) {
    unwrapTextForms(d, page);
    if (l > r) std::swap(l, r);
    if (b > t) std::swap(b, t);
    // a table found by the search that lies mostly in the box
    for (const Table& x : detect(d, page)) {
        const double ow = std::min(r, x.right()) - std::max(l, x.left()), oh = std::min(t, x.top()) - std::max(b, x.bottom());
        if (ow <= 0 || oh <= 0 || ow * oh < 0.6 * (x.right() - x.left()) * (x.top() - x.bottom())) continue;
        return tableAt(d, page, (x.left() + x.right()) / 2, (x.top() + x.bottom()) / 2, out, why);
    }
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const PageText pt = readText(d, page);
    std::vector<int> inside;
    for (size_t i = 0; i < pt.chars.size(); ++i) {
        const Char& c = pt.chars[i];
        const double cx = (c.l + c.r) / 2, cy = (c.b + c.t) / 2;
        if (cx > l && cx < r && cy > b && cy < t) inside.push_back(static_cast<int>(i));
    }
    std::vector<TLine> lines = linesOf(pt, inside);
    std::sort(lines.begin(), lines.end(), [](const TLine& a, const TLine& c) { return a.base > c.base; });
    if (lines.empty()) {
        if (why) *why = L"There is no text in that box. On a scanned page, recognise the text first (Edit PDF > Recognise text (OCR)).";
        return false;
    }
    Table x;
    x.page = page;
    x.colX = columnsFromText(lines, l, r);
    x.rowY = rowsFromText(lines, t, b);
    x.cells.assign(static_cast<size_t>(x.rows()) * x.cols(), Cell{});
    std::vector<Seg> hs0, vs0;
    std::vector<Box> fills;
    collectPaths(pg, hs0, vs0, fills);
    const std::vector<Seg> hs = joinLines(hs0, 1.0), vs = joinLines(vs0, 1.0);
    initRules(x, false);
    for (int k = 0; k <= x.rows(); ++k) x.hRule[k] = coverage(hs, x.rowY[k], x.left(), x.right(), 3.0) > 0.6;
    for (int k = 0; k <= x.cols(); ++k) x.vRule[k] = coverage(vs, x.colX[k], x.bottom(), x.top(), 3.0) > 0.6;
    lineStyle(x, hs, vs);
    x.foundL = l; x.foundR = r; x.foundT = t; x.foundB = b;
    if (!fillCells(d, x, pt, fills, why) || !takeGraphics(x, hs, vs, fills, pg, why)) return false;
    std::sort(x.objects.begin(), x.objects.end());
    x.objects.erase(std::unique(x.objects.begin(), x.objects.end()), x.objects.end());
    out = std::move(x);
    return true;
}

// ---------------------------------------------------------------- changes to the grid

void anchorOf(const Table& t, int r, int c, int& ar, int& ac) {
    ar = r; ac = c;
    if (t.cells.empty() || !t.at(r, c).covered) return;
    for (int rr = r; rr >= 0; --rr)
        for (int cc = c; cc >= 0; --cc) {
            const Cell& a = t.at(rr, cc);
            if (!a.covered && rr + a.rowSpan > r && cc + a.colSpan > c) { ar = rr; ac = cc; return; }
        }
}

void setBorders(Table& t, Borders b) {
    initRules(t, b == Borders::All || b == Borders::Rows);
    if (b == Borders::Outside || b == Borders::Rows) {
        t.hRule.front() = t.hRule.back() = 1;
        t.vRule.front() = t.vRule.back() = b == Borders::Outside ? 1 : 0;
    }
    if (b == Borders::Rows) for (auto& v : t.vRule) v = 0;
}

Borders bordersOf(const Table& t) {
    auto all = [](const std::vector<char>& v, bool on, size_t from, size_t to) { for (size_t i = from; i < to; ++i) if (static_cast<bool>(v[i]) != on) return false; return true; };
    const size_t H = t.hRule.size(), V = t.vRule.size();
    if (all(t.hRule, true, 0, H) && all(t.vRule, true, 0, V)) return Borders::All;
    if (all(t.hRule, false, 0, H) && all(t.vRule, false, 0, V)) return Borders::None;
    if (all(t.hRule, true, 0, H) && all(t.vRule, false, 0, V)) return Borders::Rows;
    if (t.hRule.front() && t.hRule.back() && t.vRule.front() && t.vRule.back() && all(t.hRule, false, 1, H - 1) && all(t.vRule, false, 1, V - 1)) return Borders::Outside;
    return Borders::All;
}

namespace {
// a cell to start a new row or column from: the style of its neighbour, no text
Cell blankLike(const Cell& c) {
    Cell n;
    n.size = c.size; n.family = c.family; n.bold = c.bold; n.italic = c.italic; n.color = c.color;
    n.fill = c.fill; n.align = c.align; n.valign = c.valign; n.like = c.like;
    return n;
}
} // namespace

void insertRow(Table& t, int at) {
    const int R = t.rows(), C = t.cols();
    at = std::clamp(at, 0, R);
    // the row it copies the look of: the one above (a new row continues the table), the first body row at the top
    const int from = at == 0 ? std::min(1, R - 1) : at - 1;
    const double h = t.rowY[from] - t.rowY[from + 1];
    std::vector<Cell> row(static_cast<size_t>(C));
    for (int c = 0; c < C; ++c) {
        int ar, ac;
        anchorOf(t, from, c, ar, ac);
        row[c] = blankLike(t.at(ar, ac));
    }
    // merged blocks that the new row falls inside grow by one row and cover it
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            Cell& a = t.at(r, c);
            if (a.covered || a.rowSpan < 2 || !(r < at && r + a.rowSpan > at)) continue;
            ++a.rowSpan;
            for (int cc = c; cc < c + a.colSpan; ++cc) { row[cc] = Cell{}; row[cc].covered = true; }
        }
    t.cells.insert(t.cells.begin() + static_cast<std::ptrdiff_t>(at) * C, row.begin(), row.end());
    // the edges below move down by h
    std::vector<double> y(t.rowY.begin(), t.rowY.begin() + at + 1);
    y.push_back(t.rowY[at] - h);
    for (int k = at + 1; k <= R; ++k) y.push_back(t.rowY[k] - h);
    t.rowY = y;
    t.hRule.insert(t.hRule.begin() + at + 1, t.hRule[std::min<size_t>(at + 1, t.hRule.size() - 1)] || t.hRule[at]);
    if (t.hOrig.size() == static_cast<size_t>(R + 1)) t.hOrig.insert(t.hOrig.begin() + at + 1, -1);
}

void deleteRow(Table& t, int r) {
    const int R = t.rows(), C = t.cols();
    if (R <= 1 || r < 0 || r >= R) return;
    for (int c = 0; c < C; ++c) {
        int ar, ac;
        anchorOf(t, r, c, ar, ac);
        Cell& a = t.at(ar, ac);
        if (a.rowSpan < 2) continue;
        if (ar == r) {   // the block's first row goes: the cell below takes over
            Cell next = a;
            --next.rowSpan; next.covered = false;
            for (int cc = ac; cc < ac + a.colSpan; ++cc) t.at(r + 1, cc) = cc == ac ? next : t.at(r + 1, cc);
            a.rowSpan = 1;
        } else if (c == ac) --a.rowSpan;
        c = ac + a.colSpan - 1;
    }
    const double h = t.rowY[r] - t.rowY[r + 1];
    t.cells.erase(t.cells.begin() + static_cast<std::ptrdiff_t>(r) * C, t.cells.begin() + static_cast<std::ptrdiff_t>(r + 1) * C);
    t.rowY.erase(t.rowY.begin() + r + 1);
    for (size_t k = static_cast<size_t>(r) + 1; k < t.rowY.size(); ++k) t.rowY[k] += h;
    t.hRule.erase(t.hRule.begin() + r + 1);
    if (t.hOrig.size() == static_cast<size_t>(R + 1)) t.hOrig.erase(t.hOrig.begin() + r + 1);
}

void insertColumn(Table& t, int at) {
    const int R = t.rows(), C = t.cols();
    at = std::clamp(at, 0, C);
    const int from = at < C ? at : C - 1;
    // the new column takes its width from all columns alike, so the table keeps its width
    const double W = t.right() - t.left(), w = W / (C + 1), k = (W - w) / W;
    std::vector<double> x;
    for (int c = 0; c <= C; ++c) x.push_back(t.left() + (t.colX[c] - t.left()) * k);
    x.insert(x.begin() + at + 1, x[at] + w);
    for (size_t c = static_cast<size_t>(at) + 2; c < x.size(); ++c) x[c] += w;
    std::vector<Cell> cells;
    for (int r = 0; r < R; ++r)
        for (int c = 0; c <= C; ++c) {
            if (c == at) {
                int ar, ac;
                anchorOf(t, r, from, ar, ac);
                Cell n = blankLike(t.at(ar, ac));
                // inside a merged block that spans the new column's place: covered
                for (int cc = 0; cc < C; ++cc) {
                    const Cell& a = t.at(r, cc);
                    int br, bc;
                    anchorOf(t, r, cc, br, bc);
                    const Cell& b = t.at(br, bc);
                    if (bc < at && bc + b.colSpan > at) { n = Cell{}; n.covered = true; break; }
                    (void)a;
                }
                cells.push_back(n);
            }
            if (c < C) cells.push_back(t.at(r, c));
        }
    t.colX = x;
    t.cells = cells;
    for (int r = 0; r < R; ++r)
        for (int c = 0; c <= C; ++c) {
            Cell& a = t.cells[static_cast<size_t>(r) * (C + 1) + c];
            if (!a.covered && a.colSpan > 1 && c < at && c + a.colSpan > at) ++a.colSpan;
        }
    t.vRule.insert(t.vRule.begin() + at + 1, t.vRule[std::min<size_t>(at + 1, t.vRule.size() - 1)] || t.vRule[at]);
    if (t.vOrig.size() == static_cast<size_t>(C + 1)) t.vOrig.insert(t.vOrig.begin() + at + 1, -1);
}

void deleteColumn(Table& t, int c) {
    const int R = t.rows(), C = t.cols();
    if (C <= 1 || c < 0 || c >= C) return;
    for (int r = 0; r < R; ++r) {
        int ar, ac;
        anchorOf(t, r, c, ar, ac);
        Cell& a = t.at(ar, ac);
        if (a.colSpan < 2) continue;
        if (ac == c) {
            Cell next = a;
            --next.colSpan; next.covered = false;
            t.at(ar, c + 1) = next;
            a.colSpan = 1;
        } else if (r == ar) --a.colSpan;
        r = ar + std::max(1, t.at(ar, ac).rowSpan) - 1;
    }
    const double W = t.right() - t.left(), w = t.colX[c + 1] - t.colX[c];
    std::vector<Cell> cells;
    for (int r = 0; r < R; ++r)
        for (int k = 0; k < C; ++k) if (k != c) cells.push_back(t.at(r, k));
    std::vector<double> x;
    for (int k = 0; k <= C; ++k) if (k != c + 1) x.push_back(k <= c ? t.colX[k] : t.colX[k] - w);
    const double k2 = W / std::max(1.0, W - w);   // the others share its width
    for (double& v : x) v = t.left() + (v - t.left()) * k2;
    t.colX = x;
    t.cells = cells;
    t.vRule.erase(t.vRule.begin() + c + 1);
    if (t.vOrig.size() == static_cast<size_t>(C + 1)) t.vOrig.erase(t.vOrig.begin() + c + 1);
}

bool merge(Table& t, int r0, int c0, int r1, int c1) {
    if (r0 > r1) std::swap(r0, r1);
    if (c0 > c1) std::swap(c0, c1);
    if (r0 < 0 || c0 < 0 || r1 >= t.rows() || c1 >= t.cols() || (r0 == r1 && c0 == c1)) return false;
    // every merged block inside must lie wholly inside
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c) {
            int ar, ac;
            anchorOf(t, r, c, ar, ac);
            const Cell& a = t.at(ar, ac);
            if (ar < r0 || ac < c0 || ar + a.rowSpan - 1 > r1 || ac + a.colSpan - 1 > c1) return false;
        }
    std::wstring text;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c) {
            Cell& x = t.at(r, c);
            if (!x.covered && !x.text.empty()) text += (text.empty() ? L"" : L"\n") + x.text;
        }
    Cell& a = t.at(r0, c0);
    a.text = text;
    a.rowSpan = r1 - r0 + 1; a.colSpan = c1 - c0 + 1; a.covered = false;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c) {
            if (r == r0 && c == c0) continue;
            Cell& x = t.at(r, c);
            x = blankLike(a);
            x.covered = true;
        }
    return true;
}

void split(Table& t, int r, int c) {
    int ar, ac;
    anchorOf(t, r, c, ar, ac);
    Cell& a = t.at(ar, ac);
    const int rs = a.rowSpan, cs = a.colSpan;
    for (int rr = ar; rr < ar + rs; ++rr)
        for (int cc = ac; cc < ac + cs; ++cc) {
            if (rr == ar && cc == ac) continue;
            Cell& x = t.at(rr, cc);
            x = blankLike(a);
        }
    a.rowSpan = a.colSpan = 1;
}

void setColumnWidth(Table& t, int c, double w) {
    if (c < 0 || c >= t.cols()) return;
    w = std::max(6.0, w);
    const double d = w - (t.colX[c + 1] - t.colX[c]);
    for (size_t k = static_cast<size_t>(c) + 1; k < t.colX.size(); ++k) t.colX[k] += d;
}

void setRowHeight(Table& t, int r, double h) {
    if (r < 0 || r >= t.rows()) return;
    h = std::max(6.0, h);
    const double d = h - (t.rowY[r] - t.rowY[r + 1]);
    for (size_t k = static_cast<size_t>(r) + 1; k < t.rowY.size(); ++k) t.rowY[k] -= d;
}

void resize(Table& t, double width, double height) {
    const double W = t.right() - t.left(), H = t.top() - t.bottom();
    if (W <= 0 || H <= 0) return;
    const double kx = std::max(10.0, width) / W, ky = std::max(6.0, height) / H;
    for (double& x : t.colX) x = t.left() + (x - t.left()) * kx;
    for (double& y : t.rowY) y = t.top() - (t.top() - y) * ky;
}

void moveTo(Table& t, double left, double top) {
    const double dx = left - t.left(), dy = top - t.top();
    for (double& x : t.colX) x += dx;
    for (double& y : t.rowY) y += dy;
}

// ---------------------------------------------------------------- drawing

namespace {

// Breaks a cell's text into lines that fit `width`: its own line breaks, then between words.
std::vector<std::wstring> wrap(pageobj::TextSetter& ts, const std::wstring& text, double width, bool& ok, std::vector<char>* ends = nullptr) {
    std::vector<std::wstring> out;
    if (ends) ends->clear();
    auto put = [&](const std::wstring& l, bool end) { out.push_back(l); if (ends) ends->push_back(end ? 1 : 0); };
    ok = true;
    size_t p = 0;
    while (p <= text.size()) {
        const size_t e = text.find(L'\n', p);
        const std::wstring para = text.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
        std::wstring line;
        size_t q = 0;
        bool any = false;
        while (q <= para.size()) {
            const size_t sp = para.find(L' ', q);
            const std::wstring word = para.substr(q, sp == std::wstring::npos ? std::wstring::npos : sp - q);
            if (!word.empty()) {
                const std::wstring cand = line.empty() ? word : line + L" " + word;
                const double w = ts.width(cand);
                if (w < 0) { ok = false; return {}; }
                if (w > width && !line.empty()) { put(line, false); line = word; }
                else line = cand;
                any = true;
            }
            if (sp == std::wstring::npos) break;
            q = sp + 1;
        }
        if (any || !line.empty() || para.empty()) put(line, true);
        if (e == std::wstring::npos) break;
        p = e + 1;
    }
    while (!out.empty() && out.back().empty()) { out.pop_back(); if (ends) ends->pop_back(); }
    return out;
}

FPDF_PAGEOBJECT lineObj(double x0, double y0, double x1, double y1, double width, COLORREF c) {
    FPDF_PAGEOBJECT o = api().FPDFPageObj_CreateNewPath(static_cast<float>(x0), static_cast<float>(y0));
    if (!o) return nullptr;
    api().FPDFPath_LineTo(o, static_cast<float>(x1), static_cast<float>(y1));
    api().FPDFPageObj_SetStrokeColor(o, GetRValue(c), GetGValue(c), GetBValue(c), 255);
    api().FPDFPageObj_SetStrokeWidth(o, static_cast<float>(width));
    api().FPDFPageObj_SetLineCap(o, 2);   // square ends: crossing lines meet without gaps
    api().FPDFPath_SetDrawMode(o, FPDF_FILLMODE_NONE, 1);
    return o;
}

FPDF_PAGEOBJECT boxObj(double l, double b, double r, double t, COLORREF c) {
    FPDF_PAGEOBJECT o = api().FPDFPageObj_CreateNewRect(static_cast<float>(l), static_cast<float>(b), static_cast<float>(r - l), static_cast<float>(t - b));
    if (!o) return nullptr;
    api().FPDFPageObj_SetFillColor(o, GetRValue(c), GetGValue(c), GetBValue(c), 255);
    api().FPDFPath_SetDrawMode(o, FPDF_FILLMODE_ALTERNATE, 0);
    return o;
}

bool sameBlock(const Table& t, int r0, int c0, int r1, int c1) {
    int a0, b0, a1, b1;
    anchorOf(t, r0, c0, a0, b0);
    anchorOf(t, r1, c1, a1, b1);
    return a0 == a1 && b0 == b1;
}

bool validLike(Document& d, const Table& t, int like) {
    if (like < 0 || std::find(t.objects.begin(), t.objects.end(), like) == t.objects.end()) return false;
    FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(d.page(t.page), like);
    return o && api().FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_TEXT;
}

void destroyAll(std::vector<FPDF_PAGEOBJECT>& v) { for (FPDF_PAGEOBJECT o : v) if (o) api().FPDFPageObj_Destroy(o); v.clear(); }


// The part of a line drawing outside `box` (page space, a little larger than the table), drawn as it was: a new path
// with the same look holding only the subpaths that are not wholly inside. Null when nothing is outside.
FPDF_PAGEOBJECT outsidePart(FPDF_PAGEOBJECT o, double bl, double bb, double br, double bt, bool& allInside) {
    FS_MATRIX m{1, 0, 0, 1, 0, 0};
    api().FPDFPageObj_GetMatrix(o, &m);
    struct Seg2 { int type; float x, y; bool close; };
    std::vector<std::vector<Seg2>> subs;
    const int n = api().FPDFPath_CountSegments(o);
    for (int k = 0; k < n; ++k) {
        FPDF_PATHSEGMENT sg = api().FPDFPath_GetPathSegment(o, k);
        if (!sg) continue;
        Seg2 x{api().FPDFPathSegment_GetType(sg), 0, 0, api().FPDFPathSegment_GetClose(sg) != 0};
        api().FPDFPathSegment_GetPoint(sg, &x.x, &x.y);
        if (x.type == FPDF_SEGMENT_MOVETO || subs.empty()) subs.push_back({});
        subs.back().push_back(x);
    }
    std::vector<const std::vector<Seg2>*> keep;
    for (const auto& sub : subs) {
        bool inside = true;
        for (const Seg2& x : sub) {
            double px, py;
            apply(m, x.x, x.y, px, py);
            if (px < bl || px > br || py < bb || py > bt) inside = false;
        }
        if (!inside) keep.push_back(&sub);
    }
    allInside = keep.empty();
    if (keep.empty() || keep.size() == subs.size()) return nullptr;
    FPDF_PAGEOBJECT p = api().FPDFPageObj_CreateNewPath(keep[0]->front().x, keep[0]->front().y);
    if (!p) return nullptr;
    bool first = true;
    for (const auto* sub : keep) {
        for (size_t k = 0; k < sub->size(); ++k) {
            const Seg2& x = (*sub)[k];
            if (x.type == FPDF_SEGMENT_MOVETO) { if (!first) api().FPDFPath_MoveTo(p, x.x, x.y); }
            else if (x.type == FPDF_SEGMENT_BEZIERTO && k + 2 < sub->size()) {
                const Seg2& b = (*sub)[k + 1]; const Seg2& c = (*sub)[k + 2];
                api().FPDFPath_BezierTo(p, x.x, x.y, b.x, b.y, c.x, c.y);
                k += 2;
                if (c.close) api().FPDFPath_Close(p);
                first = false;
                continue;
            } else api().FPDFPath_LineTo(p, x.x, x.y);
            if (x.close) api().FPDFPath_Close(p);
            first = false;
        }
    }
    unsigned R, G, B, A;
    if (api().FPDFPageObj_GetFillColor(o, &R, &G, &B, &A)) api().FPDFPageObj_SetFillColor(p, R, G, B, A);
    if (api().FPDFPageObj_GetStrokeColor(o, &R, &G, &B, &A)) api().FPDFPageObj_SetStrokeColor(p, R, G, B, A);
    float w = 1;
    if (api().FPDFPageObj_GetStrokeWidth(o, &w)) api().FPDFPageObj_SetStrokeWidth(p, w);
    api().FPDFPageObj_SetLineCap(p, api().FPDFPageObj_GetLineCap(o));
    api().FPDFPageObj_SetLineJoin(p, api().FPDFPageObj_GetLineJoin(o));
    int fillMode = 0;
    FPDF_BOOL stroke = 0;
    api().FPDFPath_GetDrawMode(o, &fillMode, &stroke);
    api().FPDFPath_SetDrawMode(p, fillMode, stroke);
    api().FPDFPageObj_SetMatrix(p, &m);
    return p;
}

// letters that the fonts of most documents lack, as their plain equivalents (a non-breaking hyphen as a hyphen...)
std::wstring plainLetters(std::wstring s) {
    for (auto& c : s) {
        if (c == 0x2010 || c == 0x2011 || c == 0x2012 || c == 0x2212) c = L'-';
        else if (c == 0x00A0 || c == 0x2007 || c == 0x2009 || c == 0x200A || c == 0x202F) c = L' ';
    }
    s.erase(std::remove_if(s.begin(), s.end(), [](wchar_t c) { return c == 0x200B || c == 0x00AD || c == 0xFEFF; }), s.end());
    return s;
}

} // namespace

bool write(Document& d, const Table& t0, std::wstring* why, double* grewBy, bool* shrunk) {
    if (grewBy) *grewBy = 0;
    if (shrunk) *shrunk = false;
    FPDF_PAGE pg = d.page(t0.page);
    if (!pg || t0.rows() < 1 || t0.cols() < 1 || t0.cells.size() != static_cast<size_t>(t0.rows()) * t0.cols()) {
        if (why) *why = L"The table is not complete.";
        return false;
    }
    const bool found = t0.foundR > t0.foundL;   // a table read from the page (else one made here)
    const double oldL = found ? t0.foundL : t0.left(), oldR = found ? t0.foundR : t0.right();
    const double oldT = found ? t0.foundT : t0.top(), oldB = found ? t0.foundB : t0.bottom();
    const int R = t0.rows(), C = t0.cols();

    // 0. cells that keep their own text objects: not changed, and their text still fits across the cell
    std::vector<char> keep(static_cast<size_t>(R) * C, 0);
    std::set<int> keptObjs;
    // Cells whose text objects are shared (Word prints a row's cells in one text object) are kept or set again together:
    // kept when none of them changed, so an edit in one row does not set the other rows again (losing their superscripts,
    // spacing and alignment as found).
    std::vector<char> unchanged(static_cast<size_t>(R) * C, 0);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            const Cell& x = t0.at(r, c);
            if (x.covered || !x.found || x.objs.empty() || t0.scanned) continue;
            const bool same = x.text == x.origText && std::fabs(x.size - x.origSize) < 0.01 && x.family == x.origFamily && x.bold == x.origBold &&
                              x.italic == x.origItalic && x.color == x.origColor && x.align == x.origAlign && x.valign == x.origValign;
            const double w = t0.colX[c + x.colSpan] - t0.colX[c];
            if (!same || w + 0.5 < x.textW + std::min(x.fR - x.fL - x.textW, 2.0)) continue;
            unchanged[static_cast<size_t>(r) * C + c] = 1;
        }
    std::vector<int> group(static_cast<size_t>(R) * C);
    for (size_t k = 0; k < group.size(); ++k) group[k] = static_cast<int>(k);
    std::function<int(int)> root = [&](int k) { return group[size_t(k)] == k ? k : group[size_t(k)] = root(group[size_t(k)]); };
    {
        std::map<int, int> ownerOf;   // text object -> a cell holding it
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c) {
                const Cell& x = t0.at(r, c);
                if (x.covered) continue;
                for (int o : x.objs) {
                    auto it = ownerOf.find(o);
                    if (it == ownerOf.end()) ownerOf[o] = r * C + c;
                    else group[size_t(root(r * C + c))] = root(it->second);
                }
            }
    }
    std::vector<char> groupOk(group.size(), 1);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            const Cell& x = t0.at(r, c);
            if (x.covered) continue;
            if (!unchanged[size_t(r) * C + c] && !x.objs.empty()) groupOk[size_t(root(r * C + c))] = 0;
        }
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            const Cell& x = t0.at(r, c);
            if (!unchanged[size_t(r) * C + c] || !groupOk[size_t(root(r * C + c))]) continue;
            keep[static_cast<size_t>(r) * C + c] = 1;
            keptObjs.insert(x.objs.begin(), x.objs.end());
        }

    d.beginChange(L"Edit a table");
    pg = d.page(t0.page);
    std::vector<FPDF_PAGEOBJECT> text, graphics;
    struct Laid { int r, c; std::vector<std::wstring> lines; std::unique_ptr<pageobj::TextSetter> ts; double lineStep, size; std::vector<char> ends; };
    std::vector<Laid> laid;
    auto fail = [&](const std::wstring& reason) {
        destroyAll(text); destroyAll(graphics);
        laid.clear();
        d.rollbackChange();
        if (why) *why = reason;
        return false;
    };

    // what lies below the table on the page (moved down when the table gets longer)
    std::vector<FPDF_PAGEOBJECT> below;
    double nextTop = -1e9, lowest = 1e9, floor = 2;
    {
        const double footZone = 0.07 * d.pageSize(t0.page).h;   // page numbers and footers stay where they are
        const int n = api().FPDFPage_CountObjects(pg);
        for (int i = 0; i < n; ++i) {
            if (std::find(t0.objects.begin(), t0.objects.end(), i) != t0.objects.end()) continue;
            FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
            float l, b, r, tp;
            if (!o || !api().FPDFPageObj_GetBounds(o, &l, &b, &r, &tp)) continue;
            if (tp > oldB + 0.5 || r < oldL || l > oldR) continue;
            if (api().FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_IMAGE && (r - l) * (tp - b) > 0.5 * d.pageSize(t0.page).w * d.pageSize(t0.page).h) continue;
            if (tp < footZone) { floor = std::max(floor, static_cast<double>(tp) + 4); continue; }
            below.push_back(o);
            nextTop = std::max(nextTop, static_cast<double>(tp));
            lowest = std::min(lowest, static_cast<double>(b));
        }
    }

    // 1. the cells that are set again, broken into lines; a row grows only for lines a cell did not have before. When
    // the table cannot grow (no room below), the text set again is made smaller until it fits.
    Table t;
    double grew = 0;
    bool fits = false, smaller = false;
    for (const double scale : {1.0, 0.94, 0.88, 0.82, 0.76, 0.7, 0.64}) {
        t = t0;
        if (t.hRule.size() != static_cast<size_t>(R + 1) || t.vRule.size() != static_cast<size_t>(C + 1)) initRules(t, true);
        laid.clear();
        std::vector<double> need(static_cast<size_t>(R), 0);
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c) {
                const Cell& cell = t.at(r, c);
                if (cell.covered || keep[static_cast<size_t>(r) * C + c]) continue;
                const std::wstring cellText = plainLetters(cell.text);
                if (cellText.find_first_not_of(L" \n") == std::wstring::npos) continue;
                const bool edited = !cell.found || cell.text != cell.origText || std::fabs(cell.size - cell.origSize) > 0.01;
                const double size = edited ? std::max(5.0, cell.size * scale) : cell.size;
                pageobj::TextStyle st;
                st.family = cell.family; st.bold = cell.bold; st.italic = cell.italic; st.sizePt = size; st.color = cell.color;
                // recognised words on a scan are invisible and stretched to the picture: new text uses a standard font
                const int like = !t.scanned && validLike(d, t0, cell.like) ? cell.like : -1;
                auto ts = std::make_unique<pageobj::TextSetter>(d, t.page, like, st);
                if (!ts->ok()) return fail(L"The text of a cell could not be set in its font.");
                const double w = t.colX[c + cell.colSpan] - t.colX[c] - 2 * t.pad;
                bool ok = true;
                std::vector<char> ends;
                std::vector<std::wstring> lines = wrap(*ts, cellText, std::max(4.0, w), ok, &ends);
                if (!ok) return fail(L"A cell holds letters that cannot be set (only letters, numbers and symbols of Western languages can be typed).");
                const double step = size * 1.2;
                const int last = r + cell.rowSpan - 1;
                double have = 0;
                for (int k = r; k <= last; ++k) have += t.rowY[k] - t.rowY[k + 1];
                const double body = lines.empty() ? 0 : size + (lines.size() - 1) * step;
                double h;
                if (cell.found && !cell.origText.empty()) {   // as much more room as the text needs more than it had
                    const int was = std::max(1, cell.lines);
                    const double before = cell.origSize + (was - 1) * cell.origSize * 1.2;
                    h = have + std::max(0.0, body - before);
                } else h = body + 2;   // a new or empty cell: its text and a little room
                if (h > have + 0.01) need[last] = std::max(need[last], (t.rowY[last] - t.rowY[last + 1]) + (h - have));
                laid.push_back({r, c, std::move(lines), std::move(ts), step, size, std::move(ends)});
            }
        grew = 0;
        for (int r = 0; r < R; ++r) {
            const double h = t.rowY[r] - t.rowY[r + 1];
            if (need[r] > h + 0.01) { const double d2 = need[r] - h; grew += d2; for (int k = r + 1; k <= R; ++k) t.rowY[k] -= d2; }
        }
        const double extra = oldB - t.bottom();
        const bool crowded = !below.empty() && oldB - nextTop < extra + 4;
        const bool canPush = !t.scanned && !below.empty() && lowest - extra >= floor;
        fits = t.bottom() >= 2 && (extra <= 0.01 || !crowded || canPush);
        if (fits) break;
        smaller = true;
    }
    if (!fits) return fail(L"The text no longer fits in its cells, and there is no room on this page for the table to grow. Make the text shorter or the columns wider.");
    if (shrunk) *shrunk = smaller;
    for (Laid& L : laid) {
        const Cell& cell = t.at(L.r, L.c);
        const double cl = t.colX[L.c] + t.pad, cr = t.colX[L.c + cell.colSpan] - t.pad;
        const double vpad = std::min(t.pad, 1.5);
        const double ct = t.rowY[L.r] - vpad, cb = t.rowY[L.r + cell.rowSpan] + vpad;
        const double block = L.lines.empty() ? 0 : (L.lines.size() - 1) * L.lineStep + L.size * 1.05;
        double top = ct;
        if (cell.valign == 1) top = (ct + cb) / 2 + block / 2;
        else if (cell.valign == 2) top = cb + block;
        double y = top - L.size * 0.82;   // the first baseline: about the letters' height below the top
        // a cell whose text still takes the lines it had sits where it was found
        const double rowTop = t.rowY[L.r], rowsH = rowTop - t.rowY[L.r + cell.rowSpan];
        if (cell.baseOff > 0 && static_cast<int>(L.lines.size()) == cell.lines && cell.baseOff < rowsH) y = rowTop - cell.baseOff;
        for (size_t li = 0; li < L.lines.size(); ++li) {
            const std::wstring& line = L.lines[li];
            if (!line.empty()) {
                const double w = L.ts->width(line);
                double x = cl;
                if (cell.align == 1) x = (cl + cr) / 2 - w / 2;
                else if (cell.align == 2) x = cr - w;
                // justified: every line but a paragraph's last reaches both edges
                const bool spread = cell.align == 3 && li < L.ends.size() && !L.ends[li];
                std::vector<FPDF_PAGEOBJECT> objs = spread ? L.ts->make(line, x, y, cr - cl) : L.ts->make(line, x, y);
                if (objs.empty()) return fail(L"The text of a cell could not be set.");
                text.insert(text.end(), objs.begin(), objs.end());
            }
            y -= L.lineStep;
        }
    }
    // 2. colours and lines: while the grid is as it was found (only text changed), its own drawings stay untouched
    bool sameGrid = !t.scanned && t.foundColX.size() == t.colX.size() && t.foundRowY.size() == t.rowY.size() && t.foundH == t.hRule && t.foundV == t.vRule &&
                    std::abs(t0.lineWidth - t.lineWidth) < 1e-9;
    for (size_t k = 0; sameGrid && k < t.colX.size(); ++k) sameGrid = std::fabs(t.colX[k] - t.foundColX[k]) < 0.01;
    for (size_t k = 0; sameGrid && k < t.rowY.size(); ++k) sameGrid = std::fabs(t.rowY[k] - t.foundRowY[k]) < 0.01;
    for (int r = 0; sameGrid && r < R; ++r)
        for (int c = 0; sameGrid && c < C; ++c) {
            const Cell& x = t.at(r, c);
            if (x.covered) continue;
            sameGrid = x.found && x.rowSpan == x.fRowSpan && x.colSpan == x.fColSpan && x.fill == x.fFill;
        }
    for (int r = 0; r < R && !sameGrid; ++r)
        for (int c = 0; c < C; ++c) {
            const Cell& cell = t.at(r, c);
            if (cell.covered || cell.fill == kNoFill) continue;
            if (FPDF_PAGEOBJECT o = boxObj(t.colX[c], t.rowY[r + cell.rowSpan], t.colX[c + cell.colSpan], t.rowY[r], cell.fill)) graphics.push_back(o);
        }
    // each piece of line looks as the piece found in its place did (an added row or column: like its neighbour)
    auto origOf = [](const std::vector<int>& orig, int k, int n) {
        if (orig.size() != static_cast<size_t>(n + 1)) return -1;
        for (int d2 = 0; d2 <= n; ++d2) {   // the nearest edge that was found
            if (k - d2 >= 0 && orig[static_cast<size_t>(k - d2)] >= 0) return orig[static_cast<size_t>(k - d2)];
            if (k + d2 <= n && orig[static_cast<size_t>(k + d2)] >= 0) return orig[static_cast<size_t>(k + d2)];
        }
        return -1;
    };
    struct Look { double w; COLORREF c; bool operator==(const Look& o) const { return std::fabs(w - o.w) < 0.01 && c == o.c; } };
    auto look = [&](bool horizontal, int edge, int span) -> Look {   // span: the column (or row) the piece runs along
        const int oe = origOf(horizontal ? t.hOrig : t.vOrig, edge, horizontal ? R : C);
        const int o0 = origOf(horizontal ? t.vOrig : t.hOrig, span, horizontal ? C : R), o1 = origOf(horizontal ? t.vOrig : t.hOrig, span + 1, horizontal ? C : R);
        if (oe < 0 || o0 < 0 || o1 < 0 || t.foundRowY.empty()) return {t.lineWidth, t.lineColor};
        const double at = horizontal ? t.foundRowY[static_cast<size_t>(oe)] : t.foundColX[static_cast<size_t>(oe)];
        const double m = horizontal ? (t.foundColX[static_cast<size_t>(o0)] + t.foundColX[static_cast<size_t>(o1)]) / 2
                                    : (t.foundRowY[static_cast<size_t>(o0)] + t.foundRowY[static_cast<size_t>(o1)]) / 2;
        const Table::Piece* best = nullptr;
        for (const auto& pc : t.pieces)
            if (pc.horizontal == horizontal && std::fabs(pc.at - at) < 1.6 + pc.width / 2 && pc.a <= m + 0.5 && pc.b >= m - 0.5)
                if (!best || std::fabs(pc.at - at) < std::fabs(best->at - at)) best = &pc;
        return best ? Look{best->width, best->colour} : Look{t.lineWidth, t.lineColor};
    };
    for (int r = 0; r <= R && !sameGrid; ++r) {
        if (!t.hRule[r]) continue;
        int start = -1;
        Look cur{0, 0};
        for (int c = 0; c <= C; ++c) {
            const bool draw = c < C && (r == 0 || r == R || !sameBlock(t, r - 1, c, r, c));
            const Look lk = draw ? look(true, r, c) : Look{0, 0};
            if (start >= 0 && (!draw || !(lk == cur))) {
                if (FPDF_PAGEOBJECT o = lineObj(t.colX[start], t.rowY[r], t.colX[c], t.rowY[r], cur.w, cur.c)) graphics.push_back(o);
                start = -1;
            }
            if (draw && start < 0) { start = c; cur = lk; }
        }
    }
    for (int c = 0; c <= C && !sameGrid; ++c) {
        if (!t.vRule[c]) continue;
        int start = -1;
        Look cur{0, 0};
        for (int r = 0; r <= R; ++r) {
            const bool draw = r < R && (c == 0 || c == C || !sameBlock(t, r, c - 1, r, c));
            const Look lk = draw ? look(false, c, r) : Look{0, 0};
            if (start >= 0 && (!draw || !(lk == cur))) {
                if (FPDF_PAGEOBJECT o = lineObj(t.colX[c], t.rowY[start], t.colX[c], t.rowY[r], cur.w, cur.c)) graphics.push_back(o);
                start = -1;
            }
            if (draw && start < 0) { start = r; cur = lk; }
        }
    }
    // 3. the page: what is below moves down by as much as the table grew; the cells kept move with their rows and
    // columns; drawings shared with the rest of the page keep their part outside; the rest of the old table goes
    const double extra = oldB - t.bottom();
    if (extra > 0.01 && !t.scanned && !below.empty() && lowest - extra >= floor)
        for (FPDF_PAGEOBJECT o : below) api().FPDFPageObj_Transform(o, 1, 0, 0, 1, 0, -extra);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            if (!keep[static_cast<size_t>(r) * C + c]) continue;
            const Cell& x = t.at(r, c);
            const double nl = t.colX[c], nr = t.colX[c + x.colSpan], nt = t.rowY[r], nb = t.rowY[r + x.rowSpan];
            const double dx = x.origAlign == 0 ? nl - x.fL : x.origAlign == 2 ? nr - x.fR : ((nl + nr) - (x.fL + x.fR)) / 2;
            const double dy = x.origValign == 0 ? nt - x.fT : x.origValign == 2 ? nb - x.fB : ((nt + nb) - (x.fT + x.fB)) / 2;
            if (std::fabs(dx) < 0.01 && std::fabs(dy) < 0.01) continue;
            for (int i : x.objs) if (FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i)) api().FPDFPageObj_Transform(o, 1, 0, 0, 1, dx, dy);
        }
    std::vector<FPDF_PAGEOBJECT> gone;
    for (int i : t0.objects) {
        if (keptObjs.count(i)) continue;
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
        if (!o) continue;
        if (sameGrid && api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;   // the lines and colours stay
        if (std::find(t0.sharedPaths.begin(), t0.sharedPaths.end(), i) != t0.sharedPaths.end()) {
            bool allInside = false;
            FPDF_PAGEOBJECT rest = outsidePart(o, std::min(oldL, t.left()) - 2, std::min(oldB, t.bottom()) - 2, std::max(oldR, t.right()) + 2, std::max(oldT, t.top()) + 2, allInside);
            if (!rest && !allInside) continue;   // nothing of it inside after all: it stays
            if (rest) api().FPDFPage_InsertObjectAtIndex(pg, rest, static_cast<size_t>(i));
        }
        gone.push_back(o);
    }
    for (FPDF_PAGEOBJECT o : gone) if (api().FPDFPage_RemoveObject(pg, o)) api().FPDFPageObj_Destroy(o);
    if (t.scanned) {
        const double l = std::min(oldL, t.left()) - 2, r = std::max(oldR, t.right()) + 2, tp = std::max(oldT, t.top()) + 2, b = std::min(oldB, t.bottom()) - 2;
        if (FPDF_PAGEOBJECT o = boxObj(l, b, r, tp, RGB(255, 255, 255))) api().FPDFPage_InsertObject(pg, o);
    }
    for (FPDF_PAGEOBJECT o : graphics) api().FPDFPage_InsertObject(pg, o);
    for (FPDF_PAGEOBJECT o : text) api().FPDFPage_InsertObject(pg, o);
    graphics.clear(); text.clear();
    laid.clear();   // the setters' fonts are released after their objects are on the page
    contentgen::generate(d, pg);
    d.resetText(t.page);
    if (grewBy) *grewBy = grew;
    return true;
}

bool remove(Document& d, const Table& t) {
    FPDF_PAGE pg = d.page(t.page);
    if (!pg) return false;
    d.beginChange(L"Delete a table");
    pg = d.page(t.page);
    std::vector<int> objs = t.objects;
    std::sort(objs.rbegin(), objs.rend());
    bool any = false;
    for (int i : objs) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
        if (o && api().FPDFPage_RemoveObject(pg, o)) { api().FPDFPageObj_Destroy(o); any = true; }
    }
    if (t.scanned) {
        if (FPDF_PAGEOBJECT o = boxObj(t.left() - 2, t.bottom() - 2, t.right() + 2, t.top() + 2, RGB(255, 255, 255))) { api().FPDFPage_InsertObject(pg, o); any = true; }
    }
    if (!any) { d.cancelChange(); return false; }
    contentgen::generate(d, pg);
    d.resetText(t.page);
    return true;
}

} // namespace tables
