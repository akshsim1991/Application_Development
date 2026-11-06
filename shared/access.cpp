#include "access.hpp"
#include "attach.hpp"   // layers::decodeString
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <cwctype>
#include <map>

namespace access {
using namespace pdfedit;

namespace {

// The file's objects as text (a stream's dictionary only), from a full save.
struct Raw {
    std::string s;
    File f{s};
    std::map<int, std::string> dict;   // object -> its dictionary text
    std::map<int, bool> stream;
    int root = -1;
    std::string cat;
    bool ok = false;
    explicit Raw(Document& d) {
        std::vector<unsigned char> b;
        if (!d.serialize(b, FPDF_NO_INCREMENTAL)) return;
        s.assign(b.begin(), b.end());
        if (!f.parseXref()) return;
        for (const auto& kv : f.offsets) {
            std::string body;
            if (!f.object(kv.first, body)) continue;
            const size_t st = body.find("stream");
            const size_t gt = body.rfind(">>", st == std::string::npos ? std::string::npos : st);
            stream[kv.first] = st != std::string::npos && gt != std::string::npos && st > gt;
            dict[kv.first] = stream[kv.first] ? body.substr(0, gt + 2) : body;
        }
        root = refOf(f.trailer, "/Root");
        ok = root >= 0 && dict.count(root);
        if (ok) cat = dict[root];
    }
    std::string value(const std::string& d0, const char* key) const {
        size_t b, e;
        if (!valueSpan(d0, key, b, e)) return {};
        std::string v = d0.substr(b, e - b);
        if (!v.empty() && v.back() == 'R') {
            const int n = atoi(v.c_str());
            auto it = dict.find(n);
            return it == dict.end() ? std::string() : it->second;
        }
        return v;
    }
};

std::string trim(std::string t) {
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front()))) t.erase(0, 1);
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back()))) t.pop_back();
    return t;
}

bool isName(const std::string& d0, const char* key, const char* name) {
    size_t b, e;
    return valueSpan(d0, key, b, e) && d0.compare(b, e - b, name) == 0;
}

std::wstring text(const std::string& token) { return layers::decodeString(trim(token)); }

bool has(const std::string& d0, const char* key) { size_t b, e; return valueSpan(d0, key, b, e); }

// A title some programs write when the author gave none: as good as no title.
bool placeholder(const std::wstring& t) {
    std::wstring l;
    for (wchar_t c : t) l += static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
    while (!l.empty() && l.back() == L' ') l.pop_back();
    if (l.empty() || l == L"untitled" || l == L"untitled document" || l == L"document" || l == L"no title" || l.rfind(L"microsoft word - ", 0) == 0) return true;
    for (const wchar_t* ext : {L".doc", L".docx", L".pdf", L".rtf", L".odt", L".xls", L".xlsx", L".ppt", L".pptx"}) {
        const std::wstring e = ext;
        if (l.size() > e.size() && l.compare(l.size() - e.size(), e.size(), e) == 0) return true;
    }
    return false;
}

// The URI of a link's action: the "/URI" key followed by a string (not the "/S /URI" type).
std::wstring uriOf(const std::string& a) {
    for (size_t p = a.find("/URI"); p != std::string::npos; p = a.find("/URI", p + 4)) {
        size_t q = p + 4;
        while (q < a.size() && std::isspace(static_cast<unsigned char>(a[q]))) ++q;
        if (q < a.size() && (a[q] == '(' || (a[q] == '<' && a.compare(q, 2, "<<") != 0))) {
            size_t b, e;
            if (valueSpan(a.substr(p), "/URI", b, e)) return layers::decodeString(a.substr(p + b, e - b));
        }
    }
    return {};
}

std::wstring plural(int n, const wchar_t* one, const wchar_t* many) { return std::to_wstring(n) + L" " + (n == 1 ? one : many); }

} // namespace

std::wstring suggestedTitle(Document& d) {
    const auto& api = pdfium::api();
    if (d.pageCount() > 0) {
        FPDF_TEXTPAGE tp = d.text(0);
        const int n = tp ? api.FPDFText_CountChars(tp) : 0;
        double best = 0;
        for (int i = 0; i < n; ++i) {
            const unsigned u = api.FPDFText_GetUnicode(tp, i);
            if (u > 32 && api.FPDFText_GetFontSize(tp, i) > best) best = api.FPDFText_GetFontSize(tp, i);
        }
        if (best > 0) {
            std::wstring t;
            bool started = false;
            for (int i = 0; i < n && t.size() < 120; ++i) {
                const unsigned u = api.FPDFText_GetUnicode(tp, i);
                const bool big = std::abs(api.FPDFText_GetFontSize(tp, i) - best) < 0.6;
                if (big && u >= 32) { t += static_cast<wchar_t>(u); started = true; }
                else if (started && (u == L'\r' || u == L'\n' || u == L' ')) t += L' ';
                else if (started && !big && u > 32) break;
            }
            std::wstring c;
            for (wchar_t ch : t) if (!(ch == L' ' && (c.empty() || c.back() == L' '))) c += ch;
            while (!c.empty() && c.back() == L' ') c.pop_back();
            if (c.size() >= 3) return c;
        }
    }
    std::wstring name = d.title();
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name.resize(dot);
    for (auto& ch : name) if (ch == L'_' || ch == L'-') ch = L' ';
    return name;
}

std::vector<Item> check(Document& d) {
    std::vector<Item> out;
    Raw r(d);
    const auto& api = pdfium::api();
    auto add = [&](const char* id, const wchar_t* rule, Result res, const std::wstring& detail, bool fixable) {
        out.push_back({id, rule, res, detail, fixable && res == Result::Failed});
    };
    if (!r.ok) {
        add("structure", L"The file can be examined", Result::Failed, L"The structure of this file could not be read.", false);
        return out;
    }
    // title, and the title shown instead of the file name
    std::wstring title;
    {
        const int info = refOf(r.f.trailer, "/Info");
        if (info >= 0 && r.dict.count(info)) title = text(r.value(r.dict[info], "/Title"));
        title.erase(std::remove_if(title.begin(), title.end(), [](wchar_t c) { return c < 32; }), title.end());
    }
    const bool noTitle = placeholder(title);
    add("title", L"The document has a title", noTitle ? Result::Failed : Result::Passed,
        title.empty() ? L"No title: screen readers announce the file name instead."
                      : noTitle ? L"Only a placeholder title (“" + title + L"”): screen readers announce it instead of a real title."
                                : L"“" + title + L"”", true);
    const std::string vp = r.value(r.cat, "/ViewerPreferences");
    const bool shows = vp.find("/DisplayDocTitle true") != std::string::npos || vp.find("/DisplayDocTitle\ntrue") != std::string::npos;
    add("showtitle", L"The title is shown in the window (not the file name)", shows ? Result::Passed : Result::Failed,
        shows ? L"Shown." : L"The window shows the file name.", true);
    // language
    const std::wstring lang = text(r.value(r.cat, "/Lang"));
    add("lang", L"The language is set", lang.empty() ? Result::Failed : Result::Passed,
        lang.empty() ? L"No language: screen readers may read the words with the wrong pronunciation." : L"Language: " + lang, true);
    // tags
    const std::string mark = r.value(r.cat, "/MarkInfo");
    const bool tagged = has(r.cat, "/StructTreeRoot") && mark.find("/Marked true") != std::string::npos;
    add("tagged", L"The document is tagged (structure and reading order)", tagged ? Result::Passed : Result::Failed,
        tagged ? L"Tagged." : L"Not tagged. Tags must be made by the program that made the document: in Word, Save As PDF with "
                              L"“Document structure tags for accessibility” turned on.", false);
    // text on every page, and text that can be read out (has Unicode values)
    int scanned = 0, chars = 0, unknown = 0, pictures = 0;
    std::wstring scannedPages;
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        int images = 0;
        const int objs = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < objs; ++i) if (api.FPDFPageObj_GetType(api.FPDFPage_GetObject(pg, i)) == FPDF_PAGEOBJ_IMAGE) ++images;
        pictures += images;
        const int n = d.charCount(p);
        if (n <= 0 && images > 0) { ++scanned; if (scannedPages.size() < 60) scannedPages += (scannedPages.empty() ? L"" : L", ") + std::to_wstring(p + 1); }
        FPDF_TEXTPAGE tp = d.text(p);
        for (int i = 0; tp && i < n; ++i) {
            const unsigned u = api.FPDFText_GetUnicode(tp, i);
            if (u == L' ' || u == L'\r' || u == L'\n') continue;
            ++chars;
            if (u == 0 || u == 0xFFFD || u == 0xFFFE || (u >= 0xE000 && u <= 0xF8FF)) ++unknown;
        }
    }
    add("text", L"Every page has text (not only a picture of text)", scanned ? Result::Failed : Result::Passed,
        scanned ? plural(scanned, L"page is", L"pages are") + L" only a picture (page " + scannedPages +
                      L"). Recognise their text (OCR) so it can be read out." : L"Every page has text.", false);
    const bool badText = chars > 0 && unknown * 50 > chars;
    add("unicode", L"The text can be read out (its letters are known)", badText ? Result::Failed : Result::Passed,
        badText ? std::to_wstring(unknown) + L" of " + std::to_wstring(chars) + L" letters cannot be told apart by a screen reader (their font gives no "
                                                                               L"Unicode values). Making the PDF again from the original usually helps."
                : L"Readable.", false);
    // pictures: alternate text
    if (tagged) {
        int figures = 0, noAlt = 0;
        for (const auto& kv : r.dict)
            if (isName(kv.second, "/S", "/Figure")) { ++figures; if (!has(kv.second, "/Alt")) ++noAlt; }
        add("alt", L"Pictures have alternate text", noAlt ? Result::Failed : Result::Passed,
            noAlt ? plural(noAlt, L"picture has", L"pictures have") + L" no alternate text (a description read out instead of the picture). Add it in the "
                                                                       L"program the document was made with."
                  : (figures ? plural(figures, L"picture is", L"pictures are") + L" described." : std::wstring(L"No tagged pictures.")),
            false);
    } else {
        add("alt", L"Pictures have alternate text", pictures ? Result::NeedsPerson : Result::Passed,
            pictures ? plural(pictures, L"picture", L"pictures") + L": without tags they cannot carry a description. Check that none of them is needed to "
                                                                    L"understand the text, or tag the document."
                     : L"No pictures.", false);
    }
    // links, form fields, tab order
    int links = 0, linksBare = 0, fields = 0, fieldsBare = 0, pagesAnnots = 0, pagesNoTabs = 0;
    for (const auto& kv : r.dict) {
        if (r.stream[kv.first]) continue;
        const std::string& o = kv.second;
        if (isName(o, "/Subtype", "/Link")) { ++links; if (text(r.value(o, "/Contents")).empty()) ++linksBare; }
        if (has(o, "/FT")) { ++fields; if (text(r.value(o, "/TU")).empty()) ++fieldsBare; }
        if (isName(o, "/Type", "/Page") && has(o, "/Annots")) { ++pagesAnnots; if (!isName(o, "/Tabs", "/S")) ++pagesNoTabs; }
    }
    add("links", L"Links say where they go", linksBare ? Result::Failed : Result::Passed,
        linksBare ? plural(linksBare, L"link has", L"links have") + L" no description." : (links ? plural(links, L"link", L"links") + L", described." : L"No links."), true);
    add("fields", L"Form fields have a tooltip (their name read out)", fieldsBare ? Result::Failed : Result::Passed,
        fieldsBare ? plural(fieldsBare, L"field has", L"fields have") + L" no tooltip." : (fields ? plural(fields, L"field", L"fields") + L", each with a tooltip." : L"No form fields."), true);
    add("tabs", L"The Tab key follows the structure of the page", pagesNoTabs ? Result::Failed : Result::Passed,
        pagesNoTabs ? L"No tab order on " + plural(pagesNoTabs, L"page", L"pages") + L" with links, comments or fields." : L"Set (or nothing to tab to).", true);
    // bookmarks for long documents
    const bool longDoc = d.pageCount() > 20;
    const bool marks = !d.bookmarks().empty();
    add("bookmarks", L"A long document has bookmarks", longDoc && !marks ? Result::Failed : Result::Passed,
        longDoc ? (marks ? L"It has bookmarks." : std::to_wstring(d.pageCount()) + L" pages and no bookmarks: add bookmarks for the chapters (Edit PDF > Add a bookmark).")
                : L"Short enough without them.", false);
    // things only a person can judge
    add("contrast", L"Colours have enough contrast", Result::NeedsPerson, L"Check that text stands out from its background (night mode helps to see it).", false);
    add("order", L"The reading order makes sense", Result::NeedsPerson,
        tagged ? L"Read it with a screen reader to check the order." : L"Without tags the order is the order the text was drawn in.", false);
    return out;
}

std::vector<std::wstring> fix(Document& d, const FixOptions& o) {
    std::vector<std::wstring> done;
    Raw r(d);
    if (!r.ok) return done;
    std::map<int, std::string> write;
    int next = r.f.size;
    std::string cat = r.cat;
    bool catChanged = false;
    // the title, in the document information
    if (o.title) {
        const int info = refOf(r.f.trailer, "/Info");
        std::wstring now;
        if (info >= 0 && r.dict.count(info)) now = text(r.value(r.dict[info], "/Title"));
        const std::wstring want = o.titleText.empty() ? suggestedTitle(d) : o.titleText;
        if ((placeholder(now) || !o.titleText.empty()) && !want.empty() && want != now) {
            if (info >= 0 && r.dict.count(info)) write[info] = setKey(r.dict[info], "/Title", pdfString(want));
            else {
                const int n = next++;
                write[n] = "<< /Title " + pdfString(want) + " >>";
                r.f.trailer = setKey(r.f.trailer, "/Info", std::to_string(n) + " 0 R");
            }
            done.push_back(L"Title set: \u201C" + want + L"\u201D.");
        }
    }
    // the title shown in the window
    if (o.showTitle) {
        size_t b, e;
        if (valueSpan(cat, "/ViewerPreferences", b, e)) {
            std::string v = cat.substr(b, e - b);
            if (v.find("/DisplayDocTitle true") == std::string::npos) {
                if (!v.empty() && v.back() == 'R') {
                    const int n = atoi(v.c_str());
                    if (r.dict.count(n)) write[n] = setKey(r.dict[n], "/DisplayDocTitle", "true");
                } else { cat = setKey(cat, "/ViewerPreferences", setKey(v, "/DisplayDocTitle", "true")); catChanged = true; }
                done.push_back(L"The window shows the title.");
            }
        } else { cat = setKey(cat, "/ViewerPreferences", "<< /DisplayDocTitle true >>"); catChanged = true; done.push_back(L"The window shows the title."); }
    }
    // the language
    if (o.lang && !o.language.empty() && text(r.value(cat, "/Lang")).empty()) {
        cat = setKey(cat, "/Lang", pdfString(o.language));
        catChanged = true;
        done.push_back(L"Language set: " + o.language + L".");
    }
    if (catChanged) write[r.root] = cat;
    // links, form fields, tab order
    int links = 0, tips = 0, tabs = 0;
    for (const auto& kv : r.dict) {
        if (r.stream[kv.first] || kv.first == r.root || write.count(kv.first)) continue;
        std::string ob = kv.second;
        bool changed = false;
        if (o.linkText && isName(ob, "/Subtype", "/Link") && text(r.value(ob, "/Contents")).empty()) {
            const std::string a = r.value(ob, "/A");
            const std::wstring uri = uriOf(a);
            ob = setKey(ob, "/Contents", pdfString(uri.empty() ? L"Link to another place in this document" : L"Link to " + uri));
            changed = true;
            ++links;
        }
        if (o.fieldTips && has(ob, "/FT") && text(r.value(ob, "/TU")).empty()) {
            std::wstring name = text(r.value(ob, "/T"));
            for (auto& c : name) if (c == L'_' || c == L'.') c = L' ';
            if (!name.empty()) { ob = setKey(ob, "/TU", pdfString(name)); changed = true; ++tips; }
        }
        if (o.tabOrder && isName(ob, "/Type", "/Page") && has(ob, "/Annots") && !isName(ob, "/Tabs", "/S")) {
            ob = setKey(ob, "/Tabs", "/S");
            changed = true;
            ++tabs;
        }
        if (changed) write[kv.first] = ob;
    }
    if (links) done.push_back(std::to_wstring(links) + L" link(s) described.");
    if (tips) done.push_back(std::to_wstring(tips) + L" form field(s) given a tooltip (from their names).");
    if (tabs) done.push_back(L"Tab order set on " + std::to_wstring(tabs) + L" page(s).");
    if (write.empty()) return {};
    std::vector<unsigned char> out = appendIncremental(r.s, r.f, write, next);
    d.beginChange(L"Accessibility fixes");
    if (d.reload(std::move(out)) != OpenResult::Ok) { d.rollbackChange(); return {}; }
    return done;
}

std::wstring report(const std::vector<Item>& items, const std::wstring& name) {
    int failed = 0, person = 0, passed = 0;
    for (const auto& i : items) (i.result == Result::Failed ? failed : i.result == Result::NeedsPerson ? person : passed)++;
    std::wstring s = L"# Accessibility check\n\n**Document:** " + name + L"\n\n**Passed:** " + std::to_wstring(passed) + L"  ·  **Failed:** " +
                     std::to_wstring(failed) + L"  ·  **For a person to check:** " + std::to_wstring(person) + L"\n\n";
    for (Result want : {Result::Failed, Result::NeedsPerson, Result::Passed}) {
        bool any = false;
        for (const auto& i : items) {
            if (i.result != want) continue;
            if (!any) {
                s += want == Result::Failed ? L"## Failed\n\n" : want == Result::NeedsPerson ? L"## For a person to check\n\n" : L"## Passed\n\n";
                any = true;
            }
            s += L"- **" + i.rule + L".** " + i.detail + (i.fixable ? L" *(Fix puts this right.)*" : L"") + L"\n";
        }
        if (any) s += L"\n";
    }
    return s;
}

} // namespace access
