#import "Updater.h"
#include <algorithm>
#include <string>
#include <vector>
#include "platform.hpp"
#include "update.hpp"

NSString* const FPCheckUpdatesKey = @"checkUpdates";
static NSString* const kSkipKey = @"skipVersion";

static NSString* ns(const std::string& s) { return [NSString stringWithUTF8String:s.c_str()] ?: @""; }
static NSString* nsw(const std::wstring& s) { return ns(platform::utf8(s)); }

static void tell(NSString* text, BOOL warning) {
    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = text;
    a.alertStyle = warning ? NSAlertStyleWarning : NSAlertStyleInformational;
    [a runModal];
}

// A small window with a progress bar while the new version downloads.
@interface FPUpdateProgress : NSWindowController
@property(nonatomic, strong) NSProgressIndicator* bar;
@property(nonatomic, strong) NSTextField* label;
@end
@implementation FPUpdateProgress
- (instancetype)initWithText:(NSString*)text {
    NSPanel* p = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 380, 96) styleMask:NSWindowStyleMaskTitled
                                              backing:NSBackingStoreBuffered defer:NO];
    p.title = @"Feather PDF";
    if (!(self = [super initWithWindow:p])) return nil;
    _label = [NSTextField labelWithString:text];
    _label.frame = NSMakeRect(20, 56, 340, 20);
    _bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 24, 340, 20)];
    _bar.indeterminate = NO;
    _bar.minValue = 0;
    _bar.maxValue = 1;
    [p.contentView addSubview:_label];
    [p.contentView addSubview:_bar];
    [p center];
    return self;
}
@end

// A request for one of the places a file can come from (update::sources), with its Accept header.
static NSURLRequest* requestFor(const update::Source& src) {
    NSMutableURLRequest* q = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:ns(src.url)]];
    q.timeoutInterval = 30;
    if (!src.accept.empty()) [q setValue:ns(src.accept) forHTTPHeaderField:@"Accept"];
    return q;
}

// Every place of `list` asked at the same moment; the first good answer from raw.githubusercontent.com or GitHub's API
// is taken, jsDelivr's (which can be a few hours behind) once neither has answered within two seconds more.
// `done(data, why, status, via)` on the main queue: data nil when none answered (why lists each), status 404 when
// nothing is published.
static void fetchFastest(std::vector<update::Source> list, void (^done)(NSData*, NSString*, NSInteger, NSString*)) {
    const size_t n = list.size();
    NSMutableArray* results = [NSMutableArray array];   // per place: NSNull waiting, NSData good, NSString why
    for (size_t i = 0; i < n; ++i) [results addObject:[NSNull null]];
    __block BOOL finished = NO;
    __block NSInteger firstStatus = 0;
    void (^decide)(BOOL) = ^(BOOL timeUp) {
        if (finished) return;
        int fresh = -1, any = -1; BOOL waiting = NO;
        for (size_t i = 0; i < n; ++i) {
            id r = results[i];
            if (r == [NSNull null]) waiting = YES;
            else if ([r isKindOfClass:[NSData class]]) { if (any < 0) any = static_cast<int>(i); if (fresh < 0 && list[i].host != "cdn.jsdelivr.net") fresh = static_cast<int>(i); }
        }
        const int pick = fresh >= 0 ? fresh : (any >= 0 && (timeUp || !waiting) ? any : -1);
        if (pick >= 0) { finished = YES; done(results[pick], @"", 200, ns(list[pick].host)); return; }
        if (waiting) return;
        finished = YES;
        NSMutableString* why = [NSMutableString string];
        for (size_t i = 0; i < n; ++i) [why appendFormat:@"\n  %@: %@", ns(list[i].host), results[i]];
        done(nil, why, firstStatus == 404 ? 404 : 0, nil);
    };
    for (size_t i = 0; i < n; ++i) {
        const update::Source src = list[i];
        [[NSURLSession.sharedSession dataTaskWithRequest:requestFor(src) completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
            const NSInteger status = [resp isKindOfClass:[NSHTTPURLResponse class]] ? [(NSHTTPURLResponse*)resp statusCode] : 0;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (i == 0) firstStatus = status;
                results[i] = data && !err && status == 200 ? data : (id)(err ? err.localizedDescription : [NSString stringWithFormat:@"the server answered with an error (%ld)", (long)status]);
                const BOOL gotStale = [results[i] isKindOfClass:[NSData class]] && src.host == "cdn.jsdelivr.net";
                decide(NO);
                if (gotStale) dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ decide(YES); });
            });
        }] resume];
    }
}

// Where the update script leaves a note when the new version could not be put in place (read at the next start).
static NSString* problemFile() {
    return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/Feather PDF/update-problem.txt"];
}

void FPReportUpdateProblem() {
    NSString* f = problemFile();
    NSString* why = [NSString stringWithContentsOfFile:f encoding:NSUTF8StringEncoding error:nil];
    if (!why.length) return;
    [[NSFileManager defaultManager] removeItemAtPath:f error:nil];
    tell([NSString stringWithFormat:@"The new version of Feather PDF could not be installed. %@\n\nYou still have version %s. Try Help > Update to New Version again; if it fails again, download the new version from the Feather PDF releases page and drag it onto Applications.",
                                    [why stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet], FEATHER_VERSION], YES);
}

// Puts the new app in place of this one after it has quit, then starts it (a small shell script, so nothing of the
// old app is running while it is replaced). False when this app's folder cannot be written to. When it cannot put the
// new app in place, it leaves a note (problemFile) and starts the old one, which then says why.
static BOOL installAndRestart(NSString* dmg) {
    NSString* app = NSBundle.mainBundle.bundlePath;
    if (![[NSFileManager defaultManager] isWritableFileAtPath:app.stringByDeletingLastPathComponent]) return NO;
    NSString* script =
        @"pid=\"$1\"; dmg=\"$2\"; app=\"$3\"; note=\"$4\"; n=0\n"
        @"mkdir -p \"$(dirname \"$note\")\"; rm -f \"$note\"\n"
        // quitting was cancelled (a document not saved): leave everything as it is
        @"while kill -0 \"$pid\" 2>/dev/null && [ $n -lt 1200 ]; do sleep 0.5; n=$((n+1)); done\n"
        @"if kill -0 \"$pid\" 2>/dev/null; then rm -f \"$dmg\"; exit 0; fi\n"
        @"mnt=$(mktemp -d)\n"
        @"if ! hdiutil attach -nobrowse -readonly -mountpoint \"$mnt\" \"$dmg\" >/dev/null 2>&1; then\n"
        @"  echo 'The downloaded disk image could not be opened.' > \"$note\"; open \"$app\"; exit 1; fi\n"
        @"rm -rf \"$app.old\"\n"
        @"if mv \"$app\" \"$app.old\" && ditto \"$mnt/Feather PDF.app\" \"$app\"; then rm -rf \"$app.old\"; xattr -dr com.apple.quarantine \"$app\" 2>/dev/null\n"
        @"else rm -rf \"$app\"; mv \"$app.old\" \"$app\"; echo \"The new version could not be put in place of the old one in $(dirname \"$app\").\" > \"$note\"; fi\n"
        @"hdiutil detach \"$mnt\" >/dev/null 2>&1\n"
        @"rm -f \"$dmg\"\n"
        @"open \"$app\"\n";
    NSTask* t = [[NSTask alloc] init];
    t.executableURL = [NSURL fileURLWithPath:@"/bin/sh"];
    t.arguments = @[ @"-c", script, @"feather-update", [NSString stringWithFormat:@"%d", NSProcessInfo.processInfo.processIdentifier], dmg, app, problemFile() ];
    NSError* e = nil;
    return [t launchAndReturnError:&e];
}

static void downloadAndInstall(const update::Release& r, NSString* via) {
    FPUpdateProgress* progress = [[FPUpdateProgress alloc] initWithText:[NSString stringWithFormat:@"Downloading Feather PDF %@…", ns(r.version)]];
    [progress showWindow:nil];
    const std::string sha = r.mac.sha256, version = r.version;
    std::vector<update::Source> list = update::sources(r.mac.url);
    const std::string prefer = via ? std::string(via.UTF8String) : std::string();   // the server that gave the version file first
    std::stable_partition(list.begin(), list.end(), [&](const update::Source& x) { return !prefer.empty() && x.host == prefer; });
    __block size_t at = 0;
    __block NSString* tried = @"";
    __block void (^attempt)(void) = nil;
    void (^tryOne)(void) = ^{
    const update::Source src = list[at];
    NSURLSessionDownloadTask* task = [NSURLSession.sharedSession downloadTaskWithRequest:requestFor(src)
                                                                       completionHandler:^(NSURL* file, NSURLResponse* resp, NSError* err) {
        NSString* kept = nil;
        const NSInteger status = [resp isKindOfClass:[NSHTTPURLResponse class]] ? [(NSHTTPURLResponse*)resp statusCode] : 0;
        if (file && !err && status == 200) {   // moved before the handler returns (the file goes then)
            kept = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"FeatherPDF-%@-mac.dmg", ns(version)]];
            [[NSFileManager defaultManager] removeItemAtPath:kept error:nil];
            if (![[NSFileManager defaultManager] moveItemAtURL:file toURL:[NSURL fileURLWithPath:kept] error:nil]) kept = nil;
        }
        NSString* why = err ? err.localizedDescription : [NSString stringWithFormat:@"the server answered with an error (%ld)", (long)status];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!kept) {
                tried = [tried stringByAppendingFormat:@"\n  %@: %@", ns(src.host), why];
                if (++at < list.size()) { attempt(); return; }   // the next place
                [progress close];
                tell([@"The new version could not be downloaded:" stringByAppendingString:tried], YES);
                attempt = nil;
                return;
            }
            attempt = nil;
            [progress close];
            NSData* d = [NSData dataWithContentsOfFile:kept];
            std::vector<unsigned char> bytes(static_cast<const unsigned char*>(d.bytes), static_cast<const unsigned char*>(d.bytes) + d.length);
            if (!update::verify(bytes, sha)) {
                [[NSFileManager defaultManager] removeItemAtPath:kept error:nil];
                tell(@"The downloaded file is not the right one (it may have been damaged on the way), so it was not installed. Try again later.", YES);
                return;
            }
            if (!installAndRestart(kept)) {   // a folder this user cannot change: the disk image opens to drag the app in
                [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:kept]];
                tell(@"Feather PDF cannot replace itself in its folder. The new version's disk image is open: drag Feather PDF onto Applications and choose Replace.", NO);
                return;
            }
            [NSApp terminate:nil];   // asks about unsaved changes as usual; the new version starts when this one has quit
        });
    }];
    [progress.bar unbind:@"doubleValue"];   // from the place tried before
    [progress.bar bind:@"doubleValue" toObject:task.progress withKeyPath:@"fractionCompleted" options:nil];
    [task resume];
    };
    attempt = tryOne;
    tryOne();
}

static void offer(const update::Release& r, BOOL manual, NSString* via) {
    NSString* version = ns(r.version);
    if (!manual && [[[NSUserDefaults standardUserDefaults] stringForKey:kSkipKey] isEqualToString:version]) return;
    NSAlert* a = [[NSAlert alloc] init];
    a.messageText = [NSString stringWithFormat:@"Feather PDF %@ is out", version];
    NSMutableString* info = [NSMutableString stringWithFormat:@"You have %s.%@\n\n", FEATHER_VERSION,
                                                              r.date.empty() ? @"" : [NSString stringWithFormat:@" The new version is from %@.", ns(r.date)]];
    if (!r.notes.empty()) [info appendFormat:@"What is new:\n%@\n\n", nsw(r.notes)];
    [info appendFormat:@"Update Now downloads it (%.1f MB), quits Feather PDF, puts the new version in its place and starts it. Your settings stay.",
                       r.mac.size / 1048576.0];
    a.informativeText = info;
    [a addButtonWithTitle:@"Update Now"];
    [a addButtonWithTitle:@"Later"];
    a.showsSuppressionButton = YES;
    a.suppressionButton.title = @"Do not remind me about this version";
    const NSModalResponse resp = [a runModal];
    if (a.suppressionButton.state == NSControlStateValueOn) [[NSUserDefaults standardUserDefaults] setObject:version forKey:kSkipKey];
    if (resp == NSAlertFirstButtonReturn) downloadAndInstall(r, via);
}

void FPCheckForUpdates(BOOL manual) {
    // the version file from the first place that answers (some networks block raw.githubusercontent.com)
    fetchFastest(update::sources(update::kLatestUrl), ^(NSData* data, NSString* tried, NSInteger status, NSString* via) {
        update::Release r;
        const bool ok = data && update::parse(std::string(static_cast<const char*>(data.bytes), data.length), r);
        {
            if (!ok && status == 404) {   // the server answered: no version has been published there yet
                if (manual) tell([NSString stringWithFormat:@"You have the newest version of Feather PDF (%s).", FEATHER_VERSION], NO);
                return;
            }
            if (!ok) {
                if (manual) tell([NSString stringWithFormat:@"Feather PDF could not find out whether there is a new version.%@\n\nIf the internet works in your browser, a proxy, VPN or security program may be stopping Feather PDF; the new version can also be downloaded from github.com/akshsim1991/FeatherPDF-releases.",
                                                       data ? @"\n\nThe version file that came back could not be read." : [@"\n\nNone of the update servers could be reached:" stringByAppendingString:tried]], YES);
                return;
            }
            if (update::compareVersions(r.version, FEATHER_VERSION) <= 0 || r.mac.url.empty()) {
                if (manual) tell([NSString stringWithFormat:@"You have the newest version of Feather PDF (%s).", FEATHER_VERSION], NO);
                return;
            }
            offer(r, manual, via);
        }
    });
}
