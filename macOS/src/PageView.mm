#import "PageView.h"
#import "Render.h"
#import "Settings.h"
#include "platform.hpp"
#include "pageobjects.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <map>
#include <vector>

NSNotificationName const FPPageViewChanged = @"FPPageViewChanged";

namespace {
constexpr double kMargin = 16, kGap = 12;         // around and between pages, in screen points
constexpr double kMinZoom = 0.1, kMaxZoom = 8.0;
constexpr double kWholePagePixels = 24e6;         // above this a page is drawn a visible slice at a time
constexpr size_t kCachedPages = 10;
}

@implementation FPPageView {
    Document* _doc;
    double _zoom;
    std::vector<NSRect> _rects;                   // where each page is, in this view
    std::map<int, CGImageRef> _cache;             // whole pages at the current zoom, sharpness and colours
    double _cacheScale;
    int _cacheRevision;
    int _shown;                                   // the page shown in the one-page layout
    std::vector<TextHit> _matches;
    int _currentMatch;
    int _spokenPage, _spokenStart, _spokenCount;  // the word being read aloud
    std::vector<double> _toolPressure;            // a tablet pen's pressure at each point of the stroke
    BOOL _penStroke, _penNear;                    // the stroke is a tablet pen's; a pen is near the tablet (palm rejection)
    int _selPage, _selAnchor, _selFocus;          // selected characters: [min, max] of anchor and focus on _selPage
    NSPoint _downAt;
    BOOL _dragged;
    NSTrackingArea* _tracking;
    // a tool at work
    BOOL _toolDown;
    int _toolPage;
    std::vector<annot::PointD> _toolPts;          // page coordinates
    NSPoint _toolStartView, _toolNowView;
    std::vector<NSPoint> _toolPathView;           // the pen and the polygon, as drawn
    std::vector<FPMeasure> _measures;
    int _annotPage, _annotIndex;
    BOOL _formFocus, _formCapture;                // a form field has the keyboard / the mouse
    int _formPage;
    std::map<int, std::vector<pageobj::EditBlock>> _blocks;   // edit mode: each page's paragraphs and pictures
    const Document* _blocksDoc;
    int _blocksRev, _hoverPage, _hoverBlock;
    int _objPage;                                 // the selected object (Select tool)
    annot::RectD _objRect;
    FPToolEvent::Drag _objDrag;
}

- (int)objectPage { return _objPage; }

- (void)setObjectBox:(int)page rect:(annot::RectD)rect {
    _objPage = page;
    _objRect = rect;
    [self setNeedsDisplay:YES];
}

- (NSRect)viewRectOnPage:(int)page rect:(annot::RectD)r {
    const NSPoint a = [self viewPointOnPage:page x:r.left y:r.top], b = [self viewPointOnPage:page x:r.right y:r.bottom];
    return NSMakeRect(std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(b.x - a.x), std::fabs(b.y - a.y));
}

- (NSRect)objectHandle {
    const NSRect r = [self viewRectOnPage:_objPage rect:_objRect];
    return NSMakeRect(NSMaxX(r) - 5, NSMaxY(r) - 5, 10, 10);
}

// The selected object: a frame and a handle; while it is dragged, the frame where it will go.
- (void)drawObjectBox {
    if (_objPage < 0) return;
    NSRect r = [self viewRectOnPage:_objPage rect:_objRect];
    if (_toolDown && _objDrag == FPToolEvent::Move) r = NSOffsetRect(r, _toolNowView.x - _toolStartView.x, _toolNowView.y - _toolStartView.y);
    if (_toolDown && _objDrag == FPToolEvent::Resize) r.size = NSMakeSize(std::max(8.0, r.size.width + _toolNowView.x - _toolStartView.x), std::max(8.0, r.size.height + _toolNowView.y - _toolStartView.y));
    NSBezierPath* b = [NSBezierPath bezierPathWithRect:NSInsetRect(r, -2, -2)];
    const CGFloat d[] = {5, 3};
    [b setLineDash:d count:2 phase:0];
    b.lineWidth = 1.5;
    [NSColor.controlAccentColor setStroke];
    [b stroke];
    [NSColor.controlAccentColor setFill];
    NSRectFill(NSMakeRect(NSMaxX(r) - 5, NSMaxY(r) - 5, 10, 10));
}

- (void)invalidatePage:(int)page {
    auto it = _cache.find(page);
    if (it != _cache.end()) { CGImageRelease(it->second); _cache.erase(it); }
    [self setNeedsDisplay:YES];
}

- (void)endFormInput {
    if (!_formFocus || !_doc || !_doc->hasForm()) return;
    pdfium::api().FORM_ForceToKillFocus(_doc->form());
    _formFocus = NO;
    [self setNeedsDisplay:YES];
}

// A form field under a point: the click goes to PDFium's form filling (text, check boxes, lists...), as on Windows.
- (BOOL)formMouseDown:(NSPoint)p {
    if (!_doc || !_doc->hasForm()) return NO;
    const auto& api = pdfium::api();
    int page;
    double x, y;
    const BOOL onPage = [self pageAt:p page:&page x:&x y:&y];
    const int type = onPage ? api.FPDFPage_HasFormFieldAtPoint(_doc->form(), _doc->page(page), x, y) : -1;
    if (type < 0 && !_formFocus) return NO;
    if (!onPage) { [self endFormInput]; return NO; }
    _doc->currentPageForForm = page;
    _doc->formBeforeInput();
    _formPage = page;
    const bool handled = api.FORM_OnLButtonDown(_doc->form(), _doc->page(page), 0, x, y) != 0;
    if (type >= 0 || handled) { _formFocus = YES; _formCapture = YES; [self setNeedsDisplay:YES]; return YES; }
    api.FORM_ForceToKillFocus(_doc->form());   // an ordinary click elsewhere
    _formFocus = NO;
    return NO;
}

- (BOOL)formKey:(NSEvent*)e {
    if (!_formFocus || !_doc) return NO;
    const auto& api = pdfium::api();
    FPDF_FORMHANDLE fh = _doc->form();
    FPDF_PAGE pg = _doc->page(_formPage);
    NSString* k = e.charactersIgnoringModifiers;
    const unichar c = k.length ? [k characterAtIndex:0] : 0;
    if (e.modifierFlags & NSEventModifierFlagCommand) return NO;   // shortcuts (copy, paste, save...) go on
    if (c == 27) { [self endFormInput]; return YES; }
    // keys that move or delete: PDFium takes Windows key codes
    int vk = 0;
    switch (c) {
        case NSLeftArrowFunctionKey: vk = 0x25; break;
        case NSUpArrowFunctionKey: vk = 0x26; break;
        case NSRightArrowFunctionKey: vk = 0x27; break;
        case NSDownArrowFunctionKey: vk = 0x28; break;
        case NSHomeFunctionKey: vk = 0x24; break;
        case NSEndFunctionKey: vk = 0x23; break;
        case NSDeleteFunctionKey: vk = 0x2E; break;
        case '\t': case 25: vk = 0x09; break;
        default: break;
    }
    const int mods = (e.modifierFlags & NSEventModifierFlagShift) ? 1 : 0;
    if (vk) { api.FORM_OnKeyDown(fh, pg, vk, mods); api.FORM_OnKeyUp(fh, pg, vk, mods); return YES; }
    if (c == NSDeleteCharacter || c == NSBackspaceCharacter) { api.FORM_OnKeyDown(fh, pg, 0x08, 0); api.FORM_OnChar(fh, pg, 0x08, 0); api.FORM_OnKeyUp(fh, pg, 0x08, 0); return YES; }
    if (c == '\r' || c == 3) { api.FORM_OnKeyDown(fh, pg, 0x0D, 0); api.FORM_OnChar(fh, pg, 0x0D, 0); api.FORM_OnKeyUp(fh, pg, 0x0D, 0); return YES; }
    NSString* typed = e.characters;
    for (NSUInteger i = 0; i < typed.length; ++i) {
        const unichar u = [typed characterAtIndex:i];
        if (u >= 0x20 && (u < 0xF700 || u > 0xF8FF)) api.FORM_OnChar(fh, pg, u, 0);   // UTF-16, as PDFium wants
    }
    return YES;
}

- (NSString*)formSelectedText {
    const auto& api = pdfium::api();
    FPDF_PAGE pg = _doc->page(_formPage);
    const unsigned long bytes = api.FORM_GetSelectedText(_doc->form(), pg, nullptr, 0);
    if (bytes < 4) return @"";
    std::vector<unsigned short> buf(bytes / 2 + 1);
    api.FORM_GetSelectedText(_doc->form(), pg, buf.data(), bytes);
    return [NSString stringWithCharacters:buf.data() length:bytes / 2 - 1];
}

- (void)formReplaceSelection:(NSString*)text {
    std::vector<unsigned short> w(text.length + 1, 0);
    [text getCharacters:w.data() range:NSMakeRange(0, text.length)];
    pdfium::api().FORM_ReplaceSelection(_doc->form(), _doc->page(_formPage), w.data());
}

- (void)paste:(id)sender {
    if (!_formFocus) return;
    NSString* t = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
    if (t) [self formReplaceSelection:t];
}

- (void)cut:(id)sender {
    if (!_formFocus) return;
    NSString* t = [self formSelectedText];
    if (!t.length) return;
    [NSPasteboard.generalPasteboard clearContents];
    [NSPasteboard.generalPasteboard setString:t forType:NSPasteboardTypeString];
    [self formReplaceSelection:@""];
}

- (int)selectedAnnotPage { return _annotPage; }
- (int)selectedAnnot { return _annotIndex; }
- (void)clearAnnotSelection { _annotPage = _annotIndex = -1; [self setNeedsDisplay:YES]; }

- (void)setTool:(FPTool)tool {
    _tool = tool;
    _hoverPage = _hoverBlock = -1;
    _objPage = -1;
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageViewChanged object:self];   // the status bar names the tool
    _toolDown = NO;
    _toolPts.clear();
    _toolPathView.clear();
    [self clearAnnotSelection];
    [self setNeedsDisplay:YES];
}

- (BOOL)selectionRects:(std::vector<annot::RectD>&)rects page:(int*)page {
    rects.clear();
    if (![self hasSelection]) return NO;
    const auto& api = pdfium::api();
    FPDF_TEXTPAGE tp = _doc->text(_selPage);
    const int a = std::min(_selAnchor, _selFocus), b = std::max(_selAnchor, _selFocus);
    const int n = tp ? api.FPDFText_CountRects(tp, a, b - a + 1) : 0;
    for (int k = 0; k < n; ++k) {
        double l, t, r, bo;
        if (api.FPDFText_GetRect(tp, k, &l, &t, &r, &bo)) rects.push_back({l, bo, r, t});
    }
    *page = _selPage;
    return !rects.empty();
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _zoom = 1.0;
        _fitWidth = YES;
        _colours = pagecolours::Mode::Normal;
        _cacheScale = 0;
        _cacheRevision = -1;
        _layout = 1;
        _shown = 0;
        _currentMatch = -1;
        _selPage = -1;
        _spokenPage = -1;
        _annotPage = _annotIndex = -1;
        _objPage = -1;
        _blocksDoc = nullptr; _blocksRev = -1; _hoverPage = _hoverBlock = -1;
        _lineWidthPt = 2;
        [self registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self clearCache];
}

- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (Document*)document { return _doc; }
- (void)setDocument:(Document*)doc {
    _doc = doc;
    [self documentChanged];
}

- (double)zoom { return _zoom; }

- (void)setColours:(pagecolours::Mode)colours {
    if (colours == _colours) return;
    _colours = colours;
    [self clearCache];
    [self setNeedsDisplay:YES];
}

- (void)clearCache {
    for (auto& kv : _cache) CGImageRelease(kv.second);
    _cache.clear();
}

// ---- layout ----

- (NSClipView*)clip {
    return [self.superview isKindOfClass:[NSClipView class]] ? (NSClipView*)self.superview : nil;
}

- (void)viewDidMoveToSuperview {
    [super viewDidMoveToSuperview];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    NSClipView* clip = [self clip];
    if (!clip) return;
    clip.postsFrameChangedNotifications = YES;
    clip.postsBoundsChangedNotifications = YES;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(clipResized:)
                                                 name:NSViewFrameDidChangeNotification object:clip];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(clipScrolled:)
                                                 name:NSViewBoundsDidChangeNotification object:clip];
}

- (void)clipResized:(NSNotification*)n {
    if (_fitWidth) [self applyFitWidth];
    else [self layoutPages];
}

- (void)clipScrolled:(NSNotification*)n {
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageViewChanged object:self];
}

- (double)widestPage {
    double w = 0;
    if (_doc) for (int i = 0; i < _doc->pageCount(); ++i) w = std::max<double>(w, _doc->pageSize(i).w);
    return w > 0 ? w : 612;
}

// The pages of each row: one each (continuous), two side by side (the first alone with `cover`), or only the page shown.
- (std::vector<std::vector<int>>)rows {
    std::vector<std::vector<int>> rows;
    const int n = _doc ? _doc->pageCount() : 0;
    if (n == 0) return rows;
    if (_layout == 0) { rows.push_back({std::clamp(_shown, 0, n - 1)}); return rows; }
    if (_layout == 1) { for (int i = 0; i < n; ++i) rows.push_back({i}); return rows; }
    int i = 0;
    if (_cover) rows.push_back({i++});
    for (; i < n; i += 2) rows.push_back(i + 1 < n ? std::vector<int>{i, i + 1} : std::vector<int>{i});
    return rows;
}

- (void)layoutPages {
    NSClipView* clip = [self clip];
    const double clipW = clip ? clip.bounds.size.width : self.frame.size.width;
    const double clipH = clip ? clip.bounds.size.height : self.frame.size.height;
    const int n = _doc ? _doc->pageCount() : 0;
    _rects.assign(n, NSZeroRect);   // pages not shown (one-page layout) keep an empty place
    const auto rows = [self rows];
    double widest = 0;
    for (const auto& row : rows) {
        double w = 0;
        for (int p : row) w += std::round(_doc->pageSize(p).w * _zoom);
        widest = std::max(widest, w + kGap * (row.size() - 1));
    }
    const double width = std::max(clipW, widest + 2 * kMargin);
    double y = kMargin;
    for (const auto& row : rows) {
        double w = 0, h = 0;
        for (int p : row) { w += std::round(_doc->pageSize(p).w * _zoom); h = std::max(h, std::round(_doc->pageSize(p).h * _zoom)); }
        w += kGap * (row.size() - 1);
        double x = std::round((width - w) / 2);
        for (int p : row) {
            const double pw = std::round(_doc->pageSize(p).w * _zoom), ph = std::round(_doc->pageSize(p).h * _zoom);
            _rects[static_cast<size_t>(p)] = NSMakeRect(x, y + std::round((h - ph) / 2), pw, ph);
            x += pw + kGap;
        }
        y += h + kGap;
    }
    const double height = std::max(clipH, rows.empty() ? clipH : y - kGap + kMargin);
    [self setFrameSize:NSMakeSize(width, height)];
    [self setNeedsDisplay:YES];
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageViewChanged object:self];
}

- (void)setLayout:(int)layout {
    const int page = [self currentPage];
    _layout = std::clamp(layout, 0, 2);
    _shown = page;
    if (_fitWidth) [self applyFitWidth]; else [self layoutPages];
    [self showPage:page];
}

- (void)setCover:(BOOL)cover {
    _cover = cover;
    [self setLayout:_layout];
}

- (void)applyFitWidth {
    NSClipView* clip = [self clip];
    if (!clip) return;
    const double z = (clip.bounds.size.width - 2 * kMargin - (_layout == 2 ? kGap : 0)) / ([self widestPage] * (_layout == 2 ? 2 : 1));
    [self zoomTo:std::clamp(z, kMinZoom, kMaxZoom) anchor:NSMakePoint(NSMidX(self.visibleRect), NSMinY(self.visibleRect))];
}

- (void)documentChanged {
    [self clearCache];
    _selPage = -1;
    if (_doc && _shown >= _doc->pageCount()) _shown = std::max(0, _doc->pageCount() - 1);
    if (_fitWidth && [self clip]) [self applyFitWidth];
    else [self layoutPages];
}

// ---- zoom ----

- (void)setFitWidth:(BOOL)fit {
    _fitWidth = fit;
    if (fit) [self applyFitWidth];
}

- (void)setZoom:(double)zoom {
    _fitWidth = NO;
    const NSRect v = self.visibleRect;
    [self zoomTo:zoom anchor:NSMakePoint(NSMidX(v), NSMidY(v))];
}

- (void)zoomIn { [self setZoom:_zoom * 1.25]; }
- (void)zoomOut { [self setZoom:_zoom / 1.25]; }

// Zooms so that the spot of the document under `anchor` (a point of this view) stays where it is in the window.
- (void)zoomTo:(double)zoom anchor:(NSPoint)anchor {
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    const NSRect before = self.visibleRect;
    const NSPoint offset = NSMakePoint(anchor.x - before.origin.x, anchor.y - before.origin.y);
    int page = -1;
    double fx = 0.5, fy = 0;
    for (size_t i = 0; i < _rects.size(); ++i) {
        if (NSIsEmptyRect(_rects[i])) continue;
        if ((anchor.y <= NSMaxY(_rects[i]) + kGap / 2 && (anchor.x <= NSMaxX(_rects[i]) + kGap / 2 || _layout != 2)) || i + 1 == _rects.size()) {
            page = static_cast<int>(i);
            fx = (anchor.x - _rects[i].origin.x) / _rects[i].size.width;
            fy = (anchor.y - _rects[i].origin.y) / _rects[i].size.height;
            break;
        }
    }
    if (std::fabs(zoom - _zoom) > 1e-9) [self clearCache];
    _zoom = zoom;
    [self layoutPages];
    if (page >= 0 && page < static_cast<int>(_rects.size()) && !NSIsEmptyRect(_rects[page])) {
        const NSRect r = _rects[page];
        [self scrollPoint:NSMakePoint(r.origin.x + fx * r.size.width - offset.x, r.origin.y + fy * r.size.height - offset.y)];
    }
}

- (void)magnifyWithEvent:(NSEvent*)event {
    _fitWidth = NO;
    [self zoomTo:_zoom * (1 + event.magnification) anchor:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)scrollWheel:(NSEvent*)event {
    if (event.modifierFlags & NSEventModifierFlagCommand) {   // Command + wheel zooms at the pointer
        _fitWidth = NO;
        const double steps = event.hasPreciseScrollingDeltas ? event.scrollingDeltaY / 40.0 : event.scrollingDeltaY;
        [self zoomTo:_zoom * std::pow(1.1, steps) anchor:[self convertPoint:event.locationInWindow fromView:nil]];
        return;
    }
    [super scrollWheel:event];
}

// ---- pages ----

- (int)currentPage {
    if (_rects.empty()) return 0;
    if (_layout == 0) return std::clamp(_shown, 0, static_cast<int>(_rects.size()) - 1);
    const NSRect v = self.visibleRect;
    const double y = NSMinY(v) + v.size.height / 3;
    for (size_t i = 0; i < _rects.size(); ++i) if (!NSIsEmptyRect(_rects[i]) && NSMaxY(_rects[i]) + kGap / 2 > y) return static_cast<int>(i);
    return static_cast<int>(_rects.size()) - 1;
}

- (void)showPage:(int)page {
    if (_rects.empty()) return;
    page = std::clamp(page, 0, static_cast<int>(_rects.size()) - 1);
    if (_layout == 0 && page != _shown) { _shown = page; [self layoutPages]; }
    [self scrollPoint:NSMakePoint(NSMinX(self.visibleRect), _rects[page].origin.y - kMargin / 2)];
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageViewChanged object:self];
}

- (void)fitPage {
    NSClipView* clip = [self clip];
    if (!clip || !_doc || _doc->pageCount() == 0) return;
    const PageSize& s = _doc->pageSize([self currentPage]);
    const double pages = _layout == 2 ? 2 : 1;
    const double z = std::min((clip.bounds.size.width - 2 * kMargin - (pages - 1) * kGap) / (s.w * pages), (clip.bounds.size.height - 2 * kMargin) / s.h);
    const int page = [self currentPage];
    _fitWidth = NO;
    [self zoomTo:z anchor:NSMakePoint(NSMidX(self.visibleRect), NSMidY(self.visibleRect))];
    [self showPage:page];
}

- (void)scrollBy:(double)dy {
    const NSRect v = self.visibleRect;
    if (_layout == 0 && _doc) {   // one page at a time: past its end is the next page
        if (dy > 0 && NSMaxY(v) >= NSHeight(self.frame) - 1 && _shown + 1 < _doc->pageCount()) { [self showPage:_shown + 1]; return; }
        if (dy < 0 && NSMinY(v) <= 0 && _shown > 0) {
            [self showPage:_shown - 1];
            [self scrollPoint:NSMakePoint(NSMinX(self.visibleRect), NSHeight(self.frame))];
            return;
        }
    }
    [self scrollPoint:NSMakePoint(v.origin.x, std::max(0.0, v.origin.y + dy))];
}

- (void)keyDown:(NSEvent*)event {
    if ([self formKey:event]) return;
    NSString* k = event.charactersIgnoringModifiers;
    const unichar c = k.length ? [k characterAtIndex:0] : 0;
    const double page = self.visibleRect.size.height - 40;
    const bool shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
    switch (c) {
        case NSDownArrowFunctionKey: [self scrollBy:40]; break;
        case NSUpArrowFunctionKey: [self scrollBy:-40]; break;
        case NSPageDownFunctionKey: [self scrollBy:page]; break;
        case NSPageUpFunctionKey: [self scrollBy:-page]; break;
        case ' ': [self scrollBy:shift ? -page : page]; break;
        case NSHomeFunctionKey: [self showPage:0]; break;
        case NSEndFunctionKey: [self showPage:_doc ? _doc->pageCount() - 1 : 0]; break;
        case NSRightArrowFunctionKey: [self showPage:[self currentPage] + 1]; break;
        case NSLeftArrowFunctionKey: [self showPage:[self currentPage] - 1]; break;
        case NSDeleteCharacter: case NSBackspaceCharacter: case NSDeleteFunctionKey:
            if (_annotIndex >= 0 && self.onToolEvent) {
                FPToolEvent ev;
                ev.kind = FPToolEvent::Delete;
                ev.page = _annotPage;
                ev.annot = _annotIndex;
                _annotIndex = -1;
                self.onToolEvent(ev);
            }
            break;
        case 27: self.tool = FPToolNone; break;   // Esc: back to selecting
        case '\r':
            if (_tool == FPToolPolygon && _toolPts.size() >= 3) { [self sendTool:FPToolEvent::Draw]; _toolPts.clear(); _toolPathView.clear(); [self setNeedsDisplay:YES]; }
            break;
        default: [super keyDown:event];
    }
}

// ---- drawing ----

- (CGImageRef)wholePage:(int)page pixelsW:(int)pw pixelsH:(int)ph scale:(double)scale {
    if (scale != _cacheScale || (_doc && _doc->revision() != _cacheRevision)) {
        [self clearCache];
        _cacheScale = scale;
        _cacheRevision = _doc ? _doc->revision() : -1;
    }
    auto it = _cache.find(page);
    if (it != _cache.end()) return it->second;
    if (_cache.size() >= static_cast<size_t>(FPPagesKept())) {   // forget the page farthest from the one being drawn
        auto far = _cache.begin();
        for (auto i = _cache.begin(); i != _cache.end(); ++i)
            if (std::abs(i->first - page) > std::abs(far->first - page)) far = i;
        CGImageRelease(far->second);
        _cache.erase(far);
    }
    CGImageRef img = FPRenderPage(*_doc, page, pw, ph, 0, 0, pw, ph, _colours);
    if (img) _cache[page] = img;
    return img;
}

- (void)drawImage:(CGImageRef)img inRect:(NSRect)r {
    NSImage* ns = [[NSImage alloc] initWithCGImage:img size:r.size];
    [ns drawInRect:r fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1 respectFlipped:YES hints:nil];
}

- (void)drawRect:(NSRect)dirty {
    [FPThemeSurround() setFill];
    NSRectFill(dirty);
    if (!_doc) return;
    const double scale = self.window ? self.window.backingScaleFactor : 2.0;
    const NSRect visible = self.visibleRect;
    for (size_t i = 0; i < _rects.size(); ++i) {
        const NSRect r = _rects[i];
        if (NSIsEmptyRect(r) || !NSIntersectsRect(r, dirty)) continue;
        [[NSColor colorWithWhite:0 alpha:0.22] setFill];   // a thin shadow round the sheet
        NSRectFillUsingOperation(NSOffsetRect(NSInsetRect(r, -1, -1), 0, 1), NSCompositingOperationSourceOver);
        const int pw = static_cast<int>(std::lround(r.size.width * scale));
        const int ph = static_cast<int>(std::lround(r.size.height * scale));
        if (static_cast<double>(pw) * ph <= kWholePagePixels) {
            if (CGImageRef img = [self wholePage:static_cast<int>(i) pixelsW:pw pixelsH:ph scale:scale]) [self drawImage:img inRect:r];
        } else {
            // A very large page: only the visible slice, on whole pixels.
            const NSRect part = NSIntersectionRect(r, NSIntersectionRect(dirty, visible));
            if (NSIsEmptyRect(part)) continue;
            const int x0 = static_cast<int>(std::floor((NSMinX(part) - NSMinX(r)) * scale));
            const int y0 = static_cast<int>(std::floor((NSMinY(part) - NSMinY(r)) * scale));
            const int x1 = std::min(pw, static_cast<int>(std::ceil((NSMaxX(part) - NSMinX(r)) * scale)));
            const int y1 = std::min(ph, static_cast<int>(std::ceil((NSMaxY(part) - NSMinY(r)) * scale)));
            CGImageRef img = FPRenderPage(*_doc, static_cast<int>(i), pw, ph, x0, y0, x1 - x0, y1 - y0, _colours);
            if (!img) continue;
            [self drawImage:img inRect:NSMakeRect(NSMinX(r) + x0 / scale, NSMinY(r) + y0 / scale, (x1 - x0) / scale, (y1 - y0) / scale)];
            CGImageRelease(img);
        }
        [self drawMarksOnPage:static_cast<int>(i)];
        [self drawRedactionsOnPage:static_cast<int>(i)];
        [self drawAnnotSelectionOnPage:static_cast<int>(i)];
        [self drawMeasuresOnPage:static_cast<int>(i)];
        if (_showRulers) [self drawRulersOnPage:static_cast<int>(i)];
    }
    [self drawToolPreview];
}

// ---- page coordinates ----

// A point of the page (PDF units, origin bottom-left) in this view.
- (NSPoint)viewPointOnPage:(int)page x:(double)x y:(double)y {
    const NSRect r = _rects[static_cast<size_t>(page)];
    const int S = 4;   // a quarter of a screen point
    int dx = 0, dy = 0;
    pdfium::api().FPDF_PageToDevice(_doc->page(page), 0, 0, static_cast<int>(std::lround(r.size.width * S)),
                                    static_cast<int>(std::lround(r.size.height * S)), 0, x, y, &dx, &dy);
    return NSMakePoint(r.origin.x + dx / static_cast<double>(S), r.origin.y + dy / static_cast<double>(S));
}

// The page under a point of this view, and the point on it (PDF units). False when the point is on no page.
- (BOOL)pageAt:(NSPoint)p page:(int*)page x:(double*)x y:(double*)y {
    if (!_doc) return NO;
    for (size_t i = 0; i < _rects.size(); ++i) {
        const NSRect r = _rects[i];
        if (NSIsEmptyRect(r) || !NSPointInRect(p, r)) continue;
        const int S = 4;
        *page = static_cast<int>(i);
        pdfium::api().FPDF_DeviceToPage(_doc->page(*page), 0, 0, static_cast<int>(std::lround(r.size.width * S)),
                                        static_cast<int>(std::lround(r.size.height * S)), 0,
                                        static_cast<int>(std::lround((p.x - r.origin.x) * S)), static_cast<int>(std::lround((p.y - r.origin.y) * S)), x, y);
        return YES;
    }
    return NO;
}

// The rectangles of a run of characters, in this view.
- (std::vector<NSRect>)rectsOnPage:(int)page start:(int)start count:(int)count {
    std::vector<NSRect> out;
    const auto& api = pdfium::api();
    FPDF_TEXTPAGE tp = _doc->text(page);
    if (!tp || count <= 0) return out;
    const int n = api.FPDFText_CountRects(tp, start, count);
    for (int k = 0; k < n; ++k) {
        double l, t, r, b;
        if (!api.FPDFText_GetRect(tp, k, &l, &t, &r, &b)) continue;
        const NSPoint a = [self viewPointOnPage:page x:l y:t], c = [self viewPointOnPage:page x:r y:b];
        out.push_back(NSMakeRect(std::min(a.x, c.x), std::min(a.y, c.y), std::fabs(c.x - a.x), std::fabs(c.y - a.y)));
    }
    return out;
}

// Search matches (yellow; the current one orange) and the selected text (the accent colour) on a page.
- (void)drawMarksOnPage:(int)page {
    for (size_t m = 0; m < _matches.size(); ++m) {
        const TextHit& h = _matches[m];
        if (h.page != page) continue;
        NSColor* c = static_cast<int>(m) == _currentMatch ? [NSColor colorWithRed:1 green:0.55 blue:0 alpha:0.45]
                                                          : [NSColor colorWithRed:1 green:0.9 blue:0 alpha:0.38];
        [c setFill];
        if (h.annot >= 0) {   // found in a comment's text: the comment framed
            const NSRect r = NSInsetRect([self viewRectOnPage:page rect:{h.left, h.bottom, h.right, h.top}], -4, -4);
            NSRectFillUsingOperation(r, NSCompositingOperationMultiply);
            [[c colorWithAlphaComponent:1] setStroke];
            NSBezierPath* frame = [NSBezierPath bezierPathWithRect:r];
            frame.lineWidth = static_cast<int>(m) == _currentMatch ? 3 : 2;
            [frame stroke];
            continue;
        }
        for (const NSRect& r : [self rectsOnPage:page start:h.start count:h.count])
            NSRectFillUsingOperation(NSInsetRect(r, -1, -1), NSCompositingOperationMultiply);
    }
    if (_spokenPage == page && _spokenCount > 0) {
        [[NSColor colorWithRed:0.19 green:0.75 blue:0.38 alpha:0.45] setFill];
        for (const NSRect& r : [self rectsOnPage:page start:_spokenStart count:_spokenCount]) NSRectFillUsingOperation(NSInsetRect(r, -1, -1), NSCompositingOperationMultiply);
    }
    if (_selPage == page && _selAnchor >= 0) {
        [[NSColor.selectedTextBackgroundColor colorWithAlphaComponent:0.55] setFill];
        const int a = std::min(_selAnchor, _selFocus), b = std::max(_selAnchor, _selFocus);
        for (const NSRect& r : [self rectsOnPage:page start:a count:b - a + 1]) NSRectFillUsingOperation(r, NSCompositingOperationMultiply);
    }
}

// ---- search ----

- (void)setSpokenPage:(int)page start:(int)start count:(int)count {
    _spokenPage = page;
    _spokenStart = start;
    _spokenCount = count;
    if (page >= 0 && _doc && page < _doc->pageCount()) {
        if (_layout == 0 && page != _shown) [self showPage:page];
        const auto rs = [self rectsOnPage:page start:start count:count];
        if (!rs.empty()) [self scrollRectToVisible:NSInsetRect(rs[0], -40, -120)];
    }
    [self setNeedsDisplay:YES];
}

- (BOOL)selectionStartPage:(int*)page char:(int*)ch {
    if (_selPage < 0 || _selAnchor < 0) return NO;
    *page = _selPage;
    *ch = std::min(_selAnchor, _selFocus);
    return YES;
}

- (void)setMatches:(std::vector<TextHit>)hits current:(int)current {
    _matches = std::move(hits);
    _currentMatch = current;
    if (current >= 0 && current < static_cast<int>(_matches.size())) {
        const TextHit& h = _matches[static_cast<size_t>(current)];
        if (_layout == 0 && h.page != _shown) [self showPage:h.page];
        const auto rs = h.annot >= 0 ? std::vector<NSRect>{[self viewRectOnPage:h.page rect:{h.left, h.bottom, h.right, h.top}]}
                                     : [self rectsOnPage:h.page start:h.start count:h.count];
        if (!rs.empty()) [self scrollRectToVisible:NSInsetRect(rs[0], -60, -80)];
    }
    [self setNeedsDisplay:YES];
}

// ---- selecting text, links, the pointer ----

- (int)charAt:(int)page x:(double)x y:(double)y tolerance:(double)tol {
    FPDF_TEXTPAGE tp = _doc->text(page);
    return tp ? pdfium::api().FPDFText_GetCharIndexAtPos(tp, x, y, tol, tol) : -1;
}

- (FPDF_LINK)linkAt:(int)page x:(double)x y:(double)y {
    return pdfium::api().FPDFLink_GetLinkAtPoint(_doc->page(page), x, y);
}

- (BOOL)hasSelection { return _selPage >= 0 && _selAnchor >= 0; }

- (NSString*)selectedText {
    if (![self hasSelection]) return @"";
    const int a = std::min(_selAnchor, _selFocus), b = std::max(_selAnchor, _selFocus);
    return [NSString stringWithUTF8String:platform::utf8(_doc->textRange(_selPage, a, b - a + 1)).c_str()] ?: @"";
}

- (void)clearSelection {
    if (_selPage < 0) return;
    _selPage = -1;
    [self setNeedsDisplay:YES];
}

// ---- tools ----

- (void)sendTool:(FPToolEvent::Kind)kind {
    if (!self.onToolEvent) return;
    FPToolEvent ev;
    ev.kind = kind;
    ev.page = _toolPage;
    ev.pts = _toolPts;
    ev.annot = _annotIndex;
    if (_tool == FPToolPen && _penStroke && _toolPressure.size() == _toolPts.size()) ev.pressure = _toolPressure;
    self.onToolEvent(ev);
}

- (void)toolDown:(NSEvent*)e at:(NSPoint)p {
    int page;
    double x, y;
    if (![self pageAt:p page:&page x:&x y:&y]) return;
    _objDrag = FPToolEvent::None;
    if ((_tool == FPToolSelectObject || _tool == FPToolEditAll) && _objPage == page) {   // on the selected object: move it, or resize it by its handle
        if (NSPointInRect(p, NSInsetRect([self objectHandle], -3, -3))) _objDrag = FPToolEvent::Resize;
        else if (NSPointInRect(p, [self viewRectOnPage:_objPage rect:_objRect])) _objDrag = FPToolEvent::Move;
    }
    if (_tool == FPToolProtractor) {   // the corner, then a point on each side
        if (!_toolPts.empty() && page != _toolPage) return;
        _toolPage = page;
        _toolPts.push_back({x, y});
        _toolPathView.push_back(p);
        if (_toolPts.size() == 3) {
            [self sendTool:FPToolEvent::Draw];
            _toolPts.clear();
            _toolPathView.clear();
        }
        [self setNeedsDisplay:YES];
        return;
    }
    if (_tool == FPToolPolygon) {   // corners one click at a time; a double-click (or Return) ends it
        if (!_toolPts.empty() && page != _toolPage) return;
        _toolPage = page;
        if (e.clickCount >= 2) {
            if (_toolPts.size() >= 3) [self sendTool:FPToolEvent::Draw];
            _toolPts.clear();
            _toolPathView.clear();
        } else {
            _toolPts.push_back({x, y});
            _toolPathView.push_back(p);
        }
        [self setNeedsDisplay:YES];
        return;
    }
    _toolDown = YES;
    _toolPage = page;
    _toolPts = {{x, y}};
    _penStroke = e.subtype == NSEventSubtypeTabletPoint;
    _toolPressure = {_penStroke ? e.pressure : 0.5};
    _toolStartView = _toolNowView = p;
    _toolPathView = {p};
}

- (void)toolDragged:(NSPoint)p {
    if (!_toolDown) return;
    const NSRect r = _rects[static_cast<size_t>(_toolPage)];
    p = NSMakePoint(std::clamp(p.x, NSMinX(r), NSMaxX(r)), std::clamp(p.y, NSMinY(r), NSMaxY(r)));
    _toolNowView = p;
    if (_tool == FPToolPen) {
        int page;
        double x, y;
        if ([self pageAt:NSMakePoint(std::min(p.x, NSMaxX(r) - 0.5), std::min(p.y, NSMaxY(r) - 0.5)) page:&page x:&x y:&y]) {
            _toolPts.push_back({x, y});
            _toolPathView.push_back(p);
            NSEvent* ev = NSApp.currentEvent;
            const bool pen = ev.subtype == NSEventSubtypeTabletPoint;
            _penStroke = _penStroke && pen;
            _toolPressure.push_back(pen ? ev.pressure : 0.5);
        }
    }
    [self setNeedsDisplay:YES];
}

- (void)toolUp:(NSEvent*)e {
    if (!_toolDown) return;
    _toolDown = NO;
    const bool moved = std::hypot(_toolNowView.x - _toolStartView.x, _toolNowView.y - _toolStartView.y) > 3;
    if (_tool != FPToolPen) {
        const NSRect r = _rects[static_cast<size_t>(_toolPage)];
        int page;
        double x, y;
        if ([self pageAt:NSMakePoint(std::min(_toolNowView.x, NSMaxX(r) - 0.5), std::min(_toolNowView.y, NSMaxY(r) - 0.5)) page:&page x:&x y:&y])
            _toolPts.push_back({x, y});
    }
    if (_objDrag != FPToolEvent::None) {
        if (moved && _toolPts.size() >= 2 && self.onToolEvent) {
            FPToolEvent ev;
            ev.kind = FPToolEvent::Draw;
            ev.page = _toolPage;
            ev.pts = {_toolPts.front(), _toolPts.back()};
            ev.drag = _objDrag;
            self.onToolEvent(ev);
        }
        _objDrag = FPToolEvent::None;
        _toolPts.clear();
        [self setNeedsDisplay:YES];
        return;
    }
    if (moved && _toolPts.size() >= 2) [self sendTool:FPToolEvent::Draw];
    else { _toolPts.resize(1); [self sendTool:e.clickCount >= 2 ? FPToolEvent::DoubleClick : FPToolEvent::Click]; }
    _toolPts.clear();
    _toolPathView.clear();
    [self setNeedsDisplay:YES];
}

// ---- edit mode: the outlines ----

- (const std::vector<pageobj::EditBlock>*)blocksOf:(int)page {
    if (_blocksDoc != _doc || _blocksRev != (_doc ? _doc->revision() : -1)) { _blocks.clear(); _blocksDoc = _doc; _blocksRev = _doc ? _doc->revision() : -1; }
    auto it = _blocks.find(page);
    if (it == _blocks.end()) it = _blocks.emplace(page, _doc ? pageobj::editableBlocks(*_doc, page) : std::vector<pageobj::EditBlock>{}).first;
    return &it->second;
}

// The block under a view point (text over a picture behind it, else the smallest); -1 when none.
- (int)editBlockAt:(NSPoint)p page:(int*)page kind:(pageobj::EditBlock::Kind*)kind {
    double x, y;
    if (!_doc || ![self pageAt:p page:page x:&x y:&y]) return -1;
    const auto& bs = *[self blocksOf:*page];
    const double m = 3.0 / _zoom;
    int best = -1;
    double bestArea = 1e18;
    for (size_t i = 0; i < bs.size(); ++i) {
        const auto& b = bs[i];
        if (x < b.left - m || x > b.right + m || y < b.bottom - m || y > b.top + m) continue;
        const double area = (b.right - b.left) * (b.top - b.bottom) * (b.kind == pageobj::EditBlock::Picture ? 4 : 1);
        if (area < bestArea) { bestArea = area; best = static_cast<int>(i); }
    }
    if (best >= 0 && kind) *kind = bs[static_cast<size_t>(best)].kind;
    return best;
}

- (void)drawEditOutlines {
    if (_tool != FPToolEditAll || !_doc || (self.editingNow && self.editingNow())) return;
    const NSRect visible = self.visibleRect;
    for (int page = 0; page < static_cast<int>(_rects.size()); ++page) {
        if (!NSIntersectsRect(_rects[static_cast<size_t>(page)], visible)) continue;
        const auto& bs = *[self blocksOf:page];
        for (size_t i = 0; i < bs.size(); ++i) {
            const auto& b = bs[i];
            const NSRect r = NSInsetRect([self viewRectOnPage:page rect:annot::RectD{b.left, b.bottom, b.right, b.top}], -3, -3);
            const bool hot = page == _hoverPage && static_cast<int>(i) == _hoverBlock;
            const bool pic = b.kind == pageobj::EditBlock::Picture;
            NSColor* c = hot ? NSColor.controlAccentColor : pic ? [NSColor colorWithSRGBRed:0.88 green:0.54 blue:0.12 alpha:1]
                                                               : [NSColor colorWithSRGBRed:0.42 green:0.65 blue:0.94 alpha:1];
            if (hot) { [[c colorWithAlphaComponent:0.09] setFill]; NSRectFillUsingOperation(r, NSCompositingOperationSourceOver); }
            NSBezierPath* path = [NSBezierPath bezierPathWithRect:NSInsetRect(r, 0.5, 0.5)];
            path.lineWidth = hot ? 2 : 1;
            if (pic && !hot) { const CGFloat dash[2] = {5, 3}; [path setLineDash:dash count:2 phase:0]; }
            [c setStroke];
            [path stroke];
        }
    }
}

// The shape being drawn, before it is added.
- (void)drawToolPreview {
    [self drawEditOutlines];
    [self drawObjectBox];
    if (_objDrag != FPToolEvent::None) return;
    if (!_toolDown && !((_tool == FPToolPolygon || _tool == FPToolProtractor) && !_toolPathView.empty())) return;
    if (_tool == FPToolSelectObject || _tool == FPToolEditText || _tool == FPToolEditPara || _tool == FPToolEditTable || _tool == FPToolPlaceTable || _tool == FPToolEditAll) return;
    NSColor* c = _tool == FPToolRedact ? NSColor.blackColor
               : _tool == FPToolRuler || _tool == FPToolProtractor ? [NSColor colorWithSRGBRed:0 green:0.56 blue:0.38 alpha:1]
                                                                   : NSColor.controlAccentColor;
    [c setStroke];
    const NSRect box = NSMakeRect(std::min(_toolStartView.x, _toolNowView.x), std::min(_toolStartView.y, _toolNowView.y),
                                  std::fabs(_toolNowView.x - _toolStartView.x), std::fabs(_toolNowView.y - _toolStartView.y));
    NSBezierPath* path = [NSBezierPath bezierPath];
    path.lineWidth = std::max(1.0, _lineWidthPt * _zoom);
    switch (_tool) {
        case FPToolEllipse: path = [NSBezierPath bezierPathWithOvalInRect:box]; break;
        case FPToolLine: case FPToolArrow: case FPToolCallout: case FPToolRuler:
            [path moveToPoint:_toolStartView]; [path lineToPoint:_toolNowView]; break;
        case FPToolPen: case FPToolPolygon: case FPToolProtractor:
            for (size_t i = 0; i < _toolPathView.size(); ++i) i ? [path lineToPoint:_toolPathView[i]] : [path moveToPoint:_toolPathView[i]];
            break;
        default: path = [NSBezierPath bezierPathWithRect:box];
    }
    if (_tool == FPToolRedact) { [[NSColor colorWithWhite:0 alpha:0.35] setFill]; [path fill]; }
    if (_tool == FPToolCloud || _tool == FPToolCrop || _tool == FPToolLink) { const CGFloat d[] = {6, 4}; [path setLineDash:d count:2 phase:0]; }
    [path stroke];
}

// ---- measuring ----

- (void)addMeasure:(const FPMeasure&)m { _measures.push_back(m); [self setNeedsDisplay:YES]; }
- (void)clearMeasures { _measures.clear(); [self setNeedsDisplay:YES]; }
- (const std::vector<FPMeasure>&)measures { return _measures; }
- (void)setShowRulers:(BOOL)on { _showRulers = on; [self setNeedsDisplay:YES]; }
- (void)setRulerUnits:(int)u { _rulerUnits = std::clamp(u, 0, 3); [self setNeedsDisplay:YES]; }

// Green lines with their length or angle in a label, as on Windows.
- (void)drawMeasuresOnPage:(int)page {
    NSColor* green = [NSColor colorWithSRGBRed:0 green:0.56 blue:0.38 alpha:1];
    NSDictionary* attrs = @{NSFontAttributeName : [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold], NSForegroundColorAttributeName : NSColor.whiteColor};
    for (const FPMeasure& m : _measures) {
        if (m.page != page) continue;
        const NSPoint a = [self viewPointOnPage:page x:m.a.x y:m.a.y], b = [self viewPointOnPage:page x:m.b.x y:m.b.y];
        NSBezierPath* path = [NSBezierPath bezierPath];
        path.lineWidth = 1.5;
        [path moveToPoint:b];
        [path lineToPoint:a];
        NSPoint at = NSMakePoint((a.x + b.x) / 2, (a.y + b.y) / 2);
        if (m.angle) {
            const NSPoint c = [self viewPointOnPage:page x:m.c.x y:m.c.y];
            [path lineToPoint:c];
            at = NSMakePoint(a.x + 14, a.y + 6);
        }
        [green setStroke];
        [path stroke];
        for (NSPoint p : {a, b}) [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(p.x - 2.5, p.y - 2.5, 5, 5)] stroke];
        NSString* t = [NSString stringWithUTF8String:platform::utf8(m.text).c_str()] ?: @"";
        const NSSize s = [t sizeWithAttributes:attrs];
        const NSRect box = NSMakeRect(at.x + 4, at.y + 4, s.width + 8, s.height + 2);
        [green setFill];
        [[NSBezierPath bezierPathWithRoundedRect:box xRadius:3 yRadius:3] fill];
        [t drawAtPoint:NSMakePoint(box.origin.x + 4, box.origin.y + 1) withAttributes:attrs];
    }
}

// Rulers along the top and left edges of the page, in the chosen units (inside the page, so nothing is covered for long).
- (void)drawRulersOnPage:(int)page {
    const NSRect r = _rects[static_cast<size_t>(page)];
    const PageSize& ps = _doc->pageSize(page);
    if (ps.w <= 0 || ps.h <= 0) return;
    static const double perUnit[] = {72 / 25.4, 72 / 2.54, 72.0, 1.0};   // points in one unit
    // tick spacing (units), every how many ticks a long tick, every how many a number
    static const double tick[] = {1, 0.1, 0.125, 10};
    static const int longEvery[] = {5, 5, 4, 5}, labelEvery[] = {10, 10, 8, 10};
    static const double labelValue[] = {10, 1, 1, 100};   // the value a labelled tick adds (mm: 10, 20...; cm: 1, 2...)
    const int u = std::clamp(_rulerUnits, 0, 3);
    const double kx = r.size.width / ps.w, ky = r.size.height / ps.h;   // view points per page point
    const double step = tick[u] * perUnit[u];
    const CGFloat band = 16;
    [[NSColor colorWithWhite:1 alpha:0.85] setFill];
    NSRectFillUsingOperation(NSMakeRect(r.origin.x, r.origin.y, r.size.width, band), NSCompositingOperationSourceOver);
    NSRectFillUsingOperation(NSMakeRect(r.origin.x, r.origin.y, band, r.size.height), NSCompositingOperationSourceOver);
    [[NSColor colorWithWhite:0.25 alpha:1] set];
    NSDictionary* attrs = @{NSFontAttributeName : [NSFont systemFontOfSize:8], NSForegroundColorAttributeName : [NSColor colorWithWhite:0.25 alpha:1]};
    if (step * std::min(kx, ky) < 2.5) return;   // too small to read
    NSBezierPath* p = [NSBezierPath bezierPath];
    p.lineWidth = 0.5;
    for (int i = 1; i * step < ps.w; ++i) {   // the flipped view: y grows downwards, the top edge is r.origin.y
        const CGFloat x = std::round(r.origin.x + i * step * kx) + 0.25;
        const CGFloat len = i % labelEvery[u] == 0 ? 10 : i % longEvery[u] == 0 ? 7 : 4;
        [p moveToPoint:NSMakePoint(x, r.origin.y)];
        [p lineToPoint:NSMakePoint(x, r.origin.y + len)];
        if (i % labelEvery[u] == 0) [[NSString stringWithFormat:@"%g", i / labelEvery[u] * labelValue[u]] drawAtPoint:NSMakePoint(x + 2, r.origin.y + 4) withAttributes:attrs];
    }
    for (int i = 1; i * step < ps.h; ++i) {
        const CGFloat y = std::round(r.origin.y + i * step * ky) + 0.25;
        const CGFloat len = i % labelEvery[u] == 0 ? 10 : i % longEvery[u] == 0 ? 7 : 4;
        [p moveToPoint:NSMakePoint(r.origin.x, y)];
        [p lineToPoint:NSMakePoint(r.origin.x + len, y)];
        if (i % labelEvery[u] == 0) [[NSString stringWithFormat:@"%g", i / labelEvery[u] * labelValue[u]] drawAtPoint:NSMakePoint(r.origin.x + 2, y + 1) withAttributes:attrs];
    }
    [p stroke];
}

// Pending redaction marks: black frames, half filled (they are applied with Protect > Apply Redactions).
- (void)drawRedactionsOnPage:(int)page {
    if (!_doc) return;
    for (const RedactMark& m : _doc->redactions) {
        if (m.page != page) continue;
        const NSPoint a = [self viewPointOnPage:page x:m.left y:m.top], b = [self viewPointOnPage:page x:m.right y:m.bottom];
        const NSRect r = NSMakeRect(std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(b.x - a.x), std::fabs(b.y - a.y));
        [[NSColor colorWithWhite:0 alpha:0.35] setFill];
        NSRectFillUsingOperation(r, NSCompositingOperationSourceOver);
        [NSColor.blackColor setStroke];
        [NSBezierPath strokeRect:r];
    }
}

// The selected annotation: a dashed frame.
- (void)drawAnnotSelectionOnPage:(int)page {
    if (page != _annotPage || _annotIndex < 0) return;
    for (const annot::Info& a : annot::list(*_doc, page)) {
        if (a.index != _annotIndex) continue;
        const NSPoint p = [self viewPointOnPage:page x:a.rect.left y:a.rect.top], q = [self viewPointOnPage:page x:a.rect.right y:a.rect.bottom];
        NSBezierPath* b = [NSBezierPath bezierPathWithRect:NSInsetRect(NSMakeRect(std::min(p.x, q.x), std::min(p.y, q.y), std::fabs(q.x - p.x), std::fabs(q.y - p.y)), -3, -3)];
        const CGFloat d[] = {4, 3};
        [b setLineDash:d count:2 phase:0];
        b.lineWidth = 1.5;
        [NSColor.controlAccentColor setStroke];
        [b stroke];
    }
}

- (NSMenu*)menuForEvent:(NSEvent*)e {
    const NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    int page;
    double x, y;
    if (!self.onContextMenu || ![self pageAt:p page:&page x:&x y:&y]) return nil;
    return self.onContextMenu(page, {x, y}, annot::hitTest(*_doc, page, {x, y}));
}

// A tablet pen came near or went away: while it is near, touches that are not the pen (a hand resting on a pen display,
// or the trackpad) do not draw with the Pen tool.
- (void)tabletProximity:(NSEvent*)e { _penNear = e.isEnteringProximity; }

- (void)mouseDown:(NSEvent*)e {
    if (_tool == FPToolPen && _penNear && e.subtype != NSEventSubtypeTabletPoint) return;   // palm rejection
    [self.window makeFirstResponder:self];
    const NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (_presenting) {   // a click: the next page (with Shift: the one before)
        [self showPage:[self currentPage] + ((e.modifierFlags & NSEventModifierFlagShift) ? -1 : 1)];
        return;
    }
    if (_tool != FPToolNone) { [self toolDown:e at:p]; return; }
    if ([self formMouseDown:p]) return;
    {   // an annotation under the pointer is selected (double-click: its comment)
        int page;
        double x, y;
        if (_doc && [self pageAt:p page:&page x:&x y:&y]) {
            const int a = annot::hitTest(*_doc, page, {x, y});
            if (a >= 0 && ![self linkAt:page x:x y:y]) {
                _annotPage = page;
                _annotIndex = a;
                _selPage = -1;
                [self setNeedsDisplay:YES];
                if (e.clickCount == 2 && self.onToolEvent) {
                    FPToolEvent ev;
                    ev.kind = FPToolEvent::DoubleClick;
                    ev.page = page;
                    ev.pts = {{x, y}};
                    ev.annot = a;
                    self.onToolEvent(ev);
                }
                return;
            }
            if (_annotIndex >= 0) [self clearAnnotSelection];
        }
    }
    _downAt = p;
    _dragged = NO;
    int page;
    double x, y;
    if (![self pageAt:p page:&page x:&x y:&y]) { [self clearSelection]; return; }
    const int c = [self charAt:page x:x y:y tolerance:4];
    if (e.clickCount == 2 && c >= 0) {   // a word
        FPDF_TEXTPAGE tp = _doc->text(page);
        const auto& api = pdfium::api();
        const int n = api.FPDFText_CountChars(tp);
        auto letter = [&](int i) { const unsigned u = api.FPDFText_GetUnicode(tp, i); return iswalnum(static_cast<wint_t>(u)) || u == '\'' || u == '-'; };
        int a = c, b = c;
        while (a > 0 && letter(a - 1)) --a;
        while (b + 1 < n && letter(b + 1)) ++b;
        _selPage = page; _selAnchor = a; _selFocus = b;
    } else if (e.clickCount == 3 && c >= 0) {   // the whole page's text
        _selPage = page; _selAnchor = 0; _selFocus = std::max(0, _doc->charCount(page) - 1);
    } else {
        _selPage = c >= 0 ? page : -1;
        _selAnchor = _selFocus = c;
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent*)e {
    [self autoscroll:e];
    const NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    if (_tool != FPToolNone) { [self toolDragged:p]; return; }
    if (_formCapture) {
        int page;
        double x, y;
        if ([self pageAt:p page:&page x:&x y:&y] && page == _formPage) pdfium::api().FORM_OnMouseMove(_doc->form(), _doc->page(page), 0, x, y);
        return;
    }
    if (std::hypot(p.x - _downAt.x, p.y - _downAt.y) > 3) _dragged = YES;
    if (_selPage < 0) {   // started beside the text: start at the first character reached
        int page;
        double x, y;
        if ([self pageAt:p page:&page x:&x y:&y]) {
            const int c = [self charAt:page x:x y:y tolerance:6];
            if (c >= 0) { _selPage = page; _selAnchor = _selFocus = c; }
        }
        [self setNeedsDisplay:YES];
        return;
    }
    const NSRect r = _rects[static_cast<size_t>(_selPage)];
    const NSPoint q = NSMakePoint(std::clamp(p.x, NSMinX(r), NSMaxX(r) - 1), std::clamp(p.y, NSMinY(r), NSMaxY(r) - 1));
    int page;
    double x, y;
    if ([self pageAt:q page:&page x:&x y:&y] && page == _selPage) {
        const int c = [self charAt:page x:x y:y tolerance:12];
        if (c >= 0) _selFocus = c;
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent*)e {
    if (_tool != FPToolNone) { [self toolUp:e]; return; }
    if (_formCapture) {
        _formCapture = NO;
        int page;
        double x, y;
        if ([self pageAt:[self convertPoint:e.locationInWindow fromView:nil] page:&page x:&x y:&y] && page == _formPage)
            pdfium::api().FORM_OnLButtonUp(_doc->form(), _doc->page(page), 0, x, y);
        return;
    }
    if (_dragged || e.clickCount > 1) return;
    if (_selPage >= 0 && _selAnchor == _selFocus) [self clearSelection];   // a click, not a selection
    int page;
    double x, y;
    if (![self pageAt:[self convertPoint:e.locationInWindow fromView:nil] page:&page x:&x y:&y]) return;
    if (FPDF_LINK link = [self linkAt:page x:x y:y]) [self followLink:link fromPage:page];
}

// A link: a page of this document, or a web address (opened in the browser).
- (void)followLink:(FPDF_LINK)link fromPage:(int)from {
    const auto& api = pdfium::api();
    FPDF_DOCUMENT doc = _doc->handle();
    FPDF_DEST dest = api.FPDFLink_GetDest(doc, link);
    if (!dest) {
        FPDF_ACTION a = api.FPDFLink_GetAction(link);
        if (!a) return;
        const unsigned long type = api.FPDFAction_GetType(a);
        if (type == PDFACTION_GOTO) dest = api.FPDFAction_GetDest(doc, a);
        else if (type == PDFACTION_URI) {
            const unsigned long n = api.FPDFAction_GetURIPath(doc, a, nullptr, 0);
            if (n <= 1) return;
            std::string uri(n, '\0');
            api.FPDFAction_GetURIPath(doc, a, uri.data(), n);
            uri.resize(n - 1);
            NSURL* url = [NSURL URLWithString:[NSString stringWithUTF8String:uri.c_str()] ?: @""];
            if (url.scheme) [[NSWorkspace sharedWorkspace] openURL:url];
            return;
        }
    }
    if (!dest) return;
    const int to = api.FPDFDest_GetDestPageIndex(doc, dest);
    if (to < 0 || to >= _doc->pageCount()) return;
    if (self.jumped) self.jumped(from);
    [self showPage:to];
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_tracking) [self removeTrackingArea:_tracking];
    _tracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                             options:NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
                                               owner:self userInfo:nil];
    [self addTrackingArea:_tracking];
}

// The pointer: a hand over a link, the text bar over text.
- (void)mouseMoved:(NSEvent*)e {
    if (_tool == FPToolEditAll) {   // edit mode: the block under the pointer lights up; text bar over text, a hand over a picture
        const NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
        int page = -1;
        pageobj::EditBlock::Kind kind = pageobj::EditBlock::Text;
        const int b = [self editBlockAt:p page:&page kind:&kind];
        if (b != _hoverBlock || (b >= 0 && page != _hoverPage)) { _hoverBlock = b; _hoverPage = b >= 0 ? page : -1; [self setNeedsDisplay:YES]; }
        if (_objPage >= 0 && NSPointInRect(p, [self viewRectOnPage:_objPage rect:_objRect])) [NSCursor.openHandCursor set];
        else if (b < 0) [NSCursor.arrowCursor set];
        else if (kind == pageobj::EditBlock::Picture) [NSCursor.openHandCursor set];
        else [NSCursor.IBeamCursor set];
        return;
    }
    if (_tool != FPToolNone) { [NSCursor.crosshairCursor set]; return; }
    {
        int fp;
        double fx, fy;
        if (_doc && _doc->hasForm() && [self pageAt:[self convertPoint:e.locationInWindow fromView:nil] page:&fp x:&fx y:&fy]) {
            pdfium::api().FORM_OnMouseMove(_doc->form(), _doc->page(fp), 0, fx, fy);   // PDFium sets the pointer (FormHost cursor)
            if (pdfium::api().FPDFPage_HasFormFieldAtPoint(_doc->form(), _doc->page(fp), fx, fy) >= 0) return;
        }
    }
    int page;
    double x, y;
    if (_doc && [self pageAt:[self convertPoint:e.locationInWindow fromView:nil] page:&page x:&x y:&y]) {
        if ([self linkAt:page x:x y:y]) { [NSCursor.pointingHandCursor set]; return; }
        if ([self charAt:page x:x y:y tolerance:3] >= 0) { [NSCursor.IBeamCursor set]; return; }
    }
    [NSCursor.arrowCursor set];
}

// ⌘C copies the selected text; ⌘A selects the text of the page shown.
- (void)copy:(id)sender {
    if (_formFocus) {
        NSString* t = [self formSelectedText];
        if (t.length) { [NSPasteboard.generalPasteboard clearContents]; [NSPasteboard.generalPasteboard setString:t forType:NSPasteboardTypeString]; }
        return;
    }
    if (![self hasSelection]) return;
    NSPasteboard* pb = NSPasteboard.generalPasteboard;
    [pb clearContents];
    [pb setString:[self selectedText] forType:NSPasteboardTypeString];
}

- (void)selectAll:(id)sender {
    if (_formFocus) { pdfium::api().FORM_SelectAllText(_doc->form(), _doc->page(_formPage)); return; }
    if (!_doc || _doc->pageCount() == 0) return;
    const int page = [self currentPage];
    const int n = _doc->charCount(page);
    if (n <= 0) return;
    _selPage = page; _selAnchor = 0; _selFocus = n - 1;
    [self setNeedsDisplay:YES];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    if (item.action == @selector(copy:)) return [self hasSelection] || _formFocus;
    if (item.action == @selector(paste:) || item.action == @selector(cut:)) return _formFocus;
    if (item.action == @selector(selectAll:)) return _doc != nullptr;
    return YES;
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self clearCache];   // moved to a screen of another sharpness
    [self setNeedsDisplay:YES];
}

// ---- dropping PDF files on the window opens them ----

- (NSArray<NSURL*>*)pdfURLs:(id<NSDraggingInfo>)info {
    NSArray<NSURL*>* urls = [info.draggingPasteboard readObjectsForClasses:@[ [NSURL class] ]
                                                                   options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
    NSMutableArray<NSURL*>* pdfs = [NSMutableArray array];
    for (NSURL* u in urls) if ([u.pathExtension.lowercaseString isEqualToString:@"pdf"]) [pdfs addObject:u];
    return pdfs;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
    return [self pdfURLs:info].count ? NSDragOperationCopy : NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
    NSArray<NSURL*>* urls = [self pdfURLs:info];
    for (NSURL* u in urls)
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:u display:YES
                                                                     completionHandler:^(NSDocument*, BOOL, NSError*) {}];
    return urls.count > 0;
}

@end
