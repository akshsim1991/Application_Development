// Portable (no Windows headers) helpers. Unit-tested on any OS.
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>

namespace fsu {

// F035: parse a raw startup/service command line without executing it.
struct ParsedCommand {
    std::wstring exe;        // resolved executable/script path ("" if unparseable)
    std::wstring args;       // remaining arguments, untouched
    bool quoted = false;
    bool exeExists = false;  // only meaningful if an exists-probe was supplied
};
using ExistsFn = std::function<bool(const std::wstring&)>;
ParsedCommand ParseCommand(const std::wstring& raw, const ExistsFn& exists = nullptr);

// F197: CSV field escaping with spreadsheet-formula neutralisation.
std::wstring CsvField(const std::wstring& v, bool neutraliseFormulas = true);
std::wstring CsvRow(const std::vector<std::wstring>& f, bool neutraliseFormulas = true);

// F014: case-insensitive substring match across fields. Empty query matches.
bool MatchesQuery(const std::vector<std::wstring>& fields, const std::wstring& query);

// F111: dependency ordering. deps[name] = services that `name` requires.
// StartOrder: prerequisites first. StopOrder: dependents first.
// Returns false and fills `cycle` if a cycle exists. Unknown deps are reported in `missing`.
struct OrderResult { std::vector<std::wstring> order, missing, cycle; bool ok = true; };
OrderResult StartOrder(const std::vector<std::wstring>& targets,
                       const std::map<std::wstring, std::vector<std::wstring>>& deps);
OrderResult StopOrder(const std::vector<std::wstring>& targets,
                      const std::map<std::wstring, std::vector<std::wstring>>& deps);

// F158: one journal line (JSON). Fields are escaped; secrets must never be passed in.
struct JournalEntry {
    std::wstring timeUtc, actor, target, source, before, after, result;
    bool reversible = false;
};
std::string JournalLine(const JournalEntry& e);

// F203: atomic write (temp file + rename). Returns false on failure; original untouched.
bool AtomicWrite(const std::wstring& path, const std::string& bytes);

// Cleaner helpers (portable, unit-tested).
std::wstring FormatBytes(unsigned long long b);                       // "12.3 MB"
// MuiCache value names look like  C:\Path\app.exe.FriendlyAppName ; returns the file path part.
bool MuiCachePath(const std::wstring& valueName, std::wstring& path);
// True for "X:\..." paths (a drive-letter absolute path with no unresolved %vars%).
bool IsDriveAbsolute(const std::wstring& p);

// Duplicate-group keeper selection. For each group (all records sharing `group`) exactly one record is kept;
// the returned indices are the records proposed for removal. mode: 0 keep oldest, 1 keep newest, 2 keep shortest path.
struct DupRec { int group; unsigned long long mtime; size_t pathLen; };
std::vector<size_t> SelectDuplicatesToRemove(const std::vector<DupRec>& recs, int mode);

// Uninstaller helpers (portable, unit-tested).
bool IsPathUnder(const std::wstring& path, const std::wstring& root);          // case-insensitive, whole path components only ("C:\\Apps\\Foo" is not under "C:\\Apps\\Fo")
std::wstring StripVersionSuffix(std::wstring name);                           // "Foo 2.3.4 (x64)" -> "Foo"
std::wstring ExtractGuid(const std::wstring& s);                              // first {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} or ""
std::wstring MsiRemoveArgs(const std::wstring& args, bool silent);            // "/I{GUID}" -> "/X{GUID}", adds /qn /norestart when silent

// Version compare for "1.9.0"-style strings: -1, 0, 1 (missing parts = 0; non-numeric parts ignored).
int CompareVersions(const std::wstring& a, const std::wstring& b);

std::string ToUtf8(const std::wstring& w);
std::wstring FromUtf8(const std::string& s);
std::wstring Lower(const std::wstring& s);

} // namespace fsu
