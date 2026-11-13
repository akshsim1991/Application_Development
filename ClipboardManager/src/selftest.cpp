// Self-test mode:  FeatherClipboardManager.exe --selftest result.txt
// Runs against a temporary data folder (never your real history) and writes PASS/FAIL lines.
#include "app.h"
#include <psapi.h>

namespace {
std::wstring g_log;
int g_fail = 0, g_total = 0;
void Check(bool ok, const wchar_t* what) {
    g_total++;
    if (!ok) g_fail++;
    g_log += std::wstring(ok ? L"PASS  " : L"FAIL  ") + what + L"\r\n";
}
void Note(const std::wstring& s) { g_log += L"NOTE  " + s + L"\r\n"; }
std::wstring TempRoot() {
    wchar_t t[MAX_PATH];
    GetTempPathW(MAX_PATH, t);
    return std::wstring(t) + L"FCM-selftest-" + std::to_wstring(GetTickCount64());
}
void RemoveTree(const std::wstring& dir) {
    std::wstring d = dir + L'\0';
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = d.c_str();
    op.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION | FOF_SILENT;
    SHFileOperationW(&op);
}
}  // namespace

int RunSelfTest(const std::wstring& outFile) {
    std::wstring root = TempRoot();
    SetEnvironmentVariableW(L"FCM_DATA_ROOT", root.c_str());
    CoInitialize(nullptr);
    GdiplusStart();
    g_set = Settings();
    Check(g_store.Init(), L"history folder is created and writable");
    Check(g_store.dir.find(L"Clipboard contents") != std::wstring::npos, L"history is kept in a Clipboard contents folder");

    bool isNew = false;
    uint64_t a = g_store.AddText(L"Hello Wörld \u0928\u092e\u0938\u094d\u0924\u0947 line1\nline2", "<b>hi</b>", "{\\rtf1 hi}", L"test.exe", 0, &isNew);
    Check(a && isNew, L"text is stored");
    uint64_t b = g_store.AddText(L"Second item about invoices", "", "", L"test.exe", 0, &isNew);
    Check(b && isNew && g_store.items.size() == 2, L"a different text is a new item");
    uint64_t a2 = g_store.AddText(L"Hello Wörld \u0928\u092e\u0938\u094d\u0924\u0947 line1\nline2", "", "", L"other.exe", 0, &isNew);
    Check(a2 == a && !isNew && g_store.items.size() == 2, L"an identical copy only moves the old item to the top");
    Check(g_store.items.back().id == a, L"the bumped duplicate is now the newest");
    Check((g_store.Find(a)->flags & F_HTML) && (g_store.Find(a)->flags & F_RTF), L"HTML and RTF formatting are kept next to the text");
    Check(g_store.ReadText(*g_store.Find(a)).find(L"\u0928\u092e\u0938\u094d\u0924\u0947") != std::wstring::npos, L"Unicode text reads back unchanged");

    Check(g_store.Matches(*g_store.Find(b), core::ParseQuery(L"invoices", 0, 0)), L"search finds a word");
    Check(g_store.Matches(*g_store.Find(b), core::ParseQuery(L"second invo", 0, 0)), L"search needs every word, case-insensitive");
    Check(!g_store.Matches(*g_store.Find(b), core::ParseQuery(L"nothing", 0, 0)), L"search rejects what is not there");
    Check(g_store.Matches(*g_store.Find(a), core::ParseQuery(L"app:other", 0, 0)), L"search also matches the program name");

    Check(g_store.Matches(*g_store.Find(b), core::ParseQuery(L"\"second item\"", 0, 0)) && !g_store.Matches(*g_store.Find(b), core::ParseQuery(L"\"item second\"", 0, 0)), L"an exact phrase must match in order");
    Check(g_store.Matches(*g_store.Find(b), core::ParseQuery(L"re:inv.ices$", 0, 0)) && g_store.Matches(*g_store.Find(b), core::ParseQuery(L"~invoces", 0, 0)), L"regular-expression and fuzzy search");
    Check(!g_store.Matches(*g_store.Find(b), core::ParseQuery(L"type:link", 0, 0)) && g_store.Matches(*g_store.Find(b), core::ParseQuery(L"type:text app:test", 0, 0)), L"type and application filters");
    std::vector<std::wstring> files = {L"C:\\Docs\\report.docx", L"C:\\Docs\\data.xlsx"};
    uint64_t f = g_store.AddFiles(files, 1, L"explorer.exe", &isNew);
    Check(f && g_store.ReadFiles(*g_store.Find(f)).size() == 2, L"a list of copied files is stored (references, not contents)");

    // picture
    {
        Gdiplus::Bitmap bmp(64, 48, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bmp);
        g.Clear(Gdiplus::Color(255, 10, 120, 200));
        std::wstring tmp = JoinPath(g_store.dir, L"incoming-test.png.tmp");
        CLSID png = EncoderFor(L"image/png");
        bool saved = bmp.Save(tmp.c_str(), &png, nullptr) == Gdiplus::Ok;
        Check(saved, L"GDI+ can write a PNG");
        std::string bytes;
        ReadFileBytes(tmp, bytes);
        uint64_t p = g_store.AddPictureFile(tmp, 64, 48, core::Fnv64(bytes.data(), bytes.size()), L"snip.exe", &isNew);
        Check(p && g_store.Find(p)->w == 64 && FileExistsW(g_store.MainPath(*g_store.Find(p))), L"a picture is stored as a PNG file");
    }

    // persistence
    size_t before = g_store.items.size();
    g_store.Save();
    Store s2;
    s2.dir = g_store.dir;
    {
        Store saved = g_store;
        Check(g_store.Init() && g_store.items.size() == before, L"history is restored after a restart");
        Check(g_store.Find(a) && g_store.Find(a)->source == L"other.exe", L"item details survive a restart");
    }

    // pin, group, delete, undo, purge
    g_store.SetPinned(b, true);
    g_store.SetGroup(b, L"Work");
    Check(g_store.Find(b)->pinned && g_store.Find(b)->group == L"Work" && g_store.Groups().size() == 1, L"pin and group");
    g_store.SoftDelete({b});
    Check(g_store.Find(b)->deleted != 0, L"delete moves an item to Recently deleted");
    g_store.Restore({b});
    Check(g_store.Find(b)->deleted == 0, L"undo brings it back");
    Check(g_store.UpdateText(b, L"Edited text"), L"an item can be edited");
    Check(g_store.ReadText(*g_store.Find(b)) == L"Edited text", L"the edit is kept");

    // sensitive expiry and cleanup
    uint64_t sec = g_store.AddText(L"4111 1111 1111 1111", "", "", L"x.exe", (int64_t)time(nullptr) - 5, &isNew);
    Check(g_store.Find(sec) != nullptr, L"a sensitive item is stored with an expiry time");
    Check(g_store.Cleanup() && g_store.Find(sec) == nullptr, L"an expired sensitive item deletes itself");
    Check(g_store.Find(b) != nullptr, L"pinned items survive the cleanup");
    g_set.maxItems = 2;
    g_store.Cleanup();
    int active = 0;
    for (auto& it : g_store.items) if (!it.pinned && !it.deleted) active++;
    Check(active <= 2, L"the item limit removes the oldest unpinned items");
    g_set.maxItems = 0;

    // backup with password
    std::wstring bak = root + L"\\test.cmbackup", err;
    size_t have = g_store.items.size();
    Check(BackupExport(bak, L"s3cret pass", err), L"an encrypted backup is written");
    std::string raw;
    ReadFileBytes(bak, raw);
    Check(raw.find("Second item") == std::string::npos && raw.find("invoices") == std::string::npos, L"the encrypted backup contains no readable text");
    int added = 0;
    Check(!BackupImport(bak, L"wrong", added, err), L"a wrong password is refused");
    Check(!BackupImport(bak, L"", added, err), L"a missing password is refused");
    std::vector<uint64_t> all;
    for (auto& it : g_store.items) all.push_back(it.id);
    g_store.Purge(all);
    Check(g_store.items.empty(), L"history can be cleared");
    bool imp = BackupImport(bak, L"s3cret pass", added, err);
    Note(L"import returned " + std::wstring(imp ? L"true" : L"false") + L", added " + std::to_wstring(added) + L" of " + std::to_wstring(have) + L", items now " + std::to_wstring(g_store.items.size()) + L" " + err);
    Check(imp && (size_t)added == have && g_store.items.size() == have, L"the backup restores every item");
    raw[raw.size() / 2] ^= 0x55;
    WriteFileAtomic(bak + L".bad", raw.data(), raw.size());
    Check(!BackupImport(bak + L".bad", L"s3cret pass", added, err), L"a damaged backup is rejected");
    Check(BackupExport(bak + L"2", L"", err) && BackupImport(bak + L"2", L"", added, err), L"a backup without password also works");

    // integrity check and recovery
    {
        std::wstring rep;
        Check(!g_store.CheckIntegrity(rep, false), L"a healthy history passes the integrity check");
        Item victim = g_store.items.front();
        DeleteFileW(g_store.MainPath(victim).c_str());
        WriteFileAtomic(JoinPath(g_store.dir, L"stray-file.dat"), "x", 1);
        Check(g_store.CheckIntegrity(rep, false), L"a missing file and a stray file are detected");
        size_t before2 = g_store.items.size();
        g_store.CheckIntegrity(rep, true);
        Check(g_store.items.size() == before2 - 1 && FileExistsW(JoinPath(g_store.dir, L"stray-file.dat")), L"repair removes only the record without a file and never deletes other files");
        WriteFileAtomic(JoinPath(g_store.dir, L"half-written.png.tmp"), "x", 1);
        g_store.Init();
        Check(!FileExistsW(JoinPath(g_store.dir, L"half-written.png.tmp")), L"startup recovery removes leftovers of an interrupted write");
    }

    // abrupt termination: a half written last journal line is ignored and everything before it survives
    {
        size_t keep = g_store.items.size();
        HANDLE hj = CreateFileW(JoinPath(g_store.dir, L"index.txt").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD wr = 0;
        const char torn[] = "0000000000000fff\t0\t0\t1\t0\t0\tbase-torn\tx.exe\t\t\t0000";
        if (hj != INVALID_HANDLE_VALUE) { WriteFile(hj, torn, sizeof torn - 1, &wr, nullptr); CloseHandle(hj); }
        Check(g_store.Init() && g_store.items.size() == keep, L"a half written last line (power cut) is ignored and earlier items survive");
    }

    // clipboard (may not be available in every test environment)
    ClipInit();
    PutResult pr = ClipPutText(nullptr, L"clipboard round trip \u20ac");
    Check(pr == PUT_OK, L"text can be put on the clipboard");
    Captured c;
    bool flagged = false;
    bool got = ClipCapture(nullptr, c, true, true, true, flagged);
    Check(got && c.kind == K_TEXT && c.text == L"clipboard round trip \u20ac", L"text is read back from the clipboard");
    if (!g_store.items.empty()) {
        Item* pic = nullptr;
        for (auto& it : g_store.items) if (it.kind == K_PICTURE) pic = &it;
        if (pic) {
            Check(ClipPutItem(nullptr, *pic, false) == PUT_OK, L"a picture can be put on the clipboard (PNG and bitmap)");
            Captured c2;
            bool ok2 = ClipCapture(nullptr, c2, true, true, true, flagged);
            Check(ok2 && c2.kind == K_PICTURE && c2.w == 64 && c2.h == 48, L"a picture is read back from the clipboard");
            if (c2.kind == K_PICTURE) DeleteFileW(c2.tmpPng.c_str());
        }
    }
    Check(IsIgnoredProgram(L"KeePass.exe") && !IsIgnoredProgram(L"notepad.exe"), L"the ignore list recognises password managers");

    g_log += L"\r\n" + std::to_wstring(g_total - g_fail) + L" of " + std::to_wstring(g_total) + L" checks passed\r\n";
    std::string u = core::ToUtf8(g_log);
    WriteFileAtomic(outFile, u.data(), u.size());
    RemoveTree(root);
    return g_fail ? 1 : 0;
}

// --seed-demo : fills the data folder with sample items (used for screenshots and manual checks). Only when FCM_DATA_ROOT is set.
int RunSeedDemo() {
    wchar_t over[MAX_PATH];
    if (GetEnvironmentVariableW(L"FCM_DATA_ROOT", over, MAX_PATH) == 0) return 2;   // never write demo data into real history
    CoInitialize(nullptr);
    GdiplusStart();
    g_set = Settings();
    if (!g_store.Init()) return 1;
    bool n;
    g_store.AddText(L"Meeting notes\r\n- Review the Q4 budget\r\n- Send the invoice to the client\r\n- Book flights for Bengaluru", "", "", L"notepad.exe", 0, &n);
    g_store.AddText(L"https://github.com/akshsim1991/Application_Development", "", "", L"chrome.exe", 0, &n);
    g_store.AddText(L"akshaya@example.com", "", "", L"outlook.exe", 0, &n);
    g_store.AddText(L"+91 98765 43210", "", "", L"whatsapp.exe", 0, &n);
    g_store.AddText(L"#0067C0", "", "", L"figma.exe", 0, &n);
    g_store.AddText(L"C:\\Users\\Akshaya\\Documents\\Reports\\2026 summary.xlsx", "", "", L"explorer.exe", 0, &n);
    g_store.AddText(L"{\"name\": \"Feather\", \"version\": 2.1, \"tags\": [\"clipboard\", \"offline\"]}", "", "", L"code.exe", 0, &n);
    uint64_t pinned = g_store.AddText(L"Thanks for your email. I will get back to you shortly.\r\n\r\nRegards,\r\nAkshaya", "", "", L"outlook.exe", 0, &n);
    g_store.SetPinned(pinned, true);
    g_store.SetGroup(pinned, L"Replies");
    std::vector<std::wstring> files = {L"C:\\Docs\\report.docx", L"C:\\Docs\\budget.xlsx", L"C:\\Docs\\photo.jpg"};
    g_store.AddFiles(files, 1, L"explorer.exe", &n);
    g_store.AddSnippet(L"Email signature", L"Best regards,\r\nAkshaya Simha\r\nFeather apps");
    Gdiplus::Bitmap bmp(480, 300, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&bmp);
        Gdiplus::LinearGradientBrush br(Gdiplus::Point(0, 0), Gdiplus::Point(480, 300), Gdiplus::Color(255, 0, 103, 192), Gdiplus::Color(255, 120, 200, 255));
        g.FillRectangle(&br, 0, 0, 480, 300);
        Gdiplus::Font f(L"Arial", 28);
        Gdiplus::SolidBrush w(Gdiplus::Color(255, 255, 255, 255));
        g.DrawString(L"Screenshot", -1, &f, Gdiplus::PointF(24, 120), &w);
    }
    std::wstring tmp = JoinPath(g_store.dir, L"incoming-demo.png.tmp");
    CLSID png = EncoderFor(L"image/png");
    bmp.Save(tmp.c_str(), &png, nullptr);
    std::string bytes;
    ReadFileBytes(tmp, bytes);
    g_store.AddPictureFile(tmp, 480, 300, core::Fnv64(bytes.data(), bytes.size()), L"snippingtool.exe", &n);
    g_store.AddText(L"Password reminder: do not copy secrets", "", "", L"notepad.exe", 0, &n);
    return 0;
}

// --bench N out.txt : measures the journal and search speed with N synthetic items (never your history: needs FCM_DATA_ROOT).
namespace {
double Now() { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
double Pct(std::vector<double>& v, double p) { std::sort(v.begin(), v.end()); return v[std::min(v.size() - 1, (size_t)(v.size() * p))]; }
}

int RunBench(int n, const std::wstring& outFile) {
    wchar_t over[MAX_PATH];
    if (GetEnvironmentVariableW(L"FCM_DATA_ROOT", over, MAX_PATH) == 0) return 2;
    CoInitialize(nullptr);
    g_set = Settings();
    if (!g_store.Init()) return 1;
    static const wchar_t* vocab[] = {L"invoice", L"meeting", L"budget", L"report", L"client", L"project", L"schedule", L"payment", L"design", L"review", L"summary", L"travel", L"flight", L"hotel",
                                     L"colour", L"server", L"deploy", L"error", L"warning", L"customer", L"order", L"delivery", L"shipment", L"quarter", L"revenue", L"forecast", L"policy", L"contract"};
    static const wchar_t* apps[] = {L"chrome.exe", L"notepad.exe", L"code.exe", L"outlook.exe", L"explorer.exe", L"excel.exe"};
    uint32_t seed = 12345;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };
    int64_t now = (int64_t)time(nullptr);
    g_store.items.reserve((size_t)n);
    for (int i = 0; i < n; i++) {
        Item it;
        it.id = g_store.NextId();
        it.kind = (rnd() % 10 == 0) ? K_PICTURE : (rnd() % 12 == 0 ? K_FILES : K_TEXT);
        it.created = now - (int64_t)(n - i) * 30;
        it.base = L"bench-" + std::to_wstring(i);
        it.source = apps[rnd() % 6];
        std::wstring h;
        for (int w = 0; w < 14; w++) { h += vocab[rnd() % 28]; h += L' '; }
        h += L"ref " + std::to_wstring(i);
        it.head = h;
        it.count = (int)h.size();
        it.hash = rnd();
        it.w = 800; it.h = 600;
        it.st = (rnd() % 20 == 0) ? core::ST_LINK : core::ST_NONE;
        g_store.items.push_back(std::move(it));
    }
    std::wstring log = L"Benchmark, " + std::to_wstring(n) + L" synthetic items (run under: " + (GetModuleHandleW(L"ntdll.dll") && GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version") ? L"Wine - NOT representative of Windows" : L"Windows") + L")\r\n";
    auto line = [&](const std::wstring& what, double ms) { wchar_t b[200]; _snwprintf(b, 200, L"%-44ls %10.2f ms\r\n", what.c_str(), ms); log += b; };
    double t0 = Now();
    g_store.Save();
    line(L"write the whole index (compaction)", Now() - t0);
    g_store.items.clear();
    t0 = Now();
    g_store.Init();
    line(L"load the index at start-up", Now() - t0);
    Check(g_store.items.size() == (size_t)n, L"all items load again");
    // one capture = one appended line
    std::vector<double> app;
    for (int i = 0; i < 200; i++) {
        Item it = g_store.items.back();
        it.id = g_store.NextId();
        t0 = Now();
        g_store.Touch(it);
        app.push_back(Now() - t0);
    }
    line(L"record one copy (append), median", Pct(app, 0.5));
    line(L"record one copy (append), p95", Pct(app, 0.95));
    struct Q { const wchar_t* label; const wchar_t* text; };
    Q qs[] = {{L"search: one word", L"invoice"}, {L"search: two words", L"invoice budget"}, {L"search: exact phrase", L"\"budget report\""},
              {L"search: fuzzy", L"~invoce"}, {L"search: regular expression", L"re:inv\\w+ ref 9\\d+$"}, {L"search: app and word", L"app:chrome meeting"},
              {L"search: type filter", L"type:link"}, {L"search: date filter", L"date:7d"}};
    for (auto& q : qs) {
        std::vector<double> ms;
        size_t hits = 0;
        for (int r = 0; r < 12; r++) {
            core::Query pq = core::ParseQuery(q.text, now, now - now % 86400);
            t0 = Now();
            hits = 0;
            for (size_t i = g_store.items.size(); i-- > 0;) if (g_store.Matches(g_store.items[i], pq)) hits++;
            ms.push_back(Now() - t0);
        }
        wchar_t b[240];
        _snwprintf(b, 240, L"%-44ls median %8.2f ms   p95 %8.2f ms   (%zu matches)\r\n", q.label, Pct(ms, 0.5), Pct(ms, 0.95), hits);
        log += b;
    }
    PROCESS_MEMORY_COUNTERS_EX pmc = {sizeof pmc};
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof pmc);
    { wchar_t b[120]; _snwprintf(b, 120, L"%-44ls %10.1f MB (working set %.1f MB)\r\n", L"private bytes with all items in memory", pmc.PrivateUsage / 1048576.0, pmc.WorkingSetSize / 1048576.0); log += b; }
    log += L"\r\n" + std::to_wstring(g_total - g_fail) + L" of " + std::to_wstring(g_total) + L" checks passed\r\n";
    std::string u = core::ToUtf8(log);
    WriteFileAtomic(outFile, u.data(), u.size());
    return g_fail ? 1 : 0;
}
