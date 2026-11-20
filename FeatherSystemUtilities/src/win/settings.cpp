// Themes, persisted settings, legacy-data migration, table export.
#include "app.h"
#include "../core/text.h"
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fsu {
using std::wstring;

static const Pal kPal[] = {
    {RGB(14, 14, 14), RGB(22, 22, 22), RGB(18, 18, 18), RGB(40, 44, 60), RGB(228, 228, 228), RGB(128, 128, 128), RGB(40, 40, 40), RGB(92, 214, 92), RGB(255, 100, 100), RGB(160, 160, 255), RGB(240, 190, 80), RGB(44, 44, 44), RGB(32, 56, 92), RGB(120, 150, 255), RGB(98, 98, 98), true, L"Dark"},
    {RGB(8, 10, 20), RGB(14, 18, 32), RGB(10, 13, 24), RGB(30, 40, 80), RGB(225, 230, 245), RGB(120, 128, 150), RGB(32, 38, 60), RGB(90, 220, 150), RGB(255, 110, 120), RGB(150, 170, 255), RGB(240, 200, 90), RGB(28, 34, 58), RGB(24, 44, 90), RGB(100, 150, 255), RGB(84, 90, 110), true, L"Midnight"},
    {RGB(26, 21, 16), RGB(36, 29, 22), RGB(30, 24, 18), RGB(70, 52, 34), RGB(236, 226, 210), RGB(150, 138, 120), RGB(58, 48, 38), RGB(150, 214, 96), RGB(255, 112, 96), RGB(220, 176, 255), RGB(246, 190, 90), RGB(52, 42, 32), RGB(76, 54, 30), RGB(236, 164, 92), RGB(104, 94, 80), true, L"Warm"},
    {RGB(251, 251, 251), RGB(238, 238, 240), RGB(232, 232, 236), RGB(205, 216, 238), RGB(28, 28, 30), RGB(120, 120, 124), RGB(216, 216, 220), RGB(18, 130, 40), RGB(196, 36, 36), RGB(70, 70, 200), RGB(160, 100, 0), RGB(224, 224, 228), RGB(200, 220, 250), RGB(30, 90, 200), RGB(170, 170, 174), false, L"Light"},
    {RGB(14, 20, 50), RGB(26, 34, 76), RGB(21, 28, 66), RGB(44, 56, 120), RGB(232, 236, 255), RGB(130, 142, 196), RGB(40, 52, 108), RGB(76, 214, 150), RGB(255, 112, 120), RGB(160, 176, 255), RGB(255, 190, 90), RGB(36, 48, 104), RGB(36, 62, 138), RGB(86, 168, 255), RGB(86, 96, 140), true, L"Navy"},
};
const Pal& PalFor(int t) { return kPal[(t < 0 || t > 4) ? 4 : t]; }
int ThemeCount() { return 5; }

static Settings g_cfg;
Settings& Cfg() { return g_cfg; }
static std::wstring LowerPath(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); while (s.size() > 3 && s.back() == L'\\') s.pop_back(); return s; }
bool IsExcluded(const std::wstring& path) {
    if (g_cfg.exclusions.empty()) return false; std::wstring p = LowerPath(path);
    for (auto& e : g_cfg.exclusions) { std::wstring x = LowerPath(e); if (!x.empty() && (p == x || p.rfind(x + L"\\", 0) == 0)) return true; } return false;
}

void LoadSettings() {
    std::ifstream f(std::filesystem::path(DataDir() + L"\\settings.ini"), std::ios::binary); std::string line; int guard = 0;
    while (std::getline(f, line) && ++guard < 2000) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('='); if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1); int n = atoi(v.c_str());
        if (k == "theme") g_cfg.theme = (n >= 0 && n < 5) ? n : 4; else if (k == "density") g_cfg.density = n ? 1 : 0;
        else if (k == "largeMinMB") g_cfg.largeMinMB = (n >= 1 && n <= 100000) ? n : 100; else if (k == "dupMinKB") g_cfg.dupMinKB = (n >= 1 && n <= 10000000) ? n : 1024;
        else if (k.rfind("scan.", 0) == 0) { if (g_cfg.scanRoots.size() < 20 && !v.empty()) g_cfg.scanRoots.push_back(FromUtf8(v)); }
        else if (k.rfind("excl.", 0) == 0) { if (g_cfg.exclusions.size() < 100 && !v.empty()) g_cfg.exclusions.push_back(FromUtf8(v)); }
        else if (k.rfind("ign.", 0) == 0) { if (g_cfg.ignored.size() < 500 && !v.empty()) g_cfg.ignored.push_back(FromUtf8(v)); }
        else if (k == "skipVersion") g_cfg.skipVersion = FromUtf8(v); else if (k == "deferUntil") g_cfg.deferUntil = atoll(v.c_str());
        else if (k == "logDays") g_cfg.logRetentionDays = (n >= 0 && n <= 3650) ? n : 0; else if (k == "quarantine") g_cfg.quarantineLeftovers = n != 0;
        else if (k == "autoUpdate") g_cfg.autoUpdate = n != 0; else if (k == "lastUpdateCheck") g_cfg.lastUpdateCheck = atoll(v.c_str());
        else if (k == "keepRecent") g_cfg.keepRecent = n != 0; else if (k == "hideSystem") g_cfg.hideSystem = n != 0; else if (k == "confirmBatch") g_cfg.confirmBatch = n != 0;
        else if (k == "startPage") g_cfg.startPage = (n >= 0 && n < 13) ? n : 0; else if (k == "maximized") g_cfg.maximized = n != 0;
        else if (k == "winX") g_cfg.winX = n; else if (k == "winY") g_cfg.winY = n;
        else if (k == "winW") g_cfg.winW = (n >= 600 && n < 10000) ? n : 1360; else if (k == "winH") g_cfg.winH = (n >= 400 && n < 10000) ? n : 780;
        else if (k.rfind("cols.", 0) == 0) {   // cols.<page>=w,w,w
            std::vector<int> w; std::stringstream ss(v); std::string t; while (std::getline(ss, t, ',') && w.size() < 32) { int x = atoi(t.c_str()); w.push_back(x >= 20 && x <= 3000 ? x : 100); }
            g_cfg.colw[FromUtf8(k.substr(5))] = w;
        }
    }
}
void SaveSettings() {
    std::ostringstream o;
    o << "theme=" << g_cfg.theme << "\ndensity=" << g_cfg.density << "\nhideSystem=" << g_cfg.hideSystem << "\nconfirmBatch=" << g_cfg.confirmBatch << "\nautoUpdate=" << g_cfg.autoUpdate << "\nlastUpdateCheck=" << g_cfg.lastUpdateCheck << "\nkeepRecent=" << g_cfg.keepRecent << "\nlargeMinMB=" << g_cfg.largeMinMB << "\ndupMinKB=" << g_cfg.dupMinKB << "\nstartPage=" << g_cfg.startPage
      << "\nmaximized=" << g_cfg.maximized << "\nwinX=" << g_cfg.winX << "\nwinY=" << g_cfg.winY << "\nwinW=" << g_cfg.winW << "\nwinH=" << g_cfg.winH << "\n";
    o << "skipVersion=" << ToUtf8(g_cfg.skipVersion) << "\ndeferUntil=" << g_cfg.deferUntil << "\nlogDays=" << g_cfg.logRetentionDays << "\nquarantine=" << g_cfg.quarantineLeftovers << "\n";
    for (size_t i = 0; i < g_cfg.exclusions.size(); ++i) o << "excl." << i << "=" << ToUtf8(g_cfg.exclusions[i]) << "\n";
    for (size_t i = 0; i < g_cfg.ignored.size(); ++i) o << "ign." << i << "=" << ToUtf8(g_cfg.ignored[i]) << "\n";
    for (size_t i = 0; i < g_cfg.scanRoots.size(); ++i) o << "scan." << i << "=" << ToUtf8(g_cfg.scanRoots[i]) << "\n";
    for (auto& kv : g_cfg.colw) { o << "cols." << ToUtf8(kv.first) << "="; for (size_t i = 0; i < kv.second.size(); ++i) o << (i ? "," : "") << kv.second[i]; o << "\n"; }
    AtomicWrite(DataDir() + L"\\settings.ini", o.str());
}

// ---- legacy migration: copy (never move/delete) the old apps' per-user data once ----
static wstring g_migNote;
static int CopyTree(const std::filesystem::path& from, const std::filesystem::path& to) {
    int n = 0; std::error_code ec;
    if (!std::filesystem::exists(from, ec)) return 0;
    std::filesystem::create_directories(to, ec);
    for (auto& e : std::filesystem::directory_iterator(from, ec)) {
        if (!e.is_regular_file(ec) || e.file_size(ec) > 5 * 1024 * 1024) continue;
        auto dst = to / e.path().filename(); if (std::filesystem::exists(dst, ec)) continue;
        std::filesystem::copy_file(e.path(), dst, ec); if (!ec) ++n;
    }
    return n;
}
void MigrateLegacyData() {
    wstring marker = DataDir() + L"\\migrated.v1"; if (GetFileAttributesW(marker.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    PWSTR p = nullptr; if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) return; std::filesystem::path la(p); CoTaskMemFree(p);
    int n = CopyTree(la / L"WindowsServicesManager" / L"Profiles", std::filesystem::path(SubDir(L"legacy")) / L"services-profiles");
    n += CopyTree(la / L"WindowsServicesManager" / L"Snapshots", std::filesystem::path(SubDir(L"legacy")) / L"services-snapshots");
    n += CopyTree(la / L"StartupManager", std::filesystem::path(SubDir(L"legacy")) / L"startup");
    g_migNote = n ? L"Copied " + std::to_wstring(n) + L" file(s) from the old Startup/Services Manager data into the 'legacy' folder (originals untouched). Their formats are kept for reference; they are not applied automatically." : L"No older Startup/Services Manager data found.";
    AtomicWrite(marker, ToUtf8(g_migNote));
}
wstring MigrationNote() {
    if (g_migNote.empty()) { std::ifstream f(std::filesystem::path(DataDir() + L"\\migrated.v1"), std::ios::binary); std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); g_migNote = FromUtf8(s); }
    return g_migNote;
}

// ---- export ----
static wstring HtmlEsc(const wstring& s) { wstring o; for (wchar_t c : s) { if (c == L'&') o += L"&amp;"; else if (c == L'<') o += L"&lt;"; else if (c == L'>') o += L"&gt;"; else if (c == L'"') o += L"&quot;"; else o += c; } return o; }
static wstring JsonEsc(const wstring& s) { wstring o; for (wchar_t c : s) { if (c == L'"') o += L"\\\""; else if (c == L'\\') o += L"\\\\"; else if (c == L'\n') o += L"\\n"; else if (c == L'\r') o += L"\\r"; else if (c == L'\t') o += L"\\t"; else if (c < 0x20) o += L' '; else o += c; } return o; }

void ExportTable(HWND owner, const wstring& title, const std::vector<wstring>& cols, const std::vector<std::vector<wstring>>& rows, const wstring& prov) {
    wchar_t file[MAX_PATH] = L""; wcsncpy_s(file, (title + L" " + NowIsoUtc().substr(0, 10)).c_str(), _TRUNCATE);
    for (wchar_t* c = file; *c; ++c) if (wcschr(L"\\/:*?\"<>|", *c)) *c = L'-';
    OPENFILENAMEW o{}; o.lStructSize = sizeof o; o.hwndOwner = owner; o.lpstrFilter = L"CSV, spreadsheet-safe (*.csv)\0*.csv\0HTML report (*.html)\0*.html\0JSON (*.json)\0*.json\0";
    o.lpstrFile = file; o.nMaxFile = MAX_PATH; o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST; o.lpstrDefExt = L"csv"; o.lpstrTitle = L"Export";
    if (!GetSaveFileNameW(&o)) return;
    wstring path = file, out; std::string bytes;
    wstring ext = Lower(path.size() > 5 ? path.substr(path.size() - 5) : path);
    if (ext == L".html") {
        out = L"<!doctype html><html><head><meta charset=\"utf-8\"><title>" + HtmlEsc(title) + L"</title><style>body{font:13px Segoe UI,sans-serif;margin:20px}table{border-collapse:collapse}th,td{border:1px solid #bbb;padding:3px 8px;text-align:left}th{background:#eee}</style></head><body><h2>" + HtmlEsc(title) + L"</h2><p>" + HtmlEsc(prov) + L"</p><table><tr>";
        for (auto& c : cols) out += L"<th>" + HtmlEsc(c) + L"</th>"; out += L"</tr>\n";
        for (auto& r : rows) { out += L"<tr>"; for (auto& c : r) out += L"<td>" + HtmlEsc(c) + L"</td>"; out += L"</tr>\n"; }
        out += L"</table></body></html>"; bytes = ToUtf8(out);
    } else if (ext == L".json") {
        out = L"{\n \"app\": \"Feather System Utilities " FSU_VERSION L"\",\n \"provenance\": \"" + JsonEsc(prov) + L"\",\n \"rows\": [\n";
        for (size_t i = 0; i < rows.size(); ++i) { out += L"  {"; for (size_t k = 0; k < cols.size() && k < rows[i].size(); ++k) out += (k ? L", \"" : L"\"") + JsonEsc(cols[k]) + L"\": \"" + JsonEsc(rows[i][k]) + L"\""; out += i + 1 < rows.size() ? L"},\n" : L"}\n"; }
        out += L" ]\n}\n"; bytes = ToUtf8(out);
    } else {
        out = L"# " + prov + L"\r\n" + CsvRow(cols) + L"\r\n"; for (auto& r : rows) out += CsvRow(r) + L"\r\n";   // formula-safe
        bytes = "\xEF\xBB\xBF" + ToUtf8(out);
    }
    if (!AtomicWrite(path, bytes)) DarkMsg(owner, L"Export", L"The file could not be written.", 1);
}

} // namespace fsu
