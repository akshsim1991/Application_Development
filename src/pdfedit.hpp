#pragma once
// Small helpers for editing a PDF written by PDFium (classic cross-reference table, plain objects) by appending an
// incremental update. Used by bookmarks and form fields.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace pdfedit {

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
inline int refOf(const std::string& dict, const char* key) {
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

inline std::vector<int> kidsOf(const std::string& dict) {
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
inline int pageRef(const File& f, int node, int& index, std::set<int>& seen) {
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

inline std::string titleHex(const std::wstring& t) {
    std::string h = "<FEFF";
    char b[8];
    for (wchar_t c : t) { snprintf(b, sizeof(b), "%04X", static_cast<unsigned>(c) & 0xFFFF); h += b; }
    return h + ">";
}

// puts `entry` before the closing ">>" of an object's dictionary
inline std::string withEntry(std::string body, const std::string& entry) {
    size_t gt = body.rfind(">>");
    if (gt == std::string::npos) return {};
    body.insert(gt, entry + " ");
    return body;
}

inline std::string replaceKey(std::string body, const char* key, const std::string& value) {
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


// ---- more helpers ----

// Position of the value that follows `key` in a dictionary text (a name, number, reference, string, array or nested dictionary).
inline bool valueSpan(const std::string& d, const char* key, size_t& begin, size_t& end) {
    size_t p = 0;
    const std::string k = key;
    while ((p = d.find(k, p)) != std::string::npos) {
        size_t q = p + k.size();
        if (q < d.size() && (std::isalnum(static_cast<unsigned char>(d[q])))) { p = q; continue; }
        while (q < d.size() && std::isspace(static_cast<unsigned char>(d[q]))) ++q;
        begin = q;
        if (q >= d.size()) return false;
        auto balanced = [&](char open, char close) {
            int depth = 0;
            for (size_t i = q; i < d.size(); ++i) {
                if (d[i] == '\\') { ++i; continue; }
                if (d[i] == open) ++depth; else if (d[i] == close) { if (--depth == 0) { end = i + 1; return true; } }
            }
            return false;
        };
        if (d.compare(q, 2, "<<") == 0) {
            int depth = 0;
            for (size_t i = q; i + 1 < d.size(); ++i) {
                if (d.compare(i, 2, "<<") == 0) { ++depth; ++i; }
                else if (d.compare(i, 2, ">>") == 0) { if (--depth == 0) { end = i + 2; return true; } ++i; }
            }
            return false;
        }
        if (d[q] == '[') return balanced('[', ']');
        if (d[q] == '(') return balanced('(', ')');
        if (d[q] == '/') { end = q + 1; while (end < d.size() && !std::isspace(static_cast<unsigned char>(d[end])) && !strchr("/[]<>()", d[end])) ++end; return true; }
        if (File::isDigit(d[q]) || d[q] == '-' || d[q] == '.') {
            char* e = nullptr;
            strtod(d.c_str() + q, &e);
            end = static_cast<size_t>(e - d.c_str());
            // "N G R" is one value
            size_t r = end;
            while (r < d.size() && d[r] == ' ') ++r;
            if (r < d.size() && File::isDigit(d[r])) {
                char* e2 = nullptr;
                strtol(d.c_str() + r, &e2, 10);
                size_t t = static_cast<size_t>(e2 - d.c_str());
                while (t < d.size() && d[t] == ' ') ++t;
                if (t < d.size() && d[t] == 'R') end = t + 1;
            }
            return true;
        }
        end = q;
        while (end < d.size() && !std::isspace(static_cast<unsigned char>(d[end])) && !strchr("/[]<>", d[end])) ++end;   // true / false / null
        return true;
    }
    return false;
}

inline std::string setKey(std::string dict, const char* key, const std::string& value) {
    size_t b, e;
    if (valueSpan(dict, key, b, e)) return dict.substr(0, b) + value + dict.substr(e);
    return withEntry(dict, std::string(key) + " " + value);
}

inline std::string pdfString(const std::wstring& t) {
    bool ascii = true;
    for (wchar_t c : t) if (c > 0x7E || c < 0x20) ascii = false;
    if (ascii) {
        std::string s = "(";
        for (wchar_t c : t) { if (c == '(' || c == ')' || c == '\\') s += '\\'; s += static_cast<char>(c); }
        return s + ")";
    }
    return titleHex(t);
}

// Appends the objects in `write` (number -> text without "N 0 obj") as an incremental update. `newSize` is the new /Size.
inline std::vector<unsigned char> appendIncremental(const std::string& s, const File& f, const std::map<int, std::string>& write, int newSize) {
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
    tr = replaceKey(tr, "/Size", std::to_string(newSize));
    size_t pv = tr.find("/Prev");
    if (pv != std::string::npos) {
        size_t e = pv + 5;
        while (e < tr.size() && std::isspace(static_cast<unsigned char>(tr[e]))) ++e;
        while (e < tr.size() && File::isDigit(tr[e])) ++e;
        tr.erase(pv, e - pv);
    }
    tr = withEntry(tr, "/Prev " + std::to_string(f.xrefPos));
    add += "trailer\n" + tr + "\nstartxref\n" + std::to_string(xr) + "\n%%EOF\n";
    return std::vector<unsigned char>(add.begin(), add.end());
}

} // namespace pdfedit
