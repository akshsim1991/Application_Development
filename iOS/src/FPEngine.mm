#import "FPEngine.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "about.hpp"
#include "annot.hpp"
#include "document.hpp"
#include "equation.hpp"
#include "pageobjects.hpp"
#include "pagecolours.hpp"
#include "pdfium_api.hpp"
#include "platform.hpp"
#include "search.hpp"

@implementation FPMatch
@end
@implementation FPBookmark
@end

namespace {
std::recursive_mutex g_lock;
bool g_previewTex = false;   // the equation last opened is in TeX's fonts: its preview in Latin Modern

struct Doc {
    Document d;
    pageobj::ParagraphHit hit;   // the paragraph found by the last paragraphAt
    equation::Found eq;          // the equation found by the last equationAt
};
Doc* docOf(FPDoc h) { return static_cast<Doc*>(h); }

// NSString (UTF-16) <-> std::wstring (UTF-32 on Apple systems)
std::wstring ws(NSString* s) {
    std::wstring out;
    if (!s) return out;
    NSData* d = [s dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
    const uint32_t* p = static_cast<const uint32_t*>(d.bytes);
    for (NSUInteger i = 0; i < d.length / 4; ++i) out.push_back(static_cast<wchar_t>(p[i]));
    return out;
}
NSString* ns(const std::wstring& s) {
    if (s.empty()) return @"";
    std::vector<uint32_t> u(s.begin(), s.end());
    return [[NSString alloc] initWithBytes:u.data() length:u.size() * 4 encoding:NSUTF32LittleEndianStringEncoding] ?: @"";
}
COLORREF colorref(uint32_t rgb) { return RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }
uint32_t rgbOf(COLORREF c) { return (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c); }

pagecolours::Mode modeOf(FPColours m) {
    switch (m) {
        case FPColoursNight: return pagecolours::Mode::Dark;
        case FPColoursDimmed: return pagecolours::Mode::Dimmed;
        case FPColoursBlackboard: return pagecolours::Mode::BlackboardInverted;
        case FPColoursBlackboardOriginal: return pagecolours::Mode::BlackboardOriginal;
        default: return pagecolours::Mode::Normal;
    }
}

// The letter at a point, else the nearest one (a finger seldom lands on a letter); -1 when the page has no text.
int nearestChar(FPDF_TEXTPAGE tp, double x, double y) {
    const auto& api = pdfium::api();
    const int at = api.FPDFText_GetCharIndexAtPos(tp, x, y, 4, 4);
    if (at >= 0) return at;
    int best = -1;
    double bestD = 1e18;
    const int n = api.FPDFText_CountChars(tp);
    for (int i = 0; i < n; ++i) {
        double l, r, b, t;
        if (!api.FPDFText_GetCharBox(tp, i, &l, &r, &b, &t) || r <= l) continue;
        const double dx = x < l ? l - x : x > r ? x - r : 0, dy = y < b ? b - y : y > t ? y - t : 0;
        const double dist = dx * dx + 9 * dy * dy;
        if (dist < bestD) { bestD = dist; best = i; }
    }
    return best;
}
bool charRange(Document& d, int page, CGPoint a0, CGPoint b0, int& a, int& b) {
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return false;
    a = nearestChar(tp, a0.x, a0.y);
    b = nearestChar(tp, b0.x, b0.y);
    if (a < 0 || b < 0) return false;
    if (a > b) std::swap(a, b);
    return true;
}
NSValue* box(double l, double b, double r, double t) { return [NSValue valueWithCGRect:CGRectMake(l, b, r - l, t - b)]; }
} // namespace

#define LOCK std::lock_guard<std::recursive_mutex> guard(g_lock)

@implementation FPEngine

+ (NSString*)start {
    LOCK;
    std::wstring err;
    if (!pdfium::load(err)) return err.empty() ? @"The PDF library could not be loaded." : ns(err);
    return nil;
}

+ (FPDoc)open:(NSString*)path password:(NSString*)password result:(int*)result {
    LOCK;
    auto doc = std::make_unique<Doc>();
    const OpenResult r = doc->d.open(ws(path), platform::utf8(ws(password)));
    if (result) *result = static_cast<int>(r);
    if (r != OpenResult::Ok) return nullptr;
    return doc.release();
}

+ (NSString*)describe:(int)result name:(NSString*)name { return ns(Document::describe(static_cast<OpenResult>(result), ws(name))); }
+ (void)close:(FPDoc)doc { LOCK; delete docOf(doc); }
+ (int)pageCount:(FPDoc)doc { LOCK; return doc ? docOf(doc)->d.pageCount() : 0; }

+ (CGSize)pageSize:(FPDoc)doc page:(int)page {
    LOCK;
    if (!doc || page < 0 || page >= docOf(doc)->d.pageCount()) return CGSizeMake(612, 792);
    const PageSize& s = docOf(doc)->d.pageSize(page);
    return CGSizeMake(s.w, s.h);
}

+ (CGImageRef)render:(FPDoc)doc page:(int)page part:(CGRect)part full:(CGSize)full colours:(FPColours)colours {
    LOCK;
    const int w = static_cast<int>(std::ceil(part.size.width)), h = static_cast<int>(std::ceil(part.size.height));
    if (!doc || page < 0 || page >= docOf(doc)->d.pageCount() || w <= 0 || h <= 0 || static_cast<long long>(w) * h > 40000000LL) return nullptr;
    const int stride = w * 4;
    std::vector<unsigned char> buf(static_cast<size_t>(stride) * h);
    const auto& api = pdfium::api();
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, buf.data(), stride);
    if (!bmp) return nullptr;
    api.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
    Document& d = docOf(doc)->d;
    const int sx = -static_cast<int>(std::lround(part.origin.x)), sy = -static_cast<int>(std::lround(part.origin.y));
    const int fw = static_cast<int>(std::lround(full.width)), fh = static_cast<int>(std::lround(full.height));
    d.renderPage(page, bmp, sx, sy, fw, fh, true);
    const pagecolours::Mode m = modeOf(colours);
    if (m != pagecolours::Mode::Normal) {
        std::vector<pagecolours::Box> keep;
        if (m == pagecolours::Mode::BlackboardOriginal) keep = pagecolours::pictureBoxes(d, page, sx, sy, fw, fh);
        pagecolours::apply(m, buf.data(), w, h, stride, keep);
    }
    api.FPDFBitmap_Destroy(bmp);
    // BGRA in memory is what Core Graphics calls 32-bit little-endian with the alpha first
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(buf.data(), w, h, 8, stride, cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGImageRef img = ctx ? CGBitmapContextCreateImage(ctx) : nullptr;
    if (ctx) CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    return img;
}

+ (CGPoint)pageToView:(FPDoc)doc page:(int)page size:(CGSize)size point:(CGPoint)p {
    LOCK;
    FPDF_PAGE pg = doc ? docOf(doc)->d.page(page) : nullptr;
    if (!pg) return CGPointZero;
    constexpr int S = 16;   // FPDF_PageToDevice gives whole pixels: work at 16 times the size
    int dx = 0, dy = 0;
    pdfium::api().FPDF_PageToDevice(pg, 0, 0, static_cast<int>(size.width * S), static_cast<int>(size.height * S), 0, p.x, p.y, &dx, &dy);
    return CGPointMake(static_cast<double>(dx) / S, static_cast<double>(dy) / S);
}

+ (CGPoint)viewToPage:(FPDoc)doc page:(int)page size:(CGSize)size point:(CGPoint)p {
    LOCK;
    FPDF_PAGE pg = doc ? docOf(doc)->d.page(page) : nullptr;
    if (!pg) return CGPointZero;
    constexpr int S = 16;
    double px = 0, py = 0;
    pdfium::api().FPDF_DeviceToPage(pg, 0, 0, static_cast<int>(size.width * S), static_cast<int>(size.height * S), 0,
                                    static_cast<int>(std::lround(p.x * S)), static_cast<int>(std::lround(p.y * S)), &px, &py);
    return CGPointMake(px, py);
}

+ (NSArray<NSValue*>*)selectionRects:(FPDoc)doc page:(int)page from:(CGPoint)a to:(CGPoint)b {
    LOCK;
    NSMutableArray* out = [NSMutableArray array];
    int i0 = 0, i1 = 0;
    if (doc && charRange(docOf(doc)->d, page, a, b, i0, i1)) {
        FPDF_TEXTPAGE tp = docOf(doc)->d.text(page);
        const auto& api = pdfium::api();
        const int n = api.FPDFText_CountRects(tp, i0, i1 - i0 + 1);
        for (int i = 0; i < n; ++i) {
            double l, t, r, bo;
            if (api.FPDFText_GetRect(tp, i, &l, &t, &r, &bo)) [out addObject:box(l, bo, r, t)];
        }
    }
    return out;
}

+ (NSString*)selectionText:(FPDoc)doc page:(int)page from:(CGPoint)a to:(CGPoint)b {
    LOCK;
    int i0 = 0, i1 = 0;
    if (!doc || !charRange(docOf(doc)->d, page, a, b, i0, i1)) return @"";
    return ns(docOf(doc)->d.textRange(page, i0, i1 - i0 + 1));
}

+ (NSArray<NSValue*>*)editableBlocks:(FPDoc)doc page:(int)page {
    LOCK;
    NSMutableArray* out = [NSMutableArray array];
    if (doc) for (const auto& b : pageobj::editableBlocks(docOf(doc)->d, page)) [out addObject:box(b.left, b.bottom, b.right, b.top)];
    return out;
}

+ (NSString*)paragraphAt:(FPDoc)doc page:(int)page point:(CGPoint)p info:(FPParagraphInfo*)info {
    LOCK;
    if (!doc) return nil;
    Doc& d = *docOf(doc);
    d.hit = pageobj::paragraphAt(d.d, page, p.x, p.y);
    if (!d.hit.found) return nil;
    if (info) {
        info->size = d.hit.style.sizePt; info->bold = d.hit.style.bold; info->italic = d.hit.style.italic;
        info->family = static_cast<int>(d.hit.style.family); info->color = rgbOf(d.hit.style.color);
        info->lines = d.hit.lines; info->align = static_cast<int>(d.hit.align);
    }
    return ns(d.hit.text);
}

+ (NSString*)replaceParagraph:(FPDoc)doc text:(NSString*)text family:(int)family bold:(BOOL)bold italic:(BOOL)italic size:(double)size color:(uint32_t)color {
    LOCK;
    if (!doc || !docOf(doc)->hit.found) return @"Tap the paragraph again.";
    Doc& d = *docOf(doc);
    pageobj::TextStyle st = d.hit.style;
    st.family = static_cast<pageobj::Family>(std::clamp(family, 0, 2));
    st.bold = bold; st.italic = italic;
    st.sizePt = size > 1 ? size : st.sizePt;
    st.color = colorref(color);
    std::wstring why;
    const bool ok = pageobj::replaceParagraph(d.d, d.hit, ws(text), st, d.hit.align, &why);
    d.hit = pageobj::ParagraphHit{};   // the page changed: the next edit finds its paragraph again
    if (ok) return nil;
    return why.empty() ? @"The paragraph could not be changed." : ns(why);
}

+ (BOOL)addText:(FPDoc)doc page:(int)page point:(CGPoint)p text:(NSString*)text size:(double)size color:(uint32_t)color {
    LOCK;
    if (!doc) return NO;
    pageobj::TextStyle st;
    st.sizePt = size > 1 ? size : 12;
    st.color = colorref(color);
    if (!pageobj::addTextBox(docOf(doc)->d, page, p.x, p.y, ws(text), st)) return NO;
    docOf(doc)->d.resetText(page);
    return YES;
}

+ (NSString*)equationAt:(FPDoc)doc page:(int)page point:(CGPoint)p any:(BOOL)any size:(double*)size uncertain:(BOOL*)uncertain {
    LOCK;
    if (!doc) return nil;
    Doc& d = *docOf(doc);
    d.eq = equation::at(d.d, page, p.x, p.y, any);
    g_previewTex = d.eq.found && d.eq.tex;
    if (!d.eq.found) return nil;
    if (size) *size = d.eq.size;
    if (uncertain) *uncertain = d.eq.uncertain;
    return ns(d.eq.source);
}

+ (NSString*)replaceEquation:(FPDoc)doc source:(NSString*)source size:(double)size {
    LOCK;
    if (!doc || !docOf(doc)->eq.found) return @"Tap the equation again.";
    Doc& d = *docOf(doc);
    std::wstring why;
    const bool ok = equation::replace(d.d, d.eq, ws(source), size, &why);
    d.eq = equation::Found{};
    d.hit = pageobj::ParagraphHit{};
    if (ok) return nil;
    return why.empty() ? @"The equation could not be changed." : ns(why);
}

+ (NSString*)insertEquation:(FPDoc)doc page:(int)page point:(CGPoint)p source:(NSString*)source size:(double)size {
    LOCK;
    if (!doc) return @"No document.";
    Doc& d = *docOf(doc);
    std::wstring why;
    const bool ok = equation::insert(d.d, page, p.x, p.y, ws(source), size, RGB(0, 0, 0), &why);
    d.hit = pageobj::ParagraphHit{};
    d.eq = equation::Found{};
    if (ok) return nil;
    return why.empty() ? @"The equation could not be added." : ns(why);
}

+ (UIImage*)equationPreview:(NSString*)source size:(double)size scale:(double)scale error:(NSString**)error {
    LOCK;
    std::vector<unsigned char> px;
    int w = 0, h = 0;
    std::wstring err;
    if (!equation::preview(ws(source), size, scale, px, w, h, &err, g_previewTex)) {
        if (error) *error = ns(err);
        return nil;
    }
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(px.data(), w, h, 8, w * 4, cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGImageRef img = ctx ? CGBitmapContextCreateImage(ctx) : nullptr;
    if (ctx) CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    if (!img) return nil;
    UIImage* out = [UIImage imageWithCGImage:img scale:scale / 1.0 orientation:UIImageOrientationUp];
    CGImageRelease(img);
    return out;
}

+ (NSString*)scriptText:(NSString*)text kind:(int)kind {
    const std::wstring t = ws(text);
    return ns(equation::scriptKind(t) == kind ? equation::fromScript(t) : equation::toScript(t, kind));
}

+ (BOOL)addMarkup:(FPDoc)doc page:(int)page kind:(FPMarkup)kind rects:(NSArray<NSValue*>*)rects color:(uint32_t)color {
    LOCK;
    if (!doc || rects.count == 0) return NO;
    std::vector<annot::RectD> r;
    for (NSValue* v in rects) { const CGRect c = v.CGRectValue; r.push_back({CGRectGetMinX(c), CGRectGetMinY(c), CGRectGetMaxX(c), CGRectGetMaxY(c)}); }
    const annot::Markup m = kind == FPMarkupUnderline ? annot::Markup::Underline : kind == FPMarkupStrikeOut ? annot::Markup::Strikethrough : annot::Markup::Highlight;
    return annot::addMarkup(docOf(doc)->d, page, m, r, colorref(color));
}

+ (BOOL)addInk:(FPDoc)doc page:(int)page points:(NSArray<NSValue*>*)points color:(uint32_t)color width:(double)width {
    LOCK;
    if (!doc || points.count == 0) return NO;
    std::vector<annot::PointD> pts;
    for (NSValue* v in points) { const CGPoint p = v.CGPointValue; pts.push_back({p.x, p.y}); }
    if (pts.size() == 1) pts.push_back({pts[0].x + 0.5, pts[0].y});   // a dot
    return annot::addInk(docOf(doc)->d, page, {pts}, colorref(color), width);
}

+ (BOOL)addNote:(FPDoc)doc page:(int)page point:(CGPoint)p text:(NSString*)text {
    LOCK;
    return doc && annot::addNote(docOf(doc)->d, page, {p.x, p.y}, ws(text), RGB(0xFF, 0xC1, 0x07));
}

+ (int)annotAt:(FPDoc)doc page:(int)page point:(CGPoint)p kind:(NSString**)kind comment:(NSString**)comment {
    LOCK;
    if (!doc) return -1;
    const int i = annot::hitTest(docOf(doc)->d, page, {p.x, p.y});
    if (i >= 0)
        for (const auto& a : annot::list(docOf(doc)->d, page)) {
            if (a.index != i) continue;
            if (kind) *kind = ns(annot::typeName(a.subtype));
            if (comment) *comment = ns(a.contents);
            break;
        }
    return i;
}

+ (BOOL)removeAnnot:(FPDoc)doc page:(int)page index:(int)index { LOCK; return doc && annot::remove(docOf(doc)->d, page, index); }
+ (BOOL)setAnnotText:(FPDoc)doc page:(int)page index:(int)index text:(NSString*)text { LOCK; return doc && annot::setContents(docOf(doc)->d, page, index, ws(text)); }

+ (NSArray<FPMatch*>*)find:(FPDoc)doc text:(NSString*)text {
    LOCK;
    NSMutableArray* out = [NSMutableArray array];
    if (!doc) return out;
    Document& d = docOf(doc)->d;
    const auto& api = pdfium::api();
    for (const auto& t : search::find(d, ws(text), search::Options{})) {
        if (t.annot >= 0 || t.count <= 0) continue;
        FPDF_TEXTPAGE tp = d.text(t.page);
        if (!tp) continue;
        double l = 1e9, b = 1e9, r = -1e9, top = -1e9;
        const int n = api.FPDFText_CountRects(tp, t.start, t.count);
        for (int i = 0; i < n; ++i) {
            double rl, rt, rr, rb;
            if (!api.FPDFText_GetRect(tp, i, &rl, &rt, &rr, &rb)) continue;
            l = std::min(l, rl); r = std::max(r, rr); b = std::min(b, std::min(rb, rt)); top = std::max(top, std::max(rb, rt));
        }
        if (r < l) continue;
        FPMatch* m = [FPMatch new];
        m.page = t.page;
        m.box = CGRectMake(l, b, r - l, top - b);
        [out addObject:m];
    }
    return out;
}

+ (NSArray<FPBookmark*>*)bookmarks:(FPDoc)doc {
    LOCK;
    NSMutableArray* out = [NSMutableArray array];
    if (!doc) return out;
    std::function<void(const std::vector<Bookmark>&, int)> walk = [&](const std::vector<Bookmark>& v, int depth) {
        for (const auto& b : v) {
            FPBookmark* m = [FPBookmark new];
            m.depth = depth; m.page = b.page; m.title = ns(b.title);
            [out addObject:m];
            if (out.count < 5000) walk(b.children, depth + 1);
        }
    };
    walk(docOf(doc)->d.bookmarks(), 0);
    return out;
}

+ (BOOL)rotatePage:(FPDoc)doc page:(int)page { LOCK; return doc && docOf(doc)->d.rotatePages({page}, 1); }
+ (BOOL)deletePage:(FPDoc)doc page:(int)page { LOCK; return doc && docOf(doc)->d.deletePages({page}); }
+ (BOOL)undo:(FPDoc)doc { LOCK; return doc && docOf(doc)->d.undo(); }
+ (BOOL)redo:(FPDoc)doc { LOCK; return doc && docOf(doc)->d.redo(); }
+ (BOOL)canUndo:(FPDoc)doc { LOCK; return doc && docOf(doc)->d.canUndo(); }
+ (BOOL)canRedo:(FPDoc)doc { LOCK; return doc && docOf(doc)->d.canRedo(); }
+ (BOOL)isModified:(FPDoc)doc { LOCK; return doc && docOf(doc)->d.dirty(); }

+ (NSData*)bytesToSave:(FPDoc)doc {
    LOCK;
    std::vector<unsigned char> out;
    if (!doc || !docOf(doc)->d.bytesToSave(out) || out.empty()) return nil;
    return [NSData dataWithBytes:out.data() length:out.size()];
}

+ (void)markSaved:(FPDoc)doc { LOCK; if (doc) docOf(doc)->d.savedAs(docOf(doc)->d.path()); }

+ (NSString*)aboutCredits {
    const std::wstring body = about::body(L"iPhone and iPad", L"your iPhone or iPad");
    const size_t at = body.find(L"Created by:");
    return ns(at == std::wstring::npos ? body : body.substr(at));
}

+ (NSString*)feedbackAddress { return ns(about::kFeedbackAddress); }

@end
