#include "core.h"
#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <cstdlib>
#include <cctype>
#include <set>
#include <regex>
#include <cstdio>

namespace core {

static int WInt(const std::wstring& s) { return (int)wcstol(s.c_str(), nullptr, 10); }
static std::wstring Base64Encode(const std::wstring& s);
static bool Base64Decode(const std::wstring& s, std::wstring& out);
static std::wstring JsonFormat(const std::wstring& s, bool minify);
static std::wstring XmlFormat(const std::wstring& s);
static std::wstring SqlFormat(const std::wstring& s);

// ---------------------------------------------------------------- strings
std::wstring Lower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = (wchar_t)std::towlower(c);
    return r;
}

static bool IsSpace(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0xA0 || c == 0x3000; }

std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && IsSpace(s[a])) a++;
    while (b > a && IsSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

bool StartsWith(const std::wstring& s, const wchar_t* p) {
    size_t n = 0;
    while (p[n]) n++;
    return s.size() >= n && s.compare(0, n, p) == 0;
}

std::vector<std::wstring> SplitLines(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i < s.size(); i++) {
        wchar_t c = s[i];
        if (c == L'\r') {
            if (i + 1 < s.size() && s[i + 1] == L'\n') i++;
            out.push_back(cur);
            cur.clear();
        } else if (c == L'\n') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty() || (s.size() && out.empty())) out.push_back(cur);
    return out;
}

std::wstring Join(const std::vector<std::wstring>& parts, const std::wstring& sep) {
    std::wstring r;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) r += sep;
        r += parts[i];
    }
    return r;
}

static std::wstring TitleCase(const std::wstring& s) {
    std::wstring r = s;
    bool start = true;
    for (auto& c : r) {
        if (std::iswalpha(c)) {
            c = start ? (wchar_t)std::towupper(c) : (wchar_t)std::towlower(c);
            start = false;
        } else if (std::iswdigit(c) || c == L'\'') {
            start = false;
        } else {
            start = true;
        }
    }
    return r;
}

static std::wstring SentenceCase(const std::wstring& s) {
    std::wstring r = Lower(s);
    bool start = true;
    for (auto& c : r) {
        if (std::iswalpha(c)) {
            if (start) c = (wchar_t)std::towupper(c);
            start = false;
        } else if (c == L'.' || c == L'!' || c == L'?' || c == L'\n') {
            start = true;
        }
    }
    return r;
}

std::wstring Apply(Xf x, const std::wstring& s) {
    switch (x) {
    case Xf::Upper: { std::wstring r = s; for (auto& c : r) c = (wchar_t)std::towupper(c); return r; }
    case Xf::Lower: return Lower(s);
    case Xf::Title: return TitleCase(s);
    case Xf::Sentence: return SentenceCase(s);
    case Xf::Trim: {
        // trim each line's ends and drop empty lines at the very start/end
        auto lines = SplitLines(s);
        for (auto& l : lines) l = Trim(l);
        while (!lines.empty() && lines.front().empty()) lines.erase(lines.begin());
        while (!lines.empty() && lines.back().empty()) lines.pop_back();
        return Join(lines, L"\r\n");
    }
    case Xf::OneLine: {
        auto lines = SplitLines(s);
        std::vector<std::wstring> kept;
        for (auto& l : lines) { auto t = Trim(l); if (!t.empty()) kept.push_back(t); }
        return Join(kept, L" ");
    }
    case Xf::SortLines: {
        auto lines = SplitLines(s);
        std::stable_sort(lines.begin(), lines.end(), [](const std::wstring& a, const std::wstring& b) { return Lower(a) < Lower(b); });
        return Join(lines, L"\r\n");
    }
    case Xf::UniqueLines: {
        auto lines = SplitLines(s);
        std::set<std::wstring> seen;
        std::vector<std::wstring> kept;
        for (auto& l : lines) if (seen.insert(l).second) kept.push_back(l);
        return Join(kept, L"\r\n");
    }
    case Xf::ReverseLines: {
        auto lines = SplitLines(s);
        std::reverse(lines.begin(), lines.end());
        return Join(lines, L"\r\n");
    }
    case Xf::NumberLines: {
        auto lines = SplitLines(s);
        for (size_t i = 0; i < lines.size(); i++) lines[i] = std::to_wstring(i + 1) + L". " + lines[i];
        return Join(lines, L"\r\n");
    }
    case Xf::Plain: return s;
    case Xf::BlankLines: {
        auto lines = SplitLines(s);
        std::vector<std::wstring> kept;
        bool lastBlank = true;
        for (auto& l : lines) {
            bool blank = Trim(l).empty();
            if (blank && lastBlank) continue;
            kept.push_back(blank ? L"" : l);
            lastBlank = blank;
        }
        while (!kept.empty() && kept.back().empty()) kept.pop_back();
        return Join(kept, L"\r\n");
    }
    case Xf::Base64Enc: return Base64Encode(s);
    case Xf::Base64Dec: { std::wstring o; return Base64Decode(s, o) ? o : s; }
    case Xf::JsonFormat: return JsonFormat(s, false);
    case Xf::JsonMin: return JsonFormat(s, true);
    case Xf::XmlFormat: return XmlFormat(s);
    case Xf::SqlFormat: return SqlFormat(s);
    case Xf::SlashesFwd: { std::wstring r = s; for (auto& c : r) if (c == L'\\') c = L'/'; return r; }
    case Xf::SlashesBack: { std::wstring r = s; for (auto& c : r) if (c == L'/') c = L'\\'; return r; }
    case Xf::Sha256: { std::wstring o; for (char ch : Sha256Hex(ToUtf8(s))) o += (wchar_t)ch; return o; }
    case Xf::UrlEncode: {
        std::string u = ToUtf8(s), o;
        for (unsigned char ch : u) {
            if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') o += (char)ch;
            else { char b[4]; snprintf(b, 4, "%%%02X", ch); o += b; }
        }
        return FromUtf8(o);
    }
    case Xf::UrlDecode: {
        std::string u = ToUtf8(s), o;
        for (size_t i = 0; i < u.size(); i++) {
            if (u[i] == '%' && i + 2 < u.size() + 0 && isxdigit((unsigned char)u[i + 1]) && isxdigit((unsigned char)u[i + 2])) { o += (char)strtol(u.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
            else if (u[i] == '+') o += ' ';
            else o += u[i];
        }
        return FromUtf8(o);
    }
    default: break;
    }
    return s;
}

// ---------------------------------------------------------------- detection
static bool NoSpace(const std::wstring& s) {
    for (auto c : s) if (IsSpace(c)) return false;
    return true;
}

static bool IsHex(wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'); }

static bool LooksJson(const std::wstring& t) {
    if (t.size() < 2) return false;
    wchar_t a = t.front(), b = t.back();
    if (!((a == L'{' && b == L'}') || (a == L'[' && b == L']'))) return false;
    int depth = 0;
    bool inStr = false, esc = false;
    std::vector<wchar_t> st;
    for (wchar_t c : t) {
        if (inStr) {
            if (esc) esc = false;
            else if (c == L'\\') esc = true;
            else if (c == L'"') inStr = false;
            continue;
        }
        if (c == L'"') inStr = true;
        else if (c == L'{' || c == L'[') { st.push_back(c); depth++; }
        else if (c == L'}' || c == L']') {
            if (st.empty()) return false;
            wchar_t o = st.back();
            st.pop_back();
            depth--;
            if ((o == L'{') != (c == L'}')) return false;
        }
    }
    if (inStr || depth != 0) return false;
    // must look like JSON, not code: needs a quote or a colon inside
    return t.find(L'"') != std::wstring::npos || t.find(L':') != std::wstring::npos || (a == L'[' && t.find_first_of(L"0123456789") != std::wstring::npos);
}

static bool LooksEmail(const std::wstring& t) {
    size_t at = t.find(L'@');
    if (at == std::wstring::npos || at == 0 || t.find(L'@', at + 1) != std::wstring::npos) return false;
    if (!NoSpace(t)) return false;
    std::wstring dom = t.substr(at + 1);
    size_t dot = dom.rfind(L'.');
    if (dot == std::wstring::npos || dot == 0 || dom.size() - dot - 1 < 2) return false;
    for (wchar_t c : t) {
        if (!(std::iswalnum(c) || wcschr(L"@._%+-'", c))) return false;
    }
    return true;
}

static bool LooksLink(const std::wstring& t) {
    if (!NoSpace(t)) return false;
    std::wstring l = Lower(t);
    if (StartsWith(l, L"http://") || StartsWith(l, L"https://") || StartsWith(l, L"ftp://") || StartsWith(l, L"file://")) return t.size() > 8;
    if (StartsWith(l, L"www.") && t.find(L'.', 4) != std::wstring::npos) return true;
    if (StartsWith(l, L"mailto:")) return t.size() > 8;
    return false;
}

static bool LooksColour(const std::wstring& t) {
    if (t.size() >= 4 && t[0] == L'#') {
        size_t n = t.size() - 1;
        if (n != 3 && n != 4 && n != 6 && n != 8) return false;
        for (size_t i = 1; i < t.size(); i++) if (!IsHex(t[i])) return false;
        return true;
    }
    std::wstring l = Lower(t);
    if ((StartsWith(l, L"rgb(") || StartsWith(l, L"rgba(") || StartsWith(l, L"hsl(") || StartsWith(l, L"hsla(")) && l.back() == L')') return true;
    return false;
}

static bool LooksPhone(const std::wstring& t) {
    int digits = 0;
    for (size_t i = 0; i < t.size(); i++) {
        wchar_t c = t[i];
        if (c >= L'0' && c <= L'9') digits++;
        else if (c == L'+' && i == 0) {}
        else if (c == L' ' || c == L'-' || c == L'(' || c == L')' || c == L'.') {}
        else return false;
    }
    return digits >= 7 && digits <= 15;
}

static bool LooksPath(const std::wstring& t) {
    if (t.find(L'\n') != std::wstring::npos) return false;
    if (t.size() >= 3 && std::iswalpha(t[0]) && t[1] == L':' && (t[2] == L'\\' || t[2] == L'/')) return true;
    if (t.size() >= 5 && t[0] == L'\\' && t[1] == L'\\' && t.find(L'\\', 2) != std::wstring::npos) return true;
    return false;
}

SmartType Detect(const std::wstring& text) {
    std::wstring t = Trim(text);
    if (t.empty() || t.size() > 100000) return ST_NONE;
    if (LooksJson(t)) return ST_JSON;
    if (t.find_first_of(L"\r\n") != std::wstring::npos) return ST_NONE;
    if (LooksLink(t)) return ST_LINK;
    if (LooksEmail(t)) return ST_EMAIL;
    if (LooksColour(t)) return ST_COLOUR;
    if (LooksPhone(t)) return ST_PHONE;
    if (LooksPath(t)) return ST_PATH;
    return ST_NONE;
}

// ---------------------------------------------------------------- sensitive
bool Luhn(const std::wstring& d) {
    if (d.size() < 13 || d.size() > 19) return false;
    int sum = 0;
    bool dbl = false;
    for (size_t i = d.size(); i-- > 0;) {
        if (d[i] < L'0' || d[i] > L'9') return false;
        int v = d[i] - L'0';
        if (dbl) { v *= 2; if (v > 9) v -= 9; }
        sum += v;
        dbl = !dbl;
    }
    return sum % 10 == 0;
}

static bool AllSame(const std::wstring& d) {
    for (auto c : d) if (c != d[0]) return false;
    return true;
}

static bool HasWord(const std::wstring& lower, const wchar_t* w) { return lower.find(w) != std::wstring::npos; }

static bool IsB64Url(wchar_t c) { return std::iswalnum(c) || c == L'-' || c == L'_'; }

bool Sensitive(const std::wstring& text, std::wstring* why) {
    auto set = [&](const wchar_t* w) { if (why) *why = w; return true; };
    std::wstring t = Trim(text);
    if (t.empty() || t.size() > 20000) return false;
    std::wstring l = Lower(t);

    if (HasWord(l, L"-----begin") && HasWord(l, L"private key")) return set(L"private key");

    // card number: only digits / spaces / dashes
    if (t.size() <= 25) {
        std::wstring digits;
        bool ok = true;
        for (wchar_t c : t) {
            if (c >= L'0' && c <= L'9') digits += c;
            else if (c == L' ' || c == L'-') {}
            else { ok = false; break; }
        }
        if (ok && Luhn(digits) && !AllSame(digits)) return set(L"card number");
    }

    // key = value style secrets
    static const wchar_t* keys[] = {L"password", L"passwd", L"pwd", L"secret", L"api_key", L"apikey", L"api-key", L"access_token", L"auth_token", L"private_key", L"client_secret"};
    for (auto k : keys) {
        size_t p = l.find(k);
        if (p == std::wstring::npos) continue;
        size_t q = p + wcslen(k);
        while (q < l.size() && (l[q] == L' ' || l[q] == L'"' || l[q] == L'\'')) q++;
        if (q < l.size() && (l[q] == L'=' || l[q] == L':')) {
            q++;
            while (q < l.size() && (l[q] == L' ' || l[q] == L'"' || l[q] == L'\'')) q++;
            size_t e = q;
            while (e < l.size() && !IsSpace(l[e]) && l[e] != L'"' && l[e] != L'\'') e++;
            if (e - q >= 6) return set(L"password or secret");
        }
    }

    if (t.find_first_of(L" \t\r\n") != std::wstring::npos) return false;

    // known token shapes (single token, no spaces)
    auto prefixTok = [&](const wchar_t* pre, size_t minLen) {
        return StartsWith(t, pre) && t.size() >= minLen;
    };
    if (prefixTok(L"sk-", 24) || prefixTok(L"ghp_", 30) || prefixTok(L"gho_", 30) || prefixTok(L"ghs_", 30) ||
        prefixTok(L"github_pat_", 40) || prefixTok(L"xoxb-", 20) || prefixTok(L"xoxp-", 20) || prefixTok(L"glpat-", 20) ||
        (prefixTok(L"AKIA", 20) && t.size() == 20) || prefixTok(L"AIza", 39))
        return set(L"API key");
    // JWT: three base64url parts, first starts with eyJ
    if (StartsWith(t, L"eyJ") && t.size() > 40) {
        int dots = 0;
        bool ok = true;
        for (wchar_t c : t) { if (c == L'.') dots++; else if (!IsB64Url(c)) { ok = false; break; } }
        if (ok && dots == 2) return set(L"token");
    }

    // password-like: 8-40 chars, upper+lower+digit+symbol, and not a link/email/path/colour
    if (t.size() >= 8 && t.size() <= 40) {
        bool up = false, lo = false, di = false, sy = false;
        for (wchar_t c : t) {
            if (c >= L'A' && c <= L'Z') up = true;
            else if (c >= L'a' && c <= L'z') lo = true;
            else if (c >= L'0' && c <= L'9') di = true;
            else if (c < 128 && std::iswprint(c)) sy = true;
            else if (c >= 128) { lo = true; }
        }
        if (up && lo && di && sy && Detect(t) == ST_NONE) return set(L"password");
    }
    return false;
}

// ---------------------------------------------------------------- hashing
uint64_t Fnv64(const void* data, size_t n) {
    const unsigned char* p = (const unsigned char*)data;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}
uint64_t HashText(const std::wstring& s) { std::string u = ToUtf8(s); return Fnv64(u.data(), u.size()); }

// ---------------------------------------------------------------- utf8
std::string ToUtf8(const std::wstring& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        uint32_t c = (uint32_t)s[i];
        if (sizeof(wchar_t) == 2 && c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
            uint32_t d = (uint32_t)s[i + 1];
            if (d >= 0xDC00 && d <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i++; }
        }
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    return o;
}

std::wstring FromUtf8(const std::string& s) {
    std::wstring o;
    size_t i = 0, n = s.size();
    if (n >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) i = 3;
    o.reserve(n);
    while (i < n) {
        unsigned char b = (unsigned char)s[i];
        uint32_t c;
        int extra;
        if (b < 0x80) { c = b; extra = 0; }
        else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; extra = 1; }
        else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; extra = 2; }
        else if ((b & 0xF8) == 0xF0) { c = b & 0x07; extra = 3; }
        else { o += (wchar_t)0xFFFD; i++; continue; }
        if (i + extra >= n) { o += (wchar_t)0xFFFD; break; }
        bool bad = false;
        for (int k = 1; k <= extra; k++) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0) != 0x80) { bad = true; break; }
            c = (c << 6) | (cc & 0x3F);
        }
        if (bad) { o += (wchar_t)0xFFFD; i++; continue; }
        i += extra + 1;
        if (sizeof(wchar_t) == 2 && c >= 0x10000) {
            c -= 0x10000;
            o += (wchar_t)(0xD800 + (c >> 10));
            o += (wchar_t)(0xDC00 + (c & 0x3FF));
        } else {
            o += (wchar_t)c;
        }
    }
    return o;
}

// ---------------------------------------------------------------- index fields
std::wstring EscapeField(const std::wstring& s) {
    std::wstring o;
    for (wchar_t c : s) {
        if (c == L'\\') o += L"\\\\";
        else if (c == L'\t') o += L"\\t";
        else if (c == L'\n') o += L"\\n";
        else if (c == L'\r') o += L"\\r";
        else o += c;
    }
    return o;
}

std::wstring UnescapeField(const std::wstring& s) {
    std::wstring o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\\' && i + 1 < s.size()) {
            wchar_t n = s[++i];
            if (n == L't') o += L'\t';
            else if (n == L'n') o += L'\n';
            else if (n == L'r') o += L'\r';
            else o += n;
        } else {
            o += s[i];
        }
    }
    return o;
}

std::vector<std::wstring> SplitTabs(const std::wstring& line) {
    std::vector<std::wstring> o;
    size_t a = 0;
    for (;;) {
        size_t b = line.find(L'\t', a);
        if (b == std::wstring::npos) { o.push_back(line.substr(a)); break; }
        o.push_back(line.substr(a, b - a));
        a = b + 1;
    }
    return o;
}

std::wstring HexId(uint64_t v) {
    static const wchar_t* d = L"0123456789abcdef";
    std::wstring s(16, L'0');
    for (int i = 15; i >= 0; i--) { s[i] = d[v & 15]; v >>= 4; }
    return s;
}

uint64_t ParseHex(const std::wstring& s) {
    uint64_t v = 0;
    for (wchar_t c : s) {
        int d;
        if (c >= L'0' && c <= L'9') d = c - L'0';
        else if (c >= L'a' && c <= L'f') d = c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') d = c - L'A' + 10;
        else break;
        v = (v << 4) | (uint64_t)d;
    }
    return v;
}

std::wstring OneLinePreview(const std::wstring& s, size_t maxChars) {
    std::wstring o;
    bool lastSpace = true;
    for (wchar_t c : s) {
        if (o.size() >= maxChars) break;
        if (c == L'\r' || c == L'\n' || c == L'\t' || c == L' ') {
            if (!lastSpace) { o += L' '; lastSpace = true; }
        } else {
            o += c;
            lastSpace = false;
        }
    }
    while (!o.empty() && o.back() == L' ') o.pop_back();
    return o;
}

// ---------------------------------------------------------------- SHA-256
std::string Sha256Hex(const std::string& data) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t H[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::string m = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    m += (char)0x80;
    while (m.size() % 64 != 56) m += (char)0;
    for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xFF);
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)(unsigned char)m[off + i * 4] << 24) | ((uint32_t)(unsigned char)m[off + i * 4 + 1] << 16) | ((uint32_t)(unsigned char)m[off + i * 4 + 2] << 8) | (uint32_t)(unsigned char)m[off + i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g);
            uint32_t t1 = h + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 8; i++)
        for (int s = 28; s >= 0; s -= 4) out += hex[(H[i] >> s) & 15];
    return out;
}

// ---------------------------------------------------------------- versions and latest.json
int CompareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& s) {
        std::vector<long> v;
        long cur = 0;
        bool any = false;
        for (char ch : s) {
            if (ch >= '0' && ch <= '9') { cur = cur * 10 + (ch - '0'); any = true; }
            else if (ch == '.') { v.push_back(cur); cur = 0; any = false; }
            else break;
        }
        if (any) v.push_back(cur);
        return v;
    };
    auto x = parts(a), y = parts(b);
    size_t n = std::max(x.size(), y.size());
    for (size_t i = 0; i < n; i++) {
        long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

namespace {
// A tiny reader for the version file: an object of strings, numbers and nested objects.
struct JReader {
    const std::string& s;
    size_t i = 0;
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i; }
    bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
    bool str(std::string& out) {
        ws();
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        out.clear();
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                if (e == 'n') out += '\n';
                else if (e == 't') out += '\t';
                else if (e == 'u') {
                    if (i + 4 > s.size()) return false;
                    unsigned v = (unsigned)strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                    i += 4;
                    if (v < 0x80) out += (char)v;
                    else if (v < 0x800) { out += (char)(0xC0 | (v >> 6)); out += (char)(0x80 | (v & 0x3F)); }
                    else { out += (char)(0xE0 | (v >> 12)); out += (char)(0x80 | ((v >> 6) & 0x3F)); out += (char)(0x80 | (v & 0x3F)); }
                } else out += e;
            } else out += c;
        }
        if (i >= s.size()) return false;
        ++i;
        return true;
    }
    bool value(const std::string& key, std::vector<std::pair<std::string, std::string>>& out) {
        ws();
        if (i >= s.size()) return false;
        if (s[i] == '"') { std::string v; if (!str(v)) return false; out.push_back({key, v}); return true; }
        if (s[i] == '{') return object(key + ".", out);
        size_t st = i;
        while (i < s.size() && (isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '.' || s[i] == '+')) ++i;
        if (i == st) return false;
        out.push_back({key, s.substr(st, i - st)});
        return true;
    }
    bool object(const std::string& prefix, std::vector<std::pair<std::string, std::string>>& out) {
        if (!eat('{')) return false;
        if (eat('}')) return true;
        do {
            std::string k;
            if (!str(k) || !eat(':') || !value(prefix + k, out)) return false;
        } while (eat(','));
        return eat('}');
    }
};
}  // namespace

bool ParseLatest(const std::string& json, Latest& out) {
    std::vector<std::pair<std::string, std::string>> kv;
    JReader r{json};
    if (!r.object("", kv)) return false;
    out = Latest();
    for (auto& p : kv) {
        if (p.first == "version") out.version = p.second;
        else if (p.first == "date") out.date = p.second;
        else if (p.first == "notes") out.notes = FromUtf8(p.second);
        else if (p.first == "windows.url") out.url = p.second;
        else if (p.first == "windows.sha256") out.sha256 = p.second;
        else if (p.first == "windows.size") out.size = atoll(p.second.c_str());
    }
    return !out.version.empty();
}

// ---------------------------------------------------------------- developer tools
static std::wstring Base64Encode(const std::wstring& s) {
    static const char* tb = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string u = ToUtf8(s), o;
    size_t i = 0;
    for (; i + 2 < u.size(); i += 3) {
        uint32_t v = ((unsigned char)u[i] << 16) | ((unsigned char)u[i + 1] << 8) | (unsigned char)u[i + 2];
        o += tb[v >> 18]; o += tb[(v >> 12) & 63]; o += tb[(v >> 6) & 63]; o += tb[v & 63];
    }
    if (i + 1 == u.size()) { uint32_t v = (unsigned char)u[i] << 16; o += tb[v >> 18]; o += tb[(v >> 12) & 63]; o += "=="; }
    else if (i + 2 == u.size()) { uint32_t v = ((unsigned char)u[i] << 16) | ((unsigned char)u[i + 1] << 8); o += tb[v >> 18]; o += tb[(v >> 12) & 63]; o += tb[(v >> 6) & 63]; o += '='; }
    return FromUtf8(o);
}

static bool Base64Decode(const std::wstring& s, std::wstring& out) {
    std::string in, o;
    for (wchar_t c : s) { if (c > 127) return false; if (!IsSpace(c)) in += (char)c; }
    if (in.empty() || in.size() % 4 != 0) return false;
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    for (size_t i = 0; i < in.size(); i += 4) {
        int a = val(in[i]), b = val(in[i + 1]);
        int c = in[i + 2] == '=' ? -2 : val(in[i + 2]), d = in[i + 3] == '=' ? -2 : val(in[i + 3]);
        if (a < 0 || b < 0 || c == -1 || d == -1) return false;
        o += (char)((a << 2) | (b >> 4));
        if (c >= 0) o += (char)(((b & 15) << 4) | (c >> 2));
        if (d >= 0 && c >= 0) o += (char)(((c & 3) << 6) | d);
    }
    out = FromUtf8(o);
    return true;
}

static std::wstring JsonFormat(const std::wstring& s, bool minify) {
    std::wstring t = Trim(s);
    if (!(t.size() >= 2 && ((t.front() == L'{' && t.back() == L'}') || (t.front() == L'[' && t.back() == L']')))) return s;
    std::wstring o;
    int depth = 0;
    bool inStr = false, esc = false;
    std::vector<wchar_t> st;
    auto nl = [&]() { if (!minify) { o += L"\r\n"; o.append((size_t)depth * 2, L' '); } };
    for (size_t i = 0; i < t.size(); i++) {
        wchar_t c = t[i];
        if (inStr) {
            o += c;
            if (esc) esc = false; else if (c == L'\\') esc = true; else if (c == L'"') inStr = false;
            continue;
        }
        if (IsSpace(c)) continue;
        if (c == L'"') { inStr = true; o += c; }
        else if (c == L'{' || c == L'[') {
            st.push_back(c);
            o += c;
            size_t j = i + 1;
            while (j < t.size() && IsSpace(t[j])) j++;
            if (j < t.size() && (t[j] == L'}' || t[j] == L']')) { o += t[j]; st.pop_back(); i = j; }
            else { depth++; nl(); }
        } else if (c == L'}' || c == L']') {
            if (st.empty() || ((st.back() == L'{') != (c == L'}'))) return s;
            st.pop_back();
            depth--;
            nl();
            o += c;
        } else if (c == L',') { o += c; nl(); }
        else if (c == L':') { o += c; if (!minify) o += L' '; }
        else o += c;
    }
    if (inStr || !st.empty()) return s;
    return o;
}

static std::wstring XmlFormat(const std::wstring& s) {
    std::wstring t = Trim(s);
    if (t.empty() || t[0] != L'<') return s;
    // tokenise into tags and text; entities are never resolved (so external entities cannot be fetched)
    std::vector<std::wstring> toks;
    size_t i = 0;
    while (i < t.size()) {
        if (t[i] == L'<') {
            size_t e;
            if (t.compare(i, 4, L"<!--") == 0) { e = t.find(L"-->", i); if (e == std::wstring::npos) return s; e += 3; }
            else if (t.compare(i, 9, L"<![CDATA[") == 0) { e = t.find(L"]]>", i); if (e == std::wstring::npos) return s; e += 3; }
            else { e = t.find(L'>', i); if (e == std::wstring::npos) return s; e++; }
            toks.push_back(t.substr(i, e - i));
            i = e;
        } else {
            size_t e = t.find(L'<', i);
            if (e == std::wstring::npos) e = t.size();
            std::wstring tx = Trim(t.substr(i, e - i));
            if (!tx.empty()) toks.push_back(tx);
            i = e;
        }
    }
    std::wstring o;
    int depth = 0;
    std::vector<std::wstring> names;
    for (size_t k = 0; k < toks.size(); k++) {
        const std::wstring& tk = toks[k];
        auto nameOf = [](const std::wstring& x) { size_t a = (x.size() > 1 && x[1] == L'/') ? 2 : 1; size_t b = a; while (b < x.size() && !IsSpace(x[b]) && x[b] != L'>' && x[b] != L'/') b++; return x.substr(a, b - a); };
        if (tk[0] != L'<') { o.append((size_t)depth * 2, L' '); o += tk + L"\r\n"; continue; }
        if (tk.compare(0, 2, L"</") == 0) {
            if (names.empty() || names.back() != nameOf(tk)) return s;       // not well formed
            names.pop_back();
            depth--;
            o.append((size_t)std::max(depth, 0) * 2, L' ');
            o += tk + L"\r\n";
        } else if (tk.compare(0, 2, L"<?") == 0 || tk.compare(0, 2, L"<!") == 0 || (tk.size() > 1 && tk[tk.size() - 2] == L'/')) {
            o.append((size_t)depth * 2, L' ');
            o += tk + L"\r\n";
        } else {
            // <a>text</a> stays on one line
            if (k + 2 < toks.size() + 0 && toks[k + 1][0] != L'<' && toks[k + 2].compare(0, 2, L"</") == 0 && nameOf(toks[k + 2]) == nameOf(tk)) {
                o.append((size_t)depth * 2, L' ');
                o += tk + toks[k + 1] + toks[k + 2] + L"\r\n";
                k += 2;
                continue;
            }
            o.append((size_t)depth * 2, L' ');
            o += tk + L"\r\n";
            names.push_back(nameOf(tk));
            depth++;
        }
    }
    if (!names.empty()) return s;
    while (o.size() >= 2 && o.compare(o.size() - 2, 2, L"\r\n") == 0) o.resize(o.size() - 2);
    return o;
}

static std::wstring SqlFormat(const std::wstring& s) {
    std::wstring t = OneLinePreview(s, s.size() + 1);
    static const wchar_t* kw[] = {L"SELECT", L"FROM", L"WHERE", L"GROUP BY", L"ORDER BY", L"HAVING", L"LEFT JOIN", L"RIGHT JOIN", L"INNER JOIN", L"OUTER JOIN", L"JOIN", L"UNION", L"LIMIT", L"INSERT INTO", L"VALUES", L"UPDATE", L"SET", L"DELETE FROM"};
    std::wstring o;
    std::wstring up = t;
    for (auto& c : up) c = (wchar_t)std::towupper(c);
    bool inStr = false;
    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == L'\'') inStr = !inStr;
        if (!inStr && (i == 0 || t[i - 1] == L' ' || t[i - 1] == L'(')) {
            bool matched = false;
            for (auto k : kw) {
                size_t n = wcslen(k);
                if (up.compare(i, n, k) == 0 && (i + n >= up.size() || up[i + n] == L' ' || up[i + n] == L'(')) {
                    if (!o.empty() && o.back() == L' ') o.pop_back();
                    if (!o.empty()) o += L"\r\n";
                    o += k;
                    i += n - 1;
                    matched = true;
                    break;
                }
            }
            if (matched) continue;
        }
        o += t[i];
    }
    return o;
}

const wchar_t* XfKey(Xf x) {
    static const wchar_t* k[] = {L"upper", L"lower", L"title", L"sentence", L"trim", L"oneline", L"sort", L"unique", L"reverse", L"number", L"plain",
                                 L"blanklines", L"b64", L"unb64", L"json", L"jsonmin", L"xml", L"sql", L"slashes", L"backslashes", L"sha256", L"urlencode", L"urldecode"};
    return (size_t)x < sizeof k / sizeof *k ? k[(size_t)x] : L"";
}

bool XfFromKey(const std::wstring& key, Xf& out) {
    std::wstring k = Lower(Trim(key));
    for (int i = 0; i < (int)Xf::COUNT; i++) if (k == XfKey((Xf)i)) { out = (Xf)i; return true; }
    return false;
}

std::wstring FindReplace(const std::wstring& s, const std::wstring& find, const std::wstring& repl, bool cs) {
    if (find.empty()) return s;
    std::wstring hay = cs ? s : Lower(s), nd = cs ? find : Lower(find), o;
    size_t pos = 0;
    for (;;) {
        size_t p = hay.find(nd, pos);
        if (p == std::wstring::npos) { o += s.substr(pos); break; }
        o += s.substr(pos, p - pos) + repl;
        pos = p + nd.size();
    }
    return o;
}

bool RunPipeline(const std::wstring& spec, const std::wstring& text, std::wstring& out, std::wstring& err) {
    std::wstring cur = text;
    size_t a = 0;
    while (a <= spec.size()) {
        size_t b = spec.find(L'|', a);
        if (b == std::wstring::npos) b = spec.size();
        std::wstring op = Trim(spec.substr(a, b - a));
        a = b + 1;
        if (op.empty()) { if (b >= spec.size()) break; continue; }
        if (StartsWith(Lower(op), L"find:")) {
            std::wstring body = op.substr(5);
            size_t arrow = body.find(L"=>");
            if (arrow == std::wstring::npos) { err = L"find needs find:old=>new"; return false; }
            cur = FindReplace(cur, body.substr(0, arrow), body.substr(arrow + 2), false);
            continue;
        }
        Xf x;
        if (!XfFromKey(op, x)) { err = L"unknown step: " + op; return false; }
        cur = Apply(x, cur);
        if (b >= spec.size()) break;
    }
    out = cur;
    return true;
}

std::wstring ExpandTemplate(const std::wstring& text, int y, int mo, int d, int hh, int mm, int ss, int wday) {
    static const wchar_t* days[] = {L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday"};
    static const wchar_t* mons[] = {L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August", L"September", L"October", L"November", L"December"};
    auto two = [](int v) { std::wstring s = std::to_wstring(v); return s.size() < 2 ? L"0" + s : s; };
    std::wstring date = two(d) + L"/" + two(mo) + L"/" + std::to_wstring(y), time = two(hh) + L":" + two(mm);
    std::wstring o = text;
    auto rep = [&](const wchar_t* k, const std::wstring& v) { o = FindReplace(o, k, v, true); };
    rep(L"{datetime}", date + L" " + time);
    rep(L"{date}", date);
    rep(L"{time}", time);
    rep(L"{iso}", std::to_wstring(y) + L"-" + two(mo) + L"-" + two(d));
    rep(L"{year}", std::to_wstring(y));
    rep(L"{weekday}", days[((wday % 7) + 7) % 7]);
    rep(L"{month}", mons[((mo - 1) % 12 + 12) % 12]);
    (void)ss;
    return o;
}

std::wstring LineDiff(const std::wstring& a, const std::wstring& b) {
    auto x = SplitLines(a), y = SplitLines(b);
    size_t n = x.size(), m = y.size();
    if (n * m > 4000000) return L"(the two texts are too long to compare line by line)";
    std::vector<std::vector<int>> L(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = n; i-- > 0;)
        for (size_t j = m; j-- > 0;) L[i][j] = x[i] == y[j] ? L[i + 1][j + 1] + 1 : std::max(L[i + 1][j], L[i][j + 1]);
    std::wstring o;
    size_t i = 0, j = 0;
    while (i < n && j < m) {
        if (x[i] == y[j]) { o += L"  " + x[i] + L"\r\n"; i++; j++; }
        else if (L[i + 1][j] >= L[i][j + 1]) { o += L"- " + x[i] + L"\r\n"; i++; }
        else { o += L"+ " + y[j] + L"\r\n"; j++; }
    }
    while (i < n) o += L"- " + x[i++] + L"\r\n";
    while (j < m) o += L"+ " + y[j++] + L"\r\n";
    return o;
}

std::wstring InspectUnicode(const std::wstring& s, size_t maxChars) {
    std::wstring o;
    size_t shown = 0;
    for (size_t i = 0; i < s.size() && shown < maxChars; i++, shown++) {
        uint32_t cp = (uint32_t)s[i];
        std::wstring ch(1, s[i]);
        if (sizeof(wchar_t) == 2 && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) { cp = 0x10000 + ((cp - 0xD800) << 10) + ((uint32_t)s[i + 1] - 0xDC00); ch += s[i + 1]; i++; }
        wchar_t hex[16];
        swprintf(hex, 16, L"U+%04X", cp);
        std::wstring shownCh = cp < 32 ? (cp == 9 ? L"<tab>" : cp == 10 ? L"<LF>" : cp == 13 ? L"<CR>" : L"<control>") : (cp == 32 ? L"<space>" : cp == 0xA0 ? L"<no-break space>" : cp == 0x200B ? L"<zero-width space>" : ch);
        o += std::wstring(hex) + L"  " + shownCh + L"\r\n";
    }
    if (shown < s.size()) o += L"\u2026\r\n";
    return o;
}

std::wstring FormatTable(const std::wstring& text) {
    auto lines = SplitLines(text);
    if (lines.size() < 2) return L"";
    size_t tabs = 0, commas = 0;
    for (auto& l : lines) { tabs += (size_t)std::count(l.begin(), l.end(), L'\t'); commas += (size_t)std::count(l.begin(), l.end(), L','); }
    wchar_t delim = tabs >= lines.size() - 1 && tabs > 0 ? L'\t' : (commas >= lines.size() - 1 && commas > 0 ? L',' : 0);
    if (!delim) return L"";
    std::vector<std::vector<std::wstring>> rows;
    size_t cols = 0;
    for (auto& l : lines) {
        std::vector<std::wstring> r;
        std::wstring cur;
        bool q = false;
        for (wchar_t c : l) {
            if (c == L'"') q = !q;
            else if (c == delim && !q) { r.push_back(cur); cur.clear(); }
            else cur += c;
        }
        r.push_back(cur);
        cols = std::max(cols, r.size());
        rows.push_back(r);
    }
    std::vector<size_t> w(cols, 1);
    for (auto& r : rows) for (size_t c = 0; c < r.size(); c++) w[c] = std::max(w[c], std::min<size_t>(r[c].size(), 40));
    std::wstring o;
    for (size_t ri = 0; ri < rows.size(); ri++) {
        for (size_t c = 0; c < cols; c++) {
            std::wstring cell = c < rows[ri].size() ? rows[ri][c].substr(0, 40) : L"";
            o += cell + std::wstring(w[c] - cell.size(), L' ') + (c + 1 < cols ? L" \u2502 " : L"");
        }
        o += L"\r\n";
        if (ri == 0) { for (size_t c = 0; c < cols; c++) { o += std::wstring(w[c], L'\u2500') + (c + 1 < cols ? L"\u2500\u253C\u2500" : L""); } o += L"\r\n"; }
    }
    return o;
}

std::wstring MaskSensitive(const std::wstring& s) {
    std::wstring o;
    size_t i = 0;
    auto isTokChar = [](wchar_t c) { return std::iswalnum(c) || c == L'.' || c == L'_' || c == L'-' || c == L'@' || c == L'+'; };
    while (i < s.size()) {
        if (!isTokChar(s[i])) { o += s[i++]; continue; }
        size_t j = i;
        while (j < s.size() && isTokChar(s[j])) j++;
        std::wstring w = s.substr(i, j - i);
        int digits = 0;
        for (wchar_t c : w) if (c >= L'0' && c <= L'9') digits++;
        bool mask = Detect(w) == ST_EMAIL || digits >= 9 || Sensitive(w);
        if (mask) {
            if (Detect(w) == ST_EMAIL) { size_t at = w.find(L'@'); o += w.substr(0, 1) + L"\u2022\u2022\u2022" + w.substr(at); }
            else o += w.substr(0, std::min<size_t>(2, w.size())) + L"\u2022\u2022\u2022\u2022";
        } else o += w;
        i = j;
    }
    return o;
}

// ---------------------------------------------------------------- search queries
Query ParseQuery(const std::wstring& text, long long now, long long startOfToday) {
    Query q;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == L' ') i++;
        if (i >= text.size()) break;
        if (text[i] == L'"') {
            size_t e = text.find(L'"', i + 1);
            std::wstring p = text.substr(i + 1, (e == std::wstring::npos ? text.size() : e) - i - 1);
            if (!p.empty()) q.phrases.push_back(Lower(p));
            i = e == std::wstring::npos ? text.size() : e + 1;
            continue;
        }
        size_t e = i;
        while (e < text.size() && text[e] != L' ') e++;
        std::wstring w = text.substr(i, e - i);
        i = e;
        std::wstring lw = Lower(w);
        if (StartsWith(lw, L"re:") && w.size() > 3) { q.regex = text.substr(e - (w.size() - 3), w.size() - 3); if (e < text.size()) { q.regex += text.substr(e); i = text.size(); } continue; }
        if (StartsWith(lw, L"app:")) { q.app = lw.substr(4); continue; }
        if (StartsWith(lw, L"type:")) { q.type = lw.substr(5); continue; }
        if (StartsWith(lw, L"tag:") || StartsWith(lw, L"group:")) { q.tag = lw.substr(lw.find(L':') + 1); continue; }
        if (StartsWith(lw, L"date:")) {
            std::wstring d = lw.substr(5);
            if (d == L"today") q.fromTime = startOfToday;
            else if (d == L"yesterday") { q.fromTime = startOfToday - 86400; q.toTime = startOfToday; }
            else if (d.size() >= 2 && d.back() == L'd' && std::iswdigit(d[0])) q.fromTime = now - (long long)WInt(d.c_str()) * 86400;
            else if (d.size() >= 2 && d.back() == L'h' && std::iswdigit(d[0])) q.fromTime = now - (long long)WInt(d.c_str()) * 3600;
            else if (d.size() == 10 && d[4] == L'-' && d[7] == L'-') {
                // yyyy-mm-dd (local midnight is not known here: the day is taken as 86400 s from startOfToday's offset)
                int y = WInt(d.substr(0, 4).c_str()), m = WInt(d.substr(5, 2).c_str()), dd = WInt(d.substr(8, 2).c_str());
                // days since 1970-01-01 (civil algorithm), then shift to local midnight using startOfToday's offset from UTC midnight
                y -= m <= 2;
                long era = (y >= 0 ? y : y - 399) / 400;
                unsigned yoe = (unsigned)(y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + dd - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
                long long days = era * 146097 + (long long)doe - 719468;
                long long offset = ((startOfToday % 86400) + 86400) % 86400;   // time of day (UTC) at which local midnight falls
                if (offset > 43200) offset -= 86400;
                long long from = days * 86400 + offset;
                q.fromTime = from;
                q.toTime = from + 86400;
            }
            continue;
        }
        if (w[0] == L'~' && w.size() > 2) { q.fuzzy.push_back(lw.substr(1)); continue; }
        q.words.push_back(lw);
    }
    return q;
}

bool FuzzyContains(const std::wstring& hay, const std::wstring& nd) {
    if (nd.empty()) return true;
    if (hay.find(nd) != std::wstring::npos) return true;
    if (nd.size() < 3) return false;
    size_t n = nd.size();
    // sliding windows of length n-1 .. n+1, edit distance <= 1
    for (size_t len = n - 1; len <= n + 1; len++) {
        if (len == 0 || hay.size() < len) continue;
        for (size_t i = 0; i + len <= hay.size(); i++) {
            // quick prefilter: first or last letter must match
            if (hay[i] != nd[0] && hay[i + len - 1] != nd[n - 1]) continue;
            // at most one of the first three letters may differ position-wise unless a letter was dropped: cheap reject
            if (len == n && hay[i] != nd[0] && hay[i + 1] != nd[1] && hay[i + 2 < hay.size() ? i + 2 : i + 1] != nd[2 < n ? 2 : 1]) continue;
            // Levenshtein with early exit
            std::vector<int> prev(len + 1), cur(len + 1);
            for (size_t b = 0; b <= len; b++) prev[b] = (int)b;
            bool ok = true;
            for (size_t a = 1; a <= n; a++) {
                cur[0] = (int)a;
                int best = cur[0];
                for (size_t b = 1; b <= len; b++) {
                    int cost = nd[a - 1] == hay[i + b - 1] ? 0 : 1;
                    cur[b] = std::min(std::min(prev[b] + 1, cur[b - 1] + 1), prev[b - 1] + cost);
                    best = std::min(best, cur[b]);
                }
                if (best > 1) { ok = false; break; }
                std::swap(prev, cur);
            }
            if (ok && prev[len] <= 1) return true;
        }
    }
    return false;
}

bool RegexSearch(const std::wstring& pattern, const std::wstring& text) {
    if (pattern.empty() || pattern.size() > 200) return false;
    try {
        std::wregex re(pattern, std::regex::icase | std::regex::ECMAScript);
        // bounded: only the first 20,000 characters are scanned
        return std::regex_search(text.substr(0, 20000), re);
    } catch (...) { return false; }
}

}  // namespace core
