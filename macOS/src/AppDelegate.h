#pragma once
// The app: its menus, the reading colours (one setting for every window), help and About.
#import <Cocoa/Cocoa.h>

@interface FPAppDelegate : NSObject <NSApplicationDelegate, NSMenuItemValidation>
@property(nonatomic, copy) NSArray<NSString*>* filesToOpen;   // given on the command line
- (NSMenu*)buildMainMenu;
@end
