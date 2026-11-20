// Recommendation knowledge base for Startup entries and Services ("safe to disable?").
// A built-in copy (data/advice.json) is always available offline; when the PC is online Feather can fetch a newer copy of the same file
// from the project's release repo (HTTPS, host allow-list, size/shape validated). These are suggestions only: nothing is ever changed automatically.
#include "app.h"
#include "json.h"
#include "../core/text.h"
#include <mutex>
#include <fstream>
#include <filesystem>
#include <algorithm>

extern const unsigned char kAdviceJson[];
extern const unsigned long kAdviceJsonSize;

namespace fsu {
using std::wstring;
struct Rule { std::vector<wstring> keys; wstring verdict, reason; };
static std::vector<Rule> g_su, g_sv; static wstring g_ver = L"0", g_updated, g_source = L"built-in"; static bool g_loaded; static std::mutex g_mx;
static wstring Low(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static bool GoodVerdict(const std::string& v) { return v == "safe" || v == "optional" || v == "caution" || v == "keep"; }

static bool Parse(const std::string& json, std::vector<Rule>& su, std::vector<Rule>& sv, wstring& ver, wstring& updated) {
    js::V v; if (json.size() > 600000 || !js::Parse(json, v) || v.t != js::V::Obj) return false; ver = FromUtf8(v.str("version")); if (ver.empty() || ver.size() > 20) return false; updated = FromUtf8(v.str("updated").substr(0, 40));
    auto load = [&](const char* key, const char* kf, std::vector<Rule>& out) { auto* a = v.get(key); if (!a || a->t != js::V::Arr || a->a.size() > 2000) return false;
        for (auto& r : a->a) { if (r.t != js::V::Obj) return false; std::string verdict = r.str("v"), reason = r.str("r"); if (!GoodVerdict(verdict) || reason.empty() || reason.size() > 400) return false; Rule x; x.verdict = FromUtf8(verdict); x.reason = FromUtf8(reason);
            auto* k = r.get(kf); if (!k || k->t != js::V::Arr || k->a.empty() || k->a.size() > 80) return false; for (auto& s : k->a) { if (s.t != js::V::Str || s.s.empty() || s.s.size() > 60) return false; x.keys.push_back(Low(FromUtf8(s.s))); } out.push_back(x); } return true; };
    return load("startup", "c", su) && load("services", "n", sv);
}
static void EnsureLoaded() {
    if (g_loaded) return; g_loaded = true; std::string emb((const char*)kAdviceJson, kAdviceJsonSize); wstring v, u; std::vector<Rule> su, sv;
    if (Parse(emb, su, sv, v, u)) { g_su = su; g_sv = sv; g_ver = v; g_updated = u; }
    std::ifstream f(std::filesystem::path(DataDir() + L"\\advice.json"), std::ios::binary); if (f) { std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); wstring v2, u2; std::vector<Rule> s2, sv2;
        if (Parse(body, s2, sv2, v2, u2) && CompareVersions(v2, g_ver) > 0) { g_su = s2; g_sv = sv2; g_ver = v2; g_updated = u2; g_source = L"downloaded"; } }
}
AdviceInfo AdviceStatus() { std::lock_guard<std::mutex> l(g_mx); EnsureLoaded(); return {g_ver, g_updated, g_source, (int)g_su.size(), (int)g_sv.size()}; }
static Advice Make(const wstring& v, const wstring& r) { return {v, r, L"Feather knowledge base " + g_ver}; }
Advice AdviceForStartup(const StartupEntry& e, bool fileMissing) {
    std::lock_guard<std::mutex> l(g_mx); EnsureLoaded();
    if (fileMissing) return Make(L"safe", L"The program this entry starts no longer exists, so the entry does nothing. It is safe to disable or delete.");
    wstring hay = Low(e.name + L"|" + e.publisher + L"|" + e.command + L"|" + e.program);
    for (auto& r : g_su) for (auto& k : r.keys) if (hay.find(k) != wstring::npos) return Make(r.verdict, r.reason);
    if (e.microsoft) return Make(L"keep", L"Part of Windows or a Microsoft product. Leave it unless you know you do not use that product.");
    return {L"unknown", L"No recommendation yet. Use “Look up online” to research it before changing anything.", L"no match"};
}
Advice AdviceForService(const ServiceEntry& s) {
    std::lock_guard<std::mutex> l(g_mx); EnsureLoaded();
    if (s.critical) return Make(L"keep", L"On Feather's critical list. Stopping or disabling it can make Windows unstable.");
    wstring n = Low(s.name); for (auto& r : g_sv) for (auto& k : r.keys) if (n == k) return Make(r.verdict, r.reason);
    if (s.isDriver) return Make(L"caution", L"A kernel driver. Leave drivers alone unless you are troubleshooting a specific problem.");
    if (Low(s.publisher).find(L"microsoft") != wstring::npos) return {L"unknown", L"A Windows service with no specific recommendation. Leave it unless you know you do not need it.", L"no match"};
    return {L"unknown", L"A third-party service with no recommendation yet. If you no longer use its program, Manual is usually safe. Use “Look up online” first.", L"no match"};
}
bool AdviceRefresh(AdviceInfo& info, wstring& err) {
    std::string body; if (!FetchAdviceJson(body, err)) return false; wstring v, u; std::vector<Rule> su, sv; if (!Parse(body, su, sv, v, u)) { err = L"The downloaded recommendations were not in the expected format and were ignored."; return false; }
    std::lock_guard<std::mutex> l(g_mx); EnsureLoaded(); if (CompareVersions(v, g_ver) > 0) { if (!AtomicWrite(DataDir() + L"\\advice.json", body)) { err = L"Could not save the recommendations."; return false; } g_su = su; g_sv = sv; g_ver = v; g_updated = u; g_source = L"downloaded"; }
    info = {g_ver, g_updated, g_source, (int)g_su.size(), (int)g_sv.size()}; return true;
}
}
