// Print, presentation, remembered reading positions, the status bar text, read aloud, the command palette, visual
// compare and "set as default PDF app": the rest of the Windows app's reading tools, on the Mac.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import "Render.h"
#import "Palette.h"
#import <AVFoundation/AVFoundation.h>
#import <objc/runtime.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <algorithm>
#include <cmath>
#include "platform.hpp"
#include "visualdiff.hpp"

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

// ---- read aloud (the Mac's voices): from the selected text, else from the page shown to the end ----

- (IBAction)readAloud:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (!g_speech) g_speech = [[AVSpeechSynthesizer alloc] init];
    if (g_speech.speaking) [g_speech stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    NSString* text = self.pageView.hasSelection ? self.pageView.selectedText : nil;
    if (!text.length) {
        std::wstring all;
        for (int p = [self.pageView currentPage]; p < d->pageCount() && all.size() < 200000; ++p) all += d->textRange(p, 0, d->charCount(p)) + L"\n";
        text = ns(all);
    }
    if (!text.length) { [self tellUser:L"There is no text to read here. Scanned pages are pictures: recognise their text first."]; return; }
    AVSpeechUtterance* u = [AVSpeechUtterance speechUtteranceWithString:text];
    static const float rates[] = {0.38f, 0.5f, 0.56f, 0.62f};
    NSNumber* r = [NSUserDefaults.standardUserDefaults objectForKey:@"readingSpeed"];
    u.rate = rates[std::clamp<NSInteger>(r ? r.integerValue : 1, 0, 3)];
    [g_speech speakUtterance:u];
}

- (IBAction)stopReading:(id)sender { if (g_speech.speaking) [g_speech stopSpeakingAtBoundary:AVSpeechBoundaryImmediate]; }

- (IBAction)setReadingSpeed:(NSMenuItem*)sender { [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"readingSpeed"]; }

// ---- visual compare: this page and a page of another file, the differences in red and green ----

- (IBAction)compareDocuments:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    p.title = @"Compare this document with";
    const int page = [self.pageView currentPage];
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        Document other;
        const std::wstring path = platform::wide(p.URL.fileSystemRepresentation);
        const OpenResult o = other.open(path);
        if (o != OpenResult::Ok) { [self tellUser:Document::describe(o, path)]; return; }
        NSString* png = [NSTemporaryDirectory() stringByAppendingPathComponent:@"Feather PDF compare.png"];
        const visualdiff::PageResult res = visualdiff::overlay(*d, page, other, std::min(page, other.pageCount() - 1), 110, platform::wide(png.UTF8String));
        if (!res.ok) { [self tellUser:L"The pages could not be compared."]; return; }
        NSImage* img = [[NSImage alloc] initWithContentsOfFile:png];
        NSPanel* w = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 760, 900) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                  backing:NSBackingStoreBuffered defer:NO];
        w.title = [NSString stringWithFormat:@"Compare: page %d — %.2f %% of the page differs (red: only here, green: only in the other file)", page + 1, res.percent];
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
    }];
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
