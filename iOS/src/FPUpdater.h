// New versions, as Feather PDF for Windows, macOS and Android find them: the releases page's latest.json says the newest
// version, what is new, and the iPhone download (an .ipa, with its size and SHA-256). Checked whenever the app opens or
// comes back (at most every 30 minutes); a newer version is downloaded at once and checked, into On My iPhone >
// Feather PDF > Updates. An app put on the iPhone from a Mac cannot replace itself, so the new one is installed by
// AltStore or SideStore when the iPhone has one (a tap), or from the Mac (Sideloadly) with the file shared to it.
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface FPUpdater : NSObject
+ (NSString*)version;
+ (NSString*)codename;
+ (NSInteger)compare:(NSString*)a with:(NSString*)b;   // -1, 0, 1 (part by part: 2.25.10 > 2.25.9)
/** When the app opens or comes back: quietly, unless there is a new version (Settings > Advanced). */
+ (void)checkOnOpen:(UIViewController*)from;
/** Checks now, and says so also when there is nothing new or no answer. */
+ (void)checkNow:(UIViewController*)from;
/** AltStore: Feather PDF's source added, so AltStore keeps it up to date by itself. */
+ (void)addAltStoreSource;
+ (BOOL)altStoreInstalled;
/** The build check: downloads `url` into the Updates folder and checks its size and SHA-256 (then file is the path). */
+ (void)download:(NSString*)url version:(NSString*)version size:(long long)size sha256:(NSString*)sha256
            then:(void (^)(NSString* _Nullable file, NSString* _Nullable error))then;
@end

NS_ASSUME_NONNULL_END
