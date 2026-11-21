// The table editor: a table found on the page shown as a grid (as the Windows app's Edit table window): type in the
// cells; add and delete rows and columns, merge and split cells, borders, bold, alignment; Apply writes it back to the
// page (tables::write keeps what was not changed as it was), Delete table removes it.
#import "WindowController.h"
#import "Form.h"
#import <objc/runtime.h>
#include <algorithm>
#include <memory>
#include "platform.hpp"
#include "tables.hpp"

using form::ns;
using form::ws;

@interface FPTableGrid : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property(nonatomic) std::shared_ptr<tables::Table> table;
@property(nonatomic, strong) NSTableView* view;
- (void)rebuild;
@end

@implementation FPTableGrid
- (void)rebuild {
    while (self.view.tableColumns.count) [self.view removeTableColumn:self.view.tableColumns.lastObject];
    for (int c = 0; c < self.table->cols(); ++c) {
        NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%d", c]];
        col.title = [NSString stringWithFormat:@"%c", static_cast<char>('A' + c % 26)];
        col.width = 120;
        col.editable = YES;
        [self.view addTableColumn:col];
    }
    [self.view reloadData];
}
- (NSInteger)numberOfRowsInTableView:(NSTableView*)t { return self.table->rows(); }
- (id)tableView:(NSTableView*)t objectValueForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    const tables::Cell& cell = self.table->at(static_cast<int>(row), col.identifier.intValue);
    if (cell.covered) return @"← merged";
    std::wstring text = cell.text;
    for (auto& ch : text) if (ch == L'\n') ch = L'↵';   // a new line, shown as ↵ (type it as \n)
    return ns(text);
}
- (void)tableView:(NSTableView*)t setObjectValue:(id)value forTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    tables::Cell& cell = self.table->at(static_cast<int>(row), col.identifier.intValue);
    if (cell.covered) return;
    std::wstring text = ws(value);
    for (auto& ch : text) if (ch == L'↵') ch = L'\n';
    for (size_t i; (i = text.find(L"\\n")) != std::wstring::npos;) text.replace(i, 2, L"\n");
    cell.text = text;
}
- (BOOL)tableView:(NSTableView*)t shouldEditTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    return !self.table->at(static_cast<int>(row), col.identifier.intValue).covered;
}
@end

@interface FPWindowController (TableSheet)
- (void)editTable:(std::shared_ptr<tables::Table>)t;
@end

@implementation FPWindowController (TableSheet)

- (void)editTable:(std::shared_ptr<tables::Table>)t {
    Document* d = self.pdf;
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 820, 520) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskDocModalWindow
                                                  backing:NSBackingStoreBuffered defer:NO];
    NSView* root = panel.contentView;
    FPTableGrid* grid = [[FPTableGrid alloc] init];
    grid.table = t;
    NSTableView* tv = [[NSTableView alloc] initWithFrame:NSZeroRect];
    tv.dataSource = grid;
    tv.delegate = grid;
    tv.usesAlternatingRowBackgroundColors = YES;
    tv.gridStyleMask = NSTableViewSolidVerticalGridLineMask | NSTableViewSolidHorizontalGridLineMask;
    tv.allowsColumnSelection = YES;
    grid.view = tv;
    [grid rebuild];
    NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    sv.documentView = tv;
    sv.hasVerticalScroller = sv.hasHorizontalScroller = YES;
    sv.translatesAutoresizingMaskIntoConstraints = NO;
    NSTextField* title = [NSTextField labelWithString:[NSString stringWithFormat:@"Table on page %d: %d rows, %d columns. Double-click a cell to type in it (\\n for a new line).",
                                                                              t->page + 1, t->rows(), t->cols()]];
    title.translatesAutoresizingMaskIntoConstraints = NO;

    NSMutableArray<NSView*>* buttons = [NSMutableArray array];
    // the row and column of the cell to act on: the selected row, and the column typed in the box (A, B, ...)
    NSComboBox* colBox = [[NSComboBox alloc] initWithFrame:NSMakeRect(0, 0, 60, 24)];
    for (int c = 0; c < t->cols(); ++c) [colBox addItemWithObjectValue:[NSString stringWithFormat:@"%c", static_cast<char>('A' + c % 26)]];
    colBox.stringValue = @"A";
    [colBox.widthAnchor constraintEqualToConstant:60].active = YES;
    auto colIndex = [colBox, t]() { NSString* v = colBox.stringValue.uppercaseString; const int c = v.length ? [v characterAtIndex:0] - 'A' : 0; return std::clamp(c, 0, t->cols() - 1); };
    auto rowIndex = [tv, t]() { return static_cast<int>(std::clamp<NSInteger>(tv.selectedRow, 0, t->rows() - 1)); };
    auto addButton = [&](NSString* label, void (^act)(void)) {
        NSButton* b = [NSButton buttonWithTitle:label target:nil action:nil];
        objc_setAssociatedObject(b, "act", act, OBJC_ASSOCIATION_COPY);
        b.target = b;
        b.action = @selector(featherRun:);
        [buttons addObject:b];
    };
    addButton(@"Row above", ^{ tables::insertRow(*t, rowIndex()); [grid rebuild]; });
    addButton(@"Row below", ^{ tables::insertRow(*t, rowIndex() + 1); [grid rebuild]; });
    addButton(@"Delete row", ^{ if (t->rows() > 1) { tables::deleteRow(*t, rowIndex()); [grid rebuild]; } });
    addButton(@"Column left", ^{ tables::insertColumn(*t, colIndex()); [grid rebuild]; });
    addButton(@"Column right", ^{ tables::insertColumn(*t, colIndex() + 1); [grid rebuild]; });
    addButton(@"Delete column", ^{ if (t->cols() > 1) { tables::deleteColumn(*t, colIndex()); [grid rebuild]; } });
    addButton(@"Merge with right", ^{ const int r = rowIndex(), c = colIndex(); if (c + 1 < t->cols() && tables::merge(*t, r, c, r, c + 1)) [grid rebuild]; else NSBeep(); });
    addButton(@"Merge with below", ^{ const int r = rowIndex(), c = colIndex(); if (r + 1 < t->rows() && tables::merge(*t, r, c, r + 1, c)) [grid rebuild]; else NSBeep(); });
    addButton(@"Split", ^{ tables::split(*t, rowIndex(), colIndex()); [grid rebuild]; });
    addButton(@"Bold", ^{ int ar, ac; tables::anchorOf(*t, rowIndex(), colIndex(), ar, ac); t->at(ar, ac).bold = !t->at(ar, ac).bold; [grid rebuild]; });
    NSPopUpButton* align = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES];
    [align addItemsWithTitles:@[ @"Align", @"Left", @"Centre", @"Right", @"Justified" ]];
    NSPopUpButton* borders = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:YES];
    [borders addItemsWithTitles:@[ @"Borders", @"All lines", @"Outside only", @"Rows only", @"No lines" ]];
    for (NSPopUpButton* p in @[ align, borders ]) { p.target = p; p.action = @selector(featherRun:); }
    objc_setAssociatedObject(align, "act", ^{
        int ar, ac; tables::anchorOf(*t, rowIndex(), colIndex(), ar, ac);
        t->at(ar, ac).align = static_cast<int>(align.indexOfSelectedItem) - 1;
    }, OBJC_ASSOCIATION_COPY);
    objc_setAssociatedObject(borders, "act", ^{
        static const tables::Borders b[] = {tables::Borders::All, tables::Borders::Outside, tables::Borders::Rows, tables::Borders::None};
        tables::setBorders(*t, b[std::clamp<NSInteger>(borders.indexOfSelectedItem - 1, 0, 3)]);
    }, OBJC_ASSOCIATION_COPY);
    NSStackView* tools1 = [NSStackView stackViewWithViews:[@[ [NSTextField labelWithString:@"Column:"], colBox ] arrayByAddingObjectsFromArray:[buttons subarrayWithRange:NSMakeRange(0, 6)]]];
    NSStackView* tools2 = [NSStackView stackViewWithViews:[[buttons subarrayWithRange:NSMakeRange(6, 4)] arrayByAddingObjectsFromArray:@[ align, borders ]]];

    NSButton* apply = [NSButton buttonWithTitle:@"Apply" target:nil action:nil];
    apply.keyEquivalent = @"\r";
    NSButton* cancel = [NSButton buttonWithTitle:@"Cancel" target:nil action:nil];
    cancel.keyEquivalent = @"\033";
    NSButton* del = [NSButton buttonWithTitle:@"Delete table" target:nil action:nil];
    for (NSButton* b in @[ apply, cancel, del ]) { b.target = b; b.action = @selector(featherRun:); }
    objc_setAssociatedObject(apply, "act", ^{ [tv.window makeFirstResponder:nil]; [self.window endSheet:panel returnCode:1]; }, OBJC_ASSOCIATION_COPY);
    objc_setAssociatedObject(cancel, "act", ^{ [self.window endSheet:panel returnCode:0]; }, OBJC_ASSOCIATION_COPY);
    objc_setAssociatedObject(del, "act", ^{ [self.window endSheet:panel returnCode:2]; }, OBJC_ASSOCIATION_COPY);
    NSStackView* bottom = [NSStackView stackViewWithViews:@[ del, [[NSView alloc] init], cancel, apply ]];

    for (NSView* v in @[ title, tools1, tools2, sv, bottom ]) { v.translatesAutoresizingMaskIntoConstraints = NO; [root addSubview:v]; }
    [NSLayoutConstraint activateConstraints:@[
        [title.topAnchor constraintEqualToAnchor:root.topAnchor constant:14], [title.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:16],
        [tools1.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:10], [tools1.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:16],
        [tools2.topAnchor constraintEqualToAnchor:tools1.bottomAnchor constant:6], [tools2.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:16],
        [sv.topAnchor constraintEqualToAnchor:tools2.bottomAnchor constant:10], [sv.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:16],
        [sv.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-16], [sv.bottomAnchor constraintEqualToAnchor:bottom.topAnchor constant:-12],
        [bottom.leadingAnchor constraintEqualToAnchor:root.leadingAnchor constant:16], [bottom.trailingAnchor constraintEqualToAnchor:root.trailingAnchor constant:-16],
        [bottom.bottomAnchor constraintEqualToAnchor:root.bottomAnchor constant:-14],
    ]];
    objc_setAssociatedObject(panel, "grid", grid, OBJC_ASSOCIATION_RETAIN);
    [self.window beginSheet:panel completionHandler:^(NSModalResponse r) {
        if (r == 2) {
            if (tables::remove(*d, *t)) [self afterChange];
            return;
        }
        if (r != 1) return;
        std::wstring why;
        double grew = 0;
        bool shrunk = false;
        if (!tables::write(*d, *t, &why, &grew, &shrunk)) { form::message(self.window, why.empty() ? L"The table could not be changed." : why, true); return; }
        [self afterChange];
        if (shrunk) form::message(self.window, L"There was no room on the page for the table to grow, so the text you changed was set a little smaller to fit. Undo (⌘Z) takes it back.");
    }];
}

@end

// A control that runs the block attached to it (the table editor's buttons).
@implementation NSControl (FeatherRun)
- (void)featherRun:(id)sender {
    void (^act)(void) = objc_getAssociatedObject(self, "act");
    if (act) act();
}
@end
