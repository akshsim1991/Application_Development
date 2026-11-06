// Repair a damaged PDF (File menu, no document needed) and save a copy for the web (Fast Web View), as on Windows.
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "AppDelegate.h"
#import "FPDocument.h"
#import "WindowController.h"
#import "Form.h"
#import "Trace.h"
#include "platform.hpp"
#include "webview.hpp"

using form::ns;

namespace {

void repairFile(NSURL* url) {
    const std::wstring path = platform::wide(url.fileSystemRepresentation);
    std::vector<unsigned char> bytes;
    const repair::Report r = repair::run(path, bytes);
    FPTrace(@"repair: opened %d fixed %d pages %d problems %d", r.opened, r.fixed, r.pages, static_cast<int>(r.problems.size()));
    std::wstring msg;
    for (const auto& p : r.problems) msg += L"• " + p + L"\n";
    const std::wstring name = path.substr(path.find_last_of(L'/') + 1);
    if (bytes.empty()) { form::message(nil, L"\"" + name + L"\" could not be repaired.\n\n" + msg, true); return; }
    const size_t dot = name.find_last_of(L'.');
    FPDocument* doc = [FPDocument openNewWithBytes:std::move(bytes) name:ns((dot == std::wstring::npos ? name : name.substr(0, dot)) + L" (repaired)")];
    NSWindow* w = doc.windowControllers.firstObject.window;   // the message on the repaired copy's window (a sheet, not a modal alert)
    if (!r.fixed) form::message(w, L"Nothing was wrong with \"" + name + L"\" (" + std::to_wstring(r.pages) + L" pages). A fresh copy is open.");
    else form::message(w, L"\"" + name + L"\" was repaired: " + std::to_wstring(r.pages) + L" page(s) could be read.\n\nWhat was wrong:\n" + msg +
                           L"\nThe repaired copy is open. Look it over, then save it under a new name.");
}

} // namespace

@interface FPAppDelegate (Repair)
@end

@implementation FPAppDelegate (Repair)

- (IBAction)repairDocument:(id)sender {
    if (NSString* f = NSProcessInfo.processInfo.environment[@"FEATHER_UI_REPAIR"]) { repairFile([NSURL fileURLWithPath:f]); return; }   // the build check
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypePDF, UTTypeData ];
    p.title = @"Choose the damaged PDF";
    if ([p runModal] == NSModalResponseOK) repairFile(p.URL);
}

@end

@interface FPWindowController (Web)
@end

@implementation FPWindowController (Web)

- (IBAction)saveForWeb:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (d->isProtected()) { form::message(self.window, L"A password-protected document cannot be made fast for the web. Remove the password first.", true); return; }
    auto write = [self, d](const std::wstring& target) {
        std::vector<unsigned char> out;
        std::wstring why;
        if (!webview::linearize(*d, out, &why)) { form::message(self.window, L"The copy could not be made. " + why, true); return; }
        if (Document::writeFile(target, out) != SaveResult::Ok) { form::message(self.window, L"The copy could not be written there.", true); return; }
        FPTrace(@"web copy: %s %lu bytes", platform::utf8(target).c_str(), static_cast<unsigned long>(out.size()));
        form::message(self.window, L"Saved for the web: the first page shows while the rest is still downloading.");
    };
    if (const char* to = getenv("FEATHER_UI_WEB_TO")) { write(platform::wide(to)); return; }   // the build check
    NSSavePanel* p = [NSSavePanel savePanel];
    std::wstring base = d->title();
    if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
    p.nameFieldStringValue = ns(base + L" (web).pdf");
    p.allowedContentTypes = @[ UTTypePDF ];
    p.title = @"Save a copy for the web (Fast Web View)";
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK) write(platform::wide(p.URL.fileSystemRepresentation));
    }];
}

@end
