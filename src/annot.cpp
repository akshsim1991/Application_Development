#include "annot.hpp"
#include <algorithm>
#include <ctime>

namespace annot {
namespace {

using pdfium::api;

std::vector<unsigned short> wide(const std::wstring& s) {
    std::vector<unsigned short> v(s.begin(), s.end());
    v.push_back(0);
    return v;
}

std::wstring authorName() {
    wchar_t buf[256]; DWORD n = 256;
    return GetUserNameW(buf, &n) ? std::wstring(buf) : std::wstring();
}

std::wstring pdfDateNow() {
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t b[32];
    swprintf(b, 32, L"D:%04d%02d%02d%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
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

} // namespace annot
