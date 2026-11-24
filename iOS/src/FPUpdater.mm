#import "FPUpdater.h"
#import "FPSettings.h"

static NSString* const kLatest = @"https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main/latest.json";
static NSString* const kMirror = @"https://cdn.jsdelivr.net/gh/akshsim1991/FeatherPDF-releases@main/latest.json";   // when GitHub cannot be reached
static NSString* const kReleases = @"https://github.com/akshsim1991/FeatherPDF-releases#iphone-and-ipad";

static BOOL busy = NO;

@implementation FPUpdater

+ (NSString*)version { return [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"0"; }
+ (NSString*)codename { return [NSBundle.mainBundle objectForInfoDictionaryKey:@"FPCodename"] ?: @""; }

+ (NSInteger)compare:(NSString*)a with:(NSString*)b {
    NSArray* x = [a componentsSeparatedByString:@"."]; NSArray* y = [b componentsSeparatedByString:@"."];
    for (NSUInteger i = 0; i < MAX(x.count, y.count); ++i) {
        const NSInteger p = i < x.count ? [x[i] integerValue] : 0, q = i < y.count ? [y[i] integerValue] : 0;
        if (p != q) return p > q ? 1 : -1;
    }
    return 0;
}

+ (void)get:(NSString*)address then:(void (^)(NSDictionary* _Nullable, NSError* _Nullable))then {
    NSURLSessionConfiguration* c = NSURLSessionConfiguration.ephemeralSessionConfiguration;
    c.timeoutIntervalForRequest = 15;
    c.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    NSMutableURLRequest* r = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:address]];
    [r setValue:@"FeatherPDF-iOS" forHTTPHeaderField:@"User-Agent"];
    [[[NSURLSession sessionWithConfiguration:c] dataTaskWithRequest:r completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
        NSDictionary* j = nil;
        if (data && [(NSHTTPURLResponse*)resp statusCode] == 200) j = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if (![j isKindOfClass:NSDictionary.class]) j = nil;
        if (!j && !err) err = [NSError errorWithDomain:@"FeatherPDF" code:1 userInfo:@{NSLocalizedDescriptionKey: @"the releases page answered oddly"}];
        dispatch_async(dispatch_get_main_queue(), ^{ then(j, j ? nil : err); });
    }] resume];
}

+ (void)check:(UIViewController*)from manual:(BOOL)manual {
    if (busy) return;
    busy = YES;
    [self get:kLatest then:^(NSDictionary* j, NSError* err) {
        void (^got)(NSDictionary*, NSError*) = ^(NSDictionary* j, NSError* err) {
            busy = NO;
            if (!from.view.window) return;
            if (!j) {
                if (manual) [self tell:from title:@"No answer from the releases page" message:[NSString stringWithFormat:@"Is the iPhone online? (%@)", err.localizedDescription ?: @""]];
                return;
            }
            NSString* v = [j[@"version"] isKindOfClass:NSString.class] ? j[@"version"] : @"0";
            NSDictionary* ios = [j[@"ios"] isKindOfClass:NSDictionary.class] ? j[@"ios"] : nil;
            if ([self compare:v with:self.version] > 0 && ios) [self offer:from version:v notes:j[@"notes"] url:ios[@"url"]];
            else if (manual) [self tell:from title:@"Feather PDF is up to date" message:[NSString stringWithFormat:@"Version %@ is the newest.", self.version]];
        };
        if (j) got(j, nil);
        else [self get:kMirror then:got];
    }];
}

+ (void)checkOnOpen:(UIViewController*)from {
    if (!FPSettings.checkUpdates) return;
    const double now = NSDate.date.timeIntervalSince1970;
    if (now - FPSettings.lastUpdateCheck < 20 * 3600) return;
    FPSettings.lastUpdateCheck = now;
    [self check:from manual:NO];
}

+ (void)checkNow:(UIViewController*)from { [self check:from manual:YES]; }

+ (void)tell:(UIViewController*)from title:(NSString*)title message:(NSString*)message {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [from presentViewController:a animated:YES completion:nil];
}

+ (void)offer:(UIViewController*)from version:(NSString*)v notes:(id)notes url:(id)url {
    NSString* n = [notes isKindOfClass:NSString.class] && [notes length] ? [notes stringByAppendingString:@"\n\n"] : @"";
    NSString* msg = [NSString stringWithFormat:@"%@You have %@.\n\nDownload the new .ipa and install it from your Mac the way you installed this one (Sideloadly, AltStore or Xcode). Your settings and recent documents are kept.", n, self.version];
    UIAlertController* a = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:@"Feather PDF %@ is available", v] message:msg preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Later" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"How to install" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [UIApplication.sharedApplication openURL:[NSURL URLWithString:kReleases] options:@{} completionHandler:nil]; }]];
    NSURL* u = [url isKindOfClass:NSString.class] ? [NSURL URLWithString:url] : nil;
    if (u) [a addAction:[UIAlertAction actionWithTitle:@"Get it" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [UIApplication.sharedApplication openURL:u options:@{} completionHandler:nil]; }]];
    [from presentViewController:a animated:YES completion:nil];
}

@end
