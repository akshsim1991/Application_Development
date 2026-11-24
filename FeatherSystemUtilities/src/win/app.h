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

#define FSU_VERSION L"3.9.0"

namespace fsu {

UINT DpiForWindow(HWND h);                      // Windows 7 compatible (GetDpiForWindow only exists on Windows 10 1607+)
DWORD WindowsBuild();                          // real build number (0 if unknown)
bool IsProcessRunning(const wchar_t* exeName);
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
    int theme = 4, density = 1, startPage = 0;
    bool hideSystem = true, confirmBatch = true, maximized = false, keepRecent = true;
    bool autoUpdate = true; long long lastUpdateCheck = 0;                // seconds since 1970 (UTC)
    int largeMinMB = 100, dupMinKB = 1024; std::vector<std::wstring> scanRoots;   // empty = default user folders
    std::vector<std::wstring> exclusions;                                // folders/files never scanned or cleaned
    std::vector<std::wstring> ignored;                                   // "kind|id|label" findings the user chose to ignore
    std::wstring skipVersion; long long deferUntil = 0;                  // update prompt: skipped version / "remind me later" (seconds since 1970)
    bool deviceCheck = true;                                             // look for device problems when Feather starts (read-only) and offer fixes
    std::wstring updateChannel = L"stable", lastSeenVersion; bool webviewPrompt = true;   // update channel; last version whose What's-new was shown; ask about the WebView2 runtime when missing
    std::wstring uiTheme = L"auto";                                     // new UI: auto | light | dark
    bool betaFeatures = false; std::map<std::string, int> flags;          // feature flags: explicit per-flag overrides (0/1); beta flags follow betaFeatures otherwise
    bool liveMetrics = false;                                           // optional low-frequency CPU/RAM/network readout on Today (off by default)
    bool onlineAdvice = true; long long adviceCheck = 0;                // download newer startup/service recommendations when online
    std::vector<std::wstring> pendingVerify;                             // "hwid|title|time" drivers installed but not yet verified after restart
    int logRetentionDays = 0; bool quarantineLeftovers = true;           // 0 = keep the journal forever
    int winX = CW_USEDEFAULT, winY = CW_USEDEFAULT, winW = 1360, winH = 780;
    std::map<std::wstring, std::vector<int>> colw;   // per page column widths (96-dpi units)
};
Settings& Cfg();
bool IsExcluded(const std::wstring& path);   // true when the path is inside a user-defined exclusion
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
int  SvcStopIfRunning(const std::wstring& name, std::wstring& err);   // 1 stopped, 0 was not running, -1 failed
bool SvcStartByName(const std::wstring& name, std::wstring& err);
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

// ---- programs: full uninstaller (programs.cpp) ----
struct ProgramEntry {
    std::wstring name, publisher, version, installDate, installLocation, uninstallString, quietUninstallString, displayIcon, type, scope, arch, keyName, key, source;
    ULONGLONG sizeBytes = 0; bool isMsi = false, canUninstall = true; HKEY root = nullptr; REGSAM view = 0; std::wstring productCode;
};
struct ProgramScan { std::vector<ProgramEntry> entries; std::wstring coverage; int skipped = 0; };
ProgramScan ScanPrograms();
struct UninstallPlan { std::wstring file, args, note; bool available = false, elevate = false; };
UninstallPlan PlanUninstall(const ProgramEntry& e, bool silent);
struct UninstallResult { bool started = false, removed = false; DWORD exitCode = 0; std::wstring note; };
void RunUninstall(const ProgramEntry& e, bool silent, UninstallResult& r, std::atomic<bool>* cancel);   // blocking; run off the UI thread
bool ProgramStillInstalled(const ProgramEntry& e);
bool IsProtectedPath(const std::wstring& path);
struct LeftoverCtx;
std::vector<struct CleanItem> FindLeftovers(const ProgramEntry& e, bool includeUninstallKey);   // evidence-based: install folder, name-matched data folders, vendor keys, startup/service/task/shell entries pointing into the install folder

// ---- cleaner: temporary files + invalid registry entries ----
struct CleanItem {
    std::wstring id, name, type, detail, location, why;
    ULONGLONG bytes = 0, files = 0; int kind = 0;               // 0 temp folders, 1 recycle bin, 2 registry key, 3 registry value, 4 run command, 5 open Windows tool
    int risk = 0;                                                // 0 low, 1 medium, 2 high
    int timeoutSec = 30;                                         // for kind 4 commands
    // kinds 6 = delete a whole leftover folder (incl. the folder itself), 8 = delete service (cmd = service name), 9 = delete scheduled task (cmd = task path)
    std::vector<std::wstring> services; std::wstring pattern, cmd; int minAgeDays = 0; bool ignoreRecent = false;
    bool needsAdmin = false; HKEY root = nullptr; REGSAM view = 0; std::wstring subKey, valueName;
    std::vector<std::wstring> paths;
};
struct CleanScan { std::vector<CleanItem> items; std::wstring coverage; ULONGLONG totalBytes = 0; int registryCount = 0; };
CleanScan ScanCleaner(bool keepRecent);
CleanScan ScanDeep(std::atomic<bool>* cancel, const std::function<void(const std::wstring&)>& progress);   // walks all fixed drives for stray junk (kind 7 file lists) + developer/app caches
struct CleanResult { ULONGLONG bytes = 0, files = 0; int ok = 0, skipped = 0, failed = 0; std::wstring firstErr, backupPath; };
void RunClean(const std::vector<CleanItem>& items, bool keepRecent, CleanResult& r, bool quarantine = false);   // quarantine: leftover folders and stray files are moved to the restorable quarantine instead of deleted
struct PreviewRow { std::wstring path; ULONGLONG bytes = 0, mtime = 0; };
void PreviewClean(const CleanItem& it, bool keepRecent, size_t maxRows, std::vector<PreviewRow>& rows, ULONGLONG& totalFiles);   // dry run: the exact files a clean would remove   // temp files are permanently deleted; registry items are backed up first
std::vector<BackupInfo> ListRegBackups();
bool RestoreRegBackup(const std::wstring& path, std::wstring& err, std::wstring& summary);

// ---- phase 2 engine (phase2.cpp): drivers, browsers, privacy checklist, event logs, redaction ----
struct DriverRec { int i = 0; std::wstring name, maker, version, date, provider, inf, cls, devId, hwid, status; bool signed_ = true, missing = false; int problem = 0; };
const wchar_t* ProblemText(int code);
std::vector<DriverRec> ListDrivers(std::wstring& err);
struct ComputerInfo { std::wstring maker, model, supportUrl; };
ComputerInfo GetComputerInfo();
struct BrowserCat { std::wstring id, label, note, base; std::vector<std::wstring> paths; ULONGLONG bytes = 0, files = 0; bool deletable = true; };
struct BrowserProfile { std::wstring key, browser, profile, display, dir; std::vector<BrowserCat> cats; };
std::vector<BrowserProfile> ScanBrowsers();
bool BrowserRunning(const std::wstring& key);
bool ClearBrowserCategory(const BrowserProfile& bp, const BrowserCat& c, ULONGLONG& bytes, std::wstring& err);
struct PrivacyItem { std::wstring id, title, state, detail, why; bool good = true; };
std::vector<PrivacyItem> PrivacyChecklist();
bool OpenPrivacySetting(const std::wstring& id);
std::vector<PrivacyItem> SecurityChecklist();
bool OpenSecuritySetting(const std::wstring& id);
struct TaskInfo { std::wstring path, name, folder, state, triggers, command, author, lastRun, nextRun, why; long lastResult = 0; bool enabled = true, microsoft = false, startup = false, hidden = false, canToggle = true; };
std::vector<TaskInfo> ListAllTasks(std::wstring& err);
bool SetTaskEnabledByPath(const std::wstring& path, bool enable, std::wstring& err);
bool OpenBrowserPrivacy(const std::wstring& key, std::wstring& err);
struct EventRec { std::wstring time, log, provider, message; int level = 0; DWORD id = 0; ULONGLONG sort = 0; };
std::vector<EventRec> QueryEvents(int hours, const std::vector<std::wstring>& logs, const std::vector<int>& levels, size_t maxN, std::wstring& err);
std::wstring Redact(std::wstring s, bool maskPaths);

// ---- diagnostics: local log, crash filter, self-test (log.cpp, selftest.cpp) ----
void Log(const char* level, const std::wstring& text);
std::vector<std::string> LogTail(size_t n);
void LogClear();
void InstallCrashLog();
struct TestResult { std::wstring name, detail; bool ok = false; int ms = 0; };
std::vector<TestResult> RunSelfTest(bool network, const std::function<void(const std::wstring&)>& progress);
std::wstring SelfTestReport(const std::vector<TestResult>& r);
bool WebUiAvailable();

// ---- device diagnosis and repair (device.cpp) ----
struct DeviceEvent { int id = 0; std::wstring time, problem, status, driver, service; };
struct DeviceDiag { bool found = false, started = false, driverLoaded = false, hasProblem = false, needsRestart = false, disableable = false, hasNtStatus = false, disabledByUser = false, serviceFound = false, serviceFileExists = false;
    int problem = 0; unsigned status = 0, ntStatus = 0; DWORD serviceStartType = 0, serviceState = 0; std::wstring instanceId, ntText, service, desc, cls, upperFilters, lowerFilters, infPath, servicePath; std::vector<DeviceEvent> events; };
struct DeviceFixResult { bool ok = false, fixed = false, found = true, needsRestart = false; int problemBefore = 0, problemAfter = 0; std::wstring message; };
DeviceDiag DiagnoseDevice(const std::wstring& instanceId);
DeviceFixResult FixDevice(const std::wstring& instanceId, const std::wstring& cls, const std::wstring& name, const std::string& action);   // rescan | restart | enable | enableService | reinstall
bool DeviceActionAllowed(const std::wstring& cls, const std::string& action);
const wchar_t* NtStatusText(unsigned s);

// ---- phase 3 engine (phase3.cpp) ----
struct OrgInfo { bool domain = false, azureAd = false, mdm = false, policies = false; std::wstring domainName; };
OrgInfo GetOrgInfo();
struct PerfSample { double cpu = 0; ULONGLONG memTotal = 0, memAvail = 0, netRx = 0, netTx = 0; };
PerfSample SamplePerf();
struct TrendPt { std::wstring date; ULONGLONG free = 0, total = 0; };
void RecordStorageTrend();
std::vector<TrendPt> StorageTrend(const std::wstring& drive);
struct WuLast { std::wstring detect, install; };
WuLast WuLastTimes();
bool RunCapture(const std::wstring& cmd, std::string& out, DWORD timeoutMs, DWORD* exitCode = nullptr);
struct ImpactRec { int boots = 0; ULONGLONG sumDeg = 0, maxTotal = 0; };
std::map<std::wstring, ImpactRec> StartupImpact(std::wstring& err);
struct Hotfix { std::wstring kb, desc, date, by; };
std::vector<Hotfix> ListHotfixes(std::wstring& err);
struct LicenseInfo { bool valid = false; std::wstring state, edition, name, expires, id; int devices = 0; };
LicenseInfo LicenseStatus();
bool LicenseActivate(const std::string& token, std::wstring& err);
void LicenseRemove();

// ---- feature flags (settings.cpp) and the administrator helper (broker.cpp) ----
struct FlagInfo { const char* id; const char* title; const char* why; bool beta; };
const std::vector<FlagInfo>& FeatureFlags();
bool FeatureOn(const char* id);
bool BrokerAllows(const std::string& cmd);
int BrokerMain(const std::wstring& cmdline, const std::function<std::string(const std::string&, const std::string&)>& exec);
bool BrokerStart(std::wstring& err);
bool BrokerConnected();
bool BrokerCall(const std::string& cmd, const std::string& argsJson, std::string& result, std::wstring& err);
void BrokerStop();
std::string BrokerExecute(const std::string& cmd, const std::string& requestJson);   // webui.cpp: runs a whitelisted handler inside the elevated helper

// ---- recommendations (advice.cpp): "can this be disabled?" ----
struct Advice { std::wstring verdict, reason, source; };   // verdict: safe | optional | caution | keep | unknown
struct AdviceInfo { std::wstring version, updated, source; int startupRules = 0, serviceRules = 0; };
AdviceInfo AdviceStatus();
Advice AdviceForStartup(const struct StartupEntry& e, bool fileMissing);
Advice AdviceForService(const struct ServiceEntry& s);
bool AdviceRefresh(AdviceInfo& info, std::wstring& err);   // downloads a newer knowledge base when online
bool FetchAdviceJson(std::string& body, std::wstring& err);   // update.cpp
bool FetchRepoFile(const wchar_t* name, std::string& body, std::wstring& err);   // any small published JSON next to latest.json
struct DeviceRule { std::wstring id, title, diagnosis, severity; std::vector<std::wstring> steps, then, advice; bool bios = false; };
bool DeviceRuleFor(const std::wstring& hwid, const std::wstring& cls, int problem, const std::wstring& ntHex, const std::wstring& name, DeviceRule& out);
std::wstring DeviceRulesVersion();
struct BiosInfo { std::wstring vendor, version, date; };
BiosInfo GetBiosInfo();
bool InSafeMode(int* kind = nullptr);                        // 1 minimal, 2 with networking

// ---- quarantine (quarantine.cpp): restorable holding area for leftovers and stray files ----
struct QItem { std::wstring id, orig, stored, source, time; ULONGLONG bytes = 0; bool isDir = false; };
bool QuarantinePath(const std::wstring& path, const std::wstring& source, std::wstring& err, bool journal = true);
std::vector<QItem> QuarantineList();
bool QuarantineRestore(const std::wstring& id, std::wstring& err);
bool QuarantinePurge(const std::wstring& id, std::wstring& err);

// ---- large-file and duplicate finder (files.cpp) ----
struct FileEntry { std::wstring path, name, folder, ext, hash; ULONGLONG size = 0, mtime = 0; int group = 0, groupCount = 0; bool verified = false; };
struct FileScan { std::vector<FileEntry> files; std::wstring coverage; ULONGLONG scanned = 0, totalBytes = 0, reclaimable = 0; int groups = 0; bool cancelled = false; };
struct FileScanOpts { std::vector<std::wstring> roots; std::vector<std::wstring> exts; int minAgeDays = 0; ULONGLONG minSize = 0; bool dupes = false, empty = false; size_t maxResults = 1000; std::atomic<bool>* cancel = nullptr; std::function<void(const std::wstring&)> progress; };
std::vector<std::wstring> DefaultScanRoots();
void ScanFiles(const FileScanOpts& o, FileScan& out);
bool RecycleFiles(HWND owner, const std::vector<std::wstring>& paths, std::wstring& err);
bool IsFolderTreeEmpty(const std::wstring& dir);   // true only if the folder tree contains no files, links or unreadable parts
bool VerifyIdentical(const std::wstring& a, const std::wstring& b);   // byte-for-byte comparison right before deleting a duplicate
bool DeleteFilesPermanently(const std::vector<std::wstring>& paths, std::wstring& err);   // not recoverable   // to the Recycle Bin (undoable from Explorer)
std::wstring FileDateText(ULONGLONG ft);

// ---- updates (update.cpp): the only network feature; reads one small manifest over HTTPS ----
struct UpdateInfo { bool ok = false; std::wstring version, date, notes, url, sha256, signer, error, prevVersion, prevUrl, prevSha; ULONGLONG size = 0, prevSize = 0; };
bool FetchLatestVersion(UpdateInfo& u, bool beta = false);   // beta: look for latest-beta.json first                                     // manifest like Feather PDF's latest.json
bool DownloadUpdate(const UpdateInfo& u, std::wstring& path, std::wstring& err, std::atomic<bool>* cancel, const std::function<void(ULONGLONG, ULONGLONG)>& progress);   // verifies size + SHA-256
bool LaunchUpdateInstaller(const std::wstring& installerPath, std::wstring& err);   // elevated silent install, then relaunches this app
bool IsInstalledCopy();

// ---- dialogs and shell actions (dialogs.cpp) ----
struct ReportTab { std::wstring name, text; };
struct ReportBtn { std::wstring label; std::function<void(const std::wstring& currentText)> fn; bool closes = false; };
void ShowReport(HWND owner, const std::wstring& title, const std::vector<ReportTab>& tabs, const std::vector<ReportBtn>& buttons);
int  DarkMsg(HWND owner, const std::wstring& title, const std::wstring& text, int kind);   // 0 info, 1 warning, 2 yes/no (default No) -> IDOK/IDYES/IDNO
struct FormField { std::wstring label, value; int type = 0; std::vector<std::wstring> options; };   // 0 edit, 1 combo (value=index), 2 checkbox (value "1"/"0")
bool FormDialog(HWND owner, const std::wstring& title, const std::wstring& okText, std::vector<FormField>& fields, const std::wstring& note = L"");
void SetDarkTitleBar(HWND h, bool dark);
void OpenFileLocation(const std::wstring& program);
bool OpenFileLocationEx(const std::wstring& program, std::wstring& err);   // no dialogs; verifies that an Explorer window for the folder appeared
int RunWebUi(HINSTANCE inst, int show, int firstPage, HANDLE* instMutex);   // webui.cpp: 0 = ran, 1 = WebView2 unavailable
void ShowInRegistryEditor(const std::wstring& location);
void OnlineLookup(HWND owner, bool virusTotal, const std::wstring& program, const std::wstring& name);
std::wstring StartupDetailsText(const StartupEntry& e);
void ExportTable(HWND owner, const std::wstring& title, const std::vector<std::wstring>& cols, const std::vector<std::vector<std::wstring>>& rows, const std::wstring& provenance);

} // namespace fsu
