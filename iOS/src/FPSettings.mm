#import "FPSettings.h"

static NSUserDefaults* D() { return NSUserDefaults.standardUserDefaults; }
static NSInteger I(NSString* k, NSInteger d) { return [D() objectForKey:k] ? [D() integerForKey:k] : d; }
static BOOL B(NSString* k, BOOL d) { return [D() objectForKey:k] ? [D() boolForKey:k] : d; }

@implementation FPSettings
+ (NSInteger)theme { return I(@"theme", 2); }
+ (void)setTheme:(NSInteger)v { [D() setInteger:v forKey:@"theme"]; [self applyTheme]; }
+ (FPColours)pageColours { return (FPColours)I(@"pageColours", FPColoursNormal); }
+ (void)setPageColours:(FPColours)v { [D() setInteger:v forKey:@"pageColours"]; }
+ (BOOL)restorePositions { return B(@"restorePositions", YES); }
+ (void)setRestorePositions:(BOOL)v { [D() setBool:v forKey:@"restorePositions"]; }
+ (BOOL)keepScreenOn { return B(@"keepScreenOn", NO); }
+ (void)setKeepScreenOn:(BOOL)v { [D() setBool:v forKey:@"keepScreenOn"]; }
+ (BOOL)sidebarOpen { return B(@"sidebarOpen", UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad); }
+ (void)setSidebarOpen:(BOOL)v { [D() setBool:v forKey:@"sidebarOpen"]; }
+ (NSArray<NSString*>*)colourNames { return @[@"Yellow", @"Green", @"Blue", @"Pink", @"Orange", @"Red", @"Purple"]; }
+ (NSArray<NSString*>*)widthNames { return @[@"Thin", @"Medium", @"Thick"]; }
+ (NSInteger)highlightIndex { return I(@"highlightColour", 0); }
+ (void)setHighlightIndex:(NSInteger)v { [D() setInteger:v forKey:@"highlightColour"]; }
+ (NSInteger)drawIndex { return I(@"drawingColour", 5); }
+ (void)setDrawIndex:(NSInteger)v { [D() setInteger:v forKey:@"drawingColour"]; }
+ (NSInteger)widthIndex { return I(@"lineWidth", 1); }
+ (void)setWidthIndex:(NSInteger)v { [D() setInteger:v forKey:@"lineWidth"]; }
+ (uint32_t)colour:(NSInteger)i {
    static const uint32_t v[] = {0xFFEB3B, 0x66BB6A, 0x42A5F5, 0xF06292, 0xFFA726, 0xE53935, 0xAB47BC};
    return v[MAX(0, MIN(6, i))];
}
+ (uint32_t)highlightColour { return [self colour:self.highlightIndex]; }
+ (uint32_t)drawColour { return [self colour:self.drawIndex]; }
+ (double)lineWidth { static const double w[] = {1, 2.5, 5}; return w[MAX(0, MIN(2, self.widthIndex))]; }
+ (BOOL)checkUpdates { return B(@"checkUpdates", YES); }
+ (void)setCheckUpdates:(BOOL)v { [D() setBool:v forKey:@"checkUpdates"]; }
+ (double)lastUpdateCheck { return [D() doubleForKey:@"lastUpdateCheck"]; }
+ (void)setLastUpdateCheck:(double)v { [D() setDouble:v forKey:@"lastUpdateCheck"]; }
+ (void)reset {
    for (NSString* k in @[@"theme", @"pageColours", @"restorePositions", @"keepScreenOn", @"sidebarOpen", @"highlightColour", @"drawingColour", @"lineWidth", @"checkUpdates"])
        [D() removeObjectForKey:k];
    [self applyTheme];
}
+ (void)applyTheme {
    const UIUserInterfaceStyle s = self.theme == 0 ? UIUserInterfaceStyleLight : self.theme == 1 ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
    for (UIScene* sc in UIApplication.sharedApplication.connectedScenes)
        if ([sc isKindOfClass:UIWindowScene.class]) for (UIWindow* w in ((UIWindowScene*)sc).windows) w.overrideUserInterfaceStyle = s;
}
+ (UIColor*)barColour {
    return [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* t) {
        return t.userInterfaceStyle == UIUserInterfaceStyleDark ? [UIColor colorWithRed:0x8E / 255.0 green:0x1B / 255.0 blue:0x1B / 255.0 alpha:1]
                                                                : [UIColor colorWithRed:0xC6 / 255.0 green:0x28 / 255.0 blue:0x28 / 255.0 alpha:1];
    }];
}
@end

@implementation FPRecent
static NSString* const kRecents = @"recents";

+ (NSArray<FPRecent*>*)list {
    NSMutableArray* out = [NSMutableArray array];
    for (NSDictionary* d in [D() arrayForKey:kRecents] ?: @[]) {
        FPRecent* r = [FPRecent new];
        r.name = d[@"name"] ?: @"Document.pdf"; r.bookmark = d[@"bookmark"] ?: [NSData data];
        r.copyName = d[@"copy"] ?: @"";
        r.page = [d[@"page"] intValue]; r.time = [d[@"time"] doubleValue];
        [out addObject:r];
    }
    return out;
}
+ (void)save:(NSArray<FPRecent*>*)list {
    NSMutableArray* a = [NSMutableArray array];
    for (FPRecent* r in list) [a addObject:@{@"name": r.name, @"bookmark": r.bookmark, @"copy": r.copyName ?: @"", @"page": @(r.page), @"time": @(r.time)}];
    [D() setObject:a forKey:kRecents];
}
static NSString* pathOf(NSURL* u) { return u.URLByStandardizingPath.URLByResolvingSymlinksInPath.path ?: @""; }
+ (NSInteger)indexOf:(NSURL*)url in:(NSArray<FPRecent*>*)list {
    NSString* want = pathOf(url);
    for (NSUInteger i = 0; i < list.count; ++i) {
        NSURL* u = [list[i] resolve:nil];
        if (u && [pathOf(u) isEqualToString:want]) return (NSInteger)i;
    }
    return -1;
}
+ (void)add:(NSURL*)url name:(NSString*)name bookmark:(NSData*)bookmark copy:(NSString*)copyName {
    NSData* b = bookmark;
    if (!b) {   // (a file of the app's own needs no permission)
        const BOOL scoped = [url startAccessingSecurityScopedResource];
        b = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
        if (scoped) [url stopAccessingSecurityScopedResource];
    }
    if (!b) return;
    NSMutableArray<FPRecent*>* list = [[self list] mutableCopy];
    const NSInteger i = [self indexOf:url in:list];
    int page = 0;
    if (i >= 0) { page = list[i].page; [list removeObjectAtIndex:i]; }
    // entries of the same name that cannot be reached go (versions before 2.28 kept bookmarks that never opened
    // again, and added the file once more each time it was opened from Files); the page read there is kept
    for (NSInteger k = (NSInteger)list.count - 1; k >= 0; --k)
        if ([list[k].name isEqualToString:name] && ![list[k] resolve:nil]) { if (i < 0 && page == 0) page = list[k].page; [list removeObjectAtIndex:k]; }
    FPRecent* r = [FPRecent new];
    r.name = name; r.bookmark = b; r.copyName = copyName ?: @""; r.page = page; r.time = NSDate.date.timeIntervalSince1970;
    [list insertObject:r atIndex:0];
    while (list.count > 20) [list removeLastObject];
    [self save:list];
}
+ (void)moved:(NSURL*)from to:(NSURL*)to {
    NSMutableArray<FPRecent*>* list = [[self list] mutableCopy];
    const NSInteger i = [self indexOf:from in:list];
    if (i < 0) {   // (the bookmark may already follow the file: then it is found by its new place)
        const NSInteger j = [self indexOf:to in:list];
        if (j >= 0) { list[j].name = to.lastPathComponent; [self save:list]; }
        return;
    }
    const BOOL scoped = [to startAccessingSecurityScopedResource];
    NSData* b = [to bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    if (scoped) [to stopAccessingSecurityScopedResource];
    if (b) list[i].bookmark = b;
    list[i].name = to.lastPathComponent;
    [self save:list];
}
+ (void)forget:(NSURL*)url {
    NSMutableArray<FPRecent*>* list = [[self list] mutableCopy];
    const NSInteger i = [self indexOf:url in:list];
    if (i >= 0) { [list removeObjectAtIndex:i]; [self save:list]; }
}
+ (void)setPage:(int)page forURL:(NSURL*)url {
    NSArray<FPRecent*>* list = [self list];
    const NSInteger i = [self indexOf:url in:list];
    if (i < 0) return;
    list[i].page = page;
    [self save:list];
}
+ (int)pageForURL:(NSURL*)url {
    NSArray<FPRecent*>* list = [self list];
    const NSInteger i = [self indexOf:url in:list];
    return i >= 0 ? list[i].page : 0;
}
+ (void)remove:(FPRecent*)r {
    NSMutableArray<FPRecent*>* list = [[self list] mutableCopy];
    for (NSUInteger i = 0; i < list.count; ++i) if ([list[i].bookmark isEqualToData:r.bookmark]) { [list removeObjectAtIndex:i]; break; }
    [self save:list];
}
+ (void)clear { [D() removeObjectForKey:kRecents]; }
- (NSURL*)resolve:(NSString**)why {
    BOOL stale = NO;
    NSError* err = nil;
    NSURL* u = [NSURL URLByResolvingBookmarkData:self.bookmark options:0 relativeToURL:nil bookmarkDataIsStale:&stale error:&err];
    if (!u) { if (why) *why = err.localizedDescription; return nil; }
    const BOOL scoped = [u startAccessingSecurityScopedResource];
    // there: a file, or (iCloud Drive) a file not downloaded yet, which opening downloads
    NSNumber* ubiquitous = nil;
    [u getResourceValue:&ubiquitous forKey:NSURLIsUbiquitousItemKey error:nil];
    const BOOL there = [NSFileManager.defaultManager fileExistsAtPath:u.path] || ubiquitous.boolValue;
    if (there && stale) {   // moved or renamed: the bookmark made again from where it is now
        NSData* b = [u bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
        if (b) {
            NSMutableArray<FPRecent*>* list = [[FPRecent list] mutableCopy];
            for (FPRecent* r in list) if ([r.bookmark isEqualToData:self.bookmark]) { r.bookmark = b; r.name = u.lastPathComponent; }
            [FPRecent save:list];
            self.bookmark = b;
        }
    }
    if (scoped) [u stopAccessingSecurityScopedResource];
    if (!there) { if (why) *why = @"It is not in its folder any more."; return nil; }
    return u;
}
- (NSString*)keptCopy {
    if (self.copyName.length == 0) return nil;
    NSString* p = [[NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).firstObject stringByAppendingPathComponent:@"open"]
                   stringByAppendingPathComponent:self.copyName];
    return [NSFileManager.defaultManager fileExistsAtPath:p] ? p : nil;
}
@end
