#include "webview.hpp"
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
#include <map>
#include <set>

// ---------------------------------------------------------------- repair

namespace repair {

Report run(const std::wstring& path, std::vector<unsigned char>& out, const std::string& password) {
    Report r;
    std::vector<unsigned char> raw;
    if (!platform::readFile(path, raw) || raw.empty()) { r.problems.push_back(L"The file could not be read, or it is empty."); return r; }
    const std::string s(raw.begin(), raw.end());
    // what is wrong, before PDFium puts it right
    const size_t hdr = s.find("%PDF-");
    if (hdr == std::string::npos || hdr > 1024) r.problems.push_back(L"The file does not start like a PDF (its header is missing).");
    else if (hdr > 0) r.problems.push_back(L"There is extra data before the start of the PDF.");
    const size_t eof = s.rfind("%%EOF");
    if (eof == std::string::npos) r.problems.push_back(L"The end of the file is missing: it was probably cut short (an interrupted download or copy).");
    else if (s.size() - eof > 1024) r.problems.push_back(L"There is extra data after the end of the PDF.");
    const size_t sx = s.rfind("startxref");
    bool indexOk = false;
    if (sx != std::string::npos) {
        const unsigned long long off = strtoull(s.c_str() + sx + 9, nullptr, 10);
        if (off > 0 && off + 4 < s.size()) {
            if (s.compare(static_cast<size_t>(off), 4, "xref") == 0) {
                pdfedit::File f(s);
                indexOk = f.parseXref();
                // every entry must point at its object
                if (indexOk)
                    for (const auto& kv : f.offsets) {
                        const std::string want = std::to_string(kv.first) + " ";
                        if (kv.second + want.size() > s.size() || s.compare(kv.second, want.size(), want) != 0) { indexOk = false; break; }
                    }
            } else {
                size_t p = static_cast<size_t>(off);   // a cross-reference stream: "N G obj ... /XRef"
                while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p]))) ++p;
                indexOk = p > off && s.find("/XRef", static_cast<size_t>(off)) < static_cast<size_t>(off) + 400;
            }
        }
    }
    if (!indexOk) r.problems.push_back(L"The index of the file (its cross-reference table) is damaged or points to the wrong places.");

    Document d;
    if (d.load(std::vector<unsigned char>(raw.begin(), raw.end()), password) != OpenResult::Ok) {
        r.problems.push_back(L"The document could not be read, even after rebuilding its index.");
        return r;
    }
    r.opened = true;
    r.pages = d.pageCount();
    for (int i = 0; i < r.pages; ++i) if (!d.page(i)) ++r.badPages;
    if (r.badPages) r.problems.push_back(std::to_wstring(r.badPages) + L" page(s) could not be read and are kept as they are.");
    if (!d.serialize(out, FPDF_NO_INCREMENTAL)) { r.problems.push_back(L"The repaired copy could not be written."); out.clear(); return r; }
    {   // a trailer entry pointing at an object that was lost (the document information of a cut-off file): blanked out,
        // keeping every offset
        std::string t(out.begin(), out.end());
        pdfedit::File of(t);
        const size_t tp = t.rfind("trailer");
        if (of.parseXref() && tp != std::string::npos) {
            for (const char* key : {"/Info"}) {
                const size_t k = t.find(key, tp);
                const int n = pdfedit::refOf(of.trailer, key);
                if (k == std::string::npos || n < 0 || of.offsets.count(n)) continue;
                size_t b, e;
                const std::string tail = t.substr(k);
                if (!pdfedit::valueSpan(tail, key, b, e)) continue;
                for (size_t i = k; i < k + e; ++i) t[i] = ' ';
            }
            out.assign(t.begin(), t.end());
        }
    }
    Document check;
    if (check.load(std::vector<unsigned char>(out.begin(), out.end()), password) != OpenResult::Ok || check.pageCount() != r.pages) {
        r.problems.push_back(L"The repaired copy did not read back correctly.");
        out.clear();
        return r;
    }
    r.fixed = !r.problems.empty();
    return r;
}

} // namespace repair

// ---------------------------------------------------------------- linearization (PDF 1.7, Annex F)

namespace webview {
namespace {

bool ws(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == 0; }
bool delim(char c) { return ws(c) || std::strchr("()<>[]{}/%", c) != nullptr; }

size_t skipLiteral(const std::string& t, size_t i) {   // at '('
    int depth = 0;
    for (; i < t.size(); ++i) {
        if (t[i] == '\\') { ++i; continue; }
        if (t[i] == '(') ++depth;
        else if (t[i] == ')' && --depth == 0) return i + 1;
    }
    return t.size();
}

// Calls f(start, end, number) for every "N G R" reference in `t` (outside strings and comments).
void eachRef(const std::string& t, const std::function<void(size_t, size_t, int)>& f) {
    struct Num { size_t a; long v; };
    Num q[2];
    int nq = 0;
    size_t i = 0;
    while (i < t.size()) {
        const char c = t[i];
        if (ws(c)) { ++i; continue; }
        if (c == '%') { while (i < t.size() && t[i] != '\n' && t[i] != '\r') ++i; continue; }
        if (c == '(') { i = skipLiteral(t, i); nq = 0; continue; }
        if (c == '<') {
            if (i + 1 < t.size() && t[i + 1] == '<') { i += 2; nq = 0; continue; }
            const size_t e = t.find('>', i);
            i = e == std::string::npos ? t.size() : e + 1;
            nq = 0;
            continue;
        }
        if (c == '>' || c == '[' || c == ']' || c == '{' || c == '}') { ++i; nq = 0; continue; }
        if (c == '/') { ++i; while (i < t.size() && !delim(t[i])) ++i; nq = 0; continue; }
        const size_t a = i;
        while (i < t.size() && !delim(t[i])) ++i;
        const std::string tok = t.substr(a, i - a);
        bool integer = !tok.empty();
        for (char k : tok) integer = integer && std::isdigit(static_cast<unsigned char>(k));
        if (integer) {
            if (nq == 2) { q[0] = q[1]; nq = 1; }
            q[nq++] = {a, strtol(tok.c_str(), nullptr, 10)};
        } else {
            if (tok == "R" && nq == 2) f(q[0].a, i, static_cast<int>(q[0].v));
            nq = 0;
        }
    }
}

std::string renumber(const std::string& t, const std::map<int, int>& map) {
    std::string out;
    size_t last = 0;
    eachRef(t, [&](size_t a, size_t b, int n) {
        auto it = map.find(n);
        out += t.substr(last, a - last);
        out += it == map.end() ? std::string("null") : std::to_string(it->second) + " 0 R";
        last = b;
    });
    return out + t.substr(last);
}

struct Obj {
    std::string dict;     // the object's value (a stream's dictionary)
    std::string data;     // a stream's bytes
    bool stream = false;
    std::vector<int> refs;
};

// The end of the value starting at i (a dictionary "<< ... >>" is matched; other values to the next keyword).
size_t valueEnd(const std::string& t, size_t i) {
    while (i < t.size() && ws(t[i])) ++i;
    if (t.compare(i, 2, "<<") != 0) return std::string::npos;
    int depth = 0;
    while (i < t.size()) {
        if (t[i] == '(') { i = skipLiteral(t, i); continue; }
        if (t.compare(i, 2, "<<") == 0) { ++depth; i += 2; continue; }
        if (t.compare(i, 2, ">>") == 0) { i += 2; if (--depth == 0) return i; continue; }
        if (t[i] == '<') { const size_t e = t.find('>', i); i = e == std::string::npos ? t.size() : e + 1; continue; }
        if (t[i] == '%') { while (i < t.size() && t[i] != '\n' && t[i] != '\r') ++i; continue; }
        ++i;
    }
    return std::string::npos;
}

bool readObjects(const std::string& s, const pdfedit::File& f, std::map<int, Obj>& objs) {
    for (const auto& kv : f.offsets) {
        const size_t o = s.find("obj", kv.second);
        if (o == std::string::npos) return false;
        size_t p = o + 3;
        Obj ob;
        const size_t de = valueEnd(s, p);
        size_t k = de;
        if (de != std::string::npos) {
            while (k < s.size() && ws(s[k])) ++k;
        }
        if (de != std::string::npos && s.compare(k, 6, "stream") == 0) {
            ob.stream = true;
            ob.dict = s.substr(p, de - p);
            size_t d = k + 6;
            if (s.compare(d, 2, "\r\n") == 0) d += 2; else if (d < s.size() && (s[d] == '\n' || s[d] == '\r')) ++d;
            size_t b, e;
            long len = -1;
            if (pdfedit::valueSpan(ob.dict, "/Length", b, e)) {
                const std::string v = ob.dict.substr(b, e - b);
                if (!v.empty() && v.back() == 'R') {
                    std::string lb;
                    if (f.object(atoi(v.c_str()), lb)) len = strtol(lb.c_str(), nullptr, 10);
                    ob.dict = pdfedit::setKey(ob.dict, "/Length", std::to_string(len));   // made direct
                } else len = strtol(v.c_str(), nullptr, 10);
            }
            if (len < 0 || d + static_cast<size_t>(len) > s.size()) return false;
            ob.data = s.substr(d, static_cast<size_t>(len));
        } else {
            const size_t e = s.find("endobj", p);
            if (e == std::string::npos) return false;
            ob.dict = s.substr(p, e - p);
        }
        while (!ob.dict.empty() && ws(ob.dict.back())) ob.dict.pop_back();
        while (!ob.dict.empty() && ws(ob.dict.front())) ob.dict.erase(0, 1);
        eachRef(ob.dict, [&](size_t, size_t, int n) { ob.refs.push_back(n); });
        objs[kv.first] = std::move(ob);
    }
    return true;
}

bool hasType(const Obj& o, const char* type) {
    size_t b, e;
    return pdfedit::valueSpan(o.dict, "/Type", b, e) && o.dict.compare(b, e - b, type) == 0;
}

struct Bits {
    std::string out;
    unsigned cur = 0;
    int n = 0;
    void put(unsigned long long v, int bits) {
        for (int i = bits - 1; i >= 0; --i) {
            cur = (cur << 1) | static_cast<unsigned>((v >> i) & 1);
            if (++n == 8) { out += static_cast<char>(cur); cur = 0; n = 0; }
        }
    }
    void flush() { if (n) { cur <<= (8 - n); out += static_cast<char>(cur); cur = 0; n = 0; } }
};

int bitsFor(unsigned long long v) { int b = 0; while (v) { ++b; v >>= 1; } return b; }

std::string padNum(long long v, int width) {
    std::string s = std::to_string(v);
    while (static_cast<int>(s.size()) < width) s += ' ';
    return s;
}

} // namespace

bool isLinearized(const std::vector<unsigned char>& pdf) {
    const size_t n = std::min<size_t>(pdf.size(), 2048);
    const std::string head(pdf.begin(), pdf.begin() + static_cast<long>(n));
    return head.find("/Linearized") != std::string::npos;
}

bool linearize(const std::vector<unsigned char>& pdf, std::vector<unsigned char>& out, std::wstring* why) {
    auto fail = [&](const wchar_t* w) { if (why) *why = w; return false; };
    const std::string s(pdf.begin(), pdf.end());
    pdfedit::File f(s);
    if (!f.parseXref()) return fail(L"The file's structure is not supported.");
    if (pdfedit::refOf(f.trailer, "/Encrypt") >= 0 || f.trailer.find("/Encrypt") != std::string::npos)
        return fail(L"A password-protected document cannot be made fast for the web. Remove the password first.");
    std::map<int, Obj> objs;
    if (!readObjects(s, f, objs)) return fail(L"The file's objects could not be read.");
    const int root = pdfedit::refOf(f.trailer, "/Root");
    if (!objs.count(root)) return fail(L"The document has no catalog.");

    // the pages, in order, and the page tree's own nodes
    std::vector<int> pages;
    std::set<int> pageSet, treeNodes;
    std::function<void(int, int)> walk = [&](int n, int depth) {
        if (depth > 64 || !objs.count(n) || treeNodes.count(n) || pageSet.count(n)) return;
        const Obj& o = objs[n];
        if (o.dict.find("/Kids") != std::string::npos && !hasType(o, "/Page")) {
            treeNodes.insert(n);
            for (int k : pdfedit::kidsOf(o.dict)) walk(k, depth + 1);
        } else { pages.push_back(n); pageSet.insert(n); }
    };
    walk(pdfedit::refOf(objs[root].dict, "/Pages"), 0);
    if (pages.empty()) return fail(L"The document has no pages.");
    const int npages = static_cast<int>(pages.size());

    // what each page uses (not its page tree, not other pages, not the catalog)
    std::map<int, std::set<int>> users;   // object -> pages
    std::vector<std::vector<int>> uses(static_cast<size_t>(npages));   // in the order found
    for (int i = 0; i < npages; ++i) {
        std::vector<int> todo{pages[static_cast<size_t>(i)]};
        std::set<int> seen;
        while (!todo.empty()) {
            const int n = todo.back();
            todo.pop_back();
            if (!objs.count(n) || seen.count(n)) continue;
            if (n != pages[static_cast<size_t>(i)] && (pageSet.count(n) || treeNodes.count(n) || n == root)) continue;
            seen.insert(n);
            uses[static_cast<size_t>(i)].push_back(n);
            users[n].insert(i);
            const auto& r = objs[n].refs;
            for (auto it = r.rbegin(); it != r.rend(); ++it) todo.push_back(*it);
        }
    }
    // what opening the document needs (part 4): the catalog and these entries, unless a page uses them
    std::vector<int> part4{root};
    {
        std::set<int> seen{root};
        std::vector<int> todo;
        for (const char* key : {"/ViewerPreferences", "/PageMode", "/Threads", "/OpenAction", "/AcroForm"}) {
            size_t b, e;
            if (pdfedit::valueSpan(objs[root].dict, key, b, e)) eachRef(objs[root].dict.substr(b, e - b), [&](size_t, size_t, int n) { todo.push_back(n); });
        }
        while (!todo.empty()) {
            const int n = todo.front();
            todo.erase(todo.begin());
            if (!objs.count(n) || seen.count(n) || pageSet.count(n) || treeNodes.count(n) || users.count(n)) continue;
            seen.insert(n);
            part4.push_back(n);
            for (int k : objs[n].refs) todo.push_back(k);
        }
    }
    std::set<int> placed(part4.begin(), part4.end());
    std::vector<int> part6;   // the first page: its page object, then everything it uses
    for (int n : uses[0]) if (!placed.count(n)) { part6.push_back(n); placed.insert(n); }
    std::vector<std::vector<int>> part7(static_cast<size_t>(npages));   // each other page: its page object and what only it uses
    for (int i = 1; i < npages; ++i)
        for (int n : uses[static_cast<size_t>(i)])
            if (!placed.count(n) && users[n].size() == 1) { part7[static_cast<size_t>(i)].push_back(n); placed.insert(n); }
    // bookmarks shown when the document opens (/PageMode /UseOutlines) come with the first page, with a table of their own
    std::vector<int> outlines;
    bool olFirst = false;
    {
        size_t b, e;
        const Obj& cat = objs[root];
        olFirst = pdfedit::valueSpan(cat.dict, "/PageMode", b, e) && cat.dict.compare(b, e - b, "/UseOutlines") == 0;
        {
            std::vector<int> todo;
            const int ol = pdfedit::refOf(cat.dict, "/Outlines");
            if (ol >= 0) todo.push_back(ol);
            while (!todo.empty()) {
                const int n = todo.front();
                todo.erase(todo.begin());
                if (!objs.count(n) || placed.count(n) || pageSet.count(n) || treeNodes.count(n) || users.count(n) || n == root) continue;
                placed.insert(n);
                outlines.push_back(n);
                for (int k : objs[n].refs) todo.push_back(k);
            }
        }
    }
    std::vector<int> part8;   // used by several pages, not the first
    for (int i = 1; i < npages; ++i)
        for (int n : uses[static_cast<size_t>(i)])
            if (!placed.count(n)) { part8.push_back(n); placed.insert(n); }
    std::vector<int> part9;   // everything else (page tree, outlines, document information ...)
    if (!olFirst) part9 = outlines;   // the bookmarks together, at the start of the other objects
    for (const auto& kv : objs) if (!placed.count(kv.first)) { part9.push_back(kv.first); placed.insert(kv.first); }

    // new numbers: the second half (parts 7-9) 1..m, then the linearization dictionary, part 4, the hint stream, part 6
    std::map<int, int> num;
    int next = 1;
    for (int i = 1; i < npages; ++i) for (int n : part7[static_cast<size_t>(i)]) num[n] = next++;
    for (int n : part8) num[n] = next++;
    for (int n : part9) num[n] = next++;
    const int m = next - 1;
    const int linNum = next++;
    for (int n : part4) num[n] = next++;
    const int hintNum = next++;
    for (int n : part6) num[n] = next++;
    if (olFirst) for (int n : outlines) num[n] = next++;
    const int total = next;   // /Size

    auto text = [&](int n) {
        const Obj& o = objs[n];
        std::string t = std::to_string(num[n]) + " 0 obj\n" + renumber(o.dict, num);
        if (o.stream) t += "\nstream\n" + o.data + "\nendstream";
        return t + "\nendobj\n";
    };
    std::map<int, std::string> body;
    for (const auto& kv : objs) body[kv.first] = text(kv.first);

    // the trailers' entries
    std::string ids;
    {
        size_t b, e;
        if (pdfedit::valueSpan(f.trailer, "/ID", b, e)) ids = " /ID " + f.trailer.substr(b, e - b);
    }
    std::string info;
    const int infoObj = pdfedit::refOf(f.trailer, "/Info");
    if (infoObj >= 0 && num.count(infoObj)) info = " /Info " + std::to_string(num[infoObj]) + " 0 R";
    const std::string rootRef = " /Root " + std::to_string(num[root]) + " 0 R";

    // fixed-size pieces: the header, the linearization dictionary, the first-page cross-reference table
    const std::string header = "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
    auto linText = [&](long long L, long long H0, long long H1, long long E, long long T) {
        return std::to_string(linNum) + " 0 obj\n<< /Linearized 1 /L " + padNum(L, 10) + " /H [ " + padNum(H0, 10) + " " + padNum(H1, 10) +
               " ] /O " + std::to_string(num[pages[0]]) + " /E " + padNum(E, 10) + " /N " + std::to_string(npages) + " /T " + padNum(T, 10) +
               " >>\nendobj\n";
    };
    const int n1 = total - (m + 1);   // objects in the first-page table
    auto xref1Text = [&](const std::map<int, long long>& at, long long prev) {
        std::string t = "xref\n" + std::to_string(m + 1) + " " + std::to_string(n1) + "\n";
        char e[24];
        for (int k = m + 1; k < total; ++k) { snprintf(e, sizeof(e), "%010lld 00000 n \n", at.count(k) ? at.at(k) : 0LL); t += e; }
        return t + "trailer\n<< /Size " + std::to_string(total) + rootRef + info + ids + " /Prev " + padNum(prev, 10) + " >>\nstartxref\n0\n%%EOF\n";
    };
    const size_t linSize = linText(0, 0, 0, 0, 0).size();
    const size_t xref1Size = xref1Text({}, 0).size();

    // layout without the hint stream (the hint tables give offsets as if it were not there)
    std::map<int, long long> pos;   // new object number -> offset
    long long at = static_cast<long long>(header.size() + linSize + xref1Size);
    for (int n : part4) { pos[num[n]] = at; at += static_cast<long long>(body[n].size()); }
    const long long hintAt = at;
    for (int n : part6) { pos[num[n]] = at; at += static_cast<long long>(body[n].size()); }
    long long outlineAt = at, outlineLen = 0;
    if (olFirst) {
        for (int n : outlines) { pos[num[n]] = at; at += static_cast<long long>(body[n].size()); }
        outlineLen = at - outlineAt;
    }
    const long long endFirst = at;   // adjusted
    std::vector<long long> pageStart(static_cast<size_t>(npages)), pageLen(static_cast<size_t>(npages));
    pageStart[0] = pos[num[pages[0]]];
    pageLen[0] = endFirst - pageStart[0];
    for (int i = 1; i < npages; ++i) {
        pageStart[static_cast<size_t>(i)] = at;
        for (int n : part7[static_cast<size_t>(i)]) { pos[num[n]] = at; at += static_cast<long long>(body[n].size()); }
        pageLen[static_cast<size_t>(i)] = at - pageStart[static_cast<size_t>(i)];
    }
    const long long sharedAt = at;
    std::vector<long long> sharedLen;
    for (int n : part8) { pos[num[n]] = at; sharedLen.push_back(static_cast<long long>(body[n].size())); at += sharedLen.back(); }
    if (!olFirst && !outlines.empty()) {
        outlineAt = at;
        for (int n : outlines) outlineLen += static_cast<long long>(body[n].size());
    }
    for (int n : part9) { pos[num[n]] = at; at += static_cast<long long>(body[n].size()); }

    // the shared object table: every first-page object, then each shared object (a group of one)
    std::vector<long long> groupLen;
    std::map<int, int> sharedId;   // old object -> index in the table
    for (int n : part6) { sharedId[n] = static_cast<int>(groupLen.size()); groupLen.push_back(static_cast<long long>(body[n].size())); }
    const int firstEntries = static_cast<int>(groupLen.size());
    for (size_t k = 0; k < part8.size(); ++k) { sharedId[part8[k]] = static_cast<int>(groupLen.size()); groupLen.push_back(sharedLen[k]); }

    // the page offset table
    std::vector<int> nobj(static_cast<size_t>(npages));
    std::vector<std::vector<int>> shared(static_cast<size_t>(npages));
    nobj[0] = static_cast<int>(part6.size() + (olFirst ? outlines.size() : 0));   // bookmarks in the first-page part count with it
    for (int i = 1; i < npages; ++i) {
        nobj[static_cast<size_t>(i)] = static_cast<int>(part7[static_cast<size_t>(i)].size());
        for (int n : uses[static_cast<size_t>(i)]) if (users[n].size() > 1) shared[static_cast<size_t>(i)].push_back(sharedId[n]);
    }
    const int minObj = *std::min_element(nobj.begin(), nobj.end()), maxObj = *std::max_element(nobj.begin(), nobj.end());
    const long long minLen = *std::min_element(pageLen.begin(), pageLen.end()), maxLen = *std::max_element(pageLen.begin(), pageLen.end());
    size_t maxShared = 0;
    for (const auto& v : shared) maxShared = std::max(maxShared, v.size());
    const int bObj = bitsFor(static_cast<unsigned long long>(maxObj - minObj)), bLen = bitsFor(static_cast<unsigned long long>(maxLen - minLen));
    const int bNShared = bitsFor(maxShared), bSharedId = bitsFor(groupLen.empty() ? 0 : groupLen.size() - 1);
    Bits h;
    h.put(static_cast<unsigned>(minObj), 32);
    h.put(static_cast<unsigned long long>(pageStart[0]), 32);
    h.put(static_cast<unsigned>(bObj), 16);
    h.put(static_cast<unsigned long long>(minLen), 32);
    h.put(static_cast<unsigned>(bLen), 16);
    h.put(0, 32); h.put(0, 16);   // content stream offset (not used by readers)
    h.put(0, 32); h.put(0, 16);   // content stream length (not used by readers)
    h.put(static_cast<unsigned>(bNShared), 16);
    h.put(static_cast<unsigned>(bSharedId), 16);
    h.put(0, 16);   // numerator bits
    h.put(1, 16);   // denominator
    for (int i = 0; i < npages; ++i) h.put(static_cast<unsigned>(nobj[static_cast<size_t>(i)] - minObj), bObj);
    h.flush();
    for (int i = 0; i < npages; ++i) h.put(static_cast<unsigned long long>(pageLen[static_cast<size_t>(i)] - minLen), bLen);
    h.flush();
    for (int i = 0; i < npages; ++i) h.put(shared[static_cast<size_t>(i)].size(), bNShared);
    h.flush();
    for (int i = 0; i < npages; ++i) for (int id : shared[static_cast<size_t>(i)]) h.put(static_cast<unsigned>(id), bSharedId);
    h.flush();
    h.flush();   // numerators: none
    for (int i = 0; i < npages; ++i) h.put(0, 0);
    h.flush();
    h.flush();
    const size_t sOffset = h.out.size();
    const long long minGroup = groupLen.empty() ? 0 : *std::min_element(groupLen.begin(), groupLen.end());
    const long long maxGroup = groupLen.empty() ? 0 : *std::max_element(groupLen.begin(), groupLen.end());
    const int bGroup = bitsFor(static_cast<unsigned long long>(maxGroup - minGroup));
    h.put(part8.empty() ? 0u : static_cast<unsigned>(num[part8[0]]), 32);
    h.put(part8.empty() ? 0ull : static_cast<unsigned long long>(sharedAt), 32);
    h.put(static_cast<unsigned>(firstEntries), 32);
    h.put(static_cast<unsigned>(groupLen.size()), 32);
    h.put(0, 16);   // bits for (objects in a group - 1): every group is one object
    h.put(static_cast<unsigned long long>(minGroup), 32);
    h.put(static_cast<unsigned>(bGroup), 16);
    for (long long g : groupLen) h.put(static_cast<unsigned long long>(g - minGroup), bGroup);
    h.flush();
    for (size_t k = 0; k < groupLen.size(); ++k) h.put(0, 1);   // no MD5 signatures
    h.flush();
    h.flush();   // objects per group - 1: zero bits each
    std::string oKey;
    if (!outlines.empty()) {   // the outline hint table (a generic hint table)
        oKey = " /O " + std::to_string(h.out.size());
        h.put(static_cast<unsigned>(num[outlines[0]]), 32);
        h.put(static_cast<unsigned long long>(outlineAt), 32);
        h.put(static_cast<unsigned>(outlines.size()), 32);
        h.put(static_cast<unsigned long long>(outlineLen), 32);
        h.flush();
    }
    const std::string hint = std::to_string(hintNum) + " 0 obj\n<< /S " + std::to_string(sOffset) + oKey + " /Length " + std::to_string(h.out.size()) +
                             " >>\nstream\n" + h.out + "\nendstream\nendobj\n";
    const long long hintLen = static_cast<long long>(hint.size());

    // the real offsets: everything from the hint stream on moves by its length
    for (auto& kv : pos) if (kv.second >= hintAt) kv.second += hintLen;
    pos[hintNum] = hintAt;
    pos[linNum] = static_cast<long long>(header.size());
    const long long E = endFirst + hintLen;
    const long long xref2At = at + hintLen;
    std::string xref2 = "xref\n0 " + std::to_string(m + 1) + "\n";
    const long long T = xref2At + static_cast<long long>(xref2.size()) - 1;   // the line break before the first entry
    xref2 += "0000000000 65535 f \n";
    char e[24];
    for (int k = 1; k <= m; ++k) { snprintf(e, sizeof(e), "%010lld 00000 n \n", pos.count(k) ? pos[k] : 0LL); xref2 += e; }
    const long long xref1At = static_cast<long long>(header.size() + linSize);
    xref2 += "trailer\n<< /Size " + std::to_string(total) + rootRef + info + ids + " >>\nstartxref\n" + std::to_string(xref1At) + "\n%%EOF\n";
    const long long L = xref2At + static_cast<long long>(xref2.size());

    std::string o;
    o.reserve(static_cast<size_t>(L));
    o += header;
    o += linText(L, hintAt, hintLen, E, T);
    o += xref1Text(pos, xref2At);
    for (int n : part4) o += body[n];
    o += hint;
    for (int n : part6) o += body[n];
    if (olFirst) for (int n : outlines) o += body[n];
    for (int i = 1; i < npages; ++i) for (int n : part7[static_cast<size_t>(i)]) o += body[n];
    for (int n : part8) o += body[n];
    for (int n : part9) o += body[n];
    o += xref2;
    if (static_cast<long long>(o.size()) != L) return fail(L"The file could not be laid out.");
    out.assign(o.begin(), o.end());
    return true;
}

bool linearize(Document& d, std::vector<unsigned char>& out, std::wstring* why) {
    std::vector<unsigned char> bytes;
    if (!d.serialize(bytes, FPDF_NO_INCREMENTAL)) { if (why) *why = L"The document could not be written."; return false; }
    return linearize(bytes, out, why);
}

} // namespace webview
