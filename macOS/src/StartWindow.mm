#import "StartWindow.h"

// ---- the drop area with the Open PDF button ----

@interface FPDropArea : NSView
@property(nonatomic) BOOL hot;   // a PDF is being dragged over it
@end

@implementation FPDropArea

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) [self registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
    return self;
}

- (void)drawRect:(NSRect)dirty {
    NSBezierPath* p = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 1.5, 1.5) xRadius:12 yRadius:12];
    [(self.hot ? [NSColor.controlAccentColor colorWithAlphaComponent:0.12] : [NSColor.labelColor colorWithAlphaComponent:0.035]) setFill];
    [p fill];
    const CGFloat dash[] = {7, 5};
    [p setLineDash:dash count:2 phase:0];
    p.lineWidth = 1.5;
    [(self.hot ? NSColor.controlAccentColor : NSColor.tertiaryLabelColor) setStroke];
    [p stroke];
}

- (NSArray<NSURL*>*)urls:(id<NSDraggingInfo>)info {
    NSArray<NSURL*>* all = [info.draggingPasteboard readObjectsForClasses:@[ [NSURL class] ]
                                                                  options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
    NSMutableArray<NSURL*>* pdfs = [NSMutableArray array];
    for (NSURL* u in all) if ([u.pathExtension.lowercaseString isEqualToString:@"pdf"]) [pdfs addObject:u];
    return pdfs;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
    self.hot = [self urls:info].count > 0;
    [self setNeedsDisplay:YES];
    return self.hot ? NSDragOperationCopy : NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
    self.hot = NO;
    [self setNeedsDisplay:YES];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
    self.hot = NO;
    [self setNeedsDisplay:YES];
    NSArray<NSURL*>* urls = [self urls:info];
    for (NSURL* u in urls)
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:u display:YES
                                                                     completionHandler:^(NSDocument*, BOOL, NSError*) {}];
    return urls.count > 0;
}

- (void)mouseUp:(NSEvent*)e {   // a click anywhere in the area opens a file, as on Windows
    [[NSDocumentController sharedDocumentController] openDocument:nil];
}

@end

// ---- the window ----

@interface FPStartWindowController () <NSTableViewDataSource, NSTableViewDelegate, NSMenuDelegate, NSWindowDelegate>
@end

@implementation FPStartWindowController {
    NSTableView* _table;
    NSTextField* _empty;
    NSButton* _clear;
    NSArray<NSURL*>* _recent;
}

+ (instancetype)shared {
    static FPStartWindowController* s = nil;
    if (!s) s = [[FPStartWindowController alloc] init];
    return s;
}

static NSTextField* label(NSString* text, NSFont* font, NSColor* colour) {
    NSTextField* l = [NSTextField labelWithString:text];
    l.font = font;
    l.textColor = colour;
    l.translatesAutoresizingMaskIntoConstraints = NO;
    return l;
}

- (instancetype)init {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 760, 600)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable |
                                                        NSWindowStyleMaskFullSizeContentView
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    if (!(self = [super initWithWindow:w])) return nil;
    w.title = @"Feather PDF";
    w.delegate = self;
    w.titlebarAppearsTransparent = YES;
    w.minSize = NSMakeSize(520, 460);
    w.tabbingMode = NSWindowTabbingModeDisallowed;
    [w setFrameAutosaveName:@"FeatherPDFStart"];
    if (NSEqualPoints(w.frame.origin, NSZeroPoint)) [w center];

    NSView* root = w.contentView;
    NSTextField* title = label(@"Feather PDF", [NSFont systemFontOfSize:30 weight:NSFontWeightSemibold], NSColor.labelColor);
    NSTextField* sub = label(@"Read, edit, sign and print PDF documents — free, on your own Mac.", [NSFont systemFontOfSize:14],
                             NSColor.secondaryLabelColor);

    FPDropArea* drop = [[FPDropArea alloc] initWithFrame:NSZeroRect];
    drop.translatesAutoresizingMaskIntoConstraints = NO;
    NSTextField* dropText = label(@"Drag and drop PDF files here", [NSFont systemFontOfSize:15], NSColor.labelColor);
    NSButton* open = [NSButton buttonWithTitle:@"Open PDF…" target:[NSDocumentController sharedDocumentController]
                                        action:@selector(openDocument:)];
    open.bezelStyle = NSBezelStyleRounded;
    open.keyEquivalent = @"\r";   // the default button: blue, and Return presses it
    open.controlSize = NSControlSizeLarge;
    open.translatesAutoresizingMaskIntoConstraints = NO;
    if (@available(macOS 11.0, *)) open.image = [NSImage imageWithSystemSymbolName:@"folder" accessibilityDescription:nil];
    open.imagePosition = NSImageLeading;
    NSTextField* hint = label(@"⌘O opens a file", [NSFont systemFontOfSize:12], NSColor.tertiaryLabelColor);
    for (NSView* v in @[ dropText, open, hint ]) [drop addSubview:v];

    NSTextField* heading = label(@"Recent files", [NSFont systemFontOfSize:15 weight:NSFontWeightSemibold], NSColor.labelColor);
    _clear = [NSButton buttonWithTitle:@"Clear list" target:self action:@selector(clearList:)];
    _clear.bordered = NO;
    _clear.contentTintColor = NSColor.secondaryLabelColor;
    _clear.translatesAutoresizingMaskIntoConstraints = NO;

    _table = [[NSTableView alloc] initWithFrame:NSZeroRect];
    NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"file"];
    col.resizingMask = NSTableColumnAutoresizingMask;
    [_table addTableColumn:col];
    _table.headerView = nil;
    _table.rowHeight = 40;
    _table.dataSource = self;
    _table.delegate = self;
    _table.target = self;
    _table.action = @selector(openClicked:);
    _table.backgroundColor = NSColor.clearColor;
    _table.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
    if (@available(macOS 11.0, *)) _table.style = NSTableViewStyleInset;
    NSMenu* menu = [[NSMenu alloc] init];
    menu.delegate = self;
    _table.menu = menu;
    NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scroll.documentView = _table;
    scroll.hasVerticalScroller = YES;
    scroll.drawsBackground = NO;
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    _empty = label(@"The files you open will be listed here.", [NSFont systemFontOfSize:13], NSColor.secondaryLabelColor);

    for (NSView* v in @[ title, sub, drop, heading, _clear, scroll, _empty ]) [root addSubview:v];
    NSLayoutGuide* g = [[NSLayoutGuide alloc] init];   // the content column: at most 640 wide, centred
    [root addLayoutGuide:g];
    [NSLayoutConstraint activateConstraints:@[
        [g.centerXAnchor constraintEqualToAnchor:root.centerXAnchor],
        [g.widthAnchor constraintLessThanOrEqualToConstant:640],
        [g.leadingAnchor constraintGreaterThanOrEqualToAnchor:root.leadingAnchor constant:28],
        [g.topAnchor constraintEqualToAnchor:root.topAnchor constant:56],
        [g.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-24],

        [title.topAnchor constraintEqualToAnchor:g.topAnchor],
        [title.centerXAnchor constraintEqualToAnchor:g.centerXAnchor],
        [sub.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:6],
        [sub.centerXAnchor constraintEqualToAnchor:g.centerXAnchor],
        [sub.widthAnchor constraintLessThanOrEqualToAnchor:g.widthAnchor],

        [drop.topAnchor constraintEqualToAnchor:sub.bottomAnchor constant:24],
        [drop.leadingAnchor constraintEqualToAnchor:g.leadingAnchor],
        [drop.trailingAnchor constraintEqualToAnchor:g.trailingAnchor],
        [drop.heightAnchor constraintEqualToConstant:170],
        [dropText.centerXAnchor constraintEqualToAnchor:drop.centerXAnchor],
        [dropText.topAnchor constraintEqualToAnchor:drop.topAnchor constant:34],
        [open.centerXAnchor constraintEqualToAnchor:drop.centerXAnchor],
        [open.topAnchor constraintEqualToAnchor:dropText.bottomAnchor constant:16],
        [hint.centerXAnchor constraintEqualToAnchor:drop.centerXAnchor],
        [hint.topAnchor constraintEqualToAnchor:open.bottomAnchor constant:14],

        [heading.topAnchor constraintEqualToAnchor:drop.bottomAnchor constant:26],
        [heading.leadingAnchor constraintEqualToAnchor:g.leadingAnchor constant:4],
        [_clear.centerYAnchor constraintEqualToAnchor:heading.centerYAnchor],
        [_clear.trailingAnchor constraintEqualToAnchor:g.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:8],
        [scroll.leadingAnchor constraintEqualToAnchor:g.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:g.trailingAnchor],
        [scroll.bottomAnchor constraintEqualToAnchor:g.bottomAnchor],
        [_empty.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:12],
        [_empty.leadingAnchor constraintEqualToAnchor:heading.leadingAnchor],
    ]];
    NSLayoutConstraint* wide = [g.widthAnchor constraintEqualToConstant:640];
    wide.priority = NSLayoutPriorityDefaultHigh;   // as wide as allowed, narrower in a small window
    wide.active = YES;

    // Hidden while a document is open; back when the last document window closes.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(somethingClosed:)
                                                 name:NSWindowWillCloseNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowShown:)
                                                 name:NSWindowDidBecomeMainNotification object:nil];
    [self refresh];
    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

static BOOL anyDocumentWindow(NSWindow* except) {
    for (NSDocument* d in [NSDocumentController sharedDocumentController].documents)
        for (NSWindowController* wc in d.windowControllers)
            if (wc.window != except && wc.window.isVisible) return YES;
    return NO;
}

- (void)showIfNoDocuments {
    if (anyDocumentWindow(nil)) return;
    [self refresh];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)somethingClosed:(NSNotification*)n {
    NSWindow* closing = n.object;
    if (closing == self.window || !closing.windowController.document) return;
    dispatch_async(dispatch_get_main_queue(), ^{   // after the window has gone
        if (!anyDocumentWindow(closing)) [self showIfNoDocuments];
    });
}

- (void)windowShown:(NSNotification*)n {
    NSWindow* w = n.object;
    if (w != self.window && w.windowController.document && self.window.isVisible) [self.window orderOut:nil];
}

// ---- recent files ----

- (void)refresh {
    _recent = [NSDocumentController sharedDocumentController].recentDocumentURLs;
    [_table reloadData];
    _empty.hidden = _recent.count > 0;
    _clear.hidden = _recent.count == 0;
    _table.enclosingScrollView.hidden = _recent.count == 0;
}

- (void)windowDidBecomeKey:(NSNotification*)n { [self refresh]; }

- (NSInteger)numberOfRowsInTableView:(NSTableView*)t { return static_cast<NSInteger>(_recent.count); }

- (NSView*)tableView:(NSTableView*)t viewForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    NSTableCellView* cell = [t makeViewWithIdentifier:@"recent" owner:self];
    if (!cell) {
        cell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 400, 40)];
        cell.identifier = @"recent";
        NSImageView* icon = [NSImageView imageViewWithImage:[NSImage imageNamed:NSImageNameMultipleDocuments]];
        icon.translatesAutoresizingMaskIntoConstraints = NO;
        NSTextField* name = label(@"", [NSFont systemFontOfSize:13], NSColor.labelColor);
        NSTextField* folder = label(@"", [NSFont systemFontOfSize:11], NSColor.secondaryLabelColor);
        name.lineBreakMode = folder.lineBreakMode = NSLineBreakByTruncatingMiddle;
        name.tag = 1;
        folder.tag = 2;
        for (NSView* v in @[ icon, name, folder ]) [cell addSubview:v];
        cell.imageView = icon;
        [NSLayoutConstraint activateConstraints:@[
            [icon.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:6],
            [icon.centerYAnchor constraintEqualToAnchor:cell.centerYAnchor],
            [icon.widthAnchor constraintEqualToConstant:28], [icon.heightAnchor constraintEqualToConstant:28],
            [name.leadingAnchor constraintEqualToAnchor:icon.trailingAnchor constant:10],
            [name.trailingAnchor constraintLessThanOrEqualToAnchor:cell.trailingAnchor constant:-8],
            [name.bottomAnchor constraintEqualToAnchor:cell.centerYAnchor constant:1],
            [folder.leadingAnchor constraintEqualToAnchor:name.leadingAnchor],
            [folder.trailingAnchor constraintLessThanOrEqualToAnchor:cell.trailingAnchor constant:-8],
            [folder.topAnchor constraintEqualToAnchor:cell.centerYAnchor constant:2],
        ]];
    }
    NSURL* u = _recent[static_cast<NSUInteger>(row)];
    const BOOL gone = ![[NSFileManager defaultManager] fileExistsAtPath:u.path];
    NSTextField* name = [cell viewWithTag:1];
    NSTextField* folder = [cell viewWithTag:2];
    name.stringValue = gone ? [u.lastPathComponent stringByAppendingString:@"  (not found)"] : u.lastPathComponent;
    name.textColor = gone ? NSColor.tertiaryLabelColor : NSColor.labelColor;
    folder.stringValue = [u.URLByDeletingLastPathComponent.path stringByAbbreviatingWithTildeInPath];
    cell.imageView.image = [[NSWorkspace sharedWorkspace] iconForFile:gone ? @"/System" : u.path];
    return cell;
}

- (void)openRow:(NSInteger)row {
    if (row < 0 || row >= static_cast<NSInteger>(_recent.count)) return;
    NSURL* u = _recent[static_cast<NSUInteger>(row)];
    [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:u display:YES
                                                                 completionHandler:^(NSDocument*, BOOL, NSError* e) {
                                                                     if (e) [NSApp presentError:e];
                                                                 }];
}

- (void)openClicked:(id)sender { [self openRow:_table.clickedRow]; }

- (void)clearList:(id)sender {
    [[NSDocumentController sharedDocumentController] clearRecentDocuments:nil];
    [self refresh];
}

// Right-click: Open, Remove from the list (macOS has no call to remove one file, so the list is written again without it).
- (void)menuNeedsUpdate:(NSMenu*)menu {
    [menu removeAllItems];
    if (_table.clickedRow < 0) return;
    [menu addItemWithTitle:@"Open" action:@selector(openFromMenu:) keyEquivalent:@""].target = self;
    [menu addItemWithTitle:@"Remove from the List" action:@selector(removeFromMenu:) keyEquivalent:@""].target = self;
}

- (void)openFromMenu:(id)sender { [self openRow:_table.clickedRow]; }

- (void)removeFromMenu:(id)sender {
    const NSInteger row = _table.clickedRow;
    if (row < 0 || row >= static_cast<NSInteger>(_recent.count)) return;
    NSMutableArray<NSURL*>* keep = [_recent mutableCopy];
    [keep removeObjectAtIndex:static_cast<NSUInteger>(row)];
    NSDocumentController* dc = [NSDocumentController sharedDocumentController];
    [dc clearRecentDocuments:nil];
    for (NSURL* u in keep.reverseObjectEnumerator) [dc noteNewRecentDocumentURL:u];   // oldest first, so the order stays
    [self refresh];
}

@end
