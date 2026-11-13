// Unit tests for src/core.cpp. Build and run natively: tests/run.sh
#include "../src/core.h"
#include <cstdio>
#include <cstdlib>
using namespace core;

static int fails = 0, total = 0;
#define CHECK(c) do { total++; if (!(c)) { fails++; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main() {
    // transforms
    CHECK(Apply(Xf::Upper, L"abc Def") == L"ABC DEF");
    CHECK(Apply(Xf::Lower, L"ABC Def") == L"abc def");
    CHECK(Apply(Xf::Title, L"hello wORLD it's me") == L"Hello World It's Me");
    CHECK(Apply(Xf::Sentence, L"hELLO there. how ARE you") == L"Hello there. How are you");
    CHECK(Apply(Xf::Trim, L"\n  a  \r\n b \n\n") == L"a\r\nb");
    CHECK(Apply(Xf::OneLine, L"a\r\n  b\n\n c") == L"a b c");
    CHECK(Apply(Xf::SortLines, L"b\nA\nc") == L"A\r\nb\r\nc");
    CHECK(Apply(Xf::UniqueLines, L"a\nb\na\nc\nb") == L"a\r\nb\r\nc");
    CHECK(Apply(Xf::ReverseLines, L"1\n2\n3") == L"3\r\n2\r\n1");
    CHECK(Apply(Xf::NumberLines, L"x\ny") == L"1. x\r\n2. y");
    CHECK(Join({L"a", L"b", L"c"}, L", ") == L"a, b, c");
    CHECK(SplitLines(L"a\r\nb\nc\rd").size() == 4);
    CHECK(SplitLines(L"").empty());

    // smart types
    CHECK(Detect(L"https://example.com/a?b=1") == ST_LINK);
    CHECK(Detect(L"www.example.com") == ST_LINK);
    CHECK(Detect(L"https://example.com is nice") == ST_NONE);
    CHECK(Detect(L"someone@example.com") == ST_EMAIL);
    CHECK(Detect(L"not an@email") == ST_NONE);
    CHECK(Detect(L"a@b.c") == ST_NONE);
    CHECK(Detect(L"+91 98765 43210") == ST_PHONE);
    CHECK(Detect(L"(555) 123-4567") == ST_PHONE);
    CHECK(Detect(L"12345") == ST_NONE);
    CHECK(Detect(L"#ff8800") == ST_COLOUR);
    CHECK(Detect(L"#abc") == ST_COLOUR);
    CHECK(Detect(L"#abcd1") == ST_NONE);
    CHECK(Detect(L"rgb(1, 2, 3)") == ST_COLOUR);
    CHECK(Detect(L"C:\\Users\\me\\file.txt") == ST_PATH);
    CHECK(Detect(L"\\\\server\\share\\x") == ST_PATH);
    CHECK(Detect(L"{\"a\": [1, 2, {\"b\": \"c}\"}]}") == ST_JSON);
    CHECK(Detect(L"{ unbalanced") == ST_NONE);
    CHECK(Detect(L"[1, 2, 3]") == ST_JSON);
    CHECK(Detect(L"just some words") == ST_NONE);

    // sensitive
    std::wstring why;
    CHECK(Sensitive(L"4111 1111 1111 1111", &why) && why == L"card number");
    CHECK(Sensitive(L"4111-1111-1111-1111"));
    CHECK(!Sensitive(L"4111 1111 1111 1112"));
    CHECK(!Sensitive(L"0000000000000000"));
    CHECK(Sensitive(L"-----BEGIN RSA PRIVATE KEY-----\nabc\n-----END RSA PRIVATE KEY-----"));
    CHECK(Sensitive(L"password = hunter2hunter2"));
    CHECK(Sensitive(L"api_key: \"abcdef123456\""));
    CHECK(!Sensitive(L"password"));
    CHECK(Sensitive(L"ghp_abcdefghijklmnopqrstuvwxyz0123456789"));
    CHECK(Sensitive(L"AKIAABCDEFGHIJKLMNOP"));
    CHECK(Sensitive(L"sk-abcdefghijklmnopqrstuvwx"));
    CHECK(Sensitive(L"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.abcdefghijk"));
    CHECK(Sensitive(L"Tr0ub4dor&3x", &why) && why == L"password");
    CHECK(!Sensitive(L"Hello world, this is fine"));
    CHECK(!Sensitive(L"https://Example.com/Path1!"));
    CHECK(!Sensitive(L"someone@Example.com1"));
    CHECK(!Sensitive(L"C:\\Users\\Me\\File1.txt"));
    CHECK(!Sensitive(L"just a note"));
    CHECK(Luhn(L"4532015112830366"));

    // hashing, utf8, escapes
    CHECK(HashText(L"abc") == HashText(L"abc"));
    CHECK(HashText(L"abc") != HashText(L"abd"));
    std::wstring u = L"h\u00e9llo \u0928\u092e\u0938\u094d\u0924\u0947 \u20ac";
    CHECK(FromUtf8(ToUtf8(u)) == u);
    CHECK(FromUtf8("\xEF\xBB\xBF" "abc") == L"abc");
    CHECK(FromUtf8("ab\xE2\x82") .size() >= 2);   // truncated sequence does not crash
    std::wstring e = L"a\tb\nc\\d\re";
    CHECK(UnescapeField(EscapeField(e)) == e);
    CHECK(EscapeField(e).find(L'\t') == std::wstring::npos && EscapeField(e).find(L'\n') == std::wstring::npos);
    CHECK(SplitTabs(L"a\t\tb").size() == 3);
    CHECK(ParseHex(HexId(0x1234abcdULL)) == 0x1234abcdULL);
    CHECK(HexId(255) == L"00000000000000ff");
    CHECK(OneLinePreview(L"  a \r\n  b\t\tc  ", 100) == L"a b c");
    CHECK(OneLinePreview(L"abcdefgh", 3) == L"abc");

    // updates: SHA-256 (known vectors), version order, latest.json
    CHECK(Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(CompareVersions("2.1.10", "2.1.9") > 0);
    CHECK(CompareVersions("2.1.0", "2.1") == 0);
    CHECK(CompareVersions("2.0.9", "2.1.0") < 0);
    CHECK(CompareVersions("3", "2.9.9") > 0);
    Latest lt;
    CHECK(ParseLatest("{\"version\":\"2.2.0\",\"date\":\"1 Dec 2026\",\"notes\":\"Faster \\u2014 better\\nline 2\",\"windows\":{\"url\":\"https://x/y.exe\",\"sha256\":\"ab12\",\"size\":12345}}", lt));
    CHECK(lt.version == "2.2.0" && lt.url == "https://x/y.exe" && lt.sha256 == "ab12" && lt.size == 12345 && lt.notes.find(L'\u2014') != std::wstring::npos);
    CHECK(!ParseLatest("not json", lt));
    CHECK(!ParseLatest("{\"date\":\"x\"}", lt));

    // developer tools and pipelines
    CHECK(Apply(Xf::BlankLines, L"a\n\n\n\nb\n   \n\nc\n\n") == L"a\r\n\r\nb\r\n\r\nc");
    CHECK(Apply(Xf::Base64Enc, L"Hello, World!") == L"SGVsbG8sIFdvcmxkIQ==");
    CHECK(Apply(Xf::Base64Enc, L"ab") == L"YWI=");
    CHECK(Apply(Xf::Base64Dec, L"SGVsbG8sIFdvcmxkIQ==") == L"Hello, World!");
    CHECK(Apply(Xf::Base64Dec, L"not base64!") == L"not base64!");
    CHECK(Apply(Xf::JsonFormat, L"{\"a\":1,\"b\":[1,2],\"c\":{}}") == L"{\r\n  \"a\": 1,\r\n  \"b\": [\r\n    1,\r\n    2\r\n  ],\r\n  \"c\": {}\r\n}");
    CHECK(Apply(Xf::JsonMin, L"{ \"a\" : 1 ,\n \"b\": \"x y\" }") == L"{\"a\":1,\"b\":\"x y\"}");
    CHECK(Apply(Xf::JsonFormat, L"{broken") == L"{broken");
    CHECK(Apply(Xf::XmlFormat, L"<a><b>t</b><c/><d><e>1</e></d></a>") == L"<a>\r\n  <b>t</b>\r\n  <c/>\r\n  <d>\r\n    <e>1</e>\r\n  </d>\r\n</a>");
    CHECK(Apply(Xf::XmlFormat, L"<a><b></a>") == L"<a><b></a>");
    CHECK(Apply(Xf::SqlFormat, L"select a, b from t where x = 1 order by a") == L"SELECT a, b\r\nFROM t\r\nWHERE x = 1\r\nORDER BY a");
    CHECK(Apply(Xf::SlashesFwd, L"C:\\a\\b") == L"C:/a/b");
    CHECK(Apply(Xf::SlashesBack, L"C:/a/b") == L"C:\\a\\b");
    CHECK(Apply(Xf::Sha256, L"abc") == L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Apply(Xf::UrlEncode, L"a b&c=d/\u00e9") == L"a%20b%26c%3Dd%2F%C3%A9");
    CHECK(Apply(Xf::UrlDecode, L"a%20b%26c%3Dd%2F%C3%A9+x") == L"a b&c=d/\u00e9 x");
    CHECK(FindReplace(L"Foo foo FOO", L"foo", L"x", false) == L"x x x");
    CHECK(FindReplace(L"Foo foo FOO", L"foo", L"x", true) == L"Foo x FOO");
    std::wstring pout, perr;
    CHECK(RunPipeline(L"trim | find:cat=>dog | upper", L"  the cat sat  ", pout, perr) && pout == L"THE DOG SAT");
    CHECK(!RunPipeline(L"trim | nonsense", L"x", pout, perr) && perr.find(L"nonsense") != std::wstring::npos);
    CHECK(ExpandTemplate(L"Date {date} {time} {iso} {weekday} {month} {year}", 2026, 11, 3, 9, 5, 0, 2) == L"Date 03/11/2026 09:05 2026-11-03 Tuesday November 2026");
    CHECK(LineDiff(L"a\nb\nc", L"a\nc\nd") == L"  a\r\n- b\r\n  c\r\n+ d\r\n");
    CHECK(InspectUnicode(L"A\t\u20ac").find(L"U+20AC") != std::wstring::npos && InspectUnicode(L"A").find(L"U+0041  A") == 0);
    CHECK(FormatTable(L"name,qty\napple,3\nfig,12").find(L"apple") != std::wstring::npos && FormatTable(L"just text").empty());
    CHECK(FormatTable(L"a\tb\n1\t2").find(L"\u2502") != std::wstring::npos);
    CHECK(MaskSensitive(L"mail me at john@example.com or 9876543210").find(L"john@") == std::wstring::npos && MaskSensitive(L"hello world") == L"hello world");

    // search queries
    Query q = ParseQuery(L"invoice \"exact phrase\" ~colour app:Chrome type:link date:7d tag:Work", 1000000, 900000);
    CHECK(q.words.size() == 1 && q.words[0] == L"invoice" && q.phrases.size() == 1 && q.phrases[0] == L"exact phrase" && q.fuzzy.size() == 1 && q.fuzzy[0] == L"colour");
    CHECK(q.app == L"chrome" && q.type == L"link" && q.tag == L"work" && q.fromTime == 1000000 - 7 * 86400);
    Query q2 = ParseQuery(L"date:today", 1000000, 900000);
    CHECK(q2.fromTime == 900000 && q2.words.empty());
    Query q3 = ParseQuery(L"re:^foo.*bar$", 0, 0);
    CHECK(q3.regex == L"^foo.*bar$");
    CHECK(RegexSearch(L"fo+\\s*bar", L"xx FOOO  bar yy") && !RegexSearch(L"fo+\\s*bar", L"nothing") && !RegexSearch(L"([unclosed", L"x"));
    CHECK(FuzzyContains(L"the quick brown fox", L"quikc"));
    CHECK(FuzzyContains(L"the quick brown fox", L"quik") && FuzzyContains(L"the quick brown fox", L"quicck") && FuzzyContains(L"colour", L"color") && !FuzzyContains(L"hello", L"zzzzz"));

    std::printf("%d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}
