#include "sign.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <map>
#include <ctime>
#include <set>
#include <string>
#ifdef _WIN32
#include <winhttp.h>
#include <bcrypt.h>
#endif
#include "crypto.hpp"
#include "pdfedit.hpp"

namespace sign {
namespace {

using Bytes = std::vector<unsigned char>;

std::string hex(const unsigned char* p, size_t n) {
    static const char* d = "0123456789ABCDEF";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { s += d[p[i] >> 4]; s += d[p[i] & 15]; }
    return s;
}

// A PDF text string for `w`: plain ASCII in parentheses when possible, otherwise UTF-16BE in hex.
std::string pdfText(const std::wstring& w) {
    bool ascii = true;
    for (wchar_t c : w) if (c < 0x20 || c > 0x7E || c == L'(' || c == L')' || c == L'\\') ascii = false;
    if (ascii) return "(" + std::string(w.begin(), w.end()) + ")";
    std::string s = "<FEFF";
    char b[8];
    for (wchar_t c : w) { snprintf(b, sizeof b, "%04X", static_cast<unsigned>(c)); s += b; }
    return s + ">";
}

std::string pdfDate() {
    const std::time_t now = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    gmtime_s(&t, &now);
#else
    gmtime_r(&now, &t);
#endif
    char b[40];
    snprintf(b, sizeof b, "D:%04d%02d%02d%02d%02d%02d+00'00'", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return b;
}

bool isWs(unsigned char c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f' || c == 0; }

struct Pdf {
    const Bytes& f;
    std::map<long, size_t> offsets;
    std::string trailer;
    size_t xrefPos = 0;
    long size = 0;

    explicit Pdf(const Bytes& file) : f(file) {}

    std::string word(size_t& p) const {
        while (p < f.size() && isWs(f[p])) ++p;
        std::string w;
        while (p < f.size() && !isWs(f[p])) w += static_cast<char>(f[p++]);
        return w;
    }

    bool parse() {
        const std::string tail(f.end() - std::min<size_t>(f.size(), 64), f.end());
        size_t sx = tail.rfind("startxref");
        if (sx == std::string::npos) return false;
        size_t p = f.size() - tail.size() + sx + 9;
        xrefPos = static_cast<size_t>(std::strtoll(word(p).c_str(), nullptr, 10));
        if (xrefPos == 0 || xrefPos >= f.size()) return false;
        p = xrefPos;
        if (word(p) != "xref") return false;
        for (;;) {
            size_t save = p;
            std::string a = word(p);
            if (a == "trailer") { p = save; break; }
            long start = atol(a.c_str()), count = atol(word(p).c_str());
            for (long i = 0; i < count; ++i) {
                std::string off = word(p), gen = word(p), kind = word(p);
                if (kind == "n") offsets[start + i] = static_cast<size_t>(std::strtoll(off.c_str(), nullptr, 10));
            }
        }
        word(p);
        while (p < f.size() && isWs(f[p])) ++p;
        size_t sxp = std::string(f.begin() + p, f.end()).find("startxref");
        trailer.assign(f.begin() + p, f.begin() + p + (sxp == std::string::npos ? f.size() - p : sxp));
        while (!trailer.empty() && isWs(static_cast<unsigned char>(trailer.back()))) trailer.pop_back();
        size_t sp = trailer.find("/Size");
        size = sp == std::string::npos ? 0 : atol(trailer.c_str() + sp + 5);
        return size > 0 && !offsets.empty();
    }

    // The text between "obj" and "endobj" of an object (it must not contain a stream).
    bool body(long num, std::string& out) const {
        auto it = offsets.find(num);
        if (it == offsets.end()) return false;
        size_t p = it->second;
        word(p); word(p);
        if (word(p) != "obj") return false;
        size_t s = p;
        std::string rest(f.begin() + s, f.begin() + std::min(f.size(), s + 200000));
        size_t e = rest.find("endobj");
        if (e == std::string::npos) return false;
        out = rest.substr(0, e);
        while (!out.empty() && isWs(static_cast<unsigned char>(out.back()))) out.pop_back();
        while (!out.empty() && isWs(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
        return true;
    }

    long refAfter(const std::string& text, const char* key) const {   // "/Key N G R" -> N
        size_t k = text.find(key);
        if (k == std::string::npos) return -1;
        const char* c = text.c_str() + k + strlen(key);
        while (*c == ' ') ++c;
        char* end = nullptr;
        long n = strtol(c, &end, 10);
        if (end == c) return -1;
        while (*end == ' ') ++end;
        strtol(end, &end, 10);
        while (*end == ' ') ++end;
        return *end == 'R' ? n : -1;
    }
};

size_t matchClose(const std::string& s, size_t open, char o, char c) {
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        if (s[i] == o) ++depth;
        else if (s[i] == c && --depth == 0) return i;
    }
    return std::string::npos;
}

// Adds "N 0 R" to the array that follows `key` in `dict` (creating the key when missing). Returns false if the array
// is stored in another object (then `indirectObj` receives that object's number).
bool addToArray(std::string& dict, const char* key, long ref, long& indirectObj) {
    indirectObj = -1;
    const std::string r = " " + std::to_string(ref) + " 0 R";
    size_t k = dict.find(key);
    if (k == std::string::npos) {
        size_t open = dict.find("<<");
        if (open == std::string::npos) return false;
        dict.insert(open + 2, std::string(key) + "[" + std::to_string(ref) + " 0 R]");
        return true;
    }
    size_t v = k + strlen(key);
    while (v < dict.size() && dict[v] == ' ') ++v;
    if (v < dict.size() && dict[v] == '[') {
        size_t close = matchClose(dict, v, '[', ']');
        if (close == std::string::npos) return false;
        dict.insert(close, r);
        return true;
    }
    char* end = nullptr;
    long n = strtol(dict.c_str() + v, &end, 10);
    if (end != dict.c_str() + v) { indirectObj = n; return false; }
    return false;
}

System g_system;   // the system's services off Windows (setSystem)

} // namespace

void setSystem(System s) { g_system = std::move(s); }
const System& system() { return g_system; }

#ifdef _WIN32
std::wstring subjectName(PCCERT_CONTEXT cert) {
    wchar_t name[256]{};
    CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, 256);
    return name;
}
#endif


// ---------- time stamp (RFC 3161) ----------
namespace {

Bytes sha256Of(const Bytes& data) {
    Bytes out;
    if (!crypto::hash(crypto::Hash::Sha256, data, out)) out.clear();
    return out;
}

Bytes derLength(size_t n) {
    if (n < 0x80) return {static_cast<unsigned char>(n)};
    Bytes b;
    for (size_t v = n; v; v >>= 8) b.insert(b.begin(), static_cast<unsigned char>(v & 0xFF));
    b.insert(b.begin(), static_cast<unsigned char>(0x80 | b.size()));
    return b;
}

Bytes tlv(unsigned char tag, const Bytes& content) {
    Bytes out{tag};
    Bytes l = derLength(content.size());
    out.insert(out.end(), l.begin(), l.end());
    out.insert(out.end(), content.begin(), content.end());
    return out;
}

struct Node { size_t start = 0, header = 0, length = 0; unsigned char tag = 0; size_t contentStart() const { return start + header; } size_t end() const { return start + header + length; } };

bool readNode(const Bytes& b, size_t pos, Node& n) {
    if (pos + 2 > b.size()) return false;
    n.start = pos; n.tag = b[pos];
    size_t len = b[pos + 1], hdr = 2;
    if (len & 0x80) {
        size_t cnt = len & 0x7F;
        if (cnt == 0 || cnt > 4 || pos + 2 + cnt > b.size()) return false;
        len = 0;
        for (size_t i = 0; i < cnt; ++i) len = len * 256 + b[pos + 2 + i];
        hdr = 2 + cnt;
    }
    n.header = hdr; n.length = len;
    return n.end() <= b.size();
}

// The last child of a constructed node.
bool lastChild(const Bytes& b, const Node& parent, Node& child) {
    size_t pos = parent.contentStart();
    bool any = false;
    while (pos < parent.end()) {
        Node n;
        if (!readNode(b, pos, n)) return false;
        child = n; any = true;
        pos = n.end();
    }
    return any;
}

// Adds an unsigned attribute (the time stamp token) to the only SignerInfo of a PKCS#7 blob and fixes the lengths above it.
// BER to DER: open-ended lengths (0x80 ... 00 00) become stated ones, nested ones too; everything else is copied.
bool berElement(const Bytes& b, size_t& pos, Bytes& out, int depth) {
    if (pos + 2 > b.size() || depth > 64) return false;
    size_t p = pos;
    Bytes tag{b[p++]};
    if ((tag[0] & 0x1F) == 0x1F) {
        while (p < b.size() && (b[p] & 0x80)) tag.push_back(b[p++]);
        if (p >= b.size()) return false;
        tag.push_back(b[p++]);
    }
    if (p >= b.size()) return false;
    const unsigned char l = b[p++];
    const bool constructed = (tag[0] & 0x20) != 0;
    Bytes content;
    if (l == 0x80) {
        if (!constructed) return false;
        for (;;) {
            if (p + 2 > b.size()) return false;
            if (b[p] == 0 && b[p + 1] == 0) { p += 2; break; }
            if (!berElement(b, p, content, depth + 1)) return false;
        }
    } else {
        size_t len = 0;
        if (l & 0x80) {
            const int n = l & 0x7F;
            if (n > 8 || p + static_cast<size_t>(n) > b.size()) return false;
            for (int i = 0; i < n; ++i) len = len * 256 + b[p++];
        } else len = l;
        if (p + len > b.size()) return false;
        if (constructed) {
            size_t q = p;
            while (q < p + len) if (!berElement(b, q, content, depth + 1)) return false;
            if (q != p + len) return false;
        } else content.assign(b.begin() + static_cast<long>(p), b.begin() + static_cast<long>(p + len));
        p += len;
    }
    out.insert(out.end(), tag.begin(), tag.end());
    const Bytes L = derLength(content.size());
    out.insert(out.end(), L.begin(), L.end());
    out.insert(out.end(), content.begin(), content.end());
    pos = p;
    return true;
}

Bytes berToDer(const Bytes& b) {
    Bytes out;
    size_t p = 0;
    return berElement(b, p, out, 0) ? out : b;
}

// A detached signature checked against the bytes it signs: 2 valid and its certificate trusted here, 1 valid but the
// certificate not trusted here, 0 not matching (changed), -1 it cannot be checked on this system. `signer` gets its name.
int verifyDetached(const Bytes& cms, const Bytes& data, std::wstring& signerName) {
#ifdef _WIN32
    CRYPT_VERIFY_MESSAGE_PARA vp{};
    vp.cbSize = sizeof(vp);
    vp.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
    const BYTE* dp = data.data();
    DWORD dl = static_cast<DWORD>(data.size());
    PCCERT_CONTEXT signer = nullptr;
    const bool valid = CryptVerifyDetachedMessageSignature(&vp, 0, cms.data(), static_cast<DWORD>(cms.size()), 1, &dp, &dl, &signer) != 0;
    if (signer) signerName = subjectName(signer);
    bool trusted = false;
    if (valid && signer) {
        CERT_CHAIN_PARA cp{sizeof(cp)};
        PCCERT_CHAIN_CONTEXT chain = nullptr;
        if (CertGetCertificateChain(nullptr, signer, nullptr, nullptr, &cp, 0, nullptr, &chain) && chain) {
            CERT_CHAIN_POLICY_PARA pp{sizeof(pp)};
            CERT_CHAIN_POLICY_STATUS ps{sizeof(ps)};
            trusted = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, chain, &pp, &ps) && ps.dwError == 0;
            CertFreeCertificateChain(chain);
        }
    }
    if (signer) CertFreeCertificateContext(signer);
    return !valid ? 0 : trusted ? 2 : 1;
#else
    return g_system.verify ? g_system.verify(cms, data, signerName) : -1;
#endif
}

bool addUnsignedAttribute(const Bytes& cms, const Bytes& token, Bytes& out) {
    const char* oid = "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x02\x0E";   // id-aa-timeStampToken 1.2.840.113549.1.9.16.2.14
    Bytes attr = tlv(0x30, [&] { Bytes c(oid, oid + 13); Bytes set = tlv(0x31, token); c.insert(c.end(), set.begin(), set.end()); return c; }());
    Bytes unsignedAttrs = tlv(0xA1, Bytes(attr.begin(), attr.end()));   // [1] IMPLICIT SET OF Attribute
    // root SEQUENCE -> [0] -> SignedData SEQUENCE -> signerInfos SET -> SignerInfo SEQUENCE
    Node path[5];
    if (!readNode(cms, 0, path[0]) || path[0].tag != 0x30) return false;
    for (int i = 1; i < 5; ++i) if (!lastChild(cms, path[i - 1], path[i])) return false;
    if (path[1].tag != 0xA0 || path[2].tag != 0x30 || path[3].tag != 0x31 || path[4].tag != 0x30) return false;
    Bytes inner(cms.begin() + path[4].start, cms.begin() + path[4].end());
    // the new content of SignerInfo
    Bytes content(cms.begin() + path[4].contentStart(), cms.begin() + path[4].end());
    content.insert(content.end(), unsignedAttrs.begin(), unsignedAttrs.end());
    Bytes child = tlv(path[4].tag, content);
    for (int level = 3; level >= 0; --level) {
        Bytes c(cms.begin() + path[level].contentStart(), cms.begin() + path[level + 1].start);   // everything before the last child
        c.insert(c.end(), child.begin(), child.end());
        child = tlv(path[level].tag, c);
    }
    out = child;
    return true;
}

bool httpPost(const std::string& url, const Bytes& body, Bytes& response, std::wstring& error) {
#ifndef _WIN32
    if (!g_system.httpPost) { error = L"Time stamps are not available in this build."; return false; }
    return g_system.httpPost(url, body, response, error);
#else
    std::wstring w(url.begin(), url.end());
    URL_COMPONENTSW uc{sizeof(uc)};
    wchar_t host[256] = L"", path[1024] = L"";
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &uc)) { error = L"The time stamp address is not valid."; return false; }
    HINTERNET ses = WinHttpOpen(L"Feather PDF", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!ses) { error = L"The network could not be used."; return false; }
    WinHttpSetTimeouts(ses, 8000, 8000, 10000, 20000);
    bool ok = false;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (req) {
        const wchar_t* hdr = L"Content-Type: application/timestamp-query\r\nAccept: application/timestamp-reply\r\n";
        if (WinHttpSendRequest(req, hdr, static_cast<DWORD>(-1), const_cast<unsigned char*>(body.data()), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0) && WinHttpReceiveResponse(req, nullptr)) {
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz, nullptr);
            if (status == 200) {
                for (;;) {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                    Bytes chunk(avail);
                    DWORD got = 0;
                    if (!WinHttpReadData(req, chunk.data(), avail, &got) || got == 0) break;
                    response.insert(response.end(), chunk.begin(), chunk.begin() + got);
                    if (response.size() > 200000) break;
                }
                ok = !response.empty();
            } else error = L"The time stamp server answered with an error (" + std::to_wstring(status) + L").";
        } else error = L"The time stamp server could not be reached. Check the internet connection, or sign without a time stamp.";
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    if (!ok && error.empty()) error = L"The time stamp server could not be used.";
    return ok;
#endif
}

// Asks the server for a time stamp over `signatureValue` and returns the token (a CMS SignedData).
bool requestTimestamp(const std::string& url, const Bytes& signatureValue, Bytes& token, std::wstring& error) {
    const Bytes hash = sha256Of(signatureValue);
    if (hash.size() != 32) { error = L"The time stamp request could not be prepared."; return false; }
    const unsigned char algId[] = {0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00};
    Bytes imprint(algId, algId + sizeof(algId));
    Bytes h = tlv(0x04, hash);
    imprint.insert(imprint.end(), h.begin(), h.end());
    Bytes req{0x02, 0x01, 0x01};
    Bytes mi = tlv(0x30, imprint);
    req.insert(req.end(), mi.begin(), mi.end());
    Bytes nonce;
    if (!crypto::random(8, nonce)) nonce.assign(8, 0x5A);
    nonce[0] &= 0x7F; if (nonce[0] == 0) nonce[0] = 1;
    Bytes nn = tlv(0x02, nonce);
    req.insert(req.end(), nn.begin(), nn.end());
    const unsigned char certReq[] = {0x01, 0x01, 0xFF};
    req.insert(req.end(), certReq, certReq + 3);
    Bytes body = tlv(0x30, req), resp;
    if (!httpPost(url, body, resp, error)) return false;
    // TimeStampResp ::= SEQUENCE { PKIStatusInfo SEQUENCE { INTEGER status ... }, timeStampToken }
    Node top, status, tok;
    if (!readNode(resp, 0, top) || top.tag != 0x30 || !readNode(resp, top.contentStart(), status) || status.tag != 0x30) { error = L"The time stamp answer could not be read."; return false; }
    Node st;
    if (!readNode(resp, status.contentStart(), st) || st.tag != 0x02 || st.length != 1) { error = L"The time stamp answer could not be read."; return false; }
    const int code = resp[st.contentStart()];
    if (code != 0 && code != 1) { error = L"The time stamp server refused the request."; return false; }
    if (!readNode(resp, status.end(), tok) || tok.tag != 0x30) { error = L"The time stamp answer holds no time stamp."; return false; }
    token.assign(resp.begin() + tok.start, resp.begin() + tok.end());
    return true;
}

// The signature value (the encrypted digest) of the only signer of a PKCS#7 blob.
bool signatureValueOf(const Bytes& cms, Bytes& value) {
    Node path[5];
    if (!readNode(cms, 0, path[0]) || path[0].tag != 0x30) return false;
    for (int i = 1; i < 5; ++i) if (!lastChild(cms, path[i - 1], path[i])) return false;
    // inside SignerInfo: version, issuerAndSerial, digestAlg, [0] signedAttrs, signatureAlg, signature OCTET STRING
    size_t pos = path[4].contentStart();
    Node n, sig;
    bool found = false;
    while (pos < path[4].end()) {
        if (!readNode(cms, pos, n)) return false;
        if (n.tag == 0x04) { sig = n; found = true; }
        pos = n.end();
    }
    if (!found) return false;
    value.assign(cms.begin() + sig.contentStart(), cms.begin() + sig.end());
    return true;
}

std::string asciiOnly(const std::wstring& w) {
    std::string s;
    for (wchar_t c : w) s += (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '?';
    return s;
}
std::string pdfEscape(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '(' || c == ')' || c == '\\') o += '\\'; o += c; }
    return o;
}

} // namespace

bool signPdf(const Bytes& pdf, const Signer& signer, const Options& opt, Bytes& out, std::wstring& error) {
    error = L"The document could not be prepared for signing.";
    Pdf in(pdf);
    if (!in.parse()) { error = L"The document could not be prepared for signing (its structure is not supported)."; return false; }

    // --- locate the catalog and the first page ---
    const long rootNum = in.refAfter(in.trailer, "/Root");
    std::string catalog;
    if (rootNum < 0 || !in.body(rootNum, catalog)) return false;
    long pageNum = in.refAfter(catalog, "/Pages");
    std::string pageText;
    for (int depth = 0; depth < 24 && pageNum >= 0; ++depth) {
        if (!in.body(pageNum, pageText)) return false;
        size_t kids = pageText.find("/Kids");
        if (kids == std::string::npos) break;
        size_t open = pageText.find('[', kids);
        if (open == std::string::npos) return false;
        const char* c = pageText.c_str() + open + 1;
        char* end = nullptr;
        long n = strtol(c, &end, 10);
        if (end == c) return false;
        pageNum = n;
    }
    if (pageNum < 0) return false;

    if (opt.visible && opt.page > 0) {   // the page the user is on, not the first one
        const std::string text(pdf.begin(), pdf.end());
        pdfedit::File pf(text);
        if (pf.parseXref()) {
            std::string cat2;
            const int rootRef = pdfedit::refOf(pf.trailer, "/Root");
            if (rootRef >= 0 && pf.object(rootRef, cat2)) {
                const int pagesRef = pdfedit::refOf(cat2, "/Pages");
                int idx = opt.page;
                std::set<int> seen;
                const int found = pagesRef >= 0 ? pdfedit::pageRef(pf, pagesRef, idx, seen) : -1;
                if (found >= 0) { pageNum = found; if (!in.body(pageNum, pageText)) return false; }
            }
        }
    }
    const long sigNum = in.size, fieldNum = in.size + 1, apNum = in.size + 2, newSize = in.size + 3;
    std::map<long, std::string> newObjects;   // object number -> new text of the whole object body

    // --- page: add the signature widget to /Annots ---
    long ind = -1;
    if (!addToArray(pageText, "/Annots", fieldNum, ind)) {
        if (ind < 0) return false;
        std::string arr;
        if (!in.body(ind, arr)) return false;
        size_t close = arr.rfind(']');
        if (arr.empty() || arr[0] != '[' || close == std::string::npos) return false;
        arr.insert(close, " " + std::to_string(fieldNum) + " 0 R");
        newObjects[ind] = arr;
    } else newObjects[pageNum] = pageText;
    // --- catalog: AcroForm with the field and SigFlags 3 (signatures exist, document is append-only) ---
    size_t af = catalog.find("/AcroForm");
    if (af == std::string::npos) {
        size_t open = catalog.find("<<");
        if (open == std::string::npos) return false;
        catalog.insert(open + 2, "/AcroForm<</Fields[" + std::to_string(fieldNum) + " 0 R]/SigFlags 3>>");
        newObjects[rootNum] = catalog;
    } else {
        size_t v = af + 9;
        while (v < catalog.size() && catalog[v] == ' ') ++v;
        std::string* target = &catalog;
        std::string formObj;
        long formNum = -1;
        size_t dictStart = std::string::npos;
        if (catalog.compare(v, 2, "<<") == 0) dictStart = v;
        else {
            formNum = in.refAfter(catalog, "/AcroForm");
            if (formNum < 0 || !in.body(formNum, formObj)) return false;
            target = &formObj;
            dictStart = formObj.find("<<");
        }
        if (dictStart == std::string::npos) return false;
        size_t dictEnd = matchClose(*target, dictStart, '<', '>');   // matches the first '<' of "<<"
        // matchClose counts single characters, so find the closing ">>" by depth of "<<" pairs
        int depth = 0; dictEnd = std::string::npos;
        for (size_t i = dictStart; i + 1 < target->size(); ++i) {
            if ((*target)[i] == '<' && (*target)[i + 1] == '<') { ++depth; ++i; }
            else if ((*target)[i] == '>' && (*target)[i + 1] == '>') { if (--depth == 0) { dictEnd = i; break; } ++i; }
        }
        if (dictEnd == std::string::npos) return false;
        std::string dict = target->substr(dictStart, dictEnd + 2 - dictStart);
        long ind2 = -1;
        if (!addToArray(dict, "/Fields", fieldNum, ind2)) return false;   // fields stored elsewhere: not handled
        size_t sf = dict.find("/SigFlags");
        if (sf == std::string::npos) dict.insert(2, "/SigFlags 3");
        else {
            size_t e = sf + 9;
            while (e < dict.size() && dict[e] == ' ') ++e;
            while (e < dict.size() && isdigit(static_cast<unsigned char>(dict[e]))) ++e;
            dict.replace(sf, e - sf, "/SigFlags 3");
        }
        target->replace(dictStart, dictEnd + 2 - dictStart, dict);
        if (formNum >= 0) newObjects[formNum] = formObj; else newObjects[rootNum] = catalog;
    }

    // --- the signature dictionary and its widget ---
    const size_t kSigBytes = opt.timestampUrl.empty() ? 14000 : 32000;   // room for the PKCS#7 blob (certificate chain and time stamp included)
    const std::string zeros(kSigBytes * 2, '0');
    std::string sigDict = "<</Type/Sig/Filter/Adobe.PPKLite/SubFilter/adbe.pkcs7.detached/ByteRange[0 0000000000 0000000000 0000000000]/Contents<" + zeros + ">"
                          "/M(" + pdfDate() + ")/Name" + pdfText(signer.name);
    if (!opt.reason.empty()) sigDict += "/Reason" + pdfText(opt.reason);
    if (!opt.location.empty()) sigDict += "/Location" + pdfText(opt.location);
    sigDict += ">>";
    newObjects[sigNum] = sigDict;
    if (opt.visible && opt.right - opt.left >= 40 && opt.top - opt.bottom >= 20) {
        const double w = opt.right - opt.left, h = opt.top - opt.bottom;
        const std::string name = asciiOnly(signer.name);
        std::string lines[3] = {"Digitally signed by " + name, "Date: " + pdfDate().substr(2, 4) + "." + pdfDate().substr(6, 2) + "." + pdfDate().substr(8, 2) + " " + pdfDate().substr(10, 2) + ":" + pdfDate().substr(12, 2),
                                opt.reason.empty() ? std::string() : "Reason: " + asciiOnly(opt.reason)};
        int count = lines[2].empty() ? 2 : 3;
        double size = std::min(9.0, (h - 6) / (count * 1.3));
        size = std::max(size, 4.0);
        char buf[256];
        std::string content;
        snprintf(buf, sizeof buf, "q 0.96 0.98 1 rg 0 0 %.2f %.2f re f 0.25 0.4 0.7 RG 0.8 w 0.4 0.4 %.2f %.2f re S Q\nBT /F1 %.2f Tf 0.1 0.1 0.35 rg\n", w, h, w - 0.8, h - 0.8, size);
        content = buf;
        double y = h - 3 - size;
        for (int i = 0; i < count; ++i) {
            snprintf(buf, sizeof buf, "1 0 0 1 4 %.2f Tm (", y);
            content += buf + pdfEscape(lines[i]) + ") Tj\n";
            y -= size * 1.3;
        }
        content += "ET";
        snprintf(buf, sizeof buf, "<</Type/XObject/Subtype/Form/BBox[0 0 %.2f %.2f]/Resources<</Font<</F1<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>>>>>/Length %zu>>\nstream\n", w, h, content.size());
        newObjects[apNum] = std::string(buf) + content + "\nendstream";
        snprintf(buf, sizeof buf, "<</Type/Annot/Subtype/Widget/FT/Sig/T(Signature1)/V %ld 0 R/F 132/Rect[%.2f %.2f %.2f %.2f]/AP<</N %ld 0 R>>/P %ld 0 R>>", sigNum, opt.left, opt.bottom, opt.right, opt.top, apNum, pageNum);
        newObjects[fieldNum] = buf;
    } else {
        newObjects[fieldNum] = "<</Type/Annot/Subtype/Widget/FT/Sig/T(Signature1)/V " + std::to_string(sigNum) + " 0 R/F 132/Rect[0 0 0 0]/P " +
                               std::to_string(pageNum) + " 0 R>>";
        newObjects[apNum] = "null";   // keeps the numbering simple; nothing refers to it
    }

    // --- assemble the incremental update ---
    out = pdf;
    std::string tail;
    if (out.empty() || (out.back() != '\n' && out.back() != '\r')) out.push_back('\n');
    std::map<long, size_t> at;
    auto append = [&](const std::string& s) { out.insert(out.end(), s.begin(), s.end()); };
    for (auto& kv : newObjects) {
        at[kv.first] = out.size();
        append(std::to_string(kv.first) + " 0 obj\r\n" + kv.second + "\r\nendobj\r\n");
    }
    const size_t xref = out.size();
    std::string x = "xref\r\n";
    for (auto it = at.begin(); it != at.end();) {
        auto run = it; long first = it->first, count = 0;
        while (run != at.end() && run->first == first + count) { ++run; ++count; }
        x += std::to_string(first) + " " + std::to_string(count) + "\r\n";
        for (auto j = it; j != run; ++j) { char b[32]; snprintf(b, sizeof b, "%010llu 00000 n\r\n", static_cast<unsigned long long>(j->second)); x += b; }
        it = run;
    }
    // the new trailer repeats Root/Info/ID and points back to the previous cross-reference table
    std::string t = "<</Size " + std::to_string(newSize) + "/Root " + std::to_string(rootNum) + " 0 R";
    long infoNum = in.refAfter(in.trailer, "/Info");
    if (infoNum >= 0) t += "/Info " + std::to_string(infoNum) + " 0 R";
    size_t idPos = in.trailer.find("/ID");
    if (idPos != std::string::npos) {
        size_t s = in.trailer.find('[', idPos);
        size_t e = in.trailer.find(']', s);
        if (s != std::string::npos && e != std::string::npos) t += "/ID" + in.trailer.substr(s, e - s + 1);
    }
    t += "/Prev " + std::to_string(in.xrefPos) + ">>";
    x += "trailer\r\n" + t + "\r\nstartxref\r\n" + std::to_string(xref) + "\r\n%%EOF\r\n";
    append(x);

    // --- byte range and signature ---
    const std::string marker = "/Contents<";
    auto found = std::search(out.begin() + static_cast<long>(at[sigNum]), out.end(), marker.begin(), marker.end());
    if (found == out.end()) return false;
    const size_t cStart = static_cast<size_t>(found - out.begin()) + marker.size() - 1;   // the '<'
    const size_t cEnd = cStart + 1 + zeros.size() + 1;                                     // after the '>'
    const std::string brMarker = "/ByteRange[0 0000000000 0000000000 0000000000]";
    auto br = std::search(out.begin() + static_cast<long>(at[sigNum]), out.end(), brMarker.begin(), brMarker.end());
    if (br == out.end()) return false;
    char brText[80];
    snprintf(brText, sizeof brText, "/ByteRange[0 %010llu %010llu %010llu]", static_cast<unsigned long long>(cStart),
             static_cast<unsigned long long>(cEnd), static_cast<unsigned long long>(out.size() - cEnd));
    std::memcpy(&*br, brText, brMarker.size());

    Bytes signedData(out.begin(), out.begin() + static_cast<long>(cStart));
    signedData.insert(signedData.end(), out.begin() + static_cast<long>(cEnd), out.end());

    Bytes sig;
    {
        std::wstring why;
        if (!signer.cms || !signer.cms(signedData, sig, why) || sig.empty()) { error = why.empty() ? L"The signature could not be made." : why; return false; }
    }
    sig = berToDer(sig);   // some systems (the Mac) write lengths the open-ended way; PDF readers expect them stated
    if (!opt.timestampUrl.empty()) {
        Bytes value, token, stamped;
        std::wstring tsErr;
        if (!signatureValueOf(sig, value)) { error = L"The signature could not be prepared for a time stamp."; return false; }
        if (!requestTimestamp(opt.timestampUrl, value, token, tsErr)) { error = tsErr; return false; }
        if (!addUnsignedAttribute(sig, token, stamped)) { error = L"The time stamp could not be added to the signature."; return false; }
        sig.swap(stamped);
    }
    if (sig.size() > kSigBytes) { error = L"The certificate chain is too large to fit in the signature."; return false; }
    const std::string h = hex(sig.data(), sig.size());
    std::memcpy(&out[cStart + 1], h.data(), h.size());
    error.clear();
    return true;
}

// ---------- checking ----------

std::vector<Status> check(const Bytes& file) {
    std::vector<Status> results;
    // Find every signature dictionary by its /ByteRange and /Contents.
    const std::string text(file.begin(), file.end());
    size_t pos = 0;
    while ((pos = text.find("/ByteRange", pos)) != std::string::npos) {
        Status st;
        size_t br = text.find('[', pos);
        size_t brEnd = br == std::string::npos ? br : text.find(']', br);
        pos += 10;
        if (br == std::string::npos || brEnd == std::string::npos) continue;
        long long r[4] = {0, 0, 0, 0};
        if (sscanf(text.c_str() + br + 1, "%lld %lld %lld %lld", &r[0], &r[1], &r[2], &r[3]) != 4) continue;
        // the dictionary around this ByteRange
        size_t dictStart = text.rfind("<<", pos);
        size_t cPos = text.find("/Contents", dictStart);
        size_t reasonPos = text.find("/Reason", dictStart);
        if (cPos == std::string::npos) continue;
        size_t hs = text.find('<', cPos + 9), he = hs == std::string::npos ? hs : text.find('>', hs);
        if (hs == std::string::npos || he == std::string::npos) continue;
        Bytes blob;
        int hi = -1;
        for (size_t i = hs + 1; i < he; ++i) {
            unsigned char ch = static_cast<unsigned char>(text[i]);
            int v = isdigit(ch) ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
            if (v < 0) continue;
            if (hi < 0) hi = v; else { blob.push_back(static_cast<unsigned char>(hi * 16 + v)); hi = -1; }
        }
        // PKCS#7 blobs are zero-padded to the reserved size: cut at the end of the DER structure.
        if (blob.size() > 4 && blob[0] == 0x30) {
            size_t len = 0, hdr = 2;
            if (blob[1] & 0x80) { size_t n = blob[1] & 0x7F; hdr = 2 + n; for (size_t i = 0; i < n && 2 + i < blob.size(); ++i) len = len * 256 + blob[2 + i]; }
            else len = blob[1];
            if (hdr + len <= blob.size()) blob.resize(hdr + len);
        }
        auto dateOf = [&](const char* key) {
            size_t k = text.find(key, dictStart);
            if (k == std::string::npos || k > he + 400) return std::wstring();
            size_t s = text.find('(', k);
            size_t e = s == std::string::npos ? s : text.find(')', s);
            if (s == std::string::npos || e == std::string::npos || e - s > 40) return std::wstring();
            std::string d = text.substr(s + 1, e - s - 1);
            if (d.size() >= 14 && d.compare(0, 2, "D:") == 0) return std::wstring(d.begin() + 2, d.begin() + 6) + L"-" + std::wstring(d.begin() + 6, d.begin() + 8) + L"-" +
                                                                       std::wstring(d.begin() + 8, d.begin() + 10) + L" " + std::wstring(d.begin() + 10, d.begin() + 12) + L":" + std::wstring(d.begin() + 12, d.begin() + 14);
            return std::wstring(d.begin(), d.end());
        };
        st.time = dateOf("/M");
        if (reasonPos != std::string::npos && reasonPos < he + 400) {
            size_t s = text.find('(', reasonPos), e = s == std::string::npos ? s : text.find(')', s);
            if (s != std::string::npos && e != std::string::npos && e - s < 200) { std::string rs = text.substr(s + 1, e - s - 1); st.reason.assign(rs.begin(), rs.end()); }
        }
        // sanity of the byte range: it must leave out exactly the hex string
        const bool rangeOk = r[0] == 0 && r[1] >= 0 && r[2] > r[1] && static_cast<unsigned long long>(r[2] + r[3]) <= file.size() &&
                             static_cast<size_t>(r[1]) == hs && static_cast<size_t>(r[2]) == he + 1;
        if (!rangeOk || blob.empty()) { st.kind = Status::Unreadable; st.note = L"The signature is incomplete or damaged."; results.push_back(st); continue; }
        Bytes data(file.begin(), file.begin() + r[1]);
        data.insert(data.end(), file.begin() + r[2], file.begin() + r[2] + r[3]);
        const bool coversAll = static_cast<size_t>(r[2] + r[3]) == file.size();

        const int verdict = verifyDetached(blob, data, st.signer);
        if (verdict < 0) {
            st.kind = Status::Unreadable;
            st.note = L"Checking signatures is not available in this build.";
            results.push_back(st);
            continue;
        }
        if (verdict == 0) {
            st.kind = Status::Changed;
            st.note = L"The signature does not match: the document was changed after it was signed. Do not trust its content.";
        } else {
            const bool trusted = verdict == 2;
            st.kind = trusted ? Status::Valid : Status::ValidUntrusted;
            st.note = trusted ? L"The signature is valid and the document has not been changed since."
#ifdef _WIN32
                              : L"The document has not been changed since it was signed, but the certificate was made by the signer or by an authority Windows does not know. Ask the sender if you need to be sure who signed.";
#else
                              : L"The document has not been changed since it was signed, but the certificate was made by the signer or by an authority this Mac does not know. Ask the sender if you need to be sure who signed.";
#endif
            if (!coversAll) st.note += L" Pages or comments were added after the signature.";
        }
        {   // a time stamp token as unsigned attribute (1.2.840.113549.1.9.16.2.14)
            static const unsigned char kTs[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x10, 0x02, 0x0E};
            if (std::search(blob.begin(), blob.end(), kTs, kTs + sizeof kTs) != blob.end() && st.kind != Status::Changed && st.kind != Status::Unreadable)
                st.note += L" It carries a trusted time stamp.";
        }
        results.push_back(st);
    }
    return results;
}

#ifdef _WIN32
// ---------- certificates ----------

PCCERT_CONTEXT certFromPfx(const std::wstring& path, const std::wstring& password, HCERTSTORE& store, std::wstring& error) {
    store = nullptr;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { error = L"The certificate file could not be read."; return nullptr; }
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    Bytes data(static_cast<size_t>(std::min<long long>(sz.QuadPart, 16 << 20)));
    DWORD got = 0;
    ReadFile(f, data.data(), static_cast<DWORD>(data.size()), &got, nullptr);
    CloseHandle(f);
    CRYPT_DATA_BLOB blob{static_cast<DWORD>(got), data.data()};
    store = PFXImportCertStore(&blob, password.c_str(), PKCS12_NO_PERSIST_KEY);
    if (!store) {
        error = GetLastError() == static_cast<DWORD>(ERROR_INVALID_PASSWORD) ? L"That password did not open the certificate file." : L"That is not a certificate file with a key (.pfx or .p12).";
        return nullptr;
    }
    PCCERT_CONTEXT c = nullptr, best = nullptr;
    while ((c = CertEnumCertificatesInStore(store, c)) != nullptr) {
        DWORD n = 0;
        if (CertGetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &n) || CertGetCertificateContextProperty(c, CERT_KEY_CONTEXT_PROP_ID, nullptr, &n)) {
            best = CertDuplicateCertificateContext(c);
            CertFreeCertificateContext(c);
            break;
        }
    }
    if (!best) { error = L"The file has no certificate with a key that can sign."; CertCloseStore(store, 0); store = nullptr; }
    return best;
}

PCCERT_CONTEXT makeOwnCertificate(const std::wstring& name, const std::wstring& email, std::wstring& error) {
    // An RSA 2048 signing key in the "Microsoft Enhanced RSA and AES Cryptographic Provider", then a self-signed
    // certificate valid for ten years, saved in the user's personal store so it can be chosen again later.
    wchar_t container[64];
    swprintf(container, 64, L"FeatherPDF-%08lX%08lX", GetTickCount(), GetCurrentProcessId());
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextW(&prov, container, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_NEWKEYSET)) {
        error = L"Windows could not make a place for the new key.";
        return nullptr;
    }
    HCRYPTKEY key = 0;
    if (!CryptGenKey(prov, AT_SIGNATURE, (2048u << 16), &key)) {
        CryptReleaseContext(prov, 0);
        error = L"Windows could not make the certificate.";
        return nullptr;
    }
    CryptDestroyKey(key);

    std::wstring dn = L"CN=" + name;
    if (!email.empty()) dn += L", E=" + email;
    DWORD nameLen = 0;
    CertStrToNameW(X509_ASN_ENCODING, dn.c_str(), CERT_X500_NAME_STR, nullptr, nullptr, &nameLen, nullptr);
    Bytes nameBlob(nameLen);
    CERT_NAME_BLOB subject{nameLen, nameBlob.data()};
    if (!nameLen || !CertStrToNameW(X509_ASN_ENCODING, dn.c_str(), CERT_X500_NAME_STR, nullptr, nameBlob.data(), &nameLen, nullptr)) {
        CryptReleaseContext(prov, 0);
        error = L"The name could not be used for a certificate.";
        return nullptr;
    }
    CRYPT_KEY_PROV_INFO kpi{};
    kpi.pwszContainerName = container;
    kpi.pwszProvName = const_cast<LPWSTR>(MS_ENH_RSA_AES_PROV_W);
    kpi.dwProvType = PROV_RSA_AES;
    kpi.dwKeySpec = AT_SIGNATURE;
    CRYPT_ALGORITHM_IDENTIFIER alg{const_cast<LPSTR>(szOID_RSA_SHA256RSA), {0, nullptr}};
    SYSTEMTIME start, end;
    GetSystemTime(&start);
    end = start; end.wYear += 10;
    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(prov, &subject, 0, &kpi, &alg, &start, &end, nullptr);
    if (!cert) {   // some providers cannot do SHA-256 certificates: fall back to the default algorithm
        cert = CertCreateSelfSignCertificate(prov, &subject, 0, &kpi, nullptr, &start, &end, nullptr);
    }
    CryptReleaseContext(prov, 0);   // the certificate keeps its own link to the key container
    if (!cert) { error = L"Windows could not make the certificate."; return nullptr; }
    HCERTSTORE my = CertOpenSystemStoreW(0, L"MY");
    if (my) {
        if (!CertAddCertificateContextToStore(my, cert, CERT_STORE_ADD_REPLACE_EXISTING, nullptr)) error = L"The certificate could not be kept in your personal certificates.";
        CertCloseStore(my, 0);
    }
    return cert;
}


// The Windows certificate as a signer: Windows makes the CMS signature (with the certificate chain).
bool signPdf(const Bytes& pdf, PCCERT_CONTEXT cert, const Options& opt, Bytes& out, std::wstring& error) {
    Signer s;
    s.name = subjectName(cert);
    s.cms = [cert](const Bytes& signedData, Bytes& sig, std::wstring& err) {
        CRYPT_SIGN_MESSAGE_PARA para{};
        para.cbSize = sizeof(para);
        para.dwMsgEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
        para.pSigningCert = cert;
        para.HashAlgorithm.pszObjId = const_cast<LPSTR>(szOID_NIST_sha256);
        // include the whole certificate chain so the signature can be checked elsewhere
        std::vector<PCCERT_CONTEXT> chainCerts{cert};
        CERT_CHAIN_PARA cp{sizeof(cp)};
        PCCERT_CHAIN_CONTEXT chain = nullptr;
        if (CertGetCertificateChain(nullptr, cert, nullptr, nullptr, &cp, 0, nullptr, &chain) && chain && chain->cChain > 0) {
            PCERT_SIMPLE_CHAIN sc = chain->rgpChain[0];
            for (DWORD i = 1; i < sc->cElement; ++i) chainCerts.push_back(sc->rgpElement[i]->pCertContext);
        }
        para.cMsgCert = static_cast<DWORD>(chainCerts.size());
        para.rgpMsgCert = chainCerts.data();
        const BYTE* dataPtr = signedData.data();
        DWORD dataLen = static_cast<DWORD>(signedData.size());
        DWORD sigLen = 0;
        bool ok = CryptSignMessage(&para, TRUE, 1, &dataPtr, &dataLen, nullptr, &sigLen) != 0;
        if (!ok && GetLastError() == static_cast<DWORD>(NTE_BAD_ALGID)) {
            // Only systems without SHA-256 support in their crypto provider (some emulation layers) end up here.
            para.HashAlgorithm.pszObjId = const_cast<LPSTR>(szOID_OIWSEC_sha1);
            ok = CryptSignMessage(&para, TRUE, 1, &dataPtr, &dataLen, nullptr, &sigLen) != 0;
        }
        sig.assign(ok ? sigLen : 0, 0);
        if (ok) ok = CryptSignMessage(&para, TRUE, 1, &dataPtr, &dataLen, sig.data(), &sigLen) != 0;
        const DWORD e = GetLastError();
        if (chain) CertFreeCertificateChain(chain);
        if (!ok) {
            err = (e == static_cast<DWORD>(NTE_BAD_KEYSET) || e == static_cast<DWORD>(NTE_NO_KEY) || e == static_cast<DWORD>(CRYPT_E_NO_KEY_PROPERTY))
                      ? L"The certificate's private key could not be used. Choose a certificate that has a private key."
                      : (L"Windows could not make the signature (error " + std::to_wstring(e) + L").");
            return false;
        }
        sig.resize(sigLen);
        return true;
    };
    return signPdf(pdf, s, opt, out, error);
}
#endif

} // namespace sign
