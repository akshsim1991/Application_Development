// Phase 3 engine: organisation detection, live performance sample, storage trend, Windows Update status, Store apps (via PowerShell, validated ids only),
// startup impact from Windows' own boot diagnostics, and offline license-token verification (ECDSA P-256, CNG).
#include "app.h"
#include "json.h"
#include "../core/text.h"
#include <lm.h>
#include <iphlpapi.h>
#include <winevt.h>
#include <bcrypt.h>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <algorithm>

extern const unsigned char kLicensePubJson[];
extern const unsigned long kLicensePubJsonSize;

namespace fsu {
using std::wstring;

// ---------------- organisation / policy ----------------
static bool KeyExists(HKEY root, const wchar_t* path) { HKEY k; if (RegOpenKeyExW(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false; RegCloseKey(k); return true; }
OrgInfo GetOrgInfo() {
    OrgInfo o; LPWSTR name = nullptr; NETSETUP_JOIN_STATUS st = NetSetupUnknownStatus; if (NetGetJoinInformation(nullptr, &name, &st) == NERR_Success) { if (st == NetSetupDomainName) { o.domain = true; if (name) o.domainName = name; } if (name) NetApiBufferFree(name); }
    o.azureAd = KeyExists(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\CloudDomainJoin\\JoinInfo");
    HKEY k; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Enrollments", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { wchar_t n[128]; for (DWORD i = 0; RegEnumKeyW(k, i, n, 128) == ERROR_SUCCESS; ++i) { HKEY e; if (RegOpenKeyExW(k, n, 0, KEY_READ | KEY_WOW64_64KEY, &e) == ERROR_SUCCESS) { wchar_t p[128] = {}; DWORD sz = sizeof p - 2; if (RegQueryValueExW(e, L"ProviderID", nullptr, nullptr, (BYTE*)p, &sz) == ERROR_SUCCESS && p[0] && _wcsicmp(p, L"Local_Management") != 0) o.mdm = true; RegCloseKey(e); } } RegCloseKey(k); }
    o.policies = KeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate") || KeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System") || KeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection");
    return o;
}

// ---------------- live performance (opt-in, polled by the page) ----------------
PerfSample SamplePerf() {
    static ULONGLONG pi = 0, pk = 0, pu = 0; FILETIME i, k, u; PerfSample s{}; if (GetSystemTimes(&i, &k, &u)) { auto q = [](FILETIME f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }; ULONGLONG ci = q(i), ck = q(k), cu = q(u); if (pk || pu) { ULONGLONG dI = ci - pi, dT = (ck - pk) + (cu - pu); if (dT) s.cpu = 100.0 * (1.0 - (double)dI / (double)dT); } pi = ci; pk = ck; pu = cu; }
    if (s.cpu < 0) s.cpu = 0; if (s.cpu > 100) s.cpu = 100; MEMORYSTATUSEX ms{sizeof ms}; GlobalMemoryStatusEx(&ms); s.memTotal = ms.ullTotalPhys; s.memAvail = ms.ullAvailPhys;
    ULONG sz = 0; GetIfTable(nullptr, &sz, FALSE); if (sz) { std::vector<BYTE> b(sz); auto* t = (PMIB_IFTABLE)b.data(); if (GetIfTable(t, &sz, FALSE) == NO_ERROR) for (DWORD n = 0; n < t->dwNumEntries; ++n) { auto& r = t->table[n]; if (r.dwType == MIB_IF_TYPE_LOOPBACK || r.dwOperStatus != MIB_IF_OPER_STATUS_OPERATIONAL) continue; s.netRx += r.dwInOctets; s.netTx += r.dwOutOctets; } }
    return s;
}

// ---------------- storage trend (one line per drive per day, kept locally) ----------------
void RecordStorageTrend() {
    wstring path = DataDir() + L"\\storage-trend.csv"; std::string today = ToUtf8(NowIsoUtc()).substr(0, 10); std::set<std::string> seen; { std::ifstream f(std::filesystem::path(path), std::ios::binary); std::string l; while (std::getline(f, l)) { size_t c1 = l.find(','), c2 = c1 == std::string::npos ? c1 : l.find(',', c1 + 1); if (c2 != std::string::npos && l.compare(0, c1, today) == 0) seen.insert(l.substr(c1 + 1, c2 - c1 - 1)); } }
    DWORD mask = GetLogicalDrives(); std::ofstream o(std::filesystem::path(path), std::ios::app | std::ios::binary);
    for (int i = 0; i < 26; ++i) if (mask & (1u << i)) { wchar_t r[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0}; if (GetDriveTypeW(r) != DRIVE_FIXED) continue; std::string d = ToUtf8(r); if (seen.count(d)) continue; ULARGE_INTEGER fr{}, tot{}, nf{}; if (GetDiskFreeSpaceExW(r, &fr, &tot, &nf)) o << today << "," << d << "," << fr.QuadPart << "," << tot.QuadPart << "\n"; }
}
std::vector<TrendPt> StorageTrend(const wstring& drive) {
    std::vector<TrendPt> v; std::ifstream f(std::filesystem::path(DataDir() + L"\\storage-trend.csv"), std::ios::binary); std::string l, want = ToUtf8(drive);
    while (std::getline(f, l)) { std::vector<std::string> p; size_t a = 0; while (true) { size_t b = l.find(',', a); p.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a)); if (b == std::string::npos) break; a = b + 1; } if (p.size() >= 4 && p[1] == want) v.push_back({FromUtf8(p[0]), strtoull(p[2].c_str(), nullptr, 10), strtoull(p[3].c_str(), nullptr, 10)}); }
    if (v.size() > 90) v.erase(v.begin(), v.end() - 90); return v;
}

// ---------------- Windows Update status (read from the registry; no network) ----------------
WuLast WuLastTimes() {
    WuLast w; auto rd = [](const wchar_t* key) { HKEY k; wstring r; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { wchar_t b[64] = {}; DWORD n = sizeof b - 2; if (RegQueryValueExW(k, L"LastSuccessTime", nullptr, nullptr, (BYTE*)b, &n) == ERROR_SUCCESS) r = b; RegCloseKey(k); } return r; };
    w.detect = rd(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\Results\\Detect"); w.install = rd(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\Results\\Install"); return w;
}

// ---------------- run a fixed command and capture its output ----------------
bool RunCapture(const wstring& cmd, std::string& out, DWORD timeoutMs, DWORD* exitCode) {
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE}; HANDLE rd = nullptr, wr = nullptr; if (!CreatePipe(&rd, &wr, &sa, 0)) return false; SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{sizeof si}; si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = nullptr; PROCESS_INFORMATION pi{}; wstring cl = cmd;
    if (!CreateProcessW(nullptr, &cl[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { CloseHandle(rd); CloseHandle(wr); return false; } CloseHandle(wr);
    ULONGLONG t0 = GetTickCount64(); char buf[8192]; DWORD n; bool done = false;
    while (!done) { DWORD avail = 0; if (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail) { if (ReadFile(rd, buf, sizeof buf, &n, nullptr) && n) { out.append(buf, n); if (out.size() > 8000000) break; } } else { if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) { while (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail && ReadFile(rd, buf, sizeof buf, &n, nullptr) && n) out.append(buf, n); done = true; } } if (GetTickCount64() - t0 > timeoutMs) { TerminateProcess(pi.hProcess, 1); break; } }
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); if (exitCode) *exitCode = code; CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd); return done;
}

// ---------------- startup impact: Windows' own "application slowed startup" events (Diagnostics-Performance, event 101) ----------------
static std::string Tag(const std::string& xml, const char* name) { std::string key = std::string("Name='") + name + "'>"; size_t p = xml.find(key); if (p == std::string::npos) return ""; p += key.size(); size_t e = xml.find('<', p); return e == std::string::npos ? "" : xml.substr(p, e - p); }
std::map<wstring, ImpactRec> StartupImpact(wstring& err) {
    std::map<wstring, ImpactRec> out; EVT_HANDLE q = EvtQuery(nullptr, L"Microsoft-Windows-Diagnostics-Performance/Operational", L"*[System[(EventID=101)]]", EvtQueryChannelPath | EvtQueryReverseDirection);
    if (!q) { err = L"Windows' boot diagnostics log could not be read (" + Win32Err(GetLastError()) + L"). Administrator rights may be needed."; return out; }
    EVT_HANDLE evs[32]; DWORD got = 0; int total = 0; while (total < 300 && EvtNext(q, 32, evs, 1000, 0, &got)) { for (DWORD i = 0; i < got; ++i) { ++total; DWORD need = 0, props = 0; EvtRender(nullptr, evs[i], EvtRenderEventXml, 0, nullptr, &need, &props);
            if (need && need < 100000) { std::vector<wchar_t> x(need / 2 + 2); if (EvtRender(nullptr, evs[i], EvtRenderEventXml, need, x.data(), &need, &props)) { std::string xml = ToUtf8(x.data()); std::string path = Tag(xml, "Path"), name = Tag(xml, "Name"); if (name.empty()) name = Tag(xml, "FriendlyName");
                    std::string key = path.empty() ? name : path; size_t sl = key.find_last_of("\\/"); if (sl != std::string::npos) key = key.substr(sl + 1); for (auto& c : key) c = (char)tolower((unsigned char)c); if (!key.empty()) { auto& r = out[FromUtf8(key)]; ++r.boots; r.sumDeg += strtoull(Tag(xml, "DegradationTime").c_str(), nullptr, 10); r.maxTotal = std::max<ULONGLONG>(r.maxTotal, strtoull(Tag(xml, "TotalTime").c_str(), nullptr, 10)); } } }
            EvtClose(evs[i]); } }
    EvtClose(q); return out;
}

// ---------------- license tokens: base64url(payload).base64url(ECDSA-P256 signature) ----------------
static bool B64Url(const std::string& s, std::vector<BYTE>& out) {
    static std::string t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"; unsigned acc = 0; int bits = 0; out.clear(); for (char c : s) { if (c == '=') break; size_t p = t.find(c); if (p == std::string::npos) { if (c == '+') p = 62; else if (c == '/') p = 63; else return false; } acc = (acc << 6) | (unsigned)p; bits += 6; if (bits >= 8) { bits -= 8; out.push_back((BYTE)((acc >> bits) & 0xFF)); } } return true;
}
static bool VerifySig(const std::vector<BYTE>& payload, const std::vector<BYTE>& sig, const std::vector<BYTE>& x, const std::vector<BYTE>& y) {
    if (x.size() != 32 || y.size() != 32 || sig.size() != 64) return false; BCRYPT_ALG_HANDLE ec = nullptr, sh = nullptr; BCRYPT_KEY_HANDLE key = nullptr; BCRYPT_HASH_HANDLE h = nullptr; bool ok = false; BYTE dig[32];
    if (BCryptOpenAlgorithmProvider(&sh, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 && BCryptCreateHash(sh, &h, nullptr, 0, nullptr, 0, 0) == 0 && BCryptHashData(h, (PUCHAR)payload.data(), (ULONG)payload.size(), 0) == 0 && BCryptFinishHash(h, dig, 32, 0) == 0
        && BCryptOpenAlgorithmProvider(&ec, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) == 0) { std::vector<BYTE> blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64); auto* hd = (BCRYPT_ECCKEY_BLOB*)blob.data(); hd->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC; hd->cbKey = 32; memcpy(blob.data() + sizeof *hd, x.data(), 32); memcpy(blob.data() + sizeof *hd + 32, y.data(), 32);
        if (BCryptImportKeyPair(ec, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(), (ULONG)blob.size(), 0) == 0) ok = BCryptVerifySignature(key, nullptr, dig, 32, (PUCHAR)sig.data(), 64, 0) == 0; }
    if (key) BCryptDestroyKey(key); if (h) BCryptDestroyHash(h); if (ec) BCryptCloseAlgorithmProvider(ec, 0); if (sh) BCryptCloseAlgorithmProvider(sh, 0); return ok;
}
static LicenseInfo Evaluate(const std::string& token) {
    LicenseInfo L; L.state = L"Free edition"; L.edition = L"free"; if (token.empty()) return L;
    js::V pub; if (!js::Parse(std::string((const char*)kLicensePubJson, kLicensePubJsonSize), pub)) { L.state = L"License keys are not available in this build."; return L; } std::vector<BYTE> x, y; B64Url(pub.str("x"), x); B64Url(pub.str("y"), y); if (x.size() != 32) { L.state = L"This build has no license key configured. Everything is free."; return L; }
    size_t dot = token.find('.'); if (dot == std::string::npos) { L.state = L"That is not a valid license key."; return L; } std::vector<BYTE> payload, sig; if (!B64Url(token.substr(0, dot), payload) || !B64Url(token.substr(dot + 1), sig) || !VerifySig(payload, sig, x, y)) { L.state = L"The license key is not valid (signature check failed)."; return L; }
    js::V v; if (!js::Parse(std::string(payload.begin(), payload.end()), v) || v.str("product") != "FeatherSystemUtilities") { L.state = L"The license key is for a different product."; return L; }
    L.edition = FromUtf8(v.str("edition", "pro")); L.name = FromUtf8(v.str("name")); L.id = FromUtf8(v.str("id")); L.expires = FromUtf8(v.str("expires")); L.devices = (int)v.num("devices", 1);
    std::string today = ToUtf8(NowIsoUtc()).substr(0, 10); if (!L.expires.empty() && ToUtf8(L.expires) < today) { L.state = L"Expired on " + L.expires + L". Core features stay free; paid features pause."; L.valid = false; return L; }
    L.valid = true; L.state = L"Licensed (" + L.edition + L")" + (L.expires.empty() ? L"" : L" until " + L.expires); return L;
}
LicenseInfo LicenseStatus() { std::ifstream f(std::filesystem::path(DataDir() + L"\\license.token"), std::ios::binary); std::string t((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); while (!t.empty() && isspace((unsigned char)t.back())) t.pop_back(); return Evaluate(t); }
bool LicenseActivate(const std::string& token, wstring& err) { std::string t = token; while (!t.empty() && isspace((unsigned char)t.back())) t.pop_back(); while (!t.empty() && isspace((unsigned char)t.front())) t.erase(t.begin()); LicenseInfo L = Evaluate(t); if (!L.valid) { err = L.state; return false; } if (!AtomicWrite(DataDir() + L"\\license.token", t)) { err = L"The license could not be saved."; return false; } WriteJournal(L"License", L"activation", L"free edition", L.state, L"ok", false); return true; }
void LicenseRemove() { DeleteFileW((DataDir() + L"\\license.token").c_str()); WriteJournal(L"License", L"removal", L"licensed", L"free edition", L"ok", false); }
}
