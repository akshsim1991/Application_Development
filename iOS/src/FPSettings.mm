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
        r.page = [d[@"page"] intValue]; r.time = [d[@"time"] doubleValue];
        [out addObject:r];
    }
    return out;
}
+ (void)save:(NSArray<FPRecent*>*)list {
    NSMutableArray* a = [NSMutableArray array];
    for (FPRecent* r in list) [a addObject:@{@"name": r.name, @"bookmark": r.bookmark, @"page": @(r.page), @"time": @(r.time)}];
    [D() setObject:a forKey:kRecents];
}
+ (NSInteger)indexOf:(NSURL*)url in:(NSArray<FPRecent*>*)list {
    for (NSUInteger i = 0; i < list.count; ++i) {
        NSURL* u = [list[i] resolve];
        if (u && [u.URLByStandardizingPath isEqual:url.URLByStandardizingPath]) return (NSInteger)i;
    }
    return -1;
}
+ (void)add:(NSURL*)url name:(NSString*)name {
    NSData* b = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    if (!b) return;
    NSMutableArray<FPRecent*>* list = [[self list] mutableCopy];
    const NSInteger i = [self indexOf:url in:list];
    int page = 0;
    if (i >= 0) { page = list[i].page; [list removeObjectAtIndex:i]; }
    FPRecent* r = [FPRecent new];
    r.name = name; r.bookmark = b; r.page = page; r.time = NSDate.date.timeIntervalSince1970;
    [list insertObject:r atIndex:0];
    while (list.count > 20) [list removeLastObject];
    [self save:list];
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
- (NSURL*)resolve {
    BOOL stale = NO;
    return [NSURL URLByResolvingBookmarkData:self.bookmark options:0 relativeToURL:nil bookmarkDataIsStale:&stale error:nil];
}
@end
