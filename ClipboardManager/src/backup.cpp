// Backup and restore of the whole history as one file (.cmbackup).
// Optional password: AES-256-CBC, key and MAC key derived with PBKDF2-HMAC-SHA256 (200,000 rounds), HMAC-SHA256 over everything.
// Without a password the file is not encrypted and carries a SHA-256 check value.
#include "app.h"
#include <bcrypt.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

static const char MAGIC[4] = {'C', 'M', 'B', 'K'};
static const uint32_t ITER = 200000;

static void PutU32(std::string& o, uint32_t v) { o.append((const char*)&v, 4); }
static void PutU64(std::string& o, uint64_t v) { o.append((const char*)&v, 8); }

static bool Hmac(const BYTE* key, ULONG keyLen, const std::string& data, BYTE out[32]) {
    BCRYPT_ALG_HANDLE a = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, key ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0))) return false;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = NT_SUCCESS(BCryptCreateHash(a, &h, nullptr, 0, (PUCHAR)key, keyLen, 0));
    if (ok) {
        size_t done = 0;
        while (ok && done < data.size()) {
            ULONG n = (ULONG)std::min<size_t>(data.size() - done, 1 << 24);
            ok = NT_SUCCESS(BCryptHashData(h, (PUCHAR)data.data() + done, n, 0));
            done += n;
        }
        if (ok) ok = NT_SUCCESS(BCryptFinishHash(h, out, 32, 0));
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(a, 0);
    return ok;
}

static bool Derive(const std::wstring& pw, const BYTE* salt, BYTE key[64]) {
    std::string pwu = core::ToUtf8(pw);
    BCRYPT_ALG_HANDLE a = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) return false;
    bool ok = NT_SUCCESS(BCryptDeriveKeyPBKDF2(a, (PUCHAR)pwu.data(), (ULONG)pwu.size(), (PUCHAR)salt, 16, ITER, key, 64, 0));
    BCryptCloseAlgorithmProvider(a, 0);
    return ok;
}

static bool AesCbc(bool enc, const BYTE key[32], const BYTE iv[16], const std::string& in, std::string& out) {
    BCRYPT_ALG_HANDLE a = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&a, BCRYPT_AES_ALGORITHM, nullptr, 0))) return false;
    bool ok = NT_SUCCESS(BCryptSetProperty(a, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0));
    BCRYPT_KEY_HANDLE k = nullptr;
    if (ok) ok = NT_SUCCESS(BCryptGenerateSymmetricKey(a, &k, nullptr, 0, (PUCHAR)key, 32, 0));
    if (ok) {
        BYTE ivc[16];
        memcpy(ivc, iv, 16);
        ULONG need = 0;
        auto fn = enc ? BCryptEncrypt : BCryptDecrypt;
        ok = NT_SUCCESS(fn(k, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, ivc, 16, nullptr, 0, &need, BCRYPT_BLOCK_PADDING));
        if (ok) {
            out.assign(need, '\0');
            memcpy(ivc, iv, 16);
            ULONG got = 0;
            ok = NT_SUCCESS(fn(k, (PUCHAR)in.data(), (ULONG)in.size(), nullptr, ivc, 16, (PUCHAR)&out[0], need, &got, BCRYPT_BLOCK_PADDING));
            if (ok) out.resize(got);
        }
        BCryptDestroyKey(k);
    }
    BCryptCloseAlgorithmProvider(a, 0);
    return ok;
}

static bool Rand(BYTE* p, ULONG n) { return NT_SUCCESS(BCryptGenRandom(nullptr, p, n, BCRYPT_USE_SYSTEM_PREFERRED_RNG)); }

static void AddEntry(std::string& arc, uint32_t& count, const std::wstring& name, const std::string& data) {
    std::string n = core::ToUtf8(name);
    PutU32(arc, (uint32_t)n.size());
    arc += n;
    PutU64(arc, data.size());
    arc += data;
    count++;
}

bool BackupExport(const std::wstring& file, const std::wstring& password, std::wstring& err) {
    g_store.Save();
    std::string body;
    uint32_t count = 0;
    std::string idx;
    ReadFileBytes(JoinPath(g_store.dir, L"index.txt"), idx);
    AddEntry(body, count, L"index.txt", idx);
    static const wchar_t* sides[] = {L".html", L".rtf", L".ocr.txt"};
    for (auto& it : g_store.items) {
        std::string d;
        std::wstring mp = g_store.MainPath(it);
        if (ReadFileBytes(mp, d)) AddEntry(body, count, mp.substr(g_store.dir.size() + 1), d);
        for (auto s : sides) {
            std::wstring sp = g_store.PathFor(it, s);
            if (FileExistsW(sp) && ReadFileBytes(sp, d)) AddEntry(body, count, sp.substr(g_store.dir.size() + 1), d);
        }
    }
    std::string arc;
    PutU32(arc, count);
    arc += body;

    std::string head(MAGIC, 4);
    uint32_t flags = password.empty() ? 0 : 1;
    PutU32(head, 1);
    PutU32(head, flags);
    BYTE salt[16] = {}, iv[16] = {};
    std::string payload;
    BYTE mac[32];
    if (flags) {
        BYTE keys[64];
        if (!Rand(salt, 16) || !Rand(iv, 16) || !Derive(password, salt, keys)) { err = L"The encryption could not be set up."; return false; }
        if (!AesCbc(true, keys, iv, arc, payload)) { err = L"The encryption failed."; return false; }
        head.append((const char*)salt, 16);
        head.append((const char*)iv, 16);
        PutU64(head, payload.size());
        std::string signedData = head + payload;
        if (!Hmac(keys + 32, 32, signedData, mac)) { err = L"The check value could not be made."; return false; }
    } else {
        head.append((const char*)salt, 16);
        head.append((const char*)iv, 16);
        payload = arc;
        PutU64(head, payload.size());
        std::string signedData = head + payload;
        if (!Hmac(nullptr, 0, signedData, mac)) { err = L"The check value could not be made."; return false; }
    }
    std::string all = head + payload;
    all.append((const char*)mac, 32);
    if (!WriteFileAtomic(file, all.data(), all.size())) { err = L"The file could not be saved (is the disk full?)."; return false; }
    return true;
}

static bool SafeName(const std::wstring& n) {
    if (n.empty() || n.size() > 200) return false;
    if (n.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) return false;
    if (n.find(L"..") != std::wstring::npos) return false;
    return true;
}

bool BackupImport(const std::wstring& file, const std::wstring& password, int& added, std::wstring& err) {
    added = 0;
    std::string all;
    if (!ReadFileBytes(file, all)) { err = L"The file could not be read."; return false; }
    const size_t H = 4 + 4 + 4 + 16 + 16 + 8;
    if (all.size() < H + 32 || memcmp(all.data(), MAGIC, 4) != 0) { err = L"This is not a Feather Clipboard Manager backup file."; return false; }
    uint32_t ver, flags;
    uint64_t plen;
    memcpy(&ver, all.data() + 4, 4);
    memcpy(&flags, all.data() + 8, 4);
    memcpy(&plen, all.data() + 44, 8);
    if (ver != 1 || plen > all.size() - H - 32 || H + plen + 32 != all.size()) { err = L"The backup file is damaged or from a newer version."; return false; }
    const BYTE* salt = (const BYTE*)all.data() + 12;
    const BYTE* iv = (const BYTE*)all.data() + 28;
    std::string signedData = all.substr(0, H + (size_t)plen);
    BYTE mac[32];
    std::string arc;
    std::string payload = all.substr(H, (size_t)plen);
    if (flags & 1) {
        if (password.empty()) { err = L"This backup is protected by a password."; return false; }
        BYTE keys[64];
        if (!Derive(password, salt, keys) || !Hmac(keys + 32, 32, signedData, mac)) { err = L"The password could not be checked."; return false; }
        if (memcmp(mac, all.data() + H + plen, 32) != 0) { err = L"Wrong password, or the file is damaged."; return false; }
        if (!AesCbc(false, keys, iv, payload, arc)) { err = L"The backup could not be decrypted."; return false; }
    } else {
        if (!Hmac(nullptr, 0, signedData, mac) || memcmp(mac, all.data() + H + plen, 32) != 0) { err = L"The backup file is damaged."; return false; }
        arc = payload;
    }
    // read the archive
    std::map<std::wstring, std::string> files;
    size_t pos = 0;
    auto need = [&](size_t n) { return pos + n <= arc.size(); };
    if (!need(4)) { err = L"The backup is empty."; return false; }
    uint32_t count;
    memcpy(&count, arc.data(), 4);
    pos = 4;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t nl;
        uint64_t sz;
        if (!need(4)) break;
        memcpy(&nl, arc.data() + pos, 4);
        pos += 4;
        if (nl > 400 || !need(nl + 8)) break;
        std::wstring name = core::FromUtf8(arc.substr(pos, nl));
        pos += nl;
        memcpy(&sz, arc.data() + pos, 8);
        pos += 8;
        if (sz > arc.size() || !need((size_t)sz)) break;
        files[name] = arc.substr(pos, (size_t)sz);
        pos += (size_t)sz;
    }
    auto ix = files.find(L"index.txt");
    if (ix == files.end()) { err = L"The backup has no index."; return false; }
    for (auto& line : core::SplitLines(core::FromUtf8(ix->second))) {
        Item old;
        if (!Store::ParseIndexLine(line, old)) continue;
        bool dup = false;
        for (auto& e : g_store.items) if (e.kind == old.kind && e.hash == old.hash && !e.deleted && old.hash) { dup = true; break; }
        if (dup) continue;
        Item n = old;
        n.id = g_store.NextId();
        n.base = g_store.NewBase();
        // copy the files under the new name
        bool wrote = false;
        auto copyOne = [&](const std::wstring& oldPath, const std::wstring& newPath) {
            std::wstring rel = oldPath.substr(g_store.dir.size() + 1);
            auto f = files.find(rel);
            if (f == files.end() || !SafeName(rel)) return false;
            return WriteFileAtomic(newPath, f->second.data(), f->second.size());
        };
        wrote = copyOne(g_store.MainPath(old), g_store.MainPath(n));
        if (!wrote) { g_store.lastId--; continue; }
        copyOne(g_store.PathFor(old, L".html"), g_store.PathFor(n, L".html"));
        copyOne(g_store.PathFor(old, L".rtf"), g_store.PathFor(n, L".rtf"));
        copyOne(g_store.PathFor(old, L".ocr.txt"), g_store.PathFor(n, L".ocr.txt"));
        g_store.items.push_back(n);
        g_store.Touch(n);
        added++;
    }
    std::stable_sort(g_store.items.begin(), g_store.items.end(), [](const Item& a, const Item& b) { return a.created < b.created; });
    g_store.Save();
    return true;
}
