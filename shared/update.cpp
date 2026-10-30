#include "update.hpp"
#include "crypto.hpp"
#include "platform.hpp"
#include <cctype>
#include <cstdlib>

namespace update {

const char* const kLatestUrl = "https://raw.githubusercontent.com/akshsim1991/FeatherPDF-releases/main/latest.json";

namespace {

// A tiny reader for the version file: an object of strings, numbers and objects (no arrays are needed).
struct Reader {
    const std::string& s;
    size_t i = 0;
    void ws() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; }
    bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }
    bool string(std::string& out) {
        ws();
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        out.clear();
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                const char e = s[i++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'u': {   // \uXXXX (no surrogate pairs needed here)
                        if (i + 4 > s.size()) return false;
                        const unsigned v = static_cast<unsigned>(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
                        i += 4;
                        out += platform::utf8(std::wstring(1, static_cast<wchar_t>(v)));
                        break;
                    }
                    default: out += e;
                }
            } else out += c;
        }
        if (i >= s.size()) return false;
        ++i;
        return true;
    }
    // A value: strings and numbers are kept as text under `key`, objects are read into `prefix.key.`.
    bool value(const std::string& key, std::vector<std::pair<std::string, std::string>>& out) {
        ws();
        if (i >= s.size()) return false;
        if (s[i] == '"') { std::string v; if (!string(v)) return false; out.push_back({key, v}); return true; }
        if (s[i] == '{') return object(key + ".", out);
        const size_t start = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '.' || s[i] == '+')) ++i;
        if (i == start) return false;
        out.push_back({key, s.substr(start, i - start)});
        return true;
    }
    bool object(const std::string& prefix, std::vector<std::pair<std::string, std::string>>& out) {
        if (!eat('{')) return false;
        if (eat('}')) return true;
        do {
            std::string k;
            if (!string(k) || !eat(':') || !value(prefix + k, out)) return false;
        } while (eat(','));
        return eat('}');
    }
};

} // namespace

bool parse(const std::string& json, Release& out) {
    std::vector<std::pair<std::string, std::string>> kv;
    Reader r{json};
    if (!r.object("", kv)) return false;
    out = Release{};
    for (const auto& [k, v] : kv) {
        if (k == "version") out.version = v;
        else if (k == "date") out.date = v;
        else if (k == "notes") out.notes = platform::wide(v);
        else if (k == "windows.url") out.windows.url = v;
        else if (k == "windows.sha256") out.windows.sha256 = v;
        else if (k == "windows.size") out.windows.size = std::atoll(v.c_str());
        else if (k == "mac.url") out.mac.url = v;
        else if (k == "mac.sha256") out.mac.sha256 = v;
        else if (k == "mac.size") out.mac.size = std::atoll(v.c_str());
    }
    return !out.version.empty();
}

int compareVersions(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        long x = 0, y = 0;
        while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i]))) x = x * 10 + (a[i++] - '0');
        while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j]))) y = y * 10 + (b[j++] - '0');
        if (x != y) return x < y ? -1 : 1;
        while (i < a.size() && !std::isdigit(static_cast<unsigned char>(a[i]))) ++i;
        while (j < b.size() && !std::isdigit(static_cast<unsigned char>(b[j]))) ++j;
    }
    return 0;
}

bool verify(const std::vector<unsigned char>& bytes, const std::string& sha256) {
    crypto::Bytes h;
    if (sha256.size() != 64 || !crypto::hash(crypto::Hash::Sha256, bytes, h)) return false;
    static const char* hex = "0123456789abcdef";
    std::string got;
    for (unsigned char c : h) { got += hex[c >> 4]; got += hex[c & 15]; }
    std::string want = sha256;
    for (auto& c : want) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return got == want;
}

} // namespace update
