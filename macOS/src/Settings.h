#pragma once
// Settings (Settings.mm) and what they mean elsewhere.
#import <Cocoa/Cocoa.h>

extern NSNotificationName const FPSettingsChanged;   // any setting changed: windows follow at once

void FPShowSettings();
void FPApplyTheme();          // the Mac's light / dark look for the chosen theme
NSColor* FPThemeSurround();   // the colour around the pages
BOOL FPColourIcons();
BOOL FPTabColours();
BOOL FPTabNumbers();
NSInteger FPDuplicateMode();  // 0 ask first, 1 go to its tab
BOOL FPRestorePositions();
NSInteger FPPagesKept();      // whole pages kept drawn in memory

// Opening a file that is already open (also through another path or an alias): its tab is shown, after a question
// when the setting says so (as on Windows).
@interface FPDocumentController : NSDocumentController
@end

// Recovery copies of unsaved changes (Performance setting): written in the background, offered again after a crash.
void FPStartRecovery();
void FPRecoveryForget(NSDocument* doc);   // the document was saved or closed: its copy goes
void FPRecoveryClearAll();               // a normal quit
