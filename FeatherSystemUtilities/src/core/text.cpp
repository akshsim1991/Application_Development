#include "text.h"
#include <algorithm>
#include <cwctype>
#include <set>
#include <filesystem>
#include <fstream>

namespace fsu {

std::wstring Lower(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = (wchar_t)std::towlower(c);
    return r;
}

std::wstring FormatBytes(unsigned long long b) {
    const wchar_t* u[] = {L"B", L"KB", L"MB", L"GB", L"TB"}; double v = (double)b; int i = 0; while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    wchar_t buf[48]; if (i == 0) swprintf(buf, 48, L"%llu B", b); else swprintf(buf, 48, v >= 100 ? L"%.0f %ls" : L"%.1f %ls", v, u[i]); return buf;
}
bool IsDriveAbsolute(const std::wstring& p) { return p.size() >= 3 && ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')) && p[1] == L':' && (p[2] == L'\\' || p[2] == L'/') && p.find(L'%') == std::wstring::npos; }
bool MuiCachePath(const std::wstring& n, std::wstring& path) {
    std::wstring l = Lower(n); size_t best = std::wstring::npos;
    for (const wchar_t* ext : {L".exe.", L".dll.", L".msc.", L".cpl."}) { size_t p = l.rfind(ext); if (p != std::wstring::npos && (best == std::wstring::npos || p > best)) best = p + 4; }
    if (best == std::wstring::npos) return false; path = n.substr(0, best); return IsDriveAbsolute(path);
}

std::vector<size_t> SelectDuplicatesToRemove(const std::vector<DupRec>& r, int mode) {
    std::map<int, size_t> keep;
    for (size_t i = 0; i < r.size(); ++i) {
        auto it = keep.find(r[i].group); if (it == keep.end()) { keep[r[i].group] = i; continue; }
        const DupRec& k = r[it->second]; const DupRec& c = r[i]; bool better = mode == 0 ? c.mtime < k.mtime : mode == 1 ? c.mtime > k.mtime : c.pathLen < k.pathLen;
        if (better) it->second = i;
    }
    std::vector<size_t> out; for (size_t i = 0; i < r.size(); ++i) if (keep[r[i].group] != i) out.push_back(i); return out;
}

std::string ToUtf8(const std::wstring& w) {
    std::string o;
    for (size_t i = 0; i < w.size(); ++i) {
        unsigned long c = (unsigned long)w[i];
#ifdef _WIN32  // wchar_t is UTF-16: combine surrogates
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < w.size() && (unsigned long)w[i+1] >= 0xDC00 && (unsigned long)w[i+1] < 0xE000)
            c = 0x10000 + ((c - 0xD800) << 10) + ((unsigned long)w[++i] - 0xDC00);
#endif
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    return o;
}

std::wstring FromUtf8(const std::string& s) {
    std::wstring o;
    for (size_t i = 0; i < s.size();) {
        unsigned char b = (unsigned char)s[i];
        unsigned long c; int n;
        if (b < 0x80) { c = b; n = 1; }
        else if ((b >> 5) == 6) { c = b & 0x1F; n = 2; }
        else if ((b >> 4) == 14) { c = b & 0x0F; n = 3; }
        else if ((b >> 3) == 30) { c = b & 0x07; n = 4; }
        else { o += L'\xFFFD'; ++i; continue; }
        if (i + n > s.size()) { o += L'\xFFFD'; break; }
        for (int k = 1; k < n; ++k) c = (c << 6) | ((unsigned char)s[i + k] & 0x3F);
        i += n;
#ifdef _WIN32
        if (c >= 0x10000) { c -= 0x10000; o += (wchar_t)(0xD800 + (c >> 10)); o += (wchar_t)(0xDC00 + (c & 0x3FF)); }
        else o += (wchar_t)c;
#else
        o += (wchar_t)c;
#endif
    }
    return o;
}

static bool HasExecExt(const std::wstring& p) {
    static const wchar_t* ext[] = {L".exe", L".bat", L".cmd", L".com", L".vbs", L".js", L".ps1", L".lnk", L".msc", L".cpl"};
    std::wstring l = Lower(p);
    for (auto e : ext) { std::wstring s = e; if (l.size() >= s.size() && l.compare(l.size() - s.size(), s.size(), s) == 0) return true; }
    return false;
}

ParsedCommand ParseCommand(const std::wstring& rawIn, const ExistsFn& exists) {
    ParsedCommand r;
    size_t b = rawIn.find_first_not_of(L" \t");
    if (b == std::wstring::npos) return r;
    std::wstring raw = rawIn.substr(b);
    if (raw[0] == L'"') {
        size_t e = raw.find(L'"', 1);
        if (e == std::wstring::npos) { r.exe = raw.substr(1); r.quoted = true; }
        else { r.exe = raw.substr(1, e - 1); r.args = raw.substr(e + 1); r.quoted = true; }
        size_t a = r.args.find_first_not_of(L" \t");
        r.args = a == std::wstring::npos ? L"" : r.args.substr(a);
    } else {
        // Unquoted: Windows tries progressively longer prefixes at each space.
        std::vector<size_t> spaces;
        for (size_t i = 0; i < raw.size(); ++i) if (raw[i] == L' ') spaces.push_back(i);
        spaces.push_back(raw.size());
        size_t chosen = spaces.front();
        bool found = false;
        if (exists) {
            for (size_t s : spaces) {
                std::wstring cand = raw.substr(0, s);
                if (exists(cand) || (!HasExecExt(cand) && exists(cand + L".exe"))) { chosen = s; found = true; break; }
            }
        }
        if (!found) {  // no probe / nothing exists: prefer first prefix ending in a known extension
            for (size_t s : spaces) if (HasExecExt(raw.substr(0, s))) { chosen = s; break; }
        }
        r.exe = raw.substr(0, chosen);
        size_t a = chosen < raw.size() ? raw.find_first_not_of(L" \t", chosen) : std::wstring::npos;
        r.args = a == std::wstring::npos ? L"" : raw.substr(a);
    }
    if (exists) r.exeExists = exists(r.exe) || (!HasExecExt(r.exe) && exists(r.exe + L".exe"));
    return r;
}

std::wstring CsvField(const std::wstring& v, bool neut) {
    std::wstring s = v;
    if (neut && !s.empty() && (s[0] == L'=' || s[0] == L'+' || s[0] == L'-' || s[0] == L'@' || s[0] == L'\t' || s[0] == L'\r'))
        s.insert(s.begin(), L'\'');
    bool q = s.find_first_of(L",\"\r\n") != std::wstring::npos;
    if (!q) return s;
    std::wstring o = L"\"";
    for (wchar_t c : s) { if (c == L'"') o += L'"'; o += c; }
    return o + L"\"";
}

std::wstring CsvRow(const std::vector<std::wstring>& f, bool neut) {
    std::wstring o;
    for (size_t i = 0; i < f.size(); ++i) { if (i) o += L','; o += CsvField(f[i], neut); }
    return o;
}

bool MatchesQuery(const std::vector<std::wstring>& fields, const std::wstring& query) {
    if (query.empty()) return true;
    std::wstring q = Lower(query);
    for (auto& f : fields) if (Lower(f).find(q) != std::wstring::npos) return true;
    return false;
}

using DepMap = std::map<std::wstring, std::vector<std::wstring>>;

static OrderResult Topo(const std::vector<std::wstring>& targets, const DepMap& deps, bool prereqFirst) {
    OrderResult r;
    // Build the closure over targets + (for start) prerequisites / (for stop) dependents.
    std::map<std::wstring, std::vector<std::wstring>> before;  // before[x] = nodes that must come before x
    std::set<std::wstring> nodes, missing;
    std::vector<std::wstring> stack(targets.begin(), targets.end());
    // reverse index for dependents
    std::map<std::wstring, std::vector<std::wstring>> rev;
    for (auto& kv : deps) for (auto& d : kv.second) rev[d].push_back(kv.first);
    while (!stack.empty()) {
        auto n = stack.back(); stack.pop_back();
        if (!nodes.insert(n).second) continue;
        if (prereqFirst) {
            auto it = deps.find(n);
            if (it == deps.end()) { missing.insert(n); continue; }
            for (auto& d : it->second) { before[n].push_back(d); stack.push_back(d); }
        } else {
            auto it = rev.find(n);
            if (it == rev.end()) continue;
            for (auto& d : it->second) { before[n].push_back(d); stack.push_back(d); }
        }
    }
    // Kahn's algorithm over `nodes`, ordered deterministically.
    std::map<std::wstring, int> indeg; std::map<std::wstring, std::vector<std::wstring>> out;
    for (auto& n : nodes) indeg[n];
    for (auto& kv : before) for (auto& p : kv.second) if (nodes.count(p)) { out[p].push_back(kv.first); indeg[kv.first]++; }
    std::set<std::wstring> ready;
    for (auto& kv : indeg) if (kv.second == 0) ready.insert(kv.first);
    while (!ready.empty()) {
        auto n = *ready.begin(); ready.erase(ready.begin());
        r.order.push_back(n);
        for (auto& m : out[n]) if (--indeg[m] == 0) ready.insert(m);
    }
    if (r.order.size() != nodes.size()) {
        r.ok = false;
        for (auto& kv : indeg) if (kv.second > 0) r.cycle.push_back(kv.first);
        r.order.clear();
    }
    // Only unknown *dependencies* matter; a target absent from the map is also reported.
    r.missing.assign(missing.begin(), missing.end());
    return r;
}

OrderResult StartOrder(const std::vector<std::wstring>& t, const DepMap& d) { return Topo(t, d, true); }
OrderResult StopOrder(const std::vector<std::wstring>& t, const DepMap& d) { return Topo(t, d, false); }

static std::string JEsc(const std::wstring& w) {
    std::string u = ToUtf8(w), o;
    for (unsigned char c : u) {
        switch (c) {
            case '"': o += "\\\""; break; case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break; case '\r': o += "\\r"; break; case '\t': o += "\\t"; break;
            default: if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c;
        }
    }
    return o;
}

std::string JournalLine(const JournalEntry& e) {
    return "{\"time\":\"" + JEsc(e.timeUtc) + "\",\"actor\":\"" + JEsc(e.actor) + "\",\"target\":\"" + JEsc(e.target) +
           "\",\"source\":\"" + JEsc(e.source) + "\",\"before\":\"" + JEsc(e.before) + "\",\"after\":\"" + JEsc(e.after) +
           "\",\"result\":\"" + JEsc(e.result) + "\",\"reversible\":" + (e.reversible ? "true" : "false") + "}\n";
}

bool AtomicWrite(const std::wstring& path, const std::string& bytes) {
    namespace fs = std::filesystem;
    try {
        fs::path p(path);
        fs::path tmp = p; tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) return false;
            f.write(bytes.data(), (std::streamsize)bytes.size());
            f.flush();
            if (!f) { std::error_code ec; fs::remove(tmp, ec); return false; }
        }
        std::error_code ec;
        fs::rename(tmp, p, ec);
        if (ec) { fs::remove(tmp, ec); return false; }
        return true;
    } catch (...) { return false; }
}

} // namespace fsu
