#pragma once
// Updates: the newest version is described by a small file in the public releases repository
// (akshsim1991/FeatherPDF-releases, latest.json); each app downloads it, compares the version with its own, and on
// "Update now" downloads its installer, checks it (SHA-256) and installs it. The downloading is each app's own
// (WinHTTP on Windows, NSURLSession on the Mac); what the file says and what to do with it is shared.
#include <string>
#include <vector>

namespace update {

// Where the version file is.
extern const char* const kLatestUrl;

struct Package {
    std::string url;      // the installer (Windows .exe) or disk image (Mac .dmg)
    std::string sha256;   // its SHA-256, 64 hex digits
    long long size = 0;   // bytes
};

struct Release {
    std::string version;     // "2.14.3"
    std::string date;        // "30 October 2026"
    std::wstring notes;      // what is new, a few lines
    Package windows, mac;
};

// Reads latest.json. False when it is not a version file.
bool parse(const std::string& json, Release& out);

// < 0 when a is older than b, 0 when the same, > 0 when newer ("2.14.10" is newer than "2.14.9").
int compareVersions(const std::string& a, const std::string& b);

// True when the downloaded bytes are the package (their SHA-256 matches).
bool verify(const std::vector<unsigned char>& bytes, const std::string& sha256);

} // namespace update
