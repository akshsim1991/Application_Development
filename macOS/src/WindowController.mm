#import "WindowController.h"
#import <objc/message.h>
#import <objc/runtime.h>
#import "FPDocument.h"
#import "PageView.h"
#import "Sidebar.h"
#import "Settings.h"
#import "Trace.h"
#include <algorithm>
#include <cmath>
#include "search.hpp"
#include "platform.hpp"

NSString* const FPPageColoursKey = @"pageColours";
NSNotificationName const FPPageColoursChanged = @"FPPageColoursChanged";

static NSToolbarItemIdentifier const kPagesItem = @"pages";
static NSToolbarItemIdentifier const kSidebarItem = @"sidebar";
static NSToolbarItemIdentifier const kFindItem = @"find";
static NSToolbarItemIdentifier const kEditItem = @"editpdf";
static NSToolbarItemIdentifier const kRotateItem = @"rotate";
extern BOOL FPInlineEditing();   // Editing.mm: a paragraph is being typed in

@interface FPWindowController (EditingCalls)   // Editing.mm
- (IBAction)chooseEditTool:(NSMenuItem*)sender;
- (void)endInlineEdit:(BOOL)apply;
@end
@interface FPWindowController (ToolsCalls)   // Tools.mm
- (IBAction)rotateRight:(id)sender;
- (IBAction)rotateLeft:(id)sender;
@end
static NSToolbarItemIdentifier const kPageItem = @"page";
static NSToolbarItemIdentifier const kZoomItem = @"zoom";

static const double kZoomPresets[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0};

static pagecolours::Mode savedColours() {
    const NSInteger c = [[NSUserDefaults standardUserDefaults] integerForKey:FPPageColoursKey];
    return c == 1 ? pagecolours::Mode::Dark : c == 2 ? pagecolours::Mode::Dimmed : c == 3 ? pagecolours::Mode::BlackboardInverted
         : c == 4 ? pagecolours::Mode::BlackboardOriginal : pagecolours::Mode::Normal;
}

NSString* const FPSidebarShownKey = @"sidebarShown";

@interface FPWindowController () <FPSidebarDelegate, NSSplitViewDelegate>
@end

@implementation FPWindowController {
    FPPageView* _view;
    FPSidebar* _sidebar;
    NSSplitView* _split;
    NSSearchField* _find;
    NSTextField* _status;
    NSString* _tabMark;   // this tab's colour (Settings > Appearance: random tab colours)
    std::vector<TextHit> _hits;
    int _hit;
    std::wstring _searched;
    std::vector<int> _back, _forward;   // pages to go back and forward to
    NSTextField* _pageField;
    NSTextField* _pageCount;
    NSComboBox* _zoomMenu;
    NSView* _editBar;                  // under the toolbar while editing: the editing tools, one click each, and Done
    NSLayoutConstraint* _splitTop;     // the pages start below it
    NSButton* _editButton;             // Edit PDF in the toolbar
}

- (instancetype)init {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1000, 760)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    if ((self = [super initWithWindow:w])) {
        w.delegate = self;
        // documents open as tabs of one window; or each in its own (the setting, or File > New Window), which can still be
        // made a tab later (Window > Merge All Windows, or dragged onto a tab bar)
        extern BOOL FPTakeOwnWindow();
        const BOOL own = FPTakeOwnWindow();
        w.tabbingMode = own ? NSWindowTabbingModeDisallowed : NSWindowTabbingModePreferred;
        if (own) dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ w.tabbingMode = NSWindowTabbingModeAutomatic; });
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
        scroll.backgroundColor = FPThemeSurround();
        _view = [[FPPageView alloc] initWithFrame:scroll.contentView.bounds];
        _view.colours = savedColours();
        scroll.documentView = _view;
        __weak FPWindowController* weakSelf = self;
        _view.jumped = ^(int from) { [weakSelf rememberJumpFrom:from]; };
        [self setupTools];

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
        // the status bar under the pages, as on Windows: page, page size, the tool in use, saved or not
        NSView* content = [[NSView alloc] initWithFrame:w.contentView.bounds];
        _status = [NSTextField labelWithString:@""];
        _status.font = [NSFont systemFontOfSize:11];
        _status.textColor = NSColor.secondaryLabelColor;
        _status.lineBreakMode = NSLineBreakByTruncatingTail;
        _split.translatesAutoresizingMaskIntoConstraints = NO;
        _status.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:_split];
        [content addSubview:_status];
        [NSLayoutConstraint activateConstraints:@[
            _splitTop = [_split.topAnchor constraintEqualToAnchor:content.topAnchor],
            [_split.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
            [_split.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
            [_split.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-24],
            [_status.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:12],
            [_status.trailingAnchor constraintLessThanOrEqualToAnchor:content.trailingAnchor constant:-12],
            [_status.centerYAnchor constraintEqualToAnchor:content.bottomAnchor constant:-12],
        ]];
        [self makeEditBarIn:content];
        w.contentView = content;
        [w setContentBorderThickness:24 forEdge:NSRectEdgeMinY];
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
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(settingsChanged:) name:FPSettingsChanged object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(settingsChanged:) name:NSWindowDidBecomeKeyNotification object:nil];
        static const char* marks[] = {"\U0001F7E5", "\U0001F7E7", "\U0001F7E8", "\U0001F7E9", "\U0001F7E6", "\U0001F7EA"};   // coloured squares
        _tabMark = [NSString stringWithUTF8String:marks[arc4random_uniform(6)]];
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
        [self hookDocument];
        [self restorePosition];
        [self viewChanged:nil];
        SEL warn = NSSelectorFromString(@"warnIfSignatureBroken");   // Signatures.mm
        if ([self respondsToSelector:warn]) dispatch_async(dispatch_get_main_queue(), ^{ ((void (*)(id, SEL))objc_msgSend)(self, warn); });
    }
}

- (void)windowDidLoad {
    [super windowDidLoad];
    [self.window makeFirstResponder:_view];
}

// ---- edit mode: the Edit PDF button and the edit bar under the toolbar ----

static BOOL editBarTool(FPTool t) {   // the tools of Edit PDF: the bar stays while one of them is in use
    return t == FPToolEditAll || t == FPToolEditText || t == FPToolEditPara || t == FPToolSelectObject || t == FPToolTextBox ||
           t == FPToolPicture || t == FPToolEditTable || t == FPToolPlaceTable || t == FPToolRedact || t == FPToolCrop;
}

- (void)makeEditBarIn:(NSView*)content {
    NSStackView* row = [[NSStackView alloc] init];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.spacing = 4;
    auto button = [&](NSString* title, SEL action, NSInteger tag) {
        NSButton* b = [NSButton buttonWithTitle:title target:self action:action];
        b.bezelStyle = NSBezelStyleRecessed;
        b.showsBorderOnlyWhileMouseInside = YES;
        b.tag = tag;
        [row addArrangedSubview:b];
        return b;
    };
    button(@"Edit Text and Pictures", @selector(editBarEdit:), 0);
    button(@"Add Text", @selector(chooseEditTool:), FPToolTextBox);
    button(@"Insert Picture…", @selector(insertPicture:), 0);
    button(@"Add a Table…", @selector(addNewTable:), 0);
    button(@"Insert Equation…", @selector(insertEquation:), 0);
    button(@"Find and Replace…", @selector(findAndReplace:), 0);
    button(@"Redact an Area", @selector(chooseTool:), FPToolRedact);
    button(@"More Editing Tools…", @selector(editBarMore:), 0);
    NSButton* done = [NSButton buttonWithTitle:@"Done" target:self action:@selector(editBarDone:)];
    done.bezelStyle = NSBezelStyleRounded;
    done.keyEquivalent = @"";
    _editBar = [[NSView alloc] init];
    _editBar.wantsLayer = YES;
    _editBar.layer.backgroundColor = [NSColor.windowBackgroundColor colorWithAlphaComponent:0.97].CGColor;
    NSBox* line = [[NSBox alloc] init];
    line.boxType = NSBoxSeparator;
    for (NSView* v in @[ row, done, line ]) { v.translatesAutoresizingMaskIntoConstraints = NO; [_editBar addSubview:v]; }
    _editBar.translatesAutoresizingMaskIntoConstraints = NO;
    _editBar.hidden = YES;
    [content addSubview:_editBar];
    [NSLayoutConstraint activateConstraints:@[
        [_editBar.topAnchor constraintEqualToAnchor:content.topAnchor],
        [_editBar.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
        [_editBar.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
        [_editBar.heightAnchor constraintEqualToConstant:36],
        [row.leadingAnchor constraintEqualToAnchor:_editBar.leadingAnchor constant:10],
        [row.centerYAnchor constraintEqualToAnchor:_editBar.centerYAnchor],
        [row.trailingAnchor constraintLessThanOrEqualToAnchor:done.leadingAnchor constant:-12],
        [done.trailingAnchor constraintEqualToAnchor:_editBar.trailingAnchor constant:-12],
        [done.centerYAnchor constraintEqualToAnchor:_editBar.centerYAnchor],
        [line.leadingAnchor constraintEqualToAnchor:_editBar.leadingAnchor],
        [line.trailingAnchor constraintEqualToAnchor:_editBar.trailingAnchor],
        [line.bottomAnchor constraintEqualToAnchor:_editBar.bottomAnchor],
    ]];
    [row setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    _view.editingNow = ^BOOL { return FPInlineEditing(); };
}

// The bar follows the tool: shown while an Edit PDF tool is in use, the pages below it.
- (void)updateEditBar {
    const BOOL on = _view.document && editBarTool(_view.tool);
    if (_editBar.hidden == !on) { _editButton.state = on ? NSControlStateValueOn : NSControlStateValueOff; return; }
    _editBar.hidden = !on;
    _splitTop.constant = on ? 36 : 0;
    _editButton.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    FPTrace(@"edit bar %d", on);
}

- (IBAction)editPdfClicked:(id)sender {
    if (!self.pdf) return;
    NSMenuItem* m = [[NSMenuItem alloc] init];
    m.tag = FPToolEditAll;   // chooseEditTool: turns edit mode off when it is on
    if (editBarTool(_view.tool) && _view.tool != FPToolEditAll) m.tag = FPToolNone;
    if (m.tag == FPToolNone) { [self editBarDone:sender]; return; }
    [self chooseEditTool:m];
}

- (IBAction)editBarEdit:(id)sender {
    if (_view.tool == FPToolEditAll) return;
    NSMenuItem* m = [[NSMenuItem alloc] init];
    m.tag = FPToolEditAll;
    [self chooseEditTool:m];
}

- (IBAction)editBarDone:(id)sender {
    [self endInlineEdit:YES];
    _view.tool = FPToolNone;
}

- (IBAction)editBarMore:(NSButton*)sender {   // the whole Edit Text menu, under the button
    NSMenu* m = [[NSApp.mainMenu itemWithTitle:@"Edit Text"].submenu copy];
    if (m) [m popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, NSHeight(sender.bounds) + 4) inView:sender];
}

// ---- the toolbar ----

- (NSArray<NSToolbarItemIdentifier>*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar {
    return @[ kSidebarItem, kEditItem, kPagesItem, kPageItem, NSToolbarFlexibleSpaceItemIdentifier, kRotateItem, kZoomItem, kFindItem ];
}

- (NSArray<NSToolbarItemIdentifier>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
    return @[ kSidebarItem, kEditItem, kPagesItem, kPageItem, kRotateItem, kZoomItem, kFindItem, NSToolbarFlexibleSpaceItemIdentifier, NSToolbarSpaceItemIdentifier ];
}

static NSImage* symbol(NSString* name, NSString* description) {
    if (@available(macOS 11.0, *)) return [NSImage imageWithSystemSymbolName:name accessibilityDescription:description];
    return nil;
}

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)ident willBeInsertedIntoToolbar:(BOOL)flag {
    NSToolbarItem* item = [[NSToolbarItem alloc] initWithItemIdentifier:ident];
    if ([ident isEqualToString:kFindItem]) {
        _find = [[NSSearchField alloc] initWithFrame:NSMakeRect(0, 0, 200, 24)];
        _find.placeholderString = @"Find";
        _find.sendsWholeSearchString = YES;
        _find.target = self;
        _find.action = @selector(findEntered:);
        [self buildFindMenu];
        [_find.widthAnchor constraintEqualToConstant:200].active = YES;
        item.view = _find;
        item.label = @"Find";
    } else if ([ident isEqualToString:kEditItem]) {   // one click: edit mode, with every paragraph and picture outlined
        _editButton = [NSButton buttonWithTitle:@"Edit PDF" image:symbol(@"pencil", @"Edit PDF") target:self action:@selector(editPdfClicked:)];
        _editButton.imagePosition = NSImageLeading;
        _editButton.bezelStyle = NSBezelStyleTexturedRounded;
        [_editButton setButtonType:NSButtonTypePushOnPushOff];
        _editButton.toolTip = @"Edit the text and pictures of the page: click any outlined text to change it (⌘E)";
        item.view = _editButton;
        item.label = @"Edit PDF";
    } else if ([ident isEqualToString:kSidebarItem]) {
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
    } else if ([ident isEqualToString:kRotateItem]) {   // the page shown; with Shift, every page
        NSSegmentedControl* s = [NSSegmentedControl segmentedControlWithImages:@[ symbol(@"rotate.left", @"Rotate left"),
                                                                                    symbol(@"rotate.right", @"Rotate right") ]
                                                                  trackingMode:NSSegmentSwitchTrackingMomentary
                                                                        target:self
                                                                        action:@selector(rotateClicked:)];
        [s setToolTip:@"Rotate this page left (⌘L; Shift-click: every page)" forSegment:0];
        [s setToolTip:@"Rotate this page right (⌘R; Shift-click: every page)" forSegment:1];
        item.view = s;
        item.label = @"Rotate";
    } else if ([ident isEqualToString:kZoomItem]) {
        NSSegmentedControl* s = [NSSegmentedControl segmentedControlWithImages:@[ symbol(@"minus.magnifyingglass", @"Zoom out"),
                                                                                    symbol(@"plus.magnifyingglass", @"Zoom in") ]
                                                                  trackingMode:NSSegmentSwitchTrackingMomentary
                                                                        target:self
                                                                        action:@selector(zoomClicked:)];
        [s setToolTip:@"Zoom out (⌘−)" forSegment:0];
        [s setToolTip:@"Zoom in (⌘+)" forSegment:1];
        // the zoom typed in (200, then Return) or chosen from the list, as the Windows app's zoom box
        _zoomMenu = [[NSComboBox alloc] initWithFrame:NSZeroRect];
        [_zoomMenu addItemWithObjectValue:@"Fit width"];
        [_zoomMenu addItemWithObjectValue:@"Fit page"];
        for (double z : kZoomPresets) [_zoomMenu addItemWithObjectValue:[NSString stringWithFormat:@"%d %%", static_cast<int>(z * 100)]];
        _zoomMenu.numberOfVisibleItems = 10;
        _zoomMenu.completes = NO;
        _zoomMenu.target = self;
        _zoomMenu.action = @selector(zoomChosen:);
        [_zoomMenu.widthAnchor constraintEqualToConstant:110].active = YES;
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

- (void)rotateClicked:(NSSegmentedControl*)s {
    const BOOL right = s.selectedSegment == 1;
    if (!(NSEvent.modifierFlags & NSEventModifierFlagShift)) { if (right) [self rotateRight:nil]; else [self rotateLeft:nil]; return; }
    Document* d = self.pdf;   // Shift: every page
    if (!d) return;
    std::vector<int> all(static_cast<size_t>(d->pageCount()));
    for (int i = 0; i < d->pageCount(); ++i) all[static_cast<size_t>(i)] = i;
    if (d->rotatePages(all, right ? 1 : 3)) [self afterChange];
}

- (IBAction)uiRotateToolbar:(id)sender {   // the build check: the toolbar's rotate right pressed, as a click would
    for (NSToolbarItem* item in self.window.toolbar.items)
        if ([item.itemIdentifier isEqualToString:kRotateItem] && [item.view isKindOfClass:[NSSegmentedControl class]]) {
            NSSegmentedControl* s = (NSSegmentedControl*)item.view;
            s.selectedSegment = 1;
            [NSApp sendAction:s.action to:s.target from:s];
        }
    Document* d = self.pdf;
    NSMutableArray* sizes = [NSMutableArray array];
    for (int i = 0; d && i < d->pageCount(); ++i) [sizes addObject:[NSString stringWithFormat:@"%.0fx%.0f", d->pageSize(i).w, d->pageSize(i).h]];
    NSMutableArray* ids = [NSMutableArray array];
    for (NSToolbarItem* item in self.window.toolbar.items) [ids addObject:item.itemIdentifier];
    FPTrace(@"rotate toolbar: items %@; pages %@", [ids componentsJoinedByString:@" "], [sizes componentsJoinedByString:@" "]);
}

- (void)zoomClicked:(NSSegmentedControl*)s {
    if (s.selectedSegment == 0) [self zoomOut:nil]; else [self zoomIn:nil];
}

- (void)zoomChosen:(NSComboBox*)c {
    NSString* t = c.stringValue;
    if ([t isEqualToString:@"Fit width"]) [self fitWidth:nil];
    else if ([t isEqualToString:@"Fit page"]) [self fitPage:nil];
    else {
        const double z = [t stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" %"]].doubleValue;
        if (z >= 10 && z <= 800) [_view setZoom:z / 100.0];
        else NSBeep();
    }
    [self.window makeFirstResponder:_view];
    [self viewChanged:nil];
}

- (void)pageEntered:(NSTextField*)f {
    [self rememberJumpFrom:[_view currentPage]];
    [_view showPage:f.intValue - 1];
    [self.window makeFirstResponder:_view];
}

// The page number, page count, zoom and the window's subtitle follow the view.
- (void)viewChanged:(NSNotification*)n {
    [self updateEditBar];
    [_sidebar setCurrentPage:[_view currentPage]];
    Document* d = _view.document;
    const int count = d ? d->pageCount() : 0;
    const int page = [_view currentPage] + 1;
    if (_pageField && !_pageField.currentEditor) _pageField.stringValue = [NSString stringWithFormat:@"%d", page];
    _pageCount.stringValue = [NSString stringWithFormat:@"of %d", count];
    const int percent = static_cast<int>(std::lround(_view.zoom * 100));
    if (_zoomMenu && !_zoomMenu.currentEditor)
        _zoomMenu.stringValue = _view.fitWidth ? @"Fit width" : [NSString stringWithFormat:@"%d %%", percent];
    if (_status && d && count) {
        const PageSize& ps = d->pageSize(page - 1);
        const int mw = static_cast<int>(std::lround(ps.w / 72.0 * 25.4)), mh = static_cast<int>(std::lround(ps.h / 72.0 * 25.4));
        NSString* name = (mw == 210 && mh == 297) || (mw == 297 && mh == 210) ? @" (A4)" : (mw == 216 && mh == 279) || (mw == 279 && mh == 216) ? @" (Letter)"
                       : (mw == 148 && mh == 210) || (mw == 210 && mh == 148) ? @" (A5)" : (mw == 297 && mh == 420) || (mw == 420 && mh == 297) ? @" (A3)" : @"";
        static NSString* const tools[] = {@"", @"Add comment", @"Rectangle", @"Ellipse", @"Line", @"Arrow", @"Pen", @"Polygon", @"Cloud", @"Callout",
                                          @"Stamp", @"Mark areas to redact", @"Text box", @"Picture", @"Crop by drawing a box", @"Link",
                                          @"Text field", @"Check box", @"Drop-down list", @"Radio buttons", @"List box", @"Date field"};
        NSString* tool = _view.tool > 0 && _view.tool < static_cast<int>(sizeof(tools) / sizeof(tools[0])) ? [NSString stringWithFormat:@"  \u00B7  Tool: %@ (Esc ends it)", tools[_view.tool]] : @"";
        _status.stringValue = [NSString stringWithFormat:@"Page %d of %d  \u00B7  %d \u00D7 %d mm%@%@  \u00B7  %@", page, count, mw, mh, name, tool,
                                                         d->dirty() ? @"Unsaved changes" : @"All changes saved"];
    }
    if (@available(macOS 11.0, *))
        self.window.subtitle = count ? [NSString stringWithFormat:@"Page %d of %d · %d %%", page, count, percent] : @"";
}

- (FPPageView*)pageView { return _view; }
- (Document*)pdf { return _view.document; }

- (void)afterChange {
    _hits.clear();
    _searched.clear();
    [_view setMatches:std::vector<TextHit>() current:-1];
    [_view documentChanged];
    [_sidebar documentChanged];
    [(FPDocument*)self.document changed];
    [self viewChanged:nil];
}

- (void)reloaded:(NSNotification*)n {
    if (n.object != self.document) return;
    _view.document = [(FPDocument*)self.document pdf];
    [_sidebar setDocument:_view.document];
    [self hookDocument];
    [self viewChanged:nil];
}

// ---- settings that change how the window looks ----

- (void)settingsChanged:(NSNotification*)n {
    NSScrollView* sv = _view.enclosingScrollView;
    sv.backgroundColor = FPThemeSurround();
    [_view setNeedsDisplay:YES];
    // colourful toolbar icons, as on Windows: each its own colour; off, the text colour
    NSDictionary<NSToolbarItemIdentifier, NSColor*>* tints = @{@"sidebar" : NSColor.systemBlueColor, @"pages" : NSColor.systemGreenColor,
                                                               @"zoom" : NSColor.systemOrangeColor, @"find" : NSColor.systemPurpleColor,
                                                               @"rotate" : NSColor.systemTealColor};
    for (NSToolbarItem* item in self.window.toolbar.items) {
        NSColor* tint = FPColourIcons() ? tints[item.itemIdentifier] : nil;
        for (NSView* v in [item.view isKindOfClass:[NSStackView class]] ? ((NSStackView*)item.view).views : @[ item.view ?: [NSView new] ])
            if ([v isKindOfClass:[NSButton class]]) ((NSButton*)v).contentTintColor = tint;
            else if ([v isKindOfClass:[NSSegmentedControl class]]) {   // grouped buttons: their symbols drawn in the colour
                NSSegmentedControl* sc = (NSSegmentedControl*)v;
                NSMutableArray<NSImage*>* plain = objc_getAssociatedObject(sc, "plainImages");
                if (!plain) {
                    plain = [NSMutableArray array];
                    for (NSInteger i = 0; i < sc.segmentCount; ++i) [plain addObject:[sc imageForSegment:i] ?: [NSImage new]];
                    objc_setAssociatedObject(sc, "plainImages", plain, OBJC_ASSOCIATION_RETAIN);
                }
                for (NSInteger i = 0; i < sc.segmentCount && i < static_cast<NSInteger>(plain.count); ++i) {
                    NSImage* img = plain[static_cast<NSUInteger>(i)];
                    if (tint) {
                        if (@available(macOS 12.0, *)) {
                            NSImage* coloured = [img imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPaletteColors:@[ tint ]]];
                            if (coloured) { [coloured setTemplate:NO]; img = coloured; }
                        }
                    }
                    [sc setImage:img forSegment:i];
                }
            }
    }
    for (NSWindow* w in self.window.tabbedWindows ?: @[ self.window ])
        [(NSWindowController*)w.windowController synchronizeWindowTitleWithDocumentName];
}

// The tab's title: its number and colour mark when the settings ask for them ("2. Report.pdf").
- (NSString*)windowTitleForDocumentDisplayName:(NSString*)name {
    NSString* t = name;
    if (FPTabNumbers()) {
        NSArray<NSWindow*>* tabs = self.window.tabbedWindows;
        const NSUInteger i = tabs ? [tabs indexOfObject:self.window] : 0;
        t = [NSString stringWithFormat:@"%lu. %@", (unsigned long)(i == NSNotFound ? 1 : i + 1), t];
    }
    if (FPTabColours()) t = [NSString stringWithFormat:@"%@ %@", _tabMark, t];
    return t;
}

// ---- reading positions: each file opens where it was last read (the last 100 files), as on Windows ----

- (void)restorePosition {
    NSString* path = [(NSDocument*)self.document fileURL].path;
    NSDictionary* all = FPRestorePositions() ? [NSUserDefaults.standardUserDefaults dictionaryForKey:@"positions"] : nil;
    NSArray* pos = path ? all[path] : nil;
    if (pos.count < 2) {   // no place remembered: the zoom chosen in Settings > PDF Viewing > Default zoom
        const NSInteger z = [NSUserDefaults.standardUserDefaults integerForKey:@"defaultZoom"];
        static const double zooms[] = {0, 0, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
        if (z <= 0 || z > 7) return;   // fit width, as always
        dispatch_async(dispatch_get_main_queue(), ^{
            if (z == 1) [self->_view fitPage];
            else [self->_view setZoom:zooms[z]];
        });
        return;
    }
    const int page = [pos[0] intValue];
    const double zoom = [pos[1] doubleValue];
    dispatch_async(dispatch_get_main_queue(), ^{   // after the window has its size
        if (zoom > 0) [self->_view setZoom:zoom];
        [self->_view showPage:page];
    });
}

- (void)rememberPosition {
    NSString* path = [(NSDocument*)self.document fileURL].path;
    if (!path || !_view.document) return;
    NSMutableDictionary* all = [[NSUserDefaults.standardUserDefaults dictionaryForKey:@"positions"] mutableCopy] ?: [NSMutableDictionary dictionary];
    NSMutableArray* order = [[NSUserDefaults.standardUserDefaults arrayForKey:@"positionsOrder"] mutableCopy] ?: [NSMutableArray array];
    all[path] = @[ @([_view currentPage]), @(_view.fitWidth ? 0.0 : _view.zoom) ];
    [order removeObject:path];
    [order addObject:path];
    while (order.count > 100) { [all removeObjectForKey:order.firstObject]; [order removeObjectAtIndex:0]; }
    [NSUserDefaults.standardUserDefaults setObject:all forKey:@"positions"];
    [NSUserDefaults.standardUserDefaults setObject:order forKey:@"positionsOrder"];
}

- (void)windowWillClose:(NSNotification*)n { [self rememberPosition]; }

// Form fields: a field that changes how it looks is drawn again; a committed answer marks the document edited.
- (void)hookDocument {
    Document* d = _view.document;
    if (!d) return;
    __weak FPWindowController* weak = self;
    d->onFormInvalidate = [weak](int page) { [weak.pageView invalidatePage:page]; };
    d->onFormChanged = [weak]() {
        [(FPDocument*)weak.document changed];
        if ([weak respondsToSelector:@selector(formCommitted)]) [weak performSelector:@selector(formCommitted)];   // FormCalc.mm
    };
    d->onFormStart = [weak]() { if ([weak respondsToSelector:@selector(formStarting)]) [weak performSelector:@selector(formStarting)]; };
}

// ---- find: every match tinted, Return / ⌘G for the next, ⇧⌘G for the previous ----

// The search options, in the magnifying glass's menu: match case, whole words, wildcards, comments too.
static NSString* const kFindOptions[] = {@"findMatchCase", @"findWholeWord", @"findWildcards", @"findComments"};

- (void)buildFindMenu {
    NSMenu* m = [[NSMenu alloc] initWithTitle:@"Find options"];
    NSArray* titles = @[ @"Match Case", @"Whole Words Only", @"Wildcards (* any letters, ? one letter)", @"Search in Comments Too" ];
    for (NSUInteger i = 0; i < titles.count; ++i) {
        NSMenuItem* it = [m addItemWithTitle:titles[i] action:@selector(toggleFindOption:) keyEquivalent:@""];
        it.target = self;
        it.tag = static_cast<NSInteger>(i);
        it.state = [NSUserDefaults.standardUserDefaults boolForKey:kFindOptions[i]] ? NSControlStateValueOn : NSControlStateValueOff;
    }
    _find.searchMenuTemplate = m;
}

- (IBAction)toggleFindOption:(NSMenuItem*)sender {
    NSString* key = kFindOptions[std::clamp<NSInteger>(sender.tag, 0, 3)];
    [NSUserDefaults.standardUserDefaults setBool:![NSUserDefaults.standardUserDefaults boolForKey:key] forKey:key];
    [self buildFindMenu];
    _searched.clear();   // the same words again, with the new options
    if (_find.stringValue.length) [self findEntered:_find];
}

- (search::Options)findOptions {
    NSUserDefaults* u = NSUserDefaults.standardUserDefaults;
    search::Options o;
    o.matchCase = [u boolForKey:kFindOptions[0]];
    o.wholeWord = [u boolForKey:kFindOptions[1]];
    o.wildcards = [u boolForKey:kFindOptions[2]];
    o.comments = [u boolForKey:kFindOptions[3]];
    return o;
}

- (void)findEntered:(NSSearchField*)f {
    Document* d = self.pdf;
    if (!d) return;
    const std::wstring q = platform::wide(f.stringValue.UTF8String ?: "");
    if (q.empty()) { _hits.clear(); _searched.clear(); [_view setMatches:std::vector<TextHit>() current:-1]; [self viewChanged:nil]; return; }
    if (q != _searched) {   // a new search: the first match from the page shown
        _searched = q;
        _hits = search::find(*d, q, [self findOptions]);
        FPTrace(@"find %@: %d", f.stringValue, static_cast<int>(_hits.size()));
        _hit = -1;
        const int page = [_view currentPage];
        for (size_t i = 0; i < _hits.size(); ++i) if (_hits[i].page >= page) { _hit = static_cast<int>(i) - 1; break; }
    }
    if (_hits.empty()) { NSBeep(); [_view setMatches:std::vector<TextHit>() current:-1]; [self viewChanged:nil]; return; }
    const bool back = (NSApp.currentEvent.modifierFlags & NSEventModifierFlagShift) != 0;
    _hit = (back ? _hit - 1 + static_cast<int>(_hits.size()) : _hit + 1) % static_cast<int>(_hits.size());
    [_view setMatches:_hits current:_hit];
    [self viewChanged:nil];
}

// The build check: FEATHER_UI_FIND="words|options" (options: case word wild comments) searched as if typed.
- (IBAction)uiFind:(id)sender {
    NSString* spec = NSProcessInfo.processInfo.environment[@"FEATHER_UI_FIND"] ?: @"";
    NSArray* parts = [spec componentsSeparatedByString:@"|"];
    NSString* opts = parts.count > 1 ? parts[1] : @"";
    NSUserDefaults* u = NSUserDefaults.standardUserDefaults;
    NSArray* names = @[ @"case", @"word", @"wild", @"comments" ];
    for (NSUInteger i = 0; i < names.count; ++i) [u setBool:[opts containsString:names[i]] forKey:kFindOptions[i]];
    [self buildFindMenu];
    _find.stringValue = parts.firstObject ?: @"";
    _searched.clear();
    [self findEntered:_find];
    for (int i = 0; i < 3 && !_hits.empty(); ++i) [self findEntered:_find];   // on to the fourth match
    if (_hit >= 0 && _hit < static_cast<int>(_hits.size())) FPTrace(@"find: at %d in a comment %d", _hit + 1, _hits[static_cast<size_t>(_hit)].annot >= 0);
}

- (IBAction)performFindPanelAction:(id)sender { [self.window makeFirstResponder:_find]; }
- (IBAction)findNextMatch:(id)sender { if (_find.stringValue.length) [self findEntered:_find]; else [self performFindPanelAction:nil]; }
- (IBAction)findPreviousMatch:(id)sender {
    if (_hits.empty()) return;
    _hit = (_hit - 1 + static_cast<int>(_hits.size())) % static_cast<int>(_hits.size());
    [_view setMatches:_hits current:_hit];
    [self viewChanged:nil];
}

// ---- back and forward (links, Go to page, thumbnails, bookmarks) ----

- (void)rememberJumpFrom:(int)page {
    if (_back.empty() || _back.back() != page) _back.push_back(page);
    if (_back.size() > 50) _back.erase(_back.begin());
    _forward.clear();
}

- (IBAction)goBack:(id)sender {
    if (_back.empty()) return;
    _forward.push_back([_view currentPage]);
    const int p = _back.back();
    _back.pop_back();
    [_view showPage:p];
}

- (IBAction)goForward:(id)sender {
    if (_forward.empty()) return;
    _back.push_back([_view currentPage]);
    const int p = _forward.back();
    _forward.pop_back();
    [_view showPage:p];
}

// ---- page layout (View menu), as on Windows ----

- (IBAction)layoutSingle:(id)sender { _view.layout = 0; }
- (IBAction)layoutContinuous:(id)sender { _view.layout = 1; }
- (IBAction)layoutTwoPages:(id)sender { _view.layout = 2; }
- (IBAction)toggleCover:(id)sender { _view.cover = !_view.cover; }
- (IBAction)fitPage:(id)sender { [_view fitPage]; }

// ---- the sidebar ----

- (void)sidebarGoToPage:(int)page {
    [self rememberJumpFrom:[_view currentPage]];
    [_view showPage:page];
}

- (BOOL)sidebarShown { return !_sidebar.hidden; }

- (NSArray<NSNumber*>*)selectedThumbPages {
    NSArray<NSNumber*>* pages = _sidebar.hidden ? @[] : [_sidebar selectedPages];
    return pages.count ? pages : @[ @([_view currentPage]) ];
}

- (IBAction)selectAllPages:(id)sender { [self selectAllThumbs]; }   // Pages > Select All Pages

- (void)setBesideView:(NSView*)other stacked:(BOOL)stacked swapped:(BOOL)swapped {
    NSScrollView* main = _view.enclosingScrollView;
    NSView* holder = main.superview;
    if (holder != _split) {   // a pair already: back to the pages alone first
        [main removeFromSuperview];
        [_split replaceSubview:holder with:main];
        for (NSView* v in [holder.subviews copy]) [v removeFromSuperview];
    }
    if (other) {
        NSSplitView* pair = [[NSSplitView alloc] initWithFrame:main.frame];
        pair.vertical = !stacked;   // dividers upright: left and right
        pair.dividerStyle = NSSplitViewDividerStyleThin;
        [_split replaceSubview:main with:pair];
        [pair addSubview:swapped ? other : main];
        [pair addSubview:swapped ? main : other];
        [pair adjustSubviews];
        [pair setPosition:(stacked ? pair.bounds.size.height : pair.bounds.size.width) / 2 ofDividerAtIndex:0];
    }
    [_split adjustSubviews];
    _view.fitWidth = YES;
}

- (void)selectAllThumbs {
    if (_sidebar.hidden) [self toggleThumbnails:nil];
    [_sidebar showBookmarks:NO];
    [_sidebar selectAllPages];
    FPTrace(@"thumbnails selected: %lu", (unsigned long)[_sidebar selectedPages].count);
}

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
    _view.enclosingScrollView.backgroundColor = FPThemeSurround();   // black around Blackboard pages
    [_view setNeedsDisplay:YES];
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
    // ⌘⌫ in a text box (Find, the page number, a form field being typed in) deletes text there, not pages
    if (item.action == NSSelectorFromString(@"deleteSelectedPages:"))
        return _view.document && ![self.window.firstResponder isKindOfClass:[NSText class]];
    SEL sigs = NSSelectorFromString(@"validateSignatures:");   // Signatures.mm
    static NSSet<NSString*>* signing = [NSSet setWithArray:@[ @"signDocument:", @"showSignatures:", @"signatureTyped:", @"signatureDrawn:",
                                                              @"signaturePicture:", @"signatureSaved:" ]];
    if ([signing containsObject:NSStringFromSelector(item.action)] && [self respondsToSelector:sigs])
        return ((BOOL (*)(id, SEL, NSMenuItem*))objc_msgSend)(self, sigs, item);
    SEL measure = NSSelectorFromString(@"validateMeasure:");   // Measure.mm
    static NSSet<NSString*>* measuring = [NSSet setWithArray:@[ @"chooseRuler:", @"chooseProtractor:", @"toggleRulers:", @"setMeasureUnits:",
                                                                @"setMeasureScale:", @"keepMeasurements:", @"clearMeasurements:" ]];
    if ([measuring containsObject:NSStringFromSelector(item.action)] && [self respondsToSelector:measure])
        return ((BOOL (*)(id, SEL, NSMenuItem*))objc_msgSend)(self, measure, item);
    SEL sbs = NSSelectorFromString(@"validateSideBySide:");   // SideBySide.mm
    if ([NSStringFromSelector(item.action) rangeOfString:@"ideBySide"].location != NSNotFound && [self respondsToSelector:sbs])
        return ((BOOL (*)(id, SEL, NSMenuItem*))objc_msgSend)(self, sbs, item);
    if (item.action == @selector(toggleThumbnails:)) item.title = self.sidebarShown ? @"Hide Sidebar" : @"Show Sidebar";
    if (item.action == @selector(goBack:)) return !_back.empty();
    if (item.action == @selector(chooseTool:)) item.state = _view.tool == item.tag ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(selectTextTool:)) item.state = _view.tool == FPToolNone ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(setDrawingColour:) || item.action == @selector(setHighlightColour:) || item.action == @selector(setLineWidth:)) {
        NSString* key = item.action == @selector(setDrawingColour:) ? @"drawingColour" : item.action == @selector(setHighlightColour:) ? @"highlightColour" : @"lineWidth";
        NSNumber* v = [NSUserDefaults.standardUserDefaults objectForKey:key];
        const NSInteger cur = v ? v.integerValue : (item.action == @selector(setDrawingColour:) ? 5 : item.action == @selector(setLineWidth:) ? 1 : 0);
        item.state = cur == item.tag ? NSControlStateValueOn : NSControlStateValueOff;
    }
    if (item.action == @selector(deleteSelectedAnnotation:) || item.action == @selector(editSelectedComment:)) return _view.selectedAnnot >= 0;
    if (item.action == @selector(goForward:)) return !_forward.empty();
    if (item.action == @selector(findPreviousMatch:)) return !_hits.empty();
    if (item.action == @selector(layoutSingle:)) item.state = _view.layout == 0;
    if (item.action == @selector(layoutContinuous:)) item.state = _view.layout == 1;
    if (item.action == @selector(layoutTwoPages:)) item.state = _view.layout == 2;
    if (item.action == @selector(toggleCover:)) { item.state = _view.cover; return _view.layout == 2; }
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
