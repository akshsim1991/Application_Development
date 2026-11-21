#include "updater.hpp"
#include <winhttp.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <memory>
#include <condition_variable>
#include <mutex>
#include "platform.hpp"
#include "update.hpp"

namespace updater {

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x00002000
#endif

namespace {

// Why a WinHTTP call failed, in words.
std::wstring netError(DWORD e) {
    switch (e) {
        case ERROR_WINHTTP_NAME_NOT_RESOLVED: return L"the address was not found (DNS)";
        case ERROR_WINHTTP_CANNOT_CONNECT: return L"no connection could be made";
        case ERROR_WINHTTP_TIMEOUT: return L"no answer in time";
        case ERROR_WINHTTP_CONNECTION_ERROR: return L"the connection was broken";
        case ERROR_WINHTTP_SECURE_FAILURE: case ERROR_WINHTTP_SECURE_CHANNEL_ERROR: return L"the secure connection failed (a proxy, antivirus or the PC's date may be the cause)";
        default: return L"network error " + std::to_wstring(e);
    }
}

// One address. `accept`: the Accept header, or empty.
bool fetch(const std::string& url, const std::string& accept, std::vector<unsigned char>& out, std::wstring& error,
           const std::function<bool(long long, long long)>& progress) {
    out.clear();
    const std::wstring w = platform::wide(url);
    URL_COMPONENTS uc{sizeof(uc)};
    wchar_t host[256] = L"", path[2048] = L"";
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    wchar_t extra[1024] = L"";
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;   // the ?query
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &uc)) { error = L"the address is not valid"; return false; }
    const std::wstring object = std::wstring(path) + extra;
    // the proxy as Windows has it set (Settings > Network > Proxy, also automatic); older Windows: WinHTTP's own setting
    HINTERNET ses = WinHttpOpen(L"FeatherPDF-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!ses) ses = WinHttpOpen(L"FeatherPDF-Updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!ses) { error = L"the network could not be used"; return false; }
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
        protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;   // Windows without TLS 1.3
        WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
    }
    WinHttpSetTimeouts(ses, 15000, 15000, 20000, 60000);
    bool ok = false;
    DWORD fail = 0;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) fail = GetLastError();
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (con && !req) fail = GetLastError();
    std::wstring headers;
    if (!accept.empty()) headers = L"Accept: " + platform::wide(accept) + L"\r\n";
    if (req && WinHttpSendRequest(req, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), headers.empty() ? 0 : static_cast<DWORD>(-1L),
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz, nullptr);
        wchar_t lenText[32] = L"";
        DWORD lenSize = sizeof(lenText);
        const long long total = WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, lenText, &lenSize, nullptr) ? _wtoi64(lenText) : 0;
        if (status == 200) {
            ok = true;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail)) { ok = false; error = L"the download stopped (" + netError(GetLastError()) + L")"; break; }
                if (avail == 0) break;
                const size_t at = out.size();
                out.resize(at + avail);
                DWORD got = 0;
                if (!WinHttpReadData(req, out.data() + at, avail, &got)) { ok = false; error = L"the download stopped (" + netError(GetLastError()) + L")"; break; }
                out.resize(at + got);
                if (progress && !progress(static_cast<long long>(out.size()), total)) { ok = false; error = L"stopped"; break; }
            }
        } else error = status == 407 ? std::wstring(L"the proxy asks for a password (407)")
                     : status == 403 && host == std::wstring(L"api.github.com") ? std::wstring(L"too many requests from this internet address for now (403)")
                     : L"the server answered with an error (" + std::to_wstring(status) + L")";
    } else error = netError(fail ? fail : GetLastError());
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

} // namespace

// The small version file: every server is asked at the same moment and the first good answer is taken, so a server
// that does not answer (blocked on some networks) costs no time. raw.githubusercontent.com and GitHub's API are the
// freshest: a jsDelivr answer is taken once neither of them has answered within two seconds more.
bool fetchFastest(const std::string& url, std::vector<unsigned char>& out, std::wstring& error, std::string& via) {
    struct Shared {
        std::mutex m;
        std::condition_variable cv;
        std::vector<int> state;   // 0 waiting, 1 good, 2 failed
        std::vector<std::vector<unsigned char>> data;
        std::vector<std::wstring> why;
    };
    auto sources = update::sources(url);
    wchar_t skip[256] = L"";
    GetEnvironmentVariableW(L"FEATHER_UPDATE_SKIP", skip, 256);   // the tests act as if those servers were blocked
    auto sh = std::make_shared<Shared>();
    sh->state.assign(sources.size(), 0);
    sh->data.resize(sources.size());
    sh->why.resize(sources.size());
    for (size_t i = 0; i < sources.size(); ++i) {
        if (*skip && std::wstring(skip).find(platform::wide(sources[i].host)) != std::wstring::npos) { sh->state[i] = 2; sh->why[i] = L"skipped (test)"; continue; }
        wchar_t slow[256] = L"";   // FEATHER_UPDATE_SLOW=host: the tests make that server answer 20 seconds late
        GetEnvironmentVariableW(L"FEATHER_UPDATE_SLOW", slow, 256);
        const bool late = *slow && std::wstring(slow).find(platform::wide(sources[i].host)) != std::wstring::npos;
        std::thread([sh, i, late, src = sources[i]] {
            if (late) Sleep(20000);
            std::vector<unsigned char> d;
            std::wstring e;
            bool ok = fetch(src.url, src.accept, d, e, nullptr);
            if (ok && !src.pinnedPath.empty()) {   // jsDelivr named the branch's commit: the file at that commit
                const std::string at = update::pinnedUrl(src, std::string(d.begin(), d.end()));
                d.clear();
                ok = !at.empty() && fetch(at, "", d, e, nullptr);
                if (at.empty()) e = L"jsDelivr did not say which commit is the newest";
            }
            std::lock_guard<std::mutex> l(sh->m);
            sh->state[i] = ok ? 1 : 2;
            if (ok) sh->data[i] = std::move(d); else sh->why[i] = e;
            sh->cv.notify_all();
        }).detach();
    }
    // jsDelivr's answers may be behind (its copy of the branch, its idea of the newest commit): taken only when no fresh
    // server answers, and then the newest version any of them gives
    auto isJsDelivr = [&](size_t i) { return sources[i].host.find("jsdelivr") != std::string::npos; };
    std::unique_lock<std::mutex> l(sh->m);
    auto best = [&]() -> int {   // a fresh server that answered, else -1
        for (size_t i = 0; i < sources.size(); ++i) if (sh->state[i] == 1 && !isJsDelivr(i)) return static_cast<int>(i);
        return -1;
    };
    auto any = [&]() -> int {
        int pick = -1;
        std::string top;
        for (size_t i = 0; i < sources.size(); ++i) {
            if (sh->state[i] != 1) continue;
            update::Release r;
            const std::string v = update::parse(std::string(sh->data[i].begin(), sh->data[i].end()), r) ? r.version : "";
            if (pick < 0 || (!v.empty() && (top.empty() || update::compareVersions(v, top) > 0))) { pick = static_cast<int>(i); top = v; }
        }
        return pick;
    };
    auto allDone = [&]() { for (int s2 : sh->state) if (s2 == 0) return false; return true; };
    sh->cv.wait(l, [&] { return best() >= 0 || any() >= 0 || allDone(); });
    if (best() < 0 && any() >= 0) sh->cv.wait_for(l, std::chrono::seconds(2), [&] { return best() >= 0 || allDone(); });
    int pick = best() >= 0 ? best() : any();
    if (pick >= 0) { out = std::move(sh->data[pick]); via = sources[pick].host; error.clear(); return true; }
    std::wstring tried;
    for (size_t i = 0; i < sources.size(); ++i) tried += L"\n  " + platform::wide(sources[i].host) + L": " + sh->why[i];
    error = L"None of the update servers could be reached:" + tried;
    return false;
}

bool download(const std::string& url, std::vector<unsigned char>& out, std::wstring& error,
              const std::function<bool(long long, long long)>& progress, const std::string& preferHost) {
    std::wstring tried;
    // FEATHER_UPDATE_SKIP=host[,host]: the tests act as if those servers were blocked
    wchar_t skip[256] = L"";
    GetEnvironmentVariableW(L"FEATHER_UPDATE_SKIP", skip, 256);
    std::vector<update::Source> list = update::sources(url);
    std::stable_partition(list.begin(), list.end(), [&](const update::Source& x) { return !preferHost.empty() && x.host == preferHost; });
    for (const update::Source& s : list) {
        std::wstring e;
        if (*skip && std::wstring(skip).find(platform::wide(s.host)) != std::wstring::npos) { tried += L"\n  " + platform::wide(s.host) + L": skipped (test)"; continue; }
        if (!s.pinnedPath.empty()) {   // which commit is the newest, then the file at that commit
            std::vector<unsigned char> feed;
            const std::string at = fetch(s.url, s.accept, feed, e, nullptr) ? update::pinnedUrl(s, std::string(feed.begin(), feed.end())) : std::string();
            if (!at.empty() && fetch(at, "", out, e, progress)) { error = L"from " + platform::wide(s.host) + L" (jsDelivr)"; return true; }
            if (e == L"stopped") { error = L"The download was stopped."; return false; }
            tried += L"\n  " + platform::wide(s.host) + L": " + (at.empty() && e.empty() ? std::wstring(L"no commit named") : e);
            continue;
        }
        if (fetch(s.url, s.accept, out, e, progress)) { error = L"from " + platform::wide(s.host); return true; }
        if (e == L"stopped") { error = L"The download was stopped."; return false; }
        if (e.find(L"(404)") != std::wstring::npos && s.url == url) { error = L"The update server answered with an error (404)."; return false; }   // nothing there
        tried += L"\n  " + platform::wide(s.host) + L": " + e;
    }
    error = L"None of the update servers could be reached:" + tried;
    return false;
}

void checkInBackground(HWND wnd, UINT msg, bool manual) {
    std::thread([wnd, msg, manual] {
        std::vector<unsigned char> bytes;
        std::wstring err;
        update::Release* r = new update::Release;
        std::string via;
        if (fetchFastest(update::kLatestUrl, bytes, err, via)) {
            if (!update::parse(std::string(bytes.begin(), bytes.end()), *r)) { *r = update::Release{}; r->problem = L"The version file that came back could not be read."; }
            r->via = via;
        } else if (err.find(L"(404)") != std::wstring::npos) {   // the server answered: nothing published there yet
            r->version = "0";
        } else r->problem = err;
        if (!PostMessageW(wnd, msg, manual ? 1 : 0, reinterpret_cast<LPARAM>(r))) delete r;
    }).detach();
}

} // namespace updater
