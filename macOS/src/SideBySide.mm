// Side by side (View menu), as on Windows: another open document, or a file, shown next to this one in the same
// window, left and right or top and bottom, with linked scrolling (both scroll and zoom together) and a swap. The
// other document stays open in its own tab; "Back to One Document" closes the second view.
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "WindowController.h"
#import "PageView.h"
#import "FPDocument.h"
#import "Form.h"
#import "Trace.h"
#include <cmath>
#include <vector>

// What side by side holds while it is on (one at a time, as on Windows).
@interface FPBeside : NSObject
@property(nonatomic, weak) FPWindowController* host;   // the window it is shown in
@property(nonatomic, weak) NSDocument* other;          // the document shown beside
@property(nonatomic, strong) NSScrollView* scroll;
@property(nonatomic, strong) FPPageView* view;
@property(nonatomic, strong) NSMutableArray* tokens;   // notification observers
@end
@implementation FPBeside
@end

namespace {
FPBeside* g_beside;
BOOL g_swapped = NO;
BOOL g_syncing = NO;

BOOL linked() { NSNumber* v = [NSUserDefaults.standardUserDefaults objectForKey:@"sideBySideLinked"]; return v ? v.boolValue : YES; }
BOOL stacked() { return [NSUserDefaults.standardUserDefaults boolForKey:@"sideBySideStacked"]; }
BOOL active() { return g_beside && g_beside.host && g_beside.other; }

// Linked: the other side follows the zoom and the position (the same share of the way through the document).
void follow(NSScrollView* a, FPPageView* fromView, NSScrollView* b, FPPageView* toView) {
    if (g_syncing || !linked() || !a || !b) return;
    g_syncing = YES;
    if (std::abs(toView.zoom - fromView.zoom) > 0.001) [toView setZoom:fromView.zoom];
    const CGFloat rangeA = NSHeight(a.documentView.frame) - NSHeight(a.contentView.bounds);
    const CGFloat rangeB = NSHeight(b.documentView.frame) - NSHeight(b.contentView.bounds);
    if (rangeA > 0 && rangeB > 0) {
        NSPoint p = b.contentView.bounds.origin;
        p.y = std::round(a.contentView.bounds.origin.y / rangeA * rangeB);
        [b.contentView scrollToPoint:p];
        [b reflectScrolledClipView:b.contentView];
    }
    g_syncing = NO;
}

void end() {
    FPBeside* b = g_beside;
    g_beside = nil;
    if (!b) return;
    for (id t in b.tokens) [NSNotificationCenter.defaultCenter removeObserver:t];
    [b.host setBesideView:nil stacked:NO swapped:NO];
}

void layout() {
    if (!active()) return;
    [g_beside.host setBesideView:g_beside.scroll stacked:stacked() swapped:g_swapped];
    g_beside.view.fitWidth = YES;
}
} // namespace

@interface FPWindowController (SideBySide)
@end

@implementation FPWindowController (SideBySide)

- (void)showBesideDocument:(NSDocument*)other {
    if (!other || other == self.document || ![other isKindOfClass:[FPDocument class]]) return;
    end();
    FPBeside* b = [[FPBeside alloc] init];
    b.host = self;
    b.other = other;
    b.tokens = [NSMutableArray array];
    b.scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)];
    b.scroll.hasVerticalScroller = YES;
    b.scroll.hasHorizontalScroller = YES;
    b.scroll.autohidesScrollers = YES;
    b.scroll.drawsBackground = YES;
    b.scroll.backgroundColor = self.pageView.enclosingScrollView.backgroundColor;
    b.view = [[FPPageView alloc] initWithFrame:b.scroll.contentView.bounds];
    b.view.colours = self.pageView.colours;
    b.view.layout = self.pageView.layout;
    b.scroll.documentView = b.view;
    b.view.document = [(FPDocument*)other pdf];
    g_beside = b;
    layout();

    NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
    __weak FPBeside* weakB = b;
    __weak FPWindowController* weakSelf = self;
    // linked scrolling, both ways
    NSClipView* mine = self.pageView.enclosingScrollView.contentView;
    NSClipView* theirs = b.scroll.contentView;
    mine.postsBoundsChangedNotifications = YES;
    theirs.postsBoundsChangedNotifications = YES;
    [b.tokens addObject:[nc addObserverForName:NSViewBoundsDidChangeNotification object:mine queue:nil usingBlock:^(NSNotification*) {
        FPBeside* s = weakB;
        if (s) follow(weakSelf.pageView.enclosingScrollView, weakSelf.pageView, s.scroll, s.view);
    }]];
    [b.tokens addObject:[nc addObserverForName:NSViewBoundsDidChangeNotification object:theirs queue:nil usingBlock:^(NSNotification*) {
        FPBeside* s = weakB;
        if (s) follow(s.scroll, s.view, weakSelf.pageView.enclosingScrollView, weakSelf.pageView);
    }]];
    // the other document read again (Revert): its new pages; changed in its own tab: drawn again here
    [b.tokens addObject:[nc addObserverForName:FPDocumentReloaded object:other queue:nil usingBlock:^(NSNotification*) {
        FPBeside* s = weakB;
        if (s) s.view.document = [(FPDocument*)s.other pdf];
    }]];
    [b.tokens addObject:[nc addObserverForName:NSWindowDidBecomeKeyNotification object:self.window queue:nil usingBlock:^(NSNotification*) {
        [weakB.view documentChanged];
    }]];
    // the page colours of this window follow the setting; the second view too
    [b.tokens addObject:[nc addObserverForName:FPPageColoursChanged object:nil queue:nil usingBlock:^(NSNotification*) {
        dispatch_async(dispatch_get_main_queue(), ^{
            FPBeside* s = weakB;
            if (!s) return;
            s.view.colours = weakSelf.pageView.colours;
            s.scroll.backgroundColor = weakSelf.pageView.enclosingScrollView.backgroundColor;
        });
    }]];
    // either document closed: back to one
    for (NSWindowController* wc in other.windowControllers)
        [b.tokens addObject:[nc addObserverForName:NSWindowWillCloseNotification object:wc.window queue:nil usingBlock:^(NSNotification*) {
            if (g_beside == weakB) end();
        }]];
    [b.tokens addObject:[nc addObserverForName:NSWindowWillCloseNotification object:self.window queue:nil usingBlock:^(NSNotification*) {
        if (g_beside == weakB) end();
    }]];
    [self.window makeKeyAndOrderFront:nil];
    NSView* pair = b.scroll.superview;
    FPTrace(@"side by side: %lu views in %@", (unsigned long)pair.subviews.count, self.window.title);
    follow(self.pageView.enclosingScrollView, self.pageView, b.scroll, b.view);
}

- (IBAction)sideBySideWithTab:(id)sender {
    NSMutableArray<NSDocument*>* others = [NSMutableArray array];
    std::vector<std::wstring> names;
    for (NSDocument* d in NSDocumentController.sharedDocumentController.documents)
        if (d != self.document && [d isKindOfClass:[FPDocument class]]) {
            [others addObject:d];
            names.push_back(form::ws(d.displayName));
        }
    if (!others.count) {
        form::message(self.window, L"Only this document is open. Open another one, or choose Open a File Next to This One.");
        return;
    }
    form::Fields f(1);
    f[0] = {form::Field::Choice, L"Show next to this one:", names, L"", 0};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Side by side", f, L"Show", nullptr, [weak, others](const form::Fields& v) {
        const NSUInteger i = static_cast<NSUInteger>(std::max(0, v[0].choice));   // read now: the answers go with the form
        if (i < others.count) {
            NSDocument* d = others[i];
            dispatch_async(dispatch_get_main_queue(), ^{ [weak showBesideDocument:d]; });
        }
    });
}

- (IBAction)sideBySideWithFile:(id)sender {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.title = @"Open next to this document";
    __weak FPWindowController* weak = self;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        NSURL* url = p.URL;
        [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:url display:YES
                                                                     completionHandler:^(NSDocument* doc, BOOL, NSError* e) {
            if (e) { [NSApp presentError:e]; return; }
            // it opens as a tab of its own (so it can be edited too); this one comes back to the front, the new one beside it
            dispatch_async(dispatch_get_main_queue(), ^{ [weak showBesideDocument:doc]; });
        }];
    }];
}

- (IBAction)endSideBySide:(id)sender { end(); }

- (IBAction)toggleSideBySideLink:(id)sender {
    [NSUserDefaults.standardUserDefaults setBool:!linked() forKey:@"sideBySideLinked"];
    if (linked() && active()) follow(g_beside.host.pageView.enclosingScrollView, g_beside.host.pageView, g_beside.scroll, g_beside.view);
}

- (IBAction)sideBySideLeftRight:(id)sender {
    [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"sideBySideStacked"];
    layout();
}

- (IBAction)sideBySideTopBottom:(id)sender {
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"sideBySideStacked"];
    layout();
}

- (IBAction)swapSideBySide:(id)sender {
    g_swapped = !g_swapped;
    layout();
}

// Called by validateMenuItem: (WindowController.mm) for the Side by Side items.
- (BOOL)validateSideBySide:(NSMenuItem*)item {
    if (item.action == @selector(toggleSideBySideLink:)) item.state = linked() ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(sideBySideLeftRight:)) item.state = stacked() ? NSControlStateValueOff : NSControlStateValueOn;
    if (item.action == @selector(sideBySideTopBottom:)) item.state = stacked() ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(endSideBySide:) || item.action == @selector(swapSideBySide:)) return active() && g_beside.host == self;
    if (item.action == @selector(sideBySideWithTab:)) return NSDocumentController.sharedDocumentController.documents.count > 1;
    return YES;
}

@end
