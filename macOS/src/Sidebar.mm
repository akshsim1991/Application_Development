#import "Sidebar.h"
#import "Render.h"
#include <algorithm>
#include <cmath>
#include "platform.hpp"

namespace {
constexpr CGFloat kThumbWidth = 128;   // points
NSString* nsText(const std::wstring& s) { return [NSString stringWithUTF8String:platform::utf8(s).c_str()] ?: @""; }
}

// One bookmark as the outline shows it.
@interface FPBookmarkItem : NSObject
@property(nonatomic, copy) NSString* title;
@property(nonatomic) int page;
@property(nonatomic, strong) NSArray<FPBookmarkItem*>* children;
@end
@implementation FPBookmarkItem
@end

static NSArray<FPBookmarkItem*>* itemsOf(const std::vector<Bookmark>& marks) {
    NSMutableArray<FPBookmarkItem*>* out = [NSMutableArray array];
    for (const Bookmark& b : marks) {
        FPBookmarkItem* i = [[FPBookmarkItem alloc] init];
        i.title = nsText(b.title);
        i.page = b.page;
        i.children = itemsOf(b.children);
        [out addObject:i];
    }
    return out;
}

static NSPasteboardType const kPagesType = @"com.featherpdf.pages";

// The thumbnail list: the Delete key deletes the selected pages.
@interface FPThumbTable : NSTableView
@property(nonatomic, copy) void (^onDelete)(void);
@end
@implementation FPThumbTable
- (void)keyDown:(NSEvent*)e {
    const unichar c = e.charactersIgnoringModifiers.length ? [e.charactersIgnoringModifiers characterAtIndex:0] : 0;
    if ((c == NSDeleteCharacter || c == NSDeleteFunctionKey) && self.onDelete) { self.onDelete(); return; }
    [super keyDown:e];
}
@end

@interface FPSidebar () <NSTableViewDataSource, NSTableViewDelegate, NSOutlineViewDataSource, NSOutlineViewDelegate, NSMenuDelegate>
@end

@implementation FPSidebar {
    Document* _doc;
    NSSegmentedControl* _switch;
    NSTableView* _thumbs;
    NSOutlineView* _marks;
    NSScrollView* _thumbScroll;
    NSScrollView* _markScroll;
    NSTextField* _noMarks;
    NSMutableDictionary<NSNumber*, NSImage*>* _images;
    NSArray<FPBookmarkItem*>* _bookmarks;
    BOOL _following;   // selecting the thumbnail of the page shown: not a click
}

- (instancetype)initWithFrame:(NSRect)frame {
    if (!(self = [super initWithFrame:frame])) return nil;
    _images = [NSMutableDictionary dictionary];
    _colours = pagecolours::Mode::Normal;

    _switch = [NSSegmentedControl segmentedControlWithLabels:@[ @"Pages", @"Bookmarks" ] trackingMode:NSSegmentSwitchTrackingSelectOne
                                                      target:self action:@selector(switched:)];
    _switch.selectedSegment = 0;
    _switch.translatesAutoresizingMaskIntoConstraints = NO;

    FPThumbTable* thumbs = [[FPThumbTable alloc] initWithFrame:NSZeroRect];
    __weak FPSidebar* weak = self;
    thumbs.onDelete = ^{
        FPSidebar* me = weak;
        NSArray<NSNumber*>* pages = [me selectedPages];
        if (pages.count && [me.delegate respondsToSelector:@selector(sidebarDeletePages:)]) [me.delegate sidebarDeletePages:pages];
    };
    _thumbs = thumbs;
    _thumbs.allowsMultipleSelection = YES;
    [_thumbs registerForDraggedTypes:@[ kPagesType ]];
    [_thumbs setDraggingSourceOperationMask:NSDragOperationMove forLocal:YES];
    _thumbs.draggingDestinationFeedbackStyle = NSTableViewDraggingDestinationFeedbackStyleGap;
    _thumbs.menu = [[NSMenu alloc] initWithTitle:@"Pages"];
    _thumbs.menu.delegate = self;
    NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"page"];
    [_thumbs addTableColumn:col];
    _thumbs.headerView = nil;
    _thumbs.dataSource = self;
    _thumbs.delegate = self;
    _thumbs.intercellSpacing = NSMakeSize(0, 6);
    _thumbs.backgroundColor = NSColor.clearColor;
    _thumbs.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
    if (@available(macOS 11.0, *)) _thumbs.style = NSTableViewStyleSourceList;
    _thumbScroll = [self scrollFor:_thumbs];

    _marks = [[NSOutlineView alloc] initWithFrame:NSZeroRect];
    NSTableColumn* mc = [[NSTableColumn alloc] initWithIdentifier:@"mark"];
    [_marks addTableColumn:mc];
    _marks.outlineTableColumn = mc;
    _marks.headerView = nil;
    _marks.dataSource = self;
    _marks.delegate = self;
    _marks.target = self;
    _marks.action = @selector(bookmarkClicked:);
    _marks.backgroundColor = NSColor.clearColor;
    _marks.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
    if (@available(macOS 11.0, *)) _marks.style = NSTableViewStyleSourceList;
    _markScroll = [self scrollFor:_marks];
    _markScroll.hidden = YES;
    _noMarks = [NSTextField wrappingLabelWithString:@"This document has no bookmarks."];
    _noMarks.textColor = NSColor.secondaryLabelColor;
    _noMarks.translatesAutoresizingMaskIntoConstraints = NO;
    _noMarks.hidden = YES;

    for (NSView* v in @[ _switch, _thumbScroll, _markScroll, _noMarks ]) [self addSubview:v];
    [NSLayoutConstraint activateConstraints:@[
        [_switch.topAnchor constraintEqualToAnchor:self.topAnchor constant:8],
        [_switch.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
        [_thumbScroll.topAnchor constraintEqualToAnchor:_switch.bottomAnchor constant:8],
        [_thumbScroll.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_thumbScroll.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_thumbScroll.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [_markScroll.topAnchor constraintEqualToAnchor:_thumbScroll.topAnchor],
        [_markScroll.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_markScroll.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_markScroll.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [_noMarks.topAnchor constraintEqualToAnchor:_thumbScroll.topAnchor constant:12],
        [_noMarks.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:14],
        [_noMarks.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-14],
    ]];
    return self;
}

- (NSScrollView*)scrollFor:(NSView*)v {
    NSScrollView* s = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    s.documentView = v;
    s.hasVerticalScroller = YES;
    s.autohidesScrollers = YES;
    s.drawsBackground = NO;
    s.translatesAutoresizingMaskIntoConstraints = NO;
    return s;
}

- (void)setDocument:(Document*)doc {
    _doc = doc;
    [self documentChanged];
}

- (void)documentChanged {
    [_images removeAllObjects];
    _bookmarks = _doc ? itemsOf(_doc->bookmarks()) : @[];
    [_thumbs reloadData];
    [_marks reloadData];
    [_marks expandItem:nil expandChildren:NO];
    [self updateEmpty];
}

- (void)setColours:(pagecolours::Mode)colours {
    if (colours == _colours) return;
    _colours = colours;
    [_images removeAllObjects];
    [_thumbs reloadData];
}

- (void)switched:(id)sender { [self showBookmarks:_switch.selectedSegment == 1]; }

- (void)showBookmarks:(BOOL)bookmarks {
    _switch.selectedSegment = bookmarks ? 1 : 0;
    _thumbScroll.hidden = bookmarks;
    _markScroll.hidden = !bookmarks;
    [self updateEmpty];
}

- (void)updateEmpty { _noMarks.hidden = _markScroll.hidden || _bookmarks.count > 0; }

- (void)setCurrentPage:(int)page {
    if (page < 0 || page >= _thumbs.numberOfRows || _thumbs.selectedRow == page) return;
    // several pages selected (for the page menu, dragging, ⌘⌫): reading on through them keeps the selection
    if (_thumbs.selectedRowIndexes.count > 1 && [_thumbs.selectedRowIndexes containsIndex:static_cast<NSUInteger>(page)]) return;
    _following = YES;
    [_thumbs selectRowIndexes:[NSIndexSet indexSetWithIndex:static_cast<NSUInteger>(page)] byExtendingSelection:NO];
    [_thumbs scrollRowToVisible:page];
    _following = NO;
}

// ---- thumbnails ----

- (NSInteger)numberOfRowsInTableView:(NSTableView*)t { return t == _thumbs && _doc ? _doc->pageCount() : 0; }

- (CGFloat)thumbHeight:(int)page {
    const PageSize& s = _doc->pageSize(page);
    return std::round(kThumbWidth * s.h / std::max(1.0f, s.w));
}

- (CGFloat)tableView:(NSTableView*)t heightOfRow:(NSInteger)row {
    return _doc ? [self thumbHeight:static_cast<int>(row)] + 26 : 40;
}

- (NSImage*)imageOf:(int)page {
    if (NSImage* i = _images[@(page)]) return i;
    const CGFloat scale = self.window ? self.window.backingScaleFactor : 2.0;
    const int w = static_cast<int>(std::lround(kThumbWidth * scale)), h = static_cast<int>(std::lround([self thumbHeight:page] * scale));
    CGImageRef img = FPRenderPage(*_doc, page, w, h, 0, 0, w, h, _colours);
    if (!img) return nil;
    NSImage* ns = [[NSImage alloc] initWithCGImage:img size:NSMakeSize(kThumbWidth, [self thumbHeight:page])];
    CGImageRelease(img);
    if (_images.count > 200) [_images removeAllObjects];   // a long document: keep memory in check
    _images[@(page)] = ns;
    return ns;
}

- (NSView*)tableView:(NSTableView*)t viewForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    NSTableCellView* cell = [t makeViewWithIdentifier:@"thumb" owner:self];
    if (!cell) {
        cell = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
        cell.identifier = @"thumb";
        NSImageView* iv = [[NSImageView alloc] initWithFrame:NSZeroRect];
        iv.imageScaling = NSImageScaleProportionallyUpOrDown;
        iv.wantsLayer = YES;
        iv.layer.borderColor = [NSColor colorWithWhite:0 alpha:0.25].CGColor;
        iv.layer.borderWidth = 1;
        iv.translatesAutoresizingMaskIntoConstraints = NO;
        NSTextField* n = [NSTextField labelWithString:@""];
        n.font = [NSFont systemFontOfSize:11];
        n.textColor = NSColor.secondaryLabelColor;
        n.translatesAutoresizingMaskIntoConstraints = NO;
        [cell addSubview:iv];
        [cell addSubview:n];
        cell.imageView = iv;
        cell.textField = n;
        [NSLayoutConstraint activateConstraints:@[
            [iv.topAnchor constraintEqualToAnchor:cell.topAnchor constant:2],
            [iv.centerXAnchor constraintEqualToAnchor:cell.centerXAnchor],
            [iv.widthAnchor constraintEqualToConstant:kThumbWidth],
            [iv.bottomAnchor constraintEqualToAnchor:n.topAnchor constant:-4],
            [n.centerXAnchor constraintEqualToAnchor:cell.centerXAnchor],
            [n.bottomAnchor constraintEqualToAnchor:cell.bottomAnchor constant:-2],
        ]];
    }
    cell.imageView.image = [self imageOf:static_cast<int>(row)];
    cell.textField.stringValue = [NSString stringWithFormat:@"%ld", static_cast<long>(row + 1)];
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification*)n {
    if (_following || _thumbs.selectedRow < 0) return;
    // several selected: the page clicked last is shown
    const NSInteger row = _thumbs.selectedRowIndexes.count > 1 && _thumbs.clickedRow >= 0 ? _thumbs.clickedRow : _thumbs.selectedRow;
    [self.delegate sidebarGoToPage:static_cast<int>(row)];
}

- (NSArray<NSNumber*>*)selectedPages {
    NSMutableArray<NSNumber*>* out = [NSMutableArray array];
    [_thumbs.selectedRowIndexes enumerateIndexesUsingBlock:^(NSUInteger i, BOOL*) { [out addObject:@(static_cast<int>(i))]; }];
    return out;
}

- (void)selectAllPages {
    _following = YES;
    [_thumbs selectAll:nil];
    _following = NO;
}

// The right-click menu: for the selected pages, or for the page clicked when it is not one of them.
- (void)menuNeedsUpdate:(NSMenu*)menu {
    [menu removeAllItems];
    const NSInteger clicked = _thumbs.clickedRow;
    if (clicked < 0 || ![self.delegate respondsToSelector:@selector(sidebarFillMenu:pages:)]) return;
    if (![_thumbs.selectedRowIndexes containsIndex:static_cast<NSUInteger>(clicked)]) {
        _following = YES;
        [_thumbs selectRowIndexes:[NSIndexSet indexSetWithIndex:static_cast<NSUInteger>(clicked)] byExtendingSelection:NO];
        _following = NO;
    }
    [self.delegate sidebarFillMenu:menu pages:[self selectedPages]];
}

// Dragging thumbnails moves their pages.
- (id<NSPasteboardWriting>)tableView:(NSTableView*)t pasteboardWriterForRow:(NSInteger)row {
    if (t != _thumbs) return nil;
    NSPasteboardItem* item = [[NSPasteboardItem alloc] init];
    [item setString:[NSString stringWithFormat:@"%ld", static_cast<long>(row)] forType:kPagesType];
    return item;
}

- (NSDragOperation)tableView:(NSTableView*)t validateDrop:(id<NSDraggingInfo>)info proposedRow:(NSInteger)row
       proposedDropOperation:(NSTableViewDropOperation)op {
    if (t != _thumbs || info.draggingSource != _thumbs) return NSDragOperationNone;
    [t setDropRow:row dropOperation:NSTableViewDropAbove];
    return NSDragOperationMove;
}

- (BOOL)tableView:(NSTableView*)t acceptDrop:(id<NSDraggingInfo>)info row:(NSInteger)row dropOperation:(NSTableViewDropOperation)op {
    NSMutableArray<NSNumber*>* pages = [NSMutableArray array];
    for (NSPasteboardItem* item in info.draggingPasteboard.pasteboardItems)
        if (NSString* s = [item stringForType:kPagesType]) [pages addObject:@(s.intValue)];
    if (!pages.count || ![self.delegate respondsToSelector:@selector(sidebarMovePages:before:)]) return NO;
    [pages sortUsingSelector:@selector(compare:)];
    [self.delegate sidebarMovePages:pages before:static_cast<int>(row)];
    return YES;
}

// ---- bookmarks ----

- (NSArray<FPBookmarkItem*>*)childrenOf:(id)item { return item ? ((FPBookmarkItem*)item).children : _bookmarks; }
- (NSInteger)outlineView:(NSOutlineView*)o numberOfChildrenOfItem:(id)item { return static_cast<NSInteger>([self childrenOf:item].count); }
- (id)outlineView:(NSOutlineView*)o child:(NSInteger)i ofItem:(id)item { return [self childrenOf:item][static_cast<NSUInteger>(i)]; }
- (BOOL)outlineView:(NSOutlineView*)o isItemExpandable:(id)item { return ((FPBookmarkItem*)item).children.count > 0; }

- (NSView*)outlineView:(NSOutlineView*)o viewForTableColumn:(NSTableColumn*)col item:(id)item {
    NSTableCellView* cell = [o makeViewWithIdentifier:@"mark" owner:self];
    if (!cell) {
        cell = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
        cell.identifier = @"mark";
        NSTextField* t = [NSTextField labelWithString:@""];
        t.lineBreakMode = NSLineBreakByTruncatingTail;
        t.translatesAutoresizingMaskIntoConstraints = NO;
        [cell addSubview:t];
        cell.textField = t;
        [NSLayoutConstraint activateConstraints:@[
            [t.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:2],
            [t.trailingAnchor constraintEqualToAnchor:cell.trailingAnchor constant:-2],
            [t.centerYAnchor constraintEqualToAnchor:cell.centerYAnchor],
        ]];
    }
    cell.textField.stringValue = ((FPBookmarkItem*)item).title;
    return cell;
}

- (void)bookmarkClicked:(id)sender {
    FPBookmarkItem* i = [_marks itemAtRow:_marks.clickedRow];
    if (i && i.page >= 0) [self.delegate sidebarGoToPage:i.page];
}

@end
