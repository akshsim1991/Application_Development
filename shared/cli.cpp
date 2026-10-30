// The command line of Feather PDF, the same on Windows and macOS: every feature that runs without a window. Used by the
// tests (tests/smoke.sh on Windows, the Mac build check) and by anyone who scripts it. The commands still marked
// Windows-only use parts of the engine that have no Mac version yet (macOS/PORTING.md).
#include "cli.hpp"
#include <algorithm>
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
#ifdef _WIN32   // not on the Mac yet (macOS/PORTING.md)
    if (!files.empty() && files[0] == L"--ocr") {
        if (files.size() < 3) return 2;
        std::wstring why;
        if (!ocr::available(why)) return 5;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<int> all(doc.pageCount());
        for (int i = 0; i < doc.pageCount(); ++i) all[i] = i;
        ocr::Result r = ocr::run(doc, all, files.size() > 3 ? files[3] : L"eng", true, nullptr);
        if (!r.error.empty()) return 4;
        if (r.words == 0) return 6;
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }
#endif

    // Command line signing: FeatherPDF.exe --sign <in.pdf> <out.pdf> <certificate.pfx> <password>
    // and checking:         FeatherPDF.exe --checksig <file.pdf> <report.txt>   (exit code 0 ok; the report lists each signature)
#ifdef _WIN32   // not on the Mac yet (macOS/PORTING.md)
    if (!files.empty() && files[0] == L"--sign") {
        if (files.size() < 5) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::vector<unsigned char> plain;
        if (!doc.serialize(plain, FPDF_REMOVE_SECURITY)) return 4;
        HCERTSTORE store = nullptr;
        std::wstring err;
        PCCERT_CONTEXT cert = sign::certFromPfx(files[3], files[4], store, err);
        if (!cert) return 5;
        std::vector<unsigned char> out;
        sign::Options so;
        so.reason = L"Approved";
        if (files.size() > 5 && files[5] != L"-") so.timestampUrl.assign(files[5].begin(), files[5].end());   // time stamp server
        if (files.size() > 6 && files[6] == L"visible") {   // bottom right of the first page
            double cl, cb, cr, ct;
            if (doc.cropBoxOf(0, cl, cb, cr, ct)) { so.visible = true; so.page = 0; so.right = cr - 36; so.left = so.right - 200; so.bottom = cb + 36; so.top = so.bottom + 60; }
        }
        bool ok = sign::signPdf(plain, cert, so, out, err);
        CertFreeCertificateContext(cert);
        if (store) CertCloseStore(store, 0);
        if (!ok) {   // leave the reason next to the output file so scripts can show it
            const std::string u = platform::utf8(err);
            std::vector<unsigned char> eb(u.begin(), u.end());
            Document::writeFile(files[2] + L".error.txt", eb);
            return 6;
        }
        return Document::writeFile(files[2], out) == SaveResult::Ok ? 0 : 4;
    }
#endif
#ifdef _WIN32   // not on the Mac yet (macOS/PORTING.md)
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
#endif
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
#ifdef _WIN32   // not on the Mac yet (macOS/PORTING.md)
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
#endif
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
        const std::string rep = line + std::string(why.begin(), why.end());
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
