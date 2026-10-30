#pragma once
// Checking for and downloading new versions on Windows (WinHTTP). What the version file says is shared/update.hpp.
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

namespace updater {

// Downloads `url` (https, redirects followed). `progress(done, total)` is called as it arrives (total 0 when unknown);
// returning false stops the download. False with a message in `error` when it did not succeed.
bool download(const std::string& url, std::vector<unsigned char>& out, std::wstring& error,
              const std::function<bool(long long done, long long total)>& progress = nullptr);

// Reads the version file on a background thread and posts `msg` to `wnd`: wParam = `manual`, lParam = a new
// update::Release* (the receiver deletes it), or 0 when it could not be read.
void checkInBackground(HWND wnd, UINT msg, bool manual);

} // namespace updater
