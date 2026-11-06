// Files attached to the document (open, save a copy, remove, attach another) and its layers (shown or hidden), as on
// Windows (File menu).
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "Form.h"
#import "Trace.h"
#include "attach.hpp"
#include "platform.hpp"

using form::Field;
using form::Fields;
using form::ns;

@interface FPWindowController (Attachments)
@end

@implementation FPWindowController (Attachments)

- (void)attachSay:(const std::wstring&)t { form::message(self.window, t); }

- (IBAction)showAttachments:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const auto files = attach::list(*d);
    std::vector<std::wstring> names;
    std::wstring listText;
    for (const auto& f : files) {
        names.push_back(f.name);
        listText += L"\n  " + f.name + L"  (" + (f.size >= 10240 ? std::to_wstring(f.size / 1024) + L" KB" : std::to_wstring(f.size) + L" bytes") + L")";
        FPTrace(@"attachments: %s %lu", platform::utf8(f.name).c_str(), static_cast<unsigned long>(f.size));
    }
    Fields f;
    f.push_back({Field::Note, L"", {}, files.empty() ? L"No files are attached to this document." : std::to_wstring(files.size()) + L" file(s) attached:" + listText});
    if (!files.empty()) {
        f.push_back({Field::Choice, L"File:", names, L"", 0});
        f.push_back({Field::Choice, L"Do:", {L"Open it", L"Save a copy…", L"Remove it from the document", L"Attach another file…"}, L"", 0});
        if (const char* what = getenv("FEATHER_UI_ATTACH")) f[2].choice = atoi(what);   // the build check
    }
    __weak FPWindowController* weak = self;
    const bool none = files.empty();
    form::show(self.window, L"Attached files", f, none ? L"Attach a File…" : L"OK", nullptr, [weak, none, names](const Fields& v) {
        const int which = none ? 0 : v[1].choice, what = none ? 3 : v[2].choice;   // read now: the answers go with the form
        dispatch_async(dispatch_get_main_queue(), ^{ [weak attachment:which action:what names:names]; });
    });
}

- (void)attachment:(int)which action:(int)what names:(std::vector<std::wstring>)names {
    Document* d = self.pdf;
    if (!d) return;
    __weak FPWindowController* weak = self;
    if (what == 3) {
        NSOpenPanel* p = [NSOpenPanel openPanel];
        p.title = @"Choose the file to attach";
        [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
            if (r != NSModalResponseOK) return;
            const std::wstring path = platform::wide(p.URL.fileSystemRepresentation);
            std::vector<unsigned char> data;
            FPWindowController* me = weak;
            if (!platform::readFile(path, data)) { [me attachSay:L"The file could not be read."]; return; }
            if (!attach::add(*d, path.substr(path.find_last_of(L'/') + 1), data)) { [me attachSay:L"The file could not be attached."]; return; }
            [me afterChange];
        }];
        return;
    }
    if (which < 0 || which >= static_cast<int>(names.size())) return;
    if (what == 2) {
        form::ask(self.window, L"Remove \"" + names[static_cast<size_t>(which)] + L"\" from the document? Undo brings it back until you save.", L"Remove",
                  [weak, d, which](bool yes) { if (yes && attach::remove(*d, which)) [weak afterChange]; });
        return;
    }
    std::vector<unsigned char> data;
    if (!attach::get(*d, which, data)) { [self attachSay:L"The attached file could not be read."]; return; }
    std::wstring name = names[static_cast<size_t>(which)];
    for (auto& c : name) if (c == L'/' || c == L':') c = L'_';
    auto write = [weak, data](const std::wstring& target, bool open) {
        if (Document::writeFile(target, data) != SaveResult::Ok) { [weak attachSay:L"The file could not be written."]; return; }
        FPTrace(@"attachment written: %s", platform::utf8(target).c_str());
        if (open) [NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:ns(target)]];   // macOS asks before running anything downloaded
    };
    if (what == 1) {
        if (const char* to = getenv("FEATHER_UI_ATTACH_TO")) { write(platform::wide(to), false); return; }   // the build check
        NSSavePanel* p = [NSSavePanel savePanel];
        p.nameFieldStringValue = ns(name);
        p.title = @"Save the attached file as";
        [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
            if (r == NSModalResponseOK) write(platform::wide(p.URL.fileSystemRepresentation), false);
        }];
        return;
    }
    NSString* dir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"Feather PDF attachments"];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    write(platform::wide([dir stringByAppendingPathComponent:ns(name)].fileSystemRepresentation), true);
}

- (IBAction)showLayers:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const auto all = layers::list(*d);
    std::wstring now;
    for (const auto& l : all) now += L" " + l.name + (l.visible ? L" on;" : L" off;");
    FPTrace(@"layers:%s", platform::utf8(now).c_str());
    if (all.empty()) { [self attachSay:L"This document has no layers."]; return; }
    Fields f;
    f.push_back({Field::Note, L"", {}, L"Ticked layers are shown and printed; the others are hidden. Saving keeps the choice."});
    const bool flip = getenv("FEATHER_UI_LAYERS_FLIP") != nullptr;   // the build check: every box the other way
    for (const auto& l : all) {
        Field c{Field::Check, l.name};
        c.checked = flip ? !l.visible : l.visible;
        f.push_back(c);
    }
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Layers", f, L"Apply", nullptr, [weak, d, all](const Fields& v) {
        std::vector<std::pair<int, bool>> changes;
        for (size_t i = 0; i < all.size(); ++i) if (v[i + 1].checked != all[i].visible) changes.push_back({all[i].object, v[i + 1].checked});
        if (changes.empty()) return;
        FPWindowController* me = weak;
        if (!layers::setVisible(*d, changes)) { [me attachSay:L"The layers could not be changed in this document."]; return; }
        [me afterChange];
        std::wstring after;
        for (const auto& l : layers::list(*d)) after += L" " + l.name + (l.visible ? L" on;" : L" off;");
        FPTrace(@"layers now:%s", platform::utf8(after).c_str());
    });
}

@end
