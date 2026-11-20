// Minimal JSON reader/writer for the WebView2 bridge (UTF-8 in/out, no dependencies).
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdlib>
#include <cstdio>

namespace js {
struct V {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false; double n = 0; std::string s; std::vector<V> a; std::vector<std::pair<std::string, V>> o;
    const V* get(const char* k) const { if (t == Obj) for (auto& p : o) if (p.first == k) return &p.second; return nullptr; }
    std::string str(const char* k, const std::string& d = "") const { auto* v = get(k); return v && v->t == Str ? v->s : d; }
    double num(const char* k, double d = 0) const { auto* v = get(k); return v && v->t == Num ? v->n : (v && v->t == Bool ? (v->b ? 1 : 0) : d); }
    bool boolean(const char* k, bool d = false) const { auto* v = get(k); return v ? (v->t == Bool ? v->b : (v->t == Num ? v->n != 0 : d)) : d; }
    std::vector<std::string> strs(const char* k) const { std::vector<std::string> r; if (auto* v = get(k)) if (v->t == Arr) for (auto& e : v->a) if (e.t == Str) r.push_back(e.s); return r; }
};
namespace detail {
struct P {
    const std::string& s; size_t i = 0; int depth = 0;
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
    static void utf8(std::string& o, unsigned c) {
        if (c < 0x80) o += (char)c; else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 63)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
    }
    bool hex4(unsigned& v) { if (i + 4 > s.size()) return false; v = 0; for (int k = 0; k < 4; ++k) { char c = s[i++]; v <<= 4; if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10; else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return false; } return true; }
    bool str(std::string& o) {
        if (i >= s.size() || s[i] != '"') return false; ++i;
        while (i < s.size()) {
            char c = s[i++]; if (c == '"') return true;
            if (c != '\\') { o += c; continue; }
            if (i >= s.size()) return false; char e = s[i++];
            switch (e) { case 'n': o += '\n'; break; case 't': o += '\t'; break; case 'r': o += '\r'; break; case 'b': o += '\b'; break; case 'f': o += '\f'; break; case '/': o += '/'; break; case '\\': o += '\\'; break; case '"': o += '"'; break;
                case 'u': { unsigned u; if (!hex4(u)) return false; if (u >= 0xD800 && u < 0xDC00 && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') { i += 2; unsigned lo; if (!hex4(lo)) return false; u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); } utf8(o, u); break; }
                default: return false; }
        }
        return false;
    }
    bool val(V& v) {
        ws(); if (i >= s.size() || ++depth > 64) return false; struct D { int& d; ~D() { --d; } } dd{depth};
        char c = s[i];
        if (c == '{') { v.t = V::Obj; ++i; ws(); if (i < s.size() && s[i] == '}') { ++i; return true; }
            for (;;) { ws(); std::string k; if (!str(k)) return false; ws(); if (i >= s.size() || s[i++] != ':') return false; V x; if (!val(x)) return false; v.o.emplace_back(std::move(k), std::move(x)); ws(); if (i >= s.size()) return false; if (s[i] == ',') { ++i; continue; } if (s[i] == '}') { ++i; return true; } return false; } }
        if (c == '[') { v.t = V::Arr; ++i; ws(); if (i < s.size() && s[i] == ']') { ++i; return true; }
            for (;;) { V x; if (!val(x)) return false; v.a.push_back(std::move(x)); ws(); if (i >= s.size()) return false; if (s[i] == ',') { ++i; continue; } if (s[i] == ']') { ++i; return true; } return false; } }
        if (c == '"') { v.t = V::Str; return str(v.s); }
        if (s.compare(i, 4, "true") == 0) { v.t = V::Bool; v.b = true; i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { v.t = V::Bool; v.b = false; i += 5; return true; }
        if (s.compare(i, 4, "null") == 0) { v.t = V::Null; i += 4; return true; }
        char* end = nullptr; double d = strtod(s.c_str() + i, &end); if (end == s.c_str() + i) return false; v.t = V::Num; v.n = d; i = (size_t)(end - s.c_str()); return true;
    }
};
}
inline bool Parse(const std::string& s, V& out) { detail::P p{s}; if (!p.val(out)) return false; p.ws(); return p.i == s.size(); }

// ---- writer: Q() quotes a UTF-8 string; Obj/Arr build text incrementally ----
inline std::string Q(const std::string& s) {
    std::string o = "\""; for (unsigned char c : s) { switch (c) { case '"': o += "\\\""; break; case '\\': o += "\\\\"; break; case '\n': o += "\\n"; break; case '\r': o += "\\r"; break; case '\t': o += "\\t"; break;
        default: if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c; } }
    return o + "\"";
}
inline std::string Dump(const V& v) {   // serialise a parsed value back to JSON text
    switch (v.t) { case V::Null: return "null"; case V::Bool: return v.b ? "true" : "false"; case V::Num: { char b[40]; snprintf(b, sizeof b, "%.15g", v.n); return b; } case V::Str: return Q(v.s);
        case V::Arr: { std::string o = "["; for (size_t i = 0; i < v.a.size(); ++i) o += (i ? "," : "") + Dump(v.a[i]); return o + "]"; }
        case V::Obj: { std::string o = "{"; for (size_t i = 0; i < v.o.size(); ++i) o += (i ? "," : "") + Q(v.o[i].first) + ":" + Dump(v.o[i].second); return o + "}"; } } return "null";
}
struct Obj { std::string s = "{"; bool first = true;
    Obj& raw(const char* k, const std::string& v) { if (!first) s += ","; first = false; s += Q(k) + ":" + v; return *this; }
    Obj& str(const char* k, const std::string& v) { return raw(k, Q(v)); }
    Obj& num(const char* k, double v) { char b[40]; snprintf(b, sizeof b, "%.0f", v); return raw(k, b); }
    Obj& boolean(const char* k, bool v) { return raw(k, v ? "true" : "false"); }
    std::string done() const { return s + "}"; } };
struct Arr { std::string s = "["; bool first = true; Arr& add(const std::string& rawJson) { if (!first) s += ","; first = false; s += rawJson; return *this; } std::string done() const { return s + "]"; } };
}
