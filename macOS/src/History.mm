// The undo history (Window > Undo History), as the History tab on Windows: the start, each change of the document in
// front (oldest first, with its time), then the changes that were undone. Clicking a line goes back (or forward) to it.
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "Trace.h"
#include <ctime>
#include "platform.hpp"

@interface FPHistory : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property(nonatomic, strong) NSPanel* panel;
@property(nonatomic, strong) NSTableView* table;
@property(nonatomic, weak) FPWindowController* target;
@property(nonatomic, strong) NSTimer* timer;
+ (instancetype)shared;
- (void)reload;
@end

@implementation FPHistory {
    std::vector<Document::Step> _done, _undone;
    int _revision;
    Document* _doc;
}

+ (instancetype)shared {
    static FPHistory* h;
    if (!h) h = [[FPHistory alloc] init];
    return h;
}

- (void)show {
    if (!self.panel) {
        self.panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 300, 420)
                                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskUtilityWindow
                                                  backing:NSBackingStoreBuffered defer:NO];
        self.panel.title = @"Undo History";
        self.panel.floatingPanel = YES;
        self.panel.hidesOnDeactivate = YES;
        self.panel.releasedWhenClosed = NO;
        NSScrollView* sv = [[NSScrollView alloc] initWithFrame:self.panel.contentView.bounds];
        sv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        sv.hasVerticalScroller = YES;
        self.table = [[NSTableView alloc] initWithFrame:sv.bounds];
        NSTableColumn* c = [[NSTableColumn alloc] initWithIdentifier:@"step"];
        c.resizingMask = NSTableColumnAutoresizingMask;
        [self.table addTableColumn:c];
        self.table.headerView = nil;
        self.table.rowHeight = 36;
        self.table.dataSource = self;
        self.table.delegate = self;
        self.table.target = self;
        self.table.action = @selector(clicked:);
        sv.documentView = self.table;
        self.panel.contentView = sv;
        [self.panel center];
        [self.panel setFrameTopLeftPoint:NSMakePoint(NSMaxX(NSScreen.mainScreen.visibleFrame) - 320, NSMaxY(NSScreen.mainScreen.visibleFrame) - 40)];
    }
    _revision = -1;
    [self reload];
    [self.panel orderFront:nil];
    if (!self.timer) self.timer = [NSTimer scheduledTimerWithTimeInterval:0.7 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
}

- (void)tick:(NSTimer*)t { if (self.panel.visible) [self reload]; }

// The window in front (a document window): its history.
- (void)reload {
    FPWindowController* wc = nil;
    for (NSWindow* w in @[ NSApp.mainWindow ?: NSNull.null, NSApp.keyWindow ?: NSNull.null ])
        if ([w isKindOfClass:[NSWindow class]] && [w.windowController isKindOfClass:[FPWindowController class]]) { wc = w.windowController; break; }
    if (!wc && self.target) wc = self.target;
    self.target = wc;
    Document* d = wc.pdf;
    const int rev = d ? d->revision() : -2;
    if (d == _doc && rev == _revision) return;
    _doc = d;
    _revision = rev;
    _done = d ? d->undoSteps() : std::vector<Document::Step>{};
    _undone = d ? d->redoSteps() : std::vector<Document::Step>{};
    [self.table reloadData];
    if (d) [self.table selectRowIndexes:[NSIndexSet indexSetWithIndex:_done.size()] byExtendingSelection:NO];
    self.panel.title = wc ? [@"Undo History — " stringByAppendingString:wc.window.title] : @"Undo History";
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv { return _doc ? static_cast<NSInteger>(1 + _done.size() + _undone.size()) : 0; }

- (NSView*)tableView:(NSTableView*)tv viewForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    NSTextField* f = [NSTextField wrappingLabelWithString:@""];
    const size_t r = static_cast<size_t>(row), now = _done.size();
    std::wstring what, when;
    if (r == 0) what = _done.size() >= 30 ? L"Earlier changes (no longer kept)" : L"As opened";
    else {
        const Document::Step& s = r <= now ? _done[r - 1] : _undone[r - now - 1];
        what = s.what;
        const time_t tt = static_cast<time_t>(s.when);
        tm lt{};
        localtime_r(&tt, &lt);
        char b[16];
        snprintf(b, sizeof(b), "%02d:%02d:%02d", lt.tm_hour, lt.tm_min, lt.tm_sec);
        when = platform::wide(b);
    }
    const std::wstring note = r == now ? L"now" : r > now ? L"undone: click to redo" : L"";
    NSString* line2 = [NSString stringWithUTF8String:platform::utf8(when + (when.empty() || note.empty() ? L"" : L" · ") + note).c_str()];
    NSMutableAttributedString* s = [[NSMutableAttributedString alloc]
        initWithString:[NSString stringWithFormat:@"%s\n%@", platform::utf8(what).c_str(), line2]];
    const NSUInteger first = s.length - line2.length - 1;
    [s addAttribute:NSFontAttributeName value:[NSFont systemFontOfSize:13] range:NSMakeRange(0, first)];
    [s addAttribute:NSForegroundColorAttributeName value:(r > now ? NSColor.secondaryLabelColor : NSColor.labelColor) range:NSMakeRange(0, first)];
    [s addAttribute:NSFontAttributeName value:[NSFont systemFontOfSize:11] range:NSMakeRange(first, s.length - first)];
    [s addAttribute:NSForegroundColorAttributeName value:NSColor.secondaryLabelColor range:NSMakeRange(first, s.length - first)];
    f.attributedStringValue = s;
    return f;
}

- (void)clicked:(id)sender {
    const NSInteger row = self.table.clickedRow >= 0 ? self.table.clickedRow : self.table.selectedRow;
    [self goTo:row];
}

- (void)goTo:(NSInteger)row {
    FPWindowController* wc = self.target;
    Document* d = wc.pdf;
    if (!d || row < 0) return;
    const int n = static_cast<int>(_done.size()) - static_cast<int>(row);
    if (n == 0) return;
    const bool ok = d->stepBack(n);
    [wc afterChange];
    FPTrace(@"history: went %d step(s) %s, ok %d, now %d steps done, %d undone, %d pages", std::abs(n), n > 0 ? "back" : "forward", ok,
            static_cast<int>(d->undoSteps().size()), static_cast<int>(d->redoSteps().size()), d->pageCount());
    _revision = -1;
    [self reload];
}

@end

@interface FPWindowController (History)
@end

@implementation FPWindowController (History)

- (IBAction)showUndoHistory:(id)sender {
    FPHistory* h = [FPHistory shared];
    h.target = self;
    // The build check: three changes, the panel, then back to the start and forward one step.
    if (getenv("FEATHER_UI_HISTORY") && self.pdf) {
        Document* d = self.pdf;
        d->rotatePages({0}, 1);
        d->insertBlankPage(0, 0);
        d->reversePages();
        [self afterChange];
    }
    [h show];
    std::wstring names;
    for (const auto& s : self.pdf ? self.pdf->undoSteps() : std::vector<Document::Step>{}) names += L" " + s.what + L";";
    FPTrace(@"history:%s", platform::utf8(names).c_str());
    if (getenv("FEATHER_UI_HISTORY") && self.pdf) {
        [h goTo:0];
        [h goTo:1];
    }
}

@end
