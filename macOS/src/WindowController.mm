#import "WindowController.h"
#import "FPDocument.h"
#import "PageView.h"
#import "Sidebar.h"
#import "Trace.h"
#include <cmath>

NSString* const FPPageColoursKey = @"pageColours";
NSNotificationName const FPPageColoursChanged = @"FPPageColoursChanged";

static NSToolbarItemIdentifier const kPagesItem = @"pages";
static NSToolbarItemIdentifier const kSidebarItem = @"sidebar";
static NSToolbarItemIdentifier const kPageItem = @"page";
static NSToolbarItemIdentifier const kZoomItem = @"zoom";

static const double kZoomPresets[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0};

static pagecolours::Mode savedColours() {
    const NSInteger c = [[NSUserDefaults standardUserDefaults] integerForKey:FPPageColoursKey];
    return c == 1 ? pagecolours::Mode::Dark : c == 2 ? pagecolours::Mode::Dimmed : pagecolours::Mode::Normal;
}

NSString* const FPSidebarShownKey = @"sidebarShown";

@interface FPWindowController () <FPSidebarDelegate, NSSplitViewDelegate>
@end

@implementation FPWindowController {
    FPPageView* _view;
    FPSidebar* _sidebar;
    NSSplitView* _split;
    NSTextField* _pageField;
    NSTextField* _pageCount;
    NSPopUpButton* _zoomMenu;
}

- (instancetype)init {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1000, 760)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    if ((self = [super initWithWindow:w])) {
        w.delegate = self;
        w.tabbingMode = NSWindowTabbingModePreferred;   // documents open as tabs of one window
        w.tabbingIdentifier = @"FeatherPDF";
        w.minSize = NSMakeSize(480, 360);
        [w setFrameAutosaveName:@"FeatherPDFDocument"];
        if (NSEqualPoints(w.frame.origin, NSZeroPoint)) [w center];

        NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:w.contentView.bounds];
        scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scroll.hasVerticalScroller = YES;
        scroll.hasHorizontalScroller = YES;
        scroll.autohidesScrollers = YES;
        scroll.drawsBackground = YES;
        scroll.backgroundColor = [NSColor underPageBackgroundColor];
        _view = [[FPPageView alloc] initWithFrame:scroll.contentView.bounds];
        _view.colours = savedColours();
        scroll.documentView = _view;

        // The sidebar (thumbnails, bookmarks) to the left of the pages, shown or hidden with ⌘B as Ctrl+B on Windows.
        _sidebar = [[FPSidebar alloc] initWithFrame:NSMakeRect(0, 0, 180, 600)];
        _sidebar.delegate = self;
        _sidebar.colours = savedColours();
        _split = [[NSSplitView alloc] initWithFrame:w.contentView.bounds];
        _split.vertical = YES;
        _split.dividerStyle = NSSplitViewDividerStyleThin;
        _split.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _split.delegate = self;
        [_split addSubview:_sidebar];
        [_split addSubview:scroll];
        [_split setHoldingPriority:NSLayoutPriorityDefaultHigh forSubviewAtIndex:0];
        w.contentView = _split;
        [_split setPosition:180 ofDividerAtIndex:0];
        NSNumber* shown = [[NSUserDefaults standardUserDefaults] objectForKey:FPSidebarShownKey];
        if (shown && !shown.boolValue) [self setSidebarShown:NO];

        NSToolbar* bar = [[NSToolbar alloc] initWithIdentifier:@"FeatherPDFToolbar"];
        bar.delegate = self;
        bar.displayMode = NSToolbarDisplayModeIconOnly;
        w.toolbar = bar;
        if (@available(macOS 11.0, *)) w.toolbarStyle = NSWindowToolbarStyleUnified;

        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(viewChanged:) name:FPPageViewChanged object:_view];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(coloursChanged:) name:FPPageColoursChanged object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(reloaded:) name:FPDocumentReloaded object:nil];
        // The document links itself to this window (addWindowController: sets the document). Setting it here first made
        // that call think the window was already added, so it was never shown.
        FPTrace(@"window made");
    }
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setDocument:(id)document {
    [super setDocument:document];
    if ([document isKindOfClass:[FPDocument class]]) {
        _view.document = [(FPDocument*)document pdf];
        [_sidebar setDocument:_view.document];
        [self viewChanged:nil];
    }
}

- (void)windowDidLoad {
    [super windowDidLoad];
    [self.window makeFirstResponder:_view];
}

// ---- the toolbar ----

- (NSArray<NSToolbarItemIdentifier>*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar {
    return @[ kSidebarItem, kPagesItem, kPageItem, NSToolbarFlexibleSpaceItemIdentifier, kZoomItem ];
}

- (NSArray<NSToolbarItemIdentifier>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
    return @[ kSidebarItem, kPagesItem, kPageItem, kZoomItem, NSToolbarFlexibleSpaceItemIdentifier, NSToolbarSpaceItemIdentifier ];
}

static NSImage* symbol(NSString* name, NSString* description) {
    if (@available(macOS 11.0, *)) return [NSImage imageWithSystemSymbolName:name accessibilityDescription:description];
    return nil;
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)ident willBeInsertedIntoToolbar:(BOOL)flag {
    NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:ident];
    if ([ident isEqualToString:kSidebarItem]) {
        NSButton* b = [NSButton buttonWithImage:symbol(@"sidebar.left", @"Thumbnails and bookmarks") target:self action:@selector(toggleThumbnails:)];
        b.bezelStyle = NSBezelStyleTexturedRounded;
        b.toolTip = @"Show or hide the thumbnails and bookmarks (\u2318B)";
        item.view = b;
        item.label = @"Sidebar";
    } else if ([ident isEqualToString:kPagesItem]) {
        NSSegmentedControl* s = [NSSegmentedControl segmentedControlWithImages:@[ symbol(@"chevron.up", @"Previous page"),
                                                                                    symbol(@"chevron.down", @"Next page") ]
                                                                  trackingMode:NSSegmentSwitchTrackingMomentary
                                                                        target:self
                                                                        action:@selector(pagesClicked:)];
        [s setToolTip:@"Previous page" forSegment:0];
        [s setToolTip:@"Next page" forSegment:1];
        item.view = s;
        item.label = @"Previous / Next";
    } else if ([ident isEqualToString:kPageItem]) {
        _pageField = [NSTextField textFieldWithString:@"1"];
        _pageField.alignment = NSTextAlignmentCenter;
        _pageField.delegate = self;
        _pageField.target = self;
        _pageField.action = @selector(pageEntered:);
        [_pageField.widthAnchor constraintEqualToConstant:52].active = YES;
        _pageCount = [NSTextField labelWithString:@"of 1"];
        _pageCount.textColor = [NSColor secondaryLabelColor];
        NSStackView* row = [NSStackView stackViewWithViews:@[ _pageField, _pageCount ]];
        row.spacing = 6;
        item.view = row;
        item.label = @"Page";
        [self viewChanged:nil];
    } else if ([ident isEqualToString:kZoomItem]) {
        NSSegmentedControl* s = [NSSegmentedControl segmentedControlWithImages:@[ symbol(@"minus.magnifyingglass", @"Zoom out"),
                                                                                    symbol(@"plus.magnifyingglass", @"Zoom in") ]
                                                                  trackingMode:NSSegmentSwitchTrackingMomentary
                                                                        target:self
                                                                        action:@selector(zoomClicked:)];
        [s setToolTip:@"Zoom out (⌘−)" forSegment:0];
        [s setToolTip:@"Zoom in (⌘+)" forSegment:1];
        _zoomMenu = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
        _zoomMenu.target = self;
        _zoomMenu.action = @selector(zoomChosen:);
        [_zoomMenu.widthAnchor constraintEqualToConstant:104].active = YES;
        NSStackView* row = [NSStackView stackViewWithViews:@[ s, _zoomMenu ]];
        row.spacing = 8;
        item.view = row;
        item.label = @"Zoom";
        [self viewChanged:nil];
    }
    return item;
}

- (void)pagesClicked:(NSSegmentedControl*)s {
    if (s.selectedSegment == 0) [self previousPage:nil]; else [self nextPage:nil];
}

- (void)zoomClicked:(NSSegmentedControl*)s {
    if (s.selectedSegment == 0) [self zoomOut:nil]; else [self zoomIn:nil];
}

- (void)zoomChosen:(NSPopUpButton*)p {
    const NSInteger tag = p.selectedItem.tag;
    if (tag == 0) [self fitWidth:nil];
    else [_view setZoom:tag / 100.0];
}

- (void)pageEntered:(NSTextField*)f {
    [_view showPage:f.intValue - 1];
    [self.window makeFirstResponder:_view];
}

// The page number, page count, zoom and the window's subtitle follow the view.
- (void)viewChanged:(NSNotification*)n {
    [_sidebar setCurrentPage:[_view currentPage]];
    Document* d = _view.document;
    const int count = d ? d->pageCount() : 0;
    const int page = [_view currentPage] + 1;
    if (_pageField && !_pageField.currentEditor) _pageField.stringValue = [NSString stringWithFormat:@"%d", page];
    _pageCount.stringValue = [NSString stringWithFormat:@"of %d", count];
    const int percent = static_cast<int>(std::lround(_view.zoom * 100));
    if (_zoomMenu) {
        [_zoomMenu removeAllItems];
        [_zoomMenu addItemWithTitle:@"Fit width"];
        _zoomMenu.lastItem.tag = 0;
        for (double z : kZoomPresets) {
            [_zoomMenu addItemWithTitle:[NSString stringWithFormat:@"%d %%", static_cast<int>(z * 100)]];
            _zoomMenu.lastItem.tag = static_cast<NSInteger>(z * 100);
        }
        if (_view.fitWidth) [_zoomMenu selectItemWithTag:0];
        else if (![_zoomMenu selectItemWithTag:percent]) {
            [_zoomMenu insertItemWithTitle:[NSString stringWithFormat:@"%d %%", percent] atIndex:1];
            [_zoomMenu itemAtIndex:1].tag = percent;
            [_zoomMenu selectItemAtIndex:1];
        }
    }
    if (@available(macOS 11.0, *))
        self.window.subtitle = count ? [NSString stringWithFormat:@"Page %d of %d · %d %%", page, count, percent] : @"";
}

- (FPPageView*)pageView { return _view; }
- (Document*)pdf { return _view.document; }

- (void)afterChange {
    [_view documentChanged];
    [_sidebar documentChanged];
    [(FPDocument*)self.document changed];
    [self viewChanged:nil];
}

- (void)reloaded:(NSNotification*)n {
    if (n.object != self.document) return;
    _view.document = [(FPDocument*)self.document pdf];
    [_sidebar setDocument:_view.document];
    [self viewChanged:nil];
}

// ---- the sidebar ----

- (void)sidebarGoToPage:(int)page { [_view showPage:page]; }

- (BOOL)sidebarShown { return !_sidebar.hidden; }

- (void)setSidebarShown:(BOOL)shown {
    _sidebar.hidden = !shown;
    [_split adjustSubviews];
    if (shown) [_split setPosition:180 ofDividerAtIndex:0];
}

- (IBAction)toggleThumbnails:(id)sender {
    [self setSidebarShown:!self.sidebarShown];
    [[NSUserDefaults standardUserDefaults] setBool:self.sidebarShown forKey:FPSidebarShownKey];
}

- (IBAction)showBookmarksPanel:(id)sender {
    [self setSidebarShown:YES];
    [_sidebar showBookmarks:YES];
}

- (CGFloat)splitView:(NSSplitView*)sv constrainMinCoordinate:(CGFloat)min ofSubviewAt:(NSInteger)i { return 140; }
- (CGFloat)splitView:(NSSplitView*)sv constrainMaxCoordinate:(CGFloat)max ofSubviewAt:(NSInteger)i { return 360; }
- (BOOL)splitView:(NSSplitView*)sv canCollapseSubview:(NSView*)v { return v == _sidebar; }

// Undo and redo (the engine's: every change is one step, 30 kept, as on Windows). Not NSWindow's undo: / redo:, which
// would go to macOS's own undo manager.
- (IBAction)featherUndo:(id)sender {
    Document* d = self.pdf;
    if (d && d->canUndo() && d->undo()) [self afterChange];
}
- (IBAction)featherRedo:(id)sender {
    Document* d = self.pdf;
    if (d && d->canRedo() && d->redo()) [self afterChange];
}

- (void)coloursChanged:(NSNotification*)n {
    _view.colours = savedColours();
    _sidebar.colours = savedColours();
}

// ---- menu commands (View and Go menus) ----

- (IBAction)zoomIn:(id)sender { [_view zoomIn]; }
- (IBAction)zoomOut:(id)sender { [_view zoomOut]; }
- (IBAction)actualSize:(id)sender { [_view setZoom:1.0]; }
- (IBAction)fitWidth:(id)sender { _view.fitWidth = YES; }
- (IBAction)nextPage:(id)sender { [_view showPage:[_view currentPage] + 1]; }
- (IBAction)previousPage:(id)sender { [_view showPage:[_view currentPage] - 1]; }
- (IBAction)firstPage:(id)sender { [_view showPage:0]; }
- (IBAction)lastPage:(id)sender { [_view showPage:_view.document ? _view.document->pageCount() - 1 : 0]; }
- (IBAction)goToPage:(id)sender { [self.window makeFirstResponder:_pageField]; }

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    if (item.action == @selector(toggleThumbnails:)) item.title = self.sidebarShown ? @"Hide Sidebar" : @"Show Sidebar";
    if (item.action == @selector(featherUndo:)) return self.pdf && self.pdf->canUndo();
    if (item.action == @selector(featherRedo:)) return self.pdf && self.pdf->canRedo();
    if (item.action == @selector(fitWidth:)) item.state = _view.fitWidth ? NSControlStateValueOn : NSControlStateValueOff;
    const int count = _view.document ? _view.document->pageCount() : 0;
    const int page = [_view currentPage];
    if (item.action == @selector(nextPage:) || item.action == @selector(lastPage:)) return page + 1 < count;
    if (item.action == @selector(previousPage:) || item.action == @selector(firstPage:)) return page > 0;
    return YES;
}

@end
