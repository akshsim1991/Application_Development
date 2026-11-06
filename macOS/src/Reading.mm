// Print, presentation, remembered reading positions, the status bar text, read aloud, the command palette, visual
// compare and "set as default PDF app": the rest of the Windows app's reading tools, on the Mac.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import "Render.h"
#import "Palette.h"
#import "Trace.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include "platform.hpp"
#include "textdiff.hpp"
#include "textpdf.hpp"
#include "visualdiff.hpp"
#include "readaloud.hpp"
#include <cwctype>

using form::Field;
using form::Fields;
using form::ns;

// ---- printing: macOS's print window (preview, copies, pages, pages per sheet, two-sided, black and white) ----

// Print on both sides of paper, as on Windows: flip on the long edge or on the short edge. A panel of its own in the
// print window (Feather PDF), set on the print job as macOS's own two-sided setting.
@interface FPDuplexPanel : NSViewController <NSPrintPanelAccessorizing>
@property(nonatomic, strong) NSButton *both, *longEdge, *shortEdge;
@end

@implementation FPDuplexPanel
- (void)loadView {
    NSView* v = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 360, 92)];
    self.both = [NSButton checkboxWithTitle:@"Print on both sides of paper" target:self action:@selector(changed:)];
    self.longEdge = [NSButton radioButtonWithTitle:@"Flip on long edge" target:self action:@selector(changed:)];
    self.shortEdge = [NSButton radioButtonWithTitle:@"Flip on short edge" target:self action:@selector(changed:)];
    self.both.frame = NSMakeRect(20, 62, 320, 20);
    self.longEdge.frame = NSMakeRect(44, 36, 300, 20);
    self.shortEdge.frame = NSMakeRect(44, 12, 300, 20);
    for (NSButton* b in @[ self.both, self.longEdge, self.shortEdge ]) [v addSubview:b];
    const NSInteger last = [NSUserDefaults.standardUserDefaults integerForKey:@"printSides"];   // 0 one side, 1 long, 2 short
    self.both.state = last ? NSControlStateValueOn : NSControlStateValueOff;
    (last == 2 ? self.shortEdge : self.longEdge).state = NSControlStateValueOn;
    self.view = v;
    [self apply];
}
- (void)changed:(NSButton*)sender {
    if (sender == self.longEdge) self.shortEdge.state = NSControlStateValueOff;   // the two radio buttons are one choice
    if (sender == self.shortEdge) self.longEdge.state = NSControlStateValueOff;
    [self apply];
}
- (NSInteger)sides { return self.both.state != NSControlStateValueOn ? 0 : self.shortEdge.state == NSControlStateValueOn ? 2 : 1; }
- (void)apply {
    const NSInteger sides = [self sides];
    self.longEdge.enabled = self.shortEdge.enabled = sides != 0;
    [NSUserDefaults.standardUserDefaults setInteger:sides forKey:@"printSides"];
    NSPrintInfo* info = self.representedObject;
    if (![info isKindOfClass:[NSPrintInfo class]]) return;
    [self willChangeValueForKey:@"localizedSummaryItems"];
    PMPrintSettings ps = static_cast<PMPrintSettings>(info.PMPrintSettings);
    // long edge: "no tumble" (the back is upright when the sheet is turned over sideways); short edge: "tumble"
    PMSetDuplex(ps, sides == 0 ? kPMDuplexNone : sides == 1 ? kPMDuplexNoTumble : kPMDuplexTumble);
    [info updateFromPMPrintSettings];
    [self didChangeValueForKey:@"localizedSummaryItems"];
}
- (void)setRepresentedObject:(id)o {
    [super setRepresentedObject:o];
    if (self.isViewLoaded) [self apply];
}
- (NSArray<NSDictionary<NSPrintPanelAccessorySummaryKey, NSString*>*>*)localizedSummaryItems {
    static NSString* names[] = {@"One side", @"Both sides, flip on long edge", @"Both sides, flip on short edge"};
    return @[ @{NSPrintPanelAccessorySummaryItemNameKey : @"Sides", NSPrintPanelAccessorySummaryItemDescriptionKey : names[self.isViewLoaded ? [self sides] : 0]} ];
}
- (NSSet<NSString*>*)keyPathsForValuesAffectingPreview { return [NSSet set]; }
@end

@interface FPPrintView : NSView
@property(nonatomic) Document* doc;
@end

@implementation FPPrintView

- (BOOL)knowsPageRange:(NSRangePointer)range {
    *range = NSMakeRange(1, static_cast<NSUInteger>(_doc ? _doc->pageCount() : 0));
    return YES;
}

- (NSRect)rectForPage:(NSInteger)page {
    const NSSize paper = NSPrintOperation.currentOperation.printInfo.paperSize;
    return NSMakeRect(0, (page - 1) * paper.height, paper.width, paper.height);
}

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirty {
    NSPrintInfo* info = NSPrintOperation.currentOperation.printInfo;
    const NSInteger page = NSPrintOperation.currentOperation.currentPage - 1;
    if (!_doc || page < 0 || page >= _doc->pageCount()) return;
    const NSRect sheet = [self rectForPage:page + 1];
    const NSRect area = NSInsetRect(info.imageablePageBounds, 0, 0);
    const PageSize& s = _doc->pageSize(static_cast<int>(page));
    // the page fits the printable area, centred; turned pages turn the paper (Page Setup) as on Windows' Auto
    const double k = std::min(area.size.width / s.w, area.size.height / s.h);
    const double w = s.w * k, h = s.h * k;
    const NSRect at = NSMakeRect(sheet.origin.x + area.origin.x + (area.size.width - w) / 2,
                                 sheet.origin.y + (sheet.size.height - NSMaxY(area)) + (area.size.height - h) / 2, w, h);
    const int pw = static_cast<int>(std::lround(w / 72.0 * 300)), ph = static_cast<int>(std::lround(h / 72.0 * 300));   // 300 dpi
    CGImageRef img = FPRenderPage(*_doc, static_cast<int>(page), pw, ph, 0, 0, pw, ph, pagecolours::Mode::Normal);
    if (!img) return;
    NSImage* ns = [[NSImage alloc] initWithCGImage:img size:at.size];
    [ns drawInRect:at fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1 respectFlipped:YES hints:nil];
    CGImageRelease(img);
}

@end

@interface FPWindowController (Reading) <AVSpeechSynthesizerDelegate>
@end

static AVSpeechSynthesizer* g_speech = nil;

@implementation FPWindowController (Reading)

- (void)tellUser:(const std::wstring&)t { form::message(self.window, t); }

- (IBAction)printDocument:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    [self.pageView endFormInput];
    d->setFieldHighlight(false);   // fields print without their tint
    FPPrintView* v = [[FPPrintView alloc] initWithFrame:NSMakeRect(0, 0, 612, 792)];
    v.doc = d;
    NSPrintInfo* info = [NSPrintInfo.sharedPrintInfo copy];
    info.horizontalPagination = NSPrintingPaginationModeAutomatic;
    info.verticalPagination = NSPrintingPaginationModeAutomatic;
    const PageSize& s = d->pageSize(std::max(0, [self.pageView currentPage]));
    info.orientation = s.w > s.h ? NSPaperOrientationLandscape : NSPaperOrientationPortrait;
    NSPrintOperation* op = [NSPrintOperation printOperationWithView:v printInfo:info];
    op.jobTitle = ns(d->title());
    op.showsPrintPanel = YES;
    op.showsProgressPanel = YES;
    op.printPanel.options |= NSPrintPanelShowsPreview | NSPrintPanelShowsPageSetupAccessory | NSPrintPanelShowsCopies | NSPrintPanelShowsPageRange;
    FPDuplexPanel* duplex = [[FPDuplexPanel alloc] init];
    duplex.title = @"Both Sides";
    [op.printPanel addAccessoryController:duplex];
    [op runOperationModalForWindow:self.window delegate:self didRunSelector:@selector(printDone:success:contextInfo:) contextInfo:nullptr];
}

- (void)printDone:(NSPrintOperation*)op success:(BOOL)ok contextInfo:(void*)ctx {
    if (Document* d = self.pdf) d->setFieldHighlight(true);
}

// ---- presentation: full screen, one page at a time, fit to the screen; a click or the arrow keys turn the page ----

- (IBAction)startPresentation:(id)sender {
    if (!self.pdf) return;
    [self.pageView setValue:@YES forKey:@"presenting"];
    self.pageView.layout = 0;
    if (!(self.window.styleMask & NSWindowStyleMaskFullScreen)) [self.window toggleFullScreen:nil];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 900 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{ [self.pageView fitPage]; });
}

- (void)windowDidExitFullScreen:(NSNotification*)n {
    if (![[self.pageView valueForKey:@"presenting"] boolValue]) return;
    [self.pageView setValue:@NO forKey:@"presenting"];
    self.pageView.layout = 1;
    self.pageView.fitWidth = YES;
}

// ---- read aloud (the Mac's voices): from the selected text, else from the page shown on; the word being spoken is tinted,
// and a small panel goes back or forward a sentence, pauses, changes the speed, or stops ----

namespace {
struct ReadState {
    readaloud::Script script;
    size_t base = 0, at = 0;
    bool paused = false;
    int words = 0;
} g_read;
__weak FPWindowController* g_reader = nil;
NSPanel* g_readPanel = nil;
NSTextField* g_readLabel = nil;
NSButton* g_pauseButton = nil;
NSTimer* g_fakeTimer = nil;   // the build check's stand-in voice (FEATHER_FAKE_VOICE)
const float kRates[] = {0.38f, 0.5f, 0.56f, 0.62f};
int speedIndex() { NSNumber* r = [NSUserDefaults.standardUserDefaults objectForKey:@"readingSpeed"]; return static_cast<int>(std::clamp<NSInteger>(r ? r.integerValue : 1, 0, 3)); }
}

- (void)readingWordAt:(size_t)pos length:(size_t)len {
    g_read.at = g_read.base + pos;
    int page = -1, start = 0, count = 0;
    if (readaloud::span(g_read.script, g_read.at, len ? len : readaloud::wordLength(g_read.script, g_read.at), page, start, count)) {
        [self.pageView setSpokenPage:page start:start count:count];
        if (++g_read.words <= 3) FPTrace(@"reading: '%s' on page %d", platform::utf8(self.pdf->textRange(page, start, count)).c_str(), page + 1);
    }
    [self updateReadingPanel];
}

- (void)updateReadingPanel {
    int page = -1, start = 0, count = 0;
    readaloud::span(g_read.script, g_read.at, 1, page, start, count);
    static NSString* speeds[] = {@"slow", @"normal", @"fast", @"very fast"};
    g_readLabel.stringValue = [NSString stringWithFormat:@"%@ page %d · %@", g_read.paused ? @"Paused on" : @"Reading", page + 1, speeds[speedIndex()]];
    g_pauseButton.title = g_read.paused ? @"Resume" : @"Pause";
}

- (void)showReadingPanel {
    if (!g_readPanel) {
        g_readPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 560, 64) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow
                                                   backing:NSBackingStoreBuffered defer:NO];
        g_readPanel.title = @"Read Aloud";
        g_readPanel.floatingPanel = YES;
        g_readPanel.hidesOnDeactivate = YES;
        g_readPanel.releasedWhenClosed = NO;
        g_readLabel = [NSTextField labelWithString:@""];
        g_readLabel.frame = NSMakeRect(12, 38, 536, 18);
        [g_readPanel.contentView addSubview:g_readLabel];
        NSArray* titles = @[ @"◀◀ Back", @"Pause", @"Forward ▶▶", @"Slower", @"Faster", @"Stop" ];
        CGFloat x = 8;
        for (NSUInteger i = 0; i < titles.count; ++i) {
            NSButton* b = [NSButton buttonWithTitle:titles[i] target:nil action:@selector(readingButton:)];
            b.tag = static_cast<NSInteger>(i);
            b.frame = NSMakeRect(x, 6, i == 5 ? 70 : 88, 28);
            x += (i == 5 ? 70 : 88) + 4;
            [g_readPanel.contentView addSubview:b];
            if (i == 1) g_pauseButton = b;
        }
        NSRect vf = NSScreen.mainScreen.visibleFrame;
        [g_readPanel setFrameOrigin:NSMakePoint(NSMidX(vf) - 280, NSMinY(vf) + 40)];
    }
    for (NSView* v in g_readPanel.contentView.subviews) if ([v isKindOfClass:[NSButton class]]) ((NSButton*)v).target = self;
    [g_readPanel orderFront:nil];
}

- (void)endReading {
    [g_fakeTimer invalidate];
    g_fakeTimer = nil;
    if (g_speech.speaking) [g_speech stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    g_read = ReadState();
    [g_reader.pageView setSpokenPage:-1 start:0 count:0];
    g_reader = nil;
    [g_readPanel orderOut:nil];
}

- (BOOL)readFrom:(size_t)offset {
    if (offset >= g_read.script.text.size()) { [self endReading]; return NO; }
    [g_fakeTimer invalidate];
    g_fakeTimer = nil;
    if (g_speech.speaking) [g_speech stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    g_read.base = g_read.at = offset;
    g_read.paused = false;
    g_reader = self;
    NSString* text = ns(g_read.script.text.substr(offset));
    if (getenv("FEATHER_FAKE_VOICE")) {   // no sound: the words go by every 120 ms
        __block size_t i = 0;
        const std::wstring t = g_read.script.text.substr(offset);
        __weak FPWindowController* weak = self;
        g_fakeTimer = [NSTimer scheduledTimerWithTimeInterval:0.12 repeats:YES block:^(NSTimer* timer) {
            if (g_read.paused) return;
            while (i < t.size() && std::iswspace(static_cast<wint_t>(t[i]))) ++i;
            if (i >= t.size()) { [weak endReading]; return; }
            size_t j = i;
            while (j < t.size() && !std::iswspace(static_cast<wint_t>(t[j]))) ++j;
            [weak readingWordAt:i length:j - i];
            i = j;
        }];
    } else {
        if (!g_speech) g_speech = [[AVSpeechSynthesizer alloc] init];
        g_speech.delegate = self;
        AVSpeechUtterance* u = [AVSpeechUtterance speechUtteranceWithString:text];
        u.rate = kRates[speedIndex()];
        [g_speech speakUtterance:u];
    }
    [self showReadingPanel];
    [self updateReadingPanel];
    return YES;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)s willSpeakRangeOfSpeechString:(NSRange)r utterance:(AVSpeechUtterance*)u {
    if (g_reader == self) [self readingWordAt:r.location length:r.length];   // UTF-16 offsets: the same as the script's for all but rare characters outside the basic plane
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)s didFinishSpeechUtterance:(AVSpeechUtterance*)u {
    if (g_reader == self && !g_read.paused) [self endReading];
}

- (IBAction)readingButton:(NSButton*)sender {
    switch (sender.tag) {
        case 0: [self readFrom:readaloud::sentence(g_read.script, g_read.at, -1)]; break;
        case 2: [self readFrom:readaloud::sentence(g_read.script, g_read.at, 1)]; break;
        case 1:
            g_read.paused = !g_read.paused;
            if (!g_fakeTimer) { if (g_read.paused) [g_speech pauseSpeakingAtBoundary:AVSpeechBoundaryWord]; else [g_speech continueSpeaking]; }
            [self updateReadingPanel];
            break;
        case 3: case 4:
            [NSUserDefaults.standardUserDefaults setInteger:std::clamp(speedIndex() + (sender.tag == 4 ? 1 : -1), 0, 3) forKey:@"readingSpeed"];
            [self readFrom:readaloud::sentence(g_read.script, g_read.at, 0)];
            break;
        default: [self endReading]; break;
    }
}

- (IBAction)readAloud:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (g_reader) { [g_reader endReading]; return; }
    int fromPage = [self.pageView currentPage], fromChar = 0;
    size_t limit = 200000;
    if (self.pageView.hasSelection) {
        int sp, sc;
        if ([self.pageView selectionStartPage:&sp char:&sc]) { fromPage = sp; fromChar = sc; }
        limit = self.pageView.selectedText.length + 1;
    }
    g_read = ReadState();
    g_read.script = readaloud::build(*d, fromPage, fromChar, limit);
    if (g_read.script.text.find_first_not_of(L" \t\r\n") == std::wstring::npos) {
        [self tellUser:L"There is no text to read here. Scanned pages are pictures: recognise their text first."];
        return;
    }
    FPTrace(@"reading: %d characters from page %d", static_cast<int>(g_read.script.text.size()), fromPage + 1);
    [self readFrom:0];
    if (getenv("FEATHER_UI_READ_FORWARD"))   // the build check: a sentence on after half a second
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
            const size_t to = readaloud::sentence(g_read.script, g_read.at, 1);
            FPTrace(@"reading: forward to '%s'", platform::utf8(g_read.script.text.substr(to, 9)).c_str());
            [self readFrom:to];
        });
}

- (IBAction)stopReading:(id)sender { if (g_reader) [g_reader endReading]; }

- (IBAction)setReadingSpeed:(NSMenuItem*)sender { [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"readingSpeed"]; }

// ---- visual compare: this page and a page of another file, the differences in red and green ----

- (IBAction)compareDocuments:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(3);
    f[0] = {Field::Choice, L"Compare:", {L"The words (a report and a marked copy)", L"How this page looks (overlay)"}, L"", 0};
    f[1] = {Field::Check, L"Words: ignore capital letters"};
    f[2] = {Field::Check, L"Words: ignore punctuation"};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Compare with another document", f, L"Choose the File\u2026", nullptr, [weak](const Fields& v) {
        const bool words = v[0].choice == 0;   // read now: the answers go with the form
        textdiff::Options o;
        o.ignoreCase = v[1].checked;
        o.ignorePunctuation = v[2].checked;
        dispatch_async(dispatch_get_main_queue(), ^{
            FPWindowController* me = weak;
            if (!me) return;
            if (NSString* path = NSProcessInfo.processInfo.environment[@"FEATHER_UI_COMPARE"]) {   // the build check
                [me compareWith:[NSURL fileURLWithPath:path] words:words options:o];
                return;
            }
            NSOpenPanel* p = [NSOpenPanel openPanel];
            p.allowedContentTypes = @[ UTTypePDF ];
            p.title = @"Compare this document with";
            [p beginSheetModalForWindow:me.window completionHandler:^(NSModalResponse r) {
                if (r == NSModalResponseOK) [weak compareWith:p.URL words:words options:o];
            }];
        });
    });
}

- (void)compareWith:(NSURL*)url words:(bool)words options:(textdiff::Options)o {
    Document* d = self.pdf;
    if (!d) return;
    Document other;
    const std::wstring path = platform::wide(url.fileSystemRepresentation);
    const OpenResult res = other.open(path);
    if (res != OpenResult::Ok) { [self tellUser:Document::describe(res, path)]; return; }
    if (words) {
        // word by word across the whole documents: a copy of this one with each change marked, and the report
        std::vector<unsigned char> bytes;
        Document copy;
        if (!d->serialize(bytes) || copy.load(std::move(bytes), "") != OpenResult::Ok) { [self tellUser:L"The comparison copy could not be made."]; return; }
        const textdiff::Result r = textdiff::compare(*d, other, o);
        int replaced = 0, inserted = 0, removed = 0;
        for (const auto& c : r.changes) (c.kind == textdiff::Change::Replaced ? replaced : c.kind == textdiff::Change::Inserted ? inserted : removed)++;
        FPTrace(@"compare: %d changes, %d replaced, %d inserted, %d removed, %d same", static_cast<int>(r.changes.size()), replaced, inserted, removed, r.same);
        if (r.changes.empty()) {
            [self tellUser:L"The words of the two documents are the same (" + std::to_wstring(r.same) + L" words). This compares the text, not how the pages look."];
            return;
        }
        textdiff::mark(copy, r);
        std::vector<unsigned char> marked, report;
        if (copy.serialize(marked)) [FPDocument openNewWithBytes:std::move(marked) name:ns(L"Changes marked - " + d->title())];
        if (textpdf::build(textdiff::report(r, d->title(), other.title()), true, report).ok)
            [FPDocument openNewWithBytes:std::move(report) name:ns(L"Comparison report - " + d->title())];
        [self tellUser:std::to_wstring(r.changes.size()) + L" change(s): " + std::to_wstring(replaced) + L" replaced, " + std::to_wstring(inserted) +
                       L" inserted, " + std::to_wstring(removed) + L" removed; " + std::to_wstring(r.same) + L" words unchanged." +
                       (r.rough ? L" The documents differ a lot, so they were compared page by page." : L"") +
                       L"\n\nTwo new documents: this one with each change marked (the comment of a mark says what it became), and the report."];
        return;
    }
    const int page = [self.pageView currentPage];
    NSString* png = [NSTemporaryDirectory() stringByAppendingPathComponent:@"Feather PDF compare.png"];
    const visualdiff::PageResult vr = visualdiff::overlay(*d, page, other, std::min(page, other.pageCount() - 1), 110, platform::wide(png.UTF8String));
    if (!vr.ok) { [self tellUser:L"The pages could not be compared."]; return; }
    NSImage* img = [[NSImage alloc] initWithContentsOfFile:png];
    NSPanel* w = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 760, 900) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                              backing:NSBackingStoreBuffered defer:NO];
    w.title = [NSString stringWithFormat:@"Compare: page %d — %.2f %% of the page differs (red: only here, green: only in the other file)", page + 1, vr.percent];
    NSScrollView* sv = [[NSScrollView alloc] initWithFrame:w.contentView.bounds];
    sv.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    sv.hasVerticalScroller = sv.hasHorizontalScroller = YES;
    NSImageView* iv = [NSImageView imageViewWithImage:img];
    iv.frame = NSMakeRect(0, 0, img.size.width, img.size.height);
    sv.documentView = iv;
    w.contentView = sv;
    w.releasedWhenClosed = NO;
    [w center];
    [w makeKeyAndOrderFront:nil];
    objc_setAssociatedObject(self, "comparePanel", w, OBJC_ASSOCIATION_RETAIN);
}

@end

// ---- the default app for PDF files (File menu on Windows; here in the app menu) ----

void FPSetAsDefaultPdfApp(NSWindow* window) {
    if (@available(macOS 12.0, *)) {
        [NSWorkspace.sharedWorkspace setDefaultApplicationAtURL:NSBundle.mainBundle.bundleURL toOpenContentType:UTTypePDF completionHandler:^(NSError* e) {
            dispatch_async(dispatch_get_main_queue(), ^{
                form::message(window, e ? L"macOS did not change the default app for PDF files. You can do it in Finder: select a PDF, choose File > Get Info, "
                                          L"then Open with: Feather PDF, and Change All."
                                        : L"Feather PDF now opens PDF files.", e != nil);
            });
        }];
    } else {
        form::message(window, L"In Finder, select a PDF, choose File > Get Info, then Open with: Feather PDF, and Change All.");
    }
}
