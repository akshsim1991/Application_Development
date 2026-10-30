#pragma once
// A document's window: the pages, and a toolbar with the page number, previous / next and zoom. Every window of the
// app opens as a tab of the same window when one is open, as on Windows.
#import <Cocoa/Cocoa.h>

extern NSString* const FPPageColoursKey;                // user default: 0 normal, 1 dark (night), 2 dimmed
extern NSNotificationName const FPPageColoursChanged;   // posted when it changes: every window follows at once
extern NSString* const FPSidebarShownKey;              // user default: the sidebar is shown (it is, unless hidden)

#include "document.hpp"
@class FPPageView;

@interface FPWindowController : NSWindowController <NSToolbarDelegate, NSWindowDelegate, NSTextFieldDelegate>
- (instancetype)init;   // the document attaches itself (NSDocument addWindowController:)
@property(nonatomic, readonly) FPPageView* pageView;
- (Document*)pdf;
- (void)afterChange;    // the pages changed: draw them again, mark the document edited, update the toolbar
- (void)selectAllThumbs; // the sidebar: shown, with every page selected
- (NSArray<NSNumber*>*)selectedThumbPages;   // the pages selected in the sidebar, else the page shown
@end

@interface FPWindowController (Annotate)
- (void)setupTools;     // Annotate.mm: the page view's tools and right-click menu
@end
