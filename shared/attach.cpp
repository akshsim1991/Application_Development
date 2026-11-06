#include "attach.hpp"
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <set>

namespace attach {

std::vector<File> list(Document& d) {
    std::vector<File> out;
    const auto& api = pdfium::api();
    if (!d.handle()) return out;
    const int n = api.FPDFDoc_GetAttachmentCount(d.handle());
    for (int i = 0; i < n; ++i) {
        FPDF_ATTACHMENT a = api.FPDFDoc_GetAttachment(d.handle(), i);
        File f;
        if (a) {
            const unsigned long bytes = api.FPDFAttachment_GetName(a, nullptr, 0);
            if (bytes >= 4) {
                std::vector<unsigned short> buf(bytes / 2 + 1);
                api.FPDFAttachment_GetName(a, buf.data(), bytes);
                f.name.assign(buf.begin(), buf.begin() + static_cast<long>(bytes / 2 - 1));
            }
            unsigned long len = 0;
            if (api.FPDFAttachment_GetFile(a, nullptr, 0, &len)) f.size = len;
        }
        if (f.name.empty()) f.name = L"attachment " + std::to_wstring(i + 1);
        out.push_back(f);
    }
    return out;
}

bool get(Document& d, int index, std::vector<unsigned char>& out) {
    const auto& api = pdfium::api();
    FPDF_ATTACHMENT a = d.handle() ? api.FPDFDoc_GetAttachment(d.handle(), index) : nullptr;
    if (!a) return false;
    unsigned long len = 0;
    if (!api.FPDFAttachment_GetFile(a, nullptr, 0, &len)) return false;
    out.resize(len);
    if (len == 0) return true;
    unsigned long got = 0;
    return api.FPDFAttachment_GetFile(a, out.data(), len, &got) && got == len;
}

bool add(Document& d, const std::wstring& name, const std::vector<unsigned char>& data) {
    const auto& api = pdfium::api();
    if (!d.handle() || name.empty()) return false;
    d.beginChange(L"Attach a file");
    std::vector<unsigned short> wname(name.begin(), name.end());
    wname.push_back(0);
    FPDF_ATTACHMENT a = api.FPDFDoc_AddAttachment(d.handle(), wname.data());
    if (!a || !api.FPDFAttachment_SetFile(a, d.handle(), data.empty() ? nullptr : data.data(), static_cast<unsigned long>(data.size()))) {
        if (a) api.FPDFDoc_DeleteAttachment(d.handle(), api.FPDFDoc_GetAttachmentCount(d.handle()) - 1);
        d.cancelChange();
        return false;
    }
    return true;
}

bool remove(Document& d, int index) {
    const auto& api = pdfium::api();
    if (!d.handle() || index < 0 || index >= api.FPDFDoc_GetAttachmentCount(d.handle())) return false;
    d.beginChange(L"Remove an attached file");
    if (!api.FPDFDoc_DeleteAttachment(d.handle(), index)) { d.cancelChange(); return false; }
    return true;
}

} // namespace attach

namespace layers {
using namespace pdfedit;

namespace {

std::vector<int> refsIn(const std::string& arr) {
    std::vector<int> out;
    size_t p = 0;
    while (p < arr.size()) {
        if (!File::isDigit(arr[p])) { ++p; continue; }
        char* e = nullptr;
        const long n = strtol(arr.c_str() + p, &e, 10);
        char* g = nullptr;
        strtol(e, &g, 10);
        while (*g == ' ' || *g == '\n' || *g == '\r') ++g;
        if (*g == 'R' && g != e) { out.push_back(static_cast<int>(n)); p = static_cast<size_t>(g - arr.c_str()) + 1; }
        else p = static_cast<size_t>(e - arr.c_str());
    }
    return out;
}

std::string refArray(const std::vector<int>& v) {
    std::string s = "[";
    for (int n : v) s += std::to_string(n) + " 0 R ";
    return s + "]";
}

// The value of `key` in `dict`; a reference is followed (its number in `ref`, else -1).
bool resolved(const File& f, const std::string& dict, const char* key, std::string& value, int& ref) {
    size_t b, e;
    ref = -1;
    if (!valueSpan(dict, key, b, e)) return false;
    value = dict.substr(b, e - b);
    if (!value.empty() && value.back() == 'R') {
        ref = atoi(value.c_str());
        return f.object(ref, value);
    }
    return true;
}

struct State {
    int root = -1, ocpRef = -1, dRef = -1;
    std::string cat, ocp, d;
    std::vector<int> ocgs;
    std::set<int> off;
};

bool read(const File& f, State& st) {
    st.root = refOf(f.trailer, "/Root");
    if (st.root < 0 || !f.object(st.root, st.cat)) return false;
    if (!resolved(f, st.cat, "/OCProperties", st.ocp, st.ocpRef)) return false;
    std::string arr;
    int r;
    if (!resolved(f, st.ocp, "/OCGs", arr, r)) return false;
    st.ocgs = refsIn(arr);
    if (!resolved(f, st.ocp, "/D", st.d, st.dRef)) st.d = "<< >>";
    std::string base;
    size_t b, e;
    const bool allOff = valueSpan(st.d, "/BaseState", b, e) && st.d.compare(b, e - b, "/OFF") == 0;
    if (allOff) {
        std::string on;
        std::set<int> onSet;
        if (resolved(f, st.d, "/ON", on, r)) for (int n : refsIn(on)) onSet.insert(n);
        for (int n : st.ocgs) if (!onSet.count(n)) st.off.insert(n);
    } else {
        std::string offs;
        if (resolved(f, st.d, "/OFF", offs, r)) for (int n : refsIn(offs)) st.off.insert(n);
    }
    return true;
}

} // namespace

std::wstring decodeString(const std::string& t) {
    std::string bytes;
    if (!t.empty() && t[0] == '<') {
        std::string hex;
        for (char c : t) if (std::isxdigit(static_cast<unsigned char>(c))) hex += c;
        if (hex.size() % 2) hex += '0';
        for (size_t i = 0; i + 1 < hex.size(); i += 2) bytes += static_cast<char>(strtol(hex.substr(i, 2).c_str(), nullptr, 16));
    } else if (!t.empty() && t[0] == '(') {
        for (size_t i = 1; i + 1 < t.size(); ++i) {
            char c = t[i];
            if (c == '\\' && i + 2 < t.size()) {
                const char n = t[++i];
                if (n == 'n') c = '\n'; else if (n == 'r') c = '\r'; else if (n == 't') c = '\t';
                else if (n >= '0' && n <= '7') {
                    int v = 0, k = 0;
                    while (k < 3 && i < t.size() - 1 && t[i] >= '0' && t[i] <= '7') { v = v * 8 + (t[i] - '0'); ++i; ++k; }
                    --i;
                    c = static_cast<char>(v);
                } else c = n;
            }
            bytes += c;
        }
    } else return {};
    std::wstring out;
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF) {
        for (size_t i = 2; i + 1 < bytes.size(); i += 2)
            out += static_cast<wchar_t>((static_cast<unsigned char>(bytes[i]) << 8) | static_cast<unsigned char>(bytes[i + 1]));
    } else for (char c : bytes) out += static_cast<wchar_t>(static_cast<unsigned char>(c));
    return out;
}

std::vector<Layer> list(const std::vector<unsigned char>& pdf) {
    std::vector<Layer> out;
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    State st;
    if (!f.parseXref() || !read(f, st)) return out;
    for (int n : st.ocgs) {
        std::string body;
        if (!f.object(n, body)) continue;
        Layer l;
        l.object = n;
        size_t b, e;
        if (valueSpan(body, "/Name", b, e)) l.name = decodeString(body.substr(b, e - b));
        if (l.name.empty()) l.name = L"Layer " + std::to_wstring(out.size() + 1);
        l.visible = !st.off.count(n);
        out.push_back(l);
    }
    return out;
}

bool setVisible(const std::vector<unsigned char>& pdf, const std::vector<std::pair<int, bool>>& changes, std::vector<unsigned char>& out) {
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    State st;
    if (!f.parseXref() || !read(f, st)) return false;
    for (const auto& c : changes) {
        if (std::find(st.ocgs.begin(), st.ocgs.end(), c.first) == st.ocgs.end()) continue;
        if (c.second) st.off.erase(c.first); else st.off.insert(c.first);
    }
    std::vector<int> off(st.off.begin(), st.off.end()), on;
    for (int n : st.ocgs) if (!st.off.count(n)) on.push_back(n);
    std::string d = setKey(st.d, "/BaseState", "/ON");
    d = setKey(d, "/ON", refArray(on));
    d = setKey(d, "/OFF", refArray(off));
    if (d.empty()) return false;
    std::map<int, std::string> write;
    if (st.dRef >= 0) write[st.dRef] = d;
    else {
        std::string ocp = setKey(st.ocp, "/D", d);
        if (ocp.empty()) return false;
        if (st.ocpRef >= 0) write[st.ocpRef] = ocp;
        else {
            const std::string cat = setKey(st.cat, "/OCProperties", ocp);
            if (cat.empty()) return false;
            write[st.root] = cat;
        }
    }
    out = appendIncremental(s, f, write, f.size);
    return true;
}

std::vector<Layer> list(Document& d) {
    std::vector<unsigned char> bytes;
    if (!d.serialize(bytes)) return {};
    return list(bytes);
}

bool setVisible(Document& d, const std::vector<std::pair<int, bool>>& changes) {
    std::vector<unsigned char> bytes, out;
    if (!d.serialize(bytes) || !setVisible(bytes, changes, out)) return false;
    d.beginChange(L"Show or hide layers");
    if (d.reload(std::move(out)) != OpenResult::Ok) { d.rollbackChange(); return false; }
    return true;
}

} // namespace layers
