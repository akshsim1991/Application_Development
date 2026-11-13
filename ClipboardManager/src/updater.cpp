// Looking for and downloading new versions (WinHTTP). What the version file says is in core.cpp (ParseLatest).
// Some networks block raw.githubusercontent.com while others work, so the file is also tried through the jsDelivr
// mirror and GitHub's API, exactly as Feather PDF does.
#include "app.h"
#include <winhttp.h>
#include <process.h>

namespace {

struct Source { std::string url, accept, host; };

std::vector<Source> Sources(const std::string& url) {
    std::vector<Source> v;
    v.push_back({url, "", ""});
    const std::string raw = "https://raw.githubusercontent.com/";
    if (url.compare(0, raw.size(), raw) == 0) {
        // /owner/repo/branch/path
        std::string rest = url.substr(raw.size());
        size_t a = rest.find('/'), b = a == std::string::npos ? a : rest.find('/', a + 1), c = b == std::string::npos ? b : rest.find('/', b + 1);
        if (c != std::string::npos) {
            std::string owner = rest.substr(0, a), repo = rest.substr(a + 1, b - a - 1), branch = rest.substr(b + 1, c - b - 1), path = rest.substr(c + 1);
            v.push_back({"https://cdn.jsdelivr.net/gh/" + owner + "/" + repo + "@" + branch + "/" + path, "", "cdn.jsdelivr.net"});
            v.push_back({"https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + path + "?ref=" + branch, "application/vnd.github.raw", "api.github.com"});
        }
        v[0].host = "raw.githubusercontent.com";
    } else {
        size_t s = url.find("://");
        size_t e = url.find('/', s == std::string::npos ? 0 : s + 3);
        v[0].host = url.substr(s == std::string::npos ? 0 : s + 3, e == std::string::npos ? std::string::npos : e - (s + 3));
    }
    return v;
}

std::wstring W(const std::string& s) { return core::FromUtf8(s); }

bool Get(const Source& src, std::string& out, std::wstring& err, const std::function<bool(long long, long long)>& progress, size_t cap) {
    out.clear();
    std::wstring url = W(src.url);
    URL_COMPONENTSW uc = {sizeof uc};
    wchar_t host[256] = L"", path[2048] = L"";
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2047;
    uc.dwExtraInfoLength = (DWORD)-1;
    wchar_t extra[1024] = L"";
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1023;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { err = L"The address is not valid."; return false; }
    std::wstring fullPath = std::wstring(path) + extra;
    HINTERNET ses = WinHttpOpen(L"FeatherClipboardManager-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!ses) ses = WinHttpOpen(L"FeatherClipboardManager-Updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!ses) { err = L"The internet connection could not be opened."; return false; }
    WinHttpSetTimeouts(ses, 8000, 8000, 15000, 30000);
    bool ok = false;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) err = L"Could not connect to " + std::wstring(host) + L".";
    else {
        DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET req = WinHttpOpenRequest(con, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!req) err = L"The request could not be made.";
        else {
            std::wstring hdr = src.accept.empty() ? L"" : L"Accept: " + W(src.accept);
            if (WinHttpSendRequest(req, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(), hdr.empty() ? 0 : (DWORD)-1L, nullptr, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
                DWORD code = 0, sz = sizeof code;
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
                if (code != 200) err = L"The update server answered with an error (" + std::to_wstring(code) + L").";
                else {
                    DWORD cl = 0;
                    sz = sizeof cl;
                    long long total = 0;
                    if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &cl, &sz, nullptr)) total = cl;
                    ok = true;
                    for (;;) {
                        DWORD avail = 0;
                        if (!WinHttpQueryDataAvailable(req, &avail)) { ok = false; err = L"The connection was interrupted."; break; }
                        if (!avail) break;
                        size_t at = out.size();
                        out.resize(at + avail);
                        DWORD got = 0;
                        if (!WinHttpReadData(req, &out[at], avail, &got)) { ok = false; err = L"The connection was interrupted."; break; }
                        out.resize(at + got);
                        if (out.size() > cap) { ok = false; err = L"The file is larger than expected."; break; }
                        if (progress && !progress((long long)out.size(), total)) { ok = false; err = L"Stopped."; break; }
                    }
                }
            } else {
                DWORD e = GetLastError();
                err = L"No answer from " + std::wstring(host) + L" (Windows error " + std::to_wstring(e) + L").";
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(con);
    }
    WinHttpCloseHandle(ses);
    return ok;
}

struct CheckJob { HWND notify; bool manual; };

unsigned __stdcall CheckThread(void* p) {
    CheckJob* j = (CheckJob*)p;
    UpdateInfo* info = new UpdateInfo();
    std::wstring all;
    bool got = false;
    for (auto& s : Sources(UPDATE_LATEST_URL)) {
        std::string body;
        std::wstring err;
        if (Get(s, body, err, nullptr, 1 << 20)) {
            core::Latest l;
            if (core::ParseLatest(body, l)) { info->rel = l; info->via = s.host; got = true; break; }
            err = L"The version file is not valid.";
        }
        if (!all.empty()) all += L"\n";
        all += W(s.host) + L": " + err;
    }
    if (!got) info->problem = all;
    if (!PostMessageW(j->notify, WM_UPDATE_RESULT, j->manual ? 1 : 0, (LPARAM)info)) delete info;
    delete j;
    return 0;
}

}  // namespace

void UpdateCheckAsync(HWND notify, bool manual) {
    CheckJob* j = new CheckJob{notify, manual};
    uintptr_t t = _beginthreadex(nullptr, 0, CheckThread, j, 0, nullptr);
    if (t) CloseHandle((HANDLE)t); else delete j;
}

bool UpdateDownload(const std::string& url, std::string& out, std::wstring& err, const std::string& via, const std::function<bool(long long, long long)>& progress) {
    auto list = Sources(url);
    // try the server that answered the version check first
    std::stable_sort(list.begin(), list.end(), [&](const Source& a, const Source& b) { return (a.host == via) > (b.host == via); });
    std::wstring all;
    for (auto& s : list) {
        std::wstring e;
        if (Get(s, out, e, progress, 300u << 20)) return true;
        if (e == L"Stopped.") { err = e; return false; }
        if (!all.empty()) all += L"\n";
        all += W(s.host) + L": " + e;
    }
    err = all;
    return false;
}
