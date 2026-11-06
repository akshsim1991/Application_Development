// The command line of Feather PDF, the same on Windows and macOS: every feature that runs without a window. Used by the
// tests (tests/smoke.sh on Windows, the Mac build check) and by anyone who scripts it. The commands still marked
// Windows-only use parts of the engine that have no Mac version yet (macOS/PORTING.md).
#include "cli.hpp"
#include <algorithm>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>
#include "annot.hpp"
#include "batch.hpp"
#include "compress.hpp"
#include "document.hpp"
#include "exporter.hpp"
#include "foldersearch.hpp"
#include "fontlist.hpp"
#include "formcreate.hpp"
#include "formdata.hpp"
#include "ocr.hpp"
#include "pageobjects.hpp"
#include "pdfa.hpp"
#include "picture.hpp"
#include "platform.hpp"
#include "protect.hpp"
#include "redact.hpp"
#include "sensitive.hpp"
#include "search.hpp"
#include "splitmerge.hpp"
#include "textdiff.hpp"
#include "attach.hpp"
#include "webview.hpp"
#include "access.hpp"
#include "review.hpp"
#include "readaloud.hpp"
#include "formcalc.hpp"
#include "sanitize.hpp"
#include "selftest.hpp"
#include "sign.hpp"
#include "textpdf.hpp"
#include "visualdiff.hpp"

namespace cli {
namespace {
int wint(const wchar_t* s) { return static_cast<int>(std::wcstol(s, nullptr, 10)); }
double wnum(const wchar_t* s) { return std::wcstod(s, nullptr); }
bool endsWith(const std::wstring& s, const wchar_t* tail) {
    const size_t n = std::wcslen(tail);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) if (std::towlower(s[s.size() - n + i]) != std::towlower(tail[i])) return false;
    return true;
}
} // namespace

int run(std::vector<std::wstring> files) {
    // Command line export, without a window: FeatherPDF.exe --export png|jpg|txt|md|docx|xlsx|csv <pdf> <output> [dpi]
    // The exit code is 0 on success, 2 for bad arguments, 3 when the PDF cannot be opened, 4 when the export fails.
    if (!files.empty() && files[0] == L"--export") {
        if (files.size() < 4) return 2;
        static const struct { const wchar_t* name; exporter::Format f; } names[] = {
            {L"png", exporter::Format::Png}, {L"jpg", exporter::Format::Jpeg}, {L"txt", exporter::Format::Text}, {L"md", exporter::Format::Markdown},
            {L"docx", exporter::Format::Word}, {L"xlsx", exporter::Format::ExcelXlsx}, {L"csv", exporter::Format::Csv}};
        int fi = -1;
        for (int i = 0; i < 7; ++i) if (files[1] == names[i].name) fi = i;
        if (fi < 0) return 2;
        Document doc;
        if (doc.open(files[2]) != OpenResult::Ok) return 3;
        std::vector<int> all(doc.pageCount());
        for (int i = 0; i < doc.pageCount(); ++i) all[i] = i;
        exporter::Result r = exporter::run(doc, all, names[fi].f, files[3], files.size() > 4 ? wint(files[4].c_str()) : 150);
        return r.ok ? 0 : 4;
    }

    // Command line compression: FeatherPDF.exe --compress <in.pdf> <out.pdf> [dpi]   (exit code 0 ok, 5 nothing to reduce)
    if (!files.empty() && files[0] == L"--compress") {
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        compress::Options o;
        if (files.size() > 3) o.dpi = std::max(30, wint(files[3].c_str()));
        if (!compress::run(doc, o).changed) return 5;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }

    // Command line protection: FeatherPDF.exe --protect <in.pdf> <out.pdf> <open-password> [owner-password]
    // With an owner password the file is also limited: no printing, copying or changes. Exit code 0 ok.
    if (!files.empty() && files[0] == L"--protect") {
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        protect::Options o;
        o.userPassword = files[3];
        if (files.size() > 4) { o.ownerPassword = files[4]; o.allowPrint = o.allowCopy = o.allowModify = false; }
        if (!doc.setProtection(o)) return 4;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }

    // Command line text recognition: FeatherPDF.exe --ocr <in.pdf> <out.pdf> [language]   (exit code 0 ok, 5 not available, 6 no text found)
    if (!files.empty() && files[0] == L"--ocr") {
        if (files.size() < 3) return 2;
        std::wstring why;
        if (!ocr::available(why)) return 5;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<int> all(doc.pageCount());
        for (int i = 0; i < doc.pageCount(); ++i) all[i] = i;
        ocr::Result r = ocr::run(doc, all, files.size() > 3 ? files[3] : L"eng", true, nullptr);
        if (!r.error.empty()) { std::fprintf(stderr, "%s\n", platform::utf8(r.error).c_str()); return 4; }
        if (r.words == 0) return 6;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }

    // Command line signing: FeatherPDF.exe --sign <in.pdf> <out.pdf> <certificate.pfx> <password>
    // and checking:         FeatherPDF.exe --checksig <file.pdf> <report.txt>   (exit code 0 ok; the report lists each signature)
    if (!files.empty() && files[0] == L"--sign") {
        if (files.size() < 5) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<unsigned char> plain;
        if (!doc.serialize(plain, FPDF_REMOVE_SECURITY)) return 4;
        std::wstring err;
        std::vector<unsigned char> out;
        sign::Options so;
        so.reason = L"Approved";
        if (files.size() > 5 && files[5] != L"-") so.timestampUrl.assign(files[5].begin(), files[5].end());   // time stamp server
        if (files.size() > 6 && files[6] == L"visible") {   // bottom right of the first page
            double cl, cb, cr, ct;
            if (doc.cropBoxOf(0, cl, cb, cr, ct)) { so.visible = true; so.page = 0; so.right = cr - 36; so.left = so.right - 200; so.bottom = cb + 36; so.top = so.bottom + 60; }
        }
#ifdef _WIN32
        HCERTSTORE store = nullptr;
        PCCERT_CONTEXT cert = sign::certFromPfx(files[3], files[4], store, err);
        if (!cert) return 5;
        bool ok = sign::signPdf(plain, cert, so, out, err);
        CertFreeCertificateContext(cert);
        if (store) CertCloseStore(store, 0);
#else
        sign::Signer signer;
        if (!sign::system().signerFromFile) err = L"Signing is not available in this build.";
        if (!sign::system().signerFromFile || !sign::system().signerFromFile(files[3], files[4], signer, err)) {
            std::fprintf(stderr, "%s\n", platform::utf8(err).c_str());
            return 5;
        }
        bool ok = sign::signPdf(plain, signer, so, out, err);
#endif
        if (!ok) {   // leave the reason next to the output file so scripts can show it
            const std::string u = platform::utf8(err);
            std::vector<unsigned char> eb(u.begin(), u.end());
            Document::writeFile(files[2] + L".error.txt", eb);
            return 6;
        }
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--search") {   // --search folder text report.txt
        if (files.size() < 4) return 2;
        const std::wstring dir = platform::appDataDir() + platform::pathSeparator() + L"index";
        foldersearch::Stats st;
        auto hits = foldersearch::search(files[1], files[2], false, dir, nullptr, st);
        std::wstring report = L"files=" + std::to_wstring(st.files) + L" scanned=" + std::to_wstring(st.scanned) + L" cached=" + std::to_wstring(st.cached) + L"\n";
        for (const auto& h : hits) report += h.file + L"|" + std::to_wstring(h.page + 1) + L"|" + h.snippet + L"\n";
        const std::string u = platform::utf8(report);
        std::vector<unsigned char> rb(u.begin(), u.end());
        return Document::writeFile(files[3], rb) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--redact") {   // --redact in.pdf out.pdf phrase : marks, applies, saves and checks
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        int marked = redact::markText(doc, files[3], false);
        std::vector<std::wstring> phrases = redact::collect(doc);
        int pages = marked ? redact::apply(doc) : 0;
        redact::Report r = redact::verify(doc, phrases);
        std::vector<unsigned char> out;
        if (!pages || !doc.serialize(out) || Document::writeFile(files[2], out) != SaveResult::Ok) return 4;
        std::string rep = "marked=" + std::to_string(marked) + " pages=" + std::to_string(pages) + " phrases=" + std::to_string(r.phrases) + " found=" + std::to_string(r.found.size()) + "\n";
        std::vector<unsigned char> rb(rep.begin(), rep.end());
        Document::writeFile(files[2] + L".txt", rb);
        return r.found.empty() ? 0 : 5;
    }
    if (!files.empty() && files[0] == L"--pressureink") {
        // --pressureink in.pdf out.pdf : a stroke from light (left) to hard (right) across page 1
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<annot::PointD> pts;
        std::vector<double> pr;
        for (int i = 0; i <= 40; ++i) { pts.push_back({100.0 + i * 10.0, 400.0}); pr.push_back(i / 40.0); }
        if (!annot::addPressureInk(doc, 0, pts, pr, RGB(0, 0, 0), 12)) return 5;
        std::vector<unsigned char> bytes;
        return doc.serialize(bytes) && Document::writeFile(files[2], bytes) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--formcalc") {
        // --formcalc in.pdf out.txt out.pdf : a total, checks on four fields, values typed, worked out and checked
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::wstring rep;
        formcalc::Calc c{L"total", formcalc::Op::Sum, {L"price", L"tax"}};
        formcalc::Rule qty{L"qty", formcalc::Kind::Range, 0, true, true, 1, 100};
        formcalc::Rule price{L"price", formcalc::Kind::Number, 2};
        formcalc::Rule total{L"total", formcalc::Kind::Number, 2};
        formcalc::Rule mail{L"email", formcalc::Kind::Email};
        formcalc::Rule date{L"date", formcalc::Kind::Date};
        formcalc::Rule phone{L"phone", formcalc::Kind::Phone};
        rep += L"set=" + std::to_wstring(formcalc::setCalculation(doc, c)) + std::to_wstring(formcalc::setRule(doc, qty)) + std::to_wstring(formcalc::setRule(doc, price)) +
               std::to_wstring(formcalc::setRule(doc, total)) + std::to_wstring(formcalc::setRule(doc, mail)) + std::to_wstring(formcalc::setRule(doc, date)) +
               std::to_wstring(formcalc::setRule(doc, phone)) + L"\n";
        for (const auto& k : formcalc::calculations(doc)) { rep += L"calc " + k.field + L"=" + formcalc::opName(k.op); for (const auto& s2 : k.sources) rep += L" " + s2; rep += L"\n"; }
        for (const auto& r : formcalc::rules(doc)) rep += L"rule " + r.field + L" kind=" + std::to_wstring(static_cast<int>(r.kind)) + L" dec=" + std::to_wstring(r.decimals) +
                                                      (r.hasMin ? L" min=" + std::to_wstring(static_cast<int>(r.min)) : L"") + (r.hasMax ? L" max=" + std::to_wstring(static_cast<int>(r.max)) : L"") + L"\n";
        formdata::setValues(doc, {{L"price", L"100.50"}, {L"tax", L"18.09"}, {L"qty", L"500"}, {L"email", L"not an address"}, {L"date", L"31/02/2026"}, {L"phone", L"+91 98450 12345"}});
        rep += L"recalculated=" + std::to_wstring(formcalc::recalculate(doc)) + L"\n";
        for (const auto& f : formdata::list(doc)) if (f.name == L"total") rep += L"total=" + f.value + L"\n";
        for (const auto& pr : formcalc::problems(doc)) rep += L"problem " + pr.first + L": " + pr.second + L"\n";
        rep += L"ok: " + formcalc::check(qty, L"42") + formcalc::check(mail, L"a@b.in") + formcalc::check(date, L"29/02/2028") + L"|\n";
        std::vector<unsigned char> bytes;
        if (!doc.serialize(bytes) || Document::writeFile(files[3], bytes) != SaveResult::Ok) return 4;
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--readaloud") {
        // --readaloud in.pdf out.txt : the script, each word's place on its page, and sentence steps
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        const readaloud::Script s = readaloud::build(doc, 0);
        std::wstring rep = L"chars=" + std::to_wstring(s.text.size()) + L"\n";
        for (const wchar_t* w : {L"Payment", L"Second", L"Final"}) {
            const size_t at = s.text.find(w);
            int page = -1, start = 0, count = 0;
            if (at != std::wstring::npos) readaloud::span(s, at, readaloud::wordLength(s, at), page, start, count);
            rep += std::wstring(w) + L"=" + std::to_wstring(page + 1) + L":" + (page >= 0 ? doc.textRange(page, start, count) : L"") + L"\n";
        }
        const size_t pay = s.text.find(L"Payment");
        rep += L"back=" + s.text.substr(readaloud::sentence(s, pay, -1), 9) + L"|here=" + s.text.substr(readaloud::sentence(s, pay + 3, 0), 9) +
               L"|next=" + s.text.substr(readaloud::sentence(s, pay, 1), 9) + L"\n";
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--review") {
        // --review in.pdf out.txt [out.pdf] : replies to the first comment, accepts it, rejects the second; then the summary
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        auto all = review::threads(doc);
        std::wstring rep = L"threads=" + std::to_wstring(all.size()) + L"\n";
        if (files.size() > 3 && all.size() >= 2) {
            const int first = all[0].object, second = all[1].object;
            const bool a = review::reply(doc, first, L"Asha", L"Agreed, please change it.");
            all = review::threads(doc);   // object numbers stay; read again after each change
            const bool b = review::reply(doc, all[0].object, L"Ravi", L"Done in version 2.");
            all = review::threads(doc);
            const bool c = review::setState(doc, all[0].object, review::State::Accepted, L"Asha");
            all = review::threads(doc);
            const bool e = review::setState(doc, all[1].object, review::State::Rejected, L"Ravi");
            rep += L"done=" + std::to_wstring(a) + std::to_wstring(b) + std::to_wstring(c) + std::to_wstring(e) + L" first=" + std::to_wstring(first) +
                   L" second=" + std::to_wstring(second) + L"\n";
            std::vector<unsigned char> bytes;
            if (!doc.serialize(bytes) || Document::writeFile(files[3], bytes) != SaveResult::Ok) return 4;
        }
        rep += review::summary(doc);
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--access") {
        // --access in.pdf out.txt [fixed.pdf] : id|passed/failed/person|fixable per check; with fixed.pdf, fixes then checks again
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        auto list = [&doc]() {
            std::wstring t;
            for (const auto& i : a11y::check(doc))
                t += platform::wide(i.id) + L"|" + (i.result == a11y::Result::Passed ? L"passed" : i.result == a11y::Result::Failed ? L"failed" : L"person") +
                     (i.fixable ? L"|fixable" : L"") + L"|" + i.detail + L"\n";
            return t;
        };
        std::wstring rep = list();
        if (files.size() > 3) {
            a11y::FixOptions o;
            o.language = L"en-GB";
            rep += L"--- fixed:\n";
            for (const auto& line : a11y::fix(doc, o)) rep += line + L"\n";
            rep += L"--- after:\n" + list();
            std::vector<unsigned char> bytes;
            if (!doc.serialize(bytes) || Document::writeFile(files[3], bytes) != SaveResult::Ok) return 4;
        }
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--history") {
        // --history in.pdf out.txt : three named changes, back to the start, forward one (the undo history's steps)
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        doc.rotatePages({0}, 1);
        doc.insertBlankPage(0, 0);
        doc.reversePages();
        std::wstring rep = L"steps:";
        for (const auto& st : doc.undoSteps()) rep += L" " + st.what + L";";
        rep += L" pages=" + std::to_wstring(doc.pageCount()) + L"\n";
        const bool back = doc.stepBack(3);
        rep += L"back=" + std::to_wstring(back) + L" pages=" + std::to_wstring(doc.pageCount()) + L" undo=" + std::to_wstring(doc.undoSteps().size()) +
               L" redo=" + std::to_wstring(doc.redoSteps().size()) + L" next=" + (doc.redoSteps().empty() ? L"" : doc.redoSteps().front().what) + L"\n";
        const bool fwd = doc.stepBack(-1);
        rep += L"forward=" + std::to_wstring(fwd) + L" pages=" + std::to_wstring(doc.pageCount()) + L" undo=" + std::to_wstring(doc.undoSteps().size()) +
               L" redo=" + std::to_wstring(doc.redoSteps().size()) + L"\n";
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--repair") {
        // --repair in.pdf out.pdf : the report next to it (out.pdf.txt)
        if (files.size() < 3) return 2;
        std::vector<unsigned char> out;
        const repair::Report r = repair::run(files[1], out);
        std::wstring rep = L"opened=" + std::to_wstring(r.opened) + L" fixed=" + std::to_wstring(r.fixed) + L" pages=" + std::to_wstring(r.pages) +
                           L" bad=" + std::to_wstring(r.badPages) + L"\n";
        for (const auto& p : r.problems) rep += p + L"\n";
        const std::string u = platform::utf8(rep);
        Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end()));
        if (out.empty()) return 5;
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--linearize") {
        // --linearize in.pdf out.pdf
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<unsigned char> out;
        std::wstring why;
        if (!webview::linearize(doc, out, &why)) { const std::string u = platform::utf8(why); Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end())); return 5; }
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--attach") {
        // --attach in.pdf list out.txt | add file out.pdf | get N outfile | remove N out.pdf
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        if (files[2] == L"list") {
            std::wstring rep;
            for (const auto& f : attach::list(doc)) rep += f.name + L"|" + std::to_wstring(f.size) + L"\n";
            const std::string u = platform::utf8(rep);
            return Document::writeFile(files[3], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
        }
        if (files[2] == L"get" && files.size() > 4) {
            std::vector<unsigned char> b;
            return attach::get(doc, static_cast<int>(wcstol(files[3].c_str(), nullptr, 10)), b) && Document::writeFile(files[4], b) == SaveResult::Ok ? 0 : 4;
        }
        bool ok = false;
        std::wstring out;
        if (files[2] == L"add" && files.size() > 4) {
            std::vector<unsigned char> data;
            if (!platform::readFile(files[3], data)) return 3;
            ok = attach::add(doc, files[3].substr(files[3].find_last_of(L"/\\") + 1), data);
            out = files[4];
        } else if (files[2] == L"remove" && files.size() > 4) {
            ok = attach::remove(doc, static_cast<int>(wcstol(files[3].c_str(), nullptr, 10)));
            out = files[4];
        }
        std::vector<unsigned char> bytes;
        return ok && doc.serialize(bytes) && Document::writeFile(out, bytes) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--layers") {
        // --layers in.pdf out.txt [out.pdf name=on|off ...] : name|on or off per layer (after the changes)
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        if (files.size() > 4) {
            std::vector<std::pair<int, bool>> ch;
            for (size_t i = 4; i < files.size(); ++i) {
                const size_t eq = files[i].rfind(L'=');
                if (eq == std::wstring::npos) return 2;
                for (const auto& l : layers::list(doc)) if (l.name == files[i].substr(0, eq)) ch.push_back({l.object, files[i].substr(eq + 1) == L"on"});
            }
            if (ch.empty() || !layers::setVisible(doc, ch)) return 5;
            std::vector<unsigned char> bytes;
            if (!doc.serialize(bytes) || Document::writeFile(files[3], bytes) != SaveResult::Ok) return 4;
        }
        std::wstring rep;
        for (const auto& l : layers::list(doc)) rep += l.name + L"|" + (l.visible ? L"on" : L"off") + L"\n";
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--textdiff") {
        // --textdiff a.pdf b.pdf report.txt [marked.pdf] [report.pdf] : summary line, then the report (Markdown)
        if (files.size() < 4) return 2;
        Document a, b;
        if (a.open(files[1]) != OpenResult::Ok || b.open(files[2]) != OpenResult::Ok) return 3;
        const textdiff::Result r = textdiff::compare(a, b);
        int rep = 0, ins = 0, rem = 0;
        for (const auto& c : r.changes) (c.kind == textdiff::Change::Replaced ? rep : c.kind == textdiff::Change::Inserted ? ins : rem)++;
        const std::wstring md = textdiff::report(r, a.title(), b.title());
        std::wstring out = L"changes=" + std::to_wstring(r.changes.size()) + L" replaced=" + std::to_wstring(rep) + L" inserted=" + std::to_wstring(ins) +
                           L" removed=" + std::to_wstring(rem) + L" same=" + std::to_wstring(r.same) + L" rough=" + std::to_wstring(r.rough) + L"\n" + md;
        if (files.size() > 4) {
            std::vector<unsigned char> bytes;
            Document copy;
            if (!a.serialize(bytes) || copy.load(std::move(bytes), "") != OpenResult::Ok) return 4;
            out += L"marks=" + std::to_wstring(textdiff::mark(copy, r)) + L"\n";
            if (!copy.serialize(bytes) || Document::writeFile(files[4], bytes) != SaveResult::Ok) return 4;
        }
        if (files.size() > 5) {
            std::vector<unsigned char> pdf;
            if (!textpdf::build(md, true, pdf).ok || Document::writeFile(files[5], pdf) != SaveResult::Ok) return 4;
        }
        const std::string u = platform::utf8(out);
        return Document::writeFile(files[3], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--split") {
        // --split in.pdf outstem every N | size MB | bookmarks LEVEL : outstem-1.pdf ... and outstem.txt (file|pages|names)
        if (files.size() < 5) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<splitmerge::Part> parts;
        const double v = wcstod(files[4].c_str(), nullptr);
        if (files[3] == L"every") parts = splitmerge::everyN(doc.pageCount(), static_cast<int>(v));
        else if (files[3] == L"size") parts = splitmerge::bySize(doc, v * 1024 * 1024);
        else if (files[3] == L"bookmarks") parts = splitmerge::byBookmarks(doc, std::max(1, static_cast<int>(v)));
        if (parts.empty()) return 5;
        std::wstring rep;
        for (size_t i = 0; i < parts.size(); ++i) {
            std::vector<unsigned char> b;
            const std::wstring file = files[2] + L"-" + std::to_wstring(i + 1) + L".pdf";
            if (!splitmerge::partBytes(doc, parts[i].pages, b) || Document::writeFile(file, b) != SaveResult::Ok) return 4;
            rep += std::to_wstring(i + 1) + L"|" + std::to_wstring(parts[i].pages.front() + 1) + L"-" + std::to_wstring(parts[i].pages.back() + 1) +
                   L"|" + parts[i].name + L"\n";
        }
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--merge") {
        // --merge out.pdf keep|files|none in1.pdf in2.pdf ...
        if (files.size() < 5) return 2;
        std::vector<std::unique_ptr<Document>> docs;
        std::vector<splitmerge::Source> src;
        for (size_t i = 3; i < files.size(); ++i) {
            docs.push_back(std::make_unique<Document>());
            if (docs.back()->open(files[i]) != OpenResult::Ok) return 3;
            std::wstring name = files[i].substr(files[i].find_last_of(L"/\\") + 1);
            if (name.size() > 4) name.resize(name.size() - 4);
            src.push_back({docs.back().get(), name});
        }
        const splitmerge::Marks m = files[2] == L"files" ? splitmerge::Marks::UnderFileNames : files[2] == L"none" ? splitmerge::Marks::None : splitmerge::Marks::Keep;
        std::vector<unsigned char> out;
        if (!splitmerge::merge(src, m, out)) return 4;
        return Document::writeFile(files[1], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--interleave") {
        // --interleave in.pdf out.pdf reversed|same [backs.pdf]
        if (files.size() < 4) return 2;
        Document doc, backs;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        if (files.size() > 4 && backs.open(files[4]) != OpenResult::Ok) return 3;
        std::vector<unsigned char> out;
        if (!splitmerge::interleave(doc, files.size() > 4 ? &backs : nullptr, files[3] == L"reversed", out)) return 4;
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--find") {
        // --find in.pdf out.txt query [case] [word] [wild] [comments] : page|matched text|"comment" or "page"
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        search::Options o;
        for (size_t i = 4; i < files.size(); ++i) {
            o.matchCase = o.matchCase || files[i] == L"case";
            o.wholeWord = o.wholeWord || files[i] == L"word";
            o.wildcards = o.wildcards || files[i] == L"wild";
            o.comments = o.comments || files[i] == L"comments";
        }
        const std::vector<TextHit> hits = search::find(doc, files[3], o);
        std::wstring rep = L"count=" + std::to_wstring(hits.size()) + L"\n";
        for (const auto& h : hits) {
            const std::wstring src = h.annot >= 0 ? search::commentText(doc, h) : doc.textRange(h.page, h.start, h.count);
            const std::wstring txt = h.annot >= 0 ? src.substr(static_cast<size_t>(h.start), static_cast<size_t>(h.count)) : src;
            rep += std::to_wstring(h.page + 1) + L"|" + txt + L"|" + (h.annot >= 0 ? L"comment" : L"page") + L"\n";
        }
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--sensitive") {
        // --sensitive in.pdf report.txt [out.pdf] : lists e-mail, phone, Aadhaar, PAN and bank details (page|kind|text);
        // with out.pdf, redacts them all, saves and checks that none is left
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        const std::vector<sensitive::Hit> hits = sensitive::scan(doc);
        std::wstring rep = L"found=" + std::to_wstring(hits.size()) + L"\n";
        for (const auto& h : hits) rep += std::to_wstring(h.page + 1) + L"|" + sensitive::name(h.kind) + L"|" + h.text + L"\n";
        int rc = 0;
        if (files.size() >= 4 && !hits.empty()) {
            const int marked = sensitive::mark(doc, hits);
            std::vector<std::wstring> phrases;
            for (const auto& h : hits) if (std::find(phrases.begin(), phrases.end(), h.text) == phrases.end()) phrases.push_back(h.text);
            const int pages = marked ? redact::apply(doc) : 0;
            const redact::Report r = redact::verify(doc, phrases);
            std::vector<unsigned char> out;
            if (!pages || !doc.serialize(out) || Document::writeFile(files[3], out) != SaveResult::Ok) return 4;
            rep += L"marked=" + std::to_wstring(marked) + L" pages=" + std::to_wstring(pages) + L" left=" + std::to_wstring(r.found.size()) + L"\n";
            rc = r.found.empty() ? 0 : 5;
        }
        const std::string u = platform::utf8(rep);
        if (Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) != SaveResult::Ok) return 4;
        return rc;
    }
    if (!files.empty() && files[0] == L"--batch") {   // --batch preset.ini : runs the saved preset on its folders, writes preset.ini.log
        if (files.size() < 2) return 2;
        batch::Job job;
        if (!batch::load(files[1], job) || job.inputDir.empty()) return 3;
        batch::Result r = batch::run(job, nullptr);
        std::wstring log = L"total=" + std::to_wstring(r.total) + L" done=" + std::to_wstring(r.done) + L" failed=" + std::to_wstring(r.failed) + L"\r\n";
        for (const auto& p : r.problems) log += p + L"\r\n";
        const std::string u = platform::utf8(log);
        Document::writeFile(files[1] + L".log", std::vector<unsigned char>(u.begin(), u.end()));
        return r.failed ? 5 : 0;
    }
    if (!files.empty() && (files[0] == L"--formdetect" || files[0] == L"--formexport" || files[0] == L"--formfill")) {
        // --formdetect in.pdf out.pdf        adds fillable fields where a flat form has blanks
        // --formexport in.pdf out.csv|xlsx   writes the answers
        // --formfill in.pdf data.csv outdir  one filled-in copy per row of the CSV file
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        if (files[0] == L"--formdetect") {
            auto specs = formdata::detect(doc);
            std::vector<unsigned char> bytes, out;
            std::wstring err;
            std::wstring dump;
            for (const auto& sp : specs) dump += (sp.kind == formcreate::Spec::Text ? L"text|" : L"check|") + sp.name + L"|" + std::to_wstring(sp.page + 1) + L"\n";
            { const std::string u = platform::utf8(dump); Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end())); }
            if (specs.empty() || !doc.serialize(bytes) || !formcreate::add(bytes, specs, out, err)) return 5;
            return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
        }
        if (files[0] == L"--formexport") {
            auto rows = formdata::answers(doc);
            if (rows.empty()) return 5;
            std::wstring err;
            const std::wstring& o = files[2];
            return exporter::writeRows(rows, o.size() > 5 && endsWith(o, L".xlsx"), o, err) ? 0 : 4;
        }
        if (files.size() < 4) return 2;
        auto rows = formdata::parseCsv(platform::readTextFile(files[2]));
        std::vector<unsigned char> original;
        if (rows.size() < 2 || !doc.serialize(original)) return 5;
        platform::makeDirectory(files[3]);
        for (size_t r = 1; r < rows.size(); ++r) {
            std::map<std::wstring, std::wstring> values;
            for (size_t c = 0; c < rows[0].size() && c < rows[r].size(); ++c) values[rows[0][c]] = rows[r][c];
            Document copy;
            std::vector<unsigned char> bytes = original, out;
            if (copy.load(std::move(bytes), {}) != OpenResult::Ok) return 6;
            formdata::setValues(copy, values);
            if (!copy.serialize(out) || Document::writeFile(files[3] + platform::pathSeparator() + L"row" + std::to_wstring(r) + L".pdf", out) != SaveResult::Ok) return 4;
        }
        return 0;
    }
    if (!files.empty() && files[0] == L"--visualdiff") {   // --visualdiff a.pdf b.pdf out.png [page] : overlay of one page pair
        if (files.size() < 4) return 2;
        Document da, db;
        if (da.open(files[1]) != OpenResult::Ok || db.open(files[2]) != OpenResult::Ok) return 3;
        const int page = files.size() > 4 ? wint(files[4].c_str()) - 1 : 0;
        visualdiff::PageResult r = visualdiff::overlay(da, page, db, page, 100, files[3]);
        std::string rep = "ok=" + std::to_string(r.ok) + " pixels=" + std::to_string(r.differingPixels) + "\n";
        Document::writeFile(files[3] + L".txt", std::vector<unsigned char>(rep.begin(), rep.end()));
        return r.ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--topdf") {   // --topdf in.txt|in.md out.pdf
        if (files.size() < 3) return 2;
        std::wstring ext = files[1].substr(files[1].find_last_of(L'.') == std::wstring::npos ? files[1].size() : files[1].find_last_of(L'.'));
        for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
        std::vector<unsigned char> bytes;
        textpdf::Result r = textpdf::build(platform::readTextFile(files[1]), ext != L".txt", bytes);
        if (!r.ok) return 4;
        return Document::writeFile(files[2], bytes) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--sanitize") {   // --sanitize in.pdf out.pdf [comments] [bookmarks] : writes out.pdf.txt with the report
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        sanitize::Options o;
        for (size_t i = 3; i < files.size(); ++i) { if (files[i] == L"comments") o.comments = true; if (files[i] == L"bookmarks") o.bookmarks = true; }
        std::vector<unsigned char> bytes, out;
        sanitize::Report rep;
        std::wstring err;
        if (!doc.serialize(bytes) || !sanitize::run(bytes, o, out, rep, err)) return 4;
        std::wstring txt;
        for (const auto& r : rep.removed) txt += r + L"\n";
        const std::string u = platform::utf8(txt);
        Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end()));
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--pages") {   // --pages in.pdf out.pdf reverse|noblanks|nodupes|header|background : writes out.pdf.txt
        if (files.size() < 4) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<int> all(doc.pageCount());
        for (int i = 0; i < doc.pageCount(); ++i) all[i] = i;
        std::string rep;
        const std::wstring& op = files[3];
        if (op == L"reverse") doc.reversePages();
        else if (op == L"noblanks") { auto b = pageobj::blankPages(doc); for (int x : b) rep += std::to_string(x + 1) + " "; if (!b.empty()) doc.deletePages(b); }
        else if (op == L"nodupes") {
            std::vector<int> extra;
            for (auto& g : pageobj::duplicatePages(doc)) { for (int x : g) rep += std::to_string(x + 1) + " "; rep += "| "; for (size_t k = 1; k < g.size(); ++k) extra.push_back(g[k]); }
            if (!extra.empty()) doc.deletePages(extra);
        } else if (op == L"header") {
            pageobj::HeaderFooter hf;
            hf.text[1] = L"Confidential report"; hf.text[3] = L"{file}"; hf.text[5] = L"Page {page} of {pages}";
            pageobj::addHeaderFooter(doc, all, hf);
        } else if (op == L"background") pageobj::addBackground(doc, all, RGB(0xFF, 0xF8, 0xD0));
        else return 2;
        Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(rep.begin(), rep.end()));
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--formtest") {   // --formtest in.pdf out : writes out-filled.pdf, out-reset.pdf, out-flat.pdf and out.txt
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<formcreate::Spec> specs(5);
        specs[0].kind = formcreate::Spec::Radio; specs[0].name = L"size"; specs[0].options = {L"Small", L"Medium", L"Large"};
        specs[0].left = 72; specs[0].right = 220; specs[0].bottom = 560; specs[0].top = 640;
        specs[1].kind = formcreate::Spec::List; specs[1].name = L"colour"; specs[1].options = {L"Red", L"Green", L"Blue"};
        specs[1].left = 260; specs[1].right = 400; specs[1].bottom = 560; specs[1].top = 640;
        specs[2].kind = formcreate::Spec::Date; specs[2].name = L"when";
        specs[2].left = 72; specs[2].right = 220; specs[2].bottom = 520; specs[2].top = 540;
        specs[3].kind = formcreate::Spec::Text; specs[3].name = L"who";
        specs[3].left = 260; specs[3].right = 400; specs[3].bottom = 520; specs[3].top = 540;
        specs[4].kind = formcreate::Spec::Check; specs[4].name = L"agree";
        specs[4].left = 72; specs[4].right = 86; specs[4].bottom = 490; specs[4].top = 504;
        std::vector<unsigned char> bytes, out;
        std::wstring err;
        if (!doc.serialize(bytes) || !formcreate::add(bytes, specs, out, err)) return 5;
        Document f;
        if (f.load(std::move(out), {}) != OpenResult::Ok) return 6;
        formdata::drawRadioLabels(f, specs);
        std::wstring rep;
        auto dump = [&](const wchar_t* label) {
            rep += std::wstring(label) + L":";
            for (const auto& fl : formdata::list(f)) rep += L" " + fl.name + L"=" + fl.value + (fl.checked ? L"[x]" : L"");
            rep += L"\n";
        };
        dump(L"new");
        formdata::setValues(f, {{L"size", L"Medium"}, {L"colour", L"Green"}, {L"when", L"25.10.2026"}, {L"who", L"Ann"}, {L"agree", L"yes"}});
        dump(L"filled");
        if (f.saveAs(files[2] + L"-filled.pdf") != SaveResult::Ok) return 4;
        formdata::resetAll(f);
        if (f.serialize(bytes) && formcreate::clearRadios(bytes, out)) f.reload(std::move(out));
        dump(L"reset");
        if (f.saveAs(files[2] + L"-reset.pdf") != SaveResult::Ok) return 4;
        Document g;
        if (g.open(files[2] + L"-filled.pdf") != OpenResult::Ok) return 6;
        rep += L"flattened pages: " + std::to_wstring(g.flattenAll()) + L"\n";
        if (g.saveAs(files[2] + L"-flat.pdf") != SaveResult::Ok) return 4;
        { Document h; h.open(files[2] + L"-flat.pdf"); rep += L"fields after flatten: " + std::to_wstring(formdata::list(h).size()) + L"\n"; }
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--pictest") {   // --pictest in.pdf outprefix [replacement.png] : one file per picture tool, and outprefix.txt
        if (files.size() < 3) return 2;
        std::string rep;
        auto run = [&](const char* name, auto op) {
            Document doc;
            if (doc.open(files[1]) != OpenResult::Ok) { rep += std::string(name) + " open-failed\n"; return; }
            int obj = -1;
            FPDF_PAGE pg = doc.page(0);
            for (int i = 0; pg && i < pdfium::api().FPDFPage_CountObjects(pg); ++i)
                if (pdfium::api().FPDFPageObj_GetType(pdfium::api().FPDFPage_GetObject(pg, i)) == FPDF_PAGEOBJ_IMAGE) { obj = i; break; }
            const bool ok = obj >= 0 && op(doc, obj);
            float l = 0, b = 0, r = 0, t = 0;
            int newObj = obj;
            if (std::string(name) == "front") newObj = pdfium::api().FPDFPage_CountObjects(doc.page(0)) - 1;
            if (std::string(name) == "back") newObj = 0;
            FPDF_PAGEOBJECT o = newObj >= 0 ? pdfium::api().FPDFPage_GetObject(doc.page(0), newObj) : nullptr;
            if (o) pdfium::api().FPDFPageObj_GetBounds(o, &l, &b, &r, &t);
            char line[200];
            snprintf(line, sizeof line, "%s %s index=%d box=%.0f,%.0f,%.0f,%.0f\n", name, ok ? "ok" : "FAILED", newObj, l, b, r, t);
            rep += line;
            if (ok) doc.saveAs(files[2] + L"-" + std::wstring(name, name + strlen(name)) + L".pdf");
        };
        auto boxOf = [](Document& d, int obj, float& l, float& b, float& r, float& t) { pdfium::api().FPDFPageObj_GetBounds(pdfium::api().FPDFPage_GetObject(d.page(0), obj), &l, &b, &r, &t); };
        run("original", [](Document&, int) { return true; });
        run("rotate", [](Document& d, int o) { return pageobj::rotateGroup(d, 0, {o}, 1); });
        run("flip", [](Document& d, int o) { return pageobj::flipGroup(d, 0, {o}, true); });
        run("crop", [&](Document& d, int o) { float l, b, r, t; boxOf(d, o, l, b, r, t); return pageobj::cropPicture(d, 0, o, l, b, (l + r) / 2, (b + t) / 2); });
        run("rotatecrop", [&](Document& d, int o) { float l, b, r, t; if (!pageobj::rotateGroup(d, 0, {o}, 1)) return false; boxOf(d, o, l, b, r, t); return pageobj::cropPicture(d, 0, o, l, b, (l + r) / 2, t); });
        run("bright", [](Document& d, int o) { return pageobj::adjustPicture(d, 0, o, 40, 20, false); });
        run("grey", [](Document& d, int o) { return pageobj::adjustPicture(d, 0, o, 0, 0, true); });
        run("front", [](Document& d, int o) { std::vector<int> v{o}; return pageobj::restackGroup(d, 0, v, true); });
        run("back", [](Document& d, int o) { std::vector<int> v{o}; return pageobj::restackGroup(d, 0, v, false); });
        if (files.size() > 3) { std::wstring err; const std::wstring with = files[3]; run("replace", [&](Document& d, int o) { return pageobj::replacePicture(d, 0, o, with, err); }); }
        run("extract", [&](Document& d, int o) { return pageobj::savePicture(d, 0, o, files[2] + L"-extract.png"); });
        return Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(rep.begin(), rep.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--pdfa") {   // --pdfa in.pdf|in.txt|in.md out.pdf [pictures] : writes out.pdf.txt
        if (files.size() < 3) return 2;
        Document doc;
        std::wstring ext = files[1].substr(files[1].find_last_of(L'.') == std::wstring::npos ? files[1].size() : files[1].find_last_of(L'.'));
        for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
        if (ext == L".txt" || ext == L".md" || ext == L".markdown") {
            std::vector<unsigned char> bytes;
            if (!textpdf::build(platform::readTextFile(files[1]), ext != L".txt", bytes).ok || doc.load(std::move(bytes), {}) != OpenResult::Ok) return 3;
        } else if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<unsigned char> out;
        pdfa::Result r;
        std::wstring err;
        const bool ok = pdfa::convert(doc, files.size() > 3 && files[3] == L"pictures", out, r, err);
        std::wstring rep = L"ok=" + std::to_wstring(ok) + L" pages=" + std::to_wstring(r.pages) + L" fonts=" + std::to_wstring(r.fontsReplaced) + L" pictures=" + std::to_wstring(r.picturePages) + L"\n" + err + L"\n";
        for (const auto& n : r.notes) rep += n + L"\n";
        std::string u(rep.begin(), rep.end());
        Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(u.begin(), u.end()));
        return ok && Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--fonts") {   // --fonts in.pdf report.txt : the fonts used, and pages without text
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        const std::wstring t = fontlist::asText(fontlist::build(doc));
        const std::string u = platform::utf8(t);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty()) {   // the engine's test commands, the same on Windows and macOS (shared/selftest.cpp)
        const int rc = selftest::run(files);
        if (rc >= 0) return rc;
    }
    if (!files.empty() && files[0] == L"--annottest") {   // --annottest in.pdf out.pdf : polygon, cloud and callout on page 1, the callout's text then changed
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        bool ok = annot::addPolygon(doc, 0, {{100, 700}, {200, 760}, {260, 690}, {180, 640}}, RGB(0xD0, 0x20, 0x20), 2)
               && annot::addCloud(doc, 0, {320, 620, 500, 760}, RGB(0x20, 0x40, 0xD0), 1.5)
               && annot::addCallout(doc, 0, {150, 500}, {300, 420}, L"Check this figure", RGB(0x10, 0x80, 0x30))
               && annot::setCalloutText(doc, 0, 2, L"Check this figure\nand the table");
        if (!ok) return 5;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--paratest") {   // --paratest in.pdf out.pdf report.txt x y same|append [text] : edits the paragraph at (x, y) on page 1
        if (files.size() < 7) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        const pageobj::ParagraphHit h = pageobj::paragraphAt(doc, 0, wnum(files[4].c_str()), wnum(files[5].c_str()));
        if (!h.found) return 5;
        const std::wstring text = files[6] == L"append" && files.size() > 7 ? h.text + L" " + files[7] : h.text;
        std::wstring why;
        const bool ok = pageobj::replaceParagraph(doc, h, text, h.style, h.align, &why);
        char line[512];
        snprintf(line, sizeof line, "lines=%d align=%d wrap=%.2f step=%.2f origin=%.2f,%.2f result=%s\n", h.lines, static_cast<int>(h.align), h.wrapWidth, h.lineStep,
                 h.originX, h.baseline, ok ? "ok" : "refused");
        const std::string rep = line + std::string(why.begin(), why.end()) + "\ntext=" + platform::utf8(h.text) + "\n";
        Document::writeFile(files[3], std::vector<unsigned char>(rep.begin(), rep.end()));
        if (!ok) return 6;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--checksig") {
        if (files.size() < 3) return 2;
        Document doc;
        std::vector<unsigned char> bytes;
        if (!platform::readFile(files[1], bytes)) return 3;
        std::string report;
        for (const auto& st : sign::check(bytes)) {
            static const char* kinds[] = {"VALID", "VALID-UNTRUSTED", "CHANGED", "UNREADABLE"};
            std::wstring line = L"; signer=" + st.signer + L"; time=" + st.time + L"; reason=" + st.reason;
            const std::string u = platform::utf8(line);
            report += std::string(kinds[st.kind]) + u + "\n";
        }
        std::vector<unsigned char> rb(report.begin(), report.end());
        return Document::writeFile(files[2], rb) == SaveResult::Ok ? 0 : 4;
    }
    return -1;
}

} // namespace cli
