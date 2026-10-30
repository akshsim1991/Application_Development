// Side by side (View menu), as on Windows: another open document, or a file, shown next to this one, left and right or
// top and bottom, with linked scrolling (both scroll and zoom together) and a swap. On the Mac the two documents are two
// windows sharing the screen; "Back to One Document" puts the other one back as a tab.
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "WindowController.h"
#import "PageView.h"
#import "Form.h"
#import <objc/runtime.h>
#include <cmath>
#include <vector>

namespace {
__weak FPWindowController* g_first;    // the document side by side was started from
__weak FPWindowController* g_second;   // the one beside it
BOOL g_swapped = NO;
BOOL g_syncing = NO;
NSRect g_before;                       // the first window's frame before, given back at the end

BOOL linked() { NSNumber* v = [NSUserDefaults.standardUserDefaults objectForKey:@"sideBySideLinked"]; return v ? v.boolValue : YES; }
BOOL stacked() { return [NSUserDefaults.standardUserDefaults boolForKey:@"sideBySideStacked"]; }
BOOL active() { return g_first && g_second && g_first.window.isVisible && g_second.window.isVisible; }

// Both windows on the screen of the first: halves, left and right or top and bottom.
void arrange() {
    if (!active()) return;
    NSRect r = (g_first.window.screen ?: NSScreen.mainScreen).visibleFrame;
    NSRect a, b;
    if (stacked()) NSDivideRect(r, &a, &b, std::floor(r.size.height / 2), NSMaxYEdge);
    else NSDivideRect(r, &a, &b, std::floor(r.size.width / 2), NSMinXEdge);
    [(g_swapped ? g_second : g_first).window setFrame:a display:YES animate:YES];
    [(g_swapped ? g_first : g_second).window setFrame:b display:YES animate:YES];
    for (FPWindowController* c in @[ g_first, g_second ]) c.pageView.fitWidth = YES;
}

// Linked: the other side follows the zoom and the position (the same share of the way through the document).
void follow(FPWindowController* from) {
    if (g_syncing || !linked() || !active()) return;
    FPWindowController* to = from == g_first ? g_second : g_first;
    NSScrollView* a = from.pageView.enclosingScrollView;
    NSScrollView* b = to.pageView.enclosingScrollView;
    if (!a || !b) return;
    g_syncing = YES;
    if (std::abs(to.pageView.zoom - from.pageView.zoom) > 0.001) [to.pageView setZoom:from.pageView.zoom];
    const CGFloat rangeA = NSHeight(a.documentView.frame) - NSHeight(a.contentView.bounds);
    const CGFloat rangeB = NSHeight(b.documentView.frame) - NSHeight(b.contentView.bounds);
    if (rangeA > 0 && rangeB > 0) {
        const CGFloat share = a.contentView.bounds.origin.y / rangeA;
        NSPoint p = b.contentView.bounds.origin;
        p.y = std::round(share * rangeB);
        [b.contentView scrollToPoint:p];
        [b reflectScrolledClipView:b.contentView];
    }
    g_syncing = NO;
}

void watch(FPWindowController* c) {
    NSClipView* clip = c.pageView.enclosingScrollView.contentView;
    clip.postsBoundsChangedNotifications = YES;
    __weak FPWindowController* weak = c;
    id token = [[NSNotificationCenter defaultCenter] addObserverForName:NSViewBoundsDidChangeNotification object:clip queue:nil
                                                             usingBlock:^(NSNotification*) { if (weak) follow(weak); }];
    objc_setAssociatedObject(c, "sbsWatch", token, OBJC_ASSOCIATION_RETAIN);
}

void unwatch(FPWindowController* c) {
    if (!c) return;
    if (id token = objc_getAssociatedObject(c, "sbsWatch")) [[NSNotificationCenter defaultCenter] removeObserver:token];
    objc_setAssociatedObject(c, "sbsWatch", nil, OBJC_ASSOCIATION_RETAIN);
}
} // namespace

@interface FPWindowController (SideBySide)
@end

@implementation FPWindowController (SideBySide)

- (void)showBeside:(FPWindowController*)other {
    if (!other || other == self) return;
    if (active()) [self endSideBySide:nil];
    g_before = self.window.frame;
    if (other.window.tabGroup.windows.count > 1 || other.window.tabbedWindows.count > 1)
        [other.window.tabGroup removeWindow:other.window];   // its own window, beside this one
    g_first = self;
    g_second = other;
    g_swapped = NO;
    arrange();
    watch(self);
    watch(other);
    [other.window orderFront:nil];
    [self.window makeKeyAndOrderFront:nil];
    follow(self);
}

- (IBAction)sideBySideWithTab:(id)sender {
    NSMutableArray<FPWindowController*>* others = [NSMutableArray array];
    std::vector<std::wstring> names;
    for (NSDocument* d in NSDocumentController.sharedDocumentController.documents)
        for (NSWindowController* wc in d.windowControllers)
            if (wc != self && [wc isKindOfClass:[FPWindowController class]]) {
                [others addObject:(FPWindowController*)wc];
                names.push_back(form::ws(d.displayName));
            }
    if (!others.count) {
        form::message(self.window, L"Only this document is open. Open another one, or choose Side by Side with a File.");
        return;
    }
    form::Fields f(1);
    f[0] = {form::Field::Choice, L"Show next to this one:", names, L"", 0};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Side by side", f, L"Show", nullptr, [weak, others](const form::Fields& v) {
        const NSUInteger i = static_cast<NSUInteger>(std::max(0, v[0].choice));
        if (i < others.count) [weak showBeside:others[i]];
    });
}

- (IBAction)sideBySideWithFile:(id)sender {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.title = @"Open next to this document";
    __weak FPWindowController* weak = self;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:p.URL display:YES
                                                                     completionHandler:^(NSDocument* doc, BOOL, NSError* e) {
            if (e) { [NSApp presentError:e]; return; }
            FPWindowController* other = (FPWindowController*)doc.windowControllers.firstObject;
            dispatch_async(dispatch_get_main_queue(), ^{ [weak showBeside:other]; });
        }];
    }];
}

- (IBAction)endSideBySide:(id)sender {
    FPWindowController* first = g_first;
    FPWindowController* second = g_second;
    unwatch(first);
    unwatch(second);
    g_first = nil;
    g_second = nil;
    if (!first) return;
    if (second && second.window.isVisible) [first.window addTabbedWindow:second.window ordered:NSWindowAbove];
    if (!NSIsEmptyRect(g_before)) [first.window setFrame:g_before display:YES animate:YES];
    first.pageView.fitWidth = YES;
    [first.window makeKeyAndOrderFront:nil];
}

- (IBAction)toggleSideBySideLink:(id)sender {
    [NSUserDefaults.standardUserDefaults setBool:!linked() forKey:@"sideBySideLinked"];
    if (linked() && active()) follow(g_first);
}

- (IBAction)sideBySideLeftRight:(id)sender {
    [NSUserDefaults.standardUserDefaults setBool:NO forKey:@"sideBySideStacked"];
    arrange();
}

- (IBAction)sideBySideTopBottom:(id)sender {
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:@"sideBySideStacked"];
    arrange();
}

- (IBAction)swapSideBySide:(id)sender {
    g_swapped = !g_swapped;
    arrange();
}

// Called by validateMenuItem: (WindowController.mm) for the Side by Side items.
- (BOOL)validateSideBySide:(NSMenuItem*)item {
    if (item.action == @selector(toggleSideBySideLink:)) item.state = linked() ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(sideBySideLeftRight:)) item.state = stacked() ? NSControlStateValueOff : NSControlStateValueOn;
    if (item.action == @selector(sideBySideTopBottom:)) item.state = stacked() ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(endSideBySide:) || item.action == @selector(swapSideBySide:)) return active();
    if (item.action == @selector(sideBySideWithTab:)) return NSDocumentController.sharedDocumentController.documents.count > 1;
    return YES;
}

@end
