#include "app.h"
#include <cstdarg>
#include <cstdio>

std::wstring Fmt(const wchar_t* f, ...) {
    va_list ap;
    va_start(ap, f);
    wchar_t buf[1024];
    _vsnwprintf(buf, 1023, f, ap);
    buf[1023] = 0;
    va_end(ap);
    return buf;
}

std::wstring Num(long long v) {
    std::wstring s = std::to_wstring(v < 0 ? -v : v), o;
    int c = 0;
    for (size_t i = s.size(); i-- > 0;) {
        o.insert(o.begin(), s[i]);
        if (++c % 3 == 0 && i > 0) o.insert(o.begin(), L',');
    }
    return v < 0 ? L"-" + o : o;
}

bool ReadFileBytes(const std::wstring& path, std::string& out) {
    out.clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > (1LL << 31)) { CloseHandle(h); return false; }
    out.resize((size_t)sz.QuadPart);
    size_t done = 0;
    while (done < out.size()) {
        DWORD got = 0;
        if (!ReadFile(h, &out[done], (DWORD)std::min<size_t>(out.size() - done, 1 << 24), &got, nullptr) || got == 0) break;
        done += got;
    }
    CloseHandle(h);
    out.resize(done);
    return true;
}

bool WriteFileAtomic(const std::wstring& path, const void* data, size_t n) {
    std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const char* p = (const char*)data;
    size_t done = 0;
    bool ok = true;
    while (done < n) {
        DWORD w = 0;
        if (!WriteFile(h, p + done, (DWORD)std::min<size_t>(n - done, 1 << 24), &w, nullptr) || w == 0) { ok = false; break; }
        done += w;
    }
    if (ok) FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

bool FileExistsW(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

uint64_t FileSizeW(const std::wstring& p) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) return 0;
    return ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}

bool EnsureDir(const std::wstring& p) {
    if (FileExistsW(p)) return true;
    return SHCreateDirectoryExW(nullptr, p.c_str(), nullptr) == ERROR_SUCCESS || FileExistsW(p);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring ExePath() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    return std::wstring(buf, n);
}

std::wstring ExeDir() {
    std::wstring p = ExePath();
    size_t s = p.find_last_of(L'\\');
    return s == std::wstring::npos ? L"." : p.substr(0, s);
}

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR w = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &w)) && w) r = w;
    if (w) CoTaskMemFree(w);
    return r;
}

std::wstring ProcessNameOf(HWND h) {
    if (!h) return L"";
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (!pid) return L"";
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return L"";
    wchar_t buf[MAX_PATH * 2];
    DWORD n = MAX_PATH * 2;
    std::wstring r;
    if (QueryFullProcessImageNameW(p, 0, buf, &n)) {
        r.assign(buf, n);
        size_t s = r.find_last_of(L'\\');
        if (s != std::wstring::npos) r = r.substr(s + 1);
    }
    CloseHandle(p);
    return r;
}

std::wstring FriendlyName(const std::wstring& exe) {
    std::wstring r = exe;
    if (r.size() > 4 && core::Lower(r.substr(r.size() - 4)) == L".exe") r.resize(r.size() - 4);
    return r;
}

static std::wstring TwoDigit(int v) { wchar_t b[8]; _snwprintf(b, 8, L"%02d", v); return b; }

std::wstring FormatTime(time_t t) {
    struct tm lt;
    localtime_s(&lt, &t);
    time_t now = time(nullptr);
    struct tm ln;
    localtime_s(&ln, &now);
    std::wstring hm = TwoDigit(lt.tm_hour) + L":" + TwoDigit(lt.tm_min);
    if (lt.tm_year == ln.tm_year && lt.tm_yday == ln.tm_yday) return std::wstring(T(L"Today")) + L" " + hm;
    time_t yest = now - 86400;
    struct tm ly;
    localtime_s(&ly, &yest);
    if (lt.tm_year == ly.tm_year && lt.tm_yday == ly.tm_yday) return std::wstring(T(L"Yesterday")) + L" " + hm;
    static const wchar_t* days[] = {L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"};
    static const wchar_t* mons[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
    if (lt.tm_year == ln.tm_year && now - t < 6 * 86400)
        return std::wstring(days[lt.tm_wday]) + L" " + TwoDigit(lt.tm_mday) + L" " + mons[lt.tm_mon] + L", " + hm;
    return TwoDigit(lt.tm_mday) + L" " + mons[lt.tm_mon] + L" " + std::to_wstring(lt.tm_year + 1900) + L", " + hm;
}

std::wstring FormatDateTime(time_t t) {
    struct tm lt;
    localtime_s(&lt, &t);
    static const wchar_t* mons[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
    return TwoDigit(lt.tm_mday) + L" " + mons[lt.tm_mon] + L" " + std::to_wstring(lt.tm_year + 1900) + L", " + TwoDigit(lt.tm_hour) + L":" + TwoDigit(lt.tm_min) + L":" + TwoDigit(lt.tm_sec);
}

std::wstring FormatBytes(uint64_t b) {
    wchar_t buf[64];
    if (b < 1024) _snwprintf(buf, 64, L"%llu bytes", (unsigned long long)b);
    else if (b < 1024 * 1024) _snwprintf(buf, 64, L"%.1f KB", b / 1024.0);
    else if (b < 1024ULL * 1024 * 1024) _snwprintf(buf, 64, L"%.1f MB", b / 1048576.0);
    else _snwprintf(buf, 64, L"%.2f GB", b / 1073741824.0);
    return buf;
}

void OpenShell(HWND owner, const std::wstring& what, const wchar_t* verb) {
    ShellExecuteW(owner, verb, what.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ShowInExplorer(const std::wstring& file) {
    std::wstring arg = L"/select,\"" + file + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
}

std::wstring TextAt(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize(n);
    return s;
}

static BOOL CALLBACK SetFontEnum(HWND h, LPARAM f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); return TRUE; }
void SetFontAll(HWND parent, HFONT f) { EnumChildWindows(parent, SetFontEnum, (LPARAM)f); }

// ---- registry (HKCU\Software\ClipboardManager\v2)
static const wchar_t* REGKEY = L"Software\\FeatherClipboardManager";

bool GetIntReg(const wchar_t* name, int& v) {
    DWORD d = 0, n = sizeof d;
    if (RegGetValueW(HKEY_CURRENT_USER, REGKEY, name, RRF_RT_REG_DWORD, nullptr, &d, &n) != ERROR_SUCCESS) return false;
    v = (int)d;
    return true;
}
void SetIntReg(const wchar_t* name, int v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGKEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    DWORD d = (DWORD)v;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&d, sizeof d);
    RegCloseKey(k);
}
bool GetStrReg(const wchar_t* name, std::wstring& v) {
    DWORD n = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, REGKEY, name, RRF_RT_REG_SZ, nullptr, nullptr, &n) != ERROR_SUCCESS || n < 2) return false;
    std::wstring s(n / sizeof(wchar_t) + 1, L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, REGKEY, name, RRF_RT_REG_SZ, nullptr, &s[0], &n) != ERROR_SUCCESS) return false;
    s.resize(wcslen(s.c_str()));
    v = s;
    return true;
}
void SetStrReg(const wchar_t* name, const std::wstring& v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGKEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}
bool GetBlobReg(const wchar_t* name, void* data, DWORD size) {
    DWORD n = size;
    return RegGetValueW(HKEY_CURRENT_USER, REGKEY, name, RRF_RT_REG_BINARY, nullptr, data, &n) == ERROR_SUCCESS && n == size;
}
void SetBlobReg(const wchar_t* name, const void* data, DWORD size) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGKEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_BINARY, (const BYTE*)data, size);
    RegCloseKey(k);
}

// ---- settings
Settings g_set;

void Settings::Load() {
    int v;
    auto I = [&](const wchar_t* n, int& d) { if (GetIntReg(n, v)) d = v; };
    auto B = [&](const wchar_t* n, bool& d) { if (GetIntReg(n, v)) d = v != 0; };
    I(L"Theme", theme); I(L"Hotkey", hotkey); I(L"HotkeyPlain", hotkeyPlain);
    B(L"QuickNumbers", quickNumbers); B(L"PasteOnEnter", pasteOnEnter);
    B(L"RecordImages", recordImages); B(L"RecordFiles", recordFiles); B(L"RecordRich", recordRich); B(L"Ocr", ocr);
    B(L"StartInTray", startInTray); B(L"CloseToTray", closeToTray);
    I(L"DeleteAfterDays", deleteAfterDays); I(L"MaxItems", maxItems); I(L"MaxMB", maxMB);
    I(L"Sensitive", sensitive); I(L"ExpireSeconds", expireSeconds);
    std::wstring s;
    if (GetStrReg(L"IgnoreList", s)) ignoreList = s;
    B(L"Sound", sound); B(L"Toast", toast); B(L"PopupAtCursor", popupAtCursor); B(L"Compact", compact);
    I(L"Language", language); I(L"Filter", filter); I(L"ListWidth", listWidth);
    I(L"MaxEntryMB", maxEntryMB); B(L"MaskPreviews", maskPreviews); I(L"FontSize", fontSize); I(L"Accent", accent); I(L"PreviewLen", previewLen);
    B(L"AlwaysOnTop", alwaysOnTop); B(L"Logging", logging);
    if (GetStrReg(L"Pipelines", s)) pipelines = s;
    B(L"AskedAutostart", askedAutostart); B(L"ToldAboutTray", toldAboutTray); B(L"CheckUpdates", checkUpdates);
    if (GetStrReg(L"SkipVersion", s)) skipVersion = s;
    theme = std::clamp(theme, 0, 2); hotkey = std::clamp(hotkey, 0, 3); hotkeyPlain = std::clamp(hotkeyPlain, 0, 3);
    deleteAfterDays = std::clamp(deleteAfterDays, 0, 36500); maxItems = std::clamp(maxItems, 0, 10000000);
    maxMB = std::clamp(maxMB, 0, 10000000); sensitive = std::clamp(sensitive, 0, 2);
    maxEntryMB = std::clamp(maxEntryMB, 0, 100000); fontSize = std::clamp(fontSize, 0, 2); accent = std::clamp(accent, 0, 4); previewLen = std::clamp(previewLen, 0, 2);
    expireSeconds = std::clamp(expireSeconds, 5, 86400); language = std::clamp(language, 0, LANG_COUNT - 1);
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        autostart = RegQueryValueExW(k, L"FeatherClipboardManager", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        RegCloseKey(k);
    }
}

void Settings::Save() const {
    SetIntReg(L"Theme", theme); SetIntReg(L"Hotkey", hotkey); SetIntReg(L"HotkeyPlain", hotkeyPlain);
    SetIntReg(L"QuickNumbers", quickNumbers); SetIntReg(L"PasteOnEnter", pasteOnEnter);
    SetIntReg(L"RecordImages", recordImages); SetIntReg(L"RecordFiles", recordFiles); SetIntReg(L"RecordRich", recordRich); SetIntReg(L"Ocr", ocr);
    SetIntReg(L"StartInTray", startInTray); SetIntReg(L"CloseToTray", closeToTray);
    SetIntReg(L"DeleteAfterDays", deleteAfterDays); SetIntReg(L"MaxItems", maxItems); SetIntReg(L"MaxMB", maxMB);
    SetIntReg(L"Sensitive", sensitive); SetIntReg(L"ExpireSeconds", expireSeconds);
    SetStrReg(L"IgnoreList", ignoreList);
    SetIntReg(L"Sound", sound); SetIntReg(L"Toast", toast); SetIntReg(L"PopupAtCursor", popupAtCursor); SetIntReg(L"Compact", compact);
    SetIntReg(L"Language", language); SetIntReg(L"Filter", filter); SetIntReg(L"ListWidth", listWidth);
    SetIntReg(L"MaxEntryMB", maxEntryMB); SetIntReg(L"MaskPreviews", maskPreviews); SetIntReg(L"FontSize", fontSize); SetIntReg(L"Accent", accent); SetIntReg(L"PreviewLen", previewLen);
    SetIntReg(L"AlwaysOnTop", alwaysOnTop); SetIntReg(L"Logging", logging); SetStrReg(L"Pipelines", pipelines);
    SetIntReg(L"AskedAutostart", askedAutostart); SetIntReg(L"ToldAboutTray", toldAboutTray); SetIntReg(L"CheckUpdates", checkUpdates);
    SetStrReg(L"SkipVersion", skipVersion);
}

bool IsIgnoredProgram(const std::wstring& exe) {
    if (exe.empty()) return false;
    std::wstring e = core::Lower(exe);
    for (auto& line : core::SplitLines(g_set.ignoreList)) {
        std::wstring t = core::Lower(core::Trim(line));
        if (t.empty()) continue;
        if (t == e || t + L".exe" == e) return true;
    }
    return false;
}

// ---- local diagnostic log: no clipboard content, bounded to 256 KB (the older half is dropped)
void LogEvent(const std::wstring& message) {
    if (!g_set.logging || g_store.dataRoot.empty()) return;
    std::wstring path = JoinPath(g_store.dataRoot, L"diagnostics.log");
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ts[40];
    _snwprintf(ts, 40, L"%04d-%02d-%02d %02d:%02d:%02d  ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string line = core::ToUtf8(std::wstring(ts) + message + L"\r\n");
    std::string cur;
    ReadFileBytes(path, cur);
    if (cur.size() > 256 * 1024) cur = cur.substr(cur.size() / 2);
    cur += line;
    WriteFileAtomic(path, cur.data(), cur.size());
}

std::wstring ReadLog() {
    std::string raw;
    if (!ReadFileBytes(JoinPath(g_store.dataRoot, L"diagnostics.log"), raw) || raw.empty()) return L"(the log is empty)";
    if (raw.size() > 60000) raw = raw.substr(raw.size() - 60000);
    return core::FromUtf8(raw);
}
