#import "AppDelegate.h"
#import "WindowController.h"
#import "StartWindow.h"
#import "Updater.h"

void FPInstallTabMenu();   // TabMenu.mm
void FPAddWorkspacesMenu(NSMenu* fileMenu);   // Workspaces.mm
#import "PageView.h"
#import "NewPdf.h"
#import "Palette.h"
#import "FolderSearch.h"
#import "Settings.h"
#import "Trace.h"
#include "about.hpp"
#include "platform.hpp"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include "buildinfo.h"
#include <cstdio>
#include <cstdlib>
#include "document.hpp"

static NSMutableDictionary<NSNumber*, NSTimer*>* g_timers;   // form field timers (blinking caret, scripts)

@implementation FPAppDelegate

- (void)applicationWillFinishLaunching:(NSNotification*)n {
    FPApplyTheme();
    g_timers = [NSMutableDictionary dictionary];
    // Form fields need timers and pointer shapes from the app (Windows gives them through its timer window).
    Document::FormHost host;
    host.startTimer = [](int id, int ms) -> bool {
        NSTimer* t = [NSTimer scheduledTimerWithTimeInterval:std::max(ms, 10) / 1000.0 repeats:YES block:^(NSTimer*) {
            Document::fireTimer(id);
        }];
        g_timers[@(id)] = t;
        return true;
    };
    host.stopTimer = [](int id) {
        [g_timers[@(id)] invalidate];
        [g_timers removeObjectForKey:@(id)];
    };
    host.cursor = [](int shape) {
        [(shape == 1 ? [NSCursor IBeamCursor] : shape == 2 ? [NSCursor pointingHandCursor] : [NSCursor arrowCursor]) set];
    };
    Document::setFormHost(host);
}

- (void)applicationDidFinishLaunching:(NSNotification*)n {
    FPInstallTabMenu();   // right-click on a tab (TabMenu.mm)
    [NSApp activateIgnoringOtherApps:YES];   // to the front, also when started from Terminal
    // AppKit itself opens the files named on the command line; open here only the ones it has not (so a file is never
    // opened twice, which would ask "already open" at start). After a turn of the run loop, when AppKit's opens are done.
    dispatch_async(dispatch_get_main_queue(), ^{
    for (NSString* f in self.filesToOpen) {
        NSURL* url = [NSURL fileURLWithPath:f];
        if ([[NSDocumentController sharedDocumentController] documentForURL:url]) continue;
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:url display:YES
                                                                     completionHandler:^(NSDocument* doc, BOOL wasOpen, NSError* e) {
                                                                         FPTrace(@"opened %@: %@ (already open %d) %@", f, doc.displayName, wasOpen, e);
                                                                         if (!e) { if (wasOpen) [doc showWindows]; return; }
                                                                         fprintf(stderr, "could not open %s: %s\n", f.UTF8String,
                                                                                 e.localizedDescription.UTF8String);
                                                                         [NSApp presentError:e];
                                                                     }];
    }
    });
    // The build check (FEATHER_UI_CHECK=folder): after a moment, report the windows and keep a picture of the first one.
    if (const char* dir = getenv("FEATHER_UI_CHECK")) {
        NSString* out = [NSString stringWithUTF8String:dir];
        if (self.filesToOpen.count == 0) [[FPStartWindowController shared] showIfNoDocuments];
        // FEATHER_UI_ACTION=showProperties: (a menu command) is chosen after 3 s, so the picture shows what it opens.
        if (const char* action = getenv("FEATHER_UI_ACTION")) {
            NSString* a = [NSString stringWithUTF8String:action];
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                FPTrace(@"action %@: %d", a, [NSApp sendAction:NSSelectorFromString(a) to:nil from:nil]);
            });
        }
        // FEATHER_UI_PRESS=1: a second later, the default button of the form that opened is pressed (as the user would)
        // (FEATHER_UI_PRESS=n: n forms one after another, 4 seconds apart)
        const int presses = getenv("FEATHER_UI_PRESS") ? std::max(1, atoi(getenv("FEATHER_UI_PRESS"))) : 0;
        for (int press = 1; press <= presses; ++press) dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(4 * press) * static_cast<int64_t>(NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            NSWindow* sheet = nil;
            for (NSWindow* w in NSApp.windows) if (w.attachedSheet) { sheet = w.attachedSheet; break; }
            __block NSButton* found = nil;
            __block void (^look)(NSView*) = nil;
            void (^walk)(NSView*) = ^(NSView* v) {
                if ([v isKindOfClass:[NSButton class]] && [((NSButton*)v).keyEquivalent isEqualToString:@"\r"]) found = (NSButton*)v;
                for (NSView* s in v.subviews) if (!found) look(s);
            };
            look = walk;
            if (sheet) look(sheet.contentView);
            FPTrace(@"press: sheet %d button %@", sheet != nil, found.title);
            [found performClick:nil];
            look = nil;
        });
        const char* wait = getenv("FEATHER_UI_WAIT");   // seconds before the picture (default 5)
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (wait ? atoi(wait) : 5) * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            [self uiCheckInto:out];
            exit(0);   // not terminate: a sheet left open (Properties) would hold the quit forever
        });
        return;
    }
    FPStartRecovery();   // offers what a crash left, then keeps copies of unsaved changes
    // A quiet look for a new version (it asks only when there is one), as the Windows app does.
    if ([self automaticUpdates]) dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ FPCheckForUpdates(NO); });
    // Started with nothing to open (and no windows brought back from last time): the start window, as on Windows.
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.filesToOpen.count == 0) [[FPStartWindowController shared] showIfNoDocuments];
    });
}

// Writes report.txt (documents, windows, tabs, title, subtitle) and window.png, the first document window as drawn (the
// app draws it into a picture itself, so no screen-recording permission is needed).
- (void)uiCheckInto:(NSString*)dir {
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSMutableString* r = [NSMutableString string];
    NSArray<NSDocument*>* docs = [NSDocumentController sharedDocumentController].documents;
    [r appendFormat:@"documents %lu\n", (unsigned long)docs.count];
    NSWindow* first = NSApp.keyWindow.windowController.document ? NSApp.keyWindow : nil;   // the window in front first
    for (NSDocument* d in docs) [r appendFormat:@"document %@ windows %lu\n", d.displayName, (unsigned long)d.windowControllers.count];
    for (NSWindow* w in NSApp.windows) {
        [r appendFormat:@"window %@ \"%@\" subtitle \"%@\" size %.0fx%.0f at %.0f,%.0f visible %d tabs %lu key %d doc %d\n",
                        NSStringFromClass([w class]), w.title, [w respondsToSelector:@selector(subtitle)] ? w.subtitle : @"",
                        w.frame.size.width, w.frame.size.height, w.frame.origin.x, w.frame.origin.y, w.isVisible,
                        (unsigned long)(w.tabbedWindows.count ?: 1), w.isKeyWindow, w.windowController.document != nil];
        if (!first && w.windowController.document) first = w;
    }
    [r appendFormat:@"active %d screens %lu\n", NSApp.isActive, (unsigned long)NSScreen.screens.count];
    [r writeToFile:[dir stringByAppendingPathComponent:@"report.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    if (!first) for (NSWindow* w in NSApp.windows) if (w.isVisible) { first = w; break; }   // the start window
    NSView* v = first.contentView.superview ?: first.contentView;   // with the title bar and toolbar
    if (!v) return;
    NSBitmapImageRep* rep = [v bitmapImageRepForCachingDisplayInRect:v.bounds];
    [v cacheDisplayInRect:v.bounds toBitmapImageRep:rep];
    [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[dir stringByAppendingPathComponent:@"window.png"]
                                                                            atomically:YES];
    if (NSWindow* sheet = first.attachedSheet) {   // a form or message on the window
        NSView* sv = sheet.contentView.superview ?: sheet.contentView;
        NSBitmapImageRep* sr = [sv bitmapImageRepForCachingDisplayInRect:sv.bounds];
        [sv cacheDisplayInRect:sv.bounds toBitmapImageRep:sr];
        [[sr representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:[dir stringByAppendingPathComponent:@"sheet.png"]
                                                                               atomically:YES];
    }
}

// No empty documents: Feather PDF opens files.
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication*)sender { return NO; }
- (BOOL)applicationOpenUntitledFile:(NSApplication*)sender { return NO; }

- (BOOL)applicationShouldHandleReopen:(NSApplication*)app hasVisibleWindows:(BOOL)visible {
    if (!visible) [[FPStartWindowController shared] showIfNoDocuments];   // the Dock icon clicked with no window open
    return NO;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app { return YES; }

- (void)applicationWillTerminate:(NSNotification*)n { FPRecoveryClearAll(); }   // a normal quit leaves nothing to recover

- (IBAction)showSettings:(id)sender { FPShowSettings(); }

// File > New Window (⇧⌘N): the PDFs chosen open in windows of their own, not as tabs (to see two at once, or one on each
// screen). A window can still become a tab later (Window > Merge All Windows).
- (IBAction)newWindow:(id)sender {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.allowsMultipleSelection = YES;
    p.title = @"Open in a new window";
    p.prompt = @"Open in New Window";
    if ([p runModal] != NSModalResponseOK) return;
    FPOpenNextInOwnWindows(static_cast<NSInteger>(p.URLs.count));
    for (NSURL* u in p.URLs)
        [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:u display:YES completionHandler:^(NSDocument* doc, BOOL wasOpen, NSError* e) {
            if (e) [NSApp presentError:e];
            if (wasOpen) [doc showWindows];
        }];
}

// ---- reading colours: one setting, applied to every window at once ----

- (IBAction)setPageColours:(NSMenuItem*)sender {
    [[NSUserDefaults standardUserDefaults] setInteger:sender.tag forKey:FPPageColoursKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageColoursChanged object:nil];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    if (item.action == @selector(toggleAutomaticUpdates:)) item.state = [self automaticUpdates] ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(setPageColours:))
        item.state = [[NSUserDefaults standardUserDefaults] integerForKey:FPPageColoursKey] == item.tag ? NSControlStateValueOn
                                                                                                       : NSControlStateValueOff;
    return YES;
}

- (IBAction)showStartPage:(id)sender {
    [[FPStartWindowController shared] refresh];
    [[FPStartWindowController shared] showWindow:nil];
}

- (IBAction)searchFolder:(id)sender { FPSearchFolder(NSApp.mainWindow.windowController.document ? NSApp.mainWindow : nil); }
- (IBAction)showCommandPalette:(id)sender { FPShowCommandPalette(NSApp.mainWindow); }
- (IBAction)setAsDefaultApp:(id)sender { FPSetAsDefaultPdfApp(NSApp.mainWindow); }

// ---- new documents ----

- (IBAction)newPdfFromPictures:(id)sender { [FPNewPdf fromPictures]; }
- (IBAction)newPdfFromText:(id)sender { [FPNewPdf fromTextFile]; }

// ---- updates ----

- (IBAction)checkForUpdates:(id)sender { FPCheckForUpdates(YES); }

- (IBAction)toggleAutomaticUpdates:(id)sender {
    NSUserDefaults* d = NSUserDefaults.standardUserDefaults;
    [d setBool:![self automaticUpdates] forKey:FPCheckUpdatesKey];
}

- (BOOL)automaticUpdates {
    NSNumber* v = [NSUserDefaults.standardUserDefaults objectForKey:FPCheckUpdatesKey];
    return v ? v.boolValue : YES;
}

// ---- help and About ----

- (IBAction)showUserGuide:(id)sender {
    NSURL* guide = [[NSBundle mainBundle] URLForResource:@"Feather PDF User Guide" withExtension:@"pdf"];
    if (guide)
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:guide display:YES
                                                                     completionHandler:^(NSDocument*, BOOL, NSError*) {}];
}

// The About window, with the Windows app's words, version and build date.
- (IBAction)showAbout:(id)sender {
    // the same words as the Windows app (shared/about.hpp); the e-mail address is a link
    const std::wstring text = about::body(L"macOS", L"your Mac") + L"\nPDF rendering by PDFium; text recognition by Apple Vision.\n\n"
                              L"Built with care for a faster, simpler document experience.";
    NSString* credits = [NSString stringWithUTF8String:platform::utf8(text).c_str()];
    NSMutableParagraphStyle* centred = [[NSMutableParagraphStyle alloc] init];
    centred.alignment = NSTextAlignmentCenter;
    NSMutableAttributedString* rich = [[NSMutableAttributedString alloc] initWithString:credits attributes:@{
        NSFontAttributeName : [NSFont systemFontOfSize:11], NSForegroundColorAttributeName : NSColor.labelColor, NSParagraphStyleAttributeName : centred}];
    for (NSString* head in @[ @"DEVELOPMENT HISTORY", @"DEVELOPMENT AND AI CONTRIBUTIONS", @"CREDITS AND ACKNOWLEDGEMENTS", @"FEEDBACK AND SUGGESTIONS" ]) {
        const NSRange r = [credits rangeOfString:head];
        if (r.location != NSNotFound) [rich addAttribute:NSFontAttributeName value:[NSFont boldSystemFontOfSize:11] range:r];
    }
    NSString* mail = [NSString stringWithUTF8String:platform::utf8(about::kFeedbackAddress).c_str()];
    const NSRange m = [credits rangeOfString:mail];
    if (m.location != NSNotFound)
        [rich addAttribute:NSLinkAttributeName value:[NSURL URLWithString:[NSString stringWithFormat:@"mailto:%@?subject=Feather%%20PDF%%20%s", mail, FEATHER_VERSION]] range:m];
    [NSApp orderFrontStandardAboutPanelWithOptions:@{
        NSAboutPanelOptionApplicationName : @"Feather PDF",
        NSAboutPanelOptionApplicationVersion : [NSString stringWithFormat:@"Version %s \u201C%s\u201D, built %@", FEATHER_VERSION, FEATHER_CODENAME, [NSString stringWithUTF8String:FEATHER_BUILD_DATE_LONG]],
        NSAboutPanelOptionVersion : @"",
        NSAboutPanelOptionCredits : rich
    }];
}

// ---- the menus ----

static NSMenuItem* add(NSMenu* m, NSString* title, SEL action, NSString* key, NSEventModifierFlags mods = NSEventModifierFlagCommand) {
    NSMenuItem* i = [m addItemWithTitle:title action:action keyEquivalent:key ?: @""];
    i.keyEquivalentModifierMask = mods;
    return i;
}

static NSMenu* submenu(NSMenu* bar, NSString* title) {
    NSMenuItem* top = [bar addItemWithTitle:title action:nil keyEquivalent:@""];
    NSMenu* m = [[NSMenu alloc] initWithTitle:title];
    top.submenu = m;
    return m;
}

- (NSMenu*)buildMainMenu {
    NSMenu* bar = [[NSMenu alloc] init];
    NSString* app = @"Feather PDF";

    NSMenu* m = submenu(bar, app);
    add(m, [@"About " stringByAppendingString:app], @selector(showAbout:), nil).target = self;
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem* services = add(m, @"Services", nil, nil);
    services.submenu = [[NSMenu alloc] init];
    NSApp.servicesMenu = services.submenu;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Settings\u2026", @selector(showSettings:), @",").target = self;
    add(m, @"Set as Default PDF App\u2026", @selector(setAsDefaultApp:), nil).target = self;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, [@"Hide " stringByAppendingString:app], @selector(hide:), @"h");
    add(m, @"Hide Others", @selector(hideOtherApplications:), @"h", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Show All", @selector(unhideAllApplications:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, [@"Quit " stringByAppendingString:app], @selector(terminate:), @"q");

    m = submenu(bar, @"File");
    add(m, @"Open…", @selector(openDocument:), @"o");
    NSMenuItem* recent = add(m, @"Open Recent", nil, nil);
    recent.submenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
    add(recent.submenu, @"Clear Menu", @selector(clearRecentDocuments:), nil);   // macOS fills this menu itself
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"New Window\u2026", @selector(newWindow:), @"n", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    FPAddWorkspacesMenu(m);
    add(m, @"Close", @selector(performClose:), @"w");
    add(m, @"Open Directory Having This File", @selector(showFileInFolder:), @"r", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Print\u2026", @selector(printDocument:), @"p");
    add(m, @"Save", @selector(saveDocument:), @"s");
    add(m, @"Save As\u2026", @selector(saveDocumentAs:), @"s", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    add(m, @"Revert to Saved", @selector(revertDocumentToSaved:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Export\u2026", @selector(exportDocument:), @"e", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    add(m, @"Save a PDF/A Copy\u2026", @selector(savePdfA:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Properties\u2026", @selector(showProperties:), @"i", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Attached Files\u2026", @selector(showAttachments:), nil);
    add(m, @"Save a Copy for the Web (Fast Web View)\u2026", @selector(saveForWeb:), nil);
    add(m, @"Repair a Damaged PDF\u2026", @selector(repairDocument:), nil);
    add(m, @"Layers (Show or Hide)\u2026", @selector(showLayers:), nil);
    add(m, @"Fonts Used\u2026", @selector(showFonts:), nil);
    add(m, @"Accessibility: Check and Fix\u2026", @selector(accessibilityCheck:), nil);

    m = submenu(bar, @"Edit");
    add(m, @"Undo", @selector(featherUndo:), @"z");
    add(m, @"Redo", @selector(featherRedo:), @"z", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Cut", @selector(cut:), @"x");
    add(m, @"Copy", @selector(copy:), @"c");
    add(m, @"Paste", @selector(paste:), @"v");
    add(m, @"Select All", @selector(selectAll:), @"a");
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Find\u2026", @selector(performFindPanelAction:), @"f");
    add(m, @"Find Next", @selector(findNextMatch:), @"g");
    add(m, @"Find Previous", @selector(findPreviousMatch:), @"g", NSEventModifierFlagCommand | NSEventModifierFlagShift);

    m = submenu(bar, @"Annotate");
    add(m, @"Select Text", @selector(selectTextTool:), @"1", NSEventModifierFlagCommand | NSEventModifierFlagControl);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Highlight", @selector(highlightSelection:), @"h", NSEventModifierFlagCommand | NSEventModifierFlagControl);
    add(m, @"Underline", @selector(underlineSelection:), @"u", NSEventModifierFlagCommand | NSEventModifierFlagControl);
    add(m, @"Strikethrough", @selector(strikeSelection:), @"s", NSEventModifierFlagCommand | NSEventModifierFlagControl);
    add(m, @"Squiggly Underline", @selector(squigglySelection:), nil);
    add(m, @"Comment on Selected Text\u2026", @selector(commentOnSelection:), @"m", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    [m addItem:[NSMenuItem separatorItem]];
    struct { NSString* title; FPTool tool; } tools[] = {
        {@"Add Comment (note)", FPToolNote}, {@"Rectangle", FPToolRectangle}, {@"Ellipse", FPToolEllipse}, {@"Line", FPToolLine},
        {@"Arrow", FPToolArrow}, {@"Pen (freehand)", FPToolPen}, {@"Polygon", FPToolPolygon}, {@"Cloud", FPToolCloud}, {@"Callout", FPToolCallout}};
    for (const auto& t : tools) add(m, t.title, @selector(chooseTool:), nil).tag = t.tool;
    NSMenuItem* stamp = add(m, @"Stamp", nil, nil);
    stamp.submenu = [[NSMenu alloc] initWithTitle:@"Stamp"];
    NSString* stamps[] = {@"APPROVED", @"FINAL", @"DRAFT", @"CONFIDENTIAL", @"NOT APPROVED", @"FOR COMMENT", @"REVIEWED"};
    for (int i = 0; i < 7; ++i) add(stamp.submenu, stamps[i], @selector(chooseStamp:), nil).tag = i;
    [stamp.submenu addItem:[NSMenuItem separatorItem]];
    NSString* dated[] = {@"Date and Time", @"APPROVED with Date", @"RECEIVED with Date", @"PAID with Date"};
    for (int i = 0; i < 4; ++i) add(stamp.submenu, dated[i], @selector(chooseStamp:), nil).tag = 10 + i;
    [stamp.submenu addItem:[NSMenuItem separatorItem]];
    add(stamp.submenu, @"Custom Stamp\u2026", @selector(chooseStamp:), nil).tag = 20;
    [m addItem:[NSMenuItem separatorItem]];
    NSString* colours[] = {@"Yellow", @"Green", @"Blue", @"Pink", @"Orange", @"Red", @"Black"};
    NSMenuItem* dc = add(m, @"Colour", nil, nil);
    dc.submenu = [[NSMenu alloc] initWithTitle:@"Colour"];
    for (int i = 0; i < 7; ++i) add(dc.submenu, colours[i], @selector(setDrawingColour:), nil).tag = i;
    NSMenuItem* hc = add(m, @"Highlight Colour", nil, nil);
    hc.submenu = [[NSMenu alloc] initWithTitle:@"Highlight Colour"];
    for (int i = 0; i < 7; ++i) add(hc.submenu, colours[i], @selector(setHighlightColour:), nil).tag = i;
    NSMenuItem* lw = add(m, @"Line Width", nil, nil);
    lw.submenu = [[NSMenu alloc] initWithTitle:@"Line Width"];
    NSString* widths[] = {@"Thin (1 pt)", @"Medium (2 pt)", @"Thick (4 pt)"};
    for (int i = 0; i < 3; ++i) add(lw.submenu, widths[i], @selector(setLineWidth:), nil).tag = i;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Edit Comment\u2026", @selector(editSelectedComment:), nil);
    add(m, @"Delete Selected Comment", @selector(deleteSelectedAnnotation:), nil);
    add(m, @"Notes Summary (highlights and comments)\u2026", @selector(notesSummary:), nil);
    add(m, @"Comments and Review: Reply, Accept, Reject, Summary\u2026", @selector(reviewComments:), nil);
    add(m, @"Flatten: Make Comments and Fields Part of the Page\u2026", @selector(flattenComments:), nil);

    m = submenu(bar, @"Edit Text");
    add(m, @"Edit a Line of Text", @selector(chooseEditTool:), @"e").tag = FPToolEditText;
    add(m, @"Edit a Paragraph", @selector(chooseEditTool:), @"e", NSEventModifierFlagCommand | NSEventModifierFlagOption).tag = FPToolEditPara;
    add(m, @"Find and Replace Text\u2026", @selector(findAndReplace:), @"f", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Add a Text Box", @selector(chooseEditTool:), @"t", NSEventModifierFlagCommand | NSEventModifierFlagOption).tag = FPToolTextBox;
    add(m, @"Style of New Text\u2026", @selector(textStyleDialog:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Insert a Picture\u2026", @selector(insertPicture:), nil);
    add(m, @"Paste a Picture", @selector(pastePicture:), @"v", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Select an Object (picture, paragraph, table)", @selector(chooseEditTool:), nil).tag = FPToolSelectObject;
    NSMenuItem* pic = add(m, @"Selected Object", nil, nil);
    pic.submenu = [[NSMenu alloc] initWithTitle:@"Selected Object"];
    add(pic.submenu, @"Turn Right", @selector(objectRotateRight:), nil);
    add(pic.submenu, @"Turn Left", @selector(objectRotateLeft:), nil);
    add(pic.submenu, @"Flip Left-Right", @selector(objectFlipHorizontal:), nil);
    add(pic.submenu, @"Flip Upside Down", @selector(objectFlipVertical:), nil);
    [pic.submenu addItem:[NSMenuItem separatorItem]];
    add(pic.submenu, @"Bring to Front", @selector(objectToFront:), nil);
    add(pic.submenu, @"Send to Back", @selector(objectToBack:), nil);
    add(pic.submenu, @"Delete", @selector(objectDelete:), nil);
    [pic.submenu addItem:[NSMenuItem separatorItem]];
    add(pic.submenu, @"Crop the Picture", @selector(pictureCrop:), nil);
    add(pic.submenu, @"Brightness and Contrast\u2026", @selector(pictureAdjust:), nil);
    add(pic.submenu, @"Black and White", @selector(pictureBlackAndWhite:), nil);
    add(pic.submenu, @"Replace the Picture\u2026", @selector(pictureReplace:), nil);
    add(pic.submenu, @"Save the Picture as PNG\u2026", @selector(pictureSave:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Edit a Table", @selector(chooseEditTool:), nil).tag = FPToolEditTable;
    add(m, @"Add a Table\u2026", @selector(addNewTable:), nil);

    m = submenu(bar, @"Forms");
    struct { NSString* title; FPTool tool; } fields[] = {{@"Add a Text Field", FPToolFormText}, {@"Add a Check Box", FPToolFormCheck},
        {@"Add a Drop-Down List", FPToolFormCombo}, {@"Add Radio Buttons (one choice of several)", FPToolFormRadio},
        {@"Add a List Box", FPToolFormList}, {@"Add a Date Field", FPToolFormDate}};
    for (const auto& f : fields) add(m, f.title, @selector(chooseFormTool:), nil).tag = f.tool;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Find Fields in a Flat Form", @selector(findFormFields:), nil);
    add(m, @"Fill from a Spreadsheet (CSV)\u2026", @selector(fillFromCsv:), nil);
    add(m, @"Export the Answers to Excel or CSV\u2026", @selector(exportFormAnswers:), nil);
    add(m, @"Calculations and Checks (Totals, Numbers, Dates, E-mail)\u2026", @selector(formCalcDialog:), nil);
    add(m, @"Clear All Answers (Reset the Form)\u2026", @selector(resetForm:), nil);
    add(m, @"Flatten: Make Fields and Comments Part of the Page\u2026", @selector(flattenComments:), nil);

    m = submenu(bar, @"Edit PDF");
    add(m, @"Watermark\u2026", @selector(addWatermark:), nil);
    add(m, @"Page Numbers\u2026", @selector(addPageNumbers:), nil);
    add(m, @"Headers and Footers\u2026", @selector(addHeadersFooters:), nil);
    add(m, @"Delete Header and Footer\u2026", @selector(deleteHeaderFooter:), nil);
    add(m, @"Background Colour\u2026", @selector(addBackground:), nil);

    m = submenu(bar, @"Pages");
    add(m, @"Rotate Right", @selector(rotateRight:), @"r");
    add(m, @"Rotate Left", @selector(rotateLeft:), @"l");
    add(m, @"Rotate Pages\u2026", @selector(rotatePages:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Insert Blank Page", @selector(insertBlankPage:), nil);
    add(m, @"Insert Pages from a File\u2026", @selector(insertPagesFromFile:), nil);
    add(m, @"Select All Pages", @selector(selectAllPages:), nil);
    add(m, @"Delete Selected Pages", @selector(deleteSelectedPages:), [NSString stringWithFormat:@"%C", (unichar)NSBackspaceCharacter]);
    add(m, @"Delete Pages\u2026", @selector(deletePages:), nil);
    add(m, @"Extract Pages\u2026", @selector(extractPages:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Move Pages\u2026", @selector(movePages:), nil);
    add(m, @"Crop Pages\u2026", @selector(cropPages:), nil);
    add(m, @"Crop by Drawing a Box", @selector(cropByBox:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Merge: Add Files at the End (Keeping Bookmarks)\u2026", @selector(mergeFiles:), nil);
    add(m, @"Split into Several Files (Pages, Bookmarks, Size)\u2026", @selector(splitDocument:), nil);
    add(m, @"Copy Page as Picture", @selector(copyPageAsPicture:), @"c", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Reverse the Order", @selector(reversePages:), nil);
    add(m, @"Two-Sided Scan: Put Fronts and Backs in Order\u2026", @selector(interleaveScan:), nil);
    add(m, @"Remove Blank Pages\u2026", @selector(removeBlankPages:), nil);
    add(m, @"Remove Duplicate Pages\u2026", @selector(removeDuplicatePages:), nil);

    m = submenu(bar, @"Protect");
    add(m, @"Compress\u2026", @selector(compressDocument:), nil);
    add(m, @"Password Protection\u2026", @selector(protectDocument:), nil);
    add(m, @"Remove Password Protection", @selector(removeProtection:), nil);
    add(m, @"Remove Hidden Data\u2026", @selector(removeHiddenData:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem* red = add(m, @"Redact", nil, nil);
    red.submenu = [[NSMenu alloc] initWithTitle:@"Redact"];
    add(red.submenu, @"Mark Areas to Redact", @selector(chooseTool:), nil).tag = FPToolRedact;
    add(red.submenu, @"Mark Selected Text", @selector(markSelectionForRedaction:), nil);
    add(red.submenu, @"Find and Mark\u2026", @selector(findAndMarkRedaction:), nil);
    add(red.submenu, @"Find Sensitive Information (E-mail, Phone, Aadhaar, PAN, Bank)\u2026", @selector(findSensitiveInformation:), nil);
    add(red.submenu, @"Apply Redactions\u2026", @selector(applyRedactions:), nil);
    add(red.submenu, @"Clear Marks", @selector(clearRedactionMarks:), nil);
    add(red.submenu, @"Check That the Text Is Gone\u2026", @selector(checkRedaction:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem* sigs = add(m, @"Digital Signatures", nil, nil);
    sigs.submenu = [[NSMenu alloc] initWithTitle:@"Digital Signatures"];
    add(sigs.submenu, @"Sign with a Certificate\u2026", @selector(signDocument:), nil);
    add(sigs.submenu, @"Make My Own Certificate\u2026", @selector(makeOwnCertificate:), nil);
    add(sigs.submenu, @"Show Signatures", @selector(showSignatures:), nil);
    NSMenuItem* hand = add(m, @"Handwritten Signature", nil, nil);
    hand.submenu = [[NSMenu alloc] initWithTitle:@"Handwritten Signature"];
    add(hand.submenu, @"Type My Name\u2026", @selector(signatureTyped:), nil);
    add(hand.submenu, @"Draw My Signature\u2026", @selector(signatureDrawn:), nil);
    add(hand.submenu, @"Use a Picture of My Signature\u2026", @selector(signaturePicture:), nil);
    add(hand.submenu, @"Use My Saved Signature", @selector(signatureSaved:), nil);

    m = submenu(bar, @"View");
    add(m, @"Show Sidebar", @selector(toggleThumbnails:), @"b");
    add(m, @"Bookmarks", @selector(showBookmarksPanel:), @"b", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Actual Size", @selector(actualSize:), @"0");
    add(m, @"Zoom In", @selector(zoomIn:), @"=");
    add(m, @"Zoom Out", @selector(zoomOut:), @"-");
    add(m, @"Fit Width", @selector(fitWidth:), @"9");
    add(m, @"Fit Page", @selector(fitPage:), @"8");
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Single Page", @selector(layoutSingle:), @"1", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Continuous", @selector(layoutContinuous:), @"2", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Two Pages", @selector(layoutTwoPages:), @"3", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Show Cover Page Separately", @selector(toggleCover:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem* pageColours = add(m, @"Page Colours", nil, nil);
    pageColours.submenu = [[NSMenu alloc] initWithTitle:@"Page Colours"];
    NSString* names[] = {@"Normal", @"Dark (night mode)", @"Dimmed (softer white)", @"Blackboard: Inverted", @"Blackboard: Original Colours"};
    for (int i = 0; i < 5; ++i) {
        NSMenuItem* c = add(pageColours.submenu, names[i], @selector(setPageColours:), nil);
        c.tag = i;
        c.target = self;
    }
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem* sbs = add(m, @"Side by Side", nil, nil);
    sbs.submenu = [[NSMenu alloc] initWithTitle:@"Side by Side"];
    add(sbs.submenu, @"Show Another Open Document Next to This One\u2026", @selector(sideBySideWithTab:), nil);
    add(sbs.submenu, @"Open a File Next to This One\u2026", @selector(sideBySideWithFile:), nil);
    add(sbs.submenu, @"Back to One Document", @selector(endSideBySide:), nil);
    [sbs.submenu addItem:[NSMenuItem separatorItem]];
    add(sbs.submenu, @"Link Scrolling (Both Scroll and Zoom Together)", @selector(toggleSideBySideLink:), nil);
    add(sbs.submenu, @"Left and Right", @selector(sideBySideLeftRight:), nil);
    add(sbs.submenu, @"Top and Bottom", @selector(sideBySideTopBottom:), nil);
    add(sbs.submenu, @"Swap the Two Sides", @selector(swapSideBySide:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Show Tab Bar", @selector(toggleTabBar:), nil);
    add(m, @"Enter Full Screen", @selector(toggleFullScreen:), @"f", NSEventModifierFlagCommand | NSEventModifierFlagControl);
    add(m, @"Presentation", @selector(startPresentation:), @"p", NSEventModifierFlagCommand | NSEventModifierFlagOption);

    m = submenu(bar, @"Go");
    add(m, @"Previous Page", @selector(previousPage:), [NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey],
        NSEventModifierFlagOption | NSEventModifierFlagCommand);
    add(m, @"Next Page", @selector(nextPage:), [NSString stringWithFormat:@"%C", (unichar)NSDownArrowFunctionKey],
        NSEventModifierFlagOption | NSEventModifierFlagCommand);
    add(m, @"First Page", @selector(firstPage:), [NSString stringWithFormat:@"%C", (unichar)NSHomeFunctionKey], 0);
    add(m, @"Last Page", @selector(lastPage:), [NSString stringWithFormat:@"%C", (unichar)NSEndFunctionKey], 0);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Go to Page…", @selector(goToPage:), @"g", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Back", @selector(goBack:), @"[");
    add(m, @"Forward", @selector(goForward:), @"]");

    m = submenu(bar, @"Tools");
    add(m, @"Command Palette\u2026", @selector(showCommandPalette:), @"k").target = self;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Recognise Text (OCR)\u2026", @selector(recogniseText:), nil);
    add(m, @"Batch: Process a Whole Folder\u2026", @selector(batchFolder:), nil);
    NSMenuItem* measure = add(m, @"Measure", nil, nil);
    measure.submenu = [[NSMenu alloc] initWithTitle:@"Measure"];
    add(measure.submenu, @"Ruler (Measure a Distance)", @selector(chooseRuler:), nil);
    add(measure.submenu, @"Protractor (Measure an Angle)", @selector(chooseProtractor:), nil);
    add(measure.submenu, @"Show Rulers on the Page", @selector(toggleRulers:), nil);
    [measure.submenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* units = add(measure.submenu, @"Units", nil, nil);
    units.submenu = [[NSMenu alloc] initWithTitle:@"Units"];
    NSString* unitNames[] = {@"Millimetres (mm)", @"Centimetres (cm)", @"Inches (in)", @"Points (pt)"};
    for (int i = 0; i < 4; ++i) add(units.submenu, unitNames[i], @selector(setMeasureUnits:), nil).tag = i;
    NSMenuItem* scale = add(measure.submenu, @"Drawing Scale", nil, nil);
    scale.submenu = [[NSMenu alloc] initWithTitle:@"Drawing Scale"];
    NSString* scaleNames[] = {@"1 : 1 (Actual Size)", @"1 : 2", @"1 : 5", @"1 : 10", @"1 : 20", @"1 : 50", @"1 : 100", @"2 : 1", @"5 : 1", @"10 : 1"};
    for (int i = 0; i < 10; ++i) add(scale.submenu, scaleNames[i], @selector(setMeasureScale:), nil).tag = i;
    [measure.submenu addItem:[NSMenuItem separatorItem]];
    add(measure.submenu, @"Keep the Measurements on the Page", @selector(keepMeasurements:), nil);
    add(measure.submenu, @"Clear the Measurements", @selector(clearMeasurements:), nil);
    add(m, @"Compare with Another Document\u2026", @selector(compareDocuments:), nil);
    add(m, @"Search All My PDFs in a Folder\u2026", @selector(searchFolder:), @"f", NSEventModifierFlagCommand | NSEventModifierFlagShift).target = self;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Read Aloud", @selector(readAloud:), nil);
    add(m, @"Stop Reading", @selector(stopReading:), @".");
    NSMenuItem* rs = add(m, @"Reading Speed", nil, nil);
    rs.submenu = [[NSMenu alloc] initWithTitle:@"Reading Speed"];
    NSString* speeds[] = {@"Slow", @"Normal", @"Fast", @"Very Fast"};
    for (int i = 0; i < 4; ++i) add(rs.submenu, speeds[i], @selector(setReadingSpeed:), nil).tag = i;

    m = submenu(bar, @"Window");
    add(m, @"Minimize", @selector(performMiniaturize:), @"m");
    add(m, @"Zoom", @selector(performZoom:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Undo History", @selector(showUndoHistory:), @"y", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Bring All to Front", @selector(arrangeInFront:), nil);
    add(m, @"Start Page", @selector(showStartPage:), @"1", NSEventModifierFlagCommand | NSEventModifierFlagShift).target = self;
    NSApp.windowsMenu = m;

    m = submenu(bar, @"Help");
    add(m, @"Feather PDF User Guide", @selector(showUserGuide:), @"?").target = self;
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"About Feather PDF", @selector(showAbout:), nil).target = self;   // also here, where the Windows app has it
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Update to New Version\u2026", @selector(checkForUpdates:), nil).target = self;
    add(m, @"Check for Updates Automatically", @selector(toggleAutomaticUpdates:), nil).target = self;
    NSApp.helpMenu = m;
    return bar;
}

@end
