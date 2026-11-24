// 3.11: driver store cleanup, winget app updates, per-process disk/network activity.
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include "app.h"
#include "../core/text.h"
#include <setupapi.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <tcpestats.h>
#include <psapi.h>
#include <algorithm>
#include <filesystem>
#include <set>
#include <map>
using std::wstring;
namespace fsu {
static wstring LowerS(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static wstring Trim(wstring s) { while (!s.empty() && iswspace(s.front())) s.erase(0, 1); while (!s.empty() && iswspace(s.back())) s.pop_back(); return s; }

// ---------------- driver store ----------------
static ULONGLONG TreeBytes(const wstring& dir, int depth = 0) {
    ULONGLONG t = 0; if (depth > 6) return 0; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return 0;
    do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) t += TreeBytes(dir + L"\\" + n, depth + 1); else t += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; } while (FindNextFileW(h, &fd));
    FindClose(h); return t;
}
static bool CopyTree(const wstring& from, const wstring& to, int depth = 0) {
    if (depth > 6) return true; CreateDirectoryW(to.c_str(), nullptr); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((from + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return false; bool ok = true;
    do { wstring n = fd.cFileName; if (n == L"." || n == L"..") continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue; if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ok = CopyTree(from + L"\\" + n, to + L"\\" + n, depth + 1) && ok; else ok = CopyFileW((from + L"\\" + n).c_str(), (to + L"\\" + n).c_str(), FALSE) && ok; } while (FindNextFileW(h, &fd));
    FindClose(h); return ok;
}
static wstring InfStr(const wstring& file, const wchar_t* sec, const wchar_t* key) { wchar_t b[512]{}; GetPrivateProfileStringW(sec, key, L"", b, 511, file.c_str()); wstring r = b; size_t sc = r.find(L';'); if (sc != wstring::npos) r.erase(sc); return Trim(r); }
static wstring InfResolve(const wstring& file, wstring v) { v = Trim(v); if (v.size() > 2 && v.front() == L'%' && v.back() == L'%') { wstring r = InfStr(file, L"Strings", v.substr(1, v.size() - 2).c_str()); if (r.size() > 1 && r.front() == L'"' && r.back() == L'"') r = r.substr(1, r.size() - 2); return r.empty() ? v : r; } return v; }
static std::vector<int> VerParts(const wstring& v) { std::vector<int> r; wstring cur; for (wchar_t c : v + L".") { if (c == L'.') { r.push_back(_wtoi(cur.c_str())); cur.clear(); } else cur += c; } return r; }
static long DateKey(const wstring& d) { int m = 0, dd = 0, y = 0; if (swscanf(d.c_str(), L"%d/%d/%d", &m, &dd, &y) == 3) return y * 10000 + m * 100 + dd; return 0; }
typedef BOOL (WINAPI *PFN_StoreLoc)(PCWSTR, PVOID, PCWSTR, PWSTR, DWORD, PDWORD);
std::vector<DrvPkg> ListDriverStore(wstring& err) {
    std::vector<DrvPkg> out; wchar_t win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH); wstring infDir = wstring(win) + L"\\INF";
    HMODULE su = LoadLibraryW(L"setupapi.dll"); auto loc = su ? (PFN_StoreLoc)GetProcAddress(su, "SetupGetInfDriverStoreLocationW") : nullptr; if (!loc) { err = L"The driver store cannot be read on this version of Windows."; return out; }
    auto devs = InfDeviceMap(); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((infDir + L"\\oem*.inf").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) { err = L"No third-party driver packages found."; return out; }
    do { wstring n = fd.cFileName, ln = LowerS(n); if (ln.size() < 8 || ln.compare(ln.size() - 4, 4, L".inf") != 0) continue; bool digits = true; for (size_t i = 3; i + 4 < ln.size(); ++i) if (!iswdigit(ln[i])) digits = false; if (!digits) continue;
        wstring full = infDir + L"\\" + n; DrvPkg p; p.inf = n; wchar_t store[MAX_PATH]{}; DWORD need = 0; if (loc(full.c_str(), nullptr, nullptr, store, MAX_PATH, &need)) { wstring sp = store; size_t sl = sp.find_last_of(L'\\'); if (sl != wstring::npos) { p.folder = sp.substr(0, sl); size_t f2 = p.folder.find_last_of(L'\\'); wstring fn = f2 == wstring::npos ? p.folder : p.folder.substr(f2 + 1); size_t k = LowerS(fn).find(L".inf_"); p.original = k == wstring::npos ? fn : fn.substr(0, k + 4); } }
        if (p.original.empty()) p.original = n;
        wstring dv = InfStr(full, L"Version", L"DriverVer"); size_t cm = dv.find(L','); p.date = Trim(cm == wstring::npos ? dv : dv.substr(0, cm)); p.version = cm == wstring::npos ? L"" : Trim(dv.substr(cm + 1));
        p.provider = InfResolve(full, InfStr(full, L"Version", L"Provider")); p.cls = InfStr(full, L"Version", L"Class"); if (!p.folder.empty()) p.bytes = TreeBytes(p.folder);
        auto it = devs.find(ln); p.inUse = it != devs.end(); if (p.inUse) p.devices = it->second; out.push_back(std::move(p)); } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::map<wstring, std::vector<size_t>> groups; for (size_t i = 0; i < out.size(); ++i) groups[LowerS(out[i].original) + L"|" + LowerS(out[i].cls)].push_back(i); int gid = 0;
    for (auto& kv : groups) { ++gid; size_t best = kv.second[0]; for (size_t i : kv.second) { out[i].group = gid; long da = DateKey(out[i].date), db = DateKey(out[best].date); if (da > db || (da == db && VerParts(out[i].version) > VerParts(out[best].version))) best = i; } out[best].newest = true; }
    std::sort(out.begin(), out.end(), [](const DrvPkg& a, const DrvPkg& b) { if (a.group != b.group) return a.group < b.group; return DateKey(a.date) > DateKey(b.date); });
    return out;
}
static bool InfNameOk(const wstring& n) { wstring l = LowerS(n); if (l.size() < 8 || l.compare(0, 3, L"oem") != 0 || l.compare(l.size() - 4, 4, L".inf") != 0) return false; for (size_t i = 3; i + 4 < l.size(); ++i) if (!iswdigit(l[i])) return false; return true; }
bool RemoveDriverPkg(const DrvPkg& p, bool backup, wstring& err, wstring& backupPath) {
    if (!InfNameOk(p.inf)) { err = L"Not a third-party driver package."; return false; } if (p.inUse) { err = L"A device is using this package."; return false; }
    if (backup && !p.folder.empty()) { wstring dir = SubDir(L"driverbackups"), nm = p.inf.substr(0, p.inf.size() - 4) + L"_" + p.original.substr(0, p.original.find(L".inf")) + L"_" + p.version; for (auto& c : nm) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_'; backupPath = dir + L"\\" + nm;
        if (!CopyTree(p.folder, backupPath)) { err = L"The backup copy could not be completed, so the package was kept."; backupPath.clear(); return false; } }
    SetLastError(0); if (!SetupUninstallOEMInfW(p.inf.c_str(), 0, nullptr)) { DWORD e = GetLastError(); err = e == ERROR_ACCESS_DENIED ? L"Administrator rights are required. Use More > Restart as administrator." : e == 0xE0000022 || e == 0xE0000203 ? L"Windows says a device still uses this package." : Win32Err(e); return false; }
    return true;
}
std::vector<DrvBackup> ListDriverBackups() {
    std::vector<DrvBackup> v; wstring dir = SubDir(L"driverbackups"); WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) return v;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue; DrvBackup b; b.name = fd.cFileName; b.path = dir + L"\\" + fd.cFileName; SYSTEMTIME st; FileTimeToSystemTime(&fd.ftCreationTime, &st); wchar_t t[32]; swprintf(t, 32, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute); b.time = t; b.bytes = TreeBytes(b.path); v.push_back(b); } while (FindNextFileW(h, &fd));
    FindClose(h); std::sort(v.begin(), v.end(), [](const DrvBackup& a, const DrvBackup& b) { return a.time > b.time; }); return v;
}
bool RestoreDriverBackup(const wstring& path, wstring& err) {
    wstring root = LowerS(SubDir(L"driverbackups")) + L"\\"; wstring lp = LowerS(path); if (lp.rfind(root, 0) != 0 || lp.find(L"..") != wstring::npos) { err = L"That is not one of Feather's driver backups."; return false; }
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((path + L"\\*.inf").c_str(), &fd); if (h == INVALID_HANDLE_VALUE) { err = L"The backup has no driver file (.inf)."; return false; } wstring inf = path + L"\\" + fd.cFileName; FindClose(h);
    wchar_t dest[MAX_PATH]{}; if (!SetupCopyOEMInfW(inf.c_str(), path.c_str(), SPOST_PATH, 0, dest, MAX_PATH, nullptr, nullptr)) { DWORD e = GetLastError(); err = e == ERROR_ACCESS_DENIED ? L"Administrator rights are required. Use More > Restart as administrator." : Win32Err(e); return false; }
    return true;
}

// ---------------- winget ----------------
static wstring CleanOut(const std::string& raw) {   // UTF-8 -> text; drop ANSI codes and progress-bar rewrites (text before a lone \r)
    wstring w = FromUtf8(raw); wstring out, line; for (size_t i = 0; i < w.size(); ++i) { wchar_t c = w[i]; if (c == 0x1B) { while (i < w.size() && !iswalpha(w[i])) ++i; continue; } if (c == L'\r') { if (i + 1 < w.size() && w[i + 1] == L'\n') continue; line.clear(); continue; } if (c == L'\n') { out += line + L"\n"; line.clear(); continue; } line += c; }
    return out + line;
}
static int CharW(wchar_t c) { return (c >= 0x1100 && (c <= 0x115F || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6))) ? 2 : 1; }
static wstring SliceW(const wstring& s, int from, int to) { wstring r; int w = 0; for (wchar_t c : s) { int cw = CharW(c); if (w >= from && (to < 0 || w < to)) r += c; w += cw; if (to >= 0 && w >= to) break; } return Trim(r); }
bool WingetPresent(wstring& version) { std::string o; DWORD code = 1; if (!RunCapture(L"winget.exe --version", o, 20000, &code) || code != 0) return false; version = Trim(CleanOut(o)); return !version.empty() && version.size() < 40; }
bool WingetUpgradable(std::vector<WingetPkg>& out, wstring& err) {
    std::string o; DWORD code = 1; if (!RunCapture(L"winget.exe upgrade --accept-source-agreements --disable-interactivity", o, 180000, &code)) { err = L"winget did not answer in time."; return false; }
    wstring t = CleanOut(o); std::vector<wstring> lines; { wstring cur; for (wchar_t c : t) { if (c == L'\n') { lines.push_back(cur); cur.clear(); } else cur += c; } if (!cur.empty()) lines.push_back(cur); }
    int sep = -1; for (size_t i = 1; i < lines.size(); ++i) { wstring l = Trim(lines[i]); if (l.size() >= 8 && l.find_first_not_of(L'-') == wstring::npos) { sep = (int)i; break; } }
    if (sep < 1) { if (t.find(L"upgrades available") == wstring::npos && code != 0 && lines.size() > 0) err = L"winget reported: " + Trim(lines.back().empty() && lines.size() > 1 ? lines[lines.size() - 2] : lines.back()); return code == 0 || sep < 1 ? (err.empty()) : false; }   // no table = nothing to update
    const wstring& hd = lines[sep - 1]; std::vector<int> starts; { int w = 0; bool inWord = false; int spaces = 2; for (wchar_t c : hd) { int cw = CharW(c); if (c == L' ') { ++spaces; inWord = false; } else { if (!inWord && spaces >= 2) starts.push_back(w); inWord = true; spaces = 0; } w += cw; } }
    if (starts.size() < 4) { err = L"The winget table could not be read (unexpected layout)."; return false; }
    for (size_t i = sep + 1; i < lines.size(); ++i) { wstring l = lines[i]; if (Trim(l).empty()) break; if (iswdigit(Trim(l)[0]) && Trim(l).find(L' ') != wstring::npos && Trim(l).find(L"upgrade") != wstring::npos) break;
        WingetPkg p; auto col = [&](size_t k) { return SliceW(l, starts[k], k + 1 < starts.size() ? starts[k + 1] : -1); }; p.name = col(0); p.id = col(1); p.version = col(2); p.available = col(3); if (starts.size() > 4) p.source = col(4); if (!p.id.empty()) out.push_back(p); }
    return true;
}
static bool IdOk(const wstring& id) { if (id.empty() || id.size() > 128) return false; for (wchar_t c : id) if (!(iswalnum(c) && c < 128) && !wcschr(L"._-+", c)) return false; return true; }
static wstring Tail(const wstring& t, size_t n) { return t.size() > n ? L"…" + t.substr(t.size() - n) : t; }
bool WingetShow(const wstring& id, wstring& text, wstring& err) { if (!IdOk(id)) { err = L"Invalid package id."; return false; } std::string o; DWORD code = 1; if (!RunCapture(L"winget.exe show --id " + id + L" --exact --accept-source-agreements --disable-interactivity", o, 90000, &code)) { err = L"winget did not answer in time."; return false; } text = CleanOut(o); if (text.size() > 6000) text.resize(6000); return true; }
bool WingetUpgrade(const wstring& id, wstring& text, int& exitCode, wstring& err) {
    if (!IdOk(id)) { err = L"Invalid package id."; return false; } std::string o; DWORD code = 1;
    if (!RunCapture(L"winget.exe upgrade --id " + id + L" --exact --silent --accept-source-agreements --accept-package-agreements --disable-interactivity", o, 1800000, &code)) { err = L"winget did not finish within 30 minutes."; text = Tail(CleanOut(o), 3000); return false; }
    text = Tail(CleanOut(o), 3000); exitCode = (int)code; return true;
}

// ---------------- per-process disk and network activity ----------------
std::vector<ProcAct> SampleProcActivity(bool& netBytes) {
    std::map<DWORD, ProcAct> m; HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (snap != INVALID_HANDLE_VALUE) { PROCESSENTRY32W pe{sizeof pe}; if (Process32FirstW(snap, &pe)) do { if (pe.th32ProcessID == 0) continue; ProcAct a; a.pid = pe.th32ProcessID; a.name = pe.szExeFile;
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, a.pid); if (h) { IO_COUNTERS io{}; if (GetProcessIoCounters(h, &io)) { a.rd = io.ReadTransferCount; a.wr = io.WriteTransferCount; } CloseHandle(h); } m[a.pid] = a; } while (Process32NextW(snap, &pe)); CloseHandle(snap); }
    netBytes = IsElevated(); ULONG sz = 0; GetExtendedTcpTable(nullptr, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (sz) { std::vector<BYTE> b(sz + 4096); sz = (ULONG)b.size(); auto* t = (PMIB_TCPTABLE_OWNER_PID)b.data(); if (GetExtendedTcpTable(t, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) for (DWORD i = 0; i < t->dwNumEntries; ++i) { auto& r = t->table[i]; if (r.dwState != MIB_TCP_STATE_ESTAB) continue; auto it = m.find(r.dwOwningPid); if (it == m.end()) continue; ++it->second.tcp;
            if (netBytes) { MIB_TCPROW row{}; row.dwState = r.dwState; row.dwLocalAddr = r.dwLocalAddr; row.dwLocalPort = r.dwLocalPort; row.dwRemoteAddr = r.dwRemoteAddr; row.dwRemotePort = r.dwRemotePort; TCP_ESTATS_DATA_RW_v0 rw{}; rw.EnableCollection = TcpBoolOptEnabled; SetPerTcpConnectionEStats(&row, TcpConnectionEstatsData, (PUCHAR)&rw, 0, sizeof rw, 0);
                TCP_ESTATS_DATA_ROD_v0 rod{}; if (GetPerTcpConnectionEStats(&row, TcpConnectionEstatsData, nullptr, 0, 0, nullptr, 0, 0, (PUCHAR)&rod, 0, sizeof rod) == NO_ERROR) { it->second.netIn += rod.DataBytesIn; it->second.netOut += rod.DataBytesOut; } } } }
    sz = 0; GetExtendedTcpTable(nullptr, &sz, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0); if (sz) { std::vector<BYTE> b(sz + 4096); sz = (ULONG)b.size(); auto* t = (PMIB_TCP6TABLE_OWNER_PID)b.data(); if (GetExtendedTcpTable(t, &sz, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) for (DWORD i = 0; i < t->dwNumEntries; ++i) { auto it = m.find(t->table[i].dwOwningPid); if (it != m.end() && t->table[i].dwState == MIB_TCP_STATE_ESTAB) ++it->second.tcp; } }
    sz = 0; GetExtendedUdpTable(nullptr, &sz, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0); if (sz) { std::vector<BYTE> b(sz + 4096); sz = (ULONG)b.size(); auto* t = (PMIB_UDPTABLE_OWNER_PID)b.data(); if (GetExtendedUdpTable(t, &sz, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) for (DWORD i = 0; i < t->dwNumEntries; ++i) { auto it = m.find(t->table[i].dwOwningPid); if (it != m.end()) ++it->second.udp; } }
    std::vector<ProcAct> v; for (auto& kv : m) v.push_back(kv.second); return v;
}
}
