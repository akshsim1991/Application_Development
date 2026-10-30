#import "AppDelegate.h"
#import "WindowController.h"
#import "StartWindow.h"
#import "Trace.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "document.hpp"

static NSMutableDictionary<NSNumber*, NSTimer*>* g_timers;   // form field timers (blinking caret, scripts)

@implementation FPAppDelegate

- (void)applicationWillFinishLaunching:(NSNotification*)n {
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
    [NSApp activateIgnoringOtherApps:YES];   // to the front, also when started from Terminal
    for (NSString* f in self.filesToOpen)
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:[NSURL fileURLWithPath:f] display:YES
                                                                     completionHandler:^(NSDocument* doc, BOOL wasOpen, NSError* e) {
                                                                         FPTrace(@"opened %@: %@ (already open %d) %@", f, doc.displayName, wasOpen, e);
                                                                         if (!e) { if (wasOpen) [doc showWindows]; return; }
                                                                         fprintf(stderr, "could not open %s: %s\n", f.UTF8String,
                                                                                 e.localizedDescription.UTF8String);
                                                                         [NSApp presentError:e];
                                                                     }];
    // The build check (FEATHER_UI_CHECK=folder): after a moment, report the windows and keep a picture of the first one.
    if (const char* dir = getenv("FEATHER_UI_CHECK")) {
        NSString* out = [NSString stringWithUTF8String:dir];
        if (self.filesToOpen.count == 0) [[FPStartWindowController shared] showIfNoDocuments];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 4 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            [self uiCheckInto:out];
            [NSApp terminate:nil];
        });
        return;
    }
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
    NSWindow* first = nil;
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
}

// No empty documents: Feather PDF opens files.
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication*)sender { return NO; }
- (BOOL)applicationOpenUntitledFile:(NSApplication*)sender { return NO; }

- (BOOL)applicationShouldHandleReopen:(NSApplication*)app hasVisibleWindows:(BOOL)visible {
    if (!visible) [[FPStartWindowController shared] showIfNoDocuments];   // the Dock icon clicked with no window open
    return NO;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app { return YES; }

// ---- reading colours: one setting, applied to every window at once ----

- (IBAction)setPageColours:(NSMenuItem*)sender {
    [[NSUserDefaults standardUserDefaults] setInteger:sender.tag forKey:FPPageColoursKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:FPPageColoursChanged object:nil];
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    if (item.action == @selector(setPageColours:))
        item.state = [[NSUserDefaults standardUserDefaults] integerForKey:FPPageColoursKey] == item.tag ? NSControlStateValueOn
                                                                                                       : NSControlStateValueOff;
    return YES;
}

- (IBAction)showStartPage:(id)sender {
    [[FPStartWindowController shared] refresh];
    [[FPStartWindowController shared] showWindow:nil];
}

// ---- help and About ----

- (IBAction)showUserGuide:(id)sender {
    NSURL* guide = [[NSBundle mainBundle] URLForResource:@"Feather PDF User Guide" withExtension:@"pdf"];
    if (guide)
        [[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:guide display:YES
                                                                     completionHandler:^(NSDocument*, BOOL, NSError*) {}];
}

- (IBAction)showAbout:(id)sender {
    NSString* credits = @"A fast, lightweight PDF reader and editor. Everything happens on your Mac, with no account, cloud "
                        @"service or tracking.\n\nCreated by: Akshaya Simha\nFree to use and share, always.\n"
                        @"PDF rendering by PDFium.\n\nThis is an early Mac version: reading only for now. "
                        @"Editing comes in the next stages.";
    [NSApp orderFrontStandardAboutPanelWithOptions:@{
        NSAboutPanelOptionCredits : [[NSAttributedString alloc] initWithString:credits attributes:@{
            NSFontAttributeName : [NSFont systemFontOfSize:11], NSForegroundColorAttributeName : [NSColor labelColor]
        }]
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
    add(m, @"Close", @selector(performClose:), @"w");
    add(m, @"Save", @selector(saveDocument:), @"s");
    add(m, @"Save As\u2026", @selector(saveDocumentAs:), @"s", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    add(m, @"Revert to Saved", @selector(revertDocumentToSaved:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Export\u2026", @selector(exportDocument:), @"e", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    add(m, @"Save a PDF/A Copy\u2026", @selector(savePdfA:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Properties\u2026", @selector(showProperties:), @"i", NSEventModifierFlagCommand | NSEventModifierFlagOption);
    add(m, @"Fonts Used\u2026", @selector(showFonts:), nil);

    m = submenu(bar, @"Edit");
    add(m, @"Undo", @selector(featherUndo:), @"z");
    add(m, @"Redo", @selector(featherRedo:), @"z", NSEventModifierFlagCommand | NSEventModifierFlagShift);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Cut", @selector(cut:), @"x");
    add(m, @"Copy", @selector(copy:), @"c");
    add(m, @"Paste", @selector(paste:), @"v");
    add(m, @"Select All", @selector(selectAll:), @"a");

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
    add(m, @"Delete Pages\u2026", @selector(deletePages:), nil);
    add(m, @"Extract Pages\u2026", @selector(extractPages:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Reverse the Order", @selector(reversePages:), nil);
    add(m, @"Remove Blank Pages\u2026", @selector(removeBlankPages:), nil);
    add(m, @"Remove Duplicate Pages\u2026", @selector(removeDuplicatePages:), nil);

    m = submenu(bar, @"Protect");
    add(m, @"Compress\u2026", @selector(compressDocument:), nil);
    add(m, @"Password Protection\u2026", @selector(protectDocument:), nil);
    add(m, @"Remove Password Protection", @selector(removeProtection:), nil);
    add(m, @"Remove Hidden Data\u2026", @selector(removeHiddenData:), nil);

    m = submenu(bar, @"View");
    add(m, @"Actual Size", @selector(actualSize:), @"0");
    add(m, @"Zoom In", @selector(zoomIn:), @"=");
    add(m, @"Zoom Out", @selector(zoomOut:), @"-");
    add(m, @"Fit Width", @selector(fitWidth:), @"9");
    [m addItem:[NSMenuItem separatorItem]];
    NSMenuItem* colours = add(m, @"Page Colours", nil, nil);
    colours.submenu = [[NSMenu alloc] initWithTitle:@"Page Colours"];
    NSString* names[] = {@"Normal", @"Dark (night mode)", @"Dimmed (softer white)"};
    for (int i = 0; i < 3; ++i) {
        NSMenuItem* c = add(colours.submenu, names[i], @selector(setPageColours:), nil);
        c.tag = i;
        c.target = self;
    }
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Show Tab Bar", @selector(toggleTabBar:), nil);
    add(m, @"Enter Full Screen", @selector(toggleFullScreen:), @"f", NSEventModifierFlagCommand | NSEventModifierFlagControl);

    m = submenu(bar, @"Go");
    add(m, @"Previous Page", @selector(previousPage:), [NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey],
        NSEventModifierFlagOption | NSEventModifierFlagCommand);
    add(m, @"Next Page", @selector(nextPage:), [NSString stringWithFormat:@"%C", (unichar)NSDownArrowFunctionKey],
        NSEventModifierFlagOption | NSEventModifierFlagCommand);
    add(m, @"First Page", @selector(firstPage:), [NSString stringWithFormat:@"%C", (unichar)NSHomeFunctionKey], 0);
    add(m, @"Last Page", @selector(lastPage:), [NSString stringWithFormat:@"%C", (unichar)NSEndFunctionKey], 0);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Go to Page…", @selector(goToPage:), @"g", NSEventModifierFlagCommand | NSEventModifierFlagOption);

    m = submenu(bar, @"Window");
    add(m, @"Minimize", @selector(performMiniaturize:), @"m");
    add(m, @"Zoom", @selector(performZoom:), nil);
    [m addItem:[NSMenuItem separatorItem]];
    add(m, @"Bring All to Front", @selector(arrangeInFront:), nil);
    add(m, @"Start Page", @selector(showStartPage:), @"1", NSEventModifierFlagCommand | NSEventModifierFlagShift).target = self;
    NSApp.windowsMenu = m;

    m = submenu(bar, @"Help");
    add(m, @"Feather PDF User Guide", @selector(showUserGuide:), @"?").target = self;
    NSApp.helpMenu = m;
    return bar;
}

@end
