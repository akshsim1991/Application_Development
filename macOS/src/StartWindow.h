#pragma once
// The start window, shown whenever no document is open (at launch, and when the last one is closed), as the Windows
// app's start page: an Open PDF button inside a drop area, and the recently opened files (click to open, right-click
// to remove one, Clear list).
#import <Cocoa/Cocoa.h>

@interface FPStartWindowController : NSWindowController
+ (instancetype)shared;
- (void)showIfNoDocuments;   // shows it when no document window is open
- (void)refresh;             // the recent files changed
@end
