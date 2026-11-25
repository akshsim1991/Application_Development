#import "FPUpdater.h"
#import "FPSettings.h"
#import <CommonCrypto/CommonDigest.h>

static NSString* const kLatest = @"https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main/latest.json";
static NSString* const kMirror = @"https://cdn.jsdelivr.net/gh/akshsim1991/FeatherPDF-releases@main/latest.json";   // when GitHub cannot be reached
static NSString* const kReleases = @"https://github.com/akshsim1991/FeatherPDF-releases#iphone-and-ipad";
static NSString* const kAltSource = @"https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main/altstore.json";

static BOOL busy = NO;
static NSString* offered = nil;   // the version offered while the app runs (asked once)

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

+ (NSURLSession*)session {
    NSURLSessionConfiguration* c = NSURLSessionConfiguration.ephemeralSessionConfiguration;
    c.timeoutIntervalForRequest = 20;
    c.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    c.HTTPAdditionalHeaders = @{@"User-Agent": @"FeatherPDF-iOS"};
    return [NSURLSession sessionWithConfiguration:c];
}

+ (void)get:(NSString*)address then:(void (^)(NSDictionary* _Nullable, NSError* _Nullable))then {
    [[[self session] dataTaskWithURL:[NSURL URLWithString:address] completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
        NSDictionary* j = nil;
        if (data && [(NSHTTPURLResponse*)resp statusCode] == 200) j = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if (![j isKindOfClass:NSDictionary.class]) j = nil;
        if (!j && !err) err = [NSError errorWithDomain:@"FeatherPDF" code:1 userInfo:@{NSLocalizedDescriptionKey: @"the releases page answered oddly"}];
        dispatch_async(dispatch_get_main_queue(), ^{ then(j, j ? nil : err); });
    }] resume];
}

+ (NSString*)updatesFolder {
    NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString* dir = [docs stringByAppendingPathComponent:@"Updates"];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return dir;
}

+ (NSString*)sha256Of:(NSString*)path {
    NSInputStream* in = [NSInputStream inputStreamWithFileAtPath:path];
    [in open];
    CC_SHA256_CTX ctx; CC_SHA256_Init(&ctx);
    uint8_t buf[65536];
    NSInteger n;
    while ((n = [in read:buf maxLength:sizeof buf]) > 0) CC_SHA256_Update(&ctx, buf, (CC_LONG)n);
    [in close];
    unsigned char h[CC_SHA256_DIGEST_LENGTH]; CC_SHA256_Final(h, &ctx);
    NSMutableString* s = [NSMutableString string];
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) [s appendFormat:@"%02x", h[i]];
    return s;
}

+ (BOOL)good:(NSString*)path size:(long long)size sha256:(NSString*)sha {
    NSDictionary* a = [NSFileManager.defaultManager attributesOfItemAtPath:path error:nil];
    if (!a) return NO;
    if (size > 0 && [a fileSize] != (unsigned long long)size) return NO;
    return sha.length == 0 || [[self sha256Of:path] caseInsensitiveCompare:sha] == NSOrderedSame;
}

+ (void)download:(NSString*)url version:(NSString*)version size:(long long)size sha256:(NSString*)sha256
            then:(void (^)(NSString*, NSString*))then {
    NSString* dest = [[self updatesFolder] stringByAppendingPathComponent:[NSString stringWithFormat:@"FeatherPDF-%@-ios.ipa", version]];
    if ([self good:dest size:size sha256:sha256]) { then(dest, nil); return; }   // (downloaded before)
    NSURL* u = [NSURL URLWithString:url];
    if (!u) { then(nil, @"the download address is not valid"); return; }
    [[[self session] downloadTaskWithURL:u completionHandler:^(NSURL* tmp, NSURLResponse* resp, NSError* err) {
        NSString* failure = nil;
        if (!tmp) failure = err.localizedDescription ?: @"the download did not finish";
        else if ([resp isKindOfClass:NSHTTPURLResponse.class] && [(NSHTTPURLResponse*)resp statusCode] != 200)
            failure = [NSString stringWithFormat:@"the server answered %ld", (long)[(NSHTTPURLResponse*)resp statusCode]];
        else {
            [NSFileManager.defaultManager removeItemAtPath:dest error:nil];
            NSError* me = nil;
            if (![NSFileManager.defaultManager moveItemAtURL:tmp toURL:[NSURL fileURLWithPath:dest] error:&me]) failure = me.localizedDescription;
            else if (![self good:dest size:size sha256:sha256]) {
                [NSFileManager.defaultManager removeItemAtPath:dest error:nil];
                failure = @"the download is not the published file (its size or fingerprint differs)";
            }
        }
        if (!failure)   // older downloads go
            for (NSString* f in [NSFileManager.defaultManager contentsOfDirectoryAtPath:[self updatesFolder] error:nil])
                if (![f isEqualToString:dest.lastPathComponent]) [NSFileManager.defaultManager removeItemAtPath:[[self updatesFolder] stringByAppendingPathComponent:f] error:nil];
        dispatch_async(dispatch_get_main_queue(), ^{ then(failure ? nil : dest, failure); });
    }] resume];
}

+ (void)check:(UIViewController*)from manual:(BOOL)manual {
    if (busy) return;
    busy = YES;
    void (^got)(NSDictionary*, NSError*) = ^(NSDictionary* j, NSError* err) {
        if (!j) {
            busy = NO;
            if (manual) [self tell:from title:@"No answer from the releases page" message:[NSString stringWithFormat:@"Is the iPhone online? (%@)", err.localizedDescription ?: @""]];
            return;
        }
        NSString* v = [j[@"version"] isKindOfClass:NSString.class] ? j[@"version"] : @"0";
        NSDictionary* ios = [j[@"ios"] isKindOfClass:NSDictionary.class] ? j[@"ios"] : nil;
        if ([self compare:v with:self.version] <= 0 || !ios || ![ios[@"url"] isKindOfClass:NSString.class]) {
            busy = NO;
            if (manual) [self tell:from title:@"Feather PDF is up to date" message:[NSString stringWithFormat:@"Version %@ is the newest.", self.version]];
            return;
        }
        if (!manual && [offered isEqualToString:v]) { busy = NO; return; }   // (asked already while the app runs)
        NSString* notes = [j[@"notes"] isKindOfClass:NSString.class] ? j[@"notes"] : @"";
        NSString* url = ios[@"url"];
        // downloaded at once, then offered
        [self download:url version:v size:[ios[@"size"] longLongValue] sha256:[ios[@"sha256"] isKindOfClass:NSString.class] ? ios[@"sha256"] : @""
                  then:^(NSString* file, NSString* error) {
            busy = NO;
            offered = v;
            UIViewController* top = from.view.window ? from : nil;
            if (!top) return;
            [self offer:top version:v notes:notes url:url file:file error:error];
        }];
    };
    [self get:kLatest then:^(NSDictionary* j, NSError* err) { if (j) got(j, nil); else [self get:kMirror then:got]; }];
}

+ (void)checkOnOpen:(UIViewController*)from {
    if (!FPSettings.checkUpdates) return;
    const double now = NSDate.date.timeIntervalSince1970;
    if (now - FPSettings.lastUpdateCheck < 30 * 60) return;
    FPSettings.lastUpdateCheck = now;
    [self check:from manual:NO];
}

+ (void)checkNow:(UIViewController*)from { [self check:from manual:YES]; }

+ (void)tell:(UIViewController*)from title:(NSString*)title message:(NSString*)message {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [from presentViewController:a animated:YES completion:nil];
}

static NSString* enc(NSString* s) { return [s stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.alphanumericCharacterSet] ?: s; }
static BOOL canOpen(NSString* scheme) { return [UIApplication.sharedApplication canOpenURL:[NSURL URLWithString:[scheme stringByAppendingString:@"://"]]]; }

+ (BOOL)altStoreInstalled { return canOpen(@"altstore"); }

+ (void)addAltStoreSource {
    [UIApplication.sharedApplication openURL:[NSURL URLWithString:[@"altstore://source?url=" stringByAppendingString:enc(kAltSource)]] options:@{} completionHandler:nil];
}

+ (void)offer:(UIViewController*)from version:(NSString*)v notes:(NSString*)notes url:(NSString*)url file:(NSString*)file error:(NSString*)error {
    NSMutableString* msg = [NSMutableString string];
    if (notes.length) [msg appendFormat:@"%@\n\n", notes];
    [msg appendFormat:@"You have %@.\n\n", self.version];
    const BOOL alt = canOpen(@"altstore"), side = canOpen(@"sidestore");
    if (file) [msg appendString:@"It is downloaded and checked: On My iPhone › Feather PDF › Updates. "];
    else [msg appendFormat:@"It could not be downloaded now (%@). ", error ?: @"no answer"];
    if (alt || side) [msg appendString:@"Install it with one tap below; your settings and documents are kept."];
    else [msg appendString:@"Install it from your Mac as the first time (Sideloadly): Share the file to the Mac with AirDrop. With AltStore on the iPhone, updates install with a tap."];
    UIAlertController* a = [UIAlertController alertControllerWithTitle:[NSString stringWithFormat:@"Feather PDF %@ is ready", v] message:msg preferredStyle:UIAlertControllerStyleAlert];
    if (alt) [a addAction:[UIAlertAction actionWithTitle:@"Install with AltStore" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [UIApplication.sharedApplication openURL:[NSURL URLWithString:[@"altstore://install?url=" stringByAppendingString:enc(url)]] options:@{} completionHandler:nil]; }]];
    if (side) [a addAction:[UIAlertAction actionWithTitle:@"Install with SideStore" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [UIApplication.sharedApplication openURL:[NSURL URLWithString:[@"sidestore://install?url=" stringByAppendingString:enc(url)]] options:@{} completionHandler:nil]; }]];
    if (file) [a addAction:[UIAlertAction actionWithTitle:@"Share the file (AirDrop to the Mac)…" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        UIActivityViewController* s = [[UIActivityViewController alloc] initWithActivityItems:@[[NSURL fileURLWithPath:file]] applicationActivities:nil];
        s.popoverPresentationController.sourceView = from.view;
        s.popoverPresentationController.sourceRect = CGRectMake(from.view.bounds.size.width / 2, from.view.bounds.size.height / 2, 1, 1);
        [from presentViewController:s animated:YES completion:nil]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"How to install" style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        [UIApplication.sharedApplication openURL:[NSURL URLWithString:kReleases] options:@{} completionHandler:nil]; }]];
    [a addAction:[UIAlertAction actionWithTitle:@"Later" style:UIAlertActionStyleCancel handler:nil]];
    [from presentViewController:a animated:YES completion:nil];
}

@end
