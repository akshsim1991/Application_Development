#include "annot.hpp"
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cwchar>
#include <cmath>

namespace annot {
namespace {

using pdfium::api;

std::vector<unsigned short> wide(const std::wstring& s) {
    std::vector<unsigned short> v(s.begin(), s.end());
    v.push_back(0);
    return v;
}

std::wstring authorName() {
#ifdef _WIN32
    wchar_t buf[256]; DWORD n = 256;
    return GetUserNameW(buf, &n) ? std::wstring(buf) : std::wstring();
#else
    const char* u = std::getenv("USER");
    return u ? platform::wide(u) : std::wstring();
#endif
}

std::wstring pdfDateNow() {
    wchar_t b[32];
#ifdef _WIN32
    SYSTEMTIME t; GetLocalTime(&t);
    swprintf(b, 32, L"D:%04d%02d%02d%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
#else
    const time_t now = time(nullptr);
    tm t{}; localtime_r(&now, &t);
    swprintf(b, 32, L"D:%04d%02d%02d%02d%02d%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
#endif
    return b;
}

// Common bookkeeping for a freshly created annotation: print flag, author, date, comment.
void stamp(FPDF_ANNOTATION a, const std::wstring& comment) {
    const auto& p = api();
    p.FPDFAnnot_SetFlags(a, 4);   // print
    auto date = wide(pdfDateNow());
    p.FPDFAnnot_SetStringValue(a, "M", date.data());
    std::wstring who = authorName();
    if (!who.empty()) { auto w = wide(who); p.FPDFAnnot_SetStringValue(a, "T", w.data()); }
    if (!comment.empty()) { auto c = wide(comment); p.FPDFAnnot_SetStringValue(a, "Contents", c.data()); }
}

// On callouts: "tipX tipY atX atY colour", so the text can be changed and the callout drawn again in the same place
constexpr const char* kCalloutKey = "FeatherCallout";

void setRect(FPDF_ANNOTATION a, const RectD& r) {
    FS_RECTF f;
    f.left = static_cast<float>(r.left);   f.right = static_cast<float>(r.right);
    f.bottom = static_cast<float>(r.bottom); f.top = static_cast<float>(r.top);
    api().FPDFAnnot_SetRect(a, &f);
}

void setColor(FPDF_ANNOTATION a, FPDFANNOT_COLORTYPE type, COLORREF c) {
    api().FPDFAnnot_SetColor(a, type, GetRValue(c), GetGValue(c), GetBValue(c), 255);
}

// Runs `fn` on a new annotation of `subtype`; on failure the undo step is dropped again.
template <typename Fn>
bool create(Document& d, int page, FPDF_ANNOTATION_SUBTYPE subtype, Fn fn) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    d.beginChange();
    pg = d.page(page);   // beginChange() does not reload, but keep the handle fresh anyway
    FPDF_ANNOTATION a = api().FPDFPage_CreateAnnot(pg, subtype);
    if (!a) { d.cancelChange(); return false; }
    bool ok = fn(a);
    api().FPDFPage_CloseAnnot(a);
    if (!ok) { d.cancelChange(); return false; }
    return true;
}

} // namespace

bool addMarkup(Document& d, int page, Markup kind, const std::vector<RectD>& rects, COLORREF color,
               const std::wstring& comment) {
    if (rects.empty()) return false;
    FPDF_ANNOTATION_SUBTYPE st = kind == Markup::Highlight ? FPDF_ANNOT_HIGHLIGHT
                               : kind == Markup::Underline ? FPDF_ANNOT_UNDERLINE
                               : kind == Markup::Strikethrough ? FPDF_ANNOT_STRIKEOUT : FPDF_ANNOT_SQUIGGLY;
    return create(d, page, st, [&](FPDF_ANNOTATION a) {
        RectD box = rects.front();
        for (const RectD& r : rects) {
            FS_QUADPOINTSF q;   // top-left, top-right, bottom-left, bottom-right
            q.x1 = static_cast<float>(r.left);  q.y1 = static_cast<float>(r.top);
            q.x2 = static_cast<float>(r.right); q.y2 = static_cast<float>(r.top);
            q.x3 = static_cast<float>(r.left);  q.y3 = static_cast<float>(r.bottom);
            q.x4 = static_cast<float>(r.right); q.y4 = static_cast<float>(r.bottom);
            api().FPDFAnnot_AppendAttachmentPoints(a, &q);
            box = {std::min(box.left, r.left), std::min(box.bottom, r.bottom), std::max(box.right, r.right), std::max(box.top, r.top)};
        }
        setRect(a, box);
        setColor(a, FPDFANNOT_COLORTYPE_Color, color);
        stamp(a, comment);
        return true;
    });
}

bool addShape(Document& d, int page, Shape kind, const RectD& r, COLORREF color, double width) {
    if (r.right - r.left < 1 || r.top - r.bottom < 1) return false;
    return create(d, page, kind == Shape::Rectangle ? FPDF_ANNOT_SQUARE : FPDF_ANNOT_CIRCLE, [&](FPDF_ANNOTATION a) {
        setRect(a, r);
        setColor(a, FPDFANNOT_COLORTYPE_Color, color);
        api().FPDFAnnot_SetBorder(a, 0, 0, static_cast<float>(width));
        stamp(a, L"");
        return true;
    });
}

bool addInk(Document& d, int page, const std::vector<std::vector<PointD>>& strokes, COLORREF color, double width) {
    if (strokes.empty()) return false;
    return create(d, page, FPDF_ANNOT_INK, [&](FPDF_ANNOTATION a) {
        RectD box{1e9, 1e9, -1e9, -1e9};
        for (const auto& s : strokes) {
            if (s.size() < 2) continue;
            std::vector<FS_POINTF> pts;
            for (const PointD& p : s) {
                pts.push_back({static_cast<float>(p.x), static_cast<float>(p.y)});
                box = {std::min(box.left, p.x), std::min(box.bottom, p.y), std::max(box.right, p.x), std::max(box.top, p.y)};
            }
            if (api().FPDFAnnot_AddInkStroke(a, pts.data(), pts.size()) < 0) return false;
        }
        if (box.left > box.right) return false;
        const double pad = width + 1;
        setRect(a, {box.left - pad, box.bottom - pad, box.right + pad, box.top + pad});
        setColor(a, FPDFANNOT_COLORTYPE_Color, color);
        api().FPDFAnnot_SetBorder(a, 0, 0, static_cast<float>(width));
        stamp(a, L"");
        return true;
    });
}

bool addNote(Document& d, int page, PointD at, const std::wstring& text, COLORREF color) {
    return create(d, page, FPDF_ANNOT_TEXT, [&](FPDF_ANNOTATION a) {
        setRect(a, {at.x, at.y - 20, at.x + 20, at.y});   // the icon hangs below-right of the click
        setColor(a, FPDFANNOT_COLORTYPE_Color, color);
        stamp(a, text);
        return true;
    });
}

std::vector<Info> list(Document& d, int page) {
    std::vector<Info> out;
    FPDF_PAGE pg = d.page(page);
    if (!pg) return out;
    const auto& p = api();
    int n = p.FPDFPage_GetAnnotCount(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_ANNOTATION a = p.FPDFPage_GetAnnot(pg, i);
        if (!a) continue;
        Info inf;
        inf.index = i;
        inf.subtype = p.FPDFAnnot_GetSubtype(a);
        FS_RECTF r{};
        if (p.FPDFAnnot_GetRect(a, &r)) inf.rect = {r.left, r.bottom, r.right, r.top};
        inf.callout = inf.subtype == FPDF_ANNOT_STAMP && p.FPDFAnnot_GetStringValue(a, kCalloutKey, nullptr, 0) >= 4;
        unsigned long bytes = p.FPDFAnnot_GetStringValue(a, "Contents", nullptr, 0);
        if (bytes >= 4) {
            std::vector<unsigned short> buf(bytes / 2 + 1);
            p.FPDFAnnot_GetStringValue(a, "Contents", buf.data(), bytes);
            inf.contents.assign(buf.begin(), buf.begin() + (bytes / 2 - 1));
        }
        p.FPDFPage_CloseAnnot(a);
        out.push_back(std::move(inf));
    }
    return out;
}

int hitTest(Document& d, int page, PointD pt) {
    auto all = list(d, page);
    for (auto it = all.rbegin(); it != all.rend(); ++it) {
        if (it->subtype == FPDF_ANNOT_LINK || it->subtype == FPDF_ANNOT_POPUP || it->subtype == FPDF_ANNOT_WIDGET) continue;
        const RectD& r = it->rect;
        if (pt.x >= r.left - 2 && pt.x <= r.right + 2 && pt.y >= r.bottom - 2 && pt.y <= r.top + 2) return it->index;
    }
    return -1;
}

bool remove(Document& d, int page, int index) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || index < 0 || index >= api().FPDFPage_GetAnnotCount(pg)) return false;
    d.beginChange();
    pg = d.page(page);
    if (!api().FPDFPage_RemoveAnnot(pg, index)) { d.cancelChange(); return false; }
    return true;
}

bool setContents(Document& d, int page, int index, const std::wstring& text) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    FPDF_ANNOTATION a = api().FPDFPage_GetAnnot(pg, index);
    if (!a) return false;
    d.beginChange();
    auto w = wide(text);
    bool ok = api().FPDFAnnot_SetStringValue(a, "Contents", w.data()) != 0;
    api().FPDFPage_CloseAnnot(a);
    if (!ok) d.cancelChange();
    return ok;
}

const wchar_t* typeName(int st) {
    switch (st) {
        case FPDF_ANNOT_TEXT: return L"note";
        case FPDF_ANNOT_HIGHLIGHT: return L"highlight";
        case FPDF_ANNOT_UNDERLINE: return L"underline";
        case FPDF_ANNOT_STRIKEOUT: return L"strikethrough";
        case FPDF_ANNOT_SQUIGGLY: return L"squiggly underline";
        case FPDF_ANNOT_SQUARE: return L"rectangle";
        case FPDF_ANNOT_CIRCLE: return L"ellipse";
        case FPDF_ANNOT_INK: return L"drawing";
        case FPDF_ANNOT_STAMP: return L"stamp";
        case FPDF_ANNOT_FREETEXT: return L"text box";
        default: return L"annotation";
    }
}

bool hasComment(int st) {
    return st == FPDF_ANNOT_TEXT || st == FPDF_ANNOT_HIGHLIGHT || st == FPDF_ANNOT_UNDERLINE ||
           st == FPDF_ANNOT_STRIKEOUT || st == FPDF_ANNOT_SQUIGGLY;
}

std::wstring notesSummary(Document& d) {
    std::wstring out;
    int total = 0;
    const auto& api = pdfium::api();
    for (int p = 0; p < d.pageCount(); ++p) {
        std::wstring block;
        FPDF_TEXTPAGE tp = d.text(p);
        for (const Info& a : list(d, p)) {
            const bool markup = a.subtype == FPDF_ANNOT_HIGHLIGHT || a.subtype == FPDF_ANNOT_UNDERLINE || a.subtype == FPDF_ANNOT_STRIKEOUT || a.subtype == FPDF_ANNOT_SQUIGGLY;
            if (!markup && a.subtype != FPDF_ANNOT_TEXT) continue;
            std::wstring marked;
            if (markup && tp) {   // the words under the markup
                std::vector<unsigned short> buf(2048);
                int n = api.FPDFText_GetBoundedText(tp, a.rect.left, a.rect.top, a.rect.right, a.rect.bottom, buf.data(), 2047);
                if (n > 0) { marked.assign(buf.begin(), buf.begin() + n); marked.resize(wcslen(marked.c_str())); }   // the count may include the terminator
                while (!marked.empty() && (marked.back() == L'\r' || marked.back() == L'\n' || marked.back() == L' ')) marked.pop_back();
                for (auto& ch : marked) if (ch == L'\r' || ch == L'\n') ch = L' ';
            }
            if (a.contents.empty() && marked.empty()) continue;
            std::wstring c = a.contents;
            for (auto& ch : c) if (ch == L'\r') ch = L'\n';
            block += L"* **" + std::wstring(markup ? (a.subtype == FPDF_ANNOT_HIGHLIGHT ? L"Highlight" : a.subtype == FPDF_ANNOT_UNDERLINE ? L"Underline" : a.subtype == FPDF_ANNOT_STRIKEOUT ? L"Strikethrough" : L"Squiggly underline") : L"Note") + L"**";
            if (!marked.empty()) block += L" \"" + marked + L"\"";
            if (!c.empty()) block += L"\n  *Comment:* " + c;
            block += L"\n";
            ++total;
        }
        if (!block.empty()) out += L"## Page " + std::to_wstring(p + 1) + L"\n\n" + block + L"\n";
    }
    if (!total) return L"";
    return L"# Notes on " + d.title() + L"\n\n" + out;
}

} // namespace annot

namespace annot {
bool addLink(Document& d, int page, const RectD& r, const std::string& uri) {
    if (r.right - r.left < 1 || r.top - r.bottom < 1 || uri.empty()) return false;
    return create(d, page, FPDF_ANNOT_LINK, [&](FPDF_ANNOTATION a) {
        setRect(a, r);
        api().FPDFAnnot_SetBorder(a, 0, 0, 0);
        return api().FPDFAnnot_SetURI(a, uri.c_str()) != 0;
    });
}
} // namespace annot

namespace annot {
namespace {
FPDF_PAGEOBJECT pathFrom(const std::vector<PointD>& pts, bool close) {
    const auto& p = api();
    FPDF_PAGEOBJECT o = p.FPDFPageObj_CreateNewPath(static_cast<float>(pts[0].x), static_cast<float>(pts[0].y));
    for (size_t i = 1; i < pts.size(); ++i) p.FPDFPath_LineTo(o, static_cast<float>(pts[i].x), static_cast<float>(pts[i].y));
    if (close) p.FPDFPath_Close(o);
    return o;
}
}

bool addPolygon(Document& d, int page, const std::vector<PointD>& pts, COLORREF color, double width) {
    if (pts.size() < 3) return false;
    std::vector<PointD> closed = pts;
    closed.push_back(pts.front());
    return addInk(d, page, {closed}, color, width);
}

bool addCloud(Document& d, int page, const RectD& r, COLORREF color, double width) {
    if (r.right - r.left < 8 || r.top - r.bottom < 8) return false;
    // scallops along the four edges, each a half circle bulging outwards, sampled as points
    std::vector<PointD> pts;
    const PointD corners[5] = {{r.left, r.bottom}, {r.right, r.bottom}, {r.right, r.top}, {r.left, r.top}, {r.left, r.bottom}};
    const double target = std::max(8.0, std::min(r.right - r.left, r.top - r.bottom) / 4);
    for (int e = 0; e < 4; ++e) {
        const PointD a = corners[e], b = corners[e + 1];
        const double len = std::hypot(b.x - a.x, b.y - a.y);
        const int n = std::max(2, static_cast<int>(len / target));
        const double ux = (b.x - a.x) / len, uy = (b.y - a.y) / len;
        const double nx = uy, ny = -ux;   // outwards for a counter-clockwise walk
        const double seg = len / n, rad = seg / 2;
        for (int k = 0; k < n; ++k) {
            const double cx = a.x + ux * (seg * k + rad), cy = a.y + uy * (seg * k + rad);
            for (int s = 0; s <= 8; ++s) {
                const double t = 3.14159265358979 * s / 8;   // from the start of the bump to its end, through the outside
                pts.push_back({cx - ux * rad * std::cos(t) + nx * rad * std::sin(t), cy - uy * rad * std::cos(t) + ny * rad * std::sin(t)});
            }
        }
    }
    return addInk(d, page, {pts}, color, width);
}

namespace {
// replace >= 0: the callout takes the place of that annotation, in the same undo step
bool makeCallout(Document& d, int page, PointD tip, PointD at, const std::wstring& text, COLORREF color, int replace) {
    if (text.empty()) return false;
    const auto& p = api();
    // lines of text that fit 220 pt
    auto widthOf = [&](const std::wstring& s) {
        FPDF_PAGEOBJECT o = p.FPDFPageObj_NewTextObj(d.handle(), "Helvetica", 10.0f);
        auto w = wide(s);
        p.FPDFText_SetText(o, w.data());
        float l, b, rr, t;
        double v = p.FPDFPageObj_GetBounds(o, &l, &b, &rr, &t) ? rr - l : s.size() * 5.0;
        p.FPDFPageObj_Destroy(o);
        return v;
    };
    std::vector<std::wstring> lines;
    for (size_t pos = 0; pos <= text.size();) {
        size_t e = text.find_first_of(L"\r\n", pos);
        std::wstring para = text.substr(pos, e == std::wstring::npos ? std::wstring::npos : e - pos);
        std::wstring cur;
        size_t q = 0;
        while (q <= para.size()) {
            size_t sp = para.find(L' ', q);
            std::wstring word = para.substr(q, sp == std::wstring::npos ? std::wstring::npos : sp - q);
            std::wstring cand = cur.empty() ? word : cur + L" " + word;
            if (!cur.empty() && widthOf(cand) > 220) { lines.push_back(cur); cur = word; } else cur = cand;
            if (sp == std::wstring::npos) break;
            q = sp + 1;
        }
        lines.push_back(cur);
        if (e == std::wstring::npos) break;
        pos = e + 1;
        if (pos < text.size() && text[pos - 1] == L'\r' && text[pos] == L'\n') ++pos;
    }
    double w = 0;
    for (auto& l : lines) w = std::max(w, widthOf(l));
    const double pad = 5, lh = 12.5, bw = w + 2 * pad, bh = lines.size() * lh + 2 * pad - 2.5;
    const bool right = at.x >= tip.x;
    const RectD box{right ? at.x : at.x - bw, at.y - bh / 2, right ? at.x + bw : at.x, at.y + bh / 2};
    const PointD attach{right ? box.left : box.right, at.y};
    RectD all{std::min(box.left, tip.x) - 4, std::min(box.bottom, tip.y) - 4, std::max(box.right, tip.x) + 4, std::max(box.top, tip.y) + 4};
    return create(d, page, FPDF_ANNOT_STAMP, [&](FPDF_ANNOTATION a) {
        setRect(a, all);
        // the box
        FPDF_PAGEOBJECT rect = pathFrom({{box.left, box.bottom}, {box.right, box.bottom}, {box.right, box.top}, {box.left, box.top}}, true);
        p.FPDFPageObj_SetFillColor(rect, 255, 255, 240, 255);
        p.FPDFPageObj_SetStrokeColor(rect, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        p.FPDFPageObj_SetStrokeWidth(rect, 1);
        p.FPDFPath_SetDrawMode(rect, FPDF_FILLMODE_ALTERNATE, 1);
        p.FPDFAnnot_AppendObject(a, rect);
        // the leader line and its arrowhead
        FPDF_PAGEOBJECT line = pathFrom({attach, tip}, false);
        p.FPDFPageObj_SetStrokeColor(line, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        p.FPDFPageObj_SetStrokeWidth(line, 1);
        p.FPDFPath_SetDrawMode(line, 0, 1);
        p.FPDFAnnot_AppendObject(a, line);
        const double ang = std::atan2(attach.y - tip.y, attach.x - tip.x), hl = 8, hw = 0.4;
        FPDF_PAGEOBJECT head = pathFrom({tip, {tip.x + hl * std::cos(ang + hw), tip.y + hl * std::sin(ang + hw)}, {tip.x + hl * std::cos(ang - hw), tip.y + hl * std::sin(ang - hw)}}, true);
        p.FPDFPageObj_SetFillColor(head, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        p.FPDFPath_SetDrawMode(head, FPDF_FILLMODE_ALTERNATE, 0);
        p.FPDFAnnot_AppendObject(a, head);
        // the text
        double y = box.top - pad - 8.5;
        for (auto& l : lines) {
            FPDF_PAGEOBJECT t = p.FPDFPageObj_NewTextObj(d.handle(), "Helvetica", 10.0f);
            auto wl = wide(l);
            p.FPDFText_SetText(t, wl.data());
            p.FPDFPageObj_SetFillColor(t, 0x20, 0x20, 0x20, 255);
            p.FPDFPageObj_Transform(t, 1, 0, 0, 1, box.left + pad, y);
            p.FPDFAnnot_AppendObject(a, t);
            y -= lh;
        }
        stamp(a, text);
        wchar_t spec[96];
        swprintf(spec, 96, L"%.2f %.2f %.2f %.2f %06lX", tip.x, tip.y, at.x, at.y, static_cast<unsigned long>(color & 0xFFFFFF));
        auto ws = wide(spec);
        p.FPDFAnnot_SetStringValue(a, kCalloutKey, ws.data());
        return replace < 0 || p.FPDFPage_RemoveAnnot(d.page(page), replace);
    });
}
} // namespace

bool addCallout(Document& d, int page, PointD tip, PointD at, const std::wstring& text, COLORREF color) {
    return makeCallout(d, page, tip, at, text, color, -1);
}

bool setCalloutText(Document& d, int page, int index, const std::wstring& text) {
    FPDF_PAGE pg = d.page(page);
    if (!pg || text.empty()) return false;
    const auto& p = api();
    FPDF_ANNOTATION a = p.FPDFPage_GetAnnot(pg, index);
    if (!a) return false;
    std::vector<unsigned short> buf(p.FPDFAnnot_GetStringValue(a, kCalloutKey, nullptr, 0) / 2 + 1);
    p.FPDFAnnot_GetStringValue(a, kCalloutKey, buf.data(), static_cast<unsigned long>(buf.size() * 2));
    p.FPDFPage_CloseAnnot(a);
    std::wstring spec(buf.begin(), buf.end());
    PointD tip{}, at{};
    unsigned long colour = 0;
    if (swscanf(spec.c_str(), L"%lf %lf %lf %lf %lx", &tip.x, &tip.y, &at.x, &at.y, &colour) != 5) return false;
    return makeCallout(d, page, tip, at, text, static_cast<COLORREF>(colour), index);
}
} // namespace annot
