#pragma once
// New versions on the Mac: Help > Update to New Version, and a quiet check when the app starts (Help > Check for
// Updates Automatically). The version file and its checks are shared (shared/update.hpp); the download is
// NSURLSession, and the new app replaces this one where it is, then starts.
#import <Cocoa/Cocoa.h>

extern NSString* const FPCheckUpdatesKey;   // user default: look for a new version at start (on unless turned off)

// `manual`: the user asked, so "you have the newest version" and network problems are said too.
void FPCheckForUpdates(BOOL manual);
