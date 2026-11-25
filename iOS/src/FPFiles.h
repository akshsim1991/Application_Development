// The PDF files on the iPhone, found and managed in one place: every PDF in Feather PDF's own folder (On My iPhone >
// Feather PDF) and in the places the user lets it search (iOS lets an app into a folder only when the user chooses it
// once: On My iPhone, iCloud Drive or any folder; remembered). Searching by name, sorting, and for each file (or several
// chosen): open, rename, duplicate, move, copy into Feather PDF's folder, share, information and delete.
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/** A PDF found. */
@interface FPFileItem : NSObject
@property(nonatomic, strong) NSURL* url;
@property(nonatomic, copy) NSString* name;       // with .pdf
@property(nonatomic, copy) NSString* where;      // the place and folder, for showing
@property(nonatomic) long long size;
@property(nonatomic, strong, nullable) NSDate* modified;
@property(nonatomic) BOOL inCloud;               // iCloud Drive, not downloaded yet (opening downloads it)
@end

/** The places searched: Feather PDF's own folder, and the folders chosen (kept as bookmarks). */
@interface FPPlaces : NSObject
+ (NSURL*)library;                                 // Documents: On My iPhone > Feather PDF
+ (NSArray<NSURL*>*)chosen;                        // the folders chosen, their permission in force
+ (void)activate;                                  // at launch: the permission for every place chosen
+ (BOOL)add:(NSURL*)folder;                        // a folder from the folder picker (its permission in force)
+ (void)removeAt:(NSUInteger)i;
+ (NSString*)nameOf:(NSURL*)folder;
@end

@interface FPFiles : UIViewController
- (instancetype)initWithSearch:(nullable NSString*)text;
@property(nonatomic, copy, nullable) void (^openFile)(NSURL* url);
// the build check
@property(nonatomic, readonly) NSArray<FPFileItem*>* found;   // every PDF found (before the search text)
@property(nonatomic, readonly) NSArray<FPFileItem*>* shown;   // after the search text
@property(nonatomic, copy, nullable) void (^scanned)(void);
- (void)rescan;
- (void)setSearchText:(NSString*)text;
// The file operations (also behind the buttons): nil when done, else why not.
+ (nullable NSString*)rename:(NSURL*)url to:(NSString*)name result:(NSURL* _Nullable * _Nullable)result;
+ (nullable NSString*)duplicate:(NSURL*)url result:(NSURL* _Nullable * _Nullable)result;
+ (nullable NSString*)move:(NSURL*)url into:(NSURL*)folder result:(NSURL* _Nullable * _Nullable)result;
+ (nullable NSString*)makeCopy:(NSURL*)url into:(NSURL*)folder result:(NSURL* _Nullable * _Nullable)result;
+ (nullable NSString*)remove:(NSURL*)url;
@end

NS_ASSUME_NONNULL_END
