#import "AppDelegate.h"
#import "WindowController.h"
#include <algorithm>
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
                                                                     completionHandler:^(NSDocument*, BOOL, NSError* e) {
                                                                         if (e) [NSApp presentError:e];
                                                                     }];
    // Started with nothing to open (and no windows brought back from last time): the Open window.
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([NSDocumentController sharedDocumentController].documents.count == 0 && self.filesToOpen.count == 0)
            [[NSDocumentController sharedDocumentController] openDocument:nil];
    });
}

// No empty documents: Feather PDF opens files.
- (BOOL)applicationShouldOpenUntitledFile:(NSApplication*)sender { return NO; }
- (BOOL)applicationOpenUntitledFile:(NSApplication*)sender { return NO; }

- (BOOL)applicationShouldHandleReopen:(NSApplication*)app hasVisibleWindows:(BOOL)visible {
    if (!visible) [[NSDocumentController sharedDocumentController] openDocument:nil];
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

    m = submenu(bar, @"Edit");   // for typing (the page number box); selecting text on pages comes in a later stage
    add(m, @"Cut", @selector(cut:), @"x");
    add(m, @"Copy", @selector(copy:), @"c");
    add(m, @"Paste", @selector(paste:), @"v");
    add(m, @"Select All", @selector(selectAll:), @"a");

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
    NSApp.windowsMenu = m;

    m = submenu(bar, @"Help");
    add(m, @"Feather PDF User Guide", @selector(showUserGuide:), @"?").target = self;
    NSApp.helpMenu = m;
    return bar;
}

@end
