#include "sanitize.hpp"
#include "pdfedit.hpp"
#include <algorithm>
#include <deque>
#include <functional>

namespace sanitize {
using namespace pdfedit;
namespace {

// The full text of an object (between "N G obj" and "endobj"), with stream data skipped by its /Length.
bool objectText(const std::string& s, const File& f, int n, std::string& body, std::string& dict) {
    auto it = f.offsets.find(n);
    if (it == f.offsets.end()) return false;
    size_t o = s.find("obj", it->second);
    if (o == std::string::npos) return false;
    o += 3;
    size_t st = s.find("stream", o), en = s.find("endobj", o);
    if (en == std::string::npos) return false;
    if (st == std::string::npos || st > en) { body = s.substr(o, en - o); dict = body; return true; }
    dict = s.substr(o, st - o);
    // the length of the stream data
    long len = -1;
    size_t vb, ve;
    if (valueSpan(dict, "/Length", vb, ve)) {
        const std::string v = dict.substr(vb, ve - vb);
        if (v.find('R') != std::string::npos) {
            const int ref = atoi(v.c_str());
            std::string lb, ld;
            auto lit = f.offsets.find(ref);
            if (lit != f.offsets.end()) { size_t lo = s.find("obj", lit->second); if (lo != std::string::npos) len = atol(s.c_str() + lo + 3); }
        } else len = atol(v.c_str());
    }
    size_t data = st + 6;
    if (data < s.size() && s[data] == '\r') ++data;
    if (data < s.size() && s[data] == '\n') ++data;
    size_t end = len >= 0 && data + static_cast<size_t>(len) <= s.size() ? data + static_cast<size_t>(len) : std::string::npos;
    size_t endObj = end == std::string::npos ? en : s.find("endobj", end);
    if (endObj == std::string::npos) return false;
    body = s.substr(o, endObj - o);
    return true;
}

// object numbers referred to in a dictionary text ("N G R")
std::vector<int> refsIn(const std::string& t) {
    std::vector<int> out;
    for (size_t i = 0; i < t.size(); ++i) {
        if (!File::isDigit(t[i]) || (i > 0 && (File::isDigit(t[i - 1]) || t[i - 1] == '.' || t[i - 1] == '-'))) continue;
        size_t j = i;
        while (j < t.size() && File::isDigit(t[j])) ++j;
        size_t k = j;
        if (k >= t.size() || t[k] != ' ') continue;
        while (k < t.size() && t[k] == ' ') ++k;
        size_t g = k;
        while (g < t.size() && File::isDigit(t[g])) ++g;
        if (g == k) continue;
        size_t r = g;
        while (r < t.size() && t[r] == ' ') ++r;
        if (r < t.size() && t[r] == 'R' && (r + 1 >= t.size() || !std::isalnum(static_cast<unsigned char>(t[r + 1])))) out.push_back(atoi(t.c_str() + i));
        i = j;
    }
    return out;
}

std::string removeKey(std::string d, const char* key, bool& removed) {
    size_t b, e;
    if (!valueSpan(d, key, b, e)) return d;
    size_t k = d.rfind(key, b);
    d.erase(k, e - k);
    removed = true;
    return d;
}

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

} // namespace

bool run(const std::vector<unsigned char>& pdf, const Options& opt, std::vector<unsigned char>& out, Report& rep, std::wstring& error) {
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    if (!f.parseXref()) { error = L"The structure of this document is not supported."; return false; }
    if (f.trailer.find("/Encrypt") != std::string::npos) { error = L"Remove the password protection first."; return false; }
    rep.before = pdf.size();
    std::map<int, std::string> body, dict;
    for (auto& kv : f.offsets) {
        std::string b, d;
        if (objectText(s, f, kv.first, b, d)) { body[kv.first] = b; dict[kv.first] = d; }
    }
    auto bodyOf = [&](int n) -> std::string& { return body[n]; };
    // whole-object edits work on the dictionary part; streams keep their data
    auto editDict = [&](int n, const std::function<std::string(std::string)>& fn) {
        std::string& b = bodyOf(n);
        size_t st = b.find("stream");
        const std::string head = st == std::string::npos ? b : b.substr(0, st);
        const std::string tail = st == std::string::npos ? "" : b.substr(st);
        b = fn(head) + tail;
    };

    std::string trailer = f.trailer;
    const int root = refOf(trailer, "/Root");
    if (root < 0 || !body.count(root)) { error = L"The document catalog could not be read."; return false; }

    // 1. document information
    int info = refOf(trailer, "/Info");
    if (opt.metadata && info >= 0) {
        std::wstring keys;
        for (const char* k : {"/Title", "/Author", "/Subject", "/Keywords", "/Creator", "/Producer", "/CreationDate", "/ModDate"})
            if (body[info].find(k) != std::string::npos) keys += (keys.empty() ? L"" : L", ") + widen(k + 1);
        bool r = false;
        trailer = removeKey(trailer, "/Info", r);
        rep.removed.push_back(L"Document information" + (keys.empty() ? std::wstring() : L" (" + keys + L")"));
        info = -1;
    }
    // 2. the catalog
    editDict(root, [&](std::string c) {
        bool r = false;
        if (opt.metadata) { c = removeKey(c, "/Metadata", r); if (r) rep.removed.push_back(L"XMP metadata of the document"); }
        if (opt.scripts) {
            r = false;
            size_t vb, ve;
            if (valueSpan(c, "/OpenAction", vb, ve)) {
                std::string v = c.substr(vb, ve - vb);
                const int ref = refOf(c, "/OpenAction");
                const std::string target = ref >= 0 && body.count(ref) ? body[ref] : v;
                if (target.find("/JavaScript") != std::string::npos || target.find("/Launch") != std::string::npos) { c = removeKey(c, "/OpenAction", r); if (r) rep.removed.push_back(L"A script that runs when the file is opened"); }
            }
            r = false;
            c = removeKey(c, "/AA", r);
            if (r) rep.removed.push_back(L"Automatic actions of the document");
        }
        if (opt.attachments) { r = false; c = removeKey(c, "/AF", r); bool r2 = false; c = removeKey(c, "/Collection", r2); }   // associated files and portfolio view
        if (opt.hidden) { r = false; c = removeKey(c, "/PieceInfo", r); if (r) rep.removed.push_back(L"Application data of the document (PieceInfo)"); }
        if (opt.bookmarks) { r = false; c = removeKey(c, "/Outlines", r); if (r) rep.removed.push_back(L"Bookmarks"); }
        // the names dictionary: attachments and document scripts
        size_t nb, ne;
        if ((opt.attachments || opt.scripts) && valueSpan(c, "/Names", nb, ne)) {
            auto clean = [&](std::string names) {
                bool rr = false;
                if (opt.attachments) { names = removeKey(names, "/EmbeddedFiles", rr); if (rr) rep.removed.push_back(L"Attached files"); }
                rr = false;
                if (opt.scripts) { names = removeKey(names, "/JavaScript", rr); if (rr) rep.removed.push_back(L"Document scripts (JavaScript)"); }
                return names;
            };
            if (c[nb] == '<') c = c.substr(0, nb) + clean(c.substr(nb, ne - nb)) + c.substr(ne);
            else { const int ref = refOf(c, "/Names"); if (ref >= 0 && body.count(ref)) editDict(ref, clean); }
        }
        // XFA form data inside the AcroForm
        size_t ab, ae;
        if (opt.hidden && valueSpan(c, "/AcroForm", ab, ae)) {
            auto noXfa = [&](std::string a) { bool rr = false; a = removeKey(a, "/XFA", rr); if (rr) rep.removed.push_back(L"XFA form data"); return a; };
            if (c[ab] == '<') c = c.substr(0, ab) + noXfa(c.substr(ab, ae - ab)) + c.substr(ae);
            else { const int ref = refOf(c, "/AcroForm"); if (ref >= 0 && body.count(ref)) editDict(ref, noXfa); }
        }
        return c;
    });
    // 3. every page and its annotations
    int thumbs = 0, pageMeta = 0, pageScripts = 0, fileAnnots = 0, hiddenAnnots = 0, comments = 0, linkScripts = 0;
    const int pagesNode = refOf(body[root], "/Pages");
    std::vector<int> pages;
    {
        std::deque<int> todo{pagesNode};
        std::set<int> seen;
        while (!todo.empty()) {
            int n = todo.front(); todo.pop_front();
            if (n < 0 || !seen.insert(n).second || !body.count(n)) continue;
            if (body[n].find("/Kids") != std::string::npos) for (int k : kidsOf(body[n])) todo.push_back(k);
            else pages.push_back(n);
        }
    }
    for (int pg : pages) {
        editDict(pg, [&](std::string p) {
            bool r = false;
            if (opt.hidden) { p = removeKey(p, "/Thumb", r); if (r) ++thumbs; r = false; p = removeKey(p, "/PieceInfo", r); if (r) ++pageMeta; }
            r = false;
            if (opt.metadata) { p = removeKey(p, "/Metadata", r); if (r) ++pageMeta; }
            r = false;
            if (opt.scripts) { p = removeKey(p, "/AA", r); if (r) ++pageScripts; }
            r = false;
            if (opt.attachments) { p = removeKey(p, "/AF", r); if (r) ++fileAnnots; }
            // annotations
            size_t vb, ve;
            if (!valueSpan(p, "/Annots", vb, ve)) return p;
            std::string arr = p[vb] == '[' ? p.substr(vb, ve - vb) : "";
            const int arrRef = p[vb] == '[' ? -1 : refOf(p, "/Annots");
            if (arrRef >= 0 && body.count(arrRef)) arr = body[arrRef];
            if (arr.find("<<") != std::string::npos) return p;   // annotations written inside the array: left as they are
            std::vector<int> refs = refsIn(arr), keep;
            for (int a : refs) {
                if (!body.count(a)) continue;
                const std::string& ad = body[a];
                size_t sb, se;
                std::string sub = valueSpan(ad, "/Subtype", sb, se) ? ad.substr(sb, se - sb) : "";
                int flags = 0;
                size_t fb, fe;
                if (valueSpan(ad, "/F", fb, fe)) flags = atoi(ad.c_str() + fb);
                bool drop = false;
                if (opt.attachments && sub == "/FileAttachment") { drop = true; ++fileAnnots; }
                else if (opt.hidden && (flags & 2) && sub != "/Widget") { drop = true; ++hiddenAnnots; }
                else if (opt.comments && sub != "/Link" && sub != "/Widget" && sub != "/Popup") { drop = true; ++comments; }
                else if (opt.comments && sub == "/Popup") drop = true;
                if (!drop && opt.scripts) {
                    editDict(a, [&](std::string x) {
                        bool rr = false;
                        x = removeKey(x, "/AA", rr);
                        if (rr) ++pageScripts;
                        size_t ab, ae;
                        if (valueSpan(x, "/A", ab, ae)) {
                            const int ar = refOf(x, "/A");
                            const std::string act = ar >= 0 && body.count(ar) ? body[ar] : x.substr(ab, ae - ab);
                            if (act.find("/JavaScript") != std::string::npos || act.find("/Launch") != std::string::npos) { bool r3 = false; x = removeKey(x, "/A", r3); if (r3) ++linkScripts; }
                        }
                        return x;
                    });
                }
                if (!drop) keep.push_back(a);
            }
            std::string na = "[";
            for (int k : keep) na += std::to_string(k) + " 0 R ";
            na += "]";
            if (arrRef >= 0) { body[arrRef] = na; return p; }
            return p.substr(0, vb) + na + p.substr(ve);
        });
    }
    if (thumbs) rep.removed.push_back(std::to_wstring(thumbs) + L" page thumbnail picture(s)");
    if (pageMeta) rep.removed.push_back(std::to_wstring(pageMeta) + L" item(s) of page metadata and application data");
    if (pageScripts || linkScripts) rep.removed.push_back(std::to_wstring(pageScripts + linkScripts) + L" script(s) or automatic action(s) on pages, links and fields");
    if (fileAnnots) rep.removed.push_back(std::to_wstring(fileAnnots) + L" file attachment(s) on pages");
    if (hiddenAnnots) rep.removed.push_back(std::to_wstring(hiddenAnnots) + L" hidden annotation(s)");
    if (comments) rep.removed.push_back(std::to_wstring(comments) + L" comment(s), highlight(s) and drawing(s)");

    // 4. keep only the objects that are still used (this also drops everything left from earlier versions)
    std::set<int> used;
    std::deque<int> todo;
    for (int r : refsIn(trailer)) todo.push_back(r);
    while (!todo.empty()) {
        int n = todo.front(); todo.pop_front();
        if (!body.count(n) || !used.insert(n).second) continue;
        const std::string& b = body[n];
        size_t st = b.find("stream");
        for (int r : refsIn(st == std::string::npos ? b : b.substr(0, st))) todo.push_back(r);
    }
    rep.leftovers = static_cast<int>(body.size() - used.size());
    if (rep.leftovers > 0) rep.removed.push_back(std::to_wstring(rep.leftovers) + L" unused object(s) (deleted content, earlier versions, removed data)");

    // 5. write a new file
    std::string o;
    const size_t eol = s.find('\n');
    o = (s.compare(0, 5, "%PDF-") == 0 && eol != std::string::npos && eol < 20 ? s.substr(0, eol) : std::string("%PDF-1.7")) + "\n%\xE2\xE3\xCF\xD3\n";
    std::map<int, size_t> at;
    int maxNum = 0;
    for (int n : used) {
        at[n] = o.size();
        maxNum = std::max(maxNum, n);
        o += std::to_string(n) + " 0 obj" + body[n] + "endobj\n";
    }
    const size_t xr = o.size();
    o += "xref\n0 " + std::to_string(maxNum + 1) + "\n";
    char line[32];
    for (int n = 0; n <= maxNum; ++n) {
        if (at.count(n)) snprintf(line, sizeof line, "%010zu 00000 n \n", at[n]);
        else snprintf(line, sizeof line, "0000000000 %05d f \n", n == 0 ? 65535 : 0);
        o += line;
    }
    std::string tr = "<< /Size " + std::to_string(maxNum + 1) + " /Root " + std::to_string(root) + " 0 R";
    if (info >= 0 && used.count(info)) tr += " /Info " + std::to_string(info) + " 0 R";
    size_t ib, ie;
    if (valueSpan(f.trailer, "/ID", ib, ie)) tr += " /ID " + f.trailer.substr(ib, ie - ib);
    tr += " >>";
    o += "trailer\n" + tr + "\nstartxref\n" + std::to_string(xr) + "\n%%EOF\n";
    out.assign(o.begin(), o.end());
    rep.after = out.size();
    return true;
}

} // namespace sanitize
