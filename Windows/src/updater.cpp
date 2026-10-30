#include "updater.hpp"
#include <winhttp.h>
#include <thread>
#include "platform.hpp"
#include "update.hpp"

namespace updater {

bool download(const std::string& url, std::vector<unsigned char>& out, std::wstring& error,
              const std::function<bool(long long, long long)>& progress) {
    out.clear();
    const std::wstring w = platform::wide(url);
    URL_COMPONENTS uc{sizeof(uc)};
    wchar_t host[256] = L"", path[2048] = L"";
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &uc)) { error = L"The download address is not valid."; return false; }
    HINTERNET ses = WinHttpOpen(L"Feather PDF", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!ses) { error = L"The network could not be used."; return false; }
    WinHttpSetTimeouts(ses, 10000, 10000, 15000, 30000);
    bool ok = false;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz, nullptr);
        wchar_t lenText[32] = L"";
        DWORD lenSize = sizeof(lenText);
        const long long total = WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, lenText, &lenSize, nullptr) ? _wtoi64(lenText) : 0;
        if (status == 200) {
            ok = true;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail)) { ok = false; error = L"The download stopped. Check the internet connection and try again."; break; }
                if (avail == 0) break;
                const size_t at = out.size();
                out.resize(at + avail);
                DWORD got = 0;
                if (!WinHttpReadData(req, out.data() + at, avail, &got)) { ok = false; error = L"The download stopped. Check the internet connection and try again."; break; }
                out.resize(at + got);
                if (progress && !progress(static_cast<long long>(out.size()), total)) { ok = false; error = L"The download was stopped."; break; }
            }
        } else error = L"The update server answered with an error (" + std::to_wstring(status) + L").";
    } else error = L"The update server could not be reached. Check the internet connection and try again.";
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

void checkInBackground(HWND wnd, UINT msg, bool manual) {
    std::thread([wnd, msg, manual] {
        std::vector<unsigned char> bytes;
        std::wstring err;
        update::Release* r = nullptr;
        if (download(update::kLatestUrl, bytes, err)) {
            r = new update::Release;
            if (!update::parse(std::string(bytes.begin(), bytes.end()), *r)) { delete r; r = nullptr; }
        }
        if (!PostMessageW(wnd, msg, manual ? 1 : 0, reinterpret_cast<LPARAM>(r))) delete r;
    }).detach();
}

} // namespace updater
