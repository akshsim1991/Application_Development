// Feather Clipboard Manager - platform independent logic (text transforms, smart type detection,
// sensitive-content detection, hashing, UTF-8, index field escaping). No Windows headers here,
// so it is unit-tested natively (tests/core_test.cpp).
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace core {

enum class Xf { Upper, Lower, Title, Sentence, Trim, OneLine, SortLines, UniqueLines, ReverseLines, NumberLines, Plain,
                BlankLines, Base64Enc, Base64Dec, JsonFormat, JsonMin, XmlFormat, SqlFormat, SlashesFwd, SlashesBack, Sha256, UrlEncode, UrlDecode, COUNT };
const wchar_t* XfKey(Xf x);                              // short name used in pipelines: "upper", "json", ...
bool XfFromKey(const std::wstring& key, Xf& out);
// A pipeline is operations separated by |, e.g.  trim | find:foo=>bar | upper . find:a=>b replaces text.
bool RunPipeline(const std::wstring& spec, const std::wstring& text, std::wstring& out, std::wstring& err);
std::wstring FindReplace(const std::wstring& s, const std::wstring& find, const std::wstring& repl, bool caseSensitive);
// {date} {time} {datetime} {year} {iso} {weekday} {month} inside text, from a local time.
std::wstring ExpandTemplate(const std::wstring& text, int y, int mo, int d, int hh, int mm, int ss, int wday);
std::wstring LineDiff(const std::wstring& a, const std::wstring& b);          // "- removed", "+ added", "  same"
std::wstring InspectUnicode(const std::wstring& s, size_t maxChars = 400);     // "U+0041  A"
std::wstring FormatTable(const std::wstring& csvOrTsv);                        // aligned columns, empty when it is not a table
std::wstring MaskSensitive(const std::wstring& s);                             // emails, phone numbers, long digit runs, tokens -> ••••

// Search: words, "exact phrases", ~fuzzy, re:regex, and filters app:name type:link tag:work date:today|yesterday|7d|2026-11-13
struct Query {
    std::vector<std::wstring> words, phrases, fuzzy;   // lower-case
    std::wstring regex, app, type, tag;
    long long fromTime = 0, toTime = 0;               // 0 = no limit
    bool empty() const { return words.empty() && phrases.empty() && fuzzy.empty() && regex.empty() && app.empty() && type.empty() && tag.empty() && !fromTime && !toTime; }
};
Query ParseQuery(const std::wstring& text, long long nowEpoch, long long startOfToday);
bool FuzzyContains(const std::wstring& hayLower, const std::wstring& needleLower);   // tolerates one wrong, missing or extra letter
bool RegexSearch(const std::wstring& pattern, const std::wstring& text);              // bounded; false on a bad pattern


std::wstring Apply(Xf x, const std::wstring& s);
std::wstring Join(const std::vector<std::wstring>& parts, const std::wstring& sep);
std::vector<std::wstring> SplitLines(const std::wstring& s);   // splits on \n, \r\n, \r (no empty trailing element)

enum SmartType { ST_NONE = 0, ST_LINK, ST_EMAIL, ST_PHONE, ST_COLOUR, ST_PATH, ST_JSON };
SmartType Detect(const std::wstring& text);

// true when the text looks like a card number, private key, API token, JWT or password.
bool Sensitive(const std::wstring& text, std::wstring* why = nullptr);
bool Luhn(const std::wstring& digitsOnly);

uint64_t Fnv64(const void* data, size_t n);
uint64_t HashText(const std::wstring& s);

std::wstring Lower(const std::wstring& s);
std::wstring Trim(const std::wstring& s);
bool StartsWith(const std::wstring& s, const wchar_t* p);

std::string ToUtf8(const std::wstring& s);
std::wstring FromUtf8(const std::string& s);   // tolerates a BOM and bad bytes

// Index fields: tab-separated lines, with \t \n \r \\ escaped.
std::wstring EscapeField(const std::wstring& s);
std::wstring UnescapeField(const std::wstring& s);
std::vector<std::wstring> SplitTabs(const std::wstring& line);

std::wstring HexId(uint64_t v);                // 16 lowercase hex digits
uint64_t ParseHex(const std::wstring& s);
std::wstring OneLinePreview(const std::wstring& s, size_t maxChars);

// ---- updates (the version file latest.json published in the releases repository)
struct Latest {
    std::string version, date, url, sha256;
    std::wstring notes;
    long long size = 0;
};
bool ParseLatest(const std::string& json, Latest& out);      // false when it is not a version file
int CompareVersions(const std::string& a, const std::string& b);   // <0 older, 0 same, >0 newer ("2.1.10" > "2.1.9")
std::string Sha256Hex(const std::string& data);              // 64 lowercase hex digits

}  // namespace core
