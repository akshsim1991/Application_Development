// New versions, as Feather PDF for Windows, macOS and Android find them: the releases page's latest.json says the newest
// version, what is new, and the iPhone download (an .ipa). An app put on the iPhone from a Mac (sideloaded) cannot
// replace itself: a newer version is offered with its notes, and "Get it" opens the download in Safari; it is then
// installed from the Mac the same way as the first time (the documents and settings are kept).
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface FPUpdater : NSObject
+ (NSString*)version;
+ (NSString*)codename;
+ (NSInteger)compare:(NSString*)a with:(NSString*)b;   // -1, 0, 1 (part by part: 2.25.10 > 2.25.9)
/** When the app opens: at most once a day, quietly unless there is a new version (Settings > Advanced). */
+ (void)checkOnOpen:(UIViewController*)from;
/** Checks now, and says so also when there is nothing new or no answer. */
+ (void)checkNow:(UIViewController*)from;
@end

NS_ASSUME_NONNULL_END
