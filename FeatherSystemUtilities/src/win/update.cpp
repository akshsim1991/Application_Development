// Auto-update, modelled on Feather PDF: one HTTPS GET of latest.json from the public releases repo, user-approved download,
// size + SHA-256 verification, elevated silent install, automatic relaunch. No identifiers are sent besides the user-agent.
#include "app.h"
#include "../core/text.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <fstream>
#include <filesystem>

namespace fsu {
using std::wstring;

// Primary: the dedicated public releases repo (like Feather PDF). Fallback: the project repo itself, so updates work before that repo exists.
static const wchar_t* kManifestUrls[] = {L"https://raw.githubusercontent.com/akshsim1991/FeatherSystemUtilities-releases/main/latest.json", L"https://cdn.jsdelivr.net/gh/akshsim1991/FeatherSystemUtilities-releases@main/latest.json", L"https://raw.githubusercontent.com/akshsim1991/Application_Development/claude/new-session-ann5h4/FeatherSystemUtilities/releases-repo/latest.json"};
static const wchar_t* kAllowedHosts[] = {L"raw.githubusercontent.com", L"cdn.jsdelivr.net", L"github.com", L"objects.githubusercontent.com", L"release-assets.githubusercontent.com"};

static const wchar_t* kAdviceUrls[] = {L"https://raw.githubusercontent.com/akshsim1991/FeatherSystemUtilities-releases/main/advice.json", L"https://cdn.jsdelivr.net/gh/akshsim1991/FeatherSystemUtilities-releases@main/advice.json", L"https://raw.githubusercontent.com/akshsim1991/Application_Development/claude/new-session-ann5h4/FeatherSystemUtilities/releases-repo/advice.json"};
static bool HostAllowed(const wstring& host) { for (auto h : kAllowedHosts) if (_wcsicmp(host.c_str(), h) == 0) return true; return false; }

static wstring HttpErr(DWORD e) {   // WinHTTP errors have no system message text: explain the common ones in plain language
    wstring t; switch (e) {
    case 12002: t = L"The connection timed out. A slow network, a proxy, or security software that scans downloads may be holding it up."; break;
    case 12007: t = L"The server name could not be resolved. Check your internet/DNS connection."; break;
    case 12029: case 12030: t = L"Could not connect to the server. A firewall or proxy may be blocking it."; break;
    case 12175: case 12038: case 12044: case 12045: case 12157: t = L"The secure connection failed. Security software or a proxy may be inspecting HTTPS traffic with a certificate Windows does not trust."; break;
    case 12031: case 12152: case 12163: t = L"The connection was interrupted."; break;
    default: t = Win32Err(e); }
    return t + L" (error " + std::to_wstring(e) + L")";
}
// Honour the user's own proxy settings (system proxy, WPAD or PAC script as configured in Windows/Edge), which WinHTTP's default does not.
static void ApplyUserProxy(HINTERNET ses, HINTERNET req, const wstring& url) {
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie{}; if (!WinHttpGetIEProxyConfigForCurrentUser(&ie)) return; WINHTTP_PROXY_INFO pi{}; bool have = false, own = false;
    if (ie.fAutoDetect || ie.lpszAutoConfigUrl) { WINHTTP_AUTOPROXY_OPTIONS ao{}; ao.dwFlags = (ie.fAutoDetect ? WINHTTP_AUTOPROXY_AUTO_DETECT : 0) | (ie.lpszAutoConfigUrl ? WINHTTP_AUTOPROXY_CONFIG_URL : 0); ao.lpszAutoConfigUrl = ie.lpszAutoConfigUrl; ao.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A; ao.fAutoLogonIfChallenged = TRUE; if (WinHttpGetProxyForUrl(ses, url.c_str(), &ao, &pi)) { have = pi.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY; own = true; } }
    else if (ie.lpszProxy) { pi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY; pi.lpszProxy = ie.lpszProxy; pi.lpszProxyBypass = ie.lpszProxyBypass; have = true; }
    if (have) WinHttpSetOption(req, WINHTTP_OPTION_PROXY, &pi, sizeof pi);
    if (own) { if (pi.lpszProxy) GlobalFree(pi.lpszProxy); if (pi.lpszProxyBypass) GlobalFree(pi.lpszProxyBypass); }
    if (ie.lpszProxy) GlobalFree(ie.lpszProxy); if (ie.lpszProxyBypass) GlobalFree(ie.lpszProxyBypass); if (ie.lpszAutoConfigUrl) GlobalFree(ie.lpszAutoConfigUrl);
}
static bool HttpGet(const wstring& url, std::vector<char>& out, ULONGLONG maxBytes, std::atomic<bool>* cancel, const std::function<void(ULONGLONG, ULONGLONG)>& progress, wstring& err) {
    URL_COMPONENTSW uc{}; uc.dwStructSize = sizeof uc; wchar_t host[256], path[2048]; uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048; uc.dwSchemeLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS || !HostAllowed(host)) { err = L"Blocked: only HTTPS downloads from the project's release hosts are allowed."; return false; }
    HINTERNET ses = WinHttpOpen(L"FeatherSystemUtilities/" FSU_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0); if (!ses) { err = Win32Err(GetLastError()); return false; }
    DWORD proto = 0x800; WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof proto);   // TLS 1.2 (Windows 7 needs this explicitly)
    if (maxBytes > 1024 * 1024) WinHttpSetTimeouts(ses, 20000, 20000, 60000, 300000); else WinHttpSetTimeouts(ses, 8000, 8000, 15000, 30000);   // installers: some networks scan executables before sending them, which can take minutes
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0); HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr; bool ok = false;
    if (!req) err = HttpErr(GetLastError());
    else {
        ApplyUserProxy(ses, req, url); DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP; WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof redir);
        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
            DWORD code = 0, sz = sizeof code; WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
            if (code != 200) err = L"The server answered HTTP " + std::to_wstring(code) + (code == 404 ? L" (no release published yet; check that the releases page exists)." : L".");
            else {
                DWORD64 total = 0; DWORD tl = sizeof total; WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64, nullptr, &total, &tl, nullptr);
                ok = true; DWORD avail = 0; while (WinHttpQueryDataAvailable(req, &avail) && avail) {
                    if (cancel && cancel->load()) { ok = false; err = L"Cancelled."; break; } if (out.size() + avail > maxBytes) { ok = false; err = L"The download is larger than expected; stopped."; break; }
                    size_t at = out.size(); out.resize(at + avail); DWORD got = 0; if (!WinHttpReadData(req, out.data() + at, avail, &got)) { ok = false; err = HttpErr(GetLastError()); break; } out.resize(at + got); if (progress) progress(out.size(), total); }
            }
        } else err = L"Could not download from the update server: " + HttpErr(GetLastError());
    }
    if (req) WinHttpCloseHandle(req); if (con) WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return ok;
}

static wstring JsonStr(const std::string& j, const char* key, size_t from = 0) {   // minimal reader for flat string values
    std::string k = std::string("\"") + key + "\""; size_t p = j.find(k, from); if (p == std::string::npos) return L""; p = j.find(':', p + k.size()); if (p == std::string::npos) return L""; p = j.find('"', p); if (p == std::string::npos) return L"";
    std::string o; for (++p; p < j.size() && j[p] != '"'; ++p) { if (j[p] == '\\' && p + 1 < j.size()) { ++p; o += j[p] == 'n' ? ' ' : j[p]; } else o += j[p]; } return FromUtf8(o);
}
static ULONGLONG JsonNum(const std::string& j, const char* key, size_t from) { std::string k = std::string("\"") + key + "\""; size_t p = j.find(k, from); if (p == std::string::npos) return 0; p = j.find(':', p); return p == std::string::npos ? 0 : (ULONGLONG)strtoull(j.c_str() + p + 1, nullptr, 10); }

bool FetchRepoFile(const wchar_t* name, std::string& body, wstring& err) {   // same hosts/limits as the manifest: raw GitHub, jsDelivr, project repo
    static const wchar_t* bases[] = {L"https://raw.githubusercontent.com/akshsim1991/FeatherSystemUtilities-releases/main/", L"https://cdn.jsdelivr.net/gh/akshsim1991/FeatherSystemUtilities-releases@main/", L"https://raw.githubusercontent.com/akshsim1991/Application_Development/claude/new-session-ann5h4/FeatherSystemUtilities/releases-repo/"};
    wstring last; for (auto b : bases) { std::vector<char> v; wstring e; if (HttpGet(wstring(b) + name, v, 512 * 1024, nullptr, nullptr, e)) { body.assign(v.begin(), v.end()); return true; } last = e; } err = last.empty() ? L"Not found online." : last; return false;
}
bool FetchAdviceJson(std::string& body, wstring& err) {
    wstring last; for (auto url : kAdviceUrls) { std::vector<char> b; wstring e; if (HttpGet(url, b, 512 * 1024, nullptr, nullptr, e)) { body.assign(b.begin(), b.end()); return true; } last = e; }
    err = last.empty() ? L"No recommendations found online." : last; return false;
}
bool FetchLatestVersion(UpdateInfo& u, bool beta) {
    wstring lastErr;
    for (auto url0 : kManifestUrls) {
        std::vector<char> body; wstring e, url = url0; bool got = false;
        if (beta) { wstring bu = url0; size_t p = bu.rfind(L"latest.json"); if (p != wstring::npos) { bu.replace(p, 11, L"latest-beta.json"); std::vector<char> b2; wstring e2; if (HttpGet(bu, b2, 256 * 1024, nullptr, nullptr, e2)) { body = b2; got = true; } } }   // no beta manifest: fall back to stable
        if (!got && !HttpGet(url, body, 256 * 1024, nullptr, nullptr, e)) { lastErr = e; continue; }
        std::string j(body.begin(), body.end()); u.version = JsonStr(j, "version"); u.date = JsonStr(j, "date"); u.notes = JsonStr(j, "notes"); size_t w = j.find("\"windows\"");
        if (u.version.empty() || w == std::string::npos) { lastErr = L"The update information is incomplete."; continue; }
        { size_t pv = j.find("\"previous\""); if (pv != std::string::npos) { u.prevVersion = JsonStr(j, "version", pv); u.prevUrl = JsonStr(j, "url", pv); u.prevSha = Lower(JsonStr(j, "sha256", pv)); u.prevSize = JsonNum(j, "size", pv); if (u.prevSha.size() != 64) { u.prevVersion.clear(); u.prevUrl.clear(); } } }
        u.signer = JsonStr(j, "signer"); u.url = JsonStr(j, "url", w); u.sha256 = Lower(JsonStr(j, "sha256", w)); u.size = JsonNum(j, "size", w);
        if (u.url.empty() || u.sha256.size() != 64) { lastErr = L"The update information is incomplete."; continue; } u.ok = true; return true;
    }
    u.error = lastErr.empty() ? L"No update information found." : lastErr; return false;
}

static wstring Sha256File(const wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr); if (f == INVALID_HANDLE_VALUE) return L"";
    BCRYPT_ALG_HANDLE a = nullptr; BCRYPT_HASH_HANDLE h = nullptr; wstring out; if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 && BCryptCreateHash(a, &h, nullptr, 0, nullptr, 0, 0) == 0) {
        std::vector<BYTE> buf(1 << 20); DWORD n; bool ok = true; while (ReadFile(f, buf.data(), (DWORD)buf.size(), &n, nullptr) && n) if (BCryptHashData(h, buf.data(), n, 0)) { ok = false; break; }
        BYTE d[32]; if (ok && BCryptFinishHash(h, d, 32, 0) == 0) for (BYTE b : d) { wchar_t t[3]; swprintf(t, 3, L"%02x", b); out += t; } }
    if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0); CloseHandle(f); return out;
}

static wstring MirrorOf(const wstring& url) {   // raw.githubusercontent.com/<owner>/<repo>/<ref>/<path>  ->  cdn.jsdelivr.net/gh/<owner>/<repo>@<ref>/<path> (the SHA-256 check still decides whether a download is accepted)
    const wchar_t* pre = L"https://raw.githubusercontent.com/"; if (url.rfind(pre, 0) != 0) return L""; wstring r = url.substr(wcslen(pre)); size_t a = r.find(L'/'), b = a == wstring::npos ? a : r.find(L'/', a + 1), c = b == wstring::npos ? b : r.find(L'/', b + 1); if (c == wstring::npos) return L"";
    return L"https://cdn.jsdelivr.net/gh/" + r.substr(0, b) + L"@" + r.substr(b + 1, c - b - 1) + r.substr(c);
}
bool DownloadUpdate(const UpdateInfo& u, wstring& path, wstring& err, std::atomic<bool>* cancel, const std::function<void(ULONGLONG, ULONGLONG)>& progress) {
    std::vector<char> body; std::vector<wstring> urls{u.url}; wstring mir = MirrorOf(u.url); if (!mir.empty()) urls.push_back(mir); bool got = false; wstring all;
    for (auto& url : urls) { for (int attempt = 0; attempt < 2 && !got; ++attempt) { body.clear(); wstring e; if (HttpGet(url, body, (u.size ? u.size : 200ull * 1024 * 1024) + 1024, cancel, progress, e)) { got = true; break; } all = e; if (cancel && cancel->load()) { err = e; return false; } if (e.find(L"Blocked") == 0) break; Sleep(1500); } if (got) break; }
    if (!got) { err = all + L" Tried the release page and a mirror. You can download the installer in your browser instead."; return false; }
    if (u.size && body.size() != u.size) { err = L"The downloaded file has the wrong size; discarded."; return false; }
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp); path = wstring(tmp) + L"FeatherSystemUtilities-Setup-" + u.version + L".exe"; { std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc); if (!f) { err = L"Cannot write the installer to the temp folder."; return false; } f.write(body.data(), (std::streamsize)body.size()); }
    if (Sha256File(path) != u.sha256) { DeleteFileW(path.c_str()); err = L"The downloaded installer failed its SHA-256 check and was deleted. Nothing was installed."; return false; }   // integrity gate
    if (!u.signer.empty()) {   // once the project's installers are code-signed, the manifest names the signer and an unsigned or differently-signed file is refused
        FileInfo fi = InspectFile(path, true); wstring want = Lower(u.signer), got = Lower(fi.signer.empty() ? fi.publisher : fi.signer);
        if (fi.sig != Sig::Valid || got.find(want) == wstring::npos) { DeleteFileW(path.c_str()); err = L"The installer is not signed by " + u.signer + L" (or its signature is invalid) and was deleted. Nothing was installed."; return false; } }
    return true;
}

bool IsInstalledCopy() {
    HKEY k; wchar_t loc[MAX_PATH] = L""; DWORD sz = sizeof loc - 2, ty = 0; bool inst = false;
    for (REGSAM v : {KEY_WOW64_64KEY, KEY_WOW64_32KEY}) if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FeatherSystemUtilities", 0, KEY_QUERY_VALUE | v, &k) == ERROR_SUCCESS) { if (RegQueryValueExW(k, L"InstallLocation", nullptr, &ty, (LPBYTE)loc, &sz) == ERROR_SUCCESS) { wchar_t me[MAX_PATH]; GetModuleFileNameW(nullptr, me, MAX_PATH); wstring m = me; size_t sl = m.find_last_of(L'\\'); inst = sl != wstring::npos && IsPathUnder(m.substr(0, sl), loc) && IsPathUnder(loc, m.substr(0, sl)); } RegCloseKey(k); if (inst) break; }
    return inst;
}

bool LaunchUpdateInstaller(const wstring& installer, wstring& err) {
    wchar_t me[MAX_PATH]; GetModuleFileNameW(nullptr, me, MAX_PATH);
    // relaunch the app (as the normal user) a little after the silent installer has finished
    wstring cmd = L"cmd.exe /c ping -n 25 127.0.0.1 >nul & start \"\" \"" + wstring(me) + L"\""; STARTUPINFOW si{sizeof si}; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
    std::wstring c2 = cmd; if (CreateProcessW(nullptr, &c2[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    if ((INT_PTR)ShellExecuteW(nullptr, L"runas", installer.c_str(), L"/S", nullptr, SW_SHOWNORMAL) <= 32) { err = L"The installer could not be started (the administrator prompt may have been declined)."; return false; }
    return true;
}

} // namespace fsu
