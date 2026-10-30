// Settings (⌘,): the Windows app's six groups (Appearance, Tabs, PDF Viewing, Editing, Performance, Advanced); every
// change applies at once and is kept. Themes: Light, Modern Light, Dark, AMOLED Dark, Midnight Blue, Graphite, Like
// macOS — the Mac's light or dark look with the theme's colours around the pages and in the sidebar.
#import "Settings.h"
#import "FPDocument.h"
#import <objc/runtime.h>
#include <algorithm>
#include "platform.hpp"
#import "Form.h"
#import "Palette.h"
#import "Updater.h"
#import "WindowController.h"

NSNotificationName const FPSettingsChanged = @"FPSettingsChanged";

static NSUserDefaults* defaults() { return NSUserDefaults.standardUserDefaults; }
static NSInteger intPref(NSString* k, NSInteger def) { NSNumber* v = [defaults() objectForKey:k]; return v ? v.integerValue : def; }

NSInteger FPTheme() { return intPref(@"theme", 6); }
BOOL FPColourIcons() { return intPref(@"colourIcons", 1) != 0; }
BOOL FPTabColours() { return intPref(@"tabColours", 0) != 0; }
BOOL FPTabNumbers() { return intPref(@"tabNumbers", 0) != 0; }
NSInteger FPDuplicateMode() { return intPref(@"duplicateOpen", 0); }
BOOL FPRestorePositions() { return intPref(@"restorePositions", 1) != 0; }
NSInteger FPPagesKept() { const NSInteger i = intPref(@"pageCache", 1); return i == 0 ? 3 : i == 1 ? 10 : 24; }

// The surround of the pages for each theme (Windows: the "view" colour of its palette).
NSColor* FPThemeSurround() {
    switch (FPTheme()) {
        case 1: return [NSColor colorWithSRGBRed:0.93 green:0.94 blue:0.96 alpha:1];   // Modern Light
        case 3: return NSColor.blackColor;                                             // AMOLED Dark
        case 4: return [NSColor colorWithSRGBRed:0.07 green:0.10 blue:0.18 alpha:1];  // Midnight Blue
        case 5: return [NSColor colorWithSRGBRed:0.16 green:0.17 blue:0.18 alpha:1];  // Graphite
        default: return NSColor.underPageBackgroundColor;
    }
}

void FPApplyTheme() {
    switch (FPTheme()) {
        case 0: case 1: NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua]; break;
        case 2: case 3: case 4: case 5: NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]; break;
        default: NSApp.appearance = nil;   // like macOS
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:FPSettingsChanged object:nil];
}

// ---- the window ----

@interface FPSettingsWindow : NSWindowController
@end

@implementation FPSettingsWindow {
    NSTabView* _tabs;
}

static NSView* row(NSString* label, NSString* help, NSView* control) {
    NSTextField* l = [NSTextField labelWithString:label];
    l.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    NSTextField* h = [NSTextField wrappingLabelWithString:help];
    h.textColor = NSColor.secondaryLabelColor;
    h.font = [NSFont systemFontOfSize:11];
    [h.widthAnchor constraintEqualToConstant:330].active = YES;
    NSStackView* left = [NSStackView stackViewWithViews:@[ l, h ]];
    left.orientation = NSUserInterfaceLayoutOrientationVertical;
    left.alignment = NSLayoutAttributeLeading;
    left.spacing = 2;
    NSStackView* r = [NSStackView stackViewWithViews:@[ left, control ]];
    r.alignment = NSLayoutAttributeCenterY;
    r.spacing = 16;
    return r;
}

static NSPopUpButton* choice(NSString* key, NSInteger def, NSArray<NSString*>* items, void (^after)(void)) {
    NSPopUpButton* p = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 220, 26) pullsDown:NO];
    [p addItemsWithTitles:items];
    [p selectItemAtIndex:std::min<NSInteger>(intPref(key, def), static_cast<NSInteger>(items.count) - 1)];
    objc_setAssociatedObject(p, "act", ^{
        [defaults() setInteger:p.indexOfSelectedItem forKey:key];
        if (after) after();
        [[NSNotificationCenter defaultCenter] postNotificationName:FPSettingsChanged object:nil];
    }, OBJC_ASSOCIATION_COPY);
    p.target = p;
    p.action = @selector(featherRun:);
    [p.widthAnchor constraintEqualToConstant:220].active = YES;
    return p;
}

static NSButton* toggle(NSString* key, BOOL def, void (^after)(void)) {
    NSButton* b = [NSButton checkboxWithTitle:@"" target:nil action:nil];
    b.state = intPref(key, def) ? NSControlStateValueOn : NSControlStateValueOff;
    objc_setAssociatedObject(b, "act", ^{
        [defaults() setInteger:b.state == NSControlStateValueOn forKey:key];
        if (after) after();
        [[NSNotificationCenter defaultCenter] postNotificationName:FPSettingsChanged object:nil];
    }, OBJC_ASSOCIATION_COPY);
    b.target = b;
    b.action = @selector(featherRun:);
    return b;
}

static NSButton* button(NSString* title, void (^act)(void)) {
    NSButton* b = [NSButton buttonWithTitle:title target:nil action:nil];
    objc_setAssociatedObject(b, "act", act, OBJC_ASSOCIATION_COPY);
    b.target = b;
    b.action = @selector(featherRun:);
    return b;
}

- (NSTabViewItem*)group:(NSString*)title rows:(NSArray<NSView*>*)rows {
    NSStackView* s = [NSStackView stackViewWithViews:rows];
    s.orientation = NSUserInterfaceLayoutOrientationVertical;
    s.alignment = NSLayoutAttributeLeading;
    s.spacing = 18;
    s.edgeInsets = NSEdgeInsetsMake(20, 24, 20, 24);
    NSTabViewItem* t = [[NSTabViewItem alloc] initWithIdentifier:title];
    t.label = title;
    t.view = s;
    return t;
}

- (instancetype)init {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 680, 460) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                backing:NSBackingStoreBuffered defer:NO];
    w.title = @"Settings";
    if (!(self = [super initWithWindow:w])) return nil;
    _tabs = [[NSTabView alloc] initWithFrame:w.contentView.bounds];
    _tabs.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    NSArray* colours = @[ @"Yellow", @"Green", @"Blue", @"Pink", @"Orange", @"Red", @"Black" ];
    [_tabs addTabViewItem:[self group:@"Appearance" rows:@[
        row(@"Theme", @"Colours of the windows and around the pages. Like macOS follows the Mac's light or dark appearance.",
            choice(@"theme", 6, @[ @"Light", @"Modern Light", @"Dark", @"AMOLED Dark (true black)", @"Midnight Blue", @"Graphite", @"Like macOS" ], ^{ FPApplyTheme(); })),
        row(@"Colourful toolbar icons", @"Each toolbar icon in its own colour. Off: icons in the text colour.", toggle(@"colourIcons", YES, nil)),
        row(@"Random tab colours", @"Each new tab gets a coloured mark of its own and keeps it until it is closed.", toggle(@"tabColours", NO, nil)) ]]];
    [_tabs addTabViewItem:[self group:@"Tabs" rows:@[
        row(@"Show tab numbers", @"Number the tabs: \"1. Report.pdf\". The numbers follow when tabs are opened, closed or moved.", toggle(@"tabNumbers", NO, nil)),
        row(@"Opening a PDF that is already open", @"The same file opened again (also through another path or an alias).",
            choice(@"duplicateOpen", 0, @[ @"Ask first", @"Go to its tab" ], nil)) ]]];
    [_tabs addTabViewItem:[self group:@"PDF Viewing" rows:@[
        row(@"Page colours", @"Dark turns pages dark for reading at night; Dimmed softens bright white.",
            choice(FPPageColoursKey, 0, @[ @"Normal", @"Dark (night mode)", @"Dimmed (softer white)" ], ^{
                [[NSNotificationCenter defaultCenter] postNotificationName:FPPageColoursChanged object:nil]; })),
        row(@"Open documents where they were last read", @"The page and zoom of the last 100 documents are remembered.", toggle(@"restorePositions", YES, nil)),
        row(@"Reading speed (read aloud)", @"How fast the Mac's voice reads.", choice(@"readingSpeed", 1, @[ @"Slow", @"Normal", @"Fast", @"Very fast" ], nil)) ]]];
    [_tabs addTabViewItem:[self group:@"Editing" rows:@[
        row(@"Highlight colour", @"For highlights and for comments on selected text.", choice(@"highlightColour", 0, colours, nil)),
        row(@"Drawing colour", @"For shapes, lines, arrows, the pen and new text boxes.", choice(@"drawingColour", 5, colours, nil)),
        row(@"Line width", @"For shapes, lines, arrows and the pen.", choice(@"lineWidth", 1, @[ @"Thin", @"Medium", @"Thick" ], nil)) ]]];
    [_tabs addTabViewItem:[self group:@"Performance" rows:@[
        row(@"Pages kept ready in memory", @"More pages make paging back and forth faster and use more memory.",
            choice(@"pageCache", 1, @[ @"Few", @"Normal", @"Many" ], nil)),
        row(@"Recovery copy of unsaved changes", @"Unsaved changes are copied so they come back after a crash. Never: no copy is kept.",
            choice(@"recoveryCopy", 0, @[ @"Every 20 seconds", @"Every minute", @"Every 5 minutes", @"Never" ], nil)) ]]];
    [_tabs addTabViewItem:[self group:@"Advanced" rows:@[
        row(@"Look for new versions", @"When Feather PDF starts, it checks whether a new version is out and asks whether to update now or later.",
            toggle(FPCheckUpdatesKey, YES, nil)),
        row(@"Update", @"Checks now whether a new version of Feather PDF is out.", button(@"Update to New Version…", ^{ FPCheckForUpdates(YES); })),
        row(@"Default PDF application", @"Makes Feather PDF the app that opens PDF files.", button(@"Set as Default…", ^{ FPSetAsDefaultPdfApp(nil); })),
        row(@"Recent files", @"The list on the start page and in Open Recent.", button(@"Clear the List", ^{ [[NSDocumentController sharedDocumentController] clearRecentDocuments:nil]; })),
        row(@"Reset settings", @"Puts every setting back as it was when Feather PDF was installed.", button(@"Reset All…", ^{
            NSAlert* a = [[NSAlert alloc] init];
            a.messageText = @"Put every setting back to its default? Recent files and reading positions are kept.";
            [a addButtonWithTitle:@"Reset"];
            [a addButtonWithTitle:@"Cancel"];
            if ([a runModal] != NSAlertFirstButtonReturn) return;
            for (NSString* k in @[ @"theme", @"colourIcons", @"tabColours", @"tabNumbers", @"duplicateOpen", @"restorePositions", FPPageColoursKey, @"readingSpeed",
                                   @"highlightColour", @"drawingColour", @"lineWidth", @"pageCache", @"recoveryCopy", FPCheckUpdatesKey ])
                [defaults() removeObjectForKey:k];
            FPApplyTheme();
            [[NSNotificationCenter defaultCenter] postNotificationName:FPPageColoursChanged object:nil];
            [self close];
        })) ]]];
    w.contentView = _tabs;
    [w center];
    return self;
}

@end

void FPShowSettings() {
    static FPSettingsWindow* s = nil;
    if (!s) s = [[FPSettingsWindow alloc] init];
    [s showWindow:nil];
}

// ---- the same file opened again ----

@implementation FPDocumentController

static id fileIdentity(NSURL* u) {
    id ident = nil;
    [u.URLByResolvingSymlinksInPath getResourceValue:&ident forKey:NSURLFileResourceIdentifierKey error:nil];
    return ident;
}

- (void)openDocumentWithContentsOfURL:(NSURL*)url display:(BOOL)display completionHandler:(void (^)(NSDocument*, BOOL, NSError*))done {
    id ident = fileIdentity(url);
    for (NSDocument* d in self.documents) {
        if (!d.fileURL || !ident || ![fileIdentity(d.fileURL) isEqual:ident]) continue;
        if (FPDuplicateMode() == 0 && display) {
            NSAlert* a = [[NSAlert alloc] init];
            a.messageText = [NSString stringWithFormat:@"\u201C%@\u201D is already open.", d.displayName];
            a.informativeText = @"Its tab is shown. (Settings > Tabs: go to its tab without asking.)";
            [a runModal];
        }
        [d showWindows];
        done(d, YES, nil);
        return;
    }
    [super openDocumentWithContentsOfURL:url display:display completionHandler:done];
}

@end

// ---- recovery copies ----

static NSString* recoveryDir() {
    NSString* dir = [[NSString stringWithUTF8String:platform::utf8(platform::appDataDir()).c_str()] stringByAppendingPathComponent:@"recovery"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return dir;
}

static NSString* recoveryName(NSDocument* doc) {
    return [NSString stringWithFormat:@"%p", (__bridge void*)doc];
}

static void writeCopies() {
    for (NSDocument* doc in NSDocumentController.sharedDocumentController.documents) {
        Document* d = [(FPDocument*)doc pdf];
        if (!d || !d->dirty()) continue;
        std::vector<unsigned char> bytes;
        if (!d->serialize(bytes)) continue;
        NSString* base = [recoveryDir() stringByAppendingPathComponent:recoveryName(doc)];
        [[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToFile:[base stringByAppendingPathExtension:@"pdf"] atomically:YES];
        [@{@"path" : doc.fileURL.path ?: @"", @"name" : doc.displayName ?: @"Recovered.pdf"} writeToFile:[base stringByAppendingPathExtension:@"plist"] atomically:YES];
    }
}

void FPRecoveryForget(NSDocument* doc) {
    NSString* base = [recoveryDir() stringByAppendingPathComponent:recoveryName(doc)];
    for (NSString* ext in @[ @"pdf", @"plist" ]) [[NSFileManager defaultManager] removeItemAtPath:[base stringByAppendingPathExtension:ext] error:nil];
}

void FPRecoveryClearAll() {
    NSString* dir = recoveryDir();
    for (NSString* f in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:nil])
        [[NSFileManager defaultManager] removeItemAtPath:[dir stringByAppendingPathComponent:f] error:nil];
}

void FPStartRecovery() {
    // copies left by a crash: offered once, then removed
    NSString* dir = recoveryDir();
    NSMutableArray<NSString*>* found = [NSMutableArray array];
    for (NSString* f in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:nil])
        if ([f.pathExtension isEqualToString:@"pdf"]) [found addObject:[dir stringByAppendingPathComponent:f]];
    if (found.count) {
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = [NSString stringWithFormat:@"Feather PDF closed unexpectedly. %lu document(s) had unsaved changes.", (unsigned long)found.count];
        a.informativeText = @"Open them again with their changes? They open as unsaved documents: save them where you like.";
        [a addButtonWithTitle:@"Open Them"];
        [a addButtonWithTitle:@"Discard"];
        if ([a runModal] == NSAlertFirstButtonReturn)
            for (NSString* f in found) {
                NSData* data = [NSData dataWithContentsOfFile:f];
                NSDictionary* info = [NSDictionary dictionaryWithContentsOfFile:[f.stringByDeletingPathExtension stringByAppendingPathExtension:@"plist"]];
                std::vector<unsigned char> bytes(static_cast<const unsigned char*>(data.bytes), static_cast<const unsigned char*>(data.bytes) + data.length);
                NSString* name = [NSString stringWithFormat:@"%@ (recovered)", [info[@"name"] ?: @"Document" stringByDeletingPathExtension]];
                [FPDocument openNewWithBytes:std::move(bytes) name:[name stringByAppendingPathExtension:@"pdf"]];
            }
        FPRecoveryClearAll();
    }
    static NSTimer* timer = nil;
    static const double every[] = {20, 60, 300, 0};
    void (^schedule)(void) = ^{
        [timer invalidate];
        const double s = every[std::clamp<NSInteger>(intPref(@"recoveryCopy", 0), 0, 3)];
        timer = s > 0 ? [NSTimer scheduledTimerWithTimeInterval:s repeats:YES block:^(NSTimer*) { writeCopies(); }] : nil;
    };
    schedule();
    [[NSNotificationCenter defaultCenter] addObserverForName:FPSettingsChanged object:nil queue:nil usingBlock:^(NSNotification*) { schedule(); }];
}
