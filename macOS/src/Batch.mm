// Batch mode (Tools > Batch: Process a Whole Folder), as on Windows: the same steps for every PDF of a folder (and its
// subfolders): text recognition, watermark, compress, greyscale pictures, password, signature with a certificate file.
// Settings can be kept as presets (the same files the command line runs: --batch preset.ini).
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "Form.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include "batch.hpp"
#include "pageobjects.hpp"
#include "platform.hpp"

using form::Field;
using form::Fields;
using form::ns;

namespace {

std::wstring presetsDir() { return platform::appDataDir() + L"/presets"; }

std::vector<std::wstring> presetNames() {
    std::vector<std::wstring> out;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(platform::utf8(presetsDir()), ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".ini") out.push_back(platform::wide(it->path().stem().string()));
    std::sort(out.begin(), out.end());
    return out;
}

NSString* chooseFolder(NSString* title) {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.canChooseFiles = NO;
    p.canChooseDirectories = YES;
    p.canCreateDirectories = YES;
    p.title = title;
    p.message = title;
    return [p runModal] == NSModalResponseOK ? p.URL.path : nil;
}

void runJob(NSWindow* parent, std::shared_ptr<batch::Job> job) {
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 460, 96) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    panel.title = @"Batch";
    NSTextField* label = [NSTextField labelWithString:@"Starting…"];
    label.frame = NSMakeRect(20, 58, 420, 20);
    label.lineBreakMode = NSLineBreakByTruncatingMiddle;
    NSProgressIndicator* bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(20, 34, 320, 16)];
    bar.indeterminate = NO;
    NSButton* stop = [NSButton buttonWithTitle:@"Stop" target:NSApp action:@selector(abortModal)];
    stop.frame = NSMakeRect(352, 26, 88, 32);
    for (NSView* v in @[ label, bar, stop ]) [panel.contentView addSubview:v];
    [panel center];
    NSModalSession session = [NSApp beginModalSessionForWindow:panel];
    bool stopped = false;
    // page by page on the main thread (PDFium is used from one thread), keeping the window answering between files
    const batch::Result r = batch::run(*job, [&](int done, int total, const std::wstring& file) {
        label.stringValue = [NSString stringWithFormat:@"%d of %d: %@", done + 1, total, ns(file).lastPathComponent];
        bar.maxValue = std::max(1, total);
        bar.doubleValue = done;
        if ([NSApp runModalSession:session] != NSModalResponseContinue) stopped = true;
        return !stopped;
    });
    [NSApp endModalSession:session];
    [panel close];
    std::wstring msg = std::to_wstring(r.done) + L" of " + std::to_wstring(r.total) + L" file(s) processed";
    if (r.failed) msg += L", " + std::to_wstring(r.failed) + L" with problems";
    if (r.cancelled) msg += L" (stopped)";
    msg += L".";
    for (size_t i = 0; i < r.problems.size() && i < 10; ++i) msg += L"\n  - " + r.problems[i];
    if (r.problems.size() > 10) msg += L"\n  ... and " + std::to_wstring(r.problems.size() - 10) + L" more";
    if (!r.total) msg = L"There are no PDF files in that folder.";
    if (parent) form::message(parent, msg, r.failed > 0);
    else {
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"Batch";
        a.informativeText = ns(msg);
        [a runModal];
    }
}

void askSteps(NSWindow* parent, std::shared_ptr<batch::Job> job) {
    Fields f(11);
    f[0] = {Field::Check, L"Recognise text (OCR)", {}, L"", 0, job->ocr};
    f[1] = {Field::Text, L"OCR language:", {}, job->ocrLanguage};
    f[2] = {Field::Text, L"Watermark text (empty = none):", {}, job->watermark};
    f[3] = {Field::Choice, L"Compress:", {L"No", L"Balanced", L"High quality", L"Smallest file"}, L"", job->compress};
    f[4] = {Field::Check, L"Greyscale pictures", {}, L"", 0, job->greyscale};
    f[5] = {Field::Password, L"Password to open (empty = none):", {}, job->userPassword};
    f[6] = {Field::Password, L"Owner password (empty = same):", {}, job->ownerPassword};
    f[7] = {Field::Text, L"Sign with certificate file (.p12 / .pfx):", {}, job->certificate};
    f[8] = {Field::Password, L"Certificate password:", {}, job->certificatePassword};
    f[9] = {Field::Check, L"Include subfolders", {}, L"", 0, job->subfolders};
    f[10] = {Field::Text, L"Save these settings as preset:", {}, L""};
    auto validate = [](const Fields& v) -> std::wstring {
        if (!v[2].text.empty() && !pageobj::isLatin(v[2].text)) return L"The watermark can use letters, numbers and common symbols of Western languages.";
        std::error_code ec;
        if (!v[7].text.empty() && !std::filesystem::exists(platform::utf8(v[7].text), ec)) return L"The certificate file was not found.";
        const bool any = v[0].checked || !v[2].text.empty() || v[3].choice > 0 || v[4].checked || !v[5].text.empty() || !v[6].text.empty() || !v[7].text.empty();
        return any ? L"" : L"Choose at least one thing to do.";
    };
    auto done = [parent, job](const Fields& v) {
        job->ocr = v[0].checked;
        job->ocrLanguage = v[1].text.empty() ? L"eng" : v[1].text;
        job->watermark = v[2].text;
        job->compress = v[3].choice;
        job->greyscale = v[4].checked;
        job->userPassword = v[5].text;
        job->ownerPassword = v[6].text;
        job->certificate = v[7].text;
        job->certificatePassword = v[8].text;
        job->subfolders = v[9].checked;
        std::wstring name = v[10].text;
        for (auto& c : name) if (std::wstring(L"\\/:*?\"<>|.").find(c) != std::wstring::npos) c = L'_';
        if (!name.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(platform::utf8(presetsDir()), ec);
            batch::save(presetsDir() + L"/" + name + L".ini", *job);
        }
        dispatch_async(dispatch_get_main_queue(), ^{ runJob(parent, job); });
    };
    if (parent) form::show(parent, L"Batch: what to do with every file", f, L"Start", validate, done);
}

} // namespace

@interface FPWindowController (Batch)
@end

@implementation FPWindowController (Batch)

- (IBAction)batchFolder:(id)sender {
    NSWindow* parent = self.window;
    auto job = std::make_shared<batch::Job>();
    auto next = [parent, job] {
        NSString* in = chooseFolder(@"Choose the folder with the PDF files to process");
        if (!in) return;
        job->inputDir = platform::wide(in.fileSystemRepresentation);
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"Where should the results go?";
        a.informativeText = @"Next to the originals, named \"… (batch).pdf\", or in another folder (the subfolders are kept).";
        [a addButtonWithTitle:@"Next to the Originals"];
        [a addButtonWithTitle:@"Another Folder…"];
        [a addButtonWithTitle:@"Cancel"];
        const NSModalResponse r = [a runModal];
        if (r == NSAlertThirdButtonReturn) return;
        job->outputDir.clear();
        if (r == NSAlertSecondButtonReturn) {
            NSString* out = chooseFolder(@"Choose the folder for the results");
            if (!out) return;
            job->outputDir = platform::wide(out.fileSystemRepresentation);
        }
        askSteps(parent, job);
    };
    const std::vector<std::wstring> names = presetNames();
    if (names.empty()) { next(); return; }
    Fields f(2);
    std::vector<std::wstring> choices{L"New settings"};
    choices.insert(choices.end(), names.begin(), names.end());
    f[0] = {Field::Choice, L"Start from:", choices, L"", 0};
    f[1] = {Field::Check, L"Delete the chosen preset instead"};
    form::show(parent, L"Batch: process a folder", f, L"Next", nullptr, [parent, job, names, next](const Fields& v) {
        if (v[0].choice > 0) {
            const std::wstring file = presetsDir() + L"/" + names[static_cast<size_t>(v[0].choice - 1)] + L".ini";
            if (v[1].checked) {
                std::error_code ec;
                std::filesystem::remove(platform::utf8(file), ec);
                form::message(parent, L"The preset was deleted.");
                return;
            }
            batch::load(file, *job);
        }
        dispatch_async(dispatch_get_main_queue(), ^{ next(); });
    });
}

@end
