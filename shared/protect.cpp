#include "protect.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace protect {
namespace {

using Bytes = std::vector<unsigned char>;

// ---------- CNG helpers ----------

struct Alg {
    BCRYPT_ALG_HANDLE h = nullptr;
    explicit Alg(LPCWSTR id, bool cbc = false, bool ecb = false) {
        if (BCryptOpenAlgorithmProvider(&h, id, nullptr, 0) != 0) { h = nullptr; return; }
        if (cbc) BCryptSetProperty(h, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)), sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
        if (ecb) BCryptSetProperty(h, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)), sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    }
    ~Alg() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

bool hashOf(LPCWSTR algId, const Bytes& data, Bytes& out) {
    Alg a(algId);
    if (!a.h) return false;
    DWORD len = 0, got = 0;
    BCryptGetProperty(a.h, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&len), sizeof(len), &got, 0);
    out.assign(len, 0);
    return BCryptHash(a.h, nullptr, 0, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), out.data(), len) == 0;
}

// AES-CBC without padding (data length must be a multiple of 16).
bool aesCbcRaw(const Bytes& key, const Bytes& iv, const Bytes& in, Bytes& out) {
    Alg a(BCRYPT_AES_ALGORITHM, true);
    if (!a.h) return false;
    BCRYPT_KEY_HANDLE k = nullptr;
    if (BCryptGenerateSymmetricKey(a.h, &k, nullptr, 0, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0) != 0) return false;
    Bytes ivc = iv;
    out.assign(in.size(), 0);
    ULONG got = 0;
    bool ok = BCryptEncrypt(k, const_cast<PUCHAR>(in.data()), static_cast<ULONG>(in.size()), nullptr, ivc.data(), static_cast<ULONG>(ivc.size()),
                            out.data(), static_cast<ULONG>(out.size()), &got, 0) == 0;
    BCryptDestroyKey(k);
    return ok;
}

// AES-256-CBC with PKCS#7 padding and a random IV placed in front, as PDF requires for strings and streams.
bool aesCbcPdf(const Bytes& key, const unsigned char* data, size_t n, Bytes& out) {
    Bytes iv(16);
    if (BCryptGenRandom(nullptr, iv.data(), 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
    const size_t pad = 16 - n % 16;
    Bytes in(data, data + n);
    in.insert(in.end(), pad, static_cast<unsigned char>(pad));
    Bytes enc;
    if (!aesCbcRaw(key, iv, in, enc)) return false;
    out = iv;
    out.insert(out.end(), enc.begin(), enc.end());
    return true;
}

bool aesEcbBlock(const Bytes& key, const Bytes& in, Bytes& out) {
    Alg a(BCRYPT_AES_ALGORITHM, false, true);
    if (!a.h) return false;
    BCRYPT_KEY_HANDLE k = nullptr;
    if (BCryptGenerateSymmetricKey(a.h, &k, nullptr, 0, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0) != 0) return false;
    out.assign(in.size(), 0);
    ULONG got = 0;
    bool ok = BCryptEncrypt(k, const_cast<PUCHAR>(in.data()), static_cast<ULONG>(in.size()), nullptr, nullptr, 0, out.data(), static_cast<ULONG>(out.size()), &got, 0) == 0;
    BCryptDestroyKey(k);
    return ok;
}

bool randomBytes(size_t n, Bytes& out) {
    out.assign(n, 0);
    return BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

// ISO 32000-2 Algorithm 2.B: the revision 6 password hash.
bool hashR6(const Bytes& pw, const Bytes& salt, const Bytes& udata, Bytes& out) {
    Bytes in = pw;
    in.insert(in.end(), salt.begin(), salt.end());
    in.insert(in.end(), udata.begin(), udata.end());
    Bytes K;
    if (!hashOf(BCRYPT_SHA256_ALGORITHM, in, K)) return false;
    for (int round = 0;; ++round) {
        Bytes unit = pw;
        unit.insert(unit.end(), K.begin(), K.end());
        unit.insert(unit.end(), udata.begin(), udata.end());
        Bytes K1;
        K1.reserve(unit.size() * 64);
        for (int i = 0; i < 64; ++i) K1.insert(K1.end(), unit.begin(), unit.end());
        Bytes E;
        if (!aesCbcRaw(Bytes(K.begin(), K.begin() + 16), Bytes(K.begin() + 16, K.begin() + 32), K1, E)) return false;
        unsigned sum = 0;
        for (int i = 0; i < 16; ++i) sum += E[i];
        LPCWSTR alg = sum % 3 == 0 ? BCRYPT_SHA256_ALGORITHM : sum % 3 == 1 ? BCRYPT_SHA384_ALGORITHM : BCRYPT_SHA512_ALGORITHM;
        if (!hashOf(alg, E, K)) return false;
        // after at least 64 rounds, stop when E's last byte is at most (rounds done - 32); rounds done is round + 1
        if (round + 1 >= 64 && E.back() <= round + 1 - 32) break;
    }
    out.assign(K.begin(), K.begin() + 32);
    return true;
}

Bytes utf8Password(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    Bytes b(static_cast<size_t>(n));
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), reinterpret_cast<char*>(b.data()), n, nullptr, nullptr);
    if (b.size() > 127) b.resize(127);
    return b;
}

std::string hex(const Bytes& b) {
    static const char* d = "0123456789ABCDEF";
    std::string s;
    for (unsigned char c : b) { s += d[c >> 4]; s += d[c & 15]; }
    return s;
}

// ---------- minimal PDF object parsing (enough for the files PDFium writes) ----------

bool isWs(unsigned char c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f' || c == 0; }
bool isDelim(unsigned char c) { return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%'; }

class Rewriter {
public:
    Rewriter(const Bytes& file, const Bytes& key) : f_(file), key_(key) {}

    // Parses the xref table and rewrites every in-use object. Fills objects_ (number -> encrypted text).
    bool run(std::string& trailerText, long& size) {
        // find "startxref" near the end
        const std::string tail(f_.end() - std::min<size_t>(f_.size(), 64), f_.end());
        size_t sx = tail.rfind("startxref");
        if (sx == std::string::npos) return false;
        size_t p = f_.size() - tail.size() + sx + 9;
        long xrefPos = 0;
        skipWs(p);
        while (p < f_.size() && isdigit(f_[p])) xrefPos = xrefPos * 10 + (f_[p++] - '0');
        if (xrefPos <= 0 || static_cast<size_t>(xrefPos) >= f_.size()) return false;
        p = static_cast<size_t>(xrefPos);
        if (word(p) != "xref") return false;          // xref streams are not produced by PDFium
        std::map<long, size_t> offsets;
        for (;;) {
            size_t save = p;
            std::string a = word(p);
            if (a == "trailer") { p = save; break; }
            long start = atol(a.c_str());
            long count = atol(word(p).c_str());
            for (long i = 0; i < count; ++i) {
                std::string off = word(p), gen = word(p), kind = word(p);
                if (kind == "n") offsets[start + i] = static_cast<size_t>(_atoi64(off.c_str()));
                else if (kind != "f") return false;
            }
        }
        word(p);   // "trailer"
        skipWs(p);
        size_t tstart = p;
        size_t tend = std::string::npos;
        for (size_t i = f_.size(); i-- > tstart + 1;) if (f_[i] == '>' && f_[i - 1] == '>') { tend = i + 1; break; }
        if (tend == std::string::npos) return false;
        // the trailer dictionary ends at the first ">>" that closes the top level: take up to "startxref"
        size_t sxp = std::string(f_.begin() + tstart, f_.end()).find("startxref");
        std::string t(f_.begin() + tstart, f_.begin() + tstart + (sxp == std::string::npos ? f_.size() - tstart : sxp));
        while (!t.empty() && (isWs(static_cast<unsigned char>(t.back())))) t.pop_back();
        trailerText = t;
        size = 0;
        size_t sp = t.find("/Size");
        if (sp != std::string::npos) size = atol(t.c_str() + sp + 5);
        for (auto& kv : offsets) {
            std::string text;
            if (!object(kv.second, kv.first, text)) return false;
            objects_[kv.first] = std::move(text);
        }
        return !objects_.empty();
    }

    std::map<long, std::string> objects_;

private:
    const Bytes& f_;
    Bytes key_;

    void skipWs(size_t& p) const {
        while (p < f_.size()) {
            if (isWs(f_[p])) ++p;
            else if (f_[p] == '%') { while (p < f_.size() && f_[p] != '\r' && f_[p] != '\n') ++p; }
            else break;
        }
    }
    std::string word(size_t& p) const {
        skipWs(p);
        std::string w;
        while (p < f_.size() && !isWs(f_[p])) w += static_cast<char>(f_[p++]);
        return w;
    }

    static void appendHexString(std::string& out, const Bytes& enc) { out += '<'; out += hex(enc); out += '>'; }

    bool encryptString(const Bytes& plain, std::string& out) {
        Bytes enc;
        if (!aesCbcPdf(key_, plain.data(), plain.size(), enc)) return false;
        appendHexString(out, enc);
        return true;
    }

    // Copies one value from the file to `out`, encrypting strings. `lengthSpan`, when non-null, receives the
    // position of an integer value in `out` if this value is a plain integer (used for /Length).
    bool value(size_t& p, std::string& out, size_t* intStart = nullptr, size_t* intLen = nullptr, long* intVal = nullptr, int depth = 0) {
        if (depth > 200) return false;
        skipWs(p);
        if (p >= f_.size()) return false;
        unsigned char c = f_[p];
        if (c == '<' && p + 1 < f_.size() && f_[p + 1] == '<') {
            out += "<<"; p += 2;
            for (;;) {
                skipWs(p);
                if (p + 1 < f_.size() && f_[p] == '>' && f_[p + 1] == '>') { out += ">>"; p += 2; return true; }
                if (p >= f_.size() || f_[p] != '/') return false;
                size_t ks = p++;
                while (p < f_.size() && !isWs(f_[p]) && !isDelim(f_[p])) ++p;
                std::string key(f_.begin() + ks, f_.begin() + p);
                out += key;
                out += ' ';
                size_t is = 0, il = 0; long iv = 0;
                // Dictionary keys of interest: remember where /Length's integer lands.
                if (key == "/Length" && depth == 0) {
                    if (!value(p, out, &is, &il, &iv, depth + 1)) return false;
                    if (is) { lengthStart_ = is; lengthLen_ = il; lengthVal_ = iv; }
                } else if (!value(p, out, nullptr, nullptr, nullptr, depth + 1)) return false;
                out += ' ';
            }
        }
        if (c == '[') {
            out += '['; ++p;
            for (;;) {
                skipWs(p);
                if (p >= f_.size()) return false;
                if (f_[p] == ']') { out += ']'; ++p; return true; }
                if (!value(p, out, nullptr, nullptr, nullptr, depth + 1)) return false;
                out += ' ';
            }
        }
        if (c == '(') {   // literal string
            Bytes plain;
            int nest = 1; ++p;
            while (p < f_.size() && nest > 0) {
                unsigned char ch = f_[p++];
                if (ch == '\\' && p < f_.size()) {
                    unsigned char e = f_[p++];
                    switch (e) {
                        case 'n': plain.push_back('\n'); break; case 'r': plain.push_back('\r'); break; case 't': plain.push_back('\t'); break;
                        case 'b': plain.push_back('\b'); break; case 'f': plain.push_back('\f'); break;
                        case '\r': if (p < f_.size() && f_[p] == '\n') ++p; break;
                        case '\n': break;
                        default:
                            if (e >= '0' && e <= '7') {
                                int v = e - '0';
                                for (int k = 0; k < 2 && p < f_.size() && f_[p] >= '0' && f_[p] <= '7'; ++k) v = v * 8 + (f_[p++] - '0');
                                plain.push_back(static_cast<unsigned char>(v));
                            } else plain.push_back(e);
                    }
                } else if (ch == '(') { ++nest; plain.push_back(ch); }
                else if (ch == ')') { if (--nest > 0) plain.push_back(ch); }
                else plain.push_back(ch);
            }
            return nest == 0 && encryptString(plain, out);
        }
        if (c == '<') {   // hex string
            Bytes plain; int hi = -1; ++p;
            while (p < f_.size() && f_[p] != '>') {
                unsigned char ch = f_[p++];
                int v = isdigit(ch) ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
                if (v < 0) continue;
                if (hi < 0) hi = v; else { plain.push_back(static_cast<unsigned char>(hi * 16 + v)); hi = -1; }
            }
            if (hi >= 0) plain.push_back(static_cast<unsigned char>(hi * 16));
            if (p >= f_.size()) return false;
            ++p;
            return encryptString(plain, out);
        }
        if (c == '/') {   // name
            size_t s = p++;
            while (p < f_.size() && !isWs(f_[p]) && !isDelim(f_[p])) ++p;
            out.append(f_.begin() + s, f_.begin() + p);
            return true;
        }
        // number / keyword / reference "n g R"
        size_t s = p;
        while (p < f_.size() && !isWs(f_[p]) && !isDelim(f_[p])) ++p;
        if (p == s) return false;
        std::string tok(f_.begin() + s, f_.begin() + p);
        bool isInt = !tok.empty() && std::all_of(tok.begin(), tok.end(), [](char ch) { return isdigit(static_cast<unsigned char>(ch)); });
        if (isInt) {   // maybe the start of a reference
            size_t q = p;
            std::string g = word(q);
            if (!g.empty() && std::all_of(g.begin(), g.end(), [](char ch) { return isdigit(static_cast<unsigned char>(ch)); })) {
                size_t q2 = q;
                if (word(q2) == "R") { out += tok + " " + g + " R"; p = q2; return true; }
            }
            if (intStart) { *intStart = out.size(); *intLen = tok.size(); *intVal = atol(tok.c_str()); }
        }
        out += tok;
        return true;
    }

    size_t lengthStart_ = 0, lengthLen_ = 0;
    long lengthVal_ = -1;

    bool object(size_t off, long expectNum, std::string& text) {
        size_t p = off;
        long num = atol(word(p).c_str());
        word(p);   // generation
        if (word(p) != "obj" || num != expectNum) return false;
        lengthStart_ = lengthLen_ = 0; lengthVal_ = -1;
        std::string body;
        if (!value(p, body)) return false;
        size_t save = p;
        std::string w = word(p);
        if (w == "stream") {
            // The stream data starts after one EOL (CRLF or LF).
            if (p < f_.size() && f_[p] == '\r') ++p;
            if (p < f_.size() && f_[p] == '\n') ++p;
            if (lengthVal_ < 0 || p + static_cast<size_t>(lengthVal_) > f_.size()) return false;   // indirect /Length is not supported
            Bytes enc;
            if (!aesCbcPdf(key_, f_.data() + p, static_cast<size_t>(lengthVal_), enc)) return false;
            body.replace(lengthStart_, lengthLen_, std::to_string(enc.size()));
            text = body + "\r\nstream\r\n";
            text.append(reinterpret_cast<const char*>(enc.data()), enc.size());
            text += "\r\nendstream";
        } else {
            p = save;
            text = body;
        }
        return true;
    }
};

} // namespace

bool encrypt(const Bytes& plain, const Options& opt, Bytes& out, std::wstring& error) {
    error = L"The document could not be protected.";
    if (opt.userPassword.empty() && opt.ownerPassword.empty()) { error = L"Type a password."; return false; }

    // ---- keys ----
    Bytes fek, vs, ks, ovs, oks;
    if (!randomBytes(32, fek) || !randomBytes(8, vs) || !randomBytes(8, ks) || !randomBytes(8, ovs) || !randomBytes(8, oks)) return false;
    const Bytes upw = utf8Password(opt.userPassword);
    Bytes opwBytes = utf8Password(opt.ownerPassword);
    if (opwBytes.empty()) { Bytes r; if (!randomBytes(16, r)) return false; opwBytes = r; }   // nobody can lift the limits

    Bytes h, U, UE, O, OE, iv0(16, 0), none;
    if (!hashR6(upw, vs, none, h)) return false;
    U = h; U.insert(U.end(), vs.begin(), vs.end()); U.insert(U.end(), ks.begin(), ks.end());
    Bytes ukey;
    if (!hashR6(upw, ks, none, ukey) || !aesCbcRaw(ukey, iv0, fek, UE)) return false;
    if (!hashR6(opwBytes, ovs, U, h)) return false;
    O = h; O.insert(O.end(), ovs.begin(), ovs.end()); O.insert(O.end(), oks.begin(), oks.end());
    Bytes okey;
    if (!hashR6(opwBytes, oks, U, okey) || !aesCbcRaw(okey, iv0, fek, OE)) return false;

    // ---- permissions ----
    uint32_t P = 0xFFFFF0C0u | 512u;                      // reserved bits set; accessibility extraction always allowed
    if (opt.allowPrint) P |= 4u | 2048u;
    if (opt.allowModify) P |= 8u | 32u | 256u | 1024u;
    if (opt.allowCopy) P |= 16u;
    Bytes perms(16);
    for (int i = 0; i < 4; ++i) perms[i] = static_cast<unsigned char>((P >> (8 * i)) & 0xFF);
    for (int i = 4; i < 8; ++i) perms[i] = 0xFF;
    perms[8] = 'T'; perms[9] = 'a'; perms[10] = 'd'; perms[11] = 'b';
    Bytes rnd;
    if (!randomBytes(4, rnd)) return false;
    for (int i = 0; i < 4; ++i) perms[12 + i] = rnd[i];
    Bytes permsEnc;
    if (!aesEcbBlock(fek, perms, permsEnc)) return false;

    // ---- rewrite the objects ----
    Rewriter rw(plain, fek);
    std::string trailer;
    long size = 0;
    if (!rw.run(trailer, size) || size <= 0) { error = L"The document could not be protected (its structure is not supported)."; return false; }

    const long encNum = size;
    std::string enc = "<</Filter/Standard/V 5/R 6/Length 256/CF<</StdCF<</AuthEvent/DocOpen/CFM/AESV3/Length 32>>>>/StmF/StdCF/StrF/StdCF"
                      "/O<" + hex(O) + ">/U<" + hex(U) + ">/OE<" + hex(OE) + ">/UE<" + hex(UE) + ">/P " + std::to_string(static_cast<int32_t>(P)) +
                      "/Perms<" + hex(permsEnc) + ">/EncryptMetadata true>>";

    std::string file = "%PDF-1.7\r\n%\xB5\xB5\xB5\xB5\r\n";
    std::vector<size_t> offsets(static_cast<size_t>(size) + 1, 0);
    std::vector<bool> present(static_cast<size_t>(size) + 1, false);
    auto writeObj = [&](long n, const std::string& body) {
        offsets[static_cast<size_t>(n)] = file.size();
        present[static_cast<size_t>(n)] = true;
        file += std::to_string(n) + " 0 obj\r\n" + body + "\r\nendobj\r\n";
    };
    for (auto& kv : rw.objects_) {
        if (kv.first <= 0 || kv.first >= size) { error = L"The document could not be protected (object numbers out of range)."; return false; }
        writeObj(kv.first, kv.second);
    }
    writeObj(encNum, enc);
    const size_t xrefPos = file.size();
    file += "xref\r\n0 " + std::to_string(size + 1) + "\r\n";
    for (long n = 0; n <= size; ++n) {
        char line[32];
        if (n == 0 || !present[static_cast<size_t>(n)]) snprintf(line, sizeof line, "%010d %05d f\r\n", 0, 65535);
        else snprintf(line, sizeof line, "%010llu %05d n\r\n", static_cast<unsigned long long>(offsets[static_cast<size_t>(n)]), 0);
        file += line;
    }
    // trailer: keep the original entries, new /Size, add /Encrypt (and an /ID when there was none)
    size_t sp = trailer.find("/Size");
    if (sp != std::string::npos) {
        size_t e = sp + 5;
        while (e < trailer.size() && isWs(static_cast<unsigned char>(trailer[e]))) ++e;
        while (e < trailer.size() && isdigit(static_cast<unsigned char>(trailer[e]))) ++e;
        trailer.replace(sp, e - sp, "/Size " + std::to_string(size + 1));
    } else trailer.insert(trailer.size() - 2, "/Size " + std::to_string(size + 1));
    std::string extra = "/Encrypt " + std::to_string(encNum) + " 0 R";
    if (trailer.find("/ID") == std::string::npos) { Bytes id; randomBytes(16, id); extra += "/ID[<" + hex(id) + "><" + hex(id) + ">]"; }
    trailer.insert(trailer.size() - 2, extra);
    file += "trailer\r\n" + trailer + "\r\nstartxref\r\n" + std::to_string(xrefPos) + "\r\n%%EOF\r\n";
    out.assign(file.begin(), file.end());
    error.clear();
    return true;
}

} // namespace protect
