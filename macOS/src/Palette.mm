// The command palette (⌘K, as Ctrl+K on Windows): type a few letters of any menu command and press Return.
#import "Palette.h"

@interface FPPalette : NSWindowController <NSSearchFieldDelegate, NSTableViewDataSource, NSTableViewDelegate>
@end

@implementation FPPalette {
    NSSearchField* _field;
    NSTableView* _table;
    NSMutableArray<NSMenuItem*>* _all;
    NSMutableArray<NSString*>* _names;
    NSArray<NSNumber*>* _shown;
    NSWindow* _target;   // the window the command is for
}

static void collect(NSMenu* m, NSString* path, NSMutableArray<NSMenuItem*>* items, NSMutableArray<NSString*>* names) {
    for (NSMenuItem* i in m.itemArray) {
        if (i.isSeparatorItem || i.isHidden) continue;
        NSString* title = (m == NSApp.mainMenu && i == m.itemArray.firstObject) ? @"Feather PDF" : i.title;
        NSString* here = path.length ? [NSString stringWithFormat:@"%@ \u203A %@", path, title] : title;
        if (i.submenu) {
            if (![i.title isEqualToString:@"Services"] && ![i.title isEqualToString:@"Open Recent"]) collect(i.submenu, here, items, names);
            continue;
        }
        if (!i.action) continue;
        [items addObject:i];
        [names addObject:here];
    }
}

- (instancetype)init {
    NSPanel* p = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 560, 380) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                              backing:NSBackingStoreBuffered defer:NO];
    p.title = @"Command palette";
    if (!(self = [super initWithWindow:p])) return nil;
    _field = [[NSSearchField alloc] initWithFrame:NSMakeRect(16, 340, 528, 26)];
    _field.placeholderString = @"Type a command, for example \"watermark\" or \"rotate\"";
    _field.delegate = self;
    _table = [[NSTableView alloc] initWithFrame:NSZeroRect];
    [_table addTableColumn:[[NSTableColumn alloc] initWithIdentifier:@"c"]];
    _table.headerView = nil;
    _table.dataSource = self;
    _table.delegate = self;
    _table.doubleAction = @selector(run:);
    _table.target = self;
    NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSMakeRect(16, 16, 528, 314)];
    sv.documentView = _table;
    sv.hasVerticalScroller = YES;
    [p.contentView addSubview:_field];
    [p.contentView addSubview:sv];
    return self;
}

- (void)showFor:(NSWindow*)w {
    _target = w;
    _all = [NSMutableArray array];
    _names = [NSMutableArray array];
    collect(NSApp.mainMenu, @"", _all, _names);
    _field.stringValue = @"";
    [self filter];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:_field];
}

- (void)filter {
    NSArray<NSString*>* words = [_field.stringValue.lowercaseString componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    NSMutableArray<NSNumber*>* shown = [NSMutableArray array];
    for (NSUInteger i = 0; i < _all.count; ++i) {
        NSString* n = _names[i].lowercaseString;
        BOOL all = YES;
        for (NSString* w in words) if (w.length && [n rangeOfString:w].location == NSNotFound) { all = NO; break; }
        if (all) [shown addObject:@(i)];
    }
    _shown = shown;
    [_table reloadData];
    if (_shown.count) [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
}

- (void)controlTextDidChange:(NSNotification*)n { [self filter]; }

- (BOOL)control:(NSControl*)c textView:(NSTextView*)tv doCommandBySelector:(SEL)cmd {
    const NSInteger row = _table.selectedRow;
    if (cmd == @selector(moveDown:)) { if (row + 1 < static_cast<NSInteger>(_shown.count)) [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:static_cast<NSUInteger>(row + 1)] byExtendingSelection:NO]; [_table scrollRowToVisible:_table.selectedRow]; return YES; }
    if (cmd == @selector(moveUp:)) { if (row > 0) [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:static_cast<NSUInteger>(row - 1)] byExtendingSelection:NO]; [_table scrollRowToVisible:_table.selectedRow]; return YES; }
    if (cmd == @selector(insertNewline:)) { [self run:nil]; return YES; }
    if (cmd == @selector(cancelOperation:)) { [self.window close]; return YES; }
    return NO;
}

- (void)run:(id)sender {
    const NSInteger row = _table.selectedRow;
    if (row < 0 || row >= static_cast<NSInteger>(_shown.count)) return;
    NSMenuItem* item = _all[_shown[static_cast<NSUInteger>(row)].unsignedIntegerValue];
    [self.window close];
    [_target makeKeyAndOrderFront:nil];
    dispatch_async(dispatch_get_main_queue(), ^{ [NSApp sendAction:item.action to:item.target from:item]; });
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)t { return static_cast<NSInteger>(_shown.count); }

- (NSView*)tableView:(NSTableView*)t viewForTableColumn:(NSTableColumn*)c row:(NSInteger)row {
    NSTextField* l = [t makeViewWithIdentifier:@"cmd" owner:self];
    if (!l) { l = [NSTextField labelWithString:@""]; l.identifier = @"cmd"; l.lineBreakMode = NSLineBreakByTruncatingMiddle; }
    l.stringValue = _names[_shown[static_cast<NSUInteger>(row)].unsignedIntegerValue];
    return l;
}

@end

void FPShowCommandPalette(NSWindow* forWindow) {
    static FPPalette* p = nil;
    if (!p) p = [[FPPalette alloc] init];
    [p showFor:forWindow];
}
