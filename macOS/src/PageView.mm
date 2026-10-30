#import "PageView.h"
#import "Render.h"
#include <algorithm>
#include <cmath>
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
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _zoom = 1.0;
        _fitWidth = YES;
        _colours = pagecolours::Mode::Normal;
        _cacheScale = 0;
        _cacheRevision = -1;
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

- (void)layoutPages {
    NSClipView* clip = [self clip];
    const double clipW = clip ? clip.bounds.size.width : self.frame.size.width;
    const double clipH = clip ? clip.bounds.size.height : self.frame.size.height;
    const int n = _doc ? _doc->pageCount() : 0;
    const double width = std::max(clipW, [self widestPage] * _zoom + 2 * kMargin);
    _rects.assign(n, NSZeroRect);
    double y = kMargin;
    for (int i = 0; i < n; ++i) {
        const PageSize& s = _doc->pageSize(i);
        const double w = std::round(s.w * _zoom), h = std::round(s.h * _zoom);
        _rects[i] = NSMakeRect(std::round((width - w) / 2), y, w, h);
        y += h + kGap;
    }
    const double height = std::max(clipH, n ? y - kGap + kMargin : clipH);
    [self setFrameSize:NSMakeSize(width, height)];
    [self setNeedsDisplay:YES];
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageViewChanged object:self];
}

- (void)applyFitWidth {
    NSClipView* clip = [self clip];
    if (!clip) return;
    const double z = (clip.bounds.size.width - 2 * kMargin) / [self widestPage];
    [self zoomTo:std::clamp(z, kMinZoom, kMaxZoom) anchor:NSMakePoint(NSMidX(self.visibleRect), NSMinY(self.visibleRect))];
}

- (void)documentChanged {
    [self clearCache];
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
        if (anchor.y <= NSMaxY(_rects[i]) + kGap / 2 || i + 1 == _rects.size()) {
            page = static_cast<int>(i);
            fx = (anchor.x - _rects[i].origin.x) / _rects[i].size.width;
            fy = (anchor.y - _rects[i].origin.y) / _rects[i].size.height;
            break;
        }
    }
    if (std::fabs(zoom - _zoom) > 1e-9) [self clearCache];
    _zoom = zoom;
    [self layoutPages];
    if (page >= 0 && page < static_cast<int>(_rects.size())) {
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
    const NSRect v = self.visibleRect;
    const double y = NSMinY(v) + v.size.height / 3;
    for (size_t i = 0; i < _rects.size(); ++i) if (NSMaxY(_rects[i]) + kGap / 2 > y) return static_cast<int>(i);
    return static_cast<int>(_rects.size()) - 1;
}

- (void)showPage:(int)page {
    if (_rects.empty()) return;
    page = std::clamp(page, 0, static_cast<int>(_rects.size()) - 1);
    [self scrollPoint:NSMakePoint(NSMinX(self.visibleRect), _rects[page].origin.y - kMargin / 2)];
}

- (void)scrollBy:(double)dy {
    const NSRect v = self.visibleRect;
    [self scrollPoint:NSMakePoint(v.origin.x, std::max(0.0, v.origin.y + dy))];
}

- (void)keyDown:(NSEvent*)event {
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
    if (_cache.size() >= kCachedPages) {   // forget the page farthest from the one being drawn
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
    [[NSColor underPageBackgroundColor] setFill];
    NSRectFill(dirty);
    if (!_doc) return;
    const double scale = self.window ? self.window.backingScaleFactor : 2.0;
    const NSRect visible = self.visibleRect;
    for (size_t i = 0; i < _rects.size(); ++i) {
        const NSRect r = _rects[i];
        if (!NSIntersectsRect(r, dirty)) continue;
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
    }
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
