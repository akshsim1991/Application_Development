#import "FPPagesView.h"
#import <QuartzCore/QuartzCore.h>
#include <algorithm>
#include <cmath>
#include <vector>

// ---- one page: tiles drawn on background threads at the zoom shown
@interface FPTileLayer : CATiledLayer
@end
@implementation FPTileLayer
+ (CFTimeInterval)fadeDuration { return 0.05; }
@end

@interface FPPageTiles : UIView
@property(atomic) FPDoc doc;
@property(atomic) int page;
@property(atomic) FPColours colours;
@property(atomic) CGSize pageSize;   // its size in the view (points)
@end
@implementation FPPageTiles
+ (Class)layerClass { return [FPTileLayer class]; }
- (instancetype)initWithFrame:(CGRect)f {
    if ((self = [super initWithFrame:f])) {
        FPTileLayer* l = (FPTileLayer*)self.layer;
        l.levelsOfDetail = 1;
        l.levelsOfDetailBias = 5;   // sharp up to 32 times
        l.tileSize = CGSizeMake(512, 512);
        self.backgroundColor = UIColor.whiteColor;
        self.opaque = YES;
    }
    return self;
}
- (void)drawRect:(CGRect)rect {
    // called for each tile on a background thread: the part of the page under it, at the scale it is shown
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    const CGFloat scale = std::fabs(CGContextGetCTM(ctx).a);
    const CGSize size = self.pageSize;
    FPDoc doc = self.doc;
    if (!doc || scale <= 0 || size.width <= 0) return;
    const CGRect part = CGRectMake(rect.origin.x * scale, rect.origin.y * scale, std::ceil(rect.size.width * scale), std::ceil(rect.size.height * scale));
    CGImageRef img = [FPEngine render:doc page:self.page part:part full:CGSizeMake(size.width * scale, size.height * scale) colours:self.colours];
    if (!img) return;
    [[UIImage imageWithCGImage:img] drawInRect:rect];
    CGImageRelease(img);
}
@end

// ---- over a page: matches, the selection, edit mode's outlines, the stroke being drawn
@interface FPPagesView () <UIScrollViewDelegate, UIGestureRecognizerDelegate>
- (void)drawOverlayOfPage:(int)page inView:(UIView*)v;
@end

@interface FPOverlay : UIView
@property(nonatomic) int page;
@property(nonatomic, weak) FPPagesView* owner;
@end
@implementation FPOverlay
- (void)drawRect:(CGRect)rect { [self.owner drawOverlayOfPage:self.page inView:self]; }
@end

@implementation FPPagesView {
    UIScrollView* _scroll;
    UIView* _content;
    NSMutableArray<FPPageTiles*>* _tiles;
    NSMutableArray<FPOverlay*>* _overlays;
    std::vector<CGSize> _sizes;       // each page, points
    std::vector<CGFloat> _tops;       // each page's top at zoom 1
    CGFloat _laidOutWidth;
    int _lastReported;
    // tools
    int _selPage;
    CGPoint _selA, _selB;              // page points
    NSArray<NSValue*>* _selRects;
    NSMutableArray<NSValue*>* _stroke; // view points of the page
    int _strokePage;
    NSMutableDictionary<NSNumber*, NSArray<NSValue*>*>* _blocks;
    UIPanGestureRecognizer* _toolPan;
    UILongPressGestureRecognizer* _press;
}

static const CGFloat kGap = 10, kMargin = 6;

- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _tiles = [NSMutableArray array];
        _overlays = [NSMutableArray array];
        _blocks = [NSMutableDictionary dictionary];
        _penColor = 0xD32F2F; _penWidth = 2;
        _lastReported = -1; _selPage = -1; _strokePage = -1; _currentMatch = -1;
        _scroll = [[UIScrollView alloc] initWithFrame:self.bounds];
        _scroll.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        _scroll.delegate = self;
        _scroll.minimumZoomScale = 1;
        _scroll.maximumZoomScale = 8;
        _scroll.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
        _scroll.backgroundColor = UIColor.clearColor;
        [self addSubview:_scroll];
        _content = [[UIView alloc] init];
        [_scroll addSubview:_content];

        UITapGestureRecognizer* dbl = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(doubleTap:)];
        dbl.numberOfTapsRequired = 2;
        [_scroll addGestureRecognizer:dbl];
        UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tap:)];
        [tap requireGestureRecognizerToFail:dbl];
        [_scroll addGestureRecognizer:tap];
        _press = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(press:)];
        [_scroll addGestureRecognizer:_press];
        _toolPan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(toolPan:)];
        _toolPan.maximumNumberOfTouches = 1;
        _toolPan.enabled = NO;
        [_scroll addGestureRecognizer:_toolPan];
    }
    return self;
}

// ---------------------------------------------------------------------------------------------------------------
// The document

- (void)setDocument:(FPDoc)doc page:(int)page {
    _doc = doc;
    _matches = @[]; _currentMatch = -1;
    _selRects = nil; _selPage = -1; _stroke = nil;
    [_blocks removeAllObjects];
    _scroll.zoomScale = 1;
    [self rebuild];
    _lastReported = -1;
    dispatch_async(dispatch_get_main_queue(), ^{ [self goToPage:page]; [self report]; });
}

- (void)rebuild {
    for (UIView* v in _tiles) [v removeFromSuperview];
    for (UIView* v in _overlays) [v removeFromSuperview];
    [_tiles removeAllObjects];
    [_overlays removeAllObjects];
    _sizes.clear();
    const int n = _doc ? [FPEngine pageCount:_doc] : 0;
    for (int i = 0; i < n; ++i) {
        _sizes.push_back([FPEngine pageSize:_doc page:i]);
        FPPageTiles* t = [[FPPageTiles alloc] initWithFrame:CGRectZero];
        t.doc = _doc; t.page = i; t.colours = _colours;
        t.layer.shadowColor = UIColor.blackColor.CGColor; t.layer.shadowOpacity = 0.15; t.layer.shadowRadius = 2; t.layer.shadowOffset = CGSizeMake(0, 1);
        [_content addSubview:t];
        [_tiles addObject:t];
        FPOverlay* o = [[FPOverlay alloc] initWithFrame:CGRectZero];
        o.page = i; o.owner = self; o.opaque = NO; o.backgroundColor = UIColor.clearColor; o.userInteractionEnabled = NO;
        [_content addSubview:o];
        [_overlays addObject:o];
    }
    _laidOutWidth = -1;
    [self layoutPages];
}

- (int)pageCount { return static_cast<int>(_sizes.size()); }

/** At zoom 1 the widest page fills the width; the pages one under another. */
- (void)layoutPages {
    const CGFloat width = self.bounds.size.width;
    if (width <= 0) return;
    const int keep = self.currentPage;
    const CGFloat zoom = _scroll.zoomScale;
    _scroll.zoomScale = 1;
    CGFloat widest = 1;
    for (const CGSize& s : _sizes) widest = std::max(widest, s.width);
    const CGFloat scale = std::max<CGFloat>(1, width - 2 * kMargin) / widest;
    _tops.assign(_sizes.size() + 1, 0);
    CGFloat y = kGap;
    for (size_t i = 0; i < _sizes.size(); ++i) {
        const CGSize s = CGSizeMake(std::round(_sizes[i].width * scale), std::round(_sizes[i].height * scale));
        const CGRect r = CGRectMake(std::round((width - s.width) / 2), y, s.width, s.height);
        _tops[i] = y;
        _tiles[i].frame = r;
        _tiles[i].pageSize = s;
        _overlays[i].frame = r;
        y += s.height + kGap;
    }
    _tops[_sizes.size()] = y;
    _content.frame = CGRectMake(0, 0, width, y);
    _scroll.contentSize = _content.frame.size;
    _laidOutWidth = width;
    for (FPPageTiles* t in _tiles) [t.layer setNeedsDisplay];
    for (UIView* o in _overlays) [o setNeedsDisplay];
    if (zoom > 1.001) _scroll.zoomScale = zoom;
    if (keep < self.pageCount && _laidOutWidth > 0) [self goToPage:keep];
}

- (void)layoutSubviews {
    [super layoutSubviews];
    if (std::fabs(self.bounds.size.width - _laidOutWidth) > 0.5) [self layoutPages];
}

- (void)documentChanged {
    // the pages may have changed in number or size (turned, deleted): laid out again, kept on the page shown
    const int keep = self.currentPage;
    const CGFloat zoom = _scroll.zoomScale;
    const CGPoint off = _scroll.contentOffset;
    std::vector<CGSize> sizes;
    const int n = _doc ? [FPEngine pageCount:_doc] : 0;
    for (int i = 0; i < n; ++i) sizes.push_back([FPEngine pageSize:_doc page:i]);
    bool same = sizes.size() == _sizes.size();
    for (size_t i = 0; same && i < sizes.size(); ++i) same = CGSizeEqualToSize(sizes[i], _sizes[i]);
    [_blocks removeAllObjects];
    _selRects = nil; _selPage = -1;
    if (same) {   // only drawn again
        for (FPPageTiles* t in _tiles) { t.layer.contents = nil; [t.layer setNeedsDisplay]; }
        for (UIView* o in _overlays) [o setNeedsDisplay];
        return;
    }
    [self rebuild];
    _scroll.zoomScale = zoom;
    if (zoom > 1.001) _scroll.contentOffset = off; else [self goToPage:std::min(keep, self.pageCount - 1)];
    [self report];
}

- (void)setColours:(FPColours)c {
    _colours = c;
    for (FPPageTiles* t in _tiles) { t.colours = c; t.layer.contents = nil; [t.layer setNeedsDisplay]; }
}

- (void)setTool:(FPTool)t {
    _tool = t;
    _selRects = nil; _selPage = -1; _stroke = nil;
    // drawing and highlighting take one finger; two still move the pages
    const BOOL oneFinger = t == FPToolDraw || t == FPToolHighlight;
    _toolPan.enabled = oneFinger;
    _scroll.panGestureRecognizer.minimumNumberOfTouches = oneFinger ? 2 : 1;
    _press.enabled = t == FPToolRead;
    for (UIView* o in _overlays) [o setNeedsDisplay];
}

- (CGFloat)zoom { return _scroll.zoomScale; }

- (int)currentPage {
    if (_sizes.empty()) return 0;
    const CGFloat mid = (_scroll.contentOffset.y + _scroll.bounds.size.height / 2) / _scroll.zoomScale;
    for (size_t i = 0; i < _sizes.size(); ++i) if (mid < _tops[i + 1]) return static_cast<int>(i);
    return static_cast<int>(_sizes.size()) - 1;
}

- (void)goToPage:(int)page {
    if (_sizes.empty()) return;
    page = std::max(0, std::min(page, self.pageCount - 1));
    CGFloat y = (_tops[page] - kGap / 2) * _scroll.zoomScale;
    y = std::max<CGFloat>(0, std::min(y, _scroll.contentSize.height - _scroll.bounds.size.height));
    [_scroll setContentOffset:CGPointMake(_scroll.contentOffset.x, std::max<CGFloat>(0, y)) animated:NO];
    [self report];
}

- (CGPoint)viewState { return _scroll.contentOffset; }
- (void)restoreViewState:(CGPoint)state zoom:(CGFloat)zoom {
    _scroll.zoomScale = std::max<CGFloat>(1, zoom);
    _scroll.contentOffset = state;
    [self report];
}

- (BOOL)settled {
    // the tiles of the pages on screen have something drawn (CATiledLayer fills them in shortly)
    if (_sizes.empty()) return NO;
    return YES;
}

- (void)report {
    const int p = self.currentPage;
    if (p != _lastReported) { _lastReported = p; [self.delegate pagesShowPage:p of:self.pageCount]; }
}

// ---------------------------------------------------------------------------------------------------------------
// Scrolling and zooming

- (UIView*)viewForZoomingInScrollView:(UIScrollView*)s { return _content; }
- (void)scrollViewDidScroll:(UIScrollView*)s { [self report]; }
- (void)scrollViewDidZoom:(UIScrollView*)s {
    // narrower than the screen: kept in the middle
    const CGFloat dx = std::max<CGFloat>(0, (s.bounds.size.width - s.contentSize.width) / 2);
    _content.center = CGPointMake(s.contentSize.width / 2 + dx, s.contentSize.height / 2);
    [self report];
}

// ---------------------------------------------------------------------------------------------------------------
// Where a touch is: the page under it and the page point

- (int)pageAt:(CGPoint)contentPoint {
    for (size_t i = 0; i < _tiles.count; ++i) if (CGRectContainsPoint(_tiles[i].frame, contentPoint)) return static_cast<int>(i);
    return -1;
}
- (CGPoint)pagePoint:(int)page from:(CGPoint)contentPoint {
    const CGRect f = _tiles[page].frame;
    return [FPEngine viewToPage:_doc page:page size:f.size point:CGPointMake(contentPoint.x - f.origin.x, contentPoint.y - f.origin.y)];
}
- (CGRect)viewRect:(int)page of:(CGRect)pageBox size:(CGSize)size {
    const CGPoint a = [FPEngine pageToView:_doc page:page size:size point:CGPointMake(CGRectGetMinX(pageBox), CGRectGetMinY(pageBox))];
    const CGPoint b = [FPEngine pageToView:_doc page:page size:size point:CGPointMake(CGRectGetMaxX(pageBox), CGRectGetMaxY(pageBox))];
    return CGRectMake(std::min(a.x, b.x), std::min(a.y, b.y), std::fabs(b.x - a.x), std::fabs(b.y - a.y));
}

- (void)tap:(UITapGestureRecognizer*)g {
    const CGPoint p = [g locationInView:_content];
    const int page = [self pageAt:p];
    if (page < 0) { [self.delegate pagesTappedOutside]; return; }
    [self.delegate pagesTapped:page at:[self pagePoint:page from:p]];
}

- (void)doubleTap:(UITapGestureRecognizer*)g {
    if (_scroll.zoomScale > 1.2) { [_scroll setZoomScale:1 animated:YES]; return; }
    const CGPoint p = [g locationInView:_content];
    const CGFloat z = 2.5;
    const CGSize s = CGSizeMake(_scroll.bounds.size.width / z, _scroll.bounds.size.height / z);
    [_scroll zoomToRect:CGRectMake(p.x - s.width / 2, p.y - s.height / 2, s.width, s.height) animated:YES];
}

- (void)press:(UILongPressGestureRecognizer*)g {
    const CGPoint p = [g locationInView:_content];
    if (g.state == UIGestureRecognizerStateBegan) {
        _selPage = [self pageAt:p];
        if (_selPage < 0) return;
        _selA = _selB = [self pagePoint:_selPage from:p];
    } else if (_selPage >= 0 && g.state == UIGestureRecognizerStateChanged) {
        if ([self pageAt:p] == _selPage) _selB = [self pagePoint:_selPage from:p];
    } else if (_selPage >= 0 && g.state == UIGestureRecognizerStateEnded) {
        [self.delegate pagesSwept:_selPage from:_selA to:_selB];
        return;
    } else if (g.state == UIGestureRecognizerStateCancelled) { [self clearSelection]; return; }
    if (_selPage >= 0) {
        _selRects = [FPEngine selectionRects:_doc page:_selPage from:_selA to:_selB];
        [_overlays[_selPage] setNeedsDisplay];
    }
}

- (void)toolPan:(UIPanGestureRecognizer*)g {
    const CGPoint p = [g locationInView:_content];
    if (g.state == UIGestureRecognizerStateBegan) {
        const int page = [self pageAt:p];
        if (page < 0) return;
        if (_tool == FPToolHighlight) { _selPage = page; _selA = _selB = [self pagePoint:page from:p]; }
        else { _strokePage = page; _stroke = [NSMutableArray array]; }
    }
    if (_tool == FPToolHighlight && _selPage >= 0) {
        if ([self pageAt:p] == _selPage) _selB = [self pagePoint:_selPage from:p];
        _selRects = [FPEngine selectionRects:_doc page:_selPage from:_selA to:_selB];
        [_overlays[_selPage] setNeedsDisplay];
        if (g.state == UIGestureRecognizerStateEnded) { [self.delegate pagesSwept:_selPage from:_selA to:_selB]; [self clearSelection]; }
        return;
    }
    if (_tool == FPToolDraw && _strokePage >= 0 && _stroke) {
        const CGRect f = _tiles[_strokePage].frame;
        [_stroke addObject:[NSValue valueWithCGPoint:CGPointMake(p.x - f.origin.x, p.y - f.origin.y)]];
        [_overlays[_strokePage] setNeedsDisplay];
        if (g.state == UIGestureRecognizerStateEnded || g.state == UIGestureRecognizerStateCancelled) {
            NSMutableArray* pts = [NSMutableArray array];
            for (NSValue* v in _stroke) [pts addObject:[NSValue valueWithCGPoint:[FPEngine viewToPage:_doc page:_strokePage size:f.size point:v.CGPointValue]]];
            const int page = _strokePage;
            _stroke = nil; _strokePage = -1;
            [_overlays[page] setNeedsDisplay];
            if (g.state == UIGestureRecognizerStateEnded && pts.count > 0) [self.delegate pagesStroke:page points:pts];
        }
    }
}

- (void)clearSelection {
    const int p = _selPage;
    _selRects = nil; _selPage = -1;
    if (p >= 0 && p < (int)_overlays.count) [_overlays[p] setNeedsDisplay];
}

// ---------------------------------------------------------------------------------------------------------------
// Search matches

- (void)setMatches:(NSArray<FPMatch*>*)m {
    _matches = [m copy] ?: @[];
    _currentMatch = -1;
    for (UIView* o in _overlays) [o setNeedsDisplay];
}

- (void)setCurrentMatch:(NSInteger)i {
    _currentMatch = i;
    for (UIView* o in _overlays) [o setNeedsDisplay];
}

- (void)showMatch:(NSInteger)i {
    if (i < 0 || i >= (NSInteger)_matches.count) return;
    self.currentMatch = i;
    FPMatch* m = _matches[i];
    if (m.page < 0 || m.page >= self.pageCount) return;
    const CGRect f = _tiles[m.page].frame;
    const CGRect r = [self viewRect:m.page of:m.box size:f.size];
    const CGFloat z = _scroll.zoomScale;
    CGFloat y = (f.origin.y + r.origin.y) * z - _scroll.bounds.size.height / 3;
    y = std::max<CGFloat>(0, std::min(y, _scroll.contentSize.height - _scroll.bounds.size.height));
    CGFloat x = _scroll.contentOffset.x;
    const CGFloat left = (f.origin.x + r.origin.x) * z;
    if (left < x || left + r.size.width * z > x + _scroll.bounds.size.width) x = std::max<CGFloat>(0, std::min(left - _scroll.bounds.size.width / 3, _scroll.contentSize.width - _scroll.bounds.size.width));
    [_scroll setContentOffset:CGPointMake(x, y) animated:NO];
    [self report];
}

// ---------------------------------------------------------------------------------------------------------------
// The overlay of a page

- (void)drawOverlayOfPage:(int)page inView:(UIView*)v {
    if (!_doc || page >= self.pageCount) return;
    const CGSize size = v.bounds.size;
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    for (NSInteger i = 0; i < (NSInteger)_matches.count; ++i) {
        FPMatch* m = _matches[i];
        if (m.page != page) continue;
        [(i == _currentMatch ? [UIColor colorWithRed:1 green:0.44 blue:0 alpha:0.5] : [UIColor colorWithRed:1 green:0.76 blue:0.03 alpha:0.35]) setFill];
        UIRectFillUsingBlendMode([self viewRect:page of:m.box size:size], kCGBlendModeNormal);
    }
    if (page == _selPage && _selRects) {
        [[UIColor colorWithRed:0.24 green:0.55 blue:0.99 alpha:0.33] setFill];
        for (NSValue* r in _selRects) UIRectFillUsingBlendMode([self viewRect:page of:r.CGRectValue size:size], kCGBlendModeNormal);
    }
    if (_tool == FPToolEditText) {
        NSArray<NSValue*>* blocks = _blocks[@(page)];
        if (!blocks) { blocks = [FPEngine editableBlocks:_doc page:page]; _blocks[@(page)] = blocks; }
        [[UIColor colorWithRed:0.12 green:0.53 blue:0.9 alpha:0.8] setStroke];
        for (NSValue* b in blocks) {
            UIBezierPath* p = [UIBezierPath bezierPathWithRect:CGRectInset([self viewRect:page of:b.CGRectValue size:size], -2, -2)];
            p.lineWidth = 1.2;
            [p stroke];
        }
    }
    if (page == _strokePage && _stroke.count > 0) {
        UIBezierPath* p = [UIBezierPath bezierPath];
        [p moveToPoint:_stroke[0].CGPointValue];
        for (NSValue* q in _stroke) [p addLineToPoint:q.CGPointValue];
        p.lineCapStyle = kCGLineCapRound; p.lineJoinStyle = kCGLineJoinRound;
        p.lineWidth = std::max(1.0, _penWidth * size.width / std::max<CGFloat>(1, _sizes[page].width));
        [[UIColor colorWithRed:((_penColor >> 16) & 0xFF) / 255.0 green:((_penColor >> 8) & 0xFF) / 255.0 blue:(_penColor & 0xFF) / 255.0 alpha:1] setStroke];
        [p stroke];
    }
    (void)ctx;
}

@end
