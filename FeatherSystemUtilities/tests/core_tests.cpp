#include "../src/core/text.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <algorithm>
using namespace fsu;
static int fails = 0, total = 0;
#define CHECK(c) do { ++total; if (!(c)) { ++fails; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

int main() {
    // ParseCommand
    auto a = ParseCommand(L"\"C:\\Program Files\\App\\a.exe\" /min -x");
    CHECK(a.exe == L"C:\\Program Files\\App\\a.exe" && a.args == L"/min -x" && a.quoted);
    std::set<std::wstring> fsx = {L"C:\\Program Files\\App\\a.exe"};
    auto ex = [&](const std::wstring& p) { return fsx.count(p) > 0; };
    auto b = ParseCommand(L"C:\\Program Files\\App\\a.exe /s", ex);
    CHECK(b.exe == L"C:\\Program Files\\App\\a.exe" && b.args == L"/s" && b.exeExists);
    auto c = ParseCommand(L"C:\\x\\tool.exe --opt=1");           // no probe: extension heuristic
    CHECK(c.exe == L"C:\\x\\tool.exe" && c.args == L"--opt=1");
    auto d = ParseCommand(L"rundll32.exe shell32.dll,Foo");
    CHECK(d.exe == L"rundll32.exe" && d.args == L"shell32.dll,Foo");
    auto e = ParseCommand(L"\"C:\\C#\\ü.exe\"");               // '#' and Unicode
    CHECK(e.exe == L"C:\\C#\\ü.exe" && e.args.empty());
    CHECK(ParseCommand(L"   ").exe.empty());
    auto f = ParseCommand(L"\"C:\\unterminated\\a.exe /x");
    CHECK(f.exe == L"C:\\unterminated\\a.exe /x");
    // CSV
    CHECK(CsvField(L"=cmd|' /C calc'!A0") == L"'=cmd|' /C calc'!A0");
    CHECK(CsvField(L"a,b") == L"\"a,b\"");
    CHECK(CsvField(L"say \"hi\"") == L"\"say \"\"hi\"\"\"");
    CHECK(CsvField(L"=1", false) == L"=1");
    CHECK(CsvRow({L"x", L"y,z"}) == L"x,\"y,z\"");
    // Query
    CHECK(MatchesQuery({L"Foo Bar", L"x"}, L"oo b"));
    CHECK(!MatchesQuery({L"Foo"}, L"zzz"));
    CHECK(MatchesQuery({}, L""));
    // Dependency order
    std::map<std::wstring, std::vector<std::wstring>> deps = {
        {L"A", {L"B", L"C"}}, {L"B", {L"C"}}, {L"C", {}}, {L"D", {L"A"}}};
    auto s = StartOrder({L"A"}, deps);
    CHECK(s.ok && s.order == std::vector<std::wstring>({L"C", L"B", L"A"}));
    auto t = StopOrder({L"C"}, deps);   // dependents first: D, A, B, then C
    CHECK(t.ok && t.order.size() == 4 && t.order.front() == L"D" && t.order.back() == L"C");
    deps[L"C"] = {L"A"};                // introduce cycle
    auto cy = StartOrder({L"A"}, deps);
    CHECK(!cy.ok && !cy.cycle.empty());
    auto mi = StartOrder({L"A"}, {{L"A", {L"Ghost"}}});
    CHECK(mi.ok && mi.missing.size() == 1 && mi.missing[0] == L"Ghost");
    // Journal
    JournalEntry je{L"2026-11-13T00:00:00Z", L"user", L"Svc\"X", L"SCM", L"Auto", L"Disabled", L"ok", true};
    auto jl = JournalLine(je);
    CHECK(jl.find("Svc\\\"X") != std::string::npos && jl.back() == '\n' && jl.find("\"reversible\":true") != std::string::npos);
    // Atomic write
    auto p = std::filesystem::temp_directory_path() / "fsu_atomic_test.txt";
    CHECK(AtomicWrite(p.wstring(), "one"));
    CHECK(AtomicWrite(p.wstring(), "two"));
    { std::ifstream in(p); std::string s2; in >> s2; CHECK(s2 == "two"); }
    CHECK(!std::filesystem::exists(p.string() + ".tmp"));
    CHECK(!AtomicWrite(L"/nonexistent_dir_fsu/x.txt", "x"));
    std::filesystem::remove(p);
    // UTF-8 round trip
    CHECK(FromUtf8(ToUtf8(L"ü€ x")) == L"ü€ x");
    // Cleaner helpers
    CHECK(FormatBytes(512) == L"512 B" && FormatBytes(1536) == L"1.5 KB" && FormatBytes(5ull * 1024 * 1024 * 1024) == L"5.0 GB");
    std::wstring mp; CHECK(MuiCachePath(L"C:\\Program Files\\App\\app.exe.FriendlyAppName", mp) && mp == L"C:\\Program Files\\App\\app.exe");
    CHECK(MuiCachePath(L"D:\\x\\lib.DLL.ApplicationCompany", mp) && mp == L"D:\\x\\lib.DLL");
    CHECK(!MuiCachePath(L"@%SystemRoot%\\system32\\foo.dll,-100", mp));
    CHECK(!MuiCachePath(L"LangID", mp));
    CHECK(IsDriveAbsolute(L"c:\\a\\b") && !IsDriveAbsolute(L"%windir%\\x") && !IsDriveAbsolute(L"\\\\srv\\share\\x"));
    // Duplicate keeper selection: one record per group always survives
    { std::vector<DupRec> d = {{1, 100, 20}, {1, 50, 30}, {1, 200, 10}, {2, 5, 8}, {2, 9, 8}, {3, 7, 4}};
      auto o = SelectDuplicatesToRemove(d, 0); CHECK(o.size() == 3 && std::find(o.begin(), o.end(), 1) == o.end() && std::find(o.begin(), o.end(), 3) == o.end() && std::find(o.begin(), o.end(), 5) == o.end());   // keeps oldest
      auto n = SelectDuplicatesToRemove(d, 1); CHECK(n.size() == 3 && std::find(n.begin(), n.end(), 2) == n.end() && std::find(n.begin(), n.end(), 4) == n.end());                                          // keeps newest
      auto s = SelectDuplicatesToRemove(d, 2); CHECK(s.size() == 3 && std::find(s.begin(), s.end(), 2) == s.end());                                                                                           // shortest path
      CHECK(SelectDuplicatesToRemove({}, 0).empty()); }
    // Uninstaller helpers
    CHECK(IsPathUnder(L"C:\\Apps\\Foo\\bin\\x.exe", L"c:\\apps\\foo") && IsPathUnder(L"C:\\Apps\\Foo", L"C:\\Apps\\Foo\\") && !IsPathUnder(L"C:\\Apps\\FooBar\\x", L"C:\\Apps\\Foo") && !IsPathUnder(L"C:\\Apps", L"C:\\Apps\\Foo") && !IsPathUnder(L"C:\\x", L""));
    CHECK(StripVersionSuffix(L"Foo Player 2.3.4 (x64)") == L"Foo Player" && StripVersionSuffix(L"7-Zip 23.01") == L"7-Zip 23.01" && StripVersionSuffix(L"Tool (64-bit)") == L"Tool");
    CHECK(ExtractGuid(L"MsiExec.exe /I{AC76BA86-7AD7-1033-7B44-AC0F074E4100}") == L"{AC76BA86-7AD7-1033-7B44-AC0F074E4100}" && ExtractGuid(L"{not-a-guid}").empty() && ExtractGuid(L"").empty());
    CHECK(MsiRemoveArgs(L"/I{AC76BA86-7AD7-1033-7B44-AC0F074E4100}", false) == L"/X{AC76BA86-7AD7-1033-7B44-AC0F074E4100}" && MsiRemoveArgs(L"/I{G}", true) == L"/X{G} /qn /norestart" && MsiRemoveArgs(L"/X{G} /qb", true) == L"/X{G} /qb");
    CHECK(CompareVersions(L"1.9.0", L"1.10.0") < 0 && CompareVersions(L"2.0", L"2.0.0") == 0 && CompareVersions(L"2.22.0", L"2.21.9") > 0 && CompareVersions(L"1.9.0", L"1.9.0") == 0 && CompareVersions(L"v1.2", L"1.2.1") < 0);
    std::printf("%d/%d passed\n", total - fails, total);
    return fails ? 1 : 0;
}
