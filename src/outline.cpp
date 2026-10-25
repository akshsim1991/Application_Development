#include "outline.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace outline {
namespace {

struct File {
    const std::string& s;
    std::map<int, size_t> offsets;     // object number -> offset of "N G obj"
    std::string trailer;               // the trailer dictionary text
    size_t xrefPos = 0;
    int size = 0;

    explicit File(const std::string& text) : s(text) {}

    static bool isDigit(char c) { return c >= '0' && c <= '9'; }

    bool parseXref() {
        size_t sx = s.rfind("startxref");
        if (sx == std::string::npos) return false;
        xrefPos = static_cast<size_t>(strtoull(s.c_str() + sx + 9, nullptr, 10));
        if (xrefPos == 0 || xrefPos + 4 > s.size() || s.compare(xrefPos, 4, "xref") != 0) return false;
        size_t p = xrefPos + 4;
        auto skipWs = [&] { while (p < s.size() && (s[p] == ' ' || s[p] == '\r' || s[p] == '\n' || s[p] == '\t')) ++p; };
        for (;;) {
            skipWs();
            if (s.compare(p, 7, "trailer") == 0) { p += 7; break; }
            char* e = nullptr;
            long first = strtol(s.c_str() + p, &e, 10);
            long count = strtol(e, &e, 10);
            p = static_cast<size_t>(e - s.c_str());
            skipWs();
            for (long i = 0; i < count; ++i) {
                if (p + 18 > s.size()) return false;
                size_t off = static_cast<size_t>(strtoull(s.c_str() + p, nullptr, 10));
                char kind = s[p + 17];
                if (kind == 'n') offsets[static_cast<int>(first + i)] = off;
                p += 20;
                if (p > 0 && s[p - 1] != '\n' && s[p - 1] != '\r') {}
            }
        }
        skipWs();
        size_t end = s.find("startxref", p);
        if (end == std::string::npos) return false;
        trailer = s.substr(p, end - p);
        size_t gt = trailer.rfind(">>");
        if (gt == std::string::npos) return false;
        trailer.resize(gt + 2);
        size_t sz = trailer.find("/Size");
        if (sz == std::string::npos) return false;
        size = atoi(trailer.c_str() + sz + 5);
        return size > 0;
    }

    // text between "obj" and "endobj" of an object
    bool object(int n, std::string& body) const {
        auto it = offsets.find(n);
        if (it == offsets.end()) return false;
        size_t o = s.find("obj", it->second);
        size_t e = s.find("endobj", o);
        if (o == std::string::npos || e == std::string::npos) return false;
        body = s.substr(o + 3, e - o - 3);
        return true;
    }
};

// "/Key N 0 R" -> N (or -1)
int refOf(const std::string& dict, const char* key) {
    size_t p = 0;
    const std::string k = key;
    while ((p = dict.find(k, p)) != std::string::npos) {
        size_t q = p + k.size();
        if (q < dict.size() && std::isalnum(static_cast<unsigned char>(dict[q]))) { p = q; continue; }   // a longer key such as /Pages vs /PagesX
        while (q < dict.size() && std::isspace(static_cast<unsigned char>(dict[q]))) ++q;
        if (q < dict.size() && File::isDigit(dict[q])) {
            char* e = nullptr;
            long n = strtol(dict.c_str() + q, &e, 10);
            long g = strtol(e, &e, 10);
            (void)g;
            while (*e == ' ') ++e;
            if (*e == 'R') return static_cast<int>(n);
        }
        p = q;
    }
    return -1;
}

std::vector<int> kidsOf(const std::string& dict) {
    std::vector<int> out;
    size_t k = dict.find("/Kids");
    if (k == std::string::npos) return out;
    size_t b = dict.find('[', k), e = dict.find(']', b);
    if (b == std::string::npos || e == std::string::npos) return out;
    size_t p = b + 1;
    while (p < e) {
        while (p < e && !File::isDigit(dict[p])) ++p;
        if (p >= e) break;
        char* x = nullptr;
        long n = strtol(dict.c_str() + p, &x, 10);
        strtol(x, &x, 10);
        while (*x == ' ') ++x;
        if (*x == 'R') { out.push_back(static_cast<int>(n)); p = static_cast<size_t>(x - dict.c_str()) + 1; } else break;
    }
    return out;
}

// object number of the page with this index, walking the page tree
int pageRef(const File& f, int node, int& index, std::set<int>& seen) {
    if (!seen.insert(node).second) return -1;
    std::string body;
    if (!f.object(node, body)) return -1;
    const bool isPages = body.find("/Kids") != std::string::npos;
    if (!isPages) { if (index == 0) return node; --index; return -1; }
    for (int kid : kidsOf(body)) {
        int r = pageRef(f, kid, index, seen);
        if (r >= 0) return r;
    }
    return -1;
}

std::string titleHex(const std::wstring& t) {
    std::string h = "<FEFF";
    char b[8];
    for (wchar_t c : t) { snprintf(b, sizeof(b), "%04X", static_cast<unsigned>(c) & 0xFFFF); h += b; }
    return h + ">";
}

// puts `entry` before the closing ">>" of an object's dictionary
std::string withEntry(std::string body, const std::string& entry) {
    size_t gt = body.rfind(">>");
    if (gt == std::string::npos) return {};
    body.insert(gt, entry + " ");
    return body;
}

std::string replaceKey(std::string body, const char* key, const std::string& value) {
    size_t p = body.find(key);
    if (p == std::string::npos) return withEntry(body, std::string(key) + " " + value);
    size_t q = p + strlen(key);
    size_t e = q;
    while (e < body.size() && std::isspace(static_cast<unsigned char>(body[e]))) ++e;
    // skip the old value: "N 0 R" or a number
    char* x = nullptr;
    strtol(body.c_str() + e, &x, 10);
    size_t after = static_cast<size_t>(x - body.c_str());
    size_t save = after;
    char* y = nullptr;
    strtol(x, &y, 10);
    while (*y == ' ') ++y;
    if (*y == 'R' && y != x) after = static_cast<size_t>(y - body.c_str()) + 1; else after = save;
    return body.substr(0, q) + " " + value + body.substr(after);
}

} // namespace

bool addBookmark(const std::vector<unsigned char>& pdf, int pageIndex, const std::wstring& title, std::vector<unsigned char>& out) {
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    if (!f.parseXref()) return false;
    const int root = refOf(f.trailer, "/Root");
    if (root < 0) return false;
    std::string cat;
    if (!f.object(root, cat)) return false;
    const int pagesNode = refOf(cat, "/Pages");
    if (pagesNode < 0) return false;
    int idx = pageIndex;
    std::set<int> seen;
    const int page = pageRef(f, pagesNode, idx, seen);
    if (page < 0) return false;

    int next = f.size;
    const int item = next++;
    std::map<int, std::string> write;   // object number -> full object text (without "N 0 obj")
    int outlines = refOf(cat, "/Outlines");
    std::string itemText = "<< /Title " + titleHex(title) + " /Parent ";
    if (outlines < 0) {
        outlines = next++;
        itemText += std::to_string(outlines) + " 0 R /Dest [" + std::to_string(page) + " 0 R /Fit] >>";
        write[outlines] = "<< /Type /Outlines /First " + std::to_string(item) + " 0 R /Last " + std::to_string(item) + " 0 R /Count 1 >>";
        write[root] = withEntry(cat, "/Outlines " + std::to_string(outlines) + " 0 R");
        if (write[root].empty()) return false;
    } else {
        std::string ob;
        if (!f.object(outlines, ob)) return false;
        const int last = refOf(ob, "/Last");
        itemText += std::to_string(outlines) + " 0 R ";
        if (last >= 0) {
            std::string lb;
            if (!f.object(last, lb)) return false;
            itemText += "/Prev " + std::to_string(last) + " 0 R ";
            std::string nl = withEntry(lb, "/Next " + std::to_string(item) + " 0 R");
            if (nl.empty()) return false;
            write[last] = nl;
        }
        itemText += "/Dest [" + std::to_string(page) + " 0 R /Fit] >>";
        std::string nb = ob;
        if (last >= 0) nb = replaceKey(nb, "/Last", std::to_string(item) + " 0 R");
        else { nb = withEntry(nb, "/First " + std::to_string(item) + " 0 R"); nb = withEntry(nb, "/Last " + std::to_string(item) + " 0 R"); }
        size_t cp = nb.find("/Count");
        int count = cp == std::string::npos ? 0 : atoi(nb.c_str() + cp + 6);
        nb = replaceKey(nb, "/Count", std::to_string(count < 0 ? count - 1 : count + 1));
        write[outlines] = nb;
    }
    write[item] = itemText;

    std::string add = s;
    if (add.empty() || add.back() != '\n') add += "\n";
    std::map<int, size_t> pos;
    for (auto& kv : write) {
        pos[kv.first] = add.size();
        add += std::to_string(kv.first) + " 0 obj\n" + kv.second + "\nendobj\n";
    }
    const size_t xr = add.size();
    add += "xref\n";
    for (auto& kv : pos) {
        char line[64];
        snprintf(line, sizeof(line), "%d 1\n%010zu 00000 n \n", kv.first, kv.second);
        add += line;
    }
    std::string tr = f.trailer;
    tr = replaceKey(tr, "/Size", std::to_string(next));
    size_t pv = tr.find("/Prev");
    if (pv != std::string::npos) {   // drop an old /Prev
        size_t e = pv + 5;
        while (e < tr.size() && std::isspace(static_cast<unsigned char>(tr[e]))) ++e;
        while (e < tr.size() && File::isDigit(tr[e])) ++e;
        tr.erase(pv, e - pv);
    }
    tr = withEntry(tr, "/Prev " + std::to_string(f.xrefPos));
    add += "trailer\n" + tr + "\nstartxref\n" + std::to_string(xr) + "\n%%EOF\n";
    out.assign(add.begin(), add.end());
    return true;
}

} // namespace outline
