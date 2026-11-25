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
    // Each download may carry its own version and notes (a release for one kind of device leaves the others' as they
    // were, so they are not offered an update that changes nothing): this app's own, when given, are the ones compared.
#if defined(_WIN32)
    const std::string mine = "windows.";
#elif defined(__APPLE__)
    const std::string mine = "mac.";
#else
    const std::string mine = "linux.";
#endif
    std::string ownVersion, ownNotes;
    for (const auto& [k, v] : kv) {
        if (k == mine + "version") ownVersion = v;
        else if (k == mine + "notes") ownNotes = v;
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
    if (!ownVersion.empty()) { out.version = ownVersion; if (!ownNotes.empty()) out.notes = platform::wide(ownNotes); }
    return !out.version.empty();
}

std::vector<Source> sources(const std::string& url) {
    std::vector<Source> out{{url, "", "raw.githubusercontent.com"}};
    const std::string prefix = "https://raw.githubusercontent.com/";
    if (url.compare(0, prefix.size(), prefix) != 0) { out[0].host = url; return out; }
    // owner/repo/branch/path
    std::string rest = url.substr(prefix.size());
    std::string part[3];
    for (auto& p : part) {
        const size_t slash = rest.find('/');
        if (slash == std::string::npos) return out;
        p = rest.substr(0, slash);
        rest = rest.substr(slash + 1);
    }
    if (rest.empty()) return out;
    // jsDelivr before GitHub's API: the API allows 60 requests an hour per address, which many people share behind one
    // provider address. jsDelivr may be up to about 12 hours behind; installers are named by version and checked by SHA-256.
    out.push_back({"https://cdn.jsdelivr.net/gh/" + part[0] + "/" + part[1] + "@" + part[2] + "/" + rest, "", "cdn.jsdelivr.net"});
    out.push_back({"https://api.github.com/repos/" + part[0] + "/" + part[1] + "/contents/" + rest + "?ref=" + part[2],
                   "application/vnd.github.raw", "api.github.com"});
    // Where raw.githubusercontent.com is blocked and GitHub's API has run out of requests (many people behind one address),
    // only jsDelivr answers, and its copy of the branch may be hours old ("you have the newest version" though a newer one
    // is out). Asking it which commit the branch is at, then for the file at that commit, is always fresh.
    // GitHub's own feed of the branch's commits (github.com: reachable where raw.githubusercontent.com is blocked, and not
    // counted against the API's limit) names the newest commit; the file at that commit comes from jsDelivr, unchanged
    // forever once published. jsDelivr's own answer to which commit is newest is asked too.
    out.push_back({"https://github.com/" + part[0] + "/" + part[1] + "/commits/" + part[2] + ".atom", "", "github.com",
                   part[0] + "/" + part[1] + "|" + rest});
    out.push_back({"https://data.jsdelivr.com/v1/packages/gh/" + part[0] + "/" + part[1] + "/resolved?specifier=" + part[2], "",
                   "data.jsdelivr.com", part[0] + "/" + part[1] + "|" + rest});
    return out;
}

std::string pinnedUrl(const Source& s, const std::string& resolved) {
    const size_t bar = s.pinnedPath.find('|');
    if (bar == std::string::npos) return "";
    std::string sha;
    if (const size_t c = resolved.find("Commit/"); c != std::string::npos) {   // the commit feed: its first (newest) entry
        size_t e = c + 7;
        while (e < resolved.size() && std::isxdigit(static_cast<unsigned char>(resolved[e]))) ++e;
        sha = resolved.substr(c + 7, e - c - 7);
    } else {   // jsDelivr: {"version": "<commit>"}
        const size_t k = resolved.find("\"version\"");
        if (k == std::string::npos) return "";
        const size_t q1 = resolved.find('"', resolved.find(':', k) + 1);
        const size_t q2 = q1 == std::string::npos ? q1 : resolved.find('"', q1 + 1);
        if (q2 == std::string::npos) return "";
        sha = resolved.substr(q1 + 1, q2 - q1 - 1);
    }
    if (sha.size() < 7 || sha.find_first_not_of("0123456789abcdef") != std::string::npos) return "";
    return "https://cdn.jsdelivr.net/gh/" + s.pinnedPath.substr(0, bar) + "@" + sha + "/" + s.pinnedPath.substr(bar + 1);
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
