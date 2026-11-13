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

namespace fsu {

std::wstring ExpandEnv(const std::wstring& s);
std::wstring DataDir();                        // %LOCALAPPDATA%\FeatherSystemUtilities (created)
std::wstring Win32Err(DWORD e);                // text + numeric code (F185)
std::wstring NowIsoUtc();
bool FileExistsExpanded(const std::wstring& p);
bool IsElevated();

// ---- file evidence (never loads/executes the target) ----
enum class Sig { Unknown, Valid, Unsigned, Invalid };
struct FileInfo {
    bool exists = false;
    std::wstring publisher, version, signer;
    Sig sig = Sig::Unknown;
    bool catalogSigned = false;
};
FileInfo InspectFile(const std::wstring& expandedPath, bool checkSignature = true);

// ---- startup ----
enum class Toggle { None, RunApproved, Task, ShellBlocked };
struct StartupEntry {
    std::wstring name, state, type, starts, scope, command, program, publisher, version;
    std::wstring signature, warnings, location, firstSeen, key;   // key = stable identity
    // provider-specific source identity
    HKEY root = nullptr; REGSAM view = 0; std::wstring subKey, valueName, clsid, taskPath;
    bool runOnce = false, baseline = false, recent = false, hasWarning = false, microsoft = false;
    Toggle toggle = Toggle::None;
};
struct StartupScan { std::vector<StartupEntry> entries; std::wstring coverage; };
StartupScan ScanStartup();
bool SetStartupEnabled(const StartupEntry& e, bool enable, std::wstring& err);   // backup->apply->verify->journal

struct BackupInfo { std::wstring path, label; FILETIME written{}; };
bool DeleteStartupEntry(const StartupEntry& e, std::wstring& err);                    // registry Run values; backup first
std::vector<BackupInfo> ListBackups();
bool RestoreBackup(const std::wstring& path, std::wstring& err, std::wstring& summary); // validated; never overwrites
bool AddStartupEntry(const std::wstring& name, const std::wstring& command, std::wstring& err); // HKCU Run

// ---- journal (F158) ----
void WriteJournal(const std::wstring& target, const std::wstring& source, const std::wstring& before, const std::wstring& after, const std::wstring& result, bool reversible);
bool JournalWritable();

// ---- services ----
struct ServiceEntry {
    std::wstring name, display, state, start, safety, account, description, warnings, path, ptype, notes, program, publisher, version, signature;
    DWORD pid = 0, stateCode = 0, startType = 0, controls = 0, svcType = 0;
    bool delayed = false, trigger = false, critical = false;
    std::vector<std::wstring> deps;
};
struct ServiceScan { std::vector<ServiceEntry> entries; std::wstring coverage; };
ServiceScan ScanServices();
enum class SvcOp { Start, Stop, Restart, Pause, Resume };
bool ServiceOp(const ServiceEntry& s, SvcOp op, std::wstring& err);          // blocking (<=30 s per wait); run off the UI thread
std::vector<std::wstring> ActiveDependents(const std::wstring& name);
bool SetServiceStartType(const std::wstring& name, DWORD startType, bool delayed, std::wstring& err, std::wstring& before);
bool IsCriticalService(const std::wstring& name);
std::wstring ServiceTabText(const ServiceEntry& s, int tab);                 // 0 General 1 Configuration 2 Recovery 3 Dependencies

// ---- dialogs and shell actions (dialogs.cpp) ----
struct ReportTab { std::wstring name, text; };
struct ReportBtn { std::wstring label; std::function<void(const std::wstring& currentText)> fn; };
void ShowReport(HWND owner, const std::wstring& title, const std::vector<ReportTab>& tabs, const std::vector<ReportBtn>& buttons);
bool AskTwo(HWND owner, const std::wstring& title, const std::wstring& l1, const std::wstring& l2, std::wstring& v1, std::wstring& v2);
void SetDarkTitleBar(HWND h, bool dark);
void OpenFileLocation(const std::wstring& program);                 // explorer /select, never runs the file
void ShowInRegistryEditor(const std::wstring& location);            // location text as shown in the Location column
void OnlineLookup(HWND owner, bool virusTotal, const std::wstring& program, const std::wstring& name);   // asks consent; opens the browser
std::wstring StartupDetailsText(const StartupEntry& e);
std::wstring StartupCopyText(const StartupEntry& e);

} // namespace fsu
