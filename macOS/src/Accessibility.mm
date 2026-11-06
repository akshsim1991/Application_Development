// Tools > Accessibility: Check and Fix, as on Windows: what screen readers, keyboards and magnifiers need, what is
// missing, and the fixes that need no person (title, language, the title shown, tab order, links, field tooltips).
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "FPDocument.h"
#import "Form.h"
#import "Trace.h"
#include <algorithm>
#include "access.hpp"
#include "platform.hpp"
#include "textpdf.hpp"

using form::Field;
using form::Fields;
using form::ns;

@interface FPWindowController (Accessibility)
@end

@implementation FPWindowController (Accessibility)

- (IBAction)accessibilityCheck:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const auto items = a11y::check(*d);
    int failed = 0, person = 0, fixable = 0;
    std::wstring summary, title;
    for (const auto& i : items) {
        if (i.id == "title" && i.result == a11y::Result::Passed && i.detail.size() > 2) title = i.detail.substr(1, i.detail.size() - 2);
        if (i.result == a11y::Result::Passed) continue;
        (i.result == a11y::Result::Failed ? failed : person)++;
        fixable += i.fixable;
        summary += (i.result == a11y::Result::Failed ? L"✗ " : L"? ") + i.rule + L"\n    " + i.detail + (i.fixable ? L" (Fix puts this right.)" : L"") + L"\n\n";
    }
    FPTrace(@"access: %d checks, %d failed, %d for a person, %d fixable", static_cast<int>(items.size()), failed, person, fixable);
    summary = std::to_wstring(items.size() - static_cast<size_t>(failed + person)) + L" of " + std::to_wstring(items.size()) + L" checks passed; " +
              std::to_wstring(failed) + L" not met, " + std::to_wstring(person) + L" for a person to check.\n\n" + summary;
    if (title.empty()) title = a11y::suggestedTitle(*d);
    static const std::vector<std::pair<std::wstring, std::wstring>> langs = {
        {L"English (United States)", L"en-US"}, {L"English (United Kingdom)", L"en-GB"}, {L"English (India)", L"en-IN"}, {L"Hindi", L"hi-IN"},
        {L"Kannada", L"kn-IN"}, {L"Tamil", L"ta-IN"}, {L"Telugu", L"te-IN"}, {L"Malayalam", L"ml-IN"}, {L"Marathi", L"mr-IN"}, {L"Bengali", L"bn-IN"},
        {L"Gujarati", L"gu-IN"}, {L"French", L"fr-FR"}, {L"German", L"de-DE"}, {L"Spanish", L"es-ES"}, {L"Portuguese", L"pt-BR"}, {L"Chinese", L"zh-CN"},
        {L"Japanese", L"ja-JP"}, {L"Arabic", L"ar"}};
    std::vector<std::wstring> names;
    int choice = 0;
    NSString* mine = [NSLocale.currentLocale.localeIdentifier stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
    for (size_t i = 0; i < langs.size(); ++i) {
        names.push_back(langs[i].first);
        if ([ns(langs[i].second) caseInsensitiveCompare:mine] == NSOrderedSame) choice = static_cast<int>(i);
    }
    Fields f(5);
    f[0] = {Field::Note, L"", {}, summary};
    f[1] = {Field::Text, L"Title:", {}, title};
    f[2] = {Field::Choice, L"Language of the text:", names, L"", choice};
    f[3] = {Field::Check, L"Fix what can be fixed now (title, language, tab order, links, field tooltips)"};
    f[3].checked = fixable > 0;
    f[4] = {Field::Check, L"Open the full report"};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Accessibility check", f, L"OK", nullptr, [weak, d](const Fields& v) {
        const bool fix = v[3].checked, report = v[4].checked;   // read now: the answers go with the form
        a11y::FixOptions o;
        o.titleText = v[1].text;
        o.language = langs[static_cast<size_t>(std::clamp(v[2].choice, 0, static_cast<int>(langs.size()) - 1))].second;
        dispatch_async(dispatch_get_main_queue(), ^{
            FPWindowController* me = weak;
            if (!me) return;
            std::wstring msg;
            if (fix) {
                const auto done = a11y::fix(*d, o);
                if (!done.empty()) [me afterChange];
                for (const auto& line : done) msg += L"• " + line + L"\n";
                int still = 0;
                for (const auto& i : a11y::check(*d)) still += i.result == a11y::Result::Failed;
                FPTrace(@"access: fixed %d, still failed %d", static_cast<int>(done.size()), still);
            }
            if (report) {
                std::vector<unsigned char> pdf;
                if (textpdf::build(a11y::report(a11y::check(*d), d->title()), true, pdf).ok)
                    [FPDocument openNewWithBytes:std::move(pdf) name:ns(L"Accessibility report - " + d->title())];
            }
            if (!msg.empty()) form::message(me.window, L"Fixed:\n" + msg + L"\nSave the document to keep the changes; Undo takes them back.");
        });
    });
}

@end
