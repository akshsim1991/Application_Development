// Search all my PDFs in a folder (Tools menu): the text of every PDF in a folder and its subfolders, with an index kept
// in the app's folder so the next search reads only the files that changed. Double-click a result to open it there.
#import <Cocoa/Cocoa.h>
#import "FolderSearch.h"
#import "Form.h"
#import "FPDocument.h"
#import "WindowController.h"
#import "PageView.h"
#include <string>
#include <vector>
#include "foldersearch.hpp"
#include "platform.hpp"

using form::Field;
using form::Fields;
using form::ns;

@interface FPSearchResults : NSWindowController <NSTableViewDataSource, NSTableViewDelegate>
@property(nonatomic) std::vector<foldersearch::Hit> hits;
@end

@implementation FPSearchResults {
    NSTableView* _table;
}

- (instancetype)initWithHits:(std::vector<foldersearch::Hit>)hits title:(NSString*)title {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 900, 520)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered defer:NO];
    w.title = title;
    if (!(self = [super initWithWindow:w])) return nil;
    _hits = std::move(hits);
    _table = [[NSTableView alloc] initWithFrame:NSZeroRect];
    NSArray* cols = @[ @[ @"file", @"File", @220 ], @[ @"page", @"Page", @50 ], @[ @"text", @"Found in", @600 ] ];
    for (NSArray* c in cols) {
        NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:c[0]];
        col.title = c[1];
        col.width = [c[2] doubleValue];
        [_table addTableColumn:col];
    }
    _table.dataSource = self;
    _table.delegate = self;
    _table.target = self;
    _table.doubleAction = @selector(openHit:);
    _table.usesAlternatingRowBackgroundColors = YES;
    NSScrollView* sv = [[NSScrollView alloc] initWithFrame:w.contentView.bounds];
    sv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    sv.documentView = _table;
    sv.hasVerticalScroller = YES;
    w.contentView = sv;
    [w center];
    return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)t { return static_cast<NSInteger>(_hits.size()); }

- (id)tableView:(NSTableView*)t objectValueForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    const foldersearch::Hit& h = _hits[static_cast<size_t>(row)];
    if ([col.identifier isEqualToString:@"file"]) return ns(h.file).lastPathComponent;
    if ([col.identifier isEqualToString:@"page"]) return @(h.page + 1);
    return ns(h.snippet);
}

- (void)openHit:(id)sender {
    const NSInteger row = _table.clickedRow;
    if (row < 0) return;
    const foldersearch::Hit h = _hits[static_cast<size_t>(row)];
    [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:[NSURL fileURLWithPath:ns(h.file)] display:YES
                                                                 completionHandler:^(NSDocument* doc, BOOL, NSError* e) {
        if (e) { [NSApp presentError:e]; return; }
        FPWindowController* wc = (FPWindowController*)doc.windowControllers.firstObject;
        dispatch_async(dispatch_get_main_queue(), ^{ [wc.pageView showPage:h.page]; });
    }];
}

@end

void FPSearchFolder(NSWindow* parent) {
    Fields f(2);
    f[0] = {Field::Text, L"Find:", {}, L""};
    f[1] = {Field::Check, L"Match case"};
    auto run = [parent](const Fields& v) {
        NSOpenPanel* p = [NSOpenPanel openPanel];
        p.canChooseFiles = NO;
        p.canChooseDirectories = YES;
        p.prompt = @"Search";
        p.title = @"Choose the folder to search (its subfolders too)";
        if ([p runModal] != NSModalResponseOK) return;
        const std::wstring folder = platform::wide(p.URL.fileSystemRepresentation), query = v[0].text;
        const bool matchCase = v[1].checked;
        NSWindow* progress = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 420, 70) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        progress.title = @"Searching…";
        NSTextField* label = [NSTextField labelWithString:@"Reading the files…"];
        label.frame = NSMakeRect(20, 24, 380, 20);
        label.lineBreakMode = NSLineBreakByTruncatingMiddle;
        [progress.contentView addSubview:label];
        [progress center];
        [progress makeKeyAndOrderFront:nil];
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            foldersearch::Stats st;
            const std::wstring index = platform::appDataDir() + L"/index";
            auto hits = foldersearch::search(folder, query, matchCase, index, [label](int done, int total, const std::wstring& file) {
                NSString* t = [NSString stringWithFormat:@"%d of %d: %@", done + 1, total, ns(file).lastPathComponent];
                dispatch_async(dispatch_get_main_queue(), ^{ label.stringValue = t; });
                return true;
            }, st);
            dispatch_async(dispatch_get_main_queue(), ^{
                [progress close];
                if (hits.empty()) {
                    form::message(parent, L"\"" + query + L"\" was not found in the " + std::to_wstring(st.files) + L" PDF file(s) of that folder" +
                                          (st.skipped ? L" (" + std::to_wstring(st.skipped) + L" could not be read: damaged or protected)." : L"."));
                    return;
                }
                NSString* title = [NSString stringWithFormat:@"“%@”: %zu place(s) in %d file(s) — double-click to open", ns(query), hits.size(), st.files];
                static NSMutableArray* windows = [NSMutableArray array];   // kept while shown
                FPSearchResults* r = [[FPSearchResults alloc] initWithHits:hits title:title];
                [windows addObject:r];
                [r showWindow:nil];
            });
        });
    };
    if (parent) form::show(parent, L"Search all my PDFs", f, L"Choose folder…", [](const Fields& v) -> std::wstring { return v[0].text.empty() ? L"Type the text to find." : L""; }, run);
    else {
        NSAlert* a = [[NSAlert alloc] init];   // no document open: ask in a plain window
        a.messageText = @"Search all my PDFs";
        a.informativeText = @"Type the text to find:";
        NSTextField* t = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 280, 24)];
        a.accessoryView = t;
        [a addButtonWithTitle:@"Choose folder…"];
        [a addButtonWithTitle:@"Cancel"];
        if ([a runModal] != NSAlertFirstButtonReturn || !t.stringValue.length) return;
        Fields v = f;
        v[0].text = form::ws(t.stringValue);
        run(v);
    }
}
