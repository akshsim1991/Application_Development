// Right-click on a tab, as on Windows: open the folder with the tab's file (Finder, the file selected), copy its path,
// close it, close the others, move it to a window of its own. macOS draws the tabs itself and its tab menu cannot be
// added to, so a right-click on a tab shows this menu instead (with macOS's own items in it). When the tab under the
// pointer cannot be told, macOS's own menu shows as before. The File menu has the folder item too.
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "Trace.h"
#include "platform.hpp"

// A menu item that runs a block (kept by the item).
@interface FPTabMenuAction : NSObject
@property(nonatomic, copy) void (^run)(void);
- (void)fire:(id)sender;
@end
@implementation FPTabMenuAction
- (void)fire:(id)sender { if (self.run) self.run(); }
@end

namespace {

// The tab bar's views are AppKit's own: found by their class names ("...TabBar...", "...TabButton..."), never relied on
// beyond telling which tab was clicked.
NSView* findView(NSView* v, NSString* part, int depth = 0) {
    if (!v || depth > 12) return nil;
    if ([NSStringFromClass([v class]) containsString:part] && !v.hidden) return v;
    for (NSView* s in v.subviews)
        if (NSView* f = findView(s, part, depth + 1)) return f;
    return nil;
}

void collectButtons(NSView* v, NSMutableArray<NSView*>* out, int depth = 0) {
    if (!v || depth > 6) return;
    NSString* c = NSStringFromClass([v class]);
    if ([c containsString:@"Tab"] && ([c containsString:@"Button"] || [c containsString:@"Item"]) && !v.hidden && v.frame.size.width > 30) {
        [out addObject:v];
        return;
    }
    for (NSView* s in v.subviews) collectButtons(s, out, depth + 1);
}

// The window whose tab is under the event, or nil.
NSWindow* tabWindowAt(NSEvent* e) {
    NSWindow* w = e.window;
    NSArray<NSWindow*>* tabs = w.tabbedWindows;
    if (!w || tabs.count < 1) return nil;
    NSView* frame = w.contentView.superview;
    NSView* bar = findView(frame, @"TabBar");
    if (!bar) return nil;
    const NSPoint inBar = [bar convertPoint:e.locationInWindow fromView:nil];
    if (!NSPointInRect(inBar, bar.bounds)) return nil;
    NSMutableArray<NSView*>* buttons = [NSMutableArray array];
    collectButtons(bar, buttons);
    [buttons sortUsingComparator:^NSComparisonResult(NSView* a, NSView* b) {
        const CGFloat ax = [a convertPoint:NSZeroPoint toView:nil].x, bx = [b convertPoint:NSZeroPoint toView:nil].x;
        return ax < bx ? NSOrderedAscending : ax > bx ? NSOrderedDescending : NSOrderedSame;
    }];
    NSInteger index = -1;
    if (buttons.count == tabs.count) {
        for (NSUInteger i = 0; i < buttons.count; ++i)
            if (NSPointInRect([buttons[i] convertPoint:e.locationInWindow fromView:nil], buttons[i].bounds)) { index = static_cast<NSInteger>(i); break; }
    } else {
        // the tabs share the bar's width equally
        const CGFloat each = bar.bounds.size.width / tabs.count;
        index = static_cast<NSInteger>(inBar.x / each);
    }
    if (index < 0 || index >= static_cast<NSInteger>(tabs.count)) return nil;
    return tabs[static_cast<NSUInteger>(index)];
}

void add(NSMenu* m, NSString* title, BOOL enabled, void (^run)(void)) {
    NSMenuItem* item = [m addItemWithTitle:title action:enabled ? @selector(fire:) : nil keyEquivalent:@""];
    FPTabMenuAction* a = [[FPTabMenuAction alloc] init];
    a.run = run;
    item.target = a;
    item.representedObject = a;
    item.enabled = enabled;
}

} // namespace

void FPShowFileInFolder(NSDocument* doc) {
    NSURL* url = doc.fileURL;
    if (url && [NSFileManager.defaultManager fileExistsAtPath:url.path]) [NSWorkspace.sharedWorkspace activateFileViewerSelectingURLs:@[ url ]];
    else NSBeep();
}

NSMenu* FPTabMenuFor(NSWindow* tab) {
    NSDocument* doc = tab.windowController.document;
    NSURL* url = doc.fileURL;
    const BOOL onDisk = url && [NSFileManager.defaultManager fileExistsAtPath:url.path];
    NSMenu* m = [[NSMenu alloc] initWithTitle:@"Tab"];
    m.autoenablesItems = NO;
    __weak NSWindow* weakTab = tab;
    __weak NSDocument* weakDoc = doc;
    add(m, @"Open Directory Having This File", onDisk, ^{ FPShowFileInFolder(weakDoc); });
    add(m, @"Copy the File's Path", url != nil, ^{
        [NSPasteboard.generalPasteboard clearContents];
        [NSPasteboard.generalPasteboard setString:weakDoc.fileURL.path ?: @"" forType:NSPasteboardTypeString];
    });
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Close Tab", YES, ^{ [weakTab performClose:nil]; });
    add(m, @"Close Other Tabs", tab.tabbedWindows.count > 1, ^{
        for (NSWindow* w in [weakTab.tabbedWindows copy]) if (w != weakTab) [w performClose:nil];   // each asks about unsaved changes
    });
    add(m, @"Move Tab to New Window", tab.tabbedWindows.count > 1, ^{ [weakTab moveTabToNewWindow:nil]; });
    return m;
}

@interface FPWindowController (TabMenu)
@end
@implementation FPWindowController (TabMenu)
- (IBAction)showFileInFolder:(id)sender { FPShowFileInFolder(self.document); }   // File > Open Directory Having This File
- (IBAction)uiTabMenu:(id)sender {   // the build check: the tab menu's items, written to the trace
    NSMutableArray* titles = [NSMutableArray array];
    for (NSMenuItem* i in FPTabMenuFor(self.window).itemArray) if (!i.separatorItem) [titles addObject:[NSString stringWithFormat:@"%@%@", i.title, i.enabled ? @"" : @" (greyed)"]];
    FPTrace(@"tab menu: %@", [titles componentsJoinedByString:@" | "]);
}
@end

void FPInstallTabMenu() {
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskRightMouseDown | NSEventMaskLeftMouseDown handler:^NSEvent*(NSEvent* e) {
        if (e.type == NSEventTypeLeftMouseDown && !(e.modifierFlags & NSEventModifierFlagControl)) return e;   // ctrl-click is a right-click
        NSWindow* tab = tabWindowAt(e);
        if (!tab || ![tab.windowController isKindOfClass:[FPWindowController class]]) return e;
        [NSMenu popUpContextMenu:FPTabMenuFor(tab) withEvent:e forView:e.window.contentView.superview];
        return nil;   // instead of macOS's own tab menu
    }];
}
