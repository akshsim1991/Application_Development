// Text recognition on the Mac: Apple's Vision framework (part of macOS, nothing to download), given to the shared OCR
// code, which draws each page, asks for its words and puts them on the page as invisible text, as on Windows.
// Tools > Recognise Text (OCR) is here too.
#import <Cocoa/Cocoa.h>
#import <Vision/Vision.h>
#import "WindowController.h"
#import "PageView.h"
#import "Form.h"
#include <algorithm>
#include <map>
#include "ocr.hpp"
#include "platform.hpp"

using form::Field;
using form::Fields;
using form::ns;

namespace {

// The Windows app's (Tesseract) language codes, for the command line and batch presets made there.
NSString* visionLanguage(const std::wstring& code) {
    static const std::map<std::wstring, NSString*> known = {
        {L"eng", @"en-US"}, {L"deu", @"de-DE"}, {L"fra", @"fr-FR"}, {L"spa", @"es-ES"}, {L"ita", @"it-IT"}, {L"por", @"pt-BR"},
        {L"chi_sim", @"zh-Hans"}, {L"chi_tra", @"zh-Hant"}, {L"jpn", @"ja-JP"}, {L"kor", @"ko-KR"}, {L"rus", @"ru-RU"},
        {L"ukr", @"uk-UA"}};
    auto it = known.find(code);
    return it != known.end() ? it->second : ns(code);
}

std::vector<std::wstring> visionLanguages() {
    std::vector<std::wstring> out;
    VNRecognizeTextRequest* r = [[VNRecognizeTextRequest alloc] init];
    r.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
    NSArray<NSString*>* list = nil;
    if (@available(macOS 12.0, *)) list = [r supportedRecognitionLanguagesAndReturnError:nil];
    else list = [VNRecognizeTextRequest supportedRecognitionLanguagesForTextRecognitionLevel:VNRequestTextRecognitionLevelAccurate
                                                                                    revision:VNRecognizeTextRequestRevision1 error:nil];
    for (NSString* l in list) out.push_back(form::ws(l));
    return out;
}

bool visionRecognise(const std::vector<unsigned char>& grey, int w, int h, const std::wstring& language,
                     std::vector<ocr::Word>& words, std::wstring& error) {
    @autoreleasepool {
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceGray();
        CFDataRef data = CFDataCreate(nullptr, grey.data(), static_cast<CFIndex>(grey.size()));
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
        CGImageRef img = CGImageCreate(static_cast<size_t>(w), static_cast<size_t>(h), 8, 8, static_cast<size_t>(w), cs, kCGImageAlphaNone,
                                       provider, nullptr, false, kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
        CFRelease(data);
        CGColorSpaceRelease(cs);
        if (!img) { error = L"The page picture could not be made."; return false; }

        VNRecognizeTextRequest* req = [[VNRecognizeTextRequest alloc] init];
        req.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        req.usesLanguageCorrection = YES;
        req.recognitionLanguages = @[ visionLanguage(language) ];
        VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCGImage:img options:@{}];
        NSError* e = nil;
        BOOL ok = [handler performRequests:@[ req ] error:&e];
        if (!ok) {   // a Mac without a graphics processor Vision can use (a virtual machine): again on the processor alone
            req.usesCPUOnly = YES;
            e = nil;
            ok = [[[VNImageRequestHandler alloc] initWithCGImage:img options:@{}] performRequests:@[ req ] error:&e];
        }
        CGImageRelease(img);
        if (!ok) {
            error = L"Text recognition failed: " + form::ws(e.localizedDescription ?: @"unknown reason");
            return false;
        }
        // Vision gives lines; each word's box comes from its range in the line (boxes are 0..1, bottom left 0, 0).
        for (VNRecognizedTextObservation* line in req.results) {
            VNRecognizedText* best = [line topCandidates:1].firstObject;
            if (!best || best.confidence < 0.25f) continue;
            NSString* text = best.string;
            [text enumerateSubstringsInRange:NSMakeRange(0, text.length) options:NSStringEnumerationByWords
                                  usingBlock:^(NSString* word, NSRange range, NSRange, BOOL*) {
                // the word with the punctuation next to it, as Tesseract gives it
                NSRange r = range;
                while (NSMaxRange(r) < text.length && ![[NSCharacterSet whitespaceCharacterSet] characterIsMember:[text characterAtIndex:NSMaxRange(r)]]) r.length++;
                while (r.location > 0 && ![[NSCharacterSet whitespaceCharacterSet] characterIsMember:[text characterAtIndex:r.location - 1]]) { r.location--; r.length++; }
                VNRectangleObservation* box = [best boundingBoxForRange:r error:nil];
                if (!box) return;
                const CGRect b = box.boundingBox;
                ocr::Word wd;
                wd.text = form::ws([text substringWithRange:r]);
                wd.x0 = static_cast<int>(std::lround(CGRectGetMinX(b) * w));
                wd.x1 = static_cast<int>(std::lround(CGRectGetMaxX(b) * w));
                wd.y0 = static_cast<int>(std::lround((1 - CGRectGetMaxY(b)) * h));
                wd.y1 = static_cast<int>(std::lround((1 - CGRectGetMinY(b)) * h));
                if (!words.empty() && words.back().text == wd.text && words.back().x0 == wd.x0) return;   // same word, two ranges
                words.push_back(wd);
            }];
        }
        return true;
    }
}

} // namespace

void FPInstallOcrEngine() {
    ocr::Engine e;
    e.languages = visionLanguages;
    e.recognise = visionRecognise;
    ocr::setEngine(e);
}

@interface FPWindowController (Ocr)
@end

@implementation FPWindowController (Ocr)

- (IBAction)recogniseText:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    std::wstring why;
    if (!ocr::available(why)) { form::message(self.window, why, true); return; }
    const std::vector<std::wstring> langs = ocr::languages();
    Fields f(4);
    f[0] = {Field::Note, L"", {}, L"Finds the words on scanned pages and adds them invisibly, so the pages can be searched, selected and copied. The pages look exactly the same."};
    f[1] = {Field::Choice, L"Language:", langs, L"", 0};
    f[2] = {Field::Text, L"Pages (empty = all):", {}, L""};
    f[3] = {Field::Check, L"Skip pages that already have text"};
    f[3].checked = true;
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Recognise text (OCR)", f, L"Recognise", [d](const Fields& v) -> std::wstring {
        if (v[2].text.empty()) return L"";
        return d->parseRange(v[2].text).empty() ? L"Type page numbers and ranges such as 1-3, 5, 8-." : L"";
    }, [weak, d, langs](const Fields& v) {
        dispatch_async(dispatch_get_main_queue(), ^{ [weak runOcr:d language:langs[static_cast<size_t>(std::clamp(v[1].choice, 0, static_cast<int>(langs.size()) - 1))]
                                                             pages:v[2].text skip:v[3].checked]; });
    });
}

// Page by page on the main thread (the pages are drawn there too), with a window that shows the progress and can stop it.
- (void)runOcr:(Document*)d language:(std::wstring)language pages:(std::wstring)pagesText skip:(bool)skip {
    std::vector<int> pages;
    if (pagesText.empty()) for (int i = 0; i < d->pageCount(); ++i) pages.push_back(i);
    else pages = d->parseRange(pagesText);
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 400, 96) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    panel.title = @"Recognise text (OCR)";
    NSTextField* label = [NSTextField labelWithString:@"Starting text recognition…"];
    label.frame = NSMakeRect(20, 56, 360, 20);
    NSProgressIndicator* bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 34, 260, 16)];
    bar.indeterminate = NO;
    bar.maxValue = static_cast<double>(pages.size());
    NSButton* stop = [NSButton buttonWithTitle:@"Stop" target:nil action:nil];
    stop.frame = NSMakeRect(292, 26, 88, 32);
    BOOL stopped = NO;
    stop.target = NSApp;
    stop.action = @selector(abortModal);
    for (NSView* v in @[ label, bar, stop ]) [panel.contentView addSubview:v];
    [panel center];
    NSModalSession session = [NSApp beginModalSessionForWindow:panel];
    ocr::Result r = ocr::run(*d, pages, language, skip, [&](int done, int total) {
        label.stringValue = [NSString stringWithFormat:@"Recognising text: page %d of %d", done + 1, total];
        bar.doubleValue = done;
        if ([NSApp runModalSession:session] != NSModalResponseContinue) stopped = YES;
        return !stopped;
    });
    [NSApp endModalSession:session];
    [panel close];
    if (r.words > 0) [self afterChange];
    std::wstring msg;
    if (!r.error.empty()) msg = r.error + L"\n\n";
    if (r.words > 0) msg += L"Text was recognised on " + std::to_wstring(r.pagesDone) + L" page" + (r.pagesDone == 1 ? L"" : L"s") + L" (" +
                            std::to_wstring(r.words) + L" words). Save the document to keep the text; Undo (⌘Z) takes it back.";
    else if (r.pagesSkipped == static_cast<int>(pages.size())) msg += L"The pages already have text, so there was nothing to recognise.";
    else if (r.cancelled) msg += L"Text recognition was stopped.";
    else msg += L"No text was found on those pages.";
    if (r.pagesSkipped && r.words > 0) msg += L"\n" + std::to_wstring(r.pagesSkipped) + L" page(s) already had text and were skipped.";
    form::message(self.window, msg, !r.error.empty());
}

@end
