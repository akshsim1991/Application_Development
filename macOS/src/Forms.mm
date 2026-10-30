// The Forms menu: making fields (drawn as boxes on the page), finding the blanks of a flat form, filling from a CSV
// sheet, exporting the answers and clearing them. Filling in is done on the page (PageView passes clicks and typing to
// PDFium). The same questions and messages as the Windows app.
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "PageView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <map>
#include <set>
#include "exporter.hpp"
#include "formcreate.hpp"
#include "formdata.hpp"
#include "platform.hpp"

using form::Field;
using form::Fields;
using form::ns;

@interface FPWindowController (Forms)
- (void)formFieldDrawn:(const FPToolEvent&)e;
@end

@implementation FPWindowController (Forms)

- (void)formNote:(const std::wstring&)t { form::message(self.window, t); }

- (BOOL)addFieldSpecs:(const std::vector<formcreate::Spec>&)specs {
    Document* d = self.pdf;
    std::vector<unsigned char> bytes, out;
    std::wstring err;
    if (!d->serialize(bytes) || !formcreate::add(bytes, specs, out, err)) { [self formNote:L"The fields could not be added. " + err]; return NO; }
    d->beginChange();
    if (d->reload(std::move(out)) != OpenResult::Ok) { d->cancelChange(); [self formNote:L"The fields could not be added to this document."]; return NO; }
    formdata::drawRadioLabels(*d, specs);
    [self afterChange];
    return YES;
}

- (IBAction)chooseFormTool:(NSMenuItem*)sender {
    if (!self.pdf) return;
    self.pageView.tool = static_cast<FPTool>(sender.tag);
    [self formNote:L"Draw a box on the page where the field should go."];
}

- (void)formFieldDrawn:(const FPToolEvent&)e {
    Document* d = self.pdf;
    if (e.kind != FPToolEvent::Draw || e.pts.size() < 2) return;
    formcreate::Spec sp;
    sp.page = e.page;
    sp.left = std::min(e.pts[0].x, e.pts[1].x); sp.right = std::max(e.pts[0].x, e.pts[1].x);
    sp.bottom = std::min(e.pts[0].y, e.pts[1].y); sp.top = std::max(e.pts[0].y, e.pts[1].y);
    if (sp.right - sp.left < 6 || sp.top - sp.bottom < 6) return;
    const FPTool t = self.pageView.tool;
    sp.kind = t == FPToolFormText ? formcreate::Spec::Text : t == FPToolFormCheck ? formcreate::Spec::Check : t == FPToolFormRadio ? formcreate::Spec::Radio
            : t == FPToolFormList ? formcreate::Spec::List : t == FPToolFormDate ? formcreate::Spec::Date : formcreate::Spec::Combo;
    const wchar_t* what = t == FPToolFormText ? L"text field" : t == FPToolFormCheck ? L"check box" : t == FPToolFormRadio ? L"radio button group"
                        : t == FPToolFormList ? L"list box" : t == FPToolFormDate ? L"date field" : L"drop-down list";
    const std::wstring base = t == FPToolFormText ? L"Text" : t == FPToolFormCheck ? L"Check" : t == FPToolFormRadio ? L"Group"
                            : t == FPToolFormList ? L"List" : t == FPToolFormDate ? L"Date" : L"Choice";
    std::set<std::wstring> used;
    for (const auto& f : formdata::list(*d)) used.insert(f.name);
    int k = 1;
    while (used.count(base + std::to_wstring(k))) ++k;
    const bool choices = sp.kind == formcreate::Spec::Combo || sp.kind == formcreate::Spec::List || sp.kind == formcreate::Spec::Radio;
    Fields f(choices ? 3 : 2);
    f[0] = {Field::Note, L"", {}, std::wstring(L"Name of the ") + what + L" (it must be different from the other fields)."};
    f[1] = {Field::Text, L"Name:", {}, base + std::to_wstring(k)};
    if (choices) f[2] = {Field::Text, L"Choices (separate them with ;):", {}, L"Yes; No"};
    form::show(self.window, L"Form field", f, L"Add", [used, choices](const Fields& v) -> std::wstring {
        if (v[1].text.empty() || used.count(v[1].text)) return L"Type a name that no other field has.";
        if (choices && v[2].text.find_first_not_of(L"; ") == std::wstring::npos) return L"Type at least one choice.";
        return L"";
    }, [self, sp, choices](const Fields& v) mutable {
        sp.name = v[1].text;
        if (choices) {
            std::wstring cur;
            for (wchar_t c : v[2].text + L";") {
                if (c == L';') {
                    const size_t a = cur.find_first_not_of(L' '), b = cur.find_last_not_of(L' ');
                    if (a != std::wstring::npos) sp.options.push_back(cur.substr(a, b - a + 1));
                    cur.clear();
                } else cur += c;
            }
        } else if (sp.kind == formcreate::Spec::Text && sp.top - sp.bottom > 40) sp.multiline = true;
        [self addFieldSpecs:std::vector<formcreate::Spec>{sp}];
    });
}

- (IBAction)findFormFields:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const std::vector<formcreate::Spec> specs = formdata::detect(*d);
    if (specs.empty()) { [self formNote:L"No blanks were found: lines of underscores, boxes and short lines after a label become fields."]; return; }
    if ([self addFieldSpecs:specs]) [self formNote:std::to_wstring(specs.size()) + L" field(s) were made. Click a field to fill it in."];
}

- (IBAction)resetForm:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (formdata::list(*d).empty()) { [self formNote:L"This document has no form fields."]; return; }
    form::ask(self.window, L"Clear every answer in this form?", L"Clear", [self, d](bool yes) {
        if (!yes) return;
        [self.pageView endFormInput];
        d->beginChange();
        formdata::resetAll(*d);
        std::vector<unsigned char> bytes, out;
        if (d->serialize(bytes) && formcreate::clearRadios(bytes, out)) d->reload(std::move(out));
        [self afterChange];
    });
}

- (IBAction)exportFormAnswers:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    [self.pageView endFormInput];
    const auto rows = formdata::answers(*d);
    if (rows.empty()) { [self formNote:L"This document has no form fields to read."]; return; }
    NSSavePanel* p = [NSSavePanel savePanel];
    std::wstring base = d->title();
    if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
    p.nameFieldStringValue = ns(base + L" (answers).xlsx");
    p.allowedContentTypes = @[ [UTType typeWithFilenameExtension:@"xlsx"], UTTypeCommaSeparatedText ];
    p.allowsOtherFileTypes = YES;
    p.title = @"Export the answers as (.xlsx or .csv)";
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        std::wstring err;
        const bool xlsx = ![p.URL.pathExtension.lowercaseString isEqualToString:@"csv"];
        if (exporter::writeRows(rows, xlsx, platform::wide(p.URL.fileSystemRepresentation), err)) [[NSWorkspace sharedWorkspace] openURL:p.URL];
        else [self formNote:L"The answers could not be saved. " + err];
    }];
}

- (IBAction)fillFromCsv:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (formdata::list(*d).empty()) { [self formNote:L"This document has no form fields to fill."]; return; }
    NSOpenPanel* open = [NSOpenPanel openPanel];
    open.allowedContentTypes = @[ UTTypeCommaSeparatedText, UTTypePlainText ];
    open.title = @"Choose the CSV sheet (the first row names the fields)";
    [open beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        const auto rows = formdata::parseCsv(platform::readTextFile(platform::wide(open.URL.fileSystemRepresentation)));
        if (rows.size() < 2) { [self formNote:L"The sheet needs a first row with the field names and at least one row of answers."]; return; }
        NSOpenPanel* dir = [NSOpenPanel openPanel];
        dir.canChooseFiles = NO;
        dir.canChooseDirectories = YES;
        dir.canCreateDirectories = YES;
        dir.prompt = @"Save Here";
        dir.title = @"Choose the folder for the filled-in copies";
        [dir beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r2) {
            if (r2 != NSModalResponseOK) return;
            const std::wstring folder = platform::wide(dir.URL.fileSystemRepresentation);
            std::vector<unsigned char> original;
            if (!d->serialize(original)) return;
            int made = 0;
            for (size_t row = 1; row < rows.size(); ++row) {
                std::map<std::wstring, std::wstring> values;
                for (size_t c = 0; c < rows[0].size() && c < rows[row].size(); ++c) values[rows[0][c]] = rows[row][c];
                Document copy;
                std::vector<unsigned char> b = original, out;
                if (copy.load(std::move(b), {}) != OpenResult::Ok) continue;
                formdata::setValues(copy, values);
                if (copy.serialize(out) && Document::writeFile(folder + L"/row" + std::to_wstring(row) + L".pdf", out) == SaveResult::Ok) ++made;
            }
            [self formNote:std::to_wstring(made) + L" filled-in copies were saved (row1.pdf, row2.pdf, ...)."];
        }];
    }];
}

@end
