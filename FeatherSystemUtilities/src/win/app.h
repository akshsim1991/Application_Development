// Shared declarations for the Win32 layer.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <atomic>

#define FSU_VERSION L"1.7.0"

namespace fsu {

std::wstring ExpandEnv(const std::wstring& s);
std::wstring DataDir();                        // %LOCALAPPDATA%\FeatherSystemUtilities (created)
std::wstring SubDir(const wchar_t* name);      // DataDir()\name (created)
std::wstring Win32Err(DWORD e);                // text + numeric code (F185)
std::wstring NowIsoUtc();
bool FileExistsExpanded(const std::wstring& p);
bool IsElevated();

// ---- theme + settings ----
struct Pal { COLORREF bg, bar, rail, railSel, text, dim, grid, green, red, lav, amber, hot, sel, accent, disabledTb; bool dark; const wchar_t* name; };
const Pal& PalFor(int theme);   // 0 Dark 1 Midnight 2 Warm 3 Light
int ThemeCount();
struct Settings {
    int theme = 0, density = 1, startPage = 0;
    bool hideSystem = true, confirmBatch = true, maximized = false, keepRecent = true;
    int largeMinMB = 100, dupMinKB = 1024; std::vector<std::wstring> scanRoots;   // empty = default user folders
    int winX = CW_USEDEFAULT, winY = CW_USEDEFAULT, winW = 1360, winH = 780;
    std::map<std::wstring, std::vector<int>> colw;   // per page column widths (96-dpi units)
};
Settings& Cfg();
void LoadSettings();
void SaveSettings();
void MigrateLegacyData();                       // copies old portable-app data (never deletes originals)
std::wstring MigrationNote();

// ---- file evidence (never loads/executes the target) ----
enum class Sig { Unknown, Valid, Unsigned, Invalid };
struct FileInfo {
    bool exists = false;
    std::wstring publisher, version, signer;
    Sig sig = Sig::Unknown;
    bool catalogSigned = false;
};
FileInfo InspectFile(const std::wstring& expandedPath, bool checkSignature = true);

// ---- journal (F158) ----
void WriteJournal(const std::wstring& target, const std::wstring& source, const std::wstring& before, const std::wstring& after, const std::wstring& result, bool reversible);
bool JournalWritable();

// ---- startup ----
enum class Toggle { None, RunApproved, Task, ShellBlocked };
struct StartupEntry {
    std::wstring name, state, type, starts, scope, command, program, publisher, version;
    std::wstring signature, warnings, location, firstSeen, key;   // key = stable identity
    HKEY root = nullptr; REGSAM view = 0; std::wstring subKey, valueName, clsid, taskPath, delayNote;
    bool runOnce = false, baseline = false, recent = false, hasWarning = false, microsoft = false, isSystem = false, managedDelay = false;
    Toggle toggle = Toggle::None;
};
struct StartupScan { std::vector<StartupEntry> entries; std::wstring coverage; };
StartupScan ScanStartup();
bool SetStartupEnabled(const StartupEntry& e, bool enable, std::wstring& err);
struct BackupInfo { std::wstring path, label; FILETIME written{}; };
bool DeleteStartupEntry(const StartupEntry& e, std::wstring& err);
std::vector<BackupInfo> ListBackups();
bool RestoreBackup(const std::wstring& path, std::wstring& err, std::wstring& summary);
bool AddStartupEntry(const std::wstring& name, const std::wstring& command, bool allUsers, std::wstring& err);
bool CanDelay(const StartupEntry& e);                                              // registry Run, current user
bool DelayStartup(const StartupEntry& e, int seconds, std::wstring& err);          // managed task + disable original, with rollback
bool RemoveDelay(const StartupEntry& taskEntry, std::wstring& err);                // restore original, delete task
int  CleanupManagedTasks(std::wstring& report);                                    // used by uninstall (--cleanup)

// ---- services & drivers ----
struct ServiceEntry {
    std::wstring name, display, state, start, safety, account, description, warnings, path, ptype, notes, program, publisher, version, signature;
    DWORD pid = 0, stateCode = 0, startType = 0, controls = 0, svcType = 0;
    bool delayed = false, trigger = false, critical = false, isDriver = false;
    std::vector<std::wstring> deps;
};
struct ServiceScan { std::vector<ServiceEntry> entries; std::wstring coverage; };
ServiceScan ScanServices(bool drivers = false);
enum class SvcOp { Start, Stop, Restart, Pause, Resume };
bool ServiceOp(const ServiceEntry& s, SvcOp op, std::wstring& err);
bool KillServiceProcess(const ServiceEntry& s, const std::vector<ServiceEntry>& all, std::wstring& err);
std::vector<std::wstring> ActiveDependents(const std::wstring& name);
bool SetServiceStartType(const std::wstring& name, DWORD startType, bool delayed, std::wstring& err, std::wstring& before);
bool IsCriticalService(const std::wstring& name);
std::wstring ServiceTabText(const ServiceEntry& s, int tab);   // 0 General 1 Configuration 2 Recovery 3 Dependencies 4 History
bool EditServiceNames(const ServiceEntry& s, const std::wstring& display, const std::wstring& description, std::wstring& err);
bool SetServiceAccount(const ServiceEntry& s, int which, std::wstring& err);   // 0 LocalSystem 1 LocalService 2 NetworkService
struct Recovery { int act[3] = {0, 0, 0}; DWORD delayMin[3] = {1, 1, 1}; DWORD resetDays = 1; bool onNonCrash = false; std::wstring cmd; };   // act: 0 none 1 restart 2 reboot 3 run
bool GetRecovery(const std::wstring& name, Recovery& r);
bool SetRecovery(const std::wstring& name, const Recovery& r, std::wstring& err);

// profiles / snapshots
struct PlanItem { std::wstring name, display, from, to, why; DWORD type = 0; bool delayed = false, skip = false; DWORD oldType = 0; bool oldDelayed = false; };
struct ProfileInfo { std::wstring id, label, impact, path; bool builtin = false; };
std::vector<ProfileInfo> ListProfiles();
bool SaveProfile(const std::wstring& label, const std::vector<ServiceEntry>& items, std::wstring& path, std::wstring& err);
bool BuildPlan(const ProfileInfo& p, const std::vector<ServiceEntry>& current, std::vector<PlanItem>& plan, std::wstring& note, std::wstring& err);
std::wstring PlanText(const std::vector<PlanItem>& plan);

// ---- cleaner: temporary files + invalid registry entries ----
struct CleanItem {
    std::wstring id, name, type, detail, location, why;
    ULONGLONG bytes = 0, files = 0; int kind = 0;               // 0 temp folders, 1 recycle bin, 2 registry key, 3 registry value
    bool needsAdmin = false; HKEY root = nullptr; REGSAM view = 0; std::wstring subKey, valueName;
    std::vector<std::wstring> paths;
};
struct CleanScan { std::vector<CleanItem> items; std::wstring coverage; ULONGLONG totalBytes = 0; int registryCount = 0; };
CleanScan ScanCleaner(bool keepRecent);
struct CleanResult { ULONGLONG bytes = 0, files = 0; int ok = 0, skipped = 0, failed = 0; std::wstring firstErr, backupPath; };
void RunClean(const std::vector<CleanItem>& items, bool keepRecent, CleanResult& r);   // temp files are permanently deleted; registry items are backed up first
std::vector<BackupInfo> ListRegBackups();
bool RestoreRegBackup(const std::wstring& path, std::wstring& err, std::wstring& summary);

// ---- large-file and duplicate finder (files.cpp) ----
struct FileEntry { std::wstring path, name, folder, ext; ULONGLONG size = 0, mtime = 0; int group = 0, groupCount = 0; };
struct FileScan { std::vector<FileEntry> files; std::wstring coverage; ULONGLONG scanned = 0, totalBytes = 0, reclaimable = 0; int groups = 0; bool cancelled = false; };
struct FileScanOpts { std::vector<std::wstring> roots; ULONGLONG minSize = 0; bool dupes = false; size_t maxResults = 1000; std::atomic<bool>* cancel = nullptr; std::function<void(const std::wstring&)> progress; };
std::vector<std::wstring> DefaultScanRoots();
void ScanFiles(const FileScanOpts& o, FileScan& out);
bool RecycleFiles(HWND owner, const std::vector<std::wstring>& paths, std::wstring& err);   // to the Recycle Bin (undoable from Explorer)
std::wstring FileDateText(ULONGLONG ft);

// ---- dialogs and shell actions (dialogs.cpp) ----
struct ReportTab { std::wstring name, text; };
struct ReportBtn { std::wstring label; std::function<void(const std::wstring& currentText)> fn; bool closes = false; };
void ShowReport(HWND owner, const std::wstring& title, const std::vector<ReportTab>& tabs, const std::vector<ReportBtn>& buttons);
int  DarkMsg(HWND owner, const std::wstring& title, const std::wstring& text, int kind);   // 0 info, 1 warning, 2 yes/no (default No) -> IDOK/IDYES/IDNO
struct FormField { std::wstring label, value; int type = 0; std::vector<std::wstring> options; };   // 0 edit, 1 combo (value=index), 2 checkbox (value "1"/"0")
bool FormDialog(HWND owner, const std::wstring& title, const std::wstring& okText, std::vector<FormField>& fields, const std::wstring& note = L"");
void SetDarkTitleBar(HWND h, bool dark);
void OpenFileLocation(const std::wstring& program);
void ShowInRegistryEditor(const std::wstring& location);
void OnlineLookup(HWND owner, bool virusTotal, const std::wstring& program, const std::wstring& name);
std::wstring StartupDetailsText(const StartupEntry& e);
void ExportTable(HWND owner, const std::wstring& title, const std::vector<std::wstring>& cols, const std::vector<std::vector<std::wstring>>& rows, const std::wstring& provenance);

} // namespace fsu
