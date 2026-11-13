// The history: every item is an ordinary file in %LOCALAPPDATA%\FeatherClipboardManager\Clipboard contents,
// plus one index file (index.txt) that works as a journal: every change appends one line (an item's complete current
// state, or a deletion mark), so a change costs one small write however long the history is. The last line for an item
// wins; a half written last line (crash, power cut) is ignored. At start-up and exit the journal is rewritten compactly
// (temporary file, then rename), so a crash never leaves a half written history.
#include "app.h"
#include <cwctype>
#include <aclapi.h>
#include <sddl.h>

Store g_store;
static const size_t HEAD_CHARS = 1000;
static const int64_t TRASH_DAYS = 7;

static const wchar_t* INDEX_HEADER = L"# Feather Clipboard Manager 2.1 index (journal). Tab separated: id, kind, pinned, created, deleted, expires, base, source, group, title, hash, width, height, bytes, flags, dropeffect, count, lines, smarttype, head, ~   |   a line \"-<TAB>id\" removes an item";

bool Store::Init() {
    std::wstring local = KnownFolder(FOLDERID_LocalAppData);
    if (local.empty()) { note = L"The Local AppData folder could not be found."; return false; }
    dataRoot = JoinPath(local, L"FeatherClipboardManager");
    wchar_t over[MAX_PATH];
    if (GetEnvironmentVariableW(L"FCM_DATA_ROOT", over, MAX_PATH) > 0) dataRoot = over;   // for tests only: keeps them away from real history
    dir = JoinPath(dataRoot, L"Clipboard contents");
    if (!EnsureDir(dir)) { note = L"The history folder could not be created: " + dir; return false; }
    // only this Windows user (and the system) may open the history: a protected ACL on the data folder
    {
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:PAI(A;OICI;FA;;;OW)(A;OICI;FA;;;SY)", SDDL_REVISION_1, &sd, nullptr)) {
            BOOL present = FALSE, deflt = FALSE;
            PACL acl = nullptr;
            if (GetSecurityDescriptorDacl(sd, &present, &acl, &deflt) && present)
                SetNamedSecurityInfoW((LPWSTR)dataRoot.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr);
            LocalFree(sd);
        }
    }
    std::wstring probe = JoinPath(dir, L".write-test");
    if (!WriteFileAtomic(probe, "x", 1)) { note = L"The history folder is not writable: " + dir; return false; }
    DeleteFileW(probe.c_str());

    // startup recovery: leftovers of an interrupted write (never part of the history) are removed
    {
        WIN32_FIND_DATAW fd;
        HANDLE ff = FindFirstFileW(JoinPath(dir, L"*.tmp").c_str(), &fd);
        if (ff != INVALID_HANDLE_VALUE) {
            do { DeleteFileW(JoinPath(dir, fd.cFileName).c_str()); } while (FindNextFileW(ff, &fd));
            FindClose(ff);
        }
    }
    std::string raw;
    items.clear();
    if (ReadFileBytes(JoinPath(dir, L"index.txt"), raw)) {
        std::map<uint64_t, Item> byId;
        size_t lines = 0;
        for (auto& line : core::SplitLines(core::FromUtf8(raw))) {
            if (line.empty() || line[0] == L'#') continue;
            lines++;
            if (line.size() > 2 && line[0] == L'-' && line[1] == L'\t') { byId.erase(core::ParseHex(line.substr(2))); continue; }
            Item it;
            if (!ParseIndexLine(line, it)) continue;
            if (it.id > lastId) lastId = it.id;
            byId[it.id] = std::move(it);
        }
        for (auto& kv : byId) items.push_back(std::move(kv.second));
        std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.created < b.created; });
        if (lines > items.size() * 2 + 200) Save();      // compact a journal that has grown
    }
    return true;
}

bool Store::ParseIndexLine(const std::wstring& line, Item& it) {
    if (line.empty() || line[0] == L'#') return false;
    auto f = core::SplitTabs(line);
    if (f.size() < 21 || f[20] != L"~") return false;       // a half written last line has no end marker
    it = Item();
    it.id = core::ParseHex(f[0]);
    it.kind = _wtoi(f[1].c_str());
    it.pinned = f[2] == L"1";
    it.created = _wtoi64(f[3].c_str());
    it.deleted = _wtoi64(f[4].c_str());
    it.expires = _wtoi64(f[5].c_str());
    it.base = core::UnescapeField(f[6]);
    it.source = core::UnescapeField(f[7]);
    it.group = core::UnescapeField(f[8]);
    it.title = core::UnescapeField(f[9]);
    it.hash = core::ParseHex(f[10]);
    it.w = _wtoi(f[11].c_str());
    it.h = _wtoi(f[12].c_str());
    it.bytes = (uint64_t)_wtoi64(f[13].c_str());
    it.flags = (unsigned)_wtoi(f[14].c_str());
    it.dropEffect = _wtoi(f[15].c_str());
    it.count = _wtoi(f[16].c_str());
    it.lines = _wtoi(f[17].c_str());
    it.st = (core::SmartType)_wtoi(f[18].c_str());
    it.head = core::UnescapeField(f[19]);
    if (it.id == 0 || it.base.empty() || it.kind < 0 || it.kind > 3) return false;
    // file names come from the index: never allow a path
    if (it.base.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) return false;
    it.truncated = it.count > (int)HEAD_CHARS && (it.kind == K_TEXT || it.kind == K_SNIPPET);
    return true;
}

static std::wstring IndexLine(const Item& it) {
    std::wstring l;
    auto T_ = [&](const std::wstring& s) { l += s; l += L'\t'; };
    T_(core::HexId(it.id)); T_(std::to_wstring(it.kind)); T_(it.pinned ? L"1" : L"0");
    T_(std::to_wstring(it.created)); T_(std::to_wstring(it.deleted)); T_(std::to_wstring(it.expires));
    T_(core::EscapeField(it.base)); T_(core::EscapeField(it.source)); T_(core::EscapeField(it.group)); T_(core::EscapeField(it.title));
    T_(core::HexId(it.hash)); T_(std::to_wstring(it.w)); T_(std::to_wstring(it.h)); T_(std::to_wstring(it.bytes));
    T_(std::to_wstring(it.flags)); T_(std::to_wstring(it.dropEffect)); T_(std::to_wstring(it.count)); T_(std::to_wstring(it.lines));
    T_(std::to_wstring((int)it.st));
    l += core::EscapeField(it.head);
    l += L"\t~";
    return l;
}

void Store::Save() {          // compact rewrite of the whole journal
    if (dir.empty()) return;
    std::wstring out = INDEX_HEADER;
    out += L"\n";
    for (auto& it : items) { out += IndexLine(it); out += L"\n"; }
    std::string u = core::ToUtf8(out);
    if (WriteFileAtomic(JoinPath(dir, L"index.txt"), u.data(), u.size())) dirty = false;
}

static bool AppendToJournal(const std::wstring& path, const std::wstring& text) {
    std::string u = core::ToUtf8(text);
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(h, u.data(), (DWORD)u.size(), &w, nullptr) && w == u.size();
    if (ok) FlushFileBuffers(h);
    CloseHandle(h);
    return ok;
}

void Store::Touch(const Item& it) {          // one small write: the item's complete current state
    if (dir.empty()) return;
    if (!AppendToJournal(JoinPath(dir, L"index.txt"), IndexLine(it) + L"\n")) Save();
}

void Store::Tombstone(const std::vector<uint64_t>& ids) {
    std::wstring t;
    for (auto id : ids) t += L"-\t" + core::HexId(id) + L"\n";
    if (!t.empty() && !AppendToJournal(JoinPath(dir, L"index.txt"), t)) Save();
}

Item* Store::Find(uint64_t id) {
    for (auto& it : items) if (it.id == id) return &it;
    return nullptr;
}

uint64_t Store::NextId() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    if (t <= lastId) t = lastId + 1;
    lastId = t;
    return t;
}

std::wstring Store::NewBase() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t b[64];
    for (int attempt = 0; attempt < 1000; attempt++) {
        _snwprintf(b, 64, L"%04d-%02d-%02d %02d.%02d.%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, (st.wMilliseconds + attempt) % 1000);
        std::wstring base = b;
        if (attempt >= 1) base += L" " + std::to_wstring(attempt);
        bool clash = false;
        for (auto& it : items) if (it.base == base) { clash = true; break; }
        if (!clash && !FileExistsW(JoinPath(dir, base + L".txt")) && !FileExistsW(JoinPath(dir, base + L".png")) && !FileExistsW(JoinPath(dir, base + L" files.txt"))) return base;
    }
    return std::to_wstring(NextId());
}

std::wstring Store::PathFor(const Item& it, const wchar_t* ext) const { return JoinPath(dir, it.base + ext); }

std::wstring Store::MainPath(const Item& it) const {
    switch (it.kind) {
    case K_PICTURE: return PathFor(it, L".png");
    case K_FILES: return PathFor(it, L" files.txt");
    case K_SNIPPET: return PathFor(it, L" snippet.txt");
    default: return PathFor(it, L".txt");
    }
}

std::wstring Store::ReadText(const Item& it) const {
    std::string raw;
    if (!ReadFileBytes(MainPath(it), raw)) return L"";
    return core::FromUtf8(raw);
}

std::vector<std::wstring> Store::ReadFiles(const Item& it) const {
    std::vector<std::wstring> v;
    for (auto& l : core::SplitLines(ReadText(it))) if (!l.empty()) v.push_back(l);
    return v;
}

std::string Store::ReadSide(const Item& it, const wchar_t* ext) const {
    std::string raw;
    ReadFileBytes(PathFor(it, ext), raw);
    return raw;
}

std::wstring Store::ReadOcr(const Item& it) const {
    std::string raw;
    if (!ReadFileBytes(PathFor(it, L".ocr.txt"), raw)) return L"";
    return core::FromUtf8(raw);
}

void Store::RemoveFiles(const Item& it) {
    DeleteFileW(MainPath(it).c_str());
    DeleteFileW(PathFor(it, L".html").c_str());
    DeleteFileW(PathFor(it, L".rtf").c_str());
    DeleteFileW(PathFor(it, L".ocr.txt").c_str());
}

static int CountLines(const std::wstring& s) {
    int n = 1;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\n') n++;
        else if (s[i] == L'\r' && (i + 1 >= s.size() || s[i + 1] != L'\n')) n++;
    }
    return s.empty() ? 0 : n;
}

static void FillTextFacts(Item& it, const std::wstring& text) {
    it.hash = core::HashText(text);
    it.count = (int)text.size();
    it.lines = CountLines(text);
    it.bytes = core::ToUtf8(text).size();
    it.head = core::OneLinePreview(text, HEAD_CHARS);
    it.truncated = text.size() > HEAD_CHARS;
    it.st = core::Detect(text);
}

// Move an identical existing item to the top instead of storing a copy.
static Item* BumpExisting(Store& s, int kind, uint64_t hash, bool* isNew, const std::wstring& source) {
    for (size_t i = 0; i < s.items.size(); i++) {
        Item& it = s.items[i];
        if (it.kind != kind || it.hash != hash || it.deleted) continue;
        bool newest = true;
        for (size_t j = i + 1; j < s.items.size(); j++) if (!s.items[j].deleted) { newest = false; break; }
        it.created = (int64_t)time(nullptr);
        if (!source.empty()) it.source = source;
        Item copy = it;
        s.items.erase(s.items.begin() + i);
        s.items.push_back(copy);
        if (isNew) *isNew = false;
        (void)newest;
        s.dirty = true;
        return &s.items.back();
    }
    return nullptr;
}

uint64_t Store::AddText(const std::wstring& text, const std::string& html, const std::string& rtf, const std::wstring& source, int64_t expires, bool* isNew) {
    if (isNew) *isNew = true;
    uint64_t h = core::HashText(text);
    if (Item* ex = BumpExisting(*this, K_TEXT, h, isNew, source)) {
        if (expires && !ex->pinned) ex->expires = expires;
        Touch(*ex);
        return ex->id;
    }
    Item it;
    it.id = NextId();
    it.kind = K_TEXT;
    it.created = (int64_t)time(nullptr);
    it.base = NewBase();
    it.source = source;
    it.expires = expires;
    FillTextFacts(it, text);
    std::string u = core::ToUtf8(text);
    if (!WriteFileAtomic(MainPath(it), u.data(), u.size())) { lastId--; return 0; }
    if (!html.empty() && WriteFileAtomic(PathFor(it, L".html"), html.data(), html.size())) it.flags |= F_HTML;
    if (!rtf.empty() && WriteFileAtomic(PathFor(it, L".rtf"), rtf.data(), rtf.size())) it.flags |= F_RTF;
    items.push_back(it);
    Touch(it);
    return it.id;
}

uint64_t Store::AddPictureFile(const std::wstring& tmpPng, int w, int h, uint64_t hash, const std::wstring& source, bool* isNew) {
    if (isNew) *isNew = true;
    if (Item* ex = BumpExisting(*this, K_PICTURE, hash, isNew, source)) {
        DeleteFileW(tmpPng.c_str());
        Touch(*ex);
        return ex->id;
    }
    Item it;
    it.id = NextId();
    it.kind = K_PICTURE;
    it.created = (int64_t)time(nullptr);
    it.base = NewBase();
    it.source = source;
    it.hash = hash;
    it.w = w;
    it.h = h;
    if (!MoveFileExW(tmpPng.c_str(), MainPath(it).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) { lastId--; return 0; }
    it.bytes = FileSizeW(MainPath(it));
    items.push_back(it);
    Touch(it);
    return it.id;
}

uint64_t Store::AddFiles(const std::vector<std::wstring>& paths, int dropEffect, const std::wstring& source, bool* isNew) {
    if (isNew) *isNew = true;
    std::wstring joined = core::Join(paths, L"\r\n");
    uint64_t h = core::HashText(joined);
    if (Item* ex = BumpExisting(*this, K_FILES, h, isNew, source)) { Touch(*ex); return ex->id; }
    Item it;
    it.id = NextId();
    it.kind = K_FILES;
    it.created = (int64_t)time(nullptr);
    it.base = NewBase();
    it.source = source;
    it.hash = h;
    it.dropEffect = dropEffect;
    it.count = (int)paths.size();
    it.lines = (int)paths.size();
    std::wstring names;
    for (auto& p : paths) {
        size_t s = p.find_last_of(L"\\/");
        if (!names.empty()) names += L", ";
        names += s == std::wstring::npos ? p : p.substr(s + 1);
        if (names.size() > HEAD_CHARS) break;
    }
    it.head = names.substr(0, HEAD_CHARS);
    std::string u = core::ToUtf8(joined);
    it.bytes = u.size();
    if (!WriteFileAtomic(MainPath(it), u.data(), u.size())) { lastId--; return 0; }
    items.push_back(it);
    Touch(it);
    return it.id;
}

uint64_t Store::AddSnippet(const std::wstring& name, const std::wstring& text) {
    Item it;
    it.id = NextId();
    it.kind = K_SNIPPET;
    it.pinned = true;
    it.created = (int64_t)time(nullptr);
    it.base = NewBase();
    it.title = name;
    FillTextFacts(it, text);
    std::string u = core::ToUtf8(text);
    if (!WriteFileAtomic(MainPath(it), u.data(), u.size())) { lastId--; return 0; }
    items.push_back(it);
    Touch(it);
    return it.id;
}

bool Store::UpdateText(uint64_t id, const std::wstring& text) {
    Item* it = Find(id);
    if (!it || (it->kind != K_TEXT && it->kind != K_SNIPPET)) return false;
    std::string u = core::ToUtf8(text);
    if (!WriteFileAtomic(MainPath(*it), u.data(), u.size())) return false;
    DeleteFileW(PathFor(*it, L".html").c_str());
    DeleteFileW(PathFor(*it, L".rtf").c_str());
    it->flags &= ~(F_HTML | F_RTF);
    FillTextFacts(*it, text);
    Touch(*it);
    return true;
}

bool Store::SetOcr(uint64_t id, const std::wstring& text) {
    Item* it = Find(id);
    if (!it) return false;
    it->flags |= F_OCRTRIED;
    std::wstring t = core::Trim(text);
    if (!t.empty()) {
        std::string u = core::ToUtf8(t);
        if (WriteFileAtomic(PathFor(*it, L".ocr.txt"), u.data(), u.size())) {
            it->flags |= F_OCR;
            it->head = core::OneLinePreview(t, HEAD_CHARS);
        }
    }
    Touch(*it);
    return true;
}

void Store::SetPinned(uint64_t id, bool p) {
    if (Item* it = Find(id)) {
        it->pinned = p;
        if (p) it->expires = 0;   // pinning rescues a "delete after a minute" item
        Touch(*it);
    }
}

void Store::SetGroup(uint64_t id, const std::wstring& g) {
    if (Item* it = Find(id)) { it->group = g; Touch(*it); }
}

void Store::SoftDelete(const std::vector<uint64_t>& ids) {
    int64_t now = (int64_t)time(nullptr);
    for (auto id : ids) if (Item* it = Find(id)) { it->deleted = now; it->expires = 0; Touch(*it); }
}

void Store::Restore(const std::vector<uint64_t>& ids) {
    for (auto id : ids) if (Item* it = Find(id)) { it->deleted = 0; Touch(*it); }
}

void Store::Purge(const std::vector<uint64_t>& ids) {
    std::set<uint64_t> kill(ids.begin(), ids.end());
    std::vector<Item> keep;
    keep.reserve(items.size());
    for (auto& it : items) {
        if (kill.count(it.id)) RemoveFiles(it);
        else keep.push_back(std::move(it));
    }
    items.swap(keep);
    Tombstone(ids);
}

void Store::PurgeAllDeleted() {
    std::vector<uint64_t> ids;
    for (auto& it : items) if (it.deleted) ids.push_back(it.id);
    if (!ids.empty()) Purge(ids);
}

void Store::DeleteAllUnpinned() {
    std::vector<uint64_t> ids;
    for (auto& it : items) if (!it.pinned && !it.deleted && it.kind != K_SNIPPET) ids.push_back(it.id);
    SoftDelete(ids);
}

int Store::CountUnpinned() const {
    int n = 0;
    for (auto& it : items) if (!it.pinned && !it.deleted && it.kind != K_SNIPPET) n++;
    return n;
}

std::vector<std::wstring> Store::Groups() const {
    std::set<std::wstring> g;
    for (auto& it : items) if (!it.group.empty() && !it.deleted) g.insert(it.group);
    return std::vector<std::wstring>(g.begin(), g.end());
}

uint64_t Store::TotalBytes() const {
    uint64_t t = 0;
    for (auto& it : items) t += it.bytes;
    return t;
}

bool Store::Cleanup() {
    int64_t now = (int64_t)time(nullptr);
    std::vector<uint64_t> kill;
    std::set<uint64_t> marked;
    auto mark = [&](const Item& it) { if (marked.insert(it.id).second) kill.push_back(it.id); };

    for (auto& it : items) {
        if (it.expires && it.expires <= now && !it.pinned) mark(it);                          // sensitive items
        else if (it.deleted && now - it.deleted > TRASH_DAYS * 86400) mark(it);              // old "recently deleted"
        else if (g_set.deleteAfterDays > 0 && !it.pinned && !it.deleted && it.kind != K_SNIPPET &&
                 now - it.created > (int64_t)g_set.deleteAfterDays * 86400) mark(it);
    }
    if (g_set.maxItems > 0) {
        int active = 0;
        for (auto& it : items) if (!it.pinned && !it.deleted && it.kind != K_SNIPPET && !marked.count(it.id)) active++;
        for (auto& it : items) {                                         // oldest first
            if (active <= g_set.maxItems) break;
            if (!it.pinned && !it.deleted && it.kind != K_SNIPPET && !marked.count(it.id)) { mark(it); active--; }
        }
    }
    if (g_set.maxMB > 0) {
        uint64_t limit = (uint64_t)g_set.maxMB * 1048576ULL, total = 0;
        for (auto& it : items) if (!marked.count(it.id)) total += it.bytes;
        for (int pass = 0; pass < 2 && total > limit; pass++) {          // deleted ones go first, then the oldest unpinned
            for (auto& it : items) {
                if (total <= limit) break;
                if (marked.count(it.id) || it.pinned || it.kind == K_SNIPPET) continue;
                if (pass == 0 && !it.deleted) continue;
                mark(it);
                total -= std::min<uint64_t>(total, it.bytes);
            }
        }
    }
    if (kill.empty()) return false;
    Purge(kill);
    return true;
}

void Store::ClearAll() {
    std::vector<uint64_t> ids;
    for (auto& it : items) ids.push_back(it.id);
    if (!ids.empty()) Purge(ids);
    // everything else in the history folder is derived data (pictures' text, half-written files, stray files): removed too
    WIN32_FIND_DATAW fd;
    HANDLE ff = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L".." || (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (core::Lower(n) == L"index.txt") continue;
            DeleteFileW(JoinPath(dir, n).c_str());
        } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    items.clear();
    Save();                                   // an empty journal replaces the old one
}

bool Store::CheckIntegrity(std::wstring& report, bool repair) {
    std::set<std::wstring> known;
    std::vector<uint64_t> missing;
    int sizeMismatch = 0;
    for (auto& it : items) {
        std::wstring mp = MainPath(it);
        known.insert(core::Lower(it.base));
        if (!FileExistsW(mp)) { missing.push_back(it.id); continue; }
        uint64_t sz = FileSizeW(mp);
        if (it.kind == K_PICTURE && sz != it.bytes) sizeMismatch++;
    }
    int stray = 0;
    std::vector<std::wstring> strayFiles;
    WIN32_FIND_DATAW fd;
    HANDLE ff = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (ff != INVALID_HANDLE_VALUE) {
        do {
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L".." || n == L"index.txt" || (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            bool owned = false;
            for (auto& b : known) if (core::Lower(n).compare(0, b.size(), b) == 0) { owned = true; break; }
            if (!owned) { stray++; strayFiles.push_back(n); }
        } while (FindNextFileW(ff, &fd));
        FindClose(ff);
    }
    report = std::to_wstring(items.size()) + L" " + T(L"items checked.") + L"\r\n";
    report += std::to_wstring(missing.size()) + L" " + T(L"records have no file on disk.") + L"\r\n";
    report += std::to_wstring(stray) + L" " + T(L"files in the folder are not part of the history.") + L"\r\n";
    if (sizeMismatch) report += std::to_wstring(sizeMismatch) + L" " + T(L"pictures changed size on disk.") + L"\r\n";
    bool problems = !missing.empty() || stray;
    if (repair && !missing.empty()) {
        std::set<uint64_t> gone(missing.begin(), missing.end());
        std::vector<Item> keep;
        for (auto& it : items) if (!gone.count(it.id)) keep.push_back(it);
        items.swap(keep);
        Save();
        report += T(L"Repaired: the records without a file were removed. Files that were not part of the history were left alone.");
    }
    return problems;
}

// Case-insensitive "contains" (the needle is already lower case). ASCII is compared without a function call.
static inline wchar_t Low(wchar_t c) { return c < 128 ? (c >= L'A' && c <= L'Z' ? (wchar_t)(c + 32) : c) : (wchar_t)std::towlower(c); }
static bool ContainsCI(const std::wstring& hay, const std::wstring& needleLower) {
    size_t n = needleLower.size();
    if (!n) return true;
    if (hay.size() < n) return false;
    const wchar_t first = needleLower[0];
    const size_t last = hay.size() - n;
    for (size_t i = 0; i <= last; i++) {
        if (Low(hay[i]) != first) continue;
        size_t k = 1;
        while (k < n && Low(hay[i + k]) == needleLower[k]) k++;
        if (k == n) return true;
    }
    return false;
}

bool Store::Matches(const Item& it, const core::Query& q) const {
    if (q.empty()) return true;
    if (q.fromTime && it.created < q.fromTime) return false;
    if (q.toTime && it.created >= q.toTime) return false;
    if (!q.app.empty() && !ContainsCI(it.source, q.app)) return false;
    if (!q.tag.empty() && !ContainsCI(it.group, q.tag)) return false;
    if (!q.type.empty()) {
        const std::wstring& t = q.type;
        bool ok = false;
        if (t == L"text") ok = it.kind == K_TEXT;
        else if (t == L"picture" || t == L"image" || t == L"pictures" || t == L"images") ok = it.kind == K_PICTURE;
        else if (t == L"file" || t == L"files") ok = it.kind == K_FILES;
        else if (t == L"snippet" || t == L"snippets") ok = it.kind == K_SNIPPET;
        else if (t == L"link" || t == L"url" || t == L"links") ok = it.kind == K_TEXT && it.st == core::ST_LINK;
        else if (t == L"email") ok = it.kind == K_TEXT && it.st == core::ST_EMAIL;
        else if (t == L"phone") ok = it.kind == K_TEXT && it.st == core::ST_PHONE;
        else if (t == L"colour" || t == L"color") ok = it.kind == K_TEXT && it.st == core::ST_COLOUR;
        else if (t == L"path") ok = it.kind == K_TEXT && it.st == core::ST_PATH;
        else if (t == L"json") ok = it.kind == K_TEXT && it.st == core::ST_JSON;
        if (!ok) return false;
    }
    std::wstring extra = it.source + L" " + it.group + L" " + it.title;
    if (it.kind == K_PICTURE) extra += L" picture image screenshot photo";
    if (it.kind == K_FILES) extra += L" files copied";
    std::wstring full;
    bool fullLoaded = false;
    auto loadFull = [&]() -> const std::wstring& { if (!fullLoaded) { full = it.truncated ? ReadText(it) : it.head; fullLoaded = true; } return full; };
    for (auto& w : q.words) {
        if (ContainsCI(it.head, w) || ContainsCI(extra, w)) continue;
        if (it.truncated && ContainsCI(loadFull(), w)) continue;
        return false;
    }
    for (auto& p : q.phrases) {
        if (ContainsCI(it.head, p) || ContainsCI(extra, p)) continue;
        if (it.truncated && ContainsCI(loadFull(), p)) continue;
        return false;
    }
    for (auto& f : q.fuzzy) {                                  // typo-tolerant search looks at the first 300 characters
        if (core::FuzzyContains(core::Lower(it.head.substr(0, 300) + L" " + extra), f)) continue;
        if (it.truncated && core::FuzzyContains(core::Lower(loadFull()), f)) continue;
        return false;
    }
    if (!q.regex.empty()) {
        if (core::RegexSearch(q.regex, it.head.substr(0, 400)) || core::RegexSearch(q.regex, extra)) return true;
        if (it.truncated && core::RegexSearch(q.regex, loadFull())) return true;
        return false;
    }
    return true;
}

std::wstring ItemTitle(const Item& it) {
    static const size_t PL[] = {70, 140, 230};
    size_t pl = PL[std::clamp(g_set.previewLen, 0, 2)];
    std::wstring s;
    switch (it.kind) {
    case K_PICTURE:
        s = std::wstring(T(L"Picture")) + L", " + std::to_wstring(it.w) + L"\u00d7" + std::to_wstring(it.h);
        if (!it.head.empty()) s += L"  \u2014  " + it.head.substr(0, pl - 20);
        break;
    case K_FILES:
        s = it.count == 1 ? std::wstring(T(L"1 copied file")) : std::to_wstring(it.count) + L" " + T(L"copied files");
        if (!it.head.empty()) s += L": " + it.head.substr(0, pl);
        break;
    case K_SNIPPET:
        s = L"\u2605 " + it.title;
        if (!it.head.empty()) s += L"  \u2014  " + it.head.substr(0, pl - 20);
        break;
    default:
        if (it.head.empty()) return it.count ? T(L"(spaces or empty lines only)") : L"";
        s = it.head.substr(0, pl);
    }
    if (g_set.maskPreviews && it.kind != K_PICTURE) s = core::MaskSensitive(s);
    return s;
}
