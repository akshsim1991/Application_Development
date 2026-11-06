// Form calculations and checks (Forms menu), as on Windows: a field worked out from others (a total), and what a field
// accepts (a number, a range, a date, an e-mail address, a phone number). After each answer: a refused value is put back
// with the reason, and the totals are worked out again.
#import <Cocoa/Cocoa.h>
#import "WindowController.h"
#import "PageView.h"
#import "Form.h"
#import "Trace.h"
#include <algorithm>
#include <map>
#include "formcalc.hpp"
#include "formdata.hpp"
#include "platform.hpp"

using form::Field;
using form::Fields;

namespace {
struct FormLogic { bool known = false, any = false; std::map<std::wstring, std::wstring> good; };
std::map<Document*, FormLogic> g_logic;
bool g_busy = false;
}

@interface FPWindowController (FormCalc)
@end

@implementation FPWindowController (FormCalc)

- (void)formStarting {
    Document* d = self.pdf;
    if (!d) return;
    FormLogic& fl = g_logic[d];
    if (!fl.good.empty()) return;
    for (const auto& f : formdata::list(*d)) fl.good[f.name] = f.value;
}

- (void)formCommitted {
    Document* d = self.pdf;
    if (g_busy || !d) return;
    FormLogic& fl = g_logic[d];
    if (!fl.known) { fl.known = true; fl.any = !formcalc::calculations(*d).empty() || !formcalc::rules(*d).empty(); }
    if (!fl.any) return;
    g_busy = true;
    std::map<std::wstring, std::wstring> now, back;
    for (const auto& f : formdata::list(*d)) now[f.name] = f.value;
    std::wstring msg;
    for (const auto& pr : formcalc::problems(*d)) {
        auto it = fl.good.find(pr.first);
        if (it != fl.good.end() && now[pr.first] == it->second) continue;   // not just typed
        back[pr.first] = it == fl.good.end() ? L"" : it->second;
        msg += L"“" + pr.first + L"”: " + pr.second + L"\n";
    }
    if (!back.empty()) formdata::setValues(*d, back);
    const int changed = formcalc::recalculate(*d);
    fl.good.clear();
    for (const auto& f : formdata::list(*d)) fl.good[f.name] = f.value;
    g_busy = false;
    FPTrace(@"formcalc: committed, %d refused, %d worked out", static_cast<int>(back.size()), changed);
    [self.pageView setNeedsDisplay:YES];
    if (!msg.empty()) form::message(self.window, msg + L"\nThe answer was put back as it was.");
}

- (IBAction)formCalcDialog:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    std::vector<std::wstring> names;
    for (const auto& f : formdata::list(*d))
        if (f.type == FPDF_FORMFIELD_TEXTFIELD && std::find(names.begin(), names.end(), f.name) == names.end()) names.push_back(f.name);
    if (names.empty()) { form::message(self.window, L"This document has no text fields. Add some first."); return; }
    std::wstring now;
    for (const auto& c : formcalc::calculations(*d)) {
        now += L"“" + c.field + L"” = " + formcalc::opName(c.op) + L" of";
        for (const auto& s : c.sources) now += L" " + s;
        now += L"\n";
    }
    Fields f(7);
    f[0] = {Field::Note, L"", {}, (now.empty() ? L"No calculations yet." : L"Now:\n" + now) +
                                  L"\nCalculated fields work out their value from others; checks refuse what does not fit. Other PDF readers do the same."};
    f[1] = {Field::Choice, L"Field:", names, L"", 0};
    f[2] = {Field::Choice, L"It should:", {L"Accept anything", L"Accept a number", L"Accept a number in a range", L"Accept a date", L"Accept an e-mail address",
                                          L"Accept a phone number", L"Be the sum of other fields", L"Be the product of other fields", L"Be the average of other fields",
                                          L"Be the smallest of other fields", L"Be the largest of other fields"}, L"", 1};
    f[3] = {Field::Text, L"Decimals (numbers):", {}, L"2"};
    f[4] = {Field::Text, L"From / to (range):", {}, L"0 / 100"};
    f[5] = {Field::Text, L"Date as:", {}, L"dd/mm/yyyy"};
    f[6] = {Field::Text, L"Of the fields (names, commas):", {}, L""};
    if (const char* t = getenv("FEATHER_UI_FORMCALC")) {   // the build check: "field|choice|sources"
        const std::wstring spec = platform::wide(t);
        const size_t a = spec.find(L'|'), b = spec.find(L'|', a + 1);
        for (size_t i = 0; i < names.size(); ++i) if (names[i] == spec.substr(0, a)) f[1].choice = static_cast<int>(i);
        f[2].choice = static_cast<int>(wcstol(spec.substr(a + 1, b - a - 1).c_str(), nullptr, 10));
        f[6].text = spec.substr(b + 1);
    }
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Calculations and checks", f, L"Apply", [](const Fields& v) -> std::wstring {
        return v[2].choice >= 6 && v[6].text.find_first_not_of(L" ,") == std::wstring::npos ? L"Type the names of the fields to work it out from." : L"";
    }, [weak, d, names](const Fields& v) {
        const std::wstring field = names[static_cast<size_t>(std::clamp(v[1].choice, 0, static_cast<int>(names.size()) - 1))];
        const int what = v[2].choice;
        const std::wstring dec = v[3].text, range = v[4].text, date = v[5].text, list = v[6].text;
        dispatch_async(dispatch_get_main_queue(), ^{
            FPWindowController* me = weak;
            if (!me) return;
            bool ok;
            if (what >= 6) {
                formcalc::Calc c;
                c.field = field;
                c.op = static_cast<formcalc::Op>(what - 6);
                std::wstring cur;
                for (wchar_t ch : list + L",") {
                    if (ch == L',') { while (!cur.empty() && cur.back() == L' ') cur.pop_back(); const size_t s0 = cur.find_first_not_of(L' '); if (s0 != std::wstring::npos) c.sources.push_back(cur.substr(s0)); cur.clear(); }
                    else cur += ch;
                }
                ok = formcalc::setCalculation(*d, c);
                if (ok) formcalc::recalculate(*d);
            } else {
                formcalc::Rule r;
                r.field = field;
                r.kind = static_cast<formcalc::Kind>(what);
                r.decimals = std::clamp(static_cast<int>(wcstol(dec.c_str(), nullptr, 10)), 0, 6);
                const size_t slash = range.find(L'/');
                const std::wstring lo = range.substr(0, slash), hi = slash == std::wstring::npos ? L"" : range.substr(slash + 1);
                r.hasMin = lo.find_first_of(L"0123456789") != std::wstring::npos;
                r.hasMax = hi.find_first_of(L"0123456789") != std::wstring::npos;
                r.min = wcstod(lo.c_str(), nullptr);
                r.max = wcstod(hi.c_str(), nullptr);
                r.dateFormat = date.empty() ? L"dd/mm/yyyy" : date;
                ok = formcalc::setRule(*d, r);
            }
            g_logic.erase(d);
            [me afterChange];
            std::wstring total;
            for (const auto& fl : formdata::list(*d)) if (fl.name == field) total = fl.value;
            FPTrace(@"formcalc: set %d, %s = %s", ok, platform::utf8(field).c_str(), platform::utf8(total).c_str());
            if (!ok) form::message(me.window, L"That could not be set in this document.", true);
        });
    });
}

@end
