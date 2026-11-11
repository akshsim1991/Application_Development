// Feather PDF setup: two screens. The first shows where Feather PDF goes (with Change...), who made it, the build date and
// an Install button; the second shows the progress and, at the end, the result with two choices and Finish.
// The same program, copied next to the installed files as uninstall.exe (without the files packed into it), removes
// Feather PDF again. Switches: /S silent, /D=folder (silent install location), /uninstall; for an update started by
// Feather PDF itself: /waitfor=<process id> (wait until that Feather PDF has closed) and /relaunch (start it again after).
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <shldisp.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "inflate.hpp"

namespace {

const wchar_t* kApp = L"Feather PDF";
const wchar_t* kExe = L"FeatherPDF.exe";
const wchar_t* kPublisher = L"Akshaya Simha";
const wchar_t* kUninstallKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\FeatherPDF";
const wchar_t* kInnoKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{B6F5A1E2-7D43-4C8E-9A5B-FEA7E2C0D913}_is1";
const wchar_t* kFileList = L"installed-files.txt";
const wchar_t* kUninstaller = L"uninstall.exe";
// "Convert to PDF" in File Explorer's right-click menu, for the files Feather PDF can make into PDFs (shared/topdf.cpp:
// extensions(), Windows part). Under SystemFileAssociations, so it is offered whichever program opens those files.
const wchar_t* const kConvertTypes[] = {L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".txt", L".md", L".markdown"};
const wchar_t* kConvertVerb = L"\\shell\\FeatherPDF.ConvertToPDF";

// ---------------------------------------------------------------- small helpers
std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}
std::string narrow(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring parentOf(const std::wstring& p) {
    const size_t i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? L"" : p.substr(0, i);
}
std::wstring leafOf(const std::wstring& p) {
    const size_t i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? p : p.substr(i + 1);
}
std::wstring trimSlash(std::wstring p) {
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    return p;
}
bool fileExists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool dirExists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) r = p;
    CoTaskMemFree(p);
    return r;
}
std::wstring sizeText(unsigned long long bytes) {
    wchar_t t[64];
    if (bytes >= 1000ull * 1000 * 1000) swprintf(t, 64, L"%.1f GB", bytes / 1e9);
    else swprintf(t, 64, L"%.0f MB", std::max(1.0, bytes / 1e6));
    return t;
}
std::wstring lastErrorText(DWORD e) {
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring s = buf ? buf : L"";
    LocalFree(buf);
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ' || s.back() == L'.')) s.pop_back();
    return s.empty() ? L"error " + std::to_wstring(e) : s;
}
std::vector<int> versionParts(const std::wstring& v) {
    std::vector<int> r;
    size_t i = 0;
    while (i < v.size()) { r.push_back(_wtoi(v.c_str() + i)); i = v.find(L'.', i); if (i == std::wstring::npos) break; ++i; }
    while (r.size() < 4) r.push_back(0);
    return r;
}

// the log, %TEMP%\FeatherPDF-Setup.log: what was done, for when something goes wrong
FILE* g_log = nullptr;
void logLine(const std::wstring& s) {
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d %s\n", t.wHour, t.wMinute, t.wSecond, narrow(s).c_str());
    fflush(g_log);
}

// registry: 64-bit view, machine-wide
bool regSet(HKEY root, const std::wstring& key, const wchar_t* name, const std::wstring& value, DWORD type = REG_SZ) {
    HKEY k;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, name, 0, type, reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
bool regSetDword(HKEY root, const std::wstring& key, const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
bool regSetNone(HKEY root, const std::wstring& key, const wchar_t* name) {
    HKEY k;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, name, 0, REG_NONE, nullptr, 0);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
std::wstring regGet(HKEY root, const std::wstring& key, const wchar_t* name) {
    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key.c_str(), name, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buf, &size) != ERROR_SUCCESS) return L"";
    return buf;
}
void regDeleteTree(HKEY root, const std::wstring& key) {
    HKEY k;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
        RegDeleteTreeW(k, nullptr);
        RegCloseKey(k);
    }
    RegDeleteKeyExW(root, key.c_str(), KEY_WOW64_64KEY, 0);
}
void regDeleteValue(HKEY root, const std::wstring& key, const wchar_t* name) {
    HKEY k;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
        RegDeleteValueW(k, name);
        RegCloseKey(k);
    }
}

// ---------------------------------------------------------------- the packed files
struct Payload {
    struct Block { unsigned long long off; uint32_t clen, ulen; };
    struct File { std::wstring path; unsigned long long size; uint32_t crc; std::vector<Block> blocks; };
    std::wstring version, buildDate, buildIso;
    std::vector<File> files;
    unsigned long long stubSize = 0, dataStart = 0, total = 0;
    std::wstring selfPath;
    bool present = false;
} g_pay;

bool readAt(HANDLE h, unsigned long long off, void* buf, DWORD len) {
    LARGE_INTEGER li; li.QuadPart = static_cast<LONGLONG>(off);
    DWORD got = 0;
    return SetFilePointerEx(h, li, nullptr, FILE_BEGIN) && ReadFile(h, buf, len, &got, nullptr) && got == len;
}

// Reads the list of packed files at the end of this program. False when there is none (the uninstaller) or it is damaged.
bool loadPayload(std::wstring& error) {
    wchar_t self[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
    g_pay.selfPath = self;
    HANDLE h = CreateFileW(self, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) { error = L"The setup program could not read itself (" + lastErrorText(GetLastError()) + L")."; return false; }
    LARGE_INTEGER size; GetFileSizeEx(h, &size);
    char tail[16];
    bool ok = size.QuadPart > 16 && readAt(h, static_cast<unsigned long long>(size.QuadPart) - 16, tail, 16) && memcmp(tail + 8, "FPDFEND1", 8) == 0;
    if (!ok) { CloseHandle(h); g_pay.stubSize = static_cast<unsigned long long>(size.QuadPart); return false; }   // no files: the uninstaller
    memcpy(&g_pay.stubSize, tail, 8);
    const unsigned long long headEnd = static_cast<unsigned long long>(size.QuadPart) - 16;
    std::vector<char> head(static_cast<size_t>(std::min<unsigned long long>(headEnd - g_pay.stubSize, 4u << 20)));
    if (!readAt(h, g_pay.stubSize, head.data(), static_cast<DWORD>(head.size())) || memcmp(head.data(), "FPDFSET1", 8) != 0) {
        CloseHandle(h);
        error = L"The setup program is damaged (download it again).";
        return false;
    }
    size_t p = 8;
    bool bad = false;
    auto need = [&](size_t n) { if (p + n > head.size()) { bad = true; return false; } return true; };
    auto u32 = [&]() -> uint32_t { if (!need(4)) return 0; uint32_t v; memcpy(&v, head.data() + p, 4); p += 4; return v; };
    auto u64 = [&]() -> unsigned long long { if (!need(8)) return 0; unsigned long long v; memcpy(&v, head.data() + p, 8); p += 8; return v; };
    auto str = [&]() -> std::wstring { const uint32_t n = u32(); if (!need(n)) return L""; std::string s(head.data() + p, n); p += n; return widen(s); };
    g_pay.version = str(); g_pay.buildDate = str(); g_pay.buildIso = str();
    const uint32_t count = u32();
    for (uint32_t i = 0; i < count && !bad; ++i) {
        Payload::File f;
        f.path = str(); f.size = u64(); f.crc = u32();
        const uint32_t nb = u32();
        for (uint32_t b = 0; b < nb && !bad; ++b) { Payload::Block bl; bl.off = u64(); bl.clen = u32(); bl.ulen = u32(); f.blocks.push_back(bl); }
        g_pay.total += f.size;
        g_pay.files.push_back(std::move(f));
    }
    CloseHandle(h);
    if (bad || g_pay.files.empty()) { error = L"The setup program is damaged (download it again)."; return false; }
    g_pay.dataStart = g_pay.stubSize + p;
    g_pay.present = true;
    return true;
}

// ---------------------------------------------------------------- what is installed already
struct Existing {
    bool ours = false;            // installed by this setup (Uninstall\FeatherPDF)
    std::wstring dir, version;
    bool inno = false;            // installed by the earlier (Inno Setup) installer
    std::wstring innoUninstall, innoDir;
    HKEY innoRoot = nullptr;
} g_old;

void findExisting() {
    g_old = Existing{};
    g_old.dir = trimSlash(regGet(HKEY_LOCAL_MACHINE, kUninstallKey, L"InstallLocation"));
    if (!g_old.dir.empty() && fileExists(g_old.dir + L"\\" + kExe)) {
        g_old.ours = true;
        g_old.version = regGet(HKEY_LOCAL_MACHINE, kUninstallKey, L"DisplayVersion");
    } else g_old.dir.clear();
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        const std::wstring u = regGet(root, kInnoKey, L"UninstallString");
        if (u.empty()) continue;
        g_old.inno = true;
        g_old.innoUninstall = u;
        g_old.innoDir = trimSlash(regGet(root, kInnoKey, L"InstallLocation"));
        g_old.innoRoot = root;
        if (!g_old.ours && g_old.version.empty()) g_old.version = regGet(root, kInnoKey, L"DisplayVersion");
        break;
    }
}

// ---------------------------------------------------------------- the work (in a thread, so the window stays live)
struct Progress {
    std::mutex m;
    std::wstring activity, error;
    std::atomic<int> permille{0};
    std::atomic<bool> cancel{false};
    HWND notify = nullptr;
};
Progress g_prog;
enum { WM_APP_PROGRESS = WM_APP + 1, WM_APP_DONE };

void say(const std::wstring& what) {
    { std::lock_guard<std::mutex> l(g_prog.m); g_prog.activity = what; }
    logLine(what);
    if (g_prog.notify) PostMessageW(g_prog.notify, WM_APP_PROGRESS, 0, 0);
}
void advance(int permille) {
    g_prog.permille = std::clamp(permille, 0, 1000);
    if (g_prog.notify) PostMessageW(g_prog.notify, WM_APP_PROGRESS, 0, 0);
}

bool makeDirs(const std::wstring& dir) {
    if (dir.empty() || dirExists(dir)) return true;
    if (!makeDirs(parentOf(dir))) return false;
    return CreateDirectoryW(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool isFeatherRunning() { return FindWindowW(L"FeatherPdfMain", nullptr) != nullptr; }

// Asks every Feather PDF window to close (each asks about its unsaved changes, as when closed by hand). Since 2.15
// every separate window is a program of its own.
void closeFeatherWindows() {
    EnumWindows([](HWND w, LPARAM) -> BOOL {
        wchar_t cls[64] = L"";
        if (GetClassNameW(w, cls, 64) && wcscmp(cls, L"FeatherPdfMain") == 0) PostMessageW(w, WM_CLOSE, 0, 0);
        return TRUE;
    }, 0);
}

bool createShortcut(const std::wstring& lnk, const std::wstring& target, const std::wstring& args, const std::wstring& dir, const std::wstring& desc) {
    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&sl)))) return false;
    sl->SetPath(target.c_str());
    if (!args.empty()) sl->SetArguments(args.c_str());
    sl->SetWorkingDirectory(dir.c_str());
    sl->SetDescription(desc.c_str());
    sl->SetIconLocation((dir + L"\\" + kExe).c_str(), 0);
    IPersistFile* pf = nullptr;
    bool ok = false;
    if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&pf)))) {
        ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
        pf->Release();
    }
    sl->Release();
    return ok;
}

std::vector<std::wstring> readList(const std::wstring& file) {
    std::vector<std::wstring> out;
    HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return out;
    LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
    std::string data(static_cast<size_t>(std::min<LONGLONG>(sz.QuadPart, 1 << 20)), '\0');
    DWORD got = 0;
    if (!data.empty()) ReadFile(h, &data[0], static_cast<DWORD>(data.size()), &got, nullptr);
    CloseHandle(h);
    data.resize(got);
    size_t i = 0;
    while (i < data.size()) {
        size_t e = data.find('\n', i);
        if (e == std::string::npos) e = data.size();
        std::string line = data.substr(i, e - i);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty() && line.find("..") == std::string::npos && line[0] != '\\' && line.find(':') == std::string::npos) out.push_back(widen(line));
        i = e + 1;
    }
    return out;
}

bool writeFileBytes(const std::wstring& path, const void* data, size_t len) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD put = 0;
    const bool ok = len == 0 || (WriteFile(h, data, static_cast<DWORD>(len), &put, nullptr) && put == len);
    CloseHandle(h);
    return ok;
}

// Removes the earlier installation made by the Inno Setup installer (it would otherwise stay as a second copy).
void removeInnoInstall() {
    if (!g_old.inno) return;
    say(L"Removing the earlier installation of Feather PDF");
    std::wstring cmd = g_old.innoUninstall + L" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 120000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        // the Inno uninstaller finishes in a copy of itself: wait until its entry is gone
        for (int i = 0; i < 240 && !regGet(g_old.innoRoot, kInnoKey, L"UninstallString").empty(); ++i) { Sleep(250); advance(std::min(30, i / 2)); }
        logLine(L"earlier installation removed: " + std::wstring(regGet(g_old.innoRoot, kInnoKey, L"UninstallString").empty() ? L"yes" : L"no"));
    } else logLine(L"could not start the earlier uninstaller: " + lastErrorText(GetLastError()));
    // what that uninstaller leaves when its own clean-up does not finish: its files, and Feather PDF's licence and
    // language folders (only Feather PDF's own files go; anything else in the folder stays, and so does the folder)
    const std::wstring d = g_old.innoDir;
    if (d.empty() || !dirExists(d)) return;
    for (int attempt = 0; attempt < 20; ++attempt) {
        bool left = false;
        for (const wchar_t* n : {L"unins000.exe", L"unins000.dat", L"unins000.msg", kExe, L"pdfium.dll", L"Feather PDF User Guide.pdf", L"THIRD_PARTY_NOTICES.md"}) {
            const std::wstring f = d + L"\\" + n;
            if (fileExists(f) && !DeleteFileW(f.c_str())) left = true;
        }
        for (const wchar_t* sub : {L"licenses", L"tessdata"}) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((d + L"\\" + sub + L"\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !DeleteFileW((d + L"\\" + sub + L"\\" + fd.cFileName).c_str())) left = true; } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
            RemoveDirectoryW((d + L"\\" + sub).c_str());
        }
        if (!left && (RemoveDirectoryW(d.c_str()) || !dirExists(d))) { logLine(L"removed the earlier folder " + d); return; }
        if (!left && attempt == 19) break;
        Sleep(250);   // the uninstaller's copy may still be ending (and holding the folder)
    }
    if (dirExists(d)) MoveFileExW(d.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);   // empty or not, Windows removes it only if empty
}

// The installation. True when everything is in place; else g_prog.error says what went wrong and nothing changed
// (files already copied for this attempt are removed again, the files that were there stay).
bool installWork(const std::wstring& dirIn) {
    const std::wstring dir = trimSlash(dirIn);
    const bool existed = dirExists(dir);
    auto fail = [&](const std::wstring& e) {
        if (!existed) RemoveDirectoryW(dir.c_str());   // a folder made for this attempt goes again (only if empty)
        std::lock_guard<std::mutex> l(g_prog.m); g_prog.error = e; logLine(L"FAILED: " + e); return false;
    };
    logLine(L"install " + g_pay.version + L" to " + dir);
    advance(0);
    removeInnoInstall();
    advance(40);
    say(L"Preparing " + dir);
    if (!makeDirs(dir)) return fail(L"The folder \"" + dir + L"\" could not be created (" + lastErrorText(GetLastError()) + L"). Choose another place with Change\x2026");
    {   // can we write there?
        const std::wstring probe = dir + L"\\.feather-write-test";
        if (!writeFileBytes(probe, "x", 1)) return fail(L"Feather PDF cannot write to \"" + dir + L"\" (" + lastErrorText(GetLastError()) + L"). Choose another folder.");
        DeleteFileW(probe.c_str());
    }
    {   // enough room? (checked again: other programs may have filled the disk meanwhile)
        ULARGE_INTEGER freeBytes{};
        if (GetDiskFreeSpaceExW(dir.c_str(), &freeBytes, nullptr, nullptr) && freeBytes.QuadPart < g_pay.total + g_pay.stubSize + (8u << 20))
            return fail(L"There is not enough free space: Feather PDF needs " + sizeText(g_pay.total + g_pay.stubSize) + L" and the disk has " + sizeText(freeBytes.QuadPart) + L" free.");
    }
    HANDLE src = CreateFileW(g_pay.selfPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (src == INVALID_HANDLE_VALUE) return fail(L"The setup program could not read its own files (" + lastErrorText(GetLastError()) + L").");
    // 1. every file is unpacked next to its place as "name.feathernew" and checked
    std::vector<std::wstring> staged;
    auto unstage = [&]() { for (const auto& s : staged) DeleteFileW(s.c_str()); };
    unsigned long long done = 0;
    std::vector<uint8_t> packed, plain;
    for (const auto& f : g_pay.files) {
        if (g_prog.cancel) { CloseHandle(src); unstage(); return fail(L"The installation was cancelled. Nothing was changed."); }
        const std::wstring target = dir + L"\\" + f.path;
        say(L"Copying " + f.path);
        if (!makeDirs(parentOf(target))) { CloseHandle(src); unstage(); return fail(L"The folder \"" + parentOf(target) + L"\" could not be created (" + lastErrorText(GetLastError()) + L")."); }
        const std::wstring tmp = target + L".feathernew";
        HANDLE out = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) { const DWORD e = GetLastError(); CloseHandle(src); unstage(); return fail(L"\"" + target + L"\" could not be written (" + lastErrorText(e) + L")."); }
        staged.push_back(tmp);
        uint32_t crc = 0;
        for (const auto& b : f.blocks) {
            packed.resize(b.clen);
            plain.clear();
            plain.reserve(b.ulen);
            if (!readAt(src, g_pay.dataStart + b.off, packed.data(), b.clen) || !inflate::raw(packed.data(), packed.size(), plain) || plain.size() != b.ulen) {
                CloseHandle(out); CloseHandle(src); unstage();
                return fail(L"The setup program is damaged (download it again).");
            }
            crc = inflate::crc32(plain.data(), plain.size(), crc);
            DWORD put = 0;
            if (!plain.empty() && (!WriteFile(out, plain.data(), static_cast<DWORD>(plain.size()), &put, nullptr) || put != plain.size())) {
                const DWORD e = GetLastError();
                CloseHandle(out); CloseHandle(src); unstage();
                return fail(e == ERROR_DISK_FULL || e == ERROR_HANDLE_DISK_FULL ? L"The disk is full. Free some space and try again."
                                                                                : L"\"" + target + L"\" could not be written (" + lastErrorText(e) + L").");
            }
            done += plain.size();
            advance(40 + static_cast<int>(820 * done / std::max(1ull, g_pay.total)));
        }
        CloseHandle(out);
        if (crc != f.crc) { CloseHandle(src); unstage(); return fail(L"The setup program is damaged (download it again)."); }
    }
    CloseHandle(src);
    // 2. the new files take the old ones' places (the old ones are kept aside until all are in)
    say(L"Putting the files in place");
    struct Swap { std::wstring target, aside; bool hadOld; };
    std::vector<Swap> swaps;
    auto rollback = [&]() {
        for (auto it = swaps.rbegin(); it != swaps.rend(); ++it) {
            DeleteFileW(it->target.c_str());
            if (it->hadOld) MoveFileExW(it->aside.c_str(), it->target.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
        unstage();
    };
    for (const auto& f : g_pay.files) {
        const std::wstring target = dir + L"\\" + f.path, tmp = target + L".feathernew", aside = target + L".featherold";
        Swap sw{target, aside, fileExists(target)};
        if (sw.hadOld) {
            DeleteFileW(aside.c_str());
            bool moved = false;
            for (int i = 0; i < 20 && !(moved = MoveFileExW(target.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING)); ++i) Sleep(150);
            if (!moved) {
                const DWORD e = GetLastError();
                rollback();
                return fail(L"\"" + target + L"\" is in use and could not be replaced (" + lastErrorText(e) + L"). Close Feather PDF and try again.");
            }
        }
        if (!MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            const DWORD e = GetLastError();
            if (sw.hadOld) MoveFileExW(aside.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
            rollback();
            return fail(L"\"" + target + L"\" could not be put in place (" + lastErrorText(e) + L").");
        }
        swaps.push_back(sw);
    }
    for (const auto& sw : swaps) if (sw.hadOld) { if (!DeleteFileW(sw.aside.c_str())) MoveFileExW(sw.aside.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT); }
    advance(890);
    // 3. files of the version before that this one no longer has
    {
        std::vector<std::wstring> now;
        for (const auto& f : g_pay.files) now.push_back(f.path);
        for (const auto& old : readList(dir + L"\\" + kFileList)) {
            bool kept = false;
            for (const auto& n : now) if (_wcsicmp(n.c_str(), old.c_str()) == 0) kept = true;
            if (!kept && _wcsicmp(old.c_str(), kUninstaller) != 0) { DeleteFileW((dir + L"\\" + old).c_str()); logLine(L"removed old file " + old); }
        }
    }
    // 4. the uninstaller (this program without the packed files) and the list of what was installed
    say(L"Writing the uninstaller");
    {
        HANDLE in = CreateFileW(g_pay.selfPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        std::vector<char> stub(static_cast<size_t>(g_pay.stubSize));
        const bool ok = in != INVALID_HANDLE_VALUE && readAt(in, 0, stub.data(), static_cast<DWORD>(stub.size()));
        if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
        if (!ok || !writeFileBytes(dir + L"\\" + kUninstaller, stub.data(), stub.size())) logLine(L"the uninstaller could not be written");
        std::string list;
        for (const auto& f : g_pay.files) list += narrow(f.path) + "\r\n";
        list += narrow(kUninstaller) + "\r\n";
        writeFileBytes(dir + L"\\" + kFileList, list.data(), list.size());
    }
    advance(920);
    // 5. Start menu shortcuts (for every user)
    say(L"Creating the Start menu shortcuts");
    const std::wstring exe = dir + L"\\" + kExe;
    {
        const std::wstring programs = knownFolder(FOLDERID_CommonPrograms);
        if (!programs.empty()) {
            if (!createShortcut(programs + L"\\Feather PDF.lnk", exe, L"", dir, L"Read and edit PDF documents")) logLine(L"Start menu shortcut failed");
            createShortcut(programs + L"\\Feather PDF User Guide.lnk", exe, L"\"" + dir + L"\\Feather PDF User Guide.pdf\"", dir, L"The Feather PDF user guide");
        }
    }
    advance(950);
    // 6. Installed apps, "Open with" and the Default apps page of Windows
    say(L"Registering Feather PDF with Windows");
    {
        const HKEY M = HKEY_LOCAL_MACHINE;
        const std::wstring cmd = L"\"" + exe + L"\" \"%1\"";
        regSet(M, kUninstallKey, L"DisplayName", kApp);
        regSet(M, kUninstallKey, L"DisplayVersion", g_pay.version);
        regSet(M, kUninstallKey, L"Publisher", kPublisher);
        regSet(M, kUninstallKey, L"DisplayIcon", exe + L",0");
        regSet(M, kUninstallKey, L"InstallLocation", dir);
        regSet(M, kUninstallKey, L"UninstallString", L"\"" + dir + L"\\" + kUninstaller + L"\" /uninstall");
        regSet(M, kUninstallKey, L"QuietUninstallString", L"\"" + dir + L"\\" + kUninstaller + L"\" /uninstall /S");
        regSet(M, kUninstallKey, L"Comments", L"Free PDF reader and editor. Built " + g_pay.buildDate + L".");
        regSet(M, kUninstallKey, L"BuildDate", g_pay.buildDate);
        wchar_t date[16]; SYSTEMTIME t; GetLocalTime(&t); swprintf(date, 16, L"%04d%02d%02d", t.wYear, t.wMonth, t.wDay);
        regSet(M, kUninstallKey, L"InstallDate", date);
        regSetDword(M, kUninstallKey, L"EstimatedSize", static_cast<DWORD>((g_pay.total + g_pay.stubSize) / 1024));
        regSetDword(M, kUninstallKey, L"NoModify", 1);
        regSetDword(M, kUninstallKey, L"NoRepair", 1);
        const std::wstring cls = L"SOFTWARE\\Classes\\";
        regSet(M, cls + L"FeatherPDF.Document", nullptr, L"PDF document");
        regSet(M, cls + L"FeatherPDF.Document\\DefaultIcon", nullptr, exe + L",0");
        regSet(M, cls + L"FeatherPDF.Document\\shell\\open\\command", nullptr, cmd);
        regSetNone(M, cls + L".pdf\\OpenWithProgids", L"FeatherPDF.Document");
        regSet(M, cls + L"Applications\\FeatherPDF.exe", L"FriendlyAppName", kApp);
        regSet(M, cls + L"Applications\\FeatherPDF.exe\\SupportedTypes", L".pdf", L"");
        regSet(M, cls + L"Applications\\FeatherPDF.exe\\shell\\open\\command", nullptr, cmd);
        regSet(M, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\FeatherPDF.exe", nullptr, exe);
        regSet(M, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\FeatherPDF.exe", L"Path", dir);
        regSet(M, L"SOFTWARE\\FeatherPDF\\Capabilities", L"ApplicationName", kApp);
        regSet(M, L"SOFTWARE\\FeatherPDF\\Capabilities", L"ApplicationDescription", L"A fast, free PDF reader and editor.");
        regSet(M, L"SOFTWARE\\FeatherPDF\\Capabilities", L"ApplicationIcon", exe + L",0");
        regSet(M, L"SOFTWARE\\FeatherPDF\\Capabilities\\FileAssociations", L".pdf", L"FeatherPDF.Document");
        regSet(M, L"SOFTWARE\\RegisteredApplications", kApp, L"SOFTWARE\\FeatherPDF\\Capabilities");
        for (const wchar_t* t : kConvertTypes) {
            const std::wstring verb = cls + L"SystemFileAssociations\\" + t + kConvertVerb;
            regSet(M, verb, nullptr, L"Convert to PDF");
            regSet(M, verb, L"Icon", exe + L",0");
            regSet(M, verb + L"\\command", nullptr, L"\"" + exe + L"\" --convert-open \"%1\"");
        }
        logLine(L"Convert to PDF added to the right-click menu");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    advance(1000);
    say(L"Done");
    logLine(L"installed");
    return true;
}

// Removes Feather PDF from `dir`. Settings and recent files (in each user's AppData) are kept.
bool uninstallWork(const std::wstring& dirIn) {
    const std::wstring dir = trimSlash(dirIn);
    auto fail = [](const std::wstring& e) { std::lock_guard<std::mutex> l(g_prog.m); g_prog.error = e; logLine(L"FAILED: " + e); return false; };
    logLine(L"uninstall from " + dir);
    if (dir.empty() || !fileExists(dir + L"\\" + kExe)) {
        // nothing there: only the Windows entries are left to remove
        logLine(L"no program files found in " + dir);
    }
    std::vector<std::wstring> list = readList(dir + L"\\" + kFileList);
    if (list.empty()) list = {kExe, L"pdfium.dll", L"Feather PDF User Guide.pdf", L"THIRD_PARTY_NOTICES.md", kUninstaller};
    say(L"Removing the program files");
    int i = 0;
    std::vector<std::wstring> busy;
    for (const auto& rel : list) {
        const std::wstring p = dir + L"\\" + rel;
        if (fileExists(p) && !DeleteFileW(p.c_str())) {
            bool gone = false;
            for (int k = 0; k < 10 && !(gone = DeleteFileW(p.c_str())); ++k) Sleep(200);
            if (!gone) { busy.push_back(rel); MoveFileExW(p.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT); }
        }
        advance(50 + 700 * ++i / static_cast<int>(list.size()));
    }
    DeleteFileW((dir + L"\\" + kFileList).c_str());
    // the folders, now empty (deepest first); a folder with other files in it stays
    std::vector<std::wstring> dirs;
    for (const auto& rel : list) for (std::wstring d = parentOf(rel); !d.empty(); d = parentOf(d)) dirs.push_back(dir + L"\\" + d);
    std::sort(dirs.begin(), dirs.end(), [](const std::wstring& a, const std::wstring& b) { return a.size() > b.size(); });
    for (const auto& d : dirs) RemoveDirectoryW(d.c_str());
    if (!RemoveDirectoryW(dir.c_str()) && dirExists(dir)) MoveFileExW(dir.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    advance(800);
    say(L"Removing the shortcuts and the Windows entries");
    const std::wstring programs = knownFolder(FOLDERID_CommonPrograms);
    DeleteFileW((programs + L"\\Feather PDF.lnk").c_str());
    DeleteFileW((programs + L"\\Feather PDF User Guide.lnk").c_str());
    const HKEY M = HKEY_LOCAL_MACHINE;
    regDeleteTree(M, kUninstallKey);
    regDeleteTree(M, L"SOFTWARE\\Classes\\FeatherPDF.Document");
    regDeleteValue(M, L"SOFTWARE\\Classes\\.pdf\\OpenWithProgids", L"FeatherPDF.Document");
    regDeleteTree(M, L"SOFTWARE\\Classes\\Applications\\FeatherPDF.exe");
    regDeleteTree(M, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\FeatherPDF.exe");
    regDeleteValue(M, L"SOFTWARE\\RegisteredApplications", kApp);
    regDeleteTree(M, L"SOFTWARE\\FeatherPDF");
    for (const wchar_t* t : kConvertTypes) regDeleteTree(M, std::wstring(L"SOFTWARE\\Classes\\SystemFileAssociations\\") + t + kConvertVerb);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    advance(1000);
    if (!busy.empty()) {
        std::wstring names;
        for (const auto& b : busy) names += L"\n" + b;
        return fail(L"Feather PDF was removed, but some files were in use and go when Windows restarts:" + names);
    }
    say(L"Done");
    logLine(L"uninstalled");
    return true;
}

// ---------------------------------------------------------------- starting things as the signed-in user
// Setup runs as administrator; Feather PDF itself must not (dragging files from Explorer onto an administrator's window
// does not work). The shell (Explorer) starts it for us, as the user; when that is not possible, it is started directly.
bool runAsUser(const std::wstring& file, const std::wstring& args) {
    bool ok = false;
    IShellWindows* sw = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&sw)))) {
        VARIANT loc; VariantInit(&loc); loc.vt = VT_I4; loc.lVal = CSIDL_DESKTOP;
        VARIANT empty; VariantInit(&empty);
        long hwnd = 0;
        IDispatch* disp = nullptr;
        if (SUCCEEDED(sw->FindWindowSW(&loc, &empty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &disp)) && disp) {
            IServiceProvider* sp = nullptr;
            if (SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&sp)))) {
                IShellBrowser* sb = nullptr;
                if (SUCCEEDED(sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&sb)))) {
                    IShellView* sv = nullptr;
                    if (SUCCEEDED(sb->QueryActiveShellView(&sv))) {
                        IDispatch* bg = nullptr;
                        if (SUCCEEDED(sv->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&bg)))) {
                            IShellFolderViewDual* fv = nullptr;
                            if (SUCCEEDED(bg->QueryInterface(IID_PPV_ARGS(&fv)))) {
                                IDispatch* appDisp = nullptr;
                                if (SUCCEEDED(fv->get_Application(&appDisp))) {
                                    IShellDispatch2* sd = nullptr;
                                    if (SUCCEEDED(appDisp->QueryInterface(IID_PPV_ARGS(&sd)))) {
                                        BSTR f = SysAllocString(file.c_str());
                                        VARIANT a; VariantInit(&a); a.vt = VT_BSTR; a.bstrVal = SysAllocString(args.c_str());
                                        VARIANT dirV; VariantInit(&dirV); dirV.vt = VT_BSTR; dirV.bstrVal = SysAllocString(parentOf(file).c_str());
                                        VARIANT op; VariantInit(&op); op.vt = VT_BSTR; op.bstrVal = SysAllocString(L"open");
                                        VARIANT show; VariantInit(&show); show.vt = VT_I4; show.lVal = SW_SHOWNORMAL;
                                        ok = SUCCEEDED(sd->ShellExecute(f, a, dirV, op, show));
                                        SysFreeString(f); VariantClear(&a); VariantClear(&dirV); VariantClear(&op);
                                        sd->Release();
                                    }
                                    appDisp->Release();
                                }
                                fv->Release();
                            }
                            bg->Release();
                        }
                        sv->Release();
                    }
                    sb->Release();
                }
                sp->Release();
            }
            disp->Release();
        }
        sw->Release();
    }
    if (!ok) ok = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", file.c_str(), args.empty() ? nullptr : args.c_str(), parentOf(file).c_str(), SW_SHOWNORMAL)) > 32;
    logLine(L"started " + file + (ok ? L"" : L" (failed)"));
    return ok;
}

// Windows lets only the user choose the default app: its own Default apps page is opened, at Feather PDF where it can
// (Windows 11), else at the list of apps.
void openDefaultApps() {
    const wchar_t* pages[] = {L"ms-settings:defaultapps?registeredAppMachine=Feather%20PDF", L"ms-settings:defaultapps"};
    for (const wchar_t* p : pages)
        if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", p, nullptr, nullptr, SW_SHOWNORMAL)) > 32) { logLine(std::wstring(L"opened ") + p); return; }
    logLine(L"the Default apps page could not be opened");
}

// ---------------------------------------------------------------- the window
struct Colours { COLORREF bg, panel, field, line, text, dim, accent, accentHot, accentText, ok, bad; };
const Colours C{RGB(0x16, 0x17, 0x1A), RGB(0x1E, 0x20, 0x24), RGB(0x25, 0x27, 0x2C), RGB(0x36, 0x39, 0x3F), RGB(0xF1, 0xF2, 0xF4),
                RGB(0xAC, 0xB1, 0xB9), RGB(0x25, 0x63, 0xEB), RGB(0x3B, 0x7B, 0xF5), RGB(0xFF, 0xFF, 0xFF), RGB(0x3F, 0xB9, 0x50), RGB(0xF8, 0x51, 0x49)};

enum Page { P_START, P_WORK, P_DONE, P_FAILED };
enum { ID_CHANGE = 10, ID_INSTALL, ID_DEFAULT, ID_LAUNCH, ID_FINISH, ID_RETRY, ID_CLOSE, ID_CANCEL, ID_PATH };

struct Ui {
    HWND wnd = nullptr;
    HINSTANCE inst = nullptr;
    int dpi = 96;
    bool uninstall = false;
    Page page = P_START;
    std::wstring dir;
    bool locked = false;          // the location is that of the installed copy (updates and repairs go there)
    std::wstring status;          // what is installed already, shown above the location
    HFONT title = nullptr, head = nullptr, body = nullptr, small = nullptr, bold = nullptr;
    HICON icon = nullptr;
    HWND path = nullptr;
    std::vector<HWND> buttons;
    HWND hover = nullptr;
    bool setDefault = false, launch = true;
    unsigned long long needed = 0, freeBytes = 0;
    bool enoughSpace = true;
    std::thread worker;
    bool closing = false;
    int px(int v) const { return MulDiv(v, dpi, 96); }
} g_ui;

std::wstring actionWord() {
    if (g_ui.uninstall) return L"Uninstall";
    if (!g_old.ours) return L"Install";   // nothing, or only the earlier installer's copy (it is replaced by a new installation)
    const auto a = versionParts(g_pay.version), b = versionParts(g_old.version);
    if (a == b) return L"Repair";
    return L"Update";
}

void updateSpace() {
    g_ui.needed = g_pay.total + g_pay.stubSize;
    std::wstring probe = g_ui.dir;
    while (!probe.empty() && !dirExists(probe)) probe = parentOf(probe);
    ULARGE_INTEGER fb{};
    g_ui.freeBytes = (!probe.empty() && GetDiskFreeSpaceExW((probe + L"\\").c_str(), &fb, nullptr, nullptr)) ? fb.QuadPart : 0;
    // what is replaced frees its own room
    unsigned long long existing = 0;
    if (g_old.ours && _wcsicmp(g_old.dir.c_str(), g_ui.dir.c_str()) == 0) existing = g_ui.needed;
    g_ui.enoughSpace = g_ui.freeBytes == 0 || g_ui.freeBytes + existing >= g_ui.needed + (8u << 20);
}

HFONT makeFont(int pt10, int weight) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(pt10, g_ui.dpi, 720);
    lf.lfWeight = weight;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy(lf.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&lf);
}

void fill(HDC dc, const RECT& r, COLORREF c) { HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }

// a rounded rectangle, anti-aliased
void rounded(HDC dc, const RECT& r, int radius, COLORREF fillC, COLORREF border = CLR_INVALID, float borderW = 1.0f) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    // inside the rectangle, border included
    const float x = static_cast<float>(r.left) + 0.5f, y = static_cast<float>(r.top) + 0.5f, w = static_cast<float>(r.right - r.left) - 1.0f, h = static_cast<float>(r.bottom - r.top) - 1.0f;
    const float d = std::min(static_cast<float>(radius) * 2, std::min(w, h));
    Gdiplus::GraphicsPath p;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
    Gdiplus::SolidBrush b(Gdiplus::Color(255, GetRValue(fillC), GetGValue(fillC), GetBValue(fillC)));
    g.FillPath(&b, &p);
    if (border != CLR_INVALID) {
        Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(border), GetGValue(border), GetBValue(border)), borderW);
        g.DrawPath(&pen, &p);
    }
}

void text(HDC dc, const std::wstring& s, RECT r, HFONT f, COLORREF c, UINT fmt = DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), -1, &r, fmt);
}

RECT clientRect() { RECT rc; GetClientRect(g_ui.wnd, &rc); return rc; }
RECT progressRect() { const RECT rc = clientRect(); return {g_ui.px(32), g_ui.px(170), rc.right - g_ui.px(32), g_ui.px(180)}; }

void paint(HDC dc) {
    const RECT rc = clientRect();
    fill(dc, rc, C.bg);
    // header: the icon, the name, the version
    DrawIconEx(dc, g_ui.px(32), g_ui.px(26), g_ui.icon, g_ui.px(48), g_ui.px(48), 0, nullptr, DI_NORMAL);
    text(dc, kApp, {g_ui.px(96), g_ui.px(22), rc.right - g_ui.px(32), g_ui.px(56)}, g_ui.title, C.text, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    std::wstring sub = L"Version " + g_pay.version + L"  \x00B7  Free PDF reader and editor";
    if (g_ui.uninstall) sub = (g_pay.version.empty() ? std::wstring() : L"Version " + g_pay.version + L"  \x00B7  ") + L"Uninstall";
    text(dc, sub, {g_ui.px(96), g_ui.px(56), rc.right - g_ui.px(32), g_ui.px(80)}, g_ui.body, C.dim, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    RECT line{0, g_ui.px(100), rc.right, g_ui.px(101)};
    fill(dc, line, C.line);
    const int L = g_ui.px(32), R = rc.right - g_ui.px(32);
    if (g_ui.page == P_START) {
        int y = g_ui.px(118);
        if (!g_ui.status.empty()) {
            text(dc, g_ui.status, {L, y, R, y + g_ui.px(40)}, g_ui.body, C.text);
            y += g_ui.px(40);
        }
        if (!g_ui.uninstall) {
            text(dc, L"Install location", {L, y, R, y + g_ui.px(20)}, g_ui.small, C.dim, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
            RECT box{L, y + g_ui.px(22), R - g_ui.px(118), y + g_ui.px(60)};
            rounded(dc, box, g_ui.px(8), C.field, C.line);
            const std::wstring space = L"Needs " + sizeText(g_ui.needed) + (g_ui.freeBytes ? L"  \x00B7  " + sizeText(g_ui.freeBytes) + L" free on this drive" : L"");
            text(dc, g_ui.enoughSpace ? space : L"Not enough free space here: Feather PDF needs " + sizeText(g_ui.needed) + L" and the drive has " + sizeText(g_ui.freeBytes) + L" free. Choose another place or free some space.",
                 {L, y + g_ui.px(68), R, y + g_ui.px(110)}, g_ui.small, g_ui.enoughSpace ? C.dim : C.bad);
        } else {
            text(dc, L"Feather PDF will be removed from:", {L, y, R, y + g_ui.px(20)}, g_ui.body, C.text, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
            text(dc, g_ui.dir, {L, y + g_ui.px(24), R, y + g_ui.px(48)}, g_ui.body, C.dim, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS);
            text(dc, L"Your settings and recent files are kept.", {L, y + g_ui.px(58), R, y + g_ui.px(80)}, g_ui.small, C.dim, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        }
        // who made it and when
        text(dc, L"Developed by Akshaya Simha", {L, rc.bottom - g_ui.px(80), R - g_ui.px(200), rc.bottom - g_ui.px(58)}, g_ui.body, C.text, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        if (!g_pay.buildDate.empty()) text(dc, L"Build Date: " + g_pay.buildDate, {L, rc.bottom - g_ui.px(56), R - g_ui.px(200), rc.bottom - g_ui.px(34)}, g_ui.small, C.dim, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }
    // the second screen: what is happening, the bar, the result
    std::wstring headText, activity, error;
    { std::lock_guard<std::mutex> l(g_prog.m); activity = g_prog.activity; error = g_prog.error; }
    COLORREF headColour = C.text;
    const bool un = g_ui.uninstall;
    if (g_ui.page == P_WORK) headText = un ? L"Removing Feather PDF\x2026" : (actionWord() == L"Install" ? L"Installing Feather PDF\x2026" : actionWord() == L"Update" ? L"Updating Feather PDF\x2026" : L"Repairing Feather PDF\x2026");
    else if (g_ui.page == P_DONE) { headText = un ? L"Feather PDF has been removed from this computer." : L"Feather PDF has been installed successfully!"; headColour = C.text; }
    else { headText = un ? L"Feather PDF could not be removed completely" : L"Feather PDF could not be installed"; headColour = C.bad; }
    int hx = L;
    if (g_ui.page == P_DONE || g_ui.page == P_FAILED) {   // a round mark: a tick, or an exclamation mark
        RECT m{L, g_ui.px(122), L + g_ui.px(28), g_ui.px(150)};
        rounded(dc, m, g_ui.px(14), g_ui.page == P_DONE ? C.ok : C.bad);
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), static_cast<float>(g_ui.px(3)));
        pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
        const float s = static_cast<float>(g_ui.px(28)) / 28.0f, x0 = static_cast<float>(m.left), y0 = static_cast<float>(m.top);
        if (g_ui.page == P_DONE) {
            Gdiplus::PointF pts[3] = {{x0 + 8 * s, y0 + 14.5f * s}, {x0 + 12.5f * s, y0 + 19 * s}, {x0 + 20.5f * s, y0 + 10 * s}};
            g.DrawLines(&pen, pts, 3);
        } else {
            g.DrawLine(&pen, x0 + 14 * s, y0 + 8 * s, x0 + 14 * s, y0 + 16 * s);
            g.DrawLine(&pen, x0 + 14 * s, y0 + 20.5f * s, x0 + 14 * s, y0 + 20.6f * s);
        }
        hx += g_ui.px(40);
    }
    text(dc, headText, {hx, g_ui.px(122), R - g_ui.px(60), g_ui.px(152)}, g_ui.head, headColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    const int pm = g_prog.permille;
    wchar_t pct[16]; swprintf(pct, 16, L"%d%%", pm / 10);
    text(dc, pct, {R - g_ui.px(60), g_ui.px(122), R, g_ui.px(152)}, g_ui.body, C.dim, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    const RECT bar = progressRect();
    rounded(dc, bar, g_ui.px(5), C.field);
    if (pm > 0) {
        RECT f = bar;
        f.right = bar.left + std::max(g_ui.px(10), static_cast<int>((bar.right - bar.left) * static_cast<long long>(pm) / 1000));
        rounded(dc, f, g_ui.px(5), g_ui.page == P_FAILED ? C.bad : g_ui.page == P_DONE ? C.ok : C.accent);
    }
    if (g_ui.page == P_WORK) text(dc, activity, {L, g_ui.px(190), R, g_ui.px(212)}, g_ui.small, C.dim, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS);
    if (g_ui.page == P_FAILED) text(dc, error, {L, g_ui.px(192), R, rc.bottom - g_ui.px(80)}, g_ui.body, C.text);
    if (g_ui.page == P_DONE && un && !error.empty()) text(dc, error, {L, g_ui.px(192), R, rc.bottom - g_ui.px(80)}, g_ui.small, C.dim);
}

// ---- the buttons and check boxes (drawn here, so they suit the dark window)
HWND button(int id, const wchar_t* label) {
    HWND b = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 10, 10, g_ui.wnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_ui.inst, nullptr);
    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.body), FALSE);
    g_ui.buttons.push_back(b);
    return b;
}

void drawControl(const DRAWITEMSTRUCT* di) {
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    fill(dc, r, C.bg);
    wchar_t label[256] = L"";
    GetWindowTextW(di->hwndItem, label, 256);
    const int id = static_cast<int>(di->CtlID);
    const bool hot = g_ui.hover == di->hwndItem, down = (di->itemState & ODS_SELECTED) != 0, focus = (di->itemState & ODS_FOCUS) != 0;
    const bool off = (di->itemState & ODS_DISABLED) != 0;
    if (id == ID_DEFAULT || id == ID_LAUNCH) {   // a check box
        const bool on = id == ID_DEFAULT ? g_ui.setDefault : g_ui.launch;
        const int s = g_ui.px(20);
        RECT box{r.left + g_ui.px(2), (r.top + r.bottom - s) / 2, r.left + g_ui.px(2) + s, (r.top + r.bottom + s) / 2};
        rounded(dc, box, g_ui.px(5), on ? C.accent : C.field, on ? C.accent : (hot ? C.dim : C.line), focus ? 2.0f : 1.0f);
        if (focus) { RECT fr{box.left - g_ui.px(3), box.top - g_ui.px(3), box.right + g_ui.px(3), box.bottom + g_ui.px(3)}; rounded(dc, fr, g_ui.px(7), C.bg, C.accentHot, 1.0f); rounded(dc, box, g_ui.px(5), on ? C.accent : C.field, on ? C.accent : C.line); }
        if (on) {
            Gdiplus::Graphics g(dc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), static_cast<float>(g_ui.px(2)));
            pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
            const float k = static_cast<float>(s) / 20.0f, x = static_cast<float>(box.left), y = static_cast<float>(box.top);
            Gdiplus::PointF pts[3] = {{x + 5 * k, y + 10.5f * k}, {x + 8.5f * k, y + 14 * k}, {x + 15 * k, y + 6.5f * k}};
            g.DrawLines(&pen, pts, 3);
        }
        text(dc, label, {box.right + g_ui.px(12), r.top, r.right, r.bottom}, g_ui.body, C.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }
    const bool primary = id == ID_INSTALL || id == ID_FINISH || id == ID_RETRY;
    COLORREF bg = primary ? (down ? C.accent : hot ? C.accentHot : C.accent) : (down ? C.line : hot ? RGB(0x2E, 0x31, 0x37) : C.field);
    COLORREF fg = primary ? C.accentText : C.text;
    if (off) { bg = C.panel; fg = RGB(0x7A, 0x7F, 0x88); }
    rounded(dc, r, g_ui.px(8), bg, primary ? bg : C.line);
    if (focus && !off) {   // the keyboard focus: a ring just inside the button
        RECT in{r.left + g_ui.px(3), r.top + g_ui.px(3), r.right - g_ui.px(3), r.bottom - g_ui.px(3)};
        rounded(dc, in, g_ui.px(6), bg, primary ? C.accentText : C.dim, 1.0f);
    }
    text(dc, label, r, id == ID_INSTALL ? g_ui.bold : g_ui.body, fg, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

LRESULT CALLBACK hoverProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (m == WM_MOUSEMOVE && g_ui.hover != h) {
        HWND was = g_ui.hover;
        g_ui.hover = h;
        if (was) InvalidateRect(was, nullptr, FALSE);
        InvalidateRect(h, nullptr, FALSE);
        TRACKMOUSEEVENT te{sizeof(te), TME_LEAVE, h, 0};
        TrackMouseEvent(&te);
    } else if (m == WM_MOUSELEAVE && g_ui.hover == h) {
        g_ui.hover = nullptr;
        InvalidateRect(h, nullptr, FALSE);
    } else if (m == WM_NCDESTROY) RemoveWindowSubclass(h, hoverProc, 1);
    return DefSubclassProc(h, m, w, l);
}

void place(HWND b, int x, int y, int w, int h, bool show) {
    MoveWindow(b, x, y, w, h, FALSE);
    ShowWindow(b, show ? SW_SHOW : SW_HIDE);
}

// shows the controls of the screen in use
void layout() {
    const RECT rc = clientRect();
    const int L = g_ui.px(32), R = rc.right - g_ui.px(32), B = rc.bottom - g_ui.px(32);
    const bool start = g_ui.page == P_START, done = g_ui.page == P_DONE, failed = g_ui.page == P_FAILED, work = g_ui.page == P_WORK;
    const int statusH = g_ui.status.empty() ? 0 : g_ui.px(40);
    HWND change = GetDlgItem(g_ui.wnd, ID_CHANGE), install = GetDlgItem(g_ui.wnd, ID_INSTALL);
    if (g_ui.path) {
        MoveWindow(g_ui.path, L + g_ui.px(12), g_ui.px(118) + statusH + g_ui.px(32), R - g_ui.px(118) - L - g_ui.px(24), g_ui.px(20), FALSE);
        ShowWindow(g_ui.path, start && !g_ui.uninstall ? SW_SHOW : SW_HIDE);
    }
    place(change, R - g_ui.px(106), g_ui.px(118) + statusH + g_ui.px(22), g_ui.px(106), g_ui.px(38), start && !g_ui.uninstall);
    EnableWindow(change, !g_ui.locked);
    SetWindowTextW(install, actionWord().c_str());
    place(install, R - g_ui.px(190), B - g_ui.px(48), g_ui.px(190), g_ui.px(48), start);
    EnableWindow(install, g_ui.uninstall || g_ui.enoughSpace);
    const bool un = g_ui.uninstall;
    place(GetDlgItem(g_ui.wnd, ID_DEFAULT), L, g_ui.px(204), R - L, g_ui.px(32), done && !un);
    place(GetDlgItem(g_ui.wnd, ID_LAUNCH), L, g_ui.px(240), R - L, g_ui.px(32), done && !un);
    place(GetDlgItem(g_ui.wnd, ID_FINISH), R - g_ui.px(140), B - g_ui.px(42), g_ui.px(140), g_ui.px(42), done);
    SetWindowTextW(GetDlgItem(g_ui.wnd, ID_FINISH), un ? L"Close" : L"Finish");
    place(GetDlgItem(g_ui.wnd, ID_RETRY), R - g_ui.px(270), B - g_ui.px(42), g_ui.px(130), g_ui.px(42), failed);
    place(GetDlgItem(g_ui.wnd, ID_CLOSE), R - g_ui.px(130), B - g_ui.px(42), g_ui.px(130), g_ui.px(42), failed);
    place(GetDlgItem(g_ui.wnd, ID_CANCEL), R - g_ui.px(120), B - g_ui.px(40), g_ui.px(120), g_ui.px(40), work && !un);
    InvalidateRect(g_ui.wnd, nullptr, TRUE);
    if (start) SetFocus(install);
    else if (done) SetFocus(GetDlgItem(g_ui.wnd, ID_FINISH));
    else if (failed) SetFocus(GetDlgItem(g_ui.wnd, ID_RETRY));
}

void showPage(Page p) { g_ui.page = p; layout(); }

void startWork() {
    if (g_ui.worker.joinable()) g_ui.worker.join();
    { std::lock_guard<std::mutex> l(g_prog.m); g_prog.error.clear(); g_prog.activity.clear(); }
    g_prog.permille = 0;
    g_prog.cancel = false;
    showPage(P_WORK);
    const std::wstring dir = g_ui.dir;
    const bool un = g_ui.uninstall;
    g_ui.worker = std::thread([dir, un] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool ok = un ? uninstallWork(dir) : installWork(dir);
        CoUninitialize();
        PostMessageW(g_ui.wnd, WM_APP_DONE, ok ? 1 : 0, 0);
    });
}

// a question in the setup's own style: returns the index of the button chosen (or `cancelIndex`)
int ask(const std::wstring& message, const std::vector<std::wstring>& choices, int cancelIndex) {
    // a plain Windows box: rare, and it must work even when the window is busy
    std::wstring t = message;
    UINT flags = MB_ICONINFORMATION | (choices.size() == 2 ? MB_RETRYCANCEL : MB_OK);
    const int r = MessageBoxW(g_ui.wnd, t.c_str(), kApp, flags);
    if (choices.size() == 2) return r == IDRETRY ? 0 : cancelIndex;
    return 0;
}

// Feather PDF must be closed before its files are replaced or removed.
bool appClosed() {
    while (isFeatherRunning()) {
        if (ask(L"Feather PDF is open. Close it (save your work first), then choose Retry.", {L"Retry", L"Cancel"}, 1) != 0) return false;
    }
    return true;
}

void chooseFolder() {
    IFileOpenDialog* fd = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fd)))) return;
    DWORD opts = 0;
    fd->GetOptions(&opts);
    fd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    fd->SetTitle(L"Choose where to install Feather PDF");
    std::wstring start = g_ui.dir;
    while (!start.empty() && !dirExists(start)) start = parentOf(start);
    IShellItem* si = nullptr;
    if (!start.empty() && SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&si)))) { fd->SetFolder(si); si->Release(); }
    if (SUCCEEDED(fd->Show(g_ui.wnd))) {
        IShellItem* res = nullptr;
        if (SUCCEEDED(fd->GetResult(&res))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(res->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                std::wstring d = trimSlash(p);
                // the program gets a folder of its own inside the one chosen (unless that one is already it)
                if (_wcsicmp(leafOf(d).c_str(), kApp) != 0) d += std::wstring(d.back() == L'\\' ? L"" : L"\\") + kApp;
                g_ui.dir = d;
                SetWindowTextW(g_ui.path, d.c_str());
                updateSpace();
                layout();
            }
            CoTaskMemFree(p);
            res->Release();
        }
    }
    fd->Release();
}

void finish() {
    if (!g_ui.uninstall && g_ui.page == P_DONE) {
        if (g_ui.setDefault) openDefaultApps();
        if (g_ui.launch) runAsUser(g_ui.dir + L"\\" + kExe, L"");
    }
    DestroyWindow(g_ui.wnd);
}

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            const RECT rc = clientRect();
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            paint(mem);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {   // the read-only location box
            HDC dc = reinterpret_cast<HDC>(w);
            SetTextColor(dc, C.text);
            SetBkColor(dc, C.field);
            static HBRUSH b = CreateSolidBrush(C.field);
            return reinterpret_cast<LRESULT>(b);
        }
        case WM_DRAWITEM: drawControl(reinterpret_cast<DRAWITEMSTRUCT*>(l)); return TRUE;
        case WM_COMMAND:
            switch (LOWORD(w)) {
                case ID_CHANGE: chooseFolder(); return 0;
                case ID_INSTALL:
                    if (!appClosed()) return 0;
                    updateSpace();
                    if (!g_ui.uninstall && !g_ui.enoughSpace) { layout(); return 0; }
                    startWork();
                    return 0;
                case ID_DEFAULT: g_ui.setDefault = !g_ui.setDefault; InvalidateRect(reinterpret_cast<HWND>(l), nullptr, FALSE); return 0;
                case ID_LAUNCH: g_ui.launch = !g_ui.launch; InvalidateRect(reinterpret_cast<HWND>(l), nullptr, FALSE); return 0;
                case ID_FINISH: finish(); return 0;
                case ID_RETRY:   // back to the first screen, where the location can be changed before trying again
                    if (g_ui.uninstall) { if (appClosed()) startWork(); return 0; }
                    findExisting();
                    updateSpace();
                    showPage(P_START);
                    return 0;
                case ID_CLOSE: DestroyWindow(h); return 0;
                case ID_CANCEL: g_prog.cancel = true; EnableWindow(reinterpret_cast<HWND>(l), FALSE); return 0;
            }
            break;
        case WM_APP_PROGRESS: InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_APP_DONE:
            if (g_ui.worker.joinable()) g_ui.worker.join();
            showPage(w ? P_DONE : P_FAILED);
            if (g_ui.uninstall && !w && g_prog.permille >= 1000) showPage(P_DONE);   // removed, with files left for the restart
            return 0;
        case WM_CLOSE:
            if (g_ui.page == P_WORK) { g_prog.cancel = true; return 0; }   // stops at the next file; nothing is left half done
            DestroyWindow(h);
            return 0;
        case WM_DPICHANGED: {
            g_ui.dpi = HIWORD(w);
            const RECT* r = reinterpret_cast<const RECT*>(l);
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            for (HFONT f : {g_ui.title, g_ui.head, g_ui.body, g_ui.small, g_ui.bold}) DeleteObject(f);
            g_ui.title = makeFont(200, FW_SEMIBOLD); g_ui.head = makeFont(130, FW_SEMIBOLD); g_ui.body = makeFont(105, FW_NORMAL);
            g_ui.small = makeFont(95, FW_NORMAL); g_ui.bold = makeFont(115, FW_SEMIBOLD);
            for (HWND b : g_ui.buttons) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.body), FALSE);
            if (g_ui.path) SendMessageW(g_ui.path, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.body), FALSE);
            layout();
            return 0;
        }
        case WM_DESTROY:
            if (g_ui.worker.joinable()) g_ui.worker.join();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int windowDpi() {
    HDC dc = GetDC(nullptr);
    const int d = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    return d > 0 ? d : 96;
}

int runWindow(HINSTANCE inst) {
    g_ui.inst = inst;
    g_ui.dpi = windowDpi();
    g_ui.title = makeFont(200, FW_SEMIBOLD); g_ui.head = makeFont(130, FW_SEMIBOLD); g_ui.body = makeFont(105, FW_NORMAL);
    g_ui.small = makeFont(95, FW_NORMAL); g_ui.bold = makeFont(115, FW_SEMIBOLD);
    g_ui.icon = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, g_ui.px(48), g_ui.px(48), 0));
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(101));
    wc.lpszClassName = L"FeatherPdfSetup";
    RegisterClassExW(&wc);
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT wr{0, 0, g_ui.px(620), g_ui.px(400)};
    AdjustWindowRectEx(&wr, style, FALSE, 0);
    RECT work; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int W = wr.right - wr.left, H = wr.bottom - wr.top;
    const std::wstring title = g_ui.uninstall ? L"Uninstall Feather PDF" : L"Feather PDF " + g_pay.version + L" Setup";
    g_ui.wnd = CreateWindowExW(WS_EX_CONTROLPARENT, wc.lpszClassName, title.c_str(), style, work.left + (work.right - work.left - W) / 2,
                               work.top + (work.bottom - work.top - H) / 2, W, H, nullptr, nullptr, inst, nullptr);
    if (!g_ui.wnd) return 1;
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_ui.wnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    COLORREF cap = C.bg;
    DwmSetWindowAttribute(g_ui.wnd, 35 /* DWMWA_CAPTION_COLOR */, &cap, sizeof(cap));
    g_prog.notify = g_ui.wnd;
    g_ui.path = CreateWindowExW(0, L"EDIT", g_ui.dir.c_str(), WS_CHILD | ES_READONLY | ES_AUTOHSCROLL, 0, 0, 10, 10, g_ui.wnd,
                                reinterpret_cast<HMENU>(ID_PATH), inst, nullptr);
    SendMessageW(g_ui.path, WM_SETFONT, reinterpret_cast<WPARAM>(g_ui.body), FALSE);
    button(ID_CHANGE, L"Change\x2026");
    button(ID_INSTALL, L"Install");
    button(ID_DEFAULT, L"Set Feather PDF as the default PDF application");
    button(ID_LAUNCH, L"Launch Feather PDF");
    button(ID_FINISH, L"Finish");
    button(ID_RETRY, L"Try again");
    button(ID_CLOSE, L"Close");
    button(ID_CANCEL, L"Cancel");
    for (HWND b : g_ui.buttons) SetWindowSubclass(b, hoverProc, 1, 0);
    updateSpace();
    showPage(P_START);
    ShowWindow(g_ui.wnd, SW_SHOW);
    SetForegroundWindow(g_ui.wnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.hwnd == g_ui.wnd || IsChild(g_ui.wnd, msg.hwnd))) {
            if (msg.wParam == VK_ESCAPE) { SendMessageW(g_ui.wnd, WM_CLOSE, 0, 0); continue; }
            if (msg.wParam == VK_RETURN) {   // Enter: the button that has the focus, else the screen's main button
                HWND f = GetFocus();
                int id = GetDlgCtrlID(f);
                if (!(f && GetParent(f) == g_ui.wnd && id >= ID_CHANGE && id <= ID_CANCEL))
                    id = g_ui.page == P_START ? ID_INSTALL : g_ui.page == P_DONE ? ID_FINISH : g_ui.page == P_FAILED ? ID_RETRY : 0;
                HWND b = GetDlgItem(g_ui.wnd, id);
                if (id && IsWindowEnabled(b) && IsWindowVisible(b)) SendMessageW(g_ui.wnd, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(b));
                continue;
            }
        }
        if (!IsDialogMessageW(g_ui.wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return g_ui.page == P_DONE ? 0 : 1;
}

// ---------------------------------------------------------------- start
// the command line as separate arguments (quotes handled as Windows does)
std::vector<std::wstring> arguments(const std::wstring& cmd) {
    std::vector<std::wstring> out;
    int n = 0;
    LPWSTR* a = CommandLineToArgvW((L"setup " + cmd).c_str(), &n);
    for (int i = 1; a && i < n; ++i) out.push_back(a[i]);
    LocalFree(a);
    return out;
}
bool hasSwitch(const std::wstring& cmd, const wchar_t* sw) {
    for (const auto& a : arguments(cmd)) if (_wcsicmp(a.c_str(), sw) == 0) return true;
    return false;
}
// /D=C:\Some Folder, /dir="...", /parent=123: the value after the prefix (one argument; /D= may also run to the end of the
// line unquoted, as other installers allow)
std::wstring switchValue(const std::wstring& cmd, const wchar_t* prefix) {
    const size_t pl = wcslen(prefix);
    const auto args = arguments(cmd);
    for (size_t i = 0; i < args.size(); ++i) {
        if (_wcsnicmp(args[i].c_str(), prefix, pl) != 0) continue;
        std::wstring v = args[i].substr(pl);
        if (_wcsicmp(prefix, L"/D=") == 0)   // unquoted with spaces: the rest of the arguments belong to it
            for (size_t k = i + 1; k < args.size() && args[k][0] != L'/'; ++k) v += L" " + args[k];
        return v;
    }
    return L"";
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdLine, int) {
    const std::wstring cmd = cmdLine ? cmdLine : L"";
    const bool silent = hasSwitch(cmd, L"/S");
    const bool uninstall = hasSwitch(cmd, L"/uninstall");
    {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        g_log = _wfopen((std::wstring(tmp) + (uninstall ? L"FeatherPDF-Uninstall.log" : L"FeatherPDF-Setup.log")).c_str(), L"a");
        logLine(L"---- " + std::wstring(uninstall ? L"uninstall" : L"setup") + L" " + cmd);
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gin;
    ULONG_PTR gtok = 0;
    Gdiplus::GdiplusStartup(&gtok, &gin, nullptr);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    HANDLE once = CreateMutexW(nullptr, TRUE, L"Local\\FeatherPdfSetupRunning");
    if (GetLastError() == ERROR_ALREADY_EXISTS && !hasSwitch(cmd, L"/fromtemp")) {   // one setup at a time
        if (HWND other = FindWindowW(L"FeatherPdfSetup", nullptr)) { ShowWindow(other, SW_RESTORE); SetForegroundWindow(other); }
        return 2;
    }
    std::wstring error;
    const bool packed = loadPayload(error);
    findExisting();
    int rc = 0;
    if (uninstall) {
        // removing a program while it runs from the folder being removed: the work is done by a copy in %TEMP%
        std::wstring dir = switchValue(cmd, L"/dir=");
        if (dir.empty()) dir = parentOf(g_pay.selfPath);
        if (!hasSwitch(cmd, L"/fromtemp")) {
            wchar_t tmp[MAX_PATH];
            GetTempPathW(MAX_PATH, tmp);
            const std::wstring copy = std::wstring(tmp) + L"FeatherPDF-uninstall-" + std::to_wstring(GetCurrentProcessId()) + L".exe";
            if (CopyFileW(g_pay.selfPath.c_str(), copy.c_str(), FALSE)) {
                std::wstring args = L"\"" + copy + L"\" /uninstall /fromtemp /parent=" + std::to_wstring(GetCurrentProcessId()) + L" /dir=\"" + dir + L"\"" + (silent ? L" /S" : L"");
                STARTUPINFOW si{sizeof(si)};
                PROCESS_INFORMATION pi{};
                ReleaseMutex(once); CloseHandle(once);
                if (CreateProcessW(copy.c_str(), &args[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
                    logLine(L"continuing in " + copy);
                    return 0;
                }
            }
            logLine(L"could not continue from a copy; removing what can be removed");
        } else {
            MoveFileExW(g_pay.selfPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);   // this copy goes at the next restart
            // the uninstaller in the program's folder must have ended before it can be deleted
            if (HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(_wtoi(switchValue(cmd, L"/parent=").c_str())))) {
                WaitForSingleObject(parent, 15000);
                CloseHandle(parent);
            }
        }
        g_ui.uninstall = true;
        g_ui.dir = dir;
        g_pay.version = regGet(HKEY_LOCAL_MACHINE, kUninstallKey, L"DisplayVersion");
        g_pay.buildDate = regGet(HKEY_LOCAL_MACHINE, kUninstallKey, L"BuildDate");
        if (silent) {
            if (isFeatherRunning()) { logLine(L"Feather PDF is running"); rc = 3; }
            else rc = uninstallWork(dir) ? 0 : 1;
        } else rc = runWindow(inst);
    } else {
        if (!packed) {
            logLine(L"no files: " + error);
            if (!silent) MessageBoxW(nullptr, (error.empty() ? std::wstring(L"This setup program has no files to install (download it again).") : error).c_str(), kApp, MB_OK | MB_ICONERROR);
            return 1;
        }
        // the location: the installed copy's (one installation only), else C:\Program Files\Feather PDF (or /D=)
        std::wstring dir = g_old.ours ? g_old.dir : L"";
        if (dir.empty()) dir = switchValue(cmd, L"/D=");
        if (dir.empty()) {
            std::wstring pf = knownFolder(FOLDERID_ProgramFiles);
            if (pf.empty()) pf = L"C:\\Program Files";
            dir = pf + L"\\" + kApp;
        }
        g_ui.dir = trimSlash(dir);
        g_ui.locked = g_old.ours;
        const std::wstring word = actionWord();
        if (g_old.ours) {
            g_ui.status = word == L"Repair" ? L"This version is already installed. Installing it again repairs it; your settings stay."
                                            : L"Version " + g_old.version + L" is installed. It will be updated to " + g_pay.version + L" in the same place; your settings stay.";
        } else if (g_old.inno) {
            g_ui.status = L"An earlier installation of Feather PDF (" + (g_old.version.empty() ? std::wstring(L"older version") : g_old.version) + L") is removed first, so there is only one copy. Your settings stay.";
        }
        // An update from Feather PDF: it closes its windows after starting this; wait for them (at most two minutes,
        // time to answer "save the changes?"). It was asked for by a person, so what goes wrong is said, not only logged.
        const std::wstring waitFor = switchValue(cmd, L"/waitfor=");
        const bool fromApp = !waitFor.empty();
        if (fromApp) {
            if (HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(wcstoul(waitFor.c_str(), nullptr, 10)))) {
                WaitForSingleObject(p, 120000);
                CloseHandle(p);
            }
            for (int i = 0; i < 150 && isFeatherRunning(); ++i) Sleep(200);   // the other windows: 30 seconds more
            while (isFeatherRunning()) {
                logLine(L"Feather PDF is still open: asking");
                const int a = MessageBoxW(nullptr, (L"Feather PDF " + g_pay.version + L" is ready to install, but Feather PDF is still open in another window.\n\n"
                                                    L"Close every Feather PDF window (save your work first), then choose Retry. "
                                                    L"Cancel leaves the version you have.").c_str(), kApp, MB_RETRYCANCEL | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST);
                if (a != IDRETRY) break;
                closeFeatherWindows();
                for (int i = 0; i < 100 && isFeatherRunning(); ++i) Sleep(200);
            }
        }
        if (silent) {
            if (isFeatherRunning()) { logLine(L"Feather PDF is running"); rc = 3; }
            else rc = installWork(g_ui.dir) ? 0 : 1;
            if (rc == 1 && fromApp) {
                std::wstring e;
                { std::lock_guard<std::mutex> l(g_prog.m); e = g_prog.error; }
                MessageBoxW(nullptr, (L"Feather PDF could not be updated to " + g_pay.version + L". Nothing was changed: you still have the version you had.\n\n" + e).c_str(),
                            kApp, MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TOPMOST);
            }
            // after an update asked for in Feather PDF it opens again, updated or not (unless it is still open)
            if (hasSwitch(cmd, L"/relaunch") && (rc == 0 || (fromApp && rc == 1))) runAsUser(g_ui.dir + L"\\" + kExe, L"");
        } else rc = runWindow(inst);
    }
    Gdiplus::GdiplusShutdown(gtok);
    CoUninitialize();
    if (g_log) fclose(g_log);
    return rc;
}
