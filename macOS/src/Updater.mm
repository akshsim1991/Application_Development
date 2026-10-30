#import "Updater.h"
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

// Puts the new app in place of this one after it has quit, then starts it (a small shell script, so nothing of the
// old app is running while it is replaced). False when this app's folder cannot be written to.
static BOOL installAndRestart(NSString* dmg) {
    NSString* app = NSBundle.mainBundle.bundlePath;
    if (![[NSFileManager defaultManager] isWritableFileAtPath:app.stringByDeletingLastPathComponent]) return NO;
    NSString* script =
        @"pid=\"$1\"; dmg=\"$2\"; app=\"$3\"; n=0\n"
        @"while kill -0 \"$pid\" 2>/dev/null && [ $n -lt 1200 ]; do sleep 0.5; n=$((n+1)); done\n"
        @"mnt=$(mktemp -d)\n"
        @"hdiutil attach -nobrowse -readonly -mountpoint \"$mnt\" \"$dmg\" >/dev/null || exit 1\n"
        @"rm -rf \"$app.old\"\n"
        @"if mv \"$app\" \"$app.old\" && ditto \"$mnt/Feather PDF.app\" \"$app\"; then rm -rf \"$app.old\"; else rm -rf \"$app\"; mv \"$app.old\" \"$app\"; fi\n"
        @"hdiutil detach \"$mnt\" >/dev/null\n"
        @"rm -f \"$dmg\"\n"
        @"open \"$app\"\n";
    NSTask* t = [[NSTask alloc] init];
    t.executableURL = [NSURL fileURLWithPath:@"/bin/sh"];
    t.arguments = @[ @"-c", script, @"feather-update", [NSString stringWithFormat:@"%d", NSProcessInfo.processInfo.processIdentifier], dmg, app ];
    NSError* e = nil;
    return [t launchAndReturnError:&e];
}

static void downloadAndInstall(const update::Release& r) {
    FPUpdateProgress* progress = [[FPUpdateProgress alloc] initWithText:[NSString stringWithFormat:@"Downloading Feather PDF %@…", ns(r.version)]];
    [progress showWindow:nil];
    const std::string sha = r.mac.sha256, version = r.version;
    NSURLSessionDownloadTask* task = [NSURLSession.sharedSession downloadTaskWithURL:[NSURL URLWithString:ns(r.mac.url)]
                                                                   completionHandler:^(NSURL* file, NSURLResponse* resp, NSError* err) {
        NSString* kept = nil;
        if (file && !err && [(NSHTTPURLResponse*)resp statusCode] == 200) {   // moved before the handler returns (the file goes then)
            kept = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"FeatherPDF-%@-mac.dmg", ns(version)]];
            [[NSFileManager defaultManager] removeItemAtPath:kept error:nil];
            if (![[NSFileManager defaultManager] moveItemAtURL:file toURL:[NSURL fileURLWithPath:kept] error:nil]) kept = nil;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [progress close];
            if (!kept) { tell(@"The new version could not be downloaded. Check the internet connection and try again.", YES); return; }
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
    if (@available(macOS 10.13, *))
        [progress.bar bind:@"doubleValue" toObject:task.progress withKeyPath:@"fractionCompleted" options:nil];
    [task resume];
}

static void offer(const update::Release& r, BOOL manual) {
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
    if (resp == NSAlertFirstButtonReturn) downloadAndInstall(r);
}

void FPCheckForUpdates(BOOL manual) {
    NSURLSessionDataTask* task = [NSURLSession.sharedSession dataTaskWithURL:[NSURL URLWithString:ns(update::kLatestUrl)]
                                                           completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
        update::Release r;
        const NSInteger status = [resp isKindOfClass:[NSHTTPURLResponse class]] ? [(NSHTTPURLResponse*)resp statusCode] : 0;
        const bool ok = data && !err && status == 200 &&
                        update::parse(std::string(static_cast<const char*>(data.bytes), data.length), r);
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!ok && !err && status == 404) {   // the server answered: no version has been published there yet
                if (manual) tell([NSString stringWithFormat:@"You have the newest version of Feather PDF (%s).", FEATHER_VERSION], NO);
                return;
            }
            if (!ok) {
                if (manual) tell(@"Feather PDF could not find out whether there is a new version. Check the internet connection and try again.", YES);
                return;
            }
            if (update::compareVersions(r.version, FEATHER_VERSION) <= 0 || r.mac.url.empty()) {
                if (manual) tell([NSString stringWithFormat:@"You have the newest version of Feather PDF (%s).", FEATHER_VERSION], NO);
                return;
            }
            offer(r, manual);
        });
    }];
    [task resume];
}
