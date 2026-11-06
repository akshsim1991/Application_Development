// Saved workspaces (File > Workspaces), as on Windows: a named group of open documents (their order, page and zoom),
// reopened together as tabs of one window. Kept in the app's settings; the documents themselves are not touched.
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "PageView.h"
#import "Form.h"
#include <string>
#include <vector>

extern void FPForceTabs(NSInteger count);   // TabMenu.mm: the next documents open as tabs, whatever the setting

@interface FPWorkspaces : NSObject <NSMenuDelegate>
+ (instancetype)shared;
@end

namespace {
NSString* const kKey = @"workspaces";   // name -> {docs: [{path, page, zoom}], active}
NSDictionary* all() { return [NSUserDefaults.standardUserDefaults dictionaryForKey:kKey] ?: @{}; }

// The document windows in tab order: the frontmost window's tabs (or the window itself).
NSArray<FPWindowController*>* openTabs() {
    NSWindow* front = NSApp.mainWindow ?: NSApp.keyWindow;
    if (![front.windowController isKindOfClass:[FPWindowController class]])
        for (NSWindow* w in NSApp.orderedWindows) if ([w.windowController isKindOfClass:[FPWindowController class]]) { front = w; break; }
    NSMutableArray* out = [NSMutableArray array];
    for (NSWindow* w in front.tabbedWindows ?: (front ? @[ front ] : @[]))
        if ([w.windowController isKindOfClass:[FPWindowController class]]) [out addObject:w.windowController];
    return out;
}
} // namespace

@implementation FPWorkspaces

+ (instancetype)shared {
    static FPWorkspaces* s = [[FPWorkspaces alloc] init];
    return s;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
    [menu removeAllItems];
    NSMenuItem* save = [menu addItemWithTitle:@"Save Open Documents as a Workspace…" action:@selector(save:) keyEquivalent:@""];
    save.target = self;
    [menu addItem:[NSMenuItem separatorItem]];
    NSArray* names = [all().allKeys sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    for (NSString* n in names) {
        NSMenuItem* i = [menu addItemWithTitle:n action:@selector(open:) keyEquivalent:@""];
        i.target = self;
        i.representedObject = n;
    }
    if (!names.count) [menu addItemWithTitle:@"(No Workspaces Yet)" action:nil keyEquivalent:@""].enabled = NO;
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* del = [menu addItemWithTitle:@"Delete a Workspace…" action:names.count ? @selector(remove:) : nil keyEquivalent:@""];
    del.target = self;
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    if (item.action == @selector(save:)) return openTabs().count > 0;
    return YES;
}

- (void)save:(id)sender {
    NSArray<FPWindowController*>* tabs = openTabs();
    NSWindow* parent = tabs.firstObject.window;
    if (!parent) return;
    form::Fields f(2);
    f[0] = {form::Field::Note, L"", {}, L"Opening it later brings back these documents as tabs, in this order, at the pages and zoom they are at now."};
    f[1] = {form::Field::Text, L"Name (for example Semester 3 Papers):", {}, L""};
    form::show(parent, L"Save as a workspace", f, L"Save", [](const form::Fields& v) { return v[1].text.empty() ? std::wstring(L"Type a name.") : std::wstring(); },
               [tabs, parent](const form::Fields& v) {
        NSString* name = form::ns(v[1].text);
        NSMutableArray* docs = [NSMutableArray array];
        NSInteger active = 0, skipped = 0;
        for (FPWindowController* wc in tabs) {
            NSURL* url = [(NSDocument*)wc.document fileURL];
            if (!url) { ++skipped; continue; }
            if (wc.window.isKeyWindow || wc.window == NSApp.mainWindow) active = static_cast<NSInteger>(docs.count);
            [docs addObject:@{@"path" : url.path, @"page" : @([wc.pageView currentPage]), @"zoom" : @(wc.pageView.zoom), @"fit" : @(wc.pageView.fitWidth)}];
        }
        NSMutableDictionary* m = [all() mutableCopy];
        m[name] = @{@"docs" : docs, @"active" : @(active)};
        [NSUserDefaults.standardUserDefaults setObject:m forKey:kKey];
        std::wstring msg = L"The workspace was saved with " + std::to_wstring(docs.count) + L" document(s). Open it from File > Workspaces.";
        if (skipped) msg += L" " + std::to_wstring(skipped) + L" new document(s) that were never saved are not in it: save them first to include them.";
        form::message(parent, msg);
    });
}

- (void)open:(NSMenuItem*)sender {
    NSDictionary* ws = all()[sender.representedObject];
    NSArray* docs = ws[@"docs"];
    const NSInteger active = [ws[@"active"] integerValue];
    NSMutableArray<NSString*>* missing = [NSMutableArray array];
    NSMutableArray* present = [NSMutableArray array];
    for (NSDictionary* e in docs) {
        if ([NSFileManager.defaultManager fileExistsAtPath:e[@"path"]]) [present addObject:e];
        else [missing addObject:e[@"path"]];
    }
    FPForceTabs(static_cast<NSInteger>(present.count));
    __block NSInteger left = static_cast<NSInteger>(present.count);
    __block NSWindow* activeWindow = nil;
    for (NSUInteger i = 0; i < present.count; ++i) {
        NSDictionary* e = present[i];
        const BOOL isActive = static_cast<NSInteger>([docs indexOfObject:e]) == active;
        [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:[NSURL fileURLWithPath:e[@"path"]] display:YES
                                                                     completionHandler:^(NSDocument* doc, BOOL wasOpen, NSError* err) {
            FPWindowController* wc = (FPWindowController*)doc.windowControllers.firstObject;
            dispatch_async(dispatch_get_main_queue(), ^{   // after the window has its size: the saved page and zoom
                if ([wc isKindOfClass:[FPWindowController class]]) {
                    if (![e[@"fit"] boolValue] && [e[@"zoom"] doubleValue] > 0) [wc.pageView setZoom:[e[@"zoom"] doubleValue]];
                    [wc.pageView showPage:[e[@"page"] intValue]];
                    if (isActive) activeWindow = wc.window;
                }
                if (--left == 0 && activeWindow) [activeWindow makeKeyAndOrderFront:nil];
            });
        }];
    }
    if (missing.count) {
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"Some files of this workspace were not found (moved, renamed or deleted):";
        a.informativeText = [missing componentsJoinedByString:@"\n"];
        [a runModal];
    }
}

- (void)remove:(id)sender {
    NSArray* names = [all().allKeys sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = @"Delete a workspace";
    a.informativeText = @"Only the list is deleted: the documents themselves stay.";
    NSPopUpButton* p = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 260, 26) pullsDown:NO];
    [p addItemsWithTitles:names];
    a.accessoryView = p;
    [a addButtonWithTitle:@"Delete"];
    [a addButtonWithTitle:@"Cancel"];
    if ([a runModal] != NSAlertFirstButtonReturn || !p.titleOfSelectedItem) return;
    NSMutableDictionary* m = [all() mutableCopy];
    [m removeObjectForKey:p.titleOfSelectedItem];
    [NSUserDefaults.standardUserDefaults setObject:m forKey:kKey];
}

@end

void FPAddWorkspacesMenu(NSMenu* fileMenu) {
    NSMenuItem* item = [fileMenu addItemWithTitle:@"Workspaces" action:nil keyEquivalent:@""];
    item.submenu = [[NSMenu alloc] initWithTitle:@"Workspaces"];
    item.submenu.delegate = [FPWorkspaces shared];
}
