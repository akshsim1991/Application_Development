#include "tables.hpp"
#include "contentgen.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>

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
        c.size = api().FPDFText_GetFontSize(tp, i);
        FPDF_PAGEOBJECT o = api().FPDFText_GetTextObject(tp, i);
        auto it = index.find(o);
        c.obj = it == index.end() ? -1 : it->second;
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
};

std::vector<TLine> linesOf(const PageText& pt, const std::vector<int>& idx) {
    std::vector<int> order = idx;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const Char& p = pt.chars[a]; const Char& q = pt.chars[b];
        return std::fabs(p.oy - q.oy) > 0.3 * std::max(p.size, q.size) ? p.oy > q.oy : p.ox < q.ox;
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
    for (TLine& L : lines) {
        std::sort(L.chars.begin(), L.chars.end(), [&](int a, int b) { return pt.chars[a].ox < pt.chars[b].ox; });
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
                for (size_t k = 0; k < ln.chars.size(); ++k) {
                    const Char& ch = pt.chars[ln.chars[k]];
                    s += ch.ch;
                    if (ch.spaceAfter && k + 1 < ln.chars.size()) s += L' ';
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
            if (std::fabs(tg - bg) < std::max(2.0, 0.2 * (tg + bg))) cell.valign = 1;
            else cell.valign = tg < bg ? 0 : 2;
            if (cell.align == 0 && lg > 0) { padSum += lg; ++padN; }
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
            auto spread = [](const std::vector<double>& v) { return *std::max_element(v.begin(), v.end()) - *std::min_element(v.begin(), v.end()); };
            std::vector<double> ms;
            for (size_t k = 0; k < ls.size(); ++k) ms.push_back((ls[k] + rs[k]) / 2);
            const double sl = spread(ls), sr = spread(rs), sm = spread(ms);
            // the edge that varies least (letters' side bearings make even a shared edge vary by about a point)
            const int align = sl <= std::min(sr, sm) + 0.5 ? 0 : sr <= sm + 0.5 ? 2 : 1;
            for (int r = 0; r < R; ++r) if (!t.at(r, c).covered && t.at(r, c).colSpan == 1) t.at(r, c).align = align;
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
        if (!inside(l, b, r, tp)) {
            if (why) *why = L"This table's lines are drawn together with other parts of the page, so it cannot be changed safely.";
            return false;
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
    return edges;
}

// Row edges between text lines (each line a row), from `top` down to `bottom`.
std::vector<double> rowsFromText(const std::vector<TLine>& lines, double top, double bottom) {
    std::vector<double> edges{top};
    for (size_t i = 1; i < lines.size(); ++i) edges.push_back((lines[i - 1].b + lines[i].t) / 2);
    edges.push_back(bottom);
    return edges;
}

void markMerges(Table& t, const std::vector<Seg>& hs, const std::vector<Seg>& vs) {
    const int R = t.rows(), C = t.cols();
    // cells joined where the line between them is missing
    std::vector<int> parent(static_cast<size_t>(R) * C);
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> find = [&](int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); };
    for (int r = 0; r < R; ++r)
        for (int c = 0; c + 1 < C; ++c)
            if (coverage(vs, t.colX[c + 1], t.rowY[r + 1], t.rowY[r], 1.6) < 0.5) parent[find(r * C + c)] = find(r * C + c + 1);
    for (int r = 0; r + 1 < R; ++r)
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
            if (!o || api().FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_IMAGE) continue;
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
    for (auto& kv : comps) {
        const auto& hi = kv.second.first;
        const auto& vi = kv.second.second;
        if (hi.size() < 2 || vi.size() < 2) continue;
        std::vector<Seg> ch, cv;
        for (int i : hi) ch.push_back(hs[i]);
        for (int j : vi) cv.push_back(vs[j]);
        std::vector<double> xs, ys;
        for (const Seg& s : cv) xs.push_back(s.at);
        for (const Seg& s : ch) ys.push_back(s.at);
        std::vector<double> colX = cluster(xs, 1.6), rowY = cluster(ys, 1.6);
        std::reverse(rowY.begin(), rowY.end());
        cleanEdges(colX, 3.0);
        cleanEdges(rowY, 3.0);
        if (colX.size() < 2 || rowY.size() < 2) continue;
        Table t;
        t.page = page;
        t.scanned = scanned;
        t.colX = colX; t.rowY = rowY;
        if (t.right() - t.left() < 20 || t.top() - t.bottom() < 8) continue;
        // only the frame: the columns or rows come from the text inside
        std::vector<int> inside;
        for (size_t i = 0; i < pt.chars.size(); ++i) {
            const Char& c = pt.chars[i];
            const double cx = (c.l + c.r) / 2, cy = (c.b + c.t) / 2;
            if (cx > t.left() && cx < t.right() && cy < t.top() && cy > t.bottom()) inside.push_back(static_cast<int>(i));
        }
        const std::vector<TLine> lines = linesOf(pt, inside);
        if (t.colX.size() == 2 && lines.size() >= 2) t.colX = columnsFromText(lines, t.left(), t.right());
        if (t.rowY.size() == 2 && lines.size() >= 2) t.rowY = rowsFromText(lines, t.top(), t.bottom());
        if (t.rows() * t.cols() < 2) continue;   // one box: a frame, not a table
        t.cells.assign(static_cast<size_t>(t.rows()) * t.cols(), Cell{});
        markMerges(t, ch, cv);
        initRules(t, true);
        // the lines that are drawn
        for (int r = 0; r <= t.rows(); ++r) t.hRule[r] = coverage(ch, t.rowY[r], t.left(), t.right(), 1.6) > 0.2;
        for (int c = 0; c <= t.cols(); ++c) t.vRule[c] = coverage(cv, t.colX[c], t.bottom(), t.top(), 1.6) > 0.2;
        lineStyle(t, ch, cv);
        for (int i : inside) used[i] = 1;
        found.push_back(std::move(t));
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
        if (!fillCells(d, t, pt, fills, nullptr)) { t.cells.clear(); out.push_back(std::move(t)); continue; }
        if (!takeGraphics(t, hs, vs, fills, pg, nullptr)) { t.cells.clear(); out.push_back(std::move(t)); continue; }
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

bool tableAt(Document& d, int page, double x, double y, Table& out, std::wstring* why) {
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
    const int from = at < R ? at : R - 1;   // the row it copies the look of
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
std::vector<std::wstring> wrap(pageobj::TextSetter& ts, const std::wstring& text, double width, bool& ok) {
    std::vector<std::wstring> out;
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
                if (w > width && !line.empty()) { out.push_back(line); line = word; }
                else line = cand;
                any = true;
            }
            if (sp == std::wstring::npos) break;
            q = sp + 1;
        }
        if (any || !line.empty() || para.empty()) out.push_back(line);
        if (e == std::wstring::npos) break;
        p = e + 1;
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
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

} // namespace

bool write(Document& d, const Table& t0, std::wstring* why, double* grewBy) {
    Table t = t0;
    if (grewBy) *grewBy = 0;
    FPDF_PAGE pg = d.page(t.page);
    if (!pg || t.rows() < 1 || t.cols() < 1 || t.cells.size() != static_cast<size_t>(t.rows()) * t.cols()) {
        if (why) *why = L"The table is not complete.";
        return false;
    }
    if (t.hRule.size() != static_cast<size_t>(t.rows() + 1) || t.vRule.size() != static_cast<size_t>(t.cols() + 1)) initRules(t, true);
    const double oldL = t0.left(), oldR = t0.right(), oldT = t0.top(), oldB = t0.bottom();
    d.beginChange();
    pg = d.page(t.page);
    std::vector<FPDF_PAGEOBJECT> text, graphics;
    auto fail = [&](const std::wstring& reason) {
        destroyAll(text); destroyAll(graphics);
        d.rollbackChange();
        if (why) *why = reason;
        return false;
    };

    // 1. the text of each cell, broken into lines; rows grow when a cell needs more room
    struct Laid { int r, c; std::vector<std::wstring> lines; std::unique_ptr<pageobj::TextSetter> ts; double lineStep; };
    std::vector<Laid> laid;
    const int R = t.rows(), C = t.cols();
    std::vector<double> need(static_cast<size_t>(R), 0);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            const Cell& cell = t.at(r, c);
            if (cell.covered || cell.text.find_first_not_of(L" \n") == std::wstring::npos) continue;
            pageobj::TextStyle st;
            st.family = cell.family; st.bold = cell.bold; st.italic = cell.italic; st.sizePt = cell.size; st.color = cell.color;
            // recognised words on a scan are invisible and stretched to the picture: new text uses a standard font
            const int like = !t.scanned && validLike(d, t0, cell.like) ? cell.like : -1;
            auto ts = std::make_unique<pageobj::TextSetter>(d, t.page, like, st);
            if (!ts->ok()) return fail(L"The text of a cell could not be set in its font.");
            const double w = t.colX[c + cell.colSpan] - t.colX[c] - 2 * t.pad;
            bool ok = true;
            std::vector<std::wstring> lines = wrap(*ts, cell.text, std::max(4.0, w), ok);
            if (!ok) return fail(L"A cell holds letters that cannot be set (only letters, numbers and symbols of Western languages can be typed).");
            const double step = cell.size * 1.2;
            // a row grows only when its text does not fit: a little room above and below, not the side padding
            const double vpad = std::min(t.pad, 1.5);
            const double h = 2 * vpad + (lines.empty() ? 0 : (lines.size() - 1) * step + cell.size * 1.05);
            const int last = r + cell.rowSpan - 1;
            double have = 0;
            for (int k = r; k <= last; ++k) have += t.rowY[k] - t.rowY[k + 1];
            if (h > have) need[last] = std::max(need[last], (t.rowY[last] - t.rowY[last + 1]) + (h - have));
            laid.push_back({r, c, std::move(lines), std::move(ts), step});
        }
    double grew = 0;
    for (int r = 0; r < R; ++r) {
        const double h = t.rowY[r] - t.rowY[r + 1];
        if (need[r] > h + 0.01) { const double d2 = need[r] - h; grew += d2; for (int k = r + 1; k <= R; ++k) t.rowY[k] -= d2; }
    }
    if (t.bottom() < 2) return fail(L"The table no longer fits on the page. Make the text shorter or the columns wider.");
    for (Laid& L : laid) {
        const Cell& cell = t.at(L.r, L.c);
        const double cl = t.colX[L.c] + t.pad, cr = t.colX[L.c + cell.colSpan] - t.pad;
        const double vpad = std::min(t.pad, 1.5);
        const double ct = t.rowY[L.r] - vpad, cb = t.rowY[L.r + cell.rowSpan] + vpad;
        const double block = L.lines.empty() ? 0 : (L.lines.size() - 1) * L.lineStep + cell.size * 1.05;
        double top = ct;
        if (cell.valign == 1) top = (ct + cb) / 2 + block / 2;
        else if (cell.valign == 2) top = cb + block;
        double y = top - cell.size * 0.82;   // the first baseline: about the letters' height below the top
        for (const std::wstring& line : L.lines) {
            if (!line.empty()) {
                const double w = L.ts->width(line);
                double x = cl;
                if (cell.align == 1) x = (cl + cr) / 2 - w / 2;
                else if (cell.align == 2) x = cr - w;
                std::vector<FPDF_PAGEOBJECT> objs = L.ts->make(line, x, y);
                if (objs.empty()) return fail(L"The text of a cell could not be set.");
                text.insert(text.end(), objs.begin(), objs.end());
            }
            y -= L.lineStep;
        }
    }
    // 2. colours and lines
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            const Cell& cell = t.at(r, c);
            if (cell.covered || cell.fill == kNoFill) continue;
            if (FPDF_PAGEOBJECT o = boxObj(t.colX[c], t.rowY[r + cell.rowSpan], t.colX[c + cell.colSpan], t.rowY[r], cell.fill)) graphics.push_back(o);
        }
    for (int r = 0; r <= R; ++r) {
        if (!t.hRule[r]) continue;
        int start = -1;
        for (int c = 0; c <= C; ++c) {
            const bool draw = c < C && (r == 0 || r == R || !sameBlock(t, r - 1, c, r, c));
            if (draw && start < 0) start = c;
            if (!draw && start >= 0) {
                if (FPDF_PAGEOBJECT o = lineObj(t.colX[start], t.rowY[r], t.colX[c], t.rowY[r], t.lineWidth, t.lineColor)) graphics.push_back(o);
                start = -1;
            }
        }
    }
    for (int c = 0; c <= C; ++c) {
        if (!t.vRule[c]) continue;
        int start = -1;
        for (int r = 0; r <= R; ++r) {
            const bool draw = r < R && (c == 0 || c == C || !sameBlock(t, r, c - 1, r, c));
            if (draw && start < 0) start = r;
            if (!draw && start >= 0) {
                if (FPDF_PAGEOBJECT o = lineObj(t.colX[c], t.rowY[start], t.colX[c], t.rowY[r], t.lineWidth, t.lineColor)) graphics.push_back(o);
                start = -1;
            }
        }
    }
    // 3. the old table goes (last index first, so the others keep theirs); a scanned one is covered with white
    std::vector<int> objs = t0.objects;
    std::sort(objs.rbegin(), objs.rend());
    for (int i : objs) {
        FPDF_PAGEOBJECT o = api().FPDFPage_GetObject(pg, i);
        if (o && api().FPDFPage_RemoveObject(pg, o)) api().FPDFPageObj_Destroy(o);
    }
    if (t.scanned) {
        const double l = std::min(oldL, t.left()) - 2, r = std::max(oldR, t.right()) + 2, tp = std::max(oldT, t.top()) + 2, b = std::min(oldB, t.bottom()) - 2;
        if (FPDF_PAGEOBJECT o = boxObj(l, b, r, tp, RGB(255, 255, 255))) api().FPDFPage_InsertObject(pg, o);
    }
    for (FPDF_PAGEOBJECT o : graphics) api().FPDFPage_InsertObject(pg, o);
    for (FPDF_PAGEOBJECT o : text) api().FPDFPage_InsertObject(pg, o);
    graphics.clear(); text.clear();
    laid.clear();   // the setters' fonts are released after their objects are on the page
    contentgen::generate(d.handle(), pg);
    d.resetText(t.page);
    if (grewBy) *grewBy = grew;
    return true;
}

bool remove(Document& d, const Table& t) {
    FPDF_PAGE pg = d.page(t.page);
    if (!pg) return false;
    d.beginChange();
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
    contentgen::generate(d.handle(), pg);
    d.resetText(t.page);
    return true;
}

} // namespace tables
