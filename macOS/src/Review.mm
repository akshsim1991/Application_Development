// Comments and review (Tools menu), as on Windows: the comments with their replies and status; reply, mark Accepted,
// Rejected, Completed or Cancelled, and the review summary.
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "WindowController.h"
#import "PageView.h"
#import "FPDocument.h"
#import "Form.h"
#import "Trace.h"
#include <algorithm>
#include "platform.hpp"
#include "review.hpp"
#include "textpdf.hpp"

using form::Field;
using form::Fields;
using form::ns;

@interface FPWindowController (Review)
@end

@implementation FPWindowController (Review)

- (IBAction)reviewComments:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    const auto all = review::threads(*d);
    FPTrace(@"review: %d comments", static_cast<int>(all.size()));
    if (all.empty()) { form::message(self.window, L"This document has no comments yet. Add one with a note, or highlight some text."); return; }
    std::wstring list;
    std::vector<std::wstring> labels;
    for (size_t i = 0; i < all.size(); ++i) {
        const auto& t = all[i];
        const std::wstring body = !t.text.empty() ? t.text : t.quoted;
        list += std::to_wstring(i + 1) + L". Page " + std::to_wstring(t.page + 1) + L" · " + t.kind + (t.author.empty() ? L"" : L" by " + t.author) + L" · " +
                (t.state == review::State::None ? L"Open" : review::stateName(t.state)) + L"\n    " + (body.size() > 120 ? body.substr(0, 120) + L"…" : body) + L"\n";
        for (const auto& r : t.replies) list += L"      reply by " + r.author + L": " + (r.text.size() > 100 ? r.text.substr(0, 100) + L"…" : r.text) + L"\n";
        list += L"\n";
        labels.push_back(std::to_wstring(i + 1) + L". Page " + std::to_wstring(t.page + 1) + L": " + (body.size() > 40 ? body.substr(0, 40) + L"…" : body));
    }
    Fields f(5);
    f[0] = {Field::Note, L"", {}, list};
    f[1] = {Field::Choice, L"Comment:", labels, L"", 0};
    f[2] = {Field::Choice, L"Do:", {L"Reply", L"Mark it Accepted", L"Mark it Rejected", L"Mark it Completed", L"Mark it Cancelled", L"Clear its status",
                                   L"Open the review summary", L"Save the review summary as a file…"}, L"", 0};
    f[3] = {Field::Text, L"Reply:", {}, L""};
    f[4] = {Field::Text, L"Your name:", {}, platform::wide(NSFullUserName().UTF8String ?: "Reviewer")};
    if (const char* t = getenv("FEATHER_UI_REVIEW")) {   // the build check: "action|reply text"
        f[2].choice = atoi(t);
        const char* bar = strchr(t, '|');
        if (bar) f[3].text = platform::wide(bar + 1);
    }
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Comments and review", f, L"OK", [](const Fields& v) -> std::wstring {
        return v[2].choice == 0 && v[3].text.empty() ? L"Type the reply." : L"";
    }, [weak, d, all](const Fields& v) {
        const int what = v[2].choice;   // read now: the answers go with the form
        const review::Thread t = all[static_cast<size_t>(std::clamp(v[1].choice, 0, static_cast<int>(all.size()) - 1))];
        const std::wstring text = v[3].text, who = v[4].text;
        dispatch_async(dispatch_get_main_queue(), ^{
            FPWindowController* me = weak;
            if (!me) return;
            if (what >= 6) {
                const std::wstring md = review::summary(*d);
                if (what == 6) {
                    std::vector<unsigned char> pdf;
                    if (textpdf::build(md, true, pdf).ok) [FPDocument openNewWithBytes:std::move(pdf) name:ns(L"Review summary - " + d->title())];
                    return;
                }
                NSSavePanel* p = [NSSavePanel savePanel];
                std::wstring base = d->title();
                if (base.find_last_of(L'.') != std::wstring::npos) base.resize(base.find_last_of(L'.'));
                p.nameFieldStringValue = ns(base + L" (review).md");
                [p beginSheetModalForWindow:me.window completionHandler:^(NSModalResponse r) {
                    if (r != NSModalResponseOK) return;
                    const std::string u = platform::utf8(md);
                    Document::writeFile(platform::wide(p.URL.fileSystemRepresentation), std::vector<unsigned char>(u.begin(), u.end()));
                }];
                return;
            }
            static const review::State states[] = {review::State::None, review::State::Accepted, review::State::Rejected, review::State::Completed,
                                                   review::State::Cancelled, review::State::None};
            const bool ok = what == 0 ? review::reply(*d, t.object, who, text) : review::setState(*d, t.object, states[what], who);
            if (ok) { [me afterChange]; [me.pageView showPage:t.page]; }
            const auto now = review::threads(*d);
            FPTrace(@"review: %s ok %d, first comment has %d replies, status %s", what == 0 ? "reply" : "status", ok,
                    now.empty() ? 0 : static_cast<int>(now[0].replies.size()), now.empty() ? "" : platform::utf8(review::stateName(now[0].state)).c_str());
            if (!ok) form::message(me.window, L"That could not be saved in this document.", true);
        });
    });
}

@end
