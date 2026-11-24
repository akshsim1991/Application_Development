// Every preference of the iPhone and iPad app, kept on the device, in the groups of Feather PDF for Windows (Settings
// there): Appearance, PDF Viewing, Editing, Advanced. As Android/.../Settings.kt.
#import <UIKit/UIKit.h>
#import "FPEngine.h"

NS_ASSUME_NONNULL_BEGIN

@interface FPSettings : NSObject
// Appearance: 0 light, 1 dark, 2 like the device
@property(class, nonatomic) NSInteger theme;
// PDF Viewing
@property(class, nonatomic) FPColours pageColours;
@property(class, nonatomic) BOOL restorePositions;
@property(class, nonatomic) BOOL keepScreenOn;
@property(class, nonatomic) BOOL sidebarOpen;
// Editing (the colour names of Feather PDF for Windows)
@property(class, nonatomic, readonly) NSArray<NSString*>* colourNames;
@property(class, nonatomic, readonly) NSArray<NSString*>* widthNames;
@property(class, nonatomic) NSInteger highlightIndex;
@property(class, nonatomic) NSInteger drawIndex;
@property(class, nonatomic) NSInteger widthIndex;
+ (uint32_t)colour:(NSInteger)index;    // 0xRRGGBB
+ (uint32_t)highlightColour;
+ (uint32_t)drawColour;
+ (double)lineWidth;
// Advanced: new versions
@property(class, nonatomic) BOOL checkUpdates;
@property(class, nonatomic) double lastUpdateCheck;
+ (void)reset;
/** The theme chosen, applied to every window. */
+ (void)applyTheme;
+ (UIColor*)barColour;
@end

/** The documents opened lately: name, the page last read, when, and a bookmark that opens the file again. */
@interface FPRecent : NSObject
@property(nonatomic, copy) NSString* name;
@property(nonatomic, copy) NSData* bookmark;
@property(nonatomic) int page;
@property(nonatomic) double time;
+ (NSArray<FPRecent*>*)list;
+ (void)add:(NSURL*)url name:(NSString*)name;
+ (void)setPage:(int)page forURL:(NSURL*)url;
+ (int)pageForURL:(NSURL*)url;
+ (void)remove:(FPRecent*)r;
+ (void)clear;
- (nullable NSURL*)resolve;
@end

NS_ASSUME_NONNULL_END
