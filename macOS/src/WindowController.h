#pragma once
// A document's window: the pages, and a toolbar with the page number, previous / next and zoom. Every window of the
// app opens as a tab of the same window when one is open, as on Windows.
#import <Cocoa/Cocoa.h>

extern NSString* const FPPageColoursKey;                // user default: 0 normal, 1 dark (night), 2 dimmed
extern NSNotificationName const FPPageColoursChanged;   // posted when it changes: every window follows at once

@interface FPWindowController : NSWindowController <NSToolbarDelegate, NSWindowDelegate, NSTextFieldDelegate>
- (instancetype)init;   // the document attaches itself (NSDocument addWindowController:)
@end
